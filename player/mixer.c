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
#include "bshift.h"
#include "util.h"   // for CLAMP

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

// ----------------------------------------------------------------------------
// MIXING MACROS
// ----------------------------------------------------------------------------

#define SNDMIX_BEGINSAMPLELOOP(bits) \
	register song_voice_t * const chan = channel; \
	position = chan->position_frac; \
	const int##bits##_t *p = (int##bits##_t *)(chan->current_sample_data) + chan->position; \
	if (chan->flags & CHN_STEREO) p += chan->position; \
	int32_t *pvol = pbuffer; \
	uint32_t max = 0; \
	do {


#define SNDMIX_ENDSAMPLELOOP \
		position += chan->increment; \
	} while (pvol < pbufmax); \
	chan->vu_meter = max >> 16; \
	chan->position  += position >> 16; \
	chan->position_frac = position & 0xFFFF;

//////////////////////////////////////////////////////////////////////////////
// Mono

// No interpolation
#define SNDMIX_GETMONOVOLNOIDO(bits) \
	int32_t vol = lshift_signed(p[position >> 16], -bits + 16);

// Linear Interpolation
#define SNDMIX_GETMONOVOLLINEAR(bits) \
	int32_t poshi  = position >> 16; \
	int32_t poslo   = (position >> 8) & 0xFF; \
	int32_t srcvol  = p[poshi]; \
	int32_t destvol = p[poshi + 1]; \
	int32_t vol     = lshift_signed(srcvol, -bits + 16) + rshift_signed(poslo * (destvol - srcvol), bits - 8);

