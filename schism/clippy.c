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

#include "clippy.h"
#include "event.h"

#include "util.h"

#include "sdlmain.h"
#include "video.h"

static char *_current_selection = NULL;
static char *_current_clipboard = NULL;
static struct widget *_widget_owner[16] = {NULL};

static void _clippy_copy_to_sys(int cb)
{
	char* freeme = NULL;

	if (_current_selection) {
		char* dst = NULL;
		int i = 0;
#ifdef WIN32
		/* need twice the space since newlines are replaced with \r\n */
		freeme = dst = malloc(strlen(_current_selection) * 2) + 1;
#else
		/* XXX what's 4?? */
		freeme = dst = malloc(strlen(_current_selection) + 4);
#endif
		if (!freeme) return;
		for (i = 0; _current_selection[i]; i++) {
#ifdef WIN32
			if (_current_selection[i] == '\r' || _current_selection[i] == '\n') {
				*(dst++) = '\r';
				*(dst++) = '\n';
			} else {
				*(dst++) = _current_selection[i];
			}
#else
			*(dst++) = (_current_selection[i] == '\r') ? '\n' : _current_selection[i];
#endif
		}
		(*dst++) = '\0';
	}

	switch (cb) {
		case CLIPPY_SELECT:
#if SDL_VERSION_ATLEAST(2, 26, 0)
			SDL_SetPrimarySelectionText(freeme);
#endif
			break;
		default:
		case CLIPPY_BUFFER:
			SDL_SetClipboardText(freeme);
			break;
	}

	if (freeme)
		free(freeme);
}

static void _string_paste(UNUSED int cb, const char *cbptr)
{
	SDL_Event event = {};

	event.user.type = SCHISM_EVENT_PASTE;
	event.user.data1 = str_dup(cbptr); /* current_clipboard... is it safe? */
	if (!event.user.data1) return; /* eh... */
	if (SDL_PushEvent(&event) == -1)
		free(event.user.data1);
}

static char *_internal_clippy_paste(int cb)
{
	switch (cb) {
		case CLIPPY_SELECT:
#if SDL_VERSION_ATLEAST(2, 26, 0)
			/* is this even remotely useful? */
			if (SDL_HasPrimarySelectionText()) {
				if (_current_selection)
					free(_current_selection);

				/* See below for why we do this. */
				char* sel = SDL_GetPrimarySelectionText();
				_current_selection = str_dup(sel);
				SDL_free(sel);
				return _current_selection;
			}
#endif

			return _current_selection;
		case CLIPPY_BUFFER:
			if (SDL_HasClipboardText()) {
				if (_current_clipboard)
					free(_current_clipboard);

				/* SDL docs explicitly says we have to call SDL_free,
			 	 * while our own code uses regular malloc. Just copy
				 * the buffer... */
				char* cb = SDL_GetClipboardText();
				_current_clipboard = str_dup(cb);
				SDL_free(cb);
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
	int i;

	if (_current_selection != _current_clipboard)
		free(_current_selection);

	if (!addr) {
		_current_selection = NULL;
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
	if (_current_selection != _current_clipboard)
		free(_current_clipboard);

	_current_clipboard = _current_selection;
	_widget_owner[CLIPPY_BUFFER] = _widget_owner[CLIPPY_SELECT];

	if (_current_selection && strlen(_current_selection) > 0) {
		status_text_flash("Copied to selection buffer");
		_clippy_copy_to_sys(CLIPPY_BUFFER);
	}
}
