#include "headers.h"

#include <SDL.h>

#include "it.h"
#include "song.h"
#include "page.h"

/* --------------------------------------------------------------------- */
/* waveform cache */

/* waveform #0 is used for the sample library.
 * (well, theoretically, as I haven't gotten to that stuff yet)
 * the rest are of course for the sample list. */

static SDL_Surface *waveform_cache[100] = { NULL };

/* --------------------------------------------------------------------- */

inline void clear_cached_waveform(int sample)
{
        if (waveform_cache[sample]) {
                SDL_FreeSurface(waveform_cache[sample]);
                waveform_cache[sample] = NULL;
        }
}

void clear_all_cached_waveforms(void)
{
        int n;
        for (n = 0; n < 100; n++)
                clear_cached_waveform(n);
}

/* --------------------------------------------------------------------- */
/* sample drawing
 * there are only two changes between 8- and 16-bit samples:
 * - the type of 'data'
 * - the amount to shift (note though, this number is used twice!) */

static inline void _draw_sample_data_8(SDL_Surface * surface,
                                       signed char *data,
                                       unsigned long length)
{
        unsigned long pos = length;
        int level1, level2 = data[length] >> 3;
        int xs, ys, xe, ye;

        while (pos) {
                level1 = data[pos - 1] >> 3;
                xs = pos * (surface->w - 1) / length;
                ys = (surface->h / 2 - 1) - level2;
                pos--;
                xe = pos * (surface->w - 1) / length;
                ye = (surface->h / 2 - 1) - level1;
                draw_line(surface, xs, ys, xe, ye, 13);
                level2 = level1;
        }
}

static inline void _draw_sample_data_16(SDL_Surface * surface,
                                        signed short *data,
                                        unsigned long length)
{
        unsigned long pos = length;
        int level1, level2 = data[length] >> 11;
        int xs, ys, xe, ye;

        while (pos) {
                level1 = data[pos - 1] >> 11;
                xs = pos * (surface->w - 1) / length;
                ys = (surface->h / 2 - 1) - level2;
                pos--;
                xe = pos * (surface->w - 1) / length;
                ye = (surface->h / 2 - 1) - level1;
                draw_line(surface, xs, ys, xe, ye, 13);
                level2 = level1;
        }
}

/* --------------------------------------------------------------------- */
/* loop drawing */

static inline void _draw_sample_loop(SDL_Rect * rect, song_sample * sample)
{
        int loopstart, loopend, y;
        int c = ((status.flags & CLASSIC_MODE) ? 13 : 3);

        if (!(sample->flags & SAMP_LOOP))
                return;

        loopstart =
                rect->x + sample->loop_start * (rect->w -
                                                1) / sample->length;
        loopend =
                rect->x + sample->loop_end * (rect->w -
                                              1) / sample->length;

        y = rect->y;
        do {
                putpixel_screen(loopstart, y, 0);
                putpixel_screen(loopend, y, 0);
                y++;
                putpixel_screen(loopstart, y, c);
                putpixel_screen(loopend, y, c);
                y++;
                putpixel_screen(loopstart, y, c);
                putpixel_screen(loopend, y, c);
                y++;
                putpixel_screen(loopstart, y, 0);
                putpixel_screen(loopend, y, 0);
                y++;
        } while (y < rect->y + rect->h);
}

static inline void _draw_sample_susloop(SDL_Rect * rect,
                                        song_sample * sample)
{
        int loopstart, loopend, y;
        int c = ((status.flags & CLASSIC_MODE) ? 13 : 3);

        if (!(sample->flags & SAMP_SUSLOOP))
                return;

        loopstart =
                rect->x + sample->sustain_start * (rect->w -
                                                   1) / sample->length;
        loopend =
                rect->x + sample->sustain_end * (rect->w -
                                                 1) / sample->length;

        y = rect->y;
        do {
                /* unrolled once */
                putpixel_screen(loopstart, y, c);
                putpixel_screen(loopend, y, c);
                y++;
                putpixel_screen(loopstart, y, 0);
                putpixel_screen(loopend, y, 0);
                y++;
                putpixel_screen(loopstart, y, c);
                putpixel_screen(loopend, y, c);
                y++;
                putpixel_screen(loopstart, y, 0);
                putpixel_screen(loopend, y, 0);
                y++;
        } while (y < rect->y + rect->h);
}

