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
#include "backend/threads.h"

#include <SDL.h>

/* ------------------------------------ */

struct schism_thread {
	SDL_Thread *thread;

	schism_thread_function_t func;
	void *userdata;
};

static int sdl2_dummy_thread_func(void *userdata)
{
	schism_thread_t *thread = userdata;

	return thread->func(thread->userdata);
}

schism_thread_t *sdl2_thread_create(schism_thread_function_t func, const char *name, void *userdata)
{
	schism_thread_t *thread = mem_alloc(sizeof(*thread));

	thread->func = func;
	thread->userdata = userdata;

	SDL_Thread *sdl_thread = SDL_CreateThread(sdl2_dummy_thread_func, name, thread);
	if (!sdl_thread) {
		free(thread);
		return NULL;
	}

	thread->thread = sdl_thread;

	return thread;
}

void sdl2_thread_wait(schism_thread_t *thread, int *status)
{
	SDL_WaitThread(thread->thread, status);
}

void sdl2_thread_set_priority(int priority)
{
	SDL_SetThreadPriority(priority);
}

/* -------------------------------------------------------------- */
/* mutexes */

struct schism_mutex {
	SDL_mutex *mutex;
};

schism_mutex_t *sdl2_mutex_create(void)
{
	schism_mutex_t *mutex = mem_alloc(sizeof(*mutex));

	mutex->mutex = SDL_CreateMutex();
	if (!mutex->mutex) {
		free(mutex);
		return NULL;
	}

	return mutex;
}

void sdl2_mutex_delete(schism_mutex_t *mutex)
{
	SDL_DestroyMutex(mutex->mutex);
	free(mutex);
}

void sdl2_mutex_lock(schism_mutex_t *mutex)
{
	SDL_LockMutex(mutex->mutex);
}

void sdl2_mutex_unlock(schism_mutex_t *mutex)
{
	SDL_UnlockMutex(mutex->mutex);
}

/* -------------------------------------------------------------- */

struct schism_cond {
	SDL_cond *cond;
};

schism_cond_t *sdl2_cond_create(void)
{
	schism_cond_t *cond = mem_alloc(sizeof(*cond));

	cond->cond = SDL_CreateCond();
	if (!cond->cond) {
		free(cond);
		return NULL;
	}

	return cond;
}

void sdl2_cond_delete(schism_cond_t *cond)
{
	SDL_DestroyCond(cond->cond);
	free(cond);
}

void sdl2_cond_signal(schism_cond_t *cond)
{
	SDL_CondSignal(cond->cond);
}

void sdl2_cond_wait(schism_cond_t *cond, schism_mutex_t *mutex)
{
	SDL_CondWait(cond->cond, mutex->mutex);
}
