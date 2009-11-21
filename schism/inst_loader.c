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
#include "log.h"
#include "fmt.h"

#include <string.h>

song_instrument *instrument_loader_init(struct instrumentloader *ii, int slot)
{
        ii->expect_samples = 0;
        ii->inst = song_get_instrument(slot, NULL);
        ii->slot = slot;
        ii->basex = 1;
        memset(ii->sample_map, 0, sizeof(ii->sample_map));
        return ii->inst;
}

int instrument_loader_abort(struct instrumentloader *ii)
{
        int n;
        song_wipe_instrument(ii->slot);
        for (n = 0; n < SCHISM_MAX_SAMPLES; n++) {
                if (ii->sample_map[n]) {
                        song_delete_sample(ii->sample_map[n]-1);
                        ii->sample_map[n] = 0;
                }
        }
        return false;
}

int instrument_loader_sample(struct instrumentloader *ii, int slot)
{
        int x;
        if (!slot) return 0;
        if (ii->sample_map[slot]) return ii->sample_map[slot];
        for (x = ii->basex; x < SCHISM_MAX_SAMPLES; x++) {
                if (!song_sample_is_empty(x-1)) continue;

                ii->expect_samples++;
                ii->sample_map[slot] = x;
                ii->basex = x+1;
                return ii->sample_map[slot];
        }
        status_text_flash("Too many samples");
        return 0;
}

