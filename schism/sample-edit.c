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
#include "util.h"
#include "song.h"
#include "sample-edit.h"

#include "player/cmixer.h"

/* --------------------------------------------------------------------- */
/* helper functions */

static void _minmax_8(signed char *data, unsigned long length, signed char *min, signed char *max)
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

static void _minmax_16(signed short *data, unsigned long length, signed short *min, signed short *max)
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

static void _sign_convert_8(signed char *data, unsigned long length)
{
	unsigned long pos = length;

	while (pos) {
		pos--;
		data[pos] += 128;
	}
}

static void _sign_convert_16(signed short *data, unsigned long length)
{
	unsigned long pos = length;

	while (pos) {
		pos--;
		data[pos] += 32768;
	}
}

void sample_sign_convert(song_sample_t * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_16BIT)
		_sign_convert_16((signed short *) sample->data,
			sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	else
		_sign_convert_8(sample->data, sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	song_unlock_audio();
}

/* --------------------------------------------------------------------- */
/* from the back to the front */

static void _reverse_8(signed char *data, unsigned long length)
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

static void _reverse_16(signed short *data, unsigned long length)
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
static void _reverse_32(signed int *data, unsigned long length)
{
	signed int tmp;
	unsigned long lpos = 0, rpos = length - 1;

	while (lpos < rpos) {
		tmp = data[lpos];
		data[lpos] = data[rpos];
		data[rpos] = tmp;
		lpos++;
		rpos--;
	}
}

void sample_reverse(song_sample_t * sample)
{
	unsigned long tmp;

	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;

	if (sample->flags & CHN_STEREO) {
		if (sample->flags & CHN_16BIT)
			_reverse_32((signed int *)sample->data, sample->length);
		else
			_reverse_16((signed short *) sample->data, sample->length);
	} else {
		if (sample->flags & CHN_16BIT)
			_reverse_16((signed short *) sample->data, sample->length);
		else
			_reverse_8(sample->data, sample->length);
	}

	tmp = sample->length - sample->loop_start;
	sample->loop_start = sample->length - sample->loop_end;
	sample->loop_end = tmp;

	tmp = sample->length - sample->sustain_start;
	sample->sustain_start = sample->length - sample->sustain_end;
	sample->sustain_end = tmp;

	song_unlock_audio();
}

/* --------------------------------------------------------------------- */

/* if convert_data is nonzero, the sample data is modified (so it sounds
 * the same); otherwise, the sample length is changed and the data is
 * left untouched. */

static void _quality_convert_8to16(signed char *idata, signed short *odata, unsigned long length)
{
	unsigned long pos = length;

	while (pos) {
		pos--;
		odata[pos] = idata[pos] << 8;
	}
}

static void _quality_convert_16to8(signed short *idata, signed char *odata, unsigned long length)
{
	unsigned long pos = length;

	while (pos) {
		pos--;
		odata[pos] = idata[pos] >> 8;
	}
}

void sample_toggle_quality(song_sample_t * sample, int convert_data)
{
	signed char *odata;

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
			_quality_convert_8to16(sample->data, (signed short *) odata,
				sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
		} else {
			_quality_convert_16to8((signed short *) sample->data, odata,
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
	song_unlock_audio();
}

/* --------------------------------------------------------------------- */
/* centralise (correct dc offset) */

static void _centralise_8(signed char *data, unsigned long length)
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

static void _centralise_16(signed short *data, unsigned long length)
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

void sample_centralise(song_sample_t * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_16BIT)
		_centralise_16((signed short *) sample->data,
			sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	else
		_centralise_8(sample->data, sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	song_unlock_audio();
}

/* --------------------------------------------------------------------- */
/* downmix stereo to mono */

static void _downmix_8(signed char *data, unsigned long length)
{
	unsigned long i, j;
	for (i = j = 0; j < length; j++, i += 2)
		data[j] = (data[i] + data[i + 1]) / 2;
}

static void _downmix_16(signed short *data, unsigned long length)
{
	unsigned long i, j;
	for (i = j = 0; j < length; j++, i += 2)
		data[j] = (data[i] + data[i + 1]) / 2;
}

void sample_downmix(song_sample_t *sample)
{
	if (!(sample->flags & CHN_STEREO))
		return; /* what are we doing here with a mono sample? */
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_16BIT)
		_downmix_16((signed short *) sample->data, sample->length);
	else
		_downmix_8(sample->data, sample->length);
	sample->flags &= ~CHN_STEREO;
	song_unlock_audio();
}

/* --------------------------------------------------------------------- */
/* amplify (or attenuate) */

static void _amplify_8(signed char *data, unsigned long length, int percent)
{
	unsigned long pos = length;
	int b;

	while (pos) {
		pos--;
		b = data[pos] * percent / 100;
		data[pos] = CLAMP(b, -128, 127);
	}
}

static void _amplify_16(signed short *data, unsigned long length, int percent)
{
	unsigned long pos = length;
	int b;

	while (pos) {
		pos--;
		b = data[pos] * percent / 100;
		data[pos] = CLAMP(b, -32768, 32767);
	}
}

void sample_amplify(song_sample_t * sample, int percent)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_16BIT)
		_amplify_16((signed short *) sample->data,
			sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1), percent);
	else
		_amplify_8(sample->data, sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1), percent);
	song_unlock_audio();
}

