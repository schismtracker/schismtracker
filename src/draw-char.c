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
#include "dmoz.h" /* for dmoz_path_concat */
#include "default-font.h"

#include <SDL.h>
#include <assert.h>
#include <errno.h>

/* preprocessor stuff */

#define CHECK_INVERT(tl,br,n) G_STMT_START {\
	if (status.flags & INVERTED_PALETTE) {\
		n = tl;\
		tl = br;\
		br = n;\
	}\
} G_STMT_END

#define CACHE_INDEX(ch, fg, bg) \
	((((ch) & 0xff) << 8) | ((fg) << 4) | (bg))

/* --------------------------------------------------------------------- */
/* statics */

static byte font_normal[2048];

/* There's no way to change the other fontsets at the moment.
 * (other than recompiling, of course) */
static byte font_alt[2048];
static byte font_half_data[1024];

/* 256 characters * 16 foreground colors * 16 background colors = 65536
Only a couple hundred of these will actually be used... */
static SDL_Surface *cache_normal[65536] = { NULL };
static SDL_Surface *cache_alt[65536] = { NULL };

/* --------------------------------------------------------------------- */
/* globals */

byte *font_data = font_normal; /* this only needs to be global for itf */
static SDL_Surface **charcache = cache_normal;

/* int font_width = 8, font_height = 8; */

/* --------------------------------------------------------------------- */
/* character cache functions */

static inline void ccache_destroy_index(SDL_Surface **cache, int n)
{
	if (cache[n]) {
		SDL_FreeSurface(cache[n]);
		cache[n] = NULL;
	}
}

/* this should probably lock something */
void ccache_destroy_char(int ch)
{
	int n;
	
	ch <<= 8;
	for (n = 0; n < 256; n++) {
		ccache_destroy_index(cache_normal, ch | n);
		ccache_destroy_index(cache_alt, ch | n);
	}
}

/* used by the palette editor */
void ccache_destroy_color(int c)
{
	int n, ch, i;

	c &= 0xf; /* just to be safe */

	for (ch = 0; ch < 256; ch++) {
		for (n = 0; n < 16; n++) {
			i = CACHE_INDEX(ch, c, n);
			ccache_destroy_index(cache_normal, i);
			ccache_destroy_index(cache_alt, i);
			if (c == n)
				continue;
			i = CACHE_INDEX(ch, n, c);
			ccache_destroy_index(cache_normal, i);
			ccache_destroy_index(cache_alt, i);
		}
	}
}

static void ccache_destroy_cache(SDL_Surface **cache)
{
	int n;

	for (n = 0; n < 65536; n++)
		ccache_destroy_index(cache, n);
}

void ccache_destroy(void)
{
	ccache_destroy_cache(cache_normal);
	ccache_destroy_cache(cache_alt);
}

/* --------------------------------------------------------------------- */
/* ITF loader */

static inline void make_half_width_middot(void)
{
        /* this copies the left half of char 184 in the normal font (two
         * half-width dots) to char 173 of the half-width font (the
         * middot), and the right half to char 184. thus, putting
         * together chars 173 and 184 of the half-width font will
         * produce the equivalent of 184 of the full-width font. */

        font_half_data[173 * 4 + 0] =
                (font_normal[184 * 8 + 0] & 0xf0) |
                (font_normal[184 * 8 + 1] & 0xf0) >> 4;
        font_half_data[173 * 4 + 1] =
                (font_normal[184 * 8 + 2] & 0xf0) |
                (font_normal[184 * 8 + 3] & 0xf0) >> 4;
        font_half_data[173 * 4 + 2] =
                (font_normal[184 * 8 + 4] & 0xf0) |
                (font_normal[184 * 8 + 5] & 0xf0) >> 4;
        font_half_data[173 * 4 + 3] =
                (font_normal[184 * 8 + 6] & 0xf0) |
                (font_normal[184 * 8 + 7] & 0xf0) >> 4;

        font_half_data[184 * 4 + 0] =
                (font_normal[184 * 8 + 0] & 0xf) << 4 |
                (font_normal[184 * 8 + 1] & 0xf);
        font_half_data[184 * 4 + 1] =
                (font_normal[184 * 8 + 2] & 0xf) << 4 |
                (font_normal[184 * 8 + 3] & 0xf);
        font_half_data[184 * 4 + 2] =
                (font_normal[184 * 8 + 4] & 0xf) << 4 |
                (font_normal[184 * 8 + 5] & 0xf);
        font_half_data[184 * 4 + 3] =
                (font_normal[184 * 8 + 6] & 0xf) << 4 |
                (font_normal[184 * 8 + 7] & 0xf);
}

