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
#include "headers.h"

#include "charset.h"
#include "it.h"
#include "vgamem.h"
#include "fonts.h"
#include "song.h"

#define SAMPLE_DATA_COLOR 13 /* Sample data */
#define SAMPLE_LOOP_COLOR 3 /* Sample loop marks */
#define SAMPLE_MARK_COLOR 6 /* Play mark color */
#define SAMPLE_BGMARK_COLOR 7 /* Play mark color after note fade / NNA */

/* ok, I'm making a hack to fit this all into 32-bits.
 * these ABSOLUTELY MUST BE checked in this order !!!
 * otherwise, nothing will work properly and we will all be sad. */

/* for halfwidth, we need all the bytes we can get, hence why it's first ;) */
#define VGAMEM_FONT_HALFWIDTH (0x80000000)
#define VGAMEM_FONT_OVERLAY (0x40000000)
#define VGAMEM_FONT_UNICODE (0x20000000) /* byte 29; leaves literally just enough space */
#define VGAMEM_FONT_BIOS (0x10000000)

/* if none of those were filled, we're dealing with a normal character. */

/* ------------------------------------------------------------------------ */
/* defines for UNICODE; this is packed ultra-tight :) */

/* colors */
#define VGAMEM_UNICODE_FG_BIT (25)
#define VGAMEM_UNICODE_FG_MASK (0xF << VGAMEM_UNICODE_FG_BIT)
#define VGAMEM_UNICODE_FG(x) (((x) & VGAMEM_UNICODE_FG_MASK) >> VGAMEM_UNICODE_FG_BIT)

#define VGAMEM_UNICODE_BG_BIT (21)
#define VGAMEM_UNICODE_BG_MASK (0xF << VGAMEM_UNICODE_BG_BIT)
#define VGAMEM_UNICODE_BG(x) (((x) & VGAMEM_UNICODE_BG_MASK) >> VGAMEM_UNICODE_BG_BIT)

/* character mask; just enough to fit one codepoint */
#define VGAMEM_UNICODE_CODEPOINT_MASK (0x001FFFFF)
#define VGAMEM_UNICODE_CODEPOINT(x) ((x) & VGAMEM_UNICODE_CODEPOINT_MASK)

/* ------------------------------------------------------------------------ */
/* halfwidth defines; stores 2 characters and both fg/bg */

#define VGAMEM_HW_FG1_BIT (26)
#define VGAMEM_HW_FG1_MASK (0xF << VGAMEM_HW_FG1_BIT)
#define VGAMEM_HW_FG1(x) (((x) & VGAMEM_HW_FG1_MASK) >> VGAMEM_HW_FG1_BIT)

#define VGAMEM_HW_BG1_BIT (22)
#define VGAMEM_HW_BG1_MASK (0xF << VGAMEM_HW_BG1_BIT)
#define VGAMEM_HW_BG1(x) (((x) & VGAMEM_HW_BG1_MASK) >> VGAMEM_HW_BG1_BIT)

#define VGAMEM_HW_CHAR1_BIT (15)
#define VGAMEM_HW_CHAR1_MASK (0x7F << VGAMEM_HW_CHAR1_BIT)
#define VGAMEM_HW_CHAR1(x) (((x) & VGAMEM_HW_CHAR1_MASK) >> VGAMEM_HW_CHAR1_BIT)

#define VGAMEM_HW_FG2_BIT (11)
#define VGAMEM_HW_FG2_MASK (0xF << VGAMEM_HW_FG2_BIT)
#define VGAMEM_HW_FG2(x) (((x) & VGAMEM_HW_FG2_MASK) >> VGAMEM_HW_FG2_BIT)

#define VGAMEM_HW_BG2_BIT (7)
#define VGAMEM_HW_BG2_MASK (0xF << VGAMEM_HW_BG2_BIT)
#define VGAMEM_HW_BG2(x) (((x) & VGAMEM_HW_BG2_MASK) >> VGAMEM_HW_BG2_BIT)

#define VGAMEM_HW_CHAR2_BIT (0)
#define VGAMEM_HW_CHAR2_MASK (0x7F << VGAMEM_HW_CHAR2_BIT)
#define VGAMEM_HW_CHAR2(x) (((x) & VGAMEM_HW_CHAR2_MASK) >> VGAMEM_HW_CHAR2_BIT)

/* ok i think i get this now, after inspecting it further.
 * good thing no one bothered putting any comments in the code. <grumble>
 * the fake vga buffer is pigeonholing the half-width characters into 14 bits.
 * why 14, i don't know, but that means 7 bits per character, and these functions
 * handle shifting stuff around to get them into that space. realistically, we
 * only need to bother with chars 32 through 127, as well as 173 (middot) and
 * 205 (the double-line used for noteoff). since 32->127 is 96 characters, there's
 * plenty of room for the printable stuff... and guess what, 173->205 is another
 * 32, which fits nice and clean into 7 bits! so if the character is within that
 * range, we're fine. otherwise it'll just result in a broken glyph. (but it
 * probably wasn't drawn in the font anyway) */

