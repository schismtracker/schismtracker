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

#include "init.h"

#include "headers.h" /* always include this one first, kthx */
#include "backend/clippy.h"
#include "mem.h"

static int enable_primary_selection = 0;

#if defined(SDL2_DYNAMIC_LOAD) || SDL_VERSION_ATLEAST(2, 26, 0)
static SDL_bool (SDLCALL *sdl2_HasPrimarySelectionText)(void);
static int (SDLCALL *sdl2_SetPrimarySelectionText)(const char *text);
static char * (SDLCALL *sdl2_GetPrimarySelectionText)(void);
#endif

static SDL_bool (SDLCALL *sdl2_HasClipboardText)(void);
static int (SDLCALL *sdl2_SetClipboardText)(const char *text);
static char * (SDLCALL *sdl2_GetClipboardText)(void);

static void (SDLCALL *sdl2_free)(void *);

static int sdl2_clippy_have_selection(void)
{
#if defined(SDL2_DYNAMIC_LOAD) || SDL_VERSION_ATLEAST(2, 26, 0)
	if (enable_primary_selection)
		return sdl2_HasPrimarySelectionText();
#endif

	return 0;
}

static int sdl2_clippy_have_clipboard(void)
{
	return sdl2_HasClipboardText();
}

static void sdl2_clippy_set_selection(const char *text)
{
#if defined(SDL2_DYNAMIC_LOAD) || SDL_VERSION_ATLEAST(2, 26, 0)
	if (enable_primary_selection)
		sdl2_SetPrimarySelectionText(text);
#endif
}

static void sdl2_clippy_set_clipboard(const char *text)
{
	sdl2_SetClipboardText(text);
}

static char *sdl2_clippy_get_selection(void)
{
#if defined(SDL2_DYNAMIC_LOAD) || SDL_VERSION_ATLEAST(2, 26, 0)
	if (enable_primary_selection) {
		char *sdl = sdl2_GetPrimarySelectionText();
		if (sdl) {
			char *us = str_dup(sdl);
			sdl2_free(sdl);
			return us;
		}
	}
#endif

	return str_dup("");
}

static char *sdl2_clippy_get_clipboard(void)
{
	char *sdl = sdl2_GetClipboardText();
	if (!sdl)
		return str_dup("");

	char *us = str_dup(sdl);
	sdl2_free(sdl);
	return us;
}

//////////////////////////////////////////////////////////////////////////////
// dynamic loading

static int sdl2_clippy_load_syms(void)
{
	SCHISM_SDL2_SYM(HasClipboardText);
	SCHISM_SDL2_SYM(SetClipboardText);
	SCHISM_SDL2_SYM(GetClipboardText);

	SCHISM_SDL2_SYM(free);

	return 0;
}

static int sdl2_26_0_clippy_load_syms(void)
{
#if defined(SDL2_DYNAMIC_LOAD) || SDL_VERSION_ATLEAST(2, 26, 0)
	SCHISM_SDL2_SYM(HasPrimarySelectionText);
	SCHISM_SDL2_SYM(SetPrimarySelectionText);
	SCHISM_SDL2_SYM(GetPrimarySelectionText);

	return 0;
#else
	return -1;
#endif
}

static int sdl2_clippy_init(void)
{
	if (!sdl2_init())
		return 0;

	if (sdl2_clippy_load_syms())
		return 0;

	if (!sdl2_26_0_clippy_load_syms())
		enable_primary_selection = 1;

	return 1;
}

static void sdl2_clippy_quit(void)
{
	sdl2_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_clippy_backend_t schism_clippy_backend_sdl2 = {
	.init = sdl2_clippy_init,
	.quit = sdl2_clippy_quit,

	.have_selection = sdl2_clippy_have_selection,
	.get_selection = sdl2_clippy_get_selection,
	.set_selection = sdl2_clippy_set_selection,

	.have_clipboard = sdl2_clippy_have_clipboard,
	.get_clipboard = sdl2_clippy_get_clipboard,
	.set_clipboard = sdl2_clippy_set_clipboard,
};
