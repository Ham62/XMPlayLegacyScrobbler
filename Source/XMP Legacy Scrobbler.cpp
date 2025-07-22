/* XMP Legacy Scrobbler.cpp : Defines the entry point for the DLL application.
 *
 */

#include <cstdlib>
#include <stdarg.h>
#include <process.h>

#include "apikeys.h"
#include "stdafx.h"
#include "xmp-sdk/xmpdsp.h"
#include "include/lfm.h"
#include "include/cacheManager.h"
#include "include/logManager.h"
#include "Resource.h"

#define SHOW_DEBUG 0 /* Opens a console window for realtime debugging */

// Plugin version number
#define VER_MAJ   1
#define VER_MIN   0
#define VER_PATCH 0
#define CR_YEAR   2025

#define MAX_USERNAME_LENGTH 29
#define MAX_PASSWORD_LENGTH 64

#define CACHE_FILE_NAME "legacyscrobbler.cache"
#define LOG_FILE_NAME   "legacyscrobbler.log"

// Imported XMPlay function pointers
static XMPFUNC_MISC   *xmpfmisc;
static XMPFUNC_STATUS *xmpfstatus;

static HINSTANCE hInstance;
static HWND hWndConf = NULL;

static unsigned int idNetHelperThread = 0;
static HANDLE hNetHelperThread = NULL;

static bool blLoggedWelcomeMsg   = FALSE;
static bool blLoggedScrobbleTime = FALSE;

// File path to cache file
static char szCacheFile[256];
// File path to log file
static char szLogFile[256];

// Functions We pass for XMPlay to call from us
static void WINAPI        DSP_About(HWND win);
static void *WINAPI       DSP_New(void);
static void WINAPI        DSP_Free(void *inst);
static const char *WINAPI DSP_GetDescription(void *inst);
static void WINAPI        DSP_Config(void *inst, HWND hWnd);
static DWORD WINAPI       DSP_GetConfig(void *inst, void *config);
static BOOL WINAPI        DSP_SetConfig(void *inst, void *config, DWORD size);
static void WINAPI        DSP_NewTrack(void *inst, const char *file);
static void WINAPI        DSP_SetFormat(void *inst, const XMPFORMAT *form);
static void WINAPI        DSP_Reset(void *inst);
static DWORD WINAPI       DSP_Process(void *inst, float *buffer, DWORD count);
static void WINAPI        DSP_NewTitle(void *inst, const char *title);

bool lfmLogin(HWND hWnd);
void updateLoginUI(HWND hWnd, int blClearCredentials = FALSE);
void enableLogSettings(HWND hWnd, bool blEnable);
void populateComboBox(HWND hWnd);

typedef struct {
	bool blAuthenticated;    // Authenticated with last.fm servers?
	char szUsername[MAX_USERNAME_LENGTH+1];
	char szSessionKey[40];   // Store session key not password

	bool blEnableScrobble;   // Register scrobbles on last.fm?
	bool blEnableNowPlaying; // Register <Now Playing> on last.fm?

	bool blEnableLog;        // Enable logging to local file?
	bool blLogSizeLimit;     // Enable log size limit?
	int  iLogSizeLimit;      // Log size limit (bytes)
	int  iLogLimitSelection; // Index of selected limit (for UI)
} CONFIG;

static CONFIG conf;

// Struct of function pointers and our plugin name
static XMPDSP dsp = {
	XMPDSP_FLAG_TITLE, // Request track title change notifications
	"XMPlay Legacy Scrobbler",
	DSP_About,
	DSP_New,
	DSP_Free,
	DSP_GetDescription,
	DSP_Config,
	DSP_GetConfig,
	DSP_SetConfig,
	DSP_NewTrack,
	DSP_SetFormat,
	DSP_Reset,
	DSP_Process,
	DSP_NewTitle,
};

// Thread messages for communicating with NetHelperThread
enum net_thread_messages {
	NTM_UPDATENOWPLAYING = WM_USER+1,
	NTM_SCROBBLE,
	NTM_SCROBBLE_CACHED
};

