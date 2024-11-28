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

/* --------------------------------------------------------------------- */

#define ARRAY_SIZE(a) ((signed)(sizeof(a)/sizeof(*(a))))


/* macros stolen from glib */
#ifndef MAX
# define MAX(X,Y) (((X)>(Y))?(X):(Y))
#endif
#ifndef MIN
# define MIN(X,Y) (((X)<(Y))?(X):(Y))
#endif
#ifndef CLAMP
# define CLAMP(N,L,H) (((N)>(H))?(H):(((N)<(L))?(L):(N)))
#endif

#if defined(__has_attribute)
# if __has_attribute(__unused__)
#  define UNUSED __attribute__((__unused__))
# endif
# if __has_attribute(__packed__)
#  define PACKED __attribute__((__packed__))
# endif
# if __has_attribute(__malloc__)
#  define MALLOC __attribute__((__malloc__))
# endif
# if __has_attribute(__pure__)
#  define PURE __attribute__((__pure__))
# endif
# if __has_attribute(__format__)
#  define SCHISM_FORMAT(x) __attribute__((__format__ x))
# endif
#endif

#if defined(__has_builtin)
# if __has_builtin (__builtin_expect)
#  define LIKELY(x)   __builtin_expect(!!(x), 1)
#  define UNLIKELY(x) __builtin_expect(!(x),  1)
# endif
#endif

#ifndef LIKELY
# define LIKELY(x) (x)
#endif
#ifndef UNLIKELY
# define UNLIKELY(x) (x)
#endif
#ifndef UNUSED
# define UNUSED
#endif
#ifndef PACKED
# define PACKED
#endif
#ifndef MALLOC
# define MALLOC
#endif
#ifndef PURE
# define PURE
#endif
#ifndef SCHISM_FORMAT
# define SCHISM_FORMAT(x)
#endif

/* Path stuff that differs by platform */
#ifdef SCHISM_WIN32
# define DIR_SEPARATOR '\\'
# define DIR_SEPARATOR_STR "\\"
# define IS_DIR_SEPARATOR(c) ((c) == '/' || (c) == '\\')
#else
# define DIR_SEPARATOR '/'
# define DIR_SEPARATOR_STR "/"
# define IS_DIR_SEPARATOR(c) ((c) == '/')
#endif

#include "mem.h" // XXX these includes suck
#include "str.h" // and need to go away

/* --------------------------------------------------------------------- */
/* functions returning const char * use a static buffer; ones returning char * malloc their return value
(thus it needs free'd)... except numtostr, get_time_string, and get_date_string, which return the buffer
passed to them in the 'buf' parameter. */

/*Conversion*/
/* linear -> deciBell*/
/* amplitude normalized to 1.0f.*/
extern double dB(double amplitude);

/// deciBell -> linear*/
extern double dB2_amp(double db);

/* linear -> deciBell*/
/* power normalized to 1.0f.*/
extern double pdB(double power);

/* deciBell -> linear*/
extern double dB2_power(double db);

/* linear -> deciBell*/
/* amplitude normalized to 1.0f.*/
/* Output scaled (and clipped) to 128 lines with noisefloor range.*/
/* ([0..128] = [-noisefloor..0dB])*/
/* correction_dBs corrects the dB after converted, but before scaling.*/
extern short dB_s(int noisefloor, double amplitude, double correction_dBs);

/* deciBell -> linear*/
/* Input scaled to 128 lines with noisefloor range.*/
/* ([0..128] = [-noisefloor..0dB])*/
/* amplitude normalized to 1.0f.*/
/* correction_dBs corrects the dB after converted, but before scaling.*/
extern short dB2_amp_s(int noisefloor, int db, double correction_dBs);

/* linear -> deciBell*/
/* power normalized to 1.0f.*/
/* Output scaled (and clipped) to 128 lines with noisefloor range.*/
/* ([0..128] = [-noisefloor..0dB])*/
/* correction_dBs corrects the dB after converted, but before scaling.*/
extern short pdB_s(int noisefloor, double power, double correction_dBs);

/* deciBell -> linear*/
/* Input scaled to 128 lines with noisefloor range.*/
/* ([0..128] = [-noisefloor..0dB])*/
/* power normalized to 1.0f.*/
/* correction_dBs corrects the dB after converted, but before scaling.*/
extern short dB2_power_s(int noisefloor, int db, double correction_dBs);

/* integer sqrt (very fast; 32 bits limited) */
unsigned int i_sqrt(unsigned int r);

// library loading functionality
void *library_load(const char *name, int current, int age);

#endif /* SCHISM_UTIL_H_ */
