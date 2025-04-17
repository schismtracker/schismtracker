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
#include "mem.h"

#include "it.h"
#include "song.h"
#include "player/sndfile.h"

/* --------------------------------------------------------------------- */

/* We simply treat an SF2 as a packed format containing a crap
 * ton of samples, rather than a crap ton of *separate instruments*.
 * This makes it a little bit easier, since we don't have to do
 * anything past making an instrument loader (and letting the
 * sample libraries do the rest of the work) */

enum {
	/* these three are the only ones we care about outside of sf2_read */
	SF2_CHUNK_INAM,
	SF2_CHUNK_shdr, /* in pdta */
	SF2_CHUNK_smpl, /* contains raw sample data */

	SF2_CHUNK_SIZE_,
};

static int sf2_read(slurp_t *fp, iff_chunk_t cs[SF2_CHUNK_SIZE_])
{
	iff_chunk_t cINFO = {0}, cpdta = {0}, csdta = {0};
	iff_chunk_t c;

	{
		unsigned char id[4];
		if (slurp_read(fp, id, sizeof(id)) != sizeof(id))
			return 0;

		if (memcmp(id, "RIFF", 4))
			return 0;

		slurp_seek(fp, 4, SEEK_CUR);

		if (slurp_read(fp, id, sizeof(id)) != sizeof(id))
			return 0;

		if (memcmp(id, "sfbk", 4))
			return 0;
	}

	while (riff_chunk_peek(&c, fp)) {
		uint32_t id;

		/* we only support LIST chunks
		 * actually, why the hell did they put
		 * everything in LIST chunks???
		 * it makes everything so damn complicated */
		if (c.id != UINT32_C(0x4C495354))
			continue;

		if (iff_chunk_read(&c, fp, &id, 4) != 4)
			return 0; /* what ? */

		switch (bswapBE32(id)) {
		case UINT32_C(0x494E464F): /* INFO */
			cINFO = c;
			break;
		case UINT32_C(0x73647461): /* sdta */
			csdta = c;
			break;
		case UINT32_C(0x70647461): /* pdta */
			cpdta = c;
			break;
		default:
			break;
		}
	}

	if (!cINFO.id || !csdta.id || !cpdta.id)
		return 0; /* definitely not sf2 */

	slurp_seek(fp, cINFO.offset + 4, SEEK_SET);

	while (riff_chunk_peek(&c, fp) && slurp_tell(fp) <= cINFO.offset + cINFO.size) {
		switch (c.id) {
		case UINT32_C(0x494E414D): /* INAM */
			cs[SF2_CHUNK_INAM] = c;
			break;
		case UINT32_C(0x6966696C): { /* ifil */
			uint32_t ver;

			if (c.size != 4)
				return 0;

			if (iff_chunk_read(&c, fp, &ver, sizeof(ver)) != sizeof(ver))
				return 0;

			ver = bswapLE32(ver);

			/* low word = major,
			 * high word = minor */

			switch (ver & UINT32_C(0xFFFF)) {
			case 2: break;
			case 1:
				/* TODO: maybe handle version 1? as well?
				 * I don't have any on hand to figure out */
				SCHISM_FALLTHROUGH;
			default:
				return 0;
			}
			break;
		}
		default:
			/* don't care */
			break;
		}
	}

	slurp_seek(fp, cpdta.offset + 4, SEEK_SET);

	while (riff_chunk_peek(&c, fp) && slurp_tell(fp) <= cpdta.offset + cpdta.size) {
		switch (c.id) {
		case UINT32_C(0x73686472): /* shdr */
			cs[SF2_CHUNK_shdr] = c;
			break;
		default:
			/* don't care */
			break;
		}
	}

	slurp_seek(fp, csdta.offset + 4, SEEK_SET);

	while (riff_chunk_peek(&c, fp) && slurp_tell(fp) <= csdta.offset + csdta.size) {
		switch (c.id) {
		case UINT32_C(0x736D706C): /* smpl */
			cs[SF2_CHUNK_smpl] = c;
			break;
		/* NOTE: there is also a `sm24` chunk that contains
		 * raw 8-bit sample data, that can be added onto the
		 * 16-bit sample data to create 24-bit data.
		 * We don't even support 24-bit though, so I don't
		 * care. :) */
		default:
			printf("%08x %.4s\n", c.id, (char *)&c.id);
			/* don't care */
			break;
		}
	}

	if (!cs[SF2_CHUNK_smpl].id)
		return 0;

	return 1;
}

