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
#include "video.h"

#include "sdlmain.h"

/* --------------------------------------------------------------------- */
/* sample drawing
there are only two changes between 8- and 16-bit samples:
- the type of 'data'
- the amount to divide (note though, this number is used twice!) */

static void _draw_sample_data_8(struct vgamem_overlay *r,
	signed char *data, unsigned long length, unsigned int channels) // 8/16
{
	unsigned long pos;
	int level, xs, ys, xe, ye, step;

	level = data[0] * r->height / (SCHAR_MAX - SCHAR_MIN + 1); // 8/16
	xs = 0;
	ys = (r->height / 2 - 1) - level;
	step = MAX(1, length / (r->width << 8));
	
	for (pos = 0; pos < length; pos += step) {
		level = data[pos] * r->height / (SCHAR_MAX - SCHAR_MIN + 1); // 8/16
		xe = pos * r->width / length;
		ye = (r->height / 2 - 1) - level;
		if (xs == ys && xe == ye)
			continue;
		vgamem_font_drawline(r, xs, ys, xe, ye);
		xs = xe;
	        ys = ye;
	}
}

static void _draw_sample_data_16(struct vgamem_overlay *r,
	 signed short *data, unsigned long length, unsigned int channels)
{
	unsigned long pos;
	int level, xs, ys, xe, ye, step;

	level = (data[0] * r->height) / (SHRT_MAX - SHRT_MIN + 1);
	xs = 0;
	ys = (r->height / 2 - 1) - level;
	step = MAX(1, length / (r->width << 8));
	for (pos = 0; pos < length; pos += step) {
		level = (data[pos] * r->height) / (SHRT_MAX - SHRT_MIN + 1);
		xe = pos * r->width / length;
		ye = (r->height / 2 - 1) - level;
		if (xs == ys && xe == ye)
			continue;
		vgamem_font_drawline(r, xs, ys, xe, ye);
		xs = xe;
	        ys = ye;
	}
}

/* --------------------------------------------------------------------- */
/* these functions assume the screen is locked! */

/* loop drawing */
static void _draw_sample_loop(struct vgamem_overlay *r, song_sample * sample)
{
        int loopstart, loopend, y;
#if 0
        int c = ((status.flags & CLASSIC_MODE) ? 13 : 3);
#endif

        if (!(sample->flags & SAMP_LOOP))
                return;

        loopstart = sample->loop_start * (r->width - 1) / sample->length;
        loopend = sample->loop_end * (r->width - 1) / sample->length;
	
	y = 0;
	do {
		vgamem_font_clearpixel(r,loopstart,y);
		vgamem_font_clearpixel(r,loopend,y);
		y++;
		vgamem_font_putpixel(r, loopstart, y);
		vgamem_font_putpixel(r, loopend, y);
		y++;
		vgamem_font_putpixel(r, loopstart, y);
		vgamem_font_putpixel(r, loopend, y);
		y++;
		vgamem_font_clearpixel(r,loopstart,y);
		vgamem_font_clearpixel(r,loopend,y);
		y++;
        } while (y < r->height);
}

static void _draw_sample_susloop(struct vgamem_overlay *r, song_sample * sample)
{
        int loopstart, loopend, y;
#if 0
        int c = ((status.flags & CLASSIC_MODE) ? 13 : 3);
#endif

        if (!(sample->flags & SAMP_SUSLOOP))
                return;

	loopstart = sample->sustain_start * (r->width - 1) / sample->length;
	loopend = sample->sustain_end * (r->width - 1) / sample->length;

	y = 0;
	do {
		vgamem_font_clearpixel(r,loopstart,y);
		vgamem_font_clearpixel(r,loopend,y);
		y++;
		vgamem_font_putpixel(r, loopstart, y);
		vgamem_font_putpixel(r, loopend, y);
		y++;
		vgamem_font_putpixel(r, loopstart, y);
		vgamem_font_putpixel(r, loopend, y);
		y++;
		vgamem_font_clearpixel(r,loopstart,y);
		vgamem_font_clearpixel(r,loopend,y);
		y++;
	} while (y < r->height);
}

/* this does the lines for playing samples */
static void _draw_sample_play_marks(struct vgamem_overlay *r, song_sample * sample)
{
        int n, x, y;
#if 0
	int c;
#endif
        song_mix_channel *channel;
        unsigned long *channel_list;

        if (song_get_mode() == MODE_STOPPED)
                return;

        song_lock_audio();

        n = song_get_mix_state(&channel_list);
        while (n--) {
                channel = song_get_mix_channel(channel_list[n]);
                if (channel->sample_data != sample->data)
                        continue;
		if (!channel->final_volume) continue;
#if 0
		c = (channel->flags & (CHN_KEYOFF | CHN_NOTEFADE)) ? 7 : 6;
#endif
                x = channel->sample_pos * (r->width - 1) / sample->length;
                if (x >= r->width) {
                        /* this does, in fact, happen :( */
                        continue;
                }
                y = 0;
                do {
                        /* unrolled 8 times */
			vgamem_font_putpixel(r,x,y++);
			vgamem_font_putpixel(r,x,y++);
			vgamem_font_putpixel(r,x,y++);
			vgamem_font_putpixel(r,x,y++);
			vgamem_font_putpixel(r,x,y++);
			vgamem_font_putpixel(r,x,y++);
			vgamem_font_putpixel(r,x,y++);
			vgamem_font_putpixel(r,x,y++);
                } while (y < r->height);
        }
	
        song_unlock_audio();
}

/* --------------------------------------------------------------------- */
/* meat! */

/* use sample #0 for the sample library */
void draw_sample_data(struct vgamem_overlay *r, song_sample *sample, int n)
{
	int need_draw = 0;
	
	vgamem_clear_reserve(r);
        if (!sample->length) {
		vgamem_fill_reserve(r, 13, 0);
		return;
	}
	
	/* do the actual drawing */
	if (sample->flags & SAMP_16_BIT)
		_draw_sample_data_16(r, (signed short *) sample->data,
                                             sample->length,
				sample->flags & SAMP_STEREO ? 2 : 1);
	else
		_draw_sample_data_8(r, sample->data, sample->length,
				sample->flags & SAMP_STEREO ? 2 : 1);

        if ((status.flags & CLASSIC_MODE) == 0)
                _draw_sample_play_marks(r, sample);
        _draw_sample_loop(r, sample);
        _draw_sample_susloop(r, sample);
	vgamem_fill_reserve(r, 13, 0);
}

/* For the oscilloscope view thing.
 * I bet this gets really screwed up with 8-bit mixing. */
void draw_sample_data_rect_16(struct vgamem_overlay *r, signed short *data, int length, unsigned int channels)
{
	vgamem_clear_reserve(r);
	_draw_sample_data_16(r, data, length, channels);
	vgamem_fill_reserve(r, 13, 0);
}
void draw_sample_data_rect_8(struct vgamem_overlay *r, signed char *data, int length, unsigned int channels)
{
	vgamem_clear_reserve(r);
	_draw_sample_data_8(r, data, length, channels);
	vgamem_fill_reserve(r, 13, 0);
}
