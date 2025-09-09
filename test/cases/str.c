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
#include "test-format.h"
#include "test-name.h"

#include "str.h"

static testresult_t test_str_from_num_thousands(int32_t n, const char *expect)
{
	test_set_name("%s (n == %d)", test_get_name(), n);

	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(n, buf);

	// Assert
	ASSERT_EQUAL_P(result, &buf[0]);
	ASSERT_EQUAL_S(result, expect);
	RETURN_PASS;
}

/* ------------------------------------------------------------------------ */

static testresult_t test_str_concat(const char *arg0, const char *arg1, const char *arg2, const char *arg3, const char *arg4, const char *expected)
{
	test_set_name("%s(%s, %s, %s, %s, %s)",
		test_get_name(),
		test_format_string(arg0),
		test_format_string(arg1),
		test_format_string(arg2),
		test_format_string(arg3),
		test_format_string(arg4));

	// Arrange
	size_t i;
	char *r;

	// Act
	r = str_concat(arg0, arg1, arg2, arg3, arg4, (char *)NULL);

	// Assert
	ASSERT_NOT_NULL(r);
	ASSERT_EQUAL_S(r, expected);

	free(r);

	RETURN_PASS;
}

/* ------------------------------------------------------------------------ */

static testresult_t test_str_from_num(int digits, uint32_t num, const char *expected_result)
{
	// Arrange
	char buf[11];

	// Act
	str_from_num(digits, num, buf);

	// Assert
	ASSERT_EQUAL_S(buf, expected_result);

	RETURN_PASS;
}

static testresult_t test_str_from_num_signed(int digits, int32_t num, const char *expected_result)
{
	// Arrange
	char buf[12];

	// Act
	str_from_num_signed(digits, num, buf);

	// Assert
	ASSERT_EQUAL_S(buf, expected_result);

	RETURN_PASS;
}

/* ------------------------------------------------------------------------ */

