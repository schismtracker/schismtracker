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
#include "osdefs.h" // os_show_message_box

#include "init.h"

static bool (SDLCALL *sdl3_Init)(SDL_InitFlags flags);
static void (SDLCALL *sdl3_Quit)(void);
static const char *(SDLCALL *sdl3_GetError)(void);
static int (SDLCALL *sdl3_GetVersion)(void);

static int load_sdl3_syms(void);

#ifdef SDL3_DYNAMIC_LOAD

#include "loadso.h"

static void *sdl3_dltrick_handle_ = NULL;

static void sdl3_dlend(void)
{
	if (sdl3_dltrick_handle_) {
		loadso_object_unload(sdl3_dltrick_handle_);
		sdl3_dltrick_handle_ = NULL;
	}
}

static int sdl3_dlinit(void)
{
	int i;

	// already have it?
	if (sdl3_dltrick_handle_)
		return 0;

	sdl3_dltrick_handle_ = library_load("SDL3-3.0", 0, 0);
	if (!sdl3_dltrick_handle_) {
		sdl3_dltrick_handle_ = library_load("SDL3", 0, 0);
		if (!sdl3_dltrick_handle_)
			return -1;
	}

	int retval = load_sdl3_syms();
	if (retval < 0)
		sdl3_dlend();

	return retval;
}

SCHISM_STATIC_ASSERT(sizeof(void (*)) == sizeof(void *), "dynamic loading code assumes function pointer and void pointer are of equivalent size");

int sdl3_load_sym(const char *fn, void *addr)
{
	void *func = loadso_function_load(sdl3_dltrick_handle_, fn);
	if (!func) {
		printf("no such function: %s\n", fn);
		return 0;
	}

	memcpy(addr, &func, sizeof(void *));

	return 1;
}

#else

static int sdl3_dlinit(void)
{
	load_sdl3_syms();
	return 0;
}

#define sdl3_dlend() // nothing

#endif

static int load_sdl3_syms(void)
{
	SCHISM_SDL3_SYM(Init);
	SCHISM_SDL3_SYM(Quit);
	SCHISM_SDL3_SYM(GetError);
	SCHISM_SDL3_SYM(GetVersion);

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

// this is used so that SDL_Quit is only called
// once every backend is done
static int roll = 0;

// returns non-zero on success or zero on error
int sdl3_init(void)
{
	if (!roll) {
		if (sdl3_dlinit())
			return 0;

		// versions prior to the first stable release (3.2.0) have not been tested at all,
		// and will likely have serious issues if we even attempt to use them, so punt,
		// and fallback to an older version of SDL, possibly sdl2-compat or sdl12-compat.
		const int ver = sdl3_GetVersion();
		if (!SCHISM_SEMVER_ATLEAST(3, 2, 0, SDL_VERSIONNUM_MAJOR(ver), SDL_VERSIONNUM_MINOR(ver), SDL_VERSIONNUM_MICRO(ver)))
			return 0;

		// the subsystems are initialized by the actual backends
		int r = sdl3_Init(0);
		if (!r) {
			os_show_message_box("SDL3: SDL_Init failed!", sdl3_GetError());
			return 0;
		}
	}
	roll++;
	return roll;
}

void sdl3_quit(void)
{
	if (roll > 0)
		roll--;

	if (roll == 0) {
		sdl3_Quit();
		sdl3_dlend();
	}
}
