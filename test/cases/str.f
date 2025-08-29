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

#ifndef TEST_THUNK
#define TEST_THUNK(...)
#endif

TEST_THUNK(test_str_from_num_thousands_0, test_str_from_num_thousands, 0, "0")
TEST_THUNK(test_str_from_num_thousands_999, test_str_from_num_thousands, 999, "999")
TEST_THUNK(test_str_from_num_thousands_1_000, test_str_from_num_thousands, 1000, "1,000")
TEST_THUNK(test_str_from_num_thousands_999_999, test_str_from_num_thousands, 999999, "999,999")
TEST_THUNK(test_str_from_num_thousands_1_000_000, test_str_from_num_thousands, 1000000, "1,000,000")
TEST_THUNK(test_str_from_num_thousands_999_999_999, test_str_from_num_thousands, 999999999, "999,999,999")
TEST_THUNK(test_str_from_num_thousands_1_000_000_000, test_str_from_num_thousands, 1000000000, "1,000,000,000")
TEST_THUNK(test_str_from_num_thousands_int32_min, test_str_from_num_thousands, INT32_MIN, "-2,147,483,648")
TEST_THUNK(test_str_from_num_thousands_int32_max, test_str_from_num_thousands, INT32_MAX, "2,147,483,647")
TEST_THUNK(test_str_from_num_thousands_digits_1_positive, test_str_from_num_thousands, 7, "7")
TEST_THUNK(test_str_from_num_thousands_digits_2_positive, test_str_from_num_thousands, 32, "32")
TEST_THUNK(test_str_from_num_thousands_digits_3_positive, test_str_from_num_thousands, 891, "891")
TEST_THUNK(test_str_from_num_thousands_digits_4_positive, test_str_from_num_thousands, 5834, "5,834")
TEST_THUNK(test_str_from_num_thousands_digits_5_positive, test_str_from_num_thousands, 45891, "45,891")
TEST_THUNK(test_str_from_num_thousands_digits_6_positive, test_str_from_num_thousands, 591848, "591,848")
TEST_THUNK(test_str_from_num_thousands_digits_7_positive, test_str_from_num_thousands, 5691834, "5,691,834")
TEST_THUNK(test_str_from_num_thousands_digits_8_positive, test_str_from_num_thousands, 68917829, "68,917,829")
TEST_THUNK(test_str_from_num_thousands_digits_9_positive, test_str_from_num_thousands, 591848968, "591,848,968")
TEST_THUNK(test_str_from_num_thousands_digits_10_positive, test_str_from_num_thousands, 2082850450, "2,082,850,450")
TEST_THUNK(test_str_from_num_thousands_digits_1_negative, test_str_from_num_thousands, -7, "-7")
TEST_THUNK(test_str_from_num_thousands_digits_2_negative, test_str_from_num_thousands, -32, "-32")
TEST_THUNK(test_str_from_num_thousands_digits_3_negative, test_str_from_num_thousands, -891, "-891")
TEST_THUNK(test_str_from_num_thousands_digits_4_negative, test_str_from_num_thousands, -5834, "-5,834")
TEST_THUNK(test_str_from_num_thousands_digits_5_negative, test_str_from_num_thousands, -45891, "-45,891")
TEST_THUNK(test_str_from_num_thousands_digits_6_negative, test_str_from_num_thousands, -591848, "-591,848")
TEST_THUNK(test_str_from_num_thousands_digits_7_negative, test_str_from_num_thousands, -5691834, "-5,691,834")
TEST_THUNK(test_str_from_num_thousands_digits_8_negative, test_str_from_num_thousands, -68917829, "-68,917,829")
TEST_THUNK(test_str_from_num_thousands_digits_9_negative, test_str_from_num_thousands, -591848968, "-591,848,968")
TEST_THUNK(test_str_from_num_thousands_digits_10_negative, test_str_from_num_thousands, -2082850450, "-2,082,850,450")

TEST_THUNK(test_str_concat_123456789012, test_str_concat, "123", "456", "789", "012", NULL, "123456789012")
TEST_THUNK(test_str_concat_123ok67, test_str_concat, "123", "ok", "67", NULL, NULL, "123ok67")

/* ------------------------------------------------------------------------ */

