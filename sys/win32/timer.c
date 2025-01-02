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

// TODO move stuff from toplevel timer.c into here

#include "headers.h"
#include "threads.h"

#include "backend/timer.h"

#include <windows.h>
#include <mmsystem.h>

static enum {
	WIN32_TIMER_IMPL_QPC,
	WIN32_TIMER_IMPL_WINMM,
} win32_timer_impl = WIN32_TIMER_IMPL_WINMM;

// The counter value when we init
static LARGE_INTEGER win32_timer_start = {0};

// Ticks per second
static LARGE_INTEGER win32_timer_resolution = {0};

// This is used to regain some lost precision in timeGetTime()
static schism_mutex_t *win32_timer_overflow_mutex = NULL;
static int32_t win32_timer_overflow = 0;
static uint32_t win32_timer_last_known_ticks = 0;

static schism_ticks_t win32_timer_ticks(void)
{
	LARGE_INTEGER ticks = {0};

	switch (win32_timer_impl) {
	case WIN32_TIMER_IMPL_QPC:
		QueryPerformanceCounter(&ticks);
		break;
	case WIN32_TIMER_IMPL_WINMM:
		ticks.LowPart = timeGetTime();

		mt_mutex_lock(win32_timer_overflow_mutex);

		if (ticks.LowPart < win32_timer_last_known_ticks)
			win32_timer_overflow++;
		win32_timer_last_known_ticks = ticks.LowPart;

		mt_mutex_unlock(win32_timer_overflow_mutex);

		ticks.HighPart = win32_timer_overflow;

		break;
	}

	// floating point here kinda sucks, but whatever
	ticks.QuadPart = ((double)ticks.QuadPart - win32_timer_start.QuadPart) * 1000.0 / win32_timer_resolution.QuadPart;

	return ticks.QuadPart;
}

static int win32_timer_ticks_passed(schism_ticks_t a, schism_ticks_t b)
{
	return (a >= b);
}

static void win32_timer_delay(uint32_t ms)
{
	SleepEx(ms, FALSE);
}

static int win32_timer_must_end_period = 0;
static TIMECAPS win32_timer_caps = {0};

static int win32_timer_init(void)
{
	if (QueryPerformanceFrequency(&win32_timer_resolution)
		&& win32_timer_resolution.QuadPart
		&& QueryPerformanceCounter(&win32_timer_start)) {
		win32_timer_impl = WIN32_TIMER_IMPL_QPC;
	} else {
		// This works everywhere and uses the maximum timer precision
		// for the physical timer device
		win32_timer_must_end_period =
			(!timeGetDevCaps(&win32_timer_caps, sizeof(win32_timer_caps))
				&& !timeBeginPeriod(win32_timer_caps.wPeriodMin));

		win32_timer_start.QuadPart = timeGetTime();
		win32_timer_resolution.QuadPart = 1000;

		win32_timer_impl = WIN32_TIMER_IMPL_WINMM;
		win32_timer_overflow_mutex = mt_mutex_create();
	}

	// ok
	return 1;
}

static void win32_timer_quit(void)
{
	if (win32_timer_must_end_period)
		timeEndPeriod(win32_timer_caps.wPeriodMin);
}

//////////////////////////////////////////////////////////////////////////////

const schism_timer_backend_t schism_timer_backend_win32 = {
	.init = win32_timer_init,
	.quit = win32_timer_quit,

	.ticks = win32_timer_ticks,
	.ticks_passed = win32_timer_ticks_passed,
	.delay = win32_timer_delay,
};
