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

#include <stdint.h>
#include <math.h>

#include "sndfile.h"
#include "snd_fm.h"
#include "snd_gm.h"
#include "cmixer.h"
#include "util.h" // for CLAMP

// For pingpong loops that work like most of Impulse Tracker's drivers
// (including SB16, SBPro, and the disk writer) -- as well as XMPlay, use 1
// To make them sound like the GUS driver, use 0.
// It's really only noticeable for very small loops... (e.g. chip samples)
// (thanks Saga_Musix for this)
#define PINGPONG_OFFSET 1



/* The following lut settings are PRECOMPUTED.
 *
 * If you plan on changing these settings, you
 * MUST also regenerate the arrays.
 */
// number of bits used to scale spline coefs
#define SPLINE_QUANTBITS        14
#define SPLINE_QUANTSCALE       (1L << SPLINE_QUANTBITS)
#define SPLINE_8SHIFT           (SPLINE_QUANTBITS - 8)
#define SPLINE_16SHIFT          (SPLINE_QUANTBITS)

// forces coefsset to unity gain
#define SPLINE_CLAMPFORUNITY

// log2(number) of precalculated splines (range is [4..14])
#define SPLINE_FRACBITS 10
#define SPLINE_LUTLEN (1L<<SPLINE_FRACBITS)


// quantizer scale of window coefs
#define WFIR_QUANTBITS          15
#define WFIR_QUANTSCALE         (1L << WFIR_QUANTBITS)
#define WFIR_8SHIFT             (WFIR_QUANTBITS - 8)
#define WFIR_16BITSHIFT         (WFIR_QUANTBITS)

// log2(number)-1 of precalculated taps range is [4..12]
#define WFIR_FRACBITS           10
#define WFIR_LUTLEN             ((1L << (WFIR_FRACBITS + 1)) + 1)

// number of samples in window
#define WFIR_LOG2WIDTH          3
#define WFIR_WIDTH              (1L << WFIR_LOG2WIDTH)
#define WFIR_SMPSPERWING        ((WFIR_WIDTH-1)>>1)
// cutoff (1.0 == pi/2)
#define WFIR_CUTOFF             0.90f
// wfir type
#define WFIR_HANN               0
#define WFIR_HAMMING            1
#define WFIR_BLACKMANEXACT      2
#define WFIR_BLACKMAN3T61       3
#define WFIR_BLACKMAN3T67       4
#define WFIR_BLACKMAN4T92       5
#define WFIR_BLACKMAN4T74       6
#define WFIR_KAISER4T           7
#define WFIR_TYPE               WFIR_BLACKMANEXACT
// wfir help
#ifndef M_zPI
#define M_zPI           3.1415926535897932384626433832795
#endif
#define M_zEPS          1e-8
#define M_zBESSELEPS    1e-21


#include "precomp_lut.h"


// ----------------------------------------------------------------------------
// MIXING MACROS
// ----------------------------------------------------------------------------

