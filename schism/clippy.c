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

static void _clippy_copy_to_sys(int do_sel)
{
	int j = 0;
	char *tmp = NULL;
	char *dst = NULL;
	char *freeme = NULL;

	if (!_current_selection) {
		dst = NULL;
		j = 0;
	} else
#if defined(WIN32)
	{
		int i;
		/* need twice the space since newlines are replaced with \r\n */
		freeme = tmp = malloc(strlen(_current_selection)*2 + 1);
		if (!tmp) return;
		for (i = j = 0; _current_selection[i]; i++) {
			if (_current_selection[i] == '\r' || _current_selection[i] == '\n') {
				tmp[j++] = '\r';
				tmp[j++] = '\n';
			} else {
				tmp[j++] = _current_selection[i];
			}
		}
		tmp[j] = '\0';
	}
#else
	{
		int i;
		/* convert to local */
		freeme = dst = malloc(strlen(_current_selection)+4);
		if (!dst) return;
		for (i = j = 0; _current_selection[i]; i++) {
			dst[j] = _current_selection[i];
			if (dst[j] == '\r') dst[j] = '\n';
			j++;
		}
		dst[j] = '\0';
	}
#endif

	if (!do_sel)
		SDL_SetClipboardText(_current_clipboard);

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
			return _current_selection;
		case CLIPPY_BUFFER:
			if (SDL_HasClipboardText())
				return _current_clipboard = SDL_GetClipboardText();

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

	if (_current_selection != _current_clipboard) {
		free(_current_selection);
	}
	if (!addr) {
		_current_selection = NULL;
		_widget_owner[CLIPPY_SELECT] = NULL;
	} else {
		for (i = 0; addr[i] && (len < 0 || i < len); i++) {
			/* nothing */
		}
		_current_selection = strn_dup(addr, i);
		_widget_owner[CLIPPY_SELECT] = w;

		/* update x11 Select (for xterms and stuff) */
		_clippy_copy_to_sys(1);
	}
}
struct widget *clippy_owner(int cb)
{
	if (cb == CLIPPY_SELECT || cb == CLIPPY_BUFFER)
		return _widget_owner[cb];
	return NULL;
}

void clippy_yank(void)
{
	if (_current_selection != _current_clipboard)
		free(_current_clipboard);

	_current_clipboard = _current_selection;
	_widget_owner[CLIPPY_BUFFER] = _widget_owner[CLIPPY_SELECT];

	if (_current_selection && strlen(_current_selection) > 0) {
		status_text_flash("Copied to selection buffer");
		_clippy_copy_to_sys(0);
	}
}
