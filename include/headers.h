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
#include <stddef.h>
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#include <stdarg.h>
#ifndef va_copy
# ifdef __va_copy
#  define va_copy(dst, sec) (__va_copy(dst, src))
# else
#  define va_copy(dst, src) (memcpy(&dst, &src, sizeof(va_list)))
# endif
#endif

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#include <string.h>

#if HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#if HAVE_SYS_TIME_H
# include <sys/time.h>
#endif
#include <time.h>

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
#ifdef HAVE_GETOPT_LONG
# include <getopt.h>
#else
/* getopt replacement defines; these intentionally do not use the names
 * getopt() and such because a system could have getopt() but not
 * getopt_long(), and doing this avoids name collisions */
# define ya_no_argument        0
# define ya_required_argument  1
# define ya_optional_argument  2

struct option {
	const char *name;
	int has_arg;
	int *flag;
	int val;
};

int ya_getopt(int argc, char * const argv[], const char *optstring);
int ya_getopt_long(int argc, char * const argv[], const char *optstring, const struct option *longopts, int *longindex);
int ya_getopt_long_only(int argc, char * const argv[], const char *optstring, const struct option *longopts, int *longindex);

extern char *ya_optarg;
extern int ya_optind, ya_opterr, ya_optopt;

// yargh
# define getopt ya_getopt
# define getopt_long ya_getopt_long
# define getopt_long_only ya_getopt_long_only
# define optarg ya_optarg
# define optind ya_optind
# define opterr ya_opterr
# define optopt ya_optopt
# define no_argument ya_no_argument
# define required_argument ya_required_argument
# define optional_argument ya_optional_argument
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

/* -------------------------------------------------------------- */
/* moved from util.h */

#define ARRAY_SIZE(a) (sizeof(a)/sizeof(*(a)))

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

/* A bunch of compiler detection stuff... don't mind this... */
#define SCHISM_SEMVER_ATLEAST(mmajor, mminor, mpatch, major, minor, patch) \
	((major >= mmajor) \
	 && (major > mmajor || minor >= mminor) \
	 && (major > mmajor || minor > mminor || patch >= mpatch))

// GNU C (not GCC!)
#if defined(__GNUC__) && defined(__GNUC_MINOR__) && defined(__GNUC_PATCHLEVEL__)
# define SCHISM_GNUC_ATLEAST(major, minor, patch) \
	SCHISM_SEMVER_ATLEAST(major, minor, patch, __GNUC__, __GNUC_MINOR__, __GNUC_PATCHLEVEL__)
#elif defined(__GNUC__) && defined(__GNUC_MINOR__)
# define SCHISM_GNUC_ATLEAST(major, minor, patch) \
	SCHISM_SEMVER_ATLEAST(major, minor, patch, __GNUC__, __GNUC_MINOR__, 0)
#elif defined(__GNUC__)
# define SCHISM_GNUC_ATLEAST(major, minor, patch) \
	SCHISM_SEMVER_ATLEAST(major, minor, patch, __GNUC__, 0, 0)
#else
# define SCHISM_GNUC_ATLEAST(major, minor, patch) (0)
#endif

// MSVC (untested)
#if defined(_MSC_FULL_VER) && (_MSC_FULL_VER >= 140000000)
# define SCHISM_MSVC_ATLEAST(major, minor, patch) \
	SCHISM_SEMVER_ATLEAST(major, minor, patch, _MSC_FULL_VER / 10000000, (_MSC_FULL_VER % 10000000 / 100000), (_MSC_FULL_VER % 100000) / 100)
#elif defined(_MSC_FULL_VER)
# define SCHISM_MSVC_ATLEAST(major, minor, patch) \
	SCHISM_SEMVER_ATLEAST(major, minor, patch, _MSC_FULL_VER / 1000000, (_MSC_FULL_VER % 1000000) / 10000, (_MSC_FULL_VER % 10000) / 10)
#elif defined(_MSC_VER)
# define SCHISM_MSVC_ATLEAST(major, minor, patch) \
	SCHISM_SEMVER_ATLEAST(major, minor, patch, _MSC_VER / 100, _MSC_VER % 100, 0)
