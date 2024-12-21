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

#include "player/sndfile.h"

#include "disko.h"
#include "log.h"

/* --------------------------------------------------------------------- */

int fmt_stx_read_info(dmoz_file_t *file, slurp_t *fp)
{
	unsigned char magic[4];
	int i;

	slurp_seek(fp, 60, SEEK_SET);
	if (slurp_read(fp, magic, sizeof(magic)) != sizeof(magic)
		|| memcmp(magic, "SCRM", sizeof(magic)))
		return 0;

	slurp_seek(fp, 20, SEEK_SET);
	for (i = 0; i < 8; i++) {
		int id = slurp_getc(fp);
		if (id < 0x20 || id > 0x7E)
			return 0;
	}

	unsigned char title[20];

	slurp_rewind(fp);
	if (slurp_read(fp, title, sizeof(title)) != sizeof(title))
		return 0;

	file->description = "ST Music Interface Kit";
	/*file->extension = str_dup("stx");*/
	file->title = strn_dup((const char *)title, sizeof(title));
	file->type = TYPE_MODULE_MOD;
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

enum {
	S3I_TYPE_NONE = 0,
	S3I_TYPE_PCM = 1,
};

int fmt_stx_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	uint16_t nsmp, nord, npat;
	int n;
	song_note_t *note;
	/* junk variables for reading stuff into */
	uint16_t tmp;
	uint8_t c;
	uint32_t tmplong;
	uint8_t b[4], b2[8];
	/* parapointers */
	uint16_t para_patptr;
	uint16_t para_smptr;
	uint16_t para_chnptr;   // supposedly settings...
	uint16_t para_smp[MAX_SAMPLES];
	uint16_t para_pat[MAX_PATTERNS];
	uint32_t para_sdata[MAX_SAMPLES] = { 0 };
	song_sample_t *sample;
	int subversion = 1;
	uint16_t first_pattern_size;
	uint16_t pattern_size;

	/* check the tag */
	slurp_seek(fp, 20, SEEK_SET);
	slurp_read(fp, b2, 8);
	slurp_seek(fp, 60, SEEK_SET);
	slurp_read(fp, b, 4);
	for (n = 0; n < 8; n++)
		if (b2[n] < 0x20 || b2[n] > 0x7E)
			return LOAD_UNSUPPORTED;
	if (memcmp(b, "SCRM", 4) != 0)
		return LOAD_UNSUPPORTED;

	/* read the title */
	slurp_rewind(fp);
	slurp_read(fp, song->title, 20);
	song->title[20] = 0;

	slurp_seek(fp, 8, SEEK_CUR);
	slurp_read(fp, &tmp, 2);
	first_pattern_size = bswapLE16(tmp);
	slurp_seek(fp, 2, SEEK_CUR);

	slurp_read(fp, &tmp, 2);
	para_patptr = bswapLE16(tmp);

	slurp_read(fp, &tmp, 2);
	para_smptr = bswapLE16(tmp);

	slurp_read(fp, &tmp, 2);
	para_chnptr = bswapLE16(tmp);

	slurp_seek(fp, 4, SEEK_CUR);
	song->initial_global_volume = slurp_getc(fp) << 1;
	int tempo = slurp_getc(fp);
	song->initial_speed = tempo >> 4 ?: 6;
	song->initial_tempo = convert_stm_tempo_to_bpm(tempo);
	slurp_seek(fp, 4, SEEK_CUR);

	slurp_read(fp, &npat, 2);
	slurp_read(fp, &nsmp, 2);
	slurp_read(fp, &nord, 2);
	nord = bswapLE16(nord);
	nsmp = bswapLE16(nsmp);

	// STX 1.0 modules sometimes have bugged sample counts...
	if (nsmp > 31)
		nsmp = 31;

	npat = bswapLE16(npat);

	if (nord > MAX_ORDERS || nsmp > MAX_SAMPLES || npat > MAX_PATTERNS)
		return LOAD_FORMAT_ERROR;

	song->flags = SONG_ITOLDEFFECTS | SONG_NOSTEREO;

	slurp_seek(fp, (para_chnptr << 4) + 32, SEEK_SET);
	/* orderlist */
	for (n = 0; n < nord; n++) {
		song->orderlist[n] = slurp_getc(fp);
		slurp_seek(fp, 4, SEEK_CUR);
	}

	/* load the parapointers */
	slurp_seek(fp, para_smptr << 4, SEEK_SET);
	slurp_read(fp, para_smp, 2 * nsmp);
	slurp_seek(fp, para_patptr << 4, SEEK_SET);
	slurp_read(fp, para_pat, 2 * npat);

	/* samples */
	for (n = 0, sample = song->samples + 1; n < nsmp; n++, sample++) {
		uint8_t type;

		slurp_seek(fp, (para_smp[n]) << 4, SEEK_SET);

		type = slurp_getc(fp);
		slurp_read(fp, sample->filename, 12);
		sample->filename[12] = 0;

		for (int i = 0; i < 12; i++) {
			if ((uint8_t)sample->filename[i] == 0xFF)
				sample->filename[i] = 0x20;
		}

		slurp_read(fp, b, 3); // data pointer for pcm, irrelevant otherwise
		switch (type) {
		case S3I_TYPE_PCM:
			para_sdata[n] = b[1] | (b[2] << 8) | (b[0] << 16);
			slurp_read(fp, &tmplong, 4);
			sample->length = bswapLE32(tmplong);
			slurp_read(fp, &tmplong, 4);
			sample->loop_start = bswapLE32(tmplong);
			slurp_read(fp, &tmplong, 4);
			sample->loop_end = bswapLE32(tmplong);
			sample->volume = slurp_getc(fp) * 4; //mphack
			slurp_seek(fp, 2, SEEK_CUR);
			c = slurp_getc(fp);  /* flags */
			if (c & 1)
				sample->flags |= CHN_LOOP;
			break;

		default:
		case S3I_TYPE_NONE:
			slurp_seek(fp, 12, SEEK_CUR);
			sample->volume = slurp_getc(fp) * 4; //mphack
			slurp_seek(fp, 3, SEEK_CUR);
			break;
		}

		slurp_read(fp, &tmplong, 4);
		sample->c5speed = bswapLE32(tmplong);
		slurp_seek(fp, 12, SEEK_CUR);        /* unused space */
		slurp_read(fp, sample->name, 25);
		sample->name[25] = 0;

		for (int i = 0; i < 25; i++) {
			if ((uint8_t)sample->name[i] == 0xFF)
				sample->name[i] = 0x20;
		}

		sample->vib_type = 0;
		sample->vib_rate = 0;
		sample->vib_depth = 0;
		sample->vib_speed = 0;
		sample->global_volume = 64;
	}

	if (first_pattern_size != 0x1A) {
		slurp_seek(fp, (bswapLE16(para_pat[0]) << 4), SEEK_SET);
		// 1.0 files have pattern size before pattern data
		// which should match the header's specified size.
		slurp_read(fp, &tmp, 2);
		pattern_size = bswapLE16(tmp);

		// Amusingly, Purple Motion's "Future Brain" actually
		// specifies pattern size in the song header even though
		// the patterns themselves don't specify their size.
		if (pattern_size == first_pattern_size)
			subversion = 0;
	}

	if (!(lflags & LOAD_NOPATTERNS)) {
		for (n = 0; n < npat; n++) {
			int row = 0;

			para_pat[n] = bswapLE16(para_pat[n]);
			if (!para_pat[n])
				continue;

			slurp_seek(fp, para_pat[n] << 4, SEEK_SET);
			if (!subversion)
				slurp_seek(fp, 2, SEEK_CUR);

			song->patterns[n] = csf_allocate_pattern(64);

			while (row < 64) {
				int mask = slurp_getc(fp);
				uint8_t chn = (mask & 31);

				if (mask == EOF) {
					log_appendf(4, " Warning: Pattern %d: file truncated", n);
					break;
				}
				if (!mask) {
					/* done with the row */
					row++;
					continue;
				}
				note = song->patterns[n] + 64 * row + chn;
				if (mask & 32) {
					/* note/instrument */
					note->note = slurp_getc(fp);
					note->instrument = slurp_getc(fp);
					//if (note->instrument > 99)
					//      note->instrument = 0;
					switch (note->note) {
					default:
						// Note; hi=oct, lo=note
						note->note = ((note->note >> 4) + 2) * 12 + (note->note & 0xf) + 13;
						break;
					case 255:
						note->note = NOTE_NONE;
						break;
					case 254:
						note->note = NOTE_CUT;
						break;
					}
				}
				if (mask & 64) {
					/* volume */
					note->voleffect = VOLFX_VOLUME;
					note->volparam = slurp_getc(fp);
					if (note->volparam == 255) {
						note->voleffect = VOLFX_NONE;
						note->volparam = 0;
					} else if (note->volparam > 64) {
						note->volparam = 64;
					}
				}
				if (mask & 128) {
					note->effect = stm_effects[slurp_getc(fp) & 0xf];
					note->param = slurp_getc(fp);
					handle_stm_effects(note);
				}

				for (chn = 0; chn < 32; chn++) {
					song_note_t* chan_note = note + chn;
					if (chan_note->effect == FX_SPEED) {
						uint32_t tempo = chan_note->param;
						chan_note->param >>= 4;
						handle_stm_tempo_pattern(note, tempo);
					}
				}
				/* ... next note, same row */
			}
		}
	}

	/* sample data */
	if (!(lflags & LOAD_NOSAMPLES)) {
		for (n = 0, sample = song->samples + 1; n < nsmp; n++, sample++) {
			if (sample->length < 3)
				continue;
			slurp_seek(fp, para_sdata[n] << 4, SEEK_SET);
			csf_read_sample(sample, SF_LE | SF_PCMS | SF_8 | SF_M, fp);
		}
	}

	for (n = 0; n < 4; n++)
		song->channels[n].panning = ((n & 1) ? 64 : 0) * 4; //mphack
	for (; n < 64; n++)
		song->channels[n].flags |= CHN_MUTE;
	song->pan_separation = 64;

	sprintf(song->tracker_id, "ST Music Interface Kit (1.%d)", subversion);

//      if (ferror(fp)) {
//              return LOAD_FILE_ERROR;
//      }
	/* done! */
	return LOAD_SUCCESS;
}

