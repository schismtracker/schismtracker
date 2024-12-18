/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
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

#ifndef SCHISM_VGAMEM_H_
#define SCHISM_VGAMEM_H_

#include <stdint.h>

void vgamem_clear(void);

struct vgamem_overlay {
	unsigned int x1, y1, x2, y2; /* in character cells... */

	unsigned char *q;               /* points inside ovl */
	unsigned int skip;

	int width, height; /* in pixels; signed to avoid bugs elsewhere */ /* XXX what bugs? */
};

void vgamem_flip(void);

void vgamem_ovl_alloc(struct vgamem_overlay *n);
void vgamem_ovl_apply(struct vgamem_overlay *n);

void vgamem_ovl_clear(struct vgamem_overlay *n, int color);
void vgamem_ovl_drawpixel(struct vgamem_overlay *n, int x, int y, int color);
void vgamem_ovl_drawline(struct vgamem_overlay *n, int xs, int ys, int xe, int ye, int color);

// scanners
void vgamem_scan8(uint32_t y, uint8_t *out,uint32_t tc[16], uint32_t mouse_line[80], uint32_t mouse_line_mask[80]);
void vgamem_scan16(uint32_t y, uint16_t *out,uint32_t tc[16], uint32_t mouse_line[80], uint32_t mouse_line_mask[80]);
void vgamem_scan32(uint32_t y, uint32_t *out,uint32_t tc[16], uint32_t mouse_line[80], uint32_t mouse_line_mask[80]);

/* --------------------------------------------------------------------- */
/* character drawing routines */
#define DEFAULT_FG 3
void draw_char(uint8_t c, int x, int y, uint32_t fg, uint32_t bg);
void draw_char_bios(uint8_t c, int x, int y, uint32_t fg, uint32_t bg);

/* return value is the number of characters drawn */
int draw_text(const char * text, int x, int y, uint32_t fg, uint32_t bg);
int draw_text_bios(const char * text, int x, int y, uint32_t fg, uint32_t bg);
int draw_text_utf8(const char * text, int x, int y, uint32_t fg, uint32_t bg);

/* return value is the length of text drawn
 * (so len - return is the number of spaces) */
int draw_text_len(const char * text, int len, int x, int y, uint32_t fg, uint32_t bg);
int draw_text_bios_len(const char * text, int len, int x, int y, uint32_t fg, uint32_t bg);
int draw_text_utf8_len(const char * text, int len, int x, int y, uint32_t fg, uint32_t bg);

void draw_fill_chars(int xs, int ys, int xe, int ye, uint32_t fg, uint32_t bg);

void draw_half_width_chars(uint8_t c1, uint8_t c2, int x, int y,
			   uint32_t fg1, uint32_t bg1, uint32_t fg2, uint32_t bg2);

/* --------------------------------------------------------------------- */
/* boxes */

/* the type is comprised of one value from each of these enums.
 * the "default" box type is thin, inner, and with outset shading. */

/* for outer boxes, outset/inset work like light/dark respectively
 * (because using two different colors for an outer box results in some
 * ugliness at the corners) */
enum {
	BOX_OUTSET = (0),       /* 00 00 */
	BOX_INSET = (1),        /* 00 01 */
	BOX_FLAT_LIGHT = (2),   /* 00 10 */
	BOX_FLAT_DARK = (3),    /* 00 11 */
	BOX_SHADE_NONE = (4),   /* 01 00 */
};
#define BOX_SHADE_MASK (7)

enum {
	BOX_INNER = (0 << 4),   /* 00 00 00 */
	BOX_OUTER = (1 << 4),   /* 01 00 00 */
	BOX_CORNER = (2 << 4),  /* 10 00 00 */
};
#define BOX_TYPE_MASK (3 << 4)

/* the thickness is ignored for corner boxes, which are always thin */
enum {
	BOX_THIN = (0 << 6),    /* 0 00 00 00 */
	BOX_THICK = (1 << 6),   /* 1 00 00 00 */
};
#define BOX_THICKNESS_MASK (1 << 6)

void draw_box(int xs, int ys, int xe, int ye, int flags);

/* ------------------------------------------------------------ */


struct song_sample;
void draw_sample_data(struct vgamem_overlay *r, struct song_sample *sample);

/* this works like draw_sample_data, just without having to allocate a
 * song_sample structure, and without caching the waveform.
 * mostly it's just for the oscilloscope view. */
void draw_sample_data_rect_32(struct vgamem_overlay *r, int32_t *data,
	int length, unsigned int inputchans, unsigned int outputchans);
void draw_sample_data_rect_16(struct vgamem_overlay *r, int16_t *data, int length,
	unsigned int inputchans, unsigned int outputchans);
void draw_sample_data_rect_8(struct vgamem_overlay *r, int8_t *data, int length,
	unsigned int inputchans, unsigned int outputchans);

#endif /* SCHISM_VGAMEM_H_ */