// Parse the data in an LFMERROR struct and log it
void logError(LFMERROR *pError) {
	//printf("track.updateNowPlaying - Error: %d\n", pError->iError);

	if (pError->pszMessage != NULL) {
		log_write("Last.fm error %d: %s\n", pError->iError, pError->pszMessage);
		printf("Message: %s\n", pError->pszMessage);
	} else {
		switch (pError->iError) {
			case LFMERROR_NETWORK:
				log_write("A network error occured. Please check connection.\n");
				puts("Network error occured! Check connection!");
				break;

			case LFMERROR_RESPONSE:
				log_write("Parser error reading last.fm response! Either last.fm broke or something is very wrong!\n");
				puts("Parser error reading response from last.fm!");
				break;

		}
	}

}

// Thread to handle all network requests so that we don't hang
// the main XMPlay thread
unsigned __stdcall NetHelperThread (void *pParams) {
	MSG msg;

	puts("Helper thread starting!");

	while (GetMessage(&msg, NULL, 0, 0)) {
		LFMERROR *pError = NULL;

		LFMSESSION *pSession = NULL;
		LFMNOWPLAYING *plfmnp = NULL;
		LFMSCROBBLE *pScrobble = NULL;
		int iCount = 0;

		switch (msg.message) {
			case NTM_UPDATENOWPLAYING:
				pSession = (LFMSESSION *)(msg.wParam);
				plfmnp = (LFMNOWPLAYING *)(msg.lParam);

				pError = track_updateNowPlaying(pSession, plfmnp);

				if (pError != NULL && pError->iError != LFMERROR_NONE) {
					logError(pError);
				} else {
					log_write("updateNowPlaying successfull!\n");
					puts("updateNowPlaying successfull!");
				}

				freeObject(pSession);
				freeObject(plfmnp);
				freeObject(pError);
				break;

			case NTM_SCROBBLE:
				pSession = (LFMSESSION *)(msg.wParam);
				pScrobble = (LFMSCROBBLE *)(msg.lParam);
				
				pError = track_scrobble(pSession, pScrobble);

				if (pError != NULL && pError->iError != LFMERROR_NONE) {
					if (pError->iError == LFMERROR_NETWORK) {
						log_write("Scrobble failed... cached for later.\n");
						// Network error, cache scrobble for later
						cache_saveScrobble(pScrobble, szCacheFile);
					} else {
						logError(pError);
					}
					freeObject(pSession);

				} else {
					log_write("Track scrobbled successfully!\n");
					puts("Scrobbled successfully!");

					// Since the scrobble went through, check if we can send any cached scrobbles
					PostThreadMessage(idNetHelperThread, NTM_SCROBBLE_CACHED, (WPARAM)pSession, 0);
				}

				freeObject(pScrobble);
				freeObject(pError);
				break;

			case NTM_SCROBBLE_CACHED:
				pSession = (LFMSESSION *)(msg.wParam);

				iCount = cache_loadScrobbles(&pScrobble, szCacheFile);
				if (iCount > 0) {
					pError = track_scrobble(pSession, pScrobble, iCount);
					if (pError != NULL && pError->iError != LFMERROR_NONE) {
						if (pError->iError == LFMERROR_NETWORK) {
							// Network error, cache scrobble for later
							cache_saveScrobble(pScrobble, szCacheFile);
						} else {
							logError(pError);
						}
					} else {
						log_write("%d track(s) submitted from cache!\n", iCount);
						puts("Cached tracks scrobbled!");
						cache_clearScrobbles(szCacheFile);
					}
				}

				freeObject(pScrobble, iCount);
				freeObject(pSession);
				freeObject(pError);
				break;

		}
	}

	puts("Helper thread terminating...");

	_endthreadex(0);
	return 0;
}

// https://stackoverflow.com/questions/20370920/convert-current-time-from-windows-to-unix-timestamp-in-c-or-c
LONGLONG GetSystemTimeAsUnixTime() {
	#define UNIX_TIME_START 0x019DB1DED53E8000
	#define	TICKS_PER_SECOND 10000000

	LONGLONG llUnixTime;
	FILETIME ft;

	GetSystemTimeAsFileTime(&ft);

	llUnixTime = ft.dwLowDateTime | ((LONGLONG)(ft.dwHighDateTime) << 32);
	llUnixTime = (llUnixTime - UNIX_TIME_START) / TICKS_PER_SECOND;

	return llUnixTime;
}

