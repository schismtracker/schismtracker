/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
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

#include "util.h"
#include "song.h"
#include "sample-edit.h"

#include <SDL.h>

/* --------------------------------------------------------------------- */
/* helper functions */

static inline void _minmax_8(signed char *data, unsigned long length,
			     signed char *min, signed char *max)
{
	unsigned long pos = length;
	
	*min = 127;
	*max = -128;
        while (pos) {
                pos--;
                if (data[pos] < *min)
                        *min = data[pos];
                else if (data[pos] > *max)
                        *max = data[pos];
        }
}

static inline void _minmax_16(signed short *data, unsigned long length,
			     signed short *min, signed short *max)
{
	unsigned long pos = length;
	
	*min = 32767;
	*max = -32768;
        while (pos) {
                pos--;
                if (data[pos] < *min)
                        *min = data[pos];
                else if (data[pos] > *max)
                        *max = data[pos];
        }
}

/* --------------------------------------------------------------------- */
/* sign convert (a.k.a. amiga flip) */

static inline void _sign_convert_8(signed char *data, unsigned long length)
{
        unsigned long pos = length;

        while (pos) {
                pos--;
                data[pos] ^= 128;
        }
}

static inline void _sign_convert_16(signed short *data,
                                    unsigned long length)
{
        unsigned long pos = length;

        while (pos) {
                pos--;
                data[pos] ^= 32768;
        }
}

void sample_sign_convert(song_sample * sample)
{
        if (sample->flags & SAMP_16_BIT)
                _sign_convert_16((signed short *) sample->data,
                                 sample->length);
        else
                _sign_convert_8(sample->data, sample->length);
}

/* --------------------------------------------------------------------- */
/* i don't think this is being done correctly :/ */

static inline void _reverse_8(signed char *data, unsigned long length)
{
        signed char tmp;
        unsigned long lpos = 0, rpos = length - 1;

        while (lpos < rpos) {
                tmp = data[lpos];
                data[lpos] = data[rpos];
                data[rpos] = tmp;
                lpos++;
                rpos--;
        }
}

static inline void _reverse_16(signed short *data, unsigned long length)
{
        signed short tmp;
        unsigned long lpos = 0, rpos = length - 1;

        while (lpos < rpos) {
                tmp = data[lpos];
                data[lpos] = data[rpos];
                data[rpos] = tmp;
                lpos++;
                rpos--;
        }
}

void sample_reverse(song_sample * sample)
{
        unsigned long tmp;

        if (sample->flags & SAMP_16_BIT)
                _reverse_16((signed short *) sample->data, sample->length);
        else
                _reverse_8(sample->data, sample->length);

        tmp = sample->length - sample->loop_start;
        sample->loop_start = sample->length - sample->loop_end;
        sample->loop_end = tmp;

        tmp = sample->length - sample->sustain_start;
        sample->sustain_start = sample->length - sample->sustain_end;
        sample->sustain_end = tmp;
}

/* --------------------------------------------------------------------- */

/* if convert_data is nonzero, the sample data is modified (so it sounds
 * the same); otherwise, the sample length is changed and the data is
 * left untouched.
 * this is irrelevant, as i haven't gotten to writing the convert stuff
 * yet. (not that it's hard, i just haven't gotten to it.) */

void sample_toggle_quality(song_sample * sample, int convert_data)
{
        if (convert_data == 0) {
                sample->flags ^= SAMP_16_BIT;

                if (sample->flags & SAMP_16_BIT) {
                        sample->length >>= 1;
                        sample->loop_start >>= 1;
                        sample->loop_end >>= 1;
                        sample->sustain_start >>= 1;
                        sample->sustain_end >>= 1;
                } else {
                        sample->length <<= 1;
                        sample->loop_start <<= 1;
                        sample->loop_end <<= 1;
                        sample->sustain_start <<= 1;
                        sample->sustain_end <<= 1;
                }
        } else {
                printf("arr! convert!\n");
        }
}

/* --------------------------------------------------------------------- */
/* centralise (correct dc offset) */

static inline void _centralise_8(signed char *data, unsigned long length)
{
        unsigned long pos = length;
        signed char min, max;
        int offset;
	
	_minmax_8(data, length, &min, &max);
	
        offset = (max + min + 1) >> 1;
        if (offset == 0)
                return;

        pos = length;
        while (pos) {
                pos--;
                data[pos] -= offset;
        }
}

