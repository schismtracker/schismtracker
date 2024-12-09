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

#ifdef USE_ALSA
#include <sys/poll.h>

#include <errno.h>

#include <alsa/asoundlib.h>

#include <sys/stat.h>

#define PORT_NAME       "Schism Tracker"


static snd_seq_t *seq;
static int local_port = -1;

#define MIDI_BUFSIZE    65536
static unsigned char big_midi_buf[MIDI_BUFSIZE];
static int alsa_queue;

struct alsa_midi {
	int c, p;
	const char *client;
	const char *port;
	snd_midi_event_t *dev;
	int mark;
};

static size_t (*ALSA_snd_seq_port_info_sizeof)(void);
static size_t (*ALSA_snd_seq_client_info_sizeof)(void);
static size_t (*ALSA_snd_seq_queue_tempo_sizeof)(void);
static int (*ALSA_snd_seq_control_queue)(snd_seq_t*s,int q,int type, int value, snd_seq_event_t *ev);
static void (*ALSA_snd_seq_queue_tempo_set_tempo)(snd_seq_queue_tempo_t *info, unsigned int tempo);
static void (*ALSA_snd_seq_queue_tempo_set_ppq)(snd_seq_queue_tempo_t *info, int ppq);
static int (*ALSA_snd_seq_set_queue_tempo)(snd_seq_t *handle, int q, snd_seq_queue_tempo_t *tempo);
static long (*ALSA_snd_midi_event_encode)(snd_midi_event_t *dev,const unsigned char *buf,long count,snd_seq_event_t *ev);
static int (*ALSA_snd_seq_event_output)(snd_seq_t *handle, snd_seq_event_t *ev);
static int (*ALSA_snd_seq_alloc_queue)(snd_seq_t*h);
static int (*ALSA_snd_seq_free_event)(snd_seq_event_t *ev);
static int (*ALSA_snd_seq_connect_from)(snd_seq_t*seeq,int my_port,int src_client, int src_port);
static int (*ALSA_snd_seq_connect_to)(snd_seq_t*seeq,int my_port,int dest_client,int dest_port);
static int (*ALSA_snd_seq_disconnect_from)(snd_seq_t*seeq,int my_port,int src_client, int src_port);
static int (*ALSA_snd_seq_disconnect_to)(snd_seq_t*seeq,int my_port,int dest_client,int dest_port);
static const char * (*ALSA_snd_strerror)(int errnum);
static int (*ALSA_snd_seq_poll_descriptors_count)(snd_seq_t*h,short e);
static int (*ALSA_snd_seq_poll_descriptors)(snd_seq_t*h,struct pollfd*pfds,unsigned int space, short e);
static int (*ALSA_snd_seq_event_input)(snd_seq_t*h,snd_seq_event_t**ev);
static int (*ALSA_snd_seq_event_input_pending)(snd_seq_t*h,int fs);
static int (*ALSA_snd_midi_event_new)(size_t s,snd_midi_event_t **rd);
static long (*ALSA_snd_midi_event_decode)(snd_midi_event_t *dev,unsigned char *buf,long count, const snd_seq_event_t*ev);
static void (*ALSA_snd_midi_event_reset_decode)(snd_midi_event_t*d);
static int (*ALSA_snd_seq_create_simple_port)(snd_seq_t*h,const char *name,unsigned int caps,unsigned int type);
static int (*ALSA_snd_seq_drain_output)(snd_seq_t*h);
static int (*ALSA_snd_seq_query_next_client)(snd_seq_t*h,snd_seq_client_info_t*info);
static int (*ALSA_snd_seq_client_info_get_client)(const snd_seq_client_info_t *info);
static void (*ALSA_snd_seq_client_info_set_client)(snd_seq_client_info_t*inf,int cl);
static void (*ALSA_snd_seq_port_info_set_client)(snd_seq_port_info_t*inf,int cl);
static void (*ALSA_snd_seq_port_info_set_port)(snd_seq_port_info_t*inf,int pl);
static int (*ALSA_snd_seq_query_next_port)(snd_seq_t*h,snd_seq_port_info_t*inf);
static unsigned int (*ALSA_snd_seq_port_info_get_capability)(const snd_seq_port_info_t *inf);
static int (*ALSA_snd_seq_port_info_get_client)(const snd_seq_port_info_t*inf);
static int (*ALSA_snd_seq_port_info_get_port)(const snd_seq_port_info_t*inf);
static const char * (*ALSA_snd_seq_client_info_get_name)(snd_seq_client_info_t*inf);
static const char * (*ALSA_snd_seq_port_info_get_name)(const snd_seq_port_info_t*inf);
static int (*ALSA_snd_seq_open)(snd_seq_t**h,const char *name,int str, int mode);
static int (*ALSA_snd_seq_set_client_name)(snd_seq_t*seeq,const char *name);

