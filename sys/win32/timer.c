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
#include "mt.h"
#include "loadso.h"
#include "osdefs.h"
#include "mem.h"

#include "backend/timer.h"

#include <windows.h>
#include <mmsystem.h>

// Old toolchains don't have these:
#ifndef CREATE_WAITABLE_TIMER_MANUAL_RESET
# define CREATE_WAITABLE_TIMER_MANUAL_RESET 0x00000001
#endif
#ifndef CREATE_WAITABLE_TIMER_HIGH_RESOLUTION
# define CREATE_WAITABLE_TIMER_HIGH_RESOLUTION 0x00000002
#endif

// QueryPerformanceCounter always succeeds on XP and up,
// which means compiling the winmm version as a fallback
// is redundant for amd64 and arm architectures, since
// those versions never existed before XP.
#if defined(__386__) || defined(__i386) || defined(__i386__) || defined(i386) || defined(_M_IX86)
# define WIN32_TIMER_COMPILE_WINMM 1
#endif

#ifdef WIN32_TIMER_COMPILE_WINMM
static enum {
	WIN32_TIMER_IMPL_QPC,
	WIN32_TIMER_IMPL_WINMM,
} win32_timer_impl = WIN32_TIMER_IMPL_WINMM;
#endif

// The counter value when we init
static LARGE_INTEGER win32_timer_start = {0};

// Ticks per second
static LARGE_INTEGER win32_timer_resolution = {0};

static uint32_t win32_mm_period = 0;

static inline SCHISM_ALWAYS_INLINE LARGE_INTEGER _win32_timer_ticks_impl(void)
{
	LARGE_INTEGER ticks;

#ifdef WIN32_TIMER_COMPILE_WINMM
	switch (win32_timer_impl) {
	case WIN32_TIMER_IMPL_QPC:
#endif
		QueryPerformanceCounter(&ticks);
#ifdef WIN32_TIMER_COMPILE_WINMM
		break;
	case WIN32_TIMER_IMPL_WINMM:
	default:
		// This used to have overflow detection, but in some experiments
		// with Windows 2000 in Virtual PC timeGetTime() actually seems
		// to ~fluctuate~, which broke our heuristics. Just get the low
		// part for now I guess.
		ticks.LowPart = timeGetTime();
		ticks.HighPart = 0;
		break;
	}
#endif

	return ticks;
}

static timer_ticks_t win32_timer_ticks(void)
{
	LARGE_INTEGER ticks = _win32_timer_ticks_impl();

	ticks.QuadPart -= win32_timer_start.QuadPart;
	ticks.QuadPart *= INT64_C(1000);
	ticks.QuadPart /= win32_timer_resolution.QuadPart;

	return ticks.QuadPart;
}

static timer_ticks_t win32_timer_ticks_us(void)
{
	LARGE_INTEGER ticks = _win32_timer_ticks_impl();

	ticks.QuadPart -= win32_timer_start.QuadPart;
	ticks.QuadPart *= INT64_C(1000000);
	ticks.QuadPart /= win32_timer_resolution.QuadPart;

	return ticks.QuadPart;
}

// based off old code from midi-core.c but less obfuscated

// These are in much, much older versions of Windows.
// Really, Windows 95 is the only version that doesn't have these symbols.
// FIXME is this actually true? Win95 at least has WaitForSingleObject.
static HANDLE (WINAPI *WIN32_CreateWaitableTimer)(LPSECURITY_ATTRIBUTES, BOOL, LPCSTR) = NULL;
static BOOL (WINAPI *WIN32_SetWaitableTimer)(HANDLE, const LARGE_INTEGER *, LONG, PTIMERAPCROUTINE, LPVOID, BOOL) = NULL;

// internal NTDLL functions; this is what SleepEx actually calls under the hood
// on NT 4 and newer.
//
// XXX For architectures that never had 9x (namely, amd64, arm, arm64, ppc, alpha)
// should we only compile NtDelayExecution and simply not bother with the timer
// crap?
static LONG /*NTSTATUS*/ (__stdcall /*NTAPI*/ *NTDLL_NtDelayExecution)(BOOLEAN Alertable, PLARGE_INTEGER Interval) = NULL;

// FIXME: we're leaking timers here on thread exit
static DWORD dw_timer_tls_index = TLS_OUT_OF_INDEXES;

