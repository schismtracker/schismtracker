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

#include <alsa/asoundlib.h>
#ifdef USE_DLTRICK_ALSA
/* ... */
#include <alsa/control.h>
#endif
#include <alsa/seq.h>

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

/* okay, we do the same trick SDL does to get our alsa library put together */
#ifdef USE_DLTRICK_ALSA
/* alright, some explanation:

The libSDL library on Linux doesn't "link with" the alsa library (-lasound)
so that dynamically-linked binaries using libSDL on Linux will work on systems
that don't have libasound.so.2 anywhere.

There DO EXIST generic solutions (relaytool) but they interact poorly with what
SDL is doing, so here is my ad-hoc solution:

We define a bunch of these routines similar to how the dynamic linker does it
when RTLD_LAZY is used. There might be a slight performance increase if we
linked them all at once (like libSDL does), but this is certainly a lot easier
to inline.

If you need additional functions in -lasound in schism, presently they will
have to be declared here for my binary builds to work.

to use:
        size_t snd_seq_port_info_sizeof(void);

add here:
        _any_dltrick(size_t,snd_seq_port_info_sizeof,(void),())

(okay, that one is already done). Here's another one:

        int snd_mixer_selem_get_playback_volume(snd_mixer_elem_t *e,
                                snd_mixer_selem_channel_id_t ch,
                                long *v);

gets:
        _any_dltrick(int,snd_mixer_selem_get_playback_volume,
        (snd_mixer_elem_t*e,snd_mixer_selem_channel_id_t ch,long*v),(e,ch,v))

If they return void, like:
        void snd_midi_event_reset_decode(snd_midi_event_t *d);
use:
        _void_dltrick(snd_midi_event_reset_decode,(snd_midi_event_t*d),(d))

None of this code is used, btw, if --enable-alsadltrick isn't supplied to
the configure script, so to test it, you should use that when developing.

*/


#include <dlfcn.h>

extern void *_dltrick_handle;

