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

#include "headers.h"

#include "backend/clippy.h"
#include "loadso.h"
#include "charset.h"
#include "mem.h"
#include "video.h"
#include "util.h"
#include "osdefs.h"

#include <windows.h>

static int win32_clippy_have_selection(void)
{
	return 0;
}

static int win32_clippy_have_clipboard(void)
{
	return IsClipboardFormatAvailable(CF_UNICODETEXT) || IsClipboardFormatAvailable(CF_TEXT);
}

static void win32_clippy_set_selection(const char *text)
{
	// win32 doesn't have this.
}

static void win32_clippy_set_clipboard(const char *text)
{
	// Only use CF_UNICODETEXT on Windows NT machines
#ifdef SCHISM_WIN32_COMPILE_ANSI
	const UINT fmt = (GetVersion() & UINT32_C(0x80000000)) ? CF_TEXT : CF_UNICODETEXT;
#else
	static const UINT fmt = CF_UNICODETEXT;
#endif
	union {
		LPWSTR w;
#ifdef SCHISM_WIN32_COMPILE_ANSI
		LPSTR a;
#endif
	} str;
	size_t i;
	size_t size = 0;

	video_wm_data_t wm_data;
	if (!video_get_wm_data(&wm_data) && wm_data.subsystem != VIDEO_WM_DATA_SUBSYSTEM_WINDOWS)
		return;

	if (!OpenClipboard(wm_data.data.windows.hwnd))
		return;

	// Convert from LF to CRLF
	if (fmt == CF_UNICODETEXT && !charset_iconv(text, &str.w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX)) {
		for (i = 0; str.w[i]; i++, size++)
			if (str.w[i] == L'\n' && (i == 0 || str.w[i - 1] != L'\r'))
				size++;
#ifdef SCHISM_WIN32_COMPILE_ANSI
	} else if (fmt == CF_TEXT && !charset_iconv(text, &str.a, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)) {
		for (i = 0; str.a[i]; i++, size++)
			if (str.a[i] == '\n' && (i == 0 || str.a[i - 1] != '\r'))
				size++;
#endif
	} else {
		// give up
		return;
	}

	size = (size + 1) * ((fmt == CF_UNICODETEXT) ? sizeof(wchar_t) : sizeof(char));

	// Copy the result to the clipboard
	HANDLE mem = GlobalAlloc(GMEM_MOVEABLE, size);
	if (mem) {
		if (fmt == CF_UNICODETEXT) {
			wchar_t *dst = (wchar_t *)GlobalLock(mem);
			if (dst) {
				for (i = 0; str.w[i]; i++) {
					if (str.w[i] == L'\n' && (i == 0 || str.w[i - 1] != L'\r'))
						*dst++ = L'\r';
					*dst++ = str.w[i];
				}
				*dst = L'\0';
				GlobalUnlock(mem);
			}
			free(str.w);
		}
#ifdef SCHISM_WIN32_COMPILE_ANSI
		else if (fmt == CF_TEXT) {
			char *dst = (char *)GlobalLock(mem);
			if (dst) {
				for (i = 0; str.a[i]; i++) {
					if (str.a[i] == '\n' && (i == 0 || str.a[i - 1] != '\r'))
						*dst++ = '\r';
					*dst++ = str.a[i];
				}
				*dst = '\0';
				GlobalUnlock(mem);
			}
			free(str.a);
		}
#endif

		if (!EmptyClipboard() || !SetClipboardData(fmt, mem))
			GlobalFree(mem);
	}

	CloseClipboard();
}

static char *win32_clippy_get_selection(void)
{
	// doesn't exist, ever
	return str_dup("");
}

static char *win32_clippy_get_clipboard(void)
{
	char *text = NULL;
	int i;
	int fmt;

	video_wm_data_t wm_data;
	if (!video_get_wm_data(&wm_data) && wm_data.subsystem != VIDEO_WM_DATA_SUBSYSTEM_WINDOWS)
		return str_dup("");

#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (GetVersion() & UINT32_C(0x80000000)) {
		// Believe it or not, CF_UNICODETEXT *does* actually work on
		// Windows 95. However, practically every application that runs
		// will completely ignore it and just use CF_TEXT instead.
		fmt = CF_TEXT;
	} else
#endif
	{
		UINT formats[] = {CF_UNICODETEXT, CF_TEXT};
		fmt = GetPriorityClipboardFormat(formats, ARRAY_SIZE(formats));
		if (fmt < 0)
			return str_dup("");
	}

	// try a couple times to open the clipboard
	for (i = 0; i < 5; i++) {
		if (OpenClipboard(wm_data.data.windows.hwnd)) {
			HANDLE mem = GetClipboardData(fmt);
			SIZE_T len = GlobalSize(mem); // no overflow!
			if (mem) {
				if (fmt == CF_UNICODETEXT) {
					LPWSTR str = (LPWSTR)GlobalLock(mem);
					if (str)
						charset_iconv(str, &text, CHARSET_WCHAR_T, CHARSET_UTF8, len);
				} else if (fmt == CF_TEXT) {
					LPSTR str = (LPSTR)GlobalLock(mem);
					if (str)
						charset_iconv(str, &text, CHARSET_ANSI, CHARSET_UTF8, len);
				}
				GlobalUnlock(mem);
			}
			CloseClipboard();
			break;
		}
		SleepEx(10, FALSE);
	}

	return text ? text : str_dup("");
}

static int win32_clippy_init(void)
{
	// nothing to do
	return 1;
}

static void win32_clippy_quit(void)
{
	// nothing to do
}

const schism_clippy_backend_t schism_clippy_backend_win32 = {
	.init = win32_clippy_init,
	.quit = win32_clippy_quit,

	.have_selection = win32_clippy_have_selection,
	.get_selection = win32_clippy_get_selection,
	.set_selection = win32_clippy_set_selection,

	.have_clipboard = win32_clippy_have_clipboard,
	.get_clipboard = win32_clippy_get_clipboard,
	.set_clipboard = win32_clippy_set_clipboard,
};
