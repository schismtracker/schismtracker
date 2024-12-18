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
#include "osdefs.h"

#include "init.h"

#include <SDL.h>

static int (SDLCALL *sdl12_Init)(Uint32 flags);
static void (SDLCALL *sdl12_Quit)(void);
static char *(SDLCALL *sdl12_GetError)(void);

static int load_sdl12_syms(void);

#ifdef SDL12_DYNAMIC_LOAD

#include "loadso.h"

static void *sdl12_dltrick_handle_ = NULL;

static void sdl12_dlend(void)
{
	if (sdl12_dltrick_handle_) {
		loadso_object_unload(sdl12_dltrick_handle_);
		sdl12_dltrick_handle_ = NULL;
	}
}

static int sdl12_dlinit(void)
{
	int i;

	// already have it?
	if (sdl12_dltrick_handle_)
		return 0;

	sdl12_dltrick_handle_ = library_load("SDL-1.2", 0, 0);
	if (!sdl12_dltrick_handle_) {
		sdl12_dltrick_handle_ = library_load("SDL", 0, 0);
		if (!sdl12_dltrick_handle_)
			return -1;
	}

	int retval = load_sdl12_syms();
	if (retval < 0)
		sdl12_dlend();

	return retval;
}

SCHISM_STATIC_ASSERT(sizeof(void (*)) == sizeof(void *), "dynamic loading code assumes function pointer and void pointer are of equivalent size");

int sdl12_load_sym(const char *fn, void *addr)
{
	void *func = loadso_function_load(sdl12_dltrick_handle_, fn);
	if (!func)
		return 0;

	memcpy(addr, &func, sizeof(void *));

	return 1;
}

#else

static int sdl12_dlinit(void)
{
	load_sdl12_syms();
	return 0;
}

#define sdl12_dlend() // nothing

#endif

static int load_sdl12_syms(void)
{
	SCHISM_SDL12_SYM(Init);
	SCHISM_SDL12_SYM(Quit);
	SCHISM_SDL12_SYM(GetError);

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

// this is used so that SDL_Quit is only called
// once every backend is done
static int roll = 0;

// returns non-zero on success or zero on error
int sdl12_init(void)
{
	if (!roll) {
		if (sdl12_dlinit())
			return 0;

		// the subsystems are initialized by the actual backends
		int r = sdl12_Init(0);
		if (r < 0) {
			os_show_message_box("Error initializing SDL 1.2:", sdl12_get_error());
			return 0;
		}
	}
	roll++;
	return roll;
}

void sdl12_quit(void)
{
	if (roll > 0)
		roll--;

	if (roll == 0) {
		sdl12_Quit();
		sdl12_dlend();
	}
}

const char *sdl12_get_error(void)
{
	if (sdl12_GetError)
		return sdl12_GetError();

	return "";
}
