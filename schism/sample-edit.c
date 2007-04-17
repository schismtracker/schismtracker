/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
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
#include "util.h"
#include "song.h"
#include "sample-edit.h"

#include "sdlmain.h"

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
                data[pos] ^= 128;
        }
}

static void _sign_convert_16(signed short *data, unsigned long length)
{
        unsigned long pos = length;

        while (pos) {
                pos--;
                data[pos] ^= 32768;
        }
}

void sample_sign_convert(song_sample * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
        if (sample->flags & SAMP_16_BIT)
                _sign_convert_16((signed short *) sample->data, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
        else
                _sign_convert_8(sample->data, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
	song_unlock_audio();
}

/* --------------------------------------------------------------------- */
/* i don't think this is being done correctly :/ */

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

static void _rstereo_8(signed char *data, unsigned long length)
{
	unsigned long i;
	signed char tmp;

	length <<= 1;
	for (i = 0; i < length; i += 2) {
		tmp = data[i];
		data[i] = data[i+1];
		data[i+1] = tmp;
	}
}
static void _rstereo_16(signed short *data, unsigned long length)
{
	unsigned long i;
	signed short tmp;

	length <<= 1;
	for (i = 0; i < length; i += 2) {
		tmp = data[i];
		data[i] = data[i+1];
		data[i+1] = tmp;
	}
}
void sample_reverse(song_sample * sample)
{
        unsigned long tmp;

	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
        if (sample->flags & SAMP_16_BIT)
                _reverse_16((signed short *) sample->data, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
        else
                _reverse_8(sample->data, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));

	if (sample->flags & SAMP_STEREO) {
        	if (sample->flags & SAMP_16_BIT)
			_rstereo_16((signed short *)sample->data, sample->length);
		else
			_rstereo_8(sample->data, sample->length);
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
 * left untouched.
 * this is irrelevant, as i haven't gotten to writing the convert stuff
 * yet. (not that it's hard, i just haven't gotten to it.) */

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

void sample_toggle_quality(song_sample * sample, int convert_data)
{
	signed char *odata;
		
	song_lock_audio();
	sample->flags ^= SAMP_16_BIT;
	
	status.flags |= SONG_NEEDS_SAVE;
        if (convert_data) {
		if (sample->flags & SAMP_16_BIT) {
			odata = song_sample_allocate(2 * sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
			_quality_convert_8to16(sample->data, (signed short *) odata, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
		} else {
			odata = song_sample_allocate(sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
			_quality_convert_16to8((signed short *) sample->data, odata, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
		}
		song_sample_free(sample->data);
		sample->data = odata;
        } else {
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

void sample_centralise(song_sample * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
        if (sample->flags & SAMP_16_BIT)
                _centralise_16((signed short *) sample->data, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
        else
                _centralise_8(sample->data, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
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

void sample_amplify(song_sample * sample, int percent)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
        if (sample->flags & SAMP_16_BIT)
                _amplify_16((signed short *) sample->data, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1), percent);
        else
                _amplify_8(sample->data, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1), percent);
	song_unlock_audio();
}

static int _get_amplify_8(signed char *data, unsigned long length)
{
	signed char min, max;
	_minmax_8(data, length, &min, &max);
	if (min == 0 && max == 0)
		return 100;
	return 128 * 100 / MAX(max, -min);
}

static int _get_amplify_16(signed short *data, unsigned long length)
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
		percent = _get_amplify_16((signed short *) sample->data, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
	else
		percent = _get_amplify_8(sample->data, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
	
	if (percent < 100) {
		/* shouldn't happen */
		printf("sample_get_amplify_amount: percent < 100. why?\n");
		percent = 100;
	}
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

void sample_delta_decode(song_sample * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
        if (sample->flags & SAMP_16_BIT)
                _delta_decode_16((signed short *) sample->data, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
        else
                _delta_decode_8(sample->data, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
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

/* why is newlen unsigned long, but oldlen is unsigned int? just curious. */
static void _resize_16(signed short *dst, unsigned long newlen,
		signed short *src, unsigned int oldlen)
{
	unsigned int i;
	for (i = 0; i < newlen; i++)
		dst[i] = src[(unsigned int)((double)i * ((double)oldlen / (double)newlen))];
}
static void _resize_8(signed char *dst, unsigned long newlen,
		signed char *src, unsigned int oldlen)
{
	unsigned int i;
	for (i = 0; i < newlen; i++)
		dst[i] = src[(unsigned int)((double)i * ((double)oldlen / (double)newlen))];
}
static void _resize_8aa(signed char *dst, unsigned long newlen,
		signed char *src, unsigned int oldlen)
{
	int avg_acc = 0;
	int avg_count = 0;
	unsigned long i, j;
	/* what is cp? should it be unsigned? -storlek */
	int cp;
	int old_pos = -1;
	for (i = 0; i < oldlen; i++) {
		cp = (int)((double)i * ((double)newlen) / (double)oldlen);
		if (cp < 0) cp = 0;
		if (cp > old_pos) {
			if (old_pos >= 0 && cp >= 0 && (unsigned long) cp < newlen) {
				for (j = 0; j < (unsigned long) (cp - old_pos); j++) {
					dst[old_pos+j] = avg_acc/avg_count;
				}
			}
			avg_count = 0;
			avg_acc = 0;
			old_pos = cp;
		}
		avg_count ++;
		avg_acc += src[i];
	}
	for (j = 0; j < (newlen-old_pos); j++) {
		dst[old_pos+j] = avg_acc/avg_count;
	}
}
static void _resize_16aa(signed short *dst, unsigned long newlen,
		signed short *src, unsigned int oldlen)
{
	int avg_acc = 0;
	int avg_count = 0;
	int i, j, cp;
	int old_pos = -1;
	for (i = 0; (unsigned int) i < oldlen; i++) {
		/* cast-o-matic! */
		cp = (int)((double)i * ((double)newlen) / (double)oldlen);
		if (cp < 0) cp = 0;
		if (cp > old_pos) {
			if (old_pos >= 0 && cp >= 0 && (unsigned long) cp < newlen) {
				for (j = 0; j < (cp-old_pos); j++) {
					dst[old_pos+j] = avg_acc/avg_count;
				}
			}
			avg_count = 0;
			avg_acc = 0;
			old_pos = cp;
		}
		avg_count ++;
		avg_acc += src[i];
	}
}



void sample_resize(song_sample * sample, unsigned long newlen, int aa)
{
	int bps;
	unsigned char *d, *z;
	unsigned long oldlen;

	if (!newlen) return;
	if (!sample->data || !sample->length) return;

	/* resizing samples while they're playing keeps crashing things.
	so here's my "fix": stop the song. --plusminus */
	song_stop();
	song_lock_audio();
	bps = (((sample->flags & SAMP_STEREO) ? 2 : 1)
		* ((sample->flags & SAMP_16_BIT) ? 2 : 1));

	status.flags |= SONG_NEEDS_SAVE;

	d = (unsigned char *) song_sample_allocate(newlen*bps);
	z = (unsigned char *) sample->data;

	sample->speed = (unsigned long)((((double)newlen) * ((double)sample->speed))
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
	if (sample->flags & SAMP_STEREO) { newlen *= 2; oldlen *= 2; }

	if (sample->flags & SAMP_16_BIT) {
		if (aa) {
			_resize_16aa((signed short *) d, newlen, (short *)sample->data, oldlen);
		} else {
			_resize_16((signed short *) d, newlen, (short *)sample->data, oldlen);
		}
	} else {
		if (aa) {
			_resize_8aa((signed char *) d, newlen, sample->data, oldlen);
		} else {
			_resize_8((signed char *) d, newlen, sample->data, oldlen);
		}
	}

	sample->data = (signed char *) d;
	song_sample_free((signed char *) z);
	song_unlock_audio();
}

void sample_invert(song_sample * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
        if (sample->flags & SAMP_16_BIT)
                _invert_16((signed short *) sample->data, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
        else
                _invert_8(sample->data, sample->length * ((sample->flags & SAMP_STEREO) ? 2 : 1));
	song_unlock_audio();
}

static void _mono_lr16(signed char *data, unsigned long length, int shift)
{
	unsigned long i, j;
	if (shift) memmove(data, data+shift+shift, (length-shift)-shift);
	for (j = 0, i = 1; j < length; j++, i += 2)
		data[j] = data[i];
}
static void _mono_lr8(signed char *data, unsigned long length, int shift)
{
	unsigned long i, j;
	if (shift) memmove(data, data+shift, length-shift);
	for (j = 0, i = 1; j < length; j++, i += 2)
		data[j] = data[i];
}
void sample_mono_left(song_sample * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & SAMP_STEREO) {
		if (sample->flags & SAMP_16_BIT)
			_mono_lr16((signed char *)sample->data, sample->length*2, 1);
		else
			_mono_lr8((signed char *)sample->data, sample->length*2, 1);
		sample->flags &= ~SAMP_STEREO;
	}
	song_unlock_audio();
}
void sample_mono_right(song_sample * sample)
{
	song_lock_audio();
	status.flags |= SONG_NEEDS_SAVE;
	if (sample->flags & SAMP_STEREO) {
		if (sample->flags & SAMP_16_BIT)
			_mono_lr16((signed char *)sample->data, sample->length*2, 0);
		else
			_mono_lr8((signed char *)sample->data, sample->length*2, 0);
		sample->flags &= ~SAMP_STEREO;
	}
	song_unlock_audio();
}
