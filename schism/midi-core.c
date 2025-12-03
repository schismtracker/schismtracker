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

//#include "backend/mt.h"
#include "events.h"
#include "util.h"
#include "midi.h"
#include "song.h"
#include "page.h"
#include "it.h"
#include "config-parser.h"
#include "config.h"
#include "mt.h"
#include "timer.h"
#include "mem.h"
#include "atomic.h"

#include "dmoz.h"

static struct atm _connected = {0};
/* midi_mutex is locked by the main thread,
midi_port_mutex is for the port thread(s),
and midi_record_mutex is by the event/sound thread
*/
static mt_mutex_t *midi_mutex = NULL;
static mt_mutex_t *midi_port_mutex = NULL;
static mt_mutex_t *midi_record_mutex = NULL;

static mt_thread_t *midi_worker_thread = NULL;
static volatile int midi_worker_thread_cancel = 0;
static volatile int midi_worker_thread_tried = 0;

static struct midi_provider *port_providers = NULL;

/* configurable midi stuff */
int midi_flags = MIDI_TICK_QUANTIZE | MIDI_RECORD_NOTEOFF
		| MIDI_RECORD_VELOCITY | MIDI_RECORD_AFTERTOUCH
		| MIDI_PITCHBEND;

int midi_pitch_depth = 12;
int midi_amplification = 100;
int midi_c5note = 60;

#define CFG_GET_MI(v,d) midi_ ## v = cfg_get_number(cfg, "MIDI", #v, d)


