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

#include "player/sndfile.h"
#include "player/snd_fm.h"
#include "player/snd_gm.h"
#include "player/cmixer.h"
#include "bits.h"
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
#define WFIR_16SHIFT            (WFIR_QUANTBITS)

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
	position = chan->position; \
	const int##bits##_t *p = (int##bits##_t *)chan->current_sample_data; \
	int32_t *pvol = pbuffer; \
	uint32_t max = chan->vu_meter; \
	do {


#define SNDMIX_ENDSAMPLELOOP \
		pvol[0] += vol_lx; \
		pvol[1] += vol_rx; \
		pvol += 2; \
		position = csf_smp_pos_add(position, chan->increment); \
	} while (pvol < pbufmax); \
	chan->vu_meter = max; \
	chan->position = position;

//////////////////////////////////////////////////////////////////////////////
// Mono

#define SNDMIX_GETNOIDOPOS /* nothing */

#define SNDMIX_GETLINEARPOS \
	int32_t poshi   = csf_smp_pos_get_whole(position); \
	int32_t poslo   = csf_smp_pos_get_frac(position) >> 24;

#define SNDMIX_GETSPLINEPOS \
	int32_t poshi = csf_smp_pos_get_whole(position); \
	/* FIXME this is stupid */ \
	int32_t poslo = rshift_signed(rshift_signed(csf_smp_pos_get_full(position), 16), SPLINE_FRACSHIFT) & SPLINE_FRACMASK;

#define SNDMIX_GETFIRFILTERPOS \
	int32_t poshi  = csf_smp_pos_get_whole(position); \
	int32_t poslo  = csf_smp_pos_get_frac(position) >> 16;

// No interpolation
#define SNDMIX_GETMONOVOLNOIDO(bits) \
	int32_t vol = lshift_signed(p[csf_smp_pos_get_whole(position)], -bits + 16);

// Linear Interpolation
#define SNDMIX_GETMONOVOLLINEAR(bits) \
	int32_t srcvol  = p[poshi]; \
	int32_t destvol = p[poshi + 1]; \
	int32_t vol     = lshift_signed(srcvol, -bits + 16) + rshift_signed(poslo * (destvol - srcvol), bits - 8);

