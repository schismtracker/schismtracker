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

#include "util.h"

testresult_t test_mem_xor(void)
{
	unsigned char x[] = {
		0x8B, 0xBE, 0xB4, 0xBA, 0x8B, 0xAD, 0xBE, 0xBC,
		0xB4, 0xBA, 0xAD, 0xBA, 0xBB, 0xFF, 0xA8, 0xB6,
		0xAB, 0xB7, 0xFF, 0xA9, 0xBA, 0xAD, 0xAC, 0xB6,
		0xB0, 0xB1, 0xFF, 0xEF, 0xF1, 0xE6, 0xBA, 0xFE,
		0xFE, 0xFE, 0xFE, 0xFE,
	};

	mem_xor(x, sizeof(x), 0xDF);

	if (memcmp(x, "TakeTrackered with version 0.9e!!!!!", sizeof(x)))
		RETURN_FAIL;

	RETURN_PASS;
}

#define PATTERN(BITS) 0, INT##BITS##_MIN, INT##BITS##_MAX, 0
#define PATTERN_ARR(BITS) \
	static const int##BITS##_t minmax_data_##BITS[16] = { \
		PATTERN(BITS), PATTERN(BITS), PATTERN(BITS), PATTERN(BITS) \
	};

PATTERN_ARR(8)
PATTERN_ARR(16)
PATTERN_ARR(32)

#define TEST_MINMAX(BITS) \
	testresult_t test_minmax_##BITS(void) \
	{ \
		int##BITS##_t min[2]; \
		int##BITS##_t max[2]; \
	\
		min[0] = INT##BITS##_MAX; \
		max[0] = INT##BITS##_MIN; \
	 \
		minmax_##BITS(minmax_data_##BITS, ARRAY_SIZE(minmax_data_##BITS), min, max, 1); \
 \
		ASSERT_PRINTF(min[0] == INT##BITS##_MIN, "%" PRId32, min[0]); \
		ASSERT_PRINTF(max[0] == INT##BITS##_MAX, "%" PRId32, max[0]); \
 \
		min[0] = INT##BITS##_MAX; \
		max[0] = INT##BITS##_MIN; \
 \
		minmax_##BITS(minmax_data_##BITS, ARRAY_SIZE(minmax_data_##BITS), min, max, 2); \
 \
		ASSERT_PRINTF(min[0] == 0, "%" PRId32, min[0]); \
		ASSERT_PRINTF(max[0] == INT##BITS##_MAX, "%" PRId32, max[0]); \
 \
		min[0] = INT##BITS##_MAX; \
		max[0] = INT##BITS##_MIN; \
 \
		minmax_##BITS(minmax_data_##BITS+1, ARRAY_SIZE(minmax_data_##BITS), min, max, 2); \
 \
		ASSERT_PRINTF(min[0] == INT##BITS##_MIN, "%" PRId32, min[0]); \
		ASSERT_PRINTF(max[0] == 0, "%" PRId32, max[0]); \
 \
		min[0] = min[1] = INT##BITS##_MAX; \
		max[0] = max[1] = INT##BITS##_MIN; \
 \
		minmax_##BITS##_arr(minmax_data_##BITS, ARRAY_SIZE(minmax_data_##BITS), min, max, 2); \
 \
		ASSERT_PRINTF(min[0] == 0, "%" PRId32, min[0]); \
		ASSERT_PRINTF(max[0] == INT##BITS##_MAX, "%" PRId32, max[0]); \
		ASSERT_PRINTF(min[1] == INT##BITS##_MIN, "%" PRId32, min[1]); \
		ASSERT_PRINTF(max[1] == 0, "%" PRId32, max[1]); \
 \
		RETURN_PASS; \
	}

TEST_MINMAX(8)
TEST_MINMAX(16)
TEST_MINMAX(32)
