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

#include <stdint.h>
#include <assert.h>

/* Portable replacements for signed integer bit shifting. These do not return the same
 * bit size as was given in; in lots of cases that won't really match up at all. */

#define SCHISM_SIGNED_LSHIFT_VARIANT(BITS, RBITS) \
	inline int ## RBITS ## _t schism_signed_lshift_ ## BITS ## _(int ## BITS ## _t x, int y) \
	{ \
		const uint ## RBITS ## _t roffset = UINT ## RBITS ## _C(1) << (RBITS - 1); \
		uint ## RBITS ## _t urx = (uint ## RBITS ## _t)x; \
		urx += roffset; \
		urx <<= y; \
		urx -= roffset << y; \
		return (int ## RBITS ## _t)urx; \
	}

SCHISM_SIGNED_LSHIFT_VARIANT(32, 64)

#undef SCHISM_SIGNED_LSHIFT_VARIANT

#define SCHISM_SIGNED_RSHIFT_VARIANT(BITS, RBITS) \
	inline int ## RBITS ## _t schism_signed_rshift_ ## BITS ## _(int ## BITS ## _t x, int y) \
	{ \
		const uint ## RBITS ## _t roffset = UINT ## RBITS ## _C(1) << (RBITS - 1); \
		uint ## RBITS ## _t urx = (uint ## RBITS ## _t)((int ## RBITS ## _t)x); \
		urx += roffset; \
		urx >>= y; \
		urx -= roffset >> y; \
		return (int ## RBITS ## _t)urx; \
	}

SCHISM_SIGNED_RSHIFT_VARIANT(32, 32)

#undef SCHISM_SIGNED_RSHIFT_VARIANT

#define lshift_signed_32(x, y) schism_signed_lshift_32_(x, y)
#define rshift_signed_32(x, y) schism_signed_rshift_32_(x, y)

#endif /* SCHISM_BSHIFT_H_ */
