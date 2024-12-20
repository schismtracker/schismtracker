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
#include "osdefs.h"

#include <fcntl.h>
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

/* This function is roughly equivalent to the mkstemp() function on POSIX
 * operating systems, but instead of returning a file descriptor it returns
 * a C stdio file pointer. */
FILE *mkfstemp(char *template)
{
	static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	static size_t letters_len = ARRAY_SIZE(letters) - 1;
	static uint64_t value;
	struct timeval tv;
	int count;

	size_t len = strlen(template);
	if (len < 6 || strcmp(&template[len - 6], "XXXXXX")) {
		errno = EINVAL;
		return NULL;
	}

	/* This is where the Xs start.  */
	char *XXXXXX = &template[len - 6];

	/* rand() is already initialized by the time we use this function */
	value += rand();

	for (count = 0; count < TMP_MAX; ++count)
	{
		uint64_t v = value;
		FILE *fp;

		/* Fill in the random bits.  */
		XXXXXX[0] = letters[v % letters_len];
		v /= letters_len;
		XXXXXX[1] = letters[v % letters_len];
		v /= letters_len;
		XXXXXX[2] = letters[v % letters_len];
		v /= letters_len;
		XXXXXX[3] = letters[v % letters_len];
		v /= letters_len;
		XXXXXX[4] = letters[v % letters_len];
		v /= letters_len;
		XXXXXX[5] = letters[v % letters_len];

		// NOTE: C11 added a new subspecifier "x" that
		// can be used to fail if the file exists. this
		// isn't very useful for us though, since we're
		// C99...
		fp = os_fopen(template, "rb");
		if (!fp) {
			// it doesn't exist! open in write mode
			fp = os_fopen(template, "w+b");
			if (fp)
				return fp;
		}

		/* This is a random value.  It is only necessary that the next
		 * TMP_MAX values generated by adding 7777 to VALUE are different
		 * with (module 2^32).  */
		value += 7777;
	}

	/* We return the null string if we can't find a unique file name.  */
	template[0] = '\0';
	return NULL;
}