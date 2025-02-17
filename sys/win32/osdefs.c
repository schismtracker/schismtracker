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

#include <ws2tcpip.h>
#include <windows.h>
#include <winerror.h>
#include <process.h>
#include <shlobj.h>

#include <direct.h>

#include <tchar.h>

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

// MinGW32 doesn't have <vsstyles.h> nor <uxtheme.h> ------

typedef int (WINAPI *DTT_CALLBACK_PROC)(HDC,LPWSTR,int,RECT*,UINT,LPARAM);

typedef struct _DTTOPTS {
    DWORD dwSize;
    DWORD dwFlags;
    COLORREF crText;
    COLORREF crBorder;
    COLORREF crShadow;
    int iTextShadowType;
    POINT ptShadowOffset;
    int iBorderSize;
    int iFontPropId;
    int iColorPropId;
    int iStateId;
    BOOL fApplyOverlay;
    int iGlowSize;
    DTT_CALLBACK_PROC pfnDrawTextCallback;
    LPARAM lParam;
} DTTOPTS, *PDTTOPTS;

/* DTTOPTS.dwFlags bits */
#ifndef DTT_TEXTCOLOR
#define DTT_TEXTCOLOR    0x00000001
#endif
#ifndef DTT_BORDERCOLOR
#define DTT_BORDERCOLOR  0x00000002
#endif
#ifndef DTT_SHADOWCOLOR
#define DTT_SHADOWCOLOR  0x00000004
#endif
#ifndef DTT_SHADOWTYPE
#define DTT_SHADOWTYPE   0x00000008
#endif
#ifndef DTT_SHADOWOFFSET
#define DTT_SHADOWOFFSET 0x00000010
#endif
#ifndef DTT_BORDERSIZE
#define DTT_BORDERSIZE   0x00000020
#endif
#ifndef DTT_FONTPROP
#define DTT_FONTPROP     0x00000040
#endif
#ifndef DTT_COLORPROP
#define DTT_COLORPROP    0x00000080
#endif
#ifndef DTT_STATEID
#define DTT_STATEID      0x00000100
#endif
#ifndef DTT_CALCRECT
#define DTT_CALCRECT     0x00000200
#endif
#ifndef DTT_APPLYOVERLAY
#define DTT_APPLYOVERLAY 0x00000400
#endif
#ifndef DTT_GLOWSIZE
#define DTT_GLOWSIZE     0x00000800
#endif
#ifndef DTT_CALLBACK
#define DTT_CALLBACK     0x00001000
#endif
#ifndef DTT_COMPOSITED
#define DTT_COMPOSITED   0x00002000
#endif
#ifndef DTT_VALIDBITS
#define DTT_VALIDBITS    0x00003fff
#endif

#ifndef ODS_SELECTED
#define ODS_SELECTED     0x0001 /* Selected */
#endif
#ifndef ODS_GRAYED
#define ODS_GRAYED       0x0002 /* Grayed (Menus only) */
#endif
#ifndef ODS_DISABLED
#define ODS_DISABLED     0x0004 /* Disabled */
#endif
#ifndef ODS_CHECKED
#define ODS_CHECKED      0x0008 /* Checked (Menus only) */
#endif
#ifndef ODS_FOCUS
#define ODS_FOCUS        0x0010 /* Has focus */
#endif
#ifndef ODS_DEFAULT
#define ODS_DEFAULT      0x0020 /* Default */
#endif
#ifndef ODS_HOTLIGHT
#define ODS_HOTLIGHT     0x0040 /* Highlighted when under mouse */
#endif
#ifndef ODS_INACTIVE
#define ODS_INACTIVE     0x0080 /* Inactive */
#endif
#ifndef ODS_NOACCEL
#define ODS_NOACCEL      0x0100 /* No keyboard accelerator */
#endif
#ifndef ODS_NOFOCUSRECT
#define ODS_NOFOCUSRECT  0x0200 /* No focus rectangle */
#endif
#ifndef ODS_COMBOBOXEDIT
#define ODS_COMBOBOXEDIT 0x1000 /* Edit of a combo box */
#endif

