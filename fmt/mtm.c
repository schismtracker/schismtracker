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
#include "fmt.h"
#include "song.h"
#include "log.h"

#include "player/tables.h"

#include <stdint.h>

/* --------------------------------------------------------------------- */

int fmt_mtm_read_info(dmoz_file_t *file, slurp_t *fp)
{
	unsigned char magic[3], title[20];

	if (slurp_read(fp, magic, sizeof(magic)) != sizeof(magic)
		|| memcmp(magic, "MTM", 3))
		return 0;

	slurp_seek(fp, 1, SEEK_CUR);

	if (slurp_read(fp, title, sizeof(title)) != sizeof(title))
		return 0;

	file->description = "MultiTracker Module";
	/*file->extension = str_dup("mtm");*/
	file->title = strn_dup((const char *)title, sizeof(title));
	file->type = TYPE_MODULE_MOD;
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

static void mtm_unpack_track(const uint8_t *b, song_note_t *note, int rows)
{
	int n;

	for (n = 0; n < rows; n++, note++, b += 3) {
		note->note = ((b[0] & 0xfc) ? ((b[0] >> 2) + 36 + 1) : NOTE_NONE);
		note->instrument = ((b[0] & 0x3) << 4) | (b[1] >> 4);
		note->voleffect = VOLFX_NONE;
		note->volparam = 0;
		note->effect = b[1] & 0xf;
		note->param = b[2];

		/* From mikmod: volume slide up always overrides slide down */
		if (note->effect == 0xa && (note->param & 0xf0)) {
			note->param &= 0xf0;
		} else if (note->effect == 0x8) {
			note->effect = note->param = 0;
		} else if (note->effect == 0xe) {
			switch (note->param >> 4) {
			case 0x0:
			case 0x3:
			case 0x4:
			case 0x6:
			case 0x7:
			case 0xF:
				note->effect = note->param = 0;
				break;
			default:
				break;
			}
		}

		if (note->effect || note->param)
			csf_import_mod_effect(note, 0);
	}
}

int fmt_mtm_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	uint8_t b[192];
	uint8_t nchan, nord, npat, nsmp;
	uint16_t ntrk, comment_len;
	int n, pat, chan, smp, rows, todo = 0;
	song_note_t *note;
	uint16_t tmp;
	uint32_t tmplong;
	song_note_t **trackdata, *tracknote;
	song_sample_t *sample;

	slurp_read(fp, b, 3);
	if (memcmp(b, "MTM", 3) != 0)
		return LOAD_UNSUPPORTED;
	n = slurp_getc(fp);
	sprintf(song->tracker_id, "MultiTracker %d.%d", n >> 4, n & 0xf);
	slurp_read(fp, song->title, 20);
	song->title[20] = 0;
	slurp_read(fp, &ntrk, 2);
	ntrk = bswapLE16(ntrk);
	npat = slurp_getc(fp);
	nord = slurp_getc(fp) + 1;

	slurp_read(fp, &comment_len, 2);
	comment_len = bswapLE16(comment_len);

	nsmp = slurp_getc(fp);
	slurp_getc(fp); /* attribute byte (unused) */
	rows = slurp_getc(fp); /* beats per track (translation: number of rows in every pattern) */
	if (rows != 64)
		todo |= 64;
	rows = MIN(rows, 64);
	nchan = slurp_getc(fp);

	if (slurp_eof(fp)) {
		return LOAD_FORMAT_ERROR;
	}

	for (n = 0; n < 32; n++) {
		int pan = slurp_getc(fp) & 0xf;
		pan = SHORT_PANNING(pan);
		pan *= 4; //mphack
		song->channels[n].panning = pan;
	}
	for (n = nchan; n < MAX_CHANNELS; n++)
		song->channels[n].flags = CHN_MUTE;

	/* samples */
	if (nsmp > MAX_SAMPLES) {
		log_appendf(4, " Warning: Too many samples");
	}
	for (n = 1, sample = song->samples + 1; n <= nsmp; n++, sample++) {
		if (n > MAX_SAMPLES) {
			slurp_seek(fp, 37, SEEK_CUR);
			continue;
		}

		/* IT truncates .mtm sample names at the first \0 rather than the normal behavior
		of presenting them as spaces (k-achaet.mtm has some "junk" in the sample text) */
		char name[23];
		slurp_read(fp, name, 22);
		name[22] = '\0';
		strcpy(sample->name, name);
		slurp_read(fp, &tmplong, 4);
		sample->length = bswapLE32(tmplong);
		slurp_read(fp, &tmplong, 4);
		sample->loop_start = bswapLE32(tmplong);
		slurp_read(fp, &tmplong, 4);
		sample->loop_end = bswapLE32(tmplong);
		if ((sample->loop_end - sample->loop_start) > 2) {
			sample->flags |= CHN_LOOP;
		} else {
			/* Both Impulse Tracker and Modplug do this */
			sample->loop_start = 0;
			sample->loop_end = 0;
		}

		// This does what OpenMPT does; it treats the finetune as Fasttracker
		// units, multiplied by 16 (which I believe makes them the same as MOD
		// but with a higher range)
		int8_t finetune;
		slurp_read(fp, &finetune, sizeof(finetune));
		song->samples[n].c5speed = transpose_to_frequency(0, finetune * 16);
		sample->volume = slurp_getc(fp);
		sample->volume *= 4; //mphack
		sample->global_volume = 64;
		if (slurp_getc(fp) & 1) {
			sample->flags |= CHN_16BIT;
			sample->length /= 2;
			sample->loop_start /= 2;
			sample->loop_end /= 2;
		}
		song->samples[n].vib_type = 0;
		song->samples[n].vib_rate = 0;
		song->samples[n].vib_depth = 0;
		song->samples[n].vib_speed = 0;
	}

	/* orderlist */
	slurp_read(fp, song->orderlist, 128);
	memset(song->orderlist + nord, ORDER_LAST, MAX_ORDERS - nord);

	/* tracks */
	trackdata = mem_calloc(ntrk, sizeof(song_note_t *));
	for (n = 0; n < ntrk; n++) {
		slurp_read(fp, b, 3 * rows);
		trackdata[n] = mem_calloc(rows, sizeof(song_note_t));
		mtm_unpack_track(b, trackdata[n], rows);
	}

	/* patterns */
	if (npat >= MAX_PATTERNS) {
		log_appendf(4, " Warning: Too many patterns");
	}
	for (pat = 0; pat <= npat; pat++) {
		// skip ones that can't be loaded
		if (pat >= MAX_PATTERNS) {
			slurp_seek(fp, 64, SEEK_CUR);
			continue;
		}

		song->patterns[pat] = csf_allocate_pattern(MAX(rows, 32));
		song->pattern_size[pat] = song->pattern_alloc_size[pat] = 64;
		for (chan = 0; chan < 32; chan++) {
			slurp_read(fp, &tmp, 2);
			tmp = bswapLE16(tmp);
			if (tmp == 0) {
				continue;
			} else if (tmp > ntrk) {
				for (n = 0; n < ntrk; n++)
					free(trackdata[n]);
				free(trackdata);
				return LOAD_FORMAT_ERROR;
			}
			note = song->patterns[pat] + chan;
			tracknote = trackdata[tmp - 1];
			for (n = 0; n < rows; n++, tracknote++, note += MAX_CHANNELS)
				*note = *tracknote;
		}
		if (rows < 32) {
			/* stick a pattern break on the first channel with an empty effect column
			 * (XXX don't do this if there's already one in another column) */
			note = song->patterns[pat] + 64 * (rows - 1);
			while (note->effect || note->param)
				note++;
			note->effect = FX_PATTERNBREAK;
		}
	}

	/* free willy */
	for (n = 0; n < ntrk; n++)
		free(trackdata[n]);
	free(trackdata);

	read_lined_message(song->message, fp, comment_len, 40);

	/* sample data */
	if (!(lflags & LOAD_NOSAMPLES)) {
		for (smp = 1; smp <= nsmp && smp <= MAX_SAMPLES; smp++) {
			if (song->samples[smp].length == 0)
				continue;

			csf_read_sample(song->samples + smp,
				(SF_LE | SF_PCMU | SF_M
				 | ((song->samples[smp].flags & CHN_16BIT) ? SF_16 : SF_8)), fp);
		}
	}

	/* set the rest of the stuff */
	song->flags = SONG_ITOLDEFFECTS | SONG_COMPATGXX;

//      if (ferror(fp)) {
//              return LOAD_FILE_ERROR;
//      }

	if (todo & 64)
		log_appendf(2, " TODO: test this file with other players (beats per track != 64)");

	/* done! */
	return LOAD_SUCCESS;
}


