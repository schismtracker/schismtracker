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

#include "../threads.h"

typedef struct {
	int (*init)(void);
	void (*quit)(void);

	// Creates a thread. `name` is optional, and if passed may be used to set
	// the description of a thread in the OS.
	mt_thread_t *(*thread_create)(schism_thread_function_t func, const char *name, void *userdata);
	// Waits for a thread to exit, then cleans it up.
	void (*thread_wait)(mt_thread_t *thread, int *status);
	// Sets the current threads priority.
	void (*thread_set_priority)(int priority);
	// Get the current thread's unique ID. (mostly unused)
	mt_thread_id_t (*thread_id)(void);

	mt_mutex_t *(*mutex_create)(void);
	void (*mutex_delete)(mt_mutex_t *mutex);
	void (*mutex_lock)(mt_mutex_t *mutex);
	void (*mutex_unlock)(mt_mutex_t *mutex);

	mt_cond_t *(*cond_create)(void);
	void (*cond_delete)(mt_cond_t *cond);
	void (*cond_signal)(mt_cond_t *cond);
	void (*cond_wait)(mt_cond_t *cond, mt_mutex_t *mutex);
	void (*cond_wait_timeout)(mt_cond_t *cond, mt_mutex_t *mutex, uint32_t timeout);
} schism_threads_backend_t;

#ifdef SCHISM_MACOS
extern const schism_threads_backend_t schism_threads_backend_macos;
#endif
#ifdef SCHISM_SDL12
extern const schism_threads_backend_t schism_threads_backend_sdl12;
#endif
#ifdef SCHISM_SDL2
extern const schism_threads_backend_t schism_threads_backend_sdl2;
#endif

#endif /* SCHISM_BACKEND_THREADS_H_ */
