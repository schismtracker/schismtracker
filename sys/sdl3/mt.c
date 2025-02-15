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
#include "backend/mt.h"

#include "init.h"

/* ------------------------------------ */

static SDL_Thread *(SDLCALL *sdl3_CreateThreadRuntime)(SDL_ThreadFunction fn, const char *name, void *data, SDL_FunctionPointer begin, SDL_FunctionPointer end) = NULL;
static void (SDLCALL *sdl3_WaitThread)(SDL_Thread * thread, int *status) = NULL;
static bool (SDLCALL *sdl3_SetCurrentThreadPriority)(SDL_ThreadPriority priority) = NULL;
static Uint64 (SDLCALL *sdl3_GetCurrentThreadID)(void) = NULL;

struct mt_thread {
	SDL_Thread *thread;

	schism_thread_function_t func;
	void *userdata;
};

static int sdl3_dummy_thread_func(void *userdata)
{
	mt_thread_t *thread = userdata;

	return thread->func(thread->userdata);
}

mt_thread_t *sdl3_thread_create(schism_thread_function_t func, const char *name, void *userdata)
{
	mt_thread_t *thread = mem_alloc(sizeof(*thread));

	thread->func = func;
	thread->userdata = userdata;

	/* ew */
	SDL_Thread *sdl_thread = sdl3_CreateThreadRuntime(sdl3_dummy_thread_func, name, thread, (SDL_FunctionPointer)(SDL_BeginThreadFunction), (SDL_FunctionPointer)(SDL_EndThreadFunction));
	if (!sdl_thread) {
		free(thread);
		return NULL;
	}

	thread->thread = sdl_thread;

	return thread;
}

void sdl3_thread_wait(mt_thread_t *thread, int *status)
{
	sdl3_WaitThread(thread->thread, status);
	free(thread);
}

void sdl3_thread_set_priority(int priority)
{
	// !!! FIXME this should use a switch statement,
	// or this API should be removed altogether
	sdl3_SetCurrentThreadPriority(priority);
}

// returns the current thread's ID
static mt_thread_id_t sdl3_thread_id(void)
{
	return sdl3_GetCurrentThreadID();
}

/* -------------------------------------------------------------- */
/* mutexes */

static SDL_Mutex *(SDLCALL *sdl3_CreateMutex)(void) = NULL;
static void (SDLCALL *sdl3_DestroyMutex)(SDL_Mutex * mutex) = NULL;
static void (SDLCALL *sdl3_LockMutex)(SDL_Mutex * mutex) = NULL;
static void (SDLCALL *sdl3_UnlockMutex)(SDL_Mutex * mutex) = NULL;

struct mt_mutex {
	SDL_Mutex *mutex;
};

mt_mutex_t *sdl3_mutex_create(void)
{
	mt_mutex_t *mutex = mem_alloc(sizeof(*mutex));

	mutex->mutex = sdl3_CreateMutex();
	if (!mutex->mutex) {
		free(mutex);
		return NULL;
	}

	return mutex;
}

void sdl3_mutex_delete(mt_mutex_t *mutex)
{
	sdl3_DestroyMutex(mutex->mutex);
	free(mutex);
}

void sdl3_mutex_lock(mt_mutex_t *mutex)
{
	sdl3_LockMutex(mutex->mutex);
}

void sdl3_mutex_unlock(mt_mutex_t *mutex)
{
	sdl3_UnlockMutex(mutex->mutex);
}

/* -------------------------------------------------------------- */

static SDL_Condition *(SDLCALL *sdl3_CreateCondition)(void) = NULL;
static void (SDLCALL *sdl3_DestroyCondition)(SDL_Condition *cond) = NULL;
static void (SDLCALL *sdl3_SignalCondition)(SDL_Condition *cond) = NULL;
static void (SDLCALL *sdl3_WaitCondition)(SDL_Condition *cond, SDL_Mutex *mutex) = NULL;
static bool (SDLCALL *sdl3_WaitConditionTimeout)(SDL_Condition *cond, SDL_Mutex *mutex, int32_t timeout) = NULL;

struct mt_cond {
	SDL_Condition *cond;
};

mt_cond_t *sdl3_cond_create(void)
{
	mt_cond_t *cond = mem_alloc(sizeof(*cond));

	cond->cond = sdl3_CreateCondition();
	if (!cond->cond) {
		free(cond);
		return NULL;
	}

	return cond;
}

void sdl3_cond_delete(mt_cond_t *cond)
{
	sdl3_DestroyCondition(cond->cond);
	free(cond);
}

void sdl3_cond_signal(mt_cond_t *cond)
{
	sdl3_SignalCondition(cond->cond);
}

void sdl3_cond_wait(mt_cond_t *cond, mt_mutex_t *mutex)
{
	sdl3_WaitCondition(cond->cond, mutex->mutex);
}

void sdl3_cond_wait_timeout(mt_cond_t *cond, mt_mutex_t *mutex, uint32_t timeout)
{
	sdl3_WaitConditionTimeout(cond->cond, mutex->mutex, timeout);
}

//////////////////////////////////////////////////////////////////////////////

static int sdl3_threads_load_syms(void)
{
	SCHISM_SDL3_SYM(CreateThreadRuntime);
	SCHISM_SDL3_SYM(WaitThread);
	SCHISM_SDL3_SYM(SetCurrentThreadPriority);
	SCHISM_SDL3_SYM(GetCurrentThreadID);

	SCHISM_SDL3_SYM(CreateMutex);
	SCHISM_SDL3_SYM(DestroyMutex);
	SCHISM_SDL3_SYM(LockMutex);
	SCHISM_SDL3_SYM(UnlockMutex);

	SCHISM_SDL3_SYM(CreateCondition);
	SCHISM_SDL3_SYM(DestroyCondition);
	SCHISM_SDL3_SYM(SignalCondition);
	SCHISM_SDL3_SYM(WaitCondition);
	SCHISM_SDL3_SYM(WaitConditionTimeout);

	return 0;
}

static int sdl3_threads_init(void)
{
	if (!sdl3_init())
		return 0;

	if (sdl3_threads_load_syms())
		return 0;

	return 1;
}

static void sdl3_threads_quit(void)
{
	sdl3_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_mt_backend_t schism_mt_backend_sdl3 = {
	.init = sdl3_threads_init,
	.quit = sdl3_threads_quit,

	.thread_create = sdl3_thread_create,
	.thread_wait = sdl3_thread_wait,
	.thread_set_priority = sdl3_thread_set_priority,
	.thread_id = sdl3_thread_id,

	.mutex_create = sdl3_mutex_create,
	.mutex_delete = sdl3_mutex_delete,
	.mutex_lock = sdl3_mutex_lock,
	.mutex_unlock = sdl3_mutex_unlock,

	.cond_create = sdl3_cond_create,
	.cond_delete = sdl3_cond_delete,
	.cond_signal = sdl3_cond_signal,
	.cond_wait = sdl3_cond_wait,
	.cond_wait_timeout = sdl3_cond_wait_timeout,
};
