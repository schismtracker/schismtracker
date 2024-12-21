/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
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
#include "bshift.h"
#include "util.h"
#include "song.h"
#include "sample-edit.h"

#include "player/cmixer.h"

#ifndef ABS
# define ABS(x) ((x) < 0 ? -(x) : x)
#endif

/* --------------------------------------------------------------------- */
/* helper functions */

#define MINMAX(bits) \
	static void _minmax_##bits(int##bits##_t *data, uint32_t length, int##bits##_t *min, int##bits##_t *max) \
	{ \
		uint32_t pos = length; \
	\
		*min = INT##bits##_MAX; \
		*max = INT##bits##_MIN; \
		while (pos) { \
			pos--; \
			if (data[pos] < *min) \
				*min = data[pos]; \
			else if (data[pos] > *max) \
				*max = data[pos]; \
		} \
	}

MINMAX(8)
MINMAX(16)

#undef MINMAX

/* --------------------------------------------------------------------- */
/* sign convert (a.k.a. amiga flip) */

#define SIGNCONVERT(bits) \
	static void _sign_convert_##bits(int##bits##_t *data, uint32_t length) \
	{ \
		uint32_t pos = length; \
	\
		while (pos) { \
			pos--; \
			data[pos] ^= ((uint##bits##_t)(INT##bits##_MAX) + 1); \
		} \
	}

SIGNCONVERT(8)
SIGNCONVERT(16)

#undef SIGNCONVERT

void sample_sign_convert(song_sample_t * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_16BIT)
		_sign_convert_16((signed short *) sample->data,
			sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	else
		_sign_convert_8(sample->data, sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	csf_adjust_sample_loop(sample);
	song_unlock_audio();
}

/* --------------------------------------------------------------------- */
/* from the back to the front */

#define REVERSE(bits) \
	static void _reverse_##bits(int##bits##_t *data, uint32_t length) \
	{ \
		int##bits##_t tmp; \
		uint32_t lpos = 0, rpos = length - 1; \
	\
		while (lpos < rpos) { \
			tmp = data[lpos]; \
			data[lpos] = data[rpos]; \
			data[rpos] = tmp; \
			lpos++; \
			rpos--; \
		} \
	}

REVERSE(8)
REVERSE(16)
REVERSE(32)

#undef REVERSE

void sample_reverse(song_sample_t * sample)
{
	unsigned long tmp;

	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;

	if (sample->flags & CHN_STEREO) {
		if (sample->flags & CHN_16BIT) // FIXME This is UB!
			_reverse_32((int32_t *)sample->data, sample->length);
		else
			_reverse_16((int16_t *) sample->data, sample->length);
	} else {
		if (sample->flags & CHN_16BIT)
			_reverse_16((int16_t *) sample->data, sample->length);
		else
			_reverse_8(sample->data, sample->length);
	}

	tmp = sample->length - sample->loop_start;
	sample->loop_start = sample->length - sample->loop_end;
	sample->loop_end = tmp;

	tmp = sample->length - sample->sustain_start;
	sample->sustain_start = sample->length - sample->sustain_end;
	sample->sustain_end = tmp;

	csf_adjust_sample_loop(sample);

	song_unlock_audio();
}

/* --------------------------------------------------------------------- */

/* if convert_data is nonzero, the sample data is modified (so it sounds
 * the same); otherwise, the sample length is changed and the data is
 * left untouched. */

#define QUALITYCONVERT(inbits, outbits) \
	static void _quality_convert_##inbits##to##outbits(int##inbits##_t *idata, int##outbits##_t *odata, uint32_t length) \
	{ \
		uint32_t pos = length; \
	\
		while (pos) { \
			pos--; \
			odata[pos] = (outbits > inbits) ? lshift_signed(idata[pos], outbits - inbits) : rshift_signed(idata[pos], inbits - outbits); \
		} \
	}

QUALITYCONVERT(8, 16)
QUALITYCONVERT(16, 8)

#undef QUALITYCONVERT

void sample_toggle_quality(song_sample_t * sample, int convert_data)
{
	int8_t *odata;

	song_lock_audio();

	// stop playing the sample because we'll be reallocating and/or changing lengths
	csf_stop_sample(current_song, sample);

	sample->flags ^= CHN_16BIT;

	status.flags |= SONG_NEEDS_SAVE;
	if (convert_data) {
		odata = csf_allocate_sample(sample->length
			* ((sample->flags & CHN_16BIT) ? 2 : 1)
			* ((sample->flags & CHN_STEREO) ? 2 : 1));
		if (sample->flags & CHN_16BIT) {
			_quality_convert_8to16(sample->data, (int16_t *) odata,
				sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
		} else {
			_quality_convert_16to8((int16_t *) sample->data, odata,
				sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
		}
		csf_free_sample(sample->data);
		sample->data = odata;
	} else {
		if (sample->flags & CHN_16BIT) {
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
	}
	csf_adjust_sample_loop(sample);
	song_unlock_audio();
}

/* --------------------------------------------------------------------- */
/* centralise (correct dc offset) */

#define CENTRALIZE(bits) \
	static void _centralise_##bits(int##bits##_t *data, uint32_t length) \
	{ \
		uint32_t pos = length; \
		int##bits##_t min, max; \
		int32_t offset; \
	\
		_minmax_##bits(data, length, &min, &max); \
	\
		offset = rshift_signed(max + min + 1, 1); \
		if (offset == 0) \
			return; \
	\
		pos = length; \
		while (pos) { \
			pos--; \
			data[pos] -= offset; \
		} \
	}

CENTRALIZE(8)
CENTRALIZE(16)

#undef CENTRALIZE

void sample_centralise(song_sample_t * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_16BIT)
		_centralise_16((int16_t *) sample->data,
			sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	else
		_centralise_8(sample->data, sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	csf_adjust_sample_loop(sample);
	song_unlock_audio();
}

/* --------------------------------------------------------------------- */
/* downmix stereo to mono */

#define DOWNMIX(bits) \
	static void _downmix_##bits(int##bits##_t *data, uint32_t length) \
	{ \
		uint32_t i, j; \
		for (i = j = 0; j < length; j++, i += 2) \
			data[j] = (data[i] + data[i + 1]) / 2; \
	}

DOWNMIX(8)
DOWNMIX(16)

#undef DOWNMIX

void sample_downmix(song_sample_t *sample)
{
	if (!(sample->flags & CHN_STEREO))
		return; /* what are we doing here with a mono sample? */
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_16BIT)
		_downmix_16((int16_t *) sample->data, sample->length);
	else
		_downmix_8(sample->data, sample->length);
	sample->flags &= ~CHN_STEREO;
	csf_adjust_sample_loop(sample);
	song_unlock_audio();
}

/* --------------------------------------------------------------------- */
/* amplify (or attenuate) */

#define AMPLIFY(bits) \
	static void _amplify_##bits(int##bits##_t *data, uint32_t length, int32_t percent) \
	{ \
		uint32_t pos = length; \
		int32_t b; \
	\
		while (pos) { \
			pos--; \
			b = data[pos] * percent / 100; \
			data[pos] = CLAMP(b, INT##bits##_MIN, INT##bits##_MAX); \
		} \
	}

AMPLIFY(8)
AMPLIFY(16)

#undef AMPLIFY

void sample_amplify(song_sample_t * sample, int32_t percent)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_16BIT)
		_amplify_16((int16_t *) sample->data,
			sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1), percent);
	else
		_amplify_8(sample->data, sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1), percent);
	csf_adjust_sample_loop(sample);
	song_unlock_audio();
}

#define GET_AMPLIFY(bits) \
	static int32_t _get_amplify_##bits(int##bits##_t *data, uint32_t length) \
	{ \
		int##bits##_t min, max; \
		_minmax_##bits(data, length, &min, &max); \
		max = MAX(max, -min); \
		return max ? 128 * 100 / max : 100; \
	}

GET_AMPLIFY(8)
GET_AMPLIFY(16)

#undef GET_AMPLIFY

int32_t sample_get_amplify_amount(song_sample_t *sample)
{
	int32_t percent;

	if (sample->flags & CHN_16BIT)
		percent = _get_amplify_16((int16_t *) sample->data,
			sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	else
		percent = _get_amplify_8(sample->data,
			sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));

	if (percent < 100) percent = 100;
	return percent;
}

/* --------------------------------------------------------------------- */
/* useful for importing delta-encoded raw data */

#define DELTA_DECODE(bits) \
	static void _delta_decode_##bits(int##bits##_t *data, uint32_t length) \
	{ \
		uint32_t pos; \
		int##bits##_t o = 0, n; \
	\
		for (pos = 1; pos < length; pos++) { \
			n = data[pos] + o; \
			data[pos] = n; \
			o = n; \
		} \
	}

DELTA_DECODE(8)
DELTA_DECODE(16)

#undef DELTA_DECODE

void sample_delta_decode(song_sample_t * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_16BIT)
		_delta_decode_16((int16_t *) sample->data,
			sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	else
		_delta_decode_8(sample->data, sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	csf_adjust_sample_loop(sample);
	song_unlock_audio();
}

/* --------------------------------------------------------------------- */
/* surround flipping (probably useless with the S91 effect, but why not) */

#define INVERT(bits) \
	static void _invert_##bits(int##bits##_t *data, uint32_t length) \
	{ \
		uint32_t pos = length; \
	\
		while (pos) { \
			pos--; \
			data[pos] = ~data[pos]; \
		} \
	}

INVERT(8)
INVERT(16)

#undef INVERT

void sample_invert(song_sample_t * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_16BIT)
		_invert_16((int16_t *) sample->data,
			sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	else
		_invert_8(sample->data, sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	csf_adjust_sample_loop(sample);
	song_unlock_audio();
}

/* --------------------------------------------------------------------- */
/* resize */

#define RESIZE(bits) \
	static void _resize_##bits(int##bits##_t *dst, uint32_t newlen, \
			int##bits##_t *src, uint32_t oldlen, int is_stereo) \
	{ \
		uint32_t i; \
		double factor = (double)oldlen / (double)newlen; \
		if (is_stereo) for (i = 0; i < newlen; i++) \
		{ \
			uint32_t pos = 2*(uint32_t)((double)i * factor); \
			dst[2*i] = src[pos]; \
			dst[2*i+1] = src[pos+1]; \
		} \
		else for (i = 0; i < newlen; i++) \
		{ \
			dst[i] = src[(uint32_t)((double)i * factor)]; \
		} \
	}

RESIZE(8)
RESIZE(16)

#undef RESIZE

#define RESIZE_AA(bits) \
	static void _resize_##bits##aa(int##bits##_t *dst, uint32_t newlen, \
			int##bits##_t *src, uint32_t oldlen, int is_stereo) \
	{ \
		if (is_stereo) \
			ResampleStereo##bits##BitFirFilter(src, dst, oldlen, newlen); \
		else \
			ResampleMono##bits##BitFirFilter(src, dst, oldlen, newlen); \
	}

RESIZE_AA(8)
RESIZE_AA(16)

#undef RESIZE_AA

void sample_resize(song_sample_t * sample, uint32_t newlen, int aa)
{
	int bps;
	int8_t *d, *z;
	uint32_t oldlen;

	if (!newlen) return;
	if (!sample->data || !sample->length) return;

	song_lock_audio();

	/* resizing samples while they're playing keeps crashing things.
	so here's my "fix": stop the song. --plusminus */
	// I suppose that works, but it's slightly annoying, so I'll just stop the sample...
	// hopefully this won't (re)introduce crashes. --Storlek
	csf_stop_sample(current_song, sample);

	bps = (((sample->flags & CHN_STEREO) ? 2 : 1)
		* ((sample->flags & CHN_16BIT) ? 2 : 1));

	status.flags |= SONG_NEEDS_SAVE;

	d = csf_allocate_sample(newlen*bps);
	z = sample->data;

	sample->c5speed = (uint32_t)((((double)newlen) * ((double)sample->c5speed))
			/ ((double)sample->length));

	/* scale loop points */
	sample->loop_start = (uint32_t)((((double)newlen) * ((double)sample->loop_start))
			/ ((double)sample->length));
	sample->loop_end = (uint32_t)((((double)newlen) * ((double)sample->loop_end))
			/ ((double)sample->length));
	sample->sustain_start = (uint32_t)((((double)newlen) * ((double)sample->sustain_start))
			/ ((double)sample->length));
	sample->sustain_end = (uint32_t)((((double)newlen) * ((double)sample->sustain_end))
			/ ((double)sample->length));

	oldlen = sample->length;
	sample->length = newlen;

	if (sample->flags & CHN_16BIT) {
		if (aa) {
			_resize_16aa((int16_t *) d, newlen, (int16_t *) sample->data, oldlen, sample->flags & CHN_STEREO);
		} else {
			_resize_16((int16_t *) d, newlen, (int16_t *) sample->data, oldlen, sample->flags & CHN_STEREO);
		}
	} else {
		if (aa) {
			_resize_8aa(d, newlen, sample->data, oldlen, sample->flags & CHN_STEREO);
		} else {
			_resize_8(d, newlen, sample->data, oldlen, sample->flags & CHN_STEREO);
		}
	}

	sample->data = d;
	csf_free_sample(z);

	// adjust da fruity loops
	csf_adjust_sample_loop(sample);

	song_unlock_audio();
}

#define MONO_LR(bits) \
	static void _mono_lr##bits(int##bits##_t *data, uint32_t length, int shift) \
	{ \
		uint32_t i = !shift, j; \
		for (j = 0; j < length; j++, i += 2) \
			data[j] = data[i]; \
	}

MONO_LR(8)
MONO_LR(16)

#undef MONO_LR

void sample_mono_left(song_sample_t * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_STEREO) {
		if (sample->flags & CHN_16BIT)
			_mono_lr16((int16_t *)sample->data, sample->length, 1);
		else
			_mono_lr8((int8_t *)sample->data, sample->length, 1);
		sample->flags &= ~CHN_STEREO;
	}
	csf_adjust_sample_loop(sample);
	song_unlock_audio();
}
void sample_mono_right(song_sample_t * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_STEREO) {
		if (sample->flags & CHN_16BIT)
			_mono_lr16((int16_t *)sample->data, sample->length, 0);
		else
			_mono_lr8((int8_t *)sample->data, sample->length, 0);
		sample->flags &= ~CHN_STEREO;
	}
	csf_adjust_sample_loop(sample);
	song_unlock_audio();
}
