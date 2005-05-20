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
there are only two changes between 8- and 16-bit samples:
- the type of 'data'
- the amount to divide (note though, this number is used twice!) */

static void _draw_sample_data_8(SDL_Surface * surface, SDL_Rect *rect,
				signed char *data, unsigned long length, // 8/16
				Uint32 c)
{
	SDL_Rect srect = {0, 0, surface->w, surface->h};
	unsigned long pos;
	int level, xs, ys, xe, ye, step;

	if (!rect)
		rect = &srect;

	level = data[0] * rect->h / (SCHAR_MAX - SCHAR_MIN + 1); // 8/16
	xs = rect->x;
	ys = (rect->h / 2 - 1) - level + rect->y;
	step = MAX(1, length / (rect->w << 8));
	
	for (pos = 0; pos < length; pos += step) {
		level = data[pos] * rect->h / (SCHAR_MAX - SCHAR_MIN + 1); // 8/16
		xe = pos * rect->w / length + rect->x;
		ye = (rect->h / 2 - 1) - level + rect->y;
		if (xs == ys && xe == ye)
			continue;
		draw_line(surface, xs, ys, xe, ye, c);
		xs = xe;
	        ys = ye;
	}
}

static void _draw_sample_data_16(SDL_Surface * surface, SDL_Rect *rect,
				 signed short *data, unsigned long length,
				 Uint32 c)
{
	SDL_Rect srect = {0, 0, surface->w, surface->h};
	unsigned long pos;
	int level, xs, ys, xe, ye, step;

	if (!rect)
		rect = &srect;

	level = data[0] * rect->h / (SHRT_MAX - SHRT_MIN + 1);
	xs = rect->x;
	ys = (rect->h / 2 - 1) - level + rect->y;
	step = MAX(1, length / (rect->w << 8));
	
	for (pos = 0; pos < length; pos += step) {
		level = data[pos] * rect->h / (SHRT_MAX - SHRT_MIN + 1);
		xe = pos * rect->w / length + rect->x;
		ye = (rect->h / 2 - 1) - level + rect->y;
		if (xs == ys && xe == ye)
			continue;
		draw_line(surface, xs, ys, xe, ye, c);
		xs = xe;
	        ys = ye;
	}
}

/* --------------------------------------------------------------------- */
/* these functions assume the screen is locked! */

/* loop drawing */
static void _draw_sample_loop(SDL_Rect * rect, song_sample * sample)
{
        int loopstart, loopend, y;
        int c = ((status.flags & CLASSIC_MODE) ? 13 : 3);

        if (!(sample->flags & SAMP_LOOP))
                return;

        loopstart = rect->x + sample->loop_start * (rect->w - 1) / sample->length;
        loopend = rect->x + sample->loop_end * (rect->w - 1) / sample->length;
	
        y = rect->y;
        do {
                putpixel(screen, loopstart, y, 0); putpixel(screen, loopend, y, 0); y++;
                putpixel(screen, loopstart, y, c); putpixel(screen, loopend, y, c); y++;
                putpixel(screen, loopstart, y, c); putpixel(screen, loopend, y, c); y++;
                putpixel(screen, loopstart, y, 0); putpixel(screen, loopend, y, 0); y++;
        } while (y < rect->y + rect->h);
}

static void _draw_sample_susloop(SDL_Rect * rect, song_sample * sample)
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
                putpixel(screen, loopstart, y, c); putpixel(screen, loopend, y, c); y++;
                putpixel(screen, loopstart, y, 0); putpixel(screen, loopend, y, 0); y++;
                putpixel(screen, loopstart, y, c); putpixel(screen, loopend, y, c); y++;
                putpixel(screen, loopstart, y, 0); putpixel(screen, loopend, y, 0); y++;
        } while (y < rect->y + rect->h);
}

/* this does the lines for playing samples */
static void _draw_sample_play_marks(SDL_Rect * rect, song_sample * sample)
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
                        putpixel(screen, x, y++, c); putpixel(screen, x, y++, c);
                        putpixel(screen, x, y++, c); putpixel(screen, x, y++, c);
                        putpixel(screen, x, y++, c); putpixel(screen, x, y++, c);
                        putpixel(screen, x, y++, c); putpixel(screen, x, y++, c);
                } while (y < rect->y + rect->h);
        }
	
        SDL_UnlockAudio();
}

/* --------------------------------------------------------------------- */
/* meat! */

/* use sample #0 for the sample library */
void draw_sample_data(SDL_Rect * rect, song_sample *sample, int n)
{
	int need_draw = 0;
	SDL_Surface *s;
	
	draw_fill_rect(screen, rect, 0);
	
        if (!sample->length)
                return;
	
	/* if it already exists, but it's the wrong size, kill it */
	if (waveform_cache[n] && (rect->w != waveform_cache[n]->w || rect->h != waveform_cache[n]->h))
		clear_cached_waveform(n);
	/* if it doesn't exist, create a new surface */
	if (waveform_cache[n] == NULL) {
		s = SDL_CreateRGBSurface
			(SDL_SWSURFACE, rect->w, rect->h, screen->format->BitsPerPixel,
			 screen->format->Rmask, screen->format->Gmask,
			 screen->format->Bmask, screen->format->Amask);
		/* Why is this needed? Isn't the surface using the display format already? */
		waveform_cache[n] = SDL_DisplayFormat(s);
		SDL_FreeSurface(s);
		need_draw = 1;
	}
	
	while (need_draw || SDL_BlitSurface(waveform_cache[n], NULL, screen, rect) == -2) {
		need_draw = 0;
		
		draw_fill_rect(waveform_cache[n], NULL, 0);
		
		if (SDL_MUSTLOCK(waveform_cache[n])) {
			while (SDL_LockSurface(waveform_cache[n]) < 0)
				SDL_Delay(10);
		}
		
		/* do the actual drawing */
                if (sample->flags & SAMP_16_BIT)
                        _draw_sample_data_16(waveform_cache[n], NULL, (signed short *) sample->data,
                                             sample->length, 13);
                else
                        _draw_sample_data_8(waveform_cache[n], NULL, sample->data, sample->length, 13);
		
		if (SDL_MUSTLOCK(waveform_cache[n]))
			SDL_UnlockSurface(waveform_cache[n]);
	}
	
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
	_draw_sample_data_16(screen, rect, data, length, 13);
	SDL_UnlockSurface(screen);
}
