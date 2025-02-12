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

#ifndef SCHISM_BACKEND_EVENTS_H_
#define SCHISM_BACKEND_EVENTS_H_

#include "../keyboard.h"

// flags
enum {
	// Enable this flag if the backend sends its own key repeats
	// If it doesn't we fall back to our internal representation
	// and getting key repeat rates from the OS.
	SCHISM_EVENTS_BACKEND_HAS_KEY_REPEAT = (1 << 0),
};

typedef struct {
	// returns 1 if succeeded, 0 if failed
	int (*init)(void);
	void (*quit)(void);

	// flags defined above
	uint32_t flags;

	void (*pump_events)(void);
	schism_keymod_t (*keymod_state)(void);
} schism_events_backend_t;

#ifdef SCHISM_SDL12
extern const schism_events_backend_t schism_events_backend_sdl12;
#endif

#ifdef SCHISM_SDL2
extern const schism_events_backend_t schism_events_backend_sdl2;
#endif

#ifdef SCHISM_SDL3
extern const schism_events_backend_t schism_events_backend_sdl3;
#endif

#endif /* SCHISM_BACKEND_EVENTS_H_ */
