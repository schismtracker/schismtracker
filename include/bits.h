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

#ifndef SCHISM_BITS_H_
#define SCHISM_BITS_H_

#include "headers.h"

/* Portable replacements for signed integer bit shifting. These share the same
 * implementation and only do different operations in the same manner and as such
 * are written using a very simple macro. */

#define SCHISM_SIGNED_RSHIFT_VARIANT(type) \
	SCHISM_CONST SCHISM_ALWAYS_INLINE static inline \
	int##type##_t schism_signed_rshift_##type##_(int##type##_t x, unsigned int y) \
	{ \
		return (x < 0) ? ~(~x >> y) : (x >> y); \
	}

/* arithmetic and logical left shift are the same operation */
#define SCHISM_SIGNED_LSHIFT_VARIANT(type) \
    SCHISM_CONST SCHISM_ALWAYS_INLINE static inline \
	int##type##_t schism_signed_lshift_##type##_(int##type##_t x, unsigned int y) \
    { \
        union { \
            int##type##_t s; \
            uint##type##_t u; \
        } xx; \
        xx.s = x; \
        xx.u <<= y; \
        return xx.s; \
    }

/* Unlike right shift, left shift is not implementation-defined (that is,
 * the compiler will do something that makes sense in a platform-defined
 * manner), but rather is full on undefined behavior, i.e. literally
 * *anything* could happen, so we use our own version everywhere. */
SCHISM_SIGNED_LSHIFT_VARIANT(8)
SCHISM_SIGNED_LSHIFT_VARIANT(16)
SCHISM_SIGNED_LSHIFT_VARIANT(32)
SCHISM_SIGNED_LSHIFT_VARIANT(64)
SCHISM_SIGNED_LSHIFT_VARIANT(max)

/* avoid UB by type promoting using ORs */
#define lshift_signed(x, y) \
	((sizeof((x) | (y)) <= sizeof(int8_t)) \
		? (schism_signed_lshift_8_(x, y)) \
		: (sizeof((x) | (y)) <= sizeof(int16_t)) \
			? (schism_signed_lshift_16_(x, y)) \
			: (sizeof((x) | (y)) <= sizeof(int32_t)) \
				? (schism_signed_lshift_32_(x, y)) \
				: (sizeof((x) | (y)) <= sizeof(int64_t)) \
					? (schism_signed_lshift_64_(x, y)) \
					: (schism_signed_lshift_max_(x, y)))

#ifdef HAVE_ARITHMETIC_RSHIFT
# define rshift_signed(x, y) ((x) >> (y))
#else
SCHISM_SIGNED_RSHIFT_VARIANT(8)
SCHISM_SIGNED_RSHIFT_VARIANT(16)
SCHISM_SIGNED_RSHIFT_VARIANT(32)
SCHISM_SIGNED_RSHIFT_VARIANT(64)
SCHISM_SIGNED_RSHIFT_VARIANT(max)
# define rshift_signed(x, y) \
	((sizeof((x) >> (y)) <= sizeof(int8_t)) \
		? (schism_signed_rshift_8_(x, y)) \
		: (sizeof((x) >> (y)) <= sizeof(int16_t)) \
			? (schism_signed_rshift_16_(x, y)) \
			: (sizeof((x) >> (y)) <= sizeof(int32_t)) \
				? (schism_signed_rshift_32_(x, y)) \
				: (sizeof((x) >> (y)) <= sizeof(int64_t)) \
					? (schism_signed_rshift_64_(x, y)) \
					: (schism_signed_rshift_max_(x, y)))
#endif

#undef SCHISM_SIGNED_LSHIFT_VARIANT
#undef SCHISM_SIGNED_RSHIFT_VARIANT
#undef SCHISM_SIGNED_SHIFT_VARIANT

/* ------------------------------------------------------------------------ */
/* byteswap */

/* bswap16 was added later in 4.8.0 */
#if SCHISM_GNUC_HAS_BUILTIN(__builtin_bswap16, 4, 8, 0)
# define bswap_16(x) __builtin_bswap16(x)
#elif SCHISM_MSVC_ATLEAST(8, 0, 0)
# define bswap_16(x) _byteswap_ushort(x)
#endif

#if SCHISM_GNUC_HAS_BUILTIN(__builtin_bswap32, 4, 3, 0)
# define bswap_32(x) __builtin_bswap32(x)
#elif SCHISM_MSVC_ATLEAST(8, 0, 0)
# define bswap_32(x) _byteswap_ulong(x)
#endif

