#include "headers.h"

#include "it.h"
#include "page.h"

/* --------------------------------------------------------------------- */
/* Thumb bars */

static inline void _draw_thumb_bar_internal(int width, int x, int y,
                                            int val, Uint32 fg)
{
        const byte thumb_chars[2][8] = {
                {155, 156, 157, 158, 159, 160, 161, 162},
                {0, 0, 0, 163, 164, 165, 166, 167}
        };
        int n = ++val >> 3;

        val %= 8;
        draw_fill_chars(x, y, x + n - 1, y, 0);
        SDL_LockSurface(screen);
        draw_char_unlocked(thumb_chars[0][val], x + n, y, fg, 0);
        if (++n < width)
                draw_char_unlocked(thumb_chars[1][val], x + n, y, fg, 0);
        SDL_UnlockSurface(screen);
        if (++n < width)
                draw_fill_chars(x + n, y, x + width - 1, y, 0);
}

void draw_thumb_bar(int x, int y, int width, int min, int max, int val,
                    int selected)
{
        /* this wouldn't happen in a perfect world :P */
        if (val < min || val > max) {
                draw_fill_chars(x, y, x + width - 1, y,
                                ((status.flags & CLASSIC_MODE) ? 2 : 0));
                return;
        }

        /* fix the range so that it's 0->n */
        val -= min;
        max -= min;

        /* draw the bar */
        _draw_thumb_bar_internal(width, x, y, val * (width - 1) * 8 / max,
                                 selected ? 3 : 2);
}

/* --------------------------------------------------------------------- */
/* VU meters */

/* 24-char-wide VU meter (for the volume bars on the info page) */
void draw_vu_meter(int x, int y, int val, int color, int peak)
{
        const byte endtext[8][3] = {
                {174, 0, 0}, {175, 0, 0}, {176, 0, 0}, {176, 177, 0},
                {176, 178, 0}, {176, 179, 180}, {176, 179, 181},
                {176, 179, 182},
        };
        int fg, leftover;

        if (!val)
                return;
        val--;
        leftover = val & 7;
        val >>= 3;
        fg = ((val == 7
               && ((status.flags & CLASSIC_MODE) == 0)) ? peak : color);

        SDL_LockSurface(screen);

        draw_char_unlocked(endtext[leftover][0], 3 * val + x + 0, y, fg,
                           0);
        draw_char_unlocked(endtext[leftover][1], 3 * val + x + 1, y, fg,
                           0);
        draw_char_unlocked(endtext[leftover][2], 3 * val + x + 2, y, fg,
                           0);
        while (val) {
                val--;
                draw_char_unlocked(176, 3 * val + x + 0, y, color, 0);
                draw_char_unlocked(179, 3 * val + x + 1, y, color, 0);
                draw_char_unlocked(182, 3 * val + x + 2, y, color, 0);
        }

        SDL_UnlockSurface(screen);
}
