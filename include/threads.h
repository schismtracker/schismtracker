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

typedef uintptr_t mt_thread_id_t;

/* private to each backend */
typedef struct mt_thread mt_thread_t;
typedef struct mt_mutex mt_mutex_t;
typedef struct mt_cond mt_cond_t;

typedef int (*schism_thread_function_t)(void *userdata);

enum {
	MT_THREAD_PRIORITY_LOW = 0,
	MT_THREAD_PRIORITY_NORMAL,
	MT_THREAD_PRIORITY_HIGH,
	MT_THREAD_PRIORITY_TIME_CRITICAL,
};

mt_thread_t *mt_thread_create(schism_thread_function_t func, const char *name, void *userdata);
void mt_thread_wait(mt_thread_t *thread, int *status);
void mt_thread_set_priority(int priority);
mt_thread_id_t mt_thread_id(void);

mt_mutex_t *mt_mutex_create(void);
void mt_mutex_delete(mt_mutex_t *mutex);
void mt_mutex_lock(mt_mutex_t *mutex);
void mt_mutex_unlock(mt_mutex_t *mutex);

mt_cond_t *mt_cond_create(void);
void mt_cond_delete(mt_cond_t *cond);
void mt_cond_signal(mt_cond_t *cond);
void mt_cond_wait(mt_cond_t *cond, mt_mutex_t *mutex);
void mt_cond_wait_timeout(mt_cond_t *cond, mt_mutex_t *mutex, uint32_t timeout);

int mt_init(void);
void mt_quit(void);

#endif /* SCHISM_THREADS_H_ */
