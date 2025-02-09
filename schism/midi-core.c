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

#include "backend/threads.h"
#include "events.h"
#include "util.h"
#include "midi.h"
#include "song.h"
#include "page.h"
#include "it.h"
#include "config-parser.h"
#include "config.h"
#include "threads.h"
#include "timer.h"
#include "mem.h"

#include "dmoz.h"

#include <ctype.h>
#include <assert.h>

#ifdef SCHISM_WIN32
# include <windows.h>
#endif

static int _connected = 0;
/* midi_mutex is locked by the main thread,
midi_port_mutex is for the port thread(s),
and midi_record_mutex is by the event/sound thread
*/
static mt_mutex_t *midi_mutex = NULL;
static mt_mutex_t *midi_port_mutex = NULL;
static mt_mutex_t *midi_record_mutex = NULL;
static mt_cond_t *midi_play_cond = NULL;

static struct midi_provider *port_providers = NULL;

/* configurable midi stuff */
int midi_flags = MIDI_TICK_QUANTIZE | MIDI_RECORD_NOTEOFF
		| MIDI_RECORD_VELOCITY | MIDI_RECORD_AFTERTOUCH
		| MIDI_PITCHBEND;

int midi_pitch_depth = 12;
int midi_amplification = 100;
int midi_c5note = 60;

#define CFG_GET_MI(v,d) midi_ ## v = cfg_get_number(cfg, "MIDI", #v, d)


static void _cfg_load_midi_part_locked(struct midi_port *q)
{
	struct cfg_section *c;
	/*struct midi_provider *p;*/
	cfg_file_t cfg;
	const char *sn;
	char *ptr, *ss, *sp;
	char buf[256];
	int j;

	ss = q->name;
	if (!ss) return;
	while (isspace(*ss)) ss++;
	if (!*ss) return;

	sp = q->provider ? q->provider->name : NULL;
	if (sp) {
		while (isspace(*sp)) sp++;
		if (!*sp) sp = NULL;
	}

	ptr = dmoz_path_concat(cfg_dir_dotschism, "config");
	cfg_init(&cfg, ptr);

	/* look for MIDI port sections */
	for (c = cfg.sections; c; c = c->next) {
		j = -1;
		if (sscanf(c->name, "MIDI Port %d", &j) != 1) continue;
		if (j < 1) continue;
		sn = cfg_get_string(&cfg, c->name, "name", buf, 255, NULL);
		if (!sn) continue;
		if (strcasecmp(ss, sn) != 0) continue;
		sn = cfg_get_string(&cfg, c->name, "provider", buf, 255, NULL);
		if (sn && sp && strcasecmp(sp, sn) != 0) continue;
		/* okay found port */
		if ((q->iocap & MIDI_INPUT) && cfg_get_number(&cfg, c->name, "input", 0)) {
			q->io |= MIDI_INPUT;
		}
		if ((q->iocap & MIDI_OUTPUT) && cfg_get_number(&cfg, c->name, "output", 0)) {
			q->io |= MIDI_OUTPUT;
		}
		if (q->io && q->enable) q->enable(q);
	}

	cfg_free(&cfg);
	free(ptr);
}


