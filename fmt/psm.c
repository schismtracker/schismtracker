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
#include "bits.h"
#include "charset.h"
#include "slurp.h"
#include "fmt.h"
#include "log.h"
#include "mem.h"
#include "str.h"

#include "player/sndfile.h"

/* There are four(!) different variations of the PSM format, all
 * incompatible with one another. In order, from oldest to newest(-ish):
 *   1. PS16: unused. I do not know of any files that actually use this
 *      format. Documentation for it *does* exist on modland though.
 *   2. PSM16: used in Silverball, and is closer in relation to Scream
 *      Tracker modules than later versions[citation needed]. Documentation
 *      for this format also exists on modland.
 *   3. "New" PSM: used in Jazz Jackrabbit, Epic Pinball, Extreme Pinball,
 *      and One Must Fall. This one is kind of a mess, probably to get
 *      everything to play as intended.
 *   4. Sinaria: a variation of the previous one, only used in the game
 *      Sinaria. There are a couple subtle differences that make it easy
 *      to detect, e.g. in pattern indexes. This format handles effects
 *      slightly different than the "New" PSM format, which is adjusted
 *      accordingly in the below code. */

/* ------------------------------------------------------------------------ */
/* "New" PSM support starts here */

#define ID_DSMP UINT32_C(0x44534D50)
#define ID_INST UINT32_C(0x494E5354)
#define ID_OPLH UINT32_C(0x4F504C48)
#define ID_PATT UINT32_C(0x50415454)
#define ID_PBOD UINT32_C(0x50424F44)
#define ID_PPAN UINT32_C(0x5050414E)
#define ID_SONG UINT32_C(0x534F4E47)
#define ID_TITL UINT32_C(0x5449544C)

/* this ends the file pointer on the start of all the chunks */
static int psm_verify_header(slurp_t *fp)
{
	unsigned char magic[4];

	if (slurp_read(fp, magic, sizeof(magic)) != sizeof(magic)
		|| memcmp(magic, "PSM ", 4))
		return 0;

	slurp_seek(fp, 4, SEEK_CUR);

	if (slurp_read(fp, magic, sizeof(magic)) != sizeof(magic)
		|| memcmp(magic, "FILE", 4))
		return 0;

	return 1;
}

int fmt_psm_read_info(dmoz_file_t *file, slurp_t *fp)
{
	if (!psm_verify_header(fp))
		return 0;

	{
		iff_chunk_t c;

		while (iff_chunk_peek_ex(&c, fp, IFF_CHUNK_SIZE_LE)) {
			if (c.id == ID_TITL) {
				/* we only need the title, really */
				unsigned char title[256];

				iff_chunk_read(&c, fp, title, sizeof(title));

				file->title = strn_dup((const char *)title, sizeof(title));
				break;
			}
		}
	}

	/* I had it all, and I looked at it and said:
	 * 'This is a bigger jail than I just got out of.' */
	file->description = "Epic MegaGames MASI";
	/*file->extension = str_dup("psm");*/
	file->type = TYPE_MODULE_S3M;

	return 1;
}

/* eh */
static uint8_t psm_convert_portamento(uint8_t param, int sinaria)
{
	if (sinaria) return param;

	return (param < 4)
		? (param | 0xF0)
		: (param >> 2);
}

