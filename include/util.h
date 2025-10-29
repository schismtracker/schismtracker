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

/*Conversion*/
/* linear -> deciBell*/
/* amplitude normalized to 1.0f.*/
SCHISM_ALWAYS_INLINE SCHISM_CONST static inline
float dB(float amplitude)
{
	return 20.0f * log10(amplitude);
}

/// deciBell -> linear*/
SCHISM_ALWAYS_INLINE SCHISM_CONST static inline
float dB2_amp(float db)
{
	return pow(10.0f, db / 20.0f);
}

/* linear -> deciBell*/
/* power normalized to 1.0f.*/
SCHISM_ALWAYS_INLINE SCHISM_CONST static inline
float pdB(float power)
{
	return 10.0f * log10(power);
}

/* deciBell -> linear*/
SCHISM_ALWAYS_INLINE SCHISM_CONST static inline
float dB2_power(float db)
{
	return pow(10.0f, db / 10.0f);
}

/* linear -> deciBell*/
/* amplitude normalized to 1.0f.*/
/* Output scaled (and clipped) to 128 lines with noisefloor range.*/
/* ([0..128] = [-noisefloor..0dB])*/
/* correction_dBs corrects the dB after converted, but before scaling.*/
SCHISM_ALWAYS_INLINE SCHISM_CONST static inline
short dB_s(int noisefloor, float amplitude, float correction_dBs)
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
SCHISM_ALWAYS_INLINE SCHISM_CONST static inline
short dB2_amp_s(int noisefloor, int db, float correction_dBs)
{
	return dB2_amp((db * noisefloor / 128.0f) - noisefloor - correction_dBs);
}

/* linear -> deciBell*/
/* power normalized to 1.0f.*/
/* Output scaled (and clipped) to 128 lines with noisefloor range.*/
/* ([0..128] = [-noisefloor..0dB])*/
/* correction_dBs corrects the dB after converted, but before scaling.*/
SCHISM_ALWAYS_INLINE SCHISM_CONST static inline
short pdB_s(int noisefloor, float power, float correction_dBs)
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
SCHISM_ALWAYS_INLINE SCHISM_CONST static inline
short dB2_power_s(int noisefloor, int db, float correction_dBs)
{
	return dB2_power((db * noisefloor / 128.0f) - noisefloor - correction_dBs);
}

/* this should be somewhere else */

/* return value: 0 if success, negative if error */
int util_getumask(mode_t *m);
int util_setumask(mode_t m);

int util_initumask(void);
void util_quitumask(void);

FILE *mkfstemp(char *template);

/* open a message box with printf formatted string. */
int msgboxv(int style, const char *title, const char *fmt, va_list ap);
int msgbox(int style, const char *title, const char *fmt, ...);

/* fast XOR with a byte value */
void mem_xor(void *vbuf, size_t len, unsigned char c);

void minmax_8(const int8_t *buf, size_t len, int8_t *min, int8_t *max,
	size_t stride);
void minmax_16(const int16_t *buf, size_t len, int16_t *min, int16_t *max,
	size_t stride);
void minmax_32(const int32_t *buf, size_t len, int32_t *min, int32_t *max,
	size_t stride);

#endif /* SCHISM_UTIL_H_ */
