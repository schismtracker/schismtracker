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

	/* return ((uint64_t)pos <= slurp_length(fp)); */
	return slurp_available(fp, 1, SEEK_CUR); /* I have NO idea if this is right */
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
	uint16_t vol, pan, gbv;

	if (slurp_read(fp, &xtra_flags, 4) != 4)
		return 0;

	xtra_flags = bswapLE32(xtra_flags);

	if (xtra_flags & 0x20)
		smp->flags |= CHN_PANNING;

	if (slurp_read(fp, &pan, 2) != 2)
		return 0;

	pan = bswapLE16(pan);
	pan = ((pan + 2) / 4) * 4; // round to nearest multiple of 64

	smp->panning = MIN(pan, 256);

	if (slurp_read(fp, &vol, 2) != 2)
		return 0;

	vol = bswapLE16(vol);
	vol = ((vol + 2) / 4) * 4; // round to nearest multiple of 64

	smp->volume = MIN(vol, 256);

	if (slurp_read(fp, &gbv, 2) != 2)
		return 0;

	gbv = bswapLE16(gbv);

	smp->global_volume = MIN(gbv, 64);

	/* reserved chunk (always zero) */
	slurp_seek(fp, 2, SEEK_CUR);

	smp->vib_type = slurp_getc(fp);
	smp->vib_speed = slurp_getc(fp);
	smp->vib_depth = slurp_getc(fp);
	smp->vib_rate = slurp_getc(fp);

	return 1;
}

static inline SCHISM_ALWAYS_INLINE int iff_read_smpl_loop_chunk(slurp_t *fp, uint32_t loopflag, uint32_t bidiflag, uint32_t *loopstart, uint32_t *loopend, uint32_t *flags)
{
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

	/* loop is bogus? */
	if (!(start | end))
		return 0;

	switch (type) {
	case 1:
		*flags |= bidiflag;
		SCHISM_FALLTHROUGH;
	case 0:
		*flags |= loopflag;
		break;
	case 2:
		/* unsupported */
		SCHISM_FALLTHROUGH;
	default:
		return 0;
	}

	*loopstart = start;
	*loopend = end + 1;

	return 1;
}

int iff_read_smpl_chunk(slurp_t *fp, song_sample_t *smp)
{
	uint32_t num_loops;

	slurp_seek(fp, 28, SEEK_CUR);

	if (slurp_read(fp, &num_loops, 4) != 4)
		return 0;

	num_loops = bswapLE32(num_loops);

	/* sustain loop */
	if (num_loops >= 2
		&& !iff_read_smpl_loop_chunk(fp, CHN_SUSTAINLOOP, CHN_PINGPONGSUSTAIN, &smp->sustain_start, &smp->sustain_end, &smp->flags))
		return 0;

	if (num_loops >= 1
		&& !iff_read_smpl_loop_chunk(fp, CHN_LOOP, CHN_PINGPONGLOOP, &smp->loop_start, &smp->loop_end, &smp->flags))
		return 0;

	return 1;
}

/* not sure if this really "fits" here; whatever, it's IFF-ish :) */
void iff_fill_xtra_chunk(song_sample_t *smp, unsigned char xtra_data[IFF_XTRA_CHUNK_SIZE], uint32_t *length)
{
	uint32_t dw;
	uint16_t w;

	/* Identifier :) */
	memcpy(xtra_data, "xtra", 4);

	/* always 16 bytes... */
	dw = bswapLE32(IFF_XTRA_CHUNK_SIZE - 8);
	memcpy(xtra_data + 4, &dw, 4);

	/* flags */
	dw = 0;
	if (smp->flags & CHN_PANNING)
		dw |= 0x20;
	dw = bswapLE32(dw);
	memcpy(xtra_data + 8, &dw, 4);

	/* default pan -- 0..256 */
	w = bswapLE16(smp->panning);
	memcpy(xtra_data + 12, &w, 2);

	/* default volume -- 0..256 */
	w = bswapLE16(smp->volume);
	memcpy(xtra_data + 14, &w, 2);

	/* global volume -- 0..64 */
	w = bswapLE16(smp->global_volume);
	memcpy(xtra_data + 16, &w, 2);

	/* reserved (always zero) */
	memset(xtra_data + 18, 0, 2);

	/* autovibrato type */
	xtra_data[20] = smp->vib_type;

	/* autovibrato sweep (speed) */
	xtra_data[21] = smp->vib_speed;

	/* autovibrato depth */
	xtra_data[22] = smp->vib_depth;

	/* autovibrato rate */
	xtra_data[23] = smp->vib_rate;

	/* after this can be a sample name and filename -- but it doesn't
	 * matter for our use case */

	*length = IFF_XTRA_CHUNK_SIZE;
}

static inline SCHISM_ALWAYS_INLINE void iff_fill_smpl_chunk_loop(unsigned char smpl_data[IFF_SMPL_CHUNK_SIZE], uint32_t *offset,
	uint32_t loop_start, uint32_t loop_end, int bidi)
{
	uint32_t dw;

	/* The FOOLY COOLY Constant */
	memcpy(smpl_data + *offset, "FLCL", 4);
	*offset += 4;

	dw = bswapLE32((bidi) ? 1 : 0);
	memcpy(smpl_data + *offset, &dw, 4);
	*offset += 4;

	dw = bswapLE32(loop_start);
	memcpy(smpl_data + *offset, &dw, 4);
	*offset += 4;

	dw = bswapLE32(loop_end - 1);
	memcpy(smpl_data + *offset, &dw, 4);
	*offset += 4;

	/* no finetune ? */
	memset(smpl_data + *offset, 0, 4);
	*offset += 4;

	/* loop infinitely */
	memset(smpl_data + *offset, 0, 4);
	*offset += 4;
}

void iff_fill_smpl_chunk(song_sample_t *smp, unsigned char smpl_data[IFF_SMPL_CHUNK_SIZE], uint32_t *length)
{
	uint32_t offset = 0;
	uint32_t dw;
	int write_loop = !!(smp->flags & CHN_LOOP);
	int write_sustain_loop = !!(smp->flags & CHN_SUSTAINLOOP);

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
	dw = bswapLE32(write_sustain_loop ? 2u : write_loop ? 1u : 0u);
	memcpy(smpl_data + offset, &dw, 4);
	offset += 4;

	/* sampler-specific data (we have none) */
	memset(smpl_data + offset, 0, 4);
	offset += 4;

	if (write_sustain_loop)
		iff_fill_smpl_chunk_loop(smpl_data, &offset, smp->sustain_start, smp->sustain_end, !!(smp->flags & CHN_PINGPONGSUSTAIN));

	if (write_loop) {
		iff_fill_smpl_chunk_loop(smpl_data, &offset, smp->loop_start, smp->loop_end, !!(smp->flags & CHN_PINGPONGLOOP));
	} else if (write_sustain_loop) {
		/* legendary hackaround from OpenMPT:
		 *
		 * Since there are no "loop types" to distinguish between sustain and normal loops, OpenMPT assumes
		 * that the first loop is a sustain loop if there are two loops. If we only want a sustain loop,
		 * we will have to write a second bogus loop. */
		iff_fill_smpl_chunk_loop(smpl_data, &offset, 0, 0, 0);
	}

	*length = offset;

	/* fill in the chunk length */
	dw = bswapLE32(offset - 8u);
	memcpy(smpl_data + 4, &dw, 4);
}