static inline SCHISM_ALWAYS_INLINE int win32_timer_usleep_timer_impl(uint64_t usec)
{
	LARGE_INTEGER due;
	HANDLE timer;

	if (!WIN32_SetWaitableTimer || !WIN32_CreateWaitableTimer || dw_timer_tls_index == TLS_OUT_OF_INDEXES)
		return 0; // NOPE

	timer = TlsGetValue(dw_timer_tls_index);
	if (!timer) {
		timer = WIN32_CreateWaitableTimer(NULL, TRUE, NULL);
		if (!timer)
			return 0;
		TlsSetValue(dw_timer_tls_index, timer);
	}

	due.QuadPart = -(10 * (int64_t)usec);
	if (!WIN32_SetWaitableTimer(timer, &due, 0, NULL, NULL, 0))
		return 0;

	if (WaitForSingleObject(timer, INFINITE) == WAIT_FAILED)
		return 0;

	return 1;
}

static void win32_timer_usleep(uint64_t usec)
{
	// Prioritize NtDelayExecution, it's what SleepEx actually uses
	// under the hood and removes the overhead of alerting.
	if (NTDLL_NtDelayExecution) {
		LARGE_INTEGER due;
		due.QuadPart = -(10 * (int64_t)usec);
		if (!NTDLL_NtDelayExecution(FALSE, &due))
			return;
	}

	// Do we even *need* the timer implementation?
	if (usec % 1000) {
		if (win32_timer_usleep_timer_impl(usec))
			return;

		// divide, rounding up
		SleepEx((usec + 999) / 1000, FALSE);
	} else {
		SleepEx(usec / 1000, FALSE);
	}
}

static void win32_timer_msleep(uint32_t msec)
{
	SleepEx(msec, FALSE);
}

//////////////////////////////////////////////////////////////////////////////

static void *kernel32 = NULL;
static void *ntdll = NULL;

static int win32_timer_must_end_period = 0;

static int win32_timer_init(void)
{
	TIMECAPS caps;

	// we check if this is valid in the timer impl
	dw_timer_tls_index = TlsAlloc();

	if (!timeGetDevCaps(&caps, sizeof(caps))) {
		win32_mm_period = caps.wPeriodMin;
		if (!timeBeginPeriod(win32_mm_period))
			win32_timer_must_end_period = 1;
	}

#ifdef WIN32_TIMER_COMPILE_WINMM
	if (win32_ntver_atleast(5, 1, 0) // This is buggy and broken on Win2k
		&& QueryPerformanceFrequency(&win32_timer_resolution)
		&& win32_timer_resolution.QuadPart
		&& QueryPerformanceCounter(&win32_timer_start)) {
		win32_timer_impl = WIN32_TIMER_IMPL_QPC;
	} else {
		// This works everywhere and is hopefully good enough
		win32_timer_start.QuadPart = timeGetTime();
		win32_timer_resolution.QuadPart = 1000;

		win32_timer_impl = WIN32_TIMER_IMPL_WINMM;
	}
#else
	QueryPerformanceFrequency(&win32_timer_resolution);
	QueryPerformanceCounter(&win32_timer_start);
#endif

	kernel32 = loadso_object_load("KERNEL32.DLL");
	if (kernel32) {
		WIN32_CreateWaitableTimer = loadso_function_load(kernel32, "CreateWaitableTimerA");
		if (!WIN32_CreateWaitableTimer)
			WIN32_CreateWaitableTimer = loadso_function_load(kernel32, "CreateWaitableTimer");
		WIN32_SetWaitableTimer = loadso_function_load(kernel32, "SetWaitableTimer");
	}

	ntdll = loadso_object_load("NTDLL.DLL");
	if (ntdll) {
		NTDLL_NtDelayExecution = loadso_function_load(ntdll, "NtDelayExecution");
	}

	// ok
	return 1;
}

static void win32_timer_quit(void)
{
	if (win32_timer_must_end_period)
		timeEndPeriod(win32_mm_period);

	if (kernel32)
		loadso_object_unload(kernel32);

	if (ntdll)
		loadso_object_unload(ntdll);

	if (dw_timer_tls_index == TLS_OUT_OF_INDEXES)
		TlsFree(dw_timer_tls_index);
}

//////////////////////////////////////////////////////////////////////////////

const schism_timer_backend_t schism_timer_backend_win32 = {
	.init = win32_timer_init,
	.quit = win32_timer_quit,

	.ticks = win32_timer_ticks,
	.ticks_us = win32_timer_ticks_us,
	.usleep = win32_timer_usleep,
	.msleep = win32_timer_msleep,
};
