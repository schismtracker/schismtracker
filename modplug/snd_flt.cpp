/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/

#include "stdafx.h"
#include "sndfile.h"

// AWE32: cutoff = reg[0-255] * 31.25 + 100 -> [100Hz-8060Hz]
// EMU10K1 docs: cutoff = reg[0-127]*62+100

#ifndef NO_FILTER

#ifdef MSC_VER
#define _ASM_MATH
#endif

#ifdef _ASM_MATH

// pow(a,b) returns a^^b -> 2^^(b.log2(a))
static float pow(float a, float b)
{
	int tmpint;
	float result;
	_asm {
	fld b				// Load b
	fld a				// Load a
	fyl2x				// ST(0) = b.log2(a)
	fist tmpint			// Store integer exponent
	fisub tmpint		// ST(0) = -1 <= (b*log2(a)) <= 1
	f2xm1				// ST(0) = 2^(x)-1
	fild tmpint			// load integer exponent
	fld1				// Load 1
	fscale				// ST(0) = 2^ST(1)
	fstp ST(1)			// Remove the integer from the stack
	fmul ST(1), ST(0)	// multiply with fractional part
	faddp ST(1), ST(0)	// add integer_part
	fstp result			// Store the result
	}
	return result;
}


#else

#include <math.h>

#endif // _ASM_MATH


#define PI	((double)3.14159265358979323846)
#define LOG10 ((double)2.30258509299)
#define SB ((double)1.059463094359295309843105314939748495817)

// Simple 2-poles resonant filter
void CSoundFile::SetupChannelFilter(MODCHANNEL *pChn, BOOL bReset, int flt_modifier,
				int freq) const
//----------------------------------------------------------------------------------------
{
	double fs = (double)gdwMixingFreq;

#if 0
	double fc;
#endif

	double fx;
//if (freq) fs = (double)freq;
	fx = (m_dwSongFlags & SONG_EXFILTERRANGE) ? 21.0 : 22.0;

	double cutoff = pChn->nCutOff * (flt_modifier+191)
				/ (double)512.0f;

	double inv_angle = (fs * pow(0.5, 0.25 + cutoff/fx)) / (PI*255.0);
				
//	double inv_angle = pow(2.0,(127.0-cutoff)/fx)-0.93;

	if (!inv_angle) return; /* avoid FPE */
	double rr = pChn->nResonance;
//	double loss = pow(10.0f, -((double)rr / 256.0f));
	double loss = exp(rr * (-LOG10/192.0));/* tried 256.0 */
				
	if (m_dwSongFlags & SONG_EXFILTERRANGE) {
		loss *= (double)2.0;
	}

	double a,b,c,d, e;

	d = (1.0f - loss) / inv_angle;
	if (d > 2.0f) d = 2.0f;
	d = (loss - d) * inv_angle;
	e = inv_angle * inv_angle;
	if (1.0+d+e == 0) return;
	a = 1.0f / (1.0f + d + e);
	c = -e * a;
	b = 1.0f - a - c;

	pChn->nFilter_A0 = a;
	pChn->nFilter_B0 = b;
	pChn->nFilter_B1 = c;

	if (bReset)
	{
		pChn->nFilter_Y1 = pChn->nFilter_Y2 = 0;
		pChn->nFilter_Y3 = pChn->nFilter_Y4 = 0;
	}
	pChn->dwFlags |= CHN_FILTER;
#if 0



	fc = 110.0f * pow(2.0, 0.25f+((double)(pChn->nCutOff*(flt_modifier+256)))/(fx*512.0f));
	fc *= (double)(2.0*PI/fs);

	double fg, fb0, fb1;

	double dmpfac = pow(10.0f, -((fx/128.0)*(double)pChn->nResonance) / fx);
	double d = (1.0f-2.0f*dmpfac)* fc;
	if (d>2.0) d = 2.0;
	d = (2.0*dmpfac - d)/fc;
	double e = pow(2.0/fc,2.0);

	fg=1.0/(1.0+d+e);
	fb0=(d+e+e)/(1.0+d+e);
	fb1=-e/(1.0+d+e);

	pChn->nFilter_A0 = fg;
	pChn->nFilter_B0 = fb0;
	pChn->nFilter_B1 = fb1;

	if (bReset)
	{
		pChn->nFilter_Y1 = pChn->nFilter_Y2 = 0;
		pChn->nFilter_Y3 = pChn->nFilter_Y4 = 0;
	}
	pChn->dwFlags |= CHN_FILTER;
#endif
}

#endif // NO_FILTER
