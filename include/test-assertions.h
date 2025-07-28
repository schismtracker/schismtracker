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

#include "test.h"

/* TODO prefix these with TEST_ */
#define ASSERT(cond) do { if (!(cond)) { test_log_printf("assertion failed: %s\n", #cond); return SCHISM_TESTRESULT_FAIL; } } while (0)
#define RETURN_PASS return SCHISM_TESTRESULT_PASS
#define RETURN_FAIL return SCHISM_TESTRESULT_FAIL
#define RETURN_INCONCLUSIVE return SCHISM_TESTRESULT_INCONCLUSIVE
#define RETURN_SKIP return SCHISM_TESTRESULT_SKIP

#endif /* SCHISM_TEST_ASSERTIONS_H_ */
