/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
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

#include "title.h"

/* --------------------------------------------------------------------- */

/* FIXME: MMCMP isn't IT-specific, and I know nothing about it */

bool fmt_it_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_it_read_info(const byte * data, size_t length, file_info * fi)
{
        bool mmcmp;

        /* "Bart just said I-M-P! He's made of pee!" */
        if (length > 30 && memcmp(data, "IMPM", 4) == 0) {
                mmcmp = false;
                /* I had an snprintf here that stuck the CMWT in the
                 * description, but I got rid of it because it really
                 * doesn't add a whole lot... */
                if (data[42] >= 0x14)
                        fi->description =
                                strdup("Compressed Impulse Tracker");
                else
                        fi->description = strdup("Impulse Tracker");
        } else if (length > 164 && memcmp(data + 132, "IMPM", 4) == 0
                   && memcmp(data, "ziRCONia", 8) == 0) {
                mmcmp = true;
                fi->description = strdup("Impulse Tracker");
        } else {
                return false;
        }

        fi->extension = strdup("it");
        fi->title = calloc(26, sizeof(char));
        memcpy(fi->title, data + (mmcmp ? 136 : 4), 25);
        fi->title[25] = 0;
        fi->type = TYPE_IT;
        return true;
}