#ifndef DT_TOP
#define DT_TOP                  0x00000000
#endif
#ifndef DT_LEFT
#define DT_LEFT                 0x00000000
#endif
#ifndef DT_CENTER
#define DT_CENTER               0x00000001
#endif
#ifndef DT_RIGHT
#define DT_RIGHT                0x00000002
#endif
#ifndef DT_VCENTER
#define DT_VCENTER              0x00000004
#endif
#ifndef DT_BOTTOM
#define DT_BOTTOM               0x00000008
#endif
#ifndef DT_WORDBREAK
#define DT_WORDBREAK            0x00000010
#endif
#ifndef DT_SINGLELINE
#define DT_SINGLELINE           0x00000020
#endif
#ifndef DT_EXPANDTABS
#define DT_EXPANDTABS           0x00000040
#endif
#ifndef DT_TABSTOP
#define DT_TABSTOP              0x00000080
#endif
#ifndef DT_NOCLIP
#define DT_NOCLIP               0x00000100
#endif
#ifndef DT_EXTERNALLEADING
#define DT_EXTERNALLEADING      0x00000200
#endif
#ifndef DT_CALCRECT
#define DT_CALCRECT             0x00000400
#endif
#ifndef DT_NOPREFIX
#define DT_NOPREFIX             0x00000800
#endif
#ifndef DT_INTERNAL
#define DT_INTERNAL             0x00001000
#endif
#ifndef DT_EDITCONTROL
#define DT_EDITCONTROL          0x00002000
#endif
#ifndef DT_PATH_ELLIPSIS
#define DT_PATH_ELLIPSIS        0x00004000
#endif
#ifndef DT_END_ELLIPSIS
#define DT_END_ELLIPSIS         0x00008000
#endif
#ifndef DT_MODIFYSTRING
#define DT_MODIFYSTRING         0x00010000
#endif
#ifndef DT_RTLREADING
#define DT_RTLREADING           0x00020000
#endif
#ifndef DT_WORD_ELLIPSIS
#define DT_WORD_ELLIPSIS        0x00040000
#endif
#ifndef DT_NOFULLWIDTHCHARBREAK
#define DT_NOFULLWIDTHCHARBREAK 0x00080000
#endif
#ifndef DT_HIDEPREFIX
#define DT_HIDEPREFIX           0x00100000
#endif
#ifndef DT_PREFIXONLY
#define DT_PREFIXONLY           0x00200000
#endif

enum BARITEMSTATES {
    MBI_NORMAL = 1,
    MBI_HOT = 2,
    MBI_PUSHED = 3,
    MBI_DISABLED = 4,
    MBI_DISABLEDHOT = 5,
    MBI_DISABLEDPUSHED = 6,
};

enum MENUPARTS {
    MENU_MENUITEM_TMSCHEMA = 1,
    MENU_MENUDROPDOWN_TMSCHEMA = 2,
    MENU_MENUBARITEM_TMSCHEMA = 3,
    MENU_MENUBARDROPDOWN_TMSCHEMA = 4,
    MENU_CHEVRON_TMSCHEMA = 5,
    MENU_SEPARATOR_TMSCHEMA = 6,
    MENU_BARBACKGROUND = 7,
    MENU_BARITEM = 8,
    MENU_POPUPBACKGROUND = 9,
    MENU_POPUPBORDERS = 10,
    MENU_POPUPCHECK = 11,
    MENU_POPUPCHECKBACKGROUND = 12,
    MENU_POPUPGUTTER = 13,
    MENU_POPUPITEM = 14,
    MENU_POPUPSEPARATOR = 15,
    MENU_POPUPSUBMENU = 16,
    MENU_SYSTEMCLOSE = 17,
    MENU_SYSTEMMAXIMIZE = 18,
    MENU_SYSTEMMINIMIZE = 19,
    MENU_SYSTEMRESTORE = 20,
};

// This sucks
#define HTHEME HANDLE

// --------------------------------------------------------

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

/* global menu object */
static HMENU menu = NULL;

/* used for dark theme crap. */
static void *lib_dwmapi = NULL;
static HRESULT (WINAPI *DWMAPI_DwmSetWindowAttribute)(HWND hwnd, DWORD key, LPCVOID data, DWORD sz_data) = NULL;

static void *lib_user32 = NULL;
static BOOL (WINAPI *USER32_SetWindowCompositionAttribute)(HWND, const WINDOWCOMPOSITIONATTRIBDATA *) = NULL;
static BOOL (WINAPI *USER32_GetMenuItemInfoW)(HMENU, UINT, BOOL, LPMENUITEMINFOW);
static BOOL (WINAPI *USER32_GetMenuBarInfo)(HWND, LONG, LONG, PMENUBARINFO);

