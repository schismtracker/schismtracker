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

#include "loadso.h"

#include "backend/timer.h"

static const schism_timer_backend_t *backend = NULL;

schism_ticks_t timer_ticks(void)
{
	return backend->ticks();
}

void timer_usleep(uint64_t usec)
{
	// FIXME move this into a sys/posix/timer.c
#if defined(HAVE_NANOSLEEP)
	// nanosleep is newer and preferred to usleep
	struct timespec s = {
		.tv_sec = usec / 1000000,
		.tv_nsec = (usec % 1000000) * 1000,
	};

	while (nanosleep(&s, &s) == -1);
#elif defined(HAVE_USLEEP)
	while (usec) {
		// useconds_t is only guaranteed to contain 0-1000000
		useconds_t t = MIN(usec, 1000000);
		usleep(t);
		usec -= t;
	}
#else
	backend->usleep(usec);
#endif
}

int timer_init(void)
{
	static const schism_timer_backend_t *backends[] = {
		// ordered by preference
#ifdef SCHISM_WIN32
		&schism_timer_backend_win32,
#endif
#ifdef SCHISM_SDL2
		&schism_timer_backend_sdl2,
#endif
#ifdef SCHISM_SDL12
		&schism_timer_backend_sdl12,
#endif
		NULL,
	};

	int i;

	for (i = 0; backends[i]; i++) {
		backend = backends[i];
		if (backend->init())
			break;

		backend = NULL;
	}

	if (!backend)
		return 0;

	return 1;
}

void timer_quit(void)
{
	if (backend) {
		backend->quit();
		backend = NULL;
	}
}

