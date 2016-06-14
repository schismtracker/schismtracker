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

#include "config.h"
#ifndef WIN32
# error You are not on Windows. What are you doing?
#endif

/* FIXME | this really ought to just provide an mmap() wrapper
   FIXME | instead of reimplementing everything separately */

#include <windows.h>
#include <sys/stat.h>

#include "log.h"
#include "slurp.h"

// indices for 'h' (handles)
enum { FILE_HANDLE = 0, MAPPING_HANDLE = 1 };

static void _win32_unmap(slurp_t *slurp)
{
        if (slurp->data != NULL) {
                UnmapViewOfFile(slurp->data);
                slurp->data = NULL;
        }

        HANDLE *h = slurp->bextra;
        if (h[FILE_HANDLE] != INVALID_HANDLE_VALUE) {
                CloseHandle(h[FILE_HANDLE]);
        }
        if (h[MAPPING_HANDLE] != NULL) {
                CloseHandle(h[MAPPING_HANDLE]);
        }
        free(h);
        slurp->bextra = NULL;
}

// This reader used to return -1 sometimes, which is kind of a hack to tell the
// the rest of the loading code to try some other means of opening the file,
// which on win32 is basically just fopen + malloc + fread. If MapViewOfFile
// won't work, chances are pretty good that stdio is going to fail as well, so
// I'm just writing these cases off as every bit as unrecoverable as if the
// file didn't exist.
// Note: this doesn't bother setting errno; maybe it should?

static int _win32_error_unmap(slurp_t *slurp, const char *filename, const char *function)
{
        DWORD err = GetLastError();
        LPTSTR errmsg;
        FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), &errmsg, 0, NULL);
        // I don't particularly want to split this stuff onto two lines, but
        // it's the only way to make the error message readable in some cases
        // (though no matter what, the message is still probably going to be
        // truncated because Windows is excessively verbose)
        log_appendf(4, "%s: %s: error %lu:", filename, function, err);
        log_appendf(4, "  %s", errmsg);
        LocalFree(errmsg);
        _win32_unmap(slurp);
        return 0;
}

int slurp_win32(slurp_t *slurp, const char *filename, size_t st)
{
        LPVOID addr;
        HANDLE *h = slurp->bextra = mem_alloc(sizeof(HANDLE) * 2);

        if ((h[FILE_HANDLE] = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                                         FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE) {
                return _win32_error_unmap(slurp, filename, "CreateFile");
        }
        if ((h[MAPPING_HANDLE] = CreateFileMapping(h[FILE_HANDLE], NULL, PAGE_READONLY, 0, 0, NULL)) == NULL) {
                return _win32_error_unmap(slurp, filename, "CreateFileMapping");
        }
        if ((slurp->data = MapViewOfFile(h[MAPPING_HANDLE], FILE_MAP_READ, 0, 0, 0)) == NULL) {
                return _win32_error_unmap(slurp, filename, "MapViewOfFile");
        }
        slurp->length = st;
        slurp->closure = _win32_unmap;
        return 1;
}