static inline void _centralise_16(signed short *data, unsigned long length)
{
        unsigned long pos = length;
        signed short min, max;
        int offset;
	
	_minmax_16(data, length, &min, &max);

        while (pos) {
                pos--;
                if (data[pos] < min)
                        min = data[pos];
                else if (data[pos] > max)
                        max = data[pos];
        }

        offset = (max + min + 1) >> 1;
        if (offset == 0)
                return;

        pos = length;
        while (pos) {
                pos--;
                data[pos] -= offset;
        }
}

void sample_centralise(song_sample * sample)
{
        if (sample->flags & SAMP_16_BIT)
                _centralise_16((signed short *) sample->data,
                               sample->length);
        else
                _centralise_8(sample->data, sample->length);
}

/* --------------------------------------------------------------------- */
/* amplify (or attenuate) */

static inline void _amplify_8(signed char *data, unsigned long length,
			      int percent)
{
        unsigned long pos = length;
	int b;
	
        while (pos) {
                pos--;
		b = data[pos] * percent / 100;
                data[pos] = CLAMP(b, -128, 127);
        }
}

static inline void _amplify_16(signed short *data, unsigned long length,
			       int percent)
{
        unsigned long pos = length;
	int b;

        while (pos) {
                pos--;
		b = data[pos] * percent / 100;
                data[pos] = CLAMP(b, -32768, 32767);
        }
}

void sample_amplify(song_sample * sample, int percent)
{
        if (sample->flags & SAMP_16_BIT)
                _amplify_16((signed short *) sample->data, sample->length,
			    percent);
        else
                _amplify_8(sample->data, sample->length, percent);
}

static inline int _get_amplify_8(signed char *data, unsigned long length)
{
	signed char min, max;
	_minmax_8(data, length, &min, &max);
	if (min == 0 && max == 0)
		return 100;
	return 128 * 100 / MAX(max, -min);
}

static inline int _get_amplify_16(signed short *data, unsigned long length)
{
	signed short min, max;
	_minmax_16(data, length, &min, &max);
	if (min == 0 && max == 0)
		return 100;
	return 32768 * 100 / MAX(max, -min);
}

int sample_get_amplify_amount(song_sample *sample)
{
	int percent;
	
	if (sample->flags & SAMP_16_BIT)
		percent = _get_amplify_16((signed short *) sample->data,
				       sample->length);
	else
		percent = _get_amplify_8(sample->data, sample->length);
	
	if (percent < 100) {
		/* shouldn't happen */
		printf("sample_get_amplify_amount: percent < 100. why?\n");
		percent = 100;
	}
	return percent;
}

/* --------------------------------------------------------------------- */
/* useful for importing delta-encoded raw data */

static inline void _delta_decode_8(signed char *data, unsigned long length)
{
        unsigned long pos;
        signed char o = 0, n;

        for (pos = 1; pos < length; pos++) {
                n = data[pos] + o;
                data[pos] = n;
                o = n;
        }
}

static inline void _delta_decode_16(signed short *data,
                                    unsigned long length)
{
        unsigned long pos;
        signed short o = 0, n;

        for (pos = 1; pos < length; pos++) {
                n = data[pos] + o;
                data[pos] = n;
                o = n;
        }
}

void sample_delta_decode(song_sample * sample)
{
        if (sample->flags & SAMP_16_BIT)
                _delta_decode_16((signed short *) sample->data,
                                 sample->length);
        else
                _delta_decode_8(sample->data, sample->length);
}

/* --------------------------------------------------------------------- */
/* surround flipping (probably useless with the S91 effect, but why not) */

static inline void _invert_8(signed char *data, unsigned long length)
{
        unsigned long pos = length;

        while (pos) {
                pos--;
                data[pos] = ~data[pos];
        }
}

static inline void _invert_16(signed short *data, unsigned long length)
{
        unsigned long pos = length;

        while (pos) {
                pos--;
                data[pos] = ~data[pos];
        }
}

void sample_invert(song_sample * sample)
{
        if (sample->flags & SAMP_16_BIT)
                _invert_16((signed short *) sample->data, sample->length);
        else
                _invert_8(sample->data, sample->length);
}
