/* https.cpp
 *
 * Basic HTTPS library implementing basic GET/POST functionality
 * using Winsocks and OpenSSL
 */
#include "StdAfx.h"
#include "include\https.h"

#define BUFFER_SIZE 4096

#define INITIAL_SIZE 1024
#define GROW_AMOUNT 512

#define HEADER_WHITESPACE " \t\r\n"

static WSADATA           wsa;
static const SSL_METHOD* SSL_MethTLS;
static SSL_CTX*          SSL_Context;
static char              buffer[BUFFER_SIZE]; // Buffer to send/recieve request

static char pszContent;

static char *szErrorTitle = "XMPlay Legacy Scrobbler -- ERROR!";

struct HTTPS {
	SSL *hSSL;
	char szDomain[255]; // max domain name length is 253 char
};

/*enum TRANFSER_ENCODING {
	TE_NORMAL, TE_CHUNKED, TE_UNKNOWN
};

struct HTTP_RESPONSE {
	int               iCode;        // HTTP return code (200, 404, etc)
	int               iLength;      // Length of pData
	TRANFSER_ENCODING EncodingType; // Transfer encoding type
	char              *pszHeader;   // HTTP header
	char              *pData;       // content of response
};*/


static HTTP_RESPONSE *parseHeader(char *pszHeader);
int processChunked(HTTPS *hHTTPS, HTTP_RESPONSE *pResponse, char *pData, int iDataLen, int iBuffSize);
int processContentLength(HTTPS *hHTTPS, HTTP_RESPONSE *pResponse, char *pData, int iDataLen, int iBuffSize);

char *urlEncode(char *pszSrc) {
	int iSrcLen = strlen(pszSrc);

	int iO = 0;                  // Index in output buffer
	int iBuffSize = iSrcLen+9;   // Output buffer size
	char *pszOut = (char *)malloc(iBuffSize);

	for (int i = 0; i < iSrcLen; i++) {
		// Make sure we have enough buffer space
		if (iO+3 >= iBuffSize) {
			iBuffSize += 9;
			char *pNew = (char *)realloc(pszOut, iBuffSize);
			if (pNew == NULL) {
				puts("Out of memory!");
				free(pszOut);
				return NULL;
			}
			pszOut = pNew;
		}

		// Check if char is vaid or requires encoding
		unsigned char ch = pszSrc[i];
		if ((ch >= '0' && ch <= '9') ||
			(ch >= 'A' && ch <= 'Z') ||
			(ch >= 'a' && ch <= 'z') ||
			 ch == '-' || ch == '.' || 
			 ch == '_' || ch == '~') {

			pszOut[iO++] = ch; // Character does not require encoding

		} else {
			// Convert char to hex code (%20, %1F, etc.)
			sprintf(pszOut+iO, "%%%2.2x", ch);
			iO += 3;
		}

	}

	pszOut = (char *)realloc(pszOut, iO+1); // Trim excess memory
	pszOut[iO] = 0; // Add null terminator to end

	return pszOut;
}

// Initalize the HTTPS library
int loadHTTPS() {
	WORD wVersionRequested;

	wVersionRequested = MAKEWORD(2, 0); // Winsock 2.0

	// Initalize WinSock library
	if (WSAStartup(wVersionRequested, &wsa) != 0) {
		int iErr;
		char szErr[64];

		iErr = WSAGetLastError(); // Could not initalize Winsock library
		sprintf(szErr, "Failed to load Winsock library!\r\nError code %d", iErr);

		MessageBox(NULL, szErr, szErrorTitle, MB_ICONERROR);
		return iErr;
	}

	// Initalize OpenSSL and error strings
	SSL_library_init();
	ERR_load_SSL_strings();

	SSL_MethTLS = TLSv1_2_client_method();
	SSL_Context = SSL_CTX_new(SSL_MethTLS);

	return 0;
}

// Unload the HTTPS library
void unloadHTTPS() {
	SSL_CTX_free(SSL_Context);
	WSACleanup();
}

