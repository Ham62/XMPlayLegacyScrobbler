/* lfm.cpp
 * 
 * Implements scrobble-related Last.fm API requests via the https library
 * 
 * Library written to conform to the Scrobble 2.0 API documentation
 * https://www.last.fm/api/scrobbling
 *
 * Requires JSMN for JSON parsing. Uses Wincrypt for MD5 hashing
 */
#include "StdAfx.h"
#include "include\https.h"
#include "include\jsmn.h"
#include "include\lfm.h"

char *pszHostname = "ws.audioscrobbler.com";
//char *pszRequest = (char *)malloc(2048);

// Generate a MD5 hash from a source string
char *generateMD5Hash(char *pszInput) {
	HCRYPTPROV hProv;
	HCRYPTHASH hHash;
	int iBuffLen = 16; // Hash is 16 bytes long
	BYTE *pbHash = (unsigned char *)malloc(iBuffLen);
	char *pszHash = (char *)malloc(iBuffLen*2+1);

    // Get a CPS handle
    CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
    
    // Create hash object
    CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash);
    
    // Hash input string
    CryptHashData(hHash, (unsigned char *)pszInput, strlen(pszInput), 0);
    
    // Get the hash value
    CryptGetHashParam(hHash, HP_HASHVAL, pbHash, (unsigned long *)(&iBuffLen), 0);

	// Convert from binary hash to hex string
	int iC = 0;
	for (int i = 0; i < iBuffLen; i++) {
		sprintf(&pszHash[iC], "%2.2x", pbHash[i]);
		iC += 2;
	}

	// Clean up
	CryptDestroyHash(hHash);
	CryptReleaseContext(hProv, 0);
    
    free(pbHash);
	return pszHash;
}

int doNetRequest(char *pszRequest, char **ppszJSON, jsmntok_t *pTokens, int iMaxTokens) {
	HTTPS *hHTTPS;
	HTTP_RESPONSE *pResponse;

	// Connect to lfm API server
	hHTTPS = connectHTTPS(pszHostname);
	if (hHTTPS == NULL) {
		printf("Failed to establish connection!\r\n");
		return LFMERROR_NETWORK;
	}
	//printf("Connection established to %s\r\n", pszHostname);

	// Send HTTP request
	if (postHTTPS(hHTTPS, "/2.0/", pszRequest, strlen(pszRequest))) {
		puts("Failed to send server request!");
		disconnectHTTPS(hHTTPS);
		return LFMERROR_NETWORK;
	}

	// Read response from server
	pResponse = readHTTPS(hHTTPS);

	disconnectHTTPS(hHTTPS);
	if (pResponse == NULL) {
		puts("Error reading HTTP response!");
		return LFMERROR_NETWORK;
	}

	//printf("HTTP Header:\r\n%s\r\n", pResponse->pszHeader);
	//printf("HTTP code: %d\r\nContent Length: %d\r\n", pResponse->iCode, pResponse->iLength);
	//printf("\r\n-------------------\r\nServer Response:\r\n%s\r\nLength: %d\r\n", pResponse->pData, strlen(pResponse->pData));

	// Get JSON object from response
	*ppszJSON = pResponse->pData;
	free(pResponse->pszHeader);
	free(pResponse);

	// Parse JSON and tokenize
	jsmn_parser parser;

	jsmn_init(&parser);

	size_t iJSONLen = strlen(*ppszJSON);
	int iTokenCount = jsmn_parse(&parser, *ppszJSON, iJSONLen, pTokens, iMaxTokens);

	if (iTokenCount < 0) {
		printf("Failed to parse JSON: %d\r\n", iTokenCount);
		free(*ppszJSON);
		*ppszJSON = 0;
		return LFMERROR_RESPONSE;

	} else if (iTokenCount < 1 || pTokens[0].type != JSMN_OBJECT) {
		puts("Error: JSON Object expected!");
		free(*ppszJSON);
		*ppszJSON = 0;
		return LFMERROR_RESPONSE;
	}

	return iTokenCount;
}

// Compare a token's name with a string
int tokenCmp(char *pszJSON, jsmntok_t *tok, char *pszStr) {
	if (tok->type == JSMN_STRING && (int)strlen(pszStr) == tok->end-tok->start &&
		strncmp(pszJSON+tok->start, pszStr, tok->end-tok->start) == 0) {
		return 1;
	}
	return 0;
}

void LFMInit() {
	loadHTTPS();
}

void LFMCleanup() {
	unloadHTTPS();
}

