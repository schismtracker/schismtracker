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

#ifndef SCHISM_TEST_ASSERTIONS_H_
#define SCHISM_TEST_ASSERTIONS_H_

#include "headers.h"

#include "test.h"

void test_assert(const char *file, long line, const char *cond, const char *msg, const char *fmt, ...);

#ifdef EXERCISE_ASSERTIONS
void exercise_assertions(void);
#endif /* EXERCISE_ASSERTIONS */

#define SCHISM__TEST_ASSERTION(cond, actual, return_value, msg, fmt, ...) \
	do { \
		if (!(cond)) { \
			test_assert(__FILE__, __LINE__, #actual, msg, fmt, __VA_ARGS__); \
			return return_value; \
		} \
	} while (0)

#define SCHISM__TEST_INITIALIZATION_ASSERTION(cond, actual, fmt, ...) \
	SCHISM__TEST_ASSERTION(cond, actual, SCHISM_TESTRESULT_INCONCLUSIVE, "test initialization failed", fmt, __VA_ARGS__)
#define SCHISM__TEST_ASSERTION_ASSERTION(cond, actual, fmt, ...) \
	SCHISM__TEST_ASSERTION(cond, actual, SCHISM_TESTRESULT_FAIL, "assertion failed", fmt, __VA_ARGS__)

/* TODO prefix these with TEST_ */
#define REQUIRE_PRINTF(init_cond, fmt, ...) \
	SCHISM__TEST_INITIALIZATION_ASSERTION(init_cond, init_cond, fmt, __VA_ARGS__)
#define ASSERT_PRINTF(cond, fmt, ...) \
	SCHISM__TEST_ASSERTION_ASSERTION(cond, cond, fmt, __VA_ARGS__)

#define SCHISM__IS_ZERO(x) ((x) == 0)
#define SCHISM__IS_NONZERO(x) ((x) != 0)

#define SCHISM__BOOLEAN_ASSERTION(actual, ASSERTION, CHECK, msg) \
	ASSERTION(CHECK(actual), actual, msg, NULL)

#define REQUIRE_TRUE(actual) SCHISM__BOOLEAN_ASSERTION(actual, SCHISM__TEST_INITIALIZATION_ASSERTION, SCHISM__IS_NONZERO, " expected: true (non-zero)\n actual: false (0)")
#define ASSERT_TRUE(actual) SCHISM__BOOLEAN_ASSERTION(actual, SCHISM__TEST_ASSERTION_ASSERTION, SCHISM__IS_NONZERO, " expected: true (non-zero)\n actual: false (0)")
#define REQUIRE_FALSE(actual) SCHISM__BOOLEAN_ASSERTION(actual, SCHISM__TEST_INITIALIZATION_ASSERTION, SCHISM__IS_ZERO, " expected: false (0)\n actual: true (non-zero)")
#define ASSERT_FALSE(actual) SCHISM__BOOLEAN_ASSERTION(actual, SCHISM__TEST_ASSERTION_ASSERTION, SCHISM__IS_ZERO, " expected: false (0)\n actual: true (non-zero)")

#define SCHISM__EQUALITY_ASSERTION(actual, expected, T, ASSERTION, PRI) \
	do { \
		T a = actual, e = expected; \
		ASSERTION(a == e, actual, " expected: %" PRI "\n actual: %" PRI, e, a); \
	} while (0)

#define REQUIRE_EQUAL_SD(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, int16_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRId16)
#define ASSERT_EQUAL_SD(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, int16_t, SCHISM__TEST_ASSERTION_ASSERTION, PRId16)
#define REQUIRE_EQUAL_D(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, int32_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRId32)
#define ASSERT_EQUAL_D(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, int32_t, SCHISM__TEST_ASSERTION_ASSERTION, PRId32)
#define REQUIRE_EQUAL_LD(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, int64_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRId64)
#define ASSERT_EQUAL_LD(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, int64_t, SCHISM__TEST_ASSERTION_ASSERTION, PRId64)
#define REQUIRE_EQUAL_SU(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, uint16_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRIu16)
#define ASSERT_EQUAL_SU(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, uint16_t, SCHISM__TEST_ASSERTION_ASSERTION, PRIu16)
#define REQUIRE_EQUAL_U(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, uint32_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRIu32)
#define ASSERT_EQUAL_U(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, uint32_t, SCHISM__TEST_ASSERTION_ASSERTION, PRIu32)
#define REQUIRE_EQUAL_LU(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, uint64_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRIu64)
#define ASSERT_EQUAL_LU(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, uint64_t, SCHISM__TEST_ASSERTION_ASSERTION, PRIu64)
#define REQUIRE_EQUAL_SX(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, int16_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRIx16)
#define ASSERT_EQUAL_SX(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, int16_t, SCHISM__TEST_ASSERTION_ASSERTION, PRIx16)
#define REQUIRE_EQUAL_X(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, int32_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRIx32)
#define ASSERT_EQUAL_X(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, int32_t, SCHISM__TEST_ASSERTION_ASSERTION, PRIx32)
#define REQUIRE_EQUAL_LX(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, int64_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRIx64)
#define ASSERT_EQUAL_LX(actual, expected) SCHISM__EQUALITY_ASSERTION(actual, expected, int64_t, SCHISM__TEST_ASSERTION_ASSERTION, PRIx64)

#define SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, T, ASSERTION, PRI) \
	do { \
		T a = actual, e = expected; \
		ASSERTION(a == e, actual, " expected: %s (%" PRI ")\n actual: %" PRI, #expected, e, a); \
	} while (0)

#define REQUIRE_EQUAL_SD_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, int16_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRId16)
#define ASSERT_EQUAL_SD_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, int16_t, SCHISM__TEST_ASSERTION_ASSERTION, PRId16)
#define REQUIRE_EQUAL_D_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, int32_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRId32)
#define ASSERT_EQUAL_D_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, int32_t, SCHISM__TEST_ASSERTION_ASSERTION, PRId32)
#define REQUIRE_EQUAL_LD_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, int64_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRId64)
#define ASSERT_EQUAL_LD_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, int64_t, SCHISM__TEST_ASSERTION_ASSERTION, PRId64)
#define REQUIRE_EQUAL_SU_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, uint16_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRIu16)
#define ASSERT_EQUAL_SU_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, uint16_t, SCHISM__TEST_ASSERTION_ASSERTION, PRIu16)
#define REQUIRE_EQUAL_U_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, uint32_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRIu32)
#define ASSERT_EQUAL_U_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, uint32_t, SCHISM__TEST_ASSERTION_ASSERTION, PRIu32)
#define REQUIRE_EQUAL_LU_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, uint64_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRIu64)
#define ASSERT_EQUAL_LU_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, uint64_t, SCHISM__TEST_ASSERTION_ASSERTION, PRIu64)
#define REQUIRE_EQUAL_SX_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, int16_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRIx16)
#define ASSERT_EQUAL_SX_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, int16_t, SCHISM__TEST_ASSERTION_ASSERTION, PRIx16)
#define REQUIRE_EQUAL_X_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, int32_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRIx32)
#define ASSERT_EQUAL_X_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, int32_t, SCHISM__TEST_ASSERTION_ASSERTION, PRIx32)
#define REQUIRE_EQUAL_LX_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, int64_t, SCHISM__TEST_INITIALIZATION_ASSERTION, PRIx64)
#define ASSERT_EQUAL_LX_NAMED(actual, expected) SCHISM__NAMED_EQUALITY_ASSERTION(actual, expected, int64_t, SCHISM__TEST_ASSERTION_ASSERTION, PRIx64)

#define SCHISM__STRING_EQUALITY_ASSERTION(actual, expected, ASSERTION, CHECK, maybe_not) \
	do { \
		const char *a = actual, *e = expected; \
		if ((a == NULL) && ("x" maybe_not == "x")) \
			ASSERTION(0, actual, " expected: \"%s\"\n actual: NULL", e); \
		else \
			ASSERTION(CHECK(strcmp(a, e)), actual, " expected: " maybe_not "\"%s\"\n actual: \"%s\"", e, a); \
	} while (0)

#define REQUIRE_EQUAL_S(actual, expected) SCHISM__STRING_EQUALITY_ASSERTION(actual, expected, SCHISM__TEST_INITIALIZATION_ASSERTION, SCHISM__IS_ZERO, "")
#define ASSERT_EQUAL_S(actual, expected) SCHISM__STRING_EQUALITY_ASSERTION(actual, expected, SCHISM__TEST_ASSERTION_ASSERTION, SCHISM__IS_ZERO, "")
#define REQUIRE_NOT_EQUAL_S(actual, expected) SCHISM__STRING_EQUALITY_ASSERTION(actual, expected, SCHISM__TEST_INITIALIZATION_ASSERTION, SCHISM__IS_NONZERO, "not ")
#define ASSERT_NOT_EQUAL_S(actual, expected) SCHISM__STRING_EQUALITY_ASSERTION(actual, expected, SCHISM__TEST_ASSERTION_ASSERTION, SCHISM__IS_NONZERO, "not ")

#define SCHISM__STRING_LENGTH_ASSERTION(actual, expected, ASSERTION) \
	do { \
		const char *a_s = actual; \
		int32_t a = a_s ? strlen(a_s) : -1, e = expected; \
		if (a_s == NULL) \
			ASSERTION(0, actual, " expected: %d characters\n actual: NULL", e); \
		else \
			ASSERTION(a == e, actual, " expected: %d characters\n actual: %d characters", e, a); \
	} while (0)

#define REQUIRE_STRLEN(actual, expected) SCHISM__STRING_LENGTH_ASSERTION(actual, expected, SCHISM__TEST_INITIALIZATION_ASSERTION)
#define ASSERT_STRLEN(actual, expected) SCHISM__STRING_LENGTH_ASSERTION(actual, expected, SCHISM__TEST_ASSERTION_ASSERTION)

#define SCHISM__POINTER_EQUALITY_ASSERTION(actual, expected, ASSERTION, CHECK, is_or_should_have_been) \
	do { \
		const void *a = actual, *e = expected; \
		const char *fmt; \
	\
		if (a == NULL) \
			fmt = " actual " is_or_should_have_been " a different pointer than expected (NULL != %p)"; \
		else if (e == NULL) \
			fmt = " actual " is_or_should_have_been " a different pointer than expected (%p != NULL)"; \
		else \
			fmt = " actual " is_or_should_have_been " a different pointer than expected (%p != %p)"; \
	\
		ASSERTION(CHECK(a == e), actual, fmt, a, e); \
	} while (0)

#define REQUIRE_EQUAL_P(actual, expected) SCHISM__POINTER_EQUALITY_ASSERTION(actual, expected, SCHISM__TEST_INITIALIZATION_ASSERTION, SCHISM__IS_NONZERO, "is")
#define ASSERT_EQUAL_P(actual, expected) SCHISM__POINTER_EQUALITY_ASSERTION(actual, expected, SCHISM__TEST_ASSERTION_ASSERTION, SCHISM__IS_NONZERO, "is")
#define REQUIRE_NOT_EQUAL_P(actual, expected) SCHISM__POINTER_EQUALITY_ASSERTION(actual, expected, SCHISM__TEST_INITIALIZATION_ASSERTION, SCHISM__IS_ZERO, "should have been")
#define ASSERT_NOT_EQUAL_P(actual, expected) SCHISM__POINTER_EQUALITY_ASSERTION(actual, expected, SCHISM__TEST_ASSERTION_ASSERTION, SCHISM__IS_ZERO, "should have been")

#define SCHISM__NULLITY_ASSERTION(actual, ASSERTION, CHECK, expected_descr, actual_descr) \
	do { \
		const void *a = actual; \
		ASSERTION(CHECK(actual), actual, " expected: " expected_descr "\n actual: " actual_descr, a); \
	} while (0)

#define REQUIRE_NOT_NULL(actual) SCHISM__NULLITY_ASSERTION(actual, SCHISM__TEST_INITIALIZATION_ASSERTION, SCHISM__IS_NONZERO, "not NULL", "NULL")
#define ASSERT_NOT_NULL(actual) SCHISM__NULLITY_ASSERTION(actual, SCHISM__TEST_ASSERTION_ASSERTION, SCHISM__IS_NONZERO, "not NULL", "NULL")
#define REQUIRE_NULL(actual) SCHISM__NULLITY_ASSERTION(actual, SCHISM__TEST_INITIALIZATION_ASSERTION, SCHISM__IS_ZERO, "NULL", "%p (not NULL)")
#define ASSERT_NULL(actual) SCHISM__NULLITY_ASSERTION(actual, SCHISM__TEST_ASSERTION_ASSERTION, SCHISM__IS_ZERO, "NULL", "%p (not NULL)")

#define REQUIRE(init_cond) \
	REQUIRE_PRINTF(init_cond, NULL, NULL)
#define ASSERT(cond) \
	ASSERT_PRINTF(cond, NULL, NULL)

/* these are dumb */
#define RETURN_PASS return SCHISM_TESTRESULT_PASS
#define RETURN_FAIL return SCHISM_TESTRESULT_FAIL
#define RETURN_INCONCLUSIVE return SCHISM_TESTRESULT_INCONCLUSIVE
#define RETURN_SKIP return SCHISM_TESTRESULT_SKIP

#endif /* SCHISM_TEST_ASSERTIONS_H_ */
