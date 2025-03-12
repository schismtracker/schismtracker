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

#ifndef SCHISM_MIDI_H_
#define SCHISM_MIDI_H_

struct midi_provider;
struct midi_port;

#define MIDI_PORT_CAN_SCHEDULE  1
struct midi_driver {
	uint32_t flags;

	void (*poll)(struct midi_provider *m);
	int (*thread)(struct midi_provider *m);

	int (*enable)(struct midi_port *d);
	int (*disable)(struct midi_port *d);

	void (*send)(struct midi_port *d,
			const unsigned char *seq, uint32_t len, uint32_t delay);
	void (*drain)(struct midi_port *d);
};

// implemented in each backend
struct mt_thread;

struct midi_provider {
	char *name;
	void (*poll)(struct midi_provider *);
	struct mt_thread *thread;
	volatile int cancelled;

	struct midi_provider *next;

	/* forwarded; don't touch */
	int (*enable)(struct midi_port *d);
	int (*disable)(struct midi_port *d);

	void (*send_now)(struct midi_port *d,
			const unsigned char *seq, uint32_t len, uint32_t delay);
	void (*send_later)(struct midi_port *d,
			const unsigned char *seq, uint32_t len, uint32_t delay);
	void (*drain)(struct midi_port *d);
};

enum {
	MIDI_INPUT = 1,
	MIDI_OUTPUT = 2,
};

struct midi_port {
	int io, iocap;
	char *name;
	int num;

	void *userdata;
	int free_userdata;
	int (*enable)(struct midi_port *d);
	int (*disable)(struct midi_port *d);
	void (*send_now)(struct midi_port *d,
			const unsigned char *seq, uint32_t len, uint32_t delay);
	void (*send_later)(struct midi_port *d,
			const unsigned char *seq, uint32_t len, uint32_t delay);
	void (*drain)(struct midi_port *d);

	struct midi_provider *provider;
};


/* schism calls these directly */
int midi_engine_start(void);
void midi_engine_reset(void);
void midi_engine_stop(void);
void midi_engine_poll_ports(void);

/* some parts of schism call this; it means "immediately" */
void midi_send_now(const unsigned char *seq, uint32_t len);

/* ... but the player calls this */
void midi_send_buffer(const unsigned char *data, uint32_t len, uint32_t pos);
void midi_send_flush(void);

/* used by the audio thread */
int midi_need_flush(void);

/* from Schism event handler */
union schism_event;
int midi_engine_handle_event(union schism_event *ev);

struct midi_port *midi_engine_port(int n, const char **name);
int midi_engine_port_count(void);

/* midi engines register a provider (one each!) */
struct midi_provider *midi_provider_register(const char *name, const struct midi_driver *f);

/* midi engines list ports this way */
int midi_port_register(struct midi_provider *p,
int inout, const char *name, void *userdata, int free_userdata);

int midi_port_foreach(struct midi_provider *p, struct midi_port **cursor);
void midi_port_unregister(int num);

int midi_port_enable(struct midi_port *p);
int midi_port_disable(struct midi_port *p);

/* only call these if the event isn't really MIDI but you want most of the system
   to act like it is...

   midi drivers should never all these...
*/
enum midi_note {
	MIDI_NOTEOFF,
	MIDI_NOTEON,
	MIDI_KEYPRESS,
};
void midi_event_note(enum midi_note mnstatus, int channel, int note, int velocity);
void midi_event_controller(int channel, int param, int value);
void midi_event_program(int channel, int value);
void midi_event_aftertouch(int channel, int value);
void midi_event_pitchbend(int channel, int value);
void midi_event_tick(void);
void midi_event_sysex(const unsigned char *data, uint32_t len);
void midi_event_system(int argv, int param);

/* midi drivers call this when they received an event */
void midi_received_cb(struct midi_port *src, unsigned char *data, uint32_t len);

// lost child
uint8_t midi_event_length(uint8_t first_byte);

int ip_midi_setup(void);        // USE_NETWORK
void ip_midi_setports(int n);   // USE_NETWORK
int ip_midi_getports(void);     // USE_NETWORK

int oss_midi_setup(void);       // USE_OSS
int alsa_midi_setup(void);      // USE_ALSA
int jack_midi_setup(void);      // USE_JACK
int win32mm_midi_setup(void);   // SCHISM_WIN32
int macosx_midi_setup(void);    // SCHISM_MACOSX

/* called by audio system when buffer stuff change */
void midi_queue_alloc(int buffer_size, int channels, int samples_per_second);

/* MIDI_PITCH_BEND is defined by OSS -- maybe these need more specific names? */
#define MIDI_TICK_QUANTIZE      0x00000001
#define MIDI_BASE_PROGRAM1      0x00000002
#define MIDI_RECORD_NOTEOFF     0x00000004
#define MIDI_RECORD_VELOCITY    0x00000008
#define MIDI_RECORD_AFTERTOUCH  0x00000010
#define MIDI_CUT_NOTE_OFF       0x00000020
#define MIDI_PITCHBEND          0x00000040
#define MIDI_DISABLE_RECORD     0x00010000

extern int midi_flags, midi_pitch_depth, midi_amplification, midi_c5note;

#endif /* SCHISM_MIDI_H_ */
