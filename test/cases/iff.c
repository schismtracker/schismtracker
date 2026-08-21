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

#include "test-assertions.h"
#include "test.h"

#include "headers.h"
#include "fmt.h"
#include "slurp.h"

#define IFFID_fmt UINT32_C(0x666D7420)

/* All of the data is still technically separated still;
 * but they are based on a common ancestor */

#define WAVE_hdr "WAVE" /* Tail of the RIFF header */

#define WAVE_CHUNK_fmt \
	"fmt " /* chunk identifier */ \
	"\x10\x00\x00\x00" /* length: 16 bytes */ \
	"\x01\x00" /* format: uncompressed PCM (1) */ \
	"\x02\x00" /* channel count */ \
	"\xDC\x9B\x00\x00" /* sample rate (0x9BDC == 39,900 Hz) */ \
	"\x70\x6F\x02\x00" /* "byte rate" (bytes per sample * sample rate) */ \
	"\x04\x00" /* bytes per sample (stereo 16-bit) */ \
	"\x10\x00" /* bits per sample */

/* doesn't actually contain anything */
#define WAVE_CHUNK_data \
	"data" /* chunk identifier */ \
	"\x80\x61\x01\x00" /* random size (??) */

testresult_t test_iff_chunk_peek_ex_middle(void)
{
	// Arrange

	static const char input_data[sizeof(WAVE_hdr WAVE_CHUNK_fmt WAVE_CHUNK_data) - 1]
		= WAVE_hdr WAVE_CHUNK_fmt WAVE_CHUNK_data;

	int fmt_start = sizeof(WAVE_hdr) - 1;
	int fmt_content_start = fmt_start + 8;
	uint32_t fmt_length;

	memcpy(&fmt_length, &input_data[fmt_start + 4], sizeof(fmt_length));

	fmt_length = bswapLE32(fmt_length);

	int data_start = fmt_content_start + fmt_length;

	if (strncmp(input_data + data_start, "data", 4) != 0) {
		RETURN_INCONCLUSIVE;
	}

	slurp_t fp;

	iff_chunk_t chunk = {0};

	int success;

	slurp_memstream(&fp, input_data, sizeof(input_data));

	slurp_seek(&fp, fmt_start, SEEK_SET);

	// Act
	success = iff_chunk_peek_ex(&chunk, &fp, IFF_CHUNK_ALIGNED | IFF_CHUNK_SIZE_LE);

	// Assert
	ASSERT(success);
	ASSERT(slurp_tell(&fp) == data_start);

	ASSERT(chunk.id == IFFID_fmt);
	ASSERT(chunk.size == 16);
	ASSERT(chunk.offset == fmt_content_start);

	RETURN_PASS;
}

testresult_t test_iff_chunk_peek_ex_end_of_file(void)
{
	// Arrange
	static const char input_data[sizeof(WAVE_hdr WAVE_CHUNK_fmt) - 1] = WAVE_hdr WAVE_CHUNK_fmt;

	int fmt_start = 4;
	int fmt_content_start = fmt_start + 8;
	uint32_t fmt_length;

	memcpy(&fmt_length, &input_data[fmt_start + 4], sizeof(fmt_length));

	fmt_length = bswapLE32(fmt_length);

	slurp_t fp;

	iff_chunk_t chunk = {0};

	int success;

	slurp_memstream(&fp, input_data, sizeof(input_data));

	slurp_seek(&fp, fmt_start, SEEK_SET);

	// Act
	success = iff_chunk_peek_ex(&chunk, &fp, IFF_CHUNK_ALIGNED | IFF_CHUNK_SIZE_LE);

	// Assert
	ASSERT(success);
	ASSERT(slurp_tell(&fp) == sizeof(input_data));

	ASSERT(chunk.id == IFFID_fmt);
	ASSERT(chunk.size == 16);
	ASSERT(chunk.offset == fmt_content_start);

	RETURN_PASS;
}

testresult_t test_iff_chunk_peek_ex_truncated(void)
{
	// Arrange
	static const char input_data[sizeof(WAVE_hdr WAVE_CHUNK_fmt WAVE_CHUNK_data) - 1]
		= WAVE_hdr WAVE_CHUNK_fmt WAVE_CHUNK_data;

	int fmt_start = 4;
	int fmt_content_start = fmt_start + 8;
	uint32_t fmt_length;

	memcpy(&fmt_length, &input_data[fmt_start + 4], sizeof(fmt_length));

	fmt_length = bswapLE32(fmt_length);

	slurp_t fp;

	iff_chunk_t chunk = {0};

	int success;

	slurp_memstream(&fp, input_data, sizeof(input_data));

	slurp_seek(&fp, fmt_start, SEEK_SET);

	// Act
	success = iff_chunk_peek_ex(&chunk, &fp, IFF_CHUNK_ALIGNED | IFF_CHUNK_SIZE_LE);

	// Assert
	ASSERT(success);
	ASSERT(slurp_tell(&fp) == (sizeof(WAVE_hdr WAVE_CHUNK_fmt) - 1));

	ASSERT(chunk.id == IFFID_fmt);
	ASSERT(chunk.size == 16);
	ASSERT(chunk.offset == fmt_content_start);

	success = iff_chunk_peek_ex(&chunk, &fp, IFF_CHUNK_ALIGNED | IFF_CHUNK_SIZE_LE);

	ASSERT(!success);
	/* where is the file pointer supposed to be? */

	RETURN_PASS;
}
