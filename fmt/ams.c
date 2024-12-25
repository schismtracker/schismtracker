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
#include "fmt.h"
#include "mem.h"

/* --------------------------------------------------------------------- */

/* TODO: test this code.
Modplug seems to have a totally different idea of ams than this.
I don't know what this data's supposed to be for :) */

/* btw: AMS stands for "Advanced Module System" */

int fmt_ams_read_info(dmoz_file_t *file, slurp_t *fp)
{
	unsigned char magic[7] = {0};
	slurp_read(fp, &magic, sizeof(magic));

	if (!(slurp_length(fp) > 38 && memcmp(magic, "AMShdr\x1a", 7) == 0))
		return 0;

	slurp_seek(fp, 7, SEEK_SET);
	int n = slurp_getc(fp);
	n = CLAMP(n, 0, 30);

	unsigned char title[30] = {0};
	slurp_read(fp, &title, n);

	file->description = "Velvet Studio";
	/*file->extension = str_dup("ams");*/
	file->title = strn_dup((const char *)title, n);
	file->type = TYPE_MODULE_XM;

	return 1;
}
