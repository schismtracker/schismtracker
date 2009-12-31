/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010 Storlek
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

#define NEED_BYTESWAP
#include "headers.h"
#include "fmt.h"

/* --------------------------------------------------------------------- */

/* MDL is nice, but it's a pain to read the title... */

int fmt_mdl_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        size_t position, block_length;
        char buf[33];

        /* data[4] = major version number (accept 0 or 1) */
        if (!(length > 5 && ((data[4] & 0xf0) >> 4) <= 1 && memcmp(data, "DMDL", 4) == 0))
                return false;

        position = 5;
        while (position + 6 < length) {
                memcpy(&block_length, data + position + 2, 4);
                block_length = bswapLE32(block_length);
                if (block_length + position > length)
                        return false;
                if (memcmp(data + position, "IN", 2) == 0) {
                        /* hey! we have a winner */
                        memcpy(buf, data + position + 6, 32);
                        buf[32] = 0;
                        file->title = str_dup(buf);
                        memcpy(buf, data + position + 38, 20);
                        buf[20] = 0;
                        file->artist = str_dup(buf);

                        file->description = "Digitrakker";
                        /*file->extension = str_dup("mdl");*/
                        file->type = TYPE_MODULE_XM;
                        return true;
                } /* else... */
                position += 6 + block_length;
        }

        return false;
}
