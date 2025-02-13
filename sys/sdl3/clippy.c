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

#include "headers.h" /* always include this one first, kthx */
#include "backend/clippy.h"
#include "mem.h"

#include "init.h"

static bool (SDLCALL *sdl3_HasPrimarySelectionText)(void);
static bool (SDLCALL *sdl3_SetPrimarySelectionText)(const char *text);
static char * (SDLCALL *sdl3_GetPrimarySelectionText)(void);

static bool (SDLCALL *sdl3_HasClipboardText)(void);
static bool (SDLCALL *sdl3_SetClipboardText)(const char *text);
static char * (SDLCALL *sdl3_GetClipboardText)(void);

static void (SDLCALL *sdl3_free)(void *);

static int sdl3_clippy_have_selection(void)
{
	return sdl3_HasPrimarySelectionText();
}

static int sdl3_clippy_have_clipboard(void)
{
	return sdl3_HasClipboardText();
}

static void sdl3_clippy_set_selection(const char *text)
{
	sdl3_SetPrimarySelectionText(text);
}

static void sdl3_clippy_set_clipboard(const char *text)
{
	sdl3_SetClipboardText(text);
}

static char *sdl3_clippy_get_selection(void)
{
	char *sdl = sdl3_GetPrimarySelectionText();
	if (sdl) {
		char *us = str_dup(sdl);
		sdl3_free(sdl);
		return us;
	}
	return str_dup("");
}

static char *sdl3_clippy_get_clipboard(void)
{
	char *sdl = sdl3_GetClipboardText();
	if (sdl) {
		char *us = str_dup(sdl);
		sdl3_free(sdl);
		return us;
	}
	return str_dup("");
}

//////////////////////////////////////////////////////////////////////////////
// dynamic loading

static int sdl3_clippy_load_syms(void)
{
	SCHISM_SDL3_SYM(HasClipboardText);
	SCHISM_SDL3_SYM(SetClipboardText);
	SCHISM_SDL3_SYM(GetClipboardText);

	SCHISM_SDL3_SYM(HasPrimarySelectionText);
	SCHISM_SDL3_SYM(SetPrimarySelectionText);
	SCHISM_SDL3_SYM(GetPrimarySelectionText);

	SCHISM_SDL3_SYM(free);

	return 0;
}

static int sdl3_clippy_init(void)
{
	if (!sdl3_init())
		return 0;

	if (sdl3_clippy_load_syms())
		return 0;

	return 1;
}

static void sdl3_clippy_quit(void)
{
	sdl3_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_clippy_backend_t schism_clippy_backend_sdl3 = {
	.init = sdl3_clippy_init,
	.quit = sdl3_clippy_quit,

	.have_selection = sdl3_clippy_have_selection,
	.get_selection = sdl3_clippy_get_selection,
	.set_selection = sdl3_clippy_set_selection,

	.have_clipboard = sdl3_clippy_have_clipboard,
	.get_clipboard = sdl3_clippy_get_clipboard,
	.set_clipboard = sdl3_clippy_set_clipboard,
};
