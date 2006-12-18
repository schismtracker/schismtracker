/*
 * This program is  free software; you can redistribute it  and modify it
 * under the terms of the GNU  General Public License as published by the
 * Free Software Foundation; either version 2  of the license or (at your
 * option) any later version.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *
 * Name                Date             Description
 * 
 * Olivier Lapicque    --/--/--         Creation
 * Trevor Nunes        26/01/04         conditional compilation for AMD,MMX calls
 *
*/
#include "stdafx.h"
#include "sndfile.h"
#include <math.h>


#define EQ_BANDWIDTH	2.0
#define EQ_ZERO			0.000001
#define REAL			float

extern REAL MixFloatBuffer[];

extern void StereoMixToFloat(const int *pSrc, float *pOut1, float *pOut2, UINT nCount);
extern void FloatToStereoMix(const float *pIn1, const float *pIn2, int *pOut, UINT nCount);
extern void MonoMixToFloat(const int *pSrc, float *pOut, UINT nCount);
extern void FloatToMonoMix(const float *pIn, int *pOut, UINT nCount);

typedef struct _EQBANDSTRUCT
{
	REAL a0, a1, a2, b1, b2;
	REAL x1, x2, y1, y2;
	REAL Gain, CenterFrequency;
	BOOL bEnable;
} EQBANDSTRUCT, *PEQBANDSTRUCT;

UINT gEqLinearToDB[33] =
{
	16, 19, 22, 25, 28, 31, 34, 37,
	40, 43, 46, 49, 52, 55, 58, 61,
	64, 76, 88, 100, 112, 124, 136, 148,
	160, 172, 184, 196, 208, 220, 232, 244, 256
};


//static REAL f2ic = (REAL)(1 << 28);
//static REAL i2fc = (REAL)(1.0 / (1 << 28));

static EQBANDSTRUCT gEQ[MAX_EQ_BANDS*2] =
{
	// Default: Flat EQ
	{0,0,0,0,0, 0,0,0,0, 1,   120, FALSE},
	{0,0,0,0,0, 0,0,0,0, 1,   600, FALSE},
	{0,0,0,0,0, 0,0,0,0, 1,  1200, FALSE},
	{0,0,0,0,0, 0,0,0,0, 1,  3000, FALSE},
	{0,0,0,0,0, 0,0,0,0, 1,  6000, FALSE},
	{0,0,0,0,0, 0,0,0,0, 1, 10000, FALSE},
	{0,0,0,0,0, 0,0,0,0, 1,   120, FALSE},
	{0,0,0,0,0, 0,0,0,0, 1,   600, FALSE},
	{0,0,0,0,0, 0,0,0,0, 1,  1200, FALSE},
	{0,0,0,0,0, 0,0,0,0, 1,  3000, FALSE},
	{0,0,0,0,0, 0,0,0,0, 1,  6000, FALSE},
	{0,0,0,0,0, 0,0,0,0, 1, 10000, FALSE},
};

void EQFilter(EQBANDSTRUCT *pbs, REAL *pbuffer, UINT nCount)
//----------------------------------------------------------
{
	for (UINT i=0; i<nCount; i++)
	{
		REAL x = pbuffer[i];
		REAL y = pbs->a1 * pbs->x1 + pbs->a2 * pbs->x2 + pbs->a0 * x + pbs->b1 * pbs->y1 + pbs->b2 * pbs->y2;
		pbs->x2 = pbs->x1;
		pbs->y2 = pbs->y1;
		pbs->x1 = x;
		pbuffer[i] = y;
		pbs->y1 = y;
	}
}

void CSoundFile::EQMono(int *pbuffer, UINT nCount)
//------------------------------------------------
{
	MonoMixToFloat(pbuffer, MixFloatBuffer, nCount);
	for (UINT b=0; b<MAX_EQ_BANDS; b++)
	{
		if ((gEQ[b].bEnable) && (gEQ[b].Gain != 1.0f))
			EQFilter(&gEQ[b], MixFloatBuffer, nCount);
	}
	FloatToMonoMix(MixFloatBuffer, pbuffer, nCount);
}