/* these are inline functions; prefix them anyway */
#define ALSA_snd_seq_ev_clear snd_seq_ev_clear
#define ALSA_snd_seq_ev_set_source snd_seq_ev_set_source
#define ALSA_snd_seq_ev_set_subs snd_seq_ev_set_subs
#define ALSA_snd_seq_ev_set_direct snd_seq_ev_set_direct
#define ALSA_snd_seq_ev_schedule_tick snd_seq_ev_schedule_tick
#define ALSA_snd_seq_client_info_alloca snd_seq_client_info_alloca
#define ALSA_snd_seq_port_info_alloca snd_seq_port_info_alloca
#define ALSA_snd_seq_queue_tempo_alloca snd_seq_queue_tempo_alloca
#define ALSA_snd_seq_start_queue snd_seq_start_queue

static int load_alsa_syms(void);

#ifdef ALSA_DYNAMIC_LOAD

#include "loadso.h"

/* said inline functions call these... */
#define snd_seq_client_info_sizeof ALSA_snd_seq_client_info_sizeof
#define snd_seq_port_info_sizeof ALSA_snd_seq_port_info_sizeof
#define snd_seq_queue_tempo_sizeof ALSA_snd_seq_queue_tempo_sizeof
#define snd_seq_control_queue ALSA_snd_seq_control_queue

void *alsa_dltrick_handle_;

static void alsa_dlend(void) {
	if (alsa_dltrick_handle_) {
		loadso_object_unload(alsa_dltrick_handle_);
		alsa_dltrick_handle_ = NULL;
	}
}

static int alsa_dlinit(void) {
	if (alsa_dltrick_handle_)
		return 0;

	// libasound.so.2
	alsa_dltrick_handle_ = library_load("asound", 2, 0);
	if (!alsa_dltrick_handle_)
		return -1;

	int retval = load_alsa_syms();
	if (retval < 0)
		alsa_dlend();

	return retval;
}

SCHISM_STATIC_ASSERT(sizeof(void (*)) == sizeof(void *), "dynamic loading code assumes function pointer and void pointer are of equivalent size");

static int load_alsa_sym(const char *fn, void *addr) {
	void *func = loadso_function_load(alsa_dltrick_handle_, fn);
	if (!func)
		return 0;

	memcpy(addr, &func, sizeof(void *));

	return 1;
}

