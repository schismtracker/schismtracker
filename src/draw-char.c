#include "headers.h"

#include <SDL.h>
#include <assert.h>
#include <errno.h>

#include "it.h"
#include "default-font.h"

/* --------------------------------------------------------------------- */
/* statics */

static byte font_normal[2048];

/* There's no way to change the other fontsets at the moment.
 * (other than recompiling, of course) */
static byte font_alt[2048];
static byte font_half_data[1024] = HALF_WIDTH_FONT;

/* --------------------------------------------------------------------- */
/* globals */

byte *font_data = font_normal;

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

// just the non-itf chars
inline void font_reset_lower(void)
{
        memcpy(font_normal, DEFAULT_LOWER, 1024);
}

// just the itf chars
inline void font_reset_upper(void)
{
        memcpy(font_normal + 1024, DEFAULT_UPPER_ITF, 1024);
        make_half_width_middot();
}

// all together now!
void font_reset(void)
{
        font_reset_lower();
        font_reset_upper();
}

// or kill the upper chars as well
void font_reset_bios(void)
{
        font_reset_lower();

        memcpy(font_normal + 1024, DEFAULT_UPPER_ALT, 1024);
        make_half_width_middot();
}

/* --------------------------------------------------------------------- */

static inline int squeeze_8x16_font(FILE * fp)
{
        byte data_8x16[4096];
        int n;

        if (fread(data_8x16, 4096, 1, fp) != 1)
                return -1;

        // the "ideal" method:
        //
        //  0 ...x.... > 0      0 ...x....
        //  1 ..xxx... \ fold   1 ..xxx...
        //  2 ..xxx... / to 1   2 .xx.xx..
        //  3 .xx.xx.. \ fold   3 xx...xx.
        //  4 .xx.xx.. / to 2   4 xxxxxxx.
        //  5 xx...xx. \ fold   5 xx...xx.
        //  6 xx...xx. / to 3   6 xx...xx.
        //  7 xx...xx. > lost   7 ........
        //  8 xxxxxxx. \ fold
        //  9 xxxxxxx. / to 4
        // 10 xx...xx. \ fold   - copy first/last scanline directly
        // 11 xx...xx. / to 5   - find unique scanlines
        // 12 xx...xx. \ fold   - if more than 6, combine adjacents
        // 13 xx...xx. / to 6     with 'or'
        // 14 xx...xx. > lost
        // 15 ........ > 7

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

        fp = fopen(filename, "rb");
        if (fp == NULL) {
                SDL_SetError("%s: %s", filename, strerror(errno));
                return -1;
        }

        fseek(fp, 0, SEEK_END);
        pos = ftell(fp);
        if (pos == 2050) {
                /* Probably an ITF. Check the version. */

                fseek(fp, -2, SEEK_CUR);
                if (fread(data, 2, 1, fp) < 1) {
                        SDL_SetError("%s: %s", filename,
                                     feof(fp) ? "Unexpected EOF on read" :
                                     strerror(errno));
                        fclose(fp);
                        return -1;
                }
                if (data[1] != 0x2 || data[0] != 0x12) {
                        SDL_SetError("%s: Unsupported ITF file version",
                                     filename);
                        fclose(fp);
                        return -1;
                }
                rewind(fp);
        } else if (pos == 2048) {
                /* It's a raw file -- nothing else to check... */
                rewind(fp);
        } else if (pos == 4096) {
                rewind(fp);
                if (squeeze_8x16_font(fp) == 0) {
                        make_half_width_middot();
                        fclose(fp);
                        return 0;
                } else {
                        SDL_SetError("%s: %s", filename,
                                     feof(fp) ? "Unexpected EOF on read" :
                                     strerror(errno));
                        fclose(fp);
                        return -1;
                }
        } else {
                SDL_SetError("%s: Invalid font file", filename);
                fclose(fp);
                return -1;
        }

        if (fread(font_normal, 2048, 1, fp) != 1) {
                SDL_SetError("%s: %s", filename,
                             feof(fp) ? "Unexpected EOF on read" :
                             strerror(errno));
                fclose(fp);
                return -1;
        }

        make_half_width_middot();

        fclose(fp);
        return 0;
}

