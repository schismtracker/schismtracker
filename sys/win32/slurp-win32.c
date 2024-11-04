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

#ifndef SCHISM_WIN32
# error You are not on Windows. What are you doing?
#endif

#include <windows.h>
#include <sys/stat.h>

#include "util.h"
#include "log.h"
#include "slurp.h"
#include "charset.h"

static void win32_unmap_(slurp_t *slurp)
{
	if (slurp->internal.memory.data != NULL) {
		UnmapViewOfFile(slurp->internal.memory.data);
		slurp->internal.memory.data = NULL;
	}

	if (slurp->internal.memory.interfaces.win32.file != INVALID_HANDLE_VALUE)
		CloseHandle(slurp->internal.memory.interfaces.win32.file);

	if (slurp->internal.memory.interfaces.win32.mapping != NULL)
		CloseHandle(slurp->internal.memory.interfaces.win32.mapping);

	slurp->internal.memory.interfaces.win32.file = NULL;
	slurp->internal.memory.interfaces.win32.mapping = NULL;
}

// This reader used to return -1 sometimes, which is kind of a hack to tell the
// the rest of the loading code to try some other means of opening the file,
// which on win32 is basically just fopen + malloc + fread. If MapViewOfFile
// won't work, chances are pretty good that stdio is going to fail as well, so
// I'm just writing these cases off as every bit as unrecoverable as if the
// file didn't exist.
// Note: this doesn't bother setting errno; maybe it should?

static int win32_error_unmap_(slurp_t *slurp, const char *filename, const char *function)
{
	DWORD err = GetLastError();
	LPWSTR errmsg = NULL;
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
		      err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), errmsg, 0, NULL);
	// I don't particularly want to split this stuff onto two lines, but
	// it's the only way to make the error message readable in some cases
	// (though no matter what, the message is still probably going to be
	// truncated because Windows is excessively verbose)
	log_appendf(4, "%s: %s: error %lu:", filename, function, err);
	if (errmsg) {
		log_appendf(4, "  %ls", errmsg);
		LocalFree(errmsg);
	}
	win32_unmap_(slurp);
	return 0;
}

int slurp_win32(slurp_t *slurp, const char *filename, size_t st)
{
	wchar_t* filename_w = NULL;
	if (charset_iconv((const uint8_t *)filename, (uint8_t **)&filename_w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
		return win32_error_unmap_(slurp, filename, "MultiByteToWideChar");

	slurp->internal.memory.interfaces.win32.file = CreateFileW(filename_w, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	free(filename_w);
	if (slurp->internal.memory.interfaces.win32.file == INVALID_HANDLE_VALUE)
		return win32_error_unmap_(slurp, filename, "CreateFileW");

	slurp->internal.memory.interfaces.win32.mapping = CreateFileMapping(slurp->internal.memory.interfaces.win32.file, NULL, PAGE_READONLY, 0, 0, NULL);
	if (!slurp->internal.memory.interfaces.win32.mapping)
		return win32_error_unmap_(slurp, filename, "CreateFileMapping");

	slurp->internal.memory.data = MapViewOfFile(slurp->internal.memory.interfaces.win32.mapping, FILE_MAP_READ, 0, 0, 0);
	if (!slurp->internal.memory.data)
		return win32_error_unmap_(slurp, filename, "MapViewOfFile");

	slurp->internal.memory.length = st;
	slurp->closure = win32_unmap_;
	return 1;
}