#define SNDMIX_BEGINSAMPLELOOP8 \
	register song_voice_t * const chan = channel; \
	position = chan->position_frac; \
	const signed char *p = (signed char *)(chan->current_sample_data + chan->position); \
	if (chan->flags & CHN_STEREO) p += chan->position; \
	int *pvol = pbuffer;\
	do {


#define SNDMIX_BEGINSAMPLELOOP16\
	register song_voice_t * const chan = channel;\
	position = chan->position_frac;\
	const signed short *p = (signed short *)(chan->current_sample_data+(chan->position*2));\
	if (chan->flags & CHN_STEREO) p += chan->position;\
	int *pvol = pbuffer;\
	do {


#define SNDMIX_ENDSAMPLELOOP \
		position += chan->increment; \
	} while (pvol < pbufmax); \
	chan->position  += position >> 16; \
	chan->position_frac = position & 0xFFFF;


#define SNDMIX_ENDSAMPLELOOP8   SNDMIX_ENDSAMPLELOOP
#define SNDMIX_ENDSAMPLELOOP16  SNDMIX_ENDSAMPLELOOP


//////////////////////////////////////////////////////////////////////////////
// Mono

// No interpolation
#define SNDMIX_GETMONOVOL8NOIDO \
    int vol = p[position >> 16] << 8;


#define SNDMIX_GETMONOVOL16NOIDO \
    int vol = p[position >> 16];


// Linear Interpolation
#define SNDMIX_GETMONOVOL8LINEAR \
    int poshi   = position >> 16; \
    int poslo   = (position >> 8) & 0xFF; \
    int srcvol  = p[poshi]; \
    int destvol = p[poshi+1]; \
    int vol     = (srcvol<<8) + ((int)(poslo * (destvol - srcvol)));


#define SNDMIX_GETMONOVOL16LINEAR \
    int poshi   = position >> 16; \
    int poslo   = (position >> 8) & 0xFF; \
    int srcvol  = p[poshi]; \
    int destvol = p[poshi + 1]; \
    int vol     = srcvol + ((int)(poslo * (destvol - srcvol)) >> 8);


// spline interpolation (2 guard bits should be enough???)
#define SPLINE_FRACSHIFT ((16 - SPLINE_FRACBITS) - 2)
#define SPLINE_FRACMASK  (((1L << (16 - SPLINE_FRACSHIFT)) - 1) & ~3)


#define SNDMIX_GETMONOVOL8SPLINE \
    int poshi = position >> 16; \
    int poslo = (position >> SPLINE_FRACSHIFT) & SPLINE_FRACMASK; \
    int vol   = (cubic_spline_lut[poslo    ] * (int)p[poshi - 1] + \
		 cubic_spline_lut[poslo + 1] * (int)p[poshi    ] + \
		 cubic_spline_lut[poslo + 3] * (int)p[poshi + 2] + \
		 cubic_spline_lut[poslo + 2] * (int)p[poshi + 1]) >> SPLINE_8SHIFT;


#define SNDMIX_GETMONOVOL16SPLINE \
    int poshi = position >> 16; \
    int poslo = (position >> SPLINE_FRACSHIFT) & SPLINE_FRACMASK; \
    int vol   = (cubic_spline_lut[poslo    ] * (int)p[poshi - 1] + \
		 cubic_spline_lut[poslo + 1] * (int)p[poshi    ] + \
		 cubic_spline_lut[poslo + 3] * (int)p[poshi + 2] + \
		 cubic_spline_lut[poslo + 2] * (int)p[poshi + 1]) >> SPLINE_16SHIFT;


// fir interpolation
#define WFIR_FRACSHIFT (16 - (WFIR_FRACBITS + 1 + WFIR_LOG2WIDTH))
#define WFIR_FRACMASK  ((((1L << (17 - WFIR_FRACSHIFT)) - 1) & ~((1L << WFIR_LOG2WIDTH) - 1)))
#define WFIR_FRACHALVE (1L << (16 - (WFIR_FRACBITS + 2)))


#define SNDMIX_GETMONOVOL8FIRFILTER \
    int poshi  = position >> 16;\
    int poslo  = (position & 0xFFFF);\
    int firidx = ((poslo + WFIR_FRACHALVE) >> WFIR_FRACSHIFT) & WFIR_FRACMASK; \
    int vol    = (windowed_fir_lut[firidx + 0] * (int)p[poshi + 1 - 4]); \
	vol   += (windowed_fir_lut[firidx + 1] * (int)p[poshi + 2 - 4]); \
	vol   += (windowed_fir_lut[firidx + 2] * (int)p[poshi + 3 - 4]); \
	vol   += (windowed_fir_lut[firidx + 3] * (int)p[poshi + 4 - 4]); \
	vol   += (windowed_fir_lut[firidx + 4] * (int)p[poshi + 5 - 4]); \
	vol   += (windowed_fir_lut[firidx + 5] * (int)p[poshi + 6 - 4]); \
	vol   += (windowed_fir_lut[firidx + 6] * (int)p[poshi + 7 - 4]); \
	vol   += (windowed_fir_lut[firidx + 7] * (int)p[poshi + 8 - 4]); \
	vol  >>= WFIR_8SHIFT;


#define SNDMIX_GETMONOVOL16FIRFILTER \
    int poshi  = position >> 16;\
    int poslo  = (position & 0xFFFF);\
    int firidx = ((poslo + WFIR_FRACHALVE) >> WFIR_FRACSHIFT) & WFIR_FRACMASK; \
    int vol1   = (windowed_fir_lut[firidx + 0] * (int)p[poshi + 1 - 4]); \
	vol1  += (windowed_fir_lut[firidx + 1] * (int)p[poshi + 2 - 4]); \
	vol1  += (windowed_fir_lut[firidx + 2] * (int)p[poshi + 3 - 4]); \
	vol1  += (windowed_fir_lut[firidx + 3] * (int)p[poshi + 4 - 4]); \
    int vol2   = (windowed_fir_lut[firidx + 4] * (int)p[poshi + 5 - 4]); \
	vol2  += (windowed_fir_lut[firidx + 5] * (int)p[poshi + 6 - 4]); \
	vol2  += (windowed_fir_lut[firidx + 6] * (int)p[poshi + 7 - 4]); \
	vol2  += (windowed_fir_lut[firidx + 7] * (int)p[poshi + 8 - 4]); \
    int vol    = ((vol1 >> 1) + (vol2 >> 1)) >> (WFIR_16BITSHIFT - 1);


/////////////////////////////////////////////////////////////////////////////
// Stereo

// No interpolation
#define SNDMIX_GETSTEREOVOL8NOIDO \
    int vol_l = p[(position >> 16) * 2    ] << 8; \
    int vol_r = p[(position >> 16) * 2 + 1] << 8;


#define SNDMIX_GETSTEREOVOL16NOIDO \
    int vol_l = p[(position >> 16) * 2    ]; \
    int vol_r = p[(position >> 16) * 2 + 1];


// Linear Interpolation
#define SNDMIX_GETSTEREOVOL8LINEAR \
    int poshi    = position >> 16; \
    int poslo    = (position >> 8) & 0xFF; \
    int srcvol_l = p[poshi * 2]; \
    int vol_l    = (srcvol_l << 8) + ((int)(poslo * (p[poshi * 2 + 2] - srcvol_l))); \
    int srcvol_r = p[poshi * 2 + 1]; \
    int vol_r    = (srcvol_r << 8) + ((int)(poslo * (p[poshi * 2 + 3] - srcvol_r)));


#define SNDMIX_GETSTEREOVOL16LINEAR \
    int poshi    = position >> 16; \
    int poslo    = (position >> 8) & 0xFF; \
    int srcvol_l = p[poshi * 2]; \
    int vol_l    = srcvol_l + ((int)(poslo * (p[poshi * 2 + 2] - srcvol_l)) >> 8);\
    int srcvol_r = p[poshi * 2 + 1];\
    int vol_r    = srcvol_r + ((int)(poslo * (p[poshi * 2 + 3] - srcvol_r)) >> 8);\


// Spline Interpolation
#define SNDMIX_GETSTEREOVOL8SPLINE \
    int poshi   = position >> 16; \
    int poslo   = (position >> SPLINE_FRACSHIFT) & SPLINE_FRACMASK; \
    int vol_l   = (cubic_spline_lut[poslo    ] * (int)p[(poshi - 1) * 2   ] + \
		   cubic_spline_lut[poslo + 1] * (int)p[(poshi    ) * 2   ] + \
		   cubic_spline_lut[poslo + 2] * (int)p[(poshi + 1) * 2   ] + \
		   cubic_spline_lut[poslo + 3] * (int)p[(poshi + 2) * 2   ]) >> SPLINE_8SHIFT; \
    int vol_r   = (cubic_spline_lut[poslo    ] * (int)p[(poshi - 1) * 2 + 1] + \
		   cubic_spline_lut[poslo + 1] * (int)p[(poshi    ) * 2 + 1] + \
		   cubic_spline_lut[poslo + 2] * (int)p[(poshi + 1) * 2 + 1] + \
		   cubic_spline_lut[poslo + 3] * (int)p[(poshi + 2) * 2 + 1]) >> SPLINE_8SHIFT;


#define SNDMIX_GETSTEREOVOL16SPLINE \
    int poshi   = position >> 16; \
    int poslo   = (position >> SPLINE_FRACSHIFT) & SPLINE_FRACMASK; \
    int vol_l   = (cubic_spline_lut[poslo    ] * (int)p[(poshi - 1) * 2    ] + \
		   cubic_spline_lut[poslo + 1] * (int)p[(poshi    ) * 2    ] + \
		   cubic_spline_lut[poslo + 2] * (int)p[(poshi + 1) * 2    ] + \
		   cubic_spline_lut[poslo + 3] * (int)p[(poshi + 2) * 2    ]) >> SPLINE_16SHIFT; \
    int vol_r   = (cubic_spline_lut[poslo    ] * (int)p[(poshi - 1) * 2 + 1] + \
		   cubic_spline_lut[poslo + 1] * (int)p[(poshi    ) * 2 + 1] + \
		   cubic_spline_lut[poslo + 2] * (int)p[(poshi + 1) * 2 + 1] + \
		   cubic_spline_lut[poslo + 3] * (int)p[(poshi + 2) * 2 + 1]) >> SPLINE_16SHIFT;


// fir interpolation
#define SNDMIX_GETSTEREOVOL8FIRFILTER \
    int poshi   = position >> 16;\
    int poslo   = (position & 0xFFFF);\
    int firidx  = ((poslo + WFIR_FRACHALVE) >> WFIR_FRACSHIFT) & WFIR_FRACMASK; \
    int vol_l   = (windowed_fir_lut[firidx + 0] * (int)p[(poshi + 1 - 4) * 2]); \
	vol_l  += (windowed_fir_lut[firidx + 1] * (int)p[(poshi + 2 - 4) * 2]); \
	vol_l  += (windowed_fir_lut[firidx + 2] * (int)p[(poshi + 3 - 4) * 2]); \
	vol_l  += (windowed_fir_lut[firidx + 3] * (int)p[(poshi + 4 - 4) * 2]); \
	vol_l  += (windowed_fir_lut[firidx + 4] * (int)p[(poshi + 5 - 4) * 2]); \
	vol_l  += (windowed_fir_lut[firidx + 5] * (int)p[(poshi + 6 - 4) * 2]); \
	vol_l  += (windowed_fir_lut[firidx + 6] * (int)p[(poshi + 7 - 4) * 2]); \
	vol_l  += (windowed_fir_lut[firidx + 7] * (int)p[(poshi + 8 - 4) * 2]); \
	vol_l >>= WFIR_8SHIFT; \
    int vol_r   = (windowed_fir_lut[firidx + 0] * (int)p[(poshi + 1 - 4) * 2 + 1]); \
	vol_r  += (windowed_fir_lut[firidx + 1] * (int)p[(poshi + 2 - 4) * 2 + 1]); \
	vol_r  += (windowed_fir_lut[firidx + 2] * (int)p[(poshi + 3 - 4) * 2 + 1]); \
	vol_r  += (windowed_fir_lut[firidx + 3] * (int)p[(poshi + 4 - 4) * 2 + 1]); \
	vol_r  += (windowed_fir_lut[firidx + 4] * (int)p[(poshi + 5 - 4) * 2 + 1]); \
	vol_r  += (windowed_fir_lut[firidx + 5] * (int)p[(poshi + 6 - 4) * 2 + 1]); \
	vol_r  += (windowed_fir_lut[firidx + 6] * (int)p[(poshi + 7 - 4) * 2 + 1]); \
	vol_r  += (windowed_fir_lut[firidx + 7] * (int)p[(poshi + 8 - 4) * 2 + 1]); \
	vol_r >>= WFIR_8SHIFT;


#define SNDMIX_GETSTEREOVOL16FIRFILTER \
    int poshi   = position >> 16;\
    int poslo   = (position & 0xFFFF);\
    int firidx  = ((poslo + WFIR_FRACHALVE) >> WFIR_FRACSHIFT) & WFIR_FRACMASK; \
    int vol1_l  = (windowed_fir_lut[firidx + 0] * (int)p[(poshi + 1 - 4) * 2]); \
	vol1_l += (windowed_fir_lut[firidx + 1] * (int)p[(poshi + 2 - 4) * 2]); \
	vol1_l += (windowed_fir_lut[firidx + 2] * (int)p[(poshi + 3 - 4) * 2]); \
	vol1_l += (windowed_fir_lut[firidx + 3] * (int)p[(poshi + 4 - 4) * 2]); \
    int vol2_l  = (windowed_fir_lut[firidx + 4] * (int)p[(poshi + 5 - 4) * 2]); \
	vol2_l += (windowed_fir_lut[firidx + 5] * (int)p[(poshi + 6 - 4) * 2]); \
	vol2_l += (windowed_fir_lut[firidx + 6] * (int)p[(poshi + 7 - 4) * 2]); \
	vol2_l += (windowed_fir_lut[firidx + 7] * (int)p[(poshi + 8 - 4) * 2]); \
    int vol_l   = ((vol1_l >> 1) + (vol2_l >> 1)) >> (WFIR_16BITSHIFT - 1); \
    int vol1_r  = (windowed_fir_lut[firidx + 0] * (int)p[(poshi + 1 - 4) * 2 + 1]);    \
	vol1_r += (windowed_fir_lut[firidx + 1] * (int)p[(poshi + 2 - 4) * 2 + 1]);    \
	vol1_r += (windowed_fir_lut[firidx + 2] * (int)p[(poshi + 3 - 4) * 2 + 1]);    \
	vol1_r += (windowed_fir_lut[firidx + 3] * (int)p[(poshi + 4 - 4) * 2 + 1]);    \
    int vol2_r  = (windowed_fir_lut[firidx + 4] * (int)p[(poshi + 5 - 4) * 2 + 1]);    \
	vol2_r += (windowed_fir_lut[firidx + 5] * (int)p[(poshi + 6 - 4) * 2 + 1]);    \
	vol2_r += (windowed_fir_lut[firidx + 6] * (int)p[(poshi + 7 - 4) * 2 + 1]);    \
	vol2_r += (windowed_fir_lut[firidx + 7] * (int)p[(poshi + 8 - 4) * 2 + 1]);    \
    int vol_r   = ((vol1_r >> 1) + (vol2_r >> 1)) >> (WFIR_16BITSHIFT - 1);


#define SNDMIX_STOREMONOVOL \
    pvol[0] += vol * chan->right_volume; \
    pvol[1] += vol * chan->left_volume; \
    pvol += 2;


#define SNDMIX_STORESTEREOVOL \
    pvol[0] += vol_l * chan->right_volume; \
    pvol[1] += vol_r * chan->left_volume; \
    pvol += 2;


#define SNDMIX_STOREFASTMONOVOL \
    int v = vol * chan->right_volume; \
    pvol[0] += v; \
    pvol[1] += v; \
    pvol += 2;


#define SNDMIX_RAMPMONOVOL \
    left_ramp_volume += chan->left_ramp; \
    right_ramp_volume += chan->right_ramp; \
    pvol[0] += vol * (right_ramp_volume >> VOLUMERAMPPRECISION); \
    pvol[1] += vol * (left_ramp_volume >> VOLUMERAMPPRECISION); \
    pvol += 2;


#define SNDMIX_RAMPFASTMONOVOL \
    right_ramp_volume += chan->right_ramp; \
    int fastvol = vol * (right_ramp_volume >> VOLUMERAMPPRECISION); \
    pvol[0] += fastvol; \
    pvol[1] += fastvol; \
    pvol += 2;


#define SNDMIX_RAMPSTEREOVOL \
    left_ramp_volume += chan->left_ramp; \
    right_ramp_volume += chan->right_ramp; \
    pvol[0] += vol_l * (right_ramp_volume >> VOLUMERAMPPRECISION); \
    pvol[1] += vol_r * (left_ramp_volume >> VOLUMERAMPPRECISION); \
    pvol += 2;


///////////////////////////////////////////////////
// Resonant Filters

#define FILT_CLIP(i) CLAMP(i, -65536, 65534)

// Mono
#define MIX_BEGIN_FILTER \
    int32_t fy1 = channel->filter_y1; \
    int32_t fy2 = channel->filter_y2; \
    int32_t ta;


#define MIX_END_FILTER \
    channel->filter_y1 = fy1; \
    channel->filter_y2 = fy2;


#define SNDMIX_PROCESSFILTER \
    ta = (vol * chan->filter_a0 + FILT_CLIP(fy1) * chan->filter_b0 + FILT_CLIP(fy2) * chan->filter_b1 \
	+ (1 << (FILTERPRECISION - 1))) >> FILTERPRECISION; \
    fy2 = fy1; \
    fy1 = ta; \
    vol = ta;


// Stereo
#define MIX_BEGIN_STEREO_FILTER \
    int32_t fy1 = channel->filter_y1; \
    int32_t fy2 = channel->filter_y2; \
    int32_t fy3 = channel->filter_y3; \
    int32_t fy4 = channel->filter_y4; \
    int32_t ta, tb;


#define MIX_END_STEREO_FILTER \
    channel->filter_y1 = fy1; \
    channel->filter_y2 = fy2; \
    channel->filter_y3 = fy3; \
    channel->filter_y4 = fy4; \


#define SNDMIX_PROCESSSTEREOFILTER \
    ta = (vol_l * chan->filter_a0 + FILT_CLIP(fy1) * chan->filter_b0 + FILT_CLIP(fy2) * chan->filter_b1 \
	+ (1 << (FILTERPRECISION - 1))) >> FILTERPRECISION; \
    tb = (vol_r * chan->filter_a0 + FILT_CLIP(fy3) * chan->filter_b0 + FILT_CLIP(fy4) * chan->filter_b1 \
	+ (1 << (FILTERPRECISION - 1))) >> FILTERPRECISION; \
    fy2 = fy1; fy1 = ta; vol_l = ta; \
    fy4 = fy3; fy3 = tb; vol_r = tb;


//////////////////////////////////////////////////////////
// Interfaces

typedef void(* mix_interface_t)(song_voice_t *, int *, int *);


#define BEGIN_MIX_INTERFACE(func) \
    static void func(song_voice_t *channel, int *pbuffer, int *pbufmax) \
    { \
	int position;


#define END_MIX_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
    }


// Volume Ramps
#define BEGIN_RAMPMIX_INTERFACE(func) \
    BEGIN_MIX_INTERFACE(func) \
	int right_ramp_volume = channel->right_ramp_volume; \
	int left_ramp_volume = channel->left_ramp_volume;


#define END_RAMPMIX_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
	channel->right_ramp_volume = right_ramp_volume; \
	channel->right_volume     = right_ramp_volume >> VOLUMERAMPPRECISION; \
	channel->left_ramp_volume  = left_ramp_volume; \
	channel->left_volume      = left_ramp_volume >> VOLUMERAMPPRECISION; \
    }


