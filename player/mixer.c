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

#include "player/sndfile.h"
#include "player/snd_fm.h"
#include "player/snd_gm.h"
#include "player/cmixer.h"
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
 * MUST also regenerate the arrays. */

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
#define WFIR_16SHIFT         (WFIR_QUANTBITS)

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

#define SPLINE_FRACSHIFT ((16 - SPLINE_FRACBITS) - 2)
#define SPLINE_FRACMASK  (((1L << (16 - SPLINE_FRACSHIFT)) - 1) & ~3)

#define WFIR_FRACSHIFT (16 - (WFIR_FRACBITS + 1 + WFIR_LOG2WIDTH))
#define WFIR_FRACMASK  ((((1L << (17 - WFIR_FRACSHIFT)) - 1) & ~((1L << WFIR_LOG2WIDTH) - 1)))
#define WFIR_FRACHALVE (1L << (16 - (WFIR_FRACBITS + 2)))

#include "player/precomp_lut.h"

/* FIXME: This has lots of undefined behavior (!!) in the form of bit shifts on
 * signed integers... need to look over each variable and find out whether it
 * needs to be signed or unsigned. */

// ----------------------------------------------------------------------------
// MIXING MACROS
// ----------------------------------------------------------------------------


#define SNDMIX_BEGINSAMPLELOOP(bits) \
	register song_voice_t * const chan = channel; \
	position = chan->position_frac; \
	const int##bits##_t *p = (int##bits##_t *)(chan->current_sample_data + (chan->position * (bits / 8))); \
	if (chan->flags & CHN_STEREO) p += chan->position; \
	int *pvol = pbuffer;\
	do {


#define SNDMIX_ENDSAMPLELOOP \
		position += chan->increment; \
	} while (pvol < pbufmax); \
	chan->position  += position >> 16; \
	chan->position_frac = position & 0xFFFF;

//////////////////////////////////////////////////////////////////////////////
// Mono

// No interpolation
#define SNDMIX_GETMONOVOLNOIDO(bits) \
	int vol = p[position >> 16] << (-bits + 16);

// Linear Interpolation
#define SNDMIX_GETMONOVOLLINEAR(bits) \
	int poshi   = position >> 16; \
	int poslo   = (position >> (bits / 2)) & 0xFF; \
	int srcvol  = p[poshi]; \
	int destvol = p[poshi + 1]; \
	int vol     = (srcvol << (-bits + 16)) + ((int)(poslo * (destvol - srcvol)) >> (bits - 8));

// spline interpolation (2 guard bits should be enough???)
#define SNDMIX_GETMONOVOLSPLINE(bits) \
	int poshi = position >> 16; \
	int poslo = (position >> SPLINE_FRACSHIFT) & SPLINE_FRACMASK; \
	int vol   = (cubic_spline_lut[poslo    ] * (int)p[poshi - 1] + \
		 cubic_spline_lut[poslo + 1] * (int)p[poshi    ] + \
		 cubic_spline_lut[poslo + 3] * (int)p[poshi + 2] + \
		 cubic_spline_lut[poslo + 2] * (int)p[poshi + 1]) >> SPLINE_##bits##SHIFT;

