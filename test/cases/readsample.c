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

#include "player/sndfile.h"

#define EXPECT_MONO 1
#define EXPECT_STEREO 2

#define EXPECT_8BIT 4
#define EXPECT_16BIT 8

typedef struct {
	uint32_t flags;
	char data[272];
	int data_length;
	int expectation_flags;
} testcase_t;

testresult_t test_readsample_case(testcase_t *testcase);

#include "readsample.cases.inc"

testresult_t test_readsample_case(testcase_t *testcase)
{
	slurp_t fp;
	song_sample_t sample;
	uint32_t result;
	int num_samples;
	const char *expected_bytes;
	int num_expected_bytes;

	int data_matches;

	// Arrange
	slurp_memstream(&fp, testcase->data, testcase->data_length);

	int is_8bit = (testcase->expectation_flags & EXPECT_8BIT) != 0;
	int is_16bit = (testcase->expectation_flags & EXPECT_16BIT) != 0;

	int is_mono = (testcase->expectation_flags & EXPECT_MONO) != 0;
	int is_stereo = (testcase->expectation_flags & EXPECT_STEREO) != 0;

	ASSERT_PRINTF(is_8bit ^ is_16bit, "exactly one of EXPECT_8BIT and EXPECT_16BIT must be set", NULL);
	ASSERT_PRINTF(is_mono ^ is_stereo, "exactly one of EXPECT_MONO and EXPECT_STEREO must be set", NULL);

	if (is_8bit) {
		num_samples = sizeof(expected_8bit_mono) / sizeof(expected_8bit_mono[0]);

		if (is_mono) {
			expected_bytes = expected_8bit_mono;
			num_expected_bytes = sizeof(expected_8bit_mono);
		} else {
			expected_bytes = expected_8bit_stereo;
			num_expected_bytes = sizeof(expected_8bit_stereo);
		}
	} else {
		num_samples = sizeof(expected_16bit_mono) / sizeof(expected_16bit_mono[0]);

		if (is_mono) {
			expected_bytes = (char *)&expected_16bit_mono[0];
			num_expected_bytes = sizeof(expected_16bit_mono);
		} else {
			expected_bytes = (char *)&expected_16bit_stereo[0];
			num_expected_bytes = sizeof(expected_16bit_stereo);
		}
	}

	memset(&sample, 0, sizeof(sample));

	sample.length = num_samples;

	// Act
	result = csf_read_sample(&sample, testcase->flags, &fp);

	// Assert
	ASSERT(result == testcase->data_length);

	data_matches = memcmp(sample.data, expected_bytes, num_expected_bytes) == 0;

	ASSERT(data_matches);

	RETURN_PASS;
}