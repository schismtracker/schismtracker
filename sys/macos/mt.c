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

/* SDL 1.2 doesn't provide threads on Mac OS, so we need to implement
 * them ourselves. */

#include "headers.h"
#include "mem.h"
#include "log.h"
#include "backend/mt.h"

#include <Multiprocessing.h>

/* -------------------------------------------------------------- */

struct mt_mutex {
	MPCriticalRegionID mutex;
};

static mt_mutex_t *macos_mutex_create(void)
{
	mt_mutex_t *mutex = mem_alloc(sizeof(*mutex));

	OSStatus err = MPCreateCriticalRegion(&mutex->mutex);
	if (err != noErr) {
		free(mutex);
		return NULL;
	}

	return mutex;
}

static void macos_mutex_delete(mt_mutex_t *mutex)
{
	MPDeleteCriticalRegion(mutex->mutex);
}

static void macos_mutex_lock(mt_mutex_t *mutex)
{
	MPEnterCriticalRegion(mutex->mutex, kDurationForever);
}

static void macos_mutex_unlock(mt_mutex_t *mutex)
{
	MPExitCriticalRegion(mutex->mutex);
}

/* -------------------------------------------------------------- */

struct mt_cond {
	MPEventID event;
};

static mt_cond_t *macos_cond_create(void)
{
	mt_cond_t *cond = mem_alloc(sizeof(*cond));

	OSStatus err = MPCreateEvent(&cond->event);
	if (err != noErr) {
		free(cond);
		return NULL;
	}

	return cond;
}

static void macos_cond_delete(mt_cond_t *cond)
{
	MPDeleteEvent(cond->event);
}

static void macos_cond_signal(mt_cond_t *cond)
{
	MPSetEvent(cond->event, 1);
}

static void macos_cond_wait(mt_cond_t *cond, mt_mutex_t *mutex)
{
	/* FIXME This is not atomic! */
	MPExitCriticalRegion(mutex->mutex);

	MPWaitForEvent(cond->event, NULL, kDurationForever);

	MPEnterCriticalRegion(mutex->mutex, kDurationForever);
}

static void macos_cond_wait_timeout(mt_cond_t *cond, mt_mutex_t *mutex, uint32_t timeout)
{
	/* FIXME This is not atomic! */
	MPExitCriticalRegion(mutex->mutex);

	MPWaitForEvent(cond->event, NULL, timeout);

	MPEnterCriticalRegion(mutex->mutex, kDurationForever);
}

/* -------------------------------------------------------------- */

// what?
static MPQueueID notification_queue = NULL;

struct mt_thread {
	MPTaskID task;
	MPEventID event; // for macos_thread_wait
	int status;

	void *userdata;

	int (*func)(void *);
};

static OSStatus macos_dummy_thread_func(void *userdata)
{
	mt_thread_t *thread = userdata;

	thread->status = thread->func(thread->userdata);

	// Notify any waiting thread
	MPSetEvent(thread->event, 1);

	return 0;
}

static mt_thread_t *macos_thread_create(schism_thread_function_t func, SCHISM_UNUSED const char *name, void *userdata)
{
	OSStatus err = noErr;
	mt_thread_t *thread = mem_alloc(sizeof(*thread));

	thread->func = func;
	thread->userdata = userdata;

	err = MPCreateEvent(&thread->event);
	if (err != noErr) {
		free(thread);
		log_appendf(4, "MPCreateEvent: %" PRId32, err);
		return NULL;
	}

	// use a 512 KiB stack size, which should be plenty for us
	err = MPCreateTask(macos_dummy_thread_func, thread, UINT32_C(524288), notification_queue, NULL, NULL, 0, &thread->task);
	if (err != noErr) {
		MPDeleteEvent(thread->event);
		free(thread);
		log_appendf(4, "MPCreateTask: %" PRId32, err);
		return NULL;
	}

	return thread;
}

static void macos_thread_wait(mt_thread_t *thread, int *status)
{
	// Wait until the dummy function calls us back
	MPWaitForEvent(thread->event, NULL, kDurationForever);
	MPDeleteEvent(thread->event);

	if (status)
		*status = thread->status;

	free(thread);
}

static void macos_thread_set_priority(int priority)
{
	MPTaskWeight weight;

	switch (priority) {
	case MT_THREAD_PRIORITY_LOW:           weight = 10;    break;
	case MT_THREAD_PRIORITY_NORMAL:        weight = 100;   break;
	case MT_THREAD_PRIORITY_HIGH:          weight = 1000;  break;
	case MT_THREAD_PRIORITY_TIME_CRITICAL: weight = 10000; break;
	default: return;
	}

	MPSetTaskWeight(MPCurrentTaskID(), weight);
}

static mt_thread_id_t macos_thread_id(void)
{
	return (mt_thread_id_t)(uintptr_t)MPCurrentTaskID();
}

//////////////////////////////////////////////////////////////////////////////

static int macos_threads_init(void)
{
	if (!MPLibraryIsLoaded())
		return 0;

	if (MPCreateQueue(&notification_queue) != noErr)
		return 0;

	return 1;
}

static void macos_threads_quit(void)
{
	MPDeleteQueue(notification_queue);
}

//////////////////////////////////////////////////////////////////////////////

const schism_mt_backend_t schism_mt_backend_macos = {
	.init = macos_threads_init,
	.quit = macos_threads_quit,

	.thread_create = macos_thread_create,
	.thread_wait = macos_thread_wait,
	.thread_set_priority = macos_thread_set_priority,
	.thread_id = macos_thread_id,

	.mutex_create = macos_mutex_create,
	.mutex_delete = macos_mutex_delete,
	.mutex_lock = macos_mutex_lock,
	.mutex_unlock = macos_mutex_unlock,

	.cond_create = macos_cond_create,
	.cond_delete = macos_cond_delete,
	.cond_signal = macos_cond_signal,
	.cond_wait   = macos_cond_wait,
	.cond_wait_timeout = macos_cond_wait_timeout,
};
