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

/* Routines to portably operate on IEEE floating point numbers. */

#include "headers.h"
#include "bswap.h"
#include "ieee-float.h"

#include <math.h>

/* These are used for hardware encoding/decoding of floating point numbers.
 * Note that even if these types are available and conform to IEEE 754,
 * this doesn't mean that these operations are done in hardware. */
#if (SIZEOF_FLOAT == 4)
typedef float float32;
# define HAVE_FLOAT32
#elif (SIZEOF_DOUBLE == 4)
typedef double float32;
# define HAVE_FLOAT32
#elif (SIZEOF_LONG_DOUBLE == 4)
typedef long double float32;
# define HAVE_FLOAT32
#endif

#if (SIZEOF_FLOAT == 8)
typedef float float64;
# define HAVE_FLOAT64
#elif (SIZEOF_DOUBLE == 8)
typedef double float64;
# define HAVE_FLOAT64
#elif (SIZEOF_LONG_DOUBLE == 8)
typedef long double float64;
# define HAVE_FLOAT64
#endif

// FIXME: Intel has 80-bit precision, but many compilers
// define long double as 128-bit and simply don't use the
// other 48 bits.
#if (SIZEOF_FLOAT == 10)
typedef float float80;
# define HAVE_FLOAT80
#elif (SIZEOF_DOUBLE == 10)
typedef double float80;
# define HAVE_FLOAT80
#elif (SIZEOF_LONG_DOUBLE == 10)
typedef long double float80;
# define HAVE_FLOAT80
#endif

/* --------------------------------------------------------------------- */
/* Copyright (C) 1988-1991 Apple Computer, Inc.
 * All rights reserved.
 *
 * Machine-independent I/O routines for IEEE floating-point numbers.
 *
 * NaN's and infinities are converted to HUGE_VAL or HUGE, which
 * happens to be infinity on IEEE machines.  Unfortunately, it is
 * impossible to preserve NaN's in a machine-independent way.
 * Infinities are, however, preserved on IEEE machines.
 *
 * These routines have been tested on the following machines:
 *    Apple Macintosh, MPW 3.1 C compiler
 *    Apple Macintosh, THINK C compiler
 *    Silicon Graphics IRIS, MIPS compiler
 *    Cray X/MP and Y/MP
 *    Digital Equipment VAX
 *
 *
 * Implemented by Malcolm Slaney and Ken Turkowski.
 *
 * Malcolm Slaney contributions during 1988-1990 include big- and little-
 * endian file I/O, conversion to and from Motorola's extended 80-bit
 * floating-point format, and conversions to and from IEEE single-
 * precision floating-point format.
 *
 * In 1991, Ken Turkowski implemented the conversions to and from
 * IEEE double-precision format, added more precision to the extended
 * conversions, and accommodated conversions involving +/- infinity,
 * NaN's, and denormalized numbers.
 */

/* hacked together to use uintXX_t instead of unsigned long and friends */

#ifndef HUGE_VAL
# define HUGE_VAL HUGE
#endif /* HUGE_VAL */

#define FloatToUnsigned(f) ((uint32_t) (((int32_t) (f - 2147483648.0)) + 2147483647L + 1))
#define UnsignedToFloat(u) (((double) ((int32_t) (u - 2147483647L - 1))) + 2147483648.0)

/****************************************************************
 * Single precision IEEE floating-point conversion routines
 ****************************************************************/

#define SEXP_MAX		255
#define SEXP_OFFSET		127
#define SEXP_SIZE		8
#define SEXP_POSITION	(32-SEXP_SIZE-1)


