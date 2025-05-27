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
#include "events.h"
#include "backend/events.h"
#include "util.h"
#include "video.h"

#include "init.h"

#include <SDL_syswm.h>

#ifdef SCHISM_MACOS
# include <Windows.h>
# include <Quickdraw.h>
# include <Events.h>
#endif

static schism_keymod_t sdl12_modkey_trans(uint16_t mod)
{
	schism_keymod_t res = 0;

	if (mod & KMOD_LSHIFT)
		res |= SCHISM_KEYMOD_LSHIFT;

	if (mod & KMOD_RSHIFT)
		res |= SCHISM_KEYMOD_RSHIFT;

	if (mod & KMOD_LCTRL)
		res |= SCHISM_KEYMOD_LCTRL;

	if (mod & KMOD_RCTRL)
		res |= SCHISM_KEYMOD_RCTRL;

	if (mod & KMOD_LALT)
		res |= SCHISM_KEYMOD_LALT;

	if (mod & KMOD_RALT)
		res |= SCHISM_KEYMOD_RALT;

	if (mod & KMOD_LMETA)
		res |= SCHISM_KEYMOD_LGUI;

	if (mod & KMOD_RMETA)
		res |= SCHISM_KEYMOD_RGUI;

	if (mod & KMOD_NUM)
		res |= SCHISM_KEYMOD_NUM;

	if (mod & KMOD_CAPS)
		res |= SCHISM_KEYMOD_CAPS;

	if (mod & KMOD_MODE)
		res |= SCHISM_KEYMOD_MODE;

	return res;
}

static schism_keysym_t sdl12_keycode_trans(SDLKey key)
{
	if ((int)key <= 255) {
		switch (key) {
		case SDLK_PAUSE: return SCHISM_KEYSYM_PAUSE;
		case SDLK_CLEAR: return SCHISM_KEYSYM_CLEAR;
		default: break;
		}

		return (schism_keysym_t)key;
	}

    switch (key) {
    #define CASEKEYSYM12TOSCHISM(schism, k12) case SDLK_##k12: return SCHISM_KEYSYM_##schism
    CASEKEYSYM12TOSCHISM(KP_0, KP0);
    CASEKEYSYM12TOSCHISM(KP_1, KP1);
    CASEKEYSYM12TOSCHISM(KP_2, KP2);
    CASEKEYSYM12TOSCHISM(KP_3, KP3);
    CASEKEYSYM12TOSCHISM(KP_4, KP4);
    CASEKEYSYM12TOSCHISM(KP_5, KP5);
    CASEKEYSYM12TOSCHISM(KP_6, KP6);
    CASEKEYSYM12TOSCHISM(KP_7, KP7);
    CASEKEYSYM12TOSCHISM(KP_8, KP8);
    CASEKEYSYM12TOSCHISM(KP_9, KP9);
    CASEKEYSYM12TOSCHISM(NUMLOCKCLEAR, NUMLOCK);
    CASEKEYSYM12TOSCHISM(SCROLLLOCK, SCROLLOCK);
    CASEKEYSYM12TOSCHISM(RGUI, RMETA);
    CASEKEYSYM12TOSCHISM(LGUI, LMETA);
    CASEKEYSYM12TOSCHISM(PRINTSCREEN, PRINT);
    #undef CASEKEYSYM12TOSCHISM

    #define CASEKEYSYM12TOSCHISM(k) case SDLK_##k: return SCHISM_KEYSYM_##k
    CASEKEYSYM12TOSCHISM(CLEAR);
    CASEKEYSYM12TOSCHISM(PAUSE);
    CASEKEYSYM12TOSCHISM(KP_PERIOD);
    CASEKEYSYM12TOSCHISM(KP_DIVIDE);
    CASEKEYSYM12TOSCHISM(KP_MULTIPLY);
    CASEKEYSYM12TOSCHISM(KP_MINUS);
    CASEKEYSYM12TOSCHISM(KP_PLUS);
    CASEKEYSYM12TOSCHISM(KP_ENTER);
    CASEKEYSYM12TOSCHISM(KP_EQUALS);
    CASEKEYSYM12TOSCHISM(UP);
    CASEKEYSYM12TOSCHISM(DOWN);
    CASEKEYSYM12TOSCHISM(RIGHT);
    CASEKEYSYM12TOSCHISM(LEFT);
    CASEKEYSYM12TOSCHISM(INSERT);
    CASEKEYSYM12TOSCHISM(HOME);
    CASEKEYSYM12TOSCHISM(END);
    CASEKEYSYM12TOSCHISM(PAGEUP);
    CASEKEYSYM12TOSCHISM(PAGEDOWN);
    CASEKEYSYM12TOSCHISM(F1);
    CASEKEYSYM12TOSCHISM(F2);
    CASEKEYSYM12TOSCHISM(F3);
    CASEKEYSYM12TOSCHISM(F4);
    CASEKEYSYM12TOSCHISM(F5);
    CASEKEYSYM12TOSCHISM(F6);
    CASEKEYSYM12TOSCHISM(F7);
    CASEKEYSYM12TOSCHISM(F8);
    CASEKEYSYM12TOSCHISM(F9);
    CASEKEYSYM12TOSCHISM(F10);
    CASEKEYSYM12TOSCHISM(F11);
    CASEKEYSYM12TOSCHISM(F12);
    CASEKEYSYM12TOSCHISM(F13);
    CASEKEYSYM12TOSCHISM(F14);
    CASEKEYSYM12TOSCHISM(F15);
    CASEKEYSYM12TOSCHISM(CAPSLOCK);
    CASEKEYSYM12TOSCHISM(RSHIFT);
    CASEKEYSYM12TOSCHISM(LSHIFT);
    CASEKEYSYM12TOSCHISM(RCTRL);
    CASEKEYSYM12TOSCHISM(LCTRL);
    CASEKEYSYM12TOSCHISM(RALT);
    CASEKEYSYM12TOSCHISM(LALT);
    //CASEKEYSYM12TOSCHISM(MODE);
    CASEKEYSYM12TOSCHISM(HELP);
    CASEKEYSYM12TOSCHISM(SYSREQ);
    CASEKEYSYM12TOSCHISM(MENU);
    CASEKEYSYM12TOSCHISM(POWER);
    CASEKEYSYM12TOSCHISM(UNDO);
    #undef CASEKEYSYM12TOSCHISM
	default: break;
	}

	return SCHISM_KEYSYM_UNKNOWN;
}

