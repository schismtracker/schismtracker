/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
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

/* This is probably overkill, but it's consistent this way. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <stdarg.h>

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#ifdef HAVE_INTTYPES_H
#include <inttypes.h>
#else
typedef int intptr_t;
#endif


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

#if HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#endif


#ifndef NAME_MAX
# ifdef MAXPATHLEN
#  define NAME_MAX MAXPATHLEN
# else
#  ifdef FILENAME_MAX
#   define NAME_MAX FILENAME_MAX
#  else
#   define NAME_MAX 256
#  endif
# endif
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
		       | (((((unsigned int)x) & 0xFF0000) >> 8) & 0xFF00) | ((((((unsigned int)x) & 0xFF000000) >> 24)) & 0xFF))
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

#ifdef __cplusplus
extern "C" {
#endif

#ifndef HAVE_ASPRINTF
int asprintf(char **strp, const char *fmt, ...);
int vasprintf(char **strp, const char *fmt, va_list ap);
#endif

#ifdef __cplusplus
}
#endif

#ifdef __APPLE_CC__
#define MACOSX	1
#endif

/* Various other stuff */
#ifdef WIN32
# define mkdir(path,mode) mkdir(path)
# define localtime_r(a,b) localtime(a) /* FIXME: not thread safe and stuff */
# define setenv(a,b,c) /* stupid windows */
#endif

#define INT_SHAPED_PTR(v)		((intptr_t)(((void*)(v))))
#define PTR_SHAPED_INT(i)		((void*)i)

