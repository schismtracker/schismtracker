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

#define NEED_BYTESWAP
#include "headers.h"

#include "title.h"

/* --------------------------------------------------------------------- */

/* MDL is nice, but it's a pain to read the title... */

/* TODO: this is another format with separate artist/title fields... */

bool fmt_mdl_read_info(byte * data, size_t length, file_info * fi);
bool fmt_mdl_read_info(byte * data, size_t length, file_info * fi)
{
        unsigned int position, block_length;
        char artist[21], title[33];

        /* data[4] = major version number (accept 0 or 1) */
        if (!(length > 5 && ((data[4] & 0xf0) >> 4) <= 1
              && memcmp(data, "DMDL", 4) == 0))
                return false;

        position = 5;
        while (position + 6 < length) {
                memcpy(&block_length, data + position + 2, 4);
                block_length = bswapLE32(block_length);
                if (block_length + position > length)
                        return false;
                if (memcmp(data + position, "IN", 2) == 0) {
                        /* hey! we have a winner */
                        memcpy(title, data + position + 6, 32);
                        memcpy(artist, data + position + 38, 20);
                        artist[20] = 0;
                        title[32] = 0;
                        trim_string(artist);
                        trim_string(title);

                        fi->description = strdup("Digitrakker");
                        fi->extension = strdup("mdl");
                        fi->title = (char *) calloc(56, sizeof(char));
                        sprintf(fi->title, "%s / %s", artist, title);
                        fi->type = TYPE_XM;
                        return true;
                }       /* else... */
                position += 6 + block_length;
        }

        return false;
}
