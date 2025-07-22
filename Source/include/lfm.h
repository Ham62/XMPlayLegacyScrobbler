/* lfm.h
 * 
 * Implements scrobble-related Last.fm API requests via the https library
 * 
 * Written to conform to the Scrobble 2.0 API documentation
 * https://www.last.fm/api/scrobbling
 *
 * Requires JSMN for JSON parsing. Uses Wincrypt for MD5 hashing
 */
#ifndef _LASTFM_HEADER
#define _LASTFM_HEADER

#include <windows.h>

#define KEY_LENGTH 32
#define SECRET_LENGTH 32

typedef struct LFMERROR {
	int  iError;
	char *pszMessage;
} lfmerror;

typedef struct LFMSESSION {
	LFMERROR error;

	char *pszUsername;
	char *pszSessionKey;
	int iSubscriber;

	char szAPIKey[KEY_LENGTH+1];
	char szSecret[SECRET_LENGTH+1];
} lfmsession;


typedef struct LFMNOWPLAYING {
	LFMERROR error;

	char *pszArtist;
	char *pszTrack;
	char *pszAlbum;
	int  iTrackNumber;
	char *pszMBID;
	int  iDuration;
	char *pszAlbumArtist;

	char *pszIgnoredMessage;
} lfmnowplaying;

typedef struct LFMSCROBBLE {
	LFMERROR error;

	char *pszArtist;
	char *pszTrack;

	LONGLONG llTimestamp;

	char *pszAlbum;
	int  iTrackNumber;
	char *pszMBID;
	int  iDuration;
	char *pszAlbumArtist;

	char *pszIgnoredMessage;
} lfmscrobble;

// Official last.fm API error codes
#define LFMERROR_NONE              0
#define LFMERROR_UNAVAILABLE       16
#define LFMERROR_INVALID_SERVICE   2
#define LFMERROR_INVALID_METHOD    3
#define LFMERROR_AUTH_FAILED       4
#define LFMERROR_INVALID_FORMAT    5
#define LFMERROR_INVALID_PARAMS    6
#define LFMERROR_INVALID_RESOURCE  7
#define LFMERROR_OPERATION_FAILED  8
#define LFMERROR_INVALID_SESSION   9
#define LFMERROR_INVALID_KEY       10
#define LFMERROR_SERVICE_OFFLINE   11
#define LFMERROR_INVALID_SIGNATURE 13
#define LFMERROR_SUSPENDED_KEY     26
#define LFMERROR_RATE_LIMITED      29

// Library-defined error codes

// Network error occured completing request
#define LFMERROR_NETWORK  -10

// Parser error occured reading API response
// This is either programmer error, or last.fm is broken
#define LFMERROR_RESPONSE -20

void LFMInit();
void LFMCleanup();

void freeObject(LFMERROR *pError);
void freeObject(LFMSESSION *pSession);
void freeObject(LFMNOWPLAYING *lfmnp);
void freeObject(LFMSCROBBLE *lfmScrobble);
void freeObject(LFMSCROBBLE *lfmScrobble, int iNumScrobbles);

LFMSESSION *auth_getMobileSession(char *pszAPIKey, char *pszUsername, char *pszPassword, char *pszSecret);

LFMERROR *track_updateNowPlaying(LFMSESSION *lfmSession, LFMNOWPLAYING *lfmnp);
LFMERROR *track_scrobble(LFMSESSION *lfmSession, LFMSCROBBLE *lfmScrobble);
LFMERROR *track_scrobble(LFMSESSION *lfmSession, LFMSCROBBLE *lfmScrobble, int iNumScrobbles);

#endif