// Establish a HTTPS connection to server at szHostname on port 443
HTTPS *connectHTTPS(char *szHostname) {
	sockaddr_in sockAddr;
	hostent *host;

	HTTPS *hHTTPS; // HTTPS connection handle
	SOCKET hSock;  // TCP socket handle
	SSL* hSSL;     // OpenSSL connection handle

	int  iErr;       // Winsock error code
	char szErr[128]; // Error messages

	int i;

	// Create a new TCP socket
	if((hSock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
		iErr = WSAGetLastError(); // Could not initalize Winsock library
		sprintf(szErr, "Could not create socket!\r\nError code %d", iErr);

		//MessageBox(NULL, szErr, szErrorTitle, MB_ICONERROR);
		return NULL;
	}

	// Look up hostname
	if ((host = gethostbyname(szHostname)) == NULL) {
		iErr = WSAGetLastError(); // Could not initalize Winsock library
		sprintf(szErr, "Failed to look up hostname!\r\nError code %d", iErr);

		//MessageBox(NULL, szErr, szErrorTitle, MB_ICONERROR);

		closesocket(hSock);
		return NULL;
	}

	// Set up connection info
	sockAddr.sin_family = AF_INET;
	sockAddr.sin_port = htons(443);

	// Attempt to connect to IPs listed in host lookup
	int connected = FALSE;
	for(i = 0; host->h_addr_list[i] != NULL && !connected; i++) {
		// Try next IP
		sockAddr.sin_addr.s_addr = *((unsigned long*)host->h_addr_list[i]);

		//Connect to server
		if (connect(hSock, (sockaddr*)&sockAddr, sizeof(sockAddr)) >= 0) {
			connected = TRUE;
		}
	}
	
	// If couldn't connect on any IP, report error
	if (!connected) {
		iErr = WSAGetLastError(); // Could not initalize Winsock library
		sprintf(szErr, "Could not connect to Last.fm servers!\r\nError code %d", iErr);

		//MessageBox(NULL, szErr, szErrorTitle, MB_ICONERROR);

		closesocket(hSock);
		return NULL;
	}


	// Bind socket through OpenSSL
	hSSL = SSL_new(SSL_Context);
	SSL_set_fd(hSSL, hSock);
	SSL_set_tlsext_host_name(hSSL, szHostname);

	if (SSL_connect(hSSL) < 0) {
		//MessageBox(NULL, "Failed to secure connection!", szErrorTitle, MB_ICONERROR);

		SSL_free(hSSL);
		closesocket(hSock);
		return NULL;
	}

	// Copy host name into struct
	hHTTPS = (HTTPS *)malloc(sizeof(HTTPS));
	strcpy(hHTTPS->szDomain, szHostname);

	hHTTPS->hSSL = hSSL;

	return hHTTPS;
}

// Close an open HTTPS connection and frees the struct
void disconnectHTTPS(HTTPS *hHTTPS) {
	SOCKET hSock;
	SSL *hSSL = hHTTPS->hSSL;

	hSock = SSL_get_fd(hSSL);
	SSL_free(hSSL);
	closesocket(hSock);
	free(hHTTPS);
}

// Send a POST request to an open HTTPS connection
int postHTTPS(HTTPS *hHTTPS, char *pszUrl, char *pData, int iDataLen) {
	int iSz;

	iSz = sprintf(buffer, 
			"POST %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Connection: keep-alive\r\n"
			"Content-Length: %d\r\n"
			"\r\n", 
			pszUrl, hHTTPS->szDomain, iDataLen);

	// Send request header
	if (SSL_write(hHTTPS->hSSL, buffer, iSz) <= 0) {
		puts("Post send failed");
		return -1;
	}

	// Send request data
	if (SSL_write(hHTTPS->hSSL, pData, iDataLen) <= 0) {
		puts("Data send failed");
		return -1;
	}

	return 0;
}

// Send a GET request to an open HTTPS connection
int getHTTPS(HTTPS *hHTTPS, char *pszUrl) {
	int iSz;

	iSz = sprintf(buffer, 
			"GET %s HTTP/1.1\r\n"
			"Host: %s\r\n"
			"Connection: keep-alive\r\n"
			"\r\n", 
			pszUrl, hHTTPS->szDomain);

	// Send request header
	if (SSL_write(hHTTPS->hSSL, buffer, iSz) <= 0) {
		puts("Get request failed");
		return -1;
	}

	return 0;
}

// Read the response from an open HTTPS connection
HTTP_RESPONSE *readHTTPS(HTTPS *hHTTPS) {
	int iRead, iTotalRead;

	char *pEnd;      // Marks end of HTTP header
	char *pszHeader; // Buffer to store HTTP header
	char *pData;     // Buffer to store response data
	char *pNew;      // Temp pointer for resizing data

	int iHeaderLen;  // Size of pszHeader
	int iBufferSize; // Size of pResponse
	int iDataLen;    // Length of data in pResponse

	HTTP_RESPONSE *pResponse; // pointer to response struct

	// Initalize response buffer
	iBufferSize = INITIAL_SIZE;
	pData = (char *)malloc(iBufferSize);

	iTotalRead = 0; iDataLen = 0;

	pEnd = NULL; // Find end of header and copy it into a buffer
	while (pEnd == NULL) {
		// Read the HTTP header
		iRead = SSL_read(hHTTPS->hSSL, &buffer, BUFFER_SIZE-1);
		if (iRead <= 0) {
			puts("Disconnected from server!");
			// Disconnected
			free(pData);
			return NULL;
		}

		iTotalRead += iRead; // New data size

		// Check if we need to grow our response buffer
		if (iTotalRead > iBufferSize) {
			iBufferSize = iTotalRead + GROW_AMOUNT;  // New buffer size
			pNew = (char *)realloc(pData, iBufferSize);

			if (pNew == NULL) {
				MessageBox(NULL, "Out of memory!", szErrorTitle, MB_ICONERROR);
				free(pData);
				return NULL;
			}

			pData = pNew; // Resize successful
		}

		// Copy data over and check for header end
		memcpy(pData+iDataLen, buffer, iRead);
		iDataLen = iTotalRead;

		pData[iTotalRead] = 0; // Add a null terminator for strstr search
		pEnd = strstr(pData, "\r\n\r\n"); // Check for end of HTTP header

	}

	// Allocate buffer and copy HTTP header over
	iHeaderLen = pEnd-pData;
	pszHeader = (char *)malloc(iHeaderLen+1);
	memcpy(pszHeader, pData, iHeaderLen);

	pszHeader[iHeaderLen] = 0; // Null terminator for end of header

	//printf("HTTP Header:\r\n%s\r\n", pszHeader);


	// Move data up to start of data buffer
	strcpy(pData, pEnd+4);  // "\r\n\r\n" is 4 bytes, start cpy after
	iDataLen -= iHeaderLen+4; // Adjust data size

	// Parse the HTTP header
	pResponse = parseHeader(pszHeader);
	switch (pResponse->EncodingType) {
	case TE_CHUNKED:
		processChunked(hHTTPS, pResponse, pData, iDataLen, iBufferSize);
		break;

	case TE_NORMAL:
		processContentLength(hHTTPS, pResponse, pData, iDataLen, iBufferSize);
		break;

	case TE_UNKNOWN:
		MessageBox(NULL, "Unsupported Encoding-Type from server!", szErrorTitle, MB_ICONERROR);
		free(pszHeader);
		free(pData);
		return NULL;
		break;
	}
	
	return pResponse;
}

// Read the HTTP header
static HTTP_RESPONSE *parseHeader(char *pszHeader) {
	HTTP_RESPONSE *pResponse; // Response struct
	char *pTok;               // For tokenizing header
	int i, iTokLen;

	// Allocate response buffer
	pResponse = (HTTP_RESPONSE *)malloc(sizeof(HTTP_RESPONSE));
	pResponse->EncodingType = TE_UNKNOWN;
	pResponse->iLength = 0;
	pResponse->pszHeader = pszHeader;

	// First parameter is "HTTP/1.1 200 OK" line
	pTok = strtok(pszHeader, "\r\n");
	iTokLen = strlen(pTok);

	// Get HTTP response code
	for (i = 0; i < iTokLen && pTok[i] != ' '; i++);
	pResponse->iCode = (int)strtol(pTok+i+1, NULL, 10);

	// Read other parameters
	pTok = strtok(NULL, "\r\n");
	while (pTok != NULL) {
		// Determine which parameter this is
		iTokLen = strlen(pTok);
		for (i = 0; i < iTokLen && pTok[i] != ':'; i++);
		pTok[i] = 0; // Null teminator in place of ':'

		// Get Transfer-Encoding type
		if (strcmpi(pTok, "Transfer-Encoding") == 0) {
			if (strcmpi(pTok+i+1, "chunked")) {
				pResponse->EncodingType = TE_CHUNKED;
			} else {
				pResponse->EncodingType = TE_UNKNOWN;
			}

		// Get content length
		} else if (strcmpi(pTok, "Content-Length") == 0) {
			pResponse->iLength = (int)strtol(pTok+i+1, NULL, 10);
			pResponse->EncodingType = TE_NORMAL;
		}

		// Get next token to check
		pTok = strtok(NULL, "\r\n");
	}

	return pResponse;
}

// Find and read chunk header, returns next chunk size
// pData     - Data buffer to return overhanging data
// iDataLen  - Length of data in pData
// iBuffSize - Total size of pData buffer
int readChunkHeader(HTTPS *hHTTPS, char *pData, int iDataLen, int iBuffSize) {

	int iHeaderSz;    // Size of chunk header (bytes)
	int iCnkSize;     // Size of chunk after header
	int iRead;        // How many bytes read from socket
	char *pEnd;       // Pointer to end of header

	pData[iDataLen] = 0;
	pEnd = strstr(pData, "\r\n");

	// Loop until end is found
	while (pEnd == NULL) {

		iRead = SSL_read(hHTTPS->hSSL, &buffer, BUFFER_SIZE-1);
		if (iRead <= 0) {
			puts("Disconnected from server!");
			// Disconnected
			free(pData);
			return NULL;
		}

		// Check if pData needs to be resized
		if (iDataLen+iRead > iBuffSize) {
			char *pNew = (char *)realloc(pData, iDataLen+iRead+1);
			if (pNew == NULL) {
				MessageBox(NULL, "Out of memory!", szErrorTitle, MB_ICONERROR);
				free(pData);
				return -1;
			}

			// Buffer is now resized for new data
			iBuffSize = iDataLen + iRead;
			pData = pNew;
		}

		// Copy newly read data into buffer
		memcpy(pData+iDataLen, &buffer, iRead);
		
		// Check for header end again
		pData[iDataLen] = 0;
		pEnd = strstr(pData, "\r\n");
	}


	// Get size of next chunk
	iCnkSize = (int)strtol(pData, NULL, 16);

	// Calculate length of header
	iHeaderSz = (pEnd-pData);

	// Shift data to get rid of header
	iDataLen -= iHeaderSz+2;   // the +2 is the \r\n
	memcpy(pData, pEnd+2, iDataLen);

	printf("Chunk Header Size: %d\r\n", iCnkSize);
	printf("Data after header: %d\r\n", iDataLen);

	return iCnkSize; // Return chunk size
}

int processChunked(HTTPS *hHTTPS, HTTP_RESPONSE *pResponse, char *pData, int iDataLen, int iBuffSize) {
	int iRead;
	int iChunkSize; // How big is the next chunk
	int doneTransfer = FALSE;

	int iFullLen = 0;
	int iFullBuffLen = 0;
	char *pFullData = NULL;

	while (!doneTransfer) {
		// Read chunk header
		iChunkSize = readChunkHeader(hHTTPS, pData, iDataLen, iBuffSize);

		if (iChunkSize == 0)  // When chunk size is 0 there is no more data
			doneTransfer = TRUE;

		// Read next chunk
		while (iDataLen < iChunkSize) {
			iRead = SSL_read(hHTTPS->hSSL, &buffer, BUFFER_SIZE-1);

			// Check if pData needs to be resized
			if (iDataLen+iRead > iBuffSize) {
				char *pNew = (char *)realloc(pData, iDataLen+iRead+1);
				if (pNew == NULL) {
					MessageBox(NULL, "Out of memory!", szErrorTitle, MB_ICONERROR);
					free(pFullData);
					free(pData);
					return -1;
				}

				// Buffer is now resized for new data
				iBuffSize = iDataLen + iRead;
				pData = pNew;
			}

			// Append data to buffer
			memcpy(pData+iDataLen-5, &buffer, iRead);
			iDataLen += iRead;
		}

		// Make sure we have room in the new buffer
		if (iFullBuffLen < iFullLen+iChunkSize) {
			char *pNew = (char *)realloc(pFullData, iFullLen+iChunkSize+1);
			if (pNew == NULL) {
				MessageBox(NULL, "Out of memory!", szErrorTitle, MB_ICONERROR);
				free(pFullData);
				free(pData);
				return -1;
			}

			// Buffer is now resized for new data
			iFullBuffLen = iFullLen + iChunkSize;
			pFullData = pNew;
		}

		// Copy data to full buffer
		memcpy(pFullData+iFullLen, pData, iChunkSize);
		iFullLen += iChunkSize;

		// Move trailing data to the front of pData
		iDataLen -= iChunkSize;
		memcpy(pData, pData+iChunkSize, iDataLen);
	}
	
	free(pData);

    if (pFullData != NULL) {
        pFullData = (char *)realloc(pFullData, iFullLen); // Shrink data buffer to remove unused space
        pFullData[iFullLen] = 0;              // Add a null terminator to end of data
    }
    
	// Return data in struct
	pResponse->iLength = iFullLen;
	pResponse->pData = pFullData;

	return 0;
}


// Read data when it's being served using Content-Length
int processContentLength(HTTPS *hHTTPS, HTTP_RESPONSE *pResponse, char *pData, int iDataLen, int iBufferSize) {
	int iRead;
	char *pNew;

	// Resize buffer to be exact length we need
	pNew = (char *)realloc(pData, pResponse->iLength+1);
	if (pNew == NULL) {
		MessageBox(NULL, "Out of memory!", szErrorTitle, MB_ICONERROR);
		free(pData);
		return -1;
	}

	pData = pNew;

	// Read response content
	while (iDataLen < pResponse->iLength) {
		// Read more of the data
		iRead = SSL_read(hHTTPS->hSSL, &buffer, BUFFER_SIZE-1);
		if (iRead <= 0) {
			puts("Disconnected from server!");
			// Disconnected
			free(pData);
			return -2;
		}

		// Copy data over and check for header end
		memcpy(pData+iDataLen, buffer, iRead);
		iDataLen += iRead; // New data size

	}
	pData[pResponse->iLength] = 0; // Add a null terminator to end of data

	pResponse->pData = pData;

	return 0;
}



