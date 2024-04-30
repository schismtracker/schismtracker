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

/* TODO: use something similar to the ALSA code and dlsym in the
 * functions we need
*/

#include "headers.h"

#include "it.h"
#include "midi.h"

#include "util.h"

#include <jack/jack.h>
#include <jack/midiport.h>

#define PORT_NAME "Schism Tracker"

/* there should only ever be one of these */
static jack_client_t* client = NULL;

static void _jack_send(struct midi_port *p, const unsigned char *data, unsigned int len, unsigned int delay) {
#if 0
	SDL_LockMutex(midi_queue_mutex);

	if (midi_queue_pos < ARRAY_SIZE(midi_queue))
		memcpy(midi_queue + midi_queue_pos, data, MIN(ARRAY_SIZE(midi_queue) - midi_queue_pos, len));

	midi_queue_pos += len;

	SDL_UnlockMutex(midi_queue_mutex);
#endif
}

static int _jack_start(UNUSED struct midi_port *p) {
	return 1; /* can't really do this */
}

static int _jack_stop(UNUSED struct midi_port *p) {
	return 1; /* ditto */
}

/* gets called by JACK in a separate thread */
static int _jack_process(jack_nframes_t nframes, void* user_data) {
	struct midi_provider* p = (struct midi_provider*)user_data;

	if (p->cancelled)
		return 0;

	struct midi_port* ptr = NULL;
	while (midi_port_foreach(p, &ptr)) {
		jack_port_t* port = (jack_port_t*)ptr->userdata;
		void* port_buffer = jack_port_get_buffer(port, nframes);

		if (ptr->io & MIDI_INPUT) {
			const jack_nframes_t count = jack_midi_get_event_count(port_buffer);

			for (jack_nframes_t i = 0; i < count; i++) {
				jack_midi_event_t event = {0};
				if (jack_midi_event_get(&event, port_buffer, i))
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

	if (client)
		return 0; /* don't init this twice */

	/* create our client */
	client = jack_client_open(PORT_NAME, JackNullOption, NULL);
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
		jack_client_close(client);
		client = NULL;
		return 0;
	}

	/* hand this over to JACK */
	if (jack_set_process_callback(client, _jack_process, (void*)p)) {
		jack_client_close(client);
		client = NULL;
		return 0;
	}

	if (jack_activate(client)) {
		jack_client_close(client);
		client = NULL;
		return 0;
	}

	/* ... */
	jack_port_t* input = jack_port_register(client, "MIDI In", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
	if (!input) {
		jack_client_close(client);
		client = NULL;
		return 0;
	}

#if 0
	jack_port_t* output = jack_port_register(client, "MIDI Out", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
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
