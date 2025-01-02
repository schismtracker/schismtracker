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
#include "threads.h"
#include "backend/timer.h"

#include "init.h"

static int (SDLCALL *sdl2_InitSubSystem)(Uint32 flags) = NULL;
static void (SDLCALL *sdl2_QuitSubSystem)(Uint32 flags) = NULL;

static void (SDLCALL *sdl2_Delay)(uint32_t ms) = NULL;
static uint32_t (SDLCALL *sdl2_GetTicks)(void) = NULL;

// Introduced in SDL 2.0.18
static uint64_t (SDLCALL *sdl2_GetTicks64)(void) = NULL;

static int sdl2_have_timer64 = 0;

// ahhh!
static schism_mutex_t *last_known_ticks_mutex = NULL;
static uint32_t last_known_ticks = 0;
static uint32_t ticks_overflow = 0;

static schism_ticks_t sdl2_timer_ticks(void)
{
#if defined(SDL2_DYNAMIC_LOAD) || SDL_VERSION_ATLEAST(2, 0, 18)
	if (sdl2_GetTicks64)
		return sdl2_GetTicks64();
#endif

	schism_ticks_t ticks = sdl2_GetTicks();

	mt_mutex_lock(last_known_ticks_mutex);

	if (ticks < last_known_ticks)
		ticks_overflow++;
	last_known_ticks = ticks;

	ticks |= ((uint64_t)ticks_overflow << 32);

	mt_mutex_unlock(last_known_ticks_mutex);

	return ticks;
}

static int sdl2_timer_ticks_passed(schism_ticks_t a, schism_ticks_t b)
{
	return (a >= b);
}

static void sdl2_timer_delay(uint32_t ms)
{
	sdl2_Delay(ms);
}

//////////////////////////////////////////////////////////////////////////////

static int sdl2_timer_load_syms(void)
{
	SCHISM_SDL2_SYM(InitSubSystem);
	SCHISM_SDL2_SYM(QuitSubSystem);

	SCHISM_SDL2_SYM(GetTicks);
	SCHISM_SDL2_SYM(Delay);

	return 0;
}

static int sdl2_timer64_load_syms(void)
{
#if defined(SDL2_DYNAMIC_LOAD) || SDL_VERSION_ATLEAST(2, 0, 18)
	SCHISM_SDL2_SYM(GetTicks64);

	return 0;
#else
	return -1;
#endif
}

static int sdl2_timer_init(void)
{
	if (!sdl2_init())
		return 0;

	if (sdl2_timer_load_syms())
		return 0;

	if (!sdl2_timer64_load_syms()) {
		sdl2_have_timer64 = 1;
	} else {
		last_known_ticks_mutex = mt_mutex_create();
		if (!last_known_ticks_mutex)
			return 0;
	}

	if (sdl2_InitSubSystem(SDL_INIT_TIMER) < 0)
		return 0;

	return 1;
}

static void sdl2_timer_quit(void)
{
	sdl2_QuitSubSystem(SDL_INIT_TIMER);

	sdl2_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_timer_backend_t schism_timer_backend_sdl2 = {
	.init = sdl2_timer_init,
	.quit = sdl2_timer_quit,

	.ticks = sdl2_timer_ticks,
	.ticks_passed = sdl2_timer_ticks_passed,
	.delay = sdl2_timer_delay,
};
