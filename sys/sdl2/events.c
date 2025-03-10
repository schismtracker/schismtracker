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

#include "init.h"

#include "headers.h"
#include "events.h"
#include "backend/events.h"
#include "util.h"
#include "mem.h"
#include "video.h"

#include <SDL_syswm.h>

/* need to redefine these on SDL < 2.0.4 */
#if !SDL_VERSION_ATLEAST(2, 0, 4)
#define SDL_AUDIODEVICEADDED (0x1100)
#define SDL_AUDIODEVICEREMOVED (0x1101)
#endif

static int (SDLCALL *sdl2_InitSubSystem)(Uint32 flags) = NULL;
static void (SDLCALL *sdl2_QuitSubSystem)(Uint32 flags) = NULL;

static SDL_Keymod (SDLCALL *sdl2_GetModState)(void);
static int (SDLCALL *sdl2_PollEvent)(SDL_Event *event) = NULL;
static SDL_bool (SDLCALL *sdl2_IsTextInputActive)(void) = NULL;

static void (SDLCALL *sdl2_free)(void *) = NULL;

static void (SDLCALL *sdl2_GetVersion)(SDL_version * ver) = NULL;

static Uint8 (SDLCALL *sdl2_EventState)(Uint32 type, int state) = NULL;

// whether SDL's wheel event gives mouse coordinates or not
static int wheel_have_mouse_coordinates = 0;

#ifdef SCHISM_CONTROLLER

// Okay, this is a bit stupid; unlike the regular events these
// are actually handled and mapped in this file rather than in the
// main logic. This really ought to not be the case and the events
// should just be sent to the main file as-is so it can handle it.

static SDL_bool (SDLCALL *sdl2_IsGameController)(int joystick_index) = NULL;
static SDL_GameController* (SDLCALL *sdl2_GameControllerOpen)(int joystick_index) = NULL;
static void (SDLCALL *sdl2_GameControllerClose)(SDL_GameController *gamecontroller) = NULL;
static SDL_Joystick* (SDLCALL *sdl2_GameControllerGetJoystick)(SDL_GameController *gamecontroller) = NULL;
static SDL_JoystickID (SDLCALL *sdl2_JoystickInstanceID)(SDL_Joystick *joystick) = NULL;
static int (SDLCALL *sdl2_NumJoysticks)(void);

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

	// FIXME: A and B are in different places on the Wii
	// and Wii U ports.
	node->controller = controller;
	node->id = sdl2_JoystickInstanceID(sdl2_GameControllerGetJoystick(controller));
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
        sdl2_GameControllerClose(temp->controller);
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
		sdl2_GameControllerClose(temp->controller);
		free(temp);
	}
}

static int sdl2_controller_load_syms(void)
{
	SCHISM_SDL2_SYM(InitSubSystem);
	SCHISM_SDL2_SYM(QuitSubSystem);

	SCHISM_SDL2_SYM(IsGameController);
	SCHISM_SDL2_SYM(GameControllerOpen);
	SCHISM_SDL2_SYM(GameControllerClose);
	SCHISM_SDL2_SYM(GameControllerGetJoystick);
	SCHISM_SDL2_SYM(JoystickInstanceID);
	SCHISM_SDL2_SYM(NumJoysticks);

	return 0;
}

static int sdl2_controller_init(void)
{
	if (!sdl2_init())
		return 0;

	if (sdl2_controller_load_syms())
		return 0;

	if (sdl2_InitSubSystem(SDL_INIT_GAMECONTROLLER) < 0)
		return 0;

	for (int i = 0; i < sdl2_NumJoysticks(); i++) {
		if (sdl2_IsGameController(i)) {
			SDL_GameController *controller = sdl2_GameControllerOpen(i);
			if (controller)
				game_controller_insert(controller);
		}
	}

	return 1;
}

static int sdl2_controller_quit(void)
{
	game_controller_free();
	sdl2_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
	sdl2_quit();
	return 1;
}

// the minimum value for schism to handle left axis events
#define CONTROLLER_LEFT_AXIS_SENSITIVITY (INT16_MAX / 2)