/* `bool`, which is 1 byte, call that `unsigned char` ;) */
static void *lib_uxtheme = NULL;
static unsigned char (WINAPI *UXTHEME_ShouldAppsUseDarkMode)(void) = NULL;
static unsigned char (WINAPI *UXTHEME_AllowDarkModeForWindow)(HWND hwnd, unsigned char allow) = NULL;
static void (WINAPI *UXTHEME_AllowDarkModeForApp)(unsigned char allow) = NULL; // v1809
static DWORD (WINAPI *UXTHEME_SetPreferredAppMode)(DWORD app_mode) = NULL; // v1903
static HTHEME (WINAPI *UXTHEME_OpenThemeData)(HWND hwnd, LPCWSTR pszClassList) = NULL;
static HRESULT (WINAPI *UXTHEME_DrawThemeTextEx)(HTHEME hTheme, HDC hdc, int iPartId, int iStateId, LPCWSTR pszText, int cchText, DWORD dwTextFlags, LPRECT pRect, const DTTOPTS *pOptions) = NULL;
static void (WINAPI *UXTHEME_RefreshImmersiveColorPolicyState)(void) = NULL;
#define APPMODE_DEFAULT    0
#define APPMODE_ALLOWDARK  1
#define APPMODE_FORCEDARK  2
#define APPMODE_FORCELIGHT 3

static void *lib_ntdll = NULL;
static long /*NTSTATUS*/ (WINAPI *NTDLL_RtlGetVersion)(OSVERSIONINFOEXW *info) = NULL;

// Set on video startup.
static int win32_dark_mode_enabled = 0;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE_OLD
# define DWMWA_USE_IMMERSIVE_DARK_MODE_OLD 19
#endif
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
# define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

void win32_get_modkey(schism_keymod_t *mk)
{
	// Translation from virtual keys to keymods. We have to do
	// this because SDL's key modifier stuff is quite buggy
	// and has caused weird modifier shenanigans in the past.

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

	const int on_windows_9x = (GetVersion() & UINT32_C(0x80000000));

	// Sometimes GetKeyboardState is out of date and calling GetKeyState
	// fixes it. Any random key will work.
	(void)GetKeyState(VK_CAPITAL);

	BYTE ks[256] = {0};
	if (!GetKeyboardState(ks)) return;

	for (int i = 0; i < ARRAY_SIZE(conv); i++) {
		// FIXME: Some keys (notably left and right variations) simply
		// do not get filled by windows 9x and as such get completely
		// ignored by this code. In that case just use whatever values
		// SDL gave and punt.
		if (on_windows_9x && !conv[i].win9x)
			continue;

		// Clear the original value
		(*mk) &= ~(conv[i].km);

		// Put in our result
		if (ks[conv[i].vk] & (conv[i].toggle ? 0x01 : 0x80))
			(*mk) |= conv[i].km;
	}
}

