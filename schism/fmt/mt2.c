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

/* FIXME:
 * - this is wrong :)
 * - look for an author name; if it's not "Unregistered" use it */

/* --------------------------------------------------------------------- */

bool fmt_mt2_read_info(dmoz_file_t *file, const byte *data, size_t length)
{
        if (!(length > 106 && memcmp(data, "MT20", 4) == 0))
                return false;

        file->description = "MadTracker 2 Module";
        /*file->extension = strdup("mt2");*/
        file->title = calloc(65, sizeof(char));
        memcpy(file->title, data + 42, 64);
        file->title[64] = 0;
        file->type = TYPE_MODULE_XM;
        return true;
}
