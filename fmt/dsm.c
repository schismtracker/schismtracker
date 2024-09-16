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
#include "charset.h"
#include "slurp.h"
#include "fmt.h"
#include "log.h"

#include "player/sndfile.h"

#include <inttypes.h> /* need uint*_t format specifiers */

/* --------------------------------------------------------------------- */

#pragma pack(push, 1)

struct dsm_chunk_patt {
	uint16_t length;
	uint8_t data[];
};

SCHISM_BINARY_STRUCT(struct dsm_chunk_patt, 2);

struct dsm_chunk_song {
	char title[28];
	uint16_t version, flags;
	uint32_t pad;
	uint16_t ordnum, smpnum, patnum, chnnum;
	uint8_t gvol, mvol, is, it;
	uint8_t chnpan[16];
	uint8_t orders[128];
};

SCHISM_BINARY_STRUCT(struct dsm_chunk_song, 192);

struct dsm_chunk_inst {
	char filename[13];
	uint16_t flags;
	uint8_t volume;
	uint32_t length, loop_start, loop_end, address_ptr;
	uint16_t c5speed, period;
	char name[28];
};

SCHISM_BINARY_STRUCT(struct dsm_chunk_inst, 64);

#pragma pack(pop)

struct dsm_process_pattern_data {
	song_note_t *pattern;
	uint16_t nchn;
	uint8_t *chn_doesnt_match;
};

/* sample flags */
enum {
	DSM_SMP_LOOP_ACTIVE = 0x01,
	DSM_SMP_SIGNED_PCM = 0x02,
	DSM_SMP_PACKED_PCM = 0x04,
	DSM_SMP_DELTA_PCM = 0x40,
};

/* pattern byte flags/masks */
enum {
	DSM_PAT_NOTE_PRESENT = 0x80,
	DSM_PAT_INST_PRESENT = 0x40,
	DSM_PAT_VOL_PRESENT = 0x20,
	DSM_PAT_CMD_PRESENT = 0x10,
	DSM_PAT_CHN_NUM_MASK = 0x0F,
};

#define ID_SONG UINT32_C(0x534F4E47)
#define ID_INST UINT32_C(0x494E5354)
#define ID_PATT UINT32_C(0x50415454)

int fmt_dsm_read_info(dmoz_file_t *file, slurp_t *fp)
{
	unsigned char riff[4], dsmf[4], title[20];

	if (!(slurp_length(fp) > 40))
		return 0;

	if (slurp_read(fp, riff, sizeof(riff)) != sizeof(dsmf)
		|| memcmp(riff, "RIFF", 4))
		return 0;

	slurp_seek(fp, 4, SEEK_CUR);
	if (slurp_read(fp, dsmf, sizeof(dsmf)) != sizeof(dsmf)
		|| memcmp(dsmf, "DSMF", 4))
		return 0;

	iff_chunk_t chunk;
	while (riff_chunk_peek(&chunk, fp)) {
		if (chunk.id == ID_SONG) {
			/* we only need the title, really */
			unsigned char title[28];

			iff_chunk_read(&chunk, fp, title, sizeof(title));

			file->title = strn_dup(title, sizeof(title));
			break;
		}
	}

	file->description = "DSIK Module";
	/*file->extension = str_dup("dsm");*/
	file->type = TYPE_MODULE_MOD;
	return 1;
}

