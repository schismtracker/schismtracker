/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <someguy@here.is> <http://here.is/someguy/>
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

/* Dunno why there's DigiTrekker and DigiTrakker, and why the
 * formats are completely different, but whatever :) */

bool fmt_dtm_read_info(byte * data, size_t length, file_info * fi);
bool fmt_dtm_read_info(byte * data, size_t length, file_info * fi)
{
        unsigned int position, block_length;

        if (!(length > 8))
                return false;

        /* find the SONG block */
        position = 0;
        while (memcmp(data + position, "SONG", 4) != 0) {
                memcpy(&block_length, data + position + 4, 4);

                position += block_length + 8;
                if (position + 8 > length)
                        return false;
        }

        /* so apparently it's a dtm */
        fi->title = NULL;

        /* "truncate" it to the length of the block */
        length = block_length + position + 8;

        /* now see if it has a title */
        while (position + 8 < length) {
                memcpy(&block_length, data + position + 4, 4);
                if (block_length + position > length)
                        return false;

                if (memcmp(data + position, "NAME", 4) == 0) {
                        /* hey! we have a winner */
                        fi->title =
                                (char *) calloc(block_length + 1,
                                                sizeof(char));
                        memcpy(fi->title, data + position + 8,
                               block_length);
                        fi->title[block_length] = 0;
                        break;
                }       /* else... */
                position += 8 + block_length;
        }

        fi->description = strdup("DigiTrekker 3");
        fi->extension = strdup("dtm");
        fi->type = TYPE_XM;
        return true;
}
