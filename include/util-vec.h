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

#ifndef UTIL_VEC_H_
#define UTIL_VEC_H_

#define MINMAX_INTRINSICS_EX(EXTERN, TARGET, NAME, TYPE, BITS, SIZE, VARS, PREFIX, SUFFIX, PREPROCESS, SET1, LOADU, MIN, MAX, STORE) \
	__attribute__((__target__(#TARGET))) \
	EXTERN void minmax_##BITS##_##NAME(const int##BITS##_t *buf, size_t len, int##BITS##_t *min, int##BITS##_t *max, size_t stride) \
	{ \
		size_t i; \
	\
		if (!len) return; /* wat */ \
	\
		if (len >= SIZE \
				&& stride < SIZE /* stride cannot be over SIZE */ \
				&& !(stride & (stride - 1))) /* stride must be a power of 2 */ \
		{ \
			size_t xlen; \
			TYPE vmin; \
			TYPE vmax; \
			__attribute__((__aligned__(SIZE * (BITS / 8)))) int##BITS##_t amin[SIZE]; \
			__attribute__((__aligned__(SIZE * (BITS / 8)))) int##BITS##_t amax[SIZE]; \
			VARS \
\
			PREFIX \
\
			/* load the min and unsign it */ \
			vmin = SET1(*min); \
			vmax = SET1(*max); \
\
			/* kludge it in */ \
			for (xlen = len / SIZE; xlen > 0; xlen--) { \
				TYPE x; \
\
				x = LOADU((const TYPE *)buf); \
				PREPROCESS \
\
				vmin = MIN(vmin, x); \
				vmax = MAX(vmax, x); \
\
				buf += SIZE; \
			} \
\
			len %= SIZE; \
\
			SUFFIX \
\
			/* TODO: do this in the actual vector so that \
			 * we can just extract the first value */ \
			STORE((TYPE *)amin, vmin); \
			STORE((TYPE *)amax, vmax); \
\
			for (i = 0; i < SIZE; i += stride) { \
				if (amin[i] < *min) *min = amin[i]; \
				if (amax[i] > *max) *max = amax[i]; \
			} \
		} \
\
		/* process the rest */ \
		minmax_##BITS##_c(buf, len, min, max, stride); \
	}

#define MINMAX_INTRINSICS(TARGET, NAME, TYPE, BITS, SIZE, VARS, PREFIX, SUFFIX, PREPROCESS, SET1, LOADU, MIN, MAX, STORE) \
	MINMAX_INTRINSICS_EX(static, TARGET, NAME, TYPE, BITS, SIZE, VARS, PREFIX, SUFFIX, PREPROCESS, SET1, LOADU, MIN, MAX, STORE)

void minmax_8_c(const int8_t *buf, size_t len, int8_t *min, int8_t *max, size_t stride);
void minmax_16_c(const int16_t *buf, size_t len, int16_t *min, int16_t *max, size_t stride);
void minmax_32_c(const int32_t *buf, size_t len, int32_t *min, int32_t *max, size_t stride);
void minmax_8_altivec(const int8_t *buf, size_t len, int8_t *min, int8_t *max, size_t stride);
void minmax_16_altivec(const int16_t *buf, size_t len, int16_t *min, int16_t *max, size_t stride);

#endif