static schism_scancode_t sdl12_scancode_trans(uint8_t sc)
{
	switch (sc) {
#ifdef SCHISM_WIN32
	case (0x13 - 8): return SCHISM_SCANCODE_0;
	case (0x0A - 8): return SCHISM_SCANCODE_1;
	case (0x0B - 8): return SCHISM_SCANCODE_2;
	case (0x0C - 8): return SCHISM_SCANCODE_3;
	case (0x0D - 8): return SCHISM_SCANCODE_4;
	case (0x0E - 8): return SCHISM_SCANCODE_5;
	case (0x0F - 8): return SCHISM_SCANCODE_6;
	case (0x10 - 8): return SCHISM_SCANCODE_7;
	case (0x11 - 8): return SCHISM_SCANCODE_8;
	case (0x12 - 8): return SCHISM_SCANCODE_9;
	case (0x26 - 8): return SCHISM_SCANCODE_A;
	case (0x30 - 8): return SCHISM_SCANCODE_APOSTROPHE;
	case (0x38 - 8): return SCHISM_SCANCODE_B;
	case (0x33 - 8): return SCHISM_SCANCODE_BACKSLASH;
	case (0x16 - 8): return SCHISM_SCANCODE_BACKSPACE;
	case (0x36 - 8): return SCHISM_SCANCODE_C;
	case (0x42 - 8): return SCHISM_SCANCODE_CAPSLOCK;
	case (0x3B - 8): return SCHISM_SCANCODE_COMMA;
	case (0x28 - 8): return SCHISM_SCANCODE_D;
	case (0x1A - 8): return SCHISM_SCANCODE_E;
	case (0x15 - 8): return SCHISM_SCANCODE_EQUALS;
	case (0x09 - 8): return SCHISM_SCANCODE_ESCAPE;
	case (0x29 - 8): return SCHISM_SCANCODE_F;
	case (0x43 - 8): return SCHISM_SCANCODE_F1;
	case (0x4C - 8): return SCHISM_SCANCODE_F10;
	case (0x5F - 8): return SCHISM_SCANCODE_F11;
	case (0x60 - 8): return SCHISM_SCANCODE_F12;
	case (0x44 - 8): return SCHISM_SCANCODE_F2;
	case (0x45 - 8): return SCHISM_SCANCODE_F3;
	case (0x46 - 8): return SCHISM_SCANCODE_F4;
	case (0x47 - 8): return SCHISM_SCANCODE_F5;
	case (0x48 - 8): return SCHISM_SCANCODE_F6;
	case (0x49 - 8): return SCHISM_SCANCODE_F7;
	case (0x4A - 8): return SCHISM_SCANCODE_F8;
	case (0x4B - 8): return SCHISM_SCANCODE_F9;
	case (0x2A - 8): return SCHISM_SCANCODE_G;
	case (0x31 - 8): return SCHISM_SCANCODE_GRAVE;
	case (0x2B - 8): return SCHISM_SCANCODE_H;
	case (0x1F - 8): return SCHISM_SCANCODE_I;
	case (0x2C - 8): return SCHISM_SCANCODE_J;
	case (0x2D - 8): return SCHISM_SCANCODE_K;
	case (0x5A - 8): return SCHISM_SCANCODE_KP_0;
	case (0x57 - 8): return SCHISM_SCANCODE_KP_1;
	case (0x58 - 8): return SCHISM_SCANCODE_KP_2;
	case (0x59 - 8): return SCHISM_SCANCODE_KP_3;
	case (0x53 - 8): return SCHISM_SCANCODE_KP_4;
	case (0x54 - 8): return SCHISM_SCANCODE_KP_5;
	case (0x55 - 8): return SCHISM_SCANCODE_KP_6;
	case (0x4F - 8): return SCHISM_SCANCODE_KP_7;
	case (0x50 - 8): return SCHISM_SCANCODE_KP_8;
	case (0x51 - 8): return SCHISM_SCANCODE_KP_9;
	case (0x52 - 8): return SCHISM_SCANCODE_KP_MINUS;
	case (0x3F - 8): return SCHISM_SCANCODE_KP_MULTIPLY;
	case (0x5B - 8): return SCHISM_SCANCODE_KP_PERIOD;
	case (0x56 - 8): return SCHISM_SCANCODE_KP_PLUS;
	case (0x2E - 8): return SCHISM_SCANCODE_L;
	case (0x40 - 8): return SCHISM_SCANCODE_LALT;
	case (0x25 - 8): return SCHISM_SCANCODE_LCTRL;
	case (0x22 - 8): return SCHISM_SCANCODE_LEFTBRACKET;
	case (0x85 - 8): return SCHISM_SCANCODE_LGUI;
	case (0x32 - 8): return SCHISM_SCANCODE_LSHIFT;
	case (0x3A - 8): return SCHISM_SCANCODE_M;
	case (0x14 - 8): return SCHISM_SCANCODE_MINUS;
	case (0x39 - 8): return SCHISM_SCANCODE_N;
	case (0x5E - 8): return SCHISM_SCANCODE_NONUSBACKSLASH;
	case (0x4D - 8): return SCHISM_SCANCODE_NUMLOCKCLEAR;
	case (0x20 - 8): return SCHISM_SCANCODE_O;
	case (0x21 - 8): return SCHISM_SCANCODE_P;
	case (0x3C - 8): return SCHISM_SCANCODE_PERIOD;
	case (0x6B - 8): return SCHISM_SCANCODE_PRINTSCREEN;
	case (0x18 - 8): return SCHISM_SCANCODE_Q;
	case (0x1B - 8): return SCHISM_SCANCODE_R;
	case (0x24 - 8): return SCHISM_SCANCODE_RETURN;
	case (0x86 - 8): return SCHISM_SCANCODE_RGUI;
	case (0x23 - 8): return SCHISM_SCANCODE_RIGHTBRACKET;
	case (0x3E - 8): return SCHISM_SCANCODE_RSHIFT;
	case (0x27 - 8): return SCHISM_SCANCODE_S;
	case (0x4E - 8): return SCHISM_SCANCODE_SCROLLLOCK;
	case (0x2F - 8): return SCHISM_SCANCODE_SEMICOLON;
	case (0x3D - 8): return SCHISM_SCANCODE_SLASH;
	case (0x41 - 8): return SCHISM_SCANCODE_SPACE;
	case (0x1C - 8): return SCHISM_SCANCODE_T;
	case (0x17 - 8): return SCHISM_SCANCODE_TAB;
	case (0x1E - 8): return SCHISM_SCANCODE_U;
	case (0x37 - 8): return SCHISM_SCANCODE_V;
	case (0x19 - 8): return SCHISM_SCANCODE_W;
	case (0x35 - 8): return SCHISM_SCANCODE_X;
	case (0x1D - 8): return SCHISM_SCANCODE_Y;
	case (0x34 - 8): return SCHISM_SCANCODE_Z;
#elif defined(SCHISM_MACOSX)
	/* Mac OS X (This is largely the same as Mac OS) */
	case 0x1D: return SCHISM_SCANCODE_0;
	case 0x12: return SCHISM_SCANCODE_1;
	case 0x13: return SCHISM_SCANCODE_2;
	case 0x14: return SCHISM_SCANCODE_3;
	case 0x15: return SCHISM_SCANCODE_4;
	case 0x17: return SCHISM_SCANCODE_5;
	case 0x16: return SCHISM_SCANCODE_6;
	case 0x1A: return SCHISM_SCANCODE_7;
	case 0x1C: return SCHISM_SCANCODE_8;
	case 0x19: return SCHISM_SCANCODE_9;
	case 0x00: return SCHISM_SCANCODE_A;
	case 0x27: return SCHISM_SCANCODE_APOSTROPHE;
	case 0x0B: return SCHISM_SCANCODE_B;
	case 0x2A: return SCHISM_SCANCODE_BACKSLASH;
	case 0x33: return SCHISM_SCANCODE_BACKSPACE;
	case 0x08: return SCHISM_SCANCODE_C;
	case 0x39: return SCHISM_SCANCODE_CAPSLOCK;
	case 0x2B: return SCHISM_SCANCODE_COMMA;
	case 0x02: return SCHISM_SCANCODE_D;
	case 0x75: return SCHISM_SCANCODE_DELETE;
	case 0x7D: return SCHISM_SCANCODE_DOWN;
	case 0x0E: return SCHISM_SCANCODE_E;
	case 0x77: return SCHISM_SCANCODE_END;
	case 0x18: return SCHISM_SCANCODE_EQUALS;
	case 0x35: return SCHISM_SCANCODE_ESCAPE;
	case 0x03: return SCHISM_SCANCODE_F;
	case 0x7A: return SCHISM_SCANCODE_F1;
	case 0x6E: return SCHISM_SCANCODE_F10;
	case 0x67: return SCHISM_SCANCODE_F11;
	case 0x6F: return SCHISM_SCANCODE_F12;
	case 0x78: return SCHISM_SCANCODE_F2;
	case 0x63: return SCHISM_SCANCODE_F3;
	case 0x76: return SCHISM_SCANCODE_F4;
	case 0x60: return SCHISM_SCANCODE_F5;
	case 0x61: return SCHISM_SCANCODE_F6;
	case 0x62: return SCHISM_SCANCODE_F7;
	case 0x64: return SCHISM_SCANCODE_F8;
	case 0x65: return SCHISM_SCANCODE_F9;
	case 0x05: return SCHISM_SCANCODE_G;
	case 0x32: return SCHISM_SCANCODE_GRAVE;
	case 0x04: return SCHISM_SCANCODE_H;
	case 0x73: return SCHISM_SCANCODE_HOME;
	case 0x22: return SCHISM_SCANCODE_I;
	case 0x72: return SCHISM_SCANCODE_INSERT;
	case 0x26: return SCHISM_SCANCODE_J;
	case 0x28: return SCHISM_SCANCODE_K;
	case 0x52: return SCHISM_SCANCODE_KP_0;
	case 0x53: return SCHISM_SCANCODE_KP_1;
	case 0x54: return SCHISM_SCANCODE_KP_2;
	case 0x55: return SCHISM_SCANCODE_KP_3;
	case 0x56: return SCHISM_SCANCODE_KP_4;
	case 0x57: return SCHISM_SCANCODE_KP_5;
	case 0x58: return SCHISM_SCANCODE_KP_6;
	case 0x59: return SCHISM_SCANCODE_KP_7;
	case 0x5B: return SCHISM_SCANCODE_KP_8;
	case 0x5C: return SCHISM_SCANCODE_KP_9;
	case 0x4B: return SCHISM_SCANCODE_KP_DIVIDE;
	case 0x4C: return SCHISM_SCANCODE_KP_ENTER;
	case 0x51: return SCHISM_SCANCODE_KP_EQUALS;
	case 0x4E: return SCHISM_SCANCODE_KP_MINUS;
	case 0x43: return SCHISM_SCANCODE_KP_MULTIPLY;
	case 0x41: return SCHISM_SCANCODE_KP_PERIOD;
	case 0x45: return SCHISM_SCANCODE_KP_PLUS;
	case 0x25: return SCHISM_SCANCODE_L;
	case 0x7B: return SCHISM_SCANCODE_LEFT;
	case 0x21: return SCHISM_SCANCODE_LEFTBRACKET;
	case 0x2E: return SCHISM_SCANCODE_M;
	case 0x1B: return SCHISM_SCANCODE_MINUS;
	case 0x2D: return SCHISM_SCANCODE_N;
	case 0x0A: return SCHISM_SCANCODE_NONUSBACKSLASH;
	case 0x47: return SCHISM_SCANCODE_NUMLOCKCLEAR;
	case 0x1F: return SCHISM_SCANCODE_O;
	case 0x23: return SCHISM_SCANCODE_P;
	case 0x79: return SCHISM_SCANCODE_PAGEDOWN;
	case 0x74: return SCHISM_SCANCODE_PAGEUP;
	case 0x2F: return SCHISM_SCANCODE_PERIOD;
	case 0x6B: return SCHISM_SCANCODE_PRINTSCREEN;
	case 0x0C: return SCHISM_SCANCODE_Q;
	case 0x0F: return SCHISM_SCANCODE_R;
	case 0x24: return SCHISM_SCANCODE_RETURN;
	case 0x7C: return SCHISM_SCANCODE_RIGHT;
	case 0x1E: return SCHISM_SCANCODE_RIGHTBRACKET;
	case 0x01: return SCHISM_SCANCODE_S;
	case 0x71: return SCHISM_SCANCODE_SCROLLLOCK;
	case 0x29: return SCHISM_SCANCODE_SEMICOLON;
	case 0x2C: return SCHISM_SCANCODE_SLASH;
	case 0x31: return SCHISM_SCANCODE_SPACE;
	case 0x11: return SCHISM_SCANCODE_T;
	case 0x30: return SCHISM_SCANCODE_TAB;
	case 0x20: return SCHISM_SCANCODE_U;
	case 0x7E: return SCHISM_SCANCODE_UP;
	case 0x09: return SCHISM_SCANCODE_V;
	case 0x0D: return SCHISM_SCANCODE_W;
	case 0x07: return SCHISM_SCANCODE_X;
	case 0x10: return SCHISM_SCANCODE_Y;
	case 0x06: return SCHISM_SCANCODE_Z;
#elif defined(SCHISM_MACOS)
	/* Taken from Inside Macintosh */
	case 0x35: return SCHISM_SCANCODE_ESCAPE;
	case 0x7A: return SCHISM_SCANCODE_F1;
	case 0x78: return SCHISM_SCANCODE_F2;
	case 0x63: return SCHISM_SCANCODE_F3;
	case 0x76: return SCHISM_SCANCODE_F4;
	case 0x60: return SCHISM_SCANCODE_F5;
	case 0x61: return SCHISM_SCANCODE_F6;
	case 0x62: return SCHISM_SCANCODE_F7;
	case 0x64: return SCHISM_SCANCODE_F8;
	case 0x65: return SCHISM_SCANCODE_F9;
	case 0x6D: return SCHISM_SCANCODE_F10;
	case 0x67: return SCHISM_SCANCODE_F11;
	case 0x6F: return SCHISM_SCANCODE_F12;
	case 0x69: return SCHISM_SCANCODE_PRINTSCREEN;
	case 0x6B: return SCHISM_SCANCODE_SCROLLLOCK;
	case 0x71: return SCHISM_SCANCODE_PAUSE;
	case 0x7F: return SCHISM_SCANCODE_POWER;
	case 0x32: return SCHISM_SCANCODE_GRAVE;
	case 0x12: return SCHISM_SCANCODE_1;
	case 0x13: return SCHISM_SCANCODE_2;
	case 0x14: return SCHISM_SCANCODE_3;
	case 0x15: return SCHISM_SCANCODE_4;
	case 0x17: return SCHISM_SCANCODE_5;
	case 0x16: return SCHISM_SCANCODE_6;
	case 0x1A: return SCHISM_SCANCODE_7;
	case 0x1C: return SCHISM_SCANCODE_8;
	case 0x19: return SCHISM_SCANCODE_9;
	case 0x1D: return SCHISM_SCANCODE_0;
	case 0x1B: return SCHISM_SCANCODE_MINUS;
	case 0x18: return SCHISM_SCANCODE_EQUALS;
	case 0x33: return SCHISM_SCANCODE_BACKSPACE;
	case 0x72: return SCHISM_SCANCODE_INSERT;
	case 0x73: return SCHISM_SCANCODE_HOME;
	case 0x74: return SCHISM_SCANCODE_PAGEUP;
	case 0x47: return SCHISM_SCANCODE_NUMLOCKCLEAR;
	case 0x51: return SCHISM_SCANCODE_KP_EQUALS;
	case 0x4B: return SCHISM_SCANCODE_KP_DIVIDE;
	case 0x43: return SCHISM_SCANCODE_KP_MULTIPLY;
	case 0x30: return SCHISM_SCANCODE_TAB;
	case 0x0C: return SCHISM_SCANCODE_Q;
	case 0x0D: return SCHISM_SCANCODE_W;
	case 0x0E: return SCHISM_SCANCODE_E;
	case 0x0F: return SCHISM_SCANCODE_R;
	case 0x11: return SCHISM_SCANCODE_T;
	case 0x10: return SCHISM_SCANCODE_Y;
	case 0x20: return SCHISM_SCANCODE_U;
	case 0x22: return SCHISM_SCANCODE_I;
	case 0x1F: return SCHISM_SCANCODE_O;
	case 0x23: return SCHISM_SCANCODE_P;
	case 0x21: return SCHISM_SCANCODE_LEFTBRACKET;
	case 0x1E: return SCHISM_SCANCODE_RIGHTBRACKET;
	case 0x2A: return SCHISM_SCANCODE_BACKSLASH;
	case 0x75: return SCHISM_SCANCODE_DELETE;
	case 0x77: return SCHISM_SCANCODE_END;
	case 0x79: return SCHISM_SCANCODE_PAGEDOWN;
	case 0x59: return SCHISM_SCANCODE_KP_7;
	case 0x5B: return SCHISM_SCANCODE_KP_8;
	case 0x5C: return SCHISM_SCANCODE_KP_9;
	case 0x4E: return SCHISM_SCANCODE_KP_MINUS;
	case 0x39: return SCHISM_SCANCODE_CAPSLOCK;
	case 0x00: return SCHISM_SCANCODE_A;
	case 0x01: return SCHISM_SCANCODE_S;
	case 0x02: return SCHISM_SCANCODE_D;
	case 0x03: return SCHISM_SCANCODE_F;
	case 0x05: return SCHISM_SCANCODE_G;
	case 0x04: return SCHISM_SCANCODE_H;
	case 0x26: return SCHISM_SCANCODE_J;
	case 0x28: return SCHISM_SCANCODE_K;
	case 0x25: return SCHISM_SCANCODE_L;
	case 0x29: return SCHISM_SCANCODE_SEMICOLON;
	case 0x27: return SCHISM_SCANCODE_APOSTROPHE;
	case 0x24: return SCHISM_SCANCODE_RETURN;
	case 0x56: return SCHISM_SCANCODE_KP_4;
	case 0x57: return SCHISM_SCANCODE_KP_5;
	case 0x58: return SCHISM_SCANCODE_KP_6;
	case 0x45: return SCHISM_SCANCODE_KP_PLUS;
	case 0x38: return SCHISM_SCANCODE_LSHIFT;
	case 0x06: return SCHISM_SCANCODE_Z;
	case 0x07: return SCHISM_SCANCODE_X;
	case 0x08: return SCHISM_SCANCODE_C;
	case 0x09: return SCHISM_SCANCODE_V;
	case 0x0B: return SCHISM_SCANCODE_B;
	case 0x2D: return SCHISM_SCANCODE_N;
	case 0x2E: return SCHISM_SCANCODE_M;
	case 0x2B: return SCHISM_SCANCODE_COMMA;
	case 0x2F: return SCHISM_SCANCODE_PERIOD;
	case 0x2C: return SCHISM_SCANCODE_SLASH;
	case 0x7E: return SCHISM_SCANCODE_UP;
	case 0x53: return SCHISM_SCANCODE_KP_1;
	case 0x54: return SCHISM_SCANCODE_KP_2;
	case 0x55: return SCHISM_SCANCODE_KP_3;
	case 0x4C: return SCHISM_SCANCODE_KP_ENTER;
	// The Right Ctrl, Alt, Super, and Shift keys
	// are treated identically by classic Mac OS,
	// and hence they cannot be used here...
	case 0x3B: return SCHISM_SCANCODE_LCTRL;
	case 0x3A: return SCHISM_SCANCODE_LALT;
	case 0x37: return SCHISM_SCANCODE_LGUI;
	case 0x31: return SCHISM_SCANCODE_SPACE;
	case 0x7B: return SCHISM_SCANCODE_LEFT;
	case 0x7D: return SCHISM_SCANCODE_DOWN;
	case 0x7C: return SCHISM_SCANCODE_RIGHT;
	case 0x52: return SCHISM_SCANCODE_KP_0;
	case 0x41: return SCHISM_SCANCODE_KP_PERIOD;
#else
	/* Linux and friends ? */
	case 0x13: return SCHISM_SCANCODE_0;
	case 0x0A: return SCHISM_SCANCODE_1;
	case 0x0B: return SCHISM_SCANCODE_2;
	case 0x0C: return SCHISM_SCANCODE_3;
	case 0x0D: return SCHISM_SCANCODE_4;
	case 0x0E: return SCHISM_SCANCODE_5;
	case 0x0F: return SCHISM_SCANCODE_6;
	case 0x10: return SCHISM_SCANCODE_7;
	case 0x11: return SCHISM_SCANCODE_8;
	case 0x12: return SCHISM_SCANCODE_9;
	case 0x26: return SCHISM_SCANCODE_A;
	case 0x30: return SCHISM_SCANCODE_APOSTROPHE;
	case 0x38: return SCHISM_SCANCODE_B;
	case 0x33: return SCHISM_SCANCODE_BACKSLASH;
	case 0x16: return SCHISM_SCANCODE_BACKSPACE;
	case 0x36: return SCHISM_SCANCODE_C;
	case 0x42: return SCHISM_SCANCODE_CAPSLOCK;
	case 0x3B: return SCHISM_SCANCODE_COMMA;
	case 0x28: return SCHISM_SCANCODE_D;
	//case 0x00: return SCHISM_SCANCODE_DELETE;
	//case 0x00: return SCHISM_SCANCODE_DOWN;
	case 0x1A: return SCHISM_SCANCODE_E;
	//case 0x00: return SCHISM_SCANCODE_END;
	case 0x15: return SCHISM_SCANCODE_EQUALS;
	case 0x09: return SCHISM_SCANCODE_ESCAPE;
	case 0x29: return SCHISM_SCANCODE_F;
	case 0x43: return SCHISM_SCANCODE_F1;
	case 0x4C: return SCHISM_SCANCODE_F10;
	case 0x5F: return SCHISM_SCANCODE_F11;
	case 0x60: return SCHISM_SCANCODE_F12;
	case 0x44: return SCHISM_SCANCODE_F2;
	case 0x45: return SCHISM_SCANCODE_F3;
	case 0x46: return SCHISM_SCANCODE_F4;
	case 0x47: return SCHISM_SCANCODE_F5;
	case 0x48: return SCHISM_SCANCODE_F6;
	case 0x49: return SCHISM_SCANCODE_F7;
	case 0x4A: return SCHISM_SCANCODE_F8;
	case 0x4B: return SCHISM_SCANCODE_F9;
	case 0x2A: return SCHISM_SCANCODE_G;
	case 0x31: return SCHISM_SCANCODE_GRAVE;
	case 0x2B: return SCHISM_SCANCODE_H;
	//case 0x00: return SCHISM_SCANCODE_HOME;
	case 0x1F: return SCHISM_SCANCODE_I;
	//case 0x00: return SCHISM_SCANCODE_INSERT;
	case 0x2C: return SCHISM_SCANCODE_J;
	case 0x2D: return SCHISM_SCANCODE_K;
	case 0x5A: return SCHISM_SCANCODE_KP_0;
	case 0x57: return SCHISM_SCANCODE_KP_1;
	case 0x58: return SCHISM_SCANCODE_KP_2;
	case 0x59: return SCHISM_SCANCODE_KP_3;
	case 0x53: return SCHISM_SCANCODE_KP_4;
	case 0x54: return SCHISM_SCANCODE_KP_5;
	case 0x55: return SCHISM_SCANCODE_KP_6;
	case 0x4F: return SCHISM_SCANCODE_KP_7;
	case 0x50: return SCHISM_SCANCODE_KP_8;
	case 0x51: return SCHISM_SCANCODE_KP_9;
	//case 0x00: return SCHISM_SCANCODE_KP_DIVIDE;
	//case 0x00: return SCHISM_SCANCODE_KP_ENTER;
	//case 0x00: return SCHISM_SCANCODE_KP_EQUALS;
	case 0x52: return SCHISM_SCANCODE_KP_MINUS;
	case 0x3F: return SCHISM_SCANCODE_KP_MULTIPLY;
	case 0x5B: return SCHISM_SCANCODE_KP_PERIOD;
	case 0x56: return SCHISM_SCANCODE_KP_PLUS;
	case 0x2E: return SCHISM_SCANCODE_L;
	case 0x40: return SCHISM_SCANCODE_LALT;
	case 0x25: return SCHISM_SCANCODE_LCTRL;
	//case 0x00: return SCHISM_SCANCODE_LEFT;
	case 0x22: return SCHISM_SCANCODE_LEFTBRACKET;
	case 0x85: return SCHISM_SCANCODE_LGUI;
	case 0x32: return SCHISM_SCANCODE_LSHIFT;
	case 0x3A: return SCHISM_SCANCODE_M;
	case 0x14: return SCHISM_SCANCODE_MINUS;
	case 0x39: return SCHISM_SCANCODE_N;
	case 0x5E: return SCHISM_SCANCODE_NONUSBACKSLASH;
	case 0x4D: return SCHISM_SCANCODE_NUMLOCKCLEAR;
	case 0x20: return SCHISM_SCANCODE_O;
	case 0x21: return SCHISM_SCANCODE_P;
	//case 0x00: return SCHISM_SCANCODE_PAGEDOWN;
	//case 0x00: return SCHISM_SCANCODE_PAGEUP;
	case 0x3C: return SCHISM_SCANCODE_PERIOD;
	case 0x6B: return SCHISM_SCANCODE_PRINTSCREEN;
	case 0x18: return SCHISM_SCANCODE_Q;
	case 0x1B: return SCHISM_SCANCODE_R;
	case 0x24: return SCHISM_SCANCODE_RETURN;
	case 0x86: return SCHISM_SCANCODE_RGUI;
	//case 0x00: return SCHISM_SCANCODE_RIGHT;
	case 0x23: return SCHISM_SCANCODE_RIGHTBRACKET;
	case 0x3E: return SCHISM_SCANCODE_RSHIFT;
	case 0x27: return SCHISM_SCANCODE_S;
	case 0x4E: return SCHISM_SCANCODE_SCROLLLOCK;
	case 0x2F: return SCHISM_SCANCODE_SEMICOLON;
	case 0x3D: return SCHISM_SCANCODE_SLASH;
	case 0x41: return SCHISM_SCANCODE_SPACE;
	case 0x1C: return SCHISM_SCANCODE_T;
	case 0x17: return SCHISM_SCANCODE_TAB;
	case 0x1E: return SCHISM_SCANCODE_U;
	//case 0x00: return SCHISM_SCANCODE_UP;
	case 0x37: return SCHISM_SCANCODE_V;
	case 0x19: return SCHISM_SCANCODE_W;
	case 0x35: return SCHISM_SCANCODE_X;
	case 0x1D: return SCHISM_SCANCODE_Y;
	case 0x34: return SCHISM_SCANCODE_Z;
#endif
	default: return SCHISM_SCANCODE_UNKNOWN;
	}
}

