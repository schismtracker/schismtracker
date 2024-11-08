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
#include "events.h"
#include "backend/events.h"
#include "util.h"

#include <SDL.h>
#ifdef SCHISM_WIN32
#include <SDL_syswm.h>
#endif

#ifdef SCHISM_CONTROLLER

#include "it.h"
#include "song.h"
#include "page.h"

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
	game_controller_list = node;
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

int sdl2_controller_init(void)
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

int sdl2_controller_quit(void)
{
	game_controller_free();
	SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
	return 1;
}

static int sdl2_controller_sdlevent(SDL_Event *event)
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

#endif

static schism_keymod_t sdl2_modkey_trans(uint16_t mod)
{
	schism_keymod_t res = 0;

	if (mod & KMOD_LSHIFT)
		res |= SCHISM_KEYMOD_LSHIFT;

	if (mod & KMOD_RSHIFT)
		res |= SCHISM_KEYMOD_RSHIFT;

	if (mod & KMOD_LCTRL)
		res |= SCHISM_KEYMOD_LCTRL;

	if (mod & KMOD_RCTRL)
		res |= SCHISM_KEYMOD_RCTRL;

	if (mod & KMOD_LALT)
		res |= SCHISM_KEYMOD_LALT;

	if (mod & KMOD_RALT)
		res |= SCHISM_KEYMOD_RALT;

	if (mod & KMOD_LGUI)
		res |= SCHISM_KEYMOD_LGUI;

	if (mod & KMOD_RGUI)
		res |= SCHISM_KEYMOD_RGUI;

	if (mod & KMOD_NUM)
		res |= SCHISM_KEYMOD_NUM;

	if (mod & KMOD_CAPS)
		res |= SCHISM_KEYMOD_CAPS;

	if (mod & KMOD_MODE)
		res |= SCHISM_KEYMOD_MODE;

	return res;
}

schism_keymod_t sdl2_event_mod_state(void)
{
	return sdl2_modkey_trans(SDL_GetModState());
}

/* These are here for linking text input to keyboard inputs.
 * If no keyboard input can be found, then the text will
 * be sent as a SCHISM_TEXTINPUT event.
 *
 * - paper */

static schism_event_t pending_keydown;
static int have_pending_keydown = 0;

static void push_pending_keydown(schism_event_t *event)
{
	if (!have_pending_keydown) {
		pending_keydown = *event;
		have_pending_keydown = 1;
	}
}

static void pop_pending_keydown(const char *text)
{
	/* text should always be in UTF-8 here */
	if (have_pending_keydown) {
		if (text) {
			strncpy(pending_keydown.key.text, text, ARRAY_SIZE(pending_keydown.text.text));
			pending_keydown.key.text[ARRAY_SIZE(pending_keydown.text.text)-1] = '\0';
		}
		schism_push_event(&pending_keydown);
		have_pending_keydown = 0;
	}
}