static inline SCHISM_ALWAYS_INLINE int vgamem_pack_halfw(int c)
{
	if (c >= 32 && c <= 127)
		return c - 32; /* 0 ... 95 */
	else if (c >= 173 && c <= 205)
		return 96 + c - 173; /* 96 ... 127 */

	//SCHISM_UNREACHABLE;
	return '?';
}

static inline SCHISM_ALWAYS_INLINE int vgamem_unpack_halfw(int c)
{
	if (c >= 0 && c <= 95)
		return c + 32;
	else if (c >= 96 && c <= 127)
		return 96 - c + 173;

	//SCHISM_UNREACHABLE;
	return '?';
}

/* ------------------------------------------------------------------------ */
/* BIOS and ITF defines; stores 1 character, and fg/bg */

#define VGAMEM_FG_BIT (12)
#define VGAMEM_FG_MASK (0xF << VGAMEM_FG_BIT)
#define VGAMEM_FG(x) (((x) & VGAMEM_FG_MASK) >> VGAMEM_FG_BIT)

#define VGAMEM_BG_BIT (8)
#define VGAMEM_BG_MASK (0xF << VGAMEM_BG_BIT)
#define VGAMEM_BG(x) (((x) & VGAMEM_BG_MASK) >> VGAMEM_BG_BIT)

#define VGAMEM_CHAR_BIT (0)
#define VGAMEM_CHAR_MASK (0xFF << VGAMEM_CHAR_BIT)
#define VGAMEM_CHAR(x) (((x) & VGAMEM_CHAR_MASK) >> VGAMEM_CHAR_BIT)

/* ------------------------------------------------------------------------ */

#define VGAMEM_CHAR_COUNT (VGAMEM_COLUMNS * VGAMEM_ROWS)

static uint32_t vgamem[VGAMEM_CHAR_COUNT] = {0};
static uint32_t vgamem_read[VGAMEM_CHAR_COUNT] = {0};

static uint8_t ovl[NATIVE_SCREEN_WIDTH*NATIVE_SCREEN_HEIGHT] = {0}; /* 256K */

#define CHECK_INVERT(tl,br,n) \
do {                                            \
	if (status.flags & INVERTED_PALETTE) {  \
		n = tl;                         \
		tl = br;                        \
		br = n;                         \
	}                                       \
} while(0)

void vgamem_flip(void)
{
	memcpy(vgamem_read, vgamem, sizeof(vgamem));
}

void vgamem_clear(void)
{
	memset(vgamem,0,sizeof(vgamem));
}

void vgamem_ovl_alloc(struct vgamem_overlay *n)
{
	n->q = &ovl[ (n->x1*8) + (n->y1 * 8 * NATIVE_SCREEN_WIDTH) ];
	n->width = 8 * ((n->x2 - n->x1) + 1);
	n->height = 8 * ((n->y2 - n->y1) + 1);
	n->skip = (NATIVE_SCREEN_WIDTH - n->width);
}

void vgamem_ovl_apply(struct vgamem_overlay *n)
{
	unsigned int x, y;

	for (y = n->y1; y <= n->y2; y++)
		for (x = n->x1; x <= n->x2; x++)
			vgamem[x + (y*VGAMEM_COLUMNS)] = VGAMEM_FONT_OVERLAY;
}

void vgamem_ovl_clear(struct vgamem_overlay *n, int color)
{
	int i, j;
	unsigned char *q = n->q;
	for (j = 0; j < n->height; j++) {
		for (i = 0; i < n->width; i++) {
			*q = color;
			q++;
		}
		q += n->skip;
	}
}

void vgamem_ovl_drawpixel(struct vgamem_overlay *n, int x, int y, int color)
{
	n->q[ (NATIVE_SCREEN_WIDTH*y) + x ] = color;
}

static inline void _draw_line_v(struct vgamem_overlay *n, int x,
	int ys, int ye, int color)
{
	unsigned char *q = n->q + x;
	int y;

	if (ys < ye) {
		q += (ys * NATIVE_SCREEN_WIDTH);
		for (y = ys; y <= ye; y++) {
			*q = color;
			q += NATIVE_SCREEN_WIDTH;
		}
	} else {
		q += (ye * NATIVE_SCREEN_WIDTH);
		for (y = ye; y <= ys; y++) {
			*q = color;
			q += NATIVE_SCREEN_WIDTH;
		}
	}
}

static inline void _draw_line_h(struct vgamem_overlay *n, int xs,
	int xe, int y, int color)
{
	unsigned char *q = n->q + (y * NATIVE_SCREEN_WIDTH);
	int x;
	if (xs < xe) {
		q += xs;
		for (x = xs; x <= xe; x++) {
			*q = color;
			q++;
		}
	} else {
		q += xe;
		for (x = xe; x <= xs; x++) {
			*q = color;
			q++;
		}
	}
}

#ifndef ABS
# define ABS(x) ((x) < 0 ? -(x) : (x))
#endif
#ifndef SGN
# define SGN(x) ((x) < 0 ? -1 : 1)      /* hey, what about zero? */
#endif

