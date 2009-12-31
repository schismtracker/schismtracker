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

#include "it.h"
#include "palettes.h"

#include "sdlmain.h"


/* --------------------------------------------------------------------- */
/* palette */

/* this is set in cfg_load() (config.c)
palette_apply() must be called after changing this to update the display. */
uint8_t current_palette[16][3];
/* this should be changed only with palette_load_preset() (which doesn't call
palette_apply() automatically, so do that as well) */
int current_palette_index;


void palette_apply(void)
{
        int n;
        unsigned char cx[16][3];

        for (n = 0; n < 16; n++) {
                cx[n][0] = current_palette[n][0] << 2;
                cx[n][1] = current_palette[n][1] << 2;
                cx[n][2] = current_palette[n][2] << 2;
        }
        video_colors(cx);

        /* is the "light" border color actually darker than the "dark" color? */
        if ((current_palette[1][0] + current_palette[1][1] + current_palette[1][2])
            > (current_palette[3][0] + current_palette[3][1] + current_palette[3][2])) {
                status.flags |= INVERTED_PALETTE;
        } else {
                status.flags &= ~INVERTED_PALETTE;
        }
}

void palette_load_preset(int palette_index)
{
        if (palette_index < -1 || palette_index >= NUM_PALETTES)
                return;

        current_palette_index = palette_index;
        if (palette_index == -1) return;
        memcpy(current_palette, palettes[palette_index].colors, sizeof(current_palette));
}
