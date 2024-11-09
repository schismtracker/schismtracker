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

#include <windows.h>
#include <ws2tcpip.h>
#include <process.h>
#include <shlobj.h>

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

/* global menu object */
HMENU menu = NULL;

/* eek... */
void win32_get_modkey(int *mk)
{
	BYTE ks[256];
	if (GetKeyboardState(ks) == 0) return;

	if (ks[VK_CAPITAL] & 128) {
		status.flags |= CAPS_PRESSED;
	} else {
		status.flags &= ~CAPS_PRESSED;
	}

	(*mk) = ((*mk) & ~(KMOD_NUM|KMOD_CAPS))
		| ((ks[VK_NUMLOCK]&1) ? KMOD_NUM : 0)
		| ((ks[VK_CAPITAL]&1) ? KMOD_CAPS : 0);
}

void win32_sysinit(UNUSED int *pargc, UNUSED char ***pargv)
{
	static WSADATA ignored = {0};

	if (WSAStartup(0x202, &ignored) == SOCKET_ERROR) {
		WSACleanup(); /* ? */
		status.flags |= NO_NETWORK;
	}

	menu = CreateMenu();
	{
		HMENU file = CreatePopupMenu();
		AppendMenuW(file, MF_STRING, IDM_FILE_NEW, L"&New\tCtrl+N");
		AppendMenuW(file, MF_STRING, IDM_FILE_LOAD, L"&Load\tF9");
		AppendMenuW(file, MF_STRING, IDM_FILE_SAVE_CURRENT, L"&Save Current\tCtrl+S");
		AppendMenuW(file, MF_STRING, IDM_FILE_SAVE_AS, L"Save &As...\tF10");
		AppendMenuW(file, MF_STRING, IDM_FILE_EXPORT, L"&Export...\tShift+F10");
		AppendMenuW(file, MF_STRING, IDM_FILE_MESSAGE_LOG, L"&Message Log\tCtrl+F11");
		AppendMenuW(file, MF_SEPARATOR, 0, NULL);
		AppendMenuW(file, MF_STRING, IDM_FILE_QUIT, L"&Quit\tCtrl+Q");
		AppendMenuW(menu, MF_POPUP, (uintptr_t)file, L"&File");
	}
	{
		/* this is equivalent to the "Schism Tracker" menu on Mac OS X */
		HMENU view = CreatePopupMenu();
		AppendMenuW(view, MF_STRING, IDM_VIEW_HELP, L"Help\tF1");
		AppendMenuW(view, MF_SEPARATOR, 0, NULL);
		AppendMenuW(view, MF_STRING, IDM_VIEW_VIEW_PATTERNS, L"View Patterns\tF2");
		AppendMenuW(view, MF_STRING, IDM_VIEW_ORDERS_PANNING, L"Orders/Panning\tF11");
		AppendMenuW(view, MF_STRING, IDM_VIEW_VARIABLES, L"Variables\tF12");
		AppendMenuW(view, MF_STRING, IDM_VIEW_MESSAGE_EDITOR, L"Message Editor\tF9");
		AppendMenuW(view, MF_SEPARATOR, 0, NULL);
		AppendMenuW(view, MF_STRING, IDM_VIEW_TOGGLE_FULLSCREEN, L"Toggle Fullscreen\tCtrl+Alt+Return");
		AppendMenuW(menu, MF_POPUP, (uintptr_t)view, L"&View");
	}
	{
		HMENU playback = CreatePopupMenu();
		AppendMenuW(playback, MF_STRING, IDM_PLAYBACK_SHOW_INFOPAGE, L"Show Infopage\tF5");
		AppendMenuW(playback, MF_STRING, IDM_PLAYBACK_PLAY_SONG, L"Play Song\tCtrl+F5");
		AppendMenuW(playback, MF_STRING, IDM_PLAYBACK_PLAY_PATTERN, L"Play Pattern\tF6");
		AppendMenuW(playback, MF_STRING, IDM_PLAYBACK_PLAY_FROM_ORDER, L"Play from Order\tShift+F6");
		AppendMenuW(playback, MF_STRING, IDM_PLAYBACK_PLAY_FROM_MARK_CURSOR, L"Play from Mark/Cursor\tF7");
		AppendMenuW(playback, MF_STRING, IDM_PLAYBACK_STOP, L"Stop\tF8");
		AppendMenuW(playback, MF_STRING, IDM_PLAYBACK_CALCULATE_LENGTH, L"Calculate Length\tCtrl+P");
		AppendMenuW(menu, MF_POPUP, (uintptr_t)playback, L"&Playback");
	}
	{
		HMENU samples = CreatePopupMenu();
		AppendMenuW(samples, MF_STRING, IDM_SAMPLES_SAMPLE_LIST, L"&Sample List\tF3");
		AppendMenuW(samples, MF_STRING, IDM_SAMPLES_SAMPLE_LIBRARY, L"Sample &Library\tShift+F3");
		AppendMenuW(samples, MF_STRING, IDM_SAMPLES_RELOAD_SOUNDCARD, L"&Reload Soundcard\tCtrl+G");
		AppendMenuW(menu, MF_POPUP, (uintptr_t)samples, L"&Samples");
	}
	{
		HMENU instruments = CreatePopupMenu();
		AppendMenuW(instruments, MF_STRING, IDM_INSTRUMENTS_INSTRUMENT_LIST, L"Instrument List\tF4");
		AppendMenuW(instruments, MF_STRING, IDM_INSTRUMENTS_INSTRUMENT_LIBRARY, L"Instrument Library\tShift+F4");
		AppendMenuW(menu, MF_POPUP, (uintptr_t)instruments, L"&Instruments");
	}
	{
		HMENU settings = CreatePopupMenu();
		AppendMenuW(settings, MF_STRING, IDM_SETTINGS_PREFERENCES, L"Preferences\tShift+F5");
		AppendMenuW(settings, MF_STRING, IDM_SETTINGS_MIDI_CONFIGURATION, L"MIDI Configuration\tShift+F1");
		AppendMenuW(settings, MF_STRING, IDM_SETTINGS_PALETTE_EDITOR, L"Palette Editor\tCtrl+F12");
		AppendMenuW(settings, MF_STRING, IDM_SETTINGS_FONT_EDITOR, L"Font Editor\tShift+F12");
		AppendMenuW(settings, MF_STRING, IDM_SETTINGS_SYSTEM_CONFIGURATION, L"System Configuration\tCtrl+F1");
		AppendMenuW(menu, MF_POPUP, (uintptr_t)settings, L"S&ettings");
	}

#ifdef USE_MEDIAFOUNDATION
	win32mf_init();
#endif
}

