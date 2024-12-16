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

#include "loadso.h"

#include "backend/timer.h"

static const schism_timer_backend_t *backend = NULL;

schism_ticks_t timer_ticks(void)
{
	return backend->ticks();
}

int timer_ticks_passed(schism_ticks_t a, schism_ticks_t b)
{
	return backend->ticks_passed(a, b);
}

void timer_delay(uint32_t ms)
{
	backend->delay(ms);
}

#ifdef SCHISM_WIN32
# include <windows.h>

// based off old code from midi-core.c but less obfuscated
static void *kernel32 = NULL;

// Added in Windows Vista; the flags we use were added in Windows 10 version 1803.
static HANDLE (WINAPI *WIN32_CreateWaitableTimerExW)(LPSECURITY_ATTRIBUTES, LPCWSTR, DWORD, DWORD) = NULL;

// These are in much, much older versions of Windows.
// Really, Windows 95 is the only version that doesn't have these symbols.
static HANDLE (WINAPI *WIN32_CreateWaitableTimer)(LPSECURITY_ATTRIBUTES, BOOL, LPCSTR) = NULL;
static BOOL (WINAPI *WIN32_SetWaitableTimer)(HANDLE, const LARGE_INTEGER *, LONG, PTIMERAPCROUTINE, LPVOID, BOOL) = NULL;
static DWORD (WINAPI *WIN32_WaitForSingleObject)(HANDLE, DWORD) = NULL;

static int timer_usleep_impl_(uint64_t usec)
{
	LARGE_INTEGER due;
	HANDLE timer;

	// If we don't even have these then we're screwed.
	if (!WIN32_SetWaitableTimer || !WIN32_WaitForSingleObject)
		goto timer_failed;

	// Old toolchains don't have these:
#ifndef CREATE_WAITABLE_TIMER_MANUAL_RESET
# define CREATE_WAITABLE_TIMER_MANUAL_RESET 0x00000001
#endif
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
# define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif
	// Create a high-resolution timer if we can.
	if (WIN32_CreateWaitableTimerExW) {
		timer = WIN32_CreateWaitableTimerExW(NULL, NULL, CREATE_WAITABLE_TIMER_MANUAL_RESET | CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE);
		if (timer)
			goto have_timer;
	}

	// Ok, the high-resolution timer isn't available, fallback:
	if (WIN32_CreateWaitableTimer) {
		timer = WIN32_CreateWaitableTimer(NULL, TRUE, NULL);
		if (timer)
			goto have_timer;
	}

	goto timer_failed;

have_timer:
	due.QuadPart = -(10 * (int64_t)usec);
	if (!WIN32_SetWaitableTimer(timer, &due, 0, NULL, NULL, 0))
		goto timer_failed;

	if (WIN32_WaitForSingleObject(timer, INFINITE) == WAIT_FAILED)
		goto timer_failed;

	return 1; // woo!

timer_failed:
	CloseHandle(timer);
	return 0;
}
#else
static int timer_usleep_impl_(uint64_t usec)
{
# if defined(HAVE_NANOSLEEP)
	// nanosleep is preferred to usleep
	struct timespec s = {
		.tv_sec = usec / 1000000,
		.tv_nsec = (usec % 1000000) * 1000,
	};

	while (nanosleep(&s, &s) == -1);

	return 1;
# elif defined(HAVE_USLEEP)
	while (usec) {
		// useconds_t is only guaranteed to contain 0-1000000
		useconds_t t = MIN(usec, 1000000);
		usleep(t);
		usec -= t;
	}

	return 1;
# else
	// :)
	return 0;
# endif
}
#endif

void timer_usleep(uint64_t usec)
{
	if (timer_usleep_impl_(usec))
		return;

	// this sucks, but its portable...
	const schism_ticks_t next = timer_ticks() + (usec / 1000);

	while (!timer_ticks_passed(timer_ticks(), next));
}

// this tries to be as accurate as possible
void timer_msleep(uint64_t msec)
{
	if (timer_usleep_impl_(msec * 1000))
		return;

	timer_delay(msec);
}

int timer_init(void)
{
	static const schism_timer_backend_t *backends[] = {
		// ordered by preference
#ifdef SCHISM_SDL2
		&schism_timer_backend_sdl2,
#endif
#ifdef SCHISM_SDL12
		&schism_timer_backend_sdl12,
#endif
		NULL,
	};

	int i;

	for (i = 0; backends[i]; i++) {
		backend = backends[i];
		if (backend->init())
			break;

		backend = NULL;
	}

	if (!backend)
		return 0;

#ifdef SCHISM_WIN32
	kernel32 = loadso_object_load("KERNEL32.DLL");

	if (kernel32) {
		WIN32_CreateWaitableTimerExW = loadso_function_load(kernel32, "CreateWaitableTimerExW");
		WIN32_CreateWaitableTimer = loadso_function_load(kernel32, "CreateWaitableTimer");
		WIN32_SetWaitableTimer = loadso_function_load(kernel32, "SetWaitableTimer");
		WIN32_WaitForSingleObject = loadso_function_load(kernel32, "WaitForSingleObject");
	}
#endif

	return 1;
}

void timer_quit(void)
{
#ifdef SCHISM_WIN32
	if (kernel32)
		loadso_object_unload(kernel32);
#endif
	if (backend) {
		backend->quit();
		backend = NULL;
	}
}

