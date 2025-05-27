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

/* Set up the event queue */

#include "headers.h"
#include "events.h"
#include "mem.h"
#include "osdefs.h"
#include "config.h" // keyboard crap

#include "mt.h"

#include "backend/events.h"

const schism_events_backend_t *events_backend = NULL;

/* ------------------------------------------------------ */

static mt_mutex_t *queue_mutex = NULL;

#define EVENTQUEUE_CAPACITY 128
static struct {
	int front, rear, size;
	schism_event_t events[EVENTQUEUE_CAPACITY];
} queue = {0};

static inline int queue_enqueue(const schism_event_t *event)
{
	if (queue.size == EVENTQUEUE_CAPACITY)
		return 0;

	queue.events[queue.rear] = *event;

	queue.rear = (queue.rear + 1) % EVENTQUEUE_CAPACITY;
	queue.size++;

	return 1;
}

static inline int queue_dequeue(schism_event_t *event)
{
	if (!queue.size)
		return 0;

	*event = queue.events[queue.front];

	queue.front = (queue.front + 1) % EVENTQUEUE_CAPACITY;
	queue.size--;

	return 1;
}

/* ------------------------------------------------------ */

// Called back by the video backend;
int events_init(const schism_events_backend_t *backend)
{
	if (backend) {
		events_backend = backend;
		if (!events_backend->init())
			return 0;
	}

	if (cfg_kbd_repeat_delay && cfg_kbd_repeat_rate) {
		// Override everything.
		kbd_set_key_repeat(cfg_kbd_repeat_delay, cfg_kbd_repeat_rate);
	} else if (events_backend && !(events_backend->flags & SCHISM_EVENTS_BACKEND_HAS_KEY_REPEAT)) {
		// Ok, we have to manually configure key repeat.

		int delay = cfg_kbd_repeat_delay, rate = cfg_kbd_repeat_rate;

		if ((!delay || !rate) && !os_get_key_repeat(&delay, &rate)) {
			delay = 500;
			rate = 30;
		}

		kbd_set_key_repeat(delay, rate);
	}

	queue_mutex = mt_mutex_create();
	SCHISM_RUNTIME_ASSERT(queue_mutex, "Failed to create event queue mutex.");

	return 1;
}

void events_quit(void)
{
	if (queue_mutex)
		mt_mutex_delete(queue_mutex);

	if (events_backend) {
		events_backend->quit();
		events_backend = NULL;
	}
}

int events_have_event(void)
{
	mt_mutex_lock(queue_mutex);

	if (queue.size) {
		mt_mutex_unlock(queue_mutex);
		return 1;
	}

	// try pumping the events.
	events_pump_events();

	if (queue.size) {
		mt_mutex_unlock(queue_mutex);
		return 1;
	}

	mt_mutex_unlock(queue_mutex);

	return 0;
}

void events_pump_events(void)
{
	// eh
	if (events_backend) events_backend->pump_events();
}

int events_poll_event(schism_event_t *event)
{
	if (!event)
		return events_have_event();

	mt_mutex_lock(queue_mutex);

	if (queue_dequeue(event)) {
		mt_mutex_unlock(queue_mutex);
		return 1;
	}

	// try pumping the events.
	events_pump_events();

	if (queue_dequeue(event)) {
		mt_mutex_unlock(queue_mutex);
		return 1;
	}

	mt_mutex_unlock(queue_mutex);

	// welp
	return 0;
}

// implicitly fills in the timestamp
int events_push_event(const schism_event_t *event)
{
	// An array of event filters. The reason this is used *here*
	// instead of in main is because we need to filter X11 events
	// *as they are pumped*, and putting win32 and macosx events
	// here is just a side effect of that.
	static int (*const event_filters[])(schism_event_t *event) = {
#ifdef SCHISM_WIN32
		win32_event,
#endif
#ifdef SCHISM_MACOSX
		macosx_event,
#endif
#ifdef SCHISM_USE_X11
		x11_event,
#endif
		NULL,
	};
	int i;
	schism_event_t e = *event;

	e.common.timestamp = timer_ticks();

	for (i = 0; event_filters[i]; i++)
		if (!event_filters[i](&e))
			return 1;

	mt_mutex_lock(queue_mutex);

	queue_enqueue(&e);

	mt_mutex_unlock(queue_mutex);

	return 1;
}

schism_keymod_t events_get_keymod_state(void)
{
	return (events_backend ? events_backend->keymod_state() : 0);
}

