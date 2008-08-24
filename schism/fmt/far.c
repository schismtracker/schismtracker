/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://rigelseven.com/schism/
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

/* --------------------------------------------------------------------- */

int fmt_far_read_info(dmoz_file_t *file, const byte *data, size_t length)
{
        /* The magic for this format is truly weird (which I suppose is good, as the chance of it
        being "accidentally" correct is pretty low) */
        if (!(length > 47 && memcmp(data + 44, "\x0d\x0a\x1a", 3) == 0 && memcmp(data, "FAR\xfe", 4) == 0))
                return false;

        file->description = "Farandole Module";
        /*file->extension = str_dup("far");*/
        file->title = calloc(41, sizeof(char));
        memcpy(file->title, data + 4, 40);
        file->title[40] = 0;
        file->type = TYPE_MODULE_S3M;
        return true;
}
