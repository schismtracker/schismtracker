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

#ifndef SCHISM_UTIL_H_
#define SCHISM_UTIL_H_

#include "headers.h"

/* Path stuff that differs by platform */
#ifdef SCHISM_WIN32
# define DIR_SEPARATOR '\\'
# define IS_DIR_SEPARATOR(c) ((c) == '/' || (c) == '\\')
#elif defined(SCHISM_MACOS)
/* I have no idea if this is right or not. */
# define DIR_SEPARATOR ':'
# define IS_DIR_SEPARATOR(c) ((c) == ':')
#else
# define DIR_SEPARATOR '/'
# define IS_DIR_SEPARATOR(c) ((c) == '/')
#endif

#ifndef DIR_SEPARATOR_STR
# define DIR_SEPARATOR_STR ((const char []){ DIR_SEPARATOR, '\0' })
#endif

#include "mem.h" // XXX these includes suck
#include "str.h" // and need to go away

/*Conversion*/
/* linear -> deciBell*/
/* amplitude normalized to 1.0f.*/
SCHISM_PURE extern double dB(double amplitude);

/// deciBell -> linear*/
SCHISM_PURE extern double dB2_amp(double db);

/* linear -> deciBell*/
/* power normalized to 1.0f.*/
SCHISM_PURE extern double pdB(double power);

/* deciBell -> linear*/
SCHISM_PURE extern double dB2_power(double db);

/* linear -> deciBell*/
/* amplitude normalized to 1.0f.*/
/* Output scaled (and clipped) to 128 lines with noisefloor range.*/
/* ([0..128] = [-noisefloor..0dB])*/
/* correction_dBs corrects the dB after converted, but before scaling.*/
SCHISM_PURE extern short dB_s(int noisefloor, double amplitude, double correction_dBs);

/* deciBell -> linear*/
/* Input scaled to 128 lines with noisefloor range.*/
/* ([0..128] = [-noisefloor..0dB])*/
/* amplitude normalized to 1.0f.*/
/* correction_dBs corrects the dB after converted, but before scaling.*/
SCHISM_PURE extern short dB2_amp_s(int noisefloor, int db, double correction_dBs);

/* linear -> deciBell*/
/* power normalized to 1.0f.*/
/* Output scaled (and clipped) to 128 lines with noisefloor range.*/
/* ([0..128] = [-noisefloor..0dB])*/
/* correction_dBs corrects the dB after converted, but before scaling.*/
SCHISM_PURE extern short pdB_s(int noisefloor, double power, double correction_dBs);

/* deciBell -> linear*/
/* Input scaled to 128 lines with noisefloor range.*/
/* ([0..128] = [-noisefloor..0dB])*/
/* power normalized to 1.0f.*/
/* correction_dBs corrects the dB after converted, but before scaling.*/
SCHISM_PURE extern short dB2_power_s(int noisefloor, int db, double correction_dBs);

/* integer sqrt (very fast; 32 bits limited) */
SCHISM_PURE unsigned int i_sqrt(unsigned int r);

// library loading functionality
void *library_load(const char *name, int current, int age);

#endif /* SCHISM_UTIL_H_ */