LFMSESSION *auth_getMobileSession(char *pszAPIKey, char *pszUsername, char *pszPassword, char *pszSecret) {
	LFMSESSION *pSession;
	jsmntok_t tokens[256];
	char *pszJSON = NULL;

	char *pszRequest = (char *)malloc(2048);
	char *pszSignature;

	// Create authentication request signature
	char *pszSigFull = (char *)malloc(1024);
	sprintf(pszSigFull, "api_key%smethodauth.getMobileSessionpassword%susername%s%s",
		pszAPIKey,
		pszPassword,
		pszUsername,
		pszSecret);

	// Get hash for signature
	pszSignature = generateMD5Hash(pszSigFull);
	free(pszSigFull);


	// Generate and send request
	sprintf(pszRequest, "method=%s&username=%s&password=%s&api_key=%s&api_sig=%s&format=json",
		"auth.getMobileSession",
		pszUsername,
		pszPassword,
		pszAPIKey,
		pszSignature);

	free(pszSignature);

	int iTokenCount = doNetRequest(pszRequest, &pszJSON, &tokens[0], 256);

	free(pszRequest);


	// Initialize session struct
	pSession = (LFMSESSION *)malloc(sizeof(LFMSESSION));
	if (pSession == NULL) {
		puts("Out of memory!");
		free(pszJSON);
		return NULL;
	}

	// Initialize everything to zero
	pSession->error.iError = LFMERROR_NONE;
	pSession->error.pszMessage = NULL;

	pSession->pszSessionKey = NULL;
	pSession->pszUsername = NULL;
	pSession->iSubscriber = 0;

	// Copy in secret and API key
	strcpy(pSession->szAPIKey, pszAPIKey);
	strcpy(pSession->szSecret, pszSecret);

	// If iTokenCount < 0 then it's an error code for the request
	if (iTokenCount < 0) {
		pSession->error.iError = iTokenCount;
	}

	// Parse JSON
	for (int iTk = 1; iTk < iTokenCount; iTk++) {
		if (tokens[iTk].type != JSMN_STRING)
			continue;

		if (tokenCmp(pszJSON, &tokens[iTk], "error")) {
			iTk++;
			pszJSON[tokens[iTk].end] = 0; // Convert to null-terminated string
			pSession->error.iError = atoi(pszJSON+tokens[iTk].start);

		} else if (tokenCmp(pszJSON, &tokens[iTk], "message")) {
			iTk++;
			pszJSON[tokens[iTk].end] = 0; // Convert to null-terminated string

			pSession->error.pszMessage = (char *)malloc(tokens[iTk].end-tokens[iTk].start+1);
			if (pSession->error.pszMessage == NULL) {
				printf("Out of memory!\r\n");
				freeObject(pSession);
				free(pszJSON);
				return NULL;
			}

			strcpy(pSession->error.pszMessage, pszJSON+tokens[iTk].start);

		} else if (tokenCmp(pszJSON, &tokens[iTk], "name")) {
			iTk++;
			pszJSON[tokens[iTk].end] = 0; // Convert to null-terminated string

			pSession->pszUsername = (char *)malloc(tokens[iTk].end-tokens[iTk].start+1);
			if (pSession->pszUsername == NULL) {
				printf("Out of memory!\r\n");
				freeObject(pSession);
				free(pszJSON);
				return NULL;
			}

			strcpy(pSession->pszUsername, pszJSON+tokens[iTk].start);

		} else if (tokenCmp(pszJSON, &tokens[iTk], "key")) {
			iTk++;
			pszJSON[tokens[iTk].end] = 0; // Convert to null-terminated string

			pSession->pszSessionKey = (char *)malloc(tokens[iTk].end-tokens[iTk].start+1);
			if (pSession->pszSessionKey == NULL) {
				printf("Out of memory!\r\n");
				freeObject(pSession);
				free(pszJSON);
				return NULL;
			}

			strcpy(pSession->pszSessionKey, pszJSON+tokens[iTk].start);

		} else if (tokenCmp(pszJSON, &tokens[iTk], "subscriber")) {
			iTk++;
			pszJSON[tokens[iTk].end] = 0; // Convert to null-terminated string
			pSession->iSubscriber = atoi(pszJSON+tokens[iTk].start);
		}
	}

	if (pszJSON != 0)
		free(pszJSON);

	return pSession;
}


