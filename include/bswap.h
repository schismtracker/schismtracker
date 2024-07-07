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

/* byteswap.h (at least on glibc) actually *doesn't* provide asm versions of byteswap
 * operations; see commit 0d40d0e. in any case, the compiler will likely be able to optimize
 * these better than assembly, and actually good compilers will notice the swapping pattern
 * and optimize them itself
 *
 *  - paper */

#define bswap_32(x) \
	  (((((uint32_t)(x)) & 0xFF) << 24) \
	|  ((((uint32_t)(x)) & 0xFF00) << 8) \
	| (((((uint32_t)(x)) & 0xFF0000) >> 8) & 0xFF00) \
	| (((((uint32_t)(x)) & 0xFF000000) >> 24) & 0xFF))

#define bswap_16(x) (((((uint16_t)(x)) >> 8) & 0xFF) | ((((uint16_t)(x)) << 8) & 0xFF00))

/* define the endian-related byte swapping (taken from libmodplug sndfile.h, glibc, and sdl) */
#if defined(ARM) && defined(_WIN32_WCE)
/* I have no idea what this does, but okay :) */

/* This forces integer operations to only occur on aligned
 * addresses. -mrsb */

/* when did schism ever run on wince? what?
 *
 *  - paper */
static inline uint16_t ARM_get16(const void *data)
{
	uint16_t s;
	memcpy(&s, data, sizeof(s));
	return s;
}
static inline uint32_t ARM_get32(const void *data)
{
	uint32_t s;
	memcpy(&s, data, sizeof(s));
	return s;
}
# define bswapLE16(x) ARM_get16(&(x))
# define bswapLE32(x) ARM_get32(&(x))
# define bswapBE16(x) bswap_16(ARM_get16(&(x)))
# define bswapBE32(x) bswap_32(ARM_get32(&(x)))
#elif WORDS_BIGENDIAN
# define bswapLE16(x) bswap_16(x)
# define bswapLE32(x) bswap_32(x)
# define bswapBE16(x) (x)
# define bswapBE32(x) (x)
#else
# define bswapBE16(x) bswap_16(x)
# define bswapBE32(x) bswap_32(x)
# define bswapLE16(x) (x)
# define bswapLE32(x) (x)
#endif

#endif /* SCHISM_BSWAP_H_ */
