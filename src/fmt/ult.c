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

/* TODO: make this not suck. I have no documentation for this format, and
 * only one file to test with, so it's kinda tricky at the moment.
 *
 * /usr/share/magic/magic says:
 *     0       string  MAS_UTrack_V00
 *     >14     string  >/0        ultratracker V1.%.1s module sound data */

bool fmt_ult_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_ult_read_info(const byte * data, size_t length, file_info * fi)
{
        if (!(length > 48 && memcmp(data, "MAS_UTrack_V00", 14) == 0))
                return false;

        fi->description = strdup("UltraTracker Module");
        fi->type = TYPE_S3M;
        fi->extension = strdup("ult");
        fi->title = calloc(33, sizeof(char));
        memcpy(fi->title, data + 15, 32);
        fi->title[32] = 0;
        return true;
}
