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

#include "str.h"

static testresult_t test_str_from_num_thousands(int32_t n, const char *expect)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(n, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT_PRINTF(strcmp(result, expect) == 0,
		"result %s was not %s as expected for %" PRId32, result, expect, n);
	RETURN_PASS;
}

#define TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(name, num, expect) \
	TEST_CASE_STUB(str_from_num_thousands_##name, test_str_from_num_thousands, num, expect)

/* wow, sublime text totally fails at syntax highlighting here */
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(0, INT32_C(0), "0")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(999, INT32_C(999), "999")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(1_000, INT32_C(1000), "1,000")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(999_999, INT32_C(999999), "999,999")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(1_000_000, INT32_C(1000000), "1,000,000")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(999_999_999, INT32_C(999999999), "999,999,999")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(1_000_000_000, INT32_C(1000000000), "1,000,000,000")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(int32_min, INT32_C(INT32_MIN), "-2,147,483,648")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(int32_max, INT32_C(INT32_MAX), "2,147,483,647")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_1_positive, INT32_C(7), "7")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_2_positive, INT32_C(32), "32")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_3_positive, INT32_C(891), "891")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_4_positive, INT32_C(5834), "5,834")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_5_positive, INT32_C(45891), "45,891")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_6_positive, INT32_C(591848), "591,848")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_7_positive, INT32_C(5691834), "5,691,834")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_8_positive, INT32_C(68917829), "68,917,829")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_9_positive, INT32_C(591848968), "591,848,968")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_10_positive, INT32_C(2082850450), "2,082,850,450")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_1_negative, INT32_C(-7), "-7")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_2_negative, INT32_C(-32), "-32")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_3_negative, INT32_C(-891), "-891")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_4_negative, INT32_C(-5834), "-5,834")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_5_negative, INT32_C(-45891), "-45,891")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_6_negative, INT32_C(-591848), "-591,848")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_7_negative, INT32_C(-5691834), "-5,691,834")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_8_negative, INT32_C(-68917829), "-68,917,829")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_9_negative, INT32_C(-591848968), "-591,848,968")
TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE(digits_10_negative, INT32_C(-2082850450), "-2,082,850,450")

#undef TEST_STR_FROM_NUM_THOUSANDS_TEMPLATE

/* ------------------------------------------------------------------------ */

static testresult_t test_str_concat_common(const char *expected,
	const char *arg1, const char *arg2, const char *arg3, const char *arg4,
	const char *arg5)
{
	char *r;

	r = str_concat(arg1, arg2, arg3, arg4, arg5, (char *)NULL);
	REQUIRE(r);

	ASSERT_PRINTF(!strcmp(expected, r), "result \"%s\" with args "
		"(\"%s\", \"%s\", \"%s\", \"%s\", \"%s\") was not \"%s\" as expected",
		r,
		(arg1) ? (arg1) : "NULL",
		(arg2) ? (arg2) : "NULL",
		(arg3) ? (arg3) : "NULL",
		(arg4) ? (arg4) : "NULL",
		(arg5) ? (arg5) : "NULL",
		expected);

	free(r);

	RETURN_PASS;
}

