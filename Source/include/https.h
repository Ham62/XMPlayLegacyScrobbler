/* https.h
 *
 * Basic HTTPS library implementing basic GET/POST functionality
 * using Winsocks and OpenSSL
 */
#ifndef _HTTPS_HEADER
#define _HTTPS_HEADER

enum TRANFSER_ENCODING {
	TE_NORMAL, TE_CHUNKED, TE_UNKNOWN
};

struct HTTP_RESPONSE {
	int               iCode;        // HTTP return code (200, 404, etc)
	int               iLength;      // Length of pData
	TRANFSER_ENCODING EncodingType; // Transfer encoding type
	char              *pszHeader;   // HTTP header
	char              *pData;       // content of responce
};

struct HTTPS;
struct HTTP_RESPONSE;

// Init the HTTPS library
int loadHTTPS();

// Establish a HTTPS connection
HTTPS* connectHTTPS(char *szHostname);

// Send POST request to an open connection
int postHTTPS(HTTPS *hHTTPS, char *pszUrl, char *pData, int iDataLen);

// Send GET request to an open connection
int getHTTPS(HTTPS *hHTTPS, char *pszUrl);

// Read the response from an open HTTPS connection
HTTP_RESPONSE *readHTTPS(HTTPS *hHTTPS);

// Close a HTTPS connection
void disconnectHTTPS(HTTPS *hHTTPS);

// Clean up the HTTPS library
void unloadHTTPS();

char *urlEncode(char *pszString);

#endif
