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
#include "charset.h"
#include "slurp.h"
#include "fmt.h"
#include "log.h"

#include "sndfile.h"

#include <inttypes.h> /* need uint*_t format specifiers */

/* --------------------------------------------------------------------- */

#pragma pack(push, 1)

typedef union chunkdata {
	struct {
		char title[28];
		uint16_t version, flags;
		uint32_t pad;
		uint16_t ordnum, smpnum, patnum, chnnum;
		uint8_t gvol, mvol, is, it;
		uint8_t chnmap[16];
		uint8_t orders[128];
	} SONG;

	struct {
		char filename[13];
		uint16_t flags;
		uint8_t volume;
		uint32_t length, loop_start, loop_end, address_ptr;
		uint16_t c5speed, period;
		char name[28];
		uint8_t smp_bytes[];
	} INST;

	struct {
		uint16_t length;
		uint8_t data[];
	} PATT;
} chunkdata_t;

typedef struct chunk {
	uint32_t id;
	uint32_t size;
	const chunkdata_t *data;
} chunk_t;

#pragma pack(pop)

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

#define ID_SONG 0x534F4E47
#define ID_INST 0x494E5354
#define ID_PATT 0x50415454

// 'chunk' is filled in with the chunk header
// return: 0 if chunk overflows EOF, 1 if it was successfully read
// pos is updated to point to the beginning of the next chunk
static int _chunk_read(chunk_t *chunk, const uint8_t *data, size_t length, size_t *pos)
{
	if (*pos + 8 > length)
		return 0;
	memcpy(&chunk->id, data + *pos, 4);
	memcpy(&chunk->size, data + *pos + 4, 4);
	chunk->id = bswapBE32(chunk->id);
	chunk->size = bswapLE32(chunk->size); /* yes, size is stored as little endian */
	chunk->data = (chunkdata_t *) (data + *pos + 8);
	*pos += 8 + chunk->size;
	return (*pos <= length);
}

int fmt_dsm_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
	uint8_t riff[4], dsmf[4];

	if (!(length > 40))
		return 0;

	memcpy(riff, data, 4);
	memcpy(dsmf, data + 8, 4);

	if (memcmp(riff, "RIFF", 4) != 0 && memcmp(dsmf, "DSMF", 4) != 0)
	return LOAD_UNSUPPORTED;

	file->description = "DSIK Module";
	/*file->extension = str_dup("dsm");*/
	file->type = TYPE_MODULE_MOD;
	file->title = strn_dup((const char *)&data[20], 20);
	return 1;
}

