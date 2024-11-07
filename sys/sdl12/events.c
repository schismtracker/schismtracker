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

#include <SDL.h>

/* These are here for linking text input to keyboard inputs.
 * If no keyboard input can be found, then the text will
 * be sent as a SCHISM_TEXTINPUT event.
 *
 * - paper */

static schism_event_t pending_keydown;
static int have_pending_keydown = 0;

static void push_pending_keydown(schism_event_t *event)
{
	if (!have_pending_keydown) {
		pending_keydown = *event;
		have_pending_keydown = 1;
	}
}

static void pop_pending_keydown(const char *text)
{
	/* text should always be in UTF-8 here */
	if (have_pending_keydown) {
		if (text) {
			strncpy(pending_keydown.text.text, text, ARRAY_SIZE(pending_keydown.text.text));
			pending_keydown.text.text[ARRAY_SIZE(pending_keydown.text.text)-1] = '\0';
		}
		schism_push_event(&pending_keydown);
		have_pending_keydown = 0;
	}
}

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
		if (key == SDLK_PAUSE)
			return SCHISM_KEYSYM_PAUSE;
		if (key == SDLK_CLEAR)
			return SCHISM_KEYSYM_CLEAR;
		return (schism_keysym_t)key;
	}

    switch (key) {
    #define CASEKEYSYM20TO12(schism, k12) case SDLK_##k12: return SCHISM_KEYSYM_##schism
    CASEKEYSYM20TO12(KP_0, KP0);
    CASEKEYSYM20TO12(KP_1, KP1);
    CASEKEYSYM20TO12(KP_2, KP2);
    CASEKEYSYM20TO12(KP_3, KP3);
    CASEKEYSYM20TO12(KP_4, KP4);
    CASEKEYSYM20TO12(KP_5, KP5);
    CASEKEYSYM20TO12(KP_6, KP6);
    CASEKEYSYM20TO12(KP_7, KP7);
    CASEKEYSYM20TO12(KP_8, KP8);
    CASEKEYSYM20TO12(KP_9, KP9);
    CASEKEYSYM20TO12(NUMLOCKCLEAR, NUMLOCK);
    CASEKEYSYM20TO12(SCROLLLOCK, SCROLLOCK);
    CASEKEYSYM20TO12(RGUI, RMETA);
    CASEKEYSYM20TO12(LGUI, LMETA);
    CASEKEYSYM20TO12(PRINTSCREEN, PRINT);
    #undef CASEKEYSYM20TO12

    #define CASEKEYSYM20TO12(k) case SDLK_##k: return SCHISM_KEYSYM_##k
    CASEKEYSYM20TO12(CLEAR);
    CASEKEYSYM20TO12(PAUSE);
    CASEKEYSYM20TO12(KP_PERIOD);
    CASEKEYSYM20TO12(KP_DIVIDE);
    CASEKEYSYM20TO12(KP_MULTIPLY);
    CASEKEYSYM20TO12(KP_MINUS);
    CASEKEYSYM20TO12(KP_PLUS);
    CASEKEYSYM20TO12(KP_ENTER);
    CASEKEYSYM20TO12(KP_EQUALS);
    CASEKEYSYM20TO12(UP);
    CASEKEYSYM20TO12(DOWN);
    CASEKEYSYM20TO12(RIGHT);
    CASEKEYSYM20TO12(LEFT);
    CASEKEYSYM20TO12(INSERT);
    CASEKEYSYM20TO12(HOME);
    CASEKEYSYM20TO12(END);
    CASEKEYSYM20TO12(PAGEUP);
    CASEKEYSYM20TO12(PAGEDOWN);
    CASEKEYSYM20TO12(F1);
    CASEKEYSYM20TO12(F2);
    CASEKEYSYM20TO12(F3);
    CASEKEYSYM20TO12(F4);
    CASEKEYSYM20TO12(F5);
    CASEKEYSYM20TO12(F6);
    CASEKEYSYM20TO12(F7);
    CASEKEYSYM20TO12(F8);
    CASEKEYSYM20TO12(F9);
    CASEKEYSYM20TO12(F10);
    CASEKEYSYM20TO12(F11);
    CASEKEYSYM20TO12(F12);
    CASEKEYSYM20TO12(F13);
    CASEKEYSYM20TO12(F14);
    CASEKEYSYM20TO12(F15);
    CASEKEYSYM20TO12(CAPSLOCK);
    CASEKEYSYM20TO12(RSHIFT);
    CASEKEYSYM20TO12(LSHIFT);
    CASEKEYSYM20TO12(RCTRL);
    CASEKEYSYM20TO12(LCTRL);
    CASEKEYSYM20TO12(RALT);
    CASEKEYSYM20TO12(LALT);
    //CASEKEYSYM20TO12(MODE);
    CASEKEYSYM20TO12(HELP);
    CASEKEYSYM20TO12(SYSREQ);
    CASEKEYSYM20TO12(MENU);
    CASEKEYSYM20TO12(POWER);
    CASEKEYSYM20TO12(UNDO);
    #undef CASEKEYSYM20TO12
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
	/* Mac OS X */
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
	//case 0x00: return SCHISM_SCANCODE_A;
	case 0x27: return SCHISM_SCANCODE_APOSTROPHE;
	case 0x0B: return SCHISM_SCANCODE_B;
	case 0x2A: return SCHISM_SCANCODE_BACKSLASH;
	case 0x33: return SCHISM_SCANCODE_BACKSPACE;
	case 0x08: return SCHISM_SCANCODE_C;
	//case 0x00: return SCHISM_SCANCODE_CAPSLOCK;
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

void sdl12_pump_events(void)
{
	/* Convert our events to Schism's internal representation */
	SDL_Event e;

	while (SDL_PollEvent(&e)) {
		schism_event_t schism_event = {0};

		// fill this in
		schism_event.common.timestamp = be_timer_ticks();

#ifdef SCHISM_CONTROLLER
		if (!controller_sdlevent())
			continue;
#endif

		switch (e.type) {
		case SDL_QUIT:
			schism_event.type = SCHISM_QUIT;
			schism_push_event(&schism_event);
			break;
		case SDL_VIDEORESIZE:
			schism_event.type = SCHISM_WINDOWEVENT_RESIZED;
			schism_event.window.data.resized.width = e.resize.w;
			schism_event.window.data.resized.height = e.resize.h;
			schism_push_event(&schism_event);
			break;
		case SDL_VIDEOEXPOSE:
			schism_event.type = SCHISM_WINDOWEVENT_EXPOSED;
			schism_push_event(&schism_event);
			break;
		case SDL_ACTIVEEVENT:
			switch (e.active.state) {
			case SDL_APPINPUTFOCUS:
				schism_event.type = (e.active.gain) ? SCHISM_WINDOWEVENT_FOCUS_GAINED : SCHISM_WINDOWEVENT_FOCUS_LOST;
				schism_push_event(&schism_event);
				break;
			case SDL_APPACTIVE:
				schism_event.type = (e.active.gain) ? SCHISM_WINDOWEVENT_SHOWN : SCHISM_WINDOWEVENT_HIDDEN;
				schism_push_event(&schism_event);
				break;
			default:
				break;
			}
			break;
		case SDL_KEYDOWN:
			pop_pending_keydown(NULL);

			schism_event.type = SCHISM_KEYDOWN;
			schism_event.key.state = KEY_PRESS;

			schism_event.key.sym = sdl12_keycode_trans(e.key.keysym.sym);
			schism_event.key.scancode = sdl12_scancode_trans(e.key.keysym.scancode);
			schism_event.key.mod = sdl12_modkey_trans(e.key.keysym.mod);

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

			schism_push_event(&schism_event);

			break;
		case SDL_KEYUP:
			pop_pending_keydown(NULL);

			schism_event.type = SCHISM_KEYUP;
			schism_event.key.state = KEY_RELEASE;
			schism_event.key.sym = e.key.keysym.sym;
			schism_event.key.scancode = e.key.keysym.scancode;
			schism_event.key.mod = e.key.keysym.mod;

			schism_push_event(&schism_event);

			break;
		case SDL_MOUSEMOTION:
			schism_event.type = SCHISM_MOUSEMOTION;
			schism_event.motion.x = e.motion.x;
			schism_event.motion.y = e.motion.y;
			schism_push_event(&schism_event);
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
				schism_push_event(&schism_event);
				break;
			case SDL_BUTTON_WHEELDOWN:
			case SDL_BUTTON_WHEELUP:
				schism_event.type = SCHISM_MOUSEWHEEL;
				schism_event.wheel.x = 0;
				schism_event.wheel.y = (e.type == SDL_BUTTON_WHEELDOWN) ? -1 : 1;
				schism_event.wheel.mouse_x = e.button.x;
				schism_event.wheel.mouse_y = e.button.y;
				schism_push_event(&schism_event);
				break;
			default:
				break;
			}
			break;
		}
	}

	pop_pending_keydown(NULL);
}