#define BEGIN_FASTRAMPMIX_INTERFACE(func) \
    BEGIN_MIX_INTERFACE(func) \
	int right_ramp_volume = channel->right_ramp_volume;


#define END_FASTRAMPMIX_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
	channel->right_ramp_volume = right_ramp_volume; \
	channel->left_ramp_volume  = right_ramp_volume; \
	channel->right_volume     = right_ramp_volume >> VOLUMERAMPPRECISION; \
	channel->left_volume      = channel->right_volume; \
    }


// Mono Resonant Filters
#define BEGIN_MIX_FLT_INTERFACE(func) \
    BEGIN_MIX_INTERFACE(func) \
    MIX_BEGIN_FILTER


#define END_MIX_FLT_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
	MIX_END_FILTER \
    }


#define BEGIN_RAMPMIX_FLT_INTERFACE(func) \
    BEGIN_MIX_INTERFACE(func) \
	int right_ramp_volume = channel->right_ramp_volume; \
	int left_ramp_volume  = channel->left_ramp_volume; \
	MIX_BEGIN_FILTER


#define END_RAMPMIX_FLT_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
	MIX_END_FILTER \
	channel->right_ramp_volume = right_ramp_volume; \
	channel->right_volume     = right_ramp_volume >> VOLUMERAMPPRECISION; \
	channel->left_ramp_volume  = left_ramp_volume; \
	channel->left_volume      = left_ramp_volume >> VOLUMERAMPPRECISION; \
    }


