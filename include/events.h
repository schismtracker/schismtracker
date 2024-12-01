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
#ifndef SCHISM_EVENTS_H_
#define SCHISM_EVENTS_H_

#include "keyboard.h"
#include "backend/timer.h"

/* types of events delivered by the current backend */
enum {
	/* Application events */
	SCHISM_QUIT           = 0x100, /* User-requested quit */

	/* Window events */
	SCHISM_WINDOWEVENT_SHOWN  = 0x200,
	SCHISM_WINDOWEVENT_HIDDEN,
	SCHISM_WINDOWEVENT_EXPOSED,
	SCHISM_WINDOWEVENT_RESIZED,
	SCHISM_WINDOWEVENT_SIZE_CHANGED,
	SCHISM_WINDOWEVENT_FOCUS_GAINED,
	SCHISM_WINDOWEVENT_FOCUS_LOST,

	//SCHISM_SYSWMEVENT,             /* System specific event */

	/* Keyboard events */
	SCHISM_KEYDOWN        = 0x300, /* Key pressed */
	SCHISM_KEYUP,                  /* Key released */
	SCHISM_TEXTINPUT,              /* Keyboard text input, sent if it wasn't mapped to a keydown */

	/* Mouse events */
	SCHISM_MOUSEMOTION    = 0x400, /* Mouse moved */
	SCHISM_MOUSEBUTTONDOWN,        /* Mouse button pressed */
	SCHISM_MOUSEBUTTONUP,          /* Mouse button released */
	SCHISM_MOUSEWHEEL,             /* Mouse wheel motion */

	/* Clipboard events */
	SCHISM_CLIPBOARDUPDATE = 0x900,  /* The clipboard or primary selection changed */

	/* Drag and drop */
	SCHISM_DROPFILE        = 0x1000, /* The system requests a file open */

	/* Audio hotplug */
	SCHISM_AUDIODEVICEADDED = 0x1100, /* A new audio device is available */
	SCHISM_AUDIODEVICEREMOVED,        /* An audio device has been removed. */

	/* Render events */
	SCHISM_RENDER_TARGETS_RESET = 0x2000, /* The render targets have been reset and their contents need to be updated */
	SCHISM_RENDER_DEVICE_RESET, /* The device has been reset and all textures need to be recreated */

	/* Schism internal events */
	SCHISM_EVENT_UPDATE_IPMIDI  = 0x3000,
	SCHISM_EVENT_PLAYBACK,
	SCHISM_EVENT_NATIVE_OPEN,
	SCHISM_EVENT_NATIVE_SCRIPT,
	SCHISM_EVENT_PASTE,

	SCHISM_EVENT_MIDI_NOTE       = 0x4000,
	SCHISM_EVENT_MIDI_CONTROLLER,
	SCHISM_EVENT_MIDI_PROGRAM,
	SCHISM_EVENT_MIDI_AFTERTOUCH,
	SCHISM_EVENT_MIDI_PITCHBEND,
	SCHISM_EVENT_MIDI_TICK,
	SCHISM_EVENT_MIDI_SYSEX,
	SCHISM_EVENT_MIDI_SYSTEM,

	SCHISM_EVENT_WM_MSG			 = 0x5000,
};

typedef struct {
	uint32_t type;
	schism_ticks_t timestamp;
} schism_common_event_t;

typedef struct {
	schism_common_event_t common;

	// event-specific data
	union {
		struct {
			uint32_t width;
			uint32_t height;
		} resized;
	} data;
} schism_window_event_t;

typedef struct {
	schism_common_event_t common;
	enum key_state state;
	int repeat;

	schism_keysym_t sym;
	schism_scancode_t scancode;
	schism_keymod_t mod;
	char text[32]; // always in UTF-8
} schism_keyboard_event_t;

typedef struct {
	schism_common_event_t common;

	char text[32]; // always in UTF-8
} schism_text_input_event_t;

