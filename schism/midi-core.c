/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
 * URL: http://rigelseven.com/schism/
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

#include "event.h"

#include "mixer.h"
#include "util.h"

#include "midi.h"
#include "song.h"

#include "sdlmain.h"

#include "page.h"

#include "it.h"

#include "dmoz.h"

#include <ctype.h>

static int _connected = 0;
/* midi_mutex is locked by the main thread,
midi_port_mutex is for the port thread(s),
and midi_record_mutex is by the event/sound thread
*/
static SDL_mutex *midi_mutex = 0;
static SDL_mutex *midi_port_mutex = 0;
static SDL_mutex *midi_record_mutex = 0;
static SDL_mutex *midi_play_mutex = 0;
static SDL_cond *midi_play_cond = 0;

static struct midi_provider *port_providers = 0;


/* time shit */
#ifdef WIN32
#include <windows.h>

static void (*__win32_usleep)(unsigned int usec) = 0;
static void __win32_old_usleep(unsigned int u)
{
	/* bah, only Win95 and "earlier" actually needs this... */
	SleepEx(u/1000,FALSE);
}

static FARPROC __ihatewindows_f1 = 0;
static FARPROC __ihatewindows_f2 = 0;
static FARPROC __ihatewindows_f3 = 0;
static HANDLE __midi_timer = 0;

static void __win32_new_usleep(unsigned int u)
{
	LARGE_INTEGER due;
	due.QuadPart = -(10 * (__int64)u);
	__ihatewindows_f2(__midi_timer, &due, 0, NULL, NULL, 0);
	__ihatewindows_f3(__midi_timer, INFINITE);
}
static void __win32_pick_usleep(void)
{
	HINSTANCE k32;

	k32 = GetModuleHandle("KERNEL32.DLL");
	if (!k32) k32 = LoadLibrary("KERNEL32.DLL");
	if (!k32) k32 = GetModuleHandle("KERNEL32.DLL");
	if (!k32) goto FAIL;
	__ihatewindows_f1 = (FARPROC)GetProcAddress(k32,"CreateWaitableTimer");
	__ihatewindows_f2 = (FARPROC)GetProcAddress(k32,"SetWaitableTimer");
	__ihatewindows_f3 = (FARPROC)GetProcAddress(k32,"WaitForSingleObject");
	if (!__ihatewindows_f1 || !__ihatewindows_f2 || !__ihatewindows_f3)
		goto FAIL;
	__midi_timer = (HANDLE)__ihatewindows_f1(NULL,TRUE,NULL);
	if (!__midi_timer) goto FAIL;

	/* grumble */
	__win32_usleep = __win32_new_usleep;
	return;
FAIL:
	__win32_usleep = __win32_old_usleep;
}

#define SLEEP_FUNC(x)	__win32_usleep(x)

#else

#define SLEEP_FUNC(x)	usleep(x)

#endif

/* configurable midi stuff */
int midi_flags = MIDI_TICK_QUANTIZE | MIDI_RECORD_NOTEOFF
		| MIDI_RECORD_VELOCITY | MIDI_RECORD_AFTERTOUCH
		| MIDI_PITCH_BEND;
		
int midi_pitch_depth = 12;
int midi_amplification = 100;
int midi_c5note = 60;

#define CFG_GET_MI(v,d) midi_ ## v = cfg_get_number(cfg, "MIDI", #v, d)
#define CFG_GET_MS(v,b,l,d) cfg_get_string(cfg, "MIDI", #v, b,l, d)

