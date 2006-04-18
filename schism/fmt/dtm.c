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
#include "fmt.h"

/* --------------------------------------------------------------------- */

/* Dunno why there's DigiTrekker and DigiTrakker, and why
the formats are completely different, but whatever :) */

int fmt_dtm_read_info(dmoz_file_t *file, const byte *data, size_t length)
{
        unsigned int position, block_length;

        if (!(length > 8))
                return false;

        /* find the SONG block */
        position = 0;
        while (memcmp(data + position, "SONG", 4) != 0) {
                memcpy(&block_length, data + position + 4, 4);
		block_length = bswapLE32(block_length);
		
                position += block_length + 8;
                if (position + 8 > length)
                        return false;
        }

        /* "truncate" it to the length of the block */
        length = block_length + position + 8;

        /* now see if it has a title */
        while (position + 8 < length) {
                memcpy(&block_length, data + position + 4, 4);
		block_length = bswapLE32(block_length);
		
                if (block_length + position > length)
                        return false;

                if (memcmp(data + position, "NAME", 4) == 0) {
                        /* hey! we have a winner */
                        file->title = (char *) calloc(block_length + 1, sizeof(char));
                        memcpy(file->title, data + position + 8, block_length);
                        file->title[block_length] = 0;
                        break;
                } /* else... */
                position += 8 + block_length;
        }

        file->description = "DigiTrekker 3";
        /*file->extension = strdup("dtm");*/
        file->type = TYPE_MODULE_XM;
        return true;
}
