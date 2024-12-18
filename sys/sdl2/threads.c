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

static SDL_Thread *(SDLCALL *sdl2_CreateThread)(SDL_ThreadFunction fn, const char *name, void *data) = NULL;
static void (SDLCALL *sdl2_WaitThread)(SDL_Thread * thread, int *status) = NULL;
static int (SDLCALL *sdl2_SetThreadPriority)(SDL_ThreadPriority priority) = NULL;
static SDL_threadID (SDLCALL *sdl2_ThreadID)(void) = NULL;

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

	SDL_Thread *sdl_thread = sdl2_CreateThread(sdl2_dummy_thread_func, name, thread);
	if (!sdl_thread) {
		free(thread);
		return NULL;
	}

	thread->thread = sdl_thread;

	return thread;
}

void sdl2_thread_wait(schism_thread_t *thread, int *status)
{
	sdl2_WaitThread(thread->thread, status);
}

void sdl2_thread_set_priority(int priority)
{
	sdl2_SetThreadPriority(priority);
}

// returns the current thread's ID
static schism_thread_id_t sdl2_thread_id(void)
{
	return sdl2_ThreadID();
}

/* -------------------------------------------------------------- */
/* mutexes */

static SDL_mutex *(SDLCALL *sdl2_CreateMutex)(void) = NULL;
static void (SDLCALL *sdl2_DestroyMutex)(SDL_mutex * mutex) = NULL;
static int (SDLCALL *sdl2_LockMutex)(SDL_mutex * mutex) = NULL;
static int (SDLCALL *sdl2_UnlockMutex)(SDL_mutex * mutex) = NULL;

struct schism_mutex {
	SDL_mutex *mutex;
};

schism_mutex_t *sdl2_mutex_create(void)
{
	schism_mutex_t *mutex = mem_alloc(sizeof(*mutex));

	mutex->mutex = sdl2_CreateMutex();
	if (!mutex->mutex) {
		free(mutex);
		return NULL;
	}

	return mutex;
}

void sdl2_mutex_delete(schism_mutex_t *mutex)
{
	sdl2_DestroyMutex(mutex->mutex);
	free(mutex);
}

void sdl2_mutex_lock(schism_mutex_t *mutex)
{
	sdl2_LockMutex(mutex->mutex);
}

void sdl2_mutex_unlock(schism_mutex_t *mutex)
{
	sdl2_UnlockMutex(mutex->mutex);
}

/* -------------------------------------------------------------- */

static SDL_cond *(SDLCALL *sdl2_CreateCond)(void) = NULL;
static void (SDLCALL *sdl2_DestroyCond)(SDL_cond *cond) = NULL;
static int (SDLCALL *sdl2_CondSignal)(SDL_cond *cond) = NULL;
static int (SDLCALL *sdl2_CondWait)(SDL_cond *cond, SDL_mutex *mutex) = NULL;

struct schism_cond {
	SDL_cond *cond;
};

schism_cond_t *sdl2_cond_create(void)
{
	schism_cond_t *cond = mem_alloc(sizeof(*cond));

	cond->cond = sdl2_CreateCond();
	if (!cond->cond) {
		free(cond);
		return NULL;
	}

	return cond;
}

void sdl2_cond_delete(schism_cond_t *cond)
{
	sdl2_DestroyCond(cond->cond);
	free(cond);
}

void sdl2_cond_signal(schism_cond_t *cond)
{
	sdl2_CondSignal(cond->cond);
}

void sdl2_cond_wait(schism_cond_t *cond, schism_mutex_t *mutex)
{
	sdl2_CondWait(cond->cond, mutex->mutex);
}

//////////////////////////////////////////////////////////////////////////////

static int sdl2_threads_load_syms(void)
{
	SCHISM_SDL2_SYM(CreateThread);
	SCHISM_SDL2_SYM(WaitThread);
	SCHISM_SDL2_SYM(SetThreadPriority);
	SCHISM_SDL2_SYM(ThreadID);

	SCHISM_SDL2_SYM(CreateMutex);
	SCHISM_SDL2_SYM(DestroyMutex);
	SCHISM_SDL2_SYM(LockMutex);
	SCHISM_SDL2_SYM(UnlockMutex);

	SCHISM_SDL2_SYM(CreateCond);
	SCHISM_SDL2_SYM(DestroyCond);
	SCHISM_SDL2_SYM(CondSignal);
	SCHISM_SDL2_SYM(CondWait);

	return 0;
}

static int sdl2_threads_init(void)
{
	if (!sdl2_init())
		return 0;

	if (sdl2_threads_load_syms())
		return 0;

	return 1;
}

static void sdl2_threads_quit(void)
{
	sdl2_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_threads_backend_t schism_threads_backend_sdl2 = {
	.init = sdl2_threads_init,
	.quit = sdl2_threads_quit,

	.flags = SCHISM_THREADS_BACKEND_SUPPORTS_MUTEX | SCHISM_THREADS_BACKEND_SUPPORTS_COND,

	.thread_create = sdl2_thread_create,
	.thread_wait = sdl2_thread_wait,
	.thread_set_priority = sdl2_thread_set_priority,
	.thread_id = sdl2_thread_id,

	.mutex_create = sdl2_mutex_create,
	.mutex_delete = sdl2_mutex_delete,
	.mutex_lock = sdl2_mutex_lock,
	.mutex_unlock = sdl2_mutex_unlock,

	.cond_create = sdl2_cond_create,
	.cond_delete = sdl2_cond_delete,
	.cond_signal = sdl2_cond_signal,
	.cond_wait = sdl2_cond_wait,
};
