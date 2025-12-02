/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* WOW this file got ugly */

#include "headers.h"

#include "config.h"
#include "it.h"
#include "osdefs.h"
#include "fmt.h"
#include "charset.h"
#include "loadso.h"
#include "mem.h"
#include "song.h"

#include <ws2tcpip.h>
#include <windows.h>
#include <winerror.h>
#include <process.h>
#include <shlobj.h>

#include <direct.h>

#include <sys/types.h>
#include <sys/stat.h>

/* helper macro for A/W funcs */
#define WINFUNC_AW(hwnd, func) \
	(IsWindowUnicode(hwnd) ? func##W : func##A)

#define CallWindowProcAW(wndproc, hwnd, msg, wparam, lparam) \
	(WINFUNC_AW(hwnd, CallWindowProc) (wndproc, hwnd, msg, wparam, lparam))
#define GetWindowLongPtrAW(hwnd, x) \
	(WINFUNC_AW(hwnd, GetWindowLongPtr) (hwnd, x))
#define SetWindowLongPtrAW(hwnd, x, y) \
	(WINFUNC_AW(hwnd, SetWindowLongPtr) (hwnd, x, y))

/* -------------------------------------------------------------------- */
/* version checking */

/* cached on startup */
static DWORD win32_ver_major = 0, win32_ver_minor = 0, win32_ver_build = 0;
static DWORD win32_platform = 0;

static int win32_ver_init(void)
{
	union {
		OSVERSIONINFOA a;
		OSVERSIONINFOEXW w;
		DWORD v;
	} ver;

	{
		/* GetVersion() and friends are artificially capped at Windows 8.1;
		 * get around it by calling RtlGetVersion() directly from NTDLL.DLL
		 *
		 * (thanks to the wxwidgets source for providing insight on this!) */
		HMODULE ntdll;

		ntdll = LoadLibraryW(L"NTDLL.DLL");
		if (ntdll) {
			int success;
			typedef long (WINAPI *NTDLL_RtlGetVersionSpec)(OSVERSIONINFOEXW*);
			NTDLL_RtlGetVersionSpec NTDLL_RtlGetVersion;

			NTDLL_RtlGetVersion =
				(NTDLL_RtlGetVersionSpec)GetProcAddress(ntdll,
					"RtlGetVersion");

			success = (NTDLL_RtlGetVersion && !NTDLL_RtlGetVersion(&ver.w));

			FreeLibrary(ntdll);

			if (success) {
				win32_ver_major = ver.w.dwMajorVersion;
				win32_ver_minor = ver.w.dwMinorVersion;
				win32_ver_build = ver.w.dwBuildNumber;
				win32_platform = ver.w.dwPlatformId;
				return 1;
			}
		}
	}

	// fallback to GetVersionExA
	ver.a.dwOSVersionInfoSize = sizeof(ver.a);
	if (GetVersionExA(&ver.a)) {
		win32_ver_major = ver.a.dwMajorVersion;
		win32_ver_minor = ver.a.dwMinorVersion;
		win32_ver_build = ver.a.dwBuildNumber;
		win32_platform = ver.a.dwPlatformId;
		return 1;
	}

	ver.v = GetVersion();

	win32_ver_major = (ver.v & 0xFF);
	win32_ver_minor = (ver.v & 0xFF00) >> 8;
	win32_ver_build = (ver.v & 0x7FFF0000) >> 16;

	/* most significant bit toggled = win9x */
	win32_platform = (ver.v & 0x80000000)
		? VER_PLATFORM_WIN32_WINDOWS
		: VER_PLATFORM_WIN32_NT;

	return 1;
}

int win32_ntver_atleast(int major, int minor, int build)
{
	if (win32_platform != VER_PLATFORM_WIN32_NT)
		return 0;

	return SCHISM_SEMVER_ATLEAST(major, minor, build,
		win32_ver_major, win32_ver_minor, win32_ver_build);
}

/* returns whether this version of windows supports Unicode APIs. */
int win32_ver_unicode(void)
{
	return (win32_platform == VER_PLATFORM_WIN32_NT);
}

/* ------------------------------------------------------------------------ */
/* key modifiers */

void win32_get_modkey(schism_keymod_t *mk)
{
	/* Translation from virtual keys to keymods. We have to do
	 * this because SDL's key modifier stuff is quite buggy
	 * and has caused weird modifier shenanigans in the past. */

	static const struct {
		uint8_t vk;
		schism_keymod_t km;

		// whether this key is a held modifier (i.e.
		// ctrl, alt, shift, win) or is toggled (i.e.
		// numlock, scrolllock)
		int toggle;

		// does this key work on win9x?
		int win9x;
	} conv[] = {
		{VK_NUMLOCK, SCHISM_KEYMOD_NUM, 1, 1},
		{VK_CAPITAL, SCHISM_KEYMOD_CAPS, 1, 1},
		{VK_CAPITAL, SCHISM_KEYMOD_CAPS_PRESSED, 0, 1},
		{VK_LSHIFT, SCHISM_KEYMOD_LSHIFT, 0, 0},
		{VK_RSHIFT, SCHISM_KEYMOD_RSHIFT, 0, 0},
		{VK_LMENU, SCHISM_KEYMOD_LALT, 0, 0},
		{VK_RMENU, SCHISM_KEYMOD_RALT, 0, 0},
		{VK_LCONTROL, SCHISM_KEYMOD_LCTRL, 0, 0},
		{VK_RCONTROL, SCHISM_KEYMOD_RCTRL, 0, 0},
		{VK_LWIN, SCHISM_KEYMOD_LGUI, 0, 0},
		{VK_RWIN, SCHISM_KEYMOD_RGUI, 0, 0},
	};
	BYTE ks[256];
	size_t i;

	const int on_windows_9x = (win32_platform == VER_PLATFORM_WIN32_WINDOWS);

	/* Sometimes GetKeyboardState is out of date and calling GetKeyState
	 * fixes it. Any random key will work. */
	(void)GetKeyState(VK_CAPITAL);

	if (!GetKeyboardState(ks)) return;

	for (i = 0; i < ARRAY_SIZE(conv); i++) {
		/* FIXME: Some keys (notably left and right variations) simply
		 * do not get filled by windows 9x and as such get completely
		 * ignored by this code. In that case just use whatever values
		 * SDL gave and punt. */
		if (on_windows_9x && !conv[i].win9x)
			continue;

		/* Clear the original value */
		(*mk) &= ~(conv[i].km);

		/* Put in our result */
		if (ks[conv[i].vk] & (conv[i].toggle ? 0x01 : 0x80))
			(*mk) |= conv[i].km;
	}
}

/* ------------------------------------------------------------------------ */

void win32_show_message_box(const char *title, const char *text, int style)
{
	const DWORD styles[] = {
		[OS_MESSAGE_BOX_INFO] = (MB_OK | MB_ICONINFORMATION),
		[OS_MESSAGE_BOX_ERROR] = (MB_OK | MB_ICONERROR),
		[OS_MESSAGE_BOX_WARNING] = (MB_OK | MB_ICONEXCLAMATION),
	};

	SCHISM_ANSI_UNICODE({
		char *title_a = NULL;
		char *text_a = NULL;
		if (!charset_iconv(title, &title_a, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)
			&& !charset_iconv(text, &text_a, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			MessageBoxA(NULL, text_a, title_a, styles[style]);
		free(title_a);
		free(text_a);
	}, {
		wchar_t *title_w = NULL;
		wchar_t *text_w = NULL;
		if (!charset_iconv(title, &title_w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX)
			&& !charset_iconv(text, &text_w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			MessageBoxW(NULL, text_w, title_w, styles[style]);
		free(title_w);
		free(text_w);
	})
}

/* -------------------------------------------------------------------- */
/* Key repeat */

int win32_get_key_repeat(int *pdelay, int *prate)
{
	DWORD speed, delay;
	if (!SystemParametersInfoA(SPI_GETKEYBOARDSPEED, 0, &speed, 0)
		|| !SystemParametersInfoA(SPI_GETKEYBOARDDELAY, 0, &delay, 0))
		return 0;

	// Sanity check
	if (speed > 31 || delay > 3)
		return 0;

	// This value is somewhat odd; it's a value from
	// 0 - 31, and even weirder is that it's non-linear,
	// that is, 0 is about a repeat every 400 ms and 31
	// is a repeat every ~33.33 ms.
	//
	// Eventually I came up with this formula to translate
	// it to the repeat rate in milliseconds.
	*prate = (int)(1000.0/((speed/(62.0/55.0)) + 2.5));

	// This one is much simpler.
	*pdelay = (delay + 1) * 250;

	return 1;
}

/* ------------------------------------------------------------------------ */
/* waveout audio device name lookup */

/* By default, waveout devices are limited to 31 chars, which means we get
 * lovely device names like "Headphones (USB-C to 3.5mm Head".
 *
 * Doing this gives us access to longer and more "general" devices names,
 * such as "G432 Gaming Headset", but only if the device supports it! */
static int win32_audio_lookup_device_name_registry_(const void *nameguid,
	char **result)
{
	/* format for printing GUIDs with printf */
#define GUIDF "%08" PRIx32 "-%04" PRIx16 "-%04" PRIx16 "-%02" PRIx8 "%02" \
	PRIx8 "-%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" \
	PRIx8
#define GUIDX(x) (x).Data1, (x).Data2, (x).Data3, (x).Data4[0], (x).Data4[1], \
	(x).Data4[2], (x).Data4[3], (x).Data4[4], (x).Data4[5], (x).Data4[6], \
	(x).Data4[7]
	// Set this to NULL before doing anything
	*result = NULL;

	WCHAR *strw = NULL;
	DWORD len = 0;

	static const GUID nullguid = {0};
	if (!memcmp(nameguid, &nullguid, sizeof(nullguid)))
		return 0;

	{
		HKEY hkey;
		DWORD type;

		WCHAR keystr[256] = {0};
		_snwprintf(keystr, ARRAY_SIZE(keystr) - 1, L"System\\CurrentControlSet\\Control\\MediaCategories\\{" GUIDF "}", GUIDX(*(const GUID *)nameguid));

		if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keystr, 0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS)
			return 0;

		if (RegQueryValueExW(hkey, L"Name", NULL, &type, NULL, &len) != ERROR_SUCCESS || type != REG_SZ) {
			RegCloseKey(hkey);
			return 0;
		}

		strw = mem_alloc(len + sizeof(WCHAR));

		if (RegQueryValueExW(hkey, L"Name", NULL, NULL, (LPBYTE)strw, &len) != ERROR_SUCCESS) {
			RegCloseKey(hkey);
			free(strw);
			return 0;
		}

		RegCloseKey(hkey);
	}

	// force NUL terminate
	strw[len >> 1] = L'\0';

	if (charset_iconv(strw, result, CHARSET_WCHAR_T, CHARSET_UTF8, len + sizeof(WCHAR))) {
		free(strw);
		return 0;
	}

	free(strw);
	return 1;

#undef GUIDF
#undef GUIDX
}

/* win32_audio_lookup_device_name: look up a device name based on
 * some given data.
 * "nameguid" is a directsound device GUID.
 * "waveoutdevid" is a waveout device ID, which is looked up in the
 * directsound DLL to find a long device name if provided */
int win32_audio_lookup_device_name(const void *nameguid,
	const uint32_t *waveoutdevid, char **result)
{
	if (nameguid
		&& win32_audio_lookup_device_name_registry_(nameguid, result))
		return 1;

	if (waveoutdevid
		&& win32_dsound_audio_lookup_waveout_name(waveoutdevid, result))
		return 1;

	return 0;
}

/* -------------------------------------------------------------------- */
/* menu bar cruft */

#define IDM_FILE_NEW  101
#define IDM_FILE_LOAD 102
#define IDM_FILE_SAVE_CURRENT 103
#define IDM_FILE_SAVE_AS 104
#define IDM_FILE_EXPORT 105
#define IDM_FILE_MESSAGE_LOG 106
#define IDM_FILE_QUIT 107
#define IDM_PLAYBACK_SHOW_INFOPAGE 201
#define IDM_PLAYBACK_PLAY_SONG 202
#define IDM_PLAYBACK_PLAY_PATTERN 203
#define IDM_PLAYBACK_PLAY_FROM_ORDER 204
#define IDM_PLAYBACK_PLAY_FROM_MARK_CURSOR 205
#define IDM_PLAYBACK_STOP 206
#define IDM_PLAYBACK_CALCULATE_LENGTH 207
#define IDM_SAMPLES_SAMPLE_LIST 301
#define IDM_SAMPLES_SAMPLE_LIBRARY 302
#define IDM_SAMPLES_RELOAD_SOUNDCARD 303
#define IDM_INSTRUMENTS_INSTRUMENT_LIST 401
#define IDM_INSTRUMENTS_INSTRUMENT_LIBRARY 402
#define IDM_VIEW_HELP 501
#define IDM_VIEW_VIEW_PATTERNS 502
#define IDM_VIEW_ORDERS_PANNING 503
#define IDM_VIEW_VARIABLES 504
#define IDM_VIEW_MESSAGE_EDITOR 505
#define IDM_VIEW_TOGGLE_FULLSCREEN 506
#define IDM_SETTINGS_PREFERENCES 601
#define IDM_SETTINGS_MIDI_CONFIGURATION 602
#define IDM_SETTINGS_PALETTE_EDITOR 603
#define IDM_SETTINGS_FONT_EDITOR 604
#define IDM_SETTINGS_SYSTEM_CONFIGURATION 605

static HMENU menu = NULL;

static int win32_menu_event(schism_event_t *event)
{
	schism_event_t e = {0};

	if (event->type != SCHISM_EVENT_WM_MSG
		|| event->wm_msg.subsystem != SCHISM_WM_MSG_SUBSYSTEM_WINDOWS
		|| event->wm_msg.msg.win.msg != WM_COMMAND)
		return 0; /* what? */

	e.type = SCHISM_EVENT_NATIVE_SCRIPT;
	switch (LOWORD(event->wm_msg.msg.win.wparam)) {
	case IDM_FILE_NEW:
		e.script.which = str_dup("new");
		break;
	case IDM_FILE_LOAD:
		e.script.which = str_dup("load");
		break;
	case IDM_FILE_SAVE_CURRENT:
		e.script.which = str_dup("save");
		break;
	case IDM_FILE_SAVE_AS:
		e.script.which = str_dup("save_as");
		break;
	case IDM_FILE_EXPORT:
		e.script.which = str_dup("export_song");
		break;
	case IDM_FILE_MESSAGE_LOG:
		e.script.which = str_dup("logviewer");
		break;
	case IDM_FILE_QUIT:
		e.type = SCHISM_QUIT;
		break;
	case IDM_PLAYBACK_SHOW_INFOPAGE:
		e.script.which = str_dup("info");
		break;
	case IDM_PLAYBACK_PLAY_SONG:
		e.script.which = str_dup("play");
		break;
	case IDM_PLAYBACK_PLAY_PATTERN:
		e.script.which = str_dup("play_pattern");
		break;
	case IDM_PLAYBACK_PLAY_FROM_ORDER:
		e.script.which = str_dup("play_order");
		break;
	case IDM_PLAYBACK_PLAY_FROM_MARK_CURSOR:
		e.script.which = str_dup("play_mark");
		break;
	case IDM_PLAYBACK_STOP:
		e.script.which = str_dup("stop");
		break;
	case IDM_PLAYBACK_CALCULATE_LENGTH:
		e.script.which = str_dup("calc_length");
		break;
	case IDM_SAMPLES_SAMPLE_LIST:
		e.script.which = str_dup("sample_page");
		break;
	case IDM_SAMPLES_SAMPLE_LIBRARY:
		e.script.which = str_dup("sample_library");
		break;
	case IDM_SAMPLES_RELOAD_SOUNDCARD:
		e.script.which = str_dup("init_sound");
		break;
	case IDM_INSTRUMENTS_INSTRUMENT_LIST:
		e.script.which = str_dup("inst_page");
		break;
	case IDM_INSTRUMENTS_INSTRUMENT_LIBRARY:
		e.script.which = str_dup("inst_library");
		break;
	case IDM_VIEW_HELP:
		e.script.which = str_dup("help");
		break;
	case IDM_VIEW_VIEW_PATTERNS:
		e.script.which = str_dup("pattern");
		break;
	case IDM_VIEW_ORDERS_PANNING:
		e.script.which = str_dup("orders");
		break;
	case IDM_VIEW_VARIABLES:
		e.script.which = str_dup("variables");
		break;
	case IDM_VIEW_MESSAGE_EDITOR:
		e.script.which = str_dup("message_edit");
		break;
	case IDM_VIEW_TOGGLE_FULLSCREEN:
		e.script.which = str_dup("fullscreen");
		break;
	case IDM_SETTINGS_PREFERENCES:
		e.script.which = str_dup("preferences");
		break;
	case IDM_SETTINGS_MIDI_CONFIGURATION:
		e.script.which = str_dup("midi_config");
		break;
	case IDM_SETTINGS_PALETTE_EDITOR:
		e.script.which = str_dup("palette_page");
		break;
	case IDM_SETTINGS_FONT_EDITOR:
		e.script.which = str_dup("font_editor");
		break;
	case IDM_SETTINGS_SYSTEM_CONFIGURATION:
		e.script.which = str_dup("system_config");
		break;
	default:
		return 0;
	}

	*event = e;
	return 1;
}

/* called by win32_sysinit() */
static int win32_init_menu(void)
{
	/* TODO check return values here */
	menu = CreateMenu();

	{
		HMENU file = CreatePopupMenu();
		AppendMenuA(file, MF_STRING, IDM_FILE_NEW, "&New\tCtrl+N");
		AppendMenuA(file, MF_STRING, IDM_FILE_LOAD, "&Load\tF9");
		AppendMenuA(file, MF_STRING, IDM_FILE_SAVE_CURRENT, "&Save Current\tCtrl+S");
		AppendMenuA(file, MF_STRING, IDM_FILE_SAVE_AS, "Save &As...\tF10");
		AppendMenuA(file, MF_STRING, IDM_FILE_EXPORT, "&Export...\tShift+F10");
		AppendMenuA(file, MF_STRING, IDM_FILE_MESSAGE_LOG, "&Message Log\tCtrl+F11");
		AppendMenuA(file, MF_SEPARATOR, 0, NULL);
		AppendMenuA(file, MF_STRING, IDM_FILE_QUIT, "&Quit\tCtrl+Q");
		AppendMenuA(menu, MF_POPUP, (uintptr_t)file, "&File");
	}
	{
		/* this is equivalent to the "Schism Tracker" menu on Mac OS X */
		HMENU view = CreatePopupMenu();
		AppendMenuA(view, MF_STRING, IDM_VIEW_HELP, "Help\tF1");
		AppendMenuA(view, MF_SEPARATOR, 0, NULL);
		AppendMenuA(view, MF_STRING, IDM_VIEW_VIEW_PATTERNS, "View Patterns\tF2");
		AppendMenuA(view, MF_STRING, IDM_VIEW_ORDERS_PANNING, "Orders/Panning\tF11");
		AppendMenuA(view, MF_STRING, IDM_VIEW_VARIABLES, "Variables\tF12");
		AppendMenuA(view, MF_STRING, IDM_VIEW_MESSAGE_EDITOR, "Message Editor\tShift+F9");
		AppendMenuA(view, MF_SEPARATOR, 0, NULL);
		AppendMenuA(view, MF_STRING, IDM_VIEW_TOGGLE_FULLSCREEN, "Toggle Fullscreen\tCtrl+Alt+Return");
		AppendMenuA(menu, MF_POPUP, (uintptr_t)view, "&View");
	}
	{
		HMENU playback = CreatePopupMenu();
		AppendMenuA(playback, MF_STRING, IDM_PLAYBACK_SHOW_INFOPAGE, "Show Infopage\tF5");
		AppendMenuA(playback, MF_STRING, IDM_PLAYBACK_PLAY_SONG, "Play Song\tCtrl+F5");
		AppendMenuA(playback, MF_STRING, IDM_PLAYBACK_PLAY_PATTERN, "Play Pattern\tF6");
		AppendMenuA(playback, MF_STRING, IDM_PLAYBACK_PLAY_FROM_ORDER, "Play from Order\tShift+F6");
		AppendMenuA(playback, MF_STRING, IDM_PLAYBACK_PLAY_FROM_MARK_CURSOR, "Play from Mark/Cursor\tF7");
		AppendMenuA(playback, MF_STRING, IDM_PLAYBACK_STOP, "Stop\tF8");
		AppendMenuA(playback, MF_STRING, IDM_PLAYBACK_CALCULATE_LENGTH, "Calculate Length\tCtrl+P");
		AppendMenuA(menu, MF_POPUP, (uintptr_t)playback, "&Playback");
	}
	{
		HMENU samples = CreatePopupMenu();
		AppendMenuA(samples, MF_STRING, IDM_SAMPLES_SAMPLE_LIST, "&Sample List\tF3");
		AppendMenuA(samples, MF_STRING, IDM_SAMPLES_SAMPLE_LIBRARY, "Sample &Library\tCtrl+F3");
		AppendMenuA(samples, MF_STRING, IDM_SAMPLES_RELOAD_SOUNDCARD, "&Reload Soundcard\tCtrl+G");
		AppendMenuA(menu, MF_POPUP, (uintptr_t)samples, "&Samples");
	}
	{
		HMENU instruments = CreatePopupMenu();
		AppendMenuA(instruments, MF_STRING, IDM_INSTRUMENTS_INSTRUMENT_LIST, "Instrument List\tF4");
		AppendMenuA(instruments, MF_STRING, IDM_INSTRUMENTS_INSTRUMENT_LIBRARY, "Instrument Library\tCtrl+F4");
		AppendMenuA(menu, MF_POPUP, (uintptr_t)instruments, "&Instruments");
	}
	{
		HMENU settings = CreatePopupMenu();
		AppendMenuA(settings, MF_STRING, IDM_SETTINGS_PREFERENCES, "Preferences\tShift+F5");
		AppendMenuA(settings, MF_STRING, IDM_SETTINGS_MIDI_CONFIGURATION, "MIDI Configuration\tShift+F1");
		AppendMenuA(settings, MF_STRING, IDM_SETTINGS_PALETTE_EDITOR, "Palette Editor\tCtrl+F12");
		AppendMenuA(settings, MF_STRING, IDM_SETTINGS_FONT_EDITOR, "Font Editor\tShift+F12");
		AppendMenuA(settings, MF_STRING, IDM_SETTINGS_SYSTEM_CONFIGURATION, "System Configuration\tCtrl+F1");
		AppendMenuA(menu, MF_POPUP, (uintptr_t)settings, "S&ettings");
	}

	return 1;
}

/* define later in this file */
static int win32_init_dark_window(void *window);

void win32_toggle_menu(void *window, int on)
{
	SetMenu((HWND)window, (cfg_video_want_menu_bar && on) ? menu : NULL);
	DrawMenuBar((HWND)window);

	/* FIXME this should not be here!! */
	win32_init_dark_window(window);
}

/* ------------------------------------------------------------------------ */
/* dark mode (windows 10+) */

/* uxtheme defines */
#include "win32-vista.h"

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_OLD
# define DWMWA_USE_IMMERSIVE_DARK_MODE_OLD 19
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
# define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

typedef enum {
    WCA_UNDEFINED = 0,
    WCA_USEDARKMODECOLORS = 26,
    WCA_LAST = 27
} WINDOWCOMPOSITIONATTRIB;

typedef struct {
    WINDOWCOMPOSITIONATTRIB Attrib;
    PVOID pvData;
    SIZE_T cbData;
} WINDOWCOMPOSITIONATTRIBDATA;

/* C-ization: "bool" replaced with "unsigned char" */
static void *lib_dwmapi;
static HRESULT (WINAPI *DWMAPI_DwmSetWindowAttribute)(HWND hwnd, DWORD key, LPCVOID data, DWORD sz_data);

static void *lib_user32;
static BOOL (WINAPI *USER32_SetWindowCompositionAttribute)(HWND, const WINDOWCOMPOSITIONATTRIBDATA *);
static BOOL (WINAPI *USER32_GetMenuItemInfoW)(HMENU, UINT, BOOL, LPMENUITEMINFOW);
static BOOL (WINAPI *USER32_GetMenuBarInfo)(HWND, LONG, LONG, PMENUBARINFO);

static void *lib_uxtheme;
static unsigned char (WINAPI *UXTHEME_ShouldAppsUseDarkMode)(void);
static unsigned char (WINAPI *UXTHEME_AllowDarkModeForWindow)(HWND hwnd, unsigned char allow);
static void (WINAPI *UXTHEME_AllowDarkModeForApp)(unsigned char allow); // v1809
static DWORD (WINAPI *UXTHEME_SetPreferredAppMode)(DWORD app_mode); // v1903
static HTHEME (WINAPI *UXTHEME_OpenThemeData)(HWND hwnd, LPCWSTR pszClassList);
static HRESULT (WINAPI *UXTHEME_DrawThemeTextEx)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCWSTR pszText, int cchText, DWORD dwTextFlags, LPRECT pRect, const DTTOPTS *pOptions);
static void (WINAPI *UXTHEME_RefreshImmersiveColorPolicyState)(void);
#define APPMODE_DEFAULT    (0)
#define APPMODE_ALLOWDARK  (1)
#define APPMODE_FORCEDARK  (2)
#define APPMODE_FORCELIGHT (3)

// Set on video startup.
static int win32_dark_mode_enabled = 0;

/* FIXME: need to store the old window procedure for every window,
 * not just the latest one. */
static WNDPROC old_wndproc = NULL;

/**
 * Hijacked from wxWidgets
 * Copyright (c) 2022 Vadim Zeitlin <vadim@wxwidgets.org>
**/
static LRESULT CALLBACK win32_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (!win32_dark_mode_enabled)
		return CallWindowProcAW(old_wndproc, hwnd, msg, wparam, lparam);

#ifndef WM_MENUBAR_DRAWMENUITEM
# define WM_MENUBAR_DRAWMENUITEM 0x92
#endif
#ifndef WM_MENUBAR_DRAWMENU
# define WM_MENUBAR_DRAWMENU 0x91
#endif

	// This is passed via LPARAM of WM_MENUBAR_DRAWMENU.
	struct MenuBarDrawMenu {
		HMENU hmenu;
		HDC hdc;
		DWORD dwReserved;
	};

	struct MenuBarMenuItem {
		int iPosition;

		/* There are more fields in this (undocumented) struct but we don't
		 * currently need them, so don't bother with declaring them. */
	};

	struct MenuBarDrawMenuItem {
		DRAWITEMSTRUCT dis;
		struct MenuBarDrawMenu mbdm;
		struct MenuBarMenuItem mbmi;
	};

	switch (msg) {
	case WM_NCPAINT:
	case WM_NCACTIVATE: {
		/* Drawing the menu bar background in WM_MENUBAR_DRAWMENU somehow
		 * leaves a single pixel line unpainted (and increasing the size of
		 * the rectangle doesn't help, i.e. drawing is clipped to an area
		 * which is one pixel too small), so we have to draw over it here
		 * to get rid of it. */
		RECT rcWindow, rc;
		LRESULT result;
		HDC hdc;
		HBRUSH hbr;

		result = DefWindowProc(hwnd, msg, wparam, lparam);

		/* Create a RECT one pixel above the client area: note that we
		 * have to use window (and not client) coordinates for this as
		 * this is outside of the client area of the window. */
		if (!GetWindowRect(hwnd, &rcWindow) || !GetClientRect(hwnd, &rc))
			break;

		/* Convert client coordinates to window ones. */
		MapWindowPoints(hwnd, HWND_DESKTOP, (LPPOINT)&rc, 2);
		OffsetRect(&rc, -rcWindow.left, -rcWindow.top);

		rc.bottom = rc.top;
		rc.top--;

		hdc = GetWindowDC(hwnd);
		hbr = CreateSolidBrush(0x2B2B2B);
		FillRect(hdc, &rc, hbr);
		DeleteObject(hbr);
		ReleaseDC(hwnd, hdc);

		return result;
	}
	case WM_MENUBAR_DRAWMENU: {
		/* Erase the menu bar background using custom brush. */
		struct MenuBarDrawMenu *drawmenu = (struct MenuBarDrawMenu *)lparam;

		if (drawmenu) {
			MENUBARINFO mbi;
			RECT rcWindow;
			HBRUSH hbr;

			mbi.cbSize = sizeof(MENUBARINFO);
			if (!USER32_GetMenuBarInfo
				|| !USER32_GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
				break;

			if (!GetWindowRect(hwnd, &rcWindow))
				break;

			/* rcBar is expressed in screen coordinates. */
			OffsetRect(&mbi.rcBar, -rcWindow.left, -rcWindow.top);

			hbr = CreateSolidBrush(0x2B2B2B);
			FillRect(drawmenu->hdc, &mbi.rcBar, hbr);
			DeleteObject(hbr);
		}

		return 1;
	}
	case WM_MENUBAR_DRAWMENUITEM: {
		struct MenuBarDrawMenuItem *drawmenuitem;

		drawmenuitem = (struct MenuBarDrawMenuItem *)lparam;

		if (drawmenuitem) {
			WCHAR buf[256];
			MENUITEMINFOW mii;
			UINT item_state;
			DTTOPTS dttopts;
			int part_state;
			uint32_t col_text, col_bg;
			DWORD drawTextFlags;
			HTHEME theme;

			/* sanity check */
			if (drawmenuitem->dis.CtlType != ODT_MENU)
				break;

			mii.cbSize = sizeof(mii);
			mii.fMask = MIIM_STRING;
			mii.dwTypeData = buf;
			mii.cch = sizeof(buf) - sizeof(*buf);

			/* Note that we need to use the iPosition field of the
			 * undocumented struct here because DRAWITEMSTRUCT::itemID is
			 * not initialized in the struct passed to us here, so this is
			 * the only way to identify the item we're dealing with. */
			if (!USER32_GetMenuItemInfoW
				|| !USER32_GetMenuItemInfoW((HMENU)drawmenuitem->dis.hwndItem,
					drawmenuitem->mbmi.iPosition, TRUE, &mii))
				break;

			item_state = drawmenuitem->dis.itemState;

			col_text = 0xFFFFFF;
			col_bg = 0x2B2B2B;
			if (item_state & ODS_INACTIVE) {
				part_state = MBI_DISABLED;
				col_text = 0x6D6D6D;
			} else if ((item_state & (ODS_GRAYED|ODS_HOTLIGHT))
					== (ODS_GRAYED|ODS_HOTLIGHT)) {
				part_state = MBI_DISABLEDHOT;
			} else if (item_state & ODS_GRAYED) {
				part_state = MBI_DISABLED;
				col_text = 0x6D6D6D;
			} else if (item_state & (ODS_HOTLIGHT | ODS_SELECTED)) {
				part_state = MBI_HOT;
				col_bg = 0x414141;
			} else {
				part_state = MBI_NORMAL;
			}

			/* Don't use DrawThemeBackground() here, as it doesn't use the
			 * correct colors in the dark mode, at least not when using
			 * the "Menu" theme. */
			{
				HBRUSH hbr = CreateSolidBrush(col_bg);
				FillRect(drawmenuitem->dis.hDC, &drawmenuitem->dis.rcItem,
					hbr);
				DeleteObject(hbr);
			}

			/* We have to specify the text color explicitly as by default
			 * black would be used, making the menu label unreadable on the
			 * (almost) black background. */
			dttopts.dwSize = sizeof(dttopts);
			dttopts.dwFlags = DTT_TEXTCOLOR;
			dttopts.crText = col_text;

			drawTextFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
			if (item_state & ODS_NOACCEL)
				drawTextFlags |= DT_HIDEPREFIX;

			theme = UXTHEME_OpenThemeData(hwnd, L"Menu");
			UXTHEME_DrawThemeTextEx(theme, drawmenuitem->dis.hDC,
				MENU_BARITEM, part_state, buf, mii.cch, drawTextFlags,
				&drawmenuitem->dis.rcItem, &dttopts);
		}
		return 0;
	}
	default:
		break;
	}

	return CallWindowProcAW(old_wndproc, hwnd, msg, wparam, lparam);
}

/* TODO: Check for changes in theme settings.
 * My hope is that windows will send an event that we can parse :) */
static inline SCHISM_ALWAYS_INLINE
void win32_toggle_dark_title_bar(void *window, int on)
{
	/* this function is a crime against humanity */
	win32_dark_mode_enabled = (on && (UXTHEME_ShouldAppsUseDarkMode && UXTHEME_ShouldAppsUseDarkMode()));

	const BOOL b = win32_dark_mode_enabled;

	if (DWMAPI_DwmSetWindowAttribute) {
		// Initialize dark theme on title bar. 20 is used on newer versions of Windows 10,
		// but 19 was used before they switched to 20 for some reason.
		if (FAILED(DWMAPI_DwmSetWindowAttribute((HWND)window, 20, &b, sizeof(b))))
			DWMAPI_DwmSetWindowAttribute((HWND)window, 19, &b, sizeof(b));
	}

	if (UXTHEME_AllowDarkModeForWindow) {
		UXTHEME_AllowDarkModeForWindow((HWND)window, on);
	}

	if (win32_ntver_atleast(10, 0, 18362)) {
		BOOL v = b; /* this value isn't const ? */
		if (USER32_SetWindowCompositionAttribute) {
			WINDOWCOMPOSITIONATTRIBDATA data = { WCA_USEDARKMODECOLORS, &v, sizeof(v) };
			USER32_SetWindowCompositionAttribute((HWND)window, &data);
		}
	} else if (win32_ntver_atleast(10, 0, 17763)) {
		SetPropW((HWND)window, L"UseImmersiveDarkModeColors", (HANDLE)(INT_PTR)on);
	}

	if (UXTHEME_SetPreferredAppMode) /* 1904 */
		UXTHEME_SetPreferredAppMode(b ? APPMODE_FORCEDARK : APPMODE_FORCELIGHT);
	else if (UXTHEME_AllowDarkModeForApp) /* old win10 */
		UXTHEME_AllowDarkModeForApp(b);

	if (UXTHEME_RefreshImmersiveColorPolicyState)
		UXTHEME_RefreshImmersiveColorPolicyState();
}

static int win32_init_dark_window(void *window)
{
	/* hack: never initialize a window twice. */
	static void **dark_windows = NULL;
	static size_t dark_windows_size = 0;
	void **new_dw;
	size_t i;

	for (i = 0; i < dark_windows_size; i++)
		if (dark_windows[i] == window)
			return 0; /* already done? */

	/* if we're here, our window is not initialized with dark mode. */
	new_dw = realloc(dark_windows, (dark_windows_size + 1) * sizeof(void *));
	if (new_dw) {
		dark_windows = new_dw;

		dark_windows[dark_windows_size++] = window;
	}

	/* finally, enable Dark Mode support on Windows 10 >= 1809 */
	if (win32_ntver_atleast(10, 0, 17763)) {
		win32_toggle_dark_title_bar(window, 1);

		old_wndproc = (WNDPROC)GetWindowLongPtrAW((HWND)window, GWLP_WNDPROC);
		SetWindowLongPtrAW((HWND)window, GWLP_WNDPROC, (LONG_PTR)win32_wndproc);
	} else {
		/* XXX is this really necessary */
		win32_toggle_dark_title_bar(window, 0);
	}

	return 1;
}

static int win32_dark_init(void)
{
	/* Load dark mode cruft */
	if (win32_ntver_atleast(10, 0, 17763)) {
		lib_dwmapi = loadso_object_load("dwmapi.dll");
		if (lib_dwmapi) {
			DWMAPI_DwmSetWindowAttribute = loadso_function_load(lib_dwmapi, "DwmSetWindowAttribute");
		}

		lib_uxtheme = loadso_object_load("uxtheme.dll");
		if (lib_uxtheme) {
			/* Just in case MS decides to actually export these functions, I'm attempting
			 * to load functions by name before trying exact ordinals. */
#define LOAD_UNDOCUMENTED_FUNC(name, ordinal) \
	do { \
		UXTHEME_##name = loadso_function_load(lib_uxtheme, #name); \
		if (!UXTHEME_##name) UXTHEME_##name = loadso_function_load(lib_uxtheme, MAKEINTRESOURCEA(ordinal)); \
	} while (0)

			LOAD_UNDOCUMENTED_FUNC(RefreshImmersiveColorPolicyState, 104);
			LOAD_UNDOCUMENTED_FUNC(ShouldAppsUseDarkMode, 132);
			LOAD_UNDOCUMENTED_FUNC(AllowDarkModeForWindow, 133);
			UXTHEME_OpenThemeData = loadso_function_load(lib_uxtheme, "OpenThemeData");
			UXTHEME_DrawThemeTextEx = loadso_function_load(lib_uxtheme, "DrawThemeTextEx");

			if (win32_ntver_atleast(10, 0, 18362)) {
				LOAD_UNDOCUMENTED_FUNC(SetPreferredAppMode, 135);
			} else {
				LOAD_UNDOCUMENTED_FUNC(AllowDarkModeForApp, 135);
			}

#undef LOAD_UNDOCUMENTED_FUNC
		}

		lib_user32 = loadso_object_load("user32.dll");
		if (lib_user32) {
			USER32_SetWindowCompositionAttribute = loadso_function_load(lib_user32, "SetWindowCompositionAttribute");
			USER32_GetMenuItemInfoW = loadso_function_load(lib_user32, "GetMenuItemInfoW");
			USER32_GetMenuBarInfo = loadso_function_load(lib_user32, "GetMenuBarInfo");
		}
	}

	return 1;
}

static void win32_dark_quit(void)
{
	if (lib_dwmapi)
		loadso_object_unload(lib_dwmapi);

	if (lib_uxtheme)
		loadso_object_unload(lib_uxtheme);

	if (lib_user32)
		loadso_object_unload(lib_user32);
}

/* ------------------------------------------------------------------------ */
/* crash handler, only available on Windows NT (?) */

static void *lib_dbghelp;
static BOOL (WINAPI *DBGHELP_SymInitialize)(HANDLE,PCSTR,BOOL);
static DWORD64 (WINAPI *DBGHELP_SymGetModuleBase64)(HANDLE,DWORD64);
static BOOL (WINAPI *DBGHELP_SymFromAddr)(HANDLE,DWORD64,PDWORD64,PSYMBOL_INFO);
static BOOL (WINAPI *DBGHELP_StackWalk64)(DWORD,HANDLE,HANDLE,LPSTACKFRAME64,
	PVOID,PREAD_PROCESS_MEMORY_ROUTINE64,PFUNCTION_TABLE_ACCESS_ROUTINE64,
	PGET_MODULE_BASE_ROUTINE64,PTRANSLATE_ADDRESS_ROUTINE64);
static PVOID (WINAPI *DBGHELP_SymFunctionTableAccess64)(HANDLE,DWORD64);

static void *lib_kernel32;
static LPTOP_LEVEL_EXCEPTION_FILTER (WINAPI *KERNEL32_SetUnhandledExceptionFilter)(
	LPTOP_LEVEL_EXCEPTION_FILTER
);

static void *lib_psapi;
static DWORD (WINAPI *PSAPI_GetModuleBaseNameA)(HANDLE,HMODULE,LPSTR,DWORD);

static void *lib_ntdll;
static USHORT (WINAPI *NTDLL_RtlCaptureStackBackTrace)(ULONG,ULONG,PVOID,PULONG);

static void win32_exception_log_cb(FILE *log, void *userdata)
{
	LPEXCEPTION_POINTERS p = userdata;
	HANDLE process, thread;
	char module_name[MAX_PATH]; /* can be schism itself or a dll */
	DWORD64 addr;
	int i;

	module_name[0] = 0;

	process = GetCurrentProcess();
	thread = GetCurrentThread();

	DBGHELP_SymInitialize(process, NULL, TRUE);

	addr = DBGHELP_SymGetModuleBase64(process, (DWORD64)(uintptr_t)p->ExceptionRecord->ExceptionAddress);
	PSAPI_GetModuleBaseNameA(process, (HMODULE)(uintptr_t)addr, module_name, sizeof(module_name));

	fprintf(log, "Exception code: 0x%08X\n",
		(uint32_t)p->ExceptionRecord->ExceptionCode);
	fprintf(log, "Exception address: 0x%p (%s+0x%llX)\n\n",
		p->ExceptionRecord->ExceptionAddress, module_name,
		(uint64_t)(uintptr_t)p->ExceptionRecord->ExceptionAddress - addr);

	fprintf(log, "General purpose and control registers:\n");
#if defined(__i386__) || defined(_M_IX86)
	fprintf(log, "EAX: 0x%08X, EBX: 0x%08X, ECX: 0x%08X\n",
		p->ContextRecord->Eax, p->ContextRecord->Ebx, p->ContextRecord->Ecx);
	fprintf(log, "EDX: 0x%08X, EBP: 0x%08X, EDI: 0x%08X\n",
		p->ContextRecord->Edx, p->ContextRecord->Ebp, p->ContextRecord->Edi);
	fprintf(log, "EIP: 0x%08X, ESI: 0x%08X, ESP: 0x%08X\n",
		p->ContextRecord->Eip, p->ContextRecord->Esi, p->ContextRecord->Esp);
#elif defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
	/* same as i386, but this time with R prefix instead of E */
	fprintf(log, "RAX: 0x%08llX, RBX: 0x%08llX, RCX: 0x%08llX\n",
		p->ContextRecord->Rax, p->ContextRecord->Rbx, p->ContextRecord->Rcx);
	fprintf(log, "RDX: 0x%08llX, RBP: 0x%08llX, RDI: 0x%08llX\n",
		p->ContextRecord->Rdx, p->ContextRecord->Rbp, p->ContextRecord->Rdi);
	fprintf(log, "RIP: 0x%08llX, RSI: 0x%08llX, RSP: 0x%08llX\n",
		p->ContextRecord->Rip, p->ContextRecord->Rsi, p->ContextRecord->Rsp);
#elif defined(__aarch64__) || defined(_M_ARM64)
	fprintf(log, "?\n");
#elif defined(__arm__) || defined(_M_ARM)
	fprintf(log, "?\n");
#else
	fprintf(log, "?\n");
#endif

	fprintf(log, "\nSegment registers:\n");
#if defined(__i386__) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
	fprintf(log, "CS: 0x%04X, DS: 0x%04X, ES: 0x%04X\n",
		p->ContextRecord->SegCs, p->ContextRecord->SegDs, p->ContextRecord->SegEs);
	fprintf(log, "FS: 0x%04X, GS: 0x%04X, SS: 0x%04X\n",
		p->ContextRecord->SegFs, p->ContextRecord->SegGs, p->ContextRecord->SegSs);
#else
	fprintf(log, "?\n");
#endif

#if defined(__i386__) || defined(_M_IX86) || defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
	/* stack trace is only implemented for x86-derivatives,
	 * because I don't have a proper arm machine to test with */
	STACKFRAME64 stack = {0};
	CONTEXT context;

	if (!DBGHELP_SymFromAddr || !DBGHELP_SymFunctionTableAccess64)
		return;

#if defined(__i386__) || defined(_M_IX86)
	stack.AddrPC.Offset    = p->ContextRecord->Eip;
	stack.AddrStack.Offset = p->ContextRecord->Esp;
	stack.AddrFrame.Offset = p->ContextRecord->Ebp;
#elif defined(__amd64__) || defined(_M_AMD64) || defined(_M_X64)
	stack.AddrPC.Offset    = p->ContextRecord->Rip;
	stack.AddrStack.Offset = p->ContextRecord->Rsp;
	stack.AddrFrame.Offset = p->ContextRecord->Rbp;
#endif
	stack.AddrPC.Mode      = AddrModeFlat;
	stack.AddrStack.Mode   = AddrModeFlat;
	stack.AddrFrame.Mode   = AddrModeFlat;

	fprintf(log, "\nStack trace:\n");

	memcpy(&context, &p->ContextRecord, sizeof(CONTEXT));

    for (i = 0; DBGHELP_StackWalk64(
#if defined(__i386__) || defined(_M_IX86)
		IMAGE_FILE_MACHINE_I386
#elif defined(__amd64__) || defined(_M_AMD64) || defined(_M_X64)
		IMAGE_FILE_MACHINE_AMD64
#else
# error whoops
#endif
		, process, thread, &stack, &context, NULL,
		DBGHELP_SymFunctionTableAccess64, DBGHELP_SymGetModuleBase64, NULL);
		i++) {
		/* This seems backwards, but it's what MSDN does */
		char symbol_buf[sizeof(SYMBOL_INFO) + 256] = {0};
		SYMBOL_INFO *symbol;

		symbol = (SYMBOL_INFO *)symbol_buf;
		symbol->SizeOfStruct = sizeof(*symbol);
		symbol->MaxNameLen = 256;

		module_name[0] = 0;

		/* stack trace wgat it is */
		addr = DBGHELP_SymGetModuleBase64(process, stack.AddrPC.Offset);
		PSAPI_GetModuleBaseNameA(process, (HMODULE)(uintptr_t)addr, module_name, sizeof(module_name));

		DBGHELP_SymFromAddr(process, stack.AddrPC.Offset, NULL, symbol);

		fprintf(log, "%d: [%s] ", i, module_name);

		// with symbols
		if (symbol->Address && (stack.AddrPC.Offset >= symbol->Address)) {
			fprintf(log, "(%s+%llX) - 0x%llX\n", symbol->Name,
				(DWORD64)stack.AddrPC.Offset - symbol->Address,
				symbol->Address);
		} else {
			fprintf(log, "- 0x%llX\n", stack.AddrPC.Offset);
		}
	}
#endif
}

static LONG WINAPI win32_exception_handler(LPEXCEPTION_POINTERS p)
{
	schism_crash(win32_exception_log_cb, p);

	return EXCEPTION_EXECUTE_HANDLER;
}

static int win32_exception_init(void)
{
	lib_kernel32 = loadso_object_load("KERNEL32.DLL");
	if (!lib_kernel32) {
		return -1;
	}

	lib_dbghelp = loadso_object_load("DBGHELP.DLL");
	if (!lib_dbghelp) {
		loadso_object_unload(lib_kernel32);
		return -1;
	}

	lib_psapi = loadso_object_load("PSAPI.DLL");
	if (!lib_psapi) {
		loadso_object_unload(lib_kernel32);
		loadso_object_unload(lib_dbghelp);
	}

	lib_ntdll = loadso_object_load("NTDLL.DLL");

	KERNEL32_SetUnhandledExceptionFilter = loadso_function_load(lib_kernel32, "SetUnhandledExceptionFilter");
	DBGHELP_SymGetModuleBase64 = loadso_function_load(lib_dbghelp, "SymGetModuleBase64");
	DBGHELP_SymInitialize = loadso_function_load(lib_dbghelp, "SymInitialize");
	DBGHELP_SymFromAddr = loadso_function_load(lib_dbghelp, "SymFromAddr");
	DBGHELP_StackWalk64 = loadso_function_load(lib_dbghelp, "StackWalk64");
	DBGHELP_SymFunctionTableAccess64 = loadso_function_load(lib_dbghelp, "SymFunctionTableAccess64");
	PSAPI_GetModuleBaseNameA = loadso_function_load(lib_psapi, "GetModuleBaseNameA");

	if (lib_ntdll)
		NTDLL_RtlCaptureStackBackTrace = loadso_function_load(lib_ntdll, "RtlCaptureStackBackTrace");

	if (!KERNEL32_SetUnhandledExceptionFilter
		|| !DBGHELP_SymGetModuleBase64
		|| !DBGHELP_SymInitialize
		|| !PSAPI_GetModuleBaseNameA) {
		loadso_object_unload(lib_kernel32);
		loadso_object_unload(lib_dbghelp);
		loadso_object_unload(lib_psapi);
		loadso_object_unload(lib_ntdll);
		return -1;
	}

	KERNEL32_SetUnhandledExceptionFilter(win32_exception_handler);

	return 0;
}

static void win32_exception_quit(void)
{
	if (KERNEL32_SetUnhandledExceptionFilter) {
		KERNEL32_SetUnhandledExceptionFilter(NULL);
		KERNEL32_SetUnhandledExceptionFilter = NULL;
	}

	if (lib_kernel32)
		loadso_object_unload(lib_kernel32);

	if (lib_dbghelp)
		loadso_object_unload(lib_dbghelp);

	if (lib_psapi)
		loadso_object_unload(lib_psapi);

	if (lib_ntdll)
		loadso_object_unload(lib_ntdll);
}

/* ------------------------------------------------------------------------ */
/* okay, FINALLY, we're at the actual init functions */

void win32_sysinit(SCHISM_UNUSED int *pargc, SCHISM_UNUSED char ***pargv)
{
	/* Initialize winsocks */
	static WSADATA ignored = {0};

	if (WSAStartup(0x202, &ignored) == SOCKET_ERROR) {
		WSACleanup(); /* ? */
		status.flags |= NO_NETWORK;
	}

	win32_ver_init();

#ifdef USE_MEDIAFOUNDATION
	win32mf_init();
#endif

	win32_init_menu();
	win32_dark_init();
	win32_exception_init();

	/* Convert command line arguments to UTF-8 */
	{
		char **utf8_argv;
		int utf8_argc;

		int i;

		/* Windows NT: use Unicode arguments if available */
		LPWSTR cmdline = GetCommandLineW();
		if (cmdline) {
			LPWSTR *argvw = CommandLineToArgvW(cmdline, &utf8_argc);

			if (argvw) {
				/* now we have Unicode arguments, so convert them to UTF-8 */
				utf8_argv = mem_alloc(sizeof(char *) * utf8_argc);

				for (i = 0; i < utf8_argc; i++) {
					charset_iconv(argvw[i], &utf8_argv[i], CHARSET_WCHAR_T,
						CHARSET_CHAR, SIZE_MAX);
					if (!utf8_argv[i])
						utf8_argv[i] = str_dup(""); // ...
				}

				LocalFree(argvw);

				goto have_utf8_args;
			}
		}

		// well, that didn't work, fallback to ANSI.
		utf8_argc = *pargc;
		utf8_argv = mem_alloc(sizeof(char *) * utf8_argc);

		for (i = 0; i < utf8_argc; i++) {
			charset_iconv((*pargv)[i], &utf8_argv[i], CHARSET_ANSI,
				CHARSET_CHAR, SIZE_MAX);
			if (!utf8_argv[i])
				utf8_argv[i] = str_dup(""); // ...
		}

have_utf8_args: ;
		*pargv = utf8_argv;
		*pargc = utf8_argc;
	}
}

void win32_sysexit(void)
{
#ifdef USE_MEDIAFOUNDATION
	win32mf_quit();
#endif

	win32_dark_quit();
	win32_exception_quit();

	/* shutdown winsocks */
	WSACleanup();
}

int win32_event(schism_event_t *event)
{
	if (win32_menu_event(event))
		return 1;

	if (event->type == SCHISM_EVENT_WM_MSG) {
		if (event->wm_msg.subsystem != SCHISM_WM_MSG_SUBSYSTEM_WINDOWS)
			return 1;

		if (event->wm_msg.msg.win.msg == WM_DROPFILES) {
			/* Drag and drop */
			schism_event_t e = {0};
			e.type = SCHISM_DROPFILE;

			HDROP drop = (HDROP)event->wm_msg.msg.win.wparam;

			SCHISM_ANSI_UNICODE({
				int needed = DragQueryFileA(drop, 0, NULL, 0);

				char *f = mem_alloc((needed + 1) * sizeof(char));
				DragQueryFileA(drop, 0, f, needed);
				f[needed] = 0;

				charset_iconv(f, &e.drop.file, CHARSET_ANSI, CHARSET_CHAR,
					needed + 1);
			}, {
				int needed = DragQueryFileW(drop, 0, NULL, 0);

				wchar_t *f = mem_alloc((needed + 1) * sizeof(wchar_t));
				DragQueryFileW(drop, 0, f, needed + 1);
				f[needed] = 0;

				charset_iconv(f, &e.drop.file, CHARSET_WCHAR_T, CHARSET_CHAR,
					(needed + 1) * sizeof(wchar_t));
			})

			if (!e.drop.file)
				return 0;

			*event = e;
			return 1;
		}

		return 0;
	} else if (event->type == SCHISM_KEYDOWN || event->type == SCHISM_KEYUP) {
		/* We get bogus keydowns for Ctrl-Pause.
		 * As a workaround, we can check what Windows thinks, but only
		 * for Right Ctrl. Left Ctrl just gets completely ignored, and there's
		 * nothing we can do about it. */
		if (event->key.sym == SCHISM_KEYSYM_SCROLLLOCK
				&& (event->key.mod & SCHISM_KEYMOD_RCTRL)
				&& !(GetKeyState(VK_SCROLL) & 0x80))
			event->key.sym = SCHISM_KEYSYM_PAUSE;

		return 1;
	}

	return 1;
}

/* ------------------------------------------------------------------------ */

/* converts FILETIME to unix time_t */
static inline int64_t win32_filetime_to_unix_time(FILETIME *ft) {
	uint64_t ul = ((uint64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
	return ((int64_t)(ul - 116444736000000000ULL) / 10000000);
}

/* this is highly related to the XBOX code */
int win32_stat(const char* path, struct stat* st)
{
	void *wpath;

	SCHISM_ANSI_UNICODE({
		wpath = charset_iconv_easy(path, CHARSET_UTF8, CHARSET_ANSI);
	}, {
		wpath = charset_iconv_easy(path, CHARSET_UTF8, CHARSET_WCHAR_T);
	})
	if (!wpath)
		return -1;

	{
		DWORD dw;

		st->st_mode = 0;

		SCHISM_ANSI_UNICODE({
			dw = GetFileAttributesA(wpath);
		}, {
			dw = GetFileAttributesW(wpath);
		})
		if (dw == INVALID_FILE_ATTRIBUTES) {
			free(wpath);
			errno = ENOENT;
			return -1;
		} else if (dw & FILE_ATTRIBUTE_DIRECTORY) {
			st->st_mode |= S_IFDIR;
		} else {
			st->st_mode |= S_IFREG;
		}
	}

	/* CreateFileA returns INVALID_HANDLE_VALUE if file is not actually a file */
	if (S_ISREG(st->st_mode)) {
		HANDLE fh;
		FILETIME ctime, atime, wtime;
		DWORD lo, hi;
		int fail = 0;

		/* we could possibly be more lenient with the access rights here */
		SCHISM_ANSI_UNICODE({
			fh = CreateFileA(wpath, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		}, {
			fh = CreateFileW(wpath, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		})
		if (fh == INVALID_HANDLE_VALUE) {
			free(wpath);
			errno = EINVAL; /* FIXME set something useful here */
			return -1;
		}

		lo = GetFileSize(fh, &hi);

		fail |= (lo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR);
		fail |= !GetFileTime(fh, &ctime, &atime, &wtime);

		CloseHandle(fh);

		if (fail) {
			free(wpath);
			errno = EINVAL;
			return -1;
		}

		/* now, copy everything into the stat structure. */
		st->st_mtime = win32_filetime_to_unix_time(&wtime);
		st->st_atime = win32_filetime_to_unix_time(&atime);
		st->st_ctime = win32_filetime_to_unix_time(&ctime);

		st->st_size = ((uint64_t)hi << 32 | lo);
	}

	free(wpath);
	return 0;
}

FILE *win32_fopen(const char* path, const char* flags)
{
	SCHISM_ANSI_UNICODE({
		// Windows 9x
		char *ac = NULL;
		char *ac_flags = NULL;
		if (charset_iconv(path, &ac, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)
			|| charset_iconv(flags, &ac_flags, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			return NULL;

		FILE *ret = fopen(ac, ac_flags);
		free(ac);
		free(ac_flags);
		return ret;
	}, {
		// Windows NT
		wchar_t *wc = NULL;
		wchar_t *wc_flags = NULL;
		if (charset_iconv(path, &wc, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX)
			|| charset_iconv(flags, &wc_flags, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			return NULL;

		FILE* ret = _wfopen(wc, wc_flags);
		free(wc);
		free(wc_flags);
		return ret;
	})

	// err
	return NULL;
}

int win32_mkdir(const char *path, SCHISM_UNUSED uint32_t mode)
{
	SCHISM_ANSI_UNICODE({
		char* ac = NULL;
		if (charset_iconv(path, &ac, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			return -1;

		int ret = _mkdir(ac);
		free(ac);
		return ret;
	}, {
		wchar_t* wc = NULL;
		if (charset_iconv(path, &wc, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			return -1;

		int ret = _wmkdir(wc);
		free(wc);
		return ret;
	})

	return -1;
}

int win32_access(const char *path, int amode)
{
	int winamode = 0;

	if (amode & X_OK) {
		/* FIXME check if ends in .exe, .scr, etc */
		return -1;
	}

	if (amode & R_OK)
		winamode |= 0x04;

	if (amode & W_OK)
		winamode |= 0x02;

	SCHISM_ANSI_UNICODE({
		char* ac = charset_iconv_easy(path, CHARSET_UTF8, CHARSET_ANSI);
		if (!ac)
			return -1;

		int ret = _access(ac, winamode);
		free(ac);
		return ret;
	}, {
		wchar_t* ac = charset_iconv_easy(path, CHARSET_UTF8, CHARSET_WCHAR_T);
		if (!ac)
			return -1;

		int ret = _waccess(ac, winamode);
		free(ac);
		return ret;
	})

	return -1;
}

struct tm *win32_localtime(const time_t *t)
{
	static struct tm our_tm = {0};

	DWORD dw;
	FILETIME ft;
	SYSTEMTIME st;
	uint64_t ul;
	TIME_ZONE_INFORMATION tz;
	LONG bias;

	ul = (*t * 10000000ULL) + 116444736000000000ULL;

	/* FileTimeToLocalFileTime does not account for
	 * daylight savings time, so we have to put in
	 * a little more effort. */
	dw = GetTimeZoneInformation(&tz);

	/* the real bias is actually the bias plus
	 * the standard/daylight bias. */
	bias = tz.Bias;

	switch (dw) {
	case TIME_ZONE_ID_STANDARD:
		bias += tz.StandardBias;
		break;
	case TIME_ZONE_ID_DAYLIGHT:
		bias += tz.DaylightBias;
		break;
	}

	/* subtract bias from UTC
	 * UTC = local time + bias
	 * local time = UTC - bias */
	ul -= ((int64_t)bias * 60LL * 10000000LL);

	/* shove it into the FILETIME */
	ft.dwHighDateTime = (ul >> 32);
	ft.dwLowDateTime = (ul & 0xFFFFFFFFU);

	/* local time -> system time */
	FileTimeToSystemTime(&ft, &st);

	/* Toggle flag for daylight savings time */
	our_tm.tm_isdst = (dw == TIME_ZONE_ID_DAYLIGHT);

	our_tm.tm_year = st.wYear - 1900;
	our_tm.tm_mon = st.wMonth - 1;
	our_tm.tm_wday = st.wDayOfWeek;
	our_tm.tm_mday = st.wDay;
	our_tm.tm_hour = st.wHour;
	our_tm.tm_min = st.wMinute;
	our_tm.tm_sec = st.wSecond;
	/* our_tm.tm_yday = ... cba calculating this.
	 *    we don't use it anyway */

	return &our_tm;
}

/* ------------------------------------------------------------------------------- */
/* exec */

#define WIN32_EXEC_IMPL(SUFFIX, CHARSET, CHAR_TYPE, SPAWNVP, CHDIR, STRDUP, GETCWD) \
	static inline SCHISM_ALWAYS_INLINE \
	int win32_exec_##SUFFIX(int *status, int *abnormal_exit, const char *dir, const char *name, va_list ap) \
	{ \
		CHAR_TYPE *argv[256]; \
		int i; \
		int r = 0; \
	\
		if (charset_iconv(name, &argv[0], CHARSET_UTF8, CHARSET, SIZE_MAX)) \
			goto cleanup; \
	\
		for (i = 1; i < 255; i++) { \
			const char *arg = va_arg(ap, const char *); \
			if (!arg) \
				break; \
	\
			/* lol what */ \
			if (charset_iconv(arg, &argv[i], CHARSET_UTF8, CHARSET, SIZE_MAX)) \
				goto cleanup; \
		} \
	\
		argv[i] = NULL; \
	\
		{ \
			intptr_t st; \
			CHAR_TYPE old_wdir[MAX_PATH]; \
	\
			if (dir) { \
				CHAR_TYPE *wdir; \
	\
				/* need to save this to chdir back */ \
				GETCWD(old_wdir, MAX_PATH); \
	\
				wdir = charset_iconv_easy(dir, CHARSET_UTF8, CHARSET); \
				if (!wdir) \
					goto cleanup; \
	\
				if (CHDIR(wdir) == -1) { \
					free(wdir); \
					goto cleanup; \
				} \
	\
				free(wdir); \
			} \
	\
			/* standard C is weird and needs an ugly cast */ \
			st = SPAWNVP(_P_WAIT, argv[0], (const CHAR_TYPE *const *)argv); \
			if (st == -1) \
				goto cleanup; \
	\
			if (status) *status = st; \
	\
			/* on Windows, if a process dies because of an unhandled exception, the status code
			 * will be STATUS_(exception type), e.g. STATUS_ACCESS_VIOLATION, which will be a
			 * 32-bit number with the top two bits set. see documentation for the
			 * EXCEPTION_RECORD structure for more info
			 */ \
			if (abnormal_exit) \
				*abnormal_exit = ((st & 0xC0000000) == 0xC0000000); \
	\
			/* hope this works? */ \
			if (dir) CHDIR(old_wdir); \
		} \
	\
		r = 1; \
	\
cleanup: \
		for (i = 0; argv[i]; i++) \
			free(argv[i]); \
	\
		return r; \
	}

WIN32_EXEC_IMPL(A, CHARSET_ANSI, CHAR, _spawnvp, _chdir, _strdup, _getcwd);
WIN32_EXEC_IMPL(W, CHARSET_WCHAR_T, WCHAR, _wspawnvp, _wchdir, _wcsdup, _wgetcwd);

int win32_exec(int *status, int *abnormal_exit, const char *dir, const char *name, ...)
{
	int r;
	va_list ap;

	va_start(ap, name);

	SCHISM_ANSI_UNICODE({
		r = win32_exec_A(status, abnormal_exit, dir, name, ap);
	}, {
		r = win32_exec_W(status, abnormal_exit, dir, name, ap);
	})

	va_end(ap);

	return r;
}

/* ------------------------------------------------------------------------------- */
/* hooks. in a perfect world we could implement this as the same thing everywhere,
 * but we don't live in a perfect world :) */

int win32_run_hook(const char *dir, const char *name, const char *maybe_arg)
{
	/* in order of preference */
	static const char *extensions[] = {
		"bat", /* prioritize .bat for legacy */
		"cmd"  /* because steel is heavier than feathers */
	};
	size_t i;
	int st;
	char *bat_name = NULL; /* yes, this is necessary */
	char *cmd;

	/* obey COMSPEC */
	SCHISM_ANSI_UNICODE({
		char *x = getenv("COMSPEC");
		if (charset_iconv(x ? x : "cmd", &cmd, CHARSET_ANSI, CHARSET_UTF8, SIZE_MAX))
			return 0;
	}, {
		wchar_t *x = _wgetenv(L"COMSPEC");
		if (charset_iconv(x ? x : L"cmd", &cmd, CHARSET_WCHAR_T, CHARSET_UTF8, SIZE_MAX))
			return 0;
	})

	for (i = 0; i < ARRAY_SIZE(extensions); i++) {
		struct stat dummy;
		char *full;
		int r;

		/* BUT THEY'RE BOTH A KILOGRAMME */
		if (asprintf(&bat_name, "%s.%s", name, extensions[i]) < 0) {
			/* out of memory error that we probably can't recover from. */
			free(cmd);
			return 0;
		}

		full = dmoz_path_concat(dir, bat_name);
		r = win32_stat(full, &dummy);
		free(full);
		if (r == -1) {
			free(bat_name);
			/* don't remove this or else the logic falls flat */
			bat_name = NULL;
			continue;
		}

		break;
	}

	if (!bat_name) {
		/* nope */
		free(cmd);
		return 0;
	}

	if (!win32_exec(&st, NULL, dir, cmd, "/c", bat_name, maybe_arg, (char *)NULL))
		st = !0; /* because the status ... is NOT quo */

	free(bat_name);
	free(cmd);

	return (st == 0);
}
