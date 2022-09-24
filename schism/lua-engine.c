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
#include "lua-engine.h"
#include "lua-yieldhook.h"

#include <assert.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

static lua_State *L;
static lua_console_write console_write = NULL;

static int running = 0;
static int interrupt = 0;

void set_lua_print(lua_console_write write_func)
{
	console_write = write_func;
}

static int lua_print_console(lua_State *L) {
    int n = lua_gettop(L);
    int i;

    if (!console_write)
		return 0;

    for (i = 1; i <= n; i++) {
	size_t l;
	const char *s = luaL_tolstring(L, i, &l);
	if (i > 1)
	    console_write("\t", 1);
	console_write(s, l);
	lua_pop(L, 1);
    }
    console_write("\n", 1);
    return 0;
}

void eval_lua_input(char *input) {
	int nres;
	int n;

	if (running) {
		return;
	}

	luaL_loadstring(L, input);
	switch (lua_resume(L, NULL, 0, &nres)) {
	case LUA_OK:
		running = 0;
		break;
	case LUA_YIELD:
		running = 1;
		break;
	default:
		// TODO: log failure
		break;
	}

	return;
}

void continue_lua_eval() {
	int nres;
	
	if (!running) {
		return;
	}

	switch (lua_resume(L, NULL, 0, &nres)) {
	case LUA_OK:
		running = 0;
		break;
	case LUA_YIELD:
		running = 1;
		break;
	default:
		// TODO: log failure
		break;
	}

}

static int lua_song_start(lua_State *L)
{
	song_start();
	return 0;
}

static int lua_song_stop(lua_State *L)
{
	song_stop();
	return 0;
}

void lua_init(void)
{
	L = luaL_newstate();
	if (!L) {
		fprintf(stderr, "Couldn't initialise lua!\n");
		exit(1);
	}

	luaL_openlibs(L);
	luaopen_yieldhook(L);
	luaL_dostring(L, "debug.sethook(function() return true end, 'y', 10)");

	lua_pushcfunction(L, lua_print_console);
	lua_setglobal(L, "print");

	lua_pushcfunction(L, lua_song_start);
	lua_setglobal(L, "song_start");

	lua_pushcfunction(L, lua_song_stop);
	lua_setglobal(L, "song_stop");

	log_append(2, 0, "Lua initialised");
	log_underline(15);
	log_appendf(5, " foobar baz");
}