static int psm_import_effect(song_note_t *note, slurp_t *fp, int sinaria)
{
	/* e[0] == effect
	 * e[1] == param */

	uint8_t e[2];

	if (slurp_read(fp, e, sizeof(e)) != sizeof(e))
		return 0;

	switch (e[0]) {
	/* the logic to handle these was gracefully stolen from openmpt. */
	case 0x01: /* Fine volume slide up */
		note->effect = FX_VOLUMESLIDE;

		note->param = (sinaria)
			? (e[1] << 4)
			: ((e[1] & 0x1E) << 3);

		note->param |= 0x0F;
		break;
	case 0x02: /* Volume slide up */
		note->effect = FX_VOLUMESLIDE;

		note->param = (sinaria)
			? (e[1] << 4)
			: (e[1] << 3);

		note->param &= 0x0F;

		break;
	case 0x03: /* Fine volume slide down */
		note->effect = FX_VOLUMESLIDE;

		note->param = (sinaria)
			? (e[1])
			: (e[1] >> 1);

		note->param |= 0xF0;

		break;
	case 0x04: /* Volume slide down */
		note->effect = FX_VOLUMESLIDE;

		note->param = (sinaria)
			? (e[1] & 0x0F)
			: (e[1] < 2)
				? (e[1] | 0xF0)
				: ((e[1] >> 1) & 0x0F);
		break;

	/* Portamento! */
	case 0x0B: /* Fine portamento up */
		note->effect = FX_PORTAMENTOUP;
		note->param = (0xF0 | psm_convert_portamento(e[1], sinaria));
		break;
	case 0x0C: /* Portamento up */
		note->effect = FX_PORTAMENTOUP;
		note->param = psm_convert_portamento(e[1], sinaria);
		break;
	case 0x0D: /* Fine portamento down */
		note->effect = FX_PORTAMENTODOWN;
		note->param = (0xF0 | psm_convert_portamento(e[1], sinaria));
		break;
	case 0x0E: /* Portamento down */
		note->effect = FX_PORTAMENTODOWN;
		note->param = psm_convert_portamento(e[1], sinaria);
		break;

	case 0x0F: /* Tone portamento */
		note->effect = FX_TONEPORTAMENTO;

		note->param = (sinaria)
			? (e[1])
			: (e[1] >> 2);
		break;
	case 0x10: /* Tone portamento + volume slide up */
		note->effect = FX_TONEPORTAVOL;
		note->param = (e[1] & 0xF0);
		break;
	case 0x12: /* Tone portamento + volume slide down */
		note->effect = FX_TONEPORTAVOL;
		note->param = ((e[1] >> 4) & 0x0F);
		break;

	case 0x11: /* Glissando control */
		note->effect = FX_SPECIAL;
		note->param = 0x10 | (e[1] & 0x01);
		break;

	case 0x13: /* Scream Tracker Sxx -- "actually hangs/crashes MASI" */
		note->effect = FX_SPECIAL;
		note->param = e[1];
		break;

	/* Vibrato */
	case 0x15: /* Vibrato */
		note->effect = FX_VIBRATO;
		note->param = e[1];
		break;
	case 0x16: /* Vibrato waveform */
		note->effect = FX_SPECIAL;
		note->param = (0x30 | (e[1] & 0x0F));
		break;
	case 0x17: /* Vibrato + volume slide up */
		note->effect = FX_VIBRATOVOL;
		note->param = e[1] & 0xF0;
		break;
	case 0x18: /* Vibrato + volume slide down */
		note->effect = FX_VIBRATOVOL;
		note->param = e[1]; /* & 0x0F ?? */
		break;

	/* Tremolo */
	case 0x1F: /* Tremolo */
		note->effect = FX_TREMOLO;
		note->param = e[1];
		break;
	case 0x20: /* Tremolo waveform */
		note->effect = FX_SPECIAL;
		note->param = (0x40 | (e[1] & 0x0F));
		break;

	/* Sample commands */
	case 0x29: /* Offset */
		/* Oxx - offset (xx corresponds to the 2nd byte) */
		if (slurp_read(fp, e, 2) != 2)
			return 0;

		note->effect = FX_OFFSET;
		note->param = e[0];

		break;
	case 0x2A: /* Retrigger */
		note->effect = FX_RETRIG;
		note->param = e[1];
		break;
	case 0x2B: /* Note Cut */
		note->effect = FX_SPECIAL;
		note->param = (0xC0 | e[1]);
		break;
	case 0x2C: /* Note Delay */
		note->effect = FX_SPECIAL;
		note->param = (0xD0 | e[1]);
		break;

	/* Position change */
	case 0x33: /* Position jump -- ignored by PLAY.EXE */
		/* just copied what OpenMPT does here... */
		note->effect = FX_POSITIONJUMP;
		note->param = (e[1] >> 1);

		slurp_seek(fp, 1, SEEK_CUR);

		break;
	case 0x34: /* Break to row */
		note->effect = FX_PATTERNBREAK;

		/* PLAY.EXE entirely ignores the parameter (it always breaks to the first
		 * row), so it is maybe best to do this in your own player as well. */
		note->param = 0;
		break;
	case 0x35: /* Pattern loop -- are you sure you want to know? :-) */
		note->effect = FX_SPECIAL;
		note->param = (0xB0 | (e[1] & 0x0F));
		break;
	case 0x36: /* Pattern delay */
		note->effect = FX_SPECIAL;
		note->param = (0xE0 | (e[1] & 0x0F));
		break;

	/* Speed change */
	case 0x3D: /* Set speed */
		note->effect = FX_SPEED;
		note->param = e[1];
		break;
	case 0x3E: /* Set tempo */
		note->effect = FX_TEMPO;
		note->param = e[1];
		break;

	/* Misc. commands */
	case 0x47: /* arpeggio */
		note->effect = FX_ARPEGGIO;
		note->param = e[1];
		break;
	case 0x48: /* Set finetune */
		note->effect = FX_SPECIAL;
		note->param = (0x20 | (e[1] & 0x0F));
		break;
	case 0x49: /* Set balance */
		note->effect = FX_SPECIAL;
		note->param = (0x80 | (e[1] & 0x0F));
		break;

	default:
		note->effect = FX_NONE;
		note->param = e[1]; /* eh */
		break;
	}

	return 1;
}

static long psm_read_pattern_index(slurp_t *fp, int *sinaria)
{
	int old_errno;
	long r;
	char id[5];
	size_t offset = 1;

	/* "Pxxx" */
	if (slurp_read(fp, id, 4) != 4)
		return -1;

	if (!memcmp(id, "PATT", 4)) {
		/* Sinaria: "PATTxxxx" */
		if (slurp_read(fp, id, 4) != 4)
			return -1;

		*sinaria = 1;
		offset = 0;
	}

	/* NUL terminate for strtol() */
	id[4] = '\0';

	old_errno = errno;
	errno = 0;

	r = strtol(id + offset, NULL, 10);

	if (!r && errno)
		return -1;

	errno = old_errno;

	return r;
}

enum {
	PSM_PATTERN_FLAG_NOTE = 0x80,
	PSM_PATTERN_FLAG_INSTRUMENT = 0x40,
	PSM_PATTERN_FLAG_VOLUME = 0x20,
	PSM_PATTERN_FLAG_EFFECT = 0x10,
};

