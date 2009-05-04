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

/* midiwriter */
#define GCF(z) &g->midi_global_data[z]
struct midi_map {
	unsigned int c, p, b;
	int nna;
};
struct midi_track {
	int muted;
	int old_tempo;
	int last_delay;
	int last_cut;
	int last_chan;
	int note_on[128];
	unsigned char *buf;
	int alloc, used;

	int pan, vol;
	unsigned int bank;

	int msec;

	int active_macro;
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
	return (int)(127.0*(log(v)/log(127)));
}
static void data(struct midi_track *t, const void *z, int zlen)
{
	if (zlen < 0) return;
	if (t->msec || !t->used) add_track_len(t, t->msec);
	add_track(t, z, zlen);
	t->msec = 0;
}
static void pad(struct midi_track *t,int frame, int tempo, int row)
{
	int msec;

	if (frame <= 0 || row <= 0 || tempo <= 0) return;

	msec = (frame * row * 480) / tempo;

	/*
	 * overflow checking here aside, this checks for a very
	 * pathological case where we have a delay in a channel that
	 * lasts for more than three days. in fact, this code *was* tested
	 *
	 * the midi spec doesn't cope with this, but if i make a corrupt file,
	 * even by accident, people will be very unhappy, so I put a
	 * garbage comment- players *must* deal with this, so this can be used
	 * to make extra long delays...
	 */
	if (((t->msec+msec) < msec) || ((t->msec+msec)>0x0FFFFFFF))  {
		/* overflow; flush some nonsense to keep the clocks going */
#define DELAY_MSG "This is a long delay"
		add_track_len(t, t->msec);
		add_track(t, "\377\1", 2);
		add_track_len(t, sizeof(DELAY_MSG)-1);
		add_track(t, DELAY_MSG, sizeof(DELAY_MSG)-1);
		t->msec = 0;
#undef DELAY_MSG
	}
	t->msec += msec;
}
static void pad_all(struct midi_track *t,int frame, int tempo, int row)
{
	int i;
	for (i = 0; i < 64; i++) pad(t+i,frame,tempo,row);
}
static void macro(unsigned char **pp, int c, const char *mac,
				int par, int note,
				int vel, struct midi_track *t)
{
	unsigned char *p = *pp;
	int open_sysx;
	int cw, ch;

	if (!mac) return;
	open_sysx = 0;
	cw = 0;
	while (*mac) {
		if (!cw) *p=0;
		switch (*mac) {
		case '0': ch=0; break;
		case '1': ch=1; break;
		case '2': ch=2; break;
		case '3': ch=3; break;
		case '4': ch=4; break;
		case '5': ch=5; break;
		case '6': ch=6; break;
		case '7': ch=7; break;
		case '8': ch=8; break;
		case '9': ch=9; break;
		case 'A': ch=10; break;
		case 'B': ch=11; break;
		case 'C': ch=12; break;
		case 'D': ch=13; break;
		case 'E': ch=14; break;
		case 'F': ch=15; break;
		case 'c': ch = c & 15; break;
		case 'n': cw=1; ch = (note & 127); break;
		case 'v': cw=1; ch = vel; break;
		case 'u': cw=1; ch = t->vol; break;
		case 'x': cw=1; ch = t->pan; break;
		case 'y': cw=1; ch = t->pan; break;
		case 'a': cw=1; ch = (t->bank >> 7) & 127; break;
		case 'b': cw=1; ch = (t->bank & 127); break;
		case 'z': case 'p': cw=1; ch = par; break;
		case ' ': case '\t':
			if (cw) {
				ch = 0;
				break;
			}
			mac++;
			continue;
		default: mac++; continue;
		};
		if (!cw) {
			*p = (ch << 4);
			cw = 1;
		} else {
			*p |= ch;
			cw = 0;
			if (*p == 0xF0) open_sysx=1;
			else if (*p == 0xF7) open_sysx=0;
			p++;
		}
		mac++;
	}
	if (cw) p++;
	if (open_sysx) {
		*p = 0xF7;
		p++;
	}
	if (*pp != p) {
		*p = 0;
		p++;
	}
	*pp = p;
}
static void keyvol(midi_config *g, unsigned char **pp, struct midi_track *t, int vol)
{
	int j;
	vol = volfix(vol);
	if (vol > 127) vol = 127;
	for (j = 0; j < 128; j++) {
		if (!t->note_on[j]) continue;
		(*pp)[0] = 0xb0|(t->note_on[j]-1);
		(*pp)[1] = 0x07;
		(*pp)[2] = vol;
		(*pp)[3] = 0;
		(*pp) = (*pp) + 4;
		macro(pp, t->note_on[j]-1,
				GCF(MIDI_GCF_VOLUME), vol, j, vol, t);
	}
}
static void keyoff(midi_config *g, unsigned char **pp, struct midi_track *t)
{
	int j;
	for (j = 0; j < 128; j++) {
		if (!t->note_on[j]) continue;
			
		/* keyoff */
		macro(pp, t->note_on[j]-1,
				GCF(MIDI_GCF_NOTEOFF), j, j, 0, t);
				
		t->note_on[j] = 0;
	}
}
void fmt_mid_save_song(diskwriter_driver_t *dw)
{
	song_note *nb;
	midi_config *g;
	byte *orderlist;
	struct midi_track trk[64];
	struct midi_map map[SCHISM_MAX_SAMPLES], *m;
	song_instrument *ins;
	song_channel *chan;
	song_sample *smp;
	char *s;
	unsigned char packet[256], *p;
	int order, row, speed;
	int i, j, nt, left, vol;
	int row_pad;
	int need_cut;
	int note_length;
	int tempo;

	g = song_get_midi_config();

	memset(map, 0, sizeof(map));
	memset(trk, 0, sizeof(trk));
	for (i = 0; i < 64; i++) {
		chan = song_get_channel(i);
		trk[i].pan = (chan->panning / 2);
		if (trk[i].pan > 127) trk[i].pan = 127;
		trk[i].vol = chan->volume;
		trk[i].muted = (chan->flags & CHN_MUTE) ? 1 : 0;
		if (chan->flags & CHN_SURROUND) trk[i].pan = 64;
	}

	if (song_is_instrument_mode()) {
		assert(SCHISM_MAX_INSTRUMENTS == SCHISM_MAX_SAMPLES);
		for (i = 1; i <= SCHISM_MAX_INSTRUMENTS; i++) {
			if (song_instrument_is_empty(i)) continue;
			ins = song_get_instrument(i,NULL);
			if (!ins) continue;
			if (ins->midi_channel_mask >= 0x10000) continue;
			map[i].c = 0;
			if (ins->midi_channel_mask)
				while (!(ins->midi_channel_mask & (1 << map[i].c)))
					++map[i].c;
			map[i].p = ins->midi_program & 127;
			map[i].b = ins->midi_bank;
			map[i].nna = ins->nna;
		}
	} else {
		for (i = 1; i <= SCHISM_MAX_SAMPLES; i++) {
			if (song_sample_is_empty(i)) continue;
			smp = song_get_sample(i,NULL);
			if (!smp) continue;
			map[i].p = 1;
			map[i].c = i;
			map[i].b = 0;
			map[i].nna = NNA_NOTECUT;
		}
	}
	dw->o(dw, (const unsigned char *)"MThd\0\0\0\6\0\1\0\100\x00\x60", 14);

	s = song_get_title();
	if (s && *s) {
		add_track(&trk[0], "\0\377\1", 3);
		add_track_len(&trk[0], i=strlen(s));
		add_track(&trk[0], s, i);
		/* put the 0msec marker back on */
		add_track_len(&trk[0], 0);
	}

	/* set initial values */
	for (i = 0; i < 64; i++) {
		p = packet;
		macro(&p, 0, GCF(MIDI_GCF_PAN), trk[i].pan, 0, 0, trk+i);
		data(trk+i,packet,(p-packet)-1);
	}

	orderlist = song_get_orderlist();
	
	order = 0;
	speed = song_get_initial_speed();
	row = 0;
	tempo = song_get_initial_tempo();
	while (order >= 0 && order < MAX_ORDERS && orderlist[order] != 255) {
		if (orderlist[order] == 254) { order++; continue; }
		if (!song_get_pattern(orderlist[order], &nb)
		|| !(left = song_get_rows_in_pattern(orderlist[order]))) {
			/* assume 64 rows of nil */
			pad_all(trk, speed, tempo, 64 - row);
			order++;
			row = 0;
			continue;
		}
		if (row > 0) {
			/* move to position */
			left -= row;
			nb += (64 * row);
			row = 0;
		}

		while (left >= 0) {
			/* scan for tempo/speed changes and row-delay */
			nt = tempo;
			row_pad = 0;
			for (j = 0; j < 64; j++) {
				switch (nb[j].effect) {
				case CMD_SPEED:
					speed = nb[j].parameter;
					break;
				case CMD_TEMPO:
					nt = nb[j].parameter;
					if (!nt) nt = trk[j].old_tempo;
					trk[j].old_tempo = nt;
					break;
				case CMD_S3MCMDEX:
					if ((nb[j].parameter & 0xF0) == 0xe0) {
						pad_all(trk,(nb[j].parameter & 15),
								tempo,1);
					}
					break;
				};
			}

			for (i = 0; i < 64; i++) {
				p = packet;

				if (nb->instrument
				&& trk[i].last_chan != nb->instrument) {
					m = &map[nb->instrument];
					if (trk[i].bank != m->b) {
						/* bank select */
						trk[i].bank = m->b;
						macro(&p, m->c-1,
								GCF(MIDI_GCF_BANKCHANGE),
								m->b,
								0,0,trk+i);
					}
					/* program change */
					macro(&p, m->c-1,
							GCF(MIDI_GCF_PROGRAMCHANGE),
							m->p,
							0,0,trk+i);
					trk[i].last_chan = nb->instrument;
				} else {
					m = &map[ trk[i].last_chan ];
				}
				if (trk[i].muted) {
					/* nothing interesting here */
				} else if (nb->note == NOTE_CUT) {
					keyvol(g,&p, trk+i, 0);
					keyoff(g,&p, trk+i);

				} else if (nb->note > 120) {
					keyoff(g,&p, trk+i);

				} else if (nb->note && nb->note <= 120 && m->c) {
					if (m->nna == NNA_NOTECUT)
						keyvol(g,&p, trk+i, 0);
					if (m->nna != NNA_CONTINUE)
						keyoff(g,&p, trk+i);

					j = nb->note - 1;
					trk[i].note_on[j] = m->c;

					vol = 127;
					if (nb->volume_effect == VOLCMD_VOLUME)
						vol = nb->volume * 2;
					if (nb->effect == CMD_VOLUME)
						vol = nb->parameter * 2;
					if (vol > 127) vol = 127;
					macro(&p, trk[i].note_on[j]-1,
							GCF(MIDI_GCF_NOTEON),
							j, j, vol, trk+i);
					keyvol(g,&p, trk+i, vol);
				} else if (nb->effect == CMD_VOLUME) {
					vol = nb->parameter * 2;
					keyvol(g,&p, trk+i, vol);
				} else if (nb->volume_effect == VOLCMD_VOLUME) {
					vol = nb->volume * 2;
					keyvol(g,&p, trk+i, vol);
				}
				j = nb->parameter;
				need_cut = 0;
				note_length = speed;
				switch (nb->effect) {
				case CMD_PATTERNBREAK:
					row = j;
					left = 0;
					break;
				case CMD_PANNING8:
					if (j < 0x80) {
						*p = 0xb0|(m->c-1); p++;
						*p = 0x10; p++;
						*p = (j & 127); p++;
						trk[i].pan = j & 127;
						/* change pan */
						macro(&p, m->c-1,
							GCF(MIDI_GCF_PAN),
							trk[i].pan, 0, 0, trk+i);
					}
					break;
				case CMD_MIDI:
					if (j < 0x80) {
						macro(&p, m->c-1,
							&g->midi_sfx[ trk[i].active_macro << 5],
							j, 0, 0, trk+i);
					} else {
						macro(&p, m->c-1,
							&g->midi_zxx[ (j & 0x127) << 5],
							0, 0, 0, trk+i);
					}
					break;

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
						note_length -= j;
						pad(trk+i,j,tempo,1);
						break;
					case 0xf0:
						/* set active macro */
						j &= 15;
						trk[i].active_macro = j;
						break;
					};
					break;
				/* anything else? */
				};

				if (p != packet) {
					data(trk+i,packet,(p-packet)-1);
				}

				if (need_cut && need_cut < speed) {
					pad(trk+i,need_cut,tempo,1);
					/* ugh, notecut is tricky */
					p = packet;
					keyoff(g,&p, trk+i);
					data(trk+i,packet,(p-packet)-1);
					pad(trk+i,(note_length-need_cut),tempo,1);
				} else {
					pad(trk+i,note_length,tempo,1);
				}

				nb++;
			}
			if (nt != tempo) {
				if (nt >= 0x20) {
					tempo = nt;
				} else if (nt == 0x10) {
					/* no op */

				} else if (nt > 0x10) {
					tempo += (speed-1) * (nt & 15);
				} else {
					tempo -= (speed-1) * (nt & 15);
				}
				if (tempo < 32)
					tempo = 32;
				else if (tempo >= 255)
					tempo = 255;
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
