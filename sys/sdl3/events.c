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
#include "mem.h"
#include "video.h"

#include "init.h"

static bool (SDLCALL *sdl3_InitSubSystem)(SDL_InitFlags flags) = NULL;
static void (SDLCALL *sdl3_QuitSubSystem)(SDL_InitFlags flags) = NULL;

static SDL_Keymod (SDLCALL *sdl3_GetModState)(void) = NULL;
static bool (SDLCALL *sdl3_PollEvent)(SDL_Event *event) = NULL;

static void (SDLCALL *sdl3_free)(void *) = NULL;

#ifdef SCHISM_WIN32
static void (SDLCALL *sdl3_SetWindowsMessageHook)(SDL_WindowsMessageHook callback, void *userdata);
#endif
#ifdef SCHISM_USE_X11
static void (SDLCALL *sdl3_SetX11EventHook)(SDL_X11EventHook callback, void *userdata);
#endif

#ifndef SDLK_EXTENDED_MASK // Debian doesn't have this for some reason
#define SDLK_EXTENDED_MASK (1u << 29)
#endif

// TODO: Re-add controller support, preferably in a global controller.c
// that only receives raw events to translate them into keyboard/mouse events

static schism_keymod_t sdl3_modkey_trans(SDL_Keymod mod)
{
	schism_keymod_t res = 0;

	if (mod & SDL_KMOD_LSHIFT)
		res |= SCHISM_KEYMOD_LSHIFT;

	if (mod & SDL_KMOD_RSHIFT)
		res |= SCHISM_KEYMOD_RSHIFT;

	if (mod & SDL_KMOD_LCTRL)
		res |= SCHISM_KEYMOD_LCTRL;

	if (mod & SDL_KMOD_RCTRL)
		res |= SCHISM_KEYMOD_RCTRL;

	if (mod & SDL_KMOD_LALT)
		res |= SCHISM_KEYMOD_LALT;

	if (mod & SDL_KMOD_RALT)
		res |= SCHISM_KEYMOD_RALT;

	if (mod & SDL_KMOD_LGUI)
		res |= SCHISM_KEYMOD_LGUI;

	if (mod & SDL_KMOD_RGUI)
		res |= SCHISM_KEYMOD_RGUI;

	if (mod & SDL_KMOD_NUM)
		res |= SCHISM_KEYMOD_NUM;

	if (mod & SDL_KMOD_CAPS)
		res |= SCHISM_KEYMOD_CAPS;

	if (mod & SDL_KMOD_MODE)
		res |= SCHISM_KEYMOD_MODE;

	return res;
}

