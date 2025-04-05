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

// 'chunk' is filled in with the chunk header
// return: 0 if chunk overflows EOF, 1 if it was successfully read
// pos is updated to point to the beginning of the next chunk
int iff_chunk_peek_ex(iff_chunk_t *chunk, slurp_t *fp, uint32_t flags)
{
	if (slurp_read(fp, &chunk->id, sizeof(chunk->id)) != sizeof(chunk->id))
		return 0;

	if (slurp_read(fp, &chunk->size, sizeof(chunk->size)) != sizeof(chunk->size))
		return 0;

	chunk->id = bswapBE32(chunk->id);
	chunk->size = (flags & IFF_CHUNK_SIZE_LE) ? bswapLE32(chunk->size) : bswapBE32(chunk->size);

	chunk->offset = slurp_tell(fp);
	if (chunk->offset < 0)
		return 0;

	// align the offset on a word boundary if desired
	slurp_seek(fp, (flags & IFF_CHUNK_ALIGNED) ? (chunk->size + (chunk->size & 1)) : (chunk->size), SEEK_CUR);

	int64_t pos = slurp_tell(fp);
	if (pos < 0)
		return 0;

	return ((size_t)pos <= slurp_length(fp));
}

/* returns the amount of bytes read or zero on error */
int iff_chunk_read(iff_chunk_t *chunk, slurp_t *fp, void *data, size_t size)
{
	int64_t pos = slurp_tell(fp);
	if (pos < 0)
		return 0;

	if (slurp_seek(fp, chunk->offset, SEEK_SET))
		return 0;

	size = slurp_read(fp, data, MIN(chunk->size, size));

	/* how ? */
	if (slurp_seek(fp, pos, SEEK_SET))
		return 0;

	return size;
}

int iff_chunk_receive(iff_chunk_t *chunk, slurp_t *fp, int (*callback)(const void *, size_t, void *), void *userdata)
{
	int64_t pos = slurp_tell(fp);
	if (pos < 0)
		return 0;

	if (slurp_seek(fp, chunk->offset, SEEK_SET))
		return 0;

	int res = slurp_receive(fp, callback, chunk->size, userdata);

	/* how ? */
	if (slurp_seek(fp, pos, SEEK_SET))
		return 0;

	return res;
}

/* offset is the offset the sample is actually located in the chunk;
 * this can be different depending on the file format... */
int iff_read_sample(iff_chunk_t *chunk, slurp_t *fp, song_sample_t *smp, uint32_t flags, size_t offset)
{
	int64_t pos = slurp_tell(fp);
	if (pos < 0)
		return 0;

	if (slurp_seek(fp, chunk->offset + offset, SEEK_SET))
		return 0;

	int r = csf_read_sample(smp, flags, fp);

	slurp_seek(fp, pos, SEEK_SET);

	return r;
}

/* ------------------------------------------------------------------------ */

int iff_read_xtra_chunk(slurp_t *fp, song_sample_t *smp)
{
	uint32_t xtra_flags;
	uint16_t vol;

	if (slurp_read(fp, &xtra_flags, 4) != 4)
		return 0;

	xtra_flags = bswapLE32(xtra_flags);

	if (xtra_flags & 0x20) {
		uint16_t pan;

		if (slurp_read(fp, &pan, 2) != 2)
			return 0;

		pan = bswapLE16(pan);

		smp->panning = MIN(pan, 255);
		smp->flags |= CHN_PANNING;
	}

	if (slurp_read(fp, &vol, 2) != 2)
		return 0;

	vol = bswapLE16(vol);
	vol = ((vol + 2) / 4) * 4; // round to nearest multiple of 64

	smp->volume = MIN(vol, 256);

	return 1;
}

int iff_read_smpl_chunk(slurp_t *fp, song_sample_t *smp)
{
	uint32_t num_loops;

	slurp_seek(fp, 28, SEEK_CUR);

	if (slurp_read(fp, &num_loops, 4) != 4)
		return 0;

	num_loops = bswapLE32(num_loops);

	if (num_loops == 1) {
		uint32_t type, start, end;

		/* skip "samplerData" and "identifier" */
		slurp_seek(fp, 8, SEEK_CUR);

		if (slurp_read(fp, &type, 4) != 4
			|| slurp_read(fp, &start, 4) != 4
			|| slurp_read(fp, &end, 4) != 4)
			return 0;
		type = bswapLE32(type);
		start = bswapLE32(start);
		end = bswapLE32(end);

		switch (type) {
		case 1:
			smp->flags |= CHN_PINGPONGLOOP;
			SCHISM_FALLTHROUGH;
		case 0:
			smp->flags |= CHN_LOOP;
			break;
		case 2:
			/* unsupported */
			SCHISM_FALLTHROUGH;
		default:
			return 0;
		}

		smp->loop_start = start;
		smp->loop_end = end + 1; /* old code did this... */
	}

	return 1;
}