static void _cfg_load_midi_part_locked(struct midi_port *q)
{
	struct cfg_section *c;
	/*struct midi_provider *p;*/
	cfg_file_t cfg;
	const char *sn;
	char *ptr, *ss, *sp;
	int i, j;

	ss = (char*)q->name;
	if (!ss) return;
	while (isspace(((unsigned int)*ss))) ss++;
	if (!*ss) return;

	sp = (q->provider) ? (char*)q->provider->name : 0;
	if (sp) {
		while (isspace(((unsigned int)*sp))) sp++;
		if (!*sp) sp = 0;
	}
	if (sp) i = strlen(sp); else i = 0;
	j = strlen(ss);
	if (j > i) i = j;
	ptr = mem_alloc(i+1);

	ptr = dmoz_path_concat(cfg_dir_dotschism, "config");
	cfg_init(&cfg, ptr);

	/* look for MIDI port sections */
	for (c = cfg.sections; c; c = c->next) {
		j = -1;
		(void)sscanf(c->name, "MIDI Port %d", &j);
		if (j < 1) continue;
		sn = cfg_get_string(&cfg, c->name, "name", ptr, i, 0);
		if (!sn) continue;
		if (strcasecmp(ss, sn) != 0) continue;
		sn = cfg_get_string(&cfg, c->name, "provider", ptr, i, 0);
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
}


void cfg_load_midi(cfg_file_t *cfg)
{
        midi_config *md;
	char buf[16], buf2[32];
	int i;

	CFG_GET_MI(flags, MIDI_TICK_QUANTIZE | MIDI_RECORD_NOTEOFF
		| MIDI_RECORD_VELOCITY | MIDI_RECORD_AFTERTOUCH
		| MIDI_PITCH_BEND);
	CFG_GET_MI(pitch_depth, 12);
	CFG_GET_MI(amplification, 100);
	CFG_GET_MI(c5note, 60);

        song_lock_audio();
        md = song_get_midi_config();
	cfg_get_string(cfg,"MIDI","start", md->midi_global_data+(0*32),32, "FF");
	cfg_get_string(cfg,"MIDI","stop", md->midi_global_data+(1*32),32, "FC");
	cfg_get_string(cfg,"MIDI","tick", md->midi_global_data+(2*32),32, "");
	cfg_get_string(cfg,"MIDI","note_on", md->midi_global_data+(3*32),32, "9c n v");
	cfg_get_string(cfg,"MIDI","note_off", md->midi_global_data+(4*32),32, "9c n 0");
	cfg_get_string(cfg,"MIDI","set_volume", md->midi_global_data+(5*32),32, "");
	cfg_get_string(cfg,"MIDI","set_panning", md->midi_global_data+(6*32),32, "");
	cfg_get_string(cfg,"MIDI","set_bank", md->midi_global_data+(7*32),32, "");
	cfg_get_string(cfg,"MIDI","set_program", md->midi_global_data+(8*32),32, "Cc p");
	for (i = 0; i < 16; i++) {
		sprintf(buf, "SF%01x", i);
		cfg_get_string(cfg, "MIDI", buf, md->midi_sfx+(i*32),32, 
				i == 0 ? "F0F000z" : "");
	}
	for (i = 0; i < 0x7f; i++) {
		sprintf(buf, "Z%02x", i + 0x80);
		if (i < 16) {
			sprintf(buf2, "F0F001%02x", i * 8);
		} else {
			buf2[0] = 0;
		}
		cfg_get_string(cfg, "MIDI", buf, md->midi_zxx+(i*32),32, buf2);
	}
	song_unlock_audio();
}
#define CFG_SET_MI(v) cfg_set_number(cfg, "MIDI", #v, midi_ ## v)
#define CFG_SET_MS(v,b,d) cfg_get_string(cfg, "MIDI", #v, b, d)
void cfg_save_midi(cfg_file_t *cfg)
{
	struct cfg_section *c;
	struct midi_provider *p;
	struct midi_port *q;
        midi_config *md;
	char buf[32], *ss;
	int i, j;

	CFG_SET_MI(flags);
	CFG_SET_MI(pitch_depth);
	CFG_SET_MI(amplification);
	CFG_SET_MI(c5note);

        song_lock_audio();
        md = song_get_midi_config();
	cfg_set_string(cfg,"MIDI","start", md->midi_global_data+(0*32));
	cfg_set_string(cfg,"MIDI","stop", md->midi_global_data+(1*32));
	cfg_set_string(cfg,"MIDI","tick", md->midi_global_data+(2*32));
	cfg_set_string(cfg,"MIDI","note_on", md->midi_global_data+(3*32));
	cfg_set_string(cfg,"MIDI","note_off", md->midi_global_data+(4*32));
	cfg_set_string(cfg,"MIDI","set_volume", md->midi_global_data+(5*32));
	cfg_set_string(cfg,"MIDI","set_panning", md->midi_global_data+(6*32));
	cfg_set_string(cfg,"MIDI","set_bank", md->midi_global_data+(7*32));
	cfg_set_string(cfg,"MIDI","set_program", md->midi_global_data+(8*32));
	for (i = 0; i < 16; i++) {
		sprintf(buf, "SF%01x", i);
		cfg_set_string(cfg, "MIDI", buf, md->midi_sfx+(i*32));
	}
	for (i = 0; i < 0x7f; i++) {
		sprintf(buf, "Z%02x", i + 0x80);
		cfg_set_string(cfg, "MIDI", buf, md->midi_zxx+(i*32));
	}
	song_unlock_audio();

	/* write out only enabled midi ports */
	i = 1;
	SDL_mutexP(midi_mutex);
	q = 0;
	for (p = port_providers; p; p = p->next) {
		while (midi_port_foreach(p, &q)) {
			ss = (char*)q->name;
			if (!ss) continue;
			while (isspace(((unsigned int)*ss))) ss++;
			if (!*ss) continue;
			if (!q->io) continue;

			sprintf(buf, "MIDI Port %d", i); i++;
			cfg_set_string(cfg, buf, "name", ss);
			ss = (char*)p->name;
			if (ss) {
				while (isspace(((unsigned int)*ss))) ss++;
				if (*ss) {
					cfg_set_string(cfg, buf, "provider", ss);
				}
			}
			cfg_set_number(cfg, buf, "input", q->io & MIDI_INPUT ? 1 : 0);
			cfg_set_number(cfg, buf, "output", q->io & MIDI_OUTPUT ? 1 : 0);
		}
	}
	SDL_mutexV(midi_mutex);

	/* delete other MIDI port sections */
	for (c = cfg->sections; c; c = c->next) {
		j = -1;
		(void)sscanf(c->name, "MIDI Port %d", &j);
		if (j < i) continue;
		c->omit = 1;
	}

}


static void _midi_engine_connect(void)
{
#ifdef USE_NETWORK
	ip_midi_setup();
#endif
#ifdef USE_OSS
	oss_midi_setup();
#endif
#ifdef USE_ALSA
	alsa_midi_setup();
#endif
#ifdef USE_WIN32MM
	win32mm_midi_setup();
#endif
#ifdef MACOSX
	macosx_midi_setup();
#endif
}

void midi_engine_poll_ports(void)
{
	struct midi_provider *n;

	if (!midi_mutex) return;

	SDL_mutexP(midi_mutex);
	for (n = port_providers; n; n = n->next) {
		if (n->poll) n->poll(n);
	}
	SDL_mutexV(midi_mutex);
}

int midi_engine_start(void)
{
	if (!_connected) {
		midi_mutex = SDL_CreateMutex();
		if (!midi_mutex) return 0;

		midi_play_cond = SDL_CreateCond();
		if (!midi_play_cond) {
			SDL_DestroyMutex(midi_mutex);
			midi_mutex = midi_record_mutex = midi_play_mutex = midi_port_mutex = 0;
			return 0;
		}

		midi_record_mutex = SDL_CreateMutex();
		if (!midi_record_mutex) {
			SDL_DestroyCond(midi_play_cond);
			SDL_DestroyMutex(midi_mutex);
			midi_mutex = midi_record_mutex = midi_play_mutex = midi_port_mutex = 0;
			return 0;
		}

		midi_play_mutex = SDL_CreateMutex();
		if (!midi_play_mutex) {
			SDL_DestroyCond(midi_play_cond);
			SDL_DestroyMutex(midi_record_mutex);
			SDL_DestroyMutex(midi_mutex);
			midi_mutex = midi_record_mutex = midi_play_mutex = midi_port_mutex = 0;
			return 0;
		}

		midi_port_mutex = SDL_CreateMutex();
		if (!midi_port_mutex) {
			SDL_DestroyCond(midi_play_cond);
			SDL_DestroyMutex(midi_play_mutex);
			SDL_DestroyMutex(midi_record_mutex);
			SDL_DestroyMutex(midi_mutex);
			midi_mutex = midi_record_mutex = midi_play_mutex = midi_port_mutex = 0;
			return 0;
		}
		_midi_engine_connect();
		_connected = 1;
	}
	return 1;
}
void midi_engine_reset(void)
{
	if (!_connected) return;
	midi_engine_stop();
	_midi_engine_connect();
}
void midi_engine_stop(void)
{
	struct midi_provider *n, *p;
	struct midi_port *q;
	
	if (!_connected) return;
	if (!midi_mutex) return;

	SDL_mutexP(midi_mutex);
	for (n = port_providers; n;) {
		p = n->next;

		q = 0;
		while (midi_port_foreach(p, &q)) {
			midi_port_unregister(q->num);
		}

		if (n->thread) {
			SDL_KillThread((SDL_Thread*)n->thread);
		}
		free((void*)n->name);
		free(n);
		n = p;
	}
	_connected = 0;
	SDL_mutexV(midi_mutex);
}


/* PORT system */
static struct midi_port **port_top = 0;
static int port_count = 0;
static int port_alloc = 0;

struct midi_port *midi_engine_port(int n, const char **name)
{
	struct midi_port *pv = 0;

	if (!midi_port_mutex) return 0;
	SDL_mutexP(midi_port_mutex);
	if (n >= 0 && n < port_count) {
		pv = port_top[n];
		if (name) *name = pv->name;
	}
	SDL_mutexV(midi_port_mutex);
	return pv;
}

int midi_engine_port_count(void)
{
	int pc;
	if (!midi_port_mutex) return 0;
	SDL_mutexP(midi_port_mutex);
	pc = port_count;
	SDL_mutexV(midi_port_mutex);
	return pc;
}

/* midi engines register a provider (one each!) */
struct midi_provider *midi_provider_register(const char *name,
		struct midi_driver *driver)
{
	struct midi_provider *n;

	if (!midi_mutex) return 0;

	n = mem_alloc(sizeof(struct midi_provider));
	n->name = (char *)strdup(name);
	n->poll = driver->poll;
	n->enable = driver->enable;
	n->disable = driver->disable;
	if (driver->flags & MIDI_PORT_CAN_SCHEDULE) {
		n->send_later = driver->send;
		n->send_now = NULL;
		n->drain = driver->drain;
	} else {
		n->send_later = NULL;
		n->drain = NULL;
		n->send_now = driver->send;
	}

	SDL_mutexP(midi_mutex);
	n->next = port_providers;
	port_providers = n;

	if (driver->thread) {
		n->thread = (void*)SDL_CreateThread((int (*)(void*))driver->thread, (void*)n);
	} else {
		n->thread = (void*)0;
	}

	SDL_mutexV(midi_mutex);

	return n;
}

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
	p->name = (char*)strdup(name);
	p->enable = pv->enable;
	p->disable = pv->disable;
	p->send_later = pv->send_later;
	p->send_now = pv->send_now;
	p->drain = pv->drain;

	p->free_userdata = free_userdata;
	p->userdata = userdata;
	p->provider = pv;

	for (i = 0; i < port_alloc; i++) {
		if (port_top[i] == 0) {
			port_top[i] = p;
			p->num = i;
			port_count++;
			_cfg_load_midi_part_locked(p);
			return i;
		}
	}

	SDL_mutexP(midi_port_mutex);
	port_alloc += 4;
	pt = realloc(port_top, sizeof(struct midi_port *) * port_alloc);
	if (!pt) {
		free(p->name);
		free(p);
		SDL_mutexV(midi_port_mutex);
		return -1;
	}
	pt[port_count] = p;
	for (i = port_count+1; i < port_alloc; i++) pt[i] = 0;
	port_top = pt;
	p->num = port_count;
	port_count++;

	/* finally, and just before unlocking, load any configuration for it... */
	_cfg_load_midi_part_locked(p);

	SDL_mutexV(midi_port_mutex);
	return p->num;
}

