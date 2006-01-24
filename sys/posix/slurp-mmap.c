/*
 * slurp - mmap isolation
 * copyright (c) 2003-2005 chisel <storlek@chisel.cjb.net>
 * URL: http://rigelseven.com/
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#if HAVE_MMAP
#include <sys/types.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>

#include "slurp.h"

static void _munmap_slurp(slurp_t *useme)
{
	(void)munmap((void*)useme->data, useme->length);
	(void)close(useme->extra);
}

int slurp_mmap(slurp_t *useme, const char *filename, size_t st)
{
	int fd;
	void *addr;

	fd = open(filename, O_RDONLY);
	if (fd == -1) return 0;
	addr = mmap(0, st, PROT_READ, MAP_SHARED, fd, 0);
	if (!addr || addr == ((void*)-1)) {
		(void)close(fd);
		return -1;
	}
	useme->closure = _munmap_slurp;
	useme->length = st;
	useme->data = addr;
	useme->extra = fd;
	return 1;
}

#endif