void CSoundFile::EQStereo(int *pbuffer, UINT nCount)
//--------------------------------------------------
{
	StereoMixToFloat(pbuffer, MixFloatBuffer, MixFloatBuffer+MIXBUFFERSIZE, nCount);
		
	for (UINT bl=0; bl<MAX_EQ_BANDS; bl++)
	{
		if ((gEQ[bl].bEnable) && (gEQ[bl].Gain != 1.0f))
			EQFilter(&gEQ[bl], MixFloatBuffer, nCount);
	}
	for (UINT br=MAX_EQ_BANDS; br<MAX_EQ_BANDS*2; br++)
	{
		if ((gEQ[br].bEnable) && (gEQ[br].Gain != 1.0f))
			EQFilter(&gEQ[br], MixFloatBuffer+MIXBUFFERSIZE, nCount);
	}

	FloatToStereoMix(MixFloatBuffer, MixFloatBuffer+MIXBUFFERSIZE, pbuffer, nCount);

}

void CSoundFile::InitializeEQ(BOOL bReset)
//----------------------------------------
{
	REAL fMixingFreq = (REAL)gdwMixingFreq;
	// Gain = 0.5 (-6dB) .. 2 (+6dB)
	for (UINT band=0; band<MAX_EQ_BANDS*2; band++) if (gEQ[band].bEnable)
	{
		REAL k, k2, r, f;
		REAL v0, v1;
		BOOL b = bReset;

		f = gEQ[band].CenterFrequency / fMixingFreq;
		if (f > 0.45f) gEQ[band].Gain = 1;
		// if (f > 0.25) f = 0.25;
		// k = tan(PI*f);
		k = f * 3.141592654f;
		k = k + k*f;
//		if (k > (REAL)0.707) k = (REAL)0.707;
		k2 = k*k;
		v0 = gEQ[band].Gain;
		v1 = 1;
		if (gEQ[band].Gain < 1.0)
		{
			v0 *= (0.5f/EQ_BANDWIDTH);
			v1 *= (0.5f/EQ_BANDWIDTH);
		} else
		{
			v0 *= (1.0f/EQ_BANDWIDTH);
			v1 *= (1.0f/EQ_BANDWIDTH);
		}
		r = (1 + v0*k + k2) / (1 + v1*k + k2);
		if (r != gEQ[band].a0)
		{
			gEQ[band].a0 = r;
			b = TRUE;
		}
		r = 2 * (k2 - 1) / (1 + v1*k + k2);
		if (r != gEQ[band].a1)
		{
			gEQ[band].a1 = r;
			b = TRUE;
		}
		r = (1 - v0*k + k2) / (1 + v1*k + k2);
		if (r != gEQ[band].a2)
		{
			gEQ[band].a2 = r;
			b = TRUE;
		}
		r = - 2 * (k2 - 1) / (1 + v1*k + k2);
		if (r != gEQ[band].b1)
		{
			gEQ[band].b1 = r;
			b = TRUE;
		}
		r = - (1 - v1*k + k2) / (1 + v1*k + k2);
		if (r != gEQ[band].b2)
		{
			gEQ[band].b2 = r;
			b = TRUE;
		}
		if (b)
		{
			gEQ[band].x1 = 0;
			gEQ[band].x2 = 0;
			gEQ[band].y1 = 0;
			gEQ[band].y2 = 0;
		}
	} else
	{
		gEQ[band].a0 = 0;
		gEQ[band].a1 = 0;
		gEQ[band].a2 = 0;
		gEQ[band].b1 = 0;
		gEQ[band].b2 = 0;
		gEQ[band].x1 = 0;
		gEQ[band].x2 = 0;
		gEQ[band].y1 = 0;
		gEQ[band].y2 = 0;
	}
}


void CSoundFile::SetEQGains(const UINT *pGains, UINT nGains, const UINT *pFreqs, BOOL bReset)
//-------------------------------------------------------------------------------------------
{
	for (UINT i=0; i<MAX_EQ_BANDS; i++)
	{
		REAL g, f = 0;
		if (i < nGains)
		{
			UINT n = pGains[i];
//			if (n > 32) n = 32;
			g = 1.0 + (((double)n) / 64.0);
			if (pFreqs) f = (REAL)(int)pFreqs[i];
		} else
		{
			g = 1;
		}
		gEQ[i].Gain = g;
		gEQ[i].CenterFrequency = f;
		gEQ[i+MAX_EQ_BANDS].Gain = g;
		gEQ[i+MAX_EQ_BANDS].CenterFrequency = f;
		if (f > 20.0f && i < nGains) /* don't enable bands outside... */
		{
			gEQ[i].bEnable = TRUE;
			gEQ[i+MAX_EQ_BANDS].bEnable = TRUE;
		} else
		{
			gEQ[i].bEnable = FALSE;
			gEQ[i+MAX_EQ_BANDS].bEnable = FALSE;
		}
	}
	InitializeEQ(bReset);
}