int midi_port_foreach(struct midi_provider *p, struct midi_port **cursor)
{
	int i;
	if (!midi_port_mutex) return 0;

	SDL_mutexP(midi_port_mutex);
NEXT:	if (!*cursor) {
		i = 0;
	} else {
		i = ((*cursor)->num) + 1;
		while (i < port_alloc && !port_top[i]) i++;
	}
	if (i >= port_alloc) {
		*cursor = 0;
		SDL_mutexV(midi_port_mutex);
		return 0;
	}
	*cursor = port_top[i];
	if (p && (*cursor)->provider != p) goto NEXT;
	SDL_mutexV(midi_port_mutex);
	return 1;
}

static int _midi_send_unlocked(unsigned char *data, unsigned int len, unsigned int delay,
			int from)
{
	struct midi_port *ptr;
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
	if (from == 0) {
		/* from == 0 means from immediate; everyone plays */
		ptr = 0;
		while (midi_port_foreach(NULL, &ptr)) {
			if ((ptr->io & MIDI_OUTPUT)) {
				if (ptr->send_now) ptr->send_now(ptr, data, len, 0);
				else if (ptr->send_later) ptr->send_later(ptr, data, len, 0);
			}
		}
	} else if (from == 1) {
		/* from == 1 means from buffer-flush; only "now" plays */
		ptr = 0;
		while (midi_port_foreach(NULL, &ptr)) {
			if ((ptr->io & MIDI_OUTPUT)) {
				if (ptr->send_now) ptr->send_now(ptr, data, len, 0);
			}
		}
	} else {
		/* from == 2 means from buffer-write; only "later" plays */
		ptr = 0;
		while (midi_port_foreach(NULL, &ptr)) {
			if ((ptr->io & MIDI_OUTPUT)) {
				if (ptr->send_later) ptr->send_later(ptr, data, len, delay);
				else if (ptr->send_now) need_timer = 1;
			}
		}
	}
	return need_timer;
}
void midi_send_now(unsigned char *seq, unsigned int len)
{
	if (!midi_record_mutex) return;

	SDL_mutexP(midi_record_mutex);
	_midi_send_unlocked(seq, len, 0, 0);
	SDL_mutexV(midi_record_mutex);
}

