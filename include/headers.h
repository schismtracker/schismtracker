/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2011 Storlek
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

#ifndef __headers_h
#define __headers_h
/* This is probably overkill, but it's consistent this way. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

/* Some stuff was conditionally included only for files that need it, but really it's not like defining
bswapLE32 or including sys/time.h adversely affects compilation time. This isn't some xboxhueg project
that takes hours to build, these are little silly overoptimizations, and it's just troublesome to try
to work out what order headers are supposed to be processed so that all the other files pick up the bits
of headers.h that they need (song_t, I'm looking at you)
Eventually I'll do some housekeeping with the headers and get rid of all these silly NEED_*'s, but this
will do for now. */
#define NEED_BYTESWAP
#define NEED_TIME
#define NEED_DIRENT


#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <stdarg.h>

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#include <stdint.h>


/* Portability is a pain. */
#if STDC_HEADERS
# include <string.h>
#else
# ifndef HAVE_STRCHR
#  define strchr index
#  define strrchr rindex
# endif
char *strchr(), *strrchr();
# ifndef HAVE_MEMMOVE
#  define memcpy(d, s, n) bcopy ((s), (d), (n))
#  define memmove(d, s, n) bcopy ((s), (d), (n))
# endif
#endif

#if !defined(HAVE_STRCASECMP) && defined(HAVE_STRICMP)
# define strcasecmp stricmp
#endif
#if !defined(HAVE_STRNCASECMP) && defined(HAVE_STRNICMP)
# define strncasecmp strnicmp
#endif
#ifndef HAVE_STRVERSCMP
# define strverscmp strcasecmp
#endif
#ifndef HAVE_STRCASESTR
# define strcasestr strstr // derp
#endif

#if HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#endif


#ifdef NEED_DIRENT
# if HAVE_DIRENT_H
#  include <dirent.h>
#  ifndef _D_EXACT_NAMLEN
#   define _D_EXACT_NAMLEN(dirent) strlen((dirent)->d_name)
#  endif
# else
#  define dirent direct
#  ifndef _D_EXACT_NAMLEN
#   define _D_EXACT_NAMLEN(dirent) strlen((dirent)->d_name)
#  endif
#  if HAVE_SYS_NDIR_H
#   include <sys/ndir.h>
#  endif
#  if HAVE_SYS_DIR_H
#   include <sys/dir.h>
#  endif
#  if HAVE_NDIR_H
#   include <ndir.h>
#  endif
# endif
#endif

/* dumb workaround for dumb devkitppc bug */
#ifdef GEKKO
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


#ifdef NEED_TIME
# if TIME_WITH_SYS_TIME
#  include <sys/time.h>
#  include <time.h>
# else
#  if HAVE_SYS_TIME_H
#   include <sys/time.h>
#  else
#   include <time.h>
#  endif
# endif
# ifndef timersub
// from FreeBSD
#  define timersub(tvp, uvp, vvp)                                       \
        do {                                                            \
                (vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;          \
                (vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;       \
                if ((vvp)->tv_usec < 0) {                               \
                        (vvp)->tv_sec--;                                \
                        (vvp)->tv_usec += 1000000;                      \
                }                                                       \
        } while (0)
# endif
#endif

#ifdef REALLY_BIG_ENDIAN
#ifndef WORDS_BIGENDIAN
#define WORDS_BIGENDIAN 1
#endif
#endif

#ifdef NEED_BYTESWAP
# if HAVE_BYTESWAP_H
/* byteswap.h uses inline assembly if possible (faster than bit-shifting) */
#  include <byteswap.h>
# else
#  define bswap_32(x) (((((unsigned int)x) & 0xFF) << 24) | ((((unsigned int)x) & 0xFF00) << 8) \
                       | (((((unsigned int)x) & 0xFF0000) >> 8) & 0xFF00) \
                       | ((((((unsigned int)x) & 0xFF000000) >> 24)) & 0xFF))
#  define bswap_16(x) (((((unsigned short)x) >> 8) & 0xFF) | ((((unsigned short)x) << 8) & 0xFF00))
# endif
/* define the endian-related byte swapping (taken from libmodplug sndfile.h, glibc, and sdl) */
# if defined(ARM) && defined(_WIN32_WCE)
/* I have no idea what this does, but okay :) */

/* This forces integer operations to only occur on aligned
   addresses. -mrsb */
static inline unsigned short int ARM_get16(const void *data)
{
        unsigned short int s;
        memcpy(&s,data,sizeof(s));
        return s;
}
static inline unsigned int ARM_get32(const void *data)
{
        unsigned int s;
        memcpy(&s,data,sizeof(s));
        return s;
}
#  define bswapLE16(x) ARM_get16(&x)
#  define bswapLE32(x) ARM_get32(&x)
#  define bswapBE16(x) bswap_16(ARM_get16(&x))
#  define bswapBE32(x) bswap_32(ARM_get32(&x))
# elif WORDS_BIGENDIAN
#  define bswapLE16(x) bswap_16(x)
#  define bswapLE32(x) bswap_32(x)
#  define bswapBE16(x) (x)
#  define bswapBE32(x) (x)
# else
#  define bswapBE16(x) bswap_16(x)
#  define bswapBE32(x) bswap_32(x)
#  define bswapLE16(x) (x)
#  define bswapLE32(x) (x)
# endif
#endif

/* Prototypes for replacement functions */

#ifndef HAVE_ASPRINTF
int asprintf(char **strp, const char *fmt, ...);
#endif
#ifndef HAVE_VASPRINTF
int vasprintf(char **strp, const char *fmt, va_list ap);
#endif
#ifdef NEED_TIME
# ifndef HAVE_STRPTIME
char *strptime(const char *buf, const char *fmt, struct tm *tm);
# endif
# ifdef WIN32
struct tm *localtime_r(const time_t *timep, struct tm *result);
# endif
#endif
#ifndef HAVE_MKSTEMP
int mkstemp(char *template);
#endif

#ifdef __APPLE_CC__
#define MACOSX  1
#endif

/* Various other stuff */
#ifdef WIN32
# define mkdir(path,mode) mkdir(path)
# define setenv(a,b,c) /* stupid windows */
# define fsync _commit
#endif

#define INT_SHAPED_PTR(v)               ((intptr_t)(((void*)(v))))
#define PTR_SHAPED_INT(i)               ((void*)i)

#endif