schism_keymod_t sdl3_event_mod_state(void)
{
	return sdl3_modkey_trans(sdl3_GetModState());
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

#ifdef SCHISM_WIN32
# include <windows.h>

static bool SDLCALL sdl3_win32_msg_hook(SCHISM_UNUSED void *userdata, MSG *msg)
{
	schism_event_t e;

	e.type = SCHISM_EVENT_WM_MSG;
	e.wm_msg.subsystem = SCHISM_WM_MSG_SUBSYSTEM_WINDOWS;
	e.wm_msg.msg.win.hwnd = msg->hwnd;
	e.wm_msg.msg.win.msg = msg->message;
	e.wm_msg.msg.win.wparam = msg->wParam;
	e.wm_msg.msg.win.lparam = msg->lParam;

	// ignore WM_DROPFILES messages. these are already handled
	// by the SDL_DROPFILES event and trying to use our implementation
	// only results in an empty string which is undesirable
	if (e.wm_msg.msg.win.msg != WM_DROPFILES)
		events_push_event(&e);

	return true;
}
#endif

#ifdef SCHISM_USE_X11
# include <X11/Xlib.h>

static bool SDLCALL sdl3_x11_msg_hook(SCHISM_UNUSED void *userdata, XEvent *xevent)
{
	schism_event_t e;

	e.type = SCHISM_EVENT_WM_MSG;
	e.wm_msg.subsystem = SCHISM_WM_MSG_SUBSYSTEM_X11;
	e.wm_msg.msg.x11.event.type = xevent->type;
#if 0 // handled by SDL's clipboard
	if (e.syswm.msg->msg.x11.event.type == SelectionRequest) {
		e.wm_msg.msg.x11.event.selection_request.serial = xevent->xselectionrequest.serial;
		e.wm_msg.msg.x11.event.selection_request.send_event = xevent->xselectionrequest.send_event;     // `Bool' in Xlib
		e.wm_msg.msg.x11.event.selection_request.display = xevent->xselectionrequest.display;      // `Display *' in Xlib
		e.wm_msg.msg.x11.event.selection_request.owner = xevent->xselectionrequest.owner;     // `Window' in Xlib
		e.wm_msg.msg.x11.event.selection_request.requestor = xevent->xselectionrequest.requestor; // `Window' in Xlib
		e.wm_msg.msg.x11.event.selection_request.selection = xevent->xselectionrequest.selection; // `Atom' in Xlib
		e.wm_msg.msg.x11.event.selection_request.target = xevent->xselectionrequest.target;    // `Atom' in Xlib
		e.wm_msg.msg.x11.event.selection_request.property = xevent->xselectionrequest.property;  // `Atom' in Xlib
		e.wm_msg.msg.x11.event.selection_request.time = xevent->xselectionrequest.time;      // `Time' in Xlib
		events_push_event(&e);
	}
#endif

	return true;
}
#endif

void sdl3_pump_events(void)
{
	SDL_Event e;

	while (sdl3_PollEvent(&e)) {
		schism_event_t schism_event = {0};

#ifdef SCHISM_CONTROLLER
		if (!sdl3_controller_sdlevent(&e))
			continue;
#endif

		switch (e.type) {
		case SDL_EVENT_QUIT:
			schism_event.type = SCHISM_QUIT;
			events_push_event(&schism_event);
			break;
		case SDL_EVENT_WINDOW_SHOWN:
			schism_event.type = SCHISM_WINDOWEVENT_SHOWN;
			events_push_event(&schism_event);
			break;
		case SDL_EVENT_WINDOW_EXPOSED:
			schism_event.type = SCHISM_WINDOWEVENT_EXPOSED;
			events_push_event(&schism_event);
			break;
		case SDL_EVENT_WINDOW_FOCUS_LOST:
			schism_event.type = SCHISM_WINDOWEVENT_FOCUS_LOST;
			events_push_event(&schism_event);
			break;
		case SDL_EVENT_WINDOW_FOCUS_GAINED:
			schism_event.type = SCHISM_WINDOWEVENT_FOCUS_GAINED;
			events_push_event(&schism_event);
			break;
		case SDL_EVENT_WINDOW_RESIZED:
			// the RESIZED event was changed in SDL 3 and now
			// is basically what SDL_WINDOWEVENT_SIZE_CHANGED
			// once was. doesn't matter for us, we handled
			// both events the same anyway.
			schism_event.type = SCHISM_WINDOWEVENT_RESIZED;
			schism_event.window.data.resized.width = e.window.data1;
			schism_event.window.data.resized.height = e.window.data2;
			events_push_event(&schism_event);
			break;
		case SDL_EVENT_KEY_DOWN:
			// pop any pending keydowns
			pop_pending_keydown(NULL);

			if (e.key.key & SDLK_EXTENDED_MASK)
				break; // I don't know how to handle these

			schism_event.type = SCHISM_KEYDOWN;
			schism_event.key.state = KEY_PRESS;
			schism_event.key.repeat = e.key.repeat;

			// Schism's and SDL3's representation of these are the same.
			schism_event.key.sym = e.key.key;
			schism_event.key.scancode = e.key.scancode;
			schism_event.key.mod = sdl3_modkey_trans(e.key.mod); // except this one!

			//if (!sdl3_TextInputActive()) {
				// push it NOW
			//	events_push_event(&schism_event);
			//} else {
				push_pending_keydown(&schism_event);
			//}

			break;
		case SDL_EVENT_KEY_UP:
			// pop any pending keydowns
			pop_pending_keydown(NULL);

			if (e.key.key & SDLK_EXTENDED_MASK)
				break; // I don't know how to handle these

			schism_event.type = SCHISM_KEYUP;
			schism_event.key.state = KEY_RELEASE;
			schism_event.key.sym = e.key.key;
			schism_event.key.scancode = e.key.scancode;
			schism_event.key.mod = sdl3_modkey_trans(e.key.mod);

			events_push_event(&schism_event);

			break;
		case SDL_EVENT_TEXT_INPUT:
			if (have_pending_keydown) {
				pop_pending_keydown(e.text.text);
			} else {
				schism_event.type = SCHISM_TEXTINPUT;

				strncpy(schism_event.text.text, e.text.text, ARRAY_SIZE(schism_event.text.text));
				schism_event.text.text[ARRAY_SIZE(schism_event.text.text)-1] = '\0';

				events_push_event(&schism_event);
			}
			break;

		case SDL_EVENT_MOUSE_MOTION:
			schism_event.type = SCHISM_MOUSEMOTION;
			schism_event.motion.x = e.motion.x;
			schism_event.motion.y = e.motion.y;
			events_push_event(&schism_event);
			break;
		case SDL_EVENT_MOUSE_BUTTON_DOWN:
		case SDL_EVENT_MOUSE_BUTTON_UP:
			schism_event.type = (e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) ? SCHISM_MOUSEBUTTONDOWN : SCHISM_MOUSEBUTTONUP;

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

			schism_event.button.state = (int)e.button.down;
			schism_event.button.clicks = e.button.clicks;

			schism_event.button.x = e.button.x;
			schism_event.button.y = e.button.y;
			events_push_event(&schism_event);
			break;
		case SDL_EVENT_MOUSE_WHEEL:
			schism_event.type = SCHISM_MOUSEWHEEL;

			schism_event.wheel.x = e.wheel.x;
			schism_event.wheel.y = e.wheel.y;

			schism_event.wheel.mouse_x = e.wheel.mouse_x;
			schism_event.wheel.mouse_y = e.wheel.mouse_y;

			events_push_event(&schism_event);
			break;
		case SDL_EVENT_DROP_FILE:
			schism_event.type = SCHISM_DROPFILE;

			schism_event.drop.file = str_dup(e.drop.data);

			events_push_event(&schism_event);
			break;
		/* these two have no structures because we don't use them */
		case SDL_EVENT_AUDIO_DEVICE_ADDED:
			schism_event.type = SCHISM_AUDIODEVICEADDED;
			events_push_event(&schism_event);
			break;
		case SDL_EVENT_AUDIO_DEVICE_REMOVED:
			schism_event.type = SCHISM_AUDIODEVICEREMOVED;
			events_push_event(&schism_event);
			break;
		default:
			break;
		}
	}

	pop_pending_keydown(NULL);
}

//////////////////////////////////////////////////////////////////////////////
// dynamic loading

static int sdl3_events_load_syms(void)
{
	SCHISM_SDL3_SYM(InitSubSystem);
	SCHISM_SDL3_SYM(QuitSubSystem);

	SCHISM_SDL3_SYM(GetModState);
	SCHISM_SDL3_SYM(PollEvent);
#ifdef SCHISM_WIN32
	SCHISM_SDL3_SYM(SetWindowsMessageHook);
#endif
#ifdef SCHISM_USE_X11
	SCHISM_SDL3_SYM(SetX11EventHook);
#endif

	SCHISM_SDL3_SYM(free);

	return 0;
}

static int sdl3_events_init(void)
{
	if (!sdl3_init())
		return 0;

	if (sdl3_events_load_syms())
		return 0;

	if (!sdl3_InitSubSystem(SDL_INIT_EVENTS))
		return 0;

#ifdef SCHISM_USE_X11
	sdl3_SetX11EventHook(sdl3_x11_msg_hook, NULL);
#endif

#ifdef SCHISM_WIN32
	sdl3_SetWindowsMessageHook(sdl3_win32_msg_hook, NULL);
#endif

	return 1;
}

static void sdl3_events_quit(void)
{
	sdl3_QuitSubSystem(SDL_INIT_EVENTS);
	sdl3_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_events_backend_t schism_events_backend_sdl3 = {
	.init = sdl3_events_init,
	.quit = sdl3_events_quit,

#if !defined(SCHISM_WII) && !defined(SCHISM_WIIU)
	/* These ports have no key repeat... */
	.flags = SCHISM_EVENTS_BACKEND_HAS_KEY_REPEAT,
#else
	.flags = 0,
#endif

	.keymod_state = sdl3_event_mod_state,
	.pump_events = sdl3_pump_events,
};
