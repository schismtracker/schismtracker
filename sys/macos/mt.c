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

static UInt32 mp_major, mp_minor, mp_revision;

static inline SCHISM_ALWAYS_INLINE
int mp_ver_atleast(UInt32 major, UInt32 minor, UInt32 rev)
{
	return SCHISM_SEMVER_ATLEAST(major, minor, rev,
		mp_major, mp_minor, mp_revision);
}

/* -------------------------------------------------------------- */
/* thread-safe implementations of memory management functions */

static inline SCHISM_ALWAYS_INLINE
void *macos_mt_allocate(size_t size)
{
#if 0
	if (mp_ver_atleast(2, 0, 0)) {
		return MPAllocate(size);
	} else
#endif
	{
		void *q;

		q = MPAllocate(size + sizeof(size_t));
		memcpy(q, &size, sizeof(size_t));

		return ((char *)q + sizeof(size_t));
	}
}

static inline SCHISM_ALWAYS_INLINE
void macos_mt_free(void *q)
{
#if 0
	if (mp_ver_atleast(2, 0, 0)) {
		MPFree(q);
	} else
#endif
	{
		MPFree((char *)q - sizeof(size_t));
	}
}

static inline SCHISM_ALWAYS_INLINE
size_t macos_mt_asize(void *q)
{
#if 0
	if (mp_ver_atleast(2, 0, 0)) {
		return MPGetAllocatedBlockSize(q);
	} else
#endif
	{
		size_t r;
		memcpy(&r, (char *)q - sizeof(size_t), sizeof(size_t));
		return r;
	}
}

void *macos_malloc(size_t size)
{
	return macos_mt_allocate(size);
}

void *macos_calloc(size_t count, size_t nmemb)
{
	size_t size;
	void *q;

	size = count * nmemb;
#if 0 /* I don't think we'll need this */
	if (size && size / count != nmemb)
		return NULL; /* nope */
#endif

	q = macos_mt_allocate(size);
	memset(q, 0, size);

	return q;
}

void *macos_realloc(void *ptr, size_t newsize)
{
	void *q;

	if (!ptr)
		return macos_mt_allocate(newsize);

	/* allocate new memory from the heap */
	q = macos_mt_allocate(newsize);
	if (!q)
		return NULL;

	/* copy everything
	 * XXX can we do faster than memcpy ? :) */
	memcpy(q, ptr, macos_mt_asize(q));

	macos_mt_free(ptr);

	return q;
}

void macos_free(void *ptr)
{
	macos_mt_free(ptr);
}

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

/* XXX this is exactly the same as the win32 "fake" implementation,
 * we could combine the two and put it in the toplevel mt.c
 *
 * (implementation stolen from BeOS code by Christopher Tate and
 * Owen Smith) */
struct mt_cond {
	MPSemaphoreID sem;
	MPSemaphoreID handshake_sem;
	MPSemaphoreID signal_sem;
	int32_t nw; // number waiting
	int32_t ns; // number signaled
};

static void macos_cond_delete(mt_cond_t *cond);

static mt_cond_t *macos_cond_create(void)
{
	mt_cond_t *cond = mem_calloc(1, sizeof(*cond));
	OSErr err;

	err = 0;
	err |= MPCreateSemaphore(ULONG_MAX, 0, &cond->sem);
	err |= MPCreateSemaphore(ULONG_MAX, 0, &cond->handshake_sem);
	err |= MPCreateSemaphore(ULONG_MAX, 1, &cond->signal_sem);
	cond->ns = 0;
	cond->nw = 0;

	if (!cond->sem || !cond->handshake_sem || !cond->signal_sem) {
		macos_cond_delete(cond);
		return NULL;
	}

	return cond;
}

static void macos_cond_delete(mt_cond_t *cond)
{
	if (cond->sem) MPDeleteSemaphore(cond->sem);
	if (cond->handshake_sem) MPDeleteSemaphore(cond->handshake_sem);
	if (cond->signal_sem) MPDeleteSemaphore(cond->signal_sem);
}

static void macos_cond_signal(mt_cond_t *cond)
{
	// we need exclusive access to the waiter count while we figure out whether
	// we need a handshake with an awakening waiter thread
	MPWaitOnSemaphore(cond->signal_sem, kDurationForever);

	// are there waiters to be awakened?
	if (cond->nw > cond->ns) {
		// inform the next awakening waiter that we need a handshake, then release
		// all the locks and block until we get the handshake.  We need to go through the
		// handshake process even if we're interrupted, to avoid breaking the CV, so we
		// just set the eventual return code if we are interrupted in the middle.
		cond->ns++;
		MPSignalSemaphore(cond->sem);
		MPSignalSemaphore(cond->signal_sem);
		MPWaitOnSemaphore(cond->handshake_sem, kDurationForever);
	} else {
		// nobody is waiting, so the signal operation is a no-op
		MPSignalSemaphore(cond->signal_sem);
	}
}

