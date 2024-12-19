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

/* SDL 1.2 doesn't provide threads; this causes issues for us,
 * so this is a reference implementation using */

#include "headers.h"
#include "mem.h"
#include "backend/threads.h"

#include <Multiprocessing.h>

/* -------------------------------------------------------------- */
/* semaphore */

struct schism_semaphore {
	MPSemaphoreID sem;
};

static schism_sem_t *macos_semaphore_create(uint32_t initial_value)
{
	schism_sem_t *sem = mem_alloc(sizeof(*sem));

	// TODO check result here
	MPCreateSemaphore(UINT32_MAX, initial_value, &sem->sem);

	return sem;
}

static void macos_semaphore_delete(schism_sem_t *sem)
{
	MPDeleteSemaphore(sem->sem);
	free(sem);
}

static void macos_semaphore_wait(schism_sem_t *sem)
{
	MPWaitOnSemaphore(sem->sem, kDurationForever);
}

static void macos_semaphore_post(schism_sem_t *sem)
{
	MPSignalSemaphore(sem->sem);
}

/* -------------------------------------------------------------- */

// what?
static MPQueueID notification_queue = NULL;

struct schism_thread {
	MPTaskID task;
	MPSemaphoreID sem;

	char *name;
	void *userdata;

	int (*func)(void *);
};

static OSStatus macos_dummy_thread_func(void *userdata)
{
	schism_thread_t *thread = userdata;

	int s = thread->func(thread->userdata);

	MPSignalSemaphore(thread->sem);

	return s;
}

static schism_thread_t *macos_thread_create(schism_thread_function_t func, const char *name, void *userdata)
{
	schism_thread_t *thread = mem_alloc(sizeof(*thread));

	thread->func = func;
	thread->userdata = userdata;

	if (MPCreateSemaphore(1, 0, &thread->sem) != noErr) {
		free(thread);
		return NULL;
	}

	// use a 512 KiB stack size, which should be plenty for what we need
	if (MPCreateTask(macos_dummy_thread_func, thread, UINT32_C(524288), notification_queue, NULL, NULL, 0, &thread->task) != noErr) {
		MPDeleteSemaphore(thread->sem);
		free(thread);
		return NULL;
	}

	return thread;
}

static void macos_thread_wait(schism_thread_t *thread, int *status)
{
	MPWaitOnSemaphore(thread->sem, kDurationForever);
}

static void macos_thread_set_priority(int priority)
{
	/* no-op */
}

static schism_thread_id_t macos_thread_id(void)
{
	return (schism_thread_id_t)MPCurrentTaskID();
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

const schism_threads_backend_t schism_threads_backend_macos = {
	.init = macos_threads_init,
	.quit = macos_threads_quit,

	.flags = SCHISM_THREADS_BACKEND_SUPPORTS_SEMAPHORE,

	.thread_create = macos_thread_create,
	.thread_wait = macos_thread_wait,
	.thread_set_priority = macos_thread_set_priority,
	.thread_id = macos_thread_id,

	.semaphore_create = macos_semaphore_create,
	.semaphore_delete = macos_semaphore_delete,
	.semaphore_wait = macos_semaphore_wait,
	.semaphore_post = macos_semaphore_post,
};
