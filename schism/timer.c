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
#include "mem.h"
#include "mt.h"

#include "backend/timer.h"

static const schism_timer_backend_t *backend = NULL;

timer_ticks_t timer_ticks(void)
{
	return backend->ticks();
}

timer_ticks_t timer_ticks_us(void)
{
	return backend->ticks_us();
}

void timer_usleep(uint64_t usec)
{
#if defined(HAVE_NANOSLEEP) && !defined(SCHISM_WIN32)
	// nanosleep is newer and preferred to usleep
	struct timespec s = {
		.tv_sec = usec / 1000000,
		.tv_nsec = (usec % 1000000) * 1000,
	};

	while (nanosleep(&s, &s) == -1);
#elif defined(HAVE_USLEEP) && !defined(SCHISM_WIN32)
	while (usec) {
		// useconds_t is only guaranteed to contain 0-1000000
		useconds_t t = MIN(usec, 1000000);
		usleep(t);
		usec -= t;
	}
#else
	backend->usleep(usec);
#endif
}

void timer_msleep(uint32_t ms)
{
	backend->msleep(ms);
}

static mt_thread_t *timer_oneshot_thread = NULL;
static int timer_oneshot_thread_cancelled = 0;

// A linked list of timers.
struct timer_oneshot_data_ {
	void (*callback)(void *param);
	void *param;

	// Ticks until the oneshot should be called, in microseconds.
	timer_ticks_t trigger;

	struct timer_oneshot_data_ *next;
};

// A list of pending timers that will be added to the real list
// by the running thread.
static struct timer_oneshot_data_ *oneshot_data_pending = NULL;
static mt_mutex_t *timer_oneshot_mutex = NULL;
static mt_cond_t *timer_oneshot_cond = NULL;

static int timer_oneshot_thread_func(void *userdata)
{
	struct timer_oneshot_data_ *oneshot_data_list = NULL;

	mt_mutex_lock(timer_oneshot_mutex);

	mt_thread_set_priority(MT_THREAD_PRIORITY_HIGH);

	while (!timer_oneshot_thread_cancelled) {
		// Add any pending timers waiting to get added to the list.
		if (oneshot_data_pending) {
			if (oneshot_data_list) {
				struct timer_oneshot_data_ *tail = oneshot_data_list;
				while (tail->next)
					tail = tail->next;
				tail->next = oneshot_data_pending;
			} else {
				oneshot_data_list = oneshot_data_pending;
			}

			oneshot_data_pending = NULL;
		}

		mt_mutex_unlock(timer_oneshot_mutex);

		timer_ticks_t wait = UINT64_MAX;

		if (oneshot_data_list) {
			struct timer_oneshot_data_ *data = oneshot_data_list, *prev = NULL;
			timer_ticks_t now = timer_ticks_us();

			while (data) {
				if (timer_ticks_passed(now, data->trigger)) {
					data->callback(data->param);

					now = timer_ticks_us();

					// Remove the timer from the list
					if (prev) {
						prev->next = data->next;
					} else {
						oneshot_data_list = data->next;
					}

					// free the data
					void *old = data;
					data = data->next;
					free(old);
				} else {
					wait = MIN(wait, data->trigger - now);

					prev = data;
					data = data->next;
				}
			}
		}

		wait /= 1000; //usec to msec

		mt_mutex_lock(timer_oneshot_mutex);

		mt_cond_wait_timeout(timer_oneshot_cond, timer_oneshot_mutex, wait);
	}

	return 0;
}

void timer_oneshot(uint32_t ms, void (*callback)(void *param), void *param)
{
	if (backend->oneshot && backend->oneshot(ms, callback, param))
		return;

	// Ok, the backend doesn't support oneshots or it failed to make an event.
	// Make a thread that "emulates" kernel-level events.

	struct timer_oneshot_data_ *data = mem_alloc(sizeof(*data));

	data->callback = callback;
	data->param = param;
	data->trigger = timer_ticks_us() + (ms * UINT64_C(1000));
	data->next = NULL;

	mt_mutex_lock(timer_oneshot_mutex);

	// initialize everything
	if (!timer_oneshot_thread) {
		oneshot_data_pending = NULL;

		timer_oneshot_thread_cancelled = 0;
		timer_oneshot_thread = mt_thread_create(timer_oneshot_thread_func, "Timer oneshot thread", NULL);
	}

	// append it to the list
	if (oneshot_data_pending) {
		struct timer_oneshot_data_ *tail = oneshot_data_pending;
		while (tail->next)
			tail = tail->next;
		tail->next = data;
	} else {
		oneshot_data_pending = data;
	}

	mt_mutex_unlock(timer_oneshot_mutex);

	mt_cond_signal(timer_oneshot_cond);
}

int timer_init(void)
{
	static const schism_timer_backend_t *backends[] = {
		// ordered by preference
#ifdef SCHISM_WIN32
		&schism_timer_backend_win32,
#endif
#ifdef SCHISM_SDL3
		&schism_timer_backend_sdl3,
#endif
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

	timer_oneshot_mutex = mt_mutex_create();
	timer_oneshot_cond = mt_cond_create();

	return 1;
}

void timer_quit(void)
{
	if (backend) {
		backend->quit();
		backend = NULL;
	}

	// Kill the oneshot stuff if needed.
	if (timer_oneshot_thread) {
		timer_oneshot_thread_cancelled = 1;
		mt_cond_signal(timer_oneshot_cond);
		mt_thread_wait(timer_oneshot_thread, NULL);
		timer_oneshot_thread = NULL;
	}

	if (timer_oneshot_mutex) {
		mt_mutex_delete(timer_oneshot_mutex);
		timer_oneshot_mutex = NULL;
	}
}