void win32_sysexit(void)
{
#ifdef USE_MEDIAFOUNDATION
	win32mf_quit();
#endif
}

int win32_event(schism_event_t *event)
{
	if (event->type != SCHISM_EVENT_WM_MSG)
		return 1;

	if (event->wm_msg.msg.win.msg == WM_COMMAND) {
		schism_event_t e = {0};
		e.type = SCHISM_EVENT_NATIVE_SCRIPT;
		switch (LOWORD(event->wm_msg.msg.win.wparam)) {
		case IDM_FILE_NEW:
			e.script.which = "new";
			break;
		case IDM_FILE_LOAD:
			e.script.which = "load";
			break;
		case IDM_FILE_SAVE_CURRENT:
			e.script.which = "save";
			break;
		case IDM_FILE_SAVE_AS:
			e.script.which = "save_as";
			break;
		case IDM_FILE_EXPORT:
			e.script.which = "export_song";
			break;
		case IDM_FILE_MESSAGE_LOG:
			e.script.which = "logviewer";
			break;
		case IDM_FILE_QUIT:
			e.type = SCHISM_QUIT;
			break;
		case IDM_PLAYBACK_SHOW_INFOPAGE:
			e.script.which = "info";
			break;
		case IDM_PLAYBACK_PLAY_SONG:
			e.script.which = "play";
			break;
		case IDM_PLAYBACK_PLAY_PATTERN:
			e.script.which = "play_pattern";
			break;
		case IDM_PLAYBACK_PLAY_FROM_ORDER:
			e.script.which = "play_order";
			break;
		case IDM_PLAYBACK_PLAY_FROM_MARK_CURSOR:
			e.script.which = "play_mark";
			break;
		case IDM_PLAYBACK_STOP:
			e.script.which = "stop";
			break;
		case IDM_PLAYBACK_CALCULATE_LENGTH:
			e.script.which = "calc_length";
			break;
		case IDM_SAMPLES_SAMPLE_LIST:
			e.script.which = "sample_page";
			break;
		case IDM_SAMPLES_SAMPLE_LIBRARY:
			e.script.which = "sample_library";
			break;
		case IDM_SAMPLES_RELOAD_SOUNDCARD:
			e.script.which = "init_sound";
			break;
		case IDM_INSTRUMENTS_INSTRUMENT_LIST:
			e.script.which = "inst_page";
			break;
		case IDM_INSTRUMENTS_INSTRUMENT_LIBRARY:
			e.script.which = "inst_library";
			break;
		case IDM_VIEW_HELP:
			e.script.which = "help";
			break;
		case IDM_VIEW_VIEW_PATTERNS:
			e.script.which = "pattern";
			break;
		case IDM_VIEW_ORDERS_PANNING:
			e.script.which = "orders";
			break;
		case IDM_VIEW_VARIABLES:
			e.script.which = "variables";
			break;
		case IDM_VIEW_MESSAGE_EDITOR:
			e.script.which = "message_edit";
			break;
		case IDM_VIEW_TOGGLE_FULLSCREEN:
			e.script.which = "fullscreen";
			break;
		case IDM_SETTINGS_PREFERENCES:
			e.script.which = "preferences";
			break;
		case IDM_SETTINGS_MIDI_CONFIGURATION:
			e.script.which = "midi_config";
			break;
		case IDM_SETTINGS_PALETTE_EDITOR:
			e.script.which = "palette_page";
			break;
		case IDM_SETTINGS_FONT_EDITOR:
			e.script.which = "font_editor";
			break;
		case IDM_SETTINGS_SYSTEM_CONFIGURATION:
			e.script.which = "system_config";
			break;
		default:
			break;
		}
		*event = e;
	}

	return 1;
}

