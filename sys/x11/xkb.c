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

#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>

#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
#  include <X11/extensions/xf86misc.h>
#endif
#ifdef HAVE_X11_EXTENSIONS_XKB_H
#include <X11/XKBlib.h>
#endif

/* FIXME: don't put declarations in c files... */
unsigned key_repeat_rate(void);
unsigned key_repeat_delay(void);
int key_scancode_lookup(int k);


static int virgin = 1;
static unsigned int delay, rate;
static int unscan_db[256];

static void _key_info_setup(void)
{
#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
	int a, b;
	XF86MiscKbdSettings kbdsettings;
#endif
	int i;
	Display *dpy;
	SDL_SysWMinfo info;

	if (!virgin) return;
	virgin = 0;

	memset(&info, 0, sizeof(info));
	SDL_VERSION(&info.version);
	if (SDL_GetWMInfo(&info)) {
		if (info.info.x11.lock_func)
			info.info.x11.lock_func();
		dpy = info.info.x11.display;
	} else {
		dpy = 0;
	}
	if (!dpy) {
		dpy = XOpenDisplay(0);
		if (!dpy) return;
		memset(&info, 0, sizeof(info));
	}

	/* all the keys you're likely to use */
	for (i = 0; i < 256; i++) {
		unscan_db[ XKeysymToKeycode(dpy, i) & 255 ] = i;
	}
	/* in case these are different... */
	unscan_db[ XKeysymToKeycode(dpy, XK_0) & 255 ] = SDLK_0;
	unscan_db[ XKeysymToKeycode(dpy, XK_1) & 255 ] = SDLK_1;
	unscan_db[ XKeysymToKeycode(dpy, XK_2) & 255 ] = SDLK_2;
	unscan_db[ XKeysymToKeycode(dpy, XK_3) & 255 ] = SDLK_3;
	unscan_db[ XKeysymToKeycode(dpy, XK_4) & 255 ] = SDLK_4;
	unscan_db[ XKeysymToKeycode(dpy, XK_5) & 255 ] = SDLK_5;
	unscan_db[ XKeysymToKeycode(dpy, XK_6) & 255 ] = SDLK_6;
	unscan_db[ XKeysymToKeycode(dpy, XK_7) & 255 ] = SDLK_7;
	unscan_db[ XKeysymToKeycode(dpy, XK_8) & 255 ] = SDLK_8;
	unscan_db[ XKeysymToKeycode(dpy, XK_9) & 255 ] = SDLK_9;
	unscan_db[ XKeysymToKeycode(dpy, XK_q) & 255 ] = SDLK_q;
	unscan_db[ XKeysymToKeycode(dpy, XK_w) & 255 ] = SDLK_w;
	unscan_db[ XKeysymToKeycode(dpy, XK_e) & 255 ] = SDLK_e;
	unscan_db[ XKeysymToKeycode(dpy, XK_r) & 255 ] = SDLK_r;
	unscan_db[ XKeysymToKeycode(dpy, XK_t) & 255 ] = SDLK_t;
	unscan_db[ XKeysymToKeycode(dpy, XK_y) & 255 ] = SDLK_y;
	unscan_db[ XKeysymToKeycode(dpy, XK_u) & 255 ] = SDLK_u;
	unscan_db[ XKeysymToKeycode(dpy, XK_i) & 255 ] = SDLK_i;
	unscan_db[ XKeysymToKeycode(dpy, XK_o) & 255 ] = SDLK_o;
	unscan_db[ XKeysymToKeycode(dpy, XK_p) & 255 ] = SDLK_p;
	unscan_db[ XKeysymToKeycode(dpy, XK_a) & 255 ] = SDLK_a;
	unscan_db[ XKeysymToKeycode(dpy, XK_s) & 255 ] = SDLK_s;
	unscan_db[ XKeysymToKeycode(dpy, XK_d) & 255 ] = SDLK_d;
	unscan_db[ XKeysymToKeycode(dpy, XK_f) & 255 ] = SDLK_f;
	unscan_db[ XKeysymToKeycode(dpy, XK_g) & 255 ] = SDLK_g;
	unscan_db[ XKeysymToKeycode(dpy, XK_h) & 255 ] = SDLK_h;
	unscan_db[ XKeysymToKeycode(dpy, XK_j) & 255 ] = SDLK_j;
	unscan_db[ XKeysymToKeycode(dpy, XK_k) & 255 ] = SDLK_k;
	unscan_db[ XKeysymToKeycode(dpy, XK_l) & 255 ] = SDLK_l;
	unscan_db[ XKeysymToKeycode(dpy, XK_z) & 255 ] = SDLK_z;
	unscan_db[ XKeysymToKeycode(dpy, XK_x) & 255 ] = SDLK_x;
	unscan_db[ XKeysymToKeycode(dpy, XK_c) & 255 ] = SDLK_c;
	unscan_db[ XKeysymToKeycode(dpy, XK_v) & 255 ] = SDLK_v;
	unscan_db[ XKeysymToKeycode(dpy, XK_b) & 255 ] = SDLK_b;
	unscan_db[ XKeysymToKeycode(dpy, XK_n) & 255 ] = SDLK_n;
	unscan_db[ XKeysymToKeycode(dpy, XK_m) & 255 ] = SDLK_m;
	unscan_db[ XKeysymToKeycode(dpy, XK_braceleft) & 255 ] = SDLK_LEFTBRACKET;
	unscan_db[ XKeysymToKeycode(dpy, XK_braceright) & 255 ] = SDLK_RIGHTBRACKET;
	unscan_db[ XKeysymToKeycode(dpy, XK_bracketleft) & 255 ] = SDLK_LEFTBRACKET;
	unscan_db[ XKeysymToKeycode(dpy, XK_bracketright) & 255 ] = SDLK_RIGHTBRACKET;
	unscan_db[ XKeysymToKeycode(dpy, XK_asciitilde) & 255 ] = SDLK_BACKQUOTE;
	unscan_db[ XKeysymToKeycode(dpy, XK_grave) & 255 ] = SDLK_BACKQUOTE;
	unscan_db[ XKeysymToKeycode(dpy, XK_comma) & 255 ] = SDLK_COMMA;
	unscan_db[ XKeysymToKeycode(dpy, XK_period) & 255 ] = SDLK_PERIOD;
	unscan_db[ XKeysymToKeycode(dpy, XK_greater) & 255 ] = SDLK_PERIOD;
	unscan_db[ XKeysymToKeycode(dpy, XK_less) & 255 ] = SDLK_PERIOD;

#ifdef HAVE_X11_EXTENSIONS_XKB_H
	if (XkbGetAutoRepeatRate(dpy, XkbUseCoreKbd, &delay, &rate)) {
		if (info.info.x11.unlock_func)
			info.info.x11.unlock_func();
		return;
	}
#endif

#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
	if (XF86MiscQueryExtension(dpy, &a, &b)) {
		XF86MiscGetKbdSettings(dpy, &kbdsettings);
		if (kbdsettings.delay > -1 && kbdsettings.rate > -1) {
			delay = kbdsettings.delay;
			rate = kbdsettings.rate;
			if (info.info.x11.unlock_func)
				info.info.x11.unlock_func();
			return;
		}
	}
#endif

	/* eh... */
	delay = 125;
	rate = 30;

	if (info.info.x11.unlock_func)
		info.info.x11.unlock_func();
}

unsigned key_repeat_rate(void)
{
	_key_info_setup(); return rate;
}

unsigned key_repeat_delay(void)
{
	_key_info_setup(); return delay;
}

int key_scancode_lookup(int k)
{
	return unscan_db[k&255];
}
