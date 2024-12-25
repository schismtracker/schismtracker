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
#include "video.h"
#include "events.h"
#include "clippy.h"
#include "backend/clippy.h"
#include "mem.h"

#include "init.h"

/* This clipboard code was mostly stolen from SDL 2: */
/*
  Simple DirectMedia Layer
  Copyright (C) 1997-2024 Sam Lantinga <slouken@libsdl.org>

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
	 claim that you wrote the original software. If you use this software
	 in a product, an acknowledgment in the product documentation would be
	 appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
	 misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

int x11_clippy_selection_waiting = 1;

/* We use our own cut-buffer for intermediate storage instead of
 * XA_CUT_BUFFER0 because their use isn't really defined for holding UTF-8. */
Atom x11_get_cut_buffer_type(Display *display, int mime_type, Atom selection_type)
{
	switch (mime_type) {
	case X11_CLIPBOARD_MIME_TYPE_STRING:
	case X11_CLIPBOARD_MIME_TYPE_TEXT_PLAIN:
	case X11_CLIPBOARD_MIME_TYPE_TEXT_PLAIN_UTF8:
	case X11_CLIPBOARD_MIME_TYPE_TEXT:
		return X11_XInternAtom(display, selection_type == XA_PRIMARY ? "SCHISM_CUTBUFFER_SELECTION" : "SCHISM_CUTBUFFER_CLIPBOARD", False);
	default:
		return XA_STRING;
	}
}

Atom x11_get_cut_buffer_external_fmt(Display *display, int mime_type)
{
	switch (mime_type) {
	case X11_CLIPBOARD_MIME_TYPE_STRING:
		/* If you don't support UTF-8, you might use XA_STRING here... */
		return X11_XInternAtom(display, "UTF8_STRING", False);
	case X11_CLIPBOARD_MIME_TYPE_TEXT_PLAIN:
		return X11_XInternAtom(display, "text/plain", False);
	case X11_CLIPBOARD_MIME_TYPE_TEXT_PLAIN_UTF8:
		return X11_XInternAtom(display, "text/plain;charset=utf-8", False);
	case X11_CLIPBOARD_MIME_TYPE_TEXT:
		return X11_XInternAtom(display, "TEXT", False);
	default:
		return XA_STRING;
	}
}

Atom x11_get_cut_buffer_internal_fmt(Display *display, int mime_type)
{
	switch (mime_type) {
	case X11_CLIPBOARD_MIME_TYPE_STRING:
	case X11_CLIPBOARD_MIME_TYPE_TEXT_PLAIN:
	case X11_CLIPBOARD_MIME_TYPE_TEXT_PLAIN_UTF8:
	case X11_CLIPBOARD_MIME_TYPE_TEXT:
		/* If you don't support UTF-8, you might use XA_STRING here... */
		return X11_XInternAtom(display, "UTF8_STRING", True);
	default:
		return XA_STRING;
	}
}

static int x11_set_selection_text(video_wm_data_t *wm_data, const char *text, Atom selection_type)
{
	Display *display = wm_data->data.x11.display;
	Window window = wm_data->data.x11.window;

	/* Lock the display; only required for SDL 1.2 for now */
	if (wm_data->data.x11.lock_func)
		wm_data->data.x11.lock_func();

	/* Save the selection on the root window */
	X11_XChangeProperty(display, DefaultRootWindow(display),
						x11_get_cut_buffer_type(display, X11_CLIPBOARD_MIME_TYPE_STRING, selection_type),
						x11_get_cut_buffer_internal_fmt(display, X11_CLIPBOARD_MIME_TYPE_STRING), 8, PropModeReplace,
						(const unsigned char *)text, strlen(text));
	X11_XSetSelectionOwner(display, selection_type, window, CurrentTime);

	if (wm_data->data.x11.unlock_func)
		wm_data->data.x11.unlock_func();

	return 0;
}