#if SCHISM_GNUC_HAS_BUILTIN(__builtin_bswap64, 4, 3, 0)
# define bswap_64(x) __builtin_bswap64(x)
#elif SCHISM_MSVC_ATLEAST(8, 0, 0)
# define bswap_64(x) _byteswap_uint64(x)
#endif

/* roll our own; it is now safe to assume that all byteswap
 * routines will act like a function and not like a macro */
#ifndef bswap_64
SCHISM_CONST SCHISM_ALWAYS_INLINE
static inline uint64_t bswap_64_schism_internal_(uint64_t x)
{
	return (
		  ((x & UINT64_C(0x00000000000000FF)) << 56)
		| ((x & UINT64_C(0x000000000000FF00)) << 40)
		| ((x & UINT64_C(0x0000000000FF0000)) << 24)
		| ((x & UINT64_C(0x00000000FF000000)) << 8)
		| ((x & UINT64_C(0x000000FF00000000)) >> 8)
		| ((x & UINT64_C(0x0000FF0000000000)) >> 24)
		| ((x & UINT64_C(0x00FF000000000000)) >> 40)
		| ((x & UINT64_C(0xFF00000000000000)) >> 56)
		);
}
# define bswap_64(x) bswap_64_schism_internal_(x)
# define SCHISM_NEED_EXTERN_DEFINE_BSWAP_64
#endif

#ifndef bswap_32
SCHISM_CONST SCHISM_ALWAYS_INLINE
static inline uint32_t bswap_32_schism_internal_(uint32_t x)
{
	return (
		  ((x & UINT32_C(0x000000FF)) << 24)
		| ((x & UINT32_C(0x0000FF00)) << 8)
		| ((x & UINT32_C(0x00FF0000)) >> 8)
		| ((x & UINT32_C(0xFF000000)) >> 24)
		);
}
# define bswap_32(x) bswap_32_schism_internal_(x)
# define SCHISM_NEED_EXTERN_DEFINE_BSWAP_32
#endif

#ifndef bswap_16
SCHISM_CONST SCHISM_ALWAYS_INLINE
static inline uint16_t bswap_16_schism_internal_(uint16_t x)
{
	return (
		  ((x & UINT16_C(0x00FF)) << 8)
		| ((x & UINT16_C(0xFF00)) >> 8)
		);
}
# define bswap_16(x) bswap_16_schism_internal_(x)
# define SCHISM_NEED_EXTERN_DEFINE_BSWAP_16
#endif

/* define the endian-related byte swapping (taken from libmodplug sndfile.h, glibc, and sdl) */
#if WORDS_BIGENDIAN
# define bswapLE16(x) bswap_16(x)
# define bswapLE32(x) bswap_32(x)
# define bswapLE64(x) bswap_64(x)
# define bswapBE16(x) (x)
# define bswapBE32(x) (x)
# define bswapBE64(x) (x)
#else
# define bswapBE16(x) bswap_16(x)
# define bswapBE32(x) bswap_32(x)
# define bswapBE64(x) bswap_64(x)
# define bswapLE16(x) (x)
# define bswapLE32(x) (x)
# define bswapLE64(x) (x)
#endif

/* ------------------------------------------------------------------------ */
/* absolute value */