void cfg_load_midi(cfg_file_t *cfg)
{
	midi_config_t *md, *mc;
	char buf[17], buf2[33];
	unsigned int i;

	CFG_GET_MI(flags, MIDI_TICK_QUANTIZE | MIDI_RECORD_NOTEOFF
		| MIDI_RECORD_VELOCITY | MIDI_RECORD_AFTERTOUCH
		| MIDI_PITCHBEND);
	CFG_GET_MI(pitch_depth, 12);
	CFG_GET_MI(amplification, 100);
	CFG_GET_MI(c5note, 60);

	song_lock_audio();
	md = &default_midi_config;
	cfg_get_string(cfg,"MIDI","start", md->start, 31, "FF");
	cfg_get_string(cfg,"MIDI","stop", md->stop, 31, "FC");
	cfg_get_string(cfg,"MIDI","tick", md->tick, 31, "");
	cfg_get_string(cfg,"MIDI","note_on", md->note_on, 31, "9c n v");
	cfg_get_string(cfg,"MIDI","note_off", md->note_off, 31, "9c n 0");
	cfg_get_string(cfg,"MIDI","set_volume", md->set_volume, 31, "");
	cfg_get_string(cfg,"MIDI","set_panning", md->set_panning, 31, "");
	cfg_get_string(cfg,"MIDI","set_bank", md->set_bank, 31, "");
	cfg_get_string(cfg,"MIDI","set_program", md->set_program, 31, "Cc p");
	for (i = 0; i < 16; i++) {
		snprintf(buf, 16, "SF%X", i);
		cfg_get_string(cfg, "MIDI", buf, md->sfx[i], 31,
				i == 0 ? "F0F000z" : "");
	}

	for (i = 0; i < 128; i++) {
		snprintf(buf, 16, "Z%02X", i + 0x80);
		if (i < 16)
			snprintf(buf2, 32, "F0F001%02x", i * 8);
		else
			buf2[0] = '\0';
		cfg_get_string(cfg, "MIDI", buf, md->zxx[i], 31, buf2);
	}

	mc = &current_song->midi_config;
	memcpy(mc, md, sizeof(midi_config_t));


	song_unlock_audio();
}

#define CFG_SET_MI(v) cfg_set_number(cfg, "MIDI", #v, midi_ ## v)
void cfg_save_midi(cfg_file_t *cfg)
{
	struct cfg_section *c;
	struct midi_provider *p;
	struct midi_port *q;
	midi_config_t *md, *mc;
	char buf[33];
	char *ss;
	unsigned int i;
	int j;

	CFG_SET_MI(flags);
	CFG_SET_MI(pitch_depth);
	CFG_SET_MI(amplification);
	CFG_SET_MI(c5note);

	song_lock_audio();
	md = &default_midi_config;

	/* overwrite default */
	mc = &current_song->midi_config;
	memcpy(md, mc, sizeof(midi_config_t));

	cfg_set_string(cfg,"MIDI","start", md->start);
	cfg_set_string(cfg,"MIDI","stop", md->stop);
	cfg_set_string(cfg,"MIDI","tick", md->tick);
	cfg_set_string(cfg,"MIDI","note_on", md->note_on);
	cfg_set_string(cfg,"MIDI","note_off", md->note_off);
	cfg_set_string(cfg,"MIDI","set_volume", md->set_volume);
	cfg_set_string(cfg,"MIDI","set_panning", md->set_panning);
	cfg_set_string(cfg,"MIDI","set_bank", md->set_bank);
	cfg_set_string(cfg,"MIDI","set_program", md->set_program);
	for (i = 0; i < 16; i++) {
		snprintf(buf, 32, "SF%X", i);
		cfg_set_string(cfg, "MIDI", buf, md->sfx[i]);
	}
	for (i = 0; i < 128; i++) {
		snprintf(buf, 32, "Z%02X", i + 0x80);
		cfg_set_string(cfg, "MIDI", buf, md->zxx[i]);
	}
	song_unlock_audio();

	/* write out only enabled midi ports */
	i = 1;
	mt_mutex_lock(midi_mutex);
	for (p = port_providers; p; p = p->next) {
		q = NULL;
		while (midi_port_foreach(p, &q)) {
			ss = q->name;
			if (!ss) continue;
			while (isspace(*ss)) ss++;
			if (!*ss) continue;
			if (!q->io) continue;

			snprintf(buf, 32, "MIDI Port %u", i);
			i++;
			cfg_set_string(cfg, buf, "name", ss);
			ss = p->name;
			if (ss) {
				while (isspace(*ss)) ss++;
				if (*ss) {
					cfg_set_string(cfg, buf, "provider", ss);
				}
			}
			cfg_set_number(cfg, buf, "input", q->io & MIDI_INPUT ? 1 : 0);
			cfg_set_number(cfg, buf, "output", q->io & MIDI_OUTPUT ? 1 : 0);
		}
	}
	//TODO: Save number of MIDI-IP ports
	mt_mutex_unlock(midi_mutex);

	/* delete other MIDI port sections */
	for (c = cfg->sections; c; c = c->next) {
		j = -1;
		if (sscanf(c->name, "MIDI Port %d", &j) != 1) continue;
		if (j < i) continue;
		c->omit = 1;
	}

}