static int _get_amplify_8(signed char *data, unsigned long length)
{
	signed char min, max;
	_minmax_8(data, length, &min, &max);
	max = MAX(max, -min);
	return max ? 128 * 100 / max : 100;
}

static int _get_amplify_16(signed short *data, unsigned long length)
{
	signed short min, max;
	_minmax_16(data, length, &min, &max);
	max = MAX(max, -min);
	return max ? 32768 * 100 / max : 100;
}

int sample_get_amplify_amount(song_sample_t *sample)
{
	int percent;

	if (sample->flags & CHN_16BIT)
		percent = _get_amplify_16((signed short *) sample->data,
			sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	else
		percent = _get_amplify_8(sample->data,
			sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));

	if (percent < 100) percent = 100;
	return percent;
}

/* --------------------------------------------------------------------- */
/* useful for importing delta-encoded raw data */

static void _delta_decode_8(signed char *data, unsigned long length)
{
	unsigned long pos;
	signed char o = 0, n;

	for (pos = 1; pos < length; pos++) {
		n = data[pos] + o;
		data[pos] = n;
		o = n;
	}
}

static void _delta_decode_16(signed short *data, unsigned long length)
{
	unsigned long pos;
	signed short o = 0, n;

	for (pos = 1; pos < length; pos++) {
		n = data[pos] + o;
		data[pos] = n;
		o = n;
	}
}

