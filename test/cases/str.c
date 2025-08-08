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

testresult_t test_str_from_num_thousands_0(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(0, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "0") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_999(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(999, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "999") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_1_000(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(1000, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "1,000") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_999_999(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(999999, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "999,999") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_1_000_000(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(1000000, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "1,000,000") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_999_999_999(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(999999999, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "999,999,999") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_1_000_000_000(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(1000000000, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "1,000,000,000") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_int32_min(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(INT32_MIN, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "-2,147,483,648") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_int32_max(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(INT32_MAX, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "2,147,483,647") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_1_positive(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(7, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "7") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_2_positive(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(32, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "32") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_3_positive(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(891, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "891") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_4_positive(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(5834, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "5,834") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_5_positive(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(45891, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "45,891") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_6_positive(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(591848, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "591,848") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_7_positive(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(5691834, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "5,691,834") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_8_positive(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(68917829, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "68,917,829") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_9_positive(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(591848968, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "591,848,968") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_10_positive(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(2082850450, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "2,082,850,450") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_1_negative(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(-7, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "-7") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_2_negative(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(-32, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "-32") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_3_negative(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(-891, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "-891") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_4_negative(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(-5834, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "-5,834") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_5_negative(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(-45891, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "-45,891") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_6_negative(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(-591848, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "-591,848") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_7_negative(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(-5691834, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "-5,691,834") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_8_negative(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(-68917829, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "-68,917,829") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_9_negative(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(-591848968, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "-591,848,968") == 0);
	RETURN_PASS;
}

testresult_t test_str_from_num_thousands_digits_10_negative(void)
{
	// Arrange
	char buf[15];

	// Act
	char *result = str_from_num_thousands(-2082850450, buf);

	// Assert
	ASSERT(result == &buf[0]);
	ASSERT(strcmp(result, "-2,082,850,450") == 0);
	RETURN_PASS;
}
