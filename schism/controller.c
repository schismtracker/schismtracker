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

/* Game controller support functionality; only used on versions of
 * Schism that really need it (i.e. game consoles) */

#include "headers.h"
#include "sdlmain.h"
#include "mem.h"
#include "song.h"
#include "page.h"
#include "it.h"

struct controller_node {
	SDL_GameController *controller;
	SDL_JoystickID id;

	struct controller_node *next;
};

static struct controller_node *game_controller_list = NULL;

static void game_controller_insert(SDL_GameController *controller)
{
	struct controller_node *node = mem_alloc(sizeof(*node));

	node->controller = controller;
	node->id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controller));
	node->next = game_controller_list;
}

static void game_controller_remove(SDL_JoystickID id)
{
    struct controller_node* prev;
    struct controller_node* temp = game_controller_list;

    if (!temp)
        return;

    if (temp->id == id) {
        game_controller_list = temp->next;
        free(temp);
        return;
    }

    while (temp && temp->id != id) {
        prev = temp;
        temp = temp->next;
    }

    if (temp) {
        prev->next = temp->next;
        SDL_GameControllerClose(temp->controller);
        free(temp);
    }

    return;
}

static void game_controller_free(void)
{
	struct controller_node* temp;

	while (game_controller_list) {
		temp = game_controller_list;
		game_controller_list = game_controller_list->next;
		SDL_GameControllerClose(temp->controller);
		free(temp);
	}
}

int controller_init(void)
{
	if (SDL_Init(SDL_INIT_GAMECONTROLLER) < 0)
		return 0;

	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		if (SDL_IsGameController(i)) {
			SDL_GameController *controller = SDL_GameControllerOpen(i);
			if (controller)
				game_controller_insert(controller);
		}
	}

	return 1;
}

int controller_quit(void)
{
	game_controller_free();
	SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
	return 1;
}

int controller_sdlevent(SDL_Event *event)
{
	SDL_Event newev = {0};
	SDL_Keycode sym;

	switch (event->type) {
#if 0
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
		newev.key.keysym.sym = sym;
		newev.key.type = newev.type; // is this a no-op?
		*event = newev;
		return 1;
#endif

	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
		switch (event->cbutton.button) {
		case SDL_CONTROLLER_BUTTON_DPAD_UP:
			sym = SDLK_UP;
			break;
		case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
			sym = SDLK_DOWN;
			break;
		case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
			sym = SDLK_LEFT;
			break;
		case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
			sym = SDLK_RIGHT;
			break;
		case SDL_CONTROLLER_BUTTON_A:
			/* "Load Module" if the song is stopped, else stop the song */
			sym = (song_get_mode() == MODE_STOPPED) ? SDLK_F9 : SDLK_F8;
			break;
		case SDL_CONTROLLER_BUTTON_B:
			if (status.current_page == PAGE_LOAD_MODULE) {
				// if the cursor is on a song, load then play; otherwise handle as enter
				// (hmm. ctrl-enter?)
				sym = SDLK_RETURN;
			} else {
				// F5 key
				sym = SDLK_F5;
			}
			break;
		case SDL_CONTROLLER_BUTTON_BACK:
			// dialog escape, or jump back a pattern
			if (status.dialog_type) {
				sym = SDLK_ESCAPE;
				break;
			} else if (event->cbutton.state == SDL_PRESSED && song_get_mode() == MODE_PLAYING) {
				song_set_current_order(song_get_current_order() - 1);
			}
			return 0;
		case SDL_CONTROLLER_BUTTON_START:
			// dialog enter, or jump forward a pattern
			if (status.dialog_type) {
				sym = SDLK_RETURN;
				break;
			} else if (event->cbutton.state == SDL_PRESSED && song_get_mode() == MODE_PLAYING) {
				song_set_current_order(song_get_current_order() + 1);
			}
			return 0;
		case SDL_CONTROLLER_BUTTON_GUIDE:
			event->type = SDL_QUIT;
			return 1;
		case SDL_CONTROLLER_BUTTON_X: // should these
		case SDL_CONTROLLER_BUTTON_Y: // do something?
		default:
			return 0;
		}
		newev.key.keysym.sym = sym;
		if (event->cbutton.state == SDL_PRESSED) {
			newev.type = SDL_KEYDOWN;
			newev.key.state = SDL_PRESSED;
		} else {
			newev.type = SDL_KEYUP;
			newev.key.state = SDL_RELEASED;
		}
		newev.key.type = newev.type; // no-op?
		*event = newev;
		return 1;

	case SDL_CONTROLLERDEVICEADDED: {
		SDL_GameController *controller = SDL_GameControllerOpen(event->cdevice.which);
		if (controller)
			game_controller_insert(controller);

		return 0;
	}
	case SDL_CONTROLLERDEVICEREMOVED:
		game_controller_remove(event->cdevice.which);
		return 0;

	default:
		break;
	}
	return 1;
}