// Create the request string and signature in alphabetical order
char *createUpdateNowPlayingString(LFMSESSION *lfmSession, LFMNOWPLAYING *lfmnp) {
	int iSigLen = 0;
	int iRequestLen = 0;
	int iLen = 0;

	char *pszRequest = (char *)malloc(2048);
	char *pszEncoded;
	char *pszSignature;

	// Create authentication request signature
	char *pszSigFull = (char *)malloc(1024);

	// Add method to request
	sprintf(pszRequest, "method=track.updateNowPlaying");
	iRequestLen = STRLEN("method=track.updateNowPlaying");

	// Check album
	if (lfmnp->pszAlbum != NULL) {
		pszEncoded = urlEncode(lfmnp->pszAlbum);

		sprintf(pszSigFull, "album%s", lfmnp->pszAlbum);
		sprintf(pszRequest+iRequestLen, "&album=%s", pszEncoded);

		iSigLen += 5+strlen(lfmnp->pszAlbum);
		iRequestLen += 7+strlen(pszEncoded);

		free(pszEncoded);
	}

	// Check albumArtist
	if (lfmnp->pszAlbumArtist != NULL) {
		pszEncoded = urlEncode(lfmnp->pszAlbumArtist);

		sprintf(pszSigFull+iSigLen, "albumArtist%s", lfmnp->pszAlbumArtist);
		sprintf(pszRequest+iRequestLen, "&albumArtist=%s", pszEncoded);

		iSigLen += 11+strlen(lfmnp->pszAlbumArtist);
		iRequestLen += 13+strlen(pszEncoded);

		free(pszEncoded);
	}

	// Add APIKey to sig and request
	sprintf(pszSigFull+iSigLen, "api_key%s", lfmSession->szAPIKey);
	sprintf(pszRequest+iRequestLen, "&api_key=%s", lfmSession->szAPIKey);

	iLen = strlen(lfmSession->szAPIKey);
	iSigLen += 7+KEY_LENGTH;
	iRequestLen += 9+KEY_LENGTH;

	// Check artist
	if (lfmnp->pszArtist != NULL) {
		pszEncoded = urlEncode(lfmnp->pszArtist);

		sprintf(pszSigFull+iSigLen, "artist%s", lfmnp->pszArtist);
		sprintf(pszRequest+iRequestLen, "&artist=%s", pszEncoded);

		iSigLen += 6+strlen(lfmnp->pszArtist);
		iRequestLen += 8+strlen(pszEncoded);

		free(pszEncoded);
	}

	// Check duration
	if (lfmnp->iDuration > 0) {
		sprintf(pszSigFull+iSigLen, "duration%d", lfmnp->iDuration);
		sprintf(pszRequest+iRequestLen, "&duration=%d", lfmnp->iDuration);

		iLen = strlen(pszRequest+iRequestLen);
		iSigLen += iLen-2;
		iRequestLen += iLen;
	}

	// Check mbid
	if (lfmnp->pszMBID != NULL) {
		sprintf(pszSigFull+iSigLen, "mbid%s", lfmnp->pszMBID);
		sprintf(pszRequest+iRequestLen, "&mbid=%s", lfmnp->pszMBID);

		iLen = strlen(lfmnp->pszMBID);
		iSigLen += 4+iLen;
		iRequestLen += 6+iLen;
	}

	// Add method to signature
	sprintf(pszSigFull+iSigLen, "methodtrack.updateNowPlaying");
	iSigLen += STRLEN("methodtrack.updateNowPlaying");

	// Add session key to sig and request
	sprintf(pszSigFull+iSigLen, "sk%s", lfmSession->pszSessionKey);
	sprintf(pszRequest+iRequestLen, "&sk=%s", lfmSession->pszSessionKey);

	iLen = strlen(lfmSession->pszSessionKey);
	iSigLen += 2+iLen;
	iRequestLen += 4+iLen;

	// Check track
	if (lfmnp->pszTrack != NULL) {
		pszEncoded = urlEncode(lfmnp->pszTrack);

		sprintf(pszSigFull+iSigLen, "track%s", lfmnp->pszTrack);
		sprintf(pszRequest+iRequestLen, "&track=%s", pszEncoded);

		iSigLen += 5+strlen(lfmnp->pszTrack);
		iRequestLen += 7+strlen(pszEncoded);

		free(pszEncoded);
	}

	// Check trackNumber
	if (lfmnp->iTrackNumber > 0) {
		sprintf(pszSigFull+iSigLen, "trackNumber%d", lfmnp->iTrackNumber);
		sprintf(pszRequest+iRequestLen, "&trackNumber=%d", lfmnp->iTrackNumber);

		iLen = strlen(pszRequest+iRequestLen);
		iSigLen += iLen-2;
		iRequestLen += iLen;
	}

	sprintf(pszSigFull+iSigLen, "%s", lfmSession->szSecret);
	//printf("Signature:\r\n%s\r\n", pszSigFull);

	// Generate hash for signature
	pszSignature = generateMD5Hash(pszSigFull);
	free(pszSigFull);

	// Add signature and finalize request string
	sprintf(pszRequest+iRequestLen, "&api_sig=%s&format=json", pszSignature);
	free(pszSignature);

	//printf("Request:\r\n%s\r\n", pszRequest);

	return pszRequest;
}

