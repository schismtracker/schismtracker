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

#ifndef RELEASE_VERSION
#include "auto/build-version.h"
static char banner[80] = { 0 };
#endif

const char *schism_banner(void)
{
	if (status.flags & CLASSIC_MODE) return TOP_BANNER_CLASSIC;
#ifdef RELEASE_VERSION
	return "Schism Tracker v" VERSION "";
#else
	/* this code is for CVS builds... but nobody will notice */
	if (*banner) return banner;
#define M "Schism Tracker CVS built at "
	strcpy(banner, M "XXXX-XX-XX XX:XX:XX");
#define T (sizeof(M)-1)
	banner[T+0] = BUILD_VERSION[7];
	banner[T+1] = BUILD_VERSION[8];
	banner[T+2] = BUILD_VERSION[9];
	banner[T+3] = BUILD_VERSION[10];
	/* XXX fixme in the year 10,000 :) */
	banner[T+5] = BUILD_VERSION[12];
	banner[T+6] = BUILD_VERSION[13];
	/* Date */
	banner[T+8] = BUILD_VERSION[15];
	banner[T+9] = BUILD_VERSION[16];
	/* Hours */
	banner[T+11]= BUILD_VERSION[18];
	banner[T+12]= BUILD_VERSION[19];
	/* Minutes */
	banner[T+14]= BUILD_VERSION[21];
	banner[T+15]= BUILD_VERSION[22];
	/* Seconds */
	banner[T+17]= BUILD_VERSION[24];
	banner[T+18]= BUILD_VERSION[25];

	banner[T+19] = '\0';
	return banner;
#endif
}


