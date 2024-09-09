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

#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>

#include "slurp.h"

static void _munmap_slurp(slurp_t *useme)
{
	(void)munmap((void *)useme->data, useme->length);
	(void)close(useme->extra);
}

int slurp_mmap(slurp_t *useme, const char *filename, size_t st)
{
	int fd;
	void *addr;

	fd = open(filename, O_RDONLY);
	if (fd == -1) return 0;

	addr = mmap(
		NULL, st, PROT_READ,
		MAP_SHARED
#if defined(MAP_POPULATE) && defined(MAP_NONBLOCK)
			| MAP_POPULATE | MAP_NONBLOCK
#endif
#if defined(MAP_NORESERVE)
			| MAP_NORESERVE
#endif
		,
		fd, 0);

	if (addr == MAP_FAILED) {
		(void)close(fd);
		return (errno == ENOMEM) ? 0 : -1;
	}

	useme->closure = _munmap_slurp;
	useme->length = st;
	useme->data = addr;
	useme->extra = fd;
	return 1;
}
