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

/* assert helper */

#include "test.h"
#include "test-assertions.h"
#include "test-name.h"

void test_assert(const char *file, long line, const char *cond, const char *msg, const char *fmt, ...)
{
	va_list ap;

	test_log_printf("%s (%s:%ld): %s: %s\n", test_get_name(), file, line, msg, cond);

	if (fmt) {
		va_start(ap, fmt);
		test_log_vprintf(fmt, ap);
		va_end(ap);
		test_log_printf("\n");
	}

	test_log_printf("\n");
}

#define ONE 1
#define TWO 2
#define THREE 3

static char test_str[] = "test";
static char test_str_2[] = "test";
static char case_str[] = "case";

static testresult_t exercise_assertions_pass(void)
{
	REQUIRE_TRUE(1);
	ASSERT_TRUE(1);
	REQUIRE_FALSE(0);
	ASSERT_FALSE(0);
	REQUIRE_EQUAL_SD(3, 3);
	ASSERT_EQUAL_SD(4, 4);
	REQUIRE_EQUAL_D(5, 5);
	ASSERT_EQUAL_D(6, 6);
	REQUIRE_EQUAL_LD(0x700000000, 0x700000000);
	ASSERT_EQUAL_LD(0x800000000, 0x800000000);
	REQUIRE_EQUAL_SU(9, 9);
	ASSERT_EQUAL_SU(10, 10);
	REQUIRE_EQUAL_U(11, 11);
	ASSERT_EQUAL_U(12, 12);
	REQUIRE_EQUAL_LU(0xD00000000, 0xD00000000);
	ASSERT_EQUAL_LU(0xE00000000, 0xE00000000);
	REQUIRE_EQUAL_SX(15, 15);
	ASSERT_EQUAL_SX(16, 16);
	REQUIRE_EQUAL_X(17, 17);
	ASSERT_EQUAL_X(18, 18);
	REQUIRE_EQUAL_LX(0x1919191919, 0x1919191919);
	ASSERT_EQUAL_LX(0x1A1A1A1A1A, 0x1A1A1A1A1A);
	REQUIRE_EQUAL_SD_NAMED(1, ONE);
	ASSERT_EQUAL_SD_NAMED(1, ONE);
	REQUIRE_EQUAL_D_NAMED(2, TWO);
	ASSERT_EQUAL_D_NAMED(2, TWO);
	REQUIRE_EQUAL_LD_NAMED(3, THREE);
	ASSERT_EQUAL_LD_NAMED(3, THREE);
	REQUIRE_EQUAL_SU_NAMED(1, ONE);
	ASSERT_EQUAL_SU_NAMED(1, ONE);
	REQUIRE_EQUAL_U_NAMED(2, TWO);
	ASSERT_EQUAL_U_NAMED(2, TWO);
	REQUIRE_EQUAL_LU_NAMED(3, THREE);
	ASSERT_EQUAL_LU_NAMED(3, THREE);
	REQUIRE_EQUAL_SX_NAMED(1, ONE);
	ASSERT_EQUAL_SX_NAMED(1, ONE);
	REQUIRE_EQUAL_X_NAMED(2, TWO);
	ASSERT_EQUAL_X_NAMED(2, TWO);
	REQUIRE_EQUAL_LX_NAMED(3, THREE);
	ASSERT_EQUAL_LX_NAMED(3, THREE);
	REQUIRE_EQUAL_S(test_str, test_str_2);
	ASSERT_EQUAL_S(test_str, test_str_2);
	REQUIRE_NOT_EQUAL_S(test_str, case_str);
	ASSERT_NOT_EQUAL_S(test_str, case_str);
	REQUIRE_STRLEN(test_str, 4);
	ASSERT_STRLEN(test_str, 4);
	REQUIRE_EQUAL_P(test_str, test_str);
	ASSERT_EQUAL_P(test_str, test_str);
	REQUIRE_NOT_EQUAL_P(test_str, test_str_2);
	ASSERT_NOT_EQUAL_P(test_str, test_str_2);
	REQUIRE_NOT_NULL(test_str);
	ASSERT_NOT_NULL(test_str);
	REQUIRE_NULL(NULL);
	ASSERT_NULL(NULL);
	RETURN_PASS;
}

