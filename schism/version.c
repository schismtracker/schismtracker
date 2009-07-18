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

/* Macros at our disposal:
	__DATE__        "Jun  3 2009"
	__TIME__        "23:39:19"
	__TIMESTAMP__   "Wed Jun  3 23:39:19 2009" */
#define TOP_BANNER_NORMAL "Schism Tracker built on " __DATE__ " " __TIME__


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
	return (classic
		? TOP_BANNER_CLASSIC
		: TOP_BANNER_NORMAL);
}