void vgamem_ovl_drawline(struct vgamem_overlay *n, int xs,
	int ys, int xe, int ye, int color)
{
	int d, x, y, ax, ay, sx, sy, dx, dy;

	dx = xe - xs;
	if (dx == 0) {
		_draw_line_v(n, xs, ys, ye, color);
		return;
	}

	dy = ye - ys;
	if (dy == 0) {
		_draw_line_h(n, xs, xe, ys, color);
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
			vgamem_ovl_drawpixel(n, x, y, color);
			if (x == xe) break;
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
			vgamem_ovl_drawpixel(n, x, y, color);
			if (y == ye) break;
			if (d >= 0) {
				x += sx;
				d -= ay;
			}
			y += sy;
			d += ax;
		}
	}
}

/* unknown character (basically an inverted '?') */
static const uint8_t uFFFD[] = {
	0x42, /* .X....X. */
	0x99, /* X..XX..X */
	0xF9, /* XXXXX..X */
	0xF3, /* XXXX..XX */
	0xE7, /* XXX..XXX */
	0xFF, /* XXXXXXXX */
	0xE7, /* XXX..XXX */
	0x7E, /* .XXXXXX. */
};

/* generic scanner; BITS must be one of 8, 16, 32, 64
 *
 * okay, so turns out, my "new" scanner was only really
 * fast on newer systems. on older systems, it actually
 * made speeds completely unbearable. SO, i went ahead
 * and converted everything back to using a single 32-bit
 * integer, where somehow I managed to get everything to fit.
 *
 * my hope is, that by reducing the size of the vgamem array
 * (size 12 for each member -> 4 for each), it will allow for
 * faster processing on older systems, who usually have smaller
 * caches and slower memory accesses.  or that's the theory,
 * anyway. I have yet to put it to the test :) */
#define VGAMEM_SCANNER_VARIANT(BITS) \
	void vgamem_scan##BITS(uint32_t ry, uint##BITS##_t *out, uint32_t tc[16],\
		uint32_t mouseline[VGAMEM_COLUMNS], uint32_t mouseline_mask[VGAMEM_COLUMNS]) \
	{ \
		/* constants */ \
		const uint_fast32_t y = (ry >> 3), yl = (ry & 7); \
		const uint8_t *q = ovl + (ry * NATIVE_SCREEN_WIDTH); \
		const uint8_t *const itf = font_data + yl, \
			*const bios = font_default_upper_alt + yl, \
			*const bioslow = font_default_lower + yl, \
			*const hf = font_half_data + (yl >> 1), \
			*const hiragana = font_hiragana + yl, \
			*const extlatin = font_extended_latin + yl, \
			*const greek = font_greek + yl, \
			*const cp866 = font_cp866 + yl; \
		const uint32_t *bp = &vgamem_read[y * VGAMEM_COLUMNS]; \
	\
		uint_fast32_t x; \
		for (x = 0; x < VGAMEM_COLUMNS; x++, bp++, q += 8) { \
			uint_fast8_t fg, bg, fg2, bg2, dg; \
	\
			if (*bp & VGAMEM_FONT_HALFWIDTH) { \
				/* halfwidth (used for patterns) */ \
				{ \
					const uint_fast8_t dg1 = hf[vgamem_unpack_halfw(VGAMEM_HW_CHAR1(*bp)) << 2]; \
					const uint_fast8_t dg2 = hf[vgamem_unpack_halfw(VGAMEM_HW_CHAR2(*bp)) << 2]; \
	\
					dg = (!(ry & 1)) \
						? ((dg1 & 0xF0) | dg2 >> 4) \
						: (dg1 << 4 | (dg2 & 0xF)); \
				} \
	\
				fg = VGAMEM_HW_FG1(*bp); \
				bg = VGAMEM_HW_BG1(*bp); \
				fg2 = VGAMEM_HW_FG2(*bp); \
				bg2 = VGAMEM_HW_BG2(*bp); \
			} else if (*bp & VGAMEM_FONT_OVERLAY) { \
				/* raw pixel data, needs special code ;) */ \
				*out++ = tc[ (q[0]|((mouseline[x] & 0x80)?15:0)) & 0xFF]; \
				*out++ = tc[ (q[1]|((mouseline[x] & 0x40)?15:0)) & 0xFF]; \
				*out++ = tc[ (q[2]|((mouseline[x] & 0x20)?15:0)) & 0xFF]; \
				*out++ = tc[ (q[3]|((mouseline[x] & 0x10)?15:0)) & 0xFF]; \
				*out++ = tc[ (q[4]|((mouseline[x] & 0x08)?15:0)) & 0xFF]; \
				*out++ = tc[ (q[5]|((mouseline[x] & 0x04)?15:0)) & 0xFF]; \
				*out++ = tc[ (q[6]|((mouseline[x] & 0x02)?15:0)) & 0xFF]; \
				*out++ = tc[ (q[7]|((mouseline[x] & 0x01)?15:0)) & 0xFF]; \
				continue; \
			} else if (*bp & VGAMEM_FONT_UNICODE) { \
				/* Any unicode character. */ \
				const uint_fast32_t c = VGAMEM_UNICODE_CODEPOINT(*bp); \
				int c8; \
	\
				/* These are ordered by how often they will probably appear
				 * for an average user of Schism (i.e., English speakers). */ \
				if (c >= 0x20 && c <= 0x7F) { \
					/* ASCII */ \
					dg = itf[c << 3]; \
				} else if (c >= 0xA0 && c <= 0xFF) { \
					/* extended latin */ \
					dg = extlatin[(c - 0xA0) << 3]; \
				} else if (c >= 0x390 && c <= 0x3C9) { \
					/* greek */ \
					dg = greek[(c - 0x390) << 3]; \
				} else if (c >= 0x3040 && c <= 0x309F) { \
					/* japanese hiragana */ \
					dg = hiragana[(c - 0x3040) << 3]; \
				} else if ((c8 = char_unicode_to_cp437(c)) >= 0) { \
					dg = (c8 & 0x80) \
						? bios[(c8 & 0x7F) << 3] \
						: bioslow[(c8 & 0x7F) << 3]; \
				} else if ((c8 = char_unicode_to_itf(c)) >= 0) { \
					dg = itf[c8 << 3]; \
				} else if ((c8 = char_unicode_to_cp866(c)) >= 0) { \
					dg = cp866[(c8 & 0x7F) << 3]; \
				} else { \
					dg = uFFFD[yl]; \
				} \
	\
				fg = VGAMEM_UNICODE_FG(*bp); \
				bg = VGAMEM_UNICODE_BG(*bp); \
				fg2 = fg; \
				bg2 = bg; \
			} else if (*bp & VGAMEM_FONT_BIOS) { \
				uint_fast8_t c = VGAMEM_CHAR(*bp); \
				fg = VGAMEM_FG(*bp); \
				bg = VGAMEM_BG(*bp); \
				fg2 = fg; \
				bg2 = bg; \
				dg = (c & 0x80) \
					? bios[(c & 0x7F) << 3] \
					: bioslow[(c & 0x7F) << 3]; \
			} else { \
				/* none of the above, this is an ITF character */ \
				fg = VGAMEM_FG(*bp); \
				bg = VGAMEM_BG(*bp); \
				fg2 = fg; \
				bg2 = bg; \
				dg = itf[VGAMEM_CHAR(*bp) << 3]; \
			} \
	\
			dg |= mouseline[x]; \
			dg &= ~(mouseline_mask[x] ^ mouseline[x]); \
	\
			*out++ = tc[(dg & 0x80) ? fg : bg]; \
			*out++ = tc[(dg & 0x40) ? fg : bg]; \
			*out++ = tc[(dg & 0x20) ? fg : bg]; \
			*out++ = tc[(dg & 0x10) ? fg : bg]; \
			*out++ = tc[(dg & 0x8) ? fg2 : bg2]; \
			*out++ = tc[(dg & 0x4) ? fg2 : bg2]; \
			*out++ = tc[(dg & 0x2) ? fg2 : bg2]; \
			*out++ = tc[(dg & 0x1) ? fg2 : bg2]; \
		} \
	}

