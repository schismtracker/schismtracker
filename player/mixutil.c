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
#include "bshift.h"
#include "util.h"

#include "player/sndfile.h"
#include "player/cmixer.h"

#define OFSDECAYSHIFT 8
#define OFSDECAYMASK  0xFF


void init_mix_buffer(int32_t *buffer, uint32_t samples)
{
    memset(buffer, 0, samples * sizeof(int32_t));
}


void stereo_fill(int32_t *buffer, uint32_t samples, int32_t* profs, int32_t *plofs)
{
	int32_t rofs = *profs;
	int32_t lofs = *plofs;

	if (!rofs && !lofs) {
		init_mix_buffer(buffer, samples * 2);
		return;
	}

    for (uint32_t i = 0; i < samples; i++) {
		int32_t x_r = rshift_signed(rofs + (rshift_signed(-rofs, 31) & OFSDECAYMASK), OFSDECAYSHIFT);
		int32_t x_l = rshift_signed(lofs + (rshift_signed(-lofs, 31) & OFSDECAYMASK), OFSDECAYSHIFT);

		rofs -= x_r;
		lofs -= x_l;
		buffer[i * 2 ]    = x_r;
		buffer[i * 2 + 1] = x_l;
	}

	*profs = rofs;
	*plofs = lofs;
}


void end_channel_ofs(song_voice_t *channel, int32_t *buffer, uint32_t samples)
{
	int32_t rofs = channel->rofs;
	int32_t lofs = channel->lofs;

	if (!rofs && !lofs)
		return;

    for (uint32_t i = 0; i < samples; i++) {
		int32_t x_r = rshift_signed(rofs + (rshift_signed(-rofs, 31) & OFSDECAYMASK), OFSDECAYSHIFT);
		int32_t x_l = rshift_signed(lofs + (rshift_signed(-lofs, 31) & OFSDECAYMASK), OFSDECAYSHIFT);

		rofs -= x_r;
		lofs -= x_l;
		buffer[i * 2]     += x_r;
		buffer[i * 2 + 1] += x_l;
	}

	channel->rofs = rofs;
	channel->lofs = lofs;
}


void mono_from_stereo(int32_t *mix_buf, uint32_t samples)
{
	for (uint32_t j, i = 0; i < samples; i++) {
		j = i << 1;
		mix_buf[i] = (mix_buf[j] + mix_buf[j + 1]) >> 1;
	}
}

// ----------------------------------------------------------------------------
// Clip and convert functions
// ----------------------------------------------------------------------------
// XXX mins/max were int[2]
//
// The original C version was written by Rani Assaf <rani@magic.metawire.com>


// Clip and convert to 8 bit. mins and maxs returned in 27bits: [MIXING_CLIPMIN..MIXING_CLIPMAX]. mins[0] left, mins[1] right.
uint32_t clip_32_to_8(void *ptr, int32_t *buffer, uint32_t samples, int32_t *mins, int32_t *maxs)
{
	unsigned char *p = (unsigned char *) ptr;

    for (uint32_t i = 0; i < samples; i++) {
		int32_t n = CLAMP(buffer[i], MIXING_CLIPMIN, MIXING_CLIPMAX);

		if (n < mins[i & 1])
		    mins[i & 1] = n;
		else if (n > maxs[i & 1])
		    maxs[i & 1] = n;

		// 8-bit unsigned
		p[i] = rshift_signed(n, 24 - MIXING_ATTENUATION) ^ 0x80;
    }

	return samples;
}


// Clip and convert to 16 bit. mins and maxs returned in 27bits: [MIXING_CLIPMIN..MIXING_CLIPMAX]. mins[0] left, mins[1] right.
uint32_t clip_32_to_16(void *ptr, int32_t *buffer, uint32_t samples, int32_t *mins, int32_t *maxs)
{
    int16_t *p = (int16_t *) ptr;

    for (uint32_t i = 0; i < samples; i++) {
		int32_t n = CLAMP(buffer[i], MIXING_CLIPMIN, MIXING_CLIPMAX);

		if (n < mins[i & 1])
		    mins[i & 1] = n;
		else if (n > maxs[i & 1])
		    maxs[i & 1] = n;

		// 16-bit signed
		p[i] = rshift_signed(n, 16 - MIXING_ATTENUATION);
    }

	return samples * 2;
}


// Clip and convert to 24 bit. mins and maxs returned in 27bits: [MIXING_CLIPMIN..MIXING_CLIPMAX]. mins[0] left, mins[1] right.
// Note, this is 24bit, not 24-in-32bits. The former is used in .wav. The latter is used in audio IO
uint32_t clip_32_to_24(void *ptr, int32_t *buffer, uint32_t samples, int32_t *mins, int32_t *maxs)
{
	/* the inventor of 24bit anything should be shot */
	unsigned char *p = (unsigned char *) ptr;

    for (uint32_t i = 0; i < samples; i++) {
		int32_t n = CLAMP(buffer[i], MIXING_CLIPMIN, MIXING_CLIPMAX);

		if (n < mins[i & 1])
		    mins[i & 1] = n;
		else if (n > maxs[i & 1])
		    maxs[i & 1] = n;

		// 24-bit signed
		n = rshift_signed(n, 8 - MIXING_ATTENUATION);

		/* err, assume same endian */
		memcpy(p, &n, 3);
		p += 3;
    }

	return samples * 3;
}


// Clip and convert to 32 bit(int). mins and maxs returned in 27bits: [MIXING_CLIPMIN..MIXING_CLIPMAX]. mins[0] left, mins[1] right.
uint32_t clip_32_to_32(void *ptr, int32_t *buffer, uint32_t samples, int32_t *mins, int32_t *maxs)
{
    int32_t *p = (int32_t *) ptr;

    for (uint32_t i = 0; i < samples; i++) {
		int32_t n = CLAMP(buffer[i], MIXING_CLIPMIN, MIXING_CLIPMAX);

		if (n < mins[i & 1])
		    mins[i & 1] = n;
		else if (n > maxs[i & 1])
		    maxs[i & 1] = n;

		// 32-bit signed
		p[i] = lshift_signed(n, MIXING_ATTENUATION);
    }

	return samples * 4;
}

