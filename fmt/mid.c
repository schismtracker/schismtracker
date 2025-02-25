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
#include "bswap.h"
#include "slurp.h"
#include "fmt.h"
#include "log.h"
#include "mem.h"

#include "player/sndfile.h"

/*
some thoughts...

really, we don't even need to adhere to the same channel numbering -- the notes
could go pretty much anywhere, as long as note-off events are handled properly.
but it'd be nice to have the channels at least sort of resemble the midi file,
whether it's arranged by track or by midi channel.

- try to allocate channels that aren't in use when possible, to avoid stomping on playing notes
- set instrument NNA to continue with dupe cut (or note off?)
*/

/* --------------------------------------------------------------------------------------------------------- */
// structs, local defines, etc.

#define MID_ROWS_PER_PATTERN 200

// Pulse/row calculations are done in fixed point for better accuracy
#define FRACBITS 12
#define FRACMASK ((1 << FRACBITS) - 1)

struct mthd {
	char tag[4]; // MThd
	uint32_t header_length;
	uint16_t format; // 0 = single-track, 1 = multi-track, 2 = multi-song
	uint16_t num_tracks; // number of track chunks
	uint16_t division; // delta timing value: positive = units/beat; negative = smpte compatible units (?)
};

struct mtrk {
	char tag[4]; // MTrk
	uint32_t length; // number of bytes of track data following
};

static int read_mid_mthd(struct mthd *hdr, slurp_t *fp)
{
#define READ_VALUE(name) \
	do { if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) { return 0; } } while (0)

	READ_VALUE(tag);

	if (memcmp(hdr->tag, "RIFF", 4) == 0) {
		// Stupid MS crap.
		slurp_seek(fp, 16, SEEK_CUR);
		READ_VALUE(tag);
	}

	READ_VALUE(header_length);
	READ_VALUE(format);
	READ_VALUE(num_tracks);
	READ_VALUE(division);

#undef READ_VALUE

	if (memcmp(hdr->tag, "MThd", 4))
		return 0;

	hdr->header_length = bswapBE32(hdr->header_length);
	// don't care about format, either there's one track or more than one track. whoop de doo.
	// (format 2 MIDs will probably be hilariously broken, but I don't have any and also don't care)
	hdr->format = bswapBE16(hdr->format);
	hdr->num_tracks = bswapBE16(hdr->num_tracks);
	hdr->division = bswapBE16(hdr->division);

	slurp_seek(fp, hdr->header_length - 6, SEEK_CUR); // account for potential weirdness

	return 1;
}

static int read_mid_mtrk(struct mtrk *mtrk, slurp_t *fp)
{
#define READ_VALUE(name) \
	do { if (slurp_read(fp, &mtrk->name, sizeof(mtrk->name)) != sizeof(mtrk->name)) { return 0; } } while (0)

	READ_VALUE(tag);
	READ_VALUE(length);

#undef READ_VALUE

	if (memcmp(mtrk->tag, "MTrk", 4)) {
		log_appendf(4, " Warning: Invalid track header (corrupt file?)");
		return 0;
	}

	mtrk->length = bswapBE32(mtrk->length);

	return 1;
}

struct event {
	unsigned int pulse; // the PPQN-tick, counting from zero, when this midi-event happens
	uint8_t chan; // target channel (0-based!)
	song_note_t note; // the note data (new data will overwrite old data in same channel+row)
	struct event *next;
};

static struct event *alloc_event(unsigned int pulse, uint8_t chan, const song_note_t *note, struct event *next)
{
	struct event *ev = malloc(sizeof(struct event));
	if (!ev) {
		perror("malloc");
		return NULL;
	}
	ev->pulse = pulse;
	ev->chan = chan;
	ev->note = *note;
	ev->next = next;
	return ev;
}

/* --------------------------------------------------------------------------------------------------------- */
// support functions

static unsigned int read_varlen(slurp_t *fp)
{
	int b;
	unsigned int v = 0;

	// This will fail tremendously if a value overflows. I don't care.
	do {
		b = slurp_getc(fp);
		if (b == EOF)
			return 0; // truncated?!
		v <<= 7;
		v |= b & 0x7f;
	} while (b & 0x80);
	return v;
}

/* --------------------------------------------------------------------------------------------------------- */
// info (this is ultra lame)