VGAMEM_SCANNER_VARIANT(8)
VGAMEM_SCANNER_VARIANT(16)
VGAMEM_SCANNER_VARIANT(32)

#undef VGAMEM_SCAN_VARIANT

void draw_char_unicode(uint32_t c, int x, int y, uint32_t fg, uint32_t bg)
{
	SCHISM_RUNTIME_ASSERT(x >= 0 && y >= 0 && x < VGAMEM_COLUMNS && y < VGAMEM_ROWS, "Coordinates should always be inbounds");

	vgamem[x + (y*VGAMEM_COLUMNS)] = (VGAMEM_FONT_UNICODE
		| (fg << VGAMEM_UNICODE_FG_BIT)
		| (bg << VGAMEM_UNICODE_BG_BIT)
		| c);
}

void draw_char_bios(uint8_t c, int x, int y, uint32_t fg, uint32_t bg)
{
	SCHISM_RUNTIME_ASSERT(x >= 0 && y >= 0 && x < VGAMEM_COLUMNS && y < VGAMEM_ROWS, "Coordinates should always be inbounds");

	vgamem[x + (y*VGAMEM_COLUMNS)] = (VGAMEM_FONT_BIOS
		| (fg << VGAMEM_FG_BIT)
		| (bg << VGAMEM_BG_BIT)
		| c);
}

void draw_char(uint8_t c, int x, int y, uint32_t fg, uint32_t bg)
{
	SCHISM_RUNTIME_ASSERT(x >= 0 && y >= 0 && x < VGAMEM_COLUMNS && y < VGAMEM_ROWS, "Coordinates should always be inbounds");

	vgamem[x + (y*VGAMEM_COLUMNS)] = ((fg << VGAMEM_FG_BIT) | (bg << VGAMEM_BG_BIT) | c);
}