void win32_sysinit(SCHISM_UNUSED int *pargc, SCHISM_UNUSED char ***pargv)
{
	/* Initialize winsocks */
	static WSADATA ignored = {0};

	if (WSAStartup(0x202, &ignored) == SOCKET_ERROR) {
		WSACleanup(); /* ? */
		status.flags |= NO_NETWORK;
	}

	/* Build the menus */
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
		AppendMenuA(samples, MF_STRING, IDM_SAMPLES_SAMPLE_LIBRARY, "Sample &Library\tShift+F3");
		AppendMenuA(samples, MF_STRING, IDM_SAMPLES_RELOAD_SOUNDCARD, "&Reload Soundcard\tCtrl+G");
		AppendMenuA(menu, MF_POPUP, (uintptr_t)samples, "&Samples");
	}
	{
		HMENU instruments = CreatePopupMenu();
		AppendMenuA(instruments, MF_STRING, IDM_INSTRUMENTS_INSTRUMENT_LIST, "Instrument List\tF4");
		AppendMenuA(instruments, MF_STRING, IDM_INSTRUMENTS_INSTRUMENT_LIBRARY, "Instrument Library\tShift+F4");
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

#ifdef USE_MEDIAFOUNDATION
	win32mf_init();
#endif

	lib_ntdll = loadso_object_load("ntdll.dll");
	if (lib_ntdll) {
		NTDLL_RtlGetVersion = loadso_function_load(lib_ntdll, "RtlGetVersion");
	}

	// Load dark mode cruft
	if (win32_ntver_atleast(10, 0, 17763)) {
		lib_dwmapi = loadso_object_load("dwmapi.dll");
		if (lib_dwmapi) {
			DWMAPI_DwmSetWindowAttribute = loadso_function_load(lib_dwmapi, "DwmSetWindowAttribute");
		}

		lib_uxtheme = loadso_object_load("uxtheme.dll");
		if (lib_uxtheme) {
			// Just in case MS decides to actually export these functions, I'm attempting
			// to load functions by name before trying exact ordinals.
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

			// SDL 3 enables dark mode, even when we don't want it.
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

	/* Convert command line arguments to UTF-8 */
	{
		char **utf8_argv;
		int utf8_argc;

		int i;

		// Windows NT: use Unicode arguments if available
		LPWSTR cmdline = GetCommandLineW();
		if (cmdline) {
			LPWSTR *argvw = CommandLineToArgvW(cmdline, &utf8_argc);

			if (argvw) {
				// now we have Unicode arguments, so convert them to UTF-8
				utf8_argv = mem_alloc(sizeof(char *) * utf8_argc);

				for (i = 0; i < utf8_argc; i++) {
					charset_iconv(argvw[i], &utf8_argv[i], CHARSET_WCHAR_T, CHARSET_CHAR, SIZE_MAX);
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
			charset_iconv((*pargv)[i], &utf8_argv[i], CHARSET_ANSI, CHARSET_CHAR, SIZE_MAX);
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

	if (lib_dwmapi)
		loadso_object_unload(lib_dwmapi);

	if (lib_uxtheme)
		loadso_object_unload(lib_uxtheme);

	if (lib_ntdll)
		loadso_object_unload(lib_ntdll);

	if (lib_user32)
		loadso_object_unload(lib_user32);
}

static LRESULT (CALLBACK *old_wndproc)(HWND, UINT, WPARAM, LPARAM) = NULL;

//
// Hijacked from wxWidgets
// Copyright (c) 2022 Vadim Zeitlin <vadim@wxwidgets.org>
//
static LRESULT CALLBACK win32_wndproc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (!win32_dark_mode_enabled)
		return old_wndproc(hwnd, msg, wparam, lparam);

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

		// There are more fields in this (undocumented) struct but we don't
		// currently need them, so don't bother with declaring them.
	};

	struct MenuBarDrawMenuItem {
		DRAWITEMSTRUCT dis;
		struct MenuBarDrawMenu mbdm;
		struct MenuBarMenuItem mbmi;
	};

	switch (msg) {
	case WM_NCPAINT:
	case WM_NCACTIVATE: {
		// Drawing the menu bar background in WM_MENUBAR_DRAWMENU somehow
		// leaves a single pixel line unpainted (and increasing the size of
		// the rectangle doesn't help, i.e. drawing is clipped to an area
		// which is one pixel too small), so we have to draw over it here
		// to get rid of it.
	
		LRESULT result = DefWindowProc(hwnd, msg, wparam, lparam);

		// Create a RECT one pixel above the client area: note that we
		// have to use window (and not client) coordinates for this as
		// this is outside of the client area of the window.
		RECT rcWindow, rc;
		if (!GetWindowRect(hwnd, &rcWindow) || !GetClientRect(hwnd, &rc))
			break;

		// Convert client coordinates to window ones.
		MapWindowPoints(hwnd, HWND_DESKTOP, (LPPOINT)&rc, 2);
		OffsetRect(&rc, -rcWindow.left, -rcWindow.top);

		rc.bottom = rc.top;
		rc.top--;

		HDC hdc = GetWindowDC(hwnd);
		HBRUSH hbr = CreateSolidBrush(0x2B2B2B);
		FillRect(hdc, &rc, hbr);
		DeleteObject(hbr);
		ReleaseDC(hwnd, hdc);

		return result;
	}
	case WM_MENUBAR_DRAWMENU: {
		// Erase the menu bar background using custom brush.
		struct MenuBarDrawMenu *drawmenu = (struct MenuBarDrawMenu *)lparam;
		
		if (drawmenu) {
			MENUBARINFO mbi = {.cbSize = sizeof(MENUBARINFO)};
			if (!USER32_GetMenuBarInfo || !USER32_GetMenuBarInfo(hwnd, OBJID_MENU, 0, &mbi))
				break;

			RECT rcWindow;
			if (!GetWindowRect(hwnd, &rcWindow))
				break;

			// rcBar is expressed in screen coordinates.
			OffsetRect(&mbi.rcBar, -rcWindow.left, -rcWindow.top);

			HBRUSH hbr = CreateSolidBrush(0x2B2B2B);
			FillRect(drawmenu->hdc, &mbi.rcBar, hbr);
			DeleteObject(hbr);
		}

		return 1;
	}
	case WM_MENUBAR_DRAWMENUITEM: {
		struct MenuBarDrawMenuItem *drawmenuitem = (struct MenuBarDrawMenuItem *)lparam;

		if (drawmenuitem) {
			// Just a sanity check.
			if (drawmenuitem->dis.CtlType != ODT_MENU)
				break;

			WCHAR buf[256];
			MENUITEMINFOW mii = {
				.cbSize = sizeof(MENUITEMINFOW),
				.fMask = MIIM_STRING,
				.dwTypeData = buf,
				.cch = sizeof(buf) - 2,
			};

			// Note that we need to use the iPosition field of the
			// undocumented struct here because DRAWITEMSTRUCT::itemID is
			// not initialized in the struct passed to us here, so this is
			// the only way to identify the item we're dealing with.
			if (!USER32_GetMenuItemInfoW || !USER32_GetMenuItemInfoW((HMENU)drawmenuitem->dis.hwndItem, drawmenuitem->mbmi.iPosition, TRUE, &mii))
				break;

			const UINT itemState = drawmenuitem->dis.itemState;

			int partState;
			uint32_t colText = 0xFFFFFF, colBg = 0x2b2b2b;
			if (itemState & ODS_INACTIVE) {
				partState = MBI_DISABLED;
				colText = 0x6D6D6D;
			} else if ((itemState & (ODS_GRAYED|ODS_HOTLIGHT)) == (ODS_GRAYED|ODS_HOTLIGHT)) {
				partState = MBI_DISABLEDHOT;
			} else if (itemState & ODS_GRAYED ) {
				partState = MBI_DISABLED;
				colText = 0x6D6D6D;
			} else if (itemState & (ODS_HOTLIGHT | ODS_SELECTED)) {
				partState = MBI_HOT;
				colBg = 0x414141;
			} else {
				partState = MBI_NORMAL;
			}

			// Don't use DrawThemeBackground() here, as it doesn't use the
			// correct colours in the dark mode, at least not when using
			// the "Menu" theme.
			{
				HBRUSH hbr = CreateSolidBrush(colBg);
				FillRect(drawmenuitem->dis.hDC, &drawmenuitem->dis.rcItem, hbr);
				DeleteObject(hbr);
			}

			// We have to specify the text colour explicitly as by default
			// black would be used, making the menu label unreadable on the
			// (almost) black background.
			DTTOPTS textOpts;
			textOpts.dwSize = sizeof(textOpts);
			textOpts.dwFlags = DTT_TEXTCOLOR;
			textOpts.crText = colText;

			DWORD drawTextFlags = DT_CENTER | DT_SINGLELINE | DT_VCENTER;
			if (itemState & ODS_NOACCEL)
				drawTextFlags |= DT_HIDEPREFIX;

			HTHEME theme = UXTHEME_OpenThemeData(hwnd, L"Menu");
			UXTHEME_DrawThemeTextEx(theme, drawmenuitem->dis.hDC, MENU_BARITEM, partState,
							  buf, mii.cch, drawTextFlags, &drawmenuitem->dis.rcItem,
							  &textOpts);
		}
		return 0;
	}
	default:
		break;
	}

	return old_wndproc(hwnd, msg, wparam, lparam);
}

int win32_event(schism_event_t *event)
{
	if (event->type == SCHISM_EVENT_WM_MSG) {
		if (event->wm_msg.subsystem != SCHISM_WM_MSG_SUBSYSTEM_WINDOWS)
			return 1;

		if (event->wm_msg.msg.win.msg == WM_COMMAND) {
			schism_event_t e = {0};
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
		} else if (event->wm_msg.msg.win.msg == WM_DROPFILES) {
			/* Drag and drop support */
			schism_event_t e = {0};
			e.type = SCHISM_DROPFILE;

			HDROP drop = (HDROP)event->wm_msg.msg.win.wparam;

#ifdef SCHISM_WIN32_COMPILE_ANSI
			if (GetVersion() & UINT32_C(0x80000000)) {
				int needed = DragQueryFileA(drop, 0, NULL, 0);

				char *f = mem_alloc((needed + 1) * sizeof(char));
				DragQueryFileA(drop, 0, f, needed);
				f[needed] = 0;

				charset_iconv(f, &e.drop.file, CHARSET_ANSI, CHARSET_CHAR, needed + 1);
			} else
#endif
			{
				int needed = DragQueryFileW(drop, 0, NULL, 0);

				wchar_t *f = mem_alloc((needed + 1) * sizeof(wchar_t));
				DragQueryFileW(drop, 0, f, needed + 1);
				f[needed] = 0;

				charset_iconv(f, &e.drop.file, CHARSET_WCHAR_T, CHARSET_CHAR, (needed + 1) * sizeof(wchar_t));
			}

			if (!e.drop.file)
				return 0;

			*event = e;
			return 1;
		}

		return 0;
	} else if (event->type == SCHISM_KEYDOWN || event->type == SCHISM_KEYUP) {
		// We get bogus keydowns for Ctrl-Pause.
		// As a workaround, we can check what Windows thinks, but only for Right Ctrl.
		// Left Ctrl just gets completely ignored and there's nothing we can do about it.
		if (event->key.sym == SCHISM_KEYSYM_SCROLLLOCK && (event->key.mod & SCHISM_KEYMOD_RCTRL) && !(GetKeyState(VK_SCROLL) & 0x80))
			event->key.sym = SCHISM_KEYSYM_PAUSE;

		return 1;
	}

	return 1;
}

// TODO: Check for changes in theme settings.
static inline SCHISM_ALWAYS_INLINE void win32_toggle_dark_title_bar(void *window, int on)
{
	// wow
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
		BOOL v = b; // this value isn't const ?
		if (USER32_SetWindowCompositionAttribute) {
			WINDOWCOMPOSITIONATTRIBDATA data = { WCA_USEDARKMODECOLORS, &v, sizeof(v) };
			USER32_SetWindowCompositionAttribute((HWND)window, &data);
		}
	} else if (win32_ntver_atleast(10, 0, 17763)) {
		SetPropW((HWND)window, L"UseImmersiveDarkModeColors", (HANDLE)(INT_PTR)on);
	}

	if (UXTHEME_SetPreferredAppMode) // 1904
		UXTHEME_SetPreferredAppMode(b ? APPMODE_FORCEDARK : APPMODE_FORCELIGHT);
	else if (UXTHEME_AllowDarkModeForApp) // old win10
		UXTHEME_AllowDarkModeForApp(b);

	if (UXTHEME_RefreshImmersiveColorPolicyState)
		UXTHEME_RefreshImmersiveColorPolicyState();
}

void win32_toggle_menu(void *window, int on)
{
	// hax
	static int init = 0;
	SetMenu((HWND)window, (cfg_video_want_menu_bar && on) ? menu : NULL);
	DrawMenuBar((HWND)window);

	if (!init) {
		init = 1;

		// Enable Dark Mode support on Windows 10 >= 1809
		if (win32_ntver_atleast(10, 0, 17763)) {
			const BOOL unicode = IsWindowUnicode((HWND)window);

			win32_toggle_dark_title_bar(window, 1);

			old_wndproc = (WNDPROC)(unicode ? GetWindowLongPtrW : GetWindowLongPtrA)((HWND)window, GWLP_WNDPROC);
			(void)(unicode ? SetWindowLongPtrW : SetWindowLongPtrA)((HWND)window, GWLP_WNDPROC, (LONG_PTR)win32_wndproc);
		} else {
			// SDL 3 sets this to true, even on older versions, which means the
			// color of the menu bar and title bar clash. Reset it to zero.
			win32_toggle_dark_title_bar(window, 0);
		}
	}
}

/* -------------------------------------------------------------------- */

void win32_show_message_box(const char *title, const char *text)
{
#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (GetVersion() & UINT32_C(0x80000000)) {
		char *title_a = NULL, *text_a = NULL;
		if (!charset_iconv(title, &title_a, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)
			&& !charset_iconv(text, &text_a, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			MessageBoxA(NULL, text_a, title_a, MB_OK | MB_ICONINFORMATION);
		free(title_a);
		free(text_a);
	} else
#endif
	{
		wchar_t *title_w = NULL, *text_w = NULL;
		if (!charset_iconv(title, &title_w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX)
			&& !charset_iconv(text, &text_w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			MessageBoxW(NULL, text_w, title_w, MB_OK | MB_ICONINFORMATION);
		free(title_w);
		free(text_w);
	}
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

/* -------------------------------------------------------------------- */

// Checks for at least some NT kernel version.
// Calls NTDLL.DLL directly because Microsoft artificially
// caps GetVersion() to Windows 8.1 for some reason.
int win32_ntver_atleast(int major, int minor, int build)
{
	// why the hell do they have to make this so difficult?
	union {
		OSVERSIONINFOA a;
		OSVERSIONINFOEXW w;
	} ver;

	ver.w.dwOSVersionInfoSize = sizeof(ver.w);
	if (NTDLL_RtlGetVersion && !NTDLL_RtlGetVersion(&ver.w)) {
		return SCHISM_SEMVER_ATLEAST(major, minor, build,
			ver.w.dwMajorVersion, ver.w.dwMinorVersion, ver.w.dwBuildNumber);
	}

	// fallback to GetVersionExA
	ver.a.dwOSVersionInfoSize = sizeof(ver.a);
	if (GetVersionExA(&ver.a) && ver.a.dwPlatformId == VER_PLATFORM_WIN32_NT)
		return SCHISM_SEMVER_ATLEAST(major, minor, build,
			ver.a.dwMajorVersion, ver.a.dwMinorVersion, ver.a.dwBuildNumber);

	// Probably win9x or something.
	return 0;
}

/* -------------------------------------------------------------------- */

// By default, waveout devices are limited to 31 chars, which means we get
// lovely device names like
//  > Headphones (USB-C to 3.5mm Head
// Doing this gives us access to longer and more "general" devices names,
// such as
//  > G432 Gaming Headset
// but only if the device supports it!
//
// We do this for DirectSound as well mainly to provide some sort of
// reliability with the device names.
int win32_audio_lookup_device_name(const void *nameguid, char **result)
{
	// format for printing GUIDs with printf
#define GUIDF "%08" PRIx32 "-%04" PRIx16 "-%04" PRIx16 "-%02" PRIx8 "%02" PRIx8 "-%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8
#define GUIDX(x) (x).Data1, (x).Data2, (x).Data3, (x).Data4[0], (x).Data4[1], (x).Data4[2], (x).Data4[3], (x).Data4[4], (x).Data4[5], (x).Data4[6], (x).Data4[7]
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

/* -------------------------------------------------------------------- */

static inline SCHISM_ALWAYS_INLINE void win32_stat_conv(struct _stat *mst, struct stat *st)
{
	st->st_gid = mst->st_gid;
	st->st_atime = mst->st_atime;
	st->st_ctime = mst->st_ctime;
	st->st_dev = mst->st_dev;
	st->st_ino = mst->st_ino;
	st->st_mode = mst->st_mode;
	st->st_mtime = mst->st_mtime;
	st->st_nlink = mst->st_nlink;
	st->st_rdev = mst->st_rdev;
	st->st_size = mst->st_size;
	st->st_uid = mst->st_uid;
}

int win32_stat(const char* path, struct stat* st)
{
	struct _stat mst;

#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (GetVersion() & UINT32_C(0x80000000)) {
		// Windows 9x
		char* ac = NULL;

		if (!charset_iconv(path, &ac, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)) {
			int ret = _stat(ac, &mst);
			free(ac);
			win32_stat_conv(&mst, st);
			return ret;
		}
	} else
#endif
	{
		wchar_t* wc = NULL;

		if (!charset_iconv(path, &wc, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX)) {
			int ret = _wstat(wc, &mst);
			free(wc);
			win32_stat_conv(&mst, st);
			return ret;
		}
	}

	return -1;
}

FILE* win32_fopen(const char* path, const char* flags)
{
#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (GetVersion() & UINT32_C(0x80000000)) {
		// Windows 9x
		char *ac = NULL, *ac_flags = NULL;
		if (charset_iconv(path, &ac, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)
			|| charset_iconv(flags, &ac_flags, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			return NULL;

		FILE *ret = fopen(ac, ac_flags);
		free(ac);
		free(ac_flags);
		return ret;
	} else
#endif
	{
		// Windows NT
		wchar_t* wc = NULL, *wc_flags = NULL;
		if (charset_iconv(path, &wc, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX)
			|| charset_iconv(flags, &wc_flags, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			return NULL;

		FILE* ret = _wfopen(wc, wc_flags);
		free(wc);
		free(wc_flags);
		return ret;
	}

	// err
	return NULL;
}

int win32_mkdir(const char *path, SCHISM_UNUSED mode_t mode)
{
#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (GetVersion() & UINT32_C(0x80000000)) {
		char* ac = NULL;
		if (charset_iconv(path, &ac, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			return -1;

		int ret = mkdir(ac);
		free(ac);
		return ret;
	} else
#endif
	{
		wchar_t* wc = NULL;
		if (charset_iconv(path, &wc, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			return -1;

		int ret = _wmkdir(wc);
		free(wc);
		return ret;
	}

	return -1;
}

/* ------------------------------------------------------------------------------- */
/* run hook */

#define WIN32_RUN_HOOK_VARIANT(name, charset, char_type, char_len_func, char_getcwd, char_chdir, char_getenv, char_spawnlp, char_stat, const_prefix) \
	static inline SCHISM_ALWAYS_INLINE int _win32_run_hook_##name(const char *dir, const char *name, const char *maybe_arg) \
	{ \
		char_type cwd[MAX_PATH] = {0}; \
		if (!char_getcwd(cwd, MAX_PATH)) \
			return 0; \
	\
		char_type batch_file[MAX_PATH] = {0}; \
	\
		{ \
			char_type *name_w; \
			if (charset_iconv(name, &name_w, CHARSET_UTF8, charset, SIZE_MAX)) \
				return 0; \
	\
			size_t name_len = char_len_func(name_w); \
			if ((name_len * sizeof(char_type)) + sizeof(const_prefix##".bat") >= sizeof(batch_file)) { \
				free(name_w); \
				return 0; \
			} \
	\
			memcpy(batch_file, name_w, name_len * sizeof(char_type)); \
			memcpy(batch_file + name_len, const_prefix##".bat", sizeof(const_prefix##".bat")); \
	\
			free(name_w); \
		} \
	\
		{ \
			char_type *dir_w; \
			if (charset_iconv(dir, &dir_w, CHARSET_UTF8, charset, SIZE_MAX)) \
				return 0; \
	\
			if (char_chdir(dir_w) == -1) { \
				free(dir_w); \
				return 0; \
			} \
	\
			free(dir_w); \
		} \
	\
		intptr_t r; \
	\
		{ \
			char_type *maybe_arg_w = NULL; \
			charset_iconv(maybe_arg, &maybe_arg_w, CHARSET_UTF8, charset, SIZE_MAX); \
	\
			struct _stat sb; \
			if (char_stat(batch_file, &sb) < 0) { \
				r = 0; \
			} else { \
				const char_type *cmd; \
	\
				cmd = char_getenv(const_prefix##"COMSPEC"); \
				if (!cmd) \
					cmd = const_prefix##"command.com"; \
	\
				r = char_spawnlp(_P_WAIT, cmd, cmd, const_prefix##"/c", batch_file, maybe_arg_w, NULL); \
			} \
	\
			free(maybe_arg_w); \
		} \
	\
		char_chdir(cwd); \
		return (r == 0); \
	}

WIN32_RUN_HOOK_VARIANT(wide, CHARSET_WCHAR_T, WCHAR, wcslen, _wgetcwd, _wchdir, _wgetenv, _wspawnlp, _wstat, L)
#ifdef SCHISM_WIN32_COMPILE_ANSI
WIN32_RUN_HOOK_VARIANT(ansi, CHARSET_ANSI, char, strlen, getcwd, _chdir, getenv, _spawnlp, _stat, /* none */)
#endif

#undef WIN32_RUN_HOOK_VARIANT

int win32_run_hook(const char *dir, const char *name, const char *maybe_arg)
{
#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (GetVersion() & UINT32_C(0x80000000)) {
		return _win32_run_hook_ansi(dir, name, maybe_arg);
	} else
#endif
	{
		return _win32_run_hook_wide(dir, name, maybe_arg);
	}
}