/*----------------------------------------------------------------------------------*/

struct midi_pl {
	unsigned int pos;
	unsigned char buffer[4096];
	unsigned int len;
	struct midi_pl *next;
	/* used by timer */
	unsigned long rpos;
};

static struct midi_pl *top = 0;
static struct midi_pl *top_free = 0;
static SDL_Thread *midi_sync_thread = 0;

static struct midi_pl *ready = 0;

static int __out_detatched(UNUSED void *xtop)
{
	struct midi_pl *x, *y;

#ifdef WIN32
	__win32_pick_usleep();
	SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);
	SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL);
	/*SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);*/
#endif

	SDL_mutexP(midi_play_mutex);
STARTFRAME:
	do {
		SDL_CondWait(midi_play_cond, midi_play_mutex);
		x = y = (struct midi_pl *)ready;
	} while (!x);
	ready = 0; /* sanity check */
	for (;;) {
		SDL_mutexP(midi_record_mutex);
		_midi_send_unlocked(x->buffer, x->len, 0, 1);
		SDL_mutexV(midi_record_mutex);

		if (!x->next) {
			/* remove them all */
			SDL_mutexP(midi_record_mutex);
			x->next = top_free;
			top_free = y;
			SDL_mutexV(midi_record_mutex);
			goto STARTFRAME;
		}
		x = x->next;
		if (x->rpos) SLEEP_FUNC(x->rpos);
	}
	/* this is dead code because gcc is brain damaged and storlek
	thinks gcc knows better than me. */
	return 0;
}