// Stereo Resonant Filters
#define BEGIN_MIX_STFLT_INTERFACE(func) \
    BEGIN_MIX_INTERFACE(func) \
    MIX_BEGIN_STEREO_FILTER


#define END_MIX_STFLT_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
	MIX_END_STEREO_FILTER \
    }


#define BEGIN_RAMPMIX_STFLT_INTERFACE(func) \
    BEGIN_MIX_INTERFACE(func) \
	int right_ramp_volume = channel->right_ramp_volume; \
	int left_ramp_volume  = channel->left_ramp_volume; \
	MIX_BEGIN_STEREO_FILTER


#define END_RAMPMIX_STFLT_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
	MIX_END_STEREO_FILTER \
	channel->right_ramp_volume = right_ramp_volume; \
	channel->right_volume     = right_ramp_volume >> VOLUMERAMPPRECISION; \
	channel->left_ramp_volume  = left_ramp_volume; \
	channel->left_volume      = left_ramp_volume >> VOLUMERAMPPRECISION; \
    }

#define BEGIN_RESAMPLE_INTERFACE(func, sampletype, numchannels) \
    void func(sampletype *oldbuf, sampletype *newbuf, unsigned long oldlen, unsigned long newlen) \
    { \
	unsigned long long position = 0; \
	const sampletype *p = oldbuf; \
	sampletype *pvol = newbuf; \
	const sampletype *pbufmax = &newbuf[newlen* numchannels]; \
	unsigned long long increment = (((unsigned long long)oldlen)<<16)/((unsigned long long)newlen); \
	do {

#define END_RESAMPLE_INTERFACEMONO() \
		*pvol = vol; \
		pvol++; \
		position += increment; \
	} while (pvol < pbufmax); \
    }

#define END_RESAMPLE_INTERFACESTEREO() \
		pvol[0] = vol_l; \
		pvol[1] = vol_r; \
		pvol += 2; \
		position += increment; \
	} while (pvol < pbufmax); \
    }



/////////////////////////////////////////////////////
// Mono samples functions

