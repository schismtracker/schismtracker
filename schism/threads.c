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

#include "backend/threads.h"

static const schism_threads_backend_t *mt_backend = NULL;

// The backend is required to support this ;)
schism_thread_t *mt_thread_create(schism_thread_function_t func, const char *name, void *userdata)
{
	return mt_backend->thread_create(func, name, userdata);
}

void mt_thread_wait(schism_thread_t *thread, int *status)
{
	mt_backend->thread_wait(thread, status);
}

void mt_thread_set_priority(int priority)
{
	mt_backend->thread_set_priority(priority);
}

// returns the current thread's ID
schism_thread_id_t mt_thread_id(void)
{
	return mt_backend->thread_id();
}

// ---------------------------------------------------------------------------

// for the generic implementation
struct schism_mutex {
	int recursive;
	uintptr_t owner;
	struct schism_semaphore *sem;
};

schism_mutex_t *mt_mutex_create(void)
{
	if (mt_backend->flags & SCHISM_THREADS_BACKEND_SUPPORTS_MUTEX)
		return mt_backend->mutex_create();

	schism_mutex_t *mutex = mem_alloc(sizeof(*mutex));

	mutex->sem = mt_semaphore_create(0);
	mutex->recursive = 0;
	mutex->owner = 0;
	if (!mutex->sem) {
		free(mutex);
		return NULL;
	}

	return mutex;
}

void mt_mutex_delete(schism_mutex_t *mutex)
{
	if (mt_backend->flags & SCHISM_THREADS_BACKEND_SUPPORTS_MUTEX) {
		mt_backend->mutex_delete(mutex);
		return;
	}

	if (mutex->sem)
		mt_semaphore_delete(mutex->sem);

	free(mutex);
}

void mt_mutex_lock(schism_mutex_t *mutex)
{
	if (mt_backend->flags & SCHISM_THREADS_BACKEND_SUPPORTS_MUTEX) {
		mt_backend->mutex_lock(mutex);
		return;
	}

	uintptr_t this_thread = mt_thread_id();
	if (mutex->owner == this_thread) {
		++mutex->recursive;
	} else {
		mt_semaphore_wait(mutex->sem);
		mutex->owner = this_thread;
		mutex->recursive = 0;
	}
}

void mt_mutex_unlock(schism_mutex_t *mutex)
{
	if (mt_backend->flags & SCHISM_THREADS_BACKEND_SUPPORTS_MUTEX) {
		mt_backend->mutex_unlock(mutex);
		return;
	}

	/* If we don't own the mutex, we can't unlock it */
	if (mt_thread_id() != mutex->owner)
		return;

	if (mutex->recursive) {
		--mutex->recursive;
	} else {
		mutex->owner = 0;
		mt_semaphore_post(mutex->sem);
	}
}

// ---------------------------------------------------------------------------

// for the generic implementation only.
struct schism_cond {
	schism_mutex_t *lock;
	int waiting;
	int signals;
	schism_sem_t *wait_sem;
	schism_sem_t *wait_done;
};

schism_cond_t *mt_cond_create(void)
{
	if (mt_backend->flags & SCHISM_THREADS_BACKEND_SUPPORTS_COND)
		return mt_backend->cond_create();

	schism_cond_t *cond = mem_alloc(sizeof(schism_cond_t));

	cond->lock = mt_mutex_create();
	cond->wait_sem = mt_semaphore_create(0);
	cond->wait_done = mt_semaphore_create(0);
	cond->waiting = cond->signals = 0;
	if (!cond->lock || !cond->wait_sem || !cond->wait_done) {
		mt_cond_delete(cond);
		cond = NULL;
	}

	return cond;
}

void mt_cond_delete(schism_cond_t *cond)
{
	if (mt_backend->flags & SCHISM_THREADS_BACKEND_SUPPORTS_COND) {
		mt_backend->cond_delete(cond);
		return;
	}

	if (cond->wait_sem)
		mt_semaphore_delete(cond->wait_sem);

	if (cond->wait_done)
		mt_semaphore_delete(cond->wait_done);

	if (cond->lock)
		mt_mutex_delete(cond->lock);

	free(cond);
}

void mt_cond_signal(schism_cond_t *cond)
{
	if (mt_backend->flags & SCHISM_THREADS_BACKEND_SUPPORTS_COND) {
		mt_backend->cond_signal(cond);
		return;
	}

	mt_mutex_lock(cond->lock);
	if (cond->waiting > cond->signals) {
		++cond->signals;
		mt_semaphore_post(cond->wait_sem);
		mt_mutex_unlock(cond->lock);
		mt_semaphore_wait(cond->wait_done);
	} else {
		mt_mutex_unlock(cond->lock);
	}
}