// spline interpolation (2 guard bits should be enough???)
#define SNDMIX_GETMONOVOLSPLINE(bits) \
	int32_t vol   = rshift_signed( \
		  cubic_spline_lut[poslo + 0] * (int32_t)p[poshi - 1] \
		+ cubic_spline_lut[poslo + 1] * (int32_t)p[poshi + 0] \
		+ cubic_spline_lut[poslo + 2] * (int32_t)p[poshi + 1] \
		+ cubic_spline_lut[poslo + 3] * (int32_t)p[poshi + 2], \
		SPLINE_##bits##SHIFT);

// fir interpolation
#define SNDMIX_GETMONOVOLFIRFILTER(bits) \
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
	int32_t vol_l = lshift_signed(p[(csf_smp_pos_get_whole(position)) * 2 + 0], -bits + 16); \
	int32_t vol_r = lshift_signed(p[(csf_smp_pos_get_whole(position)) * 2 + 1], -bits + 16);

#define SNDMIX_GETSTEREOVOLLINEAR(bits) \
	int32_t srcvol_l = p[poshi * 2 + 0]; \
	int32_t srcvol_r = p[poshi * 2 + 1]; \
	int32_t vol_l    = lshift_signed(srcvol_l, -bits + 16) + rshift_signed(poslo * (p[poshi * 2 + 2] - srcvol_l), bits - 8); \
	int32_t vol_r    = lshift_signed(srcvol_r, -bits + 16) + rshift_signed(poslo * (p[poshi * 2 + 3] - srcvol_r), bits - 8);

// Spline Interpolation
#define SNDMIX_GETSTEREOVOLSPLINE(bits) \
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

#define SNDMIX_STOREVUMETER \
	uint32_t vol_avg = avg_u32(safe_abs_32(vol_lx), safe_abs_32(vol_rx)); \
	if (max < vol_avg) max = vol_avg;

// FIXME why are these backwards? what?
#define SNDMIX_STOREMONOVOL \
	int32_t vol_lx = vol * chan->right_volume; \
	int32_t vol_rx = vol * chan->left_volume;

#define SNDMIX_STORESTEREOVOL \
	int32_t vol_lx = vol_l * chan->right_volume; \
	int32_t vol_rx = vol_r * chan->left_volume;

#define SNDMIX_RAMPMONOVOL \
	left_ramp_volume += chan->left_ramp; \
	right_ramp_volume += chan->right_ramp; \
	int32_t vol_lx = vol * rshift_signed(right_ramp_volume, VOLUMERAMPPRECISION); \
	int32_t vol_rx = vol * rshift_signed(left_ramp_volume, VOLUMERAMPPRECISION);

#define SNDMIX_RAMPSTEREOVOL \
	left_ramp_volume += chan->left_ramp; \
	right_ramp_volume += chan->right_ramp; \
	int32_t vol_lx = vol_l * rshift_signed(right_ramp_volume, VOLUMERAMPPRECISION); \
	int32_t vol_rx = vol_r * rshift_signed(left_ramp_volume, VOLUMERAMPPRECISION);

///////////////////////////////////////////////////
// Resonant Filters

static inline SCHISM_ALWAYS_INLINE
int64_t MixerMul32To64(int32_t x, int32_t y)
{
	return (int64_t)x * y;
}

static inline SCHISM_ALWAYS_INLINE
int32_t FlitClip(int32_t x)
{
	return CLAMP(x, -65536, 65534);
}

#define MIX_BEGIN_FILTER(chn) \
	int32_t fy##chn##1 = channel->filter_y[chn][0]; \
	int32_t fy##chn##2 = channel->filter_y[chn][1]; \
	int32_t t##chn;

#define SNDMIX_PROCESSFILTER(outchn, volume) \
	t##outchn = rshift_signed( \
		MixerMul32To64(volume, chan->filter_a0) \
			+ MixerMul32To64(FlitClip(fy##outchn##1), chan->filter_b0) \
			+ MixerMul32To64(FlitClip(fy##outchn##2), chan->filter_b1) \
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

#define MIX_BEGIN_RAMP \
	int32_t right_ramp_volume = channel->right_ramp_volume; \
	int32_t left_ramp_volume  = channel->left_ramp_volume;

#define MIX_END_RAMP \
	channel->right_ramp_volume = right_ramp_volume; \
	channel->right_volume      = rshift_signed(right_ramp_volume, VOLUMERAMPPRECISION); \
	channel->left_ramp_volume  = left_ramp_volume; \
	channel->left_volume       = rshift_signed(left_ramp_volume, VOLUMERAMPPRECISION);

//////////////////////////////////////////////////////////
// Interfaces

typedef void(* mix_interface_t)(song_voice_t *, int32_t *, int32_t *);

/* this is the big one */
#define DEFINE_MIX_INTERFACE_ALL(BITS, CHNS, CHNSUPPER, RESAMPLING, RESAMPUPPER, FLTNAM, FILTER, BEGINFILTER, ENDFILTER, RAMP, RAMPUPPER, BEGINRAMP, ENDRAMP) \
	static void FLTNAM##CHNS##BITS##Bit##RESAMPLING##RAMP##Mix(song_voice_t *channel, int32_t *pbuffer, int32_t *pbufmax) \
	{ \
		struct song_smp_pos position; \
		BEGINRAMP \
		BEGINFILTER \
		SNDMIX_BEGINSAMPLELOOP(BITS) \
		SNDMIX_GET##RESAMPUPPER##POS \
		SNDMIX_GET##CHNSUPPER##VOL##RESAMPUPPER(BITS) \
		FILTER \
		SNDMIX_##RAMPUPPER##CHNSUPPER##VOL \
		SNDMIX_STOREVUMETER \
		SNDMIX_ENDSAMPLELOOP \
		ENDFILTER \
		ENDRAMP \
	}

#define DEFINE_MIX_INTERFACE_RAMP(BITS, CHNS, CHNSUPPER, RESAMPLING, RESAMPUPPER, FLTNAM, FILTER, BEGINFILTER, ENDFILTER) \
	DEFINE_MIX_INTERFACE_ALL(BITS, CHNS, CHNSUPPER, RESAMPLING, RESAMPUPPER, FLTNAM, FILTER, BEGINFILTER, ENDFILTER, \
		/* nothing */, STORE, /* nothing */,  /* nothing */) \
	DEFINE_MIX_INTERFACE_ALL(BITS, CHNS, CHNSUPPER, RESAMPLING, RESAMPUPPER, FLTNAM, FILTER, BEGINFILTER, ENDFILTER, \
		Ramp,          RAMP,  MIX_BEGIN_RAMP, MIX_END_RAMP)

/* defines all resampling variations */
#define DEFINE_MIX_INTERFACE_FILTER(BITS, CHNS, CHNSUPPER, RESAMPLING, RESAMPUPPER) \
	DEFINE_MIX_INTERFACE_RAMP(BITS, CHNS, CHNSUPPER, RESAMPLING, RESAMPUPPER, \
		/* nothing */, /* nothing */, /* nothing */, /* nothing */) \
	DEFINE_MIX_INTERFACE_RAMP(BITS, CHNS, CHNSUPPER, RESAMPLING, RESAMPUPPER, \
		Filter, SNDMIX_PROCESS##CHNSUPPER##FILTER, MIX_BEGIN_##CHNSUPPER##_FILTER, MIX_END_##CHNSUPPER##_FILTER)

#define DEFINE_MIX_INTERFACE_RESAMPLING(BITS, CHNS, CHNSUPPER) \
	DEFINE_MIX_INTERFACE_FILTER(BITS, CHNS, CHNSUPPER, /* none */, NOIDO) \
	DEFINE_MIX_INTERFACE_FILTER(BITS, CHNS, CHNSUPPER, Linear,     LINEAR) \
	DEFINE_MIX_INTERFACE_FILTER(BITS, CHNS, CHNSUPPER, Spline,     SPLINE) \
	DEFINE_MIX_INTERFACE_FILTER(BITS, CHNS, CHNSUPPER, FirFilter,  FIRFILTER)

#define DEFINE_MIX_INTERFACE_CHANNELS(BITS) \
	DEFINE_MIX_INTERFACE_RESAMPLING(BITS, Mono,   MONO) \
	DEFINE_MIX_INTERFACE_RESAMPLING(BITS, Stereo, STEREO) \

DEFINE_MIX_INTERFACE_CHANNELS(8)
DEFINE_MIX_INTERFACE_CHANNELS(16)

//////////////////////////////////////////////////////////
// Resampling

#define BEGIN_RESAMPLE_INTERFACE(FUNC, SAMPLETYPE, NUMCHANNELS) \
	void FUNC(SAMPLETYPE *oldbuf, SAMPLETYPE *newbuf, uint32_t oldlen, uint32_t newlen) \
	{ \
		struct song_smp_pos position = csf_smp_pos(0,0); \
		const SAMPLETYPE *p = oldbuf; \
		SAMPLETYPE *pvol = newbuf; \
		const SAMPLETYPE *pbufmax = &newbuf[newlen * NUMCHANNELS]; \
		struct song_smp_pos increment = csf_smp_pos_div_whole(csf_smp_pos(oldlen, 0), newlen); \
		do {

#define END_RESAMPLE_INTERFACE_MONO \
			*pvol = vol; \
			pvol++; \
			position = csf_smp_pos_add(position, increment); \
		} while (pvol < pbufmax); \
	}

#define END_RESAMPLE_INTERFACE_STEREO \
			pvol[0] = vol_l; \
			pvol[1] = vol_r; \
			pvol += 2; \
			position = csf_smp_pos_add(position, increment); \
		} while (pvol < pbufmax); \
	}

// Public Resampling Methods
#define DEFINE_MONO_RESAMPLE_INTERFACE(bits) \
	BEGIN_RESAMPLE_INTERFACE(ResampleMono##bits##BitFirFilter, int##bits##_t, 1) \
		SNDMIX_GETFIRFILTERPOS \
		SNDMIX_GETMONOVOLFIRFILTER(bits) \
		vol  >>= (WFIR_16SHIFT-WFIR_##bits##SHIFT);  /* This is used to compensate, since the code assumes that it always outputs to 16bits */ \
		vol = CLAMP(vol, INT##bits##_MIN, INT##bits##_MAX); \
	END_RESAMPLE_INTERFACE_MONO

#define DEFINE_STEREO_RESAMPLE_INTERFACE(bits) \
	BEGIN_RESAMPLE_INTERFACE(ResampleStereo##bits##BitFirFilter, int##bits##_t, 2) \
		SNDMIX_GETFIRFILTERPOS \
		SNDMIX_GETSTEREOVOLFIRFILTER(bits) \
		vol_l  >>= (WFIR_16SHIFT-WFIR_##bits##SHIFT);  /* This is used to compensate, since the code assumes that it always outputs to 16bits */ \
		vol_r  >>= (WFIR_16SHIFT-WFIR_##bits##SHIFT);  /* This is used to compensate, since the code assumes that it always outputs to 16bits */ \
		vol_l = CLAMP(vol_l, INT##bits##_MIN, INT##bits##_MAX); \
		vol_r = CLAMP(vol_r, INT##bits##_MIN, INT##bits##_MAX); \
	END_RESAMPLE_INTERFACE_STEREO

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

#define BUILD_MIX_FUNCTION_TABLE_RAMP(resampling, filter, ramp) \
	filter##Mono8Bit##resampling##ramp##Mix, \
	filter##Mono16Bit##resampling##ramp##Mix, \
	filter##Stereo8Bit##resampling##ramp##Mix, \
	filter##Stereo16Bit##resampling##ramp##Mix,

#define BUILD_MIX_FUNCTION_TABLE_FILTER(resampling, filter) \
	BUILD_MIX_FUNCTION_TABLE_RAMP(resampling, filter, /* none */) \
	BUILD_MIX_FUNCTION_TABLE_RAMP(resampling, filter, Ramp)

#define BUILD_MIX_FUNCTION_TABLE(resampling) \
	BUILD_MIX_FUNCTION_TABLE_FILTER(resampling, /* none */) \
	BUILD_MIX_FUNCTION_TABLE_FILTER(resampling, Filter)

// mix_(bits)(m/s)[_filt]_(interp/spline/fir/whatever)[_ramp]
static const mix_interface_t mix_functions[2 * 2 * 16] = {
	BUILD_MIX_FUNCTION_TABLE(/* none */)
	BUILD_MIX_FUNCTION_TABLE(Linear)
	BUILD_MIX_FUNCTION_TABLE(Spline)
	BUILD_MIX_FUNCTION_TABLE(FirFilter)
};

/* yap */
static inline SCHISM_ALWAYS_INLINE
uint32_t distance_to_buffer_length(struct song_smp_pos from, struct song_smp_pos to, struct song_smp_pos increment)
{
	return ((uint32_t)csf_smp_pos_div(csf_smp_pos_sub(csf_smp_pos_sub(to, from), csf_smp_pos(1,0)), increment)) + 1;
}

struct mix_loop_state {
	int8_t *smp_ptr;
	int8_t *lookahead_ptr;
	uint32_t lookahead_start;
	int32_t maxsamples;
};

static void mix_loop_state_update_lookahead_ptrs(struct mix_loop_state *mls, song_voice_t *channel)
{
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
	mls->smp_ptr = channel->ptr_sample ? (int8_t *const)(channel->ptr_sample->data) : NULL;
	mls->lookahead_ptr = NULL;
	mls->lookahead_start = (channel->loop_end < MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE)
		? channel->loop_start
		: MAX(channel->loop_start, channel->loop_end - MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE);
	// This shouldn't be necessary with interpolation disabled but with that conditional
	// it causes weird precision loss within the sample, hence why I've removed it. This
	// shouldn't be that heavy anyway :p
	if (channel->ptr_sample && (channel->flags & CHN_LOOP)) {
		song_sample_t *pins = channel->ptr_sample;

		uint32_t lookahead_offset = (((channel->flags & CHN_SUSTAINLOOP) ? 7 : 3) * MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE)
			+ (pins->length - channel->loop_end);

		mls->lookahead_ptr = mls->smp_ptr + (lookahead_offset
			* ((pins->flags & CHN_STEREO) ? 2 : 1)
			* ((pins->flags & CHN_16BIT)  ? 2 : 1));
	}
}

static void mix_loop_state_init(struct mix_loop_state *mls, song_voice_t *chan)
{
	if (!chan->current_sample_data)
		return;

	memset(mls, 0, sizeof(*mls));

	mix_loop_state_update_lookahead_ptrs(mls, chan);

	struct song_smp_pos inv = chan->increment;
	if (csf_smp_pos_is_negative(inv))
		inv = csf_smp_pos_negate(inv);

	mls->maxsamples = 16384u / (csf_smp_pos_get_whole(inv) + 1u);
	mls->maxsamples = MAX(mls->maxsamples, 2);
}

static int32_t get_sample_count(struct mix_loop_state *mls, song_voice_t *chan, int32_t samples)
{
	int32_t loop_start = (chan->flags & CHN_LOOP) ? chan->loop_start : 0;
	struct song_smp_pos increment = chan->increment;

	if (samples <= 0 || csf_smp_pos_equals_zero(increment) || !chan->length)
		return 0;

	/* reset this */
	chan->current_sample_data = mls->smp_ptr;

	// Under zero ?

	if (csf_smp_pos_lt(chan->position, csf_smp_pos(loop_start, 0))) {
		if (csf_smp_pos_is_negative(increment)) {
			// Invert loop for bidi loops
			struct song_smp_pos delta = csf_smp_pos_sub(csf_smp_pos(loop_start, 0), chan->position); 
			chan->position = csf_smp_pos_add(csf_smp_pos(loop_start, 0), delta);

			if (csf_smp_pos_lt(chan->position, csf_smp_pos(loop_start, 0))
				|| csf_smp_pos_ge(chan->position, csf_smp_pos((loop_start + chan->length) / 2, 0))) {
				chan->position = csf_smp_pos(loop_start, 0);
			}

			increment = csf_smp_pos_negate(increment);
			chan->increment = increment;
			// go forward
			chan->flags &= ~(CHN_PINGPONGFLAG);

			if ((!(chan->flags & CHN_LOOP)) ||
				(csf_smp_pos_ge(chan->position, csf_smp_pos(chan->length, 0)))) {
				chan->position = csf_smp_pos(chan->length, 0);
				return 0;
			}
		} else {
			// We probably didn't hit the loop end yet (first loop), so we do nothing
			if (csf_smp_pos_is_negative(chan->position))
				chan->position = csf_smp_pos(0, 0);
		}
	}
	// Past the end
	else if (csf_smp_pos_ge(chan->position, csf_smp_pos(chan->length, 0))) {
		// not looping -> stop this channel
		if (!(chan->flags & CHN_LOOP))
			return 0;

		if (chan->flags & CHN_PINGPONGLOOP) {
			// Invert loop
			if (csf_smp_pos_is_positive(increment)) {
				increment = csf_smp_pos_negate(increment);
				chan->increment = increment;
			}

			chan->flags |= CHN_PINGPONGFLAG;
			// adjust loop position
			struct song_smp_pos overshoot = csf_smp_pos_sub(chan->position, csf_smp_pos(chan->length, 0));
			struct song_smp_pos loop_length = csf_smp_pos(chan->loop_end - chan->loop_start - PINGPONG_OFFSET, 0);
			if (csf_smp_pos_lt(overshoot, loop_length)) {
				chan->position = csf_smp_pos_sub(csf_smp_pos(chan->length - PINGPONG_OFFSET, 0), overshoot);
			} else {
				/* not 100% accurate, but only matters for extremely small loops played at extremely high frequencies */
				chan->position = csf_smp_pos(chan->loop_start, 0);
			}
		} else {
			// This is a bug
			if (csf_smp_pos_is_negative(increment)) {
				increment = csf_smp_pos_negate(increment);
				chan->increment = increment;
			}

			// Restart at loop start
			chan->position = csf_smp_pos_add(chan->position, csf_smp_pos(loop_start - chan->length, 0));

			if (csf_smp_pos_lt(chan->position, csf_smp_pos(loop_start, 0)))
				chan->position = csf_smp_pos(chan->loop_start, 0);

			chan->flags |= CHN_LOOP_WRAPPED;
		}
	}

	int32_t position = csf_smp_pos_get_whole(chan->position);

	// too big increment, and/or too small loop length
	if (position < loop_start) {
		if (position < 0 || csf_smp_pos_is_negative(increment))
			return 0;
	}

	if (position < 0 || position >= (int32_t)chan->length)
		return 0;

	//int32_t position_frac = csf_smp_pos_get_frac(chan->position);
	int32_t sample_count = samples;

	struct song_smp_pos inv = increment;
	if (csf_smp_pos_is_negative(inv))
		inv = csf_smp_pos_negate(inv);

	sample_count = MIN(sample_count, mls->maxsamples);

	struct song_smp_pos inc_samples = csf_smp_pos_mul_whole(increment, sample_count - 1);
	int32_t pos_dest = csf_smp_pos_get_whole(csf_smp_pos_add(chan->position, inc_samples));

	const int at_loop_start = (csf_smp_pos_ge(chan->position, csf_smp_pos(chan->loop_start, 0))
		&& csf_smp_pos_lt(chan->position, csf_smp_pos(chan->loop_start + MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE, 0)));
	if (!at_loop_start)
		chan->flags &= ~(CHN_LOOP_WRAPPED);

	int checkdest = 1;
	// Loop wrap-around magic. (yummers)
	if (mls->lookahead_ptr) {
		if (csf_smp_pos_ge(chan->position, csf_smp_pos(mls->lookahead_start, 0))) {
			if (csf_smp_pos_is_negative(chan->increment)) {
				// going backwards and we're in the loop. We have to set the sample count
				// from the position from lookahead buffer start...
				sample_count = distance_to_buffer_length(csf_smp_pos(mls->lookahead_start, 0), chan->position, inv);
				chan->current_sample_data = mls->lookahead_ptr;
			} else if (csf_smp_pos_le(chan->position, csf_smp_pos(chan->loop_end, 0))) {
				// going forwards, and we're in the loop
				sample_count = distance_to_buffer_length(chan->position, csf_smp_pos(chan->loop_end, 0), inv);
				chan->current_sample_data = mls->lookahead_ptr;
			} else {
				// loop has ended, fix the position and keep going
				sample_count = distance_to_buffer_length(chan->position, csf_smp_pos(chan->length, 0), inv);
			}
			checkdest = 0;
		} else if ((chan->flags & CHN_LOOP_WRAPPED) && at_loop_start) {
			// Interpolate properly after looping
			sample_count = distance_to_buffer_length(chan->position, csf_smp_pos(loop_start + MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE, 0), inv);
			chan->current_sample_data = mls->lookahead_ptr + ((chan->length - loop_start) * ((chan->ptr_sample->flags & CHN_STEREO) ? 2 : 1) * ((chan->ptr_sample->flags & CHN_16BIT) ? 2 : 1));
			checkdest = 0;
		} else if (csf_smp_pos_is_positive(chan->increment) && pos_dest >= (int32_t)mls->lookahead_start && sample_count > 1) {
			// Don't go past the loop start!
			sample_count = distance_to_buffer_length(chan->position, csf_smp_pos(mls->lookahead_start, 0), inv);
			checkdest = 0;
		}
	}

	if (checkdest) {
		if (csf_smp_pos_is_negative(increment)) {
			if (pos_dest < loop_start)
				sample_count = distance_to_buffer_length(csf_smp_pos(loop_start, 0), chan->position, inv);
		} else {
			if (pos_dest >= (int32_t)chan->length)
				sample_count = distance_to_buffer_length(chan->position, csf_smp_pos(chan->length, 0), inv);
		}
	}

	sample_count = CLAMP(sample_count, 1, samples);

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
		song_voice_t *const channel = &csf->voices[csf->voice_mix[nchan]];
		uint32_t flags;
		uint32_t nrampsamples;
		int32_t smpcount;
		int32_t nsamples;
		int32_t *pbuffer;

		if ((!channel->current_sample_data || !channel->ptr_sample /* HAX */)
			&& !channel->lofs
			&& !channel->rofs)
			continue;

		ofsr = &csf->dry_rofs_vol;
		ofsl = &csf->dry_lofs_vol;
		flags = 0;

		if (channel->flags & CHN_16BIT)
			flags |= MIXNDX_16BIT;

		if (channel->flags & CHN_STEREO)
			flags |= MIXNDX_STEREO;

		if (channel->flags & CHN_FILTER)
			flags |= MIXNDX_FILTER;

		if (!(channel->flags & CHN_NOIDO)) {
			uint32_t srcflags[NUM_SRC_MODES] = {
				[SRCMODE_NEAREST] = 0,
				[SRCMODE_LINEAR] = MIXNDX_LINEARSRC,
				[SRCMODE_SPLINE] = MIXNDX_SPLINESRC,
				[SRCMODE_POLYPHASE] = MIXNDX_FIRSRC,
			};

			flags |= srcflags[csf->mix_interpolation];
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

		////////////////////////////////////////////////////
		uint32_t naddmix = 0;
		struct mix_loop_state mls;
		mix_loop_state_init(&mls, channel);
		channel->vu_meter <<= 16;

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
				smpcount = get_sample_count(&mls, channel, nrampsamples);
			}

			if (smpcount <= 0) {
				// Stopping the channel
				channel->current_sample_data = NULL;
				channel->length = 0;
				channel->position = csf_smp_pos(0,0);
				channel->ramp_length = 0;
				end_channel_ofs(channel, pbuffer, nsamples);
				*ofsr += channel->rofs;
				*ofsl += channel->lofs;
				channel->rofs = channel->lofs = 0;
				channel->flags &= ~CHN_PINGPONGFLAG;
				break;
			}

			// Should we mix this channel ?

			if ((nchmixed >= csf->max_voices && !(csf->mix_flags & SNDMIX_DIRECTTODISK))
				|| (!channel->ramp_length && !(channel->left_volume | channel->right_volume))) {
				channel->position = csf_smp_pos_add(channel->position, csf_smp_pos_mul_whole(channel->increment, smpcount));
				channel->rofs = channel->lofs = 0;
				pbuffer += smpcount * 2;
			} else if (!(channel->flags & CHN_ADLIB)) {
				// Mix the stream, unless we're in AdLib mode

				// Choose function for mixing
				mix_interface_t mix_func;
				mix_func = channel->ramp_length
					? mix_functions[flags | MIXNDX_RAMP]
					: mix_functions[flags];

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
				if (channel->ramp_length <= smpcount) {
					// Ramping is done
					channel->ramp_length = 0;
					channel->right_volume = channel->right_volume_new;
					channel->left_volume = channel->left_volume_new;
					channel->right_ramp = channel->left_ramp = 0;

					if ((channel->flags & CHN_NOTEFADE)
						&& (!(channel->fadeout_volume))) {
						channel->length = 0;
						channel->current_sample_data = NULL;
					}
				} else {
					channel->ramp_length -= smpcount;
				}
			}
		} while (nsamples > 0);

		/* Restore sample pointer in case it got changed through loop wrap-around */
		channel->current_sample_data = mls.smp_ptr;

		channel->vu_meter >>= 16;
		if (channel->vu_meter > 0xFF)
			channel->vu_meter = 0xFF;

		nchmixed += naddmix;
	}

	GM_IncrementSongCounter(csf, count);

	Fmdrv_Mix(csf, count);

	return nchused;
}
