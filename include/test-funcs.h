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

#ifndef TEST_FUNC
# define TEST_FUNC(x) testresult_t x(void);
#endif

TEST_FUNC(test_bshift_arithmetic)
TEST_FUNC(test_bshift_right_shift_negative)
TEST_FUNC(test_bshift_left_shift_overflow)

TEST_FUNC(test_bswap_16)
TEST_FUNC(test_bswap_32)
TEST_FUNC(test_bswap_64)
TEST_FUNC(test_babs8)
TEST_FUNC(test_babs16)
TEST_FUNC(test_babs32)
TEST_FUNC(test_babs64)
TEST_FUNC(test_bavgu8)
TEST_FUNC(test_bavgu16)
TEST_FUNC(test_bavgu32)
TEST_FUNC(test_bavgu64)
TEST_FUNC(test_bavgs8)
TEST_FUNC(test_bavgs16)
TEST_FUNC(test_bavgs32)
TEST_FUNC(test_bavgs64)
TEST_FUNC(test_bitarray)

TEST_FUNC(test_str_from_num_thousands_0)
TEST_FUNC(test_str_from_num_thousands_999)
TEST_FUNC(test_str_from_num_thousands_1_000)
TEST_FUNC(test_str_from_num_thousands_999_999)
TEST_FUNC(test_str_from_num_thousands_1_000_000)
TEST_FUNC(test_str_from_num_thousands_999_999_999)
TEST_FUNC(test_str_from_num_thousands_1_000_000_000)
TEST_FUNC(test_str_from_num_thousands_int32_min)
TEST_FUNC(test_str_from_num_thousands_int32_max)
TEST_FUNC(test_str_from_num_thousands_digits_1_positive)
TEST_FUNC(test_str_from_num_thousands_digits_2_positive)
TEST_FUNC(test_str_from_num_thousands_digits_3_positive)
TEST_FUNC(test_str_from_num_thousands_digits_4_positive)
TEST_FUNC(test_str_from_num_thousands_digits_5_positive)
TEST_FUNC(test_str_from_num_thousands_digits_6_positive)
TEST_FUNC(test_str_from_num_thousands_digits_7_positive)
TEST_FUNC(test_str_from_num_thousands_digits_8_positive)
TEST_FUNC(test_str_from_num_thousands_digits_9_positive)
TEST_FUNC(test_str_from_num_thousands_digits_10_positive)
TEST_FUNC(test_str_from_num_thousands_digits_1_negative)
TEST_FUNC(test_str_from_num_thousands_digits_2_negative)
TEST_FUNC(test_str_from_num_thousands_digits_3_negative)
TEST_FUNC(test_str_from_num_thousands_digits_4_negative)
TEST_FUNC(test_str_from_num_thousands_digits_5_negative)
TEST_FUNC(test_str_from_num_thousands_digits_6_negative)
TEST_FUNC(test_str_from_num_thousands_digits_7_negative)
TEST_FUNC(test_str_from_num_thousands_digits_8_negative)
TEST_FUNC(test_str_from_num_thousands_digits_9_negative)
TEST_FUNC(test_str_from_num_thousands_digits_10_negative)

TEST_FUNC(test_str_from_num_0_1)
TEST_FUNC(test_str_from_num_0_10)
TEST_FUNC(test_str_from_num_0_100)
TEST_FUNC(test_str_from_num_0_1000)
TEST_FUNC(test_str_from_num_0_10000)
TEST_FUNC(test_str_from_num_0_100000)
TEST_FUNC(test_str_from_num_0_1000000)
TEST_FUNC(test_str_from_num_0_10000000)
TEST_FUNC(test_str_from_num_0_100000000)
TEST_FUNC(test_str_from_num_0_1000000000)
TEST_FUNC(test_str_from_num_2_1)
TEST_FUNC(test_str_from_num_4_5)
TEST_FUNC(test_str_from_num_9_9)