LFMERROR *track_updateNowPlaying(LFMSESSION *lfmSession, LFMNOWPLAYING *lfmnp) {
	LFMERROR *pError;
	jsmntok_t tokens[256];
	char *pszJSON = NULL;

	// Make updateNowPlaying request
	char *pszReq = createUpdateNowPlayingString(lfmSession, lfmnp);
	
	int iTokenCount = doNetRequest(pszReq, &pszJSON, &tokens[0], 256);

	free(pszReq);

	pError = (LFMERROR *)malloc(sizeof(LFMERROR));

	// Initialize everything to zero
	pError->iError = LFMERROR_NONE;
	pError->pszMessage = NULL;

	// If iTokenCount < 0 then it's an error code for the request
	if (iTokenCount < 0) {
		pError->iError = iTokenCount;
	}

	// Parse JSON
	for (int iTk = 1; iTk < iTokenCount; iTk++) {
		if (tokens[iTk].type != JSMN_STRING)
			continue;

		if (tokenCmp(pszJSON, &tokens[iTk], "error")) {
			iTk++;
			pszJSON[tokens[iTk].end] = 0; // Convert to null-terminated string
			pError->iError = atoi(pszJSON+tokens[iTk].start);

		} else if (tokenCmp(pszJSON, &tokens[iTk], "message")) {
			iTk++;
			pszJSON[tokens[iTk].end] = 0; // Convert to null-terminated string

			pError->pszMessage = (char *)malloc(tokens[iTk].end-tokens[iTk].start+1);
			if (pError->pszMessage == NULL) {
				printf("Out of memory!\r\n");
				freeObject(pError);
				free(pszJSON);
				return NULL;
			}

			strcpy(pError->pszMessage, pszJSON+tokens[iTk].start);
		}
	}

	if (pszJSON != 0)
		free(pszJSON);

	return pError;
}

