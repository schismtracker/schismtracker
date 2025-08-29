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

#include "test-case-index.h"

TEST_THUNK(test_song_get_pattern_offset_0,
	test_song_get_pattern,
		0, 15, // starting from 0:15
		0,     // advance by 0 rows
		0, 15) // expect to be at 0:15

TEST_THUNK(test_song_get_pattern_offset_same_pattern_1,
	test_song_get_pattern,
		0, 15, // starting from 0:15
		1,     // advance by 0 rows
		0, 16) // expect to be at 0:16

TEST_THUNK(test_song_get_pattern_offset_same_pattern_n,
	test_song_get_pattern,
		0, 15, // starting from
		10,    // advance by
		0, 25) // expect to be at

TEST_THUNK(test_song_get_pattern_offset_same_pattern_LAST,
	test_song_get_pattern,
		0, 15, // starting from
		16,    // advance by
		0, 31) // expect to be at

TEST_THUNK(test_song_get_pattern_offset_next_pattern_FIRST,
	test_song_get_pattern,
		0, 15, // starting from
		17,    // advance by
		1, 0) // expect to be at

TEST_THUNK(test_song_get_pattern_offset_next_pattern_n,
	test_song_get_pattern,
		0, 15, // starting from
		27,    // advance by
		1, 10) // expect to be at

TEST_THUNK(test_song_get_pattern_offset_next_pattern_LAST,
	test_song_get_pattern,
		0, 15, // starting from
		31,    // advance by
		1, 14) // expect to be at

TEST_THUNK(test_song_get_pattern_offset_more_than_two_patterns,
	test_song_get_pattern,
		0, 15, // starting from
		96,    // advance by
		3, 0)  // expect to be at

TEST_THUNK(test_song_get_pattern_offset_from_middle_same_pattern,
	test_song_get_pattern,
		2, 15, // starting from
		2,     // advance by
		2, 17) // expect to be at

TEST_THUNK(test_song_get_pattern_offset_from_middle_next_pattern,
	test_song_get_pattern,
		2, 15, // starting from
		49,    // advance by
		3, 0)  // expect to be at

TEST_THUNK(test_song_get_pattern_offset_from_middle_more_than_two_patterns,
	test_song_get_pattern,
		2, 16, // starting from
		49,    // advance by
		4, 0)  // expect to be at

TEST_THUNK(test_song_get_pattern_offset_song_LAST,
	test_song_get_pattern,
		2, 15,  // starting from
		113,    // advance by
		4, 63)  // expect to be at

TEST_THUNK(test_song_get_pattern_offset_past_end_of_song,
	test_song_get_pattern_hook,
		2, 15, // starting from
		verify_end_of_song, // pre hook
		114,   // advance by
		latch_new_pattern_length, // post hook
		5, 0,  // expect to be at
		restore_pattern_length_array)
