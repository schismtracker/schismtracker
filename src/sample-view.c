/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <someguy@here.is> <http://here.is/someguy/>
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
#include "song.h"
#include "page.h"

#include <SDL.h>

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
 * - the amount to divide (note though, this number is used twice!) */

static inline void _draw_sample_data_8(SDL_Surface * surface, SDL_Rect *rect,
                                       signed char *data, unsigned long length,
				       Uint32 c)
{
	SDL_Rect srect = {0, 0, surface->w, surface->h};
        unsigned long pos = length;
        int level1, level2, xs, ys, xe, ye;
	
	if (!rect)
		rect = &srect;
	
	level2 = data[length] * rect->h / (SCHAR_MAX - SCHAR_MIN + 1);
        while (pos) {
                level1 = data[pos - 1] * rect->h / (SCHAR_MAX - SCHAR_MIN + 1);
                xs = pos * (rect->w - 1) / length + rect->x;
                ys = (rect->h / 2 - 1) - level2 + rect->y;
                pos--;
                xe = pos * (rect->w - 1) / length + rect->x;
                ye = (rect->h / 2 - 1) - level1 + rect->y;
                draw_line(surface, xs, ys, xe, ye, c);
                level2 = level1;
        }
}

static inline void _draw_sample_data_16(SDL_Surface * surface, SDL_Rect *rect,
                                        signed short *data, unsigned long length,
					Uint32 c)
{
	SDL_Rect srect = {0, 0, surface->w, surface->h};
        unsigned long pos = length;
        int level1, level2, xs, ys, xe, ye;
	
	if (!rect)
		rect = &srect;
	
	level2 = data[length] * rect->h / (SHRT_MAX - SHRT_MIN + 1);
        while (pos) {
                level1 = data[pos - 1] * rect->h / (SHRT_MAX - SHRT_MIN + 1);
                xs = pos * (rect->w - 1) / length + rect->x;
                ys = (rect->h / 2 - 1) - level2 + rect->y;
                pos--;
                xe = pos * (rect->w - 1) / length + rect->x;
                ye = (rect->h / 2 - 1) - level1 + rect->y;
                draw_line(surface, xs, ys, xe, ye, c);
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

        loopstart = rect->x + sample->loop_start * (rect->w - 1) / sample->length;
        loopend = rect->x + sample->loop_end * (rect->w - 1) / sample->length;
	
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

static inline void _draw_sample_susloop(SDL_Rect * rect, song_sample * sample)
{
        int loopstart, loopend, y;
        int c = ((status.flags & CLASSIC_MODE) ? 13 : 3);

        if (!(sample->flags & SAMP_SUSLOOP))
                return;

        loopstart = rect->x + sample->sustain_start * (rect->w - 1) / sample->length;
        loopend = rect->x + sample->sustain_end * (rect->w - 1) / sample->length;

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

static inline void _draw_sample_play_marks(SDL_Rect * rect, song_sample * sample)
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

	/* TODO: if rect->w and rect->h don't match that of the cached
	 * surface, destroy it and redraw. */
        if (!waveform_cache[number]) {
                waveform_cache[number] = SDL_CreateRGBSurface
			(SDL_SWSURFACE, rect->w, rect->h, 8, 0, 0, 0, 0);
                /* the cache better not need locking, 'cuz I'm not doing
                 * it. :) (it shouldn't, as it's a software surface) */
                if (sample->flags & SAMP_16_BIT)
                        _draw_sample_data_16(waveform_cache[number], NULL,
                                             (signed short *) sample->data,
                                             sample->length, 13);
                else
                        _draw_sample_data_8(waveform_cache[number], NULL,
                                            sample->data, sample->length, 13);
        }
        copy_cache(number, rect);
        SDL_LockSurface(screen);
        if ((status.flags & CLASSIC_MODE) == 0)
                _draw_sample_play_marks(rect, sample);
        _draw_sample_loop(rect, sample);
        _draw_sample_susloop(rect, sample);
        SDL_UnlockSurface(screen);
}

/* For the oscilloscope view thing.
 * I bet this gets really screwed up with 8-bit mixing. */
void draw_sample_data_rect(SDL_Rect *rect, signed short *data, int length)
{
	SDL_LockSurface(screen);
	_draw_sample_data_16(screen, rect, data, length, palette_get(13));
	SDL_UnlockSurface(screen);
}