static void _cfg_load_midi_port_locked(struct midi_port *q)
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
		sn = cfg_get_string(&cfg, c->name, "name", buf, ARRAY_SIZE(buf), NULL);
		if (!sn) continue;
		if (strcasecmp(ss, sn) != 0) continue;
		sn = cfg_get_string(&cfg, c->name, "provider", buf, ARRAY_SIZE(buf), NULL);
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
	uint32_t i;

	CFG_GET_MI(flags, MIDI_TICK_QUANTIZE | MIDI_RECORD_NOTEOFF
		| MIDI_RECORD_VELOCITY | MIDI_RECORD_AFTERTOUCH
		| MIDI_PITCHBEND);
	CFG_GET_MI(pitch_depth, 12);
	CFG_GET_MI(amplification, 100);
	CFG_GET_MI(c5note, 60);

	song_lock_audio();
	md = &default_midi_config;
	cfg_get_string(cfg,"MIDI","start",       md->start,       ARRAY_SIZE(md->start),       "FF");
	cfg_get_string(cfg,"MIDI","stop",        md->stop,        ARRAY_SIZE(md->stop),        "FC");
	cfg_get_string(cfg,"MIDI","tick",        md->tick,        ARRAY_SIZE(md->tick),        "");
	cfg_get_string(cfg,"MIDI","note_on",     md->note_on,     ARRAY_SIZE(md->note_on),     "9c n v");
	cfg_get_string(cfg,"MIDI","note_off",    md->note_off,    ARRAY_SIZE(md->note_off),    "9c n 0");
	cfg_get_string(cfg,"MIDI","set_volume",  md->set_volume,  ARRAY_SIZE(md->set_volume),  "");
	cfg_get_string(cfg,"MIDI","set_panning", md->set_panning, ARRAY_SIZE(md->set_panning), "");
	cfg_get_string(cfg,"MIDI","set_bank",    md->set_bank,    ARRAY_SIZE(md->set_bank),    "");
	cfg_get_string(cfg,"MIDI","set_program", md->set_program, ARRAY_SIZE(md->set_program), "Cc p");
	for (i = 0; i < 16; i++) {
		snprintf(buf, 16, "SF%X", i);
		cfg_get_string(cfg, "MIDI", buf, md->sfx[i], ARRAY_SIZE(md->sfx[i]),
				i == 0 ? "F0F000z" : "");
	}

	for (i = 0; i < 128; i++) {
		snprintf(buf, 16, "Z%02X", i + 0x80);
		if (i < 16)
			snprintf(buf2, 32, "F0F001%02x", i * 8);
		else
			buf2[0] = '\0';
		cfg_get_string(cfg, "MIDI", buf, md->zxx[i], ARRAY_SIZE(md->zxx[i]), buf2);
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
	uint32_t i, j;

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
	mt_mutex_lock(midi_port_mutex);
	for (p = port_providers; p; p = p->next) {
		q = NULL;
		while (midi_port_foreach(p, &q)) {
			ss = q->name;
			if (!ss) continue;
			while (isspace(*ss)) ss++;
			if (!*ss) continue;
			if (!q->io) continue;

			snprintf(buf, 32, "MIDI Port %" SCNu32, i);
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
	mt_mutex_unlock(midi_port_mutex);

	/* delete other MIDI port sections */
	for (c = cfg->sections; c; c = c->next) {
		if (sscanf(c->name, "MIDI Port %" SCNu32, &j) != 1) continue;
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
#ifdef SCHISM_MACOS
	midimgr_midi_setup();
#endif
}

void midi_engine_poll_ports(void)
{
	struct midi_provider *n;

	if (!midi_mutex) return;

	mt_mutex_lock(midi_mutex);
	for (n = port_providers; n; n = n->next) {
		if (n->poll)
			n->poll(n);
	}
	mt_mutex_unlock(midi_mutex);
}

int midi_engine_start(void)
{
	if (atm_load(&_connected))
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
	atm_store(&_connected, 1);
	return 1;
}

void midi_engine_reset(void)
{
	if (!atm_load(&_connected)) return;
	midi_engine_stop();
	midi_engine_start();
}

void midi_engine_stop(void)
{
	struct midi_provider *n;
	struct midi_port *q;

	if (!atm_load(&_connected)) return;
	if (!midi_mutex) return;

	mt_mutex_lock(midi_mutex);
	atm_store(&_connected, 0);

	if (midi_worker_thread) {
		midi_worker_thread_cancel = 1;
		mt_thread_wait(midi_worker_thread, NULL);
		/* reset ... */
		midi_worker_thread_cancel = 0;
		midi_worker_thread = NULL;
	}

	for (n = port_providers; n; ) {
		uint32_t ln = 0;
		int f = 0; /* this is stupid and i hate it */

		q = NULL;
		while (midi_port_foreach(n, &q)) {
			/* We have to do it this way to avoid use-after-free. */
			if (f)
				midi_port_unregister(ln);

			ln = q->num;
			f = 1;
		}

		if (f)
			midi_port_unregister(ln);

		if (n->thread) {
			n->cancelled = 1;
			mt_thread_wait(n->thread, NULL);
		}

		if (n->destroy_userdata)
			n->destroy_userdata(n->userdata);

		free(n->name);

		void *prev = n;
		n = n->next;
		free(prev);
	}
	mt_mutex_unlock(midi_mutex);

	// now delete everything
	mt_mutex_delete(midi_mutex);
	mt_mutex_delete(midi_record_mutex);
	mt_mutex_delete(midi_port_mutex);
}

/* ------------------------------------------------------------- */
/* PORT system */

/* This port structure is somewhat convoluted.
 *
 * In order to support hotplugging, some items in the array will
 * be NULL if they are unplugged after a poll.
 *
 * This means that the index in which they are displayed on the
 * midi screen does not exactly reflect the index in which
 * they are in memory.
 *
 * The midi_engine_port and midi_engine_port_count functions
 * work on the array as if it were a simple array where all
 * ports are in order. It was made this way to simplify the
 * code for the midi screen, however it can be misleading
 * if one is not aware of this. Most importantly, it means
 * that (midi_engine_port(port->num, NULL)) may not return
 * the same port as went in, so don't even try doing that! */
static struct midi_port **port_top = NULL;
static uint32_t port_alloc = 0;

/* NOTE: (n != port->num) !!
 * this function is mainly for the midi screen. */
struct midi_port *midi_engine_port(uint32_t n, const char **name)
{
	struct midi_port *q;

	if (!midi_port_mutex) return NULL;

	mt_mutex_lock(midi_port_mutex);

	q = NULL;
	while (midi_port_foreach(NULL, &q) && (n > 0))
		n--;
	if (q && name) *name = q->name;

	mt_mutex_unlock(midi_port_mutex);

	return q;
}

/* NOTE: this does not necessarily equal port_alloc,
 * more specifically after the user hotplugs a midi device */
uint32_t midi_engine_port_count(void)
{
	uint32_t i, pc;

	if (!midi_port_mutex) return 0;

	mt_mutex_lock(midi_port_mutex);
	for (i = 0, pc = 0; i < port_alloc; i++)
		if (port_top[i])
			pc++;
	mt_mutex_unlock(midi_port_mutex);

	return pc;
}

/* these are used for locking the ports, so that for example
 * the ports can be kept intact while drawing to the screen */
void midi_engine_port_lock(void)
{
	mt_mutex_lock(midi_port_mutex);
}

void midi_engine_port_unlock(void)
{
	mt_mutex_unlock(midi_port_mutex);
}

/* ------------------------------------------------------------------------ */

static void midi_engine_worker_impl(void)
{
	struct midi_provider *n;

	if (!atm_load(&_connected))
		return;

	mt_mutex_lock(midi_mutex);

	for (n = port_providers; n; n = n->next) {
		if (n->work)
			n->work(n);
	}

	mt_mutex_unlock(midi_mutex);
}

void midi_engine_worker(void)
{
	if (midi_worker_thread)
		return;

	midi_engine_worker_impl();
}

#ifdef USE_THREADS
static int midi_engine_worker_thread_func(SCHISM_UNUSED void *z)
{
	while (!midi_worker_thread_cancel) {
		midi_engine_worker_impl();
		/* sleep as to not hog the CPU */
		timer_msleep(1);
	}

	return 0;
}

static void midi_engine_worker_thread_start(void)
{
	if (midi_worker_thread)
		return;

	mt_mutex_lock(midi_mutex);

	midi_worker_thread_cancel = 0;
	midi_worker_thread = mt_thread_create(midi_engine_worker_thread_func,
		"MIDI worker thread", NULL);

	mt_mutex_unlock(midi_mutex);
}
#endif

/* ------------------------------------------------------------------------ */

#ifdef USE_THREADS
struct midi_provider_thread_info {
	const struct midi_driver *d;
	struct midi_provider *n;
};

static int midi_provider_thread_func(void *z)
{
	struct midi_provider_thread_info *i = z;

	const struct midi_driver *d = i->d;
	struct midi_provider *n = i->n;

	free(i);

	return d->thread(n);
}
#endif

/* midi engines register a provider (one each!) */
struct midi_provider *midi_provider_register(const char *name,
	const struct midi_driver *driver, void *userdata)
{
	struct midi_provider *n;

	if (!midi_mutex) return NULL;

	n = mem_calloc(1, sizeof(struct midi_provider));
	n->name = str_dup(name);
	n->poll = driver->poll;
	n->enable = driver->enable;
	n->disable = driver->disable;
	n->destroy_userdata = driver->destroy_userdata;
	n->userdata = userdata;
	if (driver->flags & MIDI_PORT_CAN_SCHEDULE) {
		n->send_later = driver->send;
		n->drain = driver->drain;
	} else {
		n->send_now = driver->send;
	}

	mt_mutex_lock(midi_mutex);

	n->next = port_providers;
	port_providers = n;

	if (driver->thread) {
#ifdef USE_THREADS
		struct midi_provider_thread_info *i = mem_alloc(sizeof(*i));

		i->d = driver;
		i->n = n;

		n->thread = mt_thread_create(midi_provider_thread_func, n->name, i);
#else
		log_appendf(1, "Internal error launching %s MIDI: threads not supported", n->name);
		n->thread = NULL;
		port_providers = n->next;
		free(n);
		return NULL;
#endif
	}

	if (driver->work) {
		n->work = driver->work;

#ifdef USE_THREADS
		midi_engine_worker_thread_start();
#endif
	}

	mt_mutex_unlock(midi_mutex);

	return n;
}

void midi_provider_mark_ports(struct midi_provider *p)
{
	struct midi_port *q;

	mt_mutex_lock(midi_port_mutex);

	q = NULL;
	while (midi_port_foreach(p, &q))
		q->mark = 1;

	mt_mutex_unlock(midi_port_mutex);
}

void midi_provider_remove_marked_ports(struct midi_provider *p)
{
	struct midi_port *q;

	mt_mutex_lock(midi_port_mutex);

	q = NULL;
	for (;;) {
		struct midi_port *g;
		int ok;

		g = q;
		ok = midi_port_foreach(p, &q);
		if (g) {
			if (g->mark) {
				/* log_appendf(1, "removing %d %s", g->num, g->name); */
				midi_port_unregister(g->num);
			} else {
				/* log_appendf(1, "keeping %d %s", g->num, g->name); */
			}
		}
		if (!ok)
			break;
	}

	mt_mutex_unlock(midi_port_mutex);
}

/* ------------------------------------------------------------- */

/* gets an unused port from the ports array.
 * this may be an unregistered port (previously used), or
 * an allocated (but not used) port.
 *
 * returns -1 if there is an error */
static int64_t midi_port_get_unused(void)
{
	struct midi_port **pt;
	uint32_t i;

	mt_mutex_lock(midi_port_mutex);

	for (i = 0; i < port_alloc; i++) {
		if (port_top[i])
			continue;

		mt_mutex_unlock(midi_port_mutex);
		return i;
	}

	/* no overflow */
	if (port_alloc + 4u < port_alloc)
		return -1;

	pt = realloc(port_top, sizeof(*port_top) * (port_alloc + 4u));
	if (!pt) {
		mt_mutex_unlock(midi_port_mutex);
		return -1;
	}

	port_top = pt;
	for (i = 0; i < 4; i++)
		port_top[port_alloc + i] = NULL;
	i = port_alloc;
	port_alloc += 4;

	mt_mutex_unlock(midi_port_mutex);

	return i;
}

/* midi engines list ports this way */
uint32_t midi_port_register(struct midi_provider *pv, uint8_t inout, const char *name,
	void *userdata, void (*destroy_userdata)(void *))
{
	struct midi_port *p;
	int64_t i;

	if (!midi_port_mutex)
		return -1;

	p = mem_alloc(sizeof(struct midi_port));
	p->io = 0;
	p->iocap = inout;
	p->name = str_dup(name);
	p->enable = pv->enable;
	p->disable = pv->disable;
	p->send_later = pv->send_later;
	p->send_now = pv->send_now;
	p->drain = pv->drain;
	p->mark = 0;

	p->destroy_userdata = destroy_userdata;
	p->userdata = userdata;
	p->provider = pv;

	mt_mutex_lock(midi_port_mutex);

	i = midi_port_get_unused();
	if (i < 0) {
		free(p->name);
		free(p);
		mt_mutex_unlock(midi_port_mutex);
		return -1;
	}

	port_top[i] = p;
	p->num = i;

	/* finally, and just before unlocking, load any configuration for it... */
	_cfg_load_midi_port_locked(p);

	mt_mutex_unlock(midi_port_mutex);

	return p->num;
}

void midi_port_unregister(uint32_t num)
{
	if (!midi_port_mutex) return;

	mt_mutex_lock(midi_port_mutex);

	if (num < port_alloc) {
		struct midi_port *q = port_top[num];

		if (q->disable) q->disable(q);
		if (q->destroy_userdata) q->destroy_userdata(q->userdata);
		if (q->name) free(q->name);
		free(q);

		port_top[num] = NULL;
	}

	mt_mutex_unlock(midi_port_mutex);
}

int midi_port_foreach(struct midi_provider *p, struct midi_port **cursor)
{
	uint32_t i;
	if (!midi_port_mutex || !port_top || !port_alloc) return 0;

	if (!cursor)
		return 0;

	mt_mutex_lock(midi_port_mutex);
	do {
		if (!*cursor) {
			i = 0;
		} else {
			i = ((*cursor)->num) + 1;
		}

		while (i < port_alloc && !port_top[i]) i++;

		if (i >= port_alloc) {
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
	int r;

	mt_mutex_lock(midi_record_mutex);
	mt_mutex_lock(midi_port_mutex);

	r = (p->enable) ? (p->enable(p)) : 0;
	if (!r)
		p->io = 0;

	mt_mutex_unlock(midi_port_mutex);
	mt_mutex_unlock(midi_record_mutex);

	return r;
}

int midi_port_disable(struct midi_port *p)
{
	int r;

	mt_mutex_lock(midi_record_mutex);
	mt_mutex_lock(midi_port_mutex);

	r = (p->disable) ? (p->disable(p)) : 0;
	//if (!r)
	//	p->io = 0;

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

static int _midi_send_unlocked(const unsigned char *data, uint32_t len, uint32_t delay,
			enum midi_from from)
{
	struct midi_port *ptr = NULL;
	int need_timer = 0;
#if 0
	uint32_t i;
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

void midi_send_now(const unsigned char *seq, uint32_t len)
{
	if (!midi_record_mutex) return;

	mt_mutex_lock(midi_record_mutex);
	mt_mutex_lock(midi_port_mutex);
	_midi_send_unlocked(seq, len, 0, MIDI_FROM_IMMEDIATE);
	mt_mutex_unlock(midi_port_mutex);
	mt_mutex_unlock(midi_record_mutex);
}

/*----------------------------------------------------------------------------------*/
/* for drivers that don't have scheduled midi, use timers */

static uint32_t midims = 0;

void midi_queue_alloc(SCHISM_UNUSED int buffer_length, int sample_size, int samples_per_second)
{
	// bytes per millisecond, rounded up
	midims = sample_size * samples_per_second;
	midims = (midims + 999) / 1000;
}

int midi_need_flush(void)
{
	int r;

	struct midi_port *ptr;

	if (!midi_port_mutex) return 0;

	mt_mutex_lock(midi_port_mutex);

	r = 0;
	ptr = NULL;
	while (midi_port_foreach(NULL, &ptr))
		if ((ptr->io & MIDI_OUTPUT) && !ptr->drain && ptr->send_now)
			r = 1;

	mt_mutex_unlock(midi_port_mutex);

	return r;
}

void midi_send_flush(void)
{
	struct midi_port *ptr = NULL;

	if (!midi_record_mutex || !midi_port_mutex) return;

	mt_mutex_lock(midi_record_mutex);
	mt_mutex_lock(midi_port_mutex);
	while (midi_port_foreach(NULL, &ptr))
		if ((ptr->io & MIDI_OUTPUT) && ptr->drain)
			ptr->drain(ptr);
	mt_mutex_unlock(midi_port_mutex);
	mt_mutex_unlock(midi_record_mutex);
}

struct _midi_send_timer_curry {
	uint32_t len;
	unsigned char msg[SCHISM_FAM_SIZE];
};

static void _midi_send_timer_callback(void *param)
{
	// once more
	struct _midi_send_timer_curry *curry = param;

	// make sure the midi system is actually still running to prevent
	// a crash on exit
	if (midi_record_mutex && midi_port_mutex && atm_load(&_connected)) {
		mt_mutex_lock(midi_record_mutex);
		mt_mutex_lock(midi_port_mutex);
		_midi_send_unlocked(curry->msg, curry->len, 0, MIDI_FROM_NOW);
		mt_mutex_unlock(midi_record_mutex);
		mt_mutex_unlock(midi_port_mutex);
	}

	free(curry);
}

void midi_send_buffer(const unsigned char *data, uint32_t len, uint32_t pos)
{
	if (!midi_record_mutex) return;

	mt_mutex_lock(midi_record_mutex);

	/* just for fun... */
	if (status.current_page == PAGE_MIDI) {
		status.last_midi_real_len = len;
		status.last_midi_len = MIN(sizeof(status.last_midi_event), len);
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
				struct _midi_send_timer_curry *curry = mem_alloc(sizeof(*curry) + len);

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
// FIXME: this needs to handle sysex and friends as well
uint8_t midi_event_length(uint8_t first_byte)
{
	switch (first_byte & 0xF0) {
	case 0xC0:
	case 0xD0:
		return 2;
	case 0xF0:
		switch (first_byte) {
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

void midi_received_cb(struct midi_port *src, const unsigned char *data, uint32_t len)
{
	unsigned char d4[4];
	int cmd;

	if (!len) return;
	if (len < sizeof(d4)) {
		memcpy(d4, data, len);
		memset(d4 + len, 0, sizeof(d4) - len);
		data = d4;
	}

	/* just for fun... */
	mt_mutex_lock(midi_record_mutex);
	status.last_midi_real_len = len;
	status.last_midi_len = MIN(sizeof(status.last_midi_event), len);
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
			midi_event_sysex(data + 1, len - 2);
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
	schism_event_t event;
	memset(&event, 0, sizeof(event));

	event.type = SCHISM_EVENT_MIDI_NOTE;
	event.midi_note.mnstatus = mnstatus;
	event.midi_note.channel = channel;
	event.midi_note.note = note;
	event.midi_note.velocity = velocity;

	events_push_event(&event);
}

void midi_event_controller(int channel, int param, int value)
{
	schism_event_t event;
	memset(&event, 0, sizeof(event));

	event.type = SCHISM_EVENT_MIDI_CONTROLLER,
	event.midi_controller.value = value;
	event.midi_controller.param = param;
	event.midi_controller.channel = channel;

	events_push_event(&event);
}

void midi_event_program(int channel, int value)
{
	schism_event_t event;
	memset(&event, 0, sizeof(event));

	event.type = SCHISM_EVENT_MIDI_PROGRAM;
	event.midi_program.value = value;
	event.midi_program.channel = channel;

	events_push_event(&event);
}

void midi_event_aftertouch(int channel, int value)
{
	schism_event_t event;
	memset(&event, 0, sizeof(event));

	event.type = SCHISM_EVENT_MIDI_AFTERTOUCH;
	event.midi_aftertouch.value = value;
	event.midi_aftertouch.channel = channel;

	events_push_event(&event);
}

void midi_event_pitchbend(int channel, int value)
{
	schism_event_t event;
	memset(&event, 0, sizeof(event));

	event.type = SCHISM_EVENT_MIDI_PITCHBEND;
	event.midi_pitchbend.value = value;
	event.midi_pitchbend.channel = channel;

	events_push_event(&event);
}

void midi_event_system(int argv, int param)
{
	schism_event_t event;
	memset(&event, 0, sizeof(event));

	event.type = SCHISM_EVENT_MIDI_SYSTEM;
	event.midi_system.argv = argv;
	event.midi_system.param = param;

	events_push_event(&event);
}

void midi_event_tick(void)
{
	schism_event_t event;
	memset(&event, 0, sizeof(event));

	event.type = SCHISM_EVENT_MIDI_TICK;

	events_push_event(&event);
}

void midi_event_sysex(const unsigned char *data, uint32_t len)
{
	schism_event_t event;
	memset(&event, 0, sizeof(event));

	event.type = SCHISM_EVENT_MIDI_SYSEX;
	event.midi_sysex.len = len;
	event.midi_sysex.packet = (unsigned char *)strn_dup((const char *)data, len);

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