// Create new disposable LFMSESSION for an API request
LFMSESSION *getSession() {
	if (!conf.blAuthenticated) 
		return NULL; // Not authenticated with last.fm

	// Initialize session struct
	LFMSESSION *pSession = (LFMSESSION *)malloc(sizeof(LFMSESSION));
	memset(pSession, 0, sizeof(LFMSESSION));

	pSession->pszUsername = (char *)malloc(strlen(conf.szUsername)+1);
	pSession->pszSessionKey = (char *)malloc(strlen(conf.szSessionKey)+1);

	strcpy(pSession->pszUsername, conf.szUsername);
	strcpy(pSession->pszSessionKey, conf.szSessionKey);

	strcpy(pSession->szAPIKey, pszAPIKey);
	strcpy(pSession->szSecret, pszSecret);

	return pSession;
}

void applySettings() {
	log_enable(conf.blEnableLog);
	log_enableSizeLimit(conf.blLogSizeLimit);
	log_setMaxSize(conf.iLogSizeLimit);
}

#define MESS(_id, _m, _w, _l) SendDlgItemMessage(hWnd, _id, _m, (WPARAM)(_w), (LPARAM)(_l))
static BOOL CALLBACK DSPDialogProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam) {
	static HDC hdc;
	static HBRUSH hBrushRed = CreateSolidBrush(RGB(255, 55, 55));
	static HBRUSH hBrushGreen = CreateSolidBrush(RGB(0, 255, 0));
	bool blClearCredentials = FALSE;

	switch (iMsg) {
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDOK: {
					// If focus is on username or password box, don't close dialog
					int ctrlID = GetDlgCtrlID(GetFocus());
					if (ctrlID == IDC_EDITUSER || ctrlID == IDC_EDITPASS)
						break;

					// Confirm and save settings
					conf.blEnableScrobble =   (BST_CHECKED == MESS(IDC_ENABLESCROBBLE, BM_GETCHECK, 0, 0));
					conf.blEnableNowPlaying = (BST_CHECKED == MESS(IDC_ENABLENP, BM_GETCHECK, 0, 0));
					conf.blEnableLog =        (BST_CHECKED == MESS(IDC_ENABLELOG, BM_GETCHECK, 0, 0));
					conf.blLogSizeLimit =     (BST_CHECKED == MESS(IDC_LIMITLOG, BM_GETCHECK, 0, 0));

					conf.iLogLimitSelection = MESS(IDC_LOGSIZE, CB_GETCURSEL, 0, 0);
					conf.iLogSizeLimit =      MESS(IDC_LOGSIZE, CB_GETITEMDATA, conf.iLogSizeLimit, 0);

					applySettings();
				}

				case IDCANCEL:
					EndDialog(hWnd, 0);
					break;

				case IDC_ENABLELOG:
					enableLogSettings(hWnd, (BST_CHECKED == MESS(IDC_ENABLELOG, BM_GETCHECK, 0, 0)));
					break;

				// Login button
				case IDC_LOGIN:
					blClearCredentials = FALSE;

					// If logged in, log out
					if (conf.blAuthenticated) {
						// Delete authentication info
						memset(conf.szSessionKey, 0, sizeof(conf.szSessionKey));
						memset(conf.szUsername, 0, sizeof(conf.szUsername));
						conf.blAuthenticated = false;
						blClearCredentials = TRUE; // Clear credentials when logged out

					// else try to log in
					} else {
						lfmLogin(hWnd);
					}

					// Update UI
					updateLoginUI(hWnd, blClearCredentials);
					break;

				// Audioscrobbler badge
				case IDC_ASBADGE:
					ShellExecute(NULL, "open", "http://last.fm/", NULL, NULL, SW_SHOWNORMAL);
					break;
			}
			break;

		case WM_CTLCOLORSTATIC:
			switch(GetDlgCtrlID((HWND)lParam)) {
				// Make control red or green depending on logged in status
				case IDC_LOGGEDIN:
					hdc = (HDC)wParam;
					if (conf.blAuthenticated) {
						SetBkColor(hdc, RGB(0, 255, 0));
						return (INT_PTR)hBrushGreen;
					} else {
						SetBkColor(hdc, RGB(255, 55, 55));
						return (INT_PTR)hBrushRed;
					}
					break;

				default:
					DefWindowProc(hWnd, iMsg, wParam, lParam);

			}
			break;

		case WM_INITDIALOG:
			hWndConf = hWnd;
			// Initalize window state based on current config

			// Set max-length for username
			MESS(IDC_EDITUSER, EM_SETLIMITTEXT, MAX_USERNAME_LENGTH, 0);

			// Toggle checkboxes to config setting
			MESS(IDC_ENABLESCROBBLE, BM_SETCHECK, conf.blEnableScrobble, 0);
			MESS(IDC_ENABLENP, BM_SETCHECK, conf.blEnableNowPlaying, 0);
			MESS(IDC_ENABLELOG, BM_SETCHECK, conf.blEnableLog, 0);
			MESS(IDC_LIMITLOG, BM_SETCHECK, conf.blLogSizeLimit, 0);

			populateComboBox(hWnd);
			enableLogSettings(hWnd, conf.blEnableLog);

			// Show logged in status
			updateLoginUI(hWnd);
			return TRUE;

		case WM_DESTROY:
			hWndConf = NULL;
			break;
	}
	return FALSE;
}

