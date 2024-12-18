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

#ifndef SCHISM_SYS_X11_INIT_H_
#define SCHISM_SYS_X11_INIT_H_

#include "headers.h"

#include <X11/Xlib.h>
#include <X11/Xatom.h>

int x11_init(void);
void x11_quit(void);

// sys/x11/clippy.c
enum {
	X11_CLIPBOARD_MIME_TYPE_STRING,
	X11_CLIPBOARD_MIME_TYPE_TEXT_PLAIN,
	X11_CLIPBOARD_MIME_TYPE_TEXT_PLAIN_UTF8,
	X11_CLIPBOARD_MIME_TYPE_TEXT,
	X11_CLIPBOARD_MIME_TYPE_MAX_,
};

Atom x11_get_cut_buffer_type(Display *display, int mime_type, Atom selection_type);
Atom x11_get_cut_buffer_external_fmt(Display *display, int mime_type);
Atom x11_get_cut_buffer_internal_fmt(Display *display, int mime_type);

// dynamically loaded symbols. these are ONLY valid after a successful call to x11_init(),
// and they should be avoided (even if non-null) if you haven't called that.
extern Display *(*X11_XOpenDisplay)(const char *display_name);
extern int (*X11_XCloseDisplay)(Display *display);
extern Atom (*X11_XInternAtom)(Display *display, const char *atom_name, Bool only_if_exists);
extern int (*X11_XChangeProperty)(Display *display, Window w, Atom property, Atom type, int format, int mode, const unsigned char *data, int nelements);
extern Window (*X11_XGetSelectionOwner)(Display *display, Atom selection);
extern int (*X11_XConvertSelection)(Display *display, Atom selection, Atom target, Atom property, Window requestor, Time time);
extern int (*X11_XFree)(void *ptr);
extern int (*X11_XGetWindowProperty)(Display *display, Window w, Atom property, long long_offset, long long_length, Bool delete, Atom req_type, Atom *actual_type_return, int *actual_format_return, unsigned long *nitems_return, unsigned long *bytes_after_return, unsigned char **prop_return);
extern int (*X11_XSetSelectionOwner)(Display *display, Atom selection, Window owner, Time time);
extern Status (*X11_XSendEvent)(Display *display, Window w, Bool propagate, long event_mask, XEvent *event_send);
extern int (*X11_XSync)(Display *display, Bool discard);

#endif /* SCHISM_SYS_X11_INIT_H_ */