typedef struct {
	schism_common_event_t common;

	char *file;
} schism_file_drop_event_t;

typedef struct {
	schism_common_event_t common;

	/* X and Y coordinates relative to the window */
	int32_t x;
	int32_t y;
} schism_mouse_motion_event_t;

typedef struct {
	schism_common_event_t common;

	enum mouse_button button;
	int state; // zero if released, non-zero if pressed
	uint8_t clicks; // how many clicks?

	/* X and Y coordinates relative to the window */
	int32_t x;
	int32_t y;
} schism_mouse_button_event_t;

typedef struct {
	schism_common_event_t common;

	int32_t x; // how far on the horizontal axis
	int32_t y; // how far on the vertical axis

	// cursor X and Y coordinates relative to the window
	int32_t mouse_x;
	int32_t mouse_y;
} schism_mouse_wheel_event_t;

typedef struct {
	schism_common_event_t common;

	int mnstatus;
	int channel;
	int note;
	int velocity;
} schism_midi_note_event_t;

typedef struct {
	schism_common_event_t common;
	int value;
	int channel;
	int param;
} schism_midi_controller_event_t;

typedef struct {
	schism_common_event_t common;

	int value;
	int channel;
} schism_midi_program_event_t;

typedef struct {
	schism_common_event_t common;

	int value;
	int channel;
} schism_midi_aftertouch_event_t;

typedef struct {
	schism_common_event_t common;

	int value;
	int channel;
} schism_midi_pitchbend_event_t;

typedef struct {
	schism_common_event_t common;

	int argv;
	int param;
} schism_midi_system_event_t;

typedef struct {
	schism_common_event_t common;
} schism_midi_tick_event_t;

typedef struct {
	uint32_t type;
	schism_ticks_t timestamp;

	unsigned int len;
	unsigned char packet[3];
} schism_midi_sysex_event_t;

typedef struct {
	schism_common_event_t common;

	char *which;
} schism_native_script_event_t;

typedef struct {
	schism_common_event_t common;

	char *file;
} schism_native_open_event_t;

typedef struct {
	schism_common_event_t common;

	char *clipboard;
} schism_clipboard_paste_event_t;

typedef struct {
	schism_common_event_t common;

	enum {SCHISM_WM_MSG_BACKEND_SDL12, SCHISM_WM_MSG_BACKEND_SDL2} backend;
	enum {SCHISM_WM_MSG_SUBSYSTEM_WINDOWS} subsystem;

	union {
		// use generic types when possible, please
		struct {
			void *hwnd;
			uint32_t msg;
			uintptr_t wparam;
			uintptr_t lparam;
		} win;

		/* don't care about others */
	} msg;
} schism_wm_msg_event_t;

typedef union schism_event {
	uint32_t type;

	schism_common_event_t common;
	schism_window_event_t window;
	schism_keyboard_event_t key;
	schism_text_input_event_t text;
	schism_mouse_motion_event_t motion;
	schism_mouse_button_event_t button;
	schism_mouse_wheel_event_t wheel;
	schism_midi_note_event_t midi_note;
	schism_midi_controller_event_t midi_controller;
	schism_midi_program_event_t midi_program;
	schism_midi_aftertouch_event_t midi_aftertouch;
	schism_midi_pitchbend_event_t midi_pitchbend;
	schism_midi_system_event_t midi_system;
	schism_midi_tick_event_t midi_tick;
	schism_midi_sysex_event_t midi_sysex;
	schism_native_script_event_t script;
	schism_native_open_event_t open;
	schism_clipboard_paste_event_t clipboard;
	schism_file_drop_event_t drop;
	schism_wm_msg_event_t wm_msg;
} schism_event_t;

/* ------------------------------------ */

int events_init(void);
void events_quit(void);

int events_have_event(void);
int events_poll_event(schism_event_t *event);
int events_push_event(const schism_event_t *event);

schism_keymod_t events_get_keymod_state(void);

#endif /* SCHISM_EVENT_H_ */