double float_decode_ieee_32(const unsigned char bytes[4])
{
#if defined(__STDC_IEC_559__) && defined(HAVE_FLOAT32)
	union {
		uint32_t u;
		float32 f;
	} x;
	memcpy(&x, bytes, 4);
	x.u = bswapBE32(x.u);
	return (double)x.f;
#else
	double f;
	uint32_t mantissa, expon;
	uint32_t bits;

	bits =	((uint32_t)(bytes[0] & 0xFF) << 24)
		|	((uint32_t)(bytes[1] & 0xFF) << 16)
		|	((uint32_t)(bytes[2] & 0xFF) << 8)
		|	 (uint32_t)(bytes[3] & 0xFF); /* Assemble bytes into a long */

	if ((bits & 0x7FFFFFFF) == 0) {
		f = 0;
	} else {
		expon = (bits & 0x7F800000) >> SEXP_POSITION;
		if (expon == SEXP_MAX) { /* Infinity or NaN */
			f = HUGE_VAL; /* Map NaN's to infinity */
		}
		else {
			if (expon == 0) {
				/* Denormalized number */
				mantissa = (bits & 0x7fffff);
				f = ldexp((double)mantissa, expon - SEXP_OFFSET - SEXP_POSITION + 1);
			}
			else {
				/* Normalized number */
				mantissa = (bits & 0x7fffff) + 0x800000; /* Insert hidden bit */
				f = ldexp((double)mantissa, expon - SEXP_OFFSET - SEXP_POSITION);
			}
		}
	}

	return (bits & 0x80000000) ? -f : f;
#endif
}


/****************************************************************/


void float_encode_ieee_32(double num, unsigned char bytes[4])
{
#if defined(__STDC_IEC_559__) && defined(HAVE_FLOAT32)
	union {
		uint32_t u;
		float32 f;
	} x;
	x.f = num;
	x.u = bswapBE32(x.u);
	memcpy(bytes, &x, 4);
#else
	uint32_t sign;
	register uint32_t bits;

	if (num < 0) {	/* Can't distinguish a negative zero */
		sign = 0x80000000;
		num *= -1;
	} else {
		sign = 0;
	}

	if (num == 0) {
		bits = 0;
	} else {
		double fMant;
		int expon;

		fMant = frexp(num, &expon);

		if ((expon > (SEXP_MAX-SEXP_OFFSET+1)) || !(fMant < 1)) {
			/* NaN's and infinities fail second test */
			bits = sign | 0x7F800000; /* +/- infinity */
		} else {
			long mantissa;

			if (expon < -(SEXP_OFFSET-2)) { /* Smaller than normalized */
				int shift = (SEXP_POSITION+1) + (SEXP_OFFSET-2) + expon;
				if (shift < 0) { /* Way too small: flush to zero */
					bits = sign;
				} else { /* Nonzero denormalized number */
					mantissa = (long)(fMant * (1L << shift));
					bits = sign | mantissa;
				}
			} else { /* Normalized number */
				mantissa = (long)floor(fMant * (1L << (SEXP_POSITION+1)));
				mantissa -= (1L << SEXP_POSITION);			/* Hide MSB */
				bits = sign | ((long)((expon + SEXP_OFFSET - 1)) << SEXP_POSITION) | mantissa;
			}
		}
	}

	bytes[0] = bits >> 24; /* Copy to byte string */
	bytes[1] = bits >> 16;
	bytes[2] = bits >> 8;
	bytes[3] = bits;
#endif
}


/****************************************************************
 * Double precision IEEE floating-point conversion routines
 ****************************************************************/

#define DEXP_MAX		2047
#define DEXP_OFFSET		1023
#define DEXP_SIZE		11
#define DEXP_POSITION	(32-DEXP_SIZE-1)

