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

/* I wanted to just lift this code out of xscreensaver-command,
   but that wasn't really convenient. xscreensaver really should've
   had this (or something like it) as a simple library call like:
	xscreensaver_synthetic_user_active(display);
   or something like that. spawning a subprocess is really expensive on
   some systems, and might have subtle interactions with SDL or the player.

   -mrsb
*/

#define NEED_TIME
#include "headers.h"
#include "video.h"

#include "sdlmain.h"
#include "osdefs.h"

#include <X11/Xproto.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xos.h>


static XErrorHandler old_handler = NULL;
static int BadWindow_ehandler(Display *dpy, XErrorEvent *error)
{
	if (error->error_code == BadWindow) {
		return 0;
	} else {
		if (old_handler) return (*old_handler)(dpy,error);
		/* shrug */
		return 1;
	}
}

void x11_screensaver_deactivate(void)
{
	static Atom XA_SCREENSAVER_VERSION;
	static Atom XA_DEACTIVATE;
	static Atom XA_SCREENSAVER;
	static int setup = 0;
	static int useit = 0;
	static SDL_SysWMinfo info;

	Window root, tmp, parent, *kids;
	unsigned int nkids, i;

	static time_t lastpoll = 0;
	Window win;
	time_t now;

	Display *dpy = NULL;
	XEvent ev;

	if (!setup) {
		setup = 1;
		SDL_GetWindowWMInfo(video_window(), &info);
		dpy = info.info.x11.display;
		if (!dpy) {
			dpy = XOpenDisplay(NULL);
			if (!dpy) return;
			memset(&info, 0, sizeof(info));
			info.info.x11.display = dpy;
		}

		useit = 1;
		XA_SCREENSAVER = XInternAtom(dpy, "SCREENSAVER", False);
		XA_SCREENSAVER_VERSION = XInternAtom(dpy,
					"_SCREENSAVER_VERSION", False);
		XA_DEACTIVATE = XInternAtom(dpy, "DEACTIVATE", False);
	}

	if (!useit) return;

	time(&now);
	if (!(lastpoll - now)) {
		return;
	}
	lastpoll = now;

	dpy = info.info.x11.display;
	if (!dpy) {
		useit = 0;
		return;
	}

	root = RootWindowOfScreen(DefaultScreenOfDisplay(dpy));
	if (!XQueryTree(dpy, root, &tmp, &parent, &kids, &nkids)) {
		useit = 0;
		return;
	}
	if (root != tmp || parent || !(kids && nkids)) {
		useit = 0;
		return;
	}

	win = 0;
	for (i = 0; i < nkids; i++) {
		Atom type;
		int format;
		unsigned long nitems, bytesafter;
		unsigned char *v = NULL;

		XSync(dpy, False);
		old_handler = XSetErrorHandler(BadWindow_ehandler);
		if (XGetWindowProperty(dpy, kids[i],
		XA_SCREENSAVER_VERSION, 0, 200,
		False, XA_STRING, &type, &format,
		&nitems, &bytesafter,
		(unsigned char **)&v) == Success) {
			XSetErrorHandler(old_handler);
			if (v) XFree(v); /* don't care */
			if (type != None) {
				win = kids[i];
				break;
			}
		}
		XSetErrorHandler(old_handler);
	}
	XFree(kids);
	if (!win) {
		useit = 0;
		return;
	}

	ev.xany.type = ClientMessage;
	ev.xclient.display = dpy;
	ev.xclient.window = win;
	ev.xclient.message_type = XA_SCREENSAVER;
	ev.xclient.format = 32;
	memset(&ev.xclient.data, 0, sizeof(ev.xclient.data));
	ev.xclient.data.l[0] = XA_DEACTIVATE;
	ev.xclient.data.l[1] = 0;
	ev.xclient.data.l[2] = 0;
	(void)XSendEvent(dpy, win, False, 0L, &ev);
	XSync(dpy, 0);
}