int fmt_mid_read_info(dmoz_file_t *file, slurp_t *fp)
{
	song_t *tmpsong = csf_allocate();
	if (!tmpsong)
		return 0; // wahhhh

	if (fmt_mid_load_song(tmpsong, fp, LOAD_NOSAMPLES | LOAD_NOPATTERNS) == LOAD_SUCCESS) {
		file->description = "Standard MIDI File";
		file->title = str_dup(tmpsong->title);
		file->type = TYPE_MODULE_MOD;
		csf_free(tmpsong);
		return 1;
	}

	csf_free(tmpsong);
	return 0;
}

/* --------------------------------------------------------------------------------------------------------- */
// load

int fmt_mid_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	struct mthd mthd;
	struct mtrk mtrk;
	unsigned char buf[32];
	song_note_t note;
	struct event *event_queue, *cur, *prev, *new;
	struct {
		uint8_t fg_note;
		uint8_t bg_note; // really just used as a boolean...
		uint8_t instrument;
	} midich[16] = {{NOTE_NONE, NOTE_NONE, 0}};
	char *message_cur = song->message;
	unsigned int message_left = MAX_MESSAGE;
	unsigned int pulse = 0; // cumulative time from start of track
	uint8_t patch_samples[128] = {0};
	uint8_t nsmp = 1; // Next free sample

	if (!read_mid_mthd(&mthd, fp))
		return LOAD_UNSUPPORTED;

	song->title[0] = '\0'; // should be already, but to be sure...

	/* We'll count by "pulses" here, which are basically MIDI-speak for ticks, except that there are a heck
	of a lot more of them. (480 pulses/quarter is fairly common, that's like A78, if the tempo could be
	adjusted high enough to make practical use of that speed)
	Also, we'll use a 32-bit value and hopefully not overflow -- which is unlikely anyway, as it'd either
	require PPQN to be very ridiculously high, or a file that's several *hours* long.

	Stuff a useless event at the start of the event queue. */
	note = (song_note_t) {.note = NOTE_NONE};
	event_queue = alloc_event(0, 0, &note, NULL);

	for (int trknum = 0; trknum < mthd.num_tracks; trknum++) {
		unsigned int delta; // time since last event (read from file)
		unsigned int vlen; // some other generic varlen number
		unsigned char rs = 0; // running status byte
		unsigned char status; // THIS status byte (as opposed to rs)
		unsigned char hi, lo, cn, x, y;
		unsigned int bpm; // stupid
		int found_end = 0;
		long nextpos;

		cur = event_queue->next;
		prev = event_queue;
		pulse = 0;

		if (!read_mid_mtrk(&mtrk, fp))
			break;

		nextpos = slurp_tell(fp) + mtrk.length; // where this track is supposed to end

		while (!found_end && slurp_tell(fp) < nextpos) {
			delta = read_varlen(fp); // delta-time
			pulse += delta; // 'real' pulse count

			// get status byte, if there is one
			if (slurp_peek(fp, &status, sizeof(status)) == sizeof(status) && status & 0x80) {
				slurp_seek(fp, 1, SEEK_CUR);
			} else if (rs & 0x80) {
				status = rs;
			} else {
				// garbage?
				continue;
			}

			note = (song_note_t) {.note = NOTE_NONE};
			hi = status >> 4;
			lo = status & 0xf;
			cn = lo; //or: trknum * CHANNELS_PER_TRACK + lo % CHANNELS_PER_TRACK;

			switch (hi) {
			case 0x8: // note off - x, y
				rs = status;
				x = slurp_getc(fp); // note
				y = slurp_getc(fp); // release velocity
				x = CLAMP(x + NOTE_FIRST, NOTE_FIRST, NOTE_LAST); // clamp is wrong, but whatever
				// if the last note in the channel is the same as this note, just write ===
				// otherwise, if there is a note playing, assume our note got backgrounded
				// and write S71 (past note off)
				if (midich[cn].fg_note == x) {
					note = (song_note_t) {.note = NOTE_OFF};
					midich[cn].fg_note = NOTE_NONE;
				} else {
					// S71, past note off
					note = (song_note_t) {.effect = FX_SPECIAL, .param = 0x71};
					midich[cn].bg_note = NOTE_NONE;
				}
				break;
			case 0x9: // note on - x, y (velocity zero = note off)
				rs = status;
				x = slurp_getc(fp); // note
				y = slurp_getc(fp); // attack velocity
				x = CLAMP(x + NOTE_FIRST, NOTE_FIRST, NOTE_LAST); // see note off above.

				if (lo == 9) {
					// ignore percussion for now
				} else if (y == 0) {
					// this is actually another note-off, see above
					// (maybe that stuff should be split into a function or blahblah)
					if (midich[cn].fg_note == x) {
						note = (song_note_t) {.note = NOTE_OFF};
						midich[cn].fg_note = NOTE_NONE;
					} else {
						// S71, past note off
						note = (song_note_t) {.effect = FX_SPECIAL, .param = 0x71};
						midich[cn].bg_note = NOTE_NONE;
					}
				} else {
					if (nsmp == 1 && !(lflags & LOAD_NOSAMPLES)) {
						// no samples defined yet - fake a program change
						patch_samples[0] = 1;
						adlib_patch_apply(song->samples + 1, 0);
						nsmp++;
					}

					note = (song_note_t) {
						.note = x,
						.instrument = patch_samples[midich[cn].instrument],
						.voleffect = VOLFX_VOLUME,
						.volparam = (y & 0x7f) * 64 / 127,
					};
					midich[cn].fg_note = x;
					midich[cn].bg_note = midich[cn].fg_note;
				}
				break;
			case 0xa: // polyphonic key pressure (aftertouch) - x, y
				rs = status;
				x = slurp_getc(fp);
				y = slurp_getc(fp);
				// TODO polyphonic aftertouch channel=lo note=x pressure=y
				continue;
			case 0xb: // controller OR channel mode - x, y
				rs = status;
				// controller if first data byte 0-119
				// channel mode if first data byte 120-127
				x = slurp_getc(fp);
				y = slurp_getc(fp);
				// TODO controller change channel=lo controller=x value=y
				continue;
			case 0xc: // program change - x (instrument/voice selection)
				rs = status;
				x = slurp_getc(fp);
				midich[cn].instrument = x;
				// look familiar? this was copied from the .mus loader
				if (!patch_samples[x] && !(lflags & LOAD_NOSAMPLES)) {
					if (nsmp < MAX_SAMPLES) {
						// New sample!
						patch_samples[x] = nsmp;
						adlib_patch_apply(song->samples + nsmp, x);
						nsmp++;
					} else {
						log_appendf(4, " Warning: Too many samples");
					}
				}
				note = (song_note_t) {.instrument = patch_samples[x]};
				break;
			case 0xd: // channel pressure (aftertouch) - x
				rs = status;
				x = slurp_getc(fp);
				// TODO channel aftertouch channel=lo pressure=x
				continue;
			case 0xe: // pitch bend - x, y
				rs = status;
				x = slurp_getc(fp);
				y = slurp_getc(fp);
				// TODO pitch bend channel=lo lsb=x msb=y
				continue;
			case 0xf: // system messages
				switch (lo) {
				case 0xf: // meta-event (text and stuff)
					x = slurp_getc(fp); // type
					vlen = read_varlen(fp); // value length
					switch (x) {
					case 0x1: // text
					case 0x2: // copyright
					case 0x3: // track name
					case 0x4: // instrument name
					case 0x5: // lyric
					case 0x6: // marker
					case 0x7: // cue point
						y = MIN(vlen, message_left ? message_left - 1 : 0);
						slurp_read(fp, message_cur, y);
						if (x == 3 && y && !song->title[0]) {
							strncpy(song->title, message_cur, MIN(y, 25));
							song->title[25] = '\0';
						}
						message_cur += y;
						message_left -= y;
						if (y && message_cur[-1] != '\n') {
							*message_cur++ = '\n';
							message_left--;
						}
						vlen -= y;
						break;

					case 0x20: // MIDI channel (FF 20 len* cc)
						// specifies which midi-channel sysexes are assigned to
					case 0x21: // MIDI port (FF 21 len* pp)
						// specifies which port/bus this track's events are routed to
						break;

					case 0x2f:
						found_end = 1;
						break;
					case 0x51: // set tempo
						// read another stupid kind of variable length number
						// hopefully this fits into 4 bytes - if not, too bad!
						// (what is this? friggin' magic?)
						memset(buf, 0, 4);
						y = MIN(vlen, 4);
						slurp_read(fp, buf + (4 - y), y);
						bpm = buf[0] << 24 | (buf[1] << 16) | (buf[2] << 8) | buf[3];
						bpm = CLAMP(60000000 / (bpm ? bpm : 1), 0x20, 0xff);
						note = (song_note_t) {.effect = FX_TEMPO, .param = bpm};
						vlen -= y;
						break;
					case 0x54: // SMPTE offset (what time in the song this track starts)
						// (what?!)
						break;
					case 0x58: // time signature (FF 58 len* nn dd cc bb)
					case 0x59: // key signature (FF 59 len* sf mi)
						// TODO care? don't care?
						break;
					case 0x7f: // some proprietary crap
						break;

					default:
						// some mystery crap
						log_appendf(2, " Unknown meta-event FF %02X", x);
						break;
					}
					slurp_seek(fp, vlen, SEEK_CUR);
					break;
				/* sysex */
				case 0x0:
				/* syscommon */
				case 0x1: case 0x2: case 0x3:
				case 0x4: case 0x5: case 0x6:
				case 0x7:
					rs = 0; // clear running status
				/* sysrt */
				case 0x8: case 0x9: case 0xa:
				case 0xb: case 0xc: case 0xd:
				case 0xe:
					// 0xf0 - sysex
					// 0xf1-0xf7 - common
					// 0xf8-0xff - sysrt
					// sysex and common cancel running status
					// TODO handle these, or at least skip them coherently
					continue;
				}
			}

			// skip past any events with a lower pulse count (from other channels)
			while (cur && pulse > cur->pulse) {
				prev = cur;
				cur = cur->next;
			}
			// and now, cur is either NULL or has a higher timestamp, so insert before it
			new = alloc_event(pulse, cn, &note, cur);
			prev->next = new;
			prev = prev->next;
		}
		if (slurp_tell(fp) != nextpos) {
			log_appendf(2, " Track %d ended %ld bytes from boundary",
				trknum, slurp_tell(fp) - nextpos);
			slurp_seek(fp, nextpos, SEEK_SET);
		}
	}

	song->initial_speed = 3;
	song->initial_tempo = 120;

	prev = NULL;
	cur = event_queue;

	if (lflags & LOAD_NOPATTERNS) {
		while (cur) {
			prev = cur;
			cur = cur->next;
			free(prev);
		}
		return LOAD_SUCCESS;
	}

	// okey doke! now let's write this crap out to the patterns
	song_note_t *pattern = NULL, *rowdata;
	int row = MID_ROWS_PER_PATTERN; // what row of the pattern rowdata is pointing to (fixed point)
	int rowfrac = 0; // how much is left over
	int pat = 0; // next pattern number to create
	pulse = 0; // PREVIOUS event pulse.

	while (cur) {
		/* calculate pulse delta from previous event
		 * calculate row count from the pulse count using ppqn (assuming 1 row = 1/32nd note? 1/64?)
		 * advance the row as required
		it'd be nice to aim for the "middle" of ticks instead of the start of them, that way if an
			event is just barely off, it won't end up shifted way ahead.
		*/
		unsigned int delta = cur->pulse - pulse;

		if (delta) {
			// Increment position
			row <<= FRACBITS;
			row += 8 * (delta << FRACBITS) / mthd.division; // times 8 -> 32nd notes
			row += rowfrac;
			rowfrac = row & FRACMASK;
			row >>= FRACBITS;
		}
		pulse = cur->pulse;

		while (row >= MID_ROWS_PER_PATTERN) {
			// New pattern time!
			if(pat >= MAX_PATTERNS) {
				log_appendf(4, " Warning: Too many patterns, song is truncated");
				return LOAD_SUCCESS;
			}
			pattern = song->patterns[pat] = csf_allocate_pattern(MID_ROWS_PER_PATTERN);
			song->pattern_size[pat] = song->pattern_alloc_size[pat] = MID_ROWS_PER_PATTERN;
			song->orderlist[pat] = pat;
			pat++;
			row -= MID_ROWS_PER_PATTERN;
		}
		rowdata = pattern + 64 * row;
		if (cur->note.note) {
			rowdata[cur->chan].note = cur->note.note;
			rowdata[cur->chan].instrument = cur->note.instrument;
		}
		if (cur->note.voleffect) {
			rowdata[cur->chan].voleffect = cur->note.voleffect;
			rowdata[cur->chan].volparam = cur->note.volparam;
		}
		if (cur->note.effect) {
			rowdata[cur->chan].effect = cur->note.effect;
			rowdata[cur->chan].param = cur->note.param;
		}

		prev = cur;
		cur = cur->next;
		free(prev);
	}

	return LOAD_SUCCESS;
}

