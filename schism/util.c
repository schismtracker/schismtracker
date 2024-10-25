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

/* This is just a collection of some useful functions. None of these use any
extraneous libraries (i.e. GLib). */


#include "headers.h"

#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>

#include <stdarg.h>

#include <math.h>

/* --------------------------------------------------------------------- */
/* CONVERSION FUNCTIONS */

/* linear -> decibel */
/* amplitude normalized to 1.0f. */
double dB(double amplitude)
{
	return 20.0 * log10(amplitude);
}

/* decibel -> linear */
double dB2_amp(double db)
{
	return pow(10.0, db / 20.0);
}

/* linear -> decibel */
/* power normalized to 1.0f. */
double pdB(double power)
{
	return 10.0 * log10(power);
}

/* decibel -> linear */
double dB2_power(double db)
{
	return pow(10.0, db / 10.0);
}
/* linear -> decibel
 * amplitude normalized to 1.0f.
 * Output scaled (and clipped) to 128 lines with noisefloor range.
 * ([0..128] = [-noisefloor..0dB])
 * correction_dBs corrects the dB after converted, but before scaling.
*/
short dB_s(int noisefloor, double amplitude, double correction_dBs)
{
	double db = dB(amplitude) + correction_dBs;
	return CLAMP((int)(128.0*(db+noisefloor))/noisefloor, 0, 127);
}

/* decibel -> linear */
/* Input scaled to 128 lines with noisefloor range. */
/* ([0..128] = [-noisefloor..0dB]) */
/* amplitude normalized to 1.0f. */
/* correction_dBs corrects the dB after converted, but before scaling.*/
short dB2_amp_s(int noisefloor, int db, double correction_dBs)
{
	return dB2_amp((db*noisefloor/128.0)-noisefloor-correction_dBs);
}
/* linear -> decibel */
/* power normalized to 1.0f. */
/* Output scaled (and clipped) to 128 lines with noisefloor range. */
/* ([0..128] = [-noisefloor..0dB]) */
/* correction_dBs corrects the dB after converted, but before scaling.*/
short pdB_s(int noisefloor, double power, double correction_dBs)
{
	float db = pdB(power)+correction_dBs;
	return CLAMP((int)(128.0*(db+noisefloor))/noisefloor, 0, 127);
}

/* deciBell -> linear */
/* Input scaled to 128 lines with noisefloor range. */
/* ([0..128] = [-noisefloor..0dB]) */
/* power normalized to 1.0f. */
/* correction_dBs corrects the dB after converted, but before scaling.*/
short dB2_power_s(int noisefloor, int db, double correction_dBs)
{
	return dB2_power((db*noisefloor/128.f)-noisefloor-correction_dBs);
}

/* fast integer sqrt */
unsigned int i_sqrt(unsigned int r)
{
	unsigned int t, b, c=0;
	for (b = 0x10000000; b != 0; b >>= 2) {
		t = c + b;
		c >>= 1;
		if (t <= r) {
			r -= t;
			c += b;
		}
	}
	return(c);
}

void dos_time_to_timeval(struct timeval *timeval, uint32_t dos_time)
{
	// convert to microseconds
	uint64_t us = (uint64_t)dos_time * 54945;

	timeval->tv_sec = us / 1000000;
	timeval->tv_usec = us % 1000000;
}

uint32_t timeval_to_dos_time(const struct timeval *timeval)
{
	int64_t dos = ((int64_t)timeval->tv_sec * 182 / 10) + ((int64_t)timeval->tv_usec / 54945);

	// don't overflow!
	return CLAMP(dos, 0, UINT32_MAX);
}

void fat_date_time_to_tm(struct tm *tm, uint16_t fat_date, uint16_t fat_time)
{
	*tm = (struct tm){
		/* PRESENT DAY */
		.tm_mday = fat_date & 0x1F,
		.tm_mon = ((fat_date >> 5) & 0xF) - 1,
		.tm_year = (fat_date >> 9) + 80,

		/* PRESENT TIME */
		.tm_sec = (fat_time & 0x1F) << 1,
		.tm_min = ((fat_time >> 5) & 0x3F),
		.tm_hour = fat_time >> 11,
	};

	// normalize the data in case the fat time was screwed?
	mktime(tm);
}

void tm_to_fat_date_time(const struct tm *tm, uint16_t *fat_date, uint16_t *fat_time)
{
	struct tm tm_n = *tm;

	// normalize it so we can be sure that the data is valid
	mktime(&tm_n);

	*fat_date = tm_n.tm_mday | ((tm_n.tm_mon + 1) << 5) | ((tm_n.tm_year - 80) << 9);
	*fat_time = (tm_n.tm_sec >> 1) | (tm_n.tm_min << 5) | (tm_n.tm_hour << 11);
}
