/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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

#include "it.h"
#include "song.h"
#include "page.h"
#include "video.h"

#include "sdlmain.h"

#define SAMPLE_DATA_COLOR 13 /* Sample data */
#define SAMPLE_LOOP_COLOR 3 /* Sample loop marks */
#define SAMPLE_MARK_COLOR 6 /* Play mark color */
#define SAMPLE_BGMARK_COLOR 7 /* Play mark color after note fade / NNA */

/* --------------------------------------------------------------------- */
/* sample drawing
there are only two changes between 8- and 16-bit samples:
- the type of 'data'
- the amount to divide (note though, this number is used twice!) */

/* do we need 'channels' here? */
static void _draw_sample_data_8(struct vgamem_overlay *r,
        signed char *data, unsigned long length, int channels, int fakemono) // 8/16
{
        unsigned long pos;
        int level, xs, ys, xe, ye, step;
        int nh, cc, np;

        nh = (r->height / channels);
        np = r->height - (nh / 2);

        length /= channels;

        for (cc = 0; cc < channels; cc++) {
                level = (data[cc] * nh) / (SCHAR_MAX - SCHAR_MIN + 1);
                xs = 0;
                ys = (np - 1) - level;
                if (ys < 0) ys = 0;
                if (ys >= r->height) ys = r->height;
                step = MAX(1, (length / r->width) >> 8);
                for (pos = channels+cc; pos < length; pos += step) {
                        level = (data[(pos*channels*fakemono)+cc] * nh)
                                        / (SCHAR_MAX - SCHAR_MIN + 1);
                        xe = pos * r->width / length;
                        ye = (np - 1) - level;
                        if (xe < 0) xe = 0;
                        if (xe >= r->width) xe = (r->width-1);
                        if (ye < 0) ye = 0;
                        if (ye >= r->height) ye = (r->height-1);
                        vgamem_ovl_drawline(r, xs, ys, xe, ye,
                                        SAMPLE_DATA_COLOR);
                        xs = xe;
                        ys = ye;
                }
                np -= nh;
        }
}

/* again, do we need 'channels'? */
static void _draw_sample_data_16(struct vgamem_overlay *r,
         signed short *data, unsigned long length, int channels, int fakemono)
{
        unsigned long pos;
        int level, xs, ys, xe, ye, step;
        int nh, cc, np;

        nh = (r->height / channels);
        np = r->height - (nh / 2);

        length /= channels;

        for (cc = 0; cc < channels; cc++) {
                level = (data[cc] * nh) / (SHRT_MAX - SHRT_MIN + 1);
                xs = 0;
                ys = (np - 1) - level;
                if (ys < 0) ys = 0;
                if (ys >= r->height) ys = r->height;
                step = MAX(1, (length / r->width) >> 8);
                for (pos = channels+cc; (pos+cc) < length; pos += step) {
                        level = (data[(pos*channels*fakemono)+cc] * nh)
                                        / (SHRT_MAX - SHRT_MIN + 1);
                        xe = pos * r->width / length;
                        ye = (np - 1) - level;
                        if (xe < 0) xe = 0;
                        if (xe >= r->width) xe = (r->width-1);
                        if (ye < 0) ye = 0;
                        if (ye >= r->height) ye = (r->height-1);
                        vgamem_ovl_drawline(r, xs, ys, xe, ye,
                                        SAMPLE_DATA_COLOR);
                        xs = xe;
                        ys = ye;
                }
                np -= nh;
        }
}

/* --------------------------------------------------------------------- */
/* these functions assume the screen is locked! */

