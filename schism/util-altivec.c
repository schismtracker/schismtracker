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
#include "util.h"
#include "osdefs.h"
#include "mem.h"
#include "cpu.h"
#include "mt.h"
#include "util-vec.h"

#include <altivec.h>

static inline SCHISM_ALWAYS_INLINE
vector signed char altivec_set1_s8(signed char x)
{
	return (vector signed char){x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x};
}

static inline SCHISM_ALWAYS_INLINE
vector signed short altivec_set1_s16(signed short x)
{
	return (vector signed short){x, x, x, x, x, x, x, x};
}

#define altivec_load_unaligned_(x) vec_perm(vec_ld(0, x), vec_ld(15, x), vec_lvsl(0, x))

static inline SCHISM_ALWAYS_INLINE
vector signed char altivec_loadu_s8(const vector signed char *x)
{
	return altivec_load_unaligned_((const signed char *)x);
}

static inline SCHISM_ALWAYS_INLINE
vector signed short altivec_loadu_s16(const vector signed short *x)
{
	return altivec_load_unaligned_((const signed short *)x);
}

#define altivec_store(arr, x) (vec_st(x, 0, arr))

MINMAX_INTRINSICS_EX(extern, altivec, altivec, vector signed char, 8, 16,
	/* nothing */, /* nothing */, /* nothing */, /* nothing */,
	altivec_set1_s8, altivec_loadu_s8, vec_min, vec_max, altivec_store)

MINMAX_INTRINSICS_EX(extern, altivec, altivec, vector signed short, 16, 8,
	/* nothing */, /* nothing */, /* nothing */, /* nothing */,
	altivec_set1_s16, altivec_loadu_s16, vec_min, vec_max, altivec_store)