static testresult_t test_str_get_num_lines(const char *text, int expected_result)
{
	// Arrange
	int x;

	// Act
	x = str_get_num_lines(text);

	// Assert
	ASSERT_EQUAL_D(x, expected_result);

	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_0(void)
{
	return test_str_from_num_thousands(0, "0");
}

testresult_t test_str_from_num_thousands_999(void)
{
	return test_str_from_num_thousands(999, "999");
}

testresult_t test_str_from_num_thousands_1_000(void)
{
	return test_str_from_num_thousands(1000, "1,000");
}

testresult_t test_str_from_num_thousands_999_999(void)
{
	return test_str_from_num_thousands(999999, "999,999");
}

testresult_t test_str_from_num_thousands_1_000_000(void)
{
	return test_str_from_num_thousands(1000000, "1,000,000");
}

testresult_t test_str_from_num_thousands_999_999_999(void)
{
	return test_str_from_num_thousands(999999999, "999,999,999");
}

testresult_t test_str_from_num_thousands_1_000_000_000(void)
{
	return test_str_from_num_thousands(1000000000, "1,000,000,000");
}

testresult_t test_str_from_num_thousands_int32_min(void)
{
	return test_str_from_num_thousands(INT32_MIN, "-2,147,483,648");
}

testresult_t test_str_from_num_thousands_int32_max(void)
{
	return test_str_from_num_thousands(INT32_MAX, "2,147,483,647");
}

testresult_t test_str_from_num_thousands_digits_1_positive(void)
{
	return test_str_from_num_thousands(7, "7");
}

testresult_t test_str_from_num_thousands_digits_2_positive(void)
{
	return test_str_from_num_thousands(32, "32");
}

testresult_t test_str_from_num_thousands_digits_3_positive(void)
{
	return test_str_from_num_thousands(891, "891");
}

testresult_t test_str_from_num_thousands_digits_4_positive(void)
{
	return test_str_from_num_thousands(5834, "5,834");
}

testresult_t test_str_from_num_thousands_digits_5_positive(void)
{
	return test_str_from_num_thousands(45891, "45,891");
}

testresult_t test_str_from_num_thousands_digits_6_positive(void)
{
	return test_str_from_num_thousands(591848, "591,848");
}

testresult_t test_str_from_num_thousands_digits_7_positive(void)
{
	return test_str_from_num_thousands(5691834, "5,691,834");
}

testresult_t test_str_from_num_thousands_digits_8_positive(void)
{
	return test_str_from_num_thousands(68917829, "68,917,829");
}

testresult_t test_str_from_num_thousands_digits_9_positive(void)
{
	return test_str_from_num_thousands(591848968, "591,848,968");
}

testresult_t test_str_from_num_thousands_digits_10_positive(void)
{
	return test_str_from_num_thousands(2082850450, "2,082,850,450");
}

testresult_t test_str_from_num_thousands_digits_1_negative(void)
{
	return test_str_from_num_thousands(-7, "-7");
}

testresult_t test_str_from_num_thousands_digits_2_negative(void)
{
	return test_str_from_num_thousands(-32, "-32");
}

testresult_t test_str_from_num_thousands_digits_3_negative(void)
{
	return test_str_from_num_thousands(-891, "-891");
}

testresult_t test_str_from_num_thousands_digits_4_negative(void)
{
	return test_str_from_num_thousands(-5834, "-5,834");
}

testresult_t test_str_from_num_thousands_digits_5_negative(void)
{
	return test_str_from_num_thousands(-45891, "-45,891");
}

testresult_t test_str_from_num_thousands_digits_6_negative(void)
{
	return test_str_from_num_thousands(-591848, "-591,848");
}

testresult_t test_str_from_num_thousands_digits_7_negative(void)
{
	return test_str_from_num_thousands(-5691834, "-5,691,834");
}

testresult_t test_str_from_num_thousands_digits_8_negative(void)
{
	return test_str_from_num_thousands(-68917829, "-68,917,829");
}

testresult_t test_str_from_num_thousands_digits_9_negative(void)
{
	return test_str_from_num_thousands(-591848968, "-591,848,968");
}

testresult_t test_str_from_num_thousands_digits_10_negative(void)
{
	return test_str_from_num_thousands(-2082850450, "-2,082,850,450");
}

/* ------------------------------------------------------------------------ */

testresult_t test_str_concat_123456789012(void)
{
	return test_str_concat("123", "456", "789", "012", NULL, "123456789012");
}

testresult_t test_str_concat_123ok67(void)
{
	return test_str_concat("123", "ok", "67", NULL, NULL, "123ok67");
}

/* ------------------------------------------------------------------------ */

testresult_t test_str_from_num_1(void)
{
	return test_str_from_num(0, UINT32_C(1),          "1");
}

testresult_t test_str_from_num_10(void)
{
	return test_str_from_num(0, UINT32_C(10),         "10");
}

testresult_t test_str_from_num_100(void)
{
	return test_str_from_num(0, UINT32_C(100),        "100");
}

testresult_t test_str_from_num_1000(void)
{
	return test_str_from_num(0, UINT32_C(1000),       "1000");
}

testresult_t test_str_from_num_10000(void)
{
	return test_str_from_num(0, UINT32_C(10000),      "10000");
}

testresult_t test_str_from_num_100000(void)
{
	return test_str_from_num(0, UINT32_C(100000),     "100000");
}

testresult_t test_str_from_num_1000000(void)
{
	return test_str_from_num(0, UINT32_C(1000000),    "1000000");
}

testresult_t test_str_from_num_10000000(void)
{
	return test_str_from_num(0, UINT32_C(10000000),   "10000000");
}

testresult_t test_str_from_num_100000000(void)
{
	return test_str_from_num(0, UINT32_C(100000000),  "100000000");
}

testresult_t test_str_from_num_1000000000(void)
{
	return test_str_from_num(0, UINT32_C(1000000000), "1000000000");
}

testresult_t test_str_from_num_01(void)
{
	return test_str_from_num(2, UINT32_C(1),          "01");
}

testresult_t test_str_from_num_0005(void)
{
	return test_str_from_num(4, UINT32_C(5),          "0005");
}

testresult_t test_str_from_num_000000009(void)
{
	return test_str_from_num(9, UINT32_C(9),          "000000009");
}


testresult_t test_str_from_num_signed_1(void)
{
	return test_str_from_num_signed(0, INT32_C(1),          "1");
}

testresult_t test_str_from_num_signed_10(void)
{
	return test_str_from_num_signed(0, INT32_C(10),         "10");
}

testresult_t test_str_from_num_signed_100(void)
{
	return test_str_from_num_signed(0, INT32_C(100),        "100");
}

testresult_t test_str_from_num_signed_1000(void)
{
	return test_str_from_num_signed(0, INT32_C(1000),       "1000");
}

testresult_t test_str_from_num_signed_10000(void)
{
	return test_str_from_num_signed(0, INT32_C(10000),      "10000");
}

testresult_t test_str_from_num_signed_100000(void)
{
	return test_str_from_num_signed(0, INT32_C(100000),     "100000");
}

testresult_t test_str_from_num_signed_1000000(void)
{
	return test_str_from_num_signed(0, INT32_C(1000000),    "1000000");
}

testresult_t test_str_from_num_signed_10000000(void)
{
	return test_str_from_num_signed(0, INT32_C(10000000),   "10000000");
}

testresult_t test_str_from_num_signed_100000000(void)
{
	return test_str_from_num_signed(0, INT32_C(100000000),  "100000000");
}

testresult_t test_str_from_num_signed_1000000000(void)
{
	return test_str_from_num_signed(0, INT32_C(1000000000), "1000000000");
}

testresult_t test_str_from_num_signed_01(void)
{
	return test_str_from_num_signed(2, INT32_C(1),          "01");
}

testresult_t test_str_from_num_signed_0005(void)
{
	return test_str_from_num_signed(4, INT32_C(5),          "0005");
}

testresult_t test_str_from_num_signed_000000009(void)
{
	return test_str_from_num_signed(9, INT32_C(9),          "000000009");
}

testresult_t test_str_from_num_signed_neg_09(void)
{
	return test_str_from_num_signed(3, INT32_C(-9),         "-09");
}

testresult_t test_str_from_num_signed_neg_0009(void)
{
	return test_str_from_num_signed(5, INT32_C(-9),         "-0009");
}

testresult_t test_str_from_num_signed_neg_000000009(void)
{
	return test_str_from_num_signed(9, INT32_C(-9),         "-00000009");
}

/* ------------------------------------------------------------------------ */

testresult_t test_str_get_num_lines_1(void)
{
	return test_str_get_num_lines("wow\n", 1);
}

testresult_t test_str_get_num_lines_2(void)
{
	return test_str_get_num_lines("wow", 0 /* wait, what? */);
}

testresult_t test_str_get_num_lines_3(void)
{
	return test_str_get_num_lines("wow\r\n", 1);
}

testresult_t test_str_get_num_lines_4(void)
{
	return test_str_get_num_lines("wow\r\nhai\n", 2);
}

testresult_t test_str_get_num_lines_5(void)
{
	return test_str_get_num_lines("nice\n\r\n", 2);
}

testresult_t test_str_get_num_lines_6(void)
{
	return test_str_get_num_lines("awesome\r\n\n", 2);
}

/* TODO test the rest of the str functions */