#define TEST_STR_CONCAT_TEMPLATE(name, expect, arg1, arg2, arg3, arg4, arg5) \
    TEST_CASE_STUB(str_concat_##name, test_str_concat_common, expect, arg1, arg2, arg3, arg4, arg5)

TEST_STR_CONCAT_TEMPLATE(123456789012, "123456789012", "123", "456", "789", "012", NULL)
TEST_STR_CONCAT_TEMPLATE(123ok67, "123ok67", "123", "ok", "67", NULL, NULL)

#undef TEST_STR_CONCAT_TEMPLATE

/* ------------------------------------------------------------------------ */

static testresult_t test_str_from_num_common(int digits, uint32_t num,
	const char *expect)
{
	char buf[11];
	char *r;

	r = str_from_num(digits, num, buf);

	ASSERT_PRINTF(r == &buf[0], "return value \"%p\" is not the same "
		"as input pointer \"%p\"", r, buf);
	ASSERT_PRINTF(!strcmp(expect, buf), "result with integer %" PRIu32
		" was \"%s\", not \"%s\" as expected", num, buf, expect);

	RETURN_PASS;
}

#define TEST_STR_FROM_NUM_TEMPLATE(name, digits, num, expect) \
    TEST_CASE_STUB(str_from_num_##name, test_str_from_num_common, digits, num, expect)

TEST_STR_FROM_NUM_TEMPLATE(0_1, 0, UINT32_C(1),          "1")
TEST_STR_FROM_NUM_TEMPLATE(0_10, 0, UINT32_C(10),         "10")
TEST_STR_FROM_NUM_TEMPLATE(0_100, 0, UINT32_C(100),        "100")
TEST_STR_FROM_NUM_TEMPLATE(0_1000, 0, UINT32_C(1000),       "1000")
TEST_STR_FROM_NUM_TEMPLATE(0_10000, 0, UINT32_C(10000),      "10000")
TEST_STR_FROM_NUM_TEMPLATE(0_100000, 0, UINT32_C(100000),     "100000")
TEST_STR_FROM_NUM_TEMPLATE(0_1000000, 0, UINT32_C(1000000),    "1000000")
TEST_STR_FROM_NUM_TEMPLATE(0_10000000, 0, UINT32_C(10000000),   "10000000")
TEST_STR_FROM_NUM_TEMPLATE(0_100000000, 0, UINT32_C(100000000),  "100000000")
TEST_STR_FROM_NUM_TEMPLATE(0_1000000000, 0, UINT32_C(1000000000), "1000000000")
TEST_STR_FROM_NUM_TEMPLATE(2_1, 2, UINT32_C(1),          "01")
TEST_STR_FROM_NUM_TEMPLATE(4_5, 4, UINT32_C(5),          "0005")
TEST_STR_FROM_NUM_TEMPLATE(9_9, 9, UINT32_C(9),          "000000009")

#undef TEST_STR_FROM_NUM_TEMPLATE

/* ------------------------------------------------------------------------ */

static testresult_t test_str_from_num_signed_common(int digits, int32_t num,
	const char *expect)
{
	char buf[12];
	char *r;

	r = str_from_num_signed(digits, num, buf);

	ASSERT_PRINTF(r == &buf[0], "return value \"%p\" is not the same "
		"as input pointer \"%p\"", r, buf);
	ASSERT_PRINTF(!strcmp(expect, buf), "result with integer %" PRId32
		" was \"%s\", not \"%s\" as expected", num, buf, expect);

	RETURN_PASS;
}

#define TEST_STR_FROM_NUM_SIGNED_TEMPLATE(name, digits, num, expect) \
    TEST_CASE_STUB(str_from_num_signed_##name, \
    	test_str_from_num_signed_common, digits, num, expect)

TEST_STR_FROM_NUM_SIGNED_TEMPLATE(0_1, 0, INT32_C(1),          "1")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(0_10, 0, INT32_C(10),         "10")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(0_100, 0, INT32_C(100),        "100")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(0_1000, 0, INT32_C(1000),       "1000")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(0_10000, 0, INT32_C(10000),      "10000")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(0_100000, 0, INT32_C(100000),     "100000")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(0_1000000, 0, INT32_C(1000000),    "1000000")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(0_10000000, 0, INT32_C(10000000),   "10000000")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(0_100000000, 0, INT32_C(100000000),  "100000000")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(0_1000000000, 0, INT32_C(1000000000), "1000000000")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(2_1, 2, INT32_C(1),          "01")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(4_5, 4, INT32_C(5),          "0005")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(9_9, 9, INT32_C(9),          "000000009")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(3_neg9, 3, INT32_C(-9),         "-09")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(5_neg9, 5, INT32_C(-9),         "-0009")
TEST_STR_FROM_NUM_SIGNED_TEMPLATE(9_neg9, 9, INT32_C(-9),         "-00000009")

#undef TEST_STR_FROM_NUM_SIGNED_TEMPLATE

/* ------------------------------------------------------------------------ */

static testresult_t test_str_get_num_lines_common(const char *text,
	int result)
{
	int x;

	x = str_get_num_lines(text);

	ASSERT_PRINTF(x == result, "result for string \"%s\" was %d, not %d as expected", text, x, result);

	RETURN_PASS;
}

#define TEST_STR_GET_NUM_LINES_TEMPLATE(name, text, result) \
    TEST_CASE_STUB(str_get_num_lines_##name, \
    	test_str_get_num_lines_common, text, result)

TEST_STR_GET_NUM_LINES_TEMPLATE(wow_LF, "wow\n", 1)
TEST_STR_GET_NUM_LINES_TEMPLATE(wow, "wow", 0 /* wait, what? */)
TEST_STR_GET_NUM_LINES_TEMPLATE(wow_CRLF, "wow\r\n", 1)
TEST_STR_GET_NUM_LINES_TEMPLATE(wow_CRLF_hai_LF, "wow\r\nhai\n", 2)
TEST_STR_GET_NUM_LINES_TEMPLATE(nice_LF_CRLF, "nice\n\r\n", 2)
TEST_STR_GET_NUM_LINES_TEMPLATE(awesome_CRLF_LF, "awesome\r\n\n", 2)
TEST_STR_GET_NUM_LINES_TEMPLATE(awesome_CR_hai_LF, "awesome\rhai\n", 2)

#undef TEST_STR_GET_NUM_LINES_TEMPLATE

/* TODO test the rest of the str functions */