static int psm_read_pattern(iff_chunk_t *c, slurp_t *fp, song_t *song, int *sinaria, uint32_t *nchns)
{
	uint32_t length;
	uint16_t rows, i;
	long index;

	*nchns = 0;

	slurp_seek(fp, c->offset, SEEK_SET);

	if (slurp_read(fp, &length, 4) != 4)
		return 0;

	length = bswapLE32(length);

	if (length != c->size || length < 8)
		return 0;

	index = psm_read_pattern_index(fp, sinaria);
	if (index < 0 || index >= MAX_PATTERNS)
		return 0;

	if (slurp_read(fp, &rows, 2) != 2)
		return 0;

	rows = bswapLE16(rows);

	song->patterns[index] = csf_allocate_pattern(rows);
	song->pattern_size[index] = rows;

	for (i = 0; i < rows; i++) {
		song_note_t *row = (song->patterns[index] + (i * MAX_CHANNELS));
		uint16_t row_size;
		int64_t start;

		if (slurp_read(fp, &row_size, 2) != 2)
			return 0;

		row_size = bswapLE16(row_size);
		if (row_size <= 2)
			continue;

		start = slurp_tell(fp);

		while (slurp_tell(fp) + 3 - 2 < start + row_size - 2) {
			/* xx[0] == flags
			 * xx[1] == channel */
			uint8_t xx[2];
			song_note_t *note;

			if (slurp_read(fp, xx, sizeof(xx)) != sizeof(xx))
				return 0;

			if (xx[1] >= MAX_CHANNELS)
				continue;

			*nchns = MAX(*nchns, xx[1]);

			note = row + xx[1];

			if (xx[0] & 0x80) {
				/* note */
				int n = slurp_getc(fp);
				if (n == EOF)
					return 0;

				if (*sinaria) {
					note->note = (n < 85)
						? (n + 36)
						: n;
				} else {
					note->note = (n == 0xFF)
						? NOTE_CUT
						: (n < 129)
							? (n & 0x0F) + (12 * (n >> 4) + 13)
							: NOTE_NONE;
				}
			}

			if (xx[0] & 0x40) {
				int s = slurp_getc(fp);
				if (s == EOF)
					return 0;

				note->instrument = s + 1;
			}

			if (xx[0] & 0x20) {
				int v = slurp_getc(fp);
				if (v == EOF)
					return 0;

				note->voleffect = VOLFX_VOLUME;
				note->volparam = (MIN(v, 127) + 1) / 2;
			}

			if ((xx[0] & 0x10) && !psm_import_effect(note, fp, *sinaria))
				return 0;
		}
	}

	return 1;
}

