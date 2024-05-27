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

#include "it.h"
#include "osdefs.h"
#include "event.h"
#include "song.h"
#include "page.h"

/* this gets stored at startup as the initial value of fnswitch before
 * we tamper with it, so we can restore it on shutdown */
static int ibook_helper = -1;

int macosx_sdlevent(SDL_Event *event)
{
	switch (event->type) {
	case SDL_WINDOWEVENT:
		switch (event->window.event) {
		case SDL_WINDOWEVENT_FOCUS_GAINED:
			macosx_ibook_fnswitch(1);
			break;
		case SDL_WINDOWEVENT_FOCUS_LOST:
			macosx_ibook_fnswitch(ibook_helper);
			break;
		default:
			break;
		}
		return 1;
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		switch (status.fix_numlock_setting) {
		case NUMLOCK_GUESS:
			/* why is this checking for ibook_helper? */
			if (ibook_helper != -1) {
				if (ACTIVE_PAGE.selected_widget > -1
				    && ACTIVE_PAGE.selected_widget < ACTIVE_PAGE.total_widgets
				    && ACTIVE_PAGE_WIDGET.accept_text) {
					/* text is more likely? */
					event->key.keysym.mod |= KMOD_NUM;
				} else {
					event->key.keysym.mod &= ~KMOD_NUM;
				}
			} /* otherwise honor it */
			break;
		default:
			/* other cases are handled in schism/main.c */
			break;
		}

		switch (event->key.keysym.scancode) {
		case SDL_SCANCODE_KP_ENTER:
			/* On portables, the regular Insert key
			 * isn't available. This is equivalent to
			 * pressing Fn-Return, which just so happens
			 * to be a "de facto" Insert in mac land.
			 * However, on external keyboards this causes
			 * a real keypad enter to get eaten by this
			 * function as well. IMO it's more important
			 * for now that portable users can actually
			 * have an Insert key.
			 *
			 *   - paper */
			event->key.keysym.sym = SDLK_INSERT;
			break;
		default:
			break;
		};
		return 1;
	default:
		break;
	}
	return 1;
}

void macosx_sysexit(void) {
	/* return back to default */
	if (ibook_helper != -1)
		macosx_ibook_fnswitch(ibook_helper);
}

void macosx_sysinit(UNUSED int *pargc, UNUSED char ***pargv) {
	/* macosx_ibook_fnswitch only sets the value if it's one of (0, 1) */
	ibook_helper = macosx_ibook_fnswitch(-1);
}
