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

testresult_t test_str_concat(void)
{
	static const struct {
		const char *args[5];
		const char *expected;
	} c[] = {
		{{"123", "456", "789", "012"}, "123456789012"},
		{{"123", "ok", "67"}, "123ok67"},
	};
	size_t i;
	char *r;

	for (i = 0; i < ARRAY_SIZE(c); i++) {
		r = str_concat(c[i].args[0], c[i].args[1], c[i].args[2], c[i].args[3], c[i].args[4], (char *)NULL);
		REQUIRE(r);

		ASSERT_PRINTF(!strcmp(c[i].expected, r), "result \"%s\" with args "
			"(\"%s\", \"%s\", \"%s\", \"%s\", \"%s\") was not \"%s\" as expected",
			r,
			c[i].args[0] ? c[i].args[0] : "NULL",
			c[i].args[1] ? c[i].args[1] : "NULL",
			c[i].args[2] ? c[i].args[2] : "NULL",
			c[i].args[3] ? c[i].args[3] : "NULL",
			c[i].args[4] ? c[i].args[4] : "NULL",
			c[i].expected);

		free(r);
	}

	RETURN_PASS;
}

/* ------------------------------------------------------------------------ */

testresult_t test_str_from_num(void)
{
	static const struct {
		int digits;
		uint32_t num;
		const char *result;
	} c[] = {
		{0, UINT32_C(1),          "1"},
		{0, UINT32_C(10),         "10"},
		{0, UINT32_C(100),        "100"},
		{0, UINT32_C(1000),       "1000"},
		{0, UINT32_C(10000),      "10000"},
		{0, UINT32_C(100000),     "100000"},
		{0, UINT32_C(1000000),    "1000000"},
		{0, UINT32_C(10000000),   "10000000"},
		{0, UINT32_C(100000000),  "100000000"},
		{0, UINT32_C(1000000000), "1000000000"},
		{2, UINT32_C(1),          "01"},
		{4, UINT32_C(5),          "0005"},
		{9, UINT32_C(9),          "000000009"},
	};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(c); i++) {
		char buf[11];

		str_from_num(c[i].digits, c[i].num, buf);

		ASSERT_PRINTF(!strcmp(c[i].result, buf), "result with integer %" PRIu32
			" was \"%s\", not \"%s\" as expected", c[i].num, buf, c[i].result);
	}

	RETURN_PASS;
}

testresult_t test_str_from_num_signed(void)
{
	static const struct {
		int digits;
		int32_t num;
		const char *result;
	} c[] = {
		{0, INT32_C(1),          "1"},
		{0, INT32_C(10),         "10"},
		{0, INT32_C(100),        "100"},
		{0, INT32_C(1000),       "1000"},
		{0, INT32_C(10000),      "10000"},
		{0, INT32_C(100000),     "100000"},
		{0, INT32_C(1000000),    "1000000"},
		{0, INT32_C(10000000),   "10000000"},
		{0, INT32_C(100000000),  "100000000"},
		{0, INT32_C(1000000000), "1000000000"},
		{2, INT32_C(1),          "01"},
		{4, INT32_C(5),          "0005"},
		{9, INT32_C(9),          "000000009"},
		{3, INT32_C(-9),         "-09"},
		{5, INT32_C(-9),         "-0009"},
		{9, INT32_C(-9),         "-00000009"},
	};
	size_t i;

	for (i = 0; i < ARRAY_SIZE(c); i++) {
		char buf[12];

		str_from_num_signed(c[i].digits, c[i].num, buf);

		ASSERT_PRINTF(!strcmp(c[i].result, buf), "result with integer %" PRId32
			" was \"%s\", not \"%s\" as expected", c[i].num, buf, c[i].result);
	}

	RETURN_PASS;
}

/* ------------------------------------------------------------------------ */

testresult_t test_str_get_num_lines(void)
{
	static const struct {
		const char *text;
		int result;
	} c[] = {
		/* stress test */
		{"wow\n", 1},
		{"wow", 0 /* wait, what? */},
		{"wow\r\n", 1},
		{"wow\r\nhai\n", 2},
		{"nice\n\r\n", 2},
		{"awesome\r\n\n", 2},
	};

	size_t i;

	for (i = 0; i < ARRAY_SIZE(c); i++) {
		int x;

		x = str_get_num_lines(c[i].text);

		ASSERT_PRINTF(x == c[i].result, "result for string \"%s\" was %d, not %d as expected", c[i].text, x, c[i].result);
	}

	RETURN_PASS;
}

/* TODO test the rest of the str functions */