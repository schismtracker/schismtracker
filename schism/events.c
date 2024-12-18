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

#include "threads.h"

#include "backend/events.h"

const schism_events_backend_t *events_backend = NULL;

/* ------------------------------------------------------ */

static schism_mutex_t *queue_mutex = NULL;

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
	int i;
	int success;

	if (!backend)
		return 0;

	events_backend = backend;
	if (!events_backend->init())
		return 0;

	if (cfg_kbd_repeat_delay && cfg_kbd_repeat_rate) {
		// Override everything.
		kbd_set_key_repeat(cfg_kbd_repeat_delay, cfg_kbd_repeat_rate);
	} else if (!(events_backend->flags & SCHISM_EVENTS_BACKEND_HAS_KEY_REPEAT)) {
		// Ok, we have to manually configure key repeat.

		int delay = cfg_kbd_repeat_delay, rate = cfg_kbd_repeat_rate;

		if ((!delay || !rate) && !os_get_key_repeat(&delay, &rate)) {
			delay = 500;
			rate = 30;
		}

		kbd_set_key_repeat(delay, rate);
	}

	queue_mutex = mt_mutex_create();
	if (!queue_mutex) {
		os_show_message_box("Critical error!", "Failed to create event queue mutex...");
		return 0;
	}

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
	events_backend->pump_events();

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
	events_backend->pump_events();
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
	events_backend->pump_events();

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
			continue;

	mt_mutex_lock(queue_mutex);

	queue_enqueue(&e);

	mt_mutex_unlock(queue_mutex);

	return 1;
}

schism_keymod_t events_get_keymod_state(void)
{
	return events_backend->keymod_state();
}
