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

#ifndef SCHISM_THREADS_H_
#define SCHISM_THREADS_H_

#include "headers.h"

typedef uintptr_t schism_thread_id_t;

/* private to each backend */
typedef struct schism_thread schism_thread_t;
typedef struct schism_mutex schism_mutex_t;
typedef struct schism_cond schism_cond_t;
typedef struct schism_semaphore schism_sem_t;

typedef int (*schism_thread_function_t)(void *userdata);

enum {
	BE_THREAD_PRIORITY_LOW = 0,
	BE_THREAD_PRIORITY_NORMAL,
	BE_THREAD_PRIORITY_HIGH,
	BE_THREAD_PRIORITY_TIME_CRITICAL,
};

schism_thread_t *mt_thread_create(schism_thread_function_t func, const char *name, void *userdata);
void mt_thread_wait(schism_thread_t *thread, int *status);
void mt_thread_set_priority(int priority);

schism_mutex_t *mt_mutex_create(void);
void mt_mutex_delete(schism_mutex_t *mutex);
void mt_mutex_lock(schism_mutex_t *mutex);
void mt_mutex_unlock(schism_mutex_t *mutex);

schism_cond_t *mt_cond_create(void);
void mt_cond_delete(schism_cond_t *cond);
void mt_cond_signal(schism_cond_t *cond);
void mt_cond_wait(schism_cond_t *cond, schism_mutex_t *mutex);

schism_sem_t *mt_semaphore_create(uint32_t initial_value);
void mt_semaphore_delete(schism_sem_t *sem);
void mt_semaphore_wait(schism_sem_t *sem);
void mt_semaphore_post(schism_sem_t *sem);

int mt_init(void);
void mt_quit(void);

#endif /* SCHISM_THREADS_H_ */
