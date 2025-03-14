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
#include "mt.h"
#include "mem.h"
#include "backend/timer.h"

#include "init.h"

static void (SDLCALL *sdl3_Delay)(uint32_t ms) = NULL;
static void (SDLCALL *sdl3_DelayNS)(uint64_t ns) = NULL;

static uint64_t (SDLCALL *sdl3_GetTicks)(void) = NULL;
static uint64_t (SDLCALL *sdl3_GetTicksNS)(void) = NULL;

static SDL_TimerID (SDLCALL *sdl3_AddTimer)(uint32_t ms, SDL_TimerCallback callback, void *param);

static timer_ticks_t sdl3_timer_ticks(void)
{
	return sdl3_GetTicks();
}

static timer_ticks_t sdl3_timer_ticks_us(void)
{
	return sdl3_GetTicksNS() / 1000ULL;
}

static void sdl3_timer_usleep(uint64_t us)
{
	sdl3_DelayNS(us * 1000ULL);
}

static void sdl3_timer_msleep(uint32_t ms)
{
	sdl3_Delay(ms);
}

//////////////////////////////////////////////////////////////////////////////
// oneshot timer

struct _sdl3_timer_oneshot_curry {
	void (*callback)(void *param);
	void *param;
};

static uint32_t SDLCALL _sdl3_timer_oneshot_callback(void *param, SCHISM_UNUSED SDL_TimerID timerID, SCHISM_UNUSED uint32_t interval)
{
	struct _sdl3_timer_oneshot_curry *curry = (struct _sdl3_timer_oneshot_curry *)param;

	curry->callback(curry->param);
	free(curry);

	return 0;
}

static int sdl3_timer_oneshot(uint32_t interval, void (*callback)(void *param), void *param)
{
	struct _sdl3_timer_oneshot_curry *curry = mem_alloc(sizeof(*curry));

	curry->callback = callback;
	curry->param = param;
	SDL_TimerID id = sdl3_AddTimer(interval, _sdl3_timer_oneshot_callback, curry);
	if (!id)
		free(curry);

	return !!id;
}

//////////////////////////////////////////////////////////////////////////////

static int sdl3_timer_load_syms(void)
{
	SCHISM_SDL3_SYM(Delay);
	SCHISM_SDL3_SYM(DelayNS);

	SCHISM_SDL3_SYM(GetTicks);
	SCHISM_SDL3_SYM(GetTicksNS);

	SCHISM_SDL3_SYM(AddTimer);

	return 0;
}

static int sdl3_timer_init(void)
{
	if (!sdl3_init())
		return 0;

	if (sdl3_timer_load_syms())
		return 0;

	return 1;
}

static void sdl3_timer_quit(void)
{
	sdl3_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_timer_backend_t schism_timer_backend_sdl3 = {
	.init = sdl3_timer_init,
	.quit = sdl3_timer_quit,

	.ticks = sdl3_timer_ticks,
	.ticks_us = sdl3_timer_ticks_us,
	.usleep = sdl3_timer_usleep,
	.msleep = sdl3_timer_msleep,

	.oneshot = sdl3_timer_oneshot,
};
