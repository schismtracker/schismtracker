/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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
#include "it.h" // need for kbd_get_alnum
#include "page.h" // need for struct key_event
#include "log.h"

const char *osname = "wii";

void wii_sdlinit(void)
{
        int n, total;

        if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
                log_appendf(4, "joystick init failed: %s", SDL_GetError());
                return;
        }

        total = SDL_NumJoysticks();
        for (n = 0; n < total; n++) {
                SDL_Joystick *js = SDL_JoystickOpen(n);
                if (js == NULL) {
                        log_appendf(4, "[%d] open fail", n);
                        continue;
                }
                log_appendf(2, "[%d] %s axes=%d balls=%d hats=%d buttons=%d",
                        n,
                        SDL_JoystickName(n),
                        SDL_JoystickNumAxes(js),
                        SDL_JoystickNumBalls(js),
                        SDL_JoystickNumHats(js),
                        SDL_JoystickNumButtons(js));
        }
}


static int lasthatsym = 0;

static SDLKey hat_to_keysym(int value)
{
        // up/down take precedence over left/right
        switch (value) {
        case SDL_HAT_LEFTUP:
        case SDL_HAT_UP:
        case SDL_HAT_RIGHTUP:
                return SDLK_UP;
        case SDL_HAT_LEFTDOWN:
        case SDL_HAT_DOWN:
        case SDL_HAT_RIGHTDOWN:
                return SDLK_DOWN;
        case SDL_HAT_LEFT:
                return SDLK_LEFT;
        case SDL_HAT_RIGHT:
                return SDLK_RIGHT;
        case SDL_HAT_CENTERED:
                return 0;
        }
}

// Huge event-rewriting hack to get at least a sort of useful interface with no keyboard.
// It's obviously impossible to provide any sort of editing functions in this manner,
// but it at least allows simple song playback.
int wii_sdlevent(SDL_Event *event)
{
        SDL_Event newev;
        SDLKey sym;
        memset(&newev, 0, sizeof(newev));

        switch (event->type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
                { // argh
                        struct key_event k = {
                                .mod = event->key.keysym.mod,
                                .sym = event->key.keysym.sym,
                        };
                        event->key.keysym.unicode = kbd_get_alnum(&k);
                }
                log_appendf(2, "key");
                return 1;

        case SDL_JOYHATMOTION:
                // TODO key repeat for these, somehow
                sym = hat_to_keysym(event->jhat.value);
                if (sym) {
                        newev.type = SDL_KEYDOWN;
                        newev.key.state = SDL_PRESSED;
                        lasthatsym = sym;
                } else {
                        newev.type = SDL_KEYUP;
                        newev.key.state = SDL_RELEASED;
                        sym = lasthatsym;
                        lasthatsym = 0;
                }
                newev.key.which = newev.jhat.which;
                newev.key.keysym.mod = 0;
                newev.key.keysym.scancode = 0;
                newev.key.keysym.unicode = 0;
                newev.key.keysym.sym = sym;
                newev.key.type = newev.type;
                *event = newev;
                return 1;

        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
                newev.key.keysym.mod = 0;
                switch (event->jbutton.button) {
                case 0: // A
                case 1: // B
                default:
                        return 0;
                case 2: // 1
                        if (song_get_mode() == MODE_STOPPED) {
                                // nothing playing? go to load screen
                                sym = SDLK_F9;
                        } else {
                                sym = SDLK_F8;
                        }
                        break;
                case 3: // 2
                        if (status.current_page == PAGE_LOAD_MODULE) {
                                // if the cursor is on a song, load then play; otherwise handle as enter
                                // (hmm. ctrl-enter?)
                                sym = SDLK_RETURN;
                        } else {
                                // F5 key
                                sym = SDLK_F5;
                        }
                        break;
                case 4: // -
                        // dialog escape, or jump back a pattern
                        if (status.dialog_type) {
                                sym = SDLK_ESCAPE;
                                break;
                        } else if (event->type == SDL_JOYBUTTONDOWN && song_get_mode() == MODE_PLAYING) {
                                song_set_current_order(song_get_current_order() - 1);
                        }
                        return 0;
                case 5: // +
                        // dialog enter, or jump forward a pattern
                        if (status.dialog_type) {
                                sym = SDLK_RETURN;
                                break;
                        } else if (event->type == SDL_JOYBUTTONDOWN && song_get_mode() == MODE_PLAYING) {
                                song_set_current_order(song_get_current_order() + 1);
                        }
                        return 0;
                case 6: // Home
                        event->type = SDL_QUIT;
                        return 1;
                }
                newev.key.which = newev.jbutton.which;
                newev.key.keysym.scancode = 0;
                newev.key.keysym.unicode = 0;
                newev.key.keysym.sym = sym;
                if (event->type == SDL_JOYBUTTONDOWN) {
                        newev.type = SDL_KEYDOWN;
                        newev.key.state = SDL_PRESSED;
                } else {
                        newev.type = SDL_KEYUP;
                        newev.key.state = SDL_RELEASED;
                }
                newev.key.type = newev.type;
                log_appendf(2, "joy %d button %d down", event->jbutton.which, event->jbutton.button);
                *event = newev;
                return 1;
        }
        return 1;
}

