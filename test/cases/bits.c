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

#include "bits.h"

/* ------------------------------------------------------------------------ */
/* portable signed bit shift */

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

/* ------------------------------------------------------------------------ */
/* byteswap */

testresult_t test_bswap_16(void)
{
	ASSERT(bswap_16(0xFEEE) == 0xEEFE);
	RETURN_PASS;
}

testresult_t test_bswap_32(void)
{
	ASSERT(bswap_32(0x12345678) == 0x78563412);
	RETURN_PASS;
}

testresult_t test_bswap_64(void)
{
	ASSERT(bswap_64(0x0123456789ABCDEF) == 0xEFCDAB8967452301);
	RETURN_PASS;
}

/* ------------------------------------------------------------------------ */
/* absolute value */

testresult_t test_babs8(void)
{
	ASSERT(babs8(-1) == 1U);
	ASSERT(babs8(INT8_MIN) == UINT8_C(0x80));

	RETURN_PASS;
}

testresult_t test_babs16(void)
{
	ASSERT(babs16(-1) == 1U);
	ASSERT(babs16(INT16_MIN) == UINT16_C(0x8000));

	RETURN_PASS;
}

testresult_t test_babs32(void)
{
	ASSERT(babs32(-1) == 1);
	ASSERT(babs32(INT32_MIN) == UINT32_C(0x80000000));

	/* XXX whats the point of these "RETURN_*" macros? */
	RETURN_PASS;
}

testresult_t test_babs64(void)
{
	ASSERT(babs64(-1) == 1);
	ASSERT(babs64(INT64_MIN) == UINT64_C(0x8000000000000000));

	/* XXX whats the point of these "RETURN_*" macros? */
	RETURN_PASS;
}

/* ------------------------------------------------------------------------ */
/* unsigned average */

testresult_t test_bavgu8(void)
{
	/* generic */
	ASSERT(bavgu8(0, 2) == 1);
	/* round up */
	ASSERT(bavgu8(1, 2) == 2);
	/* edge case */
	ASSERT(bavgu8(UINT8_MAX, UINT8_MAX) == UINT8_MAX);

	RETURN_PASS;
}

testresult_t test_bavgu16(void)
{
	/* generic */
	ASSERT(bavgu16(0, 2) == 1);
	/* round up */
	ASSERT(bavgu16(1, 2) == 2);
	/* edge case */
	ASSERT(bavgu16(UINT16_MAX, UINT16_MAX) == UINT16_MAX);

	RETURN_PASS;
}

testresult_t test_bavgu32(void)
{
	/* generic */
	ASSERT(bavgu32(0, 2) == 1);
	/* round up */
	ASSERT(bavgu32(1, 2) == 2);
	/* edge case */
	ASSERT(bavgu32(UINT32_MAX, UINT32_MAX) == UINT32_MAX);

	RETURN_PASS;
}

testresult_t test_bavgu64(void)
{
	/* generic */
	ASSERT(bavgu64(0, 2) == 1);
	/* round up */
	ASSERT(bavgu64(1, 2) == 2);
	/* edge case */
	ASSERT(bavgu64(UINT64_MAX, UINT64_MAX) == UINT64_MAX);

	RETURN_PASS;
}

/* ------------------------------------------------------------------------ */
/* signed average */

testresult_t test_bavgs8(void)
{
	/* generic */
	ASSERT(bavgs8(0, 2) == 1);
	/* round up */
	ASSERT(bavgs8(1, 2) == 2);
	/* edge cases */
	ASSERT(bavgs8(INT8_MAX, INT8_MAX) == INT8_MAX);
	ASSERT(bavgs8(INT8_MIN, INT8_MAX) == 0);
	ASSERT(bavgs8(INT8_MIN, INT8_MIN) == INT8_MIN);

	RETURN_PASS;
}

testresult_t test_bavgs16(void)
{
	/* generic */
	ASSERT(bavgs16(0, 2) == 1);
	/* round up */
	ASSERT(bavgs16(1, 2) == 2);
	/* edge cases */
	ASSERT(bavgs16(INT16_MAX, INT16_MAX) == INT16_MAX);
	ASSERT(bavgs16(INT16_MIN, INT16_MAX) == 0);
	ASSERT(bavgs16(INT16_MIN, INT16_MIN) == INT16_MIN);

	RETURN_PASS;
}

testresult_t test_bavgs32(void)
{
	/* generic */
	ASSERT(bavgs32(0, 2) == 1);
	/* round up */
	ASSERT(bavgs32(1, 2) == 2);
	/* edge cases */
	ASSERT(bavgs32(INT32_MAX, INT32_MAX) == INT32_MAX);
	ASSERT(bavgs32(INT32_MIN, INT32_MAX) == 0);
	ASSERT(bavgs32(INT32_MIN, INT32_MIN) == INT32_MIN);

	RETURN_PASS;
}

testresult_t test_bavgs64(void)
{
	/* generic */
	ASSERT(bavgs64(0, 2) == 1);
	/* round up */
	ASSERT(bavgs64(1, 2) == 2);
	/* edge cases */
	ASSERT(bavgs64(INT64_MAX, INT64_MAX) == INT64_MAX);
	ASSERT(bavgs64(INT64_MIN, INT64_MAX) == 0);
	ASSERT(bavgs64(INT64_MIN, INT64_MIN) == INT64_MIN);

	RETURN_PASS;
}

/* ------------------------------------------------------------------------ */
/* bit array (somewhat similar concept to std::vector<bool>) */

#define TEST_BITARRAY_SIZE 515

testresult_t test_bitarray(void)
{
	int i;
	BITARRAY_DECLARE(x, TEST_BITARRAY_SIZE);

	/* always succeeds */
	BITARRAY_ZERO(x);

	for (i = 0; i < TEST_BITARRAY_SIZE; i++) {
		ASSERT(!BITARRAY_ISSET(x, i));
		BITARRAY_SET(x, i);
		ASSERT(BITARRAY_ISSET(x, i));
		BITARRAY_CLEAR(x, i);
		ASSERT(!BITARRAY_ISSET(x, i));
	}

	RETURN_PASS;
}