// Create the request string and signature in alphabetical order
char *createScrobbleString(LFMSESSION *lfmSession, LFMSCROBBLE *lfmScrobble) {
	int iSigLen = 0;
	int iRequestLen = 0;
	int iLen = 0;

	char *pszRequest = (char *)malloc(2048);
	char *pszEncoded;
	char *pszSignature;

	// Create authentication request signature
	char *pszSigFull = (char *)malloc(1024);

	// Add method to request
	sprintf(pszRequest, "method=track.scrobble");
	iRequestLen = STRLEN("method=track.scrobble");

	// Check album
	if (lfmScrobble->pszAlbum != NULL) {
		pszEncoded = urlEncode(lfmScrobble->pszAlbum);

		sprintf(pszSigFull, "album%s", lfmScrobble->pszAlbum);
		sprintf(pszRequest+iRequestLen, "&album=%s", pszEncoded);

		iSigLen += 5+strlen(lfmScrobble->pszAlbum);
		iRequestLen += 7+strlen(pszEncoded);

		free(pszEncoded);
	}

	// Check albumArtist
	if (lfmScrobble->pszAlbumArtist != NULL) {
		pszEncoded = urlEncode(lfmScrobble->pszAlbumArtist);

		sprintf(pszSigFull+iSigLen, "albumArtist%s", lfmScrobble->pszAlbumArtist);
		sprintf(pszRequest+iRequestLen, "&albumArtist=%s", pszEncoded);

		iSigLen += 11+strlen(lfmScrobble->pszAlbumArtist);
		iRequestLen += 13+strlen(pszEncoded);

		free(pszEncoded);
	}

	// Add APIKey to sig and request
	sprintf(pszSigFull+iSigLen, "api_key%s", lfmSession->szAPIKey);
	sprintf(pszRequest+iRequestLen, "&api_key=%s", lfmSession->szAPIKey);

	iLen = strlen(lfmSession->szAPIKey);
	iSigLen += 7+KEY_LENGTH;
	iRequestLen += 9+KEY_LENGTH;

	// Check artist
	if (lfmScrobble->pszArtist != NULL) {
		pszEncoded = urlEncode(lfmScrobble->pszArtist);

		sprintf(pszSigFull+iSigLen, "artist%s", lfmScrobble->pszArtist);
		sprintf(pszRequest+iRequestLen, "&artist=%s", pszEncoded);

		iSigLen += 6+strlen(lfmScrobble->pszArtist);
		iRequestLen += 8+strlen(pszEncoded);

		free(pszEncoded);
	}

	// Check duration
	if (lfmScrobble->iDuration > 0) {
		sprintf(pszSigFull+iSigLen, "duration%d", lfmScrobble->iDuration);
		sprintf(pszRequest+iRequestLen, "&duration=%d", lfmScrobble->iDuration);

		iLen = strlen(pszRequest+iRequestLen);
		iSigLen += iLen-2;
		iRequestLen += iLen;
	}

	// Check mbid
	if (lfmScrobble->pszMBID != NULL) {
		sprintf(pszSigFull+iSigLen, "mbid%s", lfmScrobble->pszMBID);
		sprintf(pszRequest+iRequestLen, "&mbid=%s", lfmScrobble->pszMBID);

		iLen = strlen(lfmScrobble->pszMBID);
		iSigLen += 4+iLen;
		iRequestLen += 6+iLen;
	}

	// Add method to signature
	sprintf(pszSigFull+iSigLen, "methodtrack.scrobble");
	iSigLen += STRLEN("methodtrack.scrobble");

	// Add session key to sig and request
	sprintf(pszSigFull+iSigLen, "sk%s", lfmSession->pszSessionKey);
	sprintf(pszRequest+iRequestLen, "&sk=%s", lfmSession->pszSessionKey);

	iLen = strlen(lfmSession->pszSessionKey);
	iSigLen += 2+iLen;
	iRequestLen += 4+iLen;

	// Add timestamp to sig and request
	sprintf(pszSigFull+iSigLen, "timestamp%lli", lfmScrobble->llTimestamp);
	sprintf(pszRequest+iRequestLen, "&timestamp=%lli", lfmScrobble->llTimestamp);
	
	iLen = strlen(pszRequest+iRequestLen);
	iSigLen += iLen-2;
	iRequestLen += iLen;

	// Check track
	if (lfmScrobble->pszTrack != NULL) {
		pszEncoded = urlEncode(lfmScrobble->pszTrack);

		sprintf(pszSigFull+iSigLen, "track%s", lfmScrobble->pszTrack);
		sprintf(pszRequest+iRequestLen, "&track=%s", pszEncoded);

		iSigLen += 5+strlen(lfmScrobble->pszTrack);
		iRequestLen += 7+strlen(pszEncoded);

		free(pszEncoded);
	}

	// Check trackNumber
	if (lfmScrobble->iTrackNumber > 0) {
		sprintf(pszSigFull+iSigLen, "trackNumber%d", lfmScrobble->iTrackNumber);
		sprintf(pszRequest+iRequestLen, "&trackNumber=%d", lfmScrobble->iTrackNumber);

		iLen = strlen(pszRequest+iRequestLen);
		iSigLen += iLen-2;
		iRequestLen += iLen;
	}

	sprintf(pszSigFull+iSigLen, "%s", lfmSession->szSecret);
	//printf("Signature:\r\n%s\r\n", pszSigFull);

	// Generate hash for signature
	pszSignature = generateMD5Hash(pszSigFull);
	free(pszSigFull);

	// Add signature and finalize request string
	sprintf(pszRequest+iRequestLen, "&api_sig=%s&format=json", pszSignature);
	free(pszSignature);

	//printf("Request:\r\n%s\r\n", pszRequest);

	return pszRequest;
}

LFMERROR *track_scrobble(LFMSESSION *lfmSession, LFMSCROBBLE *lfmScrobble) {
	LFMERROR *pError;
	jsmntok_t tokens[256];
	char *pszJSON = NULL;

	// Make scrobble request
	char *pszReq = createScrobbleString(lfmSession, lfmScrobble);
	
	int iTokenCount = doNetRequest(pszReq, &pszJSON, &tokens[0], 256);

	free(pszReq);

	pError = (LFMERROR *)malloc(sizeof(LFMERROR));

	pError->iError = LFMERROR_NONE;
	pError->pszMessage = NULL;

	if (iTokenCount < 0)
		pError->iError = iTokenCount;

	// Parse JSON
	for (int iTk = 1; iTk < iTokenCount; iTk++) {
		if (tokens[iTk].type != JSMN_STRING)
			continue;

		if (tokenCmp(pszJSON, &tokens[iTk], "error")) {
			iTk++;
			pszJSON[tokens[iTk].end] = 0; // Convert to null-terminated string
			pError->iError = atoi(pszJSON+tokens[iTk].start);

		} else if (tokenCmp(pszJSON, &tokens[iTk], "message")) {
			iTk++;
			pszJSON[tokens[iTk].end] = 0; // Convert to null-terminated string

			pError->pszMessage = (char *)malloc(tokens[iTk].end-tokens[iTk].start+1);
			if (pError->pszMessage == NULL) {
				printf("Out of memory!\r\n");
				freeObject(pError);
				free(pszJSON);
				return NULL;
			}

			strcpy(pError->pszMessage, pszJSON+tokens[iTk].start);
		}
	}

	if (pszJSON != 0)
		free(pszJSON);

	return pError;
}