// fir interpolation
#define SNDMIX_GETMONOVOLFIRFILTER(bits) \
	int poshi  = position >> 16; \
	int poslo  = (position & 0xFFFF); \
	int firidx = ((poslo + WFIR_FRACHALVE) >> WFIR_FRACSHIFT) & WFIR_FRACMASK; \
	int vol1   = (windowed_fir_lut[firidx + 0] * (int)p[poshi + 1 - 4]); \
	vol1  += (windowed_fir_lut[firidx + 1] * (int)p[poshi + 2 - 4]); \
	vol1  += (windowed_fir_lut[firidx + 2] * (int)p[poshi + 3 - 4]); \
	vol1  += (windowed_fir_lut[firidx + 3] * (int)p[poshi + 4 - 4]); \
	int vol2   = (windowed_fir_lut[firidx + 4] * (int)p[poshi + 5 - 4]); \
	vol2  += (windowed_fir_lut[firidx + 5] * (int)p[poshi + 6 - 4]); \
	vol2  += (windowed_fir_lut[firidx + 6] * (int)p[poshi + 7 - 4]); \
	vol2  += (windowed_fir_lut[firidx + 7] * (int)p[poshi + 8 - 4]); \
	int vol    = ((vol1 >> 1) + (vol2 >> 1)) >> (WFIR_##bits##SHIFT - 1);

/////////////////////////////////////////////////////////////////////////////
// Stereo

#define SNDMIX_GETSTEREOVOLNOIDO(bits) \
	int vol_l = p[(position >> 16) * 2    ] << (-bits + 16); \
	int vol_r = p[(position >> 16) * 2 + 1] << (-bits + 16);

#define SNDMIX_GETSTEREOVOLLINEAR(bits) \
	int poshi    = position >> 16; \
	int poslo    = (position >> 8) & 0xFF; \
	int srcvol_l = p[poshi * 2]; \
	int vol_l    = (srcvol_l << (-bits + 16)) + ((int)(poslo * (p[poshi * 2 + 2] - srcvol_l)) >> (bits - 8));\
	int srcvol_r = p[poshi * 2 + 1];\
	int vol_r    = (srcvol_r << (-bits + 16)) + ((int)(poslo * (p[poshi * 2 + 3] - srcvol_r)) >> (bits - 8));

// Spline Interpolation
#define SNDMIX_GETSTEREOVOLSPLINE(bits) \
	int poshi   = position >> 16; \
	int poslo   = (position >> SPLINE_FRACSHIFT) & SPLINE_FRACMASK; \
	int vol_l   = (cubic_spline_lut[poslo    ] * (int)p[(poshi - 1) * 2   ] + \
		   cubic_spline_lut[poslo + 1] * (int)p[(poshi    ) * 2   ] + \
		   cubic_spline_lut[poslo + 2] * (int)p[(poshi + 1) * 2   ] + \
		   cubic_spline_lut[poslo + 3] * (int)p[(poshi + 2) * 2   ]) >> SPLINE_##bits##SHIFT; \
	int vol_r   = (cubic_spline_lut[poslo    ] * (int)p[(poshi - 1) * 2 + 1] + \
		   cubic_spline_lut[poslo + 1] * (int)p[(poshi    ) * 2 + 1] + \
		   cubic_spline_lut[poslo + 2] * (int)p[(poshi + 1) * 2 + 1] + \
		   cubic_spline_lut[poslo + 3] * (int)p[(poshi + 2) * 2 + 1]) >> SPLINE_##bits##SHIFT;

// fir interpolation
#define SNDMIX_GETSTEREOVOLFIRFILTER(bits) \
	int poshi   = position >> 16; \
	int poslo   = (position & 0xFFFF); \
	int firidx  = ((poslo + WFIR_FRACHALVE) >> WFIR_FRACSHIFT) & WFIR_FRACMASK; \
	int vol1_l  = (windowed_fir_lut[firidx + 0] * (int)p[(poshi + 1 - 4) * 2]);    \
	vol1_l += (windowed_fir_lut[firidx + 1] * (int)p[(poshi + 2 - 4) * 2]);    \
	vol1_l += (windowed_fir_lut[firidx + 2] * (int)p[(poshi + 3 - 4) * 2]);    \
	vol1_l += (windowed_fir_lut[firidx + 3] * (int)p[(poshi + 4 - 4) * 2]);    \
	int vol2_l  = (windowed_fir_lut[firidx + 4] * (int)p[(poshi + 5 - 4) * 2]);    \
	vol2_l += (windowed_fir_lut[firidx + 5] * (int)p[(poshi + 6 - 4) * 2]);    \
	vol2_l += (windowed_fir_lut[firidx + 6] * (int)p[(poshi + 7 - 4) * 2]);    \
	vol2_l += (windowed_fir_lut[firidx + 7] * (int)p[(poshi + 8 - 4) * 2]);    \
	int vol_l   = ((vol1_l >> 1) + (vol2_l >> 1)) >> (WFIR_ ## bits ## SHIFT - 1); \
	int vol1_r  = (windowed_fir_lut[firidx + 0] * (int)p[(poshi + 1 - 4) * 2 + 1]);    \
	vol1_r += (windowed_fir_lut[firidx + 1] * (int)p[(poshi + 2 - 4) * 2 + 1]);    \
	vol1_r += (windowed_fir_lut[firidx + 2] * (int)p[(poshi + 3 - 4) * 2 + 1]);    \
	vol1_r += (windowed_fir_lut[firidx + 3] * (int)p[(poshi + 4 - 4) * 2 + 1]);    \
	int vol2_r  = (windowed_fir_lut[firidx + 4] * (int)p[(poshi + 5 - 4) * 2 + 1]);    \
	vol2_r += (windowed_fir_lut[firidx + 5] * (int)p[(poshi + 6 - 4) * 2 + 1]);    \
	vol2_r += (windowed_fir_lut[firidx + 6] * (int)p[(poshi + 7 - 4) * 2 + 1]);    \
	vol2_r += (windowed_fir_lut[firidx + 7] * (int)p[(poshi + 8 - 4) * 2 + 1]);    \
	int vol_r   = ((vol1_r >> 1) + (vol2_r >> 1)) >> (WFIR_ ## bits ## SHIFT - 1);

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

#define MUL_32_TO_64(x, y) ((int64_t)(x) * (y))
#define FILT_CLIP(i) CLAMP(i, -65536, 65534)

#define MIX_BEGIN_FILTER(chn) \
	int32_t fy##chn##1 = channel->filter_y[chn][0]; \
	int32_t fy##chn##2 = channel->filter_y[chn][1]; \
	int32_t t##chn;

#define SNDMIX_PROCESSFILTER(outchn, volume) \
	t##outchn = (MUL_32_TO_64(volume, chan->filter_a0) + MUL_32_TO_64(FILT_CLIP(fy##outchn##1), chan->filter_b0) + MUL_32_TO_64(FILT_CLIP(fy##outchn##2), chan->filter_b1) \
	+ (1 << (FILTERPRECISION - 1))) >> FILTERPRECISION; \
	fy##outchn##2 = fy##outchn##1; fy##outchn##1 = t##outchn; volume = t##outchn;

#define MIX_END_FILTER(chn) \
	channel->filter_y[chn][0] = fy##chn##1; \
	channel->filter_y[chn][1] = fy##chn##2;

// aliases
#define MIX_BEGIN_MONO_FILTER MIX_BEGIN_FILTER(0)
#define MIX_END_MONO_FILTER MIX_END_FILTER(0)
#define SNDMIX_PROCESSMONOFILTER SNDMIX_PROCESSFILTER(0, vol)

#define MIX_BEGIN_STEREO_FILTER MIX_BEGIN_FILTER(0) MIX_BEGIN_FILTER(1)
#define MIX_END_STEREO_FILTER MIX_END_FILTER(0) MIX_END_FILTER(1)
#define SNDMIX_PROCESSSTEREOFILTER SNDMIX_PROCESSFILTER(0, vol_l) SNDMIX_PROCESSFILTER(1, vol_r)

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

/* aliases here */
#define BEGIN_FASTMIX_INTERFACE(func) BEGIN_MIX_INTERFACE(func)
#define END_FASTMIX_INTERFACE(func) END_MIX_INTERFACE(func)

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
#define BEGIN_MIX_MONO_FLT_INTERFACE(func) \
	BEGIN_MIX_INTERFACE(func) \
	MIX_BEGIN_MONO_FILTER


#define END_MIX_MONO_FLT_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
	MIX_END_MONO_FILTER \
	}


#define BEGIN_RAMPMIX_MONO_FLT_INTERFACE(func) \
	BEGIN_MIX_INTERFACE(func) \
	int right_ramp_volume = channel->right_ramp_volume; \
	int left_ramp_volume  = channel->left_ramp_volume; \
	MIX_BEGIN_MONO_FILTER


#define END_RAMPMIX_MONO_FLT_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
	MIX_END_MONO_FILTER \
	channel->right_ramp_volume = right_ramp_volume; \
	channel->right_volume     = right_ramp_volume >> VOLUMERAMPPRECISION; \
	channel->left_ramp_volume  = left_ramp_volume; \
	channel->left_volume      = left_ramp_volume >> VOLUMERAMPPRECISION; \
	}


