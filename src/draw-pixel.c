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

#include "it.h"
#include "palettes.h"

#include <SDL.h>

#ifndef ABS
# define ABS(x) ((x) < 0 ? -(x) : (x))
#endif
#ifndef SGN
# define SGN(x) ((x) < 0 ? -1 : 1)      /* hey, what about zero? */
#endif

/* --------------------------------------------------------------------- */
/* palette */

/* this is set in cfg_load() (config.c)
palette_apply() must be called after changing this to update the display. */
byte current_palette[16][3];
/* this should be changed only with palette_load_preset() (which doesn't call
palette_apply() automatically, so do that as well) */
int current_palette_index;

static Uint32 palette_lookup[16] = { 0 };

#define VALUE_TRANSLATE(n) ((int) ((n) / 63.0 * 255.0 + 0.5))

void palette_apply(void)
{
        int n;

        if (screen->format->BytesPerPixel == 1) {
                SDL_Color c[16];

                for (n = 0; n < 16; n++) {
                        c[n].r = VALUE_TRANSLATE(current_palette[n][0]);
                        c[n].g = VALUE_TRANSLATE(current_palette[n][1]);
                        c[n].b = VALUE_TRANSLATE(current_palette[n][2]);
                }
                SDL_SetColors(screen, c, 0, 16);
        } else {
                for (n = 0; n < 16; n++) {
                        palette_lookup[n] = SDL_MapRGB
				(screen->format,
				 VALUE_TRANSLATE(current_palette[n][0]),
				 VALUE_TRANSLATE(current_palette[n][1]),
				 VALUE_TRANSLATE(current_palette[n][2]));
		}
        }
        
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
        if (palette_index < 0 || palette_index >= NUM_PALETTES)
                return;

	current_palette_index = palette_index;
	memcpy(current_palette, palettes[palette_index].colors, sizeof(current_palette));
}

inline Uint32 palette_get(Uint32 c)
{
	return (screen->format->BytesPerPixel == 1) ? c : palette_lookup[c];
}

/* --------------------------------------------------------------------- */

/*
static void putpixel_8(SDL_Surface * surface, int x, int y,
                       Uint32 c)
{
        *((Uint8 *) surface->pixels + y * surface->pitch + x) = c;
}
*/

inline void putpixel(SDL_Surface * surface, int x, int y, Uint32 pixel)
{
        int bpp = surface->format->BytesPerPixel;
        /* Here p is the address to the pixel we want to set */
        Uint8 *p =
                (Uint8 *) surface->pixels + y * surface->pitch + x * bpp;

        switch (bpp) {
        case 1:
                *p = pixel;
                break;

        case 2:
                *(Uint16 *) p = pixel;
                break;

        case 3:
                if (SDL_BYTEORDER == SDL_BIG_ENDIAN) {
                        p[0] = (pixel >> 16) & 0xff;
                        p[1] = (pixel >> 8) & 0xff;
                        p[2] = pixel & 0xff;
                } else {
                        p[0] = pixel & 0xff;
                        p[1] = (pixel >> 8) & 0xff;
                        p[2] = (pixel >> 16) & 0xff;
                }
                break;

        case 4:
                *(Uint32 *) p = pixel;
                break;
        }
}

void putpixel_screen(int x, int y, Uint32 pixel)
{
        putpixel(screen, x, y, palette_get(pixel));
}

/* --------------------------------------------------------------------- */

inline void draw_fill_rect(SDL_Rect * rect, Uint32 color)
{
	SDL_FillRect(screen, rect, palette_get(color));
}

void draw_fill_chars(int xs, int ys, int xe, int ye, Uint32 color)
{
        SDL_Rect rect = {
                xs << 3, ys << 3, (xe - xs + 1) << 3, (ye - ys + 1) << 3
        };
        draw_fill_rect(&rect, color);
}

/* --------------------------------------------------------------------- */
/* From SDLroids. Hacked a bit. */

static inline void _draw_line_horiz(SDL_Surface * surface, int xs, int xe,
                                    int y, Uint32 c)
{
        int x;
        if (xs < xe)
                for (x = xs; x <= xe; x++)
                        putpixel(surface, x, y, c);
        else
                for (x = xe; x <= xs; x++)
                        putpixel(surface, x, y, c);
}

static inline void _draw_line_vert(SDL_Surface * surface, int x, int ys,
                                   int ye, Uint32 c)
{
        int y;
        if (ys < ye)
                for (y = ys; y <= ye; y++)
                        putpixel(surface, x, y, c);
        else
                for (y = ye; y <= ys; y++)
                        putpixel(surface, x, y, c);
}

/* Draw a line between two coordinates */
void draw_line(SDL_Surface * surface, int xs, int ys, int xe, int ye,
               Uint32 c)
{
        int d, x, y, ax, ay, sx, sy, dx, dy;

        dx = xe - xs;
        if (dx == 0) {
                _draw_line_vert(surface, xs, ys, ye, c);
                return;
        }

        dy = ye - ys;
        if (dy == 0) {
                _draw_line_horiz(surface, xs, xe, ys, c);
                return;
        }

        ax = ABS(dx) << 1;
        sx = SGN(dx);
        ay = ABS(dy) << 1;
        sy = SGN(dy);

        x = xs;
        y = ys;
        if (ax > ay) {
                /* x dominant */
                d = ay - (ax >> 1);
                for (;;) {
                        putpixel(surface, x, y, c);
                        if (x == xe)
                                return;
                        if (d >= 0) {
                                y += sy;
                                d -= ax;
                        }
                        x += sx;
                        d += ay;
                }
        } else {
                /* y dominant */
                d = ax - (ay >> 1);
                for (;;) {
                        putpixel(surface, x, y, c);
                        if (y == ye)
                                return;
                        if (d >= 0) {
                                x += sx;
                                d -= ay;
                        }
                        y += sy;
                        d += ax;
                }
        }
}

void draw_line_screen(int xs, int ys, int xe, int ye, Uint32 c)
{
        draw_line(screen, xs, ys, xe, ye, palette_get(c));
}