void addMemberString(char *pszParameter, int iIndex, char *pszValue,
					 char *pszRequest, int &iRequestLen, 
					 char *pszSignature, int &iSigLen) {

	char *pszI = (char *)malloc(4);
	sprintf(pszI, "%d", iIndex);

	int iILen = strlen(pszI);
	int iParamLen = strlen(pszParameter);

	char *pszEncoded;

	if (pszValue != NULL) {
		pszEncoded = urlEncode(pszValue);

		sprintf(pszSignature+iSigLen, "%s[%s]%s", pszParameter, pszI, pszValue);
		sprintf(pszRequest+iRequestLen, "&%s[%s]=%s", pszParameter, pszI, pszEncoded);

		iSigLen += 2+iParamLen+iILen+strlen(pszValue);
		iRequestLen += 4+iParamLen+iILen+strlen(pszEncoded);

		free(pszEncoded);
	}

	free(pszI);
}

void addMemberInt(char *pszParameter, int iIndex, int iValue,
					 char *pszRequest, int &iRequestLen, 
					 char *pszSignature, int &iSigLen) {

	int iLen = 0;
	int iParamLen = strlen(pszParameter);

	if (iValue > 0) {
		sprintf(pszSignature+iSigLen, "%s[%d]%u", pszParameter, iIndex, iValue);
		sprintf(pszRequest+iRequestLen, "&%s[%d]=%u", pszParameter, iIndex, iValue);

		iLen = strlen(pszRequest+iRequestLen);
		iSigLen += iLen-2;
		iRequestLen += iLen;
	}
}

void addMemberStrings(char *pszParameter, char **ppszFirstMember, int iNumScrobbles,
					 char *pszRequest, int &iRequestLen, 
					 char *pszSignature, int &iSigLen) {

	char *pszValue = *ppszFirstMember;

	// Generate album[0], albumArtist[0] first
	addMemberString(pszParameter, 0, pszValue, pszRequest, iRequestLen, pszSignature, iSigLen);

	// Go through all numbers 1-9 (for ASCII sorting purpose)
	for (int iBaseInt = 1; iBaseInt < iNumScrobbles && iBaseInt < 10; iBaseInt++) {
		// Order is (0, 10, 11, ..., 19, 1, 20, 21, etc.)
		for (int i = iBaseInt*10; i < iNumScrobbles && i < (iBaseInt+1)*10; i++) {
			// this loop goes [10, ..., 19] or [20, ..., 29], etc.
			pszValue = *(char **)((char *)ppszFirstMember + (i*sizeof(LFMSCROBBLE)));
			addMemberString(pszParameter, i, pszValue, pszRequest, iRequestLen, pszSignature, iSigLen);
		}		
		// add artist[iBaseInt] now
		pszValue = *(char **)((char *)ppszFirstMember + (iBaseInt*sizeof(LFMSCROBBLE)));
		/*puts("");
		printf("%d %d\r\n", iBaseInt, sizeof(LFMSCROBBLE));
		printf("%d %d\r\n", ppszFirstMember, pszValue);
		printf("%s %s\r\n", *ppszFirstMember, pszValue);*/
		addMemberString(pszParameter, iBaseInt, pszValue, pszRequest, iRequestLen, pszSignature, iSigLen);
	}
}

void addMemberInts(char *pszParameter, int *piFirstMember, int iNumScrobbles,
					 char *pszRequest, int &iRequestLen, 
					 char *pszSignature, int &iSigLen) {

	int iValue = *piFirstMember;

	// Generate album[0], albumArtist[0] first
	addMemberInt(pszParameter, 0, iValue, pszRequest, iRequestLen, pszSignature, iSigLen);

	// Go through all numbers 1-9 (for ASCII sorting purpose)
	for (int iBaseInt = 1; iBaseInt < iNumScrobbles && iBaseInt < 10; iBaseInt++) {
		// Order is (0, 10, 11, ..., 19, 1, 20, 21, etc.)
		for (int i = iBaseInt*10; i < iNumScrobbles && i < (iBaseInt+1)*10; i++) {
			// this loop goes [10, ..., 19] or [20, ..., 29], etc.
			iValue = *(int *)((char *)piFirstMember + (i*sizeof(LFMSCROBBLE)));
			addMemberInt(pszParameter, i, iValue, pszRequest, iRequestLen, pszSignature, iSigLen);
		}		
		// add artist[iBaseInt] now
		iValue = *(int *)((char *)piFirstMember + (iBaseInt*sizeof(LFMSCROBBLE)));
		addMemberInt(pszParameter, iBaseInt, iValue, pszRequest, iRequestLen, pszSignature, iSigLen);
	}
}