void populateComboBox(HWND hWnd) {
	MESS(IDC_LOGSIZE, CB_ADDSTRING, 0, "4KB");
	MESS(IDC_LOGSIZE, CB_SETITEMDATA, 0, 4096);

	MESS(IDC_LOGSIZE, CB_ADDSTRING, 0, "8KB");
	MESS(IDC_LOGSIZE, CB_SETITEMDATA, 1, 8192);

	MESS(IDC_LOGSIZE, CB_ADDSTRING, 0, "16KB");
	MESS(IDC_LOGSIZE, CB_SETITEMDATA, 2, 16384);

	MESS(IDC_LOGSIZE, CB_ADDSTRING, 0, "32KB");
	MESS(IDC_LOGSIZE, CB_SETITEMDATA, 3, 32768);

	MESS(IDC_LOGSIZE, CB_ADDSTRING, 0, "64KB");
	MESS(IDC_LOGSIZE, CB_SETITEMDATA, 4, 65536);

	MESS(IDC_LOGSIZE, CB_ADDSTRING, 0, "128KB");
	MESS(IDC_LOGSIZE, CB_SETITEMDATA, 5, 131072);

	MESS(IDC_LOGSIZE, CB_ADDSTRING, 0, "256KB");
	MESS(IDC_LOGSIZE, CB_SETITEMDATA, 6, 262144);

	MESS(IDC_LOGSIZE, CB_ADDSTRING, 0, "512KB");
	MESS(IDC_LOGSIZE, CB_SETITEMDATA, 7, 524288);

	MESS(IDC_LOGSIZE, CB_ADDSTRING, 0, "1MB");
	MESS(IDC_LOGSIZE, CB_SETITEMDATA, 8, 1048576);

	MESS(IDC_LOGSIZE, CB_ADDSTRING, 0, "2MB");
	MESS(IDC_LOGSIZE, CB_SETITEMDATA, 9, 2097152);

	MESS(IDC_LOGSIZE, CB_SETCURSEL, conf.iLogLimitSelection, 0);
}

void enableLogSettings(HWND hWnd, bool blEnable) {
	EnableWindow(GetDlgItem(hWnd, IDC_LIMITLOG), blEnable);
	EnableWindow(GetDlgItem(hWnd, IDC_LOGSIZE), blEnable);
}