int font_save(const char *filename)
{
        FILE *fp;
        byte ver[2] = { 0x12, 0x2 };

        if (!filename)
                return 0;

        fp = fopen(filename, "wb");
        if (fp == NULL) {
                SDL_SetError("%s: %s", filename, strerror(errno));
                return -1;
        }

        if (fwrite(font_normal, 2048, 1, fp) < 1
            || fwrite(ver, 2, 1, fp) < 1) {
                SDL_SetError("%s: %s", filename, strerror(errno));
                fclose(fp);
                return -1;
        }

        fclose(fp);
        return 0;
}

void font_set_bank(int bank)
{
        font_data = bank ? font_alt : font_normal;
}

void font_init(void)
{
        if (font_load("font.cfg") != 0)
                font_reset();
        memcpy(font_alt, DEFAULT_LOWER, 1024);
        memcpy(font_alt + 1024, DEFAULT_UPPER_ALT, 1024);
}

/* --------------------------------------------------------------------- */

void draw_char_unlocked(byte c, int x, int y, Uint32 fg, Uint32 bg)
{
        int pos, scanline, ci = c << 3;

        assert(x >= 0 && y >= 0 && x < 80 && y < 50);

        x <<= 3;
        y <<= 3;
        /* GCC unrolls these... I hope :) */
        for (scanline = 0; scanline < 8; scanline++)
                for (pos = 0; pos < 8; pos++)
                        putpixel_screen(x + pos, y + scanline,
                                        (font_data[ci + scanline] &
                                         (128 >> pos) ? fg : bg));
}

int draw_text_unlocked(const byte * text, int x, int y, Uint32 fg,
                       Uint32 bg)
{
        int n = 0;

        while (*text) {
                draw_char_unlocked(*text, x + n, y, fg, bg);
                n++;
                text++;
        }

        return n;
}

int draw_text_len(const byte * text, int len, int x, int y, Uint32 fg,
                  Uint32 bg)
{
        int n = 0;

        SDL_LockSurface(screen);
        while (*text && n < len) {
                draw_char_unlocked(*text, x + n, y, fg, bg);
                n++;
                text++;
        }
        SDL_UnlockSurface(screen);
        draw_fill_chars(x + n, y, x + len - 1, y, bg);
        return n;
}

/* --------------------------------------------------------------------- */
/* half-width characters */

