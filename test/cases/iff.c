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

#include "test.h"
#include "test-assertions.h"

#include "slurp.h"
#include "fmt.h"

#include <stdio.h>

#define IFFID_fmt  UINT32_C(0x666D7420)

testresult_t test_iff_chunk_peek_ex_middle(void)
{
	// Arrange

	char input_data[] = {
		'W', 'A', 'V', 'E', // Tail of the RIFF header
		'f', 'm', 't', ' ', // Actual fmt chunk from a real WAV file
		0x10, 0x00, 0x00, 0x00, // length: 16 bytes
		0x01, 0x00, // format: uncompressed PCM (1)
		0x02, 0x00, // channel count
		0xDC, 0x9B, 0x00, 0x00, // sample rate (0x9BDC == 39,900 Hz)
		0x70, 0x6F, 0x02, 0x00, // average byte rate (samples x 2 bytes per sample x 2 channels = 0x26F70 == 159,600)
		0x04, 0x00, // frame alignment (4 bytes)
		0x10, 0x00, // bits per sample (16)
		'd', 'a', 't', 'a', // Lead in to the following chunk
		0x80, 0x61, 0x01, 0x00, // etc.
	};

	int fmt_start = 4;
	int fmt_content_start = fmt_start + 8;
	int fmt_length = bswapLE32(((int *)&input_data[fmt_start])[1]);

	int data_start = fmt_content_start + fmt_length;

	if (strncmp(input_data + data_start, "data", 4) != 0) {
		RETURN_INCONCLUSIVE;
	}

	slurp_t fp;

	iff_chunk_t chunk = { 0 };

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

	char input_data[] = {
		'W', 'A', 'V', 'E', // Tail of the RIFF header
		'f', 'm', 't', ' ', // Actual fmt chunk from a real WAV file
		0x10, 0x00, 0x00, 0x00, // length: 16 bytes
		0x01, 0x00, // format: uncompressed PCM (1)
		0x02, 0x00, // channel count
		0xDC, 0x9B, 0x00, 0x00, // sample rate (0x9BDC == 39,900 Hz)
		0x70, 0x6F, 0x02, 0x00, // average byte rate (samples x 2 bytes per sample x 2 channels = 0x26F70 == 159,600)
		0x04, 0x00, // frame alignment (4 bytes)
		0x10, 0x00, // bits per sample (16)
	};

	int fmt_start = 4;
	int fmt_content_start = fmt_start + 8;
	int fmt_length = bswapLE32(((int *)&input_data[fmt_start])[1]);

	slurp_t fp;

	iff_chunk_t chunk = { 0 };

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