const char *events_describe_physical_key_for_qwerty_key(char qwerty_key)
{
	schism_keysym_t keysym = (schism_keysym_t)qwerty_key;
	schism_scancode_t scancode;
	int shifted = 0;

	const char *key_name;

	static char description[50];

	switch (keysym) {
		case SCHISM_KEYSYM_UNKNOWN: scancode = SCHISM_SCANCODE_UNKNOWN; break;

		case SCHISM_KEYSYM_RETURN: scancode = SCHISM_SCANCODE_RETURN; break;
		case SCHISM_KEYSYM_ESCAPE: scancode = SCHISM_SCANCODE_ESCAPE; break;
		case SCHISM_KEYSYM_BACKSPACE: scancode = SCHISM_SCANCODE_BACKSPACE; break;
		case SCHISM_KEYSYM_TAB: scancode = SCHISM_SCANCODE_TAB; break;
		case SCHISM_KEYSYM_SPACE: scancode = SCHISM_SCANCODE_SPACE; break;
		case SCHISM_KEYSYM_EXCLAIM: scancode = SCHISM_SCANCODE_1; shifted = 1; break;
		case SCHISM_KEYSYM_QUOTEDBL: scancode = SCHISM_SCANCODE_APOSTROPHE; shifted = 1; break;
		case SCHISM_KEYSYM_HASH: scancode = SCHISM_SCANCODE_3; shifted = 1; break;
		case SCHISM_KEYSYM_PERCENT: scancode = SCHISM_SCANCODE_5; shifted = 1; break;
		case SCHISM_KEYSYM_DOLLAR: scancode = SCHISM_SCANCODE_4; shifted = 1; break;
		case SCHISM_KEYSYM_AMPERSAND: scancode = SCHISM_SCANCODE_7; shifted = 1; break;
		case SCHISM_KEYSYM_QUOTE: scancode = SCHISM_SCANCODE_APOSTROPHE; break;
		case SCHISM_KEYSYM_LEFTPAREN: scancode = SCHISM_SCANCODE_9; shifted = 1; break;
		case SCHISM_KEYSYM_RIGHTPAREN: scancode = SCHISM_SCANCODE_0; shifted = 1; break;
		case SCHISM_KEYSYM_ASTERISK: scancode = SCHISM_SCANCODE_8; shifted = 1; break;
		case SCHISM_KEYSYM_PLUS: scancode = SCHISM_SCANCODE_EQUALS; shifted = 1; break;
		case SCHISM_KEYSYM_COMMA: scancode = SCHISM_SCANCODE_COMMA; break;
		case SCHISM_KEYSYM_MINUS: scancode = SCHISM_SCANCODE_MINUS; break;
		case SCHISM_KEYSYM_PERIOD: scancode = SCHISM_SCANCODE_PERIOD; break;
		case SCHISM_KEYSYM_SLASH: scancode = SCHISM_SCANCODE_SLASH; break;
		case SCHISM_KEYSYM_0: scancode = SCHISM_SCANCODE_0; break;
		case SCHISM_KEYSYM_1: scancode = SCHISM_SCANCODE_1; break;
		case SCHISM_KEYSYM_2: scancode = SCHISM_SCANCODE_2; break;
		case SCHISM_KEYSYM_3: scancode = SCHISM_SCANCODE_3; break;
		case SCHISM_KEYSYM_4: scancode = SCHISM_SCANCODE_4; break;
		case SCHISM_KEYSYM_5: scancode = SCHISM_SCANCODE_5; break;
		case SCHISM_KEYSYM_6: scancode = SCHISM_SCANCODE_6; break;
		case SCHISM_KEYSYM_7: scancode = SCHISM_SCANCODE_7; break;
		case SCHISM_KEYSYM_8: scancode = SCHISM_SCANCODE_8; break;
		case SCHISM_KEYSYM_9: scancode = SCHISM_SCANCODE_9; break;
		case SCHISM_KEYSYM_COLON: scancode = SCHISM_SCANCODE_SEMICOLON; shifted = 1; break;
		case SCHISM_KEYSYM_SEMICOLON: scancode = SCHISM_SCANCODE_SEMICOLON; break;
		case SCHISM_KEYSYM_LESS: scancode = SCHISM_SCANCODE_COMMA; shifted = 1; break;
		case SCHISM_KEYSYM_EQUALS: scancode = SCHISM_SCANCODE_EQUALS; break;
		case SCHISM_KEYSYM_GREATER: scancode = SCHISM_SCANCODE_PERIOD; shifted = 1; break;
		case SCHISM_KEYSYM_QUESTION: scancode = SCHISM_SCANCODE_SLASH; shifted = 1; break;
		case SCHISM_KEYSYM_AT: scancode = SCHISM_SCANCODE_2; shifted = 1; break;

		case SCHISM_KEYSYM_LEFTBRACKET: scancode = SCHISM_SCANCODE_LEFTBRACKET; break;
		case SCHISM_KEYSYM_BACKSLASH: scancode = SCHISM_SCANCODE_BACKSLASH; break;
		case SCHISM_KEYSYM_RIGHTBRACKET: scancode = SCHISM_SCANCODE_RIGHTBRACKET; break;
		case SCHISM_KEYSYM_CARET: scancode = SCHISM_SCANCODE_6; shifted = 1; break;
		case SCHISM_KEYSYM_UNDERSCORE: scancode = SCHISM_SCANCODE_MINUS; shifted = 1; break;
		case SCHISM_KEYSYM_BACKQUOTE: scancode = SCHISM_SCANCODE_GRAVE; break;
		case SCHISM_KEYSYM_a: scancode = SCHISM_SCANCODE_A; break;
		case SCHISM_KEYSYM_b: scancode = SCHISM_SCANCODE_B; break;
		case SCHISM_KEYSYM_c: scancode = SCHISM_SCANCODE_C; break;
		case SCHISM_KEYSYM_d: scancode = SCHISM_SCANCODE_D; break;
		case SCHISM_KEYSYM_e: scancode = SCHISM_SCANCODE_E; break;
		case SCHISM_KEYSYM_f: scancode = SCHISM_SCANCODE_F; break;
		case SCHISM_KEYSYM_g: scancode = SCHISM_SCANCODE_G; break;
		case SCHISM_KEYSYM_h: scancode = SCHISM_SCANCODE_H; break;
		case SCHISM_KEYSYM_i: scancode = SCHISM_SCANCODE_I; break;
		case SCHISM_KEYSYM_j: scancode = SCHISM_SCANCODE_J; break;
		case SCHISM_KEYSYM_k: scancode = SCHISM_SCANCODE_K; break;
		case SCHISM_KEYSYM_l: scancode = SCHISM_SCANCODE_L; break;
		case SCHISM_KEYSYM_m: scancode = SCHISM_SCANCODE_M; break;
		case SCHISM_KEYSYM_n: scancode = SCHISM_SCANCODE_N; break;
		case SCHISM_KEYSYM_o: scancode = SCHISM_SCANCODE_O; break;
		case SCHISM_KEYSYM_p: scancode = SCHISM_SCANCODE_P; break;
		case SCHISM_KEYSYM_q: scancode = SCHISM_SCANCODE_Q; break;
		case SCHISM_KEYSYM_r: scancode = SCHISM_SCANCODE_R; break;
		case SCHISM_KEYSYM_s: scancode = SCHISM_SCANCODE_S; break;
		case SCHISM_KEYSYM_t: scancode = SCHISM_SCANCODE_T; break;
		case SCHISM_KEYSYM_u: scancode = SCHISM_SCANCODE_U; break;
		case SCHISM_KEYSYM_v: scancode = SCHISM_SCANCODE_V; break;
		case SCHISM_KEYSYM_w: scancode = SCHISM_SCANCODE_W; break;
		case SCHISM_KEYSYM_x: scancode = SCHISM_SCANCODE_X; break;
		case SCHISM_KEYSYM_y: scancode = SCHISM_SCANCODE_Y; break;
		case SCHISM_KEYSYM_z: scancode = SCHISM_SCANCODE_Z; break;

		case SCHISM_KEYSYM_CAPSLOCK: scancode = SCHISM_SCANCODE_CAPSLOCK; break;

		case SCHISM_KEYSYM_DELETE: scancode = SCHISM_SCANCODE_DELETE; break;
	}

	key_name = events_backend->get_key_name_from_scancode(scancode);

	if (!shifted)
		return key_name;
	else {
		if (strcmp(key_name, ",") == 0)
			return "<";
		else if (strcmp(key_name, ".") == 0)
			return ">";
		else
		{
			sprintf(description, "Shift-%s", key_name);

			return description;
		}
	}
}