#else
# define SCHISM_MSVC_ATLEAST(major, minor, patch) (0)
#endif

#ifdef __has_attribute
# define SCHISM_GNUC_HAS_ATTRIBUTE(x, major, minor, patch) \
	__has_attribute(x)
#else
# define SCHISM_GNUC_HAS_ATTRIBUTE(x, major, minor, patch) \
	SCHISM_GNUC_ATLEAST(major, minor, patch)
#endif

#ifdef __has_builtin
# define SCHISM_GNUC_HAS_BUILTIN(x, major, minor, patch) \
	__has_builtin(x)
#else
# define SCHISM_GNUC_HAS_BUILTIN(x, major, minor, patch) \
	SCHISM_GNUC_ATLEAST(major, minor, patch)
#endif

#ifdef __has_extension
# define SCHISM_GNUC_HAS_EXTENSION(x, major, minor, patch) \
	__has_extension(x)
#else
# define SCHISM_GNUC_HAS_EXTENSION(x, major, minor, patch) \
	SCHISM_GNUC_ATLEAST(major, minor, patch)
#endif

/* C23 requires that this exists. maybe compiler versions
 * ~could~ be used as a fallback but I don't care enough */
#ifdef __has_c_attribute
# define SCHISM_HAS_C23_ATTRIBUTE(x) __has_c_attribute(x)
#else
# define SCHISM_HAS_C23_ATTRIBUTE(x) (0)
#endif

#if SCHISM_HAS_C23_ATTRIBUTE(maybe_unused)
# define SCHISM_UNUSED [[maybe_unused]]
#elif SCHISM_GNUC_HAS_ATTRIBUTE(__unused__, 2, 7, 0)
# define SCHISM_UNUSED __attribute__((__unused__))
#endif

#if SCHISM_GNUC_HAS_ATTRIBUTE(__malloc__, 3, 0, 0)
# define SCHISM_MALLOC __attribute__((__malloc__))
#elif SCHISM_MSVC_ATLEAST(14, 0, 0)
# define SCHISM_MALLOC __declspec(allocator)
#endif

#if SCHISM_HAS_C23_ATTRIBUTE(reproducible)
# define SCHISM_PURE [[reproducible]]
#elif SCHISM_GNUC_HAS_ATTRIBUTE(__pure__, 2, 96, 0)
# define SCHISM_PURE __attribute__((__pure__))
#endif

#if SCHISM_HAS_C23_ATTRIBUTE(unsequenced)
# define SCHISM_CONST [[unsequenced]]
#elif SCHISM_GNUC_HAS_ATTRIBUTE(__const__, 2, 5, 0)
# define SCHISM_CONST __attribute__((__const__))
#endif

#if SCHISM_GNUC_HAS_ATTRIBUTE(__format__, 2, 3, 0)
# define SCHISM_FORMAT(function, format_index, first_index) \
	__attribute__((__format__(function, format_index, first_index)))
#endif

#if SCHISM_GNUC_HAS_ATTRIBUTE(__alloc_size__, 9, 1, 0)
# define SCHISM_ALLOC_SIZE(x) __attribute__((__alloc_size__(x)))
# define SCHISM_ALLOC_SIZE_EX(x, y) __attribute__((__alloc_size__(x, y)))
#endif

// FIXME what is the real minimum version here? mac os x
// seems to disagree with the idea that it's in gcc 4.0
#if SCHISM_GNUC_HAS_ATTRIBUTE(__always_inline__, 100, 0, 0)
# define SCHISM_ALWAYS_INLINE __attribute__((__always_inline__))
#elif SCHISM_MSVC_ATLEAST(12, 0, 0)
# define SCHISM_ALWAYS_INLINE __forceinline
#endif

#if SCHISM_HAS_C23_ATTRIBUTE(deprecated)
# define SCHISM_DEPRECATED [[deprecated]]
#elif SCHISM_GNUC_HAS_ATTRIBUTE(__deprecated__, 3, 1, 0)
# define SCHISM_DEPRECATED __attribute__((__deprecated__))
#elif SCHISM_MSVC_ATLEAST(13, 10, 0)
# define SCHISM_DEPRECATED __declspec(deprecated)
#endif

