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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.    See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA    02111-1307    USA
 */

#include "headers.h"

#include "it.h"
#include "util.h"
#include "song.h"

#include <assert.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static song_note_t *_get_note_at(lua_State *L, int pattern, int channel, int row)
{
	song_note_t *notes = NULL;
    
	int length = song_get_pattern(pattern, &notes);
	if (!notes || row >= length || channel >= MAX_CHANNELS) {
		luaL_error(L, "requested note %d:%d:%d does not exist");
	}

	return notes + 64 * row + channel - 1;
}

static int set_note(lua_State *L)
{
	int pattern = luaL_checkinteger(L, 1);
	int channel = luaL_checkinteger(L, 2);
	int row = luaL_checkinteger(L, 3);
	int note = luaL_checkinteger(L, 4);

	_get_note_at(L, pattern, channel, row)->note = note;
	return 0;
}

static const struct luaL_Reg patternlib [] = {
    {"set_note", set_note},
    {NULL, NULL}
};

int luaopen_pattern(lua_State *L)
{
	luaL_newlib(L, patternlib);
	return 1;
}