// Stereo Resonant Filters
#define BEGIN_MIX_STEREO_FLT_INTERFACE(func) \
	BEGIN_MIX_INTERFACE(func) \
	MIX_BEGIN_STEREO_FILTER


#define END_MIX_STEREO_FLT_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
	MIX_END_STEREO_FILTER \
	}


#define BEGIN_RAMPMIX_STEREO_FLT_INTERFACE(func) \
	BEGIN_MIX_INTERFACE(func) \
	int right_ramp_volume = channel->right_ramp_volume; \
	int left_ramp_volume  = channel->left_ramp_volume; \
	MIX_BEGIN_STEREO_FILTER


#define END_RAMPMIX_STEREO_FLT_INTERFACE() \
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

#define END_RESAMPLE_INTERFACE_MONO() \
		*pvol = vol; \
		pvol++; \
		position += increment; \
	} while (pvol < pbufmax); \
	}

#define END_RESAMPLE_INTERFACE_STEREO() \
		pvol[0] = vol_l; \
		pvol[1] = vol_r; \
		pvol += 2; \
		position += increment; \
	} while (pvol < pbufmax); \
	}

/* --------------------------------------------------------------------------- */
/* generate processing functions */

/* This is really just a diet version of C++'s templates. */
#define DEFINE_MIX_INTERFACE_ALL(bits, chns, chnsupper, resampling, resampupper, fast, fastupper, filter, fltnam, fltint, ramp, rampupper, rmpint) \
	BEGIN_ ## fastupper ## rmpint ## MIX_ ## fltint ## INTERFACE(fast ## fltnam ## chns ## bits ## Bit ## resampling ## ramp ## Mix) \
		SNDMIX_BEGINSAMPLELOOP(bits) \
		SNDMIX_GET ## chnsupper ## VOL ## resampupper(bits) \
		filter \
		SNDMIX_ ## rampupper ## fastupper ## chnsupper ## VOL \
	END_ ## fastupper ## rmpint ## MIX_ ## fltint ## INTERFACE()