// Update the UI components for login/credentials
void updateLoginUI(HWND hWnd, int blClearCredentials) {
	if (conf.blAuthenticated) {
		MESS(IDC_EDITUSER, WM_SETTEXT, 0, &conf.szUsername);
		MESS(IDC_EDITPASS, WM_SETTEXT, 0, "************");
		MESS(IDC_EDITUSER, EM_SETREADONLY, TRUE, 0);
		MESS(IDC_EDITPASS, EM_SETREADONLY, TRUE, 0);

		// Show logged in status
		MESS(IDC_LOGGEDIN, WM_SETTEXT, 0, "Logged In");
		MESS(IDC_LOGIN, WM_SETTEXT, 0, "Log Out");

	} else {
		MESS(IDC_LOGGEDIN, WM_SETTEXT, 0, "Logged Out");

		// Clear username & pass boxes?
		if (blClearCredentials) {
			MESS(IDC_EDITUSER, WM_SETTEXT, 0, "");
			MESS(IDC_EDITPASS, WM_SETTEXT, 0, "");
		}

		MESS(IDC_EDITUSER, EM_SETREADONLY, FALSE, 0);
		MESS(IDC_EDITPASS, EM_SETREADONLY, FALSE, 0);
		MESS(IDC_LOGIN, WM_SETTEXT, 0, "Log In");
	}
}

// Attempt to authenticate user with credentials in settings screen
// Returns TRUE if authenticated, FALSE otherwise
bool lfmLogin(HWND hWnd) {
	LFMSESSION *pSession = NULL;

	// TODO: Make this method asynchronous as well

	int iUserLength = MESS(IDC_EDITUSER, WM_GETTEXTLENGTH, 0, 0)+1;
	int iPassLength = MESS(IDC_EDITPASS, WM_GETTEXTLENGTH, 0, 0)+1;

	if (iUserLength == 1 || iPassLength == 1) {
		MessageBox(hWnd, "Username or password cannot be left blank!", "Authentication Error", MB_ICONERROR);
		return FALSE;
	}

	char *pszUsername = (char *)malloc(iUserLength);
	char *pszPassword = (char *)malloc(iPassLength);

	MESS(IDC_EDITUSER, WM_GETTEXT, iUserLength, pszUsername);
	MESS(IDC_EDITPASS, WM_GETTEXT, iPassLength, pszPassword);

	pSession = auth_getMobileSession(pszAPIKey, pszUsername, pszPassword, pszSecret);

	free(pszUsername);
	free(pszPassword);

	// Error handling
	if (pSession == NULL) {
		MessageBox(hWnd, "Failed to connect to last.fm servers!\n\nCheck network connection and try again.", "Network Error", MB_ICONERROR);
		return FALSE;
	} else if (pSession->error.iError != LFMERROR_NONE) {
		// Authentication failed
		if (pSession->error.iError == LFMERROR_AUTH_FAILED) {
			MessageBox(hWnd, "Incorrect Username or Password!\n\nCheck your credentials and try again.", "Authentication Failed", MB_ICONERROR);

		// Unhandled error with server message
		} else if (pSession->error.pszMessage != NULL) {
			MessageBox(hWnd, pSession->error.pszMessage, "Authentication Failure", MB_ICONERROR);

		// Unhandled error without server message
		} else {
			char *pszMessage = (char *)malloc(64);
			sprintf(pszMessage, "Failed to log in -- LFM Error: %d!", pSession->error.iError);
			MessageBox(hWnd, pszMessage, "Authentication Failure", MB_ICONERROR);
			free(pszMessage);
		}
		conf.blAuthenticated = FALSE;
		freeObject(pSession);
		return FALSE;
	}

	conf.blAuthenticated = TRUE;
	strcpy((char *)&conf.szUsername, pSession->pszUsername);
	strcpy((char *)&conf.szSessionKey, pSession->pszSessionKey);

	freeObject(pSession);

	return TRUE;
}

static void WINAPI DSP_About(HWND hWnd) {
	char *pszMessageBody = (char *)malloc(1024);
	sprintf(pszMessageBody, "XMPlay Legacy Scrobbler Version %d.%d.%d\n"
							"Copyright (C) 2022-%d Graham Downey\n\n"
							"More plugins and software available at:\n"
							"http://grahamdowney.com/\n\n"
							"This product includes software developed by the OpenSSL Project\n"
							"for use in the OpenSSL Toolkit (http://www.openssl.org/)\n",
							VER_MAJ, VER_MIN, VER_PATCH, CR_YEAR);

	MessageBoxA(hWnd, pszMessageBody, "XMPlay Legacy Scrobbler", MB_OK | MB_ICONINFORMATION);
	free(pszMessageBody);
}

