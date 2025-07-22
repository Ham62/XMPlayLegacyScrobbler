/* logManager.h
 *
 * Basic file logging library. Enables specifying file size limits.
 * Written so log writes are done in a thread-safe manor
 */
#ifndef _LOG_MANEGER_HEADER
#define _LOG_MANEGER_HEADER

#include <stdarg.h>
#include <stdio.h>
#include <windows.h>

#define MIN_LOG_SIZE 1024

void log_enable(bool blEnable);
void log_enableSizeLimit(bool blEnable);

void log_setFile(char *pszNewLogFile);
int  log_setMaxSize(int iSize);

void log_write(const char *pszFormat, ...);

#endif