int draw_text(const char * text, int x, int y, uint32_t fg, uint32_t bg)
{
	int n = 0;

	while (*text) {
		draw_char(*text, x + n, y, fg, bg);
		n++;
		text++;
	}

	return n;
}

int draw_text_bios(const char * text, int x, int y, uint32_t fg, uint32_t bg)
{
	int n = 0;

	while (*text) {
		draw_char_bios(*text, x + n, y, fg, bg);
		n++;
		text++;
	}

	return n;
}

int draw_text_charset(const void *text, charset_t set, int x, int y, uint32_t fg, uint32_t bg)
{
	uint32_t *composed;
	size_t i;

	switch (set) {
	case CHARSET_ITF: return draw_text(text, x, y, fg, bg);
	case CHARSET_CP437: return draw_text_bios(text, x, y, fg, bg);
	/* else do unicode */
	default: break;
	}

	composed = charset_compose_to_set(text, set, CHARSET_UCS4);
	if (!composed)
		return draw_text_bios(text, x, y, fg, bg);

	for (i = 0; composed[i]; i++)
		draw_char_unicode(composed[i], x + i, y, fg, bg);

	free(composed);

	return i;
}

int draw_text_utf8(const char * text, int x, int y, uint32_t fg, uint32_t bg)
{
	return draw_text_charset(text, CHARSET_UTF8, x, y, fg, bg);
}

void draw_fill_chars(int xs, int ys, int xe, int ye, uint32_t fg, uint32_t bg)
{
	uint32_t *mm;
	int x, len;

	mm = &vgamem[(ys * VGAMEM_COLUMNS) + xs];
	len = (xe - xs)+1;
	ye -= ys;
	do {
		for (x = 0; x < len; x++)
			mm[x] = (fg << VGAMEM_FG_BIT) | (bg << VGAMEM_BG_BIT);
		mm += VGAMEM_COLUMNS;
		ye--;
	} while (ye >= 0);
}

int draw_text_len(const char * text, int len, int x, int y, uint32_t fg, uint32_t bg)
{
	int n = 0;

	while (*text && n < len) {
		draw_char(*text, x + n, y, fg, bg);
		n++;
		text++;
	}
	draw_fill_chars(x + n, y, x + len - 1, y, fg, bg);
	return n;
}

int draw_text_bios_len(const char * text, int len, int x, int y, uint32_t fg, uint32_t bg)
{
	int n = 0;

	while (*text && n < len) {
		draw_char_bios(*text, x + n, y, fg, bg);
		n++;
		text++;
	}
	draw_fill_chars(x + n, y, x + len - 1, y, fg, bg);
	return n;
}

int draw_text_charset_len(const void *text, charset_t set, int len, int x, int y, uint32_t fg, uint32_t bg)
{
	uint32_t *composed;
	int i;

	switch (set) {
	case CHARSET_ITF: return draw_text_len(text, len, x, y, fg, bg);
	case CHARSET_CP437: return draw_text_bios_len(text, len, x, y, fg, bg);
	default: break;
	}

	composed = charset_compose_to_set(text, set, CHARSET_UCS4);
	if (!composed)
		return draw_text_bios(text, x, y, fg, bg);

	for (i = 0; composed[i] && i < len; i++)
		draw_char_unicode(composed[i], x + i, y, fg, bg);

	free(composed);

	draw_fill_chars(x + i, y, x + len - 1, y, fg, bg);

	return i;
}

int draw_text_utf8_len(const char * text, int len, int x, int y, uint32_t fg, uint32_t bg)
{
	return draw_text_charset_len(text, CHARSET_UTF8, len, x, y, fg, bg);
}

/* --------------------------------------------------------------------- */

void draw_half_width_chars(uint8_t c1, uint8_t c2, int x, int y,
			   uint32_t fg1, uint32_t bg1, uint32_t fg2, uint32_t bg2)
{
	SCHISM_RUNTIME_ASSERT(x >= 0 && y >= 0 && x < VGAMEM_COLUMNS && y < VGAMEM_ROWS, "Coordinates should always be inbounds");


	vgamem[x + (y*VGAMEM_COLUMNS)] = (VGAMEM_FONT_HALFWIDTH
		| (fg1 << VGAMEM_HW_FG1_BIT)
		| (fg2 << VGAMEM_HW_FG2_BIT)
		| (bg1 << VGAMEM_HW_BG1_BIT)
		| (bg2 << VGAMEM_HW_BG2_BIT)
		| (vgamem_pack_halfw(c1) << VGAMEM_HW_CHAR1_BIT)
		| (vgamem_pack_halfw(c2) << VGAMEM_HW_CHAR2_BIT));
}

/* --------------------------------------------------------------------- */
/* boxes */

