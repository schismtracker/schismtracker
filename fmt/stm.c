/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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

#include "headers.h"
#include "fmt.h"

/* --------------------------------------------------------------------- */

/* TODO: get more stm's and test this... one file's not good enough */

int fmt_stm_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        /* data[29] is the type: 1 = song, 2 = module (with samples) */
        if (!(length > 28 && data[28] == 0x1a && (data[29] == 1 || data[29] == 2)
              && (memcmp(data + 14, "!Scream!", 8) || memcmp(data + 14, "BMOD2STM", 8))))
                return false;

        /* I used to check whether it was a 'song' or 'module' and set the description
        accordingly, but it's fairly pointless information :) */
        file->description = "Scream Tracker 2";
        /*file->extension = str_dup("stm");*/
        file->type = TYPE_MODULE_MOD;
        file->title = calloc(21, sizeof(char));
        memcpy(file->title, data, 20);
        file->title[20] = 0;
        return true;
}