double float_decode_ieee_64(const unsigned char bytes[8])
{
#if defined(__STDC_IEC_559__) && defined(HAVE_FLOAT64)
	union {
		float64 f;
		uint64_t u;
	} x;
	memcpy(&x, bytes, 8);
	x.u = bswapBE64(x.u);
	return (double)x.f;
#else
	double f;
	uint32_t mantissa, expon;
	uint32_t first, second;

	first = ((uint32_t)(bytes[0] & 0xFF) << 24)
		|	((uint32_t)(bytes[1] & 0xFF) << 16)
		|	((uint32_t)(bytes[2] & 0xFF) << 8)
		|	 (uint32_t)(bytes[3] & 0xFF);
	second= ((uint32_t)(bytes[4] & 0xFF) << 24)
		|	((uint32_t)(bytes[5] & 0xFF) << 16)
		|	((uint32_t)(bytes[6] & 0xFF) << 8)
		|	 (uint32_t)(bytes[7] & 0xFF);
	
	if (first == 0 && second == 0) {
		f = 0;
	} else {
		expon = (first & 0x7FF00000) >> DEXP_POSITION;
		if (expon == DEXP_MAX) { /* Infinity or NaN */
			f = HUGE_VAL; /* Map NaN's to infinity */
		}
		else {
			if (expon == 0) { /* Denormalized number */
				mantissa = (first & 0x000FFFFF);
				f = ldexp((double)mantissa, expon - DEXP_OFFSET - DEXP_POSITION + 1);
				f += ldexp(UnsignedToFloat(second), expon - DEXP_OFFSET - DEXP_POSITION + 1 - 32);
			}
			else { /* Normalized number */
				mantissa = (first & 0x000FFFFF) + 0x00100000; /* Insert hidden bit */
				f = ldexp((double)mantissa, expon - DEXP_OFFSET - DEXP_POSITION);
				f += ldexp(UnsignedToFloat(second), expon - DEXP_OFFSET - DEXP_POSITION - 32);
			}
		}
	}

	return (first & 0x80000000) ? -f : f;
#endif
}


/****************************************************************/


void float_encode_ieee_64(double num, unsigned char bytes[8])
{
#if defined(__STDC_IEC_559__) && defined(HAVE_FLOAT64)
	union {
		uint64_t u;
		float64 f;
	} x;
	x.f = num;
	x.u = bswapBE64(x.u);
	memcpy(bytes, &x, 8);
#else
	uint32_t sign;
	uint32_t first, second;

	if (num < 0) {	/* Can't distinguish a negative zero */
		sign = 0x80000000;
		num *= -1;
	} else {
		sign = 0;
	}

	if (num == 0) {
		first = 0;
		second = 0;
	} else {
		double fMant, fsMant;
		int expon;

		fMant = frexp(num, &expon);

		if ((expon > (DEXP_MAX-DEXP_OFFSET+1)) || !(fMant < 1)) {
			/* NaN's and infinities fail second test */
			first = sign | 0x7FF00000; /* +/- infinity */
			second = 0;
		}

		else {
			uint32_t mantissa;

			if (expon < -(DEXP_OFFSET-2)) { /* Smaller than normalized */
				int shift = (DEXP_POSITION+1) + (DEXP_OFFSET-2) + expon;
				if (shift < 0) { /* Too small for something in the MS word */
					first = sign;
					shift += 32;
					if (shift < 0) { /* Way too small: flush to zero */
						second = 0;
					} else { /* Pretty small demorn */
						second = FloatToUnsigned(floor(ldexp(fMant, shift)));
					}
				} else { /* Nonzero denormalized number */
					fsMant = ldexp(fMant, shift);
					mantissa = (uint32_t)floor(fsMant);
					first = sign | mantissa;
					second = FloatToUnsigned(floor(ldexp(fsMant - mantissa, 32)));
				}
			} else { /* Normalized number */
				fsMant = ldexp(fMant, DEXP_POSITION+1);
				mantissa = (uint32_t)floor(fsMant);
				mantissa -= (1L << DEXP_POSITION); /* Hide MSB */
				fsMant -= (1L << DEXP_POSITION);
				first = sign | ((uint32_t)((expon + DEXP_OFFSET - 1)) << DEXP_POSITION) | mantissa;
				second = FloatToUnsigned(floor(ldexp(fsMant - mantissa, 32)));
			}
		}
	}
	
	bytes[0] = first >> 24;
	bytes[1] = first >> 16;
	bytes[2] = first >> 8;
	bytes[3] = first;
	bytes[4] = second >> 24;
	bytes[5] = second >> 16;
	bytes[6] = second >> 8;
	bytes[7] = second;
#endif
}

/* ------------------------------------------------------------------------  */

