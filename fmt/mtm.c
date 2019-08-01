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

#define NEED_BYTESWAP
#include "headers.h"
#include "fmt.h"
#include "song.h"
#include "tables.h"
#include "log.h"

#include <stdint.h>

/* --------------------------------------------------------------------- */

#pragma pack(push, 1)
typedef struct mtm_header {
	char filever[4]; /* M T M \x10 */
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
} mtm_header_t;

typedef struct mtm_sample {
	char name[22];
	uint32_t length, loop_start, loop_end;
	uint8_t finetune, volume, flags;
} mtm_sample_t;
#pragma pack(pop)

/* --------------------------------------------------------------------- */

int fmt_mtm_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
	if (!(length > 24 && memcmp(data, "MTM", 3) == 0))
		return 0;

	file->description = "MultiTracker Module";
	/*file->extension = str_dup("mtm");*/
	file->title = strn_dup((const char *)data + 4, 20);
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
		if (note->effect == 0xa && (note->param & 0xf0))
			note->param &= 0xf0;
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
		song->samples[n].c5speed = MOD_FINETUNE(slurp_getc(fp));
		sample->volume = slurp_getc(fp);
		sample->volume *= 4; //mphack
		sample->global_volume = 64;
		if (slurp_getc(fp) & 1) {
			todo |= 16;
			sample->flags |= CHN_16BIT;
			sample->length >>= 1;
			sample->loop_start >>= 1;
			sample->loop_end >>= 1;
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
		tracknote = trackdata[n];
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
			uint32_t ssize;

			if (song->samples[smp].length == 0)
				continue;
			ssize = csf_read_sample(song->samples + smp,
				(SF_LE | SF_PCMU | SF_M
				 | ((song->samples[smp].flags & CHN_16BIT) ? SF_16 : SF_8)),
				fp->data + fp->pos, fp->length - fp->pos);
			slurp_seek(fp, ssize, SEEK_CUR);
		}
	}

	/* set the rest of the stuff */
	song->flags = SONG_ITOLDEFFECTS | SONG_COMPATGXX;

//      if (ferror(fp)) {
//              return LOAD_FILE_ERROR;
//      }

	if (todo & 64)
		log_appendf(2, " TODO: test this file with other players (beats per track != 64)");
	if (todo & 16)
		log_appendf(2, " TODO: double check 16 bit sample loading");

	/* done! */
	return LOAD_SUCCESS;
}

