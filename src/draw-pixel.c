#include "headers.h"

#include <SDL.h>

#include "it.h"
#include "palettes.h"

#ifndef ABS
# define ABS(x) ((x) < 0 ? -(x) : (x))
#endif
#ifndef SGN
# define SGN(x) ((x) < 0 ? -1 : 1)      /* hey, what about zero? */
#endif

/* --------------------------------------------------------------------- */
/* palette */

int current_palette = 2;

static Uint32 palette_lookup[16] = { 0 };

#define VALUE_TRANSLATE(n) ((int) ((n) / 63.0 * 255.0 + 0.5))

void palette_set(byte colors[16][3])
{
        int n;

        if (screen->format->BytesPerPixel == 1) {
                SDL_Color c[16];

                for (n = 0; n < 16; n++) {
                        c[n].r = VALUE_TRANSLATE(colors[n][0]);
                        c[n].g = VALUE_TRANSLATE(colors[n][1]);
                        c[n].b = VALUE_TRANSLATE(colors[n][2]);
                }
                SDL_SetColors(screen, c, 0, 16);
        } else {
                for (n = 0; n < 16; n++)
                        palette_lookup[n] =
                                SDL_MapRGB(screen->format,
                                           VALUE_TRANSLATE(colors[n][0]),
                                           VALUE_TRANSLATE(colors[n][1]),
                                           VALUE_TRANSLATE(colors[n][2]));
        }
}

void palette_load_preset(int palette_index)
{
        if (palette_index < 0 || palette_index >= NUM_PALETTES)
                return;

        current_palette = palette_index;
        palette_set(palettes[current_palette].colors);
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
        if (screen->format->BytesPerPixel != 1)
                pixel = palette_lookup[pixel];
        putpixel(screen, x, y, pixel);
}

/* --------------------------------------------------------------------- */

inline void draw_fill_rect(SDL_Rect * rect, Uint32 color)
{
        if (screen->format->BytesPerPixel == 1)
                SDL_FillRect(screen, rect, color);
        else
                SDL_FillRect(screen, rect, palette_lookup[color]);
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
        if (screen->format->BytesPerPixel != 1)
                c = palette_lookup[c];
        draw_line(screen, xs, ys, xe, ye, c);
}