void sample_delta_decode(song_sample_t * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_16BIT)
		_delta_decode_16((signed short *) sample->data,
			sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	else
		_delta_decode_8(sample->data, sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	song_unlock_audio();
}

/* --------------------------------------------------------------------- */
/* surround flipping (probably useless with the S91 effect, but why not) */

static void _invert_8(signed char *data, unsigned long length)
{
	unsigned long pos = length;

	while (pos) {
		pos--;
		data[pos] = ~data[pos];
	}
}

static void _invert_16(signed short *data, unsigned long length)
{
	unsigned long pos = length;

	while (pos) {
		pos--;
		data[pos] = ~data[pos];
	}
}


static void _resize_16(signed short *dst, unsigned long newlen,
		signed short *src, unsigned long oldlen, unsigned int is_stereo)
{
	unsigned int i;
	double factor = (double)oldlen / (double)newlen;
	if (is_stereo) for (i = 0; i < newlen; i++)
	{
		unsigned int pos = 2*(unsigned int)((double)i * factor);
		dst[2*i] = src[pos];
		dst[2*i+1] = src[pos+1];
	}
	else for (i = 0; i < newlen; i++)
	{
		dst[i] = src[(unsigned int)((double)i * factor)];
	}
}
static void _resize_8(signed char *dst, unsigned long newlen,
		signed char *src, unsigned long oldlen, unsigned int is_stereo)
{
	unsigned int i;
	double factor = (double)oldlen / (double)newlen;
	if (is_stereo) {
		for (i = 0; i < newlen; i++) {
			unsigned int pos = 2*(unsigned int)((double)i * factor);
			dst[2*i] = src[pos];
			dst[2*i+1] = src[pos+1];
		}
	} else {
		for (i = 0; i < newlen; i++) {
			dst[i] = src[(unsigned int)((double)i * factor)];
		}
	}
}
static void _resize_8aa(signed char *dst, unsigned long newlen,
		signed char *src, unsigned long oldlen, unsigned int is_stereo)
{
	if (is_stereo)
		ResampleStereo8BitFirFilter(src, dst, oldlen, newlen);
	else
		ResampleMono8BitFirFilter(src, dst, oldlen, newlen);
}
static void _resize_16aa(signed short *dst, unsigned long newlen,
		signed short *src, unsigned long oldlen, unsigned int is_stereo)
{
	if (is_stereo)
		ResampleStereo16BitFirFilter(src, dst, oldlen, newlen);
	else
		ResampleMono16BitFirFilter(src, dst, oldlen, newlen);
}



void sample_resize(song_sample_t * sample, unsigned long newlen, int aa)
{
	int bps;
	signed char *d, *z;
	unsigned long oldlen;

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

	sample->c5speed = (unsigned long)((((double)newlen) * ((double)sample->c5speed))
			/ ((double)sample->length));

	/* scale loop points */
	sample->loop_start = (unsigned long)((((double)newlen) * ((double)sample->loop_start))
			/ ((double)sample->length));
	sample->loop_end = (unsigned long)((((double)newlen) * ((double)sample->loop_end))
			/ ((double)sample->length));
	sample->sustain_start = (unsigned long)((((double)newlen) * ((double)sample->sustain_start))
			/ ((double)sample->length));
	sample->sustain_end = (unsigned long)((((double)newlen) * ((double)sample->sustain_end))
			/ ((double)sample->length));

	oldlen = sample->length;
	sample->length = newlen;

	if (sample->flags & CHN_16BIT) {
		if (aa) {
			_resize_16aa((signed short *) d, newlen, (short *) sample->data, oldlen, sample->flags & CHN_STEREO);
		} else {
			_resize_16((signed short *) d, newlen, (short *) sample->data, oldlen, sample->flags & CHN_STEREO);
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
	song_unlock_audio();
}

void sample_invert(song_sample_t * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_16BIT)
		_invert_16((signed short *) sample->data,
			sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	else
		_invert_8(sample->data, sample->length * ((sample->flags & CHN_STEREO) ? 2 : 1));
	song_unlock_audio();
}

static void _mono_lr16(signed short *data, unsigned long length, int shift)
{
	unsigned long i=1, j;
	if (shift) { i=0; }
	for (j = 0; j < length; j++, i += 2)
		data[j] = data[i];
}
static void _mono_lr8(signed char *data, unsigned long length, int shift)
{
	unsigned long i=1, j;
	if (shift) { i=0; }
	for (j = 0; j < length; j++, i += 2)
		data[j] = data[i];
}
void sample_mono_left(song_sample_t * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_STEREO) {
		if (sample->flags & CHN_16BIT)
			_mono_lr16((signed short *)sample->data, sample->length, 1);
		else
			_mono_lr8((signed char *)sample->data, sample->length, 1);
		sample->flags &= ~CHN_STEREO;
	}
	song_unlock_audio();
}
void sample_mono_right(song_sample_t * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & CHN_STEREO) {
		if (sample->flags & CHN_16BIT)
			_mono_lr16((signed short *)sample->data, sample->length, 0);
		else
			_mono_lr8((signed char *)sample->data, sample->length, 0);
		sample->flags &= ~CHN_STEREO;
	}
	song_unlock_audio();
}
