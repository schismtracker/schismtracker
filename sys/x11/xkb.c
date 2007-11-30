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

#ifdef HAVE_X11_EXTENSIONS_XKB_H
static XkbDescPtr us_kb_map;
#endif

static void _key_info_setup(void)
{
#ifdef HAVE_X11_EXTENSIONS_XF86MISC_H
	int a, b;
	XF86MiscKbdSettings kbdsettings;
#endif
#ifdef HAVE_X11_EXTENSIONS_XKB_H
	XkbComponentNamesRec rec;
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

#ifdef HAVE_X11_EXTENSIONS_XKB_H
	rec.keymap = "";
	rec.keycodes = "";
	rec.types = "";
	rec.compat = "";
	rec.symbols = "+us(basic)";
	rec.geometry = "";
	us_kb_map = XkbGetKeyboardByName(dpy, XkbUseCoreKbd, &rec,
			XkbGBN_AllComponentsMask, XkbGBN_AllComponentsMask, False);
#else
	us_kb_map = NULL;
#endif
	if (us_kb_map == NULL) {
		log_appendf(3, "Warning: XKB support missing or broken; keymap might be off");
	} else {
		log_appendf(3, "Note: XKB will be used to override scancodes");
	}

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
	static unsigned int d;
	KeySym sym;

	if (us_kb_map != NULL &&
			XkbTranslateKeyCode(us_kb_map, k, 0, &d, &sym)) {
		return sym;
	}
	return -1;
}