static void macos_cond_wait_timeout(mt_cond_t *cond, mt_mutex_t *mutex, uint32_t timeout)
{
	// record the fact that we're waiting on the semaphore.  This action is
	// protected by a mutex because exclusive access to the waiter count is
	// needed by both waiting threads and signalling threads.  If someone interrupts
	// us while we're waiting for the lock (e.g. by calling kill() or send_signal()), we
	// abort and return the appropriate failure code.
	MPWaitOnSemaphore(cond->signal_sem, kDurationForever);
	cond->nw++;
	MPSignalSemaphore(cond->signal_sem);

	// actually wait for a signal -- we have to unlock the mutex before calling the
	// underlying blocking primitive.  The potential preemption between unlocking
	// the mutex and calling acquire_sem() is why we needed to record, prior to
	// this point, that we're in the process of waiting on the condition variable.
	mt_mutex_unlock(mutex);

	MPWaitOnSemaphore(cond->sem, kDurationForever);

	// we just awoke, either via a signal or by being interrupted.  If there's
	// a signaller running, he'll think he needs to handshake whether or not
	// we actually awoke due to his signal.  So, we reacquire the signalSem
	// mutex, and handshake if there's a positive signaller count.  It's critical
	// that we continue with the handshake process even if we've been interrupted,
	// so we just set the eventual error code and proceed with the CV state
	// unwinding in that case.
	MPWaitOnSemaphore(cond->signal_sem, kDurationForever);
	if (cond->ns > 0) {
		MPSignalSemaphore(cond->handshake_sem);
		cond->ns--;
	}
	cond->nw--;
	MPSignalSemaphore(cond->signal_sem);

	// always reacquire the mutex before returning, even in error cases
	mt_mutex_lock(mutex);
}

static void macos_cond_wait(mt_cond_t *cond, mt_mutex_t *mutex)
{
	macos_cond_wait_timeout(cond, mutex, kDurationForever);
}

/* -------------------------------------------------------------- */

/* not entirely sure wtf this is supposed to do */
static MPQueueID notification_queue = NULL;

struct mt_thread {
	MPTaskID task;
	MPSemaphoreID sem; /* for macos_thread_wait */
	int status;

	void *userdata;

	int (*func)(void *);
};

static OSStatus macos_dummy_thread_func(void *userdata)
{
	mt_thread_t *thread = userdata;

	thread->status = thread->func(thread->userdata);

	/* increment the semaphore so that any waiting thread will
	 * know that we're done */
	MPSignalSemaphore(thread->sem);

	return 0;
}

static mt_thread_t *macos_thread_create(schism_thread_function_t func, SCHISM_UNUSED const char *name, void *userdata)
{
	OSStatus err = noErr;
	mt_thread_t *thread = mem_alloc(sizeof(*thread));

	thread->func = func;
	thread->userdata = userdata;

	/* create a locked binary semaphore */
	err = MPCreateSemaphore(1, 0, &thread->sem);
	if (err != noErr) {
		free(thread);
		log_appendf(4, "MPCreateEvent: %" PRId32, err);
		return NULL;
	}

	// use a 512 KiB stack size, which should be plenty for us
	err = MPCreateTask(macos_dummy_thread_func, thread, UINT32_C(524288), notification_queue, NULL, NULL, 0, &thread->task);
	if (err != noErr) {
		MPDeleteSemaphore(thread->sem);
		free(thread);
		log_appendf(4, "MPCreateTask: %" PRId32, err);
		return NULL;
	}

	return thread;
}

static void macos_thread_wait(mt_thread_t *thread, int *status)
{
	/* the thread function increments the semaphore once it's finished */
	MPWaitOnSemaphore(thread->sem, kDurationForever);
	MPDeleteSemaphore(thread->sem);

	if (status)
		*status = thread->status;

	free(thread);
}

static void macos_thread_set_priority(int priority)
{
	MPTaskWeight weight;

	/* MPSetTaskWeight was added in Multiprocessing Services 2.0 */
	if (!mp_ver_atleast(2, 0, 0))
		return; /* no-op */

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
	const char *cstr;
	UInt32 release;

	if (!MPLibraryIsLoaded())
		return 0;

	if (MPCreateQueue(&notification_queue) != noErr)
		return 0;

	/* retrieve the library version.
	 * as the leading underscore suggests, this function is undocumented;
	 * however, it was still present in Multiprocessing Services 2.0, and
	 * was even implemented in Carbon! */
	_MPLibraryVersion(&cstr, &mp_major, &mp_minor, &release, &mp_revision);

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