/* --------------------------------------------------------------------------------------------------------- */
// written by storlek in 2009, removed in 2010, added back in 2024 by RepellantMold and paper.
// mostly unedited but added a couple warnings and song message saving.

enum {
	WARN_MAXPATTERNS,
	WARN_CHANNELVOL,
	WARN_LINEARSLIDES,
	WARN_SAMPLEVOL,
	WARN_LOOPS,
	WARN_SAMPLEVIB,
	WARN_INSTRUMENTS,
	WARN_PATTERNLEN,
	WARN_MAXCHANNELS,
	WARN_MUTED,
	WARN_NOTERANGE,
	WARN_MAXSAMPLES,
	WARN_MSGLENGTHCOLS,
	WARN_MSGLENGTHROWS,
	WARN_DEFSPEEDTEMPO,
	WARN_PATTERNLENOF,
	WARN_EFFECTMEMORY,
	MAX_WARN,
};

static const char *mtm_warnings[] = {
	[WARN_MAXPATTERNS]   = "Over 100 patterns",
	[WARN_CHANNELVOL]    = "Channel volumes",
	[WARN_LINEARSLIDES]  = "Linear slides",
	[WARN_SAMPLEVOL]     = "Sample volumes",
	[WARN_LOOPS]         = "Sustain and Ping Pong loops",
	[WARN_SAMPLEVIB]     = "Sample vibrato",
	[WARN_INSTRUMENTS]   = "Instrument functions",
	[WARN_PATTERNLEN]    = "Inconsistent pattern lengths",
	[WARN_MAXCHANNELS]   = "Data outside 32 channels",
	[WARN_MUTED]         = "Data in muted channels",
	[WARN_NOTERANGE]     = "Notes outside the range C-1 to B-8",
	[WARN_MAXSAMPLES]    = "Over 31 samples",
	[WARN_MSGLENGTHCOLS] = "Over 39 columns in song message",
	[WARN_MSGLENGTHROWS] = "Over 20 rows in song message",
	[WARN_DEFSPEEDTEMPO] = "Default speed & tempo",
	[WARN_PATTERNLENOF]  = "Patterns with over 64 rows",
	[WARN_EFFECTMEMORY]  = "Effect memory",

	[MAX_WARN]           = NULL,
};

