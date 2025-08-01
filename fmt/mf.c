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

int fmt_mf_read_info(dmoz_file_t *file, slurp_t *fp)
{
	unsigned char moonfish[8];
	if (slurp_read(fp, moonfish, sizeof(moonfish)) != sizeof(moonfish)
		|| memcmp(moonfish, "MOONFISH", sizeof(moonfish)))
		return 0;

	unsigned char title[32];

	slurp_seek(fp, 25, SEEK_CUR);

	int title_length = slurp_getc(fp);
	if (title_length == EOF)
		return 0;

	title_length = MIN((int)sizeof(title), title_length);

	if (slurp_read(fp, title, title_length) != (size_t)title_length)
		return 0;

	file->description = "MoonFish";
	/*file->extension = str_dup("mf");*/
	file->title = strn_dup((const char *)title, title_length);
	file->type = TYPE_MODULE_MOD;    /* ??? */
	return 1;
}

