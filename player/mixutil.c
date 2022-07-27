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

#include <string.h>

#include "sndfile.h"

#include "cmixer.h"

#define OFSDECAYSHIFT 8
#define OFSDECAYMASK  0xFF


void init_mix_buffer(int *buffer, unsigned int samples)
{
    memset(buffer, 0, samples * sizeof(int));
}


void stereo_fill(int *buffer, unsigned int samples, int* profs, int *plofs)
{
    int rofs = *profs;
    int lofs = *plofs;

    if (!rofs && !lofs) {
	init_mix_buffer(buffer, samples * 2);
	return;
    }

    for (unsigned int i = 0; i < samples; i++) {
	int x_r = (rofs + (((-rofs) >> 31) & OFSDECAYMASK)) >> OFSDECAYSHIFT;
	int x_l = (lofs + (((-lofs) >> 31) & OFSDECAYMASK)) >> OFSDECAYSHIFT;

	rofs -= x_r;
	lofs -= x_l;
	buffer[i * 2 ]    = x_r;
	buffer[i * 2 + 1] = x_l;
    }

    *profs = rofs;
    *plofs = lofs;
}


void end_channel_ofs(song_voice_t *channel, int *buffer, unsigned int samples)
{
    int rofs = channel->rofs;
    int lofs = channel->lofs;

    if (!rofs && !lofs)
	return;

    for (unsigned int i = 0; i < samples; i++) {
	int x_r = (rofs + (((-rofs) >> 31) & OFSDECAYMASK)) >> OFSDECAYSHIFT;
	int x_l = (lofs + (((-lofs) >> 31) & OFSDECAYMASK)) >> OFSDECAYSHIFT;

	rofs -= x_r;
	lofs -= x_l;
	buffer[i * 2]     += x_r;
	buffer[i * 2 + 1] += x_l;
    }

    channel->rofs = rofs;
    channel->lofs = lofs;
}


void mono_from_stereo(int *mix_buf, unsigned int samples)
{
    for (unsigned int j, i = 0; i < samples; i++) {
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
unsigned int clip_32_to_8(void *ptr, int *buffer, unsigned int samples, int *mins, int *maxs)
{
    unsigned char *p = (unsigned char *) ptr;

    for (unsigned int i = 0; i < samples; i++) {
	int n = buffer[i];

	if (n < MIXING_CLIPMIN)
	    n = MIXING_CLIPMIN;
	else if (n > MIXING_CLIPMAX)
	    n = MIXING_CLIPMAX;

	if (n < mins[i & 1])
	    mins[i & 1] = n;
	else if (n > maxs[i & 1])
	    maxs[i & 1] = n;

	// 8-bit unsigned
	p[i] = (n >> (24 - MIXING_ATTENUATION)) ^ 0x80;
    }

    return samples;
}


// Clip and convert to 16 bit. mins and maxs returned in 27bits: [MIXING_CLIPMIN..MIXING_CLIPMAX]. mins[0] left, mins[1] right.
unsigned int clip_32_to_16(void *ptr, int *buffer, unsigned int samples, int *mins, int *maxs)
{
    signed short *p = (signed short *) ptr;

    for (unsigned int i = 0; i < samples; i++) {
	int n = buffer[i];

	if (n < MIXING_CLIPMIN)
	    n = MIXING_CLIPMIN;
	else if (n > MIXING_CLIPMAX)
	    n = MIXING_CLIPMAX;

	if (n < mins[i & 1])
	    mins[i & 1] = n;
	else if (n > maxs[i & 1])
	    maxs[i & 1] = n;

	// 16-bit signed
	p[i] = n >> (16 - MIXING_ATTENUATION);
    }

    return samples * 2;
}


// Clip and convert to 24 bit. mins and maxs returned in 27bits: [MIXING_CLIPMIN..MIXING_CLIPMAX]. mins[0] left, mins[1] right.
// Note, this is 24bit, not 24-in-32bits. The former is used in .wav. The latter is used in audio IO
unsigned int clip_32_to_24(void *ptr, int *buffer, unsigned int samples, int *mins, int *maxs)
{
    /* the inventor of 24bit anything should be shot */
    unsigned char *p = (unsigned char *) ptr;

    for (unsigned int i = 0; i < samples; i++) {
	int n = buffer[i];

	if (n < MIXING_CLIPMIN)
	    n = MIXING_CLIPMIN;
	else if (n > MIXING_CLIPMAX)
	    n = MIXING_CLIPMAX;

	if (n < mins[i & 1])
	    mins[i & 1] = n;
	else if (n > maxs[i & 1])
	    maxs[i & 1] = n;

	// 24-bit signed
	n = n >> (8 - MIXING_ATTENUATION);

	/* err, assume same endian */
	memcpy(p, &n, 3);
	p += 3;
    }

    return samples * 3;
}


// Clip and convert to 32 bit(int). mins and maxs returned in 27bits: [MIXING_CLIPMIN..MIXING_CLIPMAX]. mins[0] left, mins[1] right.
unsigned int clip_32_to_32(void *ptr, int *buffer, unsigned int samples, int *mins, int *maxs)
{
    signed int *p = (signed int *) ptr;

    for (unsigned int i = 0; i < samples; i++) {
	int n = buffer[i];

	if (n < MIXING_CLIPMIN)
	    n = MIXING_CLIPMIN;
	else if (n > MIXING_CLIPMAX)
	    n = MIXING_CLIPMAX;

	if (n < mins[i & 1])
	    mins[i & 1] = n;
	else if (n > maxs[i & 1])
	    maxs[i & 1] = n;

	// 32-bit signed
	p[i] = (n << MIXING_ATTENUATION);
    }

    return samples * 4;
}

