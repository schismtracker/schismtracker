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

#ifndef SCHISM_TEST_H_
#define SCHISM_TEST_H_

#include "headers.h"

/* ------------------------------------------------------------------------ */

/* numerically, FAIL == false and PASS == true */
typedef enum {
	/* weird, "pass" should usually be a 0 return value.
	 * whatever, I guess it's fine for now? */
	SCHISM_TESTRESULT_FAIL,
	SCHISM_TESTRESULT_PASS,
	SCHISM_TESTRESULT_INCONCLUSIVE,
	SCHISM_TESTRESULT_SKIP,
	SCHISM_TESTRESULT_CRASH,
	SCHISM_TESTRESULT_NOT_RUN,
} testresult_t;

/* See testresult.c / testresult_str */
#define TESTRESULT_STR_MAX_LEN 12

/* fit within 80 columns */
#define TESTRESULT_MAX_LINE_LEN (79)

typedef testresult_t (*testfunctor_t)(void);

typedef struct {
	const char *name;
	testfunctor_t test;
} test_index_entry;

/* not sure if I like this being a global; whatever, it's fine for now */
extern test_index_entry automated_tests[];

test_index_entry *test_get_case(const char *name);

const char *testresult_str(testresult_t result);

/* ------------------------------------------------------------------------ */
/* logging functions */

/* frees any existing buffer, and initializes a new one.
 * this must be called before any logging is done. */
void test_log_clear(void);

/* prints a string of known length to the log */
void test_logn(const char *str, int len);

/* prints a string of unknown length to the log */
void test_log(const char *str);

/* snprintf() wrapper for the memory buffer */
void test_log_vprintf(const char *fmt, va_list ap);
void test_log_printf(const char *fmt, ...);

void test_log_dump(void);

/* ------------------------------------------------------------------------ */
/* entrypoint takeover */

int schism_test_main(int argc, char *argv[]);

#ifdef SCHISM_TEST_BUILD
# define ENTRYPOINT schism_test_main
#else
# define ENTRYPOINT schism_main
#endif

/* declare functions */
#include "test-funcs.h"

/* make sure we don't overflow the terminal */
#define TEST_FUNC(x) \
	SCHISM_STATIC_ASSERT((6 + ARRAY_SIZE(#x) + TESTRESULT_STR_MAX_LEN) < TESTRESULT_MAX_LINE_LEN, \
		"function name cannot overflow the terminal");

#include "test-funcs.h"

#endif /* SCHISM_TEST_H_ */
