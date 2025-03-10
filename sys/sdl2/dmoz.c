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
#include "backend/dmoz.h"
#include "mem.h"

static char * (SDLCALL *sdl2_GetBasePath)(void);

static void (SDLCALL *sdl2_free)(void *);

static char *sdl2_dmoz_get_exe_path(void)
{
	char *sdl = sdl2_GetBasePath();
	if (!sdl)
		return NULL;

	char *us = str_dup(sdl);
	sdl2_free(sdl);
	return us;
}

//////////////////////////////////////////////////////////////////////////////
// dynamic loading

static int sdl2_dmoz_load_syms(void)
{
	SCHISM_SDL2_SYM(GetBasePath);

	SCHISM_SDL2_SYM(free);

	return 0;
}

static int sdl2_dmoz_init(void)
{
	if (!sdl2_init())
		return 0;

	if (sdl2_dmoz_load_syms())
		return 0;

	return 1;
}

static void sdl2_dmoz_quit(void)
{
	sdl2_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_dmoz_backend_t schism_dmoz_backend_sdl2 = {
	.init = sdl2_dmoz_init,
	.quit = sdl2_dmoz_quit,

	.get_exe_path = sdl2_dmoz_get_exe_path,
};
