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

TEST(test_bshift_arithmetic)
TEST(test_bshift_right_shift_negative)
TEST(test_bshift_left_shift_overflow)
TEST(test_bswap_16)
TEST(test_bswap_32)
TEST(test_bswap_64)
TEST(test_babs8)
TEST(test_babs16)
TEST(test_babs32)
TEST(test_babs64)
TEST(test_bavgu8)
TEST(test_bavgu16)
TEST(test_bavgu32)
TEST(test_bavgu64)
TEST(test_bavgs8)
TEST(test_bavgs16)
TEST(test_bavgs32)
TEST(test_bavgs64)
TEST(test_bitarray)

TEST(test_config_file_defined_values)
TEST(test_config_file_undefined_values_in_defined_section)
TEST(test_config_file_undefined_section)
TEST(test_config_file_obviously_broken_values)
TEST(test_config_file_null_default_with_value_set)
TEST(test_config_file_null_default_with_value_set_defined_key)
TEST(test_config_file_string_boundary_defined_key)
TEST(test_config_file_string_boundary_default_value)
TEST(test_config_file_string_boundary_zero)

TEST(test_config_file_set_string_in_new_section)
TEST(test_config_file_set_new_string_in_existing_section)
TEST(test_config_file_set_number_in_new_section)
TEST(test_config_file_set_new_number_in_existing_section)
TEST(test_config_file_set_string_in_null_section)
TEST(test_config_file_set_number_in_null_section)
TEST(test_config_file_set_string_with_null_key)
TEST(test_config_file_set_number_with_null_key)
TEST(test_config_file_set_string_with_null_value)
TEST(test_config_file_get_string_with_null_value)

TEST(test_song_get_pattern_offset_0)
TEST(test_song_get_pattern_offset_same_pattern_1)
TEST(test_song_get_pattern_offset_same_pattern_n)
TEST(test_song_get_pattern_offset_same_pattern_LAST)
TEST(test_song_get_pattern_offset_next_pattern_FIRST)
TEST(test_song_get_pattern_offset_next_pattern_n)
TEST(test_song_get_pattern_offset_next_pattern_LAST)
TEST(test_song_get_pattern_offset_more_than_two_patterns)
TEST(test_song_get_pattern_offset_from_middle_same_pattern)
TEST(test_song_get_pattern_offset_from_middle_next_pattern)
TEST(test_song_get_pattern_offset_from_middle_more_than_two_patterns)
TEST(test_song_get_pattern_offset_song_LAST)
TEST(test_song_get_pattern_offset_past_end_of_song)

TEST(test_slurp_memstream)
TEST(test_slurp_2memstream)
TEST(test_slurp_sf2)
TEST(test_slurp_stdio)

TEST(test_str_from_num_thousands_0)
TEST(test_str_from_num_thousands_999)
TEST(test_str_from_num_thousands_1_000)
TEST(test_str_from_num_thousands_999_999)
TEST(test_str_from_num_thousands_1_000_000)
TEST(test_str_from_num_thousands_999_999_999)
TEST(test_str_from_num_thousands_1_000_000_000)
TEST(test_str_from_num_thousands_int32_min)
TEST(test_str_from_num_thousands_int32_max)
TEST(test_str_from_num_thousands_digits_1_positive)
TEST(test_str_from_num_thousands_digits_2_positive)
TEST(test_str_from_num_thousands_digits_3_positive)
TEST(test_str_from_num_thousands_digits_4_positive)
TEST(test_str_from_num_thousands_digits_5_positive)
TEST(test_str_from_num_thousands_digits_6_positive)
TEST(test_str_from_num_thousands_digits_7_positive)
TEST(test_str_from_num_thousands_digits_8_positive)
TEST(test_str_from_num_thousands_digits_9_positive)
TEST(test_str_from_num_thousands_digits_10_positive)
TEST(test_str_from_num_thousands_digits_1_negative)
TEST(test_str_from_num_thousands_digits_2_negative)
TEST(test_str_from_num_thousands_digits_3_negative)
TEST(test_str_from_num_thousands_digits_4_negative)
TEST(test_str_from_num_thousands_digits_5_negative)
TEST(test_str_from_num_thousands_digits_6_negative)
TEST(test_str_from_num_thousands_digits_7_negative)
TEST(test_str_from_num_thousands_digits_8_negative)
TEST(test_str_from_num_thousands_digits_9_negative)
TEST(test_str_from_num_thousands_digits_10_negative)

TEST(test_str_concat_123456789012)
TEST(test_str_concat_123ok67)

/* ------------------------------------------------------------------------ */

TEST(test_str_from_num_1)
TEST(test_str_from_num_10)
TEST(test_str_from_num_100)
TEST(test_str_from_num_1000)
TEST(test_str_from_num_10000)
TEST(test_str_from_num_100000)
TEST(test_str_from_num_1000000)
TEST(test_str_from_num_10000000)
TEST(test_str_from_num_100000000)
TEST(test_str_from_num_1000000000)
TEST(test_str_from_num_01)
TEST(test_str_from_num_0005)
TEST(test_str_from_num_000000009)

TEST(test_str_from_num_signed_1)
TEST(test_str_from_num_signed_10)
TEST(test_str_from_num_signed_100)
TEST(test_str_from_num_signed_1000)
TEST(test_str_from_num_signed_10000)
TEST(test_str_from_num_signed_100000)
TEST(test_str_from_num_signed_1000000)
TEST(test_str_from_num_signed_10000000)
TEST(test_str_from_num_signed_100000000)
TEST(test_str_from_num_signed_1000000000)
TEST(test_str_from_num_signed_01)
TEST(test_str_from_num_signed_0005)
TEST(test_str_from_num_signed_000000009)
TEST(test_str_from_num_signed_neg_09)
TEST(test_str_from_num_signed_neg_0009)
TEST(test_str_from_num_signed_neg_000000009)

/* ------------------------------------------------------------------------ */

TEST(test_str_get_num_lines_1)
TEST(test_str_get_num_lines_2)
TEST(test_str_get_num_lines_3)
TEST(test_str_get_num_lines_4)
TEST(test_str_get_num_lines_5)
TEST(test_str_get_num_lines_6)
