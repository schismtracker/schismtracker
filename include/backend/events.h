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

#ifndef SCHISM_BACKEND_CLIPPY_H_
#define SCHISM_BACKEND_CLIPPY_H_

#include "headers.h"
#include "keyboard.h"

#ifdef SCHISM_SDL2
# define be_pump_events sdl2_pump_events
# define be_event_mod_state sdl2_event_mod_state
#elif SCHISM_SDL12
# define be_pump_events sdl12_pump_events
# define be_event_mod_state sdl12_event_mod_state
#endif

void sdl2_pump_events(void);
void sdl12_pump_events(void);

schism_keymod_t sdl2_event_mod_state(void);
schism_keymod_t sdl12_event_mod_state(void);

#endif /* SCHISM_BACKEND_CLIPPY_H_ */