static void _midi_engine_connect(void)
{
#ifdef USE_NETWORK
	ip_midi_setup();
#endif
#ifdef USE_JACK
	jack_midi_setup();
#endif
//Prefer ALSA MIDI over OSS, but do not enable both since ALSA's OSS emulation can cause conflicts
#if defined(USE_ALSA) && defined(USE_OSS)
	if (!alsa_midi_setup())
		oss_midi_setup();
#elif !defined(USE_ALSA) && defined(USE_OSS)
	oss_midi_setup();
#elif defined(USE_ALSA) && !defined(USE_OSS)
	alsa_midi_setup();
#endif
#ifdef SCHISM_WIN32
	win32mm_midi_setup();
#endif
#ifdef SCHISM_MACOSX
	macosx_midi_setup();
#endif
}

void midi_engine_poll_ports(void)
{
	struct midi_provider *n;

	if (!midi_mutex) return;

	mt_mutex_lock(midi_mutex);
	for (n = port_providers; n; n = n->next) {
		if (n->poll) n->poll(n);
	}
	mt_mutex_unlock(midi_mutex);
}

int midi_engine_start(void)
{
	if (_connected)
		return 1;

	midi_mutex        = mt_mutex_create();
	midi_record_mutex = mt_mutex_create();
	midi_port_mutex   = mt_mutex_create();

	if (!(midi_mutex && midi_record_mutex && midi_port_mutex)) {
		if (midi_mutex)        mt_mutex_delete(midi_mutex);
		if (midi_record_mutex) mt_mutex_delete(midi_record_mutex);
		if (midi_port_mutex)   mt_mutex_delete(midi_port_mutex);
		midi_mutex = midi_record_mutex = midi_port_mutex = NULL;
		return 0;
	}

	_midi_engine_connect();
	_connected = 1;
	return 1;
}

void midi_engine_reset(void)
{
	if (!_connected) return;
	midi_engine_stop();
	midi_engine_start();
}

void midi_engine_stop(void)
{
	struct midi_provider *n;
	struct midi_port *q;

	if (!_connected) return;
	if (!midi_mutex) return;

	mt_mutex_lock(midi_mutex);
	for (n = port_providers; n; ) {
		q = NULL;
		while (midi_port_foreach(n, &q))
			midi_port_unregister(q->num);

		if (n->thread) {
			n->cancelled = 1;
			mt_thread_wait(n->thread, NULL);
		}

		free(n->name);

		void *prev = n;
		n = n->next;
		free(prev);
	}
	_connected = 0;
	mt_mutex_unlock(midi_mutex);

	// now delete everything
	mt_mutex_delete(midi_mutex);
	mt_mutex_delete(midi_record_mutex);
	mt_mutex_delete(midi_port_mutex);
}

/* ------------------------------------------------------------- */
/* PORT system */

static struct midi_port **port_top = NULL;
static int port_count = 0;
static int port_alloc = 0;

struct midi_port *midi_engine_port(int n, const char **name)
{
	struct midi_port *pv = NULL;

	if (!midi_port_mutex) return NULL;
	mt_mutex_lock(midi_port_mutex);
	if (n >= 0 && n < port_count) {
		pv = port_top[n];
		if (name && pv) *name = pv->name;
	}
	mt_mutex_unlock(midi_port_mutex);
	return pv;
}

int midi_engine_port_count(void)
{
	int pc;
	if (!midi_port_mutex) return 0;
	mt_mutex_lock(midi_port_mutex);
	pc = port_count;
	mt_mutex_unlock(midi_port_mutex);
	return pc;
}

/* ------------------------------------------------------------- */
/* midi engines register a provider (one each!) */
struct midi_provider *midi_provider_register(const char *name,
		struct midi_driver *driver)
{
	struct midi_provider *n;

	if (!midi_mutex) return NULL;

	n = mem_calloc(1, sizeof(struct midi_provider));
	n->name = str_dup(name);
	n->poll = driver->poll;
	n->enable = driver->enable;
	n->disable = driver->disable;
	if (driver->flags & MIDI_PORT_CAN_SCHEDULE) {
		n->send_later = driver->send;
		n->drain = driver->drain;
	} else {
		n->send_now = driver->send;
	}

