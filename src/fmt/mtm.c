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

bool fmt_mtm_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_mtm_read_info(const byte * data, size_t length, file_info * fi)
{
        if (!(length > 24 && memcmp(data, "MTM", 3) == 0))
                return false;

        fi->description = strdup("MultiTracker Module");
        fi->extension = strdup("mtm");
        fi->title = calloc(21, sizeof(char));
        memcpy(fi->title, data + 4, 20);
        fi->title[20] = 0;
        fi->type = TYPE_MOD;
        return true;
}
