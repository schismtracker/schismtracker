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

int fmt_liq_read_info(dmoz_file_t *file, slurp_t *fp)
{
	unsigned char magic1[14], artist[20], title[30];

	if (slurp_read(fp, magic1, sizeof(magic1)) != sizeof(magic1)
		|| memcmp(magic1, "Liquid Module:", sizeof(magic1)))
		return 0;

	slurp_seek(fp, 64, SEEK_SET);
	int magic2 = slurp_getc(fp);
	if (magic2 != 0x1a)
		return 0;

	slurp_seek(fp, 14, SEEK_SET);
	if (slurp_read(fp, title, sizeof(title)) != sizeof(title))
		return 0;

	slurp_seek(fp, 44, SEEK_SET);
	if (slurp_read(fp, artist, sizeof(artist)) != sizeof(artist))
		return 0;

	file->description = "Liquid Tracker";
	/*file->extension = str_dup("liq");*/
	file->artist = strn_dup((const char *)title, sizeof(title));
	file->title = strn_dup((const char *)artist, sizeof(artist));
	file->type = TYPE_MODULE_S3M;

	return 1;
}
