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
#include "bswap.h"
#include "fmt.h"
#include "util.h"
#include "mem.h"

static int edl_load_file(slurp_t *fp, slurp_t *fakefp)
{
	unsigned char id[4];
	if (slurp_read(fp, id, sizeof(id)) != sizeof(id))
		return 0;

	// these are here in every EDL file I can find
	if (memcmp(id, "\x00\x06\xFE\xFD", 4))
		return 0;

	// go back to the beginning
	slurp_seek(fp, 0, SEEK_SET);

	disko_t memdisko = {0};
	if (disko_memopen(&memdisko) < 0)
		return 0;

	if (huffman_decompress(fp, &memdisko)) {
		disko_memclose(&memdisko, 0);
		return 0;
	}

	// EDL files are basically just a dump of whatever is in memory.
	// This means that it's very easy to check whether a given EDL
	// file is legitimate or not by just comparing the length to
	// a magic value.
	if (memdisko.length != 195840 && memdisko.length != 179913) {
		disko_memclose(&memdisko, 0);
		return 0;
	}

	if (slurp_memstream_free(fakefp, memdisko.data, memdisko.length)) {
		disko_memclose(&memdisko, 0);
		return 0;
	}

	disko_memclose(&memdisko, 1);
	return 1;
}

int fmt_edl_read_info(dmoz_file_t *file, slurp_t *fp)
{
	slurp_t fakefp = {0};
	if (!edl_load_file(fp, &fakefp))
		return 0;

	slurp_seek(&fakefp, 0x1FE0B, SEEK_SET);

	unsigned char title[32];
	if (slurp_read(&fakefp, title, sizeof(title)) != sizeof(title)) {
		unslurp(&fakefp);
		return 0;
	}

	file->title = strn_dup(title, sizeof(title));
	file->description = "EdLib Tracker EDL";
	file->type = TYPE_MODULE_S3M;

	unslurp(&fakefp);

	return 1;
}
