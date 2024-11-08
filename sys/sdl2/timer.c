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
#include "backend/timer.h"

#include <SDL.h>

schism_ticks_t sdl2_timer_ticks(void)
{
#if SDL_VERSION_ATLEAST(2, 0, 18)
	return SDL_GetTicks64();
#else
	return SDL_GetTicks();
#endif
}

int sdl2_timer_ticks_passed(schism_ticks_t a, schism_ticks_t b)
{
#if SDL_VERSION_ATLEAST(2, 0, 18)
	return (a >= b);
#else
	return ((int32_t)(b - a) <= 0);
#endif
}

void sdl2_delay(uint32_t ms)
{
	SDL_Delay(ms);
}
