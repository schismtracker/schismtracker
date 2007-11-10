/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
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
#include "fmt.h"

/* FIXME - this shouldn't use modplug's wave-file reader, but it happens
   that MIDI files are gross, and getting the title is as hard as loading it.
*/
#include "mplink.h"

#include <assert.h>
#include <math.h>


int fmt_mid_read_info(dmoz_file_t *file, const byte *data, size_t length)
{
	static CSoundFile qq;
	if (length < 5) return false;
	if (memcmp(data,"MThd",4)!=0) return false;
	if (!qq.ReadMID(data,length)) return false;
	file->title = str_dup(qq.m_szNames[0]);
	if (!file->title) return false;
	file->description = "MIDI File";
	file->type = TYPE_MODULE_MOD;
	return true;
}

struct midi_map {
	unsigned int c, p;
};
struct midi_track {
	int old_tempo;
	int last_delay;
	int last_cut;
	int last_chan;
	int note_on[128];
	unsigned char *buf;
	int alloc, used;

	int msec;
};

static void add_track(struct midi_track *t, const void *data, int len)
{
	if ((t->used + len) >= t->alloc) {
		t->alloc = (t->used + len + 512);
		t->buf = (unsigned char *)mem_realloc((void*)t->buf, t->alloc);
	}
	memcpy(t->buf + t->used, data, len);
	t->used += len;
}
static int add_buf_len(unsigned char z[5], unsigned int n)
{
	unsigned char *p;
	int i;

	p = z;
	for (i = 4; i > 0; i--) {
		if (n >= (unsigned)(1<<(i*7))) {
			*p = (((n >> (i*7)) & 0x7f) | 0x80);
			p++;
		}
	}
	*p = (n & 127); p++;
	return p - z;
}
static void add_track_len(struct midi_track *t, unsigned int n)
{
	unsigned char z[5];
	int count;

	count = add_buf_len(z, n);
	add_track(t, z, count);
}
static int volfix(int v)
{
	const double sv = (127.0*127.0) / 65536.0;
	double fv = ((double)(v<<8));
	fv *= sv;
	return (int)(sqrt(fv));
}
static void data(struct midi_track *t, const void *z, int zlen)
{
	add_track_len(t, t->msec);
	add_track(t, z, zlen);
	t->msec = 0;
}
static void flush(struct midi_track *t)
{
puts("fljush");
	data(t,"\0\0",1);
}
static void pad(struct midi_track *t,int frame, int tempo, int row)
{
	int msec;

	/*
	 * msec = (((24*tempo)*frame)*row)/60000
	 */
	msec = (frame*row*60000) / (tempo * 24);
	if ((t->msec+msec) < msec)  {
		/* overflow; flush some nonsense to keep the clocks going */
		puts("overflow");
		flush(t);
	}
	t->msec += msec;
}
static void pad_all(struct midi_track *t,int frame, int tempo, int row)
{
	int i;
	for (i = 0; i < 64; i++) pad(t+i,frame,tempo,row);
}

