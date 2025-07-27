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

#include "automated-testing.h"

#include "osdefs.h"
#include "timer.h"
#include "mt.h"

#define STRINGIZE(x) #x

int result_to_exit_code(testresult_t result)
{
	switch (result) {
	case SCHISM_TESTRESULT_PASS: return 0;
	default:
	case SCHISM_TESTRESULT_FAIL: return 32;
	case SCHISM_TESTRESULT_SKIP: return 64;
	case SCHISM_TESTRESULT_INCONCLUSIVE: return 96;
	case SCHISM_TESTRESULT_CRASH: return 1;
	}
}

testresult_t exit_code_to_result(int exit_code)
{
	switch (exit_code) {
	case 0: return SCHISM_TESTRESULT_PASS;
	case 32: return SCHISM_TESTRESULT_FAIL;
	case 64: return SCHISM_TESTRESULT_SKIP;
	case 96: return SCHISM_TESTRESULT_INCONCLUSIVE;
	default: return SCHISM_TESTRESULT_CRASH;
	}
}

static int run_test(test_index_entry *entry)
{
	timer_ticks_t start_time, end_time;
	testresult_t result;
	int i;

	test_output_clear();

	printf("TEST: %s ", entry->name);
	fflush(stdout); // in case the test crashes

	start_time = timer_ticks();
	result = entry->test();
	end_time = timer_ticks();

	for (i = 6 + strlen(entry->name) + 1; i < 78 - TESTRESULT_STR_MAX_LEN; i++)
		fputc('.', stdout);

	printf(" %s (%" PRIu64 " ms)\n", testresult_str(result), end_time - start_time);

	test_dump_output();

	return result;
}

static int inproc_warn = 0;

static testresult_t run_test_child(const char *self, test_index_entry *entry)
{
#ifdef os_exec
	int exit_code = os_shell(self, entry->name);

	return exit_code_to_result(exit_code);
#else
	if (!inproc_warn) {
		fprintf(stderr, "warning: platform does not have a supported exec function, running tests in-process\n");
		fflush(stderr);
		inproc_warn = 1;
	}

	return run_test(entry);
#endif
}

int schism_test_main(int argc, char** argv)
{
	int exit_code;

	mt_init();
	timer_init();

	if (argc <= 1) {
		int passed_tests = 0;
		int failed_tests = 0;
		int i, j;

		for (i = 0; automated_tests[i].name; i++) {
			testresult_t result = run_test_child(argv[0], &automated_tests[i]);

			if (result == SCHISM_TESTRESULT_PASS)
				passed_tests++;
			else {
				failed_tests++;

				// We assume the crash happened during the test case itself, which means
				// we've output the name of the test but not the string of dots leading
				// up to the result.

				if (result == SCHISM_TESTRESULT_CRASH) {
					for (j = 6 + strlen(automated_tests[i].name) + 1; j < 78 - TESTRESULT_STR_MAX_LEN; j++)
						fputc('.', stdout);

					puts(" CRASH");
				}
			}
		}

		printf("Results: %d passed, %d failed\n", passed_tests, failed_tests);

		exit_code = (failed_tests == 0) ? 0 : 1;
	}
	else {
		test_index_entry *test_case = test_get_case(argv[1]);
		testresult_t result;

		if (!test_case) {
			fprintf(stderr, "%s (test build): no such test was found: %s\n", argv[0], argv[1]);
			return 3;
		}

		result = run_test(test_case);

		exit_code = result_to_exit_code(result);
	}

	timer_quit();
	mt_quit();

	return exit_code;
}