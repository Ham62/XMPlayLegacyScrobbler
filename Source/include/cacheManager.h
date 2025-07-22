/* cacheManager.h
 *
 * Manage saving and loading cached scrobbles to disk.
 */
#ifndef _CACHE_MANEGER_HEADER
#define _CACHE_MANEGER_HEADER

#include "include/lfm.h"

#define CACHE_FILE_VERSION 1

void cache_saveScrobble(LFMSCROBBLE *lfmScrobble, char *pszCacheFile);

int cache_loadScrobbles(LFMSCROBBLE **ppScrobbles, char *pszCacheFile);

void cache_clearScrobbles(char *pszCacheFile);

#endif