int fmt_sf2_read_info(dmoz_file_t *file, slurp_t *fp)
{
	iff_chunk_t cs[SF2_CHUNK_SIZE_] = {0};

	if (!sf2_read(fp, cs))
		return 0;

	file->description = "SoundFont2";
	file->type = TYPE_INST_OTHER;

	if (cs[SF2_CHUNK_INAM].id) {
		file->title = mem_alloc(cs[SF2_CHUNK_INAM].size + 1);
		iff_chunk_read(&cs[SF2_CHUNK_INAM], fp, file->title, cs[SF2_CHUNK_INAM].size);
		file->title[cs[SF2_CHUNK_INAM].size] = '\0';
	}

	return 1;
}

int fmt_sf2_load_instrument(slurp_t *fp, int slot)
{
	iff_chunk_t cs[SF2_CHUNK_SIZE_] = {0};
	struct instrumentloader ii;
	song_instrument_t *g;
	uint32_t i, nsmp;

	if (!slot)
		return 0;

	if (!sf2_read(fp, cs))
		return 0;

	/* this size should always be a multiple of 46 */
	if (cs[SF2_CHUNK_shdr].size % 46)
		return 0;

	nsmp = cs[SF2_CHUNK_shdr].size / 46;

	nsmp = MIN(nsmp, MAX_SAMPLES);

	g = instrument_loader_init(&ii, slot);
	iff_chunk_read(&cs[SF2_CHUNK_INAM], fp, g->name, sizeof(g->name));
	g->name[15] = '\0';

	for (i = 0; i < 120; i++) {
		g->sample_map[i] = 0;
		g->note_map[i] = i + 1;
	}

	slurp_seek(fp, cs[SF2_CHUNK_shdr].offset, SEEK_SET);

	for (i = 0; i < nsmp; i++) {
		char name[20];
		song_sample_t *smp;
		int n;
		uint32_t smpl_offset, smpl_end, loop_start, loop_end, rate;
		uint16_t link, type;

		slurp_read(fp, name, 20);

		slurp_read(fp, &smpl_offset, 4);
		slurp_read(fp, &smpl_end, 4);
		slurp_read(fp, &loop_start, 4);
		slurp_read(fp, &loop_end, 4);
		slurp_read(fp, &rate, 4);
		slurp_seek(fp, 1, SEEK_CUR); /* uint8_t -- original pitch midi note (should not be ignoring this...) */
		slurp_seek(fp, 1, SEEK_CUR); /* int8_t -- finetune (cents) */
		slurp_read(fp, &link, 2);
		slurp_read(fp, &type, 2);

		smpl_offset = bswapLE32(smpl_offset);
		smpl_end    = bswapLE32(smpl_end);
		loop_start  = bswapLE32(loop_start);
		loop_end    = bswapLE32(loop_end);
		rate        = bswapLE32(rate);
		link        = bswapLE16(link); /* this will be useful for stereo support.. */
		type        = bswapLE16(type);

		switch (type) {
		case 1: /* mono */
		case 4: /* left stereo (treated as mono) */
			break;
		case 2: /* right stereo */
		case 8: /* linked sample (will never support this) */
			printf("invalid type\n");
			continue; /* unsupported */
		}

		/* invalid ?? */
		if (smpl_offset + smpl_end > (cs[SF2_CHUNK_smpl].size / 2)) {
			printf("%u, %u > %u; invalid size\n", smpl_offset, smpl_end, cs[SF2_CHUNK_smpl].size);
			continue;
		}

		/* NOW, allocate a sample number. */
		n = instrument_loader_sample(&ii, i + 1);
		if (!n)
			break;

		smp = song_get_sample(n);
		if (!smp)
			break;

		memcpy(smp->name, name, MIN(sizeof(smp->name), sizeof(name)));

		smp->length = smpl_end - smpl_offset;
		if (loop_start | loop_end) {
			smp->flags |= CHN_LOOP;
			smp->loop_start = loop_start;
			smp->loop_end = loop_end;
		}
		smp->c5speed = rate;

		iff_read_sample(&cs[SF2_CHUNK_smpl], fp, smp,
			SF_16 | SF_M | SF_LE | SF_PCMS, smpl_offset << 1);
	}

	return 1;
}