	mt_mutex_lock(midi_mutex);

	n->next = port_providers;
	port_providers = n;

	if (driver->thread)
		n->thread = mt_thread_create((int (*)(void*))driver->thread, n->name, n);

	mt_mutex_unlock(midi_mutex);

	return n;
}

/* ------------------------------------------------------------- */

/* midi engines list ports this way */
int midi_port_register(struct midi_provider *pv, int inout, const char *name,
	void *userdata, int free_userdata)
{
	struct midi_port *p, **pt;
	int i;

	if (!midi_port_mutex) return -1;

	p = mem_alloc(sizeof(struct midi_port));
	p->io = 0;
	p->iocap = inout;
	p->name = str_dup(name);
	p->enable = pv->enable;
	p->disable = pv->disable;
	p->send_later = pv->send_later;
	p->send_now = pv->send_now;
	p->drain = pv->drain;

	p->free_userdata = free_userdata;
	p->userdata = userdata;
	p->provider = pv;

	for (i = 0; i < port_alloc; i++) {
		if (port_top[i] == NULL) {
			port_top[i] = p;
			p->num = i;
			port_count++;
			_cfg_load_midi_part_locked(p);
			return i;
		}
	}

	mt_mutex_lock(midi_port_mutex);
	port_alloc += 4;
	pt = realloc(port_top, sizeof(struct midi_port *) * port_alloc);
	if (!pt) {
		free(p->name);
		free(p);
		mt_mutex_unlock(midi_port_mutex);
		return -1;
	}
	pt[port_count] = p;
	for (i = port_count+1; i < port_alloc; i++) pt[i] = NULL;
	port_top = pt;
	p->num = port_count;
	port_count++;

	/* finally, and just before unlocking, load any configuration for it... */
	_cfg_load_midi_part_locked(p);

	mt_mutex_unlock(midi_port_mutex);
	return p->num;
}

int midi_port_foreach(struct midi_provider *p, struct midi_port **cursor)
{
	int i;
	if (!midi_port_mutex || !port_top || !port_count) return 0;

	mt_mutex_lock(midi_port_mutex);
	do {
		if (!*cursor) {
			i = 0;
		} else {
			i = ((*cursor)->num) + 1;
			while (i < port_count && !port_top[i]) i++;
		}

		if (i >= port_count) {
			*cursor = NULL;
			mt_mutex_unlock(midi_port_mutex);
			return 0;
		}

		*cursor = port_top[i];
	} while (p && (*cursor)->provider != p);
	mt_mutex_unlock(midi_port_mutex);
	return 1;
}

// both of these functions return 1 on success and 0 on failure
int midi_port_enable(struct midi_port *p)
{
	int r = 0;

	mt_mutex_lock(midi_record_mutex);
	mt_mutex_lock(midi_port_mutex);

	if (p->enable)
		r = p->enable(p);

	if (!r)
		p->io = 0; // why

	mt_mutex_unlock(midi_port_mutex);
	mt_mutex_unlock(midi_record_mutex);

	return r;
}

int midi_port_disable(struct midi_port *p)
{
	int r = 0;

	mt_mutex_lock(midi_record_mutex);
	mt_mutex_lock(midi_port_mutex);

	if (p->disable)
		r = p->disable(p);

	mt_mutex_unlock(midi_port_mutex);
	mt_mutex_unlock(midi_record_mutex);

	return r;
}

/* ------------------------------------------------------------- */
/* send exactly one midi message */

enum midi_from {
	MIDI_FROM_IMMEDIATE = 0,
	MIDI_FROM_NOW = 1,
	MIDI_FROM_LATER = 2,
};

