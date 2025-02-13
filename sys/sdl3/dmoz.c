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
#include "backend/dmoz.h"
#include "mem.h"

#include "init.h"

static const char * (SDLCALL *sdl3_GetBasePath)(void);

static char *sdl3_dmoz_get_exe_path(void)
{
	const char *sdl = sdl3_GetBasePath();
	if (!sdl)
		return NULL;

	char *us = str_dup(sdl);
	return us;
}

//////////////////////////////////////////////////////////////////////////////
// dynamic loading

static int sdl3_dmoz_load_syms(void)
{
	SCHISM_SDL3_SYM(GetBasePath);

	return 0;
}

static int sdl3_dmoz_init(void)
{
	if (!sdl3_init())
		return 0;

	if (sdl3_dmoz_load_syms())
		return 0;

	return 1;
}

static void sdl3_dmoz_quit(void)
{
	sdl3_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_dmoz_backend_t schism_dmoz_backend_sdl3 = {
	.init = sdl3_dmoz_init,
	.quit = sdl3_dmoz_quit,

	.get_exe_path = sdl3_dmoz_get_exe_path,
};