struct mtm_track {
	uint8_t data[192];
	uint_fast16_t id;
	struct mtm_track *next;
};

struct mtm_header {
	char filever[4]; /* M T M \x10 or \x11 */
	char title[20]; /* asciz */
	uint16_t ntracks;
	uint8_t last_pattern;
	uint8_t last_order; /* songlength - 1 */
	uint16_t msglen;
	uint8_t nsamples;
	uint8_t flags; /* always 0 */
	uint8_t rows; /* prob. 64 */
	uint8_t nchannels;
	uint8_t panpos[32];
};

struct mtm_sample {
	char name[22];
	uint32_t length, loop_start, loop_end;
	uint8_t finetune, volume, flags;
};

static struct mtm_track *mtm_track(song_note_t *note, int rows, uint32_t *warn)
{
	int saw_data = 0;
	uint8_t *d;
	struct mtm_track *trk;

	trk = calloc(1, sizeof(struct mtm_track));
	if (trk == NULL)
		return NULL;
	d = trk->data;
	trk->next = NULL;

	if (rows > 64)
		rows = 64;

	while (rows--) {
		/* pack it in */
		uint8_t n = note->note, i = note->instrument, e = 0, p = note->param;

		/* ? */
		if (n > 36 && n < 101)
			n -= 37;
		else
			n = 0;

		int check_effect_memory = 0;

		switch (note->effect) {
		default:
			e = p = 0;
			break;
		case FX_ARPEGGIO:
			e = 0x0;
			check_effect_memory = 1;
			break;
		case FX_PORTAMENTOUP:
			e = 0x1;
			check_effect_memory = 1;
			break;
		case FX_PORTAMENTODOWN:
			e = 0x2;
			check_effect_memory = 1;
			break;
		case FX_TONEPORTAMENTO:
			e = 0x3;
			check_effect_memory = 1;
			break;
		case FX_VIBRATO:
			e = 0x4;
			check_effect_memory = 1;
			break;
		case FX_TONEPORTAVOL:
			e = 0x5;
			check_effect_memory = 1;
			break;
		case FX_VIBRATOVOL:
			e = 0x6;
			check_effect_memory = 1;
			break;
		case FX_TREMOLO:
			e = 0x7;
			check_effect_memory = 1;
			break;
		case FX_PANNING:
			e = 0xE;
			p = (0x80 | (p >> 4));
			check_effect_memory = 1;
			break;
		case FX_OFFSET:
			e = 0x9;
			check_effect_memory = 1;
			break;
		case FX_VOLUMESLIDE:
			/* storlek never fixed this in the original code! >:( */
			if ((p >> 4 == 0x0F) && (p & 0x0F)) {
				e = 0xe;
				p = 0xb0 | (p & 0xf);
			} else if (((p & 0x0F) == 0x0F) && p >> 4) {
				e = 0xe;
				p = 0xa0 | (p >> 4);
			} else {
				e = 0xa;
			}
			check_effect_memory = 1;
			break;
		case FX_POSITIONJUMP:
			e = 0xb;
			break;
		case FX_VOLUME:
			/* blah */
			e = 0xc;
			check_effect_memory = 1;
			break;
		case FX_PATTERNBREAK:
			e = 0xd; /* XXX decimal? */
			break;
		// FIXME multitracker is quirky; a speed command resets the
		// tempo to the default and vice versa. however, many players
		// didn't actually implement this quirk (MikMod, DMP) which
		// means files created by us will play fine there.
		//
		// I'm not entirely sure what the best way to go about this is.
		// OpenMPT checks for speed & tempo on the same track before
		// interpreting it as MultiTracker would, so maybe we should
		// warn on *any* speed/tempo effects?
		// For now I'm just going to keep the ProTracker-like behavior
		// since it makes the most sense to me...
		case FX_SPEED:
			e = 0xf;
			p = MIN(p, 0x1f);
			break;
		case FX_TEMPO:
			e = 0xf;
			p = MAX(p, 0x20);
			break;
		case FX_RETRIG:
			e = 0xe;
			p = 0x90 | (p & 0xf); 
			check_effect_memory = 1;
			break;
		case FX_SPECIAL:
			e = 0xe;
			switch (p >> 4) {
			default:
				e = p = 0;
				break;
			case 0x8: case 0xc: case 0xd: case 0xe:
				/* ok */
				break;
			// multitracker doesn't support these -paper
			//case 0x3:
			//	p = 0x40 | (p & 0xf);
			//	break;
			//case 0x4:
			//	p = 0x70 | (p & 0xf);
			//	break;
			//case 0xb:
			//	p = 0x60 | (p & 0xf);
			//	break;
			case 0x9:
				if (p == 0x91) {
					e = 0x8;
					p = 0xa4;
				} else {
					e = p = 0;
				}
				break;
			}
			check_effect_memory = 1;
			break;
		}

		if (check_effect_memory && !note->param)
			*warn |= (1 << WARN_EFFECTMEMORY);

		// reuse this for volume column
		check_effect_memory = 0;

		if (!e && !p) {
			switch (note->voleffect) {
			case VOLFX_VOLUME:
				e = 0xc;
				p = note->volparam;
				break;
			case VOLFX_PANNING:
				e = 0x8;
				p = note->volparam * 255 / 64;
				break;
			/* new additions by RepellantMold ;) */
			case VOLFX_VOLSLIDEUP:
				e = 0xa;
				p = note->volparam << 4;
				check_effect_memory = 1;
				break;
			case VOLFX_VOLSLIDEDOWN:
				e = 0xa;
				p = note->volparam;
				check_effect_memory = 1;
				break;
			case VOLFX_FINEVOLUP:
				e = 0xe;
				p = 0xa0 | (note->volparam & 0xf);
				check_effect_memory = 1;
				break;
			case VOLFX_FINEVOLDOWN:
				e = 0xe;
				p = 0xb0 | (note->volparam & 0xf);
				check_effect_memory = 1;
				break;
			case VOLFX_PORTAUP:
				e = 0x1;
				p = vc_portamento_table[note->volparam & 0xf];
				check_effect_memory = 1;
				break;
			case VOLFX_PORTADOWN:
				e = 0x2;
				p = vc_portamento_table[note->volparam & 0xf];
				check_effect_memory = 1;
				break;
			case VOLFX_TONEPORTAMENTO:
				e = 0x3;
				p = vc_portamento_table[note->volparam & 0xf];
				check_effect_memory = 1;
				break;
			default:
				/* oh well */
				break;
			}
		}

		if (check_effect_memory && !note->volparam)
			*warn |= (1 << WARN_EFFECTMEMORY);

		saw_data |= (n || i || e || p);

		d[0] = (n << 2) | ((i >> 4) & 0x3);
		d[1] = (i << 4) | e;
		d[2] = p;
		d += 3;
		note += 64;
	}

