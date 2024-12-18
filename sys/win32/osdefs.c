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

/* Predominantly this file is keyboard crap, but we also get the network configured here */

#include "headers.h"

#include "config.h"
#include "it.h"
#include "osdefs.h"
#include "fmt.h"
#include "charset.h"
#include "loadso.h"

#include <ws2tcpip.h>
#include <windows.h>
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

// slurp-win32.c
int win32_slurp_init(void);
void win32_slurp_quit(void);

/* global menu object */
static HMENU menu = NULL;

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
	} conv[] = {
		{VK_NUMLOCK, SCHISM_KEYMOD_NUM, 1},
		{VK_CAPITAL, SCHISM_KEYMOD_CAPS, 1},
		{VK_CAPITAL, SCHISM_KEYMOD_CAPS_PRESSED, 0},
		{VK_LSHIFT, SCHISM_KEYMOD_LSHIFT, 0},
		{VK_RSHIFT, SCHISM_KEYMOD_RSHIFT, 0},
		{VK_LMENU, SCHISM_KEYMOD_LALT, 0},
		{VK_RMENU, SCHISM_KEYMOD_RALT, 0},
		{VK_LCONTROL, SCHISM_KEYMOD_LCTRL, 0},
		{VK_RCONTROL, SCHISM_KEYMOD_RCTRL, 0},
		{VK_LWIN, SCHISM_KEYMOD_LGUI, 0},
		{VK_RWIN, SCHISM_KEYMOD_RGUI, 0},
	};

	BYTE ks[256] = {0};
	if (!GetKeyboardState(ks)) return;

	for (int i = 0; i < ARRAY_SIZE(conv); i++) {
		// Clear the original value
		(*mk) &= ~(conv[i].km);

		// Put in our result
		if (ks[conv[i].vk] & (conv[i].toggle ? 0x01 : 0x80))
			(*mk) |= conv[i].km;
	}
}

void win32_sysinit(SCHISM_UNUSED int *pargc, SCHISM_UNUSED char ***pargv)
{
	static WSADATA ignored = {0};

	if (WSAStartup(0x202, &ignored) == SOCKET_ERROR) {
		WSACleanup(); /* ? */
		status.flags |= NO_NETWORK;
	}

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
		AppendMenuA(view, MF_STRING, IDM_VIEW_MESSAGE_EDITOR, "Message Editor\tF9");
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

	win32_slurp_init();
}

void win32_sysexit(void)
{
#ifdef USE_MEDIAFOUNDATION
	win32mf_quit();
#endif
	
	win32_slurp_quit();
}

int win32_event(schism_event_t *event)
{
	if (event->type != SCHISM_EVENT_WM_MSG)
		return 1;

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

		if (GetVersion() & UINT32_C(0x80000000)) {
			int needed = DragQueryFileW(drop, 0, NULL, 0);

			wchar_t *f = mem_alloc((needed + 1) * sizeof(wchar_t));
			DragQueryFileW(drop, 0, f, needed + 1);
			f[needed] = 0;

			charset_iconv(f, &e.drop.file, CHARSET_WCHAR_T, CHARSET_CHAR, (needed + 1) * sizeof(wchar_t));
		} else {
			int needed = DragQueryFileA(drop, 0, NULL, 0);

			char *f = mem_alloc((needed + 1) * sizeof(char));
			DragQueryFileA(drop, 0, f, needed);
			f[needed] = 0;

			charset_iconv(f, &e.drop.file, CHARSET_ANSI, CHARSET_CHAR, needed + 1);
		}

		if (!e.drop.file)
			return 0;

		*event = e;
		return 1;
	}

	return 0;
}

void win32_toggle_menu(void *window, int on)
{
	SetMenu((HWND)window, (cfg_video_want_menu_bar && on) ? menu : NULL);
	DrawMenuBar((HWND)window);
}

/* -------------------------------------------------------------------- */

