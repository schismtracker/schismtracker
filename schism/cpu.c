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

#include "bits.h"
#include "cpu.h"

#if SCHISM_GNUC_HAS_BUILTIN(__builtin_cpu_init, 4, 8, 0) \
		&& SCHISM_GNUC_HAS_BUILTIN(__builtin_cpu_supports, 4, 8, 0)
# define SCHISM_HAS_GNUC_CPU_BUILTINS
#endif

static BITARRAY_DECLARE(features, CPU_FEATURE_MAX_);

/* populates the CPU feature list */
int cpu_init(void)
{
	/* zero it out if it was already initialized? */
	BITARRAY_ZERO(features);

#ifdef SCHISM_HAS_GNUC_CPU_BUILTINS
	__builtin_cpu_init();

# define CPU_FEATURE(NAME, BIT) \
do { \
	if (__builtin_cpu_supports(NAME)) \
		BITARRAY_SET(features, (BIT)); \
} while (0)

# if defined(__x86_64__) || defined(__i386__)
	CPU_FEATURE("sse2", CPU_FEATURE_SSE2);
	CPU_FEATURE("avx2", CPU_FEATURE_AVX2);
# elif defined(__powerpc__) || defined(__ppc__) || defined(__ppc64__)
	CPU_FEATURE("altivec", CPU_FEATURE_ALTIVEC);
# else
	/* arm? alpha? sparc? */
# endif

	return 0;
#else
	return -1;
#endif
}

int cpu_has_feature(int feature)
{
	if (feature >= CPU_FEATURE_MAX_)
		return 0;

	return BITARRAY_ISSET(features, feature);
}