// spline interpolation (2 guard bits should be enough???)
#define SNDMIX_GETMONOVOLSPLINE(bits) \
	int32_t poshi = position >> 16; \
	int32_t poslo = rshift_signed(position, SPLINE_FRACSHIFT) & SPLINE_FRACMASK; \
	int32_t vol   = rshift_signed( \
		  cubic_spline_lut[poslo + 0] * (int32_t)p[poshi - 1] \
		+ cubic_spline_lut[poslo + 1] * (int32_t)p[poshi + 0] \
		+ cubic_spline_lut[poslo + 2] * (int32_t)p[poshi + 1] \
		+ cubic_spline_lut[poslo + 3] * (int32_t)p[poshi + 2], \
		SPLINE_##bits##SHIFT);

// fir interpolation
#define SNDMIX_GETMONOVOLFIRFILTER(bits) \
	int32_t poshi  = position >> 16; \
	int32_t poslo  = (position & 0xFFFF); \
	int32_t firidx = rshift_signed(poslo + WFIR_FRACHALVE, WFIR_FRACSHIFT) & WFIR_FRACMASK; \
	int32_t vol = rshift_signed( \
		rshift_signed( \
			(windowed_fir_lut[firidx + 0] * (int32_t)p[poshi + 1 - 4]) + \
			(windowed_fir_lut[firidx + 1] * (int32_t)p[poshi + 2 - 4]) + \
			(windowed_fir_lut[firidx + 2] * (int32_t)p[poshi + 3 - 4]) + \
			(windowed_fir_lut[firidx + 3] * (int32_t)p[poshi + 4 - 4]) \
			, 1) + \
		rshift_signed( \
			(windowed_fir_lut[firidx + 4] * (int32_t)p[poshi + 5 - 4]) + \
			(windowed_fir_lut[firidx + 5] * (int32_t)p[poshi + 6 - 4]) + \
			(windowed_fir_lut[firidx + 6] * (int32_t)p[poshi + 7 - 4]) + \
			(windowed_fir_lut[firidx + 7] * (int32_t)p[poshi + 8 - 4]) \
			, 1), \
		WFIR_##bits##SHIFT - 1);

/////////////////////////////////////////////////////////////////////////////
// Stereo

#define SNDMIX_GETSTEREOVOLNOIDO(bits) \
	int32_t vol_l = lshift_signed(p[(position >> 16) * 2 + 0], -bits + 16); \
	int32_t vol_r = lshift_signed(p[(position >> 16) * 2 + 1], -bits + 16);

#define SNDMIX_GETSTEREOVOLLINEAR(bits) \
	int32_t poshi    = position >> 16; \
	int32_t poslo    = (position >> 8) & 0xFF; \
	int32_t srcvol_l = p[poshi * 2 + 0]; \
	int32_t srcvol_r = p[poshi * 2 + 1]; \
	int32_t vol_l    = lshift_signed(srcvol_l, -bits + 16) + rshift_signed(poslo * (p[poshi * 2 + 2] - srcvol_l), bits - 8); \
	int32_t vol_r    = lshift_signed(srcvol_r, -bits + 16) + rshift_signed(poslo * (p[poshi * 2 + 3] - srcvol_r), bits - 8);

// Spline Interpolation
#define SNDMIX_GETSTEREOVOLSPLINE(bits) \
	int32_t poshi   = position >> 16; \
	int32_t poslo   = (position >> SPLINE_FRACSHIFT) & SPLINE_FRACMASK; \
	int32_t vol_l   = rshift_signed( \
			cubic_spline_lut[poslo + 0] * (int32_t)p[(poshi - 1) * 2] + \
			cubic_spline_lut[poslo + 1] * (int32_t)p[(poshi + 0) * 2] + \
			cubic_spline_lut[poslo + 2] * (int32_t)p[(poshi + 1) * 2] + \
			cubic_spline_lut[poslo + 3] * (int32_t)p[(poshi + 2) * 2], \
			SPLINE_##bits##SHIFT); \
	int32_t vol_r   = rshift_signed( \
			cubic_spline_lut[poslo + 0] * (int32_t)p[(poshi - 1) * 2 + 1] + \
			cubic_spline_lut[poslo + 1] * (int32_t)p[(poshi + 0) * 2 + 1] + \
			cubic_spline_lut[poslo + 2] * (int32_t)p[(poshi + 1) * 2 + 1] + \
			cubic_spline_lut[poslo + 3] * (int32_t)p[(poshi + 2) * 2 + 1], \
			SPLINE_##bits##SHIFT);

// fir interpolation
#define SNDMIX_GETSTEREOVOLFIRFILTER(bits) \
	int32_t poshi   = position >> 16; \
	int32_t poslo   = (position & 0xFFFF); \
	int32_t firidx  = rshift_signed(poslo + WFIR_FRACHALVE, WFIR_FRACSHIFT) & WFIR_FRACMASK; \
	int32_t vol_l = rshift_signed( \
		rshift_signed( \
			(windowed_fir_lut[firidx + 0] * p[(poshi + 1 - 4) * 2]) + \
			(windowed_fir_lut[firidx + 1] * p[(poshi + 2 - 4) * 2]) + \
			(windowed_fir_lut[firidx + 2] * p[(poshi + 3 - 4) * 2]) + \
			(windowed_fir_lut[firidx + 3] * p[(poshi + 4 - 4) * 2]) \
			, 1) + \
		rshift_signed( \
			(windowed_fir_lut[firidx + 4] * p[(poshi + 5 - 4) * 2]) + \
			(windowed_fir_lut[firidx + 5] * p[(poshi + 6 - 4) * 2]) + \
			(windowed_fir_lut[firidx + 6] * p[(poshi + 7 - 4) * 2]) + \
			(windowed_fir_lut[firidx + 7] * p[(poshi + 8 - 4) * 2]) \
			, 1), \
		WFIR_##bits##SHIFT - 1); \
	int32_t vol_r = rshift_signed( \
		rshift_signed( \
			(windowed_fir_lut[firidx + 0] * p[(poshi + 1 - 4) * 2 + 1]) + \
			(windowed_fir_lut[firidx + 1] * p[(poshi + 2 - 4) * 2 + 1]) + \
			(windowed_fir_lut[firidx + 2] * p[(poshi + 3 - 4) * 2 + 1]) + \
			(windowed_fir_lut[firidx + 3] * p[(poshi + 4 - 4) * 2 + 1]) \
			, 1) + \
		rshift_signed( \
			(windowed_fir_lut[firidx + 4] * p[(poshi + 5 - 4) * 2 + 1]) + \
			(windowed_fir_lut[firidx + 5] * p[(poshi + 6 - 4) * 2 + 1]) + \
			(windowed_fir_lut[firidx + 6] * p[(poshi + 7 - 4) * 2 + 1]) + \
			(windowed_fir_lut[firidx + 7] * p[(poshi + 8 - 4) * 2 + 1]) \
			, 1), \
		WFIR_##bits##SHIFT - 1);

// FIXME why are these backwards? what?
#define SNDMIX_STOREMONOVOL \
	int32_t vol_lx = vol * chan->right_volume; \
	int32_t vol_rx = vol * chan->left_volume; \
	pvol[0] += vol_lx; \
	pvol[1] += vol_rx; \
	int32_t vol_avg = rshift_signed(vol_lx, 1) + rshift_signed(vol_rx, 1); \
	vol_avg = (vol_avg < 0) ? -vol_avg : vol_avg; \
	if (vol_avg > max) max = vol_avg; \
	pvol += 2;

#define SNDMIX_STORESTEREOVOL \
	int32_t vol_lx = vol_l * chan->right_volume; \
	int32_t vol_rx = vol_r * chan->left_volume; \
	pvol[0] += vol_lx; \
	pvol[1] += vol_rx; \
	int32_t vol_avg = rshift_signed(vol_lx, 1) + rshift_signed(vol_rx, 1); \
	vol_avg = (vol_avg < 0) ? -vol_avg : vol_avg; \
	if (vol_avg > max) max = vol_avg; \
	pvol += 2;

#define SNDMIX_STOREFASTMONOVOL \
	int32_t v = vol * chan->right_volume; \
	pvol[0] += v; \
	pvol[1] += v; \
	v = (v < 0) ? -v : v; \
	if (v > max) max = v; \
	pvol += 2;

#define SNDMIX_RAMPMONOVOL \
	left_ramp_volume += chan->left_ramp; \
	right_ramp_volume += chan->right_ramp; \
	int32_t vol_lx = vol * rshift_signed(right_ramp_volume, VOLUMERAMPPRECISION); \
	int32_t vol_rx = vol * rshift_signed(left_ramp_volume, VOLUMERAMPPRECISION); \
	pvol[0] += vol_lx; \
	pvol[1] += vol_rx; \
	int32_t vol_avg = rshift_signed(vol_lx, 1) + rshift_signed(vol_rx, 1); \
	vol_avg = (vol_avg < 0) ? -vol_avg : vol_avg; \
	if (vol_avg > max) max = vol_avg; \
	pvol += 2;

#define SNDMIX_RAMPFASTMONOVOL \
	right_ramp_volume += chan->right_ramp; \
	int32_t fastvol = vol * rshift_signed(right_ramp_volume, VOLUMERAMPPRECISION); \
	pvol[0] += fastvol; \
	pvol[1] += fastvol; \
	fastvol = (fastvol < 0) ? -fastvol : fastvol; \
	if (fastvol > max) max = fastvol; \
	pvol += 2;

#define SNDMIX_RAMPSTEREOVOL \
	left_ramp_volume += chan->left_ramp; \
	right_ramp_volume += chan->right_ramp; \
	int32_t vol_lx = vol_l * rshift_signed(right_ramp_volume, VOLUMERAMPPRECISION); \
	int32_t vol_rx = vol_r * rshift_signed(left_ramp_volume, VOLUMERAMPPRECISION); \
	pvol[0] += vol_lx; \
	pvol[1] += vol_rx; \
	int32_t vol_avg = rshift_signed(vol_lx, 1) + rshift_signed(vol_rx, 1); \
	vol_avg = (vol_avg < 0) ? -vol_avg : vol_avg; \
	if (vol_avg > max) max = vol_avg; \
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
	t##outchn = rshift_signed( \
		MUL_32_TO_64(volume, chan->filter_a0) \
			+ MUL_32_TO_64(FILT_CLIP(fy##outchn##1), chan->filter_b0) \
			+ MUL_32_TO_64(FILT_CLIP(fy##outchn##2), chan->filter_b1) \
			+ lshift_signed(1, FILTERPRECISION - 1), \
		FILTERPRECISION); \
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

typedef void(* mix_interface_t)(song_voice_t *, int32_t *, int32_t *);


#define BEGIN_MIX_INTERFACE(func) \
	static void func(song_voice_t *channel, int32_t *pbuffer, int32_t *pbufmax) \
	{ \
		int_fast32_t position;


#define END_MIX_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
	}

/* aliases here */
#define BEGIN_FASTMIX_INTERFACE(func) BEGIN_MIX_INTERFACE(func)
#define END_FASTMIX_INTERFACE()       END_MIX_INTERFACE()

// Volume Ramps
#define BEGIN_RAMPMIX_INTERFACE(func) \
	BEGIN_MIX_INTERFACE(func) \
	int32_t right_ramp_volume = channel->right_ramp_volume; \
	int32_t left_ramp_volume = channel->left_ramp_volume;


#define END_RAMPMIX_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
	channel->right_ramp_volume = right_ramp_volume; \
	channel->right_volume     = rshift_signed(right_ramp_volume, VOLUMERAMPPRECISION); \
	channel->left_ramp_volume  = left_ramp_volume; \
	channel->left_volume      = rshift_signed(left_ramp_volume, VOLUMERAMPPRECISION); \
	}


#define BEGIN_FASTRAMPMIX_INTERFACE(func) \
	BEGIN_MIX_INTERFACE(func) \
	int32_t right_ramp_volume = channel->right_ramp_volume;


#define END_FASTRAMPMIX_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
	channel->right_ramp_volume = right_ramp_volume; \
	channel->left_ramp_volume  = right_ramp_volume; \
	channel->right_volume     = rshift_signed(right_ramp_volume, VOLUMERAMPPRECISION); \
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
	int32_t right_ramp_volume = channel->right_ramp_volume; \
	int32_t left_ramp_volume  = channel->left_ramp_volume; \
	MIX_BEGIN_MONO_FILTER


#define END_RAMPMIX_MONO_FLT_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
	MIX_END_MONO_FILTER \
	channel->right_ramp_volume = right_ramp_volume; \
	channel->right_volume     = rshift_signed(right_ramp_volume, VOLUMERAMPPRECISION); \
	channel->left_ramp_volume  = left_ramp_volume; \
	channel->left_volume      = rshift_signed(left_ramp_volume, VOLUMERAMPPRECISION); \
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
	int32_t right_ramp_volume = channel->right_ramp_volume; \
	int32_t left_ramp_volume  = channel->left_ramp_volume; \
	MIX_BEGIN_STEREO_FILTER


#define END_RAMPMIX_STEREO_FLT_INTERFACE() \
	SNDMIX_ENDSAMPLELOOP \
	MIX_END_STEREO_FILTER \
	channel->right_ramp_volume = right_ramp_volume; \
	channel->right_volume     = rshift_signed(right_ramp_volume, VOLUMERAMPPRECISION); \
	channel->left_ramp_volume  = left_ramp_volume; \
	channel->left_volume      = rshift_signed(left_ramp_volume, VOLUMERAMPPRECISION); \
	}

#define BEGIN_RESAMPLE_INTERFACE(func, sampletype, numchannels) \
	void func(sampletype *oldbuf, sampletype *newbuf, uint32_t oldlen, uint32_t newlen) \
	{ \
	uint64_t position = 0; \
	const sampletype *p = oldbuf; \
	sampletype *pvol = newbuf; \
	const sampletype *pbufmax = &newbuf[newlen* numchannels]; \
	uint64_t increment = (((uint64_t)oldlen)<<16)/((uint64_t)newlen); \
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
		vol = CLAMP(vol, INT##bits##_MIN, INT##bits##_MAX); \
	END_RESAMPLE_INTERFACE_MONO()

#define DEFINE_STEREO_RESAMPLE_INTERFACE(bits) \
	BEGIN_RESAMPLE_INTERFACE(ResampleStereo##bits##BitFirFilter, int##bits##_t, 2) \
		SNDMIX_GETSTEREOVOLFIRFILTER(bits) \
		vol_l  >>= (WFIR_16SHIFT-WFIR_##bits##SHIFT);  /* This is used to compensate, since the code assumes that it always outputs to 16bits */ \
		vol_r  >>= (WFIR_16SHIFT-WFIR_##bits##SHIFT);  /* This is used to compensate, since the code assumes that it always outputs to 16bits */ \
		vol_l = CLAMP(vol_l, INT##bits##_MIN, INT##bits##_MAX); \
		vol_r = CLAMP(vol_r, INT##bits##_MIN, INT##bits##_MAX); \
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

static inline int32_t buffer_length_to_samples(int32_t mix_buf_cnt, song_voice_t *chan)
{
	return (chan->increment * (int32_t)mix_buf_cnt) + (int32_t)chan->position_frac;
}

static inline int32_t samples_to_buffer_length(int32_t samples, song_voice_t *chan)
{
	int32_t x = (lshift_signed(samples, 16)) / abs(chan->increment);
	return MAX(1, x);
}

static int32_t get_sample_count(song_voice_t *chan, int32_t samples)
{
	int32_t loop_start = (chan->flags & CHN_LOOP) ? chan->loop_start : 0;
	int32_t increment = chan->increment;

	if (samples <= 0 || !increment || !chan->length)
		return 0;

	// Under zero ?
	if ((int32_t)chan->position < loop_start) {
		if (increment < 0) {
			// Invert loop for bidi loops
			int32_t delta = ((loop_start - chan->position) << 16) - (chan->position_frac & 0xFFFF);
			chan->position = loop_start + (delta >> 16);
			chan->position_frac = delta & 0xFFFF;

			if ((int32_t) chan->position < loop_start ||
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
			if ((int32_t)chan->position < 0)
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

			chan->flags |= CHN_LOOP_WRAPPED;
		}
	}

	int32_t position = chan->position;

	// too big increment, and/or too small loop length
	if (position < loop_start) {
		if (position < 0 || increment < 0)
			return 0;
	}

	if (position < 0 || position >= (int32_t)chan->length)
		return 0;

	int32_t position_frac = (uint16_t) chan->position_frac,
		 sample_count = samples;

	if (increment < 0) {
		int32_t inv = -increment;
		int32_t maxsamples = 16384 / ((inv >> 16) + 1);

		if (maxsamples < 2)
			maxsamples = 2;

		if (samples > maxsamples)
			samples = maxsamples;

		int32_t delta_hi = (inv >> 16) * (samples - 1);
		int32_t delta_lo = (inv & 0xffff) * (samples - 1);
		int32_t pos_dest = position - delta_hi + ((position_frac - delta_lo) >> 16);

		if (pos_dest < loop_start) {
			sample_count =
				(uint32_t) (((((int64_t) position -
					loop_start) << 16) + position_frac -
					  1) / inv) + 1;
		}
	} else {
		int32_t maxsamples = 16384 / ((increment >> 16) + 1);

		if (maxsamples < 2)
			maxsamples = 2;

		if (samples > maxsamples)
			samples = maxsamples;

		int32_t delta_hi = (increment >> 16) * (samples - 1);
		int32_t delta_lo = (increment & 0xffff) * (samples - 1);
		int32_t pos_dest = position + delta_hi + ((position_frac + delta_lo) >> 16);

		if (pos_dest >= (int32_t) chan->length) {
			sample_count = (uint32_t)
				(((((int64_t) chan->length - position) << 16) - position_frac - 1) / increment) + 1;
		}
	}

	if (sample_count <= 1)
		return 1;
	else if (sample_count > samples)
		return samples;

	return sample_count;
}


uint32_t csf_create_stereo_mix(song_t *csf, uint32_t count)
{
	int32_t* ofsl, *ofsr;
	unsigned int nchused, nchmixed;

	if (!count)
		return 0;

	nchused = nchmixed = 0;

	// yuck
	if (csf->multi_write)
		for (uint32_t nchan = 0; nchan < MAX_CHANNELS; nchan++)
			memset(csf->multi_write[nchan].buffer, 0, sizeof(csf->multi_write[nchan].buffer));

	for (uint32_t nchan = 0; nchan < csf->num_voices; nchan++) {
		const mix_interface_t *mix_func_table;
		song_voice_t *const channel = &csf->voices[csf->voice_mix[nchan]];
		uint32_t flags;
		uint32_t nrampsamples;
		int32_t smpcount;
		int32_t nsamples;
		int32_t *pbuffer;

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
			int32_t master = (csf->voice_mix[nchan] < MAX_CHANNELS)
				? csf->voice_mix[nchan]
				: (channel->master_channel - 1);
			pbuffer = csf->multi_write[master].buffer;
			csf->multi_write[master].used = 1;
		} else {
			pbuffer = csf->mix_buffer;
		}

		nchused++;

		// Our loop lookahead buffer is basically the exact same as OpenMPT's.
		// (in essence, it is mostly just a backport)
		//
		// This means that it has the same bugs that are notated in OpenMPT's
		// `soundlib/Fastmix.cpp' file, which are the following:
		//
		// - Playing samples backwards should reverse interpolation LUTs for interpolation modes
		//   with more than two taps since they're not symmetric. We might need separate LUTs
		//   because otherwise we will add tons of branches.
		// - Loop wraparound works pretty well in general, but not at the start of bidi samples.
		// - The loop lookahead stuff might still fail for samples with backward loops.
		int8_t *const smp_ptr = (int8_t *const)(channel->ptr_sample->data);
		int8_t *lookahead_ptr = NULL;
		const uint32_t lookahead_start = (channel->loop_end < MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE) ? channel->loop_start : MAX(channel->loop_start, channel->loop_end - MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE);
		// This shouldn't be necessary with interpolation disabled but with that conditional
		// it causes weird precision loss within the sample, hence why I've removed it. This
		// shouldn't be that heavy anyway :p
		if (channel->flags & CHN_LOOP) {
			song_sample_t *pins = channel->ptr_sample;

			uint32_t lookahead_offset = 3 * MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE + pins->length - channel->loop_end;
			if (channel->flags & CHN_SUSTAINLOOP)
				lookahead_offset += 4 * MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE;

			lookahead_ptr = smp_ptr + lookahead_offset * ((pins->flags & CHN_STEREO) ? 2 : 1) * ((pins->flags & CHN_16BIT) ? 2 : 1);
		}

		////////////////////////////////////////////////////
		uint32_t naddmix = 0;

		do {
			nrampsamples = nsamples;

			if (channel->ramp_length > 0) {
				if ((int32_t)nrampsamples > channel->ramp_length)
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
				int32_t delta = buffer_length_to_samples(smpcount, channel);
				channel->position_frac = delta & 0xFFFF;
				channel->position += (delta >> 16);
				channel->rofs = channel->lofs = 0;
				pbuffer += smpcount * 2;
			} else if (!(channel->flags & CHN_ADLIB)) {
				// Mix the stream, unless we're in AdLib mode

				// Choose function for mixing
				mix_interface_t mix_func;
				mix_func = channel->ramp_length
					? mix_func_table[flags | MIXNDX_RAMP]
					: mix_func_table[flags];

				// Loop wrap-around magic
				if (lookahead_ptr) {
					const int32_t oldcount = smpcount;
					const int32_t read_length = rshift_signed(buffer_length_to_samples(smpcount - 1, channel), 16);
					const int at_loop_start = (channel->position >= channel->loop_start && channel->position < channel->loop_start + MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE);
					if (!at_loop_start)
						channel->flags &= ~CHN_LOOP_WRAPPED;

					channel->current_sample_data = smp_ptr;
					if (channel->position >= lookahead_start) {
						int32_t samples_to_read = (channel->increment < 0)
							? (channel->position - lookahead_start)
							: (channel->loop_end - channel->position);
						// this line causes sample 8 in BUTTERFL.XM to play incorrectly
						//samples_to_read = MAX(samples_to_read, channel->loop_end - channel->loop_start);
						smpcount = samples_to_buffer_length(samples_to_read, channel);

						channel->current_sample_data = lookahead_ptr;
					// This code keeps causing clicks with bidi loops, so I'm just gonna comment it out
					// for now.
					//} else if ((channel->flags & (CHN_LOOP | CHN_LOOP_WRAPPED)) && at_loop_start) {
					//	// Interpolate properly after looping
					//	smpcount = samples_to_buffer_length((channel->loop_start + MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE) - channel->position, channel);
					//	channel->current_sample_data = lookahead_ptr + (channel->loop_end - channel->loop_start) * ((channel->ptr_sample->flags & CHN_STEREO) ? 2 : 1) * ((channel->ptr_sample->flags & CHN_16BIT) ? 2 : 1);
					} else if (channel->increment > 0 && channel->position + read_length >= lookahead_start && smpcount > 1) {
						smpcount = samples_to_buffer_length(lookahead_start - channel->position, channel);
					}

					smpcount = CLAMP(smpcount, 1, oldcount);
				}

				int32_t *pbufmax = pbuffer + (smpcount * 2);
				channel->rofs = -*(pbufmax - 2);
				channel->lofs = -*(pbufmax - 1);

				mix_func(channel, pbuffer, pbufmax);
				channel->rofs += *(pbufmax - 2);
				channel->lofs += *(pbufmax - 1);
				pbuffer = pbufmax;
				naddmix = 1;
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