void midi_send_flush(void)
{
	struct midi_pl *x;
	struct midi_port *ptr;
	unsigned int acc;
	int need_explicit_flush = 0;

	if (!midi_record_mutex || !midi_play_mutex) return;

	ptr = 0;
	while (midi_port_foreach(NULL, &ptr)) {
		if ((ptr->io & MIDI_OUTPUT)) {
			if (ptr->drain) ptr->drain(ptr);
			else if (ptr->send_now) need_explicit_flush=1;
		}
	}

	/* no need for midi sync huzzah; driver does it for us... */
	if (!need_explicit_flush) return;

	while (!midi_sync_thread && top) {
		midi_sync_thread = SDL_CreateThread(__out_detatched, top);
		if (midi_sync_thread) {
			log_appendf(3, "Started MIDI sync thread");
			break;
		}
		log_appendf(2, "AAACK: Couldn't start off MIDI, sending ahead");

		SDL_mutexP(midi_record_mutex);
		_midi_send_unlocked(top->buffer, top->len, 0, 1);
		x = top->next;
		top->next = top_free;
		top_free = top;
		top = x;
		SDL_mutexV(midi_record_mutex);
	}
	if (!top) return;

	SDL_mutexP(midi_record_mutex);
	/* calculate relative */
	acc = 0;
	for (x = top; x; x = x->next) {
		x->rpos = x->pos - acc;
		acc = x->pos;
	}

	x = top;
	top = 0;
	SDL_mutexV(midi_record_mutex);

	if (x) {
		SDL_mutexP(midi_play_mutex);
		ready = x;
		SDL_CondSignal(midi_play_cond);
		SDL_mutexV(midi_play_mutex);
	}
}
static void __add_pl(struct midi_pl *x, unsigned char *data, unsigned int len)
{
	struct midi_pl *q;
	int dr;

#if 0
printf("adding %u bytes\n",len);
#endif
RECURSE:
	if (len + x->len < sizeof(x->buffer)) {
		memcpy(x->buffer + x->len, data, len);
		x->len += len;
		return;
	}

	memcpy(x->buffer + x->len, data, dr = (sizeof(x->buffer) - x->len));
	len -= dr;

	if (len > 0) {
		/* overflow */
		if (top_free) {
			q = top_free;
			top_free = top_free->next;
		} else {
			q = mem_alloc(sizeof(struct midi_pl));
		}
		q->pos = x->pos;
		q->len = 0;
		q->next = x->next;
		x->next = q;
		x = q;
		goto RECURSE;	
	}
}