void win32_show_message_box(const char *title, const char *text)
{
	if (GetVersion() & UINT32_C(0x80000000)) {
		char *title_a = NULL, *text_a = NULL;
		if (!charset_iconv(title, &title_a, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)
			&& !charset_iconv(text, &text_a, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			MessageBoxA(NULL, text_a, title_a, MB_OK | MB_ICONINFORMATION);
		free(title_a);
		free(text_a);
	} else {
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

static void win32_stat_conv(struct _stat *mst, struct stat *st)
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

	if (GetVersion() & UINT32_C(0x80000000)) {
		// Windows 9x
		char* ac = NULL;

		if (!charset_iconv(path, &ac, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)) {
			int ret = _stat(ac, &mst);
			free(ac);
			win32_stat_conv(&mst, st);
			return ret;
		}
	} else {
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

int win32_open(const char* path, int flags)
{
	if (GetVersion() & UINT32_C(0x80000000)) {
		// Windows 9x
		char* ac = NULL;
		if (charset_iconv(path, &ac, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			return -1;

		int ret = _open(ac, flags);
		free(ac);
		return ret;
	} else {
		wchar_t* wc = NULL;
		if (charset_iconv(path, &wc, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			return -1;

		int ret = _wopen(wc, flags);
		free(wc);
		return ret;
	}

}

FILE* win32_fopen(const char* path, const char* flags)
{
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
	} else {
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
	if (GetVersion() & UINT32_C(0x80000000)) {
		char* ac = NULL;
		if (charset_iconv(path, &ac, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			return -1;

		int ret = mkdir(ac);
		free(ac);
		return ret;
	} else {
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

int win32_run_hook_wide(const char *dir, const char *name, const char *maybe_arg)
{
#define DOT_BAT L".bat"
	WCHAR cwd[PATH_MAX] = {0};
	if (!_wgetcwd(cwd, PATH_MAX))
		return 0;

	WCHAR batch_file[PATH_MAX] = {0};

	{
		wchar_t *name_w;
		if (charset_iconv(name, &name_w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			return 0;

		size_t name_len = wcslen(name_w);
		if ((name_len * sizeof(WCHAR)) + sizeof(DOT_BAT) >= sizeof(batch_file)) {
			free(name_w);
			return 0;
		}

		memcpy(batch_file, name_w, name_len * sizeof(WCHAR));
		memcpy(batch_file + name_len, DOT_BAT, sizeof(DOT_BAT));

		free(name_w);
	}

	{
		wchar_t *dir_w;
		if (charset_iconv(dir, &dir_w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			return 0;

		if (_wchdir(dir_w) == -1) {
			free(dir_w);
			return 0;
		}

		free(dir_w);
	}

	int r;

	{
		wchar_t *maybe_arg_w;
		if (charset_iconv(maybe_arg, &maybe_arg_w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			return 0;

		struct _stat sb;
		if (_wstat(batch_file, &sb) < 0) {
			r = 0;
		} else {
			const WCHAR *cmd;

			cmd = _wgetenv(L"COMSPEC");
			if (!cmd)
				cmd = L"command.com";

			r = _wspawnlp(_P_WAIT, cmd, cmd, "/c", batch_file, maybe_arg_w, 0);
		}

		free(maybe_arg_w);
	}


	_wchdir(cwd);
	if (r == 0) return 1;
	return 0;
#undef DOT_BAT
}

int win32_run_hook_ansi(const char *dir, const char *name, const char *maybe_arg)
{
#define DOT_BAT ".bat"
	char cwd[PATH_MAX] = {0};
	if (!getcwd(cwd, PATH_MAX))
		return 0;

	char batch_file[PATH_MAX] = {0};

	{
		char *name_w;
		if (charset_iconv(name, &name_w, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			return 0;

		size_t name_len = strlen(name_w);
		if ((name_len * sizeof(char)) + sizeof(DOT_BAT) >= sizeof(batch_file)) {
			free(name_w);
			return 0;
		}

		memcpy(batch_file, name_w, name_len * sizeof(char));
		memcpy(batch_file + name_len, DOT_BAT, sizeof(DOT_BAT));

		free(name_w);
	}

	{
		char *dir_w;
		if (charset_iconv(dir, &dir_w, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			return 0;

		if (_chdir(dir_w) == -1) {
			free(dir_w);
			return 0;
		}

		free(dir_w);
	}

	int r;

	{
		char *maybe_arg_w;
		if (charset_iconv(maybe_arg, &maybe_arg_w, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			return 0;

		struct _stat sb;
		if (_stat(batch_file, &sb) < 0) {
			r = 0;
		} else {
			const char *cmd;

			cmd = getenv("COMSPEC");
			if (!cmd)
				cmd = "command.com";

			r = _spawnlp(_P_WAIT, cmd, cmd, "/c", batch_file, maybe_arg_w, 0);
		}

		free(maybe_arg_w);
	}


	_chdir(cwd);
	if (r == 0) return 1;
	return 0;
#undef DOT_BAT
}

int win32_run_hook(const char *dir, const char *name, const char *maybe_arg)
{
	if (GetVersion() & UINT32_C(0x80000000)) {
		return win32_run_hook_ansi(dir, name, maybe_arg);
	} else {
		return win32_run_hook_wide(dir, name, maybe_arg);
	}
}