double float_decode_ieee_80(const unsigned char bytes[10])
{
#if defined(__STDC_IEC_559__) && defined(HAVE_FLOAT80)
	union {
		unsigned char b[10];
		float80 f;
	} x;
	memcpy(&x, bytes, 10);
# ifndef WORDS_BIGENDIAN
#  define SWAP(y, z) do { unsigned char u = x.b[y]; x.b[y] = x.b[z]; x.b[y] = u; } while (0)
	SWAP(0, 9);
	SWAP(1, 8);
	SWAP(2, 7);
	SWAP(3, 6);
	SWAP(4, 5);
#  undef SWAP
# endif
	return x.f;
#else
	double f;
	int expon;
	uint32_t hiMant, loMant;

	expon = ((bytes[0] & 0x7F) << 8) | (bytes[1] & 0xFF);
	hiMant = ((uint32_t) (bytes[2] & 0xFF) << 24)
		| ((uint32_t) (bytes[3] & 0xFF) << 16)
		| ((uint32_t) (bytes[4] & 0xFF) << 8)
		| ((uint32_t) (bytes[5] & 0xFF));
	loMant = ((uint32_t) (bytes[6] & 0xFF) << 24)
		| ((uint32_t) (bytes[7] & 0xFF) << 16)
		| ((uint32_t) (bytes[8] & 0xFF) << 8)
		| ((uint32_t) (bytes[9] & 0xFF));

	if (expon == 0 && hiMant == 0 && loMant == 0) {
		f = 0;
	} else if (expon == 0x7FFF) {
		/* Infinity or NaN */
		f = HUGE_VAL;
	} else {
		expon -= 16383;
		f = ldexp(UnsignedToFloat(hiMant), expon -= 31);
		f += ldexp(UnsignedToFloat(loMant), expon -= 32);
	}

	if (bytes[0] & 0x80)
		return -f;
	else
		return f;
#endif
}

void float_encode_ieee_80(double num, unsigned char bytes[10])
{
#if defined(__STDC_IEC_559__) && defined(HAVE_FLOAT80)
	union {
		unsigned char b[10];
		float80 f;
	} x;
	x.f = num;
# ifndef WORDS_BIGENDIAN
#  define SWAP(y, z) do { unsigned char u = x.b[y]; x.b[y] = x.b[z]; x.b[y] = u; } while (0)
	SWAP(0, 9);
	SWAP(1, 8);
	SWAP(2, 7);
	SWAP(3, 6);
	SWAP(4, 5);
#  undef SWAP
# endif
	memcpy(bytes, &x, 10);
#else
	double fMant, fsMant;
	int expon;
	int32_t sign;
	uint32_t hiMant, loMant;

	if (num < 0) {
		sign = 0x8000;
		num *= -1;
	} else {
		sign = 0;
	}

	if (num == 0) {
		expon = 0;
		hiMant = 0;
		loMant = 0;
	} else {
		fMant = frexp(num, &expon);
		if ((expon > 16384) || !(fMant < 1)) {
			/* Infinity or NaN */
			expon = sign | 0x7FFF;
			hiMant = 0;
			loMant = 0; /* infinity */
		} else {
			/* Finite */
			expon += 16382;
			if (expon < 0) {
				/* denormalized */
				fMant = ldexp(fMant, expon);
				expon = 0;
			}
			expon |= sign;
			fMant = ldexp(fMant, 32);
			fsMant = floor(fMant);
			hiMant = FloatToUnsigned(fsMant);
			fMant = ldexp(fMant - fsMant, 32);
			fsMant = floor(fMant);
			loMant = FloatToUnsigned(fsMant);
		}
	}

	bytes[0] = expon >> 8;
	bytes[1] = expon;
	bytes[2] = hiMant >> 24;
	bytes[3] = hiMant >> 16;
	bytes[4] = hiMant >> 8;
	bytes[5] = hiMant;
	bytes[6] = loMant >> 24;
	bytes[7] = loMant >> 16;
	bytes[8] = loMant >> 8;
	bytes[9] = loMant;
#endif
}
