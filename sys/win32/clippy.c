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

#include <windows.h>

static int win32_clippy_have_selection(void)
{
	return 0;
}

static int win32_clippy_have_clipboard(void)
{
	return 1;
}

static void win32_clippy_set_selection(const char *text)
{
	// win32 doesn't have this.
}

static void win32_clippy_set_clipboard(const char *text)
{
	int result = 0;

	UINT fmt = 0;

	if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
		fmt = CF_UNICODETEXT;
	} else if (IsClipboardFormatAvailable(CF_TEXT)) {
		fmt = CF_TEXT;
	} else {
		// welp
		return;
	}

	if (OpenClipboard(NULL)) {
		size_t i, size;

		/* Find out the size of the data */
		for (size = 0, i = 0; text[i]; ++i, ++size)
			if (text[i] == '\n' && (i == 0 || text[i - 1] != '\r'))
				/* We're going to insert a carriage return */
				size++;

		size = (size + 1) * (fmt == CF_UNICODETEXT) ? sizeof(wchar_t) : sizeof(char);

		/* Save the data to the clipboard */
		HANDLE mem = GlobalAlloc(GMEM_MOVEABLE, size);
		if (mem) {
			if (fmt == CF_UNICODETEXT) {
				LPWSTR unicode;
				if (!charset_iconv(text, &unicode, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX)) {
					LPWSTR dst = (LPWSTR)GlobalLock(mem);
					if (dst) {
						/* Copy the text over, adding carriage returns as necessary */
						for (i = 0; unicode[i]; ++i)
							if (unicode[i] == L'\n' && (i == 0 || unicode[i - 1] != L'\r'))
								*dst++ = L'\r';
							*dst++ = unicode[i];
						*dst = L'\0';
						GlobalUnlock(mem);
					}
					free(unicode);
				}
			} else if (fmt == CF_TEXT) {
				LPSTR ansi;
				if (!charset_iconv(text, &ansi, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)) {
					LPSTR dst = (LPSTR)GlobalLock(mem);
					if (dst) {
						/* Copy the text over, adding carriage returns as necessary */
						for (i = 0; ansi[i]; ++i) {
							if (ansi[i] == '\n' && (i == 0 || ansi[i - 1] != '\r'))
								*dst++ = '\r';
							*dst++ = ansi[i];
						}
						*dst = '\0';
						GlobalUnlock(mem);
					}
					free(ansi);
				}
			}

			EmptyClipboard();
			SetClipboardData(fmt, mem);
		}

		CloseClipboard();
	}
}

static char *win32_clippy_get_selection(void)
{
	return str_dup("");
}

static char *win32_clippy_get_clipboard(void)
{
	char *text = NULL;
	UINT fmt;
	int i;

	if (IsClipboardFormatAvailable(CF_UNICODETEXT)) {
		fmt = CF_UNICODETEXT;
	} else if (IsClipboardFormatAvailable(CF_TEXT)) {
		fmt = CF_TEXT;
	} else {
		return str_dup("");
	}

	// try a couple times to open the clipboard
	for (i = 0; i < 5; i++) {
		if (OpenClipboard(NULL)) {
			HANDLE mem = GetClipboardData(fmt);
			SIZE_T len = GlobalSize(mem);
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
	if (!IsClipboardFormatAvailable(CF_UNICODETEXT)
		&& !IsClipboardFormatAvailable(CF_TEXT))
		return 0;

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
