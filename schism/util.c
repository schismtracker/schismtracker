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
#include "backend/object.h"
#include "backend/timer.h"
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

/* -------------------------------------------------- */
/* high precision sleep */

#ifdef SCHISM_WIN32
# include <windows.h>
#endif

int rt_usleep_impl_(uint64_t usec)
{
#ifdef SCHISM_WIN32
	HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
	LARGE_INTEGER ft;

	// 100 ns interval, negate to indicate relative time
	ft.QuadPart = -(10 * usec);

	SetWaitableTimer(timer, &ft, 0, NULL, NULL, 0);
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);

	return 1;
#elif defined(HAVE_NANOSLEEP)
	// nanosleep is preferred to usleep
	struct timespec s = {
		.tv_sec = usec / 1000000,
		.tv_nsec = (usec % 1000000) * 1000,
	};

	while (nanosleep(&s, &s) == -1);

	return 1;
#elif defined(HAVE_USLEEP)
	while (usec) {
		// useconds_t is only guaranteed to contain 0-1000000
		useconds_t t = MIN(usec, 1000000);
		usleep(t);
		usec -= t;
	}

	return 1;
#else
	// :)
	return 0;
#endif
}

void rt_usleep(uint64_t usec)
{
	if (rt_usleep_impl_(usec))
		return;

	// this sucks, but its portable...
	const schism_ticks_t next = be_timer_ticks() + (usec / 1000);

	while (!be_timer_ticks_passed(be_timer_ticks(), next));
}

// this tries to be as accurate as possible
void msleep(uint64_t msec)
{
	if (rt_usleep_impl_(msec * 1000))
		return;

	be_delay(msec);
}

/* ---------------------------------------------------- */

#ifdef SCHISM_WIN32
# define LIBTOOL_FMT "lib%s-%d.dll"
#elif defined(SCHISM_MACOSX)
# define LIBTOOL_FMT "lib%s.%d.dylib"
#else
# define LIBTOOL_FMT "lib%s.so.%d"
#endif

static void *library_load_revision(const char *name, int revision)
{
	char *buf;

	if (asprintf(&buf, LIBTOOL_FMT, name, revision) < 0)
		return NULL;

	void *res = be_object_load(buf);

	free(buf);

	return res;
}

#undef LIBTOOL_FMT

/* loads a library using the current and age versions
 * as documented by GNU libtool. MAKE SURE the objects
 * you are importing are actually there in the library!
 * (i.e. check return values) */
void *library_load(const char *name, int current, int age)
{
	int i;
	void *res = NULL;

	for (i = 0; i <= age; i++)
		if ((res = library_load_revision(name, current - i)))
			break;

	return res;
}