int fmt_psm_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	/* the (way too much) stack data */
	iff_chunk_t song_chunks[MAX_ORDERS] = {0},
	            titl_chunk = {0},
	            dsmp_chunks[MAX_SAMPLES] = {0},
	            pbod_chunks[MAX_PATTERNS] = {0};
	uint32_t nsubsongs = 0,
	         nsamples = 0,
	         npatterns = 0,
	         norders = 0,
	         nchns = 0,
	         i;

	/* "Now, grovel at the feet of the Archdemon Satanichia!" */
	int sinaria = 0;

	if (!psm_verify_header(fp))
		return LOAD_UNSUPPORTED;

	{
		iff_chunk_t c;
		while (iff_chunk_peek_ex(&c, fp, IFF_CHUNK_SIZE_LE)) {
			switch (c.id) {
			case ID_SONG: /* order list, channel & module settings */
				if (nsubsongs >= MAX_ORDERS)
					break; /* don't care */

				song_chunks[nsubsongs++] = c;
				break;
			case ID_DSMP:
				if (nsamples >= MAX_SAMPLES)
					break;

				dsmp_chunks[nsamples++] = c;
				break;
			case ID_PBOD:
				if (npatterns >= MAX_PATTERNS)
					break;

				pbod_chunks[npatterns++] = c;
				break;
			case ID_TITL:
				titl_chunk = c;
				break;
			}
		}
	}

	if (titl_chunk.id) {
		/* UNTESTED -- I don't have any PSM files with this chunk. :) */
		iff_chunk_read(&titl_chunk, fp, song->title, sizeof(song->title));
	}

	/* need to load patterns; otherwise we won't know
	 * whether we're Sinaria or not! */
	for (i = 0; i < npatterns; i++)
		psm_read_pattern(pbod_chunks + i, fp, song, &sinaria, &nchns);

	if (!(lflags & LOAD_NOSAMPLES)) {
		for (i = 0; i < nsamples; i++) {
			song_sample_t *smp;
			uint8_t flags;    /* flags: 0x80 == loop -- I don't know of any others */
			char filename[8]; /* Filename of the original module (without extension) */
			char name[33];    /* ??? */
			uint8_t b;
			uint16_t w;
			uint32_t dw;

			slurp_seek(fp, dsmp_chunks[i].offset, SEEK_SET);

			if (slurp_read(fp, &flags, 1) != 1) break;
			if (slurp_read(fp, &filename, sizeof(filename)) != sizeof(filename)) break;

			/* skip sample ID */
			slurp_seek(fp, (sinaria ? 8 : 4), SEEK_CUR);

			if (slurp_read(fp, &name, sizeof(name)) != sizeof(name)) break;

			/* skip unknown bytes */
			slurp_seek(fp, 6, SEEK_CUR);

			/* this is the actual sample number */
			if (slurp_read(fp, &w, sizeof(w)) != sizeof(w))
				break;

			w = bswapLE16(w);
			if (w > MAX_SAMPLES)
				continue; /* hm */
			smp = &song->samples[w + 1];

			/* now, put everything we just read into the sample slot */
			smp->flags = 0;
			if (flags & 0x80)
				smp->flags |= CHN_LOOP;

			memcpy(smp->name, name, MIN(sizeof(smp->name), sizeof(name)));
			memcpy(smp->filename, filename, MIN(sizeof(smp->filename), sizeof(filename)));

			if (slurp_read(fp, &dw, sizeof(dw)) != sizeof(dw)) break;
			smp->length = bswapLE32(dw);

			if (slurp_read(fp, &dw, sizeof(dw)) != sizeof(dw)) break;
			smp->loop_start = bswapLE32(dw);

			if (slurp_read(fp, &dw, sizeof(dw)) != sizeof(dw)) break;

			if (sinaria) {
				smp->loop_end = bswapLE32(dw);
			} else if (dw) {
				/* blurb from OpenMPT:
				 * Note that we shouldn't add + 1 for MTM conversions here (e.g. the OMF 2097 music),
				 * but I think there is no way to figure out the original format, and in the case of the OMF 2097 soundtrack
				 * it doesn't make a huge audible difference anyway (no chip samples are used).
				 * On the other hand, sample 8 of MUSIC_A.PSM from Extreme Pinball will sound detuned if we don't adjust the loop end here. */
				smp->loop_end = bswapLE32(dw) + 1;
			}

			/* skip unknown bytes */
			slurp_seek(fp, (sinaria ? 2 : 1), SEEK_CUR);

			/* skip finetune */
			slurp_seek(fp, 1, SEEK_CUR);

			if (slurp_read(fp, &b, sizeof(b)) != sizeof(b)) break;
			smp->volume = (b + 1) * 2; /* this is what OpenMPT does */

			/* skip unknown bytes */
			slurp_seek(fp, 4, SEEK_CUR);

			if (sinaria) {
				if (slurp_read(fp, &w, sizeof(w)) != sizeof(w)) break;
				smp->c5speed = bswapLE16(w);
			} else {
				if (slurp_read(fp, &dw, sizeof(dw)) != sizeof(dw)) break;
				smp->c5speed = bswapLE32(dw);
			}

			/* skip padding */
			slurp_seek(fp, (sinaria) ? 16 : 19, SEEK_CUR);

			csf_read_sample(smp, SF(LE,M,8,PCMD), fp);
		}
	}

	/* Now that we've loaded samples and patterns, let's load all
	 * the stupid subsong information. */
	for (i = 0; i < nsubsongs; i++) {
		iff_chunk_t oplh_chunk = {0}, ppan_chunk = {0};
		iff_chunk_t c;
		int subsong_channels;

		slurp_seek(fp, song_chunks[i].offset, SEEK_SET);

		/* skip song type (9 characters), and compression (1 8-bit int) */
		slurp_seek(fp, 10, SEEK_CUR);

		subsong_channels = slurp_getc(fp);
		if (subsong_channels == EOF)
			break; /* ??? */

		if (norders > 0) {
			/* can't really load anything else? */
			if (norders + 1 > MAX_ORDERS)
				break;

			/* anything past the first song are "hidden patterns" */
			song->orderlist[norders++] = ORDER_LAST;
		}

		/* sub-chunks */
		while (iff_chunk_peek_ex(&c, fp, IFF_CHUNK_SIZE_LE) && (slurp_tell(fp) <= song_chunks[i].offset + song_chunks[i].size)) {
			switch (c.id) {
			/* should we handle DATE? */
			case ID_OPLH: /* Order list, channel, and module settings */
				oplh_chunk = c;
				break;
			case ID_PPAN: /* Channel pan table */
				ppan_chunk = c;
				break;
			}
		}

		if (oplh_chunk.id && oplh_chunk.size >= 9) {
			uint32_t chunk_count = 0, first_order_chunk = UINT32_MAX;

			slurp_seek(fp, oplh_chunk.offset, SEEK_SET);

			/* skip chunk amount (uint16_t) */
			slurp_seek(fp, 2, SEEK_CUR);

			while (slurp_tell(fp) <= oplh_chunk.offset + oplh_chunk.size) {
				/* FIXME: Pretty much all of these treat the subsong it's working
				 * on as if it were the *whole song*, which is wrong. */
				int opcode = slurp_getc(fp);
				if (opcode == EOF || !opcode)
					break; /* last chunk reached? */

				/* this stuff was all stolen from openmpt. */
				switch (opcode) {
				case 0x01: {
					/* Play order list item */
					long index;

					index = psm_read_pattern_index(fp, &sinaria);
					if (index < 0)
						break; /* wut */

					if (norders + 1 > MAX_ORDERS)
						break;

					if (index == 0xFF)
						index = ORDER_LAST;
					else if (index == 0xFE)
						index = ORDER_SKIP;

					song->orderlist[norders++] = index;

					// Decide whether this is the first order chunk or not (for finding out the correct restart position)
					if (first_order_chunk == UINT32_MAX)
						first_order_chunk = chunk_count;
					break;
				}

				case 0x02:
					/* Play Range (xx from line yy to line zz).
					 * Three 16-bit parameters but it seems like the next opcode
					 * is parsed at the same position as the third parameter. */
					slurp_seek(fp, 4, SEEK_CUR);
					break;

				case 0x03:
					/* Jump Loop (like Jump Line, but with a third, unknown byte
					 * following -- nope, it does not appear to be a loop count) */
				case 0x04: {
					/* Jump Line (Restart position) */
				
					uint16_t restart_chunk = 0;
					slurp_read(fp, &restart_chunk, 2);
					restart_chunk = bswapLE16(restart_chunk);

					if (restart_chunk >= first_order_chunk)
						csf_insert_restart_pos(song, restart_chunk - first_order_chunk);

					if (opcode == 0x03)
						slurp_seek(fp, 1, SEEK_CUR);

					break;
				}

				case 0x06: // Transpose (appears to be a no-op in MASI)
					slurp_seek(fp, 1, SEEK_CUR);
					break;

				case 0x05: {
					/* Channel Flip (changes channel type without changing pan position) */
					uint8_t x[2];

					if (slurp_read(fp, x, sizeof(x)) != sizeof(x))
						break;

					if (x[0] >= MAX_CHANNELS)
						break;

					song->channels[x[0]].panning = x[1];

					break;
				}

				case 0x07: { /* Default Speed */
					int speed = slurp_getc(fp);
					if (speed != EOF)
						song->initial_speed = speed;
					break;
				}

				case 0x08: { /* Default Tempo */
					int tempo = slurp_getc(fp);
					if (tempo != EOF)
						song->initial_tempo = tempo;
					break;
				}

				case 0x0C: { /* "Sample map table" */
					uint8_t table[6];

					if (slurp_read(fp, table, 6) != 6)
						break;

					if (memcmp(table, "\x00\xFF\x00\x00\x01\x00", 6))
						return LOAD_UNSUPPORTED;

					break;
				}

				case 0x0D: {
					/* Channel panning table - can be set using CONVERT.EXE /E */
					uint8_t x[3];

					if (slurp_read(fp, x, sizeof(x)) != sizeof(x))
						break;

					if (x[0] >= MAX_CHANNELS)
						break;

					song->channels[x[0]].panning = x[1];

					/* FIXME: need to handle the pan TYPE as well
					 *
					 * this format is so fucking bad */

					break;
				}

				case 0x0E: {
					/* Channel volume table (0...255) - can be set using CONVERT.EXE /E, is
					 * 255 in all "official" PSMs except for some OMF 2097 tracks */
					uint8_t x[2];

					if (slurp_read(fp, x, sizeof(x)) != sizeof(x))
						break;

					if (x[0] >= MAX_CHANNELS)
						break;

					song->channels[x[0]].volume = (x[1] / 4) + 1;

					break;
				}

				default:
					log_appendf(4, " PSM/OPLH: unknown opcode: %" PRIx8 " at %" PRIu32, opcode, chunk_count);
					return LOAD_UNSUPPORTED;
				}

				chunk_count++;
			}
		}

		if (ppan_chunk.id && !i /* TODO handle other subsongs */) {
			uint8_t xx[MAX_CHANNELS * 2];
			uint32_t j;

			/* FIXME don't crash here? lol */
			SCHISM_RUNTIME_ASSERT(ppan_chunk.size >= (uint32_t)subsong_channels * 2,
				"PSM: PPAN chunk is too small");

			slurp_seek(fp, ppan_chunk.offset, SEEK_SET);

			if (slurp_read(fp, xx, subsong_channels * 2) != (size_t)(subsong_channels * 2))
				break;

			for (j = 0; j < nchns; j++) {
				uint8_t *x = xx + (j * 2);

				switch (x[0]) {
				case 0: /* normal panning */
					song->channels[j].panning = x[1];
					break;
				case 1: /* surround */
					song->channels[j].panning = 128;
					song->channels[j].flags = CHN_SURROUND;
					break;
				case 2:
					song->channels[j].panning = 128;
					break;
				}
			}
		}
	}

	song->pan_separation = 128;
	song->flags = SONG_ITOLDEFFECTS | SONG_COMPATGXX;

	snprintf(song->tracker_id, sizeof(song->tracker_id), "%s",
		(sinaria)
			? "Epic MegaGames MASI (New Version / Sinaria)"
			: "Epic MegaGames MASI (New Version)");

	return LOAD_SUCCESS;
}