char *createScrobbleString(LFMSESSION *lfmSession, LFMSCROBBLE *lfmScrobbles, int iNumScrobbles) {
	int iSigLen = 0;
	int iRequestLen = 0;

	char *pszSignature;
	char *pszRequest = (char *)malloc(2048*iNumScrobbles);

	// Create authentication request signature
	char *pszSigFull = (char *)malloc(1024*iNumScrobbles);

	// Add method to request
	sprintf(pszRequest, "method=track.scrobble");
	iRequestLen = STRLEN("method=track.scrobble");

	// Generate albumArtist[0], album[0] first
	addMemberStrings("albumArtist", &lfmScrobbles[0].pszAlbumArtist, iNumScrobbles, pszRequest, iRequestLen, pszSigFull, iSigLen);
	addMemberStrings("album", &lfmScrobbles[0].pszAlbum, iNumScrobbles, pszRequest, iRequestLen, pszSigFull, iSigLen);

	// Add APIKey to sig and request
	sprintf(pszSigFull+iSigLen, "api_key%s", lfmSession->szAPIKey);
	sprintf(pszRequest+iRequestLen, "&api_key=%s", lfmSession->szAPIKey);

	int iLen = strlen(lfmSession->szAPIKey);
	iSigLen += 7+KEY_LENGTH;
	iRequestLen += 9+KEY_LENGTH;

	// Now add artist, duration, mbid
	addMemberStrings("artist", &lfmScrobbles[0].pszArtist, iNumScrobbles, pszRequest, iRequestLen, pszSigFull, iSigLen);
	addMemberInts("duration", &lfmScrobbles[0].iDuration, iNumScrobbles, pszRequest, iRequestLen, pszSigFull, iSigLen);
	addMemberStrings("mbid", &lfmScrobbles[0].pszMBID, iNumScrobbles, pszRequest, iRequestLen, pszSigFull, iSigLen);

	// Add method to signature
	sprintf(pszSigFull+iSigLen, "methodtrack.scrobble");
	iSigLen += STRLEN("methodtrack.scrobble");

	// Add session key to sig and request
	sprintf(pszSigFull+iSigLen, "sk%s", lfmSession->pszSessionKey);
	sprintf(pszRequest+iRequestLen, "&sk=%s", lfmSession->pszSessionKey);

	iLen = strlen(lfmSession->pszSessionKey);
	iSigLen += 2+iLen;
	iRequestLen += 4+iLen;

	// Add timestamp
	addMemberInts("timestamp", (int *)&lfmScrobbles[0].llTimestamp, iNumScrobbles, pszRequest, iRequestLen, pszSigFull, iSigLen);

	// Now add trackNumber[], track[]
	addMemberInts("trackNumber", &lfmScrobbles[0].iTrackNumber, iNumScrobbles, pszRequest, iRequestLen, pszSigFull, iSigLen);
	addMemberStrings("track", &lfmScrobbles[0].pszTrack, iNumScrobbles, pszRequest, iRequestLen, pszSigFull, iSigLen);

	sprintf(pszSigFull+iSigLen, "%s", lfmSession->szSecret);
	//printf("Signature:\r\n%s\r\n", pszSigFull);

	// Generate hash for signature
	pszSignature = generateMD5Hash(pszSigFull);
	free(pszSigFull);

	// Add signature and finalize request string
	sprintf(pszRequest+iRequestLen, "&api_sig=%s&format=json", pszSignature);
	free(pszSignature);

	//printf("Request:\r\n%s\r\n", pszRequest);

	return pszRequest;
}