BEGIN_MIX_INTERFACE(Mono8BitMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8NOIDO
	SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono16BitMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16NOIDO
	SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono8BitLinearMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8LINEAR
	SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono16BitLinearMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16LINEAR
	SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono8BitSplineMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8SPLINE
	SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono16BitSplineMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16SPLINE
	SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono8BitFirFilterMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8FIRFILTER
	SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono16BitFirFilterMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16FIRFILTER
	SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()


// Volume Ramps
BEGIN_RAMPMIX_INTERFACE(Mono8BitRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8NOIDO
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono16BitRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16NOIDO
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono8BitLinearRampMix)
   SNDMIX_BEGINSAMPLELOOP8
   SNDMIX_GETMONOVOL8LINEAR
   SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono16BitLinearRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16LINEAR
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono8BitSplineRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8SPLINE
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono16BitSplineRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16SPLINE
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono8BitFirFilterRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8FIRFILTER
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono16BitFirFilterRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16FIRFILTER
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()


//////////////////////////////////////////////////////
// Fast mono mix for leftvol=rightvol (1 less imul)

BEGIN_MIX_INTERFACE(FastMono8BitMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8NOIDO
	SNDMIX_STOREFASTMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(FastMono16BitMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16NOIDO
	SNDMIX_STOREFASTMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(FastMono8BitLinearMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8LINEAR
	SNDMIX_STOREFASTMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(FastMono16BitLinearMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16LINEAR
	SNDMIX_STOREFASTMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(FastMono8BitSplineMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8SPLINE
	SNDMIX_STOREFASTMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(FastMono16BitSplineMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16SPLINE
	SNDMIX_STOREFASTMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(FastMono8BitFirFilterMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8FIRFILTER
	SNDMIX_STOREFASTMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(FastMono16BitFirFilterMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16FIRFILTER
	SNDMIX_STOREFASTMONOVOL
END_MIX_INTERFACE()


// Fast Ramps
BEGIN_FASTRAMPMIX_INTERFACE(FastMono8BitRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8NOIDO
	SNDMIX_RAMPFASTMONOVOL
END_FASTRAMPMIX_INTERFACE()

BEGIN_FASTRAMPMIX_INTERFACE(FastMono16BitRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16NOIDO
	SNDMIX_RAMPFASTMONOVOL
END_FASTRAMPMIX_INTERFACE()

BEGIN_FASTRAMPMIX_INTERFACE(FastMono8BitLinearRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8LINEAR
	SNDMIX_RAMPFASTMONOVOL
END_FASTRAMPMIX_INTERFACE()

BEGIN_FASTRAMPMIX_INTERFACE(FastMono16BitLinearRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16LINEAR
	SNDMIX_RAMPFASTMONOVOL
END_FASTRAMPMIX_INTERFACE()

BEGIN_FASTRAMPMIX_INTERFACE(FastMono8BitSplineRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8SPLINE
	SNDMIX_RAMPFASTMONOVOL
END_FASTRAMPMIX_INTERFACE()

BEGIN_FASTRAMPMIX_INTERFACE(FastMono16BitSplineRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16SPLINE
	SNDMIX_RAMPFASTMONOVOL
END_FASTRAMPMIX_INTERFACE()

BEGIN_FASTRAMPMIX_INTERFACE(FastMono8BitFirFilterRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8FIRFILTER
	SNDMIX_RAMPFASTMONOVOL
END_FASTRAMPMIX_INTERFACE()

BEGIN_FASTRAMPMIX_INTERFACE(FastMono16BitFirFilterRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16FIRFILTER
	SNDMIX_RAMPFASTMONOVOL
END_FASTRAMPMIX_INTERFACE()


//////////////////////////////////////////////////////
// Stereo samples
BEGIN_MIX_INTERFACE(Stereo8BitMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8NOIDO
	SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Stereo16BitMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16NOIDO
	SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Stereo8BitLinearMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8LINEAR
	SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Stereo16BitLinearMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16LINEAR
	SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Stereo8BitSplineMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8SPLINE
	SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Stereo16BitSplineMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16SPLINE
	SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Stereo8BitFirFilterMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8FIRFILTER
	SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Stereo16BitFirFilterMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16FIRFILTER
	SNDMIX_STORESTEREOVOL
END_MIX_INTERFACE()


// Volume Ramps
BEGIN_RAMPMIX_INTERFACE(Stereo8BitRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8NOIDO
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Stereo16BitRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16NOIDO
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Stereo8BitLinearRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8LINEAR
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Stereo16BitLinearRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16LINEAR
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Stereo8BitSplineRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8SPLINE
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Stereo16BitSplineRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16SPLINE
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Stereo8BitFirFilterRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8FIRFILTER
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Stereo16BitFirFilterRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16FIRFILTER
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_INTERFACE()


//////////////////////////////////////////////////////
// Resonant Filter Mix
// Mono Filter Mix
BEGIN_MIX_FLT_INTERFACE(FilterMono8BitMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8NOIDO
	SNDMIX_PROCESSFILTER
	SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

BEGIN_MIX_FLT_INTERFACE(FilterMono16BitMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16NOIDO
	SNDMIX_PROCESSFILTER
	SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

BEGIN_MIX_FLT_INTERFACE(FilterMono8BitLinearMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8LINEAR
	SNDMIX_PROCESSFILTER
	SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

BEGIN_MIX_FLT_INTERFACE(FilterMono16BitLinearMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16LINEAR
	SNDMIX_PROCESSFILTER
	SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

BEGIN_MIX_FLT_INTERFACE(FilterMono8BitSplineMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8SPLINE
	SNDMIX_PROCESSFILTER
	SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

BEGIN_MIX_FLT_INTERFACE(FilterMono16BitSplineMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16SPLINE
	SNDMIX_PROCESSFILTER
	SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

BEGIN_MIX_FLT_INTERFACE(FilterMono8BitFirFilterMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8FIRFILTER
	SNDMIX_PROCESSFILTER
	SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()

BEGIN_MIX_FLT_INTERFACE(FilterMono16BitFirFilterMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16FIRFILTER
	SNDMIX_PROCESSFILTER
	SNDMIX_STOREMONOVOL
END_MIX_FLT_INTERFACE()


// Filter + Ramp
BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono8BitRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8NOIDO
	SNDMIX_PROCESSFILTER
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono16BitRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16NOIDO
	SNDMIX_PROCESSFILTER
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono8BitLinearRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8LINEAR
	SNDMIX_PROCESSFILTER
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono16BitLinearRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16LINEAR
	SNDMIX_PROCESSFILTER
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono8BitSplineRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8SPLINE
	SNDMIX_PROCESSFILTER
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono16BitSplineRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16SPLINE
	SNDMIX_PROCESSFILTER
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono8BitFirFilterRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETMONOVOL8FIRFILTER
	SNDMIX_PROCESSFILTER
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()

BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono16BitFirFilterRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETMONOVOL16FIRFILTER
	SNDMIX_PROCESSFILTER
	SNDMIX_RAMPMONOVOL
END_RAMPMIX_FLT_INTERFACE()


// Stereo Filter Mix
BEGIN_MIX_STFLT_INTERFACE(FilterStereo8BitMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8NOIDO
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

BEGIN_MIX_STFLT_INTERFACE(FilterStereo16BitMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16NOIDO
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

BEGIN_MIX_STFLT_INTERFACE(FilterStereo8BitLinearMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8LINEAR
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

BEGIN_MIX_STFLT_INTERFACE(FilterStereo16BitLinearMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16LINEAR
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

BEGIN_MIX_STFLT_INTERFACE(FilterStereo8BitSplineMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8SPLINE
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

BEGIN_MIX_STFLT_INTERFACE(FilterStereo16BitSplineMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16SPLINE
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

BEGIN_MIX_STFLT_INTERFACE(FilterStereo8BitFirFilterMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8FIRFILTER
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()

BEGIN_MIX_STFLT_INTERFACE(FilterStereo16BitFirFilterMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16FIRFILTER
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_STORESTEREOVOL
END_MIX_STFLT_INTERFACE()


// Stereo Filter + Ramp
BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo8BitRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8NOIDO
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo16BitRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16NOIDO
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo8BitLinearRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8LINEAR
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo16BitLinearRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16LINEAR
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo8BitSplineRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8SPLINE
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo16BitSplineRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16SPLINE
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo8BitFirFilterRampMix)
	SNDMIX_BEGINSAMPLELOOP8
	SNDMIX_GETSTEREOVOL8FIRFILTER
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()

BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo16BitFirFilterRampMix)
	SNDMIX_BEGINSAMPLELOOP16
	SNDMIX_GETSTEREOVOL16FIRFILTER
	SNDMIX_PROCESSSTEREOFILTER
	SNDMIX_RAMPSTEREOVOL
END_RAMPMIX_STFLT_INTERFACE()



// Public resampling Methods (
BEGIN_RESAMPLE_INTERFACE(ResampleMono8BitFirFilter, signed char, 1)
	SNDMIX_GETMONOVOL8FIRFILTER
	vol  >>= (WFIR_16BITSHIFT-WFIR_8SHIFT);  //This is used to compensate, since the code assumes that it always outputs to 16bits
	vol = CLAMP(vol,-128,127);
END_RESAMPLE_INTERFACEMONO()

BEGIN_RESAMPLE_INTERFACE(ResampleMono16BitFirFilter, signed short, 1)
	SNDMIX_GETMONOVOL16FIRFILTER
	vol = CLAMP(vol,-32768,32767);
END_RESAMPLE_INTERFACEMONO()

BEGIN_RESAMPLE_INTERFACE(ResampleStereo8BitFirFilter, signed char, 2)
	SNDMIX_GETSTEREOVOL8FIRFILTER
	vol_l  >>= (WFIR_16BITSHIFT-WFIR_8SHIFT);  //This is used to compensate, since the code assumes that it always outputs to 16bits
	vol_r  >>= (WFIR_16BITSHIFT-WFIR_8SHIFT);  //This is used to compensate, since the code assumes that it always outputs to 16bits
	vol_l = CLAMP(vol_l,-128,127);
	vol_r = CLAMP(vol_r,-128,127);
END_RESAMPLE_INTERFACESTEREO()

BEGIN_RESAMPLE_INTERFACE(ResampleStereo16BitFirFilter, signed short, 2)
	SNDMIX_GETSTEREOVOL16FIRFILTER
	vol_l = CLAMP(vol_l,-32768,32767);
	vol_r = CLAMP(vol_r,-32768,32767);
END_RESAMPLE_INTERFACESTEREO()



/////////////////////////////////////////////////////////////////////////////////////
//
// Mix function tables
//
//
// Index is as follow:
//      [b1-b0] format (8-bit-mono, 16-bit-mono, 8-bit-stereo, 16-bit-stereo)
//      [b2]    ramp
//      [b3]    filter
//      [b5-b4] src type
//
#define MIXNDX_16BIT        0x01
#define MIXNDX_STEREO       0x02
#define MIXNDX_RAMP         0x04
#define MIXNDX_FILTER       0x08
#define MIXNDX_LINEARSRC    0x10
#define MIXNDX_SPLINESRC    0x20
#define MIXNDX_FIRSRC       0x30


// mix_(bits)(m/s)[_filt]_(interp/spline/fir/whatever)[_ramp]
static const mix_interface_t mix_functions[2 * 2 * 16] = {
	// No SRC
	Mono8BitMix,                        Mono16BitMix,
	Stereo8BitMix,                      Stereo16BitMix,
	Mono8BitRampMix,                    Mono16BitRampMix,
	Stereo8BitRampMix,                  Stereo16BitRampMix,

	// No SRC, Filter
	FilterMono8BitMix,                  FilterMono16BitMix,
	FilterStereo8BitMix,                FilterStereo16BitMix,
	FilterMono8BitRampMix,              FilterMono16BitRampMix,
	FilterStereo8BitRampMix,            FilterStereo16BitRampMix,

	// Linear SRC
	Mono8BitLinearMix,                  Mono16BitLinearMix,
	Stereo8BitLinearMix,                Stereo16BitLinearMix,
	Mono8BitLinearRampMix,              Mono16BitLinearRampMix,
	Stereo8BitLinearRampMix,            Stereo16BitLinearRampMix,

	// Linear SRC, Filter
	FilterMono8BitLinearMix,            FilterMono16BitLinearMix,
	FilterStereo8BitLinearMix,          FilterStereo16BitLinearMix,
	FilterMono8BitLinearRampMix,        FilterMono16BitLinearRampMix,
	FilterStereo8BitLinearRampMix,      FilterStereo16BitLinearRampMix,

	// Spline SRC
	Mono8BitSplineMix,                  Mono16BitSplineMix,
	Stereo8BitSplineMix,                Stereo16BitSplineMix,
	Mono8BitSplineRampMix,              Mono16BitSplineRampMix,
	Stereo8BitSplineRampMix,            Stereo16BitSplineRampMix,

	// Spline SRC, Filter
	FilterMono8BitSplineMix,            FilterMono16BitSplineMix,
	FilterStereo8BitSplineMix,          FilterStereo16BitSplineMix,
	FilterMono8BitSplineRampMix,        FilterMono16BitSplineRampMix,
	FilterStereo8BitSplineRampMix,      FilterStereo16BitSplineRampMix,

	// FirFilter  SRC
	Mono8BitFirFilterMix,               Mono16BitFirFilterMix,
	Stereo8BitFirFilterMix,             Stereo16BitFirFilterMix,
	Mono8BitFirFilterRampMix,           Mono16BitFirFilterRampMix,
	Stereo8BitFirFilterRampMix,         Stereo16BitFirFilterRampMix,

	// FirFilter  SRC, Filter
	FilterMono8BitFirFilterMix,         FilterMono16BitFirFilterMix,
	FilterStereo8BitFirFilterMix,       FilterStereo16BitFirFilterMix,
	FilterMono8BitFirFilterRampMix,     FilterMono16BitFirFilterRampMix,
	FilterStereo8BitFirFilterRampMix,   FilterStereo16BitFirFilterRampMix
};


static const mix_interface_t fastmix_functions[2 * 2 * 16] = {
	// No SRC
	FastMono8BitMix,                    FastMono16BitMix,
	Stereo8BitMix,                      Stereo16BitMix,
	FastMono8BitRampMix,                FastMono16BitRampMix,
	Stereo8BitRampMix,                  Stereo16BitRampMix,

	// No SRC, Filter
	FilterMono8BitMix,                  FilterMono16BitMix,
	FilterStereo8BitMix,                FilterStereo16BitMix,
	FilterMono8BitRampMix,              FilterMono16BitRampMix,
	FilterStereo8BitRampMix,            FilterStereo16BitRampMix,

	// Linear SRC
	FastMono8BitLinearMix,              FastMono16BitLinearMix,
	Stereo8BitLinearMix,                Stereo16BitLinearMix,
	FastMono8BitLinearRampMix,          FastMono16BitLinearRampMix,
	Stereo8BitLinearRampMix,            Stereo16BitLinearRampMix,

	// Linear SRC, Filter
	FilterMono8BitLinearMix,            FilterMono16BitLinearMix,
	FilterStereo8BitLinearMix,          FilterStereo16BitLinearMix,
	FilterMono8BitLinearRampMix,        FilterMono16BitLinearRampMix,
	FilterStereo8BitLinearRampMix,      FilterStereo16BitLinearRampMix,

	// Spline SRC
	FastMono8BitSplineMix,              FastMono16BitSplineMix,
	Stereo8BitSplineMix,                Stereo16BitSplineMix,
	FastMono8BitSplineRampMix,          FastMono16BitSplineRampMix,
	Stereo8BitSplineRampMix,            Stereo16BitSplineRampMix,

	// Spline SRC, Filter
	FilterMono8BitSplineMix,            FilterMono16BitSplineMix,
	FilterStereo8BitSplineMix,          FilterStereo16BitSplineMix,
	FilterMono8BitSplineRampMix,        FilterMono16BitSplineRampMix,
	FilterStereo8BitSplineRampMix,      FilterStereo16BitSplineRampMix,

	// FirFilter SRC
	FastMono8BitFirFilterMix,           FastMono16BitFirFilterMix,
	Stereo8BitFirFilterMix,             Stereo16BitFirFilterMix,
	FastMono8BitFirFilterRampMix,       FastMono16BitFirFilterRampMix,
	Stereo8BitFirFilterRampMix,         Stereo16BitFirFilterRampMix,

	// FirFilter SRC, Filter
	FilterMono8BitFirFilterMix,         FilterMono16BitFirFilterMix,
	FilterStereo8BitFirFilterMix,       FilterStereo16BitFirFilterMix,
	FilterMono8BitFirFilterRampMix,     FilterMono16BitFirFilterRampMix,
	FilterStereo8BitFirFilterRampMix,   FilterStereo16BitFirFilterRampMix,
};


static int get_sample_count(song_voice_t *chan, int samples)
{
	int loop_start = (chan->flags & CHN_LOOP) ? chan->loop_start : 0;
	int increment = chan->increment;

	if (samples <= 0 || !increment || !chan->length)
		return 0;

	// Under zero ?
	if ((int) chan->position < loop_start) {
		if (increment < 0) {
			// Invert loop for bidi loops
			int delta = ((loop_start - chan->position) << 16) - (chan->position_frac & 0xFFFF);
			chan->position = loop_start + (delta >> 16);
			chan->position_frac = delta & 0xFFFF;

			if ((int) chan->position < loop_start ||
				chan->position >= (loop_start + chan->length) / 2) {
				chan->position = loop_start;
				chan->position_frac = 0;
			}

			increment = -increment;
			chan->increment = increment;
			// go forward
			chan->flags &= ~(CHN_PINGPONGFLAG);

			if ((!(chan->flags & CHN_LOOP)) ||
			    (chan->position >= chan->length)) {
				chan->position = chan->length;
				chan->position_frac = 0;
				return 0;
			}
		}
		else {
			// We probably didn't hit the loop end yet (first loop), so we do nothing
			if ((int) chan->position < 0)
				chan->position = 0;
		}
	}
	// Past the end
	else if (chan->position >= chan->length) {
		// not looping -> stop this channel
		if (!(chan->flags & CHN_LOOP))
			return 0;

		if (chan->flags & CHN_PINGPONGLOOP) {
			// Invert loop
			if (increment > 0) {
				increment = -increment;
				chan->increment = increment;
			}

			chan->flags |= CHN_PINGPONGFLAG;
			// adjust loop position
			uint64_t overshoot = (uint64_t)((chan->position - chan->length) << 16) + chan->position_frac;
			uint64_t loop_length = (uint64_t)(chan->loop_end - chan->loop_start - PINGPONG_OFFSET) << 16;
			if (overshoot < loop_length) {
				uint64_t new_position = ((uint64_t)(chan->length - PINGPONG_OFFSET) << 16) - overshoot;
				chan->position = (uint32_t)(new_position >> 16);
				chan->position_frac = (uint32_t)(new_position & 0xFFFF);
			}
			else {
				chan->position = chan->loop_start; /* not 100% accurate, but only matters for extremely small loops played at extremely high frequencies */
				chan->position_frac = 0;
			}
		}
		else {
			// This is a bug
			if (increment < 0) {
				increment = -increment;
				chan->increment = increment;
			}

			// Restart at loop start
			chan->position += loop_start - chan->length;

			if ((int) chan->position < loop_start)
				chan->position = chan->loop_start;
		}
	}

	int position = chan->position;

	// too big increment, and/or too small loop length
	if (position < loop_start) {
		if (position < 0 || increment < 0)
			return 0;
	}

	if (position < 0 || position >= (int) chan->length)
		return 0;

	int position_frac = (unsigned short) chan->position_frac,
		 sample_count = samples;

	if (increment < 0) {
		int inv = -increment;
		int maxsamples = 16384 / ((inv >> 16) + 1);

		if (maxsamples < 2)
			maxsamples = 2;

		if (samples > maxsamples)
			samples = maxsamples;

		int delta_hi = (inv >> 16) * (samples - 1);
		int delta_lo = (inv & 0xffff) * (samples - 1);
		int pos_dest = position - delta_hi + ((position_frac - delta_lo) >> 16);

		if (pos_dest < loop_start) {
			sample_count =
				(unsigned int) (((((long long) position -
					loop_start) << 16) + position_frac -
					  1) / inv) + 1;
		}
	}
	else {
		int maxsamples = 16384 / ((increment >> 16) + 1);

		if (maxsamples < 2)
			maxsamples = 2;

		if (samples > maxsamples)
			samples = maxsamples;

		int delta_hi = (increment >> 16) * (samples - 1);
		int delta_lo = (increment & 0xffff) * (samples - 1);
		int pos_dest = position + delta_hi + ((position_frac + delta_lo) >> 16);

		if (pos_dest >= (int) chan->length) {
			sample_count = (unsigned int)
				(((((long long) chan->length - position) << 16) - position_frac - 1) / increment) + 1;
		}
	}

	if (sample_count <= 1)
		return 1;
	else if (sample_count > samples)
		return samples;

	return sample_count;
}


unsigned int csf_create_stereo_mix(song_t *csf, int count)
{
	int* ofsl, *ofsr;
	unsigned int nchused, nchmixed;

	if (!count)
		return 0;

	nchused = nchmixed = 0;

	// yuck
	if (csf->multi_write)
		for (unsigned int nchan = 0; nchan < MAX_CHANNELS; nchan++)
			memset(csf->multi_write[nchan].buffer, 0, sizeof(csf->multi_write[nchan].buffer));

	for (unsigned int nchan = 0; nchan < csf->num_voices; nchan++) {
		const mix_interface_t *mix_func_table;
		song_voice_t *const channel = &csf->voices[csf->voice_mix[nchan]];
		unsigned int flags;
		unsigned int nrampsamples;
		int smpcount;
		int nsamples;
		int *pbuffer;

		if (!channel->current_sample_data)
			continue;

		ofsr = &g_dry_rofs_vol;
		ofsl = &g_dry_lofs_vol;
		flags = 0;

		if (channel->flags & CHN_16BIT)
			flags |= MIXNDX_16BIT;

		if (channel->flags & CHN_STEREO)
			flags |= MIXNDX_STEREO;

		if (channel->flags & CHN_FILTER)
			flags |= MIXNDX_FILTER;

		if (!(channel->flags & CHN_NOIDO) &&
		    !(csf->mix_flags & SNDMIX_NORESAMPLING)) {
			// use hq-fir mixer?
			if ((csf->mix_flags & (SNDMIX_HQRESAMPLER | SNDMIX_ULTRAHQSRCMODE))
						== (SNDMIX_HQRESAMPLER | SNDMIX_ULTRAHQSRCMODE))
				flags |= MIXNDX_FIRSRC;
			else if (csf->mix_flags & SNDMIX_HQRESAMPLER)
				flags |= MIXNDX_SPLINESRC;
			else
				flags |= MIXNDX_LINEARSRC;    // use
		}

		if ((flags < 0x40) &&
			(channel->left_volume == channel->right_volume) &&
			((!channel->ramp_length) ||
			(channel->left_ramp == channel->right_ramp))) {
			mix_func_table = fastmix_functions;
		} else {
			mix_func_table = mix_functions;
		}

		nsamples = count;

		if (csf->multi_write) {
			int master = (csf->voice_mix[nchan] < MAX_CHANNELS)
				? csf->voice_mix[nchan]
				: (channel->master_channel - 1);
			pbuffer = csf->multi_write[master].buffer;
			csf->multi_write[master].used = 1;
		} else {
			pbuffer = csf->mix_buffer;
		}

		nchused++;
		////////////////////////////////////////////////////
		unsigned int naddmix = 0;

		do {
			nrampsamples = nsamples;

			if (channel->ramp_length > 0) {
				if ((int) nrampsamples > channel->ramp_length)
					nrampsamples = channel->ramp_length;
			}

			smpcount = 1;

			/* Figure out the number of remaining samples,
			 * unless we're in AdLib or MIDI mode (to prevent
			 * artificial KeyOffs)
			 */
			if (!(channel->flags & CHN_ADLIB)) {
				smpcount = get_sample_count(channel, nrampsamples);
			}

			if (smpcount <= 0) {
				// Stopping the channel
				channel->current_sample_data = NULL;
				channel->length = 0;
				channel->position = 0;
				channel->position_frac = 0;
				channel->ramp_length = 0;
				end_channel_ofs(channel, pbuffer, nsamples);
				*ofsr += channel->rofs;
				*ofsl += channel->lofs;
				channel->rofs = channel->lofs = 0;
				channel->flags &= ~CHN_PINGPONGFLAG;
				break;
			}

			// Should we mix this channel ?

			if ((nchmixed >= max_voices && !(csf->mix_flags & SNDMIX_DIRECTTODISK))
			    || (!channel->ramp_length && !(channel->left_volume | channel->right_volume))) {
				int delta = (channel->increment * (int) smpcount) + (int) channel->position_frac;
				channel->position_frac = delta & 0xFFFF;
				channel->position += (delta >> 16);
				channel->rofs = channel->lofs = 0;
				pbuffer += smpcount * 2;
			} else {
				// Do mixing

				/* Mix the stream, unless we're in AdLib mode */
				if (!(channel->flags & CHN_ADLIB)) {
					// Choose function for mixing
					mix_interface_t mix_func;
					mix_func = channel->ramp_length
						? mix_func_table[flags | MIXNDX_RAMP]
						: mix_func_table[flags];
					int *pbufmax = pbuffer + (smpcount * 2);
					channel->rofs = -*(pbufmax - 2);
					channel->lofs = -*(pbufmax - 1);

					mix_func(channel, pbuffer, pbufmax);
					channel->rofs += *(pbufmax - 2);
					channel->lofs += *(pbufmax - 1);
					pbuffer = pbufmax;
					naddmix = 1;
				}
			}

			nsamples -= smpcount;

			if (channel->ramp_length) {
				channel->ramp_length -= smpcount;
				if (channel->ramp_length <= 0) {
					channel->ramp_length = 0;
					channel->right_volume = channel->right_volume_new;
					channel->left_volume = channel->left_volume_new;
					channel->right_ramp = channel->left_ramp = 0;

					if ((channel->flags & CHN_NOTEFADE)
						&& (!(channel->fadeout_volume))) {
						channel->length = 0;
						channel->current_sample_data = NULL;
					}
				}
			}

		} while (nsamples > 0);

		nchmixed += naddmix;
	}

	GM_IncrementSongCounter(count);

	if (csf->multi_write) {
		/* mix all adlib onto track one */
		Fmdrv_MixTo(csf->multi_write[0].buffer, count);
	} else {
		Fmdrv_MixTo(csf->mix_buffer, count);
	}

	return nchused;
}
