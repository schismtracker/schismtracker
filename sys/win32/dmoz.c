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

#include "backend/dmoz.h"
#include "loadso.h"
#include "charset.h"
#include "dmoz.h"
#include "util.h"

#include <windows.h>

static char *win32_dmoz_get_exe_path(void)
{
	char *utf8 = NULL;

	if (GetVersion() & UINT32_C(0x80000000)) {
		// Windows 9x
		char path[MAX_PATH];

		if (GetModuleFileNameA(NULL, path, ARRAY_SIZE(path)))
			charset_iconv(path, &utf8, CHARSET_ANSI, CHARSET_UTF8, sizeof(path));
	} else {
		// Windows NT. This uses dynamic allocation to account for e.g. UNC paths.
		DWORD pathsize = MAX_PATH;
		WCHAR *path = NULL;

		for (;;) {
			{
				void *new = mem_realloc(path, pathsize * sizeof(*path));
				if (!new) {
					free(path);
					return NULL;
				}

				path = new;
			}

			DWORD len = GetModuleFileNameW(NULL, path, pathsize);
			if (len < pathsize - 1)
				break;

			pathsize *= 2;
		}

		charset_iconv(path, &utf8, CHARSET_WCHAR_T, CHARSET_UTF8, pathsize * sizeof(*path));

		free(path);
	}

	if (utf8) {
		char *parent = dmoz_path_get_parent_directory(utf8);
		free(utf8);
		if (parent)
			return parent;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////////
// init/quit

static int win32_dmoz_init(void)
{
	// nothing to do
	return 1;
}

static void win32_dmoz_quit(void)
{
	// nothing
}

const schism_dmoz_backend_t schism_dmoz_backend_win32 = {
	.init = win32_dmoz_init,
	.quit = win32_dmoz_quit,

	.get_exe_path = win32_dmoz_get_exe_path,
};