static int dsm_process_pattern(const void *data, size_t size, void *userdata)
{
	const struct dsm_chunk_patt *patt = (const struct dsm_chunk_patt *)data;
	const struct dsm_process_pattern_data *ppd = userdata;
	song_note_t *pattern = ppd->pattern;

	if (size < sizeof(*patt))
		return 0;

	size_t offset = 0;
	size_t length = size - sizeof(*patt);
	length = MIN(length, patt->length);

	/* make sure our offset doesn't pass the length */
#define DSM_ASSERT_OFFSET(o, l) \
	if ((o) >= (l)) { \
		log_appendf(4, " WARNING: Offset (%zu) passed length (%zu) while parsing pattern!", (size_t)(o), (size_t)(l)); \
		return 0; \
	}

	int row = 0;
	while (row < 64) {
		DSM_ASSERT_OFFSET(offset, length)

		uint8_t mask = patt->data[offset++];

		if (!mask) {
			/* done with the row */
			row++;
			continue;
		}

		uint8_t chn = (mask & DSM_PAT_CHN_NUM_MASK);

		if (chn > MAX_CHANNELS) /* whoops */
			return 0;

		if (chn > ppd->nchn) /* header doesn't match? warn. */
			*ppd->chn_doesnt_match = MAX(chn, *ppd->chn_doesnt_match);

		song_note_t *note = ppd->pattern + 64 * row + chn;
		if (mask & DSM_PAT_NOTE_PRESENT) {
			DSM_ASSERT_OFFSET(offset, length)

			uint8_t c = patt->data[offset++];

			if (c <= 168)
				note->note = c + 12;
		}

		if (mask & DSM_PAT_INST_PRESENT) {
			DSM_ASSERT_OFFSET(offset, length)

			note->instrument = patt->data[offset++];
		}

		if (mask & DSM_PAT_VOL_PRESENT) {
			/* volume */
			DSM_ASSERT_OFFSET(offset, length)

			uint8_t param = patt->data[offset++];

			if (param != 0xFF) {
				note->voleffect = VOLFX_VOLUME;
				note->volparam = MIN(param, 64);
			}
		}

		if (mask & DSM_PAT_CMD_PRESENT) {
			DSM_ASSERT_OFFSET(offset, length)

			note->effect = patt->data[offset++];

			DSM_ASSERT_OFFSET(offset, length)

			note->param = patt->data[offset++];

			csf_import_mod_effect(note, 0);

			if (note->effect == FX_PANNING) {
				if (note->param <= 0x80) {
					note->param <<= 1;
				} else if (note->param == 0xA4) {
				    note->effect = FX_SPECIAL;
				    note->param = 0x91;
				}
			}
		}
	}
#undef DSM_ASSERT_OFFSET

	return 1;
}