/* loop drawing */
static void _draw_sample_loop(struct vgamem_overlay *r, song_sample * sample)
{
        int loopstart, loopend, y;
        int c = ((status.flags & CLASSIC_MODE) ? SAMPLE_DATA_COLOR : SAMPLE_LOOP_COLOR);

        if (!(sample->flags & SAMP_LOOP))
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

static void _draw_sample_susloop(struct vgamem_overlay *r, song_sample * sample)
{
        int loopstart, loopend, y;
        int c = ((status.flags & CLASSIC_MODE) ? SAMPLE_DATA_COLOR : SAMPLE_LOOP_COLOR);

        if (!(sample->flags & SAMP_SUSLOOP))
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
static void _draw_sample_play_marks(struct vgamem_overlay *r, song_sample * sample)
{
        int n, x, y;
        int c;
        song_mix_channel *channel;
        unsigned int *channel_list;

        if (song_get_mode() == MODE_STOPPED)
                return;

        song_lock_audio();

        n = song_get_mix_state(&channel_list);
        while (n--) {
                channel = song_get_mix_channel(channel_list[n]);
                if (channel->sample_data != sample->data)
                        continue;
                if (!channel->final_volume) continue;
                c = (channel->flags & (CHN_KEYOFF | CHN_NOTEFADE)) ? SAMPLE_BGMARK_COLOR : SAMPLE_MARK_COLOR;
                x = channel->sample_pos * (r->width - 1) / sample->length;
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

/* use sample #0 for the sample library
what was n for? can we get rid of it? */
void draw_sample_data(struct vgamem_overlay *r, song_sample *sample, UNUSED int n)
{
        vgamem_ovl_clear(r, 0);

        if(sample->flags & SAMP_ADLIB)
        {
            vgamem_ovl_clear(r, 2);
            vgamem_ovl_apply(r);
                char Buf1[32], Buf2[32];

                int y1 = r->y1, y2 = y1+3;

                draw_box(59,y1, 77,y2, BOX_THICK | BOX_INNER | BOX_INSET); // data
                draw_box(54,y1, 58,y2, BOX_THIN | BOX_INNER | BOX_OUTSET); // button
                draw_text_len("Mod", 3, 55,y1+1, 0,2);
                draw_text_len("Car", 3, 55,y1+2, 0,2);

                sprintf(Buf1, "%02X %02X %02X %02X %02X %02X", // length:6*3-1=17
                        sample->AdlibBytes[0],
                        sample->AdlibBytes[2],
                        sample->AdlibBytes[4],
                        sample->AdlibBytes[6],
                        sample->AdlibBytes[8],
                        sample->AdlibBytes[10]);
                sprintf(Buf2, "%02X %02X %02X %02X %02X",      // length: 5*3-1=14
                        sample->AdlibBytes[1],
                        sample->AdlibBytes[3],
                        sample->AdlibBytes[5],
                        sample->AdlibBytes[7],
                        sample->AdlibBytes[9]);
                draw_text_len(Buf1, 17, 60,y1+1, 2,0);
                draw_text_len(Buf2, 17, 60,y1+2, 2,0);
                return;
        }

        if (!sample->length) {
                vgamem_ovl_apply(r);
                return;
        }

        /* do the actual drawing */
        if (sample->flags & SAMP_16_BIT)
                _draw_sample_data_16(r, (signed short *) sample->data,
                                             sample->length * (sample->flags & SAMP_STEREO ? 2 : 1),
                                sample->flags & SAMP_STEREO ? 2 : 1, 1);
        else
                _draw_sample_data_8(r, sample->data,
                                             sample->length * (sample->flags & SAMP_STEREO ? 2 : 1),
                                sample->flags & SAMP_STEREO ? 2 : 1, 1);

        if ((status.flags & CLASSIC_MODE) == 0)
                _draw_sample_play_marks(r, sample);
        _draw_sample_loop(r, sample);
        _draw_sample_susloop(r, sample);
        vgamem_ovl_apply(r);
}

void draw_sample_data_rect_16(struct vgamem_overlay *r, signed short *data, int length, unsigned int channels, int fakemono)
{
        vgamem_ovl_clear(r, 0);
        _draw_sample_data_16(r, data, length, channels, fakemono);
        vgamem_ovl_apply(r);
}

void draw_sample_data_rect_8(struct vgamem_overlay *r, signed char *data, int length, unsigned int channels, int fakemono)
{
        vgamem_ovl_clear(r, 0);
        _draw_sample_data_8(r, data, length, channels, fakemono);
        vgamem_ovl_apply(r);
}