//////////////////////////////////////////////////////////////////////////////

static SDLMod (SDLCALL *sdl12_GetModState)(void);
static int (SDLCALL *sdl12_PollEvent)(SDL_Event *event);
static Uint8 (SDLCALL *sdl12_EventState)(Uint8 type, int state);
static Uint8 (SDLCALL *sdl12_GetAppState)(void);

static schism_keymod_t sdl12_event_mod_state(void)
{
	return sdl12_modkey_trans(sdl12_GetModState());
}

static uint8_t app_state = 0;
static uint8_t mouse_in_window = 0;

static void sdl12_pump_events(void)
{
	/* Convert our events to Schism's internal representation */
	SDL_Event e;

	// SDL_ACTIVEEVENT is broken on Windows, and probably others too.
	// This just brute forces the app state each time we pump events
	// which fixes it (ugh)
	{
		uint8_t app_state_new = sdl12_GetAppState();

		static const struct {
			uint8_t mask;
			uint32_t gain;
			uint32_t lost;
		} conv[] = {
			{SDL_APPINPUTFOCUS, SCHISM_WINDOWEVENT_FOCUS_GAINED, SCHISM_WINDOWEVENT_FOCUS_LOST},
			{SDL_APPMOUSEFOCUS, SCHISM_WINDOWEVENT_ENTER, SCHISM_WINDOWEVENT_LEAVE},
			{SDL_APPACTIVE, SCHISM_WINDOWEVENT_SHOWN, SCHISM_WINDOWEVENT_HIDDEN},
		};

		for (size_t i = 0; i < ARRAY_SIZE(conv); i++) {
			if ((app_state & conv[i].mask) == (app_state_new & conv[i].mask))
				continue;

			schism_event_t e;
			e.type = (app_state_new & conv[i].mask) ? conv[i].gain : conv[i].lost;
			events_push_event(&e);
		}

		app_state = app_state_new;
	}

#ifdef SCHISM_MACOS
	if (app_state & SDL_APPINPUTFOCUS) {
		/* Macintosh has no way of detecting whether the mouse has left the
		 * window bounds. Manually detect it ourselves.
		 *
		 * This is hacky as FUCK... */
		WindowPtr win;

		win = FrontWindow();
		if (win) {
			GrafPtr saveport;
			Boolean mouseinwin;
			Rect bounds;
			Point pos;

			bounds = win->portRect;

			/* this stuff was copied from SDL-1.2 */
			GetPort(&saveport);

			SetPort(win);

			GetMouse(&pos);
			LocalToGlobal(&pos);
			LocalToGlobal((Point *)&bounds.top);
			LocalToGlobal((Point *)&bounds.bottom);

			SetPort(saveport);

			mouseinwin = PtInRect(pos, &bounds);
			if (mouse_in_window != mouseinwin) {
				/* push the event */
				schism_event_t e;

				e.type = (mouseinwin)
					? SCHISM_WINDOWEVENT_ENTER
					: SCHISM_WINDOWEVENT_LEAVE;
				events_push_event(&e);

				mouse_in_window = mouseinwin;
			}
		}
	}
#endif

	while (sdl12_PollEvent(&e)) {
		schism_event_t schism_event = {0};

		switch (e.type) {
		case SDL_QUIT:
			schism_event.type = SCHISM_QUIT;
			events_push_event(&schism_event);
			break;
		case SDL_VIDEORESIZE:
			schism_event.type = SCHISM_WINDOWEVENT_RESIZED;
			schism_event.window.data.resized.width = e.resize.w;
			schism_event.window.data.resized.height = e.resize.h;
			events_push_event(&schism_event);
			break;
		case SDL_VIDEOEXPOSE:
			schism_event.type = SCHISM_WINDOWEVENT_EXPOSED;
			events_push_event(&schism_event);
			break;
		case SDL_KEYDOWN:
			schism_event.type = SCHISM_KEYDOWN;
			schism_event.key.state = KEY_PRESS;

			schism_event.key.sym = sdl12_keycode_trans(e.key.keysym.sym);
			schism_event.key.scancode = sdl12_scancode_trans(e.key.keysym.scancode);
			schism_event.key.mod = sdl12_modkey_trans(e.key.keysym.mod);

			/* Only convert the Unicode if it's actually useful;
			 * this tripped a bug under mac os x where the left & right
			 * arrow keys are translated to some odd unicode char. */
			if (!(schism_event.key.sym & SCHISM_KEYSYM_SCANCODE_MASK)
				&& !(schism_event.key.mod & SCHISM_KEYMOD_CTRL)
				&& schism_event.key.sym != SCHISM_KEYSYM_ESCAPE) {
				/* convert UCS-2 to UTF-8 */
				if (e.key.keysym.unicode < 0x80) {
					schism_event.key.text[0] = e.key.keysym.unicode;
				} else if (e.key.keysym.unicode < 0x800) {
					schism_event.key.text[0] = 0xC0 | (e.key.keysym.unicode >> 6);
					schism_event.key.text[1] = 0x80 | (e.key.keysym.unicode & 0x3F);
				} else {
					schism_event.key.text[0] = 0xE0 |  (e.key.keysym.unicode >> 12);
					schism_event.key.text[1] = 0x80 | ((e.key.keysym.unicode >> 6) & 0x3F);
					schism_event.key.text[2] = 0x80 |  (e.key.keysym.unicode & 0x3F);
				}
			}

			events_push_event(&schism_event);

			break;
		case SDL_KEYUP:
			schism_event.type = SCHISM_KEYUP;
			schism_event.key.state = KEY_RELEASE;
			schism_event.key.sym = sdl12_keycode_trans(e.key.keysym.sym);
			schism_event.key.scancode = sdl12_scancode_trans(e.key.keysym.scancode);
			schism_event.key.mod = sdl12_modkey_trans(e.key.keysym.mod);

			events_push_event(&schism_event);

			break;
		case SDL_MOUSEMOTION:
			schism_event.type = SCHISM_MOUSEMOTION;
			schism_event.motion.x = e.motion.x;
			schism_event.motion.y = e.motion.y;
			events_push_event(&schism_event);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			schism_event.type = (e.type == SDL_MOUSEBUTTONDOWN) ? SCHISM_MOUSEBUTTONDOWN : SCHISM_MOUSEBUTTONUP;

			switch (e.button.button) {
			case SDL_BUTTON_LEFT:
				schism_event.button.button = MOUSE_BUTTON_LEFT;
				break;
			case SDL_BUTTON_MIDDLE:
				schism_event.button.button = MOUSE_BUTTON_MIDDLE;
				break;
			case SDL_BUTTON_RIGHT:
				schism_event.button.button = MOUSE_BUTTON_RIGHT;
				break;
			default:
				break;
			}

			switch (e.button.button) {
			case SDL_BUTTON_LEFT:
			case SDL_BUTTON_MIDDLE:
			case SDL_BUTTON_RIGHT:
				schism_event.button.state = !!e.button.state;
				//schism_event.button.clicks = e.button.clicks;

				schism_event.button.x = e.button.x;
				schism_event.button.y = e.button.y;
				events_push_event(&schism_event);
				break;
			case SDL_BUTTON_WHEELDOWN:
			case SDL_BUTTON_WHEELUP: {
				schism_event.type = SCHISM_MOUSEWHEEL;
				schism_event.wheel.x = 0;
				schism_event.wheel.y = (e.button.button == SDL_BUTTON_WHEELDOWN) ? -1 : 1;
				unsigned int mx, my;
				video_get_mouse_coordinates(&mx, &my);
				schism_event.wheel.mouse_x = mx;
				schism_event.wheel.mouse_y = my;
				events_push_event(&schism_event);
				break;
			}
			default:
				break;
			}
			break;
		case SDL_SYSWMEVENT:
			schism_event.type = SCHISM_EVENT_WM_MSG;
			schism_event.wm_msg.backend = SCHISM_WM_MSG_BACKEND_SDL12;
#ifdef SCHISM_WIN32
			schism_event.wm_msg.subsystem = SCHISM_WM_MSG_SUBSYSTEM_WINDOWS;
			schism_event.wm_msg.msg.win.hwnd = e.syswm.msg->hwnd;
			schism_event.wm_msg.msg.win.msg = e.syswm.msg->msg;
			schism_event.wm_msg.msg.win.wparam = e.syswm.msg->wParam;
			schism_event.wm_msg.msg.win.lparam = e.syswm.msg->lParam;
#elif defined(SDL_VIDEO_DRIVER_X11) && defined(SCHISM_USE_X11)
			if (e.syswm.msg->subsystem == SDL_SYSWM_X11) {
				schism_event.wm_msg.subsystem = SCHISM_WM_MSG_SUBSYSTEM_X11;
				schism_event.wm_msg.msg.x11.event.type = e.syswm.msg->event.xevent.type;
				if (e.syswm.msg->event.xevent.type == SelectionRequest) {
					schism_event.wm_msg.msg.x11.event.selection_request.serial = e.syswm.msg->event.xevent.xselectionrequest.serial;
					schism_event.wm_msg.msg.x11.event.selection_request.send_event = e.syswm.msg->event.xevent.xselectionrequest.send_event;     // `Bool' in Xlib
					schism_event.wm_msg.msg.x11.event.selection_request.display = e.syswm.msg->event.xevent.xselectionrequest.display;      // `Display *' in Xlib
					schism_event.wm_msg.msg.x11.event.selection_request.owner = e.syswm.msg->event.xevent.xselectionrequest.owner;     // `Window' in Xlib
					schism_event.wm_msg.msg.x11.event.selection_request.requestor = e.syswm.msg->event.xevent.xselectionrequest.requestor; // `Window' in Xlib
					schism_event.wm_msg.msg.x11.event.selection_request.selection = e.syswm.msg->event.xevent.xselectionrequest.selection; // `Atom' in Xlib
					schism_event.wm_msg.msg.x11.event.selection_request.target = e.syswm.msg->event.xevent.xselectionrequest.target;    // `Atom' in Xlib
					schism_event.wm_msg.msg.x11.event.selection_request.property = e.syswm.msg->event.xevent.xselectionrequest.property;  // `Atom' in Xlib
					schism_event.wm_msg.msg.x11.event.selection_request.time = e.syswm.msg->event.xevent.xselectionrequest.time;      // `Time' in Xlib
				}
			}
#endif
			events_push_event(&schism_event);
			break;
		default:
			break;
		}
	}
}

