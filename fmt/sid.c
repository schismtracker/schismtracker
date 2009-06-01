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

/* TODO: copyright field? */

/*
00 | 50 53 49 44  00 02 00 7c  00 00 11 62  11 68 00 02 | PSID...|...b.h..
10 | 00 01 00 00  00 00 53 6f  6c 69 74 61  78 20 28 45 | ......Solitax (E
20 | 6e 64 20 53  65 71 75 65  6e 63 65 29  00 00 00 00 | nd Sequence)....
30 | 00 00 00 00  00 00 4a 65  73 70 65 72  20 4f 6c 73 | ......Jesper Ols
40 | 65 6e 00 00  00 00 00 00  00 00 00 00  00 00 00 00 | en..............
50 | 00 00 00 00  00 00 31 39  39 30 2d 39  32 20 41 6d | ......1990-92 Am
60 | 6f 6b 20 53  6f 75 6e 64  20 44 65 70  74 2e 00 00 | ok Sound Dept...
70 | 00 00 00 00  00 00 00 00  00 00 00 00  62 11 4c 72 | ............b.Lr
*/

int fmt_sid_read_info(dmoz_file_t *file, const byte *data, size_t length);
{
        char buf[33];
        int n;

        /* i'm not sure what the upper bound on the size of a sid is, but
         * the biggest one i have is jch/vibrants - "better late than
         * never", and it's only 20k. */
        if (length > 32767)
                return false;

        if (!(length > 128 && memcmp(data, "PSID", 4) == 0))
                return false;

	memcpy(buf, data + 22, 32);
	buf[32] = 0;
	file->title = str_dup(buf);
	memcpy(buf, data + 54, 32);
	buf[32] = 0;
	file->artist = str_dup(buf);
	/* memcpy(buf, data + 86, 32); - copyright... */

        file->description = "Commodore 64 SID";
        /*file->extension = str_dup("sid");*/
        file->type = TYPE_SAMPLE_COMPR; /* FIXME: not even close. */
        return true;
}
