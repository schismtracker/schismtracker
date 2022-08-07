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
#include "event.h"
#include "song.h"

const char *osname = "macosx";


int macosx_sdlevent(SDL_Event *event)
{
	if (event->type == SDL_KEYDOWN || event->type == SDL_KEYUP) {
		if (event->key.keysym.sym == 0) {
			switch (event->key.keysym.scancode) {
			case 106: // mac F16 key
				event->key.keysym.sym = SDLK_PRINTSCREEN;
				event->key.keysym.mod = KMOD_CTRL;
				return 1;
			case 234: // XXX what key is this?
				if (event->type == SDL_KEYDOWN)
					song_set_current_order(song_get_current_order() - 1);
				return 0;
			case 233: // XXX what key is this?
				if (event->type == SDL_KEYUP)
					song_set_current_order(song_get_current_order() + 1);
				return 0;
			};
		}
	}
	return 1;
}

