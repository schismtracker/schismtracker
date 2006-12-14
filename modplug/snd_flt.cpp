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
// Simple 2-poles resonant filter
void CSoundFile::SetupChannelFilter(MODCHANNEL *pChn, BOOL bReset, int flt_modifier, int freq) const
//----------------------------------------------------------------------------------------
{
	float fs = (float)gdwMixingFreq;
	float fg, fb0, fb1, fc, dmpfac;

	UINT nCutOff = pChn->nCutOff&0x7F;
	UINT nResonance = pChn->nResonance & 0x7F;
	fc = 105.0f * pow(2.0f, 0.25f + ((float)(nCutOff*(flt_modifier+256)))/(20.0f*512.0f));
	//fc *= fs/((float)freq);
	if (fc < 120) fc=120;
	if (fc > 20000) fc=20000;
	if (fc*2 > (float)gdwMixingFreq) fc = gdwMixingFreq>>1;
	
	dmpfac = pow(10.0f, -((24.0f / 128.0f)*(float)(nResonance)) / 20.0f);

	fc *= (float)(2.0*PI/fs);
		
	float d = (1.0f-2.0f*dmpfac)* fc;
	if (d>2.0) d = 2.0;
	d = (2.0f*dmpfac - d)/fc;
	float e = pow(1.0f/fc,2.0f);

	fg=1.0f/(1.0f+d+e);
	fb0=(d+e+e)/(1.0f+d+e);
	fb1=-e/(1.0f+d+e);

	pChn->nFilter_A0 = fg;
	pChn->nFilter_B0 = fb0;
	pChn->nFilter_B1 = fb1;
	
	if (bReset) {
		pChn->nFilter_Y1 = pChn->nFilter_Y2 = 0;
		pChn->nFilter_Y3 = pChn->nFilter_Y4 = 0;
	}
	pChn->dwFlags |= CHN_FILTER;
}
#endif // NO_FILTER