	if (saw_data)
		return trk;

	free(trk);
	return NULL;
}

static struct mtm_track *link_track(struct mtm_track *head, struct mtm_track *newtrk)
{
	while (head->next) {
		if (memcmp(head->next->data, newtrk->data, 192) == 0) {
			free(newtrk);
			return head->next;
		}
		head = head->next;
	}
	head->next = newtrk;
	return newtrk;
}

static int write_mtm_song_header(const struct mtm_header *hdr, disko_t *fp)
{
#define WRITE_VALUE(x) do { disko_write(fp, &hdr->x, sizeof(hdr->x)); } while (0)
	WRITE_VALUE(filever);
	WRITE_VALUE(title);
	WRITE_VALUE(ntracks);
	WRITE_VALUE(last_pattern);
	WRITE_VALUE(last_order);
	WRITE_VALUE(msglen);
	WRITE_VALUE(nsamples);
	WRITE_VALUE(flags);
	WRITE_VALUE(rows);
	WRITE_VALUE(nchannels);
	WRITE_VALUE(panpos);
	return 1;
}

static int write_mtm_sample_header(const struct mtm_sample *hdr, disko_t *fp)
{
	WRITE_VALUE(name);
	WRITE_VALUE(length);
	WRITE_VALUE(loop_start);
	WRITE_VALUE(loop_end);
	WRITE_VALUE(finetune);
	WRITE_VALUE(volume);
	WRITE_VALUE(flags);
#undef WRITE_VALUE
	return 1;
}