/* not sure if this really "fits" here; whatever, it's IFF-ish :) */
void iff_fill_xtra_chunk(song_sample_t *smp, unsigned char xtra_data[IFF_XTRA_CHUNK_SIZE], uint32_t *length)
{
	uint32_t dw;
	uint16_t w;
	uint32_t offset;
	int save_panning = !!(smp->flags & CHN_PANNING);

	/* Identifier :) */
	memcpy(xtra_data, "xtra", 4);

	/* FLAGS */
	dw = 0;
	if (save_panning)
		dw |= 0x20;
	dw = bswapLE32(dw);
	memcpy(xtra_data + 8, &dw, 4);

	/* from now on, some parts are optional. */
	offset = 12;

	/* PAN */
	if (save_panning) {
		w = bswapLE16(smp->panning);
		memcpy(xtra_data + offset, &w, 2);
		offset += 2;
	}

	/* VOLUME */
	w = bswapLE16(smp->volume);
	memcpy(xtra_data + offset, &w, 2);
	offset += 2;

	*length = offset;

	/* now, go back and fill in the chunk length. */
	dw = bswapLE32(offset - 8u);
	memcpy(xtra_data + 4, &dw, 4);
}

void iff_fill_smpl_chunk(song_sample_t *smp, unsigned char smpl_data[IFF_SMPL_CHUNK_SIZE], uint32_t *length)
{
	uint32_t offset = 0;
	uint32_t dw;
	uint32_t loops = !!(smp->flags & CHN_LOOP);

	memcpy(smpl_data + offset, "smpl", 4);
	offset += 4;

	/* size is filled in later */
	offset += 4;

	/* manufacturer (zero == "no specific manufacturer") */
	memset(smpl_data + offset, 0, 4);
	offset += 4;

	/* product (zero == "no specific manufacturer") */
	memset(smpl_data + offset, 0, 4);
	offset += 4;

	/* period of one sample in nanoseconds */
	dw = bswapLE32(1000000000ULL / smp->c5speed);
	memcpy(smpl_data + offset, &dw, 4);
	offset += 4;

	/* MIDI unity note */
	dw = bswapLE32(72); /* C5 */
	memcpy(smpl_data + offset, &dw, 4);
	offset += 4;

	/* MIDI pitch fraction (???) -- I don't think this is relevant at all to us */
	memset(smpl_data + offset, 0, 4);
	offset += 4;

	/* SMPTE format (?????????????) */
	memset(smpl_data + offset, 0, 4);
	offset += 4;

	/* SMPTE offset */
	memset(smpl_data + offset, 0, 4);
	offset += 4;

	/* finally... the sample loops */
	dw = bswapLE32(loops);
	memcpy(smpl_data + offset, &dw, 4);
	offset += 4;

	/* sampler-specific data (we have none) */
	memset(smpl_data + offset, 0, 4);
	offset += 4;

	if (loops) {
		/* The FOOLY COOLY Constant */
		memcpy(smpl_data + offset, "FLCL", 4);
		offset += 4;

		dw = bswapLE32((smp->flags & CHN_PINGPONGLOOP) ? 1 : 0);
		memcpy(smpl_data + offset, &dw, 4);
		offset += 4;

		dw = bswapLE32(smp->loop_start);
		memcpy(smpl_data + offset, &dw, 4);
		offset += 4;

		/* TODO: Is this right? */
		dw = bswapLE32(smp->loop_end - 1);
		memcpy(smpl_data + offset, &dw, 4);
		offset += 4;

		/* no finetune ? */
		memset(smpl_data + offset, 0, 4);
		offset += 4;

		/* loop infinitely */
		memset(smpl_data + offset, 0, 4);
		offset += 4;
	}

	*length = offset;

	/* fill in the chunk length */
	dw = bswapLE32(offset - 8u);
	memcpy(smpl_data + 4, &dw, 4);
}