void draw_box(int xs, int ys, int xe, int ye, uint32_t flags)
{
	static const uint8_t colors[5][2] = { {3, 1}, {1, 3}, {3, 3}, {1, 1}, {0, 0} };

	enum {
		BOX_THIN_INNER = 0,
		BOX_THIN_OUTER,
		BOX_CORNER_OUTER,
		BOX_THICK_INNER,
		BOX_THICK_OUTER,
	};

	static const uint8_t boxes[5][8] = {
		[BOX_THIN_INNER]   = {139, 138, 137, 136, 134, 129, 132, 131},
		[BOX_THIN_OUTER]   = {128, 130, 133, 135, 129, 134, 131, 132},
		[BOX_CORNER_OUTER] = {128, 141, 140, 135, 129, 134, 131, 132},
		[BOX_THICK_INNER]  = {153, 152, 151, 150, 148, 143, 146, 145},
		[BOX_THICK_OUTER]  = {142, 144, 147, 149, 143, 148, 145, 146},
	};

	uint8_t tl = colors[flags & BOX_SHADE_MASK][0];
	uint8_t br = colors[flags & BOX_SHADE_MASK][1];
	uint8_t trbl;
	uint32_t box;

	int n;
	CHECK_INVERT(tl, br, n);

	switch (flags & (BOX_TYPE_MASK | BOX_THICKNESS_MASK)) {
	case BOX_THIN | BOX_INNER:
		trbl = br;
		box = BOX_THIN_INNER;
		break;
	case BOX_THICK | BOX_INNER:
		trbl = tl;
		box = BOX_THICK_INNER;
		break;
	case BOX_THIN | BOX_OUTER:
		trbl = br = tl;
		box = BOX_THIN_OUTER;
		break;
	case BOX_THICK | BOX_OUTER:
		trbl = br = tl;
		box = BOX_THICK_OUTER;
		break;
	case BOX_THIN | BOX_CORNER:
	case BOX_THICK | BOX_CORNER:
		trbl = 1;
		box = BOX_CORNER_OUTER;
		break;
	default:
		return; // ?
	}

	/* now, the actual magic :) */
	draw_char(boxes[box][0], xs, ys, tl, 2);       /* TL corner */
	draw_char(boxes[box][1], xe, ys, trbl, 2);     /* TR corner */
	draw_char(boxes[box][2], xs, ye, trbl, 2);     /* BL corner */
	draw_char(boxes[box][3], xe, ye, br, 2);       /* BR corner */

	for (n = xs + 1; n < xe; n++) {
		draw_char(boxes[box][4], n, ys, tl, 2);        /* top */
		draw_char(boxes[box][5], n, ye, br, 2);        /* bottom */
	}
	for (n = ys + 1; n < ye; n++) {
		draw_char(boxes[box][6], xs, n, tl, 2);        /* left */
		draw_char(boxes[box][7], xe, n, br, 2);        /* right */
	}
}

/* ----------------------------------------------------------------- */

static inline void _draw_thumb_bar_internal(int width, int x, int y,
						int val, uint32_t fg)
{
	const uint8_t thumb_chars[2][8] = {
		{155, 156, 157, 158, 159, 160, 161, 162},
		{0, 0, 0, 163, 164, 165, 166, 167}
	};
	int n = ++val >> 3;

	val %= 8;
	draw_fill_chars(x, y, x + n - 1, y, DEFAULT_FG, 0);
	draw_char(thumb_chars[0][val], x + n, y, fg, 0);
	if (++n < width)
		draw_char(thumb_chars[1][val], x + n, y, fg, 0);
	if (++n < width)
		draw_fill_chars(x + n, y, x + width - 1, y, DEFAULT_FG, 0);
}

void draw_thumb_bar(int x, int y, int width, int min, int max, int val,
			int selected)
{
	/* this wouldn't happen in a perfect world :P */
	if (val < min || val > max) {
		draw_fill_chars(x, y, x + width - 1, y, DEFAULT_FG,
				((status.flags & CLASSIC_MODE) ? 2 : 0));
		return;
	}

	/* fix the range so that it's 0->n */
	val -= min;
	max -= min;

	/* draw the bar */
	if (!max)
		_draw_thumb_bar_internal(width, x, y, 0,
				 selected ? 3 : 2);
	else
		_draw_thumb_bar_internal(width, x, y,
				val * (width - 1) * 8 / max,
				selected ? 3 : 2);
}

/* --------------------------------------------------------------------- */
/* VU meters */

void draw_vu_meter(int x, int y, int width, int val, int color, int peak)
{
	const uint8_t endtext[8][3] = {
		{174, 0, 0}, {175, 0, 0}, {176, 0, 0}, {176, 177, 0},
		{176, 178, 0}, {176, 179, 180}, {176, 179, 181},
		{176, 179, 182},
	};
	int leftover;
	int chunks = (width / 3);
	int maxval = width * 8 / 3;

	/* reduced from (val * maxval / 64) */
	val = CLAMP((val*width/24), 0, (maxval-1));
	if (!val)
		return;

	leftover = val & 7;
	val >>= 3;
	if ((val < chunks - 1) || (status.flags & CLASSIC_MODE))
		peak = color;

	draw_char(endtext[leftover][0], 3 * val + x + 0, y, peak, 0);
	draw_char(endtext[leftover][1], 3 * val + x + 1, y, peak, 0);
	draw_char(endtext[leftover][2], 3 * val + x + 2, y, peak, 0);
	while (val--) {
		draw_char(176, 3 * val + x + 0, y, color, 0);
		draw_char(179, 3 * val + x + 1, y, color, 0);
		draw_char(182, 3 * val + x + 2, y, color, 0);
	}
}

