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

/* sanity.c: these don't test Schism, and actually just make sure
 * that everything works right. if not, there is probably a bug in
 * the OS or the libc. */

/* test_sanity_time: makes sure that
 *  time() -> localtime() -> mktime()
 * always returns back the same value for time_t */
testresult_t test_sanity_time(void)
{
	time_t x;
	struct tm t;

	time(&x);

	localtime_r(&x, &t);

	if (x != mktime(&t))
		RETURN_FAIL;

	RETURN_PASS;
}