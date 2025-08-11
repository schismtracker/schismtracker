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
	ASSERT(strcmp(result, expect) == 0);
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

		ASSERT(!strcmp(c[i].expected, r));

		free(r);
	}

	RETURN_PASS;
}