TEST_FUNC(test_str_from_num_signed_0_1)
TEST_FUNC(test_str_from_num_signed_0_10)
TEST_FUNC(test_str_from_num_signed_0_100)
TEST_FUNC(test_str_from_num_signed_0_1000)
TEST_FUNC(test_str_from_num_signed_0_10000)
TEST_FUNC(test_str_from_num_signed_0_100000)
TEST_FUNC(test_str_from_num_signed_0_1000000)
TEST_FUNC(test_str_from_num_signed_0_10000000)
TEST_FUNC(test_str_from_num_signed_0_100000000)
TEST_FUNC(test_str_from_num_signed_0_1000000000)
TEST_FUNC(test_str_from_num_signed_2_1)
TEST_FUNC(test_str_from_num_signed_4_5)
TEST_FUNC(test_str_from_num_signed_9_9)
TEST_FUNC(test_str_from_num_signed_3_neg9)
TEST_FUNC(test_str_from_num_signed_5_neg9)
TEST_FUNC(test_str_from_num_signed_9_neg9)

TEST_FUNC(test_str_get_num_lines_wow_LF)
TEST_FUNC(test_str_get_num_lines_wow)
TEST_FUNC(test_str_get_num_lines_wow_CRLF)
TEST_FUNC(test_str_get_num_lines_wow_CRLF_hai_LF)
TEST_FUNC(test_str_get_num_lines_nice_LF_CRLF)
TEST_FUNC(test_str_get_num_lines_awesome_CRLF_LF)
TEST_FUNC(test_str_get_num_lines_awesome_CR_hai_LF)

TEST_FUNC(test_str_concat_123456789012)
TEST_FUNC(test_str_concat_123ok67)

TEST_FUNC(test_slurp_memstream)
TEST_FUNC(test_slurp_2memstream)
TEST_FUNC(test_slurp_sf2)
TEST_FUNC(test_slurp_stdio)
#ifdef SCHISM_WIN32
TEST_FUNC(test_slurp_win32)
TEST_FUNC(test_slurp_win32_mmap)
#endif
#ifdef HAVE_MMAP
TEST_FUNC(test_slurp_mmap)
#endif
#ifdef USE_ZLIB
TEST_FUNC(test_slurp_gzip)
#endif

TEST_FUNC(test_config_file_defined_values)
TEST_FUNC(test_config_file_undefined_values_in_defined_section)
TEST_FUNC(test_config_file_undefined_section)
TEST_FUNC(test_config_file_obviously_broken_values)
TEST_FUNC(test_config_file_null_default_with_value_set)
TEST_FUNC(test_config_file_null_default_with_value_set_defined_key)
TEST_FUNC(test_config_file_string_boundary_defined_key)
TEST_FUNC(test_config_file_string_boundary_default_value)
TEST_FUNC(test_config_file_string_boundary_zero)
TEST_FUNC(test_config_file_set_string_in_new_section)
TEST_FUNC(test_config_file_set_new_string_in_existing_section)
TEST_FUNC(test_config_file_set_number_in_new_section)
TEST_FUNC(test_config_file_set_new_number_in_existing_section)
TEST_FUNC(test_config_file_set_string_in_null_section)
TEST_FUNC(test_config_file_set_number_in_null_section)
TEST_FUNC(test_config_file_set_string_with_null_key)
TEST_FUNC(test_config_file_set_number_with_null_key)
TEST_FUNC(test_config_file_set_string_with_null_value)
TEST_FUNC(test_config_file_get_string_with_null_value)

TEST_FUNC(test_song_get_pattern_offset_0)
TEST_FUNC(test_song_get_pattern_offset_same_pattern_1)
TEST_FUNC(test_song_get_pattern_offset_same_pattern_n)
TEST_FUNC(test_song_get_pattern_offset_same_pattern_LAST)
TEST_FUNC(test_song_get_pattern_offset_next_pattern_FIRST)
TEST_FUNC(test_song_get_pattern_offset_next_pattern_n)
TEST_FUNC(test_song_get_pattern_offset_next_pattern_LAST)
TEST_FUNC(test_song_get_pattern_offset_more_than_two_patterns)
TEST_FUNC(test_song_get_pattern_offset_from_middle_same_pattern)
TEST_FUNC(test_song_get_pattern_offset_from_middle_next_pattern)
TEST_FUNC(test_song_get_pattern_offset_from_middle_over_two_patterns)
TEST_FUNC(test_song_get_pattern_offset_song_LAST)
TEST_FUNC(test_song_get_pattern_offset_past_end_of_song)

TEST_FUNC(test_mem_xor)

TEST_FUNC(test_disko_mem)

#undef TEST_FUNC