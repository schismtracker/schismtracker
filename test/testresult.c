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

// Allow 12 characters
const char *testresult_str(testresult_t result)
{
	switch (result) {
	case SCHISM_TESTRESULT_NOT_RUN: return "NOT RUN";
	case SCHISM_TESTRESULT_PASS: return "PASS";
	case SCHISM_TESTRESULT_FAIL: return "FAIL";
	case SCHISM_TESTRESULT_INCONCLUSIVE: return "INCONCLUSIVE";
	case SCHISM_TESTRESULT_SKIP: return "SKIP";
	default: return "#UNKNOWN";
	}
}