static const char *sdl12_get_key_name_from_scancode(int scancode)
{
	switch (scancode)
	{
	/* USB keyboard page... */
		case SCHISM_SCANCODE_A: return "A";
		case SCHISM_SCANCODE_B: return "B";
		case SCHISM_SCANCODE_C: return "C";
		case SCHISM_SCANCODE_D: return "D";
		case SCHISM_SCANCODE_E: return "E";
		case SCHISM_SCANCODE_F: return "F";
		case SCHISM_SCANCODE_G: return "G";
		case SCHISM_SCANCODE_H: return "H";
		case SCHISM_SCANCODE_I: return "I";
		case SCHISM_SCANCODE_J: return "J";
		case SCHISM_SCANCODE_K: return "K";
		case SCHISM_SCANCODE_L: return "L";
		case SCHISM_SCANCODE_M: return "M";
		case SCHISM_SCANCODE_N: return "N";
		case SCHISM_SCANCODE_O: return "O";
		case SCHISM_SCANCODE_P: return "P";
		case SCHISM_SCANCODE_Q: return "Q";
		case SCHISM_SCANCODE_R: return "R";
		case SCHISM_SCANCODE_S: return "S";
		case SCHISM_SCANCODE_T: return "T";
		case SCHISM_SCANCODE_U: return "U";
		case SCHISM_SCANCODE_V: return "V";
		case SCHISM_SCANCODE_W: return "W";
		case SCHISM_SCANCODE_X: return "X";
		case SCHISM_SCANCODE_Y: return "Y";
		case SCHISM_SCANCODE_Z: return "Z";

		case SCHISM_SCANCODE_1: return "1";
		case SCHISM_SCANCODE_2: return "2";
		case SCHISM_SCANCODE_3: return "3";
		case SCHISM_SCANCODE_4: return "4";
		case SCHISM_SCANCODE_5: return "5";
		case SCHISM_SCANCODE_6: return "6";
		case SCHISM_SCANCODE_7: return "7";
		case SCHISM_SCANCODE_8: return "8";
		case SCHISM_SCANCODE_9: return "9";
		case SCHISM_SCANCODE_0: return "0";

		case SCHISM_SCANCODE_RETURN: return "Enter";
		case SCHISM_SCANCODE_ESCAPE: return "Esc";
		case SCHISM_SCANCODE_BACKSPACE: return "Backspace";
		case SCHISM_SCANCODE_TAB: return "Tab";
		case SCHISM_SCANCODE_SPACE: return "Space";

		case SCHISM_SCANCODE_MINUS: return "-";
		case SCHISM_SCANCODE_EQUALS: return "=";
		case SCHISM_SCANCODE_LEFTBRACKET: return "[";
		case SCHISM_SCANCODE_RIGHTBRACKET: return "]";
		case SCHISM_SCANCODE_BACKSLASH: return "\\";
		case SCHISM_SCANCODE_NONUSHASH: return "#";
		case SCHISM_SCANCODE_SEMICOLON: return ";";
		case SCHISM_SCANCODE_APOSTROPHE: return "'";
		case SCHISM_SCANCODE_GRAVE: return "`";
		case SCHISM_SCANCODE_COMMA: return ",";
		case SCHISM_SCANCODE_PERIOD: return ".";
		case SCHISM_SCANCODE_SLASH: return "/";

		case SCHISM_SCANCODE_CAPSLOCK: return "CapsLock";

		case SCHISM_SCANCODE_F1: return "F1";
		case SCHISM_SCANCODE_F2: return "F2";
		case SCHISM_SCANCODE_F3: return "F3";
		case SCHISM_SCANCODE_F4: return "F4";
		case SCHISM_SCANCODE_F5: return "F5";
		case SCHISM_SCANCODE_F6: return "F6";
		case SCHISM_SCANCODE_F7: return "F7";
		case SCHISM_SCANCODE_F8: return "F8";
		case SCHISM_SCANCODE_F9: return "F9";
		case SCHISM_SCANCODE_F10: return "F10";
		case SCHISM_SCANCODE_F11: return "F11";
		case SCHISM_SCANCODE_F12: return "F12";

		case SCHISM_SCANCODE_PRINTSCREEN: return "PrScr";
		case SCHISM_SCANCODE_SCROLLLOCK: return "ScrLk";
		case SCHISM_SCANCODE_PAUSE: return "Pause";
		case SCHISM_SCANCODE_INSERT: return "Ins";
		case SCHISM_SCANCODE_HOME: return "Home";
		case SCHISM_SCANCODE_PAGEUP: return "PgUp";
		case SCHISM_SCANCODE_DELETE: return "Del";
		case SCHISM_SCANCODE_END: return "End";
		case SCHISM_SCANCODE_PAGEDOWN: return "PgDn";
		case SCHISM_SCANCODE_RIGHT: return "Right";
		case SCHISM_SCANCODE_LEFT: return "Left";
		case SCHISM_SCANCODE_DOWN: return "Down";
		case SCHISM_SCANCODE_UP: return "Up";

		case SCHISM_SCANCODE_NUMLOCKCLEAR: return "NumLk";
		case SCHISM_SCANCODE_KP_DIVIDE: return "/";
		case SCHISM_SCANCODE_KP_MULTIPLY: return "*";
		case SCHISM_SCANCODE_KP_MINUS: return "-";
		case SCHISM_SCANCODE_KP_PLUS: return "+";
		case SCHISM_SCANCODE_KP_ENTER: return "Enter";
		case SCHISM_SCANCODE_KP_1: return "1";
		case SCHISM_SCANCODE_KP_2: return "2";
		case SCHISM_SCANCODE_KP_3: return "3";
		case SCHISM_SCANCODE_KP_4: return "4";
		case SCHISM_SCANCODE_KP_5: return "5";
		case SCHISM_SCANCODE_KP_6: return "6";
		case SCHISM_SCANCODE_KP_7: return "7";
		case SCHISM_SCANCODE_KP_8: return "8";
		case SCHISM_SCANCODE_KP_9: return "9";
		case SCHISM_SCANCODE_KP_0: return "0";
		case SCHISM_SCANCODE_KP_PERIOD: return ".";

		case SCHISM_SCANCODE_NONUSBACKSLASH: return "\\";
		case SCHISM_SCANCODE_APPLICATION: return "Win";
		case SCHISM_SCANCODE_POWER: return "Power";
		case SCHISM_SCANCODE_KP_EQUALS: return "=";
		case SCHISM_SCANCODE_F13: return "F13";
		case SCHISM_SCANCODE_F14: return "F14";
		case SCHISM_SCANCODE_F15: return "F15";
		case SCHISM_SCANCODE_F16: return "F16";
		case SCHISM_SCANCODE_F17: return "F17";
		case SCHISM_SCANCODE_F18: return "F18";
		case SCHISM_SCANCODE_F19: return "F19";
		case SCHISM_SCANCODE_F20: return "F20";
		case SCHISM_SCANCODE_F21: return "F21";
		case SCHISM_SCANCODE_F22: return "F22";
		case SCHISM_SCANCODE_F23: return "F23";
		case SCHISM_SCANCODE_F24: return "F24";
		case SCHISM_SCANCODE_EXECUTE: return "Exe";
		case SCHISM_SCANCODE_HELP: return "Help";
		case SCHISM_SCANCODE_MENU: return "Menu";
		case SCHISM_SCANCODE_SELECT: return "Sel";
		case SCHISM_SCANCODE_STOP: return "Stop";
		case SCHISM_SCANCODE_AGAIN: return "Redo";
		case SCHISM_SCANCODE_UNDO: return "Undo";
		case SCHISM_SCANCODE_CUT: return "Cut";
		case SCHISM_SCANCODE_COPY: return "Copy";
		case SCHISM_SCANCODE_PASTE: return "Paste";
		case SCHISM_SCANCODE_FIND: return "Find";
		case SCHISM_SCANCODE_MUTE: return "Mute";
		case SCHISM_SCANCODE_VOLUMEUP: return "VolUp";
		case SCHISM_SCANCODE_VOLUMEDOWN: return "VolDn";
		case SCHISM_SCANCODE_KP_COMMA: return ",";
		case SCHISM_SCANCODE_KP_EQUALSAS400: return "=";

		case SCHISM_SCANCODE_ALTERASE: return "Erase";
		case SCHISM_SCANCODE_SYSREQ: return "SysRq";
		case SCHISM_SCANCODE_CANCEL: return "Cancel";
		case SCHISM_SCANCODE_CLEAR: return "Clr";
		case SCHISM_SCANCODE_PRIOR: return "Prior";
		case SCHISM_SCANCODE_RETURN2: return "Return";
		case SCHISM_SCANCODE_SEPARATOR: return "Sep";
		case SCHISM_SCANCODE_OUT: return "Out";
		case SCHISM_SCANCODE_OPER: return "Oper";
		case SCHISM_SCANCODE_CLEARAGAIN: return "ClrAgain";
		case SCHISM_SCANCODE_CRSEL: return "CrSel";
		case SCHISM_SCANCODE_EXSEL: return "ExSel";

		case SCHISM_SCANCODE_KP_00: return "00";
		case SCHISM_SCANCODE_KP_000: return "000";
		case SCHISM_SCANCODE_THOUSANDSSEPARATOR: return ",...";
		case SCHISM_SCANCODE_DECIMALSEPARATOR: return ".__";
		case SCHISM_SCANCODE_CURRENCYUNIT: return "$";
		case SCHISM_SCANCODE_CURRENCYSUBUNIT: return "¢";
		case SCHISM_SCANCODE_KP_LEFTPAREN: return "[";
		case SCHISM_SCANCODE_KP_RIGHTPAREN: return "]";
		case SCHISM_SCANCODE_KP_LEFTBRACE: return "{";
		case SCHISM_SCANCODE_KP_RIGHTBRACE: return "}";
		case SCHISM_SCANCODE_KP_TAB: return "Tab";
		case SCHISM_SCANCODE_KP_BACKSPACE: return "Backspace";
		case SCHISM_SCANCODE_KP_COLON: return ":";
		case SCHISM_SCANCODE_KP_HASH: return "#";
		case SCHISM_SCANCODE_KP_SPACE: return "Space";
		case SCHISM_SCANCODE_KP_AT: return "@";
		case SCHISM_SCANCODE_KP_EXCLAM: return "!";
		case SCHISM_SCANCODE_KP_MEMSTORE: return "MemSt";
		case SCHISM_SCANCODE_KP_MEMRECALL: return "MemRe";
		case SCHISM_SCANCODE_KP_MEMCLEAR: return "MemCl";
		case SCHISM_SCANCODE_KP_MEMADD: return "MemAdd";
		case SCHISM_SCANCODE_KP_MEMSUBTRACT: return "MemSub";
		case SCHISM_SCANCODE_KP_MEMMULTIPLY: return "MemMul";
		case SCHISM_SCANCODE_KP_MEMDIVIDE: return "MemDiv";
		case SCHISM_SCANCODE_KP_PLUSMINUS: return "+/-";
		case SCHISM_SCANCODE_KP_CLEAR: return "Clr";
		case SCHISM_SCANCODE_KP_CLEARENTRY: return "ClrEn";
		case SCHISM_SCANCODE_KP_BINARY: return "Bin";
		case SCHISM_SCANCODE_KP_OCTAL: return "Oct";
		case SCHISM_SCANCODE_KP_DECIMAL: return "Dec";
		case SCHISM_SCANCODE_KP_HEXADECIMAL: return "Hex";

		case SCHISM_SCANCODE_LCTRL: return "Ctrl";
		case SCHISM_SCANCODE_LSHIFT: return "Shift";
		case SCHISM_SCANCODE_LALT: return "Alt";
		case SCHISM_SCANCODE_LGUI: return "Win";
		case SCHISM_SCANCODE_RCTRL: return "Ctrl";
		case SCHISM_SCANCODE_RSHIFT: return "Shift";
		case SCHISM_SCANCODE_RALT: return "Alt";
		case SCHISM_SCANCODE_RGUI: return "Win";
	}

	// SCHISM_SCANCODE_UNKNOWN or actually unknown
	return "<unknown>";
}

//////////////////////////////////////////////////////////////////////////////

static int sdl12_events_load_syms(void)
{
	SCHISM_SDL12_SYM(GetModState);
	SCHISM_SDL12_SYM(PollEvent);
	SCHISM_SDL12_SYM(EventState);
	SCHISM_SDL12_SYM(GetAppState);

	return 0;
}

static int sdl12_events_init(void)
{
	if (!sdl12_init())
		return 0;

	if (sdl12_events_load_syms())
		return 0;

#if defined(SCHISM_WIN32) || defined(SCHISM_USE_X11)
	sdl12_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#endif

	app_state = sdl12_GetAppState();

	return 1;
}

static void sdl12_events_quit(void)
{
	sdl12_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_events_backend_t schism_events_backend_sdl12 = {
	.init = sdl12_events_init,
	.quit = sdl12_events_quit,

	.keymod_state = sdl12_event_mod_state,
	.pump_events = sdl12_pump_events,
	.get_key_name_from_scancode = sdl12_get_key_name_from_scancode,
};
