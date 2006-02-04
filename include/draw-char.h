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

#if !defined(DRAW_CHAR_H) && !defined(__cplusplus)
#define DRAW_CHAR_H

#include "sdlmain.h"

/* --------------------------------------------------------------------- */

void draw_char(unsigned int c, int x, int y, Uint32 fg, Uint32 bg);

/* return value is the number of characters drawn */
int draw_text(const byte * text, int x, int y, Uint32 fg, Uint32 bg);

/* return value is the length of text drawn
 * (so len - return is the number of spaces) */
int draw_text_len(const byte * text, int len, int x, int y, Uint32 fg, Uint32 bg);

void draw_fill_chars(int xs, int ys, int xe, int ye, Uint32 color);

void draw_half_width_chars(byte c1, byte c2, int x, int y,
			   Uint32 fg1, Uint32 bg1, Uint32 fg2, Uint32 bg2);

/* --------------------------------------------------------------------- */
/* boxes */

/* don't use these directly */
void draw_thin_inner_box(int xs, int ys, int xe, int ye, Uint32 tl, Uint32 br);
void draw_thick_inner_box(int xs, int ys, int xe, int ye, Uint32 tl, Uint32 br);
void draw_thin_outer_box(int xs, int ys, int xe, int ye, Uint32 c);
void draw_thick_outer_box(int xs, int ys, int xe, int ye, Uint32 c);
void draw_thin_outer_cornered_box(int xs, int ys, int xe, int ye, int shade_mask);

/* the type is comprised of one value from each of these enums.
 * the "default" box type is thin, inner, and with outset shading. */

/* for outer boxes, outset/inset work like light/dark respectively
 * (because using two different colors for an outer box results in some
 * ugliness at the corners) */
enum {
        BOX_OUTSET = (0),
        BOX_INSET = (1),
        BOX_FLAT_LIGHT = (2),
        BOX_FLAT_DARK = (3),
};
#define BOX_SHADE_MASK 3

enum {
        BOX_INNER = (0 << 2),   /* 00 00 */
        BOX_OUTER = (1 << 2),   /* 01 00 */
        BOX_CORNER = (2 << 2),  /* 10 00 */
};
#define BOX_TYPE_MASK 12

/* the thickness is ignored for corner boxes, which are always thin */
enum {
        BOX_THIN = (0 << 4),    /* 0 00 00 */
        BOX_THICK = (1 << 4),   /* 1 00 00 */
};
#define BOX_THICKNESS_MASK 16

static inline void draw_box(int xs, int ys, int xe, int ye, int flags)
{
        const int colors[4][2] = { {3, 1}, {1, 3}, {3, 3}, {1, 1} };
        int tl = colors[flags & BOX_SHADE_MASK][0];
        int br = colors[flags & BOX_SHADE_MASK][1];

        switch (flags & (BOX_TYPE_MASK | BOX_THICKNESS_MASK)) {
        case BOX_THIN | BOX_INNER:
                draw_thin_inner_box(xs, ys, xe, ye, tl, br);
                break;
        case BOX_THICK | BOX_INNER:
                draw_thick_inner_box(xs, ys, xe, ye, tl, br);
                break;
        case BOX_THIN | BOX_OUTER:
                draw_thin_outer_box(xs, ys, xe, ye, tl);
                break;
        case BOX_THICK | BOX_OUTER:
                draw_thick_outer_box(xs, ys, xe, ye, tl);
                break;
        case BOX_THIN | BOX_CORNER:
        case BOX_THICK | BOX_CORNER:
                draw_thin_outer_cornered_box(xs, ys, xe, ye, flags & BOX_SHADE_MASK);
                break;
        }
}

/* .... */
void toggle_display_fullscreen(void);

#endif /* ! DRAW_CHAR_H */