/* ------------------------------------------------------------------------ */
/* here comes PSM16...
 *
 * ...you know, I wrote this loader hoping that there would actually be some
 * value in the songs that I'd actually be able to load, but the only one
 * I really like is "snooker champ". also there's that one that uses samples
 * from "beyond music", I guess. besides that there's not a whole lot of
 * value to this format. :p */

static int psm16_verify_header(slurp_t *fp)
{
	/* This is probably way more validation than necessary, but oh well :) */
	int b, i;
	uint16_t w;
	unsigned char magic[4];

	if (slurp_read(fp, magic, 4) != 4 || memcmp(magic, "PSM\xFE", 4))
		return 0;

	slurp_seek(fp, 59, SEEK_CUR);

	/* "The final byte, 60, MUST have a ^Z in it." */
	if (slurp_getc(fp) != 0x1A)
		return 0;

	b = slurp_getc(fp);
	if (b == EOF || (b & 0x03))
		return 0;

	b = slurp_getc(fp);
	if (b != 0x10 && b != 0x01 /* from OpenMPT: this is sometimes 0x01 !!! */)
		return 0;

	b = slurp_getc(fp);
	if (b != 0 /* "255ch" format not documented, and I don't care */)
		return 0;

	/* skip speed */
	slurp_seek(fp, 1, SEEK_CUR);

	/* skip """BPM""", actually tempo, and yes, they are very different */
	b = slurp_getc(fp);
	if (!(b >= 32 && b <= 255))
		return 0;

	/* skip master volume */
	slurp_seek(fp, 1, SEEK_CUR);

	for (i = 0; i < 4; i++) {
		/* next four values are all 16-bit values,
		 * and all have the exact same limits */
		if (slurp_read(fp, &w, 2) != 2)
			return 0;

		w = bswapLE16(w);

		if (!(w >= 1 && w <= 255))
			return 0;
	}

	/* "Number of Channels to Play" and "Number of Channels to Process"...
	 * docs say that this varies from 1 to 32, so I suppose the 255
	 * channel format is just completely meaningless. */
	for (i = 0; i < 2; i++) {
		if (slurp_read(fp, &w, 2) != 2)
			return 0;

		w = bswapLE16(w);

		if (!(w >= 1 && w <= 32))
			return 0;
	}

	return 1;
}