void midi_send_buffer(unsigned char *data, unsigned int len, unsigned int pos)
{
	struct midi_pl *x, *lx, *y;

	pos /= song_buffer_msec();

	if (!midi_record_mutex) return;

	SDL_mutexP(midi_record_mutex);

	/* just for fun... */
	if (status.current_page == PAGE_MIDI) {
		status.last_midi_real_len = len;
		if (len > sizeof(status.last_midi_event)) {
			status.last_midi_len = sizeof(status.last_midi_event);
		} else {
			status.last_midi_len = len;
		}
		memcpy(status.last_midi_event, data, status.last_midi_len);
		status.flags |= MIDI_EVENT_CHANGED;
		status.last_midi_port = 0;
		time(&status.last_midi_time);
		status.flags |= NEED_UPDATE;
	}

	/* pos is still in miliseconds */
	if (!_midi_send_unlocked(data, len, pos, 2)) {
		/* rock, we don't need a timer... */
		goto TAIL;
	}


	pos *= 1000; /* microseconds for usleep */

	lx = 0;
	x = top;
	while (x) {
		if (x->pos == pos) {
			__add_pl(x, data, len);
			goto TAIL;
		}
		if (x->pos > pos) break; /* but after lx */
		lx = x;
		x = x->next;
	}
	if (top_free) {
		y = top_free;
		top_free = top_free->next;
	} else {
		y = mem_alloc(sizeof(struct midi_pl));
	}
	y->next = x;
	if (lx) {
		lx->next = y;
	} else {
		/* then x = top */
		top = y;
	}
	y->pos = pos;
	y->len = 0;
	__add_pl(y, data, len);
TAIL:	SDL_mutexV(midi_record_mutex);
}

