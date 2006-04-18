/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
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

/* FIXME: MMCMP isn't IT-specific, and I know nothing about it */

int fmt_it_read_info(dmoz_file_t *file, const byte *data, size_t length)
{
        int mmcmp;

        /* "Bart just said I-M-P! He's made of pee!" */
        if (length > 30 && memcmp(data, "IMPM", 4) == 0) {
                mmcmp = false;
		/* This ought to be more particular; if it's not actually made *with* Impulse Tracker,
		it's probably not compressed, irrespective of what the CMWT says. */
                if (data[42] >= 0x14)
                        file->description = "Compressed Impulse Tracker";
                else
                        file->description = "Impulse Tracker";
        } else if (length > 164 && memcmp(data + 132, "IMPM", 4) == 0 && memcmp(data, "ziRCONia", 8) == 0) {
                mmcmp = true;
                file->description = "Impulse Tracker";
        } else {
                return false;
        }

        /*file->extension = strdup("it");*/
        file->title = calloc(26, sizeof(char));
        memcpy(file->title, data + (mmcmp ? 136 : 4), 25);
        file->title[25] = 0;
        file->type = TYPE_MODULE_IT;
        return true;
}