static int _midi_send_unlocked(const unsigned char *data, unsigned int len, unsigned int delay,
			enum midi_from from)
{
	struct midi_port *ptr = NULL;
	int need_timer = 0;
#if 0
	unsigned int i;
printf("MIDI: ");
	for (i = 0; i < len; i++) {
		printf("%02x ", data[i]);
	}
puts("");
fflush(stdout);
#endif
	switch (from) {
	case MIDI_FROM_IMMEDIATE:
		/* everyone plays */
		while (midi_port_foreach(NULL, &ptr)) {
			if ((ptr->io & MIDI_OUTPUT)) {
				if (ptr->send_now)
					ptr->send_now(ptr, data, len, 0);
				else if (ptr->send_later)
					ptr->send_later(ptr, data, len, 0);
			}
		}
		break;
	case MIDI_FROM_NOW:
		/* only "now" plays */
		while (midi_port_foreach(NULL, &ptr)) {
			if ((ptr->io & MIDI_OUTPUT)) {
				if (ptr->send_now)
					ptr->send_now(ptr, data, len, 0);
			}
		}
		break;
	case MIDI_FROM_LATER:
		/* only "later" plays */
		while (midi_port_foreach(NULL, &ptr)) {
			if ((ptr->io & MIDI_OUTPUT)) {
				if (ptr->send_later)
					ptr->send_later(ptr, data, len, delay);
				else if (ptr->send_now)
					need_timer = 1;
			}
		}
		break;
	default:
		break;
	}

	return need_timer;
}

void midi_send_now(const unsigned char *seq, unsigned int len)
{
	if (!midi_record_mutex) return;

	mt_mutex_lock(midi_record_mutex);
	_midi_send_unlocked(seq, len, 0, MIDI_FROM_IMMEDIATE);
	mt_mutex_unlock(midi_record_mutex);
}

/*----------------------------------------------------------------------------------*/
/* for drivers that don't have scheduled midi, use timers */

static uint32_t midims = 0;

void midi_queue_alloc(int buffer_length, int sample_size, int samples_per_second)
{
	// bytes per millisecond, rounded up
	midims = sample_size * samples_per_second;
	midims += 1000 - (midims % 1000);
	midims /= 1000;

	// nothing else to do now
}

int midi_need_flush(void)
{
	struct midi_port *ptr;
	int need_explicit_flush = 0;
	unsigned int i;

	if (!midi_record_mutex) return 0;

	ptr = NULL;
	while (midi_port_foreach(NULL, &ptr))
		if ((ptr->io & MIDI_OUTPUT) && !ptr->drain && ptr->send_now)
			return 1;

	return 0;
}

void midi_send_flush(void)
{
	struct midi_port *ptr = NULL;

	if (!midi_record_mutex) return;

	mt_mutex_lock(midi_record_mutex);
	while (midi_port_foreach(NULL, &ptr))
		if ((ptr->io & MIDI_OUTPUT) && ptr->drain)
			ptr->drain(ptr);
	mt_mutex_unlock(midi_record_mutex);
}

// The old queue was kind of dumb and unnecessary. In fact, the Windows version
// did... exactly this! :)
//
// I guess mrsbrisby just didn't know that SDL provided a timer API, or it wasn't
// provided by that time.
struct _midi_send_timer_curry {
	unsigned int len;
	unsigned char msg[];
};

static void _midi_send_timer_callback(void *param)
{
	// once more
	struct _midi_send_timer_curry *curry = (struct _midi_send_timer_curry *)param;

	if (!midi_record_mutex) return;

	mt_mutex_lock(midi_record_mutex);
	_midi_send_unlocked(curry->msg, curry->len, 0, MIDI_FROM_NOW);
	mt_mutex_unlock(midi_record_mutex);

	free(curry);
}

void midi_send_buffer(const unsigned char *data, unsigned int len, unsigned int pos)
{
	if (!midi_record_mutex) return;

	mt_mutex_lock(midi_record_mutex);

	/* just for fun... */
	if (status.current_page == PAGE_MIDI) {
		status.last_midi_real_len = len;
		if (len > sizeof(status.last_midi_event)) {
			status.last_midi_len = sizeof(status.last_midi_event);
		} else {
			status.last_midi_len = len;
		}
		memcpy(status.last_midi_event, data, status.last_midi_len);
		status.last_midi_port = NULL;
		status.last_midi_tick = timer_ticks();
		status.flags |= NEED_UPDATE | MIDI_EVENT_CHANGED;
	}

	if (midims > 0) { // should always be true but I'm paranoid
		pos /= midims;

		if (pos > 0) {
			if (_midi_send_unlocked(data, len, pos, MIDI_FROM_LATER)) {

				// ok, we need a timer.
				struct _midi_send_timer_curry *curry = mem_alloc(sizeof(struct _midi_send_timer_curry) + len);

				memcpy(curry->msg, data, len);
				curry->len = len;

				// one s'more
				timer_oneshot(pos, _midi_send_timer_callback, curry);
			}
		} else {
			// put the bread in the basket
			midi_send_now(data, len);
		}
	}

	mt_mutex_unlock(midi_record_mutex);
}

