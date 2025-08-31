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

#include "song.h"

static int test_pattern_length[] = { 32, 15, 64, 1, 64, 0, 0 };

static song_t *create_subject(void)
{
	song_t *ret = csf_allocate();

	for (int i = 0; test_pattern_length[i]; i++)
	{
		ret->patterns[i] = csf_allocate_pattern(test_pattern_length[i]);
		ret->pattern_size[i] = test_pattern_length[i];
	}

	return ret;
}

static testresult_t test_song_get_pattern_hook(
	int start_pattern_number, int start_row_number, // for arrange
	testresult_t (*pre_act)(song_t *csf),
	int test_offset, // for act
	testresult_t (*post_act)(song_t *csf),
	int expected_pattern_number, int expected_row_number, // for assert
	testresult_t (*exit_act)(song_t *csf))
{
	// Arrange
	song_t *csf = create_subject();

	current_song = csf;

	song_note_t *pattern;

	int start_pattern_length = song_get_pattern(start_pattern_number, &pattern);

	REQUIRE_EQUAL_D(start_pattern_length, test_pattern_length[start_pattern_number]);

	int pattern_number = start_pattern_number;
	int row_number = start_row_number;

	// Hook
	if (pre_act != NULL) {
		testresult_t pre_result = pre_act(csf);

		if (pre_result != SCHISM_TESTRESULT_PASS) {
			if (exit_act != NULL) exit_act(csf);
			return pre_result;
		}
	}

	// Act
	int result = song_get_pattern_offset(&pattern_number, &pattern, &row_number, test_offset);

	// Hook
	if (post_act != NULL) {
		testresult_t post_result = post_act(csf);

		if (post_result != SCHISM_TESTRESULT_PASS) {
			if (exit_act != NULL) exit_act(csf);
			return post_result;
		}
	}

	// Assert
	if (expected_pattern_number < 0) { /* expect failure */
		ASSERT_EQUAL_D(result, 0);
	}
	else {
		ASSERT_EQUAL_D(result, test_pattern_length[expected_pattern_number]);
		ASSERT_EQUAL_D(pattern_number, expected_pattern_number);
		ASSERT_EQUAL_D(row_number, expected_row_number);
		ASSERT_EQUAL_P(pattern, csf->patterns[pattern_number]);
	}

	csf_free(csf);

	if (exit_act != NULL) {
		testresult_t exit_result = exit_act(csf);

		if (exit_result != SCHISM_TESTRESULT_PASS) {
			return exit_result;
		}
	}

	RETURN_PASS;
}

static testresult_t test_song_get_pattern(
	int start_pattern_number, int start_row_number, // for arrange
	int test_offset, // for act
	int expected_pattern_number, int expected_row_number) // for assert
{
	return test_song_get_pattern_hook(
		start_pattern_number, start_row_number,
		NULL,  // pre-action hook
		test_offset,
		NULL,  // post-action hook
		expected_pattern_number, expected_row_number,
		NULL); // exit hook
}

static testresult_t verify_end_of_song(song_t *csf)
{
	ASSERT(csf_get_num_patterns(csf) <= 5);
	ASSERT_NULL(csf->patterns[5]);

	RETURN_PASS;
}

static testresult_t latch_new_pattern_length(song_t *csf)
{
	test_pattern_length[5] = song_get_max_row_number_in_pattern(5) + 1;

	RETURN_PASS;
}

static testresult_t restore_pattern_length_array(song_t *csf)
{
	test_pattern_length[5] = 0;

	RETURN_PASS;
}

testresult_t test_song_get_pattern_offset_0(void)
{
	return test_song_get_pattern(
		0, 15,  // starting from 0:15
		0,      // advance by 0 rows
		0, 15); // expect to be at 0:15
}

testresult_t test_song_get_pattern_offset_same_pattern_1(void)
{
	return test_song_get_pattern(
		0, 15,  // starting from 0:15
		1,      // advance by 0 rows
		0, 16); // expect to be at 0:16
}

testresult_t test_song_get_pattern_offset_same_pattern_n(void)
{
	return test_song_get_pattern(
		0, 15,  // starting from
		10,     // advance by
		0, 25); // expect to be at
}

testresult_t test_song_get_pattern_offset_same_pattern_LAST(void)
{
	return test_song_get_pattern(
		0, 15,  // starting from
		16,     // advance by
		0, 31); // expect to be at
}

testresult_t test_song_get_pattern_offset_next_pattern_FIRST(void)
{
	return test_song_get_pattern(
		0, 15, // starting from
		17,    // advance by
		1, 0); // expect to be at
}

testresult_t test_song_get_pattern_offset_next_pattern_n(void)
{
	return test_song_get_pattern(
		0, 15,  // starting from
		27,     // advance by
		1, 10); // expect to be at
}

testresult_t test_song_get_pattern_offset_next_pattern_LAST(void)
{
	return test_song_get_pattern(
		0, 15,  // starting from
		31,     // advance by
		1, 14); // expect to be at
}

testresult_t test_song_get_pattern_offset_more_than_two_patterns(void)
{
	return test_song_get_pattern(
		0, 15, // starting from
		96,    // advance by
		3, 0); // expect to be at
}

testresult_t test_song_get_pattern_offset_from_middle_same_pattern(void)
{
	return test_song_get_pattern(
		2, 15,  // starting from
		2,      // advance by
		2, 17); // expect to be at
}

testresult_t test_song_get_pattern_offset_from_middle_next_pattern(void)
{
	return test_song_get_pattern(
		2, 15, // starting from
		49,    // advance by
		3, 0); // expect to be at
}

testresult_t test_song_get_pattern_offset_from_middle_more_than_two_patterns(void)
{
	return test_song_get_pattern(
		2, 16, // starting from
		49,    // advance by
		4, 0); // expect to be at
}

testresult_t test_song_get_pattern_offset_song_LAST(void)
{
	return test_song_get_pattern(
		2, 15,  // starting from
		113,    // advance by
		4, 63); // expect to be at
}

testresult_t test_song_get_pattern_offset_past_end_of_song(void)
{
	return test_song_get_pattern_hook(
		2, 15, // starting from
		verify_end_of_song, // pre hook
		114,   // advance by
		latch_new_pattern_length, // post hook
		5, 0,  // expect to be at
		restore_pattern_length_array);
}

