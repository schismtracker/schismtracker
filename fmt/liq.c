/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2011 Storlek
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

int fmt_liq_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        char buf[32];

        if (!(length > 64 && data[64] == 0x1a && memcmp(data, "Liquid Module:", 14) == 0))
                return 0;

        file->description = "Liquid Tracker";
        /*file->extension = str_dup("liq");*/
        memcpy(buf, data + 44, 20);
        buf[20] = 0;
        file->artist = str_dup(buf);
        memcpy(buf, data + 14, 30);
        buf[30] = 0;
        file->title = str_dup(buf);
        file->type = TYPE_MODULE_S3M;

        return 1;
}