void sdl2_pump_events(void)
{
	SDL_Event e;

	while (SDL_PollEvent(&e)) {
		schism_event_t schism_event = {0};

		// fill this in
		schism_event.common.timestamp = e.common.timestamp;

#ifdef SCHISM_CONTROLLER
		if (!sdl2_controller_sdlevent(&e))
			continue;
#endif

		switch (e.type) {
		case SDL_QUIT:
			schism_event.type = SCHISM_QUIT;
			schism_push_event(&schism_event);
			break;
		case SDL_WINDOWEVENT:
			switch (e.window.event) {
			case SDL_WINDOWEVENT_SHOWN:
				schism_event.type = SCHISM_WINDOWEVENT_SHOWN;
				schism_push_event(&schism_event);
				break;
			case SDL_WINDOWEVENT_EXPOSED:
				schism_event.type = SCHISM_WINDOWEVENT_EXPOSED;
				schism_push_event(&schism_event);
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				schism_event.type = SCHISM_WINDOWEVENT_FOCUS_LOST;
				schism_push_event(&schism_event);
				break;
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				schism_event.type = SCHISM_WINDOWEVENT_FOCUS_GAINED;
				schism_push_event(&schism_event);
				break;
			case SDL_WINDOWEVENT_RESIZED:
				schism_event.type = SCHISM_WINDOWEVENT_RESIZED;
				schism_event.window.data.resized.width = e.window.data1;
				schism_event.window.data.resized.height = e.window.data2;
				schism_push_event(&schism_event);
				break;
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				schism_event.type = SCHISM_WINDOWEVENT_SIZE_CHANGED;
				schism_event.window.data.resized.width = e.window.data1;
				schism_event.window.data.resized.height = e.window.data2;
				schism_push_event(&schism_event);
				break;
			}
			break;
		case SDL_KEYDOWN:
			// pop any pending keydowns
			pop_pending_keydown(NULL);

			schism_event.type = SCHISM_KEYDOWN;
			schism_event.key.state = KEY_PRESS;
			schism_event.key.repeat = e.key.repeat;

			// Schism's and SDL2's representation of these are the same.
			schism_event.key.sym = e.key.keysym.sym;
			schism_event.key.scancode = e.key.keysym.scancode;
			schism_event.key.mod = sdl2_modkey_trans(e.key.keysym.mod); // except this one!

			if (!SDL_IsTextInputActive()) {
				// push it NOW
				schism_push_event(&schism_event);
			} else {
				push_pending_keydown(&schism_event);
			}

			break;
		case SDL_KEYUP:
			// pop any pending keydowns
			pop_pending_keydown(NULL);

			schism_event.type = SCHISM_KEYUP;
			schism_event.key.state = KEY_RELEASE;
			schism_event.key.sym = e.key.keysym.sym;
			schism_event.key.scancode = e.key.keysym.scancode;
			schism_event.key.mod = sdl2_modkey_trans(e.key.keysym.mod);

			schism_push_event(&schism_event);

			break;
		case SDL_TEXTINPUT:
			if (have_pending_keydown) {
				pop_pending_keydown(e.text.text);
			} else {
				schism_event.type = SCHISM_TEXTINPUT;

				strncpy(schism_event.text.text, e.text.text, ARRAY_SIZE(schism_event.text.text));
				schism_event.text.text[ARRAY_SIZE(schism_event.text.text)-1] = '\0';

				schism_push_event(&schism_event);
			}
			break;

		case SDL_MOUSEMOTION:
			schism_event.type = SCHISM_MOUSEMOTION;
			schism_event.motion.x = e.motion.x;
			schism_event.motion.y = e.motion.y;
			schism_push_event(&schism_event);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			schism_event.type = (e.type == SDL_MOUSEBUTTONDOWN) ? SCHISM_MOUSEBUTTONDOWN : SCHISM_MOUSEBUTTONUP;

			switch (e.button.button) {
			case SDL_BUTTON_LEFT:
				schism_event.button.button = MOUSE_BUTTON_LEFT;
				break;
			case SDL_BUTTON_MIDDLE:
				schism_event.button.button = MOUSE_BUTTON_MIDDLE;
				break;
			case SDL_BUTTON_RIGHT:
				schism_event.button.button = MOUSE_BUTTON_RIGHT;
				break;
			}

			schism_event.button.state = !!e.button.state;
			schism_event.button.clicks = e.button.clicks;

			schism_event.button.x = e.button.x;
			schism_event.button.y = e.button.y;
			schism_push_event(&schism_event);
			break;
		case SDL_MOUSEWHEEL:
			schism_event.type = SCHISM_MOUSEWHEEL;

			schism_event.wheel.x = e.wheel.x;
			schism_event.wheel.y = e.wheel.y;

#if SDL_VERSION_ATLEAST(2, 26, 0)
			schism_event.wheel.mouse_x = e.wheel.mouseX;
			schism_event.wheel.mouse_y = e.wheel.mouseY;
#else
			video_get_mouse_coordinates(&schism_event.wheel.mouse_x, &schism_event.wheel.mouse_y);
#endif
			schism_push_event(&schism_event);
			break;
		case SDL_DROPFILE:
			schism_event.type = SCHISM_DROPFILE;

			schism_event.drop.file = str_dup(e.drop.file);

			SDL_free(e.drop.file);

			schism_push_event(&schism_event);
			break;
		/* these two have no structures because we don't use them */
		case SDL_AUDIODEVICEADDED:
			schism_event.type = SCHISM_AUDIODEVICEADDED;
			schism_push_event(&schism_event);
			break;
		case SDL_AUDIODEVICEREMOVED:
			schism_event.type = SCHISM_AUDIODEVICEREMOVED;
			schism_push_event(&schism_event);
			break;
		case SDL_SYSWMEVENT:
			schism_event.type = SCHISM_EVENT_WM_MSG;
#ifdef SCHISM_WIN32
			schism_event.wm_msg.msg.win.hwnd = e.syswm.msg->msg.win.hwnd;
			schism_event.wm_msg.msg.win.msg = e.syswm.msg->msg.win.msg;
			schism_event.wm_msg.msg.win.wparam = e.syswm.msg->msg.win.wParam;
			schism_event.wm_msg.msg.win.lparam = e.syswm.msg->msg.win.lParam;
#endif
			schism_push_event(&schism_event);
			break;
		default:
			break;
		}
	}

	pop_pending_keydown(NULL);
}
