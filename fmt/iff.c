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

	size = slurp_peek(fp, data, MIN(chunk->size, size));

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
