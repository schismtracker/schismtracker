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

#include "title.h"

/* --------------------------------------------------------------------- */

/* TODO: get more stm's and test this... one file's not good enough */

bool fmt_stm_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_stm_read_info(const byte * data, size_t length, file_info * fi)
{
        // data[29] is the type: 1 = song, 2 = module (with samples)
        if (!(length > 28 && data[28] == 0x1a
              && (data[29] == 1 || data[29] == 2)
              && (memcmp(data + 14, "!Scream!", 8)
                  || memcmp(data + 14, "BMOD2STM", 8))
            ))
                return false;

        /* I used to check whether it was a 'song' or 'module' and set
         * the description accordingly, but it's fairly pointless
         * information :) */
        fi->description = strdup("Scream Tracker 2");
        fi->extension = strdup("stm");
        fi->type = TYPE_MOD;
        fi->title = calloc(21, sizeof(char));
        memcpy(fi->title, data, 20);
        fi->title[20] = 0;
        return true;
}
