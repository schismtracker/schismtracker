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

#include "it.h"
#include "midi.h"

#include "util.h"
#include "timer.h"
#include "mem.h"

#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>

#ifdef SCHISM_WIN32
/* we shouldn't need this actually */
# define pid_t _pid_t
#endif

#include <sched.h>

#define PORT_NAME "Schism Tracker"

/* shamelessly ripped off SDL_jackaudio.c here */
static jack_client_t *(*JACK_jack_client_open)(const char *, jack_options_t, jack_status_t *, ...);
static int (*JACK_jack_client_close)(jack_client_t *);
static int (*JACK_jack_activate)(jack_client_t *);
static void *(*JACK_jack_port_get_buffer)(jack_port_t *, jack_nframes_t);
static int (*JACK_jack_port_unregister)(jack_client_t *, jack_port_t *);
static jack_port_t *(*JACK_jack_port_register)(jack_client_t *, const char *, const char *, unsigned long, unsigned long);
static int (*JACK_jack_connect)(jack_client_t *, const char *, const char *);
static int (*JACK_jack_set_process_callback)(jack_client_t *, JackProcessCallback, void *);
static jack_nframes_t (*JACK_jack_midi_get_event_count)(void *);
static int (*JACK_jack_midi_event_get)(jack_midi_event_t *, void *, uint32_t);
static jack_port_t* (*JACK_jack_port_by_name)(jack_client_t *, const char*);
static jack_ringbuffer_t* (*JACK_jack_ringbuffer_create)(size_t);
static size_t (*JACK_jack_ringbuffer_write_space)(const jack_ringbuffer_t *);
static size_t (*JACK_jack_ringbuffer_write)(jack_ringbuffer_t *, const char *, size_t);
static size_t (*JACK_jack_ringbuffer_peek)(jack_ringbuffer_t *, char *, size_t);
static size_t (*JACK_jack_ringbuffer_read_space)(const jack_ringbuffer_t *);
static void (*JACK_jack_ringbuffer_read_advance)(jack_ringbuffer_t *, size_t);
static size_t (*JACK_jack_ringbuffer_read)(jack_ringbuffer_t *, char *, size_t);
static jack_midi_data_t * (*JACK_jack_midi_event_reserve)(void *, jack_nframes_t, size_t);
static void (*JACK_jack_ringbuffer_free)(jack_ringbuffer_t *);
static const char * (*JACK_jack_port_name)(const jack_port_t *);
static int (*JACK_jack_connect)(jack_client_t *, const char *, const char *);
static int (*JACK_jack_disconnect)(jack_client_t *, const char *, const char *);
static const char ** (*JACK_jack_get_ports)(jack_client_t *, const char *, const char *, unsigned long);
static int (*JACK_jack_port_is_mine)(const jack_client_t *, const jack_port_t *);
static void (*JACK_jack_midi_clear_buffer)(void *);
static jack_time_t (*JACK_jack_frames_to_time)(const jack_client_t *client, jack_nframes_t);
static jack_nframes_t (*JACK_jack_time_to_frames)(const jack_client_t *, jack_time_t);
static jack_time_t (*JACK_jack_get_time)(void);
static void (*JACK_jack_free)(void *ptr);

static int load_jack_syms(void);

#ifdef JACK_DYNAMIC_LOAD

#include "loadso.h"

void *jack_dltrick_handle_ = NULL;

static void jack_dlend(void) {
	if (jack_dltrick_handle_) {
		loadso_object_unload(jack_dltrick_handle_);
		jack_dltrick_handle_ = NULL;
	}
}

static int jack_dlinit(void) {
	if (jack_dltrick_handle_)
		return 0;

	// libjack.so.0
	jack_dltrick_handle_ = library_load("jack", 0, 0);
	if (!jack_dltrick_handle_)
		return -1;

	int retval = load_jack_syms();
	if (retval < 0)
		jack_dlend();

	return retval;
}

// FIXME this is repeated in many places
SCHISM_STATIC_ASSERT(sizeof(void (*)) == sizeof(void *), "dynamic loading code assumes function pointer and void pointer are of equivalent size");

