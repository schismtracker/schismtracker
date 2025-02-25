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

#ifndef SCHISM_UTIL_H_
#define SCHISM_UTIL_H_

#include "headers.h"

#include <math.h>

#ifdef SCHISM_OS2
// TODO autoconf check for this
# define log10f log10
# define powf pow
#endif

/*Conversion*/
/* linear -> deciBell*/
/* amplitude normalized to 1.0f.*/
SCHISM_CONST inline SCHISM_ALWAYS_INLINE float dB(float amplitude)
{
	return 20.0f * log10f(amplitude);
}

/// deciBell -> linear*/
SCHISM_CONST inline SCHISM_ALWAYS_INLINE float dB2_amp(float db)
{
	return powf(10.0f, db / 20.0f);
}

/* linear -> deciBell*/
/* power normalized to 1.0f.*/
SCHISM_CONST inline SCHISM_ALWAYS_INLINE float pdB(float power)
{
	return 10.0f * log10f(power);
}

/* deciBell -> linear*/
SCHISM_CONST inline SCHISM_ALWAYS_INLINE float dB2_power(float db)
{
	return powf(10.0f, db / 10.0f);
}

/* linear -> deciBell*/
/* amplitude normalized to 1.0f.*/
/* Output scaled (and clipped) to 128 lines with noisefloor range.*/
/* ([0..128] = [-noisefloor..0dB])*/
/* correction_dBs corrects the dB after converted, but before scaling.*/
SCHISM_CONST inline SCHISM_ALWAYS_INLINE short dB_s(int noisefloor, float amplitude, float correction_dBs)
{
	const float db = dB(amplitude) + correction_dBs;
	const int x = (int)(128.0f * (db + noisefloor)) / noisefloor;
	return CLAMP(x, 0, 127);
}

/* deciBell -> linear*/
/* Input scaled to 128 lines with noisefloor range.*/
/* ([0..128] = [-noisefloor..0dB])*/
/* amplitude normalized to 1.0f.*/
/* correction_dBs corrects the dB after converted, but before scaling.*/
SCHISM_CONST inline SCHISM_ALWAYS_INLINE short dB2_amp_s(int noisefloor, int db, float correction_dBs)
{
	return dB2_amp((db * noisefloor / 128.0f) - noisefloor - correction_dBs);
}

/* linear -> deciBell*/
/* power normalized to 1.0f.*/
/* Output scaled (and clipped) to 128 lines with noisefloor range.*/
/* ([0..128] = [-noisefloor..0dB])*/
/* correction_dBs corrects the dB after converted, but before scaling.*/
SCHISM_CONST inline SCHISM_ALWAYS_INLINE short pdB_s(int noisefloor, float power, float correction_dBs)
{
	const float db = pdB(power) + correction_dBs;
	const int x = (int)(128.0f * (db + noisefloor)) / noisefloor;
	return CLAMP(x, 0, 127);
}

/* deciBell -> linear*/
/* Input scaled to 128 lines with noisefloor range.*/
/* ([0..128] = [-noisefloor..0dB])*/
/* power normalized to 1.0f.*/
/* correction_dBs corrects the dB after converted, but before scaling.*/
SCHISM_CONST inline SCHISM_ALWAYS_INLINE short dB2_power_s(int noisefloor, int db, float correction_dBs)
{
	return dB2_power((db * noisefloor / 128.0f) - noisefloor - correction_dBs);
}

/* integer sqrt (very fast; 32 bits limited) */
SCHISM_CONST inline SCHISM_ALWAYS_INLINE uint32_t i_sqrt(uint32_t r)
{
	uint32_t t, b, c = 0;

	for (b = 0x10000000; b != 0; b >>= 2) {
		t = c + b;
		c >>= 1;
		if (t <= r) {
			r -= t;
			c += b;
		}
	}

	return c;
}

FILE *mkfstemp(char *template);

#endif /* SCHISM_UTIL_H_ */