void win32_toggle_menu(void *window, int on)
{
	SetMenu((HWND)window, (cfg_video_want_menu_bar && on) ? menu : NULL);
	DrawMenuBar((HWND)window);
}

/* -------------------------------------------------------------------- */

int win32_wstat(const wchar_t* path, struct stat* st)
{
	struct _stat mstat;

	int ws = _wstat(path, &mstat);
	if (ws < 0)
		return ws;

	/* copy all the values */
	st->st_gid = mstat.st_gid;
	st->st_atime = mstat.st_atime;
	st->st_ctime = mstat.st_ctime;
	st->st_dev = mstat.st_dev;
	st->st_ino = mstat.st_ino;
	st->st_mode = mstat.st_mode;
	st->st_mtime = mstat.st_mtime;
	st->st_nlink = mstat.st_nlink;
	st->st_rdev = mstat.st_rdev;
	st->st_size = mstat.st_size;
	st->st_uid = mstat.st_uid;

	return ws;
}

/* you may wonder: why is this needed? can't we just use
 * _mktemp() even on UTF-8 encoded strings?
 *
 * well, you *can*, but it will bite you in the ass once
 * you get a string that has a mysterious "X" stored somewhere
 * in the filename; better to just give it as a wide string */
int win32_mktemp(char* template, size_t size)
{
	wchar_t* wc = NULL;
	if (charset_iconv(template, &wc, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
		return -1;

	if (!_wmktemp(wc)) {
		free(wc);
		return -1;
	}

	/* still have to WideCharToMultiByte here */
	if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wc, -1, template, size, NULL, NULL)) {
		free(wc);
		return -1;
	}

	free(wc);
	return 0;
}

int win32_stat(const char* path, struct stat* st)
{
	wchar_t* wc = NULL;
	if (charset_iconv(path, &wc, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
		return -1;

	int ret = win32_wstat(wc, st);
	free(wc);
	return ret;
}

int win32_open(const char* path, int flags)
{
	wchar_t* wc = NULL;
	if (charset_iconv(path, &wc, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
		return -1;

	int ret = _wopen(wc, flags);
	free(wc);
	return ret;
}

FILE* win32_fopen(const char* path, const char* flags)
{
	wchar_t* wc = NULL, *wc_flags = NULL;
	if (charset_iconv(path, &wc, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX)
		|| charset_iconv(flags, &wc_flags, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
		return NULL;

	FILE* ret = _wfopen(wc, wc_flags);
	free(wc);
	free(wc_flags);
	return ret;
}

int win32_mkdir(const char *path, UNUSED mode_t mode)
{
	wchar_t* wc = NULL;
	if (charset_iconv(path, &wc, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
		return -1;

	int ret = _wmkdir(wc);
	free(wc);
	return ret;
}

/* ------------------------------------------------------------------------------- */
/* run hook */

int win32_run_hook(const char *dir, const char *name, const char *maybe_arg)
{
#define DOT_BAT L".bat"
	wchar_t cwd[PATH_MAX] = {L'\0'};
	if (!GetCurrentDirectoryW(PATH_MAX, cwd))
		return 0;

	wchar_t batch_file[PATH_MAX] = {L'\0'};

	{
		wchar_t* name_w = NULL;
		if (charset_iconv(name, &name_w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			return 0;

		size_t name_len = wcslen(name_w);
		if ((name_len * sizeof(wchar_t)) + sizeof(DOT_BAT) >= PATH_MAX) {
			free(name_w);
			return 0;
		}

		memcpy(batch_file, name_w, name_len * sizeof(wchar_t));
		memcpy(batch_file + name_len, DOT_BAT, sizeof(DOT_BAT));

		free(name_w);
	}

	{
		wchar_t* dir_w = NULL;
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
		wchar_t* maybe_arg_w = NULL;
		if (charset_iconv(maybe_arg, &maybe_arg_w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			return 0;

		struct stat sb;
		if (win32_wstat(batch_file, &sb) == -1) {
			r = 0;
		} else {
			const wchar_t *cmd;

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
}
