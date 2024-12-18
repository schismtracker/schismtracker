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
#include "loadso.h"
#include "util.h"

#include "init.h"

#include <X11/Xlib.h>



/* Our dynamically loaded symbols */
Display *(*X11_XOpenDisplay)(const char *display_name) = NULL;
int (*X11_XCloseDisplay)(Display *display) = NULL;
Atom (*X11_XInternAtom)(Display *display, const char *atom_name, Bool only_if_exists) = NULL;
int (*X11_XChangeProperty)(Display *display, Window w, Atom property, Atom type, int format, int mode, const unsigned char *data, int nelements) = NULL;
Window (*X11_XGetSelectionOwner)(Display *display, Atom selection) = NULL;
int (*X11_XConvertSelection)(Display *display, Atom selection, Atom target, Atom property, Window requestor, Time time) = NULL;
int (*X11_XFree)(void *ptr) = NULL;
int (*X11_XGetWindowProperty)(Display *display, Window w, Atom property, long long_offset, long long_length, Bool delete, Atom req_type, 
	Atom *actual_type_return, int *actual_format_return, unsigned long *nitems_return, unsigned long *bytes_after_return,
	unsigned char **prop_return) = NULL;
int (*X11_XSetSelectionOwner)(Display *display, Atom selection, Window owner, Time time) = NULL;
Status (*X11_XSendEvent)(Display *display, Window w, Bool propagate, long event_mask, XEvent *event_send) = NULL;
int (*X11_XSync)(Display *display, Bool discard) = NULL;

static int load_x11_syms(void);

#ifdef X11_DYNAMIC_LOAD

#include "loadso.h"

enum {
	X11_LIBRARY_LIBX11,
	X11_LIBRARY_MAX_,
};

#ifndef X11_LIBRARY_LIBX11_PATH
# define X11_LIBRARY_LIBX11_PATH NULL
#endif

/* Libraries table. Currently, we only ever use libX11. */
static struct {
	const char *path;
	void *lib;
} x11_dltrick_handles_[X11_LIBRARY_MAX_] = {
	[X11_LIBRARY_LIBX11] = {X11_LIBRARY_LIBX11_PATH, NULL},
};

static void x11_dlend(void)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(x11_dltrick_handles_); i++) {
		// unload
		if (x11_dltrick_handles_[i].lib) {
			loadso_object_unload(x11_dltrick_handles_[i].lib);
			x11_dltrick_handles_[i].lib = NULL;
		}
	}
}

static int x11_dlinit(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(x11_dltrick_handles_); i++) {
		// do we already have it and does the library exist?
		if (x11_dltrick_handles_[i].lib || !x11_dltrick_handles_[i].path)
			return 0;

		// eh
		x11_dltrick_handles_[i].lib = loadso_object_load(x11_dltrick_handles_[i].path);
	}

	int retval = load_x11_syms();
	if (retval < 0)
		x11_dlend();

	return retval;
}

SCHISM_STATIC_ASSERT(sizeof(void (*)) == sizeof(void *), "dynamic loading code assumes function pointer and void pointer are of equivalent size");

// `library' is the enum above
static int x11_load_sym(int library, const char *fn, void *addr)
{
	if (!x11_dltrick_handles_[library].lib)
		return 0;

	void *func = loadso_function_load(x11_dltrick_handles_[library].lib, fn);
	if (!func)
		return 0;

	memcpy(addr, &func, sizeof(void *));

	return 1;
}

# define SCHISM_X11_SYM(library, x) \
	if (!x11_load_sym(X11_LIBRARY_LIB ## library, #x, &X11_##x)) return -1

#else

static int x11_dlinit(void)
{
	load_x11_syms();
	return 0;
}

#define x11_dlend() // nothing

# define SCHISM_X11_SYM(library, x) \
	X11_##x = x

#endif

static int load_x11_syms(void)
{
	SCHISM_X11_SYM(X11, XOpenDisplay);
	SCHISM_X11_SYM(X11, XCloseDisplay);
	SCHISM_X11_SYM(X11, XInternAtom);
	SCHISM_X11_SYM(X11, XChangeProperty);
	SCHISM_X11_SYM(X11, XConvertSelection);
	SCHISM_X11_SYM(X11, XFree);
	SCHISM_X11_SYM(X11, XGetSelectionOwner);
	SCHISM_X11_SYM(X11, XSetSelectionOwner);
	SCHISM_X11_SYM(X11, XGetWindowProperty);
	SCHISM_X11_SYM(X11, XSendEvent);
	SCHISM_X11_SYM(X11, XSync);

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

static int roll = 0;

// returns non-zero on success or zero on error
int x11_init(void)
{
	if (!roll) {
		if (x11_dlinit())
			return 0;

		// ok, let's try opening the default display.
		Display *display = X11_XOpenDisplay(NULL);
		if (!display)
			return 0; // d'oh!

		X11_XCloseDisplay(display);
	}
	roll++;
	return 1;
}

void x11_quit(void)
{
	if (roll > 0)
		roll--;

	if (roll == 0)
		x11_dlend();
}