void mt_cond_wait(schism_cond_t *cond, schism_mutex_t *mutex)
{
	if (mt_backend->flags & SCHISM_THREADS_BACKEND_SUPPORTS_COND) {
		mt_backend->cond_wait(cond, mutex);
		return;
	}

	mt_mutex_lock(cond->lock);
	++cond->waiting;
	mt_mutex_unlock(cond->lock);

	mt_mutex_unlock(mutex);

	mt_semaphore_wait(cond->wait_sem);

	mt_mutex_lock(cond->lock);
	if (cond->signals > 0) {
		mt_semaphore_wait(cond->wait_sem);

		mt_semaphore_post(cond->wait_done);

		--cond->signals;
	}
	--cond->waiting;
	mt_mutex_unlock(cond->lock);

	mt_mutex_lock(mutex);
}

// ---------------------------------------------------------------------------

// for the UNTESTED generic implementation only.
struct schism_semaphore {
	uint32_t count;
	uint32_t waiters_count;
	schism_mutex_t *count_lock;
	schism_cond_t *count_nonzero;
};

schism_sem_t *mt_semaphore_create(uint32_t initial_value)
{
	if (mt_backend->flags & SCHISM_THREADS_BACKEND_SUPPORTS_SEMAPHORE)
		return mt_backend->semaphore_create(initial_value);

	schism_sem_t *sem = mem_alloc(sizeof(*sem));
	if (!sem)
		return NULL;

	sem->count = initial_value;
	sem->waiters_count = 0;

	sem->count_lock = mt_mutex_create();
	sem->count_nonzero = mt_cond_create();
	if (!sem->count_lock || !sem->count_nonzero) {
		mt_semaphore_delete(sem);
		return NULL;
	}

	return sem;
}

void mt_semaphore_delete(schism_sem_t *sem)
{
	if (mt_backend->flags & SCHISM_THREADS_BACKEND_SUPPORTS_SEMAPHORE) {
		mt_backend->semaphore_delete(sem);
		return;
	}

	sem->count = 0xFFFFFFFF;
	while (sem->waiters_count > 0) {
		mt_cond_signal(sem->count_nonzero);
		timer_delay(10);
	}
	mt_cond_delete(sem->count_nonzero);
	if (sem->count_lock) {
		mt_mutex_lock(sem->count_lock);
		mt_mutex_unlock(sem->count_lock);
		mt_mutex_delete(sem->count_lock);
	}
	free(sem);
}

void mt_semaphore_wait(schism_sem_t *sem)
{
	if (mt_backend->flags & SCHISM_THREADS_BACKEND_SUPPORTS_SEMAPHORE) {
		mt_backend->semaphore_wait(sem);
		return;
	}

	mt_mutex_lock(sem->count_lock);
	++sem->waiters_count;

	while (sem->count == 0)
		mt_cond_wait(sem->count_nonzero, sem->count_lock);

	--sem->waiters_count;
	--sem->count;
	mt_mutex_unlock(sem->count_lock);
}

void mt_semaphore_post(schism_sem_t *sem)
{
	if (mt_backend->flags & SCHISM_THREADS_BACKEND_SUPPORTS_SEMAPHORE) {
		mt_backend->semaphore_post(sem);
		return;
	}

	mt_mutex_lock(sem->count_lock);
	if (sem->waiters_count > 0)
		mt_cond_signal(sem->count_nonzero);

	++sem->count;
	mt_mutex_unlock(sem->count_lock);
}

// ---------------------------------------------------------------------------

int mt_init(void)
{
	static const schism_threads_backend_t *backends[] = {
		// ordered by preference
#ifdef SCHISM_MACOS
		&schism_threads_backend_macos,
#endif
#ifdef SCHISM_SDL2
		&schism_threads_backend_sdl2,
#endif
#ifdef SCHISM_SDL12
		&schism_threads_backend_sdl12,
#endif
		NULL,
	};

	int i;

	for (i = 0; backends[i]; i++) {
		if (backends[i]->init()) {
			mt_backend = backends[i];
			break;
		}
	}

	if (!mt_backend)
		return 0;

	return 1;
}

void mt_quit(void)
{
	if (mt_backend) {
		mt_backend->quit();
		mt_backend = NULL;
	}
}