// Called when our plugin is loaded/initialized
static void *WINAPI DSP_New(void) {
	#if (SHOW_DEBUG)
		AllocConsole();
		freopen("CONOUT$", "w", stdout);
	#endif

	// Haven't logged welcome message on first load
	blLoggedWelcomeMsg = FALSE;

	// Set default config
	memset(&conf, 0, sizeof(CONFIG));

	conf.blAuthenticated = FALSE;

	conf.blEnableScrobble = TRUE;
	conf.blEnableNowPlaying = TRUE;
	conf.blEnableLog = TRUE;
	conf.blLogSizeLimit = FALSE;

	// Get xmplay file path directory
	char szFilePath[256];
	int iPos;
	int iLen = GetModuleFileName(NULL, szFilePath, 255);
	for (iPos = iLen; iPos > 0 && szFilePath[iPos-1] != '\\'; iPos--);
	szFilePath[iPos] = '\0';

	// Set cache and log file names
	sprintf(szCacheFile, "%s%s", szFilePath, CACHE_FILE_NAME);
	sprintf(szLogFile, "%s%s", szFilePath, LOG_FILE_NAME);

	log_setFile(szLogFile);

	// Start network thread
	hNetHelperThread = (HANDLE)_beginthreadex(NULL, 0, NetHelperThread, NULL, 0, &idNetHelperThread);

	return (void *)1; // Don't allow multiple instances of this plugin
}

// Clean up plugin for unload
static void WINAPI DSP_Free(void *inst) {
	PostThreadMessage(idNetHelperThread, WM_QUIT, 0, 0);
	LFMCleanup();
}

static const char *WINAPI DSP_GetDescription(void *inst) {
	return dsp.name;
}

static void WINAPI DSP_Config(void *inst, HWND hWnd) {
	// Dialog box
	DialogBox(hInstance, MAKEINTRESOURCE(IDD_CONFIG), hWnd, &DSPDialogProc);
}

// Save our config in XMPlay DSP config
static DWORD WINAPI DSP_GetConfig(void *inst, void *config) {
	if (config != NULL)
		puts("Config loaded!");
	memcpy(config, &conf, sizeof(conf));
	return sizeof(conf); // return size of config data
}

// Load our config from XMPlay storage
static BOOL WINAPI DSP_SetConfig(void *inst, void *config, DWORD size) {
	memcpy(&conf, config, min(size, sizeof(conf)));

	// Apply log settings, etc.
	applySettings();

	// Do we need to log the welcome message?
	if (!blLoggedWelcomeMsg && conf.blEnableLog) {
		log_write("Welcome to XMPlay Legacy Scrobbler v%d.%d.%d\n", VER_MAJ, VER_MIN, VER_PATCH);
		blLoggedWelcomeMsg = TRUE;
	}

	return TRUE;
}