#define SCHISM_ALSA_SYM(x) \
	if (!load_alsa_sym(#x, &ALSA_##x)) return -1

#else

#define SCHISM_ALSA_SYM(x) ALSA_##x = x

static int alsa_dlinit(void) {
	return load_alsa_syms();
}

#endif

static int load_alsa_syms(void) {
	SCHISM_ALSA_SYM(snd_seq_port_info_sizeof);
	SCHISM_ALSA_SYM(snd_seq_client_info_sizeof);
	SCHISM_ALSA_SYM(snd_seq_control_queue);
	SCHISM_ALSA_SYM(snd_seq_queue_tempo_sizeof);
	SCHISM_ALSA_SYM(snd_seq_queue_tempo_set_tempo);
	SCHISM_ALSA_SYM(snd_seq_queue_tempo_set_ppq);
	SCHISM_ALSA_SYM(snd_seq_set_queue_tempo);
	SCHISM_ALSA_SYM(snd_midi_event_encode);
	SCHISM_ALSA_SYM(snd_seq_event_output);
	SCHISM_ALSA_SYM(snd_seq_alloc_queue);
	SCHISM_ALSA_SYM(snd_seq_free_event);
	SCHISM_ALSA_SYM(snd_seq_connect_from);
	SCHISM_ALSA_SYM(snd_seq_connect_to);
	SCHISM_ALSA_SYM(snd_seq_disconnect_from);
	SCHISM_ALSA_SYM(snd_seq_disconnect_to);
	SCHISM_ALSA_SYM(snd_strerror);
	SCHISM_ALSA_SYM(snd_seq_poll_descriptors_count);
	SCHISM_ALSA_SYM(snd_seq_poll_descriptors);
	SCHISM_ALSA_SYM(snd_seq_event_input);
	SCHISM_ALSA_SYM(snd_seq_event_input_pending);
	SCHISM_ALSA_SYM(snd_midi_event_new);
	SCHISM_ALSA_SYM(snd_midi_event_decode);
	SCHISM_ALSA_SYM(snd_midi_event_reset_decode);
	SCHISM_ALSA_SYM(snd_seq_create_simple_port);
	SCHISM_ALSA_SYM(snd_seq_drain_output);
	SCHISM_ALSA_SYM(snd_seq_query_next_client);
	SCHISM_ALSA_SYM(snd_seq_client_info_get_client);
	SCHISM_ALSA_SYM(snd_seq_client_info_set_client);
	SCHISM_ALSA_SYM(snd_seq_port_info_set_client);
	SCHISM_ALSA_SYM(snd_seq_port_info_set_port);
	SCHISM_ALSA_SYM(snd_seq_query_next_port);
	SCHISM_ALSA_SYM(snd_seq_port_info_get_capability);
	SCHISM_ALSA_SYM(snd_seq_port_info_get_client);
	SCHISM_ALSA_SYM(snd_seq_port_info_get_port);
	SCHISM_ALSA_SYM(snd_seq_client_info_get_name);
	SCHISM_ALSA_SYM(snd_seq_port_info_get_name);
	SCHISM_ALSA_SYM(snd_seq_open);
	SCHISM_ALSA_SYM(snd_seq_set_client_name);

	return 0;
}

/* see mixer-alsa.c */
#undef assert
#define assert(x)

static void _alsa_drain(struct midi_port *p SCHISM_UNUSED)
{
	/* not port specific */
	ALSA_snd_seq_drain_output(seq);
}

static void _alsa_send(struct midi_port *p, const unsigned char *data, unsigned int len, unsigned int delay)
{
	struct alsa_midi *ex;
	snd_seq_event_t ev;
	long rr;

	ex = (struct alsa_midi *)p->userdata;

	while (len > 0) {
		ALSA_snd_seq_ev_clear(&ev);
		ALSA_snd_seq_ev_set_source(&ev, local_port);
		ALSA_snd_seq_ev_set_subs(&ev);
		if (!delay) {
			ALSA_snd_seq_ev_set_direct(&ev);
		} else {
			ALSA_snd_seq_ev_schedule_tick(&ev, alsa_queue, 1, delay);
		}

		/* we handle our own */
		ev.dest.port = ex->p;
		ev.dest.client = ex->c;

		rr = ALSA_snd_midi_event_encode(ex->dev, data, len, &ev);
		if (rr < 1) break;
		ALSA_snd_seq_event_output(seq, &ev);
		ALSA_snd_seq_free_event(&ev);
		data += rr;
		len -= rr;
	}
}

static int _alsa_start(struct midi_port *p)
{
	struct alsa_midi *data;
	int err;

	err = 0;
	data = (struct alsa_midi *)p->userdata;

	if (p->io & MIDI_INPUT)
		err = ALSA_snd_seq_connect_from(seq, 0, data->c, data->p);

	if (p->io & MIDI_OUTPUT)
		err = ALSA_snd_seq_connect_to(seq, 0, data->c, data->p);

	if (err < 0) {
		log_appendf(4, "ALSA: %s", ALSA_snd_strerror(err));
		return 0;
	}
	return 1;
}

static int _alsa_stop(struct midi_port *p)
{
	struct alsa_midi *data;
	int err;

	err = 0;
	data = (struct alsa_midi *)p->userdata;
	if (p->io & MIDI_OUTPUT)
		err = ALSA_snd_seq_disconnect_to(seq, 0, data->c, data->p);

	if (p->io & MIDI_INPUT)
		err = ALSA_snd_seq_disconnect_from(seq, 0, data->c, data->p);

	if (err < 0) {
		log_appendf(4, "ALSA: %s", ALSA_snd_strerror(err));
		return 0;
	}
	return 1;
}

static int _alsa_thread(struct midi_provider *p)
{
	int npfd;
	struct pollfd *pfd;
	struct midi_port *ptr, *src;
	struct alsa_midi *data;
	static snd_midi_event_t *dev = NULL;
	snd_seq_event_t *ev;
	long s;

	npfd = ALSA_snd_seq_poll_descriptors_count(seq, POLLIN);
	if (npfd <= 0) return 0;

	pfd = (struct pollfd *)mem_alloc(npfd * sizeof(struct pollfd));
	if (!pfd) return 0;

	while (!p->cancelled) {
		if (ALSA_snd_seq_poll_descriptors(seq, pfd, npfd, POLLIN) != npfd) {
			free(pfd);
			return 0;
		}

		(void)poll(pfd, npfd, -1);
		do {
			if (ALSA_snd_seq_event_input(seq, &ev) < 0) {
				break;
			}
			if (!ev) continue;

			ptr = src = NULL;
			while (midi_port_foreach(p, &ptr)) {
				data = (struct alsa_midi *)ptr->userdata;
				if (ev->source.client == data->c
				&& ev->source.port == data->p
				&& (ptr->io & MIDI_INPUT)) {
					src = ptr;
				}
			}
			if (!src || !ev) {
				ALSA_snd_seq_free_event(ev);
				continue;
			}

			if (!dev && ALSA_snd_midi_event_new(sizeof(big_midi_buf), &dev) < 0) {
				/* err... */
				break;
			}

			s = ALSA_snd_midi_event_decode(dev, big_midi_buf,
					sizeof(big_midi_buf), ev);
			if (s > 0) midi_received_cb(src, big_midi_buf, s);
			ALSA_snd_midi_event_reset_decode(dev);
			ALSA_snd_seq_free_event(ev);
		} while (ALSA_snd_seq_event_input_pending(seq, 0) > 0);
//              snd_seq_drain_output(seq);
	}
	return 0;
}

static void _alsa_poll(struct midi_provider *_alsa_provider)
{
	struct midi_port *ptr;
	struct alsa_midi *data;
	char *buffer;
	int c, p, ok, io;
	const char *ctext, *ptext;

	snd_seq_client_info_t *cinfo;
	snd_seq_port_info_t *pinfo;

	ptr = NULL;
	while (midi_port_foreach(_alsa_provider, &ptr)) {
		data = (struct alsa_midi *)ptr->userdata;
		data->mark = 0;
	}

	/* believe it or not this is the RIGHT way to do this */
	ALSA_snd_seq_client_info_alloca(&cinfo);
	ALSA_snd_seq_port_info_alloca(&pinfo);

	ALSA_snd_seq_client_info_set_client(cinfo, -1);
	while (ALSA_snd_seq_query_next_client(seq, cinfo) >= 0) {
		int cn = ALSA_snd_seq_client_info_get_client(cinfo);
		ALSA_snd_seq_port_info_set_client(pinfo, cn);
		ALSA_snd_seq_port_info_set_port(pinfo, -1);
		while (ALSA_snd_seq_query_next_port(seq, pinfo) >= 0) {
			io = 0;

			if ((ALSA_snd_seq_port_info_get_capability(pinfo)
			 & (SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ))
			== (SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ))
				io |= MIDI_INPUT;

			if ((ALSA_snd_seq_port_info_get_capability(pinfo)
			 & (SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE))
			== (SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE))
				io |= MIDI_OUTPUT;

			if (!io)
				continue;

			/* Now get clients and ports */
			c = ALSA_snd_seq_port_info_get_client(pinfo);
			if (c == SND_SEQ_CLIENT_SYSTEM) continue;

			p = ALSA_snd_seq_port_info_get_port(pinfo);
			ptr = NULL;
			ok = 0;
			while (midi_port_foreach(_alsa_provider, &ptr)) {
				data = (struct alsa_midi *)ptr->userdata;
				if (data->c == c && data->p == p) {
					data->mark = 1;
					ok = 1;
				}
			}
			if (ok)
				continue;

			ctext = ALSA_snd_seq_client_info_get_name(cinfo);
			ptext = ALSA_snd_seq_port_info_get_name(pinfo);
			if (strcmp(ctext, PORT_NAME) == 0
			&& strcmp(ptext, PORT_NAME) == 0) {
				continue;
			}
			data = mem_alloc(sizeof(struct alsa_midi));
			data->c = c; data->p = p;
			data->client = ctext;
			data->mark = 1;
			data->port = ptext;
			buffer = NULL;

			if (ALSA_snd_midi_event_new(MIDI_BUFSIZE, &data->dev) < 0) {
				/* err... */
				free(data);
				continue;
			}

			if (asprintf(&buffer, "%3d:%-3d %-20.20s %s",
					c, p, ctext, ptext) == -1) {
				free(data);
				continue;
			}

			midi_port_register(_alsa_provider, io, buffer, data, 1);
			free(buffer);
		}
	}

	/* remove "disappeared" midi ports */
	ptr = NULL;
	while (midi_port_foreach(_alsa_provider, &ptr)) {
		data = (struct alsa_midi *)ptr->userdata;
		if (data->mark) continue;
		midi_port_unregister(ptr->num);
	}
}

static struct midi_driver alsa_driver = {
	.poll = _alsa_poll,
	.thread = _alsa_thread,
	.enable = _alsa_start,
	.disable = _alsa_stop,
	.send = _alsa_send,
	.flags = MIDI_PORT_CAN_SCHEDULE,
	.drain = _alsa_drain,
};

int alsa_midi_setup(void)
{
	snd_seq_queue_tempo_t *tempo;

	/* only bother if alsa midi actually exists, otherwise this will
	 * produce useless and annoying error messages on systems where alsa
	 * libs are installed but which aren't actually running it */
	struct stat sbuf;
	if (stat("/dev/snd/seq", &sbuf) != 0)
		return 0;

#ifdef ALSA_DYNAMIC_LOAD
	if (!alsa_dltrick_handle_)
#endif
		if (alsa_dlinit())
			return 0;

	if (ALSA_snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0
	|| ALSA_snd_seq_set_client_name(seq, PORT_NAME) < 0)
		return 0;

	alsa_queue = ALSA_snd_seq_alloc_queue(seq);
	ALSA_snd_seq_queue_tempo_alloca(&tempo);
	ALSA_snd_seq_queue_tempo_set_tempo(tempo, 480000);
	ALSA_snd_seq_queue_tempo_set_ppq(tempo, 480);
	ALSA_snd_seq_set_queue_tempo(seq, alsa_queue, tempo);
	ALSA_snd_seq_start_queue(seq, alsa_queue, NULL);
	ALSA_snd_seq_drain_output(seq);

	local_port = ALSA_snd_seq_create_simple_port(
		seq,
		PORT_NAME,
		SND_SEQ_PORT_CAP_READ
		| SND_SEQ_PORT_CAP_WRITE
		| SND_SEQ_PORT_CAP_SYNC_READ
		| SND_SEQ_PORT_CAP_SYNC_WRITE
		| SND_SEQ_PORT_CAP_DUPLEX
		| SND_SEQ_PORT_CAP_SUBS_READ
		| SND_SEQ_PORT_CAP_SUBS_WRITE,
		SND_SEQ_PORT_TYPE_APPLICATION
		| SND_SEQ_PORT_TYPE_SYNTH
		| SND_SEQ_PORT_TYPE_MIDI_GENERIC
		| SND_SEQ_PORT_TYPE_MIDI_GM
		| SND_SEQ_PORT_TYPE_MIDI_GS
		| SND_SEQ_PORT_TYPE_MIDI_XG
		| SND_SEQ_PORT_TYPE_MIDI_MT32
	);
	if (local_port < 0) {
		log_appendf(4, "ALSA: %s", ALSA_snd_strerror(local_port));
		return 0;
	}

	if (!midi_provider_register("ALSA", &alsa_driver)) return 0;
	return 1;
}


#endif
