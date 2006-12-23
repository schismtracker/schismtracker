/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
 * URL: http://rigelseven.com/schism/
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

#include "it.h"

#include "sdlmain.h"

#define TOP_BANNER_CLASSIC "Impulse Tracker v2.14 Copyright (C) 1995-1998 Jeffrey Lim"
#define TOP_BANNER_NORMAL "Schism Tracker v" VERSION
#define TOP_BANNER_CVS "Schism Tracker CVS built on Y___-m_-d_ H_:M_"

/*
Ok, here's a thought... what if we kept the old top banner that just had "CVS"
instead of a version number, and instead displayed the build date in, say, the
about box? We could also chuck the whole string in the log page for CVS builds.
I don't suppose it'd hurt anything, and it would provide potentially useful
information.

Oh and also, would -DBUILD_DATE=`date` or something work? That'd be interesting
for "real" builds as well... and it would also serve as a sort of indicator to
how well a particular package maintainer is keeping up with the version... kind
of in the same mindset as 0-day-warez :)

Note there'd be a difference between the CVS source date and the *build* date,
so both would be applicable -- that is, always show what date it was built, but
dump the uglified dollar-sign revision string to the log page for CVS builds as
well. Then we'd be able to determine pretty much everything about what version
someone is using -- including whether they pulled from CVS themselves and built
it, or downloaded a prebuilt package, etc.
*/

#ifndef RELEASE_VERSION
#include "auto/build-version.h"
static char banner[80] = { 0 };
#endif

const char *schism_banner(void)
{
#ifndef RELEASE_VERSION
	char *ptr;
#endif
	if (status.flags & CLASSIC_MODE) return TOP_BANNER_CLASSIC;
#ifdef RELEASE_VERSION
	return TOP_BANNER_NORMAL;
#else
	/* this code is for CVS builds... but nobody will notice */
	if (banner[0] == 0) {
		strcpy(banner, TOP_BANNER_CVS);

		/* fix in the year 10,000 :) */
		if ((ptr = strstr(banner, "Y___")) != NULL) {
			/* Year */
			ptr[0] = BUILD_VERSION[7];
			ptr[1] = BUILD_VERSION[8];
			ptr[2] = BUILD_VERSION[9];
			ptr[3] = BUILD_VERSION[10];
		}
		if ((ptr = strstr(banner, "m_")) != NULL) {
			/* Month */
			ptr[0] = BUILD_VERSION[12];
			ptr[1] = BUILD_VERSION[13];
		}
		if ((ptr = strstr(banner, "d_")) != NULL) {
			/* Day */
			ptr[0] = BUILD_VERSION[15];
			ptr[1] = BUILD_VERSION[16];
		}
		if ((ptr = strstr(banner, "H_")) != NULL) {
			/* Hour */
			ptr[0] = BUILD_VERSION[18];
			ptr[1] = BUILD_VERSION[19];
		}
		if ((ptr = strstr(banner, "M_")) != NULL) {
			/* Minute */
			ptr[0] = BUILD_VERSION[21];
			ptr[1] = BUILD_VERSION[22];
		}
		if ((ptr = strstr(banner, "S_")) != NULL) {
			/* Second */
			ptr[0] = BUILD_VERSION[24];
			ptr[1] = BUILD_VERSION[25];
		}
	}
	return banner;
#endif
}