void draw_half_width_chars_unlocked(byte c1, byte c2, int x, int y,
                                    Uint32 fg1, Uint32 bg1, Uint32 fg2,
                                    Uint32 bg2)
{
        int pos, half_scanline, ci1 = c1 << 2, ci2 = c2 << 2;

        assert(x >= 0 && y >= 0 && x < 80 && y < 50);

        x <<= 3;
        y <<= 3;

        if (c2 == 173) {
                c2 = 184;
                ci2 = 184 << 2;
        }

        for (half_scanline = 0; half_scanline < 4; half_scanline++) {
                int scanline = half_scanline * 2;
                for (pos = 0; pos < 4; pos++) {
                        /* first character */
                        putpixel_screen
                                (x + pos, y + scanline,
                                 (((font_half_data[ci1 + half_scanline]
                                    & 0xf0) >> 4) & (8 >> pos))
                                 ? fg1 : bg1);
                        putpixel_screen
                                (x + pos, y + scanline + 1,
                                 ((font_half_data[ci1 + half_scanline]
                                   & 0xf) & (8 >> pos))
                                 ? fg1 : bg1);
                        /* second character */
                        putpixel_screen
                                (x + pos + 4, y + scanline,
                                 (((font_half_data[ci2 + half_scanline]
                                    & 0xf0) >> 4) & (8 >> pos))
                                 ? fg2 : bg2);
                        putpixel_screen
                                (x + pos + 4, y + scanline + 1,
                                 ((font_half_data[ci2 + half_scanline]
                                   & 0xf) & (8 >> pos))
                                 ? fg2 : bg2);
                }
        }
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

static inline void _draw_box_internal(int xs, int ys, int xe, int ye,
                                      Uint32 tl, Uint32 br,
                                      const byte ch[8])
{
        int n;

        draw_char_unlocked(ch[0], xs, ys, tl, 2);       /* TL corner */
        draw_char_unlocked(ch[1], xe, ys, br, 2);       /* TR corner */
        draw_char_unlocked(ch[2], xs, ye, br, 2);       /* BL corner */
        draw_char_unlocked(ch[3], xe, ye, br, 2);       /* BR corner */

        for (n = xs + 1; n < xe; n++) {
                draw_char_unlocked(ch[4], n, ys, tl, 2);        /* top */
                draw_char_unlocked(ch[5], n, ye, br, 2);        /* bottom */
        }
        for (n = ys + 1; n < ye; n++) {
                draw_char_unlocked(ch[6], xs, n, tl, 2);        /* left */
                draw_char_unlocked(ch[7], xe, n, br, 2);        /* right */
        }
}

void draw_thin_inner_box(int xs, int ys, int xe, int ye, Uint32 tl,
                         Uint32 br)
{
        _draw_box_internal(xs, ys, xe, ye, tl, br, boxes[BOX_THIN_INNER]);
}

void draw_thick_inner_box(int xs, int ys, int xe, int ye, Uint32 tl,
                          Uint32 br)
{
        /* this one can't use _draw_box_internal because the corner
         * colors are different */

        int n;

        draw_char_unlocked(153, xs, ys, tl, 2); /* TL corner */
        draw_char_unlocked(152, xe, ys, tl, 2); /* TR corner */
        draw_char_unlocked(151, xs, ye, tl, 2); /* BL corner */
        draw_char_unlocked(150, xe, ye, br, 2); /* BR corner */

        for (n = xs + 1; n < xe; n++) {
                draw_char_unlocked(148, n, ys, tl, 2);  /* top */
                draw_char_unlocked(143, n, ye, br, 2);  /* bottom */
        }
        for (n = ys + 1; n < ye; n++) {
                draw_char_unlocked(146, xs, n, tl, 2);  /* left */
                draw_char_unlocked(145, xe, n, br, 2);  /* right */
        }
}

void draw_thin_outer_box(int xs, int ys, int xe, int ye, Uint32 c)
{
        _draw_box_internal(xs, ys, xe, ye, c, c, boxes[BOX_THIN_OUTER]);
}

void draw_thin_outer_cornered_box(int xs, int ys, int xe, int ye,
                                  int flags)
{
        const int colors[4][2] = { {3, 1}, {1, 3}, {3, 3}, {1, 1} };
        int tl = colors[flags & BOX_SHADE_MASK][0];
        int br = colors[flags & BOX_SHADE_MASK][1];
        int n;

        draw_char_unlocked(128, xs, ys, tl, 2); /* TL corner */
        draw_char_unlocked(141, xe, ys, 1, 2);  /* TR corner */
        draw_char_unlocked(140, xs, ye, 1, 2);  /* BL corner */
        draw_char_unlocked(135, xe, ye, br, 2); /* BR corner */

        for (n = xs + 1; n < xe; n++) {
                draw_char_unlocked(129, n, ys, tl, 2);  /* top */
                draw_char_unlocked(134, n, ye, br, 2);  /* bottom */
        }

        for (n = ys + 1; n < ye; n++) {
                draw_char_unlocked(131, xs, n, tl, 2);  /* left */
                draw_char_unlocked(132, xe, n, br, 2);  /* right */
        }
}

void draw_thick_outer_box(int xs, int ys, int xe, int ye, Uint32 c)
{
        _draw_box_internal(xs, ys, xe, ye, c, c, boxes[BOX_THICK_OUTER]);
}