/* defines all ramping variations */
#define DEFINE_MIX_INTERFACE_RAMP(bits, chns, chnsupper, filter, fltnam, fltint, fast, fastupper, resampling, resampupper) \
	DEFINE_MIX_INTERFACE_ALL(bits, chns, chnsupper, resampling, resampupper, fast, fastupper, filter, fltnam, fltint, /* none */, STORE, /* none */) \
	DEFINE_MIX_INTERFACE_ALL(bits, chns, chnsupper, resampling, resampupper, fast, fastupper, filter, fltnam, fltint, Ramp,       RAMP,  RAMP)

/* defines all resampling variations */
#define DEFINE_MIX_INTERFACE_RESAMPLING(bits, chns, chnsupper, filter, fltnam, fltint, fast, fastupper) \
	DEFINE_MIX_INTERFACE_RAMP(bits, chns, chnsupper, filter, fltnam, fltint, fast, fastupper, /* none */, NOIDO) \
	DEFINE_MIX_INTERFACE_RAMP(bits, chns, chnsupper, filter, fltnam, fltint, fast, fastupper, Linear,     LINEAR) \
	DEFINE_MIX_INTERFACE_RAMP(bits, chns, chnsupper, filter, fltnam, fltint, fast, fastupper, Spline,     SPLINE) \
	DEFINE_MIX_INTERFACE_RAMP(bits, chns, chnsupper, filter, fltnam, fltint, fast, fastupper, FirFilter,  FIRFILTER)

/* defines filter + no-filter variants */
#define DEFINE_MIX_INTERFACE(bits) \
	DEFINE_MIX_INTERFACE_RESAMPLING(bits, Mono,   MONO,   /* none */, /* none */, /* none */, /* none */, /* none */) \
	DEFINE_MIX_INTERFACE_RESAMPLING(bits, Mono,   MONO,   SNDMIX_PROCESSMONOFILTER,   Filter, MONO_FLT_, /* none */, /* none */) \
	DEFINE_MIX_INTERFACE_RESAMPLING(bits, Stereo, STEREO, /* none */, /* none */, /* none */, /* none */, /* none */) \
	DEFINE_MIX_INTERFACE_RESAMPLING(bits, Stereo, STEREO, SNDMIX_PROCESSSTEREOFILTER, Filter, STEREO_FLT_, /* none */, /* none */)

/* defines "fast" interfaces; no-filter + mono only */
#define DEFINE_MIX_INTERFACE_FAST(bits) \
	DEFINE_MIX_INTERFACE_RESAMPLING(bits, Mono, MONO, /* none */, /* none */, /* none */, Fast, FAST)

DEFINE_MIX_INTERFACE_FAST(8)
DEFINE_MIX_INTERFACE_FAST(16)

DEFINE_MIX_INTERFACE(8)
DEFINE_MIX_INTERFACE(16)