int fmt_dsm_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	chunk_t chunk;
	uint8_t riff[4], dsmf[4];
	size_t pos = 0, length = 0;
	size_t s = 0, p = 0, n = 0;
	uint16_t nord = 0, nsmp = 0, npat = 0, nchn = 0;
	uint8_t chn_doesnt_match = 0;
	size_t num_song_headers = 0;

	slurp_read(fp, &riff, 4);
	slurp_seek(fp, 4, SEEK_CUR);
	slurp_read(fp, &dsmf, 4);

	if (memcmp(riff, "RIFF", 4) != 0 && memcmp(dsmf, "DSMF", 4) != 0)
		return LOAD_UNSUPPORTED;

	pos = slurp_tell(fp);
	slurp_seek(fp, 0, SEEK_END);
	length = slurp_tell(fp);
	slurp_rewind(fp);

	while (_chunk_read(&chunk, fp->data, length, &pos)) {
		switch(chunk.id) {
		case ID_SONG:
			nord = bswapLE16(chunk.data->SONG.ordnum);
			nsmp = bswapLE16(chunk.data->SONG.smpnum);
			npat = bswapLE16(chunk.data->SONG.patnum);
			nchn = bswapLE16(chunk.data->SONG.chnnum);

			if (nord > MAX_ORDERS || nsmp > MAX_SAMPLES || npat > MAX_PATTERNS || nchn > MAX_CHANNELS)
				return LOAD_UNSUPPORTED;

			song->initial_global_volume = chunk.data->SONG.gvol << 1;
			song->initial_speed = chunk.data->SONG.is;
			song->initial_tempo = chunk.data->SONG.it;
			strncpy(song->title, chunk.data->SONG.title, 25);
			song->title[25] = '\0';

			memcpy(song->orderlist, chunk.data->SONG.orders, nord);
			num_song_headers++;
			break;
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

			/* samples internally start at index 1 */
			song_sample_t *sample = song->samples + s + 1;
			uint32_t flags = SF_LE | SF_8 | SF_M;

			if (chunk.data->INST.flags & DSM_SMP_LOOP_ACTIVE)
				sample->flags |= CHN_LOOP;

			/* these are mutually exclusive (?) */
			if (chunk.data->INST.flags & DSM_SMP_SIGNED_PCM)
				flags |= SF_PCMS;
			else if (chunk.data->INST.flags & DSM_SMP_DELTA_PCM)
				flags |= SF_PCMD;
			else
				flags |= SF_PCMU;

			memcpy(sample->name, chunk.data->INST.name, 25);
			sample->name[25] = '\0';

			memcpy(sample->filename, chunk.data->INST.filename, 12);
			sample->filename[12] = '\0';

			sample->length = bswapLE32(chunk.data->INST.length);
			sample->loop_start = bswapLE32(chunk.data->INST.loop_start);
			sample->loop_end = bswapLE32(chunk.data->INST.loop_end);
			sample->c5speed = bswapLE16(chunk.data->INST.c5speed);
			sample->volume = chunk.data->INST.volume * 4; // modplug

			csf_read_sample(sample, flags, chunk.data->INST.smp_bytes, chunk.data->INST.length);
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

			uint16_t offset = 0, length = bswapLE16(chunk.data->PATT.length);

			song->patterns[p] = csf_allocate_pattern(64);

			/* make sure our offset doesn't pass the length */
#define DSM_ASSERT_OFFSET(o, l) \
	if ((o) >= (l)) { \
		log_appendf(4, " WARNING: Offset (%"PRIu16") passed length (%"PRIu16") while parsing pattern!", (uint16_t)(o), (uint16_t)(l)); \
		break; \
	}

			int row = 0;
			while (row < 64) {
				uint8_t mask = chunk.data->PATT.data[offset++];

				DSM_ASSERT_OFFSET(offset, chunk.data->PATT.length)

				if (!mask) {
					/* done with the row */
					row++;
					continue;
				}

				uint8_t chn = (mask & DSM_PAT_CHN_NUM_MASK);

				if (chn > MAX_CHANNELS) /* whoops */
					return LOAD_UNSUPPORTED;

				if (chn > nchn) /* header doesn't match? warn. */
					chn_doesnt_match = MAX(chn, chn_doesnt_match);

				song_note_t *note = song->patterns[p] + 64 * row + chn;
				if (mask & DSM_PAT_NOTE_PRESENT) {
					uint8_t c = chunk.data->PATT.data[offset++];

					DSM_ASSERT_OFFSET(offset, chunk.data->PATT.length)

					if (c <= 168)
						note->note = c + 12;
				}

				if (mask & DSM_PAT_INST_PRESENT) {
					note->instrument = chunk.data->PATT.data[offset++];

					DSM_ASSERT_OFFSET(offset, chunk.data->PATT.length)
				}

				if (mask & DSM_PAT_VOL_PRESENT) {
					/* volume */
					uint8_t param = chunk.data->PATT.data[offset++];

					DSM_ASSERT_OFFSET(offset, chunk.data->PATT.length)

					if (param != 0xFF) {
						note->voleffect = VOLFX_VOLUME;
						note->volparam = param;
					}
				}

				if (mask & DSM_PAT_CMD_PRESENT) {
					note->effect = chunk.data->PATT.data[offset++];

					DSM_ASSERT_OFFSET(offset, chunk.data->PATT.length)

					note->param = chunk.data->PATT.data[offset++];

					DSM_ASSERT_OFFSET(offset, chunk.data->PATT.length)

					csf_import_mod_effect(note, 0);
				}
			}

#undef DSM_ASSERT_OFFSET

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
		log_appendf(4, " WARNING: # of samples (%zu) different than expected (%"PRIu16")", s, nsmp);

	if (p != npat)
		log_appendf(4, " WARNING: # of patterns (%zu) different than expected (%"PRIu16")", p, npat);

	if (chn_doesnt_match && chn_doesnt_match != nchn)
		log_appendf(4, " WARNING: # of channels (%"PRIu8") different than expected (%"PRIu16")", chn_doesnt_match, nchn);

	for (n = 0; n < nchn; n++)
		song->channels[n].panning = (n & 0x1) ? 256 : 0;

	for (; n < MAX_CHANNELS; n++)
		song->channels[n].flags |= CHN_MUTE;

	song->pan_separation = 128;
	song->flags = SONG_ITOLDEFFECTS | SONG_COMPATGXX;

	sprintf(song->tracker_id, "Digital Sound Interface Kit module");

	return LOAD_SUCCESS;
}
