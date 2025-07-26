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

#include <stdlib.h>
#include <string.h>

testresult_t test_bshift_arithmetic();
testresult_t test_bshift_right_shift_negative();
testresult_t test_bshift_left_shift_overflow();

#define CASE(test_case_name) { #test_case_name, test_case_name }
#define END_OF_LIST { 0, 0 }

test_index_entry automated_tests[] =
	{
		CASE(test_bshift_arithmetic),
		CASE(test_bshift_right_shift_negative),
		CASE(test_bshift_left_shift_overflow),

		END_OF_LIST
	};

test_index_entry *test_get_case(const char *name)
{
	int i;

	for (i = 0; automated_tests[i].name; i++)
		if (!strcmp(automated_tests[i].name, name))
			return &automated_tests[i];

	return NULL;
}
