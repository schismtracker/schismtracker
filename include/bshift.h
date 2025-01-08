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

#ifndef SCHISM_BSHIFT_H_
#define SCHISM_BSHIFT_H_

#include "headers.h"
#include <stdint.h>
#include <assert.h>
#include <limits.h>

/* Portable replacements for signed integer bit shifting. These share the same
 * implementation and only do different operations in the same manner and as such
 * are written using a very simple macro. */

#define SCHISM_SIGNED_SHIFT_VARIANT(type, typec, PREFIX, OPERATION) \
	SCHISM_CONST SCHISM_ALWAYS_INLINE inline int##type##_t schism_signed_##PREFIX##shift_##type##_(int##type##_t x, unsigned int y) \
	{ \
		return (x < 0) ? ~(~x OPERATION y) : (x OPERATION y); \
	}

#define SCHISM_SIGNED_LSHIFT_VARIANT(type, typec) \
	SCHISM_SIGNED_SHIFT_VARIANT(type, typec, l, <<)

#define SCHISM_SIGNED_RSHIFT_VARIANT(type, typec) \
	SCHISM_SIGNED_SHIFT_VARIANT(type, typec, r, >>)

/* Unlike right shift, left shift is not implementation-defined (that is,
 * the compiler will do something that makes sense in a platform-defined
 * manner), but rather is full on undefined behavior, i.e. literally
 * *anything* could happen, so we use bit complement shifting everywhere. */
SCHISM_SIGNED_LSHIFT_VARIANT(8, 8)
SCHISM_SIGNED_LSHIFT_VARIANT(16, 16)
SCHISM_SIGNED_LSHIFT_VARIANT(32, 32)
SCHISM_SIGNED_LSHIFT_VARIANT(64, 64)
SCHISM_SIGNED_LSHIFT_VARIANT(max, MAX)
#define lshift_signed(x, y) \
	((sizeof(x) == sizeof(int8_t)) \
		? (schism_signed_lshift_8_(x, y)) \
		: (sizeof(x) == sizeof(int16_t)) \
			? (schism_signed_lshift_16_(x, y)) \
			: (sizeof(x) == sizeof(int32_t)) \
				? (schism_signed_lshift_32_(x, y)) \
				: (sizeof(x) == sizeof(int64_t)) \
					? (schism_signed_lshift_64_(x, y)) \
					: (schism_signed_lshift_max_(x, y)))

#ifdef HAVE_ARITHMETIC_RSHIFT
# define rshift_signed(x, y) ((x) >> (y))
#else
SCHISM_SIGNED_RSHIFT_VARIANT(8, 8)
SCHISM_SIGNED_RSHIFT_VARIANT(16, 16)
SCHISM_SIGNED_RSHIFT_VARIANT(32, 32)
SCHISM_SIGNED_RSHIFT_VARIANT(64, 64)
SCHISM_SIGNED_RSHIFT_VARIANT(max, MAX)
# define rshift_signed(x, y) \
	((sizeof(x) == sizeof(int8_t)) \
		? (schism_signed_rshift_8_(x, y)) \
		: (sizeof(x) == sizeof(int16_t)) \
			? (schism_signed_rshift_16_(x, y)) \
			: (sizeof(x) == sizeof(int32_t)) \
				? (schism_signed_rshift_32_(x, y)) \
				: (sizeof(x) == sizeof(int64_t)) \
					? (schism_signed_rshift_64_(x, y)) \
					: (schism_signed_rshift_max_(x, y)))
#endif

#undef SCHISM_SIGNED_LSHIFT_VARIANT
#undef SCHISM_SIGNED_RSHIFT_VARIANT
#undef SCHISM_SIGNED_SHIFT_VARIANT

#endif /* SCHISM_BSHIFT_H_ */