int fmt_dsm_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	uint8_t riff[4], dsmf[4], chnpan[16];
	size_t pos = 0;
	size_t s = 0, p = 0, n = 0;
	uint16_t nord = 0, nsmp = 0, npat = 0, nchn = 0;
	uint8_t chn_doesnt_match = 0;
	size_t num_song_headers = 0;

	slurp_read(fp, &riff, 4);
	slurp_seek(fp, 4, SEEK_CUR);
	slurp_read(fp, &dsmf, 4);

	if (memcmp(riff, "RIFF", 4) || memcmp(dsmf, "DSMF", 4))
		return LOAD_UNSUPPORTED;

	iff_chunk_t chunk;
	while (riff_chunk_peek(&chunk, fp)) {
		switch(chunk.id) {
		case ID_SONG: {
			struct dsm_chunk_song chunk_song;

			iff_chunk_read(&chunk, fp, &chunk_song, sizeof(chunk_song));

			nord = bswapLE16(chunk_song.ordnum);
			nsmp = bswapLE16(chunk_song.smpnum);
			npat = bswapLE16(chunk_song.patnum);
			nchn = bswapLE16(chunk_song.chnnum);

			if (nord > MAX_ORDERS || nsmp > MAX_SAMPLES || npat > MAX_PATTERNS || nchn > MAX_CHANNELS)
				return LOAD_UNSUPPORTED;

			song->initial_global_volume = chunk_song.gvol << 1;
			song->mixing_volume = chunk_song.mvol >> 1;
			song->initial_speed = chunk_song.is;
			song->initial_tempo = chunk_song.it;

			strncpy(song->title, chunk_song.title, 25);
			song->title[25] = '\0';

			memcpy(&chnpan, chunk_song.chnpan, 16);

			memcpy(song->orderlist, chunk_song.orders, nord);
			num_song_headers++;

			break;
		}
		case ID_INST: {
			/* sanity check. it doesn't matter if nsmp isn't the real sample
			 * count; the file isn't "tainted" because of it, so just print
			 * a warning if the amount of samples loaded wasn't what was expected */
			if (s > MAX_SAMPLES)
				return LOAD_UNSUPPORTED; /* punt */

			if (lflags & LOAD_NOSAMPLES) {
				s++;
				continue;
			}

			struct dsm_chunk_inst inst;

			iff_chunk_read(&chunk, fp, &inst, sizeof(inst));

			/* samples internally start at index 1 */
			song_sample_t *sample = song->samples + s + 1;
			uint32_t flags = SF_LE | SF_8 | SF_M;

			if (inst.flags & DSM_SMP_LOOP_ACTIVE)
				sample->flags |= CHN_LOOP;

			/* these are mutually exclusive (?) */
			if (inst.flags & DSM_SMP_SIGNED_PCM)
				flags |= SF_PCMS;
			else if (inst.flags & DSM_SMP_DELTA_PCM)
				flags |= SF_PCMD;
			else
				flags |= SF_PCMU;

			memcpy(sample->name, inst.name, 25);
			sample->name[25] = '\0';

			memcpy(sample->filename, inst.filename, 12);
			sample->filename[12] = '\0';

			sample->length = bswapLE32(inst.length);
			sample->loop_start = bswapLE32(inst.loop_start);
			sample->loop_end = bswapLE32(inst.loop_end);
			sample->c5speed = bswapLE16(inst.c5speed);
			sample->volume = inst.volume * 4; // modplug

			iff_read_sample(&chunk, fp, sample, flags, sizeof(inst));
			s++;

			break;
		}
		case ID_PATT: {
			if (p > MAX_PATTERNS)
				return LOAD_UNSUPPORTED; /* punt */

			if (lflags & LOAD_NOPATTERNS) {
				p++;
				continue;
			}

			int64_t pos = slurp_tell(fp);

			song->patterns[p] = csf_allocate_pattern(64);

			struct dsm_process_pattern_data data = {
				.pattern = song->patterns[p],
				.chn_doesnt_match = &chn_doesnt_match,
				.nchn = nchn,
			};

			slurp_seek(fp, chunk.offset, SEEK_SET);
			slurp_receive(fp, dsm_process_pattern, chunk.size, &data);
			slurp_seek(fp, pos, SEEK_SET);

			p++;

			break;
		}
		default:
			break;
		}
	}

	/* With our loader, it's possible that the song header wasn't even found to begin with.
	 * This causes the order list and title to be empty and the global volume to remain as it was.
	 * Make sure to notify the user if this ever actually happens. */
	if (!num_song_headers)
		log_appendf(4, " WARNING: No SONG chunk found! (invalid DSM file?\?)");
	else if (num_song_headers > 1)
		log_appendf(4, " WARNING: Multiple (%zu) SONG chunks found!", num_song_headers);

	if (s != nsmp)
		log_appendf(4, " WARNING: # of samples (%zu) different than expected (%" PRIu16 ")", s, nsmp);

	if (p != npat)
		log_appendf(4, " WARNING: # of patterns (%zu) different than expected (%" PRIu16 ")", p, npat);

	if (chn_doesnt_match && chn_doesnt_match != nchn)
		log_appendf(4, " WARNING: # of channels (%"PRIu8") different than expected (%" PRIu16 ")", chn_doesnt_match, nchn);

	for (n = 0; n < nchn; n++) {
		if (chnpan[n & 15] <= 0x80)
			song->channels[n].panning = chnpan[n & 15] << 1;
	}

	for (; n < MAX_CHANNELS; n++)
		song->channels[n].flags |= CHN_MUTE;

	song->pan_separation = 128;
	song->flags = SONG_ITOLDEFFECTS | SONG_COMPATGXX;

	sprintf(song->tracker_id, "Digital Sound Interface Kit mod");

	return LOAD_SUCCESS;
}
