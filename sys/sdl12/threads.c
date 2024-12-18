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

#include "init.h"

/* ------------------------------------ */

static SDL_Thread *(SDLCALL *sdl12_CreateThread)(int (*fn)(void *), void *data);
static void (SDLCALL *sdl12_WaitThread)(SDL_Thread *thread, int *status);
static uint32_t (SDLCALL *sdl12_ThreadID)(void);

struct schism_thread {
	SDL_Thread *thread;
	char *name;

	schism_thread_function_t func;
	void *userdata;
};

static int sdl12_dummy_thread_func(void *userdata)
{
	schism_thread_t *thread = userdata;

	return thread->func(thread->userdata);
}

static schism_thread_t *sdl12_thread_create(schism_thread_function_t func, const char *name, void *userdata)
{
	schism_thread_t *thread = mem_alloc(sizeof(*thread));

	thread->func = func;
	thread->name = name ? str_dup(name) : NULL;
	thread->userdata = userdata;

	SDL_Thread *sdl_thread = sdl12_CreateThread(sdl12_dummy_thread_func, thread);
	if (!sdl_thread) {
		free(thread);
		return NULL;
	}

	thread->thread = sdl_thread;

	return thread;
}

static void sdl12_thread_wait(schism_thread_t *thread, int *status)
{
	sdl12_WaitThread(thread->thread, status);
}

static void sdl12_thread_set_priority(SCHISM_UNUSED int priority)
{
	/* no-op */
}

// returns the current thread's ID
static schism_thread_id_t sdl12_thread_id(void)
{
	return sdl12_ThreadID();
}

/* -------------------------------------------------------------- */
/* mutexes */

static SDL_mutex *(SDLCALL *sdl12_CreateMutex)(void);
static void (SDLCALL *sdl12_DestroyMutex)(SDL_mutex *mutex);
static int (SDLCALL *sdl12_mutexP)(SDL_mutex *mutex);
static int (SDLCALL *sdl12_mutexV)(SDL_mutex *mutex);

struct schism_mutex {
	SDL_mutex *mutex;
};

static schism_mutex_t *sdl12_mutex_create(void)
{
	schism_mutex_t *mutex = mem_alloc(sizeof(*mutex));

	mutex->mutex = sdl12_CreateMutex();
	if (!mutex->mutex) {
		free(mutex);
		return NULL;
	}

	return mutex;
}

static void sdl12_mutex_delete(schism_mutex_t *mutex)
{
	sdl12_DestroyMutex(mutex->mutex);
	free(mutex);
}

static void sdl12_mutex_lock(schism_mutex_t *mutex)
{
	sdl12_mutexP(mutex->mutex);
}

static void sdl12_mutex_unlock(schism_mutex_t *mutex)
{
	sdl12_mutexV(mutex->mutex);
}

/* -------------------------------------------------------------- */

static SDL_cond *(SDLCALL *sdl12_CreateCond)(void);
static void (SDLCALL *sdl12_DestroyCond)(SDL_cond *cond);
static int (SDLCALL *sdl12_CondSignal)(SDL_cond *cond);
static int (SDLCALL *sdl12_CondWait)(SDL_cond *cond, SDL_mutex *mut);

struct schism_cond {
	SDL_cond *cond;
};

static schism_cond_t *sdl12_cond_create(void)
{
	schism_cond_t *cond = mem_alloc(sizeof(*cond));

	cond->cond = sdl12_CreateCond();
	if (!cond->cond) {
		free(cond);
		return NULL;
	}

	return cond;
}

static void sdl12_cond_delete(schism_cond_t *cond)
{
	sdl12_DestroyCond(cond->cond);
	free(cond);
}

static void sdl12_cond_signal(schism_cond_t *cond)
{
	sdl12_CondSignal(cond->cond);
}

static void sdl12_cond_wait(schism_cond_t *cond, schism_mutex_t *mutex)
{
	sdl12_CondWait(cond->cond, mutex->mutex);
}

//////////////////////////////////////////////////////////////////////////////

static int sdl12_threads_load_syms(void)
{
	SCHISM_SDL12_SYM(CreateThread);
	SCHISM_SDL12_SYM(WaitThread);
	SCHISM_SDL12_SYM(ThreadID);

	SCHISM_SDL12_SYM(CreateMutex);
	SCHISM_SDL12_SYM(DestroyMutex);
	SCHISM_SDL12_SYM(mutexP);
	SCHISM_SDL12_SYM(mutexV);

	SCHISM_SDL12_SYM(CreateCond);
	SCHISM_SDL12_SYM(DestroyCond);
	SCHISM_SDL12_SYM(CondSignal);
	SCHISM_SDL12_SYM(CondWait);

	return 0;
}

static int sdl12_threads_init(void)
{
	if (!sdl12_init())
		return 0;

	if (sdl12_threads_load_syms())
		return 0;

	return 1;
}

static void sdl12_threads_quit(void)
{
	sdl12_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_threads_backend_t schism_threads_backend_sdl12 = {
	.init = sdl12_threads_init,
	.quit = sdl12_threads_quit,

	.flags = SCHISM_THREADS_BACKEND_SUPPORTS_MUTEX | SCHISM_THREADS_BACKEND_SUPPORTS_COND,

	.thread_create = sdl12_thread_create,
	.thread_wait = sdl12_thread_wait,
	.thread_set_priority = sdl12_thread_set_priority,
	.thread_id = sdl12_thread_id,

	.mutex_create = sdl12_mutex_create,
	.mutex_delete = sdl12_mutex_delete,
	.mutex_lock = sdl12_mutex_lock,
	.mutex_unlock = sdl12_mutex_unlock,

	.cond_create = sdl12_cond_create,
	.cond_delete = sdl12_cond_delete,
	.cond_signal = sdl12_cond_signal,
	.cond_wait = sdl12_cond_wait,
};
