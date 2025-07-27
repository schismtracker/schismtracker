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
#ifndef SCHISM_AUTOMATED_TESTING_H_
#define SCHISM_AUTOMATED_TESTING_H_

#ifdef SCHISM_TEST_BUILD
#define ENTRYPOINT schism_test_main
#else
#define ENTRYPOINT schism_main
#endif

/* numerically, FAIL == false and PASS == true */
typedef enum {
	SCHISM_TESTRESULT_FAIL,
	SCHISM_TESTRESULT_PASS,
	SCHISM_TESTRESULT_INCONCLUSIVE,
	SCHISM_TESTRESULT_SKIP,
	SCHISM_TESTRESULT_CRASH,
	SCHISM_TESTRESULT_NOT_RUN,
} testresult_t;

// See testresult.c / testresult_str
#define TESTRESULT_STR_MAX_LEN 12

typedef testresult_t (*testfunctor_t)();

typedef struct {
	const char *name;
	testfunctor_t test;
} test_index_entry;

extern test_index_entry automated_tests[];

test_index_entry *test_get_case(const char *name);

const char *testresult_str(testresult_t result);

void test_output_clear(void);
void test_output(const char *str);
void test_printf(const char *fmt, ...);
void test_dump_output(void);

int schism_test_main(int argc, char** argv);

#endif /* SCHISM_AUTOMATED_TESTING_H_ */