static void UpdateNowPlaying() {
	// Initialize session struct
	LFMSESSION *pSession = getSession();
	if (pSession == NULL) {
		return; // Not authenticated
	}

	// Initalize NowPlaying struct
	LFMNOWPLAYING *lfmnp = (LFMNOWPLAYING *)malloc(sizeof(LFMNOWPLAYING));
	memset(lfmnp, 0, sizeof(LFMNOWPLAYING));

	char *pszArtist = xmpfmisc->GetTag(TAG_ARTIST);
	char *pszTitle  = xmpfmisc->GetTag(TAG_TITLE);
	char *pszAlbum  = xmpfmisc->GetTag(TAG_ALBUM);
	char *pszTrack  = xmpfmisc->GetTag(TAG_TRACK);
	char *pszLength = xmpfmisc->GetTag(TAG_LENGTH);

	log_write("Current track: %s - %s (%s)\n", pszArtist, pszTitle, pszAlbum);
	
	printf("%s, %s, %s, %s, %s\n", pszArtist, pszTitle, pszAlbum, pszTrack, pszLength);

	if (conf.blEnableNowPlaying && pszArtist != NULL && pszTitle != NULL) {
		// Copy tags into lfmnp struct
		lfmnp->pszArtist = (char *)malloc(strlen(pszArtist)+1);
		lfmnp->pszTrack = (char *)malloc(strlen(pszTitle)+1);

		strcpy(lfmnp->pszArtist, pszArtist);
		strcpy(lfmnp->pszTrack, pszTitle);

		// Only grab album tag if it has one
		if (pszAlbum != NULL) {
			lfmnp->pszAlbum = (char *)malloc(strlen(pszAlbum)+1);
			strcpy(lfmnp->pszAlbum, pszAlbum);
		}

		if (pszTrack != NULL) {
			lfmnp->iTrackNumber = strtol(pszTrack, NULL, 10);
		}

		if (pszLength != NULL) {
			lfmnp->iDuration = strtol(pszLength, NULL, 10);
		}

		// Post message for net thread to update now playing
		// NetHelperThread is responsible for freeing pSession and lfmnp
		PostThreadMessage(idNetHelperThread, NTM_UPDATENOWPLAYING, (WPARAM)pSession, (LPARAM)lfmnp);

	} else {
		puts("Insufficient track tags... scrobble skiped");

		freeObject(lfmnp);
		freeObject(pSession);
	}

	xmpfmisc->Free(pszArtist);
	xmpfmisc->Free(pszTitle);
	xmpfmisc->Free(pszAlbum);
	xmpfmisc->Free(pszTrack);
	xmpfmisc->Free(pszLength);
}

int iTrackTimeCount = 0, iTargetPos = 0;
bool blTrackScrobbled = false;

unsigned int iChannels, iRate;

void setScrobbleCounter() {
	// Get track length, if no length, set track length to 60s
	char *pszLen = xmpfmisc->GetTag(TAG_LENGTH);
	int iLen = strtol(pszLen, NULL, 10)*iRate;
	if (iLen == 0) {
		iLen = 60*iRate;
	}

	iTargetPos = iLen/2;

	// If 1/2 length is still > 4 min, set scrobble timer to 4 min.
	if ((unsigned)iTargetPos > (4*60)*iRate) {
		iTargetPos = (4*60)*iRate;
	}

	printf("Length: %s, cntLen: %d\n", pszLen, iLen);
	printf("Target Time: %d\n", iTargetPos);
	xmpfmisc->Free(pszLen);

}

// New track opened (or closed if file == NULL)
static void WINAPI DSP_NewTrack(void *inst, const char *file) {
	// Track stopped or NowPlaying reports disabled
	if (file == NULL) { 
		return;
	}

	blLoggedScrobbleTime = false;
	blTrackScrobbled = false;
	iTrackTimeCount = 0;

	UpdateNowPlaying();

	setScrobbleCounter();

}


// Called at the start of a new track (if sample rate changes)
void WINAPI DSP_SetFormat(void *inst, const XMPFORMAT *form) {
	if (form != NULL) {
		printf("Sample rate: %u, Channels: %d\n", form->rate, form->chan);

		iChannels = form->chan;
		iRate = form->rate;

		setScrobbleCounter();

	} else  {
		puts("Format is NULL -- song closed!");
	}

}

// Called by XMPlay when seeking in a track
static void WINAPI DSP_Reset(void *inst) {
	
}