/*----------------------------------------------------------------------------------*/
void midi_port_unregister(int num)
{
	struct midi_port *q;
	int i;

	if (!midi_port_mutex) return;

	SDL_mutexP(midi_port_mutex);
	for (i = 0; i < port_alloc; i++) {
		if (port_top[i] && port_top[i]->num == num) {
			q = port_top[i];
			if (q->disable) q->disable(q);
			if (q->free_userdata) free(q->userdata);
			free(q);

			port_top[i] = 0;
			port_count--;
			break;
		}
	}
	SDL_mutexV(midi_port_mutex);
}

void midi_received_cb(struct midi_port *src, unsigned char *data, unsigned int len)
{
	unsigned char d4[4];
	int cmd;

	if (!len) return;
	if (len < 4) {
		memset(d4,0,sizeof(d4));
		memcpy(d4,data,len);
		data = d4;
	}

	/* just for fun... */
	SDL_mutexP(midi_record_mutex);
	status.last_midi_real_len = len;
	if (len > sizeof(status.last_midi_event)) {
		status.last_midi_len = sizeof(status.last_midi_event);
	} else {
		status.last_midi_len = len;
	}
	memcpy(status.last_midi_event, data, status.last_midi_len);
	status.flags |= MIDI_EVENT_CHANGED;
	status.last_midi_port = src;
	time(&status.last_midi_time);
	SDL_mutexV(midi_record_mutex);

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
	int *st;
	SDL_Event e;

	st = mem_alloc(sizeof(int)*4);
	st[0] = mnstatus;
	st[1] = channel;
	st[2] = note;
	st[3] = velocity;
	e.user.type = SCHISM_EVENT_MIDI;
	e.user.code = SCHISM_EVENT_MIDI_NOTE;
	e.user.data1 = st;
	e.user.data2 = 0;
	SDL_PushEvent(&e);
}
void midi_event_controller(int channel, int param, int value)
{
	int *st;
	SDL_Event e;

	st = mem_alloc(sizeof(int)*4);
	st[0] = value;
	st[1] = channel;
	st[2] = param;
	st[3] = 0;
	e.user.type = SCHISM_EVENT_MIDI;
	e.user.code = SCHISM_EVENT_MIDI_CONTROLLER;
	e.user.data1 = st;
	e.user.data2 = 0;
	SDL_PushEvent(&e);
}
void midi_event_program(int channel, int value)
{
	int *st;
	SDL_Event e;

	st = mem_alloc(sizeof(int)*4);
	st[0] = value;
	st[1] = channel;
	st[2] = 0;
	st[3] = 0;
	e.user.type = SCHISM_EVENT_MIDI;
	e.user.code = SCHISM_EVENT_MIDI_PROGRAM;
	e.user.data1 = st;
	e.user.data2 = 0;
	SDL_PushEvent(&e);
}
void midi_event_aftertouch(int channel, int value)
{
	int *st;
	SDL_Event e;

	st = mem_alloc(sizeof(int)*4);
	st[0] = value;
	st[1] = channel;
	st[2] = 0;
	st[3] = 0;
	e.user.type = SCHISM_EVENT_MIDI;
	e.user.code = SCHISM_EVENT_MIDI_AFTERTOUCH;
	e.user.data1 = st;
	e.user.data2 = 0;
	SDL_PushEvent(&e);
}
void midi_event_pitchbend(int channel, int value)
{
	int *st;
	SDL_Event e;

	st = mem_alloc(sizeof(int)*4);
	st[0] = value;
	st[1] = channel;
	st[2] = 0;
	st[3] = 0;
	e.user.type = SCHISM_EVENT_MIDI;
	e.user.code = SCHISM_EVENT_MIDI_PITCHBEND;
	e.user.data1 = st;
	e.user.data2 = 0;
	SDL_PushEvent(&e);
}
void midi_event_system(int argv, int param)
{
	int *st;
	SDL_Event e;

	st = mem_alloc(sizeof(int)*4);
	st[0] = argv;
	st[1] = param;
	st[2] = 0;
	st[3] = 0;
	e.user.type = SCHISM_EVENT_MIDI;
	e.user.code = SCHISM_EVENT_MIDI_SYSTEM;
	e.user.data1 = st;
	e.user.data2 = 0;
	SDL_PushEvent(&e);
}
void midi_event_tick(void)
{
	SDL_Event e;

	e.user.type = SCHISM_EVENT_MIDI;
	e.user.code = SCHISM_EVENT_MIDI_TICK;
	e.user.data1 = 0;
	e.user.data2 = 0;
	SDL_PushEvent(&e);
}
void midi_event_sysex(const unsigned char *data, unsigned int len)
{
	SDL_Event e;

	e.user.type = SCHISM_EVENT_MIDI;
	e.user.code = SCHISM_EVENT_MIDI_SYSEX;
	e.user.data1 = mem_alloc(len+sizeof(len));
	memcpy(e.user.data1, &len, sizeof(len));
	memcpy(e.user.data1+sizeof(len), data, len);
	e.user.data2 = 0;
	SDL_PushEvent(&e);
}

