/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * URL: http://rigelseven.com/schism/
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
#include "sdlmain.h"
#include "it.h"

#include <windows.h>

/* eek... */
void win32_get_modkey(int *mk)
{
	BYTE ks[256];
	if (GetKeyboardState(ks) == 0) return;

	if (ks[VK_CAPITAL] & 128) {
		status.flags |= CAPS_PRESSED;
	} else {
		status.flags &= ~CAPS_PRESSED;
	}

	(*mk) = ((*mk) & ~(KMOD_NUM|KMOD_CAPS))
		| ((ks[VK_NUMLOCK]&1) ? KMOD_NUM : 0)
		| ((ks[VK_CAPITAL]&1) ? KMOD_CAPS : 0);
}

/* more windows key stuff... */
unsigned key_repeat_rate(void)
{
	DWORD spd;
	if (!SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &spd, 0)) return 0;
	if (!spd) return 1;
	return spd;
}
unsigned key_repeat_delay(void)
{
	int delay;

	if (!SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &delay, 0)) return 0;
	switch (delay) {
	case 0: return 250;
	case 1: return 500;
	case 2: return 750;
	};
	return 1000;
}

static HKL default_keymap;
static HKL us_keymap;

void win32_setup_keymap(void)
{
	default_keymap = GetKeyboardLayout(0);
	us_keymap = LoadKeyboardLayout("00000409", KLF_ACTIVATE|KLF_REPLACELANG|KLF_NOTELLSHELL);
	(void)ActivateKeyboardLayout(default_keymap,0);
}
int key_scancode_lookup(int k)
{
#ifndef VK_0
#define VK_0	'0'
#define VK_1	'1'
#define VK_2	'2'
#define VK_3	'3'
#define VK_4	'4'
#define VK_5	'5'
#define VK_6	'6'
#define VK_7	'7'
#define VK_8	'8'
#define VK_9	'9'
#define VK_A	'A'
#define VK_B	'B'
#define VK_C	'C'
#define VK_D	'D'
#define VK_E	'E'
#define VK_F	'F'
#define VK_G	'G'
#define VK_H	'H'
#define VK_I	'I'
#define VK_J	'J'
#define VK_K	'K'
#define VK_L	'L'
#define VK_M	'M'
#define VK_N	'N'
#define VK_O	'O'
#define VK_P	'P'
#define VK_Q	'Q'
#define VK_R	'R'
#define VK_S	'S'
#define VK_T	'T'
#define VK_U	'U'
#define VK_V	'V'
#define VK_W	'W'
#define VK_X	'X'
#define VK_Y	'Y'
#define VK_Z	'Z'
#endif /* VK_0 */

/* These keys haven't been defined, but were experimentally determined */
#define VK_SEMICOLON	0xBA
#define VK_EQUALS	0xBB
#define VK_COMMA	0xBC
#define VK_MINUS	0xBD
#define VK_PERIOD	0xBE
#define VK_SLASH	0xBF
#define VK_GRAVE	0xC0
#define VK_LBRACKET	0xDB
#define VK_BACKSLASH	0xDC
#define VK_RBRACKET	0xDD
#define VK_APOSTROPHE	0xDE
#define VK_BACKTICK	0xDF
#define VK_OEM_102	0xE2
	switch (MapVirtualKeyEx(k, 1 /* MAPVK_VSC_TO_VK */, us_keymap)) {
	case VK_0: return SDLK_0;
	case VK_1: return SDLK_1;
	case VK_2: return SDLK_2;
	case VK_3: return SDLK_3;
	case VK_4: return SDLK_4;
	case VK_5: return SDLK_5;
	case VK_6: return SDLK_6;
	case VK_7: return SDLK_7;
	case VK_8: return SDLK_8;
	case VK_9: return SDLK_9;
	case VK_A: return SDLK_a;
	case VK_B: return SDLK_b;
	case VK_C: return SDLK_c;
	case VK_D: return SDLK_d;
	case VK_E: return SDLK_e;
	case VK_F: return SDLK_f;
	case VK_G: return SDLK_g;
	case VK_H: return SDLK_h;
	case VK_I: return SDLK_i;
	case VK_J: return SDLK_j;
	case VK_K: return SDLK_k;
	case VK_L: return SDLK_l;
	case VK_M: return SDLK_m;
	case VK_N: return SDLK_n;
	case VK_O: return SDLK_o;
	case VK_P: return SDLK_p;
	case VK_Q: return SDLK_q;
	case VK_R: return SDLK_r;
	case VK_S: return SDLK_s;
	case VK_T: return SDLK_t;
	case VK_U: return SDLK_u;
	case VK_V: return SDLK_v;
	case VK_W: return SDLK_w;
	case VK_X: return SDLK_x;
	case VK_Y: return SDLK_y;
	case VK_Z: return SDLK_z;
	case VK_SEMICOLON: return SDLK_SEMICOLON;
	case VK_GRAVE: return SDLK_BACKQUOTE;
	case VK_APOSTROPHE: return SDLK_QUOTE;
	case VK_BACKTICK: return SDLK_BACKQUOTE;
	case VK_BACKSLASH: return SDLK_BACKSLASH;
	case VK_LBRACKET: return SDLK_LEFTBRACKET;
	case VK_RBRACKET: return SDLK_RIGHTBRACKET;
	};
	return -1;
}
