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

#ifndef SCHISM_BSWAP_H_
#define SCHISM_BSWAP_H_

#include "headers.h"

/* better than macros, I guess. */
SCHISM_CONST inline uint64_t bswap_64_schism_internal_(uint64_t x)
{
	return (
		  ((x & UINT64_C(0x00000000000000FF)) << 56)
		| ((x & UINT64_C(0x000000000000FF00)) << 40)
		| ((x & UINT64_C(0x0000000000FF0000)) << 24)
		| ((x & UINT64_C(0x00000000FF000000)) << 8)
		| ((x & UINT64_C(0x000000FF00000000)) << 8)
		| ((x & UINT64_C(0x0000FF0000000000)) << 24)
		| ((x & UINT64_C(0x00FF000000000000)) << 40)
		| ((x & UINT64_C(0xFF00000000000000)) << 56)
	);
}

SCHISM_CONST inline uint32_t bswap_32_schism_internal_(uint32_t x)
{
	return (
		  ((x & UINT32_C(0x000000FF)) << 24)
		| ((x & UINT32_C(0x0000FF00)) << 8)
		| ((x & UINT32_C(0x00FF0000)) >> 8)
		| ((x & UINT32_C(0xFF000000)) >> 24)
	);
}

SCHISM_CONST inline uint16_t bswap_16_schism_internal_(uint16_t x)
{
	return (
		  ((x & UINT16_C(0x00FF)) << 8)
		| ((x & UINT16_C(0xFF00)) >> 8)
	);
}

/* check for compiler builtins, for gcc >= 10 and probably clang too */
#ifdef __has_builtin
# if __has_builtin(__builtin_bswap16)
#  define bswap_16(x) __builtin_bswap16(x)
# endif
# if __has_builtin(__builtin_bswap32)
#  define bswap_32(x) __builtin_bswap32(x)
# endif
# if __has_builtin(__builtin_bswap64)
#  define bswap_64(x) __builtin_bswap64(x)
# endif
#endif

/* roll our own; it is now safe to assume that all byteswap
 * routines will act like a function and not like a macro */
#ifndef bswap_64
# define bswap_64(x) bswap_64_schism_internal_(x)
#endif

#ifndef bswap_32
# define bswap_32(x) bswap_32_schism_internal_(x)
#endif

#ifndef bswap_16
# define bswap_16(x) bswap_16_schism_internal_(x)
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

#endif /* SCHISM_BSWAP_H_ */
