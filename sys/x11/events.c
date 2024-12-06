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
#include "osdefs.h"
#include "events.h"
#include "video.h"

#include "init.h"

/* The clipboard code in this file was taken from SDL 2,
 * whose license is included in sys/x11/clippy.c */

// in sys/x11/clippy.c, set to 0 when SelectionNotify event received.
extern int x11_clippy_selection_waiting;

int x11_event(schism_event_t *event)
{
	int i;

	if (event->type != SCHISM_EVENT_WM_MSG || event->wm_msg.subsystem != SCHISM_WM_MSG_SUBSYSTEM_X11)
		return 1;

	video_wm_data_t wm_data;
	if (!video_get_wm_data(&wm_data) || wm_data.subsystem != VIDEO_WM_DATA_SUBSYSTEM_X11)
		return 1; // ???

	if (wm_data.data.x11.lock_func)
		wm_data.data.x11.lock_func();

	switch (event->wm_msg.msg.x11.event.type) {
	case SelectionNotify:
		// sent when a selection is received from XConvertSelection
		x11_clippy_selection_waiting = 0;
		break;
	case SelectionRequest: {
		int seln_format, mime_formats;
		unsigned long nbytes;
		unsigned long overflow;
		unsigned char *seln_data;
		Atom supportedFormats[X11_CLIPBOARD_MIME_TYPE_MAX_ + 1];
		Atom XA_TARGETS = X11_XInternAtom(wm_data.data.x11.display, "TARGETS", False);

		XEvent sevent = {
			.xselection = {
				.type = SelectionNotify,
				.selection = event->wm_msg.msg.x11.event.selection_request.selection,
				.target = None,
				.property = None,
				.requestor = event->wm_msg.msg.x11.event.selection_request.requestor,
				.time = event->wm_msg.msg.x11.event.selection_request.time,
			},
		};

		if (event->wm_msg.msg.x11.event.selection_request.target == XA_TARGETS) {
			supportedFormats[0] = XA_TARGETS;
			mime_formats = 1;
			for (i = 0; i < X11_CLIPBOARD_MIME_TYPE_MAX_; i++)
				supportedFormats[mime_formats++] = x11_get_cut_buffer_external_fmt(wm_data.data.x11.display, i);
			X11_XChangeProperty(wm_data.data.x11.display, event->wm_msg.msg.x11.event.selection_request.requestor,
								event->wm_msg.msg.x11.event.selection_request.property,
								XA_ATOM, 32, PropModeReplace,
								(unsigned char *)supportedFormats,
								mime_formats);
			sevent.xselection.property = event->wm_msg.msg.x11.event.selection_request.property;
			sevent.xselection.target = XA_TARGETS;
		} else {
			for (i = 0; i < X11_CLIPBOARD_MIME_TYPE_MAX_; ++i) {
				if (x11_get_cut_buffer_external_fmt(wm_data.data.x11.display, i) != event->wm_msg.msg.x11.event.selection_request.target)
					continue;

				if (X11_XGetWindowProperty(wm_data.data.x11.display, DefaultRootWindow(wm_data.data.x11.display),
										   x11_get_cut_buffer_type(wm_data.data.x11.display, i, event->wm_msg.msg.x11.event.selection_request.selection),
										   0, INT_MAX / 4, False,
										   x11_get_cut_buffer_internal_fmt(wm_data.data.x11.display, i),
										   &sevent.xselection.target, &seln_format, &nbytes,
										   &overflow, &seln_data) == Success) {
					if (seln_format != None) {
						X11_XChangeProperty(wm_data.data.x11.display, event->wm_msg.msg.x11.event.selection_request.requestor,
											event->wm_msg.msg.x11.event.selection_request.property,
											event->wm_msg.msg.x11.event.selection_request.target, 8, PropModeReplace,
											seln_data, nbytes);
						sevent.xselection.property = event->wm_msg.msg.x11.event.selection_request.property;
						sevent.xselection.target = event->wm_msg.msg.x11.event.selection_request.target;
						X11_XFree(seln_data);
						break;
					} else {
						X11_XFree(seln_data);
					}
				}
			}
		}
		X11_XSendEvent(wm_data.data.x11.display, event->wm_msg.msg.x11.event.selection_request.requestor, False, 0, &sevent);
		X11_XSync(wm_data.data.x11.display, False);
		break;
	}
	}

	if (wm_data.data.x11.unlock_func)
		wm_data.data.x11.unlock_func();

	return 0;
}