int midi_engine_handle_event(void *ev)
{
	struct key_event kk;
	unsigned int len;
	char *sysex;
	int *st;

	SDL_Event *e = (SDL_Event *)ev;
	if (e->type != SCHISM_EVENT_MIDI) return 0;

	if (midi_flags & MIDI_DISABLE_RECORD) return 1;

	memset(&kk, 0, sizeof(kk));
	kk.is_synthetic = 0;

	st = e->user.data1;
	switch (e->user.code) {
	case SCHISM_EVENT_MIDI_NOTE:
		if (st[0] == MIDI_NOTEON) {
			kk.state = 0;
		} else {
			if (!(midi_flags & MIDI_RECORD_NOTEOFF)) {
				/* don't record noteoff? okay... */
				break;
			}
			kk.state = 1;
		}
		kk.midi_channel = st[1]+1;
		kk.midi_note = (st[2]+1 + midi_c5note) - 60;
		if (midi_flags & MIDI_RECORD_VELOCITY)
			kk.midi_volume = st[3];
		else
			kk.midi_volume = 128;
		kk.midi_volume = (kk.midi_volume * midi_amplification) / 100;
		handle_key(&kk);
		break;
	case SCHISM_EVENT_MIDI_PITCHBEND:
		/* wheel */
		kk.midi_channel = st[1]+1;
		kk.midi_volume = -1;
		kk.midi_note = -1;
		kk.midi_bend = st[0];
		handle_key(&kk);
		break;
	case SCHISM_EVENT_MIDI_CONTROLLER:
		/* controller events */
		break;
	case SCHISM_EVENT_MIDI_SYSTEM:
		switch (st[0]) {
		case 0xA: /* MIDI start */
		case 0xB: /* MIDI continue */
			break;
		case 0xC: /* MIDI stop */
		case 0xF: /* MIDI reset */
			/* this is helpful when miditracking */
			song_stop();
			break;
		};
	case SCHISM_EVENT_MIDI_SYSEX:
		/* but missing the F0 and the stop byte (F7) */
		len = *((unsigned int *)e->user.data1);
		sysex = ((char *)e->user.data1)+sizeof(unsigned int);
		break;

	default:
		break;
	}
	free(e->user.data1);

/* blah... */

	return 1;
}
