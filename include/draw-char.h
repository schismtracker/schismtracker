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

#if !defined(DRAW_CHAR_H) && !defined(__cplusplus)
#define DRAW_CHAR_H

#include <stdint.h>

/* --------------------------------------------------------------------- */

void draw_char(unsigned char c, int x, int y, uint32_t fg, uint32_t bg);
void draw_char_bios(unsigned char c, int x, int y, uint32_t fg, uint32_t bg);

/* return value is the number of characters drawn */
int draw_text(const char * text, int x, int y, uint32_t fg, uint32_t bg);
int draw_text_bios(const char * text, int x, int y, uint32_t fg, uint32_t bg);

/* return value is the length of text drawn
 * (so len - return is the number of spaces) */
int draw_text_len(const char * text, int len, int x, int y, uint32_t fg, uint32_t bg);
int draw_text_bios_len(const char * text, int len, int x, int y, uint32_t fg, uint32_t bg);

void draw_fill_chars(int xs, int ys, int xe, int ye, uint32_t color);

void draw_half_width_chars(uint8_t c1, uint8_t c2, int x, int y,
                           uint32_t fg1, uint32_t bg1, uint32_t fg2, uint32_t bg2);

/* --------------------------------------------------------------------- */
/* boxes */

/* don't use these directly */
void draw_thin_inner_box(int xs, int ys, int xe, int ye, uint32_t tl, uint32_t br);
void draw_thick_inner_box(int xs, int ys, int xe, int ye, uint32_t tl, uint32_t br);
void draw_thin_outer_box(int xs, int ys, int xe, int ye, uint32_t c);
void draw_thick_outer_box(int xs, int ys, int xe, int ye, uint32_t c);
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

void draw_box(int xs, int ys, int xe, int ye, int flags);
/* .... */
void toggle_display_fullscreen(void); /* FIXME why on earth is this in this header? */

#endif /* ! DRAW_CHAR_H */
