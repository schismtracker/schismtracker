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
#include "sdlmain.h"
#include "it.h"
#include "osdefs.h"
#include "fmt.h"
#include "charset.h"

#include <windows.h>
#include <ws2tcpip.h>
#include <process.h>
#include <shlobj.h>

/* global menu object */
static HMENU menu = NULL;

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
		struct keybinds_menu_item *i = keybinds_menu_find_item_from_id(LOWORD(event->syswm.msg->msg.win.wParam));
		if (!i)
			return 1;

		keybinds_menu_item_pressed(i, event);
	}

	return 1;
}

void win32_toggle_menu(SDL_Window* window, int yes)
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

void win32_create_menu(void) {
	struct keybinds_menu **mm;
	menu = CreateMenu();

	for (mm = keybinds_menus; (*mm)->type != KEYBINDS_MENU_NULL; mm++) {
		struct keybinds_menu *m = *mm;
		if (m->type != KEYBINDS_MENU_REGULAR && m->type != KEYBINDS_MENU_APPLE)
			continue;

		HMENU submenu = CreatePopupMenu();

		for (struct keybinds_menu_item *i = m->items; i->type != KEYBINDS_MENU_ITEM_NULL; i++) {
			switch (i->type) {
			case KEYBINDS_MENU_ITEM_REGULAR: {
				char *never_back_down_never_what = STR_CONCAT(3, i->info.regular.name, "\t", i->info.regular.bind->shortcut_text);
				wchar_t *str = NULL;
				charset_iconv((const uint8_t *)never_back_down_never_what, (uint8_t **)&str, CHARSET_UTF8, CHARSET_WCHAR_T);
				free(never_back_down_never_what);
				AppendMenuW(submenu, MF_STRING, i->info.regular.id, str);
				free(str);
				break;
			}
			case KEYBINDS_MENU_ITEM_SEPARATOR:
				AppendMenuW(submenu, MF_SEPARATOR, 0, NULL);
				break;
			default:
				break;
			}
		}

		wchar_t *str = NULL;
		charset_iconv((const uint8_t *)m->info.regular.name, (uint8_t **)&str, CHARSET_UTF8, CHARSET_WCHAR_T);
		AppendMenuW(menu, MF_POPUP, (uintptr_t)submenu, str);
		free(str);
	}
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
	if (charset_iconv((const uint8_t*)template, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T))
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
	if (charset_iconv((const uint8_t*)path, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T))
		return -1;

	int ret = win32_wstat(wc, st);
	free(wc);
	return ret;
}

int win32_open(const char* path, int flags)
{
	wchar_t* wc = NULL;
	if (charset_iconv((const uint8_t*)path, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T))
		return -1;

	int ret = _wopen(wc, flags);
	free(wc);
	return ret;
}

FILE* win32_fopen(const char* path, const char* flags)
{
	wchar_t* wc = NULL, *wc_flags = NULL;
	if (charset_iconv((const uint8_t*)path, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T)
		|| charset_iconv((const uint8_t*)flags, (uint8_t**)&wc_flags, CHARSET_UTF8, CHARSET_WCHAR_T))
		return NULL;

	FILE* ret = _wfopen(wc, wc_flags);
	free(wc);
	free(wc_flags);
	return ret;
}

int win32_mkdir(const char *path, UNUSED mode_t mode)
{
	wchar_t* wc = NULL;
	if (charset_iconv((const uint8_t*)path, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T))
		return -1;

	int ret = _wmkdir(wc);
	free(wc);
	return ret;
}

/* ------------------------------------------------------------------------------- */
/* run hook */

int win32_run_hook(const char *dir, const char *name, const char *maybe_arg)
{
	wchar_t cwd[PATH_MAX] = {L'\0'};
	const wchar_t *cmd = NULL;
	wchar_t batch_file[PATH_MAX] = {L'\0'};
	struct stat sb = {0};
	int r;

	if (!GetCurrentDirectoryW(PATH_MAX-1, cwd))
		return 0;

	wchar_t* name_w = NULL;
	if (charset_iconv((const uint8_t*)name, (uint8_t**)&name_w, CHARSET_UTF8, CHARSET_WCHAR_T))
		return 0;

	size_t name_len = wcslen(name_w);
	wcsncpy(batch_file, name_w, name_len);
	wcscpy(&batch_file[name_len], L".bat");

	free(name_w);

	wchar_t* dir_w = NULL;
	if (charset_iconv((const uint8_t*)dir, (uint8_t**)&dir_w, CHARSET_UTF8, CHARSET_WCHAR_T))
		return 0;

	if (_wchdir(dir_w) == -1) {
		free(dir_w);
		return 0;
	}

	free(dir_w);

	wchar_t* maybe_arg_w = NULL;
	if (charset_iconv((const uint8_t*)maybe_arg, (uint8_t**)&maybe_arg_w, CHARSET_UTF8, CHARSET_WCHAR_T))
		return 0;

	if (win32_wstat(batch_file, &sb) == -1) {
		r = 0;
	} else {
		cmd = _wgetenv(L"COMSPEC");
		if (!cmd)
			cmd = L"command.com";

		r = _wspawnlp(_P_WAIT, cmd, cmd, "/c", batch_file, maybe_arg_w, 0);
	}

	free(maybe_arg_w);

	_wchdir(cwd);
	if (r == 0) return 1;
	return 0;
}
