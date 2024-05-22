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

#include <jack/jack.h>
#include <jack/midiport.h>
#include <jack/ringbuffer.h>

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
static void (*JACK_jack_on_shutdown)(jack_client_t *, JackShutdownCallback, void *);

static int load_jack_syms(void);

#ifdef JACK_DYNAMIC_LOAD

void *jack_dltrick_handle_;

static void jack_dlend(void) {
	if (jack_dltrick_handle_) {
		SDL_UnloadObject(jack_dltrick_handle_);
		jack_dltrick_handle_ = NULL;
	}
}

static int jack_dlinit(void) {
	if (jack_dltrick_handle_)
		return 0;

	jack_dltrick_handle_ = SDL_LoadObject("libjack.so.0");
	if (!jack_dltrick_handle_)
		return -1;

	int retval = load_jack_syms();
	if (retval < 0)
		jack_dlend();

	return retval;
}

static int load_jack_sym(const char *fn, void **addr) {
	*addr = SDL_LoadFunction(jack_dltrick_handle_, fn);
	if (!*addr)
		return 0;

	return 1;
}

/* cast funcs to char* first, to please GCC's strict aliasing rules. */
#define SCHISM_JACK_SYM(x) \
	if (!load_jack_sym(#x, (void **)(char *)&JACK_##x)) \
	return -1

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
	SCHISM_JACK_SYM(jack_on_shutdown);

	return 0;
}

/* ------------------------------------------------------ */

/* this is based off of code from RtMidi */

#define JACK_RINGBUFFER_SIZE 16384

/* need a mutex because JACK tells us about server disconnection
 * in a separate thread */
static SDL_mutex* client_mutex = NULL;
static jack_client_t* client = NULL;
static jack_port_t* midi_in_port = NULL;
static jack_port_t* midi_out_port = NULL;

static jack_ringbuffer_t* ringbuffer = NULL;
static size_t ringbuffer_max_write = 0;

static void _jack_send(UNUSED struct midi_port *p, const unsigned char *data, unsigned int len, unsigned int delay) {
	/* from JACK docs:
	 * Clients must write normalised MIDI data to the port - no
	 * running status and no (1-byte) realtime messages interspersed
	 * with other messages. */
	if (len == 1)
		return;

	if (len + sizeof(len) > ringbuffer_max_write)
		return;

	while (JACK_jack_ringbuffer_write_space(ringbuffer) < len + sizeof(len));

	JACK_jack_ringbuffer_write(ringbuffer, (const char*)&len, sizeof(len));
	JACK_jack_ringbuffer_write(ringbuffer, (const char*)data, len);
}

static int _jack_start(struct midi_port *p) {
	jack_port_t* jack_port = (jack_port_t*)p->userdata;

	if (p->io & MIDI_INPUT)
		return !JACK_jack_connect(client, JACK_jack_port_name(jack_port), JACK_jack_port_name(midi_in_port));
	else if (p->io & MIDI_OUTPUT)
		return !JACK_jack_connect(client, JACK_jack_port_name(midi_out_port), JACK_jack_port_name(jack_port));

	return 1;
}

static int _jack_stop(struct midi_port *p) {
	jack_port_t* jack_port = (jack_port_t*)p->userdata;

	if (p->io & MIDI_INPUT)
		return !JACK_jack_disconnect(client, JACK_jack_port_name(jack_port), JACK_jack_port_name(midi_in_port));
	else if (p->io & MIDI_OUTPUT)
		return !JACK_jack_disconnect(client, JACK_jack_port_name(midi_out_port), JACK_jack_port_name(jack_port));

	return 1;
}

/* gets called by JACK in a separate thread */
static int _jack_process(jack_nframes_t nframes, void* user_data) {
	struct midi_provider* p = (struct midi_provider*)user_data;

	if (p->cancelled)
		return 1;

	/* handle midi in */
	void* midi_in_buffer = JACK_jack_port_get_buffer(midi_in_port, nframes);
	const jack_nframes_t count = JACK_jack_midi_get_event_count(midi_in_buffer);

	for (jack_nframes_t i = 0; i < count; i++) {
		jack_midi_event_t event = {0};
		if (JACK_jack_midi_event_get(&event, midi_in_buffer, i))
			break;

		/* TODO: is this real-time like JACK wants it to be? or do we have to make
		 * another ringbuffer for this */
		midi_received_cb(NULL, event.buffer, event.size);
	}

	/* handle midi out */
	void* midi_out_buffer = JACK_jack_port_get_buffer(midi_out_port, nframes);

	JACK_jack_midi_clear_buffer(midi_out_buffer);

	unsigned int size = 0;

	while (JACK_jack_ringbuffer_peek(ringbuffer, (char*)&size, sizeof(size)) == sizeof(size)
	       && JACK_jack_ringbuffer_read_space(ringbuffer) >= sizeof(size) + size) {
		JACK_jack_ringbuffer_read_advance(ringbuffer, sizeof(size));

		jack_midi_data_t* data = JACK_jack_midi_event_reserve(midi_out_buffer, 0, size);

		/* XXX this doesn't fucking work and I don't know why. This is what RtMidi does
		 * so it's definitely how it's *supposed* to work.
		 *
		 * I digress... */
		if (data) return (JACK_jack_ringbuffer_read(ringbuffer, (char*)data, size) != size);
		else JACK_jack_ringbuffer_read_advance(ringbuffer, size);
	}

	return 0;
}

/* inout for these functions should be EITHER MIDI_INPUT or MIDI_OUTPUT, never both.
 * jack has no concept of duplex ports */
static struct midi_port* _jack_is_name_in_midi_ports(struct midi_provider* p, const char* name, int inout) {
	struct midi_port* ptr = NULL;
	while (midi_port_foreach(p, &ptr)) {
		if (!(ptr->iocap & inout))
			continue;

		jack_port_t* port = (jack_port_t*)ptr->userdata;

		if (JACK_jack_port_by_name(client, name) == port)
			return ptr;
	}

	return NULL;
}

static const char* _jack_is_port_in_name_list(const char** names, struct midi_port* ptr, int inout) {
	jack_port_t* m = (jack_port_t*)ptr->userdata;
	const char** name;

	if (!(ptr->iocap & inout))
		return NULL;

	for (name = names; *name; name++)
		if (JACK_jack_port_by_name(client, *name) == m)
			return *name;

	return NULL;
}

static void _jack_enumerate_ports(const char** port_names, struct midi_provider* p, int inout) {
	struct midi_port* ptr = NULL;
	while (midi_port_foreach(p, &ptr))
		if ((ptr->iocap & inout) && !_jack_is_port_in_name_list(port_names, ptr, inout))
			midi_port_unregister(ptr->num);

	const char** port_name = NULL;

	/* THEN search for new ports to insert */
	for (port_name = port_names; *port_name; port_name++) {
		jack_port_t* port = JACK_jack_port_by_name(client, *port_name);
		if (_jack_is_name_in_midi_ports(p, *port_name, inout)
			|| JACK_jack_port_is_mine(client, port))
			continue;

		/* create the UI name... */
		char ptr[55];
		snprintf(ptr, 55, " %-*.*s (JACK)", 55 - 9, 55 - 9, *port_name);

		midi_port_register(p, inout, ptr, port, 0);
	}
}

static void _jack_shutdown_callback(void* arg) {
	/* we don't need extra info, the building is being demolished */
	SDL_LockMutex(client_mutex);
	client = NULL;
	SDL_UnlockMutex(client_mutex);
}

static int _jack_attempt_connect(struct midi_provider* jack_provider_) {
	SDL_LockMutex(client_mutex);

	if (client) {
		SDL_UnlockMutex(client_mutex);
		return 1;
	}

	/* create our client */
	client = JACK_jack_client_open(PORT_NAME, JackNoStartServer, NULL);
	if (!client) {
		SDL_UnlockMutex(client_mutex);
		return 0;
	}

	JACK_jack_on_shutdown(client, _jack_shutdown_callback, NULL);

	midi_in_port = JACK_jack_port_register(client, "MIDI In", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
	if (!midi_in_port) {
		JACK_jack_client_close(client);
		client = NULL;
		SDL_UnlockMutex(client_mutex);
		return 0;
	}

	midi_out_port = JACK_jack_port_register(client, "MIDI Out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
	if (!midi_out_port) {
		JACK_jack_port_unregister(client, midi_in_port);
		JACK_jack_client_close(client);
		client = NULL;
		SDL_UnlockMutex(client_mutex);
		return 0;
	}

	/* hand this over to JACK */
	if (JACK_jack_set_process_callback(client, _jack_process, (void*)jack_provider_)) {
		JACK_jack_port_unregister(client, midi_in_port);
		JACK_jack_port_unregister(client, midi_out_port);
		JACK_jack_client_close(client);
		client = NULL;
		SDL_UnlockMutex(client_mutex);
		return 0;
	}

	if (JACK_jack_activate(client)) {
		JACK_jack_port_unregister(client, midi_in_port);
		JACK_jack_port_unregister(client, midi_out_port);
		JACK_jack_client_close(client);
		client = NULL;
		SDL_UnlockMutex(client_mutex);
		return 0;
	}

	SDL_UnlockMutex(client_mutex);
	return 1;
}

static void _jack_poll(struct midi_provider* jack_provider_)
{
	if (!_jack_attempt_connect(jack_provider_))
		return;

	SDL_LockMutex(client_mutex);

	const char** ports = NULL;

	ports = JACK_jack_get_ports(client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput);
	_jack_enumerate_ports(ports, jack_provider_, MIDI_INPUT);
	free(ports);

	ports = JACK_jack_get_ports(client, NULL, JACK_DEFAULT_MIDI_TYPE, JackPortIsInput);
	_jack_enumerate_ports(ports, jack_provider_, MIDI_OUTPUT);
	free(ports);

	SDL_UnlockMutex(client_mutex);
}

int jack_midi_setup(void)
{
	static struct midi_driver driver;

#ifdef JACK_DYNAMIC_LOAD
	if (!jack_dltrick_handle_)
#endif
		if (jack_dlinit())
			return 0;

	driver.poll = _jack_poll;
	driver.thread = NULL;
	driver.enable = _jack_start;
	driver.disable = _jack_stop;
	driver.send = _jack_send;

	client_mutex = SDL_CreateMutex();

	ringbuffer = JACK_jack_ringbuffer_create(JACK_RINGBUFFER_SIZE);
	if (!ringbuffer)
		return 0;

	ringbuffer_max_write = JACK_jack_ringbuffer_write_space(ringbuffer);

	struct midi_provider* p = midi_provider_register("JACK-MIDI", &driver);
	if (!p) {
		/* how? can this check be removed? */
		JACK_jack_ringbuffer_free(ringbuffer);
		return 0;
	}

	return 1;
}