void ScrobbleTrack() {
	// Initialize session struct
	LFMSESSION *pSession = getSession();
	if (pSession == NULL) {
		return; // Not authenticated
	}

	// Initalize NowPlaying struct
	LFMSCROBBLE *lfmScrobble = (LFMSCROBBLE *)malloc(sizeof(LFMSCROBBLE));
	memset(lfmScrobble, 0, sizeof(LFMSCROBBLE));

	char *pszArtist = xmpfmisc->GetTag(TAG_ARTIST);
	char *pszTitle  = xmpfmisc->GetTag(TAG_TITLE);
	char *pszAlbum  = xmpfmisc->GetTag(TAG_ALBUM);
	char *pszTrack  = xmpfmisc->GetTag(TAG_TRACK);
	char *pszLength = xmpfmisc->GetTag(TAG_LENGTH);

	printf("%s, %s, %s, %s, %s\n", pszArtist, pszTitle, pszAlbum, pszTrack, pszLength);

	if (pszArtist != NULL && pszTrack != NULL) {
		// Copy tags into lfmnp struct
		lfmScrobble->pszArtist = (char *)malloc(strlen(pszArtist)+1);
		lfmScrobble->pszTrack = (char *)malloc(strlen(pszTitle)+1);

		strcpy(lfmScrobble->pszArtist, pszArtist);
		strcpy(lfmScrobble->pszTrack, pszTitle);

		if (pszAlbum != NULL) {
			lfmScrobble->pszAlbum = (char *)malloc(strlen(pszAlbum)+1);
			strcpy(lfmScrobble->pszAlbum, pszAlbum);
		}

		if (pszTrack != NULL) {
			lfmScrobble->iTrackNumber = strtol(pszTrack, NULL, 10);
		}

		if (pszLength != NULL) {
			lfmScrobble->iDuration = strtol(pszLength, NULL, 10);
		}

		lfmScrobble->llTimestamp = GetSystemTimeAsUnixTime();


		puts("Scrobble request sent...");

		// Post message for net thread to update now playing
		// NetHelperThread is responsible for freeing pSession and lfmScrobble
		PostThreadMessage(idNetHelperThread, NTM_SCROBBLE, (WPARAM)pSession, (LPARAM)lfmScrobble);

	} else {
		puts("Insufficient track tags... scrobble skiped");

		freeObject(lfmScrobble);
		freeObject(pSession);
	}

	xmpfmisc->Free(pszArtist);
	xmpfmisc->Free(pszTitle);
	xmpfmisc->Free(pszAlbum);
	xmpfmisc->Free(pszTrack);
	xmpfmisc->Free(pszLength);
}

static DWORD WINAPI DSP_Process(void *inst, float *buffer, DWORD count) {
	int iCalcTime = (iTrackTimeCount / iRate)-1;
	float fCalcTime = ((float)(iTrackTimeCount) / iRate);

	// Log how long song must play to register scrobble
	if (!blLoggedScrobbleTime && conf.blEnableScrobble) {
		blLoggedScrobbleTime = true;
		log_write("Scrobbling in %d seconds...\n", (iTargetPos/iRate));
	}

	//puts("----------------------");
	//printf("count: %d, time: %d, calc'd time: %d, calc'd time: %f\n", count, (int)xmpfstatus->GetTime(), iCalcTime, fCalcTime);
	//printf("Count: %d, Count*Chan: %d\n", count, count*2);

	iTrackTimeCount += count;

	if (!blTrackScrobbled && iTargetPos > 0 && iTrackTimeCount >= iTargetPos) {
		blTrackScrobbled = true;
		iTargetPos = 0;

		if (conf.blEnableScrobble) {
			ScrobbleTrack();
		}
	}

	return 0;
}

// Track title changed
static void WINAPI DSP_NewTitle(void *inst, const char *title) {
	printf("Track title changed: %s\n", title);

	iTrackTimeCount = 0; // Reset time counter for new track title
	blTrackScrobbled = false;
	blLoggedScrobbleTime = false;

	UpdateNowPlaying();  // Send last.fm new track title
}


#ifdef __cplusplus
extern "C"
#endif
XMPDSP *WINAPI XMPDSP_GetInterface2(DWORD dwFace, InterfaceProc faceproc) {
	if (dwFace != XMPDSP_FACE) return NULL;

	xmpfmisc = (XMPFUNC_MISC *)faceproc(XMPFUNC_MISC_FACE); // import misc functions
	xmpfstatus = (XMPFUNC_STATUS *)faceproc(XMPFUNC_STATUS_FACE); // import status functions

	LFMInit(); // Init the lib here so we don't freeze the whole player while loading

	return &dsp;
}

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD reason, LPVOID lpReserved) {
	if (reason == DLL_PROCESS_ATTACH) {
		hInstance = hModule;
		DisableThreadLibraryCalls(hModule);
	}
    return TRUE;
}

