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

#include "version.h"

testresult_t test_ver_mktime(void)
{
	uint32_t verl;

#define DATE(ver, y, m, d) \
	verl = ver_mktime((y), (m), (d)); \
	ASSERT_PRINTF((ver) == verl, "%#" PRIx32 " and %#" PRIx32 " (ver_mktime) do not match", ver, verl);
#include "version-values.h"

	RETURN_PASS;
}


testresult_t test_ver_to_date(void)
{
	uint32_t y, m, d;

#define DATE(ver, yv, mv, dv) \
	ASSERT(ver_to_date((ver), &y, &m, &d) == 0); \
	ASSERT_PRINTF(y == (yv) && m == (mv) && d == (dv), "%04" PRIu32 "-%02" PRIu32 "-%02" PRIu32, y, m, d);
#include "version-values.h"

	RETURN_PASS;
}
