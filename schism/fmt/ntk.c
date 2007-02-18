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

/* --------------------------------------------------------------------- */

int fmt_ntk_read_info(dmoz_file_t *file, const byte *data, size_t length)
{
        if (!(length > 25 && memcmp(data, "TWNNSNG2", 8) == 0))
                return false;

        file->description = "NoiseTrekker";
        /*file->extension = str_dup("ntk");*/
        file->title = calloc(16, sizeof(char));
        memcpy(file->title, data + 9, 15);
        file->title[15] = 0;
        file->type = TYPE_MODULE_MOD;    /* ??? */
        return true;
}
