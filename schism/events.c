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
#include "sdlmain.h"
#include "mem.h"

#include "backend/events.h"

/* ------------------------------------------------------ */

static SDL_mutex *queue_mutex = NULL;

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

int schism_init_event(void)
{
	queue_mutex = SDL_CreateMutex();
	if (!queue_mutex)
		return 0;

	return 1;
}

int schism_have_event(void)
{
	SDL_LockMutex(queue_mutex);

	if (queue.size) {
		SDL_UnlockMutex(queue_mutex);
		return 1;
	}

	SDL_UnlockMutex(queue_mutex);

	be_pump_events();

	SDL_LockMutex(queue_mutex);

	if (queue.size) {
		SDL_UnlockMutex(queue_mutex);
		return 1;
	}

	SDL_UnlockMutex(queue_mutex);

	return 0;
}

int schism_poll_event(schism_event_t *event)
{
	SDL_LockMutex(queue_mutex);

	if (queue_dequeue(event)) {
		SDL_UnlockMutex(queue_mutex);
		return 1;
	}

	// try pumping the events.
	be_pump_events();

	if (queue_dequeue(event)) {
		SDL_UnlockMutex(queue_mutex);
		return 1;
	}

	SDL_UnlockMutex(queue_mutex);

	// welp
	return 0;
}

// implicitly fills in the timestamp
int schism_push_event(const schism_event_t *event)
{
	schism_event_t e = *event;

	e.common.timestamp = be_timer_ticks();

	SDL_LockMutex(queue_mutex);

	queue_enqueue(&e);

	SDL_UnlockMutex(queue_mutex);

	return 1;
}