static int load_jack_sym(const char *fn, void *addr) {
	void *func = loadso_function_load(jack_dltrick_handle_, fn);
	if (!func)
		return 0;

	memcpy(addr, &func, sizeof(void *));

	return 1;
}

#define SCHISM_JACK_SYM(x) \
	if (!load_jack_sym(#x, &JACK_##x)) return -1

#else

#define SCHISM_JACK_SYM(x) JACK_##x = x

static int jack_dlinit(void) {
	load_jack_syms();
	return 0;
}

#endif

static int load_jack_syms(void) {
	SCHISM_JACK_SYM(jack_client_open);
	SCHISM_JACK_SYM(jack_client_close);
	SCHISM_JACK_SYM(jack_activate);
	SCHISM_JACK_SYM(jack_port_get_buffer);
	SCHISM_JACK_SYM(jack_port_register);
	SCHISM_JACK_SYM(jack_connect);
	SCHISM_JACK_SYM(jack_set_process_callback);
	SCHISM_JACK_SYM(jack_midi_event_get);
	SCHISM_JACK_SYM(jack_midi_get_event_count);
	SCHISM_JACK_SYM(jack_port_by_name);
	SCHISM_JACK_SYM(jack_ringbuffer_create);
	SCHISM_JACK_SYM(jack_ringbuffer_free);
	SCHISM_JACK_SYM(jack_ringbuffer_write_space);
	SCHISM_JACK_SYM(jack_ringbuffer_write);
	SCHISM_JACK_SYM(jack_ringbuffer_peek);
	SCHISM_JACK_SYM(jack_ringbuffer_read_space);
	SCHISM_JACK_SYM(jack_ringbuffer_read_advance);
	SCHISM_JACK_SYM(jack_ringbuffer_read);
	SCHISM_JACK_SYM(jack_midi_event_reserve);
	SCHISM_JACK_SYM(jack_port_name);
	SCHISM_JACK_SYM(jack_connect);
	SCHISM_JACK_SYM(jack_disconnect);
	SCHISM_JACK_SYM(jack_get_ports);
	SCHISM_JACK_SYM(jack_port_is_mine);
	SCHISM_JACK_SYM(jack_midi_clear_buffer);
	SCHISM_JACK_SYM(jack_get_time);
	SCHISM_JACK_SYM(jack_time_to_frames);
	SCHISM_JACK_SYM(jack_frames_to_time);
	SCHISM_JACK_SYM(jack_free);
	SCHISM_JACK_SYM(jack_port_unregister);

	return 0;
}

/* ------------------------------------------------------ */

/* this is based off of code from RtMidi */

#define JACK_RINGBUFFER_SIZE 16384

static jack_client_t *client = NULL;
static jack_port_t *midi_in_port = NULL;
static jack_port_t *midi_out_port = NULL;

static jack_ringbuffer_t *ringbuffer_in = NULL;
static jack_ringbuffer_t *ringbuffer_out = NULL;
static size_t ringbuffer_in_max_write = 0;
static size_t ringbuffer_out_max_write = 0;

struct jack_midi {
	jack_port_t* port;
};

static void _jack_send(SCHISM_UNUSED struct midi_port *p, const unsigned char *data, uint32_t len, uint32_t delay) {
	while (len > 0) {
		uint32_t real_len = midi_event_length(*data);
		if (real_len > len)
			return;

		if (real_len + sizeof(real_len) > ringbuffer_out_max_write)
			return;

		// yield for other threads while the process thread is busy
		while (JACK_jack_ringbuffer_write_space(ringbuffer_out) < real_len + sizeof(real_len))
			sched_yield();

		JACK_jack_ringbuffer_write(ringbuffer_out, (const char*)&real_len, sizeof(real_len));
		JACK_jack_ringbuffer_write(ringbuffer_out, (const char*)data, real_len);

		data += real_len;
		len -= real_len;
	}

	(void)delay;
}

static int _jack_start(struct midi_port *p) {
	struct jack_midi* m = (struct jack_midi *)p->userdata;

	if (p->io & MIDI_INPUT)
		return !JACK_jack_connect(client, JACK_jack_port_name(m->port), JACK_jack_port_name(midi_in_port));
	else if (p->io & MIDI_OUTPUT)
		return !JACK_jack_connect(client, JACK_jack_port_name(midi_out_port), JACK_jack_port_name(m->port));

	return 1;
}

static int _jack_stop(struct midi_port *p) {
	struct jack_midi* m = (struct jack_midi *)p->userdata;

	if (p->io & MIDI_INPUT)
		return !JACK_jack_disconnect(client, JACK_jack_port_name(m->port), JACK_jack_port_name(midi_in_port));
	else if (p->io & MIDI_OUTPUT)
		return !JACK_jack_disconnect(client, JACK_jack_port_name(midi_out_port), JACK_jack_port_name(m->port));

	return 1;
}

/* gets called by JACK in a separate thread */
static int _jack_process(jack_nframes_t nframes, void *user_data) {
	struct midi_provider *p = (struct midi_provider *)user_data;
	if (p->cancelled)
		return 1; /* try to exit safely */

	/* handle midi in */
	void *midi_in_buffer = JACK_jack_port_get_buffer(midi_in_port, nframes);
	const jack_nframes_t count = JACK_jack_midi_get_event_count(midi_in_buffer);

	for (jack_nframes_t i = 0; i < count; i++) {
		jack_midi_event_t event = {0};
		if (JACK_jack_midi_event_get(&event, midi_in_buffer, i))
			break;

		const uint32_t len = event.size;

		if (len + sizeof(len) > ringbuffer_out_max_write)
			break;

		if (JACK_jack_ringbuffer_write_space(ringbuffer_in) < len + sizeof(len))
			break; /* give up */

		JACK_jack_ringbuffer_write(ringbuffer_in, (const char*)&len, sizeof(len));
		JACK_jack_ringbuffer_write(ringbuffer_in, (const char*)event.buffer, len);
	}

	/* handle midi out */
	void *midi_out_buffer = JACK_jack_port_get_buffer(midi_out_port, nframes);

	JACK_jack_midi_clear_buffer(midi_out_buffer);

	uint32_t size = 0;

	while (JACK_jack_ringbuffer_peek(ringbuffer_out, (char*)&size, sizeof(size)) == sizeof(size)
	       && JACK_jack_ringbuffer_read_space(ringbuffer_out) >= sizeof(size) + size) {
		JACK_jack_ringbuffer_read_advance(ringbuffer_out, sizeof(size));

		jack_midi_data_t* data = JACK_jack_midi_event_reserve(midi_out_buffer, 0, size);

		if (data)
			JACK_jack_ringbuffer_read(ringbuffer_out, (char*)data, size);
		else
			JACK_jack_ringbuffer_read_advance(ringbuffer_out, size);
	}

	return 0;
}

static void _jack_disconnect(void)
{
	if (client) {
		if (midi_in_port) {
			JACK_jack_port_unregister(client, midi_in_port);
			midi_out_port = NULL;
		}

		if (midi_out_port) {
			JACK_jack_port_unregister(client, midi_out_port);
			midi_out_port = NULL;
		}

		JACK_jack_client_close(client);
		client = NULL;
	}
}

static int _jack_thread(struct midi_provider *p)
{
	/* processes midi in events... */
	while (!p->cancelled) {
		uint32_t size = 0;

		while (JACK_jack_ringbuffer_peek(ringbuffer_in, (char*)&size, sizeof(size)) == sizeof(size)
		       && JACK_jack_ringbuffer_read_space(ringbuffer_in) >= sizeof(size) + size) {
			JACK_jack_ringbuffer_read_advance(ringbuffer_in, sizeof(size));

			unsigned char *event = malloc(size);

			if (event) {
				JACK_jack_ringbuffer_read(ringbuffer_in, (char *)event, size);
				midi_received_cb(NULL, event, size);
				free(event);
			} else {
				JACK_jack_ringbuffer_read_advance(ringbuffer_in, size);
			}
		}

		timer_msleep(10);
	}

	// if we're cancelled, disconnect
	_jack_disconnect();

	return 0;
}

/* inout for these functions should be EITHER MIDI_INPUT or MIDI_OUTPUT, never both.
 * jack has no concept of duplex ports */
static void _jack_enumerate_ports(const char **port_names, struct midi_provider *p, int inout) {
	/* search for new ports to insert */
	const char **port_name;
	for (port_name = port_names; *port_name; port_name++) {
		struct midi_port *ptr;
		struct jack_midi *m;
		jack_port_t* port = JACK_jack_port_by_name(client, *port_name);
		int ok;

		if (JACK_jack_port_is_mine(client, port))
			continue;

		ptr = NULL;
		ok = 0;
		while (midi_port_foreach(p, &ptr)) {
			m = (struct jack_midi *)ptr->userdata;
			if (ptr->iocap == inout && m->port == port) {
				ptr->mark = 0;
				ok = 1;
			}
		}

		if (ok)
			continue;

		/* create the UI name... */
		char name[55];
		snprintf(name, 55, " %-*.*s (JACK)", 55 - 9, 55 - 9, *port_name);

		m = mem_alloc(sizeof(*m));
		m->port = port;

		midi_port_register(p, inout, name, m, 1);
	}
}

static int _jack_attempt_connect(struct midi_provider* jack_provider_) {
	/* already connected? */
	if (client)
		return 1;

	/* create our client */
	client = JACK_jack_client_open(PORT_NAME, 0, NULL);
	if (!client)
		goto fail;

	midi_in_port = JACK_jack_port_register(client, "MIDI In", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
	if (!midi_in_port)
		goto fail;

	midi_out_port = JACK_jack_port_register(client, "MIDI Out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
	if (!midi_out_port)
		goto fail;

	/* hand this over to JACK */
	if (JACK_jack_set_process_callback(client, _jack_process, (void*)jack_provider_))
		goto fail;

	if (JACK_jack_activate(client))
		goto fail;

	return 1;

fail:
	_jack_disconnect();

	return 0;
}

static void _jack_poll(struct midi_provider* jack_provider_)
{
	const char** ports;

	if (!_jack_attempt_connect(jack_provider_))
		return;

	midi_provider_mark_ports(jack_provider_);

	ports = JACK_jack_get_ports(client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput);
	_jack_enumerate_ports(ports, jack_provider_, MIDI_INPUT);
	JACK_jack_free(ports);

	ports = JACK_jack_get_ports(client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput);
	_jack_enumerate_ports(ports, jack_provider_, MIDI_OUTPUT);
	JACK_jack_free(ports);

	midi_provider_remove_marked_ports(jack_provider_);
}

int jack_midi_setup(void)
{
	static const struct midi_driver jack_driver = {
		.poll = _jack_poll,
		.wake = NULL,
		.flags = 0, // jack is realtime
		.thread = _jack_thread,
		.enable = _jack_start,
		.disable = _jack_stop,
		.send = _jack_send,
	};

#ifdef JACK_DYNAMIC_LOAD
	if (!jack_dltrick_handle_)
#endif
		if (jack_dlinit())
			return 0;

	ringbuffer_in = JACK_jack_ringbuffer_create(JACK_RINGBUFFER_SIZE);
	if (!ringbuffer_in)
		goto fail;

	ringbuffer_out = JACK_jack_ringbuffer_create(JACK_RINGBUFFER_SIZE);
	if (!ringbuffer_out)
		goto fail;

	ringbuffer_in_max_write = JACK_jack_ringbuffer_write_space(ringbuffer_in);
	ringbuffer_out_max_write = JACK_jack_ringbuffer_write_space(ringbuffer_out);

	struct midi_provider* p = midi_provider_register("JACK-MIDI", &jack_driver);
	if (!p) /* how? can this check be removed? */
		goto fail;

	return 1;

fail:
	_jack_disconnect();

	return 0;
}
