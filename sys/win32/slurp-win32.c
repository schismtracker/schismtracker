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
#include "loadso.h"
#include "osdefs.h"

static void win32_unmap_(slurp_t *slurp)
{
	if (slurp->internal.memory.data) {
		UnmapViewOfFile(slurp->internal.memory.data);
		slurp->internal.memory.data = NULL;
	}

	// collect and free all of the handles
	HANDLE *handles[] = {
		&slurp->internal.memory.interfaces.win32.file,
		&slurp->internal.memory.interfaces.win32.mapping,
	};

	for (int i = 0; i < ARRAY_SIZE(handles); i++) {
		if (*handles[i] != NULL && *handles[i] != INVALID_HANDLE_VALUE)
			CloseHandle(*handles[i]);

		*handles[i] = NULL;
	}
}

static inline HANDLE CreateFileUTF8(const char *filename, DWORD a, DWORD b, LPSECURITY_ATTRIBUTES c, DWORD d, DWORD e, HANDLE f)
{
	HANDLE h;

	SCHISM_ANSI_UNICODE({
		// Windows 9x
		char *filename_a;
		if (charset_iconv(filename, &filename_a, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			return INVALID_HANDLE_VALUE;

		h = CreateFileA(filename_a, a, b, c, d, e, f);
		free(filename_a);
	}, {
		wchar_t *filename_w;
		if (charset_iconv(filename, &filename_w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			return INVALID_HANDLE_VALUE;

		h = CreateFileW(filename_w, a, b, c, d, e, f);
		free(filename_w);
	})

	return h;
}

// this function name is misleading now
static int win32_error_unmap_(slurp_t *slurp, const char *filename, const char *function, int val)
{
	/* this could be moved to osdefs.c  --paper */
	DWORD err = GetLastError();
	char *ptr = NULL;

	SCHISM_ANSI_UNICODE({
		LPSTR errmsg = NULL;
		FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
				err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&errmsg, 0, NULL);
		charset_iconv(errmsg, &ptr, CHARSET_ANSI, CHARSET_UTF8, SIZE_MAX);
		LocalFree(errmsg);
	}, {
		LPWSTR errmsg = NULL;
		FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
				err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&errmsg, 0, NULL);
		charset_iconv(errmsg, &ptr, CHARSET_WCHAR_T, CHARSET_UTF8, SIZE_MAX);
		LocalFree(errmsg);
	})

	// I don't particularly want to split this stuff onto two lines, but
	// it's the only way to make the error message readable in some cases
	// (though no matter what, the message is still probably going to be
	// truncated because Windows is excessively verbose)
	log_appendf(4, "%s: %s: error %lu:", filename, function, err);
	if (ptr) {
		log_appendf(4, "  %s", ptr);
		free(ptr);
	}

	//win32_unmap_(slurp);
	return val;
}

int slurp_win32_mmap(slurp_t *slurp, const char *filename, size_t st)
{
	/* updated this to hopefully have no possible race conditions regarding the
	 * actual size of the memory mapping. if older versions of windows don't support
	 * it, then we can simply fall back to the regular file functions  --paper */
	LPVOID data;
	DWORD hi, lo;
	HANDLE file;
	HANDLE mapping;

	file = CreateFileUTF8(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return win32_error_unmap_(slurp, filename, "CreateFile", SLURP_OPEN_FAIL);

	hi = (st >> 32);
	lo = st; /* cast truncates to 32bits */

	// These functions are stubs on Windows 95 & 98, so simply ignore if
	// they fail and fall back to the regular file implementation
	mapping = CreateFileMapping(file, NULL, PAGE_READONLY, hi, lo, NULL);
	if (!mapping) {
		CloseHandle(file);
		return SLURP_OPEN_IGNORE;
	}

	data = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, st);
	if (!data) {
		CloseHandle(mapping);
		CloseHandle(file);
		return SLURP_OPEN_IGNORE;
	}

	slurp_memstream(slurp, data, st);

	slurp->closure = win32_unmap_;

	slurp->internal.memory.interfaces.win32.file = file;
	slurp->internal.memory.interfaces.win32.mapping = mapping;

	return SLURP_OPEN_SUCCESS;
}

/* --------------------------------------------------------------------- */

static int slurp_win32_seek_(slurp_t *t, int64_t offset, int whence)
{
	union {
		LONG l[2];
		int64_t x;
	} x;
	DWORD move;
	DWORD r;

	x.x = offset;

	switch (whence) {
	case SEEK_SET: move = FILE_BEGIN; break;
	case SEEK_CUR: move = FILE_CURRENT; break;
	case SEEK_END: move = FILE_END; break;
	default: return -1;
	}

	return (SetFilePointer(t->internal.win32.handle, x.l[0], &x.l[1], move) == INVALID_SET_FILE_POINTER) ? -1 : 0;
}

static int64_t slurp_win32_tell_(slurp_t *t)
{
	LONG x = 0;
	DWORD r;

	r = SetFilePointer(t->internal.win32.handle, 0, &x, FILE_CURRENT);
	if (r == INVALID_SET_FILE_POINTER)
		return -1;

	return (int64_t)x << 32 | r;
}

static uint64_t slurp_win32_length_(slurp_t *t)
{
	DWORD lo, hi;

	lo = GetFileSize(t->internal.win32.handle, &hi);
	if (lo == INVALID_FILE_SIZE && GetLastError() != NO_ERROR)
		return 0; /* eh */

	return (uint64_t)hi << 32 | lo;
}

static size_t slurp_win32_read_(slurp_t *t, void *ptr, size_t count)
{
	DWORD bytes_read;

	if (!ReadFile(t->internal.win32.handle, ptr, count, &bytes_read, NULL))
		return 0;

	/* ok */
	return bytes_read;
}

static void slurp_win32_closure_(slurp_t *t)
{
	CloseHandle(t->internal.win32.handle);
}

int slurp_win32(slurp_t *t, const char *filename, SCHISM_UNUSED size_t size)
{
	t->internal.win32.handle = CreateFileUTF8(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (t->internal.win32.handle == INVALID_HANDLE_VALUE)
		return SLURP_OPEN_FAIL;

	t->seek = slurp_win32_seek_;
	t->tell = slurp_win32_tell_;
	t->length = slurp_win32_length_;
	t->read = slurp_win32_read_;
	t->closure = slurp_win32_closure_;

	return SLURP_OPEN_SUCCESS;
}
