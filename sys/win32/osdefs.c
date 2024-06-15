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
#include "sdlmain.h"
#include "it.h"
#include "osdefs.h"
#include "fmt.h"

#include <windows.h>
#include <ws2tcpip.h>

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

void win32_sdlinit(void)
{
	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
}

int win32_sdlevent(SDL_Event* event)
{
	if (event->type != SDL_SYSWMEVENT)
		return 1;

	if (event->syswm.msg->msg.win.msg == WM_COMMAND) {
		SDL_Event e;
		e.type = SCHISM_EVENT_NATIVE;
		e.user.code = SCHISM_EVENT_NATIVE_SCRIPT;
		switch (LOWORD(event->syswm.msg->msg.win.wParam)) {
			case IDM_FILE_NEW:
				e.user.data1 = "new";
				break;
			case IDM_FILE_LOAD:
				e.user.data1 = "load";
				break;
			case IDM_FILE_SAVE_CURRENT:
				e.user.data1 = "save";
				break;
			case IDM_FILE_SAVE_AS:
				e.user.data1 = "save_as";
				break;
			case IDM_FILE_EXPORT:
				e.user.data1 = "export_song";
				break;
			case IDM_FILE_MESSAGE_LOG:
				e.user.data1 = "logviewer";
				break;
			case IDM_FILE_QUIT:
				e.type = SDL_QUIT;
				break;
			case IDM_PLAYBACK_SHOW_INFOPAGE:
				e.user.data1 = "info";
				break;
			case IDM_PLAYBACK_PLAY_SONG:
				e.user.data1 = "play";
				break;
			case IDM_PLAYBACK_PLAY_PATTERN:
				e.user.data1 = "play_pattern";
				break;
			case IDM_PLAYBACK_PLAY_FROM_ORDER:
				e.user.data1 = "play_order";
				break;
			case IDM_PLAYBACK_PLAY_FROM_MARK_CURSOR:
				e.user.data1 = "play_mark";
				break;
			case IDM_PLAYBACK_STOP:
				e.user.data1 = "stop";
				break;
			case IDM_PLAYBACK_CALCULATE_LENGTH:
				e.user.data1 = "calc_length";
				break;
			case IDM_SAMPLES_SAMPLE_LIST:
				e.user.data1 = "sample_page";
				break;
			case IDM_SAMPLES_SAMPLE_LIBRARY:
				e.user.data1 = "sample_library";
				break;
			case IDM_SAMPLES_RELOAD_SOUNDCARD:
				e.user.data1 = "init_sound";
				break;
			case IDM_INSTRUMENTS_INSTRUMENT_LIST:
				e.user.data1 = "inst_page";
				break;
			case IDM_INSTRUMENTS_INSTRUMENT_LIBRARY:
				e.user.data1 = "inst_library";
				break;
			case IDM_VIEW_HELP:
				e.user.data1 = "help";
				break;
			case IDM_VIEW_VIEW_PATTERNS:
				e.user.data1 = "pattern";
				break;
			case IDM_VIEW_ORDERS_PANNING:
				e.user.data1 = "orders";
				break;
			case IDM_VIEW_VARIABLES:
				e.user.data1 = "variables";
				break;
			case IDM_VIEW_MESSAGE_EDITOR:
				e.user.data1 = "message_edit";
				break;
			case IDM_VIEW_TOGGLE_FULLSCREEN:
				e.user.data1 = "fullscreen";
				break;
			case IDM_SETTINGS_PREFERENCES:
				e.user.data1 = "preferences";
				break;
			case IDM_SETTINGS_MIDI_CONFIGURATION:
				e.user.data1 = "midi_config";
				break;
			case IDM_SETTINGS_PALETTE_EDITOR:
				e.user.data1 = "palette_page";
				break;
			case IDM_SETTINGS_FONT_EDITOR:
				e.user.data1 = "font_editor";
				break;
			case IDM_SETTINGS_SYSTEM_CONFIGURATION:
				e.user.data1 = "system_config";
				break;
			default:
				break;
		}
		*event = e;
	}

	return 1;
}

void win32_toggle_menu(SDL_Window* window)
{
	const int flags = SDL_GetWindowFlags(window);
	int width, height;

	const int cache_size = !(flags & SDL_WINDOW_MAXIMIZED);
	if (cache_size)
		SDL_GetWindowSize(window, &width, &height);

	/* Get the HWND */
	SDL_SysWMinfo wm_info;
	SDL_VERSION(&wm_info.version);
	if (!SDL_GetWindowWMInfo(window, &wm_info))
		return;

	SetMenu(wm_info.info.win.window, (cfg_video_want_menu_bar && !(flags & SDL_WINDOW_FULLSCREEN)) ? menu : NULL);
	DrawMenuBar(wm_info.info.win.window);

	if (cache_size)
		SDL_SetWindowSize(window, width, height);
}