static char *x11_get_selection_text(video_wm_data_t *wm_data, Atom selection_type)
{
	Atom format;
	Window owner;
	Atom selection;
	Atom seln_type;
	int seln_format;
	unsigned long nbytes;
	unsigned long overflow;
	unsigned char *src;
	char *text = NULL;

	Display *display = wm_data->data.x11.display;
	Window window = wm_data->data.x11.window;

	if (wm_data->data.x11.lock_func)
		wm_data->data.x11.lock_func();

	format = x11_get_cut_buffer_internal_fmt(display, X11_CLIPBOARD_MIME_TYPE_STRING);
	owner = X11_XGetSelectionOwner(display, selection_type);
	if (owner == None) {
		/* Fall back to ancient X10 cut-buffers which do not support UTF-8 strings */
		owner = DefaultRootWindow(display);
		selection = XA_CUT_BUFFER0;
		format = XA_STRING;
	} else if (owner == window) {
		owner = DefaultRootWindow(display);
		selection = x11_get_cut_buffer_type(display, X11_CLIPBOARD_MIME_TYPE_STRING, selection_type);
	} else {
		/* Request that the selection owner copy the data to our window */
		owner = window;
		selection = X11_XInternAtom(display, "SCHISM_SELECTION", False);
		X11_XConvertSelection(display, selection_type, format, selection, owner, CurrentTime);

		/* Time out if the other window never responds... */
		schism_ticks_t start = timer_ticks();
		x11_clippy_selection_waiting = 1;
		do {
			events_pump_events();
			schism_ticks_t elapsed = timer_ticks() - start;
			/* Wait one second for a selection response. */
			if (elapsed > 1000) {
				x11_clippy_selection_waiting = 0;

				if (wm_data->data.x11.unlock_func)
					wm_data->data.x11.unlock_func();

				/* We need to set the selection text so that next time we won't
				 * timeout, otherwise we will hang on every call to this function. */
				x11_set_selection_text(wm_data, "", selection_type);

				return str_dup("");
			}
		} while (x11_clippy_selection_waiting);
	}

	if (X11_XGetWindowProperty(display, owner, selection, 0, INT_MAX / 4, False,
							   format, &seln_type, &seln_format, &nbytes, &overflow, &src) == Success) {
		if (seln_type == format) {
			text = mem_alloc(nbytes + 1);
			if (text) {
				memcpy(text, src, nbytes);
				text[nbytes] = '\0';
			}
		}
		X11_XFree(src);
	}

	if (wm_data->data.x11.unlock_func)
		wm_data->data.x11.unlock_func();

	return text ? text : str_dup("");
}

static void x11_clippy_set_clipboard(const char *text)
{
	int x_needs_close = 0;

	video_wm_data_t wm_data;
	if (!video_get_wm_data(&wm_data) || wm_data.subsystem != VIDEO_WM_DATA_SUBSYSTEM_X11)
		return;

	if (wm_data.data.x11.lock_func)
		wm_data.data.x11.lock_func();

	Atom XA_CLIPBOARD = X11_XInternAtom(wm_data.data.x11.display, "CLIPBOARD", False);

	if (wm_data.data.x11.unlock_func)
		wm_data.data.x11.unlock_func();

	if (XA_CLIPBOARD == None)
		return;

	x11_set_selection_text(&wm_data, text, XA_CLIPBOARD);
}

static void x11_clippy_set_selection(const char *text)
{
	video_wm_data_t wm_data;
	if (!video_get_wm_data(&wm_data) || wm_data.subsystem != VIDEO_WM_DATA_SUBSYSTEM_X11)
		return;

	x11_set_selection_text(&wm_data, text, XA_PRIMARY);
}

static char *x11_clippy_get_clipboard(void)
{
	video_wm_data_t wm_data;
	if (!video_get_wm_data(&wm_data) || wm_data.subsystem != VIDEO_WM_DATA_SUBSYSTEM_X11)
		return str_dup("");

	if (wm_data.data.x11.lock_func)
		wm_data.data.x11.lock_func();

	Atom XA_CLIPBOARD = X11_XInternAtom(wm_data.data.x11.display, "CLIPBOARD", False);

	if (wm_data.data.x11.unlock_func)
		wm_data.data.x11.unlock_func();

	if (XA_CLIPBOARD == None)
		return str_dup("");

	return x11_get_selection_text(&wm_data, XA_CLIPBOARD);
}

static char *x11_clippy_get_selection(void)
{
	video_wm_data_t wm_data;
	if (!video_get_wm_data(&wm_data) || wm_data.subsystem != VIDEO_WM_DATA_SUBSYSTEM_X11)
		return str_dup("");

	return x11_get_selection_text(&wm_data, XA_PRIMARY);
}

static int x11_clippy_have_clipboard(void)
{
	int result = 0;
	char *text = x11_clippy_get_clipboard();
	if (text) {
		result = text[0] != '\0';
		free(text);
	}
	return result;
}

static int x11_clippy_have_selection(void)
{
	int result = 0;
	char *text = x11_clippy_get_selection();
	if (text) {
		result = text[0] != '\0';
		free(text);
	}
	return result;
}

///////////////////////////////////////////////////////////////////

static int x11_clippy_init(void)
{
	if (!x11_init())
		return 0;

	// wew
	return 1;
}

static void x11_clippy_quit(void)
{
	x11_quit();
}

const schism_clippy_backend_t schism_clippy_backend_x11 = {
	.init = x11_clippy_init,
	.quit = x11_clippy_quit,

	.have_selection = x11_clippy_have_selection,
	.get_selection = x11_clippy_get_selection,
	.set_selection = x11_clippy_set_selection,

	.have_clipboard = x11_clippy_have_clipboard,
	.get_clipboard = x11_clippy_get_clipboard,
	.set_clipboard = x11_clippy_set_clipboard,
};