static int sdl2_controller_sdlevent(SDL_Event *event)
{
	SDL_Event newev = {0};
	SDL_Keycode sym = SDLK_UNKNOWN;

	switch (event->type) {
	case SDL_CONTROLLERAXISMOTION:
		if (event->caxis.axis == SDL_CONTROLLER_AXIS_LEFTX
			|| event->caxis.axis == SDL_CONTROLLER_AXIS_LEFTY) {
			// Left axis simply acts as a D-pad

			static SDL_Keycode lastaxissym = SDLK_UNKNOWN;

			switch (event->caxis.axis) {
			case SDL_CONTROLLER_AXIS_LEFTX:
				if (event->caxis.value > CONTROLLER_LEFT_AXIS_SENSITIVITY) {
					sym = SDLK_RIGHT;
				} else if (event->caxis.value < -CONTROLLER_LEFT_AXIS_SENSITIVITY) {
					sym = SDLK_LEFT;
				}
				break;
			case SDL_CONTROLLER_AXIS_LEFTY:
				if (event->caxis.value > CONTROLLER_LEFT_AXIS_SENSITIVITY) {
					sym = SDLK_DOWN;
				} else if (event->caxis.value < -CONTROLLER_LEFT_AXIS_SENSITIVITY) {
					sym = SDLK_UP;
				}
				break;
			}

			if (sym == lastaxissym)
				return 0;

			if (sym != SDLK_UNKNOWN) {
				newev.type = SDL_KEYDOWN;
				newev.key.state = SDL_PRESSED;
				lastaxissym = sym;
			} else {
				newev.type = SDL_KEYUP;
				newev.key.state = SDL_RELEASED;
				sym = lastaxissym;
				lastaxissym = SDLK_UNKNOWN;
			}

			newev.key.keysym.sym = sym;
			*event = newev;
			return 1;
		} else if (event->caxis.axis == SDL_CONTROLLER_AXIS_RIGHTX
			|| event->caxis.axis == SDL_CONTROLLER_AXIS_RIGHTY) {
			// TODO control the mouse here; we'd need access to main()
			// to do that, so i'm putting it off until this crap
			// gets moved into events.c/events.h
		}
		return 0;
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
		SDL_GameController *controller = sdl2_GameControllerOpen(event->cdevice.which);
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
	return sdl2_modkey_trans(sdl2_GetModState());
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
		events_push_event(&pending_keydown);
		have_pending_keydown = 0;
	}
}