// Get the length of a MIDI event in bytes
uint8_t midi_event_length(uint8_t first_byte)
{
	switch(first_byte & 0xF0)
	{
	case 0xC0:
	case 0xD0:
		return 2;
	case 0xF0:
		switch(first_byte)
		{
		case 0xF1:
		case 0xF3:
			return 2;
		case 0xF2:
			return 3;
		default:
			return 1;
		}
		break;
	default:
		return 3;
	}
}

/*----------------------------------------------------------------------------------*/

void midi_port_unregister(int num)
{
	struct midi_port *q;
	int i;

	if (!midi_port_mutex) return;

	mt_mutex_lock(midi_port_mutex);
	for (i = 0; i < port_alloc; i++) {
		if (port_top[i] && port_top[i]->num == num) {
			q = port_top[i];
			if (q->disable) q->disable(q);
			if (q->free_userdata) free(q->userdata);
			if (q->name) free(q->name);
			free(q);

			port_count--;

			memmove(port_top + i, port_top + i + 1, sizeof(*port_top) * (port_count - i));
			port_top[port_count] = NULL;

			break;
		}
	}
	mt_mutex_unlock(midi_port_mutex);
}

void midi_received_cb(struct midi_port *src, unsigned char *data, unsigned int len)
{
	unsigned char d4[4];
	int cmd;

	if (!len) return;
	if (len < 4) {
		memset(d4, 0, sizeof(d4));
		memcpy(d4, data, len);
		data = d4;
	}

	/* just for fun... */
	mt_mutex_lock(midi_record_mutex);
	status.last_midi_real_len = len;
	if (len > sizeof(status.last_midi_event)) {
		status.last_midi_len = sizeof(status.last_midi_event);
	} else {
		status.last_midi_len = len;
	}
	memcpy(status.last_midi_event, data, status.last_midi_len);
	status.flags |= MIDI_EVENT_CHANGED;
	status.last_midi_port = src;
	status.last_midi_tick = timer_ticks();
	mt_mutex_unlock(midi_record_mutex);

	/* pass through midi events when on midi page */
	if (status.current_page == PAGE_MIDI) {
		midi_send_now(data,len);
		status.flags |= NEED_UPDATE;
	}

	cmd = ((*data) & 0xF0) >> 4;
	if (cmd == 0x8 || (cmd == 0x9 && data[2] == 0)) {
		midi_event_note(MIDI_NOTEOFF, data[0] & 15, data[1], 0);
	} else if (cmd == 0x9) {
		midi_event_note(MIDI_NOTEON, data[0] & 15, data[1], data[2]);
	} else if (cmd == 0xA) {
		midi_event_note(MIDI_KEYPRESS, data[0] & 15, data[1], data[2]);
	} else if (cmd == 0xB) {
		midi_event_controller(data[0] & 15, data[1], data[2]);
	} else if (cmd == 0xC) {
		midi_event_program(data[0] & 15, data[1]);
	} else if (cmd == 0xD) {
		midi_event_aftertouch(data[0] & 15, data[1]);
	} else if (cmd == 0xE) {
		midi_event_pitchbend(data[0] & 15, data[1]);
	} else if (cmd == 0xF) {
		switch ((*data & 15)) {
		case 0: /* sysex */
			if (len <= 2) return;
			midi_event_sysex(data+1, len-2);
			break;
		case 6: /* tick */
			midi_event_tick();
			break;
		default:
			/* something else */
			midi_event_system((*data & 15), (data[1])
					| (data[2] << 8)
					| (data[3] << 16));
			break;
		}
	}
}

void midi_event_note(enum midi_note mnstatus, int channel, int note, int velocity)
{
	schism_event_t event = {0};

	event.type = SCHISM_EVENT_MIDI_NOTE;
	event.midi_note.mnstatus = mnstatus;
	event.midi_note.channel = channel;
	event.midi_note.note = note;
	event.midi_note.velocity = velocity;

	events_push_event(&event);
}