LFMERROR *track_scrobble(LFMSESSION *lfmSession, LFMSCROBBLE *lfmScrobbles, int iNumScrobbles) {
	LFMERROR *pError;
	jsmntok_t tokens[4096];
	char *pszJSON = NULL;

	char *pszReq = createScrobbleString(lfmSession, lfmScrobbles, iNumScrobbles);

	int iTokenCount = doNetRequest(pszReq, &pszJSON, &tokens[0], 4096);

	free(pszReq);

	pError = (LFMERROR *)malloc(sizeof(LFMERROR));

	pError->iError = LFMERROR_NONE;
	pError->pszMessage = NULL;

	// If iTokenCount < 0 then it's an error code for the request
	if (iTokenCount < 0)
		pError->iError = iTokenCount;

	// Parse JSON
	for (int iTk = 1; iTk < iTokenCount; iTk++) {
		if (tokens[iTk].type != JSMN_STRING)
			continue;

		if (tokenCmp(pszJSON, &tokens[iTk], "error")) {
			iTk++;
			pszJSON[tokens[iTk].end] = 0; // Convert to null-terminated string
			pError->iError = atoi(pszJSON+tokens[iTk].start);

		} else if (tokenCmp(pszJSON, &tokens[iTk], "message")) {
			iTk++;
			pszJSON[tokens[iTk].end] = 0; // Convert to null-terminated string

			pError->pszMessage = (char *)malloc(tokens[iTk].end-tokens[iTk].start);
			if (pError->pszMessage == NULL) {
				printf("Out of memory!\r\n");
				freeObject(pError);
				free(pszJSON);
				return NULL;
			}

			strcpy(pError->pszMessage, pszJSON+tokens[iTk].start);
		}
	}

	if (pszJSON != 0)
		free(pszJSON);

	return pError;
}


// Free all the resources allocated in a LFMERROR struct
void freeObject(LFMERROR *pError) {
	if (pError == NULL)
		return;

	if (pError->pszMessage != NULL) {
		free(pError->pszMessage);
	}
	free(pError);
}

// Free all the resources allocated for a LFMSESSION struct
void freeObject(LFMSESSION *pSession) {
	if (pSession == NULL)
		return;

	if (pSession->error.pszMessage != NULL) {
		free(pSession->error.pszMessage);
	} else if (pSession->pszSessionKey != NULL) {
		free(pSession->pszSessionKey);
	} else if (pSession->pszUsername != NULL) {
		free(pSession->pszUsername);
	}
	free(pSession);
}

// Free all the resources allocated for a LFMNOWPLAYING struct
void freeObject(LFMNOWPLAYING *lfmnp) {
	if (lfmnp == NULL)
		return;

	if (lfmnp->error.pszMessage != NULL) {
		free(lfmnp->error.pszMessage);
	} else if (lfmnp->pszAlbum != NULL) {
		free(lfmnp->pszAlbum);
	} else if (lfmnp->pszAlbumArtist != NULL) {
		free(lfmnp->pszAlbumArtist);
	} else if (lfmnp->pszArtist != NULL) {
		free(lfmnp->pszArtist);
	} else if (lfmnp->pszTrack != NULL) {
		free(lfmnp->pszTrack);
	} else if (lfmnp->pszMBID != NULL) {
		free(lfmnp->pszMBID);
	} else if (lfmnp->pszIgnoredMessage != NULL) {
		free(lfmnp->pszIgnoredMessage);
	}
	free(lfmnp);
}

// Free all the resources allocated for a LFMSCROBBLE struct
void freeObject(LFMSCROBBLE *pScrobble) {
	if (pScrobble == NULL)
		return;

	if (pScrobble->error.pszMessage != NULL) {
		free(pScrobble->error.pszMessage);
	} else if (pScrobble->pszAlbum != NULL) {
		free(pScrobble->pszAlbum);
	} else if (pScrobble->pszAlbumArtist != NULL) {
		free(pScrobble->pszAlbumArtist);
	} else if (pScrobble->pszArtist != NULL) {
		free(pScrobble->pszArtist);
	} else if (pScrobble->pszTrack != NULL) {
		free(pScrobble->pszTrack);
	} else if (pScrobble->pszMBID != NULL) {
		free(pScrobble->pszMBID);
	} else if (pScrobble->pszIgnoredMessage != NULL) {
		free(pScrobble->pszIgnoredMessage);
	}
	free(pScrobble);
}

// Free all the resources allocated for a LFMSCROBBLE array
void freeObject(LFMSCROBBLE *pScrobble, int iNumScrobbles) {
	if (pScrobble == NULL)
		return;

	for (int i = 0; i < iNumScrobbles; i++) {
		if (pScrobble[i].error.pszMessage != NULL) {
			free(pScrobble[i].error.pszMessage);
		} else if (pScrobble[i].pszAlbum != NULL) {
			free(pScrobble[i].pszAlbum);
		} else if (pScrobble[i].pszAlbumArtist != NULL) {
			free(pScrobble[i].pszAlbumArtist);
		} else if (pScrobble[i].pszArtist != NULL) {
			free(pScrobble[i].pszArtist);
		} else if (pScrobble[i].pszTrack != NULL) {
			free(pScrobble[i].pszTrack);
		} else if (pScrobble[i].pszMBID != NULL) {
			free(pScrobble[i].pszMBID);
		} else if (pScrobble[i].pszIgnoredMessage != NULL) {
			free(pScrobble[i].pszIgnoredMessage);
		}
	}
	free(pScrobble);
}
