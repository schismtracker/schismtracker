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

bool fmt_liq_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_liq_read_info(const byte * data, size_t length, file_info * fi)
{
        char artist[21], title[31];

        if (!
            (length > 64 && data[64] == 0x1a
             && memcmp(data, "Liquid Module:", 14) == 0))
                return false;

        fi->description = strdup("Liquid Tracker");
        fi->extension = strdup("liq");
        fi->title = calloc(54, sizeof(char));

        memcpy(artist, data + 44, 20);
        memcpy(title, data + 14, 30);
        artist[20] = 0;
        title[30] = 0;
        trim_string(artist);
        trim_string(title);
        /* TODO: separate artist/title fields: make this customizable */
        sprintf(fi->title, "%s / %s", artist, title);
        fi->type = TYPE_S3M;

        return true;
}