void sdl2_pump_events(void)
{
	SDL_Event e;

	while (sdl2_PollEvent(&e)) {
		schism_event_t schism_event = {0};

#ifdef SCHISM_CONTROLLER
		if (!sdl2_controller_sdlevent(&e))
			continue;
#endif

		switch (e.type) {
		case SDL_QUIT:
			schism_event.type = SCHISM_QUIT;
			events_push_event(&schism_event);
			break;
		case SDL_WINDOWEVENT:
			switch (e.window.event) {
			case SDL_WINDOWEVENT_SHOWN:
				schism_event.type = SCHISM_WINDOWEVENT_SHOWN;
				events_push_event(&schism_event);
				break;
			case SDL_WINDOWEVENT_EXPOSED:
				schism_event.type = SCHISM_WINDOWEVENT_EXPOSED;
				events_push_event(&schism_event);
				break;
			case SDL_WINDOWEVENT_FOCUS_LOST:
				schism_event.type = SCHISM_WINDOWEVENT_FOCUS_LOST;
				events_push_event(&schism_event);
				break;
			case SDL_WINDOWEVENT_FOCUS_GAINED:
				schism_event.type = SCHISM_WINDOWEVENT_FOCUS_GAINED;
				events_push_event(&schism_event);
				break;
			case SDL_WINDOWEVENT_RESIZED:
				schism_event.type = SCHISM_WINDOWEVENT_RESIZED;
				schism_event.window.data.resized.width = e.window.data1;
				schism_event.window.data.resized.height = e.window.data2;
				events_push_event(&schism_event);
				break;
			case SDL_WINDOWEVENT_SIZE_CHANGED:
				schism_event.type = SCHISM_WINDOWEVENT_SIZE_CHANGED;
				schism_event.window.data.resized.width = e.window.data1;
				schism_event.window.data.resized.height = e.window.data2;
				events_push_event(&schism_event);
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

			if (!sdl2_IsTextInputActive()) {
				// push it NOW
				events_push_event(&schism_event);
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

			events_push_event(&schism_event);

			break;
		case SDL_TEXTINPUT:
			if (have_pending_keydown) {
				pop_pending_keydown(e.text.text);
			} else {
				schism_event.type = SCHISM_TEXTINPUT;

				strncpy(schism_event.text.text, e.text.text, ARRAY_SIZE(schism_event.text.text));
				schism_event.text.text[ARRAY_SIZE(schism_event.text.text)-1] = '\0';

				events_push_event(&schism_event);
			}
			break;

		case SDL_MOUSEMOTION:
			schism_event.type = SCHISM_MOUSEMOTION;
			schism_event.motion.x = e.motion.x;
			schism_event.motion.y = e.motion.y;
			events_push_event(&schism_event);
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
			events_push_event(&schism_event);
			break;
		case SDL_MOUSEWHEEL:
			schism_event.type = SCHISM_MOUSEWHEEL;

			schism_event.wheel.x = e.wheel.x;
			schism_event.wheel.y = e.wheel.y;

#if SDL_VERSION_ATLEAST(2, 26, 0) // TODO: this #if might not be necessary
			if (wheel_have_mouse_coordinates) {
				schism_event.wheel.mouse_x = e.wheel.mouseX;
				schism_event.wheel.mouse_y = e.wheel.mouseY;
			} else
#endif
			{
				unsigned int x, y;
				video_get_mouse_coordinates(&x, &y);
				schism_event.wheel.mouse_x = x;
				schism_event.wheel.mouse_y = y;
			}

			events_push_event(&schism_event);
			break;
		case SDL_DROPFILE:
			schism_event.type = SCHISM_DROPFILE;

			schism_event.drop.file = str_dup(e.drop.file);

			sdl2_free(e.drop.file);

			events_push_event(&schism_event);
			break;
		/* these two have no structures because we don't use them */
		case SDL_AUDIODEVICEADDED:
			schism_event.type = SCHISM_AUDIODEVICEADDED;
			events_push_event(&schism_event);
			break;
		case SDL_AUDIODEVICEREMOVED:
			schism_event.type = SCHISM_AUDIODEVICEREMOVED;
			events_push_event(&schism_event);
			break;
		case SDL_SYSWMEVENT:
			schism_event.type = SCHISM_EVENT_WM_MSG;
			schism_event.wm_msg.backend = SCHISM_WM_MSG_BACKEND_SDL2;
			switch (e.syswm.msg->subsystem) {
#if defined(SDL_VIDEO_DRIVER_WINDOWS)
			case SDL_SYSWM_WINDOWS:
				schism_event.wm_msg.subsystem = SCHISM_WM_MSG_SUBSYSTEM_WINDOWS;
				schism_event.wm_msg.msg.win.hwnd = e.syswm.msg->msg.win.hwnd;
				schism_event.wm_msg.msg.win.msg = e.syswm.msg->msg.win.msg;
				schism_event.wm_msg.msg.win.wparam = e.syswm.msg->msg.win.wParam;
				schism_event.wm_msg.msg.win.lparam = e.syswm.msg->msg.win.lParam;

				// ignore WM_DROPFILES messages. these are already handled
				// by the SDL_DROPFILES event and trying to use our implementation
				// only results in an empty string which is undesirable
				if (schism_event.wm_msg.msg.win.msg != WM_DROPFILES)
					events_push_event(&schism_event);
				break;
#endif
#if defined(SDL_VIDEO_DRIVER_X11)
			case SDL_SYSWM_X11:
				schism_event.wm_msg.subsystem = SCHISM_WM_MSG_SUBSYSTEM_X11;
				schism_event.wm_msg.msg.x11.event.type = e.syswm.msg->msg.x11.event.type;
				if (e.syswm.msg->msg.x11.event.type == SelectionRequest) {
					schism_event.wm_msg.msg.x11.event.selection_request.serial = e.syswm.msg->msg.x11.event.xselectionrequest.serial;
					schism_event.wm_msg.msg.x11.event.selection_request.send_event = e.syswm.msg->msg.x11.event.xselectionrequest.send_event;     // `Bool' in Xlib
					schism_event.wm_msg.msg.x11.event.selection_request.display = e.syswm.msg->msg.x11.event.xselectionrequest.display;      // `Display *' in Xlib
					schism_event.wm_msg.msg.x11.event.selection_request.owner = e.syswm.msg->msg.x11.event.xselectionrequest.owner;     // `Window' in Xlib
					schism_event.wm_msg.msg.x11.event.selection_request.requestor = e.syswm.msg->msg.x11.event.xselectionrequest.requestor; // `Window' in Xlib
					schism_event.wm_msg.msg.x11.event.selection_request.selection = e.syswm.msg->msg.x11.event.xselectionrequest.selection; // `Atom' in Xlib
					schism_event.wm_msg.msg.x11.event.selection_request.target = e.syswm.msg->msg.x11.event.xselectionrequest.target;    // `Atom' in Xlib
					schism_event.wm_msg.msg.x11.event.selection_request.property = e.syswm.msg->msg.x11.event.xselectionrequest.property;  // `Atom' in Xlib
					schism_event.wm_msg.msg.x11.event.selection_request.time = e.syswm.msg->msg.x11.event.xselectionrequest.time;      // `Time' in Xlib
				}
				// ...?
				//events_push_event(&schism_event);
				break;
#endif
			default:
				break;
			}
		}
	}

	pop_pending_keydown(NULL);
}

//////////////////////////////////////////////////////////////////////////////
// dynamic loading

static int sdl2_events_load_syms(void)
{
	SCHISM_SDL2_SYM(InitSubSystem);
	SCHISM_SDL2_SYM(QuitSubSystem);

	SCHISM_SDL2_SYM(GetModState);
	SCHISM_SDL2_SYM(IsTextInputActive);
	SCHISM_SDL2_SYM(PollEvent);
	SCHISM_SDL2_SYM(EventState);

	SCHISM_SDL2_SYM(free);

	SCHISM_SDL2_SYM(GetVersion);

	return 0;
}

static int sdl2_events_init(void)
{
	if (!sdl2_init())
		return 0;

	if (sdl2_events_load_syms())
		return 0;

	if (sdl2_InitSubSystem(SDL_INIT_EVENTS) < 0)
		return 0;

#ifdef SCHISM_CONTROLLER
	{
		int r = sdl2_controller_init();
# if defined(SCHISM_WII) || defined(SCHISM_WIIU)
		// only warn the user if controller initialization failed
		// when on an actual console.
		if (!r)
			log_appendf(4, "SDL: Failed to initialize game controllers!");
# endif
	}
#endif


	SDL_version ver;
	sdl2_GetVersion(&ver);

	wheel_have_mouse_coordinates = SDL2_VERSION_ATLEAST(ver, 2, 26, 0);

#if defined(SCHISM_WIN32) || defined(SCHISM_USE_X11)
	sdl2_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
#endif

	return 1;
}

static void sdl2_events_quit(void)
{
#ifdef SCHISM_CONTROLLER
	sdl2_controller_quit();
#endif
	sdl2_QuitSubSystem(SDL_INIT_EVENTS);
	sdl2_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_events_backend_t schism_events_backend_sdl2 = {
	.init = sdl2_events_init,
	.quit = sdl2_events_quit,

#if !defined(SCHISM_WII) && !defined(SCHISM_WIIU)
	/* These ports have no key repeat... */
	.flags = SCHISM_EVENTS_BACKEND_HAS_KEY_REPEAT,
#else
	.flags = 0,
#endif

	.keymod_state = sdl2_event_mod_state,
	.pump_events = sdl2_pump_events,
};