void midi_event_controller(int channel, int param, int value)
{
	schism_event_t event = {0};

	event.type = SCHISM_EVENT_MIDI_CONTROLLER,
	event.midi_controller.value = value;
	event.midi_controller.param = param;
	event.midi_controller.channel = channel;

	events_push_event(&event);
}

void midi_event_program(int channel, int value)
{
	schism_event_t event = {0};

	event.type = SCHISM_EVENT_MIDI_PROGRAM;
	event.midi_program.value = value;
	event.midi_program.channel = channel;

	events_push_event(&event);
}

void midi_event_aftertouch(int channel, int value)
{
	schism_event_t event = {0};

	event.type = SCHISM_EVENT_MIDI_AFTERTOUCH;
	event.midi_aftertouch.value = value;
	event.midi_aftertouch.channel = channel;

	events_push_event(&event);
}

void midi_event_pitchbend(int channel, int value)
{
	schism_event_t event = {0};

	event.type = SCHISM_EVENT_MIDI_PITCHBEND;
	event.midi_pitchbend.value = value;
	event.midi_pitchbend.channel = channel;

	events_push_event(&event);
}

void midi_event_system(int argv, int param)
{
	schism_event_t event = {0};

	event.type = SCHISM_EVENT_MIDI_SYSTEM;
	event.midi_system.argv = argv;
	event.midi_system.param = param;

	events_push_event(&event);
}

void midi_event_tick(void)
{
	schism_event_t event = {0};

	event.type = SCHISM_EVENT_MIDI_TICK;

	events_push_event(&event);
}

void midi_event_sysex(const unsigned char *data, unsigned int len)
{
	schism_event_t event = {0};

	event.type = SCHISM_EVENT_MIDI_SYSEX;
	event.midi_sysex.len = len;
	event.midi_sysex.packet = strn_dup(data, len);

	events_push_event(&event);
}

int midi_engine_handle_event(schism_event_t *ev)
{
	struct key_event kk = {.is_synthetic = 0};

	if (midi_flags & MIDI_DISABLE_RECORD)
		return 0;

	switch (ev->type) {
	case SCHISM_EVENT_MIDI_NOTE:
		if (ev->midi_note.mnstatus == MIDI_NOTEON) {
			kk.state = KEY_PRESS;
		} else {
			if (!(midi_flags & MIDI_RECORD_NOTEOFF)) {
				/* don't record noteoff? okay... */
				break;
			}
			kk.state = KEY_RELEASE;
		}
		kk.midi_channel = ev->midi_note.channel + 1;
		kk.midi_note = ((ev->midi_note.note)+1 + midi_c5note) - 60;
		if (midi_flags & MIDI_RECORD_VELOCITY)
			kk.midi_volume = (ev->midi_note.velocity);
		else
			kk.midi_volume = 128;
		kk.midi_volume = (kk.midi_volume * midi_amplification) / 100;
		handle_key(&kk);
		return 1;
	case SCHISM_EVENT_MIDI_PITCHBEND:
		/* wheel */
		kk.midi_channel = ev->midi_pitchbend.channel+1;
		kk.midi_volume = -1;
		kk.midi_note = -1;
		kk.midi_bend = ev->midi_pitchbend.value;
		handle_key(&kk);
		return 1;
	case SCHISM_EVENT_MIDI_CONTROLLER:
		/* controller events */
		return 1;
	case SCHISM_EVENT_MIDI_SYSTEM:
		switch (ev->midi_system.argv) {
		case 0x8: /* MIDI tick */
			break;
		case 0xA: /* MIDI start */
		case 0xB: /* MIDI continue */
			song_start();
			break;
		case 0xC: /* MIDI stop */
		case 0xF: /* MIDI reset */
			/* this is helpful when miditracking */
			song_stop();
			break;
		};
		return 1;
	case SCHISM_EVENT_MIDI_SYSEX: /* but missing the F0 and the stop byte (F7) */
		/* tfw midi ports just hand us friggin packets yo */
		free(ev->midi_sysex.packet);
		return 1;
	default:
		return 0;
	}

	return 1;
}

