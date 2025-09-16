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

#ifndef SCHISM_CPU_H_
#define SCHISM_CPU_H_

#include "headers.h"

enum {
	/* x86 */
	CPU_FEATURE_SSE2,
	CPU_FEATURE_SSE41, /* SSE 4.1 */
	CPU_FEATURE_AVX2,
/*
	CPU_FEATURE_AVX512F,
*/
	CPU_FEATURE_AVX512BW,

	/* PowerPC */
	CPU_FEATURE_ALTIVEC,
/*
	CPU_FEATURE_VSX,
*/

	CPU_FEATURE_MAX_,
};

/* initializes CPU feature list (must be called at startup) */
int cpu_init(void);
/* 'feature': one of the CPU features listed above */
int cpu_has_feature(int feature);

#endif /* SCHISM_CPU_H_ */
