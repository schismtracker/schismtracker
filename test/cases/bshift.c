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

#include "bshift.h"

testresult_t test_bshift_arithmetic(void)
{
#ifdef HAVE_ARITHMETIC_RSHIFT
	ASSERT(rshift_signed(-0xFFFF, 8) == (-0xFFFF >> 8));
	RETURN_PASS;
#else
	RETURN_SKIP;
#endif
}

testresult_t test_bshift_right_shift_negative(void)
{
	ASSERT(rshift_signed(INT32_C(-0xFFFF), 8) == INT32_C(-0x100));
	RETURN_PASS;
}

testresult_t test_bshift_left_shift_overflow(void)
{
	ASSERT(lshift_signed((int32_t)0xFF000000, 4) == (int32_t)0xF0000000);
	RETURN_PASS;
}