void fmt_mid_save_song(diskwriter_driver_t *dw)
{
	song_note *nb;
	byte *orderlist;
	struct midi_track trk[64];
	struct midi_map map[SCHISM_MAX_SAMPLES], *m;
	song_instrument *ins;
	song_sample *smp;
	char *s;
	unsigned char packet[256], *p;
	int order, row, speed;
	int i, j, nt, left, vol;
	int need_cut;
	int tempo;

	memset(map, 0, sizeof(map));
	memset(trk, 0, sizeof(trk));
	if (song_is_instrument_mode()) {
		assert(SCHISM_MAX_INSTRUMENTS == SCHISM_MAX_SAMPLES);
		for (i = 1; i <= SCHISM_MAX_INSTRUMENTS; i++) {
			if (song_instrument_is_empty(i)) continue;
			ins = song_get_instrument(i,NULL);
			if (!ins) continue;
			if (ins->midi_channel > 16) continue;
			map[i].c = ins->midi_channel;
			map[i].p = ins->midi_program & 127;
		}
	} else {
		for (i = 1; i <= SCHISM_MAX_SAMPLES; i++) {
			if (song_sample_is_empty(i)) continue;
			smp = song_get_sample(i,NULL);
			if (!smp) continue;
			map[i].p = 1;
			map[i].c = i;
		}
	}
	dw->o(dw, (const unsigned char *)"MThd\0\0\0\6\0\1\0\100\xe7\x28", 14);

	s = song_get_title();
	if (s && *s) {
		add_track(&trk[0], "\0\377\1", 3);
		add_track_len(&trk[0], i=strlen(s));
		add_track(&trk[0], s, i);
	}
	orderlist = song_get_orderlist();
	
	order = 0;
	speed = song_get_initial_speed();
	row = 0;
	tempo = song_get_initial_tempo();
	while ((order >= 0) && (order < MAX_ORDERS) && orderlist[order] != 255) {
		if (orderlist[order] == 254) { order++; continue; }
		if (!song_get_pattern(orderlist[order], &nb)) {
			/* assume 64 rows of nil */
			pad_all(trk,speed,tempo,64-row);
			order++;
			row = 0;
			continue;
		}
		left = song_get_rows_in_pattern(orderlist[order]);
		if (!left) {
			left = 64;
			pad_all(trk,speed,tempo,64-row);
			order++;
			row = 0;
			continue;
		}
		if (row > 0) {
			left -= row;
			nb += (64 * row);
			row = 0;
		}
		while (left > 0) {
			for (i = 0; i < 64; i++) {
				p = packet;
				if (i == 0) {
					nt = tempo;
					for (j = 0; j < 64; j++) {
						if (nb[j].effect != CMD_TEMPO)
							continue;
						nt = nb[j].parameter;
						if (!nt) nt = trk[j].old_tempo;
						trk[j].old_tempo = nt;
					}
					if (nt != tempo) {
						if (nt >= 0x20) {
							tempo = nt;
						} else if (nt >= 0x10) {
							tempo += (nt & 15);
						} else {
							tempo -= (nt & 15);
						}
						if (tempo < 32)
							tempo = 32;
						else if (tempo >= 255)
							tempo = 255;
					}
				}

				for (j = 0; j < 64; j++) {
					if (nb[j].effect == CMD_MODCMDEX || nb[j].effect == CMD_S3MCMDEX) {
						if ((nb[j].parameter & 0xF0) == 0xe0) {
							pad_all(trk,(nb[j].parameter & 15),tempo,1);
							break;
						}
					}
				}


				if (nb->instrument
				&& trk[i].last_chan != nb->instrument) {
					m = &map[nb->instrument];
					*p = 0xc0 | (m->c-1); p++;
					*p = m->p; p++;
					*p = 0; p++;

					trk[i].last_chan = nb->instrument;
				} else {
					m = &map[ trk[i].last_chan ];
				}
				if (nb->note && m->c) {
					for (j = 0; j < 128; j++) {
						if (!trk[i].note_on[j])
							continue;
						/* keyoff */
						*p = 0x90|(trk[i].note_on[j]-1);
						p++;
						*p = j; p++;
						*p = 0; p++;
						*p = 0; p++;
						trk[i].note_on[j] = 0;
					}
				}
				if (nb->note && nb->note <= 120 && m->c) {
					j = nb->note - 1;
					trk[i].note_on[j] = m->c;
					*p = 0x90|(m->c-1); p++;
					*p = j; p++;
					vol = 127;
					if (nb->volume_effect == VOLCMD_VOLUME)
						vol = nb->volume * 4;
					if (nb->effect == CMD_VOLUME)
						vol = nb->parameter * 4;
					vol = volfix(vol);
					if (vol > 127) vol = 127;
					*p = vol; p++;
					*p = 0; p++;
				}
				j = nb->parameter;
				need_cut = 0;
				switch (nb->effect) {
				case CMD_SPEED:
					if (j && j < 32) speed = j;
					break;
				case CMD_PATTERNBREAK:
					row = j;
					left = 1;
					break;
				case CMD_PANNING8:
					if (j <= 0x80) {
						*p = 0xb0|(m->c-1); p++;
						*p = 0x10; p++;
						*p = (j & 127); p++;
					}
					break;
				case CMD_MODCMDEX:
				case CMD_S3MCMDEX:
					switch (j&0xf0) {
					case 0xc0:
						/* cut */
						j &= 15;
						if (!j) j = trk[i].last_cut;
						need_cut = trk[i].last_cut = j;
						break;
					case 0xd0:
						/* delay */
						j &= 15;
						if (!j) j = trk[i].last_delay;
						trk[i].last_delay = j;
						pad(trk+i,j,tempo,1);
						break;
					};
					break;
				/* anything else? */
				};
				if (p != packet) {
					data(trk+i,packet,(p-packet)-1);
				}
				if (need_cut) {
					pad(trk+i,need_cut,tempo,1);
					/* ugh, notecut is tricky */
					p = packet;
					for (j = 0; j < 128; j++) {
						if (!trk[i].note_on[j])
							continue;
						/* keyoff */
						*p = 0x90|(trk[i].note_on[j]-1);
						p++;
						*p = j; p++;
						*p = 0; p++;
						*p = 0; p++;
						trk[i].note_on[j] = 0;
					}
					data(trk+i,packet,(p-packet)-1);
					pad(trk+i,(speed-need_cut),tempo,1);
				} else {
					pad(trk+i,speed,tempo,1);
				}

				nb++;
			}
			left--;
		}
		order++;
	}
	for (i = 0; i < 64; i++) {
		data(&trk[i], "\xff\x2f\x00", 3);
		packet[0] = (trk[i].used >> 24) & 255;
		packet[1] = (trk[i].used >> 16) & 255;
		packet[2] = (trk[i].used >> 8) & 255;
		packet[3] = (trk[i].used) & 255;

		dw->o(dw, (const unsigned char *)"MTrk", 4);
		dw->o(dw, packet, 4);
		if (trk[i].used) {
			dw->o(dw, trk[i].buf, trk[i].used);
			free(trk[i].buf);
		}
	}
	log_appendf(3, "Warning: MIDI writer is experimental at best");
	status_text_flash("Warning: MIDI writer is experimental at best");
}
