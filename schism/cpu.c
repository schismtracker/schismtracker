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

/* Prioritize OS-level crap before gcc CPU builtins */
#ifdef SCHISM_WIN32
# include <windows.h> /* IsProcessorFeaturePresent */

/* just to be sure */
# ifndef PF_AVX2_INSTRUCTIONS_AVAILABLE
#  define PF_AVX2_INSTRUCTIONS_AVAILABLE (40)
# endif
# ifndef PF_SSE4_1_INSTRUCTIONS_AVAILABLE
#  define PF_SSE4_1_INSTRUCTIONS_AVAILABLE (37)
# endif
# ifndef PF_XMMI64_INSTRUCTIONS_AVAILABLE
#  define PF_XMMI64_INSTRUCTIONS_AVAILABLE (10)
# endif
#elif defined(SCHISM_MACOSX)
# include <sys/sysctl.h>
#elif defined(SCHISM_MACOS)
# include <Gestalt.h>
#elif SCHISM_GNUC_HAS_BUILTIN(__builtin_cpu_init, 4, 8, 0) \
		&& SCHISM_GNUC_HAS_BUILTIN(__builtin_cpu_supports, 4, 8, 0) \
		&& !defined(SCHISM_XBOX) /* broken toolchain */
# define SCHISM_HAS_GNUC_CPU_BUILTINS
#endif

static BITARRAY_DECLARE(features, CPU_FEATURE_MAX_);

/* populates the CPU feature list */
int cpu_init(void)
{
	/* zero it out if it was already initialized? */
	BITARRAY_ZERO(features);

#ifdef SCHISM_WIN32
	/* Load kernel32 functions dynamically; this function
	 * doesn't exist on Windows 95 */
	HMODULE kernel32;
	typedef BOOL (WINAPI *LPFN_K32_IsProcessorFeaturePresent)(DWORD);
	LPFN_K32_IsProcessorFeaturePresent K32_IsProcessorFeaturePresent;

	kernel32 = LoadLibraryA("KERNEL32.DLL");
	if (!kernel32)
		return -1;

	/* sigh */
	K32_IsProcessorFeaturePresent =
		(LPFN_K32_IsProcessorFeaturePresent)GetProcAddress(kernel32,
			"IsProcessorFeaturePresent");
	if (!K32_IsProcessorFeaturePresent) {
		FreeLibrary(kernel32);
		return -1;
	}

# define CPU_FEATURE(WINBIT, BIT) \
do { \
	if (K32_IsProcessorFeaturePresent(WINBIT)) \
		BITARRAY_SET(features, (BIT)); \
} while (0)

	CPU_FEATURE(PF_XMMI64_INSTRUCTIONS_AVAILABLE, CPU_FEATURE_SSE2);
	CPU_FEATURE(PF_SSE4_1_INSTRUCTIONS_AVAILABLE, CPU_FEATURE_SSE41);
	CPU_FEATURE(PF_AVX2_INSTRUCTIONS_AVAILABLE, CPU_FEATURE_AVX2);
	/* TODO avx512bw */

# undef CPU_FEATURE

	FreeLibrary(kernel32);

	return 0;
#elif defined(SCHISM_MACOSX)

# define CPU_FEATURE(NAME, BIT) \
do { \
	int enabled; \
	size_t enabled_len = sizeof(enabled); \
	if (!sysctlbyname("hw.optional." NAME, &enabled, &enabled_len, NULL, 0) \
			&& enabled_len == sizeof(enabled) \
			&& enabled) \
		BITARRAY_SET(features, (BIT)); \
} while (0)

	CPU_FEATURE("altivec", CPU_FEATURE_ALTIVEC);
	CPU_FEATURE("sse2", CPU_FEATURE_SSE2);
	CPU_FEATURE("avx2", CPU_FEATURE_AVX2);
	CPU_FEATURE("avx512bw", CPU_FEATURE_AVX512BW); /* XXX is this correct? */

# undef CPU_FEATURE

	return 0;
#elif defined(SCHISM_MACOS)
	long feat = 0;
	OSErr err;

	/* weird */
	err = Gestalt(gestaltPowerPCProcessorFeatures, &feat);
	if (err != noErr)
		return -1;

# define CPU_FEATURE(NAME,BIT) \
do { \
	if ((feat) & (1UL << (NAME))) \
		BITARRAY_SET(features, (BIT)); \
} while (0)

	CPU_FEATURE(gestaltPowerPCHasVectorInstructions, CPU_FEATURE_ALTIVEC);

	return 0;
#elif defined(SCHISM_HAS_GNUC_CPU_BUILTINS)
	__builtin_cpu_init();

# define CPU_FEATURE(NAME, BIT) \
do { \
	if (__builtin_cpu_supports(NAME)) \
		BITARRAY_SET(features, (BIT)); \
} while (0)

# if defined(__x86_64__) || defined(__i386__)
	CPU_FEATURE("sse2", CPU_FEATURE_SSE2);
	CPU_FEATURE("sse4.1", CPU_FEATURE_SSE41);
	CPU_FEATURE("avx2", CPU_FEATURE_AVX2);
	CPU_FEATURE("avx512bw", CPU_FEATURE_AVX512BW);
# elif defined(__powerpc__) || defined(__ppc__) || defined(__ppc64__)
	CPU_FEATURE("altivec", CPU_FEATURE_ALTIVEC);
# else
	/* arm? alpha? sparc? */
# endif

#undef CPU_FEATURE

	return 0;
#else
	/* unimplemented */
	return -1;
#endif
}

int cpu_has_feature(int feature)
{
	if (feature < 0 || feature >= CPU_FEATURE_MAX_)
		return 0;

	return BITARRAY_ISSET(features, feature);
}
