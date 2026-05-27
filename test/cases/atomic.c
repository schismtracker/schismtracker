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

#include "headers.h"
#include "test.h"
#include "test-assertions.h"

#include "atomic.h"

/* NOTE: We only test the behavior of atomic functions here.
 * Unit-testing atomicity is impossible. We could maybe *fuzz*,
 * but atomicity isn't something you can test for. */

testresult_t test_atm_cmpxchg(void)
{
	struct atm a;

	atm_store(&a, 0);

	ASSERT(atm_cmpxchg(&a, 0, 1));
	ASSERT(!atm_cmpxchg(&a, 0, 1));
	ASSERT(atm_load(&a) == 1);

	RETURN_PASS;
}

testresult_t test_atm_load_store(void)
{
	struct atm a;

	atm_store(&a, 67);

	ASSERT(atm_load(&a) == 67);

	RETURN_PASS;
}

testresult_t test_atm_add(void)
{
	struct atm a;

	atm_store(&a, 0);
	ASSERT(atm_add(&a, 10) == 0);
	ASSERT(atm_add(&a, 10) == 10);
	ASSERT(atm_load(&a) == 20);

	RETURN_PASS;
}
