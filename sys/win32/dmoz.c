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

#include <windows.h>

// XXX Is this necessary?
static DWORD (WINAPI *WIN32_GetModuleFileNameW)(HMODULE hModule, LPWSTR lpFilename, DWORD nSize);
static DWORD (WINAPI *WIN32_GetModuleFileNameA)(HMODULE hModule, LPSTR  lpFilename, DWORD nSize);

static char *win32_dmoz_get_exe_path(void)
{
	if (GetVersion() & UINT32_C(0x80000000)) {
		// Windows 9x
		char path[PATH_MAX];
		char *utf8;

		if (WIN32_GetModuleFileNameA && WIN32_GetModuleFileNameA(NULL, path, PATH_MAX) && !charset_iconv(path, &utf8, CHARSET_ANSI, CHARSET_UTF8, sizeof(path)))
			return utf8;
	} else {
		// Windows NT
		wchar_t path[PATH_MAX];
		char *utf8;

		if (WIN32_GetModuleFileNameW && WIN32_GetModuleFileNameW(NULL, path, PATH_MAX) && !charset_iconv(path, &utf8, CHARSET_WCHAR_T, CHARSET_UTF8, sizeof(path)))
			return utf8;
	}

	return NULL;
}

//////////////////////////////////////////////////////////////////////////////
// dynamic loading

static void *lib_kernel32 = NULL;

static int win32_dmoz_init(void)
{
	lib_kernel32 = loadso_object_load("KERNEL32.DLL");
	if (lib_kernel32) {
		WIN32_GetModuleFileNameA = loadso_function_load(lib_kernel32, "GetModuleFileNameA");
		WIN32_GetModuleFileNameW = loadso_function_load(lib_kernel32, "GetModuleFileNameW");
	}

	return 1;
}

static void win32_dmoz_quit(void)
{
	if (lib_kernel32)
		loadso_object_unload(lib_kernel32);
}

const schism_dmoz_backend_t schism_dmoz_backend_win32 = {
	.init = win32_dmoz_init,
	.quit = win32_dmoz_quit,

	.get_exe_path = win32_dmoz_get_exe_path,
};