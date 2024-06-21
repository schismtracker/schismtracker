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

#ifdef HAVE_CONFIG_H
# include <config.h>
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
#ifndef HAVE_STRCASESTR
# define strcasestr strstr // derp
#endif

#if HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#endif

#include <dirent.h>

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

/* Prototypes for replacement functions; if the standard library
 * declaration doesn't match these, we're screwed anyway... */

int asprintf(char **strp, const char *fmt, ...);
int vasprintf(char **strp, const char *fmt, va_list ap);
char *strptime(const char *buf, const char *fmt, struct tm *tm);
int mkstemp(char *template);

#ifdef SCHISM_WIN32
struct tm *localtime_r(const time_t *timep, struct tm *result);
#endif

#define INT_SHAPED_PTR(v)               ((intptr_t)(void*)(v))
#define PTR_SHAPED_INT(i)               ((void*)(i))

#endif /* SCHISM_HEADERS_H_ */
