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
#include "mem.h"
#include "timer.h"

#include "backend/mt.h"

#define MT_DUMMY_ADDR ((void *)0xDEADBEEFCAFEBABE)

#ifdef USE_THREADS
static const schism_mt_backend_t *mt_backend = NULL;
#endif

mt_thread_t *mt_thread_create(schism_thread_function_t func, const char *name, void *userdata)
{
#ifdef USE_THREADS
	return mt_backend ? mt_backend->thread_create(func, name, userdata) : NULL;
#else
	return NULL;
#endif
}

void mt_thread_wait(mt_thread_t *thread, int *status)
{
#ifdef USE_THREADS
	if (mt_backend)
		mt_backend->thread_wait(thread, status);
#endif
}

void mt_thread_set_priority(int priority)
{
#ifdef USE_THREADS
	if (mt_backend)
		mt_backend->thread_set_priority(priority);
#endif
}

// returns the current thread's ID
mt_thread_id_t mt_thread_id(void)
{
#ifdef USE_THREADS
	return mt_backend ? mt_backend->thread_id() : 0;
#else
	return 0;
#endif
}

// ---------------------------------------------------------------------------

mt_mutex_t *mt_mutex_create(void)
{
#ifdef USE_THREADS
	if (mt_backend)
		return mt_backend->mutex_create();
#endif

	/* dummy addr */
	return MT_DUMMY_ADDR;
}

void mt_mutex_delete(mt_mutex_t *mutex)
{
#ifdef USE_THREADS
	if (mt_backend) {
		mt_backend->mutex_delete(mutex);
		return;
	}
#endif
/*
	SCHISM_RUNTIME_ASSERT(mutex == MT_DUMMY_ADDR,
		"make sure we're actually a mutex?");
*/
}

void mt_mutex_lock(mt_mutex_t *mutex)
{
#ifdef USE_THREADS
	if (mt_backend) {
		mt_backend->mutex_lock(mutex);
		return;
	}
#endif

/*
	SCHISM_RUNTIME_ASSERT(mutex == MT_DUMMY_ADDR,
		"make sure we're actually a mutex?");
*/
}

void mt_mutex_unlock(mt_mutex_t *mutex)
{
#ifdef USE_THREADS
	if (mt_backend) {
		mt_backend->mutex_unlock(mutex);
		return;
	}
#endif

/*
	SCHISM_RUNTIME_ASSERT(mutex == MT_DUMMY_ADDR,
		"make sure we're actually a mutex?");
*/
}

// ---------------------------------------------------------------------------
/* condition variables are inherently incompatible with non-threaded
 * environments (or disabled threading support), so we simply do a runtime
 * assertion for this case */

mt_cond_t *mt_cond_create(void)
{
#ifdef USE_THREADS
	if (mt_backend)
		return mt_backend->cond_create();

	return NULL;
#else
	SCHISM_RUNTIME_ASSERT(0, "this should never be called, since the way"
		"it works is incompatible with non-threaded models");
#endif
}

void mt_cond_delete(mt_cond_t *cond)
{
#ifdef USE_THREADS
	if (mt_backend && cond)
		mt_backend->cond_delete(cond);
#else
	SCHISM_RUNTIME_ASSERT(0, "this should never be called, since the way"
		"it works is incompatible with non-threaded models");
#endif
}

void mt_cond_signal(mt_cond_t *cond)
{
#ifdef USE_THREADS
	mt_backend->cond_signal(cond);
#else
	SCHISM_RUNTIME_ASSERT(0, "this should never be called, since the way"
		"it works is incompatible with non-threaded models");
#endif
}

void mt_cond_wait(mt_cond_t *cond, mt_mutex_t *mutex)
{
#ifdef USE_THREADS
	mt_backend->cond_wait(cond, mutex);
#else
	SCHISM_RUNTIME_ASSERT(0, "this should never be called, since the way"
		"it works is incompatible with non-threaded models");
#endif
}

void mt_cond_wait_timeout(mt_cond_t *cond, mt_mutex_t *mutex, uint32_t timeout)
{
#ifdef USE_THREADS
	mt_backend->cond_wait_timeout(cond, mutex, timeout);
#else
	SCHISM_RUNTIME_ASSERT(0, "this should never be called, since the way"
		"it works is incompatible with non-threaded models");
#endif
}

// ---------------------------------------------------------------------------

static int mt_test_thread_(void *xyzzy)
{
	return xyzzy != MT_DUMMY_ADDR;
}

int mt_init(void)
{
#ifdef USE_THREADS
	static const schism_mt_backend_t *backends[] = {
		// ordered by preference
#if defined(SCHISM_WIN32) || defined(SCHISM_XBOX)
		&schism_mt_backend_win32,
#endif
#ifdef SCHISM_SDL2
		&schism_mt_backend_sdl2,
#endif
#if defined(SCHISM_SDL12) && !defined(SCHISM_MACOS)
		&schism_mt_backend_sdl12,
#endif
//#ifdef SCHISM_SDL3
		/* we get sporadic thread leaks under SDL3 */
//		&schism_mt_backend_sdl3,
//#endif
		NULL,
	};

	int i;

	for (i = 0; backends[i]; i++) {
		int st;
		mt_thread_t *t;

		if (!backends[i]->init())
			continue;

		/* make sure that threads actually work.
		 * this cruft is here because SDL 1.2 thought it was a
		 * EXTREMELY GOOD IDEA to keep compiling thread support,
		 * and provide no way except starting a thread to detect
		 * whether it was even compiled. NICE! */
		t = backends[i]->thread_create(mt_test_thread_, "mt test thread", MT_DUMMY_ADDR);
		if (!t)
			continue;

		backends[i]->thread_wait(t, &st);
		SCHISM_RUNTIME_ASSERT(st == 0, "WHOOPS! threads are borked");

		mt_backend = backends[i];
		break;
	}

	if (!mt_backend)
		return 0;

	return 1;
#else
	return 0;
#endif
}

void mt_quit(void)
{
#ifdef USE_THREADS
	if (mt_backend) {
		mt_backend->quit();
		mt_backend = NULL;
	}
#endif
}