static void write_mtm_song_message(const char *msg, disko_t *fp, uint32_t *warn)
{
	const long start = disko_tell(fp);

	const char *nl = msg;

	for (int i = 0; i < 20; i++) {
		const char *next = strchr(nl, '\r');

		const int at_end = !next;
		if (at_end)
			next = nl + strlen(nl);

		ptrdiff_t len = next - nl;
		len = MAX(0, len); // what

		if (len > 39) {
			len = 39;
			*warn |= (1 << WARN_MSGLENGTHCOLS);
		}

		disko_write(fp, nl, len);

		// pad each line to 40 bytes each
		while (len++ < 40)
			disko_putc(fp, '\0');

		if (at_end) {
			break;
		} else if (i == 19) {
			*warn |= (1 << WARN_MSGLENGTHROWS);
		}
		nl = next + 1;
	}

	const long end = disko_tell(fp);

	// fix the length in the header
	disko_seek(fp, 28, SEEK_SET);

	uint16_t len = end - start;
	disko_write(fp, &len, sizeof(len));

	// seek back to the end of the message
	disko_seek(fp, end, SEEK_SET);
}

int fmt_mtm_save_song(disko_t *fp, song_t *song)
{
	char *t = NULL;
	int n = 0, c = 0, rows = 0;
	struct mtm_header hdr = {0};
	song_note_t *pat;
	struct mtm_track tracks = {0};
	struct mtm_track *trk = NULL, *trk2 = NULL;
	uint16_t *seq = NULL;
	uint8_t ord[128] = {0};
	uint8_t *o = NULL;
	int nord, nsmp, npat;
	song_sample_t *ss = NULL;
	struct mtm_sample smp = {0};
	uint32_t warn = 0;

	if (song->flags & SONG_INSTRUMENTMODE)
		warn |= 1 << WARN_INSTRUMENTS;
	if (song->flags & SONG_LINEARSLIDES)
		warn |= 1 << WARN_LINEARSLIDES;
	if (song->initial_speed != 6 || song->initial_tempo != 125)
		warn |= 1 << WARN_DEFSPEEDTEMPO;

	nord = csf_get_num_orders(song) + 1;
	nord = CLAMP(nord, 2, MAX_ORDERS);

	nsmp = csf_get_num_samples(song);
	if (!nsmp)
		nsmp = 1;

	if (nsmp > 31) {
		nsmp = 31;
		warn |= 1 << WARN_MAXSAMPLES;
	}

	npat = csf_get_num_patterns(song);
	if (!npat)
		npat = 1;

	if (npat > 100) {
		npat = 100;
		warn |= 1 << WARN_MAXPATTERNS;
	}

	log_appendf(5, " %d orders, %d samples, %d patterns", nord, nsmp, npat);

	memcpy(hdr.filever, "MTM\x10", 4);
	strncpy(hdr.title, song->title, 20);

	// pack the tracks and get the counts
	hdr.last_pattern = npat;

	// get how many rows we need
	for (n = 0; n <= hdr.last_pattern; n++) {
		int x = song_get_pattern(n, &pat);

		if (n > 0 && x != rows)
			warn |= (1 << WARN_PATTERNLEN);

		if (x > rows)
			rows = x;
	}

	if (rows > 64) {
		warn |= (1 << WARN_PATTERNLENOF);
		rows = 64;
	}

	tracks.next = NULL;
	seq = calloc(32 * (hdr.last_pattern + 1), 2);
	for (n = 0; n <= hdr.last_pattern; n++) {
		rows = song_get_pattern(n, &pat);
		for (c = 0; c < 32; c++) {
			if (song_get_channel(c)->flags & CHN_MUTE)
				continue;
			trk = mtm_track(pat + c, rows, &warn);
			if (!trk)
				continue;
			hdr.nchannels = MAX(hdr.nchannels, c + 1);
			trk2 = link_track(&tracks, trk);
			if (trk2 == trk)
				trk2->id = ++hdr.ntracks;
			seq[32 * n + c] = bswapLE16(trk2->id);
		}
	}
	/* be nice to the big indians */
	hdr.ntracks = bswapLE16(hdr.ntracks);

	o = song->orderlist;
	for (n = 0; n < 128; n++) {
		if (o[n] > 199)
			break;
		ord[n] = o[n];
	}
	hdr.last_order = MAX(0, n - 1);

	// This is filled in later when the song message is written
	hdr.msglen = 0;

	hdr.nsamples = nsmp;

	// XXX can we use this to mark this as created by Schism?
	// Do any players specifically check for zero here?
	hdr.flags = 0;

	hdr.rows = rows;

	for (n = 0; n < 32; n++) {
		song_channel_t *ch = song->channels + n;
		if (song->flags & SONG_NOSTEREO)
			hdr.panpos[n] = 8;
		else
			hdr.panpos[n] = (ch->panning * 15 / 256) & 0xf; /* XXX modplug */
	}

	/* yay, we can write the header now */
	write_mtm_song_header(&hdr, fp);

	/* sampletime */
	for (n = 0, ss = song->samples + 1; n < nsmp; n++, ss++) {
		const int multiply = (ss->flags & CHN_16BIT) ? 2 : 1;

		smp.flags = (ss->flags & CHN_16BIT) ? 1 : 0;

		if (ss->flags & CHN_LOOP) {
			smp.loop_start = bswapLE32(ss->loop_start * multiply);
			smp.loop_end = bswapLE32(ss->loop_end * multiply)+1;
		} else {
			smp.loop_start = smp.loop_end = 0;
		}

		strcpy(smp.name, ss->name);
		smp.length = ss->data ? bswapLE32(ss->length * multiply) : 0;
		smp.volume = ss->volume / 4; //mphack

		// convert frequency, then clamp to 8-bit min/max
		int32_t x = frequency_to_transpose(ss->c5speed) / 16;
		smp.finetune = CLAMP(x, INT8_MIN, INT8_MAX);

		write_mtm_sample_header(&smp, fp);
	}

	disko_write(fp, ord, sizeof(ord));

	trk2 = NULL;
	for (trk = tracks.next; trk; trk = trk->next) {
		disko_write(fp, trk->data, sizeof(trk->data));
		free(trk2);
		trk2 = trk;
	}
	disko_write(fp, seq, 2 * 32 * (hdr.last_pattern + 1));

	free(seq);

	/* TODO: stupidly-formatted message. should be 20 rows of exactly 40 chars
	   each, and \0-padded (plus, at least one \0 at the end of each line) */
	write_mtm_song_message(song->message, fp, &warn);

	for (n = 0, ss = song->samples + 1; n < nsmp; n++, ss++) {
		if (ss->global_volume != 64)
			warn |= 1 << WARN_SAMPLEVOL;

		if ((ss->flags & (CHN_LOOP | CHN_PINGPONGLOOP)) == (CHN_LOOP | CHN_PINGPONGLOOP)
			|| (ss->flags & CHN_SUSTAINLOOP))
			warn |= 1 << WARN_LOOPS;

		if (ss->vib_depth != 0)
			warn |= 1 << WARN_SAMPLEVIB;

		csf_write_sample(fp, ss, SF_LE | SF_PCMU
			| ((ss->flags & CHN_16BIT) ? SF_16 : SF_8)
			| (SF_M),
			UINT32_MAX);
	}

	/* announce all the things we broke */
	for (n = 0; n < MAX_WARN; n++)
		if (warn & (1 << n))
			log_appendf(4, " Warning: %s unsupported in MTM format", mtm_warnings[n]);

	return SAVE_SUCCESS;
}
