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

#ifndef SCHISM_BACKEND_THREADS_H_
#define SCHISM_BACKEND_THREADS_H_

#include "headers.h"

/* private to each backend */
typedef struct schism_thread schism_thread_t;
typedef struct schism_mutex schism_mutex_t;
typedef struct schism_cond schism_cond_t;

typedef int (*schism_thread_function_t)(void *userdata);

enum {
	BE_THREAD_PRIORITY_LOW = 0,
	BE_THREAD_PRIORITY_NORMAL,
	BE_THREAD_PRIORITY_HIGH,
	BE_THREAD_PRIORITY_TIME_CRITICAL,
};

#ifdef SCHISM_SDL2
# define be_thread_create sdl2_thread_create
# define be_thread_wait sdl2_thread_wait
# define be_thread_set_priority sdl2_thread_set_priority

# define be_mutex_create sdl2_mutex_create
# define be_mutex_delete sdl2_mutex_delete
# define be_mutex_lock sdl2_mutex_lock
# define be_mutex_unlock sdl2_mutex_unlock

# define be_cond_create sdl2_cond_create
# define be_cond_delete sdl2_cond_delete
# define be_cond_signal sdl2_cond_signal
# define be_cond_wait sdl2_cond_wait
#elif defined(SCHISM_SDL12)
# define be_thread_create sdl12_thread_create
# define be_thread_wait sdl12_thread_wait
# define be_thread_set_priority sdl12_thread_set_priority

# define be_mutex_create sdl12_mutex_create
# define be_mutex_delete sdl12_mutex_delete
# define be_mutex_lock sdl12_mutex_lock
# define be_mutex_unlock sdl12_mutex_unlock

# define be_cond_create sdl12_cond_create
# define be_cond_delete sdl12_cond_delete
# define be_cond_signal sdl12_cond_signal
# define be_cond_wait sdl12_cond_wait
#else
# error Threads are unimplemented on this backend
#endif

schism_thread_t *sdl12_thread_create(schism_thread_function_t func, const char *name, void *userdata);
void sdl12_thread_wait(schism_thread_t *thread, int *status);
void sdl12_thread_set_priority(int priority);

schism_mutex_t *sdl12_mutex_create(void);
void sdl12_mutex_delete(schism_mutex_t *mutex);
void sdl12_mutex_lock(schism_mutex_t *mutex);
void sdl12_mutex_unlock(schism_mutex_t *mutex);

schism_cond_t *sdl12_cond_create(void);
void sdl12_cond_delete(schism_cond_t *cond);
void sdl12_cond_signal(schism_cond_t *cond);
void sdl12_cond_wait(schism_cond_t *cond, schism_mutex_t *mutex);

schism_thread_t *sdl2_thread_create(schism_thread_function_t func, const char *name, void *userdata);
void sdl2_thread_wait(schism_thread_t *thread, int *status);
void sdl2_thread_set_priority(int priority);

schism_mutex_t *sdl2_mutex_create(void);
void sdl2_mutex_delete(schism_mutex_t *mutex);
void sdl2_mutex_lock(schism_mutex_t *mutex);
void sdl2_mutex_unlock(schism_mutex_t *mutex);

schism_cond_t *sdl2_cond_create(void);
void sdl2_cond_delete(schism_cond_t *cond);
void sdl2_cond_signal(schism_cond_t *cond);
void sdl2_cond_wait(schism_cond_t *cond, schism_mutex_t *mutex);

#endif /* SCHISM_BACKEND_THREADS_H_ */