#define SCHISM_ABS_VARIANT(BITS) \
	static inline SCHISM_CONST SCHISM_ALWAYS_INLINE \
	uint##BITS##_t babs##BITS(int##BITS##_t x) \
	{ \
		return (x < 0) ? (~(uint##BITS##_t)x + 1) : (uint##BITS##_t)x; \
	}

SCHISM_ABS_VARIANT(8)
SCHISM_ABS_VARIANT(16)
SCHISM_ABS_VARIANT(32)
SCHISM_ABS_VARIANT(64)

#undef SCHISM_ABS_VARIANT

/* old name (should be removed) */
#define safe_abs_32 babs32

/* ------------------------------------------------------------------------ */
/* average (unsigned integers) */

/* round((x + y) / 2) */
#define SCHISM_UAVG_VARIANT(BITS) \
	static inline SCHISM_ALWAYS_INLINE \
	uint##BITS##_t bavgu##BITS(uint##BITS##_t x, uint##BITS##_t y) \
	{ \
		uint##BITS##_t x_d_rem    = (x & 1); \
		uint##BITS##_t y_d_rem    = (y & 1); \
		uint##BITS##_t rem_d_quot = ((x_d_rem + y_d_rem) >> 1); \
		uint##BITS##_t rem_d_rem  = ((x_d_rem + y_d_rem) &  1); \
	\
		return ((x / 2) + (y / 2)) + rem_d_quot + rem_d_rem; \
	}

SCHISM_UAVG_VARIANT(8)
SCHISM_UAVG_VARIANT(16)
SCHISM_UAVG_VARIANT(32)
SCHISM_UAVG_VARIANT(64)

#undef SCHISM_UAVG_VARIANT

/* old name (should be removed) */
#define avg_u32 bavgu32

/* ------------------------------------------------------------------------ */
/* average (signed integers) */

/* round((x + y) / 2) */
#define SCHISM_SAVG_VARIANT(BITS) \
	static inline SCHISM_ALWAYS_INLINE \
	int##BITS##_t bavgs##BITS(int##BITS##_t x, int##BITS##_t y) \
	{ \
		int##BITS##_t x_d_rem    = (x % 2); \
		int##BITS##_t y_d_rem    = (y % 2); \
		int##BITS##_t rem_d_quot = ((x_d_rem + y_d_rem) / 2); \
		int##BITS##_t rem_d_rem  = ((x_d_rem + y_d_rem) % 2); \
	\
		return ((x / 2) + (y / 2)) + (rem_d_quot) + (rem_d_rem == 1); \
	}

SCHISM_SAVG_VARIANT(8)
SCHISM_SAVG_VARIANT(16)
SCHISM_SAVG_VARIANT(32)
SCHISM_SAVG_VARIANT(64)

#undef SCHISM_SAVG_VARIANT

/* ------------------------------------------------------------------------ */
/* floor(log10(x)) */

static inline SCHISM_ALWAYS_INLINE
int bplacesu8(uint8_t x)
{
	if (x < UINT8_C(10)) return 1;
	if (x < UINT8_C(100)) return 2;
	return 3;
}

static inline SCHISM_ALWAYS_INLINE
int bplacesu16(uint16_t x)
{
	if (x < UINT16_C(10)) return 1;
	if (x < UINT16_C(100)) return 2;
	if (x < UINT16_C(1000)) return 3;
	if (x < UINT16_C(10000)) return 4;
	return 5;
}

static inline SCHISM_ALWAYS_INLINE
int bplacesu32(uint32_t x)
{
	if (x < UINT32_C(10)) return 1;
	if (x < UINT32_C(100)) return 2;
	if (x < UINT32_C(1000)) return 3;
	if (x < UINT32_C(10000)) return 4;
	if (x < UINT32_C(100000)) return 5;
	if (x < UINT32_C(1000000)) return 6;
	if (x < UINT32_C(10000000)) return 7;
	if (x < UINT32_C(100000000)) return 8;
	if (x < UINT32_C(1000000000)) return 9;
	return 10;
}

static inline SCHISM_ALWAYS_INLINE
int bplacesu64(uint64_t x)
{
	if (x < UINT64_C(10)) return 1;
	if (x < UINT64_C(100)) return 2;
	if (x < UINT64_C(1000)) return 3;
	if (x < UINT64_C(10000)) return 4;
	if (x < UINT64_C(100000)) return 5;
	if (x < UINT64_C(1000000)) return 6;
	if (x < UINT64_C(10000000)) return 7;
	if (x < UINT64_C(100000000)) return 8;
	if (x < UINT64_C(1000000000)) return 9;
	if (x < UINT64_C(10000000000)) return 10;
	if (x < UINT64_C(100000000000)) return 11;
	if (x < UINT64_C(1000000000000)) return 12;
	if (x < UINT64_C(10000000000000)) return 13;
	if (x < UINT64_C(100000000000000)) return 14;
	if (x < UINT64_C(1000000000000000)) return 15;
	if (x < UINT64_C(10000000000000000)) return 16;
	if (x < UINT64_C(100000000000000000)) return 17;
	if (x < UINT64_C(1000000000000000000)) return 18;
	return 19;
}

/* ------------------------------------------------------------------------ */
/* floor(log10(abs(x))) */

#define SCHISM_SPLACES_VARIANT(BITS) \
	static inline SCHISM_ALWAYS_INLINE \
	int bplacess##BITS(int##BITS##_t x) \
	{ \
		return bplacesu##BITS(babs##BITS(x)); \
	}

SCHISM_SPLACES_VARIANT(8)
SCHISM_SPLACES_VARIANT(16)
SCHISM_SPLACES_VARIANT(32)
SCHISM_SPLACES_VARIANT(64)

#undef SCHISM_SPLACES_VARIANT

/* ------------------------------------------------------------------------ */
/* pow(base, exponent) */

SCHISM_CONST SCHISM_ALWAYS_INLINE static inline
uint32_t bpow32(uint32_t base, uint32_t exponent)
{
	uint32_t r = 1, i;
	for (i = 0; i < exponent; i++)
		r *= base;
	return r;
}

/* ------------------------------------------------------------------------ */
/* sqrt(x) */

/* integer sqrt (very fast; 32 bits limited) */
SCHISM_CONST SCHISM_ALWAYS_INLINE static inline
uint32_t bsqrt32(uint32_t r)
{
	uint32_t t, b, c = 0;

	for (b = 0x10000000; b != 0; b >>= 2) {
		t = c + b;
		c >>= 1;
		if (t <= r) {
			r -= t;
			c += b;
		}
	}

	return c;
}

/* ------------------------------------------------------------------------ */
/* bitarrays.
 * this has the same general idea as the UNIX-y fd_set, with the added
 * benefit of being expandible to whatever size.
 *
 * you should make sure that you're within range, because these macros
 * do no checks. */

#define BITARRAY_DECLARE(name, size) \
	uint32_t name[(size + 31) / 32]
#define BITARRAY_ZERO(name) \
	((void)memset(name, 0, sizeof(name)))

/* this overflows (into bits not defined in size), but I don't
 * think it matters */
#define BITARRAY_FILL(name) \
	((void)memset(name, 0xFF, sizeof(name)))

/* these should not be touched outside of bits.h.
 * users should use the macros */
static inline SCHISM_ALWAYS_INLINE
void barray_set_impl(uint32_t *barray, int bit)
{
	barray[bit >> 5] |= ((uint32_t)1 << (bit & 0x1F));
}

static inline SCHISM_ALWAYS_INLINE
void barray_clear_impl(uint32_t *barray, int bit)
{
	barray[bit >> 5] &= ~((uint32_t)1 << (bit & 0x1F));
}

static inline SCHISM_ALWAYS_INLINE
int barray_isset_impl(uint32_t *barray, int bit)
{
	return !!(barray[bit >> 5] & ((uint32_t)1 << (bit & 0x1F)));
}

#define BITARRAY_SET(name, bit) \
	(barray_set_impl(name, bit))
#define BITARRAY_CLEAR(name, bit) \
	(barray_clear_impl(name, bit))
#define BITARRAY_ISSET(name, bit) \
	(barray_isset_impl(name, bit))

/* ------------------------------------------------------------------------ */

/* Gets the nearest power of 2 from a 32-bit integer
 * Can easily be used for 8, 16, or 64 bit as well
 * by adding or removing shifts */
static inline SCHISM_ALWAYS_INLINE
uint32_t bnextpow2(uint32_t x)
{
	x--;
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	x++;
	return x;
}

/* ------------------------------------------------------------------------ */
/* fast 32-bit log2. */

static inline SCHISM_ALWAYS_INLINE
uint32_t blog2(uint32_t x)
{
#if SCHISM_GNUC_HAS_BUILTIN(__builtin_clz, 3, 4, 6)
	/* On x86, this optimizes to the bsr instruction, even as
	 * far back as gcc 3.4.6, which also happens to be the earliest
	 * compiler available on Compiler Explorer ;) */
	return 31 - __builtin_clz(x);
#else
	/* Uh oh, slow! */
	int n;

	for (n = 31; n >= 0; n--)
		if (x & (UINT32_C(1) << n))
			return n;

	return 0;
#endif
}

/* ------------------------------------------------------------------------ */
/* reverses the bits in x, fast */

static inline SCHISM_ALWAYS_INLINE
uint32_t breverse32(uint32_t x)
{
	x = (x & 0xFFFF0000) >> 16 | (x & 0x0000FFFF) << 16;
	x = (x & 0xFF00FF00) >> 8  | (x & 0x00FF00FF) << 8;
	x = (x & 0xF0F0F0F0) >> 4  | (x & 0x0F0F0F0F) << 4;
	x = (x & 0xCCCCCCCC) >> 2  | (x & 0x33333333) << 2;
	x = (x & 0xAAAAAAAA) >> 1  | (x & 0x55555555) << 1;
	return x;
}

/* ------------------------------------------------------------------------ */
/* fast greatest common divisor/factor implementation */

static inline SCHISM_ALWAYS_INLINE
uint32_t bgcd32(uint32_t a, uint32_t b)
{
	while (a != 0) {
		uint32_t tmp = a;
		a = b % tmp;
		b = tmp;
	}

	return b;
}

#endif /* SCHISM_BITS_H_ */
