/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010 Storlek
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

static void _win32_unmap(slurp_t *useme)
{
        HANDLE *bp;
        (void)UnmapViewOfFile((LPVOID)useme->data);

        bp = (HANDLE*)useme->bextra;
        CloseHandle(bp[0]);
        CloseHandle(bp[1]);
        free(bp);
}
int slurp_win32(slurp_t *useme, const char *filename, size_t st)
{
        HANDLE h, m, *bp;
        LPVOID addr;
        SECURITY_ATTRIBUTES sa;

        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = 0;

        bp = (HANDLE*)mem_alloc(sizeof(HANDLE)*2);

        h = CreateFile(filename, GENERIC_READ,
                        FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (!h) {
                log_appendf(4, "CreateFile(%s) failed with %lu", filename,
                                        GetLastError());
                free(bp);
                return 0;
        }

        m = CreateFileMapping(h, NULL, PAGE_READONLY, 0, 0, NULL);
        if (!h) {
                log_appendf(4, "CreateFileMapping failed with %lu", GetLastError());
                CloseHandle(h);
                free(bp);
                return -1;
        }
        addr = MapViewOfFile(m, FILE_MAP_READ, 0, 0, 0);
        if (!addr) {
                log_appendf(4, "MapViewOfFile failed with %lu", GetLastError());
                CloseHandle(m);
                CloseHandle(h);
                free(bp);
                return -1;
        }
        useme->data = addr;
        useme->length = st;
        useme->closure = _win32_unmap;

        bp[0] = m; bp[1] = h;
        useme->bextra = bp;
        return 1;
}