int fmt_psm16_read_info(dmoz_file_t *file, slurp_t *fp)
{
	if (!psm16_verify_header(fp))
		return 0;

	{
		char title[60];
		int i;

		slurp_seek(fp, 4, SEEK_SET);

		slurp_read(fp, title, 59);
		title[59] = '\0';

		/* discover never-before-seen secrets of the damned */
		for (i = 0; i < 59; i++)
			title[i] = title[i] ? title[i] : ' ';

		/* trim it up! */
		str_rtrim(title);

		/* only put in a title if there's actually any data there */
		if (*title)
			file->title = str_dup(title);
	}

	file->description = "Epic MegaGames MASI";
	/*file->extension = str_dup(".psm");*/
	file->type = TYPE_MODULE_S3M;

	return 1;
}

/* if this function succeeds, the file position is at the offset.
 * otherwise, it is unknown, and needs manual readjustment :) */
static int psm16_check_parapointer(slurp_t *fp, uint32_t paraptr,
	const char magic[4])
{
	unsigned char x[4];

	if (paraptr < 4)
		return 0;

	slurp_seek(fp, paraptr - 4, SEEK_SET);

	if (slurp_read(fp, x, 4) != 4 || memcmp(x, magic, 4))
		return 0;

	return 1;
}

enum {
	/* sample data flags */
	PSM16_SAMPLE_FLAG_SYNTHESIZED = (0x01), /* unused, AFAIK */
	PSM16_SAMPLE_FLAG_16BIT = (0x04),
	PSM16_SAMPLE_FLAG_UNSIGNED = (0x08),
	PSM16_SAMPLE_FLAG_DELTA = (0x10),
	PSM16_SAMPLE_FLAG_BIDI_LOOP = (0x20),

	/* loop flags */
	PSM16_SAMPLE_FLAG_LOOP = (0x80),

	PSM16_SAMPLE_FLAG_SAMPLE = (0xFF & ~PSM16_SAMPLE_FLAG_LOOP),
};

