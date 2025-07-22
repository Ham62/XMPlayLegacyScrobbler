/* cacheManager.cpp
 *
 * Manage saving and loading cached scrobbles to disk.
 */
#include "include/cacheManager.h"

#include <stdio.h>

void writeMember(char *pszMember, FILE *hFile) {
	int iLen = 0;
	if (pszMember != NULL) {
		iLen = strlen(pszMember);
		fwrite(&iLen, sizeof(int), 1, hFile);
		fwrite(pszMember, iLen, 1, hFile);
	} else {
		fwrite(&iLen, sizeof(int), 1, hFile);
	}
}

FILE *createCacheFile(char *pszCacheFile) {
	int iCacheVersion = CACHE_FILE_VERSION;
	int iCachedCount = 1;
	FILE *hCache;

	hCache = fopen(pszCacheFile, "w+b");

	// Write new cache version and number of new cached scrobbles
	fwrite(&iCacheVersion, sizeof(int), 1, hCache);
	fwrite(&iCachedCount, sizeof(int), 1, hCache);

	return hCache;
}

void cache_saveScrobble(LFMSCROBBLE *lfmScrobble, char *pszCacheFile) {
	int iCacheVersion = 0;
	int iCachedCount = 0;

	FILE *hCache = fopen(pszCacheFile, "r+b");

	// If cache file doesn't exist, make it!
	if (hCache == NULL) {
		hCache = createCacheFile(pszCacheFile);
		rewind(hCache);
	}

	// Make sure cache file version is compatible
	fread(&iCacheVersion, sizeof(int), 1, hCache);

	// If incompatible, overwrite with new cache
	if (iCacheVersion != CACHE_FILE_VERSION) {
		fclose(hCache);

		puts("Cache file version mismatch! Discarding old cache...");
		hCache = createCacheFile(pszCacheFile);

	} else {
		// Get total number of previously cached scrobbles
		fread(&iCachedCount, sizeof(int), 1, hCache);
		fseek(hCache, -(int)sizeof(int), SEEK_CUR);

		// Update number of cached scrobbles
		iCachedCount += 1;
		fwrite(&iCachedCount, sizeof(int), 1, hCache);

		// Seek to end of file to append data
		fseek(hCache, 0, SEEK_END);
	}

	// Write scrobble to file
	writeMember(lfmScrobble->pszArtist, hCache);
	writeMember(lfmScrobble->pszTrack, hCache);

	fwrite(&lfmScrobble->llTimestamp, sizeof(lfmScrobble->llTimestamp), 1, hCache);

	writeMember(lfmScrobble->pszAlbum, hCache);
	fwrite(&lfmScrobble->iTrackNumber, sizeof(lfmScrobble->iTrackNumber), 1, hCache);
	writeMember(lfmScrobble->pszMBID, hCache);
	fwrite(&lfmScrobble->iDuration, sizeof(lfmScrobble->iDuration), 1, hCache);
	writeMember(lfmScrobble->pszAlbumArtist, hCache);

	fclose(hCache);
}

char *readString(FILE *hFile) {
	int iLength = 0;
	char *pszValue = NULL;

	// Get length of the string
	fread(&iLength, sizeof(int), 1, hFile);
	if (iLength > 0) {
		// Read the string
		pszValue = (char *)malloc(iLength+1);
		fread(pszValue, iLength, 1, hFile);
		pszValue[iLength] = 0;
	}
	return pszValue;
}

int cache_loadScrobbles(LFMSCROBBLE **ppScrobbles, char *pszCacheFile) {
	int iCacheVersion = 0; 
	int iCachedCount = 0;

	FILE *hCache = fopen(pszCacheFile, "rb");
	if (hCache == NULL)
		return 0; // No cache file exists

	// Check cache version first (Also quits on empty file)
	fread(&iCacheVersion, sizeof(int), 1, hCache);
	if (iCacheVersion != CACHE_FILE_VERSION) {
		//puts("Incompatible cache file version! Skipping...");
		return 0;
	}

	// Check how many scrobbles we're loading
	fread(&iCachedCount, sizeof(int), 1, hCache);
	if (iCachedCount == 0)
		return 0;

	LFMSCROBBLE *pScrobbles = (LFMSCROBBLE *)malloc(sizeof(LFMSCROBBLE)*iCachedCount);
	memset(pScrobbles, 0, sizeof(LFMSCROBBLE)*iCachedCount);

	// Read the scrobbles in
	for (int i = 0; i < iCachedCount; i++) {
		pScrobbles[i].pszArtist = readString(hCache);
		pScrobbles[i].pszTrack = readString(hCache);

		fread(&pScrobbles[i].llTimestamp, sizeof(pScrobbles[i].llTimestamp), 1, hCache);

		pScrobbles[i].pszAlbum = readString(hCache);
		fread(&pScrobbles[i].iTrackNumber, sizeof(pScrobbles[i].iTrackNumber), 1, hCache);
		pScrobbles[i].pszMBID = readString(hCache);
		fread(&pScrobbles[i].iDuration, sizeof(pScrobbles[i].iDuration), 1, hCache);
		pScrobbles[i].pszAlbumArtist = readString(hCache);

	}

	*ppScrobbles = pScrobbles;

	return iCachedCount;
}

// Wipe the cache file
void cache_clearScrobbles(char *pszCacheFile) {
	fclose(fopen(pszCacheFile, "w"));
	//fclose(createCacheFile(pszCacheFile));
}

