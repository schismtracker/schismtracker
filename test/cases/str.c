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

#include "str.f"

/* TODO test the rest of the str functions */
