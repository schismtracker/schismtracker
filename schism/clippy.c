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

#include "headers.h" /* always include this one first, kthx */
#include "backend/clippy.h"
#include "mem.h"

#include "charset.h"
#include "clippy.h"
#include "events.h"

#include "util.h"

#include "video.h"

// system backend
static const schism_clippy_backend_t *backend = NULL;

static char* _current_selection = NULL;
static char* _current_clipboard = NULL;
static struct widget* _widget_owner[16] = {NULL};

static void _free_current_selection(void) {
	if (_current_selection) {
		free(_current_selection);
		_current_selection = NULL;
	}
}

static void _free_current_clipboard(void) {
	if (_current_clipboard) {
		free(_current_clipboard);
		_current_clipboard = NULL;
	}
}

static void _clippy_copy_to_sys(int cb)
{
	if (!_current_selection)
		return;

	/* use calloc() here because we aren't guaranteed to actually
	 * fill the whole buffer */
	size_t sel_len = strlen(_current_selection);
	uint8_t* out = mem_alloc((sel_len + 1) * sizeof(char));

	/* normalize line breaks
	 *
	 * TODO: this needs to be done internally as well; every paste
	 * handler ought to expect Unix LF format. */
	size_t i = 0, j = 0;
	for (; i < sel_len && j < sel_len; i++, j++) {
		if (_current_selection[i] == '\r' && _current_selection[i + 1] == '\n') {
			/* CRLF -> LF */
			out[j] = '\n';
			i++;
		} else if (_current_selection[i] == '\r') {
			/* CR -> LF */
			out[j] = '\n';
		} else {
			/* we're good */
			out[j] = _current_selection[i];
		}
	}
	out[j] = 0;

	char *out_utf8 = NULL;
	if (charset_iconv(out, &out_utf8, CHARSET_CP437, CHARSET_UTF8, SIZE_MAX))
		return;

	free(out);

	switch (cb) {
		case CLIPPY_SELECT:
			if (backend)
				backend->set_selection(out_utf8);
			break;
		default:
		case CLIPPY_BUFFER:
			if (backend)
				backend->set_clipboard(out_utf8);
			break;
	}

	free(out_utf8);
}

static void _string_paste(SCHISM_UNUSED int cb, const char *cbptr)
{
	schism_event_t event = {0};
	event.type = SCHISM_EVENT_PASTE;
	event.clipboard.clipboard = str_dup(cbptr);
	events_push_event(&event);
}

static char *_internal_clippy_paste(int cb)
{
	switch (cb) {
		case CLIPPY_SELECT:
			if (backend && backend->have_selection()) {
				_free_current_selection();

				char* sel = backend->get_selection();

				if (charset_iconv(sel, &_current_selection, CHARSET_UTF8, CHARSET_CP437, SIZE_MAX))
					_current_selection = str_dup(sel);

				free(sel);

				return _current_selection;
			}

			return _current_selection;
		case CLIPPY_BUFFER:
			if (backend && backend->have_clipboard()) {
				_free_current_clipboard();

				char *c = backend->get_clipboard();

				if (charset_iconv(c, &_current_clipboard, CHARSET_UTF8, CHARSET_CP437, SIZE_MAX))
					_current_clipboard = str_dup(c);

				free(c);

				return _current_clipboard;
			}

			return _current_clipboard;
		default: break;
	}

	return NULL;
}

void clippy_paste(int cb)
{
	char *q = _internal_clippy_paste(cb);
	if (!q) return;
	_string_paste(cb, q);
}

void clippy_select(struct widget *w, char *addr, int len)
{
	_free_current_selection();

	if (!addr) {
		_widget_owner[CLIPPY_SELECT] = NULL;
	} else {
		_current_selection = (len < 0) ? str_dup(addr) : strn_dup(addr, len);
		_widget_owner[CLIPPY_SELECT] = w;

		/* notify SDL about our selection change */
		_clippy_copy_to_sys(CLIPPY_SELECT);
	}
}

struct widget *clippy_owner(int cb)
{
	return (cb == CLIPPY_SELECT || cb == CLIPPY_BUFFER) ? _widget_owner[cb] : NULL;
}

void clippy_yank(void)
{
	if (_current_selection && strlen(_current_selection) > 0) {
		_free_current_clipboard();
		_current_clipboard = str_dup(_current_selection);
		_widget_owner[CLIPPY_BUFFER] = _widget_owner[CLIPPY_SELECT];
		_clippy_copy_to_sys(CLIPPY_BUFFER);
		status_text_flash("Copied to selection buffer");
	}
}

int clippy_init(void)
{
	static const schism_clippy_backend_t *backends[] = {
		// ordered by preference
#ifdef SCHISM_WIN32
		&schism_clippy_backend_win32,
#endif
#ifdef SCHISM_MACOSX
		&schism_clippy_backend_macosx,
#endif
#ifdef SCHISM_SDL2
		&schism_clippy_backend_sdl2,
#endif
#ifdef SCHISM_USE_X11
		/* Our X11 clipboard overrides the SDL2 clipboard,
		 * causing copy/paste to fail. */
		&schism_clippy_backend_x11,
#endif
		NULL,
	};

	int i;

	for (i = 0; backends[i]; i++) {
		backend = backends[i];
		if (backend->init())
			break;

		backend = NULL;
	}

	if (!backend)
		return 0;

	return 1;
}

void clippy_quit(void)
{
	if (backend) {
		backend->quit();
		backend = NULL;
	}
}