/* --------------------------------------------------------------------- */
/* sample drawing
 *
 * output channels = number of oscis
 * input channels = number of channels in data
*/

/* somewhat heavily based on CViewSample::DrawSampleData2 in modplug
 *
 * TODO: vectorized sample drawing. */
#define DRAW_SAMPLE_DATA_VARIANT(bits, doublebits) \
	static void _draw_sample_data_##bits(struct vgamem_overlay *r, \
		int##bits##_t *data, uint32_t length, unsigned int inputchans, unsigned int outputchans) \
	{ \
		const int32_t nh = r->height / outputchans; \
		int32_t np = r->height - nh / 2; \
		uint32_t cc; \
		uint64_t step; \
	\
		length /= inputchans; \
		step = ((uint64_t)length << 32) / r->width; \
	\
		for (cc = 0; cc < outputchans; cc++) { \
			int x; \
			uint64_t poshi = 0, poslo = 0; \
	\
			for (x = 0; x < r->width; x++) { \
				uint32_t scanlength, i; \
				int##bits##_t min = INT##bits##_MAX, max = INT##bits##_MIN; \
	\
				poslo += step; \
				scanlength = ((poslo + UINT32_C(0xFFFFFFFF)) >> 32); \
				if (poshi >= length) poshi = length - 1; \
				if (poshi + scanlength > length) scanlength = length - poshi; \
				scanlength = MAX(scanlength, 1); \
	\
				for (i = 0; i < scanlength; i++) { \
					uint32_t co = 0; \
	\
					do { \
						int##bits##_t s = data[((poshi + i) * inputchans) + cc + co]; \
						if (s < min) min = s; \
						if (s > max) max = s; \
					} while (co++ < inputchans - outputchans); \
				} \
	\
				/* XXX is doing this with integers faster than say, floating point?
				 * I mean, it sure is a bit more ~accurate~ at least, and it'll work the same everywhere. */ \
				min = rshift_signed((int##doublebits##_t)min * nh, bits); \
				max = rshift_signed((int##doublebits##_t)max * nh, bits); \
	\
				vgamem_ovl_drawline(r, x, np - 1 - max, x, np - 1 - min, SAMPLE_DATA_COLOR); \
	\
				poshi += (poslo >> 32); \
				poslo &= UINT32_C(0xFFFFFFFF); \
			} \
	\
			np -= nh; \
		} \
	}

DRAW_SAMPLE_DATA_VARIANT(8, 16)
DRAW_SAMPLE_DATA_VARIANT(16, 32)
DRAW_SAMPLE_DATA_VARIANT(32, 64)

#undef DRAW_SAMPLE_DATA_VARIANT

/* --------------------------------------------------------------------- */
/* these functions assume the screen is locked! */

/* loop drawing */
static void _draw_sample_loop(struct vgamem_overlay *r, song_sample_t * sample)
{
	int loopstart, loopend, y;
	int c = ((status.flags & CLASSIC_MODE) ? SAMPLE_DATA_COLOR : SAMPLE_LOOP_COLOR);

	if (!(sample->flags & CHN_LOOP))
		return;

	loopstart = sample->loop_start * (r->width - 1) / sample->length;
	loopend = sample->loop_end * (r->width - 1) / sample->length;

	y = 0;
	do {
		vgamem_ovl_drawpixel(r, loopstart, y, 0); vgamem_ovl_drawpixel(r, loopend, y, 0); y++;
		vgamem_ovl_drawpixel(r, loopstart, y, c); vgamem_ovl_drawpixel(r, loopend, y, c); y++;
		vgamem_ovl_drawpixel(r, loopstart, y, c); vgamem_ovl_drawpixel(r, loopend, y, c); y++;
		vgamem_ovl_drawpixel(r, loopstart, y, 0); vgamem_ovl_drawpixel(r, loopend, y, 0); y++;
	} while (y < r->height);
}

static void _draw_sample_susloop(struct vgamem_overlay *r, song_sample_t * sample)
{
	int loopstart, loopend, y;
	int c = ((status.flags & CLASSIC_MODE) ? SAMPLE_DATA_COLOR : SAMPLE_LOOP_COLOR);

	if (!(sample->flags & CHN_SUSTAINLOOP))
		return;

	loopstart = sample->sustain_start * (r->width - 1) / sample->length;
	loopend = sample->sustain_end * (r->width - 1) / sample->length;

	y = 0;
	do {
		vgamem_ovl_drawpixel(r, loopstart, y, c); vgamem_ovl_drawpixel(r, loopend, y, c); y++;
		vgamem_ovl_drawpixel(r, loopstart, y, 0); vgamem_ovl_drawpixel(r, loopend, y, 0); y++;
		vgamem_ovl_drawpixel(r, loopstart, y, c); vgamem_ovl_drawpixel(r, loopend, y, c); y++;
		vgamem_ovl_drawpixel(r, loopstart, y, 0); vgamem_ovl_drawpixel(r, loopend, y, 0); y++;
	} while (y < r->height);
}

/* this does the lines for playing samples */
static void _draw_sample_play_marks(struct vgamem_overlay *r, song_sample_t * sample)
{
	int n, x, y;
	int c;
	song_voice_t *channel;
	uint32_t *channel_list;

	if (song_get_mode() == MODE_STOPPED)
		return;

	song_lock_audio();

	n = song_get_mix_state(&channel_list);
	while (n--) {
		channel = song_get_mix_channel(channel_list[n]);
		if (channel->current_sample_data != sample->data)
			continue;
		if (!channel->final_volume) continue;
		c = (channel->flags & (CHN_KEYOFF | CHN_NOTEFADE)) ? SAMPLE_BGMARK_COLOR : SAMPLE_MARK_COLOR;
		x = channel->position * (r->width - 1) / sample->length;
		if (x >= r->width) {
			/* this does, in fact, happen :( */
			continue;
		}
		y = 0;
		do {
			/* unrolled 8 times */
			vgamem_ovl_drawpixel(r, x, y++, c);
			vgamem_ovl_drawpixel(r, x, y++, c);
			vgamem_ovl_drawpixel(r, x, y++, c);
			vgamem_ovl_drawpixel(r, x, y++, c);
			vgamem_ovl_drawpixel(r, x, y++, c);
			vgamem_ovl_drawpixel(r, x, y++, c);
			vgamem_ovl_drawpixel(r, x, y++, c);
			vgamem_ovl_drawpixel(r, x, y++, c);
		} while (y < r->height);
	}

	song_unlock_audio();
}

/* --------------------------------------------------------------------- */
/* meat! */

void draw_sample_data(struct vgamem_overlay *r, song_sample_t *sample)
{
	vgamem_ovl_clear(r, 0);

	if (sample->flags & CHN_ADLIB) {
		vgamem_ovl_clear(r, 2);
		vgamem_ovl_apply(r);
		char buf1[32], buf2[32];

		int y1 = r->y1, y2 = y1+3;

		draw_box(59,y1, 77,y2, BOX_THICK | BOX_INNER | BOX_INSET); // data
		draw_box(54,y1, 58,y2, BOX_THIN | BOX_INNER | BOX_OUTSET); // button
		draw_text_len("Mod", 3, 55,y1+1, 0,2);
		draw_text_len("Car", 3, 55,y1+2, 0,2);

		sprintf(buf1, "%02X %02X %02X %02X %02X %02X", // length:6*3-1=17
			sample->adlib_bytes[0],
			sample->adlib_bytes[2],
			sample->adlib_bytes[4],
			sample->adlib_bytes[6],
			sample->adlib_bytes[8],
			sample->adlib_bytes[10]);
		sprintf(buf2, "%02X %02X %02X %02X %02X",      // length: 5*3-1=14
			sample->adlib_bytes[1],
			sample->adlib_bytes[3],
			sample->adlib_bytes[5],
			sample->adlib_bytes[7],
			sample->adlib_bytes[9]);
		draw_text_len(buf1, 17, 60,y1+1, 2,0);
		draw_text_len(buf2, 17, 60,y1+2, 2,0);
		return;
	}

	if (!sample->length || !sample->data) {
		vgamem_ovl_apply(r);
		return;
	}

	/* do the actual drawing */
	int chans = sample->flags & CHN_STEREO ? 2 : 1;
	if (sample->flags & CHN_16BIT)
		_draw_sample_data_16(r, (signed short *) sample->data,
				sample->length * chans,
				chans, chans);
	else
		_draw_sample_data_8(r, sample->data,
				sample->length * chans,
				chans, chans);

	if ((status.flags & CLASSIC_MODE) == 0)
		_draw_sample_play_marks(r, sample);
	_draw_sample_loop(r, sample);
	_draw_sample_susloop(r, sample);
	vgamem_ovl_apply(r);
}

void draw_sample_data_rect_32(struct vgamem_overlay *r, int32_t *data,
	int length, unsigned int inputchans, unsigned int outputchans)
{
	vgamem_ovl_clear(r, 0);
	_draw_sample_data_32(r, data, length, inputchans, outputchans);
	vgamem_ovl_apply(r);
}

void draw_sample_data_rect_16(struct vgamem_overlay *r, int16_t *data,
	int length, unsigned int inputchans, unsigned int outputchans)
{
	vgamem_ovl_clear(r, 0);
	_draw_sample_data_16(r, data, length, inputchans, outputchans);
	vgamem_ovl_apply(r);
}

void draw_sample_data_rect_8(struct vgamem_overlay *r, int8_t *data,
	int length, unsigned int inputchans, unsigned int outputchans)
{
	vgamem_ovl_clear(r, 0);
	_draw_sample_data_8(r, data, length, inputchans, outputchans);
	vgamem_ovl_apply(r);
}