int fmt_psm16_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	int speed, tempo, master_volume;
	uint16_t norders, npatterns, nsamples, nchnplay, nchnprocess, nchns;
	uint32_t oorders, ochnpans, opatterns, osamples, omessage;
	int n;

	if (!psm16_verify_header(fp))
		return LOAD_UNSUPPORTED;

	slurp_seek(fp, 4, SEEK_SET);

	/* something fun I discovered: by doing it this way, we actually
	 * uncover mysterious hidden bits of the title (one of the songs
	 * in silverball literally is titled "\0eyond_music", lol) */
	slurp_read(fp, song->title, sizeof(song->title));
	slurp_seek(fp, 60 - sizeof(song->title), SEEK_CUR);

	/* skip stuff that has already been verified */
	slurp_seek(fp, 3, SEEK_CUR);

	speed = slurp_getc(fp);
	if (speed == EOF)
		return LOAD_UNSUPPORTED;

	tempo = slurp_getc(fp);
	if (tempo == EOF)
		return LOAD_UNSUPPORTED;

	master_volume = slurp_getc(fp);
	if (master_volume == EOF)
		return LOAD_UNSUPPORTED;

	song->initial_speed = speed;
	song->initial_tempo = tempo;

	/* from openmpt:
	 * Most of the time, the master volume value makes sense...
	 * ...Just not when it's 255. */
	song->mixing_volume = (master_volume == 255) ? 48 : master_volume;

	/* skip "song length" ??? */
	slurp_seek(fp, 2, SEEK_CUR);

	slurp_read(fp, &norders, sizeof(norders));
	slurp_read(fp, &npatterns, sizeof(npatterns));
	slurp_read(fp, &nsamples, sizeof(nsamples));
	slurp_read(fp, &nchnplay, sizeof(nchnplay));
	slurp_read(fp, &nchnprocess, sizeof(nchnprocess));

	norders = bswapLE16(norders);
	npatterns = bswapLE16(npatterns);
	nsamples = bswapLE16(nsamples);
	nchnplay = bswapLE16(nchnplay);
	nchnprocess = bswapLE16(nchnprocess);

	nchns = CLAMP(nchnprocess, nchnplay, MAX_CHANNELS);
	nsamples = MIN(nsamples, MAX_SAMPLES);

	slurp_read(fp, &oorders, sizeof(oorders));
	slurp_read(fp, &ochnpans, sizeof(ochnpans));
	slurp_read(fp, &opatterns, sizeof(opatterns));
	slurp_read(fp, &osamples, sizeof(osamples));
	slurp_read(fp, &omessage, sizeof(omessage));

	oorders = bswapLE32(oorders);
	ochnpans = bswapLE32(ochnpans);
	opatterns = bswapLE32(opatterns);
	osamples = bswapLE32(osamples);
	omessage = bswapLE32(omessage);

	/* mcmeat deluxe special now available at mickey d's */

	if (psm16_check_parapointer(fp, oorders, "PORD")) {
		size_t x = slurp_read(fp, song->orderlist, MIN(norders, MAX_ORDERS - 1));

		/* insert last order ... */
		song->orderlist[x] = ORDER_LAST;
	}

	if (psm16_check_parapointer(fp, ochnpans, "PPAN")) {
		uint8_t chnpans[MAX_CHANNELS];

		slurp_read(fp, chnpans, nchns);

		for (n = 0; n < nchns; n++)
			/* panning is reversed in PSM16 (15 is full left, 0 is full right) */
			song->channels[n].panning = (((15 - (chnpans[n] & 15)) * 256 + 8) / 15);
	}

	/* mute any extra channels */
	for (n = nchns; n < MAX_CHANNELS; n++)
		song->channels[n].flags |= CHN_MUTE;

	if (psm16_check_parapointer(fp, osamples, "PSAH")) {
		uint16_t i;

		for (i = 0; i < nsamples; i++) {
			song_sample_t *smp;
			char filename[13];
			char description[24];
			uint32_t osmpdata, flags, dw;
			uint16_t smpnum, c2freq;
			int b, finetune;

			slurp_seek(fp, osamples + i * 64, SEEK_SET);

			slurp_read(fp, filename, 13);
			slurp_read(fp, description, 24);
			slurp_read(fp, &osmpdata, 4);
			osmpdata = bswapLE32(osmpdata);
			slurp_seek(fp, 4, SEEK_CUR); /* "Memory Location" */
			slurp_read(fp, &smpnum, 2);
			smpnum = bswapLE16(smpnum);

			if (smpnum >= MAX_SAMPLES)
				continue; /* uhh? */

			/* finally... */
			smp = &song->samples[smpnum];

			memcpy(smp->filename, filename, 13);
			smp->filename[13] = '\0';
			memcpy(smp->name, description, 24);
			smp->name[24] = '\0';

			b = slurp_getc(fp);
			if (b == EOF)
				continue;

			/* convert bit flags to internal bit flags */
			flags = SF_M | SF_LE | ((b & PSM16_SAMPLE_FLAG_16BIT) ? SF_16 : SF_8);

			if (b & PSM16_SAMPLE_FLAG_UNSIGNED) {
				flags |= SF_PCMU;
			} else if ((b & PSM16_SAMPLE_FLAG_DELTA) || !(b & PSM16_SAMPLE_FLAG_SAMPLE)) {
				flags |= SF_PCMD;
			} else {
				flags |= SF_PCMS;
			}

			if (b & PSM16_SAMPLE_FLAG_BIDI_LOOP)
				smp->flags |= CHN_PINGPONGLOOP;

			if (b & PSM16_SAMPLE_FLAG_LOOP)
				smp->flags |= CHN_LOOP;

			slurp_read(fp, &dw, sizeof(dw));
			smp->length = bswapLE32(dw);

			if (b & PSM16_SAMPLE_FLAG_16BIT)
				smp->length >>= 1;

			slurp_read(fp, &dw, sizeof(dw));
			smp->loop_start = bswapLE32(dw);

			slurp_read(fp, &dw, sizeof(dw));
			smp->loop_end = bswapLE32(dw);

			finetune = slurp_getc(fp);
			if (finetune == EOF)
				continue;

			b = slurp_getc(fp);
			if (b == EOF)
				continue;

			smp->volume = b * 4; //mphack

			slurp_read(fp, &c2freq, 2);
			c2freq = bswapLE16(c2freq);

			/* voodoo magic effortlessly copied from openmpt  :) */
			smp->c5speed = c2freq * pow(2.0, ((finetune ^ 0x08) - 0x78) / (12.0 * 16.0));

			if (!(lflags & LOAD_NOSAMPLES)) {
				slurp_seek(fp, osmpdata, SEEK_SET);
				csf_read_sample(smp, flags, fp);
			}
		}
	}

	if (!(lflags & LOAD_NOPATTERNS) && psm16_check_parapointer(fp, opatterns, "PPAT")) {
		/* ok, let's do this */
		uint16_t i;

		for (i = 0; i < npatterns; i++) {
			int64_t start = slurp_tell(fp);

			uint16_t size;
			int nrows, nchans;
			int row = 0;

			slurp_read(fp, &size, 2);
			size = bswapLE16(size);

			if (size < 4)
				continue; /* WTF */

			nrows = slurp_getc(fp);
			nchans = slurp_getc(fp);

			song->patterns[i] = csf_allocate_pattern(nrows);
			song->pattern_size[i] = nrows;

			while (slurp_tell(fp) <= start + size && row < nrows) {
				int command = slurp_getc(fp);
				int chan;
				song_note_t *nn;

				if (command == 0 || command == EOF) {
					row++;
					continue;
				}

				chan = MIN(command & 31, nchans);

				nn = song->patterns[i] + (row * MAX_CHANNELS) + chan;

				if (command & 0x80) {
					int note, instr;

					note = slurp_getc(fp);
					if (note == EOF) break;

					instr = slurp_getc(fp);
					if (instr == EOF) break;

					nn->note = note + (3 * 12); /* 3 octaves up to adjust for c2freq */
					nn->instrument = instr;
				}

				if (command & 0x40) {
					int volume;

					volume = slurp_getc(fp);
					if (volume == EOF) break;

					nn->voleffect = VOLFX_VOLUME;
					nn->volparam = MIN(volume, 64);
				}

				if (command & 0x20) {
					int effect, param;

					effect = slurp_getc(fp);
					if (effect == EOF) break;

					param = slurp_getc(fp);
					if (param == EOF) break;

					switch (effect) {
					/* Volume Commands */
					case 0x01: /* Fine Volume Slide Up */
						nn->effect = FX_VOLUMESLIDE;
						nn->param = (param << 4) | 0x0F;
						break;
					case 0x02: /* Volume Slide Up */
						nn->effect = FX_VOLUMESLIDE;
						nn->param = (param << 4) & 0xF0;
						break;
					case 0x03: /* Fine Volume Slide Down */
						nn->effect = FX_VOLUMESLIDE;
						nn->param = param | 0xF0;
						break;
					case 0x04: /* Volume Slide Down */
						nn->effect = FX_VOLUMESLIDE;
						nn->param = param & 0x0F;
						break;

					/* Portamento Commands */
					case 0x0A: /* Fine Portamento Up */
						nn->effect = FX_PORTAMENTOUP;
						nn->param = param | 0xF0;
						break;
					case 0x0B: /* Portamento Down */
						nn->effect = FX_PORTAMENTOUP;
						nn->param = param;
						break;
					case 0x0C: /* Fine Portamento Down */
						nn->effect = FX_PORTAMENTODOWN;
						nn->param = param | 0xF0;
						break;
					case 0x0D: /* Portamento Down */
						nn->effect = FX_PORTAMENTODOWN;
						nn->param = param;
						break;
					case 0x0E: /* Tone Portamento */
						nn->effect = FX_TONEPORTAMENTO;
						nn->param = param;
						break;
					case 0x0F: /* Set Glissando Control */
						nn->effect = FX_SPECIAL;
						nn->param = 0x10 | (param & 0x0F);
						break;
					case 0x10: /* Tone Port+Vol Slide Up */
						nn->effect = FX_TONEPORTAVOL;
						nn->param = (param << 4);
						break;
					case 0x11: /* Tone Port+Vol Slide Down */
						nn->effect = FX_TONEPORTAVOL;
						nn->param = (param & 0x0F);
						break;

					/* Vibrato Commands */
					case 0x14: /* Vibrato */
						nn->effect = FX_VIBRATO;
						nn->param = param;
						break;
					case 0x15: /* Set Vibrato Waveform */
						nn->effect = FX_SPECIAL;
						nn->param = (0x30 | (param & 0x0F));
						break;
					case 0x16: /* Vibrato+Vol Slide Up */
						nn->effect = FX_VIBRATOVOL;
						nn->param = (param << 4);
						break;
					case 0x17: /* Vibrato+Vol Slide Down */
						nn->effect = FX_VIBRATOVOL;
						nn->param = (param & 0x0F);
						break;

					/* Tremolo Commands */
					case 0x1E: /* Tremolo */
						nn->effect = FX_TREMOLO;
						nn->param = param;
						break;
					case 0x1F: /* Set Tremolo Control */
						nn->effect = FX_SPECIAL;
						nn->param = 0x40 | (param & 0x0F);
						break;

					/* Sample Commands */
					case 0x28: /* Sample Offset */
						/* 3-byte offset, but we can only import the middle one */
						nn->effect = FX_OFFSET;
						nn->param = slurp_getc(fp);
						slurp_seek(fp, 1, SEEK_CUR);
						break;
					case 0x29: /* Retrig Note */
						nn->effect = FX_RETRIG;
						nn->param = (param & 0x0F);
						break;
					case 0x2A: /* Note Cut */
						nn->effect = FX_SPECIAL;
						nn->param = 0xC0;
						break;
					case 0x2B: /* Note Delay */
						nn->effect = FX_SPECIAL;
						nn->param = (0xD0 | (param & 0x0F));
						break;

					/* Pos. Change Commands */
					case 0x32: /* Position Jump */
						nn->effect = FX_POSITIONJUMP;
						nn->param = param;
						break;
					case 0x33: /* Pattern Break */
						nn->effect = FX_PATTERNBREAK;
						nn->param = param;
						break;
					case 0x34: /* Jump Loop */
						nn->effect = FX_SPECIAL;
						nn->param = (0xB0 | (param & 0x0F));
						break;
					case 0x35: /* Pattern Delay */
						nn->effect = FX_SPECIAL;
						nn->param = (0xE0 | (param & 0x0F));
						break;

					/* Speed Change Cmds */
					case 0x3C: /* Set Regular Speed */
						nn->effect = FX_SPEED;
						nn->param = param;
						break;
					case 0x3D: /* Set BPM (Tempo) */
						nn->effect = FX_TEMPO;
						nn->param = param;
						break;

					/* Misc. Commands */
					case 0x46: /* Arpeggio */
						nn->effect = FX_ARPEGGIO;
						nn->param = param;
						break;
					case 0x47: /* Set Finetune */
						nn->effect = FX_SPECIAL;
						nn->param = (0x20 | (param & 0x0F));
						break;
					case 0x48: /* Set Balance */
						/* openmpt handles this as panning, so that's what
						 * I'm doing here as well. */
						nn->effect = FX_SPECIAL;
						nn->param = (0x80 | (param & 0x0F));
						break;

					default:
						nn->effect = FX_NONE;
						nn->param = param;
						break;
					}
				}
			}

			/* readjust file pointer, as the patterns are 16 bytes aligned */
			slurp_seek(fp, start + ((size + 15) & ~15), SEEK_SET);
		}
	}

#if 0 /* 100% untested, dunno if it even works :P */
	if (psm16_check_parapointer(fp, omessage, "TEXT")) {
		uint16_t size;

		if (slurp_read(fp, &size, sizeof(size)) == sizeof(size))
			slurp_read(fp, song->message, MIN(size, MAX_MESSAGE - 1));
	}
#endif

	song->pan_separation = 128;
	/* FIXME: should compat Gxx be here? */
	song->flags = SONG_ITOLDEFFECTS | SONG_COMPATGXX;

	snprintf(song->tracker_id, sizeof(song->tracker_id), "%s",
		"Epic MegaGames MASI (Old Version)");

	return LOAD_SUCCESS;
}
