/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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

#ifndef MIDI_H
#define MIDI_H

#ifdef __cplusplus
extern "C" {
#endif

struct midi_provider;
struct midi_port;

struct midi_driver {
	unsigned int flags;
#define MIDI_PORT_CAN_SCHEDULE	1

	void (*poll)(struct midi_provider *m);
	int (*thread)(struct midi_provider *m);

	int (*enable)(struct midi_port *d);
	int (*disable)(struct midi_port *d);

	void (*send)(struct midi_port *d,
			const unsigned char *seq, unsigned int len, unsigned int delay);
	void (*drain)(struct midi_port *d);
};

struct midi_provider {
	const char *name;
	void (*poll)(struct midi_provider *);
	void *thread; /*actually SDL_Thread* */

	struct midi_provider *next;

	/* forwarded; don't touch */
	int (*enable)(struct midi_port *d);
	int (*disable)(struct midi_port *d);

	void (*send_now)(struct midi_port *d,
			const unsigned char *seq, unsigned int len, unsigned int delay);
	void (*send_later)(struct midi_port *d,
			const unsigned char *seq, unsigned int len, unsigned int delay);
	void (*drain)(struct midi_port *d);
};
struct midi_port {
	int io, iocap;
#define MIDI_INPUT	1
#define MIDI_OUTPUT	2
	char *name;
	int num;

	void *userdata;
	int free_userdata;
	int (*enable)(struct midi_port *d);
	int (*disable)(struct midi_port *d);
	void (*send_now)(struct midi_port *d,
			const unsigned char *seq, unsigned int len, unsigned int delay);
	void (*send_later)(struct midi_port *d,
			const unsigned char *seq, unsigned int len, unsigned int delay);
	void (*drain)(struct midi_port *d);

	struct midi_provider *provider;
};


/* schism calls these directly */
int midi_engine_start(void);
void midi_engine_reset(void);
void midi_engine_stop(void);
void midi_engine_poll_ports(void);

/* some parts of schism call this; it means "immediately" */
void midi_send_now(const unsigned char *seq, unsigned int len);

/* ... but the player calls this */
void midi_send_buffer(const unsigned char *data, unsigned int len, unsigned int pos);
void midi_send_flush(void);

/* used by the audio thread */
int midi_need_flush(void);

/* from the SDL event mechanism (x is really SDL_Event) */
int midi_engine_handle_event(void *x);

struct midi_port *midi_engine_port(int n, const char **name);
int midi_engine_port_count(void);

/* midi engines register a provider (one each!) */
struct midi_provider *midi_provider_register(const char *name, struct midi_driver *f);
		

/* midi engines list ports this way */
int midi_port_register(struct midi_provider *p,
int inout, const char *name, void *userdata, int free_userdata);

int midi_port_foreach(struct midi_provider *p, struct midi_port **cursor);
void midi_port_unregister(int num);

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
void midi_event_sysex(const unsigned char *data, unsigned int len);
void midi_event_system(int argv, int param);

/* midi drivers call this when they received an event */
void midi_received_cb(struct midi_port *src, unsigned char *data, unsigned int len);


#ifdef USE_NETWORK
int ip_midi_setup(void);
#endif
#ifdef USE_OSS
int oss_midi_setup(void);
#endif
#ifdef USE_ALSA
int alsa_midi_setup(void);
#endif
#ifdef USE_WIN32MM
int win32mm_midi_setup(void);
#endif
#ifdef MACOSX
int macosx_midi_setup(void);
#endif



#define MIDI_TICK_QUANTIZE	0x00000001
#define MIDI_BASE_PROGRAM1	0x00000002
#define MIDI_RECORD_NOTEOFF	0x00000004
#define MIDI_RECORD_VELOCITY	0x00000008
#define MIDI_RECORD_AFTERTOUCH	0x00000010
#define MIDI_CUT_NOTE_OFF	0x00000020
#define MIDI_PITCH_BEND		0x00000040
#define MIDI_EMBED_DATA		0x00000080
#define MIDI_RECORD_SDX		0x00000100
#define MIDI_DISABLE_RECORD	0x00010000

extern int midi_flags, midi_pitch_depth, midi_amplification, midi_c5note;

/* only available with networks */
void ip_midi_setports(int n);
int ip_midi_getports(void);

#ifdef __cplusplus
};
#endif

#endif