/* just the non-itf chars */
void font_reset_lower(void)
{
        memcpy(font_normal, font_default_lower, 1024);
        ccache_destroy_cache(cache_normal);
}

/* just the itf chars */
void font_reset_upper(void)
{
        memcpy(font_normal + 1024, font_default_upper_itf, 1024);
        make_half_width_middot();
        ccache_destroy_cache(cache_normal);
}

/* all together now! */
void font_reset(void)
{
        memcpy(font_normal, font_default_lower, 1024);
        memcpy(font_normal + 1024, font_default_upper_itf, 1024);
        make_half_width_middot();
        ccache_destroy_cache(cache_normal);
}

/* or kill the upper chars as well */
void font_reset_bios(void)
{
        font_reset_lower();
        memcpy(font_normal + 1024, font_default_upper_alt, 1024);
        make_half_width_middot();
}

/* --------------------------------------------------------------------- */

static inline int squeeze_8x16_font(FILE * fp)
{
        byte data_8x16[4096];
        int n;

        if (fread(data_8x16, 4096, 1, fp) != 1)
                return -1;

        for (n = 0; n < 2048; n++)
                font_normal[n] = data_8x16[2 * n] | data_8x16[2 * n + 1];

        return 0;
}

/* Hmm. I could've done better with this one. */
int font_load(const char *filename)
{
        FILE *fp;
        long pos;
        byte data[4];
        char *font_dir, *font_file;

        font_dir = dmoz_path_concat(cfg_dir_dotschism, "fonts");
        font_file = dmoz_path_concat(font_dir, filename);
        free(font_dir);

        fp = fopen(font_file, "rb");
        if (fp == NULL) {
                SDL_SetError("%s: %s", font_file, strerror(errno));
		free(font_file);
                return -1;
        }

        fseek(fp, 0, SEEK_END);
        pos = ftell(fp);
        if (pos == 2050) {
                /* Probably an ITF. Check the version. */

                fseek(fp, -2, SEEK_CUR);
                if (fread(data, 2, 1, fp) < 1) {
                        SDL_SetError("%s: %s", font_file,
                                     feof(fp) ? "Unexpected EOF on read" : strerror(errno));
                        fclose(fp);
			free(font_file);
                        return -1;
                }
                if (data[1] != 0x2 || data[0] != 0x12) {
                        SDL_SetError("%s: Unsupported ITF file version", font_file);
                        fclose(fp);
			free(font_file);
                        return -1;
                }
                rewind(fp);
        } else if (pos == 2048) {
                /* It's a raw file -- nothing else to check... */
                rewind(fp);
        } else if (pos == 4096) {
                rewind(fp);
                if (squeeze_8x16_font(fp) == 0) {
			ccache_destroy();
                        make_half_width_middot();
                        fclose(fp);
			free(font_file);
                        return 0;
                } else {
                        SDL_SetError("%s: %s", font_file,
                                     feof(fp) ? "Unexpected EOF on read" : strerror(errno));
                        fclose(fp);
			free(font_file);
                        return -1;
                }
        } else {
                SDL_SetError("%s: Invalid font file", font_file);
                fclose(fp);
		free(font_file);
                return -1;
        }

        if (fread(font_normal, 2048, 1, fp) != 1) {
                SDL_SetError("%s: %s", font_file,
                             feof(fp) ? "Unexpected EOF on read" : strerror(errno));
                fclose(fp);
		free(font_file);
                return -1;
        }

	ccache_destroy();
        make_half_width_middot();

        fclose(fp);
	free(font_file);
        return 0;
}

int font_save(const char *filename)
{
        FILE *fp;
        byte ver[2] = { 0x12, 0x2 };
        char *font_dir, *font_file;

        font_dir = dmoz_path_concat(cfg_dir_dotschism, "fonts");
        font_file = dmoz_path_concat(font_dir, filename);
	free(font_dir);

        fp = fopen(font_file, "wb");
        if (fp == NULL) {
                SDL_SetError("%s: %s", font_file, strerror(errno));
		free(font_file);
                return -1;
        }

        if (fwrite(font_normal, 2048, 1, fp) < 1 || fwrite(ver, 2, 1, fp) < 1) {
                SDL_SetError("%s: %s", font_file, strerror(errno));
                fclose(fp);
		free(font_file);
                return -1;
        }

        fclose(fp);
	free(font_file);
        return 0;
}