#if SCHISM_GNUC_HAS_BUILTIN(__builtin_expect, 3, 0, 0)
# define SCHISM_LIKELY(x)   __builtin_expect(!!(x), 1)
# define SCHISM_UNLIKELY(x) __builtin_expect(!(x),  1)
#endif

#ifndef SCHISM_LIKELY
# define SCHISM_LIKELY(x) (x)
#endif
#ifndef SCHISM_UNLIKELY
# define SCHISM_UNLIKELY(x) (x)
#endif
#ifndef SCHISM_UNUSED
# define SCHISM_UNUSED
#endif
#ifndef SCHISM_PACKED
# define SCHISM_PACKED
#endif
#ifndef SCHISM_MALLOC
# define SCHISM_MALLOC
#endif
#ifndef SCHISM_PURE
# define SCHISM_PURE
#endif
#ifndef SCHISM_CONST
# define SCHISM_CONST
#endif
#ifndef SCHISM_FORMAT
# define SCHISM_FORMAT(x)
#endif
#ifndef SCHISM_ALLOC_SIZE
# define SCHISM_ALLOC_SIZE(x)
#endif
#ifndef SCHISM_ALLOC_SIZE_EX
# define SCHISM_ALLOC_SIZE_EX(x, y)
#endif
#ifndef SCHISM_ALWAYS_INLINE
# define SCHISM_ALWAYS_INLINE
#endif
#ifndef SCHISM_DEPRECATED
# define SCHISM_DEPRECATED
#endif

/* ------------------------------------------------------------------------ */

#ifdef SCHISM_WIN32
/* TODO We can actually enable long path support on windows in the manifest
 * https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation */
# define SCHISM_PATH_MAX (3 + 256 + 1) // drive letter, colon, name components, NUL
#else
# define SCHISM_PATH_MAX (8192) // 8 KiB (should be more than enough)
#endif

// redefine our value if it's smaller than the implementation's
#ifdef PATH_MAX
# if PATH_MAX > SCHISM_PATH_MAX
#  undef SCHISM_PATH_MAX
#  define SCHISM_PATH_MAX PATH_MAX
# endif
#endif

#ifdef MAXPATHLEN
# if MAXPATHLEN > SCHISM_PATH_MAX
#  undef SCHISM_PATH_MAX
#  define SCHISM_PATH_MAX MAXPATHLEN
# endif
#endif

// SCHISM_PATH_MAX is a safe minimum, i guess
#define SCHISM_NAME_MAX SCHISM_PATH_MAX

#ifdef NAME_MAX
# if NAME_MAX > SCHISM_NAME_MAX
#  undef SCHISM_NAME_MAX
#  define SCHISM_NAME_MAX NAME_MAX
# endif
#endif

// FILENAME_MAX is not used here because it shouldn't be used for array bounds
// i.e. it can be like, INT_MAX or something huge like that

/* ------------------------------------------------------------------------ */

#ifndef HAVE_STRCASECMP
# ifdef HAVE_STRICMP
#  define strcasecmp(s1, s2) stricmp(s1, s2)
# else
#  include <charset.h>
#  define strcasecmp(s1, s2) charset_strcasecmp(s1, CHARSET_CHAR, s2, CHARSET_CHAR)
# endif
#endif
#ifndef HAVE_STRNCASECMP
# ifdef HAVE_STRNICMP
#  define strncasecmp(s1, s2, n) strnicmp(s1, s2)
# else
#  include <charset.h>
#  define strncasecmp(s1, s2, n) charset_strncasecmp(s1, CHARSET_CHAR, s2, CHARSET_CHAR)
# endif
#endif
#ifndef HAVE_STRVERSCMP
# include <charset.h>
# define strverscmp(s1, s2) charset_strverscmp(s1, CHARSET_CHAR, s2, CHARSET_CHAR)
#endif
#ifndef HAVE_STRCASESTR
# include <charset.h>
# define strcasestr(haystack, needle) charset_strcasestr(haystack, CHARSET_CHAR, needle, CHARSET_CHAR)
#endif

#endif /* SCHISM_HEADERS_H_ */
