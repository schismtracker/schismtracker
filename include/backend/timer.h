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

#ifndef SCHISM_BACKEND_TIMER_H_
#define SCHISM_BACKEND_TIMER_H_

#include "../timer.h"

typedef struct {
	int (*init)(void);
	void (*quit)(void);

	timer_ticks_t (*ticks)(void);
	timer_ticks_t (*ticks_us)(void);
	void (*usleep)(uint64_t us);
	void (*msleep)(uint32_t ms);

	// returns 1 if a timer was successful
	// or 0 if we have to emulate it
	int (*oneshot)(uint32_t ms, void (*callback)(void *param), void *param);
} schism_timer_backend_t;

#ifdef SCHISM_WIN32
extern const schism_timer_backend_t schism_timer_backend_win32;
#endif
#ifdef SCHISM_SDL12
extern const schism_timer_backend_t schism_timer_backend_sdl12;
#endif
#ifdef SCHISM_SDL2
extern const schism_timer_backend_t schism_timer_backend_sdl2;
#endif
#ifdef SCHISM_SDL3
extern const schism_timer_backend_t schism_timer_backend_sdl3;
#endif

#endif /* SCHISM_BACKEND_TIMER_H_ */