void font_set_bank(int bank)
{
        font_data = bank ? font_alt : font_normal;
        charcache = bank ? cache_alt : cache_normal;
}

void font_init(void)
{
	memcpy(font_half_data, font_half_width, 1024);
	
        if (font_load(cfg_font) != 0)
                font_reset();
	
        memcpy(font_alt, font_default_lower, 1024);
        memcpy(font_alt + 1024, font_default_upper_alt, 1024);
        ccache_destroy_cache(cache_alt);
}

/* --------------------------------------------------------------------- */

static inline void ccache_create_char(SDL_Surface *cached, byte c, Uint32 fg, Uint32 bg)
{
        int pos, scanline, ci = c << 3;
	
	if (SDL_MUSTLOCK(cached))
		SDL_LockSurface(cached);
        /* GCC unrolls these... I hope :) */
        for (scanline = 0; scanline < 8; scanline++)
                for (pos = 0; pos < 8; pos++)
                        putpixel(cached, pos, scanline, font_data[ci + scanline] & (128 >> pos) ? fg : bg);
	if (SDL_MUSTLOCK(cached))
		SDL_UnlockSurface(cached);
}

void draw_char(byte c, int x, int y, Uint32 fg, Uint32 bg)
{
	int need_draw = 0;
	int i = CACHE_INDEX(c, fg, bg);
	SDL_Rect rect;
	
        assert(x >= 0 && y >= 0 && x < 80 && y < 50);
	
	if (charcache[i] == NULL) {
		SDL_Surface *tmp = SDL_CreateRGBSurface
			(SDL_HWSURFACE, 8, 8, screen->format->BitsPerPixel,
			 screen->format->Rmask, screen->format->Gmask,
			 screen->format->Bmask, screen->format->Amask);
		charcache[i] = SDL_DisplayFormat(tmp);
		SDL_FreeSurface(tmp);
		need_draw = 1;
	}
	
	rect.x = x << 3;
	rect.y = y << 3;
	
	while (need_draw || SDL_BlitSurface(charcache[i], NULL, screen, &rect) == -2) {
		need_draw = 0;
		
		if (SDL_MUSTLOCK(charcache[i])) {
			while (SDL_LockSurface(charcache[i]) < 0)
				SDL_Delay(10);
		}
		
		ccache_create_char(charcache[i], c, fg, bg);
		
		if (SDL_MUSTLOCK(charcache[i]))
			SDL_UnlockSurface(charcache[i]);
	}
}

int draw_text(const byte * text, int x, int y, Uint32 fg, Uint32 bg)
{
        int n = 0;

        while (*text) {
                draw_char(*text, x + n, y, fg, bg);
                n++;
                text++;
        }
	
        return n;
}

int draw_text_len(const byte * text, int len, int x, int y, Uint32 fg, Uint32 bg)
{
        int n = 0;

        while (*text && n < len) {
                draw_char(*text, x + n, y, fg, bg);
                n++;
                text++;
        }
        draw_fill_chars(x + n, y, x + len - 1, y, bg);
        return n;
}

/* --------------------------------------------------------------------- */
/* half-width characters */

void draw_half_width_chars(byte c1, byte c2, int x, int y,
			   Uint32 fg1, Uint32 bg1, Uint32 fg2, Uint32 bg2)
{
        int pos, half_scanline, ci1 = c1 << 2, ci2 = c2 << 2;

        assert(x >= 0 && y >= 0 && x < 80 && y < 50);

        x <<= 3;
        y <<= 3;

        if (c2 == 173) {
                c2 = 184;
                ci2 = 184 << 2;
        }

	SDL_LockSurface(screen);
        for (half_scanline = 0; half_scanline < 4; half_scanline++) {
                int scanline = half_scanline * 2;
                for (pos = 0; pos < 4; pos++) {
                        /* first character */
                        putpixel(screen, x + pos, y + scanline,
				 (((font_half_data[ci1 + half_scanline] & 0xf0) >> 4) & (8 >> pos))
				 ? fg1 : bg1);
                        putpixel(screen, x + pos, y + scanline + 1,
				 ((font_half_data[ci1 + half_scanline] & 0xf) & (8 >> pos))
				 ? fg1 : bg1);
                        /* second character */
                        putpixel(screen, x + pos + 4, y + scanline,
				 (((font_half_data[ci2 + half_scanline] & 0xf0) >> 4) & (8 >> pos))
				 ? fg2 : bg2);
                        putpixel(screen, x + pos + 4, y + scanline + 1,
				 ((font_half_data[ci2 + half_scanline] & 0xf) & (8 >> pos))
				 ? fg2 : bg2);
                }
        }
	SDL_UnlockSurface(screen);
}