static testresult_t exercise_assertions_fail(int test_index)
{
	switch (test_index)	{
	case 0: REQUIRE_TRUE(0); break;
	case 1: ASSERT_TRUE(0); break;
	case 2: REQUIRE_FALSE(1); break;
	case 3: ASSERT_FALSE(1); break;
	case 4: REQUIRE_EQUAL_SD(3, 4); break;
	case 5: ASSERT_EQUAL_SD(4, 5); break;
	case 6: REQUIRE_EQUAL_D(5, 6); break;
	case 7: ASSERT_EQUAL_D(6, 7); break;
	case 8: REQUIRE_EQUAL_LD(0x700000000, 0x800000000); break;
	case 9: ASSERT_EQUAL_LD(0x800000000, 0x900000000); break;
	case 10: REQUIRE_EQUAL_SU(9, 10); break;
	case 11: ASSERT_EQUAL_SU(10, 11); break;
	case 12: REQUIRE_EQUAL_U(11, 12); break;
	case 13: ASSERT_EQUAL_U(12, 13); break;
	case 14: REQUIRE_EQUAL_LU(0xD00000000, 0xE00000000); break;
	case 15: ASSERT_EQUAL_LU(0xE00000000, 0xF00000000); break;
	case 16: REQUIRE_EQUAL_SX(15, 16); break;
	case 17: ASSERT_EQUAL_SX(16, 17); break;
	case 18: REQUIRE_EQUAL_X(17, 18); break;
	case 19: ASSERT_EQUAL_X(18, 19); break;
	case 20: REQUIRE_EQUAL_LX(0x1919191919, 0x1A19191919); break;
	case 21: ASSERT_EQUAL_LX(0x1A1A1A1A1A, 0x1B1A1A1A1A); break;
	case 22: REQUIRE_EQUAL_SD_NAMED(2, ONE); break;
	case 23: ASSERT_EQUAL_SD_NAMED(2, ONE); break;
	case 24: REQUIRE_EQUAL_D_NAMED(3, TWO); break;
	case 25: ASSERT_EQUAL_D_NAMED(3, TWO); break;
	case 26: REQUIRE_EQUAL_LD_NAMED(4, THREE); break;
	case 27: ASSERT_EQUAL_LD_NAMED(4, THREE); break;
	case 28: REQUIRE_EQUAL_SU_NAMED(2, ONE); break;
	case 29: ASSERT_EQUAL_SU_NAMED(2, ONE); break;
	case 30: REQUIRE_EQUAL_U_NAMED(3, TWO); break;
	case 31: ASSERT_EQUAL_U_NAMED(3, TWO); break;
	case 32: REQUIRE_EQUAL_LU_NAMED(4, THREE); break;
	case 33: ASSERT_EQUAL_LU_NAMED(4, THREE); break;
	case 34: REQUIRE_EQUAL_SX_NAMED(2, ONE); break;
	case 35: ASSERT_EQUAL_SX_NAMED(2, ONE); break;
	case 36: REQUIRE_EQUAL_X_NAMED(3, TWO); break;
	case 37: ASSERT_EQUAL_X_NAMED(3, TWO); break;
	case 38: REQUIRE_EQUAL_LX_NAMED(4, THREE); break;
	case 39: ASSERT_EQUAL_LX_NAMED(4, THREE); break;
	case 40: REQUIRE_EQUAL_S(test_str, case_str); break;
	case 41: ASSERT_EQUAL_S(test_str, case_str); break;
	case 42: REQUIRE_NOT_EQUAL_S(test_str, test_str_2); break;
	case 43: ASSERT_NOT_EQUAL_S(test_str, test_str_2); break;
	case 44: REQUIRE_STRLEN(test_str, 5); break;
	case 45: ASSERT_STRLEN(test_str, 5); break;
	case 46: REQUIRE_EQUAL_P(test_str, test_str_2); break;
	case 47: ASSERT_EQUAL_P(test_str, test_str_2); break;
	case 48: REQUIRE_NOT_EQUAL_P(test_str, test_str); break;
	case 49: ASSERT_NOT_EQUAL_P(test_str, test_str); break;
	case 50: REQUIRE_NOT_NULL(NULL); break;
	case 51: ASSERT_NOT_NULL(NULL); break;
	case 52: REQUIRE_NULL(test_str); break;
	case 53: ASSERT_NULL(test_str); break;
	}

	printf("ERROR: assertion with index %d failed to return on assertion failure", test_index);
	RETURN_FAIL;
}

void exercise_assertions(void)
{
	test_set_name("exercise_assertions");

	test_log_clear();

	exercise_assertions_pass();

	for (int i=0; i < 54; i++)
		exercise_assertions_fail(i);
}