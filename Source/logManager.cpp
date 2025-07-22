/* logManager.cpp
 *
 * Basic file logging library. Enables specifying file size limits.
 * Log writes are done in a thread-safe manor
 */
#include "include/logManager.h"

char *pszFilePath    = NULL;
bool  blEnableLogging = false;
bool  blSetSizeLimit  = false;
int   iMaxLogSize     = 4096;

CRITICAL_SECTION csCriticalSection;

// Enable/disable file logging
void log_enable(bool blEnable) {
	if (blEnable && !blEnableLogging)
		InitializeCriticalSection(&csCriticalSection);
	else if (!blEnable)
		DeleteCriticalSection(&csCriticalSection);

	blEnableLogging = blEnable;
}

// Specify output log file
void log_setFile(char *pszNewLogFile) {
	if (pszFilePath != NULL)
		free(pszFilePath);

	pszFilePath = pszNewLogFile;
}

// Enable/disable size limit on log file
void log_enableSizeLimit(bool blEnable) {
	blSetSizeLimit = blEnable;
}

// Specify maximum size of log file
int log_setMaxSize(int iSize) {
	if (iSize < MIN_LOG_SIZE && iSize != 0)
		return false;

	iMaxLogSize = iSize;
	return true;
}

// Log messae to file. Works in a thread-safe manor
void log_write(const char *format, ...) {
	SYSTEMTIME st;
	FILE *pLogFile;
	char *pszOut;
	int   iStrLen;
	int   iFileLen;

	if (!blEnableLogging || pszFilePath == NULL)
		return;

	va_list args;
	va_start(args, format);

	pszOut = (char *)malloc(4096);
	// Write timestamp to formatted buffer
	GetLocalTime(&st);
	iStrLen = sprintf(pszOut, "[%d-%02d-%02d %02d:%02d:%02d]\t", st.wYear, st.wMonth, st.wDay,
																 st.wHour, st.wMinute, st.wSecond);
	// Append formatted string to formatted buffer
	iStrLen = vsprintf(pszOut+iStrLen, format, args)+iStrLen;

	// Synchronize writes so we don't get any log corruption
	EnterCriticalSection(&csCriticalSection);

	pLogFile = fopen(pszFilePath, "r+");

	// If above open fails file doesn't exist. Make it
	if (pLogFile == NULL)
		pLogFile = fopen(pszFilePath, "w");

	// Get file length
	fseek(pLogFile, 0, SEEK_END);
	iFileLen = ftell(pLogFile);

	// Check if we can append or need to truncate top of file
	if (blSetSizeLimit && iMaxLogSize > 0 && iFileLen+iStrLen > iMaxLogSize) {
		char *pszLogFile = (char *)malloc(iMaxLogSize);

		// Only copy enough from end of file to fit within size limit
		fseek(pLogFile, -iMaxLogSize+iStrLen+1, SEEK_END);

		// Read log file into memory and append new entry
		int iLen = fread(pszLogFile, 1, iMaxLogSize, pLogFile);
		strcpy(pszLogFile+iLen, pszOut);

		// Start the log file at the first full line so we don't truncate just half of one
		char *pszLineStart = strstr(pszLogFile, "\n");
		if (pszLineStart == NULL)
			pszLineStart = pszLogFile;
		else
			pszLineStart++;

		// Clear file and write truncated log
		freopen(pszFilePath, "w", pLogFile);

		// Write to disk
		fprintf(pLogFile, pszLineStart);

		free(pszLogFile);

	} else {
		fprintf(pLogFile, pszOut);
	}

	fclose(pLogFile);
	LeaveCriticalSection(&csCriticalSection);

	free(pszOut);
	va_end(args);
}