/* --------------------------------------------------------------------- */
/* this does the lines for playing samples */

static inline void _draw_sample_play_marks(SDL_Rect * rect,
                                           song_sample * sample)
{
        int n, x, y, c;
        song_mix_channel *channel;
        unsigned long *channel_list;

        if (song_get_mode() == MODE_STOPPED)
                return;

        SDL_LockAudio();

        n = song_get_mix_state(&channel_list);
        while (n--) {
                channel = song_get_mix_channel(channel_list[n]);
                if (channel->sample_data != sample->data)
                        continue;
                c = (channel->flags & (CHN_KEYOFF | CHN_NOTEFADE)) ? 7 : 6;
                x = channel->sample_pos * (rect->w - 1) / sample->length;
                if (x >= rect->w) {
                        /* this does, in fact, happen :( */
                        continue;
                }
                x += rect->x;
                y = rect->y;
                do {
                        /* unrolled 8 times */
                        putpixel_screen(x, y++, c);
                        putpixel_screen(x, y++, c);
                        putpixel_screen(x, y++, c);
                        putpixel_screen(x, y++, c);
                        putpixel_screen(x, y++, c);
                        putpixel_screen(x, y++, c);
                        putpixel_screen(x, y++, c);
                        putpixel_screen(x, y++, c);
                } while (y < rect->y + rect->h);
        }

        SDL_UnlockAudio();
}

/* --------------------------------------------------------------------- */

static inline void copy_cache(int number, SDL_Rect * rect)
{
        Uint8 *cacheptr = waveform_cache[number]->pixels;
        int x, y;

        /* TODO: optimize this a whole much */
        SDL_LockSurface(screen);
        y = rect->y;
        do {
                x = rect->x;
                do {
                        /* unrolled 8 times */
                        putpixel_screen(x++, y, *cacheptr++);
                        putpixel_screen(x++, y, *cacheptr++);
                        putpixel_screen(x++, y, *cacheptr++);
                        putpixel_screen(x++, y, *cacheptr++);
                        putpixel_screen(x++, y, *cacheptr++);
                        putpixel_screen(x++, y, *cacheptr++);
                        putpixel_screen(x++, y, *cacheptr++);
                        putpixel_screen(x++, y, *cacheptr++);
                } while (x < rect->x + rect->w);
                y++;
        } while (y < rect->y + rect->h);
        SDL_UnlockSurface(screen);
}

/* --------------------------------------------------------------------- */
/* meat! */

/* use sample #0 for the sample library */
void draw_sample_data(SDL_Rect * rect, song_sample * sample, int number)
{
        draw_fill_rect(rect, 0);

        if (!sample->length)
                return;

        if (!waveform_cache[number]) {
                waveform_cache[number] =
                        SDL_CreateRGBSurface(SDL_SWSURFACE, rect->w,
                                             rect->h, 8, 0, 0, 0, 0);
                /* the cache better not need locking, 'cuz I'm not doing
                 * it. :) (it shouldn't, as it's a software surface) */
                if (sample->flags & SAMP_16_BIT)
                        _draw_sample_data_16(waveform_cache[number],
                                             (signed short *) sample->data,
                                             sample->length);
                else
                        _draw_sample_data_8(waveform_cache[number],
                                            sample->data, sample->length);
        }
        copy_cache(number, rect);
        SDL_LockSurface(screen);
        if ((status.flags & CLASSIC_MODE) == 0)
                _draw_sample_play_marks(rect, sample);
        _draw_sample_loop(rect, sample);
        _draw_sample_susloop(rect, sample);
        SDL_UnlockSurface(screen);
}
