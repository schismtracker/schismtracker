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

static int (SDLCALL *sdl12_InitSubSystem)(Uint32 flags);
static void (SDLCALL *sdl12_QuitSubSystem)(Uint32 flags);
static uint32_t (SDLCALL *sdl12_GetTicks)(void);
static void (SDLCALL *sdl12_Delay)(uint32_t ms);

static schism_mutex_t *last_known_ticks_mutex = NULL;
static uint32_t last_known_ticks = 0;
static uint32_t ticks_overflow = 0;

static schism_ticks_t sdl12_timer_ticks(void)
{
	schism_ticks_t ticks = sdl12_GetTicks();

	mt_mutex_lock(last_known_ticks_mutex);

	if (ticks < last_known_ticks)
		ticks_overflow++;
	last_known_ticks = ticks;

	ticks |= ((uint64_t)ticks_overflow << 32);

	mt_mutex_unlock(last_known_ticks_mutex);

	return ticks;
}

static int sdl12_timer_ticks_passed(schism_ticks_t a, schism_ticks_t b)
{
	return (a >= b);
}

static void sdl12_delay(uint32_t ms)
{
	sdl12_Delay(ms);
}

//////////////////////////////////////////////////////////////////////////////

static int sdl12_timer_load_syms(void)
{
	SCHISM_SDL12_SYM(InitSubSystem);
	SCHISM_SDL12_SYM(QuitSubSystem);
	SCHISM_SDL12_SYM(GetTicks);
	SCHISM_SDL12_SYM(Delay);

	return 0;
}

static int sdl12_timer_init(void)
{
	if (!sdl12_init())
		return 0;

	if (sdl12_timer_load_syms())
		return 0;

	if (sdl12_InitSubSystem(SDL_INIT_TIMER) < 0)
		return 0;

	last_known_ticks_mutex = mt_mutex_create();
	if (!last_known_ticks_mutex)
		return 0;

	return 1;
}

static void sdl12_timer_quit(void)
{
	sdl12_QuitSubSystem(SDL_INIT_TIMER);

	sdl12_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_timer_backend_t schism_timer_backend_sdl12 = {
	.init = sdl12_timer_init,
	.quit = sdl12_timer_quit,

	.ticks = sdl12_timer_ticks,
	.ticks_passed = sdl12_timer_ticks_passed,
	.delay = sdl12_delay,
};