// Public Resampling Methods
#define DEFINE_MONO_RESAMPLE_INTERFACE(bits) \
	BEGIN_RESAMPLE_INTERFACE(ResampleMono##bits##BitFirFilter, int##bits##_t, 1) \
		SNDMIX_GETMONOVOLFIRFILTER(bits) \
		vol  >>= (WFIR_16SHIFT-WFIR_##bits##SHIFT);  /* This is used to compensate, since the code assumes that it always outputs to 16bits */ \
		vol = CLAMP(vol, INT ## bits ## _MIN, INT ## bits ## _MIN); \
	END_RESAMPLE_INTERFACE_MONO()

#define DEFINE_STEREO_RESAMPLE_INTERFACE(bits) \
	BEGIN_RESAMPLE_INTERFACE(ResampleStereo##bits##BitFirFilter, int##bits##_t, 2) \
		SNDMIX_GETSTEREOVOLFIRFILTER(bits) \
		vol_l  >>= (WFIR_16SHIFT-WFIR_##bits##SHIFT);  /* This is used to compensate, since the code assumes that it always outputs to 16bits */ \
		vol_r  >>= (WFIR_16SHIFT-WFIR_##bits##SHIFT);  /* This is used to compensate, since the code assumes that it always outputs to 16bits */ \
		vol_l = CLAMP(vol_l, INT ## bits ## _MIN, INT ## bits ## _MIN); \
		vol_r = CLAMP(vol_r, INT ## bits ## _MIN, INT ## bits ## _MIN); \
	END_RESAMPLE_INTERFACE_STEREO()

DEFINE_MONO_RESAMPLE_INTERFACE(8)
DEFINE_MONO_RESAMPLE_INTERFACE(16)

DEFINE_STEREO_RESAMPLE_INTERFACE(8)
DEFINE_STEREO_RESAMPLE_INTERFACE(16)

/////////////////////////////////////////////////////////////////////////////////////
//
// Mix function tables
//
//
// Index is as follows:
//      [b1-b0] format (8-bit-mono, 16-bit-mono, 8-bit-stereo, 16-bit-stereo)
//      [b2]    ramp
//      [b3]    filter
//      [b5-b4] src type

#define MIXNDX_16BIT        0x01
#define MIXNDX_STEREO       0x02
#define MIXNDX_RAMP         0x04
#define MIXNDX_FILTER       0x08
#define MIXNDX_LINEARSRC    0x10
#define MIXNDX_SPLINESRC    0x20
#define MIXNDX_FIRSRC       0x30

#define BUILD_MIX_FUNCTION_TABLE_RAMP(fast, resampling, filter, ramp) \
	fast##filter##Mono8Bit##resampling##ramp##Mix, \
	fast##filter##Mono16Bit##resampling##ramp##Mix, \
	filter##Stereo8Bit##resampling##ramp##Mix, \
	filter##Stereo16Bit##resampling##ramp##Mix,

#define BUILD_MIX_FUNCTION_TABLE_FILTER(fast, resampling, filter) \
	BUILD_MIX_FUNCTION_TABLE_RAMP(fast, resampling, filter, /* none */) \
	BUILD_MIX_FUNCTION_TABLE_RAMP(fast, resampling, filter, Ramp)

/* diverges for regular and fast variations */

/* no fast filter variant at all, fallback to normal */
#define BUILD_MIX_FUNCTION_TABLE_FAST(resampling) \
	BUILD_MIX_FUNCTION_TABLE_FILTER(Fast, resampling, /* none */) \
	BUILD_MIX_FUNCTION_TABLE_FILTER(/* none */, resampling, Filter)

#define BUILD_MIX_FUNCTION_TABLE(resampling) \
	BUILD_MIX_FUNCTION_TABLE_FILTER(/* none */, resampling, /* none */) \
	BUILD_MIX_FUNCTION_TABLE_FILTER(/* none */, resampling, Filter)

// mix_(bits)(m/s)[_filt]_(interp/spline/fir/whatever)[_ramp]
static const mix_interface_t mix_functions[2 * 2 * 16] = {
	BUILD_MIX_FUNCTION_TABLE(/* none */)
	BUILD_MIX_FUNCTION_TABLE(Linear)
	BUILD_MIX_FUNCTION_TABLE(Spline)
	BUILD_MIX_FUNCTION_TABLE(FirFilter)
};

static const mix_interface_t fastmix_functions[2 * 2 * 16] = {
	BUILD_MIX_FUNCTION_TABLE_FAST(/* none */)
	BUILD_MIX_FUNCTION_TABLE_FAST(Linear)
	BUILD_MIX_FUNCTION_TABLE_FAST(Spline)
	BUILD_MIX_FUNCTION_TABLE_FAST(FirFilter)
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