/* don't try this at home... */
#define _void_dltrick(a,b,c) static void (*_dltrick_ ## a)b = NULL; \
void a b { if (!_dltrick_##a) _dltrick_##a = dlsym(_dltrick_handle, #a); \
if (!_dltrick_##a) abort(); _dltrick_ ## a c; }

#define _any_dltrick(r,a,b,c) static r (*_dltrick_ ## a)b = NULL; \
r a b { if (!_dltrick_##a) _dltrick_##a = dlsym(_dltrick_handle, #a); \
if (!_dltrick_##a) abort(); return _dltrick_ ## a c; }


_any_dltrick(size_t,snd_seq_port_info_sizeof,(void),())
_any_dltrick(size_t,snd_seq_client_info_sizeof,(void),())

_any_dltrick(int,snd_seq_control_queue,(snd_seq_t*s,int q,int type, int value, snd_seq_event_t *ev),
        (s,q,type,value,ev))

_any_dltrick(int,snd_seq_queue_tempo_malloc,(snd_seq_queue_tempo_t**ptr),(ptr))
_void_dltrick(snd_seq_queue_tempo_set_tempo,(snd_seq_queue_tempo_t *info, unsigned int tempo),(info,tempo))
_void_dltrick(snd_seq_queue_tempo_set_ppq,(snd_seq_queue_tempo_t *info, int ppq),(info,ppq))
_any_dltrick(int,snd_seq_set_queue_tempo,(snd_seq_t *handle, int q, snd_seq_queue_tempo_t *tempo),
        (handle,q,tempo))
_any_dltrick(long,snd_midi_event_encode,
(snd_midi_event_t *dev,const unsigned char *buf,long count,snd_seq_event_t *ev),
(dev,buf,count,ev))
_any_dltrick(int,snd_seq_event_output,
(snd_seq_t *handle, snd_seq_event_t *ev),
(handle,ev))
_any_dltrick(int,snd_seq_alloc_queue,(snd_seq_t*h),(h))
_any_dltrick(int,snd_seq_free_event,
(snd_seq_event_t *ev),
(ev))
_any_dltrick(int,snd_seq_connect_from,
(snd_seq_t*seeq,int my_port,int src_client, int src_port),
(seeq,my_port,src_client,src_port))
_any_dltrick(int,snd_seq_connect_to,
(snd_seq_t*seeq,int my_port,int dest_client,int dest_port),
(seeq,my_port,dest_client,dest_port))
_any_dltrick(int,snd_seq_disconnect_from,
(snd_seq_t*seeq,int my_port,int src_client, int src_port),
(seeq,my_port,src_client,src_port))
_any_dltrick(int,snd_seq_disconnect_to,
(snd_seq_t*seeq,int my_port,int dest_client,int dest_port),
(seeq,my_port,dest_client,dest_port))
_any_dltrick(const char *,snd_strerror,(int errnum),(errnum))
_any_dltrick(int,snd_seq_poll_descriptors_count,(snd_seq_t*h,short e),(h,e))
_any_dltrick(int,snd_seq_poll_descriptors,(snd_seq_t*h,struct pollfd*pfds,unsigned int space, short e),
        (h,pfds,space,e))
_any_dltrick(int,snd_seq_event_input,(snd_seq_t*h,snd_seq_event_t**ev),(h,ev))
_any_dltrick(int,snd_seq_event_input_pending,(snd_seq_t*h,int fs),(h,fs))
_any_dltrick(int,snd_midi_event_new,(size_t s,snd_midi_event_t **rd),(s,rd))
_any_dltrick(long,snd_midi_event_decode,
(snd_midi_event_t *dev,unsigned char *buf,long count, const snd_seq_event_t*ev),
(dev,buf,count,ev))
_void_dltrick(snd_midi_event_reset_decode,(snd_midi_event_t*d),(d))
_any_dltrick(int,snd_seq_create_simple_port,
(snd_seq_t*h,const char *name,unsigned int caps,unsigned int type),
(h,name,caps,type))
_any_dltrick(int,snd_seq_drain_output,(snd_seq_t*h),(h))
_any_dltrick(int,snd_seq_query_next_client,
(snd_seq_t*h,snd_seq_client_info_t*info),(h,info))
_any_dltrick(int,snd_seq_client_info_get_client,
(const snd_seq_client_info_t *info),(info))
_void_dltrick(snd_seq_client_info_set_client,(snd_seq_client_info_t*inf,int cl),(inf,cl))
_void_dltrick(snd_seq_port_info_set_client,(snd_seq_port_info_t*inf,int cl),(inf,cl))
_void_dltrick(snd_seq_port_info_set_port,(snd_seq_port_info_t*inf,int pl),(inf,pl))
_any_dltrick(int,snd_seq_query_next_port,(snd_seq_t*h,snd_seq_port_info_t*inf),(h,inf))
_any_dltrick(unsigned int,snd_seq_port_info_get_capability,
(const snd_seq_port_info_t *inf),(inf))
_any_dltrick(int,snd_seq_port_info_get_client,(const snd_seq_port_info_t*inf),(inf))
_any_dltrick(int,snd_seq_port_info_get_port,(const snd_seq_port_info_t*inf),(inf))
_any_dltrick(const char *,snd_seq_client_info_get_name,(snd_seq_client_info_t*inf),(inf))
_any_dltrick(const char *,snd_seq_port_info_get_name,(const snd_seq_port_info_t*inf),(inf))
_any_dltrick(int,snd_seq_open,(snd_seq_t**h,const char *name,int str, int mode),
(h,name,str,mode))
_any_dltrick(int,snd_seq_set_client_name,(snd_seq_t*seeq,const char *name),(seeq,name))
#endif

/* see mixer-alsa.c */
#undef assert
#define assert(x)

static void _alsa_drain(struct midi_port *p UNUSED)
{
        /* not port specific */
        snd_seq_drain_output(seq);
}
static void _alsa_send(struct midi_port *p, const unsigned char *data, unsigned int len, unsigned int delay)
{
        struct alsa_midi *ex;
        snd_seq_event_t ev;
        long rr;

        ex = (struct alsa_midi *)p->userdata;

        while (len > 0) {
                snd_seq_ev_clear(&ev);
                snd_seq_ev_set_source(&ev, local_port);
                snd_seq_ev_set_subs(&ev);
                if (!delay) {
                        snd_seq_ev_set_direct(&ev);
                } else {
                        snd_seq_ev_schedule_tick(&ev, alsa_queue, 1, delay);
                }

                /* we handle our own */
                ev.dest.port = ex->p;
                ev.dest.client = ex->c;

                rr = snd_midi_event_encode(ex->dev, data, len, &ev);
                if (rr < 1) break;
                snd_seq_event_output(seq, &ev);
                snd_seq_free_event(&ev);
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
        if (p->io & MIDI_INPUT) {
                err = snd_seq_connect_from(seq, 0, data->c, data->p);
        }
        if (p->io & MIDI_OUTPUT) {
                err = snd_seq_connect_to(seq, 0, data->c, data->p);
        }
        if (err < 0) {
                log_appendf(4, "ALSA: %s", snd_strerror(err));
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
        if (p->io & MIDI_OUTPUT) {
                err = snd_seq_disconnect_to(seq, 0, data->c, data->p);
        }
        if (p->io & MIDI_INPUT) {
                err = snd_seq_disconnect_from(seq, 0, data->c, data->p);
        }
        if (err < 0) {
                log_appendf(4, "ALSA: %s", snd_strerror(err));
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

        npfd = snd_seq_poll_descriptors_count(seq, POLLIN);
        if (npfd <= 0) return 0;

        pfd = (struct pollfd *)mem_alloc(npfd * sizeof(struct pollfd));
        if (!pfd) return 0;

        for (;;) {
                if (snd_seq_poll_descriptors(seq, pfd, npfd, POLLIN) != npfd) {
                        free(pfd);
                        return 0;
                }

                (void)poll(pfd, npfd, -1);
                do {
                        if (snd_seq_event_input(seq, &ev) < 0) {
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
                                snd_seq_free_event(ev);
                                continue;
                        }

                        if (!dev && snd_midi_event_new(sizeof(big_midi_buf), &dev) < 0) {
                                /* err... */
                                break;
                        }

                        s = snd_midi_event_decode(dev, big_midi_buf,
                                        sizeof(big_midi_buf), ev);
                        if (s > 0) midi_received_cb(src, big_midi_buf, s);
                        snd_midi_event_reset_decode(dev);
                        snd_seq_free_event(ev);
                } while (snd_seq_event_input_pending(seq, 0) > 0);
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

        if (local_port == -1) {

                local_port = snd_seq_create_simple_port(seq,
                                PORT_NAME,
                                SND_SEQ_PORT_CAP_READ
                        |       SND_SEQ_PORT_CAP_WRITE
                        |       SND_SEQ_PORT_CAP_SYNC_READ
                        |       SND_SEQ_PORT_CAP_SYNC_WRITE
                        |       SND_SEQ_PORT_CAP_DUPLEX
                        |       SND_SEQ_PORT_CAP_SUBS_READ
                        |       SND_SEQ_PORT_CAP_SUBS_WRITE,

                                SND_SEQ_PORT_TYPE_APPLICATION
                        |       SND_SEQ_PORT_TYPE_SYNTH
                        |       SND_SEQ_PORT_TYPE_MIDI_GENERIC
                        |       SND_SEQ_PORT_TYPE_MIDI_GM
                        |       SND_SEQ_PORT_TYPE_MIDI_GS
                        |       SND_SEQ_PORT_TYPE_MIDI_XG
                        |       SND_SEQ_PORT_TYPE_MIDI_MT32);
        } else {
                /* XXX check to see if changes have been made */
                return;
        }

        ptr = NULL;
        while (midi_port_foreach(_alsa_provider, &ptr)) {
                data = (struct alsa_midi *)ptr->userdata;
                data->mark = 0;
        }

        snd_seq_client_info_alloca(&cinfo);
        snd_seq_port_info_alloca(&pinfo);
        snd_seq_client_info_set_client(cinfo, -1);
        while (snd_seq_query_next_client(seq, cinfo) >= 0) {
                int cn = snd_seq_client_info_get_client(cinfo);
                snd_seq_port_info_set_client(pinfo, cn);
                snd_seq_port_info_set_port(pinfo, -1);
                while (snd_seq_query_next_port(seq, pinfo) >= 0) {
                        io = 0;
                        if ((snd_seq_port_info_get_capability(pinfo)
                         & (SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ))
                        == (SND_SEQ_PORT_CAP_READ|SND_SEQ_PORT_CAP_SUBS_READ))
                                io |= MIDI_INPUT;
                        if ((snd_seq_port_info_get_capability(pinfo)
                         & (SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE))
                        == (SND_SEQ_PORT_CAP_WRITE|SND_SEQ_PORT_CAP_SUBS_WRITE))
                                io |= MIDI_OUTPUT;

                        if (!io) continue;

                        c = snd_seq_port_info_get_client(pinfo);
                        if (c == SND_SEQ_CLIENT_SYSTEM) continue;

                        p = snd_seq_port_info_get_port(pinfo);
                        ptr = NULL;
                        ok = 0;
                        while (midi_port_foreach(_alsa_provider, &ptr)) {
                                data = (struct alsa_midi *)ptr->userdata;
                                if (data->c == c && data->p == p) {
                                        data->mark = 1;
                                        ok = 1;
                                }
                        }
                        if (ok) continue;
                        ctext = snd_seq_client_info_get_name(cinfo);
                        ptext = snd_seq_port_info_get_name(pinfo);
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

                        if (snd_midi_event_new(MIDI_BUFSIZE, &data->dev) < 0) {
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
int alsa_midi_setup(void)
{
        static snd_seq_queue_tempo_t *tempo;
        static struct midi_driver driver;

        /* only bother if alsa midi actually exists, otherwise this will
        produce useless and annoying error messages on systems where alsa
        libs are installed but which aren't actually running it */
        struct stat sbuf;
        if (stat("/dev/snd/seq", &sbuf) != 0)
                return 0;


#ifdef USE_DLTRICK_ALSA
        if (!dlsym(_dltrick_handle,"snd_seq_open")) return 0;
#endif
        driver.poll = _alsa_poll;
        driver.thread = _alsa_thread;
        driver.enable = _alsa_start;
        driver.disable = _alsa_stop;
        driver.send = _alsa_send;
        driver.flags = MIDI_PORT_CAN_SCHEDULE;
        driver.drain = _alsa_drain;

        if (snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0) < 0
        || snd_seq_set_client_name(seq, PORT_NAME) < 0) {
                return 0;
        }

        alsa_queue = snd_seq_alloc_queue(seq);
        snd_seq_queue_tempo_malloc(&tempo);
        snd_seq_queue_tempo_set_tempo(tempo,480000);
        snd_seq_queue_tempo_set_ppq(tempo, 480);
        snd_seq_set_queue_tempo(seq, alsa_queue, tempo);
        snd_seq_start_queue(seq, alsa_queue, NULL);
        snd_seq_drain_output(seq);

        if (!midi_provider_register("ALSA", &driver)) return 0;
        return 1;
}


#endif
