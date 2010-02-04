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

#include "headers.h"
#include "fmt.h"

#include <errno.h>

/* --------------------------------------------------------------------- */

/* does IT's raw sample loader use signed or unsigned samples? */

int fmt_raw_load_sample(const uint8_t *data, size_t length, song_sample *smp, UNUSED char *title)
{
        /* we'll uphold IT's limit of 4mb */
        if (length > 4 * 1048576) {
                errno = EFBIG;
                return 0;
        }

        smp->speed = 8363;
        smp->volume = 64 * 4;
        smp->global_volume = 64;

        smp->data = song_sample_allocate(length);
        memcpy(smp->data, data, length);
        smp->length = length;

        return 1;
}

int fmt_raw_save_sample(disko_t *fp, song_sample *smp, UNUSED char *title)
{
        fp->write(fp, smp->data, ((smp->flags & SAMP_16_BIT) ? 2:1)*smp->length);
        return 1;
}