TEST_THUNK(test_str_from_num_1,          test_str_from_num, 0, UINT32_C(1),          "1")
TEST_THUNK(test_str_from_num_10,         test_str_from_num, 0, UINT32_C(10),         "10")
TEST_THUNK(test_str_from_num_100,        test_str_from_num, 0, UINT32_C(100),        "100")
TEST_THUNK(test_str_from_num_1000,       test_str_from_num, 0, UINT32_C(1000),       "1000")
TEST_THUNK(test_str_from_num_10000,      test_str_from_num, 0, UINT32_C(10000),      "10000")
TEST_THUNK(test_str_from_num_100000,     test_str_from_num, 0, UINT32_C(100000),     "100000")
TEST_THUNK(test_str_from_num_1000000,    test_str_from_num, 0, UINT32_C(1000000),    "1000000")
TEST_THUNK(test_str_from_num_10000000,   test_str_from_num, 0, UINT32_C(10000000),   "10000000")
TEST_THUNK(test_str_from_num_100000000,  test_str_from_num, 0, UINT32_C(100000000),  "100000000")
TEST_THUNK(test_str_from_num_1000000000, test_str_from_num, 0, UINT32_C(1000000000), "1000000000")
TEST_THUNK(test_str_from_num_01,         test_str_from_num, 2, UINT32_C(1),          "01")
TEST_THUNK(test_str_from_num_0005,       test_str_from_num, 4, UINT32_C(5),          "0005")
TEST_THUNK(test_str_from_num_000000009,  test_str_from_num, 9, UINT32_C(9),          "000000009")

TEST_THUNK(test_str_from_num_signed_1,             test_str_from_num_signed, 0, INT32_C(1),          "1")
TEST_THUNK(test_str_from_num_signed_10,            test_str_from_num_signed, 0, INT32_C(10),         "10")
TEST_THUNK(test_str_from_num_signed_100,           test_str_from_num_signed, 0, INT32_C(100),        "100")
TEST_THUNK(test_str_from_num_signed_1000,          test_str_from_num_signed, 0, INT32_C(1000),       "1000")
TEST_THUNK(test_str_from_num_signed_10000,         test_str_from_num_signed, 0, INT32_C(10000),      "10000")
TEST_THUNK(test_str_from_num_signed_100000,        test_str_from_num_signed, 0, INT32_C(100000),     "100000")
TEST_THUNK(test_str_from_num_signed_1000000,       test_str_from_num_signed, 0, INT32_C(1000000),    "1000000")
TEST_THUNK(test_str_from_num_signed_10000000,      test_str_from_num_signed, 0, INT32_C(10000000),   "10000000")
TEST_THUNK(test_str_from_num_signed_100000000,     test_str_from_num_signed, 0, INT32_C(100000000),  "100000000")
TEST_THUNK(test_str_from_num_signed_1000000000,    test_str_from_num_signed, 0, INT32_C(1000000000), "1000000000")
TEST_THUNK(test_str_from_num_signed_01,            test_str_from_num_signed, 2, INT32_C(1),          "01")
TEST_THUNK(test_str_from_num_signed_0005,          test_str_from_num_signed, 4, INT32_C(5),          "0005")
TEST_THUNK(test_str_from_num_signed_000000009,     test_str_from_num_signed, 9, INT32_C(9),          "000000009")
TEST_THUNK(test_str_from_num_signed_neg_09,        test_str_from_num_signed, 3, INT32_C(-9),         "-09")
TEST_THUNK(test_str_from_num_signed_neg_0009,      test_str_from_num_signed, 5, INT32_C(-9),         "-0009")
TEST_THUNK(test_str_from_num_signed_neg_000000009, test_str_from_num_signed, 9, INT32_C(-9),         "-00000009")

/* ------------------------------------------------------------------------ */

TEST_THUNK(test_str_get_num_lines_1, test_str_get_num_lines, "wow\n", 1)
TEST_THUNK(test_str_get_num_lines_2, test_str_get_num_lines, "wow", 0 /* wait, what? */)
TEST_THUNK(test_str_get_num_lines_3, test_str_get_num_lines, "wow\r\n", 1)
TEST_THUNK(test_str_get_num_lines_4, test_str_get_num_lines, "wow\r\nhai\n", 2)
TEST_THUNK(test_str_get_num_lines_5, test_str_get_num_lines, "nice\n\r\n", 2)
TEST_THUNK(test_str_get_num_lines_6, test_str_get_num_lines, "awesome\r\n\n", 2)