/* --------------------------------------------------------------------- */
/* boxes */

enum box_type {
        BOX_THIN_INNER = 0, BOX_THIN_OUTER, BOX_THICK_OUTER
};

static const byte boxes[4][8] = {
        {139, 138, 137, 136, 134, 129, 132, 131},       /* thin inner */
        {128, 130, 133, 135, 129, 134, 131, 132},       /* thin outer */
        {142, 144, 147, 149, 143, 148, 145, 146},       /* thick outer */
};

static void _draw_box_internal(int xs, int ys, int xe, int ye, Uint32 tl, Uint32 br, const byte ch[8])
{
        int n;

	CHECK_INVERT(tl, br, n);

        draw_char(ch[0], xs, ys, tl, 2);       /* TL corner */
        draw_char(ch[1], xe, ys, br, 2);       /* TR corner */
        draw_char(ch[2], xs, ye, br, 2);       /* BL corner */
        draw_char(ch[3], xe, ye, br, 2);       /* BR corner */

        for (n = xs + 1; n < xe; n++) {
                draw_char(ch[4], n, ys, tl, 2);        /* top */
                draw_char(ch[5], n, ye, br, 2);        /* bottom */
        }
        for (n = ys + 1; n < ye; n++) {
                draw_char(ch[6], xs, n, tl, 2);        /* left */
                draw_char(ch[7], xe, n, br, 2);        /* right */
        }
}

void draw_thin_inner_box(int xs, int ys, int xe, int ye, Uint32 tl, Uint32 br)
{
        _draw_box_internal(xs, ys, xe, ye, tl, br, boxes[BOX_THIN_INNER]);
}

void draw_thick_inner_box(int xs, int ys, int xe, int ye, Uint32 tl, Uint32 br)
{
        /* this one can't use _draw_box_internal because the corner
         * colors are different */

        int n;

	CHECK_INVERT(tl, br, n);

        draw_char(153, xs, ys, tl, 2); /* TL corner */
        draw_char(152, xe, ys, tl, 2); /* TR corner */
        draw_char(151, xs, ye, tl, 2); /* BL corner */
        draw_char(150, xe, ye, br, 2); /* BR corner */

        for (n = xs + 1; n < xe; n++) {
                draw_char(148, n, ys, tl, 2);  /* top */
                draw_char(143, n, ye, br, 2);  /* bottom */
        }
        for (n = ys + 1; n < ye; n++) {
                draw_char(146, xs, n, tl, 2);  /* left */
                draw_char(145, xe, n, br, 2);  /* right */
        }
}

void draw_thin_outer_box(int xs, int ys, int xe, int ye, Uint32 c)
{
        _draw_box_internal(xs, ys, xe, ye, c, c, boxes[BOX_THIN_OUTER]);
}

void draw_thin_outer_cornered_box(int xs, int ys, int xe, int ye, int flags)
{
        const int colors[4][2] = { {3, 1}, {1, 3}, {3, 3}, {1, 1} };
        int tl = colors[flags & BOX_SHADE_MASK][0];
        int br = colors[flags & BOX_SHADE_MASK][1];
        int n;

	CHECK_INVERT(tl, br, n);

        draw_char(128, xs, ys, tl, 2); /* TL corner */
        draw_char(141, xe, ys, 1, 2);  /* TR corner */
        draw_char(140, xs, ye, 1, 2);  /* BL corner */
        draw_char(135, xe, ye, br, 2); /* BR corner */

        for (n = xs + 1; n < xe; n++) {
                draw_char(129, n, ys, tl, 2);  /* top */
                draw_char(134, n, ye, br, 2);  /* bottom */
        }

        for (n = ys + 1; n < ye; n++) {
                draw_char(131, xs, n, tl, 2);  /* left */
                draw_char(132, xe, n, br, 2);  /* right */
        }
}

void draw_thick_outer_box(int xs, int ys, int xe, int ye, Uint32 c)
{
        _draw_box_internal(xs, ys, xe, ye, c, c, boxes[BOX_THICK_OUTER]);
}
