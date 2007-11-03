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
#include "fmt.h"

#include "diskwriter.h"

/* FIXME - this shouldn't use modplug's wave-file reader, but it happens
   to be easier than testing various ADPCM junk...
*/
#include "mplink.h"

int fmt_wav_load_sample(const byte *data, size_t length, song_sample *smp,
			UNUSED char *title)
{
	static CSoundFile qq;
	unsigned char *pl, *pr, *oo;
	int mm, nm;
	unsigned long i;

	if (!qq.ReadWav(data, length)) return false;
	if (qq.m_nSamples != 1 && qq.m_nSamples != 2) {
		// can't load multichannel wav...
		return false;
	}
	if (qq.m_nSamples == 2) {
		/* err... */
		if (qq.Ins[1].nLength != qq.Ins[2].nLength) return false;
	}
	memcpy(smp, qq.Ins+1, sizeof(MODINSTRUMENT));
	/* delete panning */
	smp->panning = 0;
	smp->flags &= ~SAMP_PANNING;

	mm = 1;
	if (qq.Ins[1].uFlags & CHN_16BIT) mm++;
	if (qq.m_nSamples == 2) {
		nm = 1;
		if (qq.Ins[2].uFlags & CHN_16BIT) nm++;
		if (nm != nm) return false;
		mm += nm;
	}
	smp->data = song_sample_allocate(smp->length*mm);
	if (!smp->data) return false;
	if (qq.m_nSamples == 2) {
		smp->flags |= CHN_STEREO;
		/* okay, we need to weave in the stereoness */
		pl = (unsigned char *) qq.Ins[1].pSample;
		pr = (unsigned char *) qq.Ins[2].pSample;
		oo = (unsigned char *) smp->data;
		if (mm == 4) {
			for (i = 0; i < smp->length; i++) {
				*oo = *pl; oo++; pl++;
				*oo = *pl; oo++; pl++;
				*oo = *pr; oo++; pr++;
				*oo = *pr; oo++; pr++;
			}
		} else {
			for (i = 0; i < smp->length; i++) {
				*oo = *pl; oo++; pl++;
				*oo = *pr; oo++; pr++;
			}
		}
	} else {
		memcpy(smp->data, qq.Ins[1].pSample, smp->length*mm);
	}

	return true;
}

int fmt_wav_read_info(dmoz_file_t *file, const byte *data, size_t length)
{
	if (length > 12 && *data == 'R'
	&& data[1] == 'I'
	&& data[2] == 'F'
	&& data[3] == 'F'
	&& data[8] == 'W'
	&& data[9] == 'A'
	&& data[10] == 'V'
	&& data[11] == 'E') {
		// good possibility
		DWORD q;
		memcpy(&q, data+4, 4);
		q = bswapLE32(q);
		if (q == (length-8)) {
			file->description = "IBM/Microsoft RIFF Audio";
			file->type = TYPE_SAMPLE_PLAIN;
			file->smp_flags = 0;
			if (data[22] >= 2) {
				file->smp_flags |= SAMP_STEREO;
			}
			file->smp_speed = (unsigned int)data[24]
				| (((unsigned int)data[25]) << 8)
				| (((unsigned int)data[26]) << 16)
				| (((unsigned int)data[27]) << 24);
			if (data[32] != 1 || data[33] != 0) {
				file->smp_flags |= SAMP_16_BIT;
			}
			/* estimate */
			file->smp_length = length - 44;
			if (file->smp_flags & SAMP_16_BIT) {
				file->smp_length /= 2;
			}
			if (file->smp_flags & SAMP_STEREO) {
				file->smp_length /= 2;
			}
			file->smp_filename = file->base;
			return true;
		}
	}
	return false;
}
/* ... wavewriter :) */
static void _wavout_header(diskwriter_driver_t *x)
{
	unsigned char le[4];
	unsigned int sps;
	x->o(x, (const unsigned char *)"RIFF\1\1\1\1WAVEfmt \x10\0\0\0\1\0", 22);
	le[0] = x->channels;
	le[1] = le[2] = le[3] = 0;
	x->o(x, le, 2);
	le[0] = x->rate & 255;
	le[1] = (x->rate >> 8) & 255;
	le[2] = (x->rate >> 16) & 255;
	le[3] = (x->rate >> 24) & 255;
	x->o(x, le, 4);
	sps = x->rate * x->channels * (x->bits / 8);
	le[0] = sps & 255;
	le[1] = (sps >> 8) & 255;
	le[2] = (sps >> 16) & 255;
	le[3] = (sps >> 24) & 255;
	x->o(x, le, 4);
	sps = (x->bits / 8);
	le[0] = sps & 255;
	le[1] = (sps >> 8) & 255;
	le[2] = le[3] = 0;
	x->o(x, le, 2);
	le[0] = x->bits;
	le[1] = 0;
	x->o(x, le, 2);
	x->o(x, (const unsigned char *)"data\1\1\1\1", 8);
}
static void _wavout_tail(diskwriter_driver_t *x)
{
	off_t tt;
	unsigned char le[4];

	tt = x->pos;
	x->l(x, 4);
	tt -= 8;
	le[0] = tt & 255;
	le[1] = (tt >> 8) & 255;
	le[2] = (tt >> 16) & 255;
	le[3] = (tt >> 24) & 255;
	x->o(x, le, 4);
	x->l(x, 40);
	tt -= 36;
	le[0] = tt & 255;
	le[1] = (tt >> 8) & 255;
	le[2] = (tt >> 16) & 255;
	le[3] = (tt >> 24) & 255;
	x->o(x, le, 4);
}
static void _wavout_data(diskwriter_driver_t *x,unsigned char *buf,
				unsigned int len)
{
	x->o(x, buf, len);
}

extern "C" {
diskwriter_driver_t wavewriter = {
"WAV","wav",1,
_wavout_header,
_wavout_data,
NULL, /* no midi data */
_wavout_tail,
NULL,NULL,NULL,
NULL, /* setup page */
NULL,
44100,16,2,1,
0 /* pos */
};
};
