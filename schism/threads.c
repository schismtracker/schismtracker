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

#include "backend/threads.h"

static const schism_threads_backend_t *mt_backend = NULL;

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

schism_mutex_t *mt_mutex_create(void)
{
	return mt_backend->mutex_create();
}

void mt_mutex_delete(schism_mutex_t *mutex)
{
	mt_backend->mutex_delete(mutex);
}

void mt_mutex_lock(schism_mutex_t *mutex)
{
	mt_backend->mutex_lock(mutex);
}

void mt_mutex_unlock(schism_mutex_t *mutex)
{
	mt_backend->mutex_unlock(mutex);
}

schism_cond_t *mt_cond_create(void)
{
	return mt_backend->cond_create();
}

void mt_cond_delete(schism_cond_t *cond)
{
	mt_backend->cond_delete(cond);
}

void mt_cond_signal(schism_cond_t *cond)
{
	mt_backend->cond_signal(cond);
}

void mt_cond_wait(schism_cond_t *cond, schism_mutex_t *mutex)
{
	mt_backend->cond_wait(cond, mutex);
}

int mt_init(void)
{
	static const schism_threads_backend_t *backends[] = {
		// ordered by preference
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