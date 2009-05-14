/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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

#include "it.h"

#include "sdlmain.h"

#define TOP_BANNER_CLASSIC "Impulse Tracker v2.14 Copyright (C) 1995-1998 Jeffrey Lim"
#define TOP_BANNER_NORMAL "Schism Tracker built on Y___-m_-d_ H_:M_"

#include "auto/build-version.h"
static char banner[80] = { 0 };


/* this should be 50 characters or shorter, as they are used in the startup dialog */
const char *ver_short_copyright =
	"Copyright (c) 2003-2009 Storlek & Mrs. Brisby";
const char *ver_short_based_on =
	"Based on Impulse Tracker by Jeffrey Lim aka Pulse";

/* and these should be no more than 74 chars per line */
const char *ver_copyright_credits[] = {
	/* same as the boilerplate for each .c file */
	"Copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>",
	"Copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>",
	"Copyright (c) 2009 Storlek & Mrs. Brisby",
	"",
	"Based on Impulse Tracker which is copyright (C) 1995-1998 Jeffrey Lim.",
	"Contains code by Olivier Lapicque, Markus Fick, Adam Goode, Ville Jokela,",
	"Juan Linietsky, Juha Niemim\x84ki, and others. See the file AUTHORS in",
	"the source distribution for details.",
	NULL,
};
const char *ver_license[] = {
	"This program is free software; you can redistribute it and/or modify it",
	"under the terms of the GNU General Public License as published by the",
	"Free Software Foundation; either version 2 of the License, or (at your",
	"option) any later version.",
	"",
	"You should have received a copy of the GNU General Public License along",
	"with this program; if not, write to the Free Software Foundation, Inc.,",
	"59 Temple Place, Suite 330, Boston, MA 02111-1307  USA",
	NULL,
};


const char *schism_banner(int classic)
{
	char *ptr;

	if (classic)
		return TOP_BANNER_CLASSIC;

	if (banner[0] == 0) {
		strcpy(banner, TOP_BANNER_NORMAL);

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
}

