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

#ifndef SCHISM_HEADERS_H_
#define SCHISM_HEADERS_H_
/* This is probably overkill, but it's consistent this way. */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE // not necessarily true but whatever
#endif

#ifdef HAVE_CONFIG_H
# include <build-config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <stdarg.h>

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#include <string.h>

#if !defined(HAVE_STRCASECMP) && defined(HAVE_STRICMP)
# define strcasecmp stricmp
#endif
#if !defined(HAVE_STRNCASECMP) && defined(HAVE_STRNICMP)
# define strncasecmp strnicmp
#endif
#ifndef HAVE_STRVERSCMP
# define strverscmp strcasecmp
#else
/* need to declare this because its a GNU function, and
 * we specifically don't want to define _GNU_SOURCE */
int strverscmp(const char *s1, const char *s2);
#endif
#ifdef HAVE_STRCASESTR
char *strcasestr(const char *haystack, const char *needle);
#else
# define strcasestr strstr // derp
#endif

#if HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#endif

#include <dirent.h>
#ifndef _D_EXACT_NAMLEN
# define _D_EXACT_NAMLEN(dirent) strlen((dirent)->d_name)
#endif

/* dumb workaround for dumb devkitppc bug
 *
 * XXX is this still relevant at all? */
#ifdef SCHISM_WII
# undef NAME_MAX
# undef PATH_MAX
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifndef NAME_MAX
# ifdef MAXPATHLEN
#  define NAME_MAX MAXPATHLEN /* BSD name */
# else
#  ifdef FILENAME_MAX
#   define NAME_MAX FILENAME_MAX
#  else
#   define NAME_MAX 256
#  endif
# endif
#endif


#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

#ifndef timersub
// from FreeBSD
# define timersub(tvp, uvp, vvp)                                       \
	do {                                                            \
		(vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;          \
		(vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;       \
		if ((vvp)->tv_usec < 0) {                               \
			(vvp)->tv_sec--;                                \
			(vvp)->tv_usec += 1000000;                      \
		}                                                       \
	} while (0)
#endif

#ifndef HAVE_ASPRINTF
int asprintf(char **strp, const char *fmt, ...);
#endif
#ifndef HAVE_VASPRINTF
int vasprintf(char **strp, const char *fmt, va_list ap);
#endif
#ifndef HAVE_STRPTIME
char *strptime(const char *buf, const char *fmt, struct tm *tm);
#endif
#ifndef HAVE_MKSTEMP
int mkstemp(char *template);
#endif
#ifndef HAVE_LOCALTIME_R
struct tm *localtime_r(const time_t *timep, struct tm *result);
void localtime_r_quit(void);
int localtime_r_init(void);
#endif
#ifndef HAVE_SETENV
int setenv(const char *name, const char *value, int overwrite);
#endif
#ifndef HAVE_UNSETENV
int unsetenv(const char *name);
#endif

#define INT_SHAPED_PTR(v)               ((intptr_t)(void*)(v))
#define PTR_SHAPED_INT(i)               ((void*)(i))

/* -------------------------------------------------------------- */
/* C99 compatible static assertion */

#if (__STDC_VERSION__ >= 201112L)
# define SCHISM_STATIC_ASSERT(x, msg) _Static_assert(x, msg)
#else
/* should work anywhere and shouldn't dump random stack allocations
 * BUT it fails to provide any sort of useful message to the user */
# define SCHISM_STATIC_ASSERT(x, msg) \
    extern int (*schism_static_assert_function_no_touchy_touchy_plz(void)) \
      [!!sizeof (struct { int __error_if_negative: (x) ? 2 : -1; })]
#endif

#endif /* SCHISM_HEADERS_H_ */
