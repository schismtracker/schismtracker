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

static int load_jack_syms(void);

#ifdef JACK_DYNAMIC_LOAD

#include <dlfcn.h>

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

int jack_dlinit(void) {
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

	return 0;
}

/* ------------------------------------------------------ */

/* there should only ever be one of these */
static jack_client_t* client = NULL;

static void _jack_send(struct midi_port *p, const unsigned char *data, unsigned int len, unsigned int delay) {
	/* todo */
}

static int _jack_start(UNUSED struct midi_port *p) {
	return 1; /* can't really do this */
}

static int _jack_stop(UNUSED struct midi_port *p) {
	return 1; /* ditto */
}

/* gets called by JACK in a separate thread */
int _jack_process(jack_nframes_t nframes, void* user_data) {
	struct midi_provider* p = (struct midi_provider*)user_data;

	if (p->cancelled)
		return 0;

	struct midi_port* ptr = NULL;
	while (midi_port_foreach(p, &ptr)) {
		jack_port_t* port = (jack_port_t*)ptr->userdata;
		void* port_buffer = JACK_jack_port_get_buffer(port, nframes);

		if (ptr->io & MIDI_INPUT) {
			const jack_nframes_t count = JACK_jack_midi_get_event_count(port_buffer);

			for (jack_nframes_t i = 0; i < count; i++) {
				jack_midi_event_t event = {0};
				if (JACK_jack_midi_event_get(&event, port_buffer, i))
					break;

				midi_received_cb(ptr, event.buffer, event.size);
			}
		} else if (ptr->io & MIDI_OUTPUT) {
			/* todo */
		}
	}

	return 0;
}

static void _jack_poll(struct midi_provider* jack_provider_)
{
	/* do nothing */
}


int jack_midi_setup(void)
{
	static struct midi_driver driver;

#ifdef JACK_DYNAMIC_LOAD
	if (!jack_dltrick_handle_)
		if (jack_dlinit())
			return 0;
#endif

	if (client)
		return 0; /* don't init this twice */

	/* create our client */
	client = JACK_jack_client_open(PORT_NAME, JackNullOption, NULL);
	if (!client)
		return 0; /* welp */

	driver.poll = _jack_poll;
	driver.thread = NULL;
	driver.enable = _jack_start;
	driver.disable = _jack_stop;
	driver.send = _jack_send;
	//driver.flags = MIDI_PORT_CAN_SCHEDULE;
	//driver.drain = _jack_drain;

	struct midi_provider* p = midi_provider_register("JACK-MIDI", &driver);
	if (!p) {
		/* better luck next time */
		JACK_jack_client_close(client);
		client = NULL;
		return 0;
	}

	/* hand this over to JACK */
	if (JACK_jack_set_process_callback(client, _jack_process, (void*)p)) {
		JACK_jack_client_close(client);
		client = NULL;
		return 0;
	}

	if (JACK_jack_activate(client)) {
		JACK_jack_client_close(client);
		client = NULL;
		return 0;
	}

	/* ... */
	jack_port_t* input = JACK_jack_port_register(client, "MIDI In", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
	if (!input) {
		JACK_jack_client_close(client);
		client = NULL;
		return 0;
	}

#if 0
	jack_port_t* output = JACK_jack_port_register(client, "MIDI Out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
	if (!output) {
		jack_port_unregister(client, input);
		jack_client_close(client);
		client = NULL;
		return 0;
	}
#endif

	midi_port_register(p, MIDI_INPUT,  " MIDI In          (JACK)", input, 0);
	/* midi_port_register(p, MIDI_OUTPUT, " MIDI Out         (JACK)", output, 0); */

	return 1;
}
