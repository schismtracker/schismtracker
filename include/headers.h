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

/* this file includes general C language headers that are useful everywhere,
 * along with some stupid stuff for broken toolchains that don't fully comply
 * to C99, like VLA support, va_copy stuff, etc
 *
 * In other projects this header is usually called "stdinc.h" ;) */

#ifndef SCHISM_HEADERS_H_
#define SCHISM_HEADERS_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* need this for some stupid gnu crap */
#endif

#ifdef HAVE_CONFIG_H
# include <build-config.h>
#endif

/* ------------------------------------------------------------------------ */
/* Actual standard C stuff */

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>

#include <errno.h>

#include <time.h>

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

/* TODO we should handle this stuff ourselves, rather than giving it off
 * to autoconf, since we use the PRIx* and INT*_C macros. but that's for
 * another day. */
#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif

#include <math.h>
#if defined(HAVE_TGMATH_H) && !defined(SCHISM_MACOS) /* Macintosh toolchain has tgmath.h, but it's broken as shit */
# include <tgmath.h>
#endif

/* Math constants, not standard but in most toolchains, notably
 * OpenWatcom doesn't have them. */
#ifndef M_PI
# define M_PI 3.1415926535897932384626433832795028841971
#endif

#ifdef HAVE_ASSERT_H
# include <assert.h>
#else
# ifndef NDEBUG
/* untested. does this work? */
#  define assert(x) do { if (!(x)) { fprintf(stderr, "%s: assertion failed", #x); exit(1); } } while (0)
# else
#  define assert(x)
# endif
#endif

#include <stdarg.h>
#ifndef va_copy
# ifdef __va_copy /* GNU */
#  define va_copy(dst, sec) (__va_copy(dst, src))
# else
#  define va_copy(dst, src) (memcpy(&dst, &src, sizeof(va_list)))
# endif
#endif

/* ------------------------------------------------------------------------ */
/* POSIX/UNIX/BSD crap */

/* This defines MAXPATHLEN (used later in this file for the definition of
 * SCHISM_PATH_MAX) */
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

/* These files provide struct stat, which we currently require to be
 * provided by the implementation. Ideally we shouldn't need this and we
 * would use our own "schism-stat" instead but this is okay for now. */
#if HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#endif

/* ------------------------------------------------------------------------ */
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
#ifndef ABS
# define ABS(x) ((x) < 0 ? -(x) : x)
#endif

#define INT_SHAPED_PTR(v)               ((intptr_t)(void*)(v))
#define PTR_SHAPED_INT(i)               ((void*)(i))

/* Compares two version numbers following Semantic Versioning.
 * For example:
 *   SCHISM_SEMVER_ATLEAST(1, 2, 3, 1, 2, 4) -> TRUE
 *   SCHISM_SEMVER_ATLEAST(1, 2, 3, 2, 0, 0) -> TRUE
 *   SCHISM_SEMVER_ATLEAST(1, 2, 1, 1, 1, 0) -> FALSE */
#define SCHISM_SEMVER_ATLEAST(mmajor, mminor, mpatch, major, minor, patch) \
	(((major) >= (mmajor)) \
	 && ((major) > (mmajor) || (minor) >= (mminor)) \
	 && ((major) > (mmajor) || (minor) > (mminor) || (patch) >= (mpatch)))

/* A bunch of compiler detection stuff... don't mind this... */

// GNU C (not necessarily GCC!)
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
#elif defined(__has_feature)
# define SCHISM_GNUC_HAS_EXTENSION(x, major, minor, patch) \
	__has_feature(x)
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

/* ------------------------------------------------------------------------ */

// Ok, now after all that mess, we can define these attributes:

/* This is used for variables or parameters that are
 * known to be unused. */
#if SCHISM_HAS_C23_ATTRIBUTE(maybe_unused)
# define SCHISM_UNUSED [[maybe_unused]]
#elif SCHISM_GNUC_HAS_ATTRIBUTE(__unused__, 2, 7, 0)
# define SCHISM_UNUSED __attribute__((__unused__))
#else
# define SCHISM_UNUSED
#endif

/* Functions with this attribute must return a pointer
 * that is guaranteed to never alias any other pointer
 * still valid when the function returns. */
#if SCHISM_GNUC_HAS_ATTRIBUTE(__malloc__, 3, 0, 0)
# define SCHISM_MALLOC __attribute__((__malloc__))
#elif SCHISM_MSVC_ATLEAST(14, 0, 0)
# define SCHISM_MALLOC __declspec(allocator) __declspec(restrict)
#elif SCHISM_MSVC_ATLEAST(8, 0, 0)
# define SCHISM_MALLOC __declspec(restrict)
#else
# define SCHISM_MALLOC
#endif

/* Used to declare a function that has no effects except
 * for the return value, which only depends on parameters
 * and/or global variables. */
#if SCHISM_GNUC_HAS_ATTRIBUTE(__pure__, 2, 96, 0)
# define SCHISM_PURE __attribute__((__pure__))
#elif SCHISM_HAS_C23_ATTRIBUTE(reproducible)
# define SCHISM_PURE [[reproducible]]
#else
# define SCHISM_PURE
#endif

/* Used to declare a function that:
 * 1. do not examine any arguments except their parameters
 * 2. have no effects except for the return value
 * 3. return (i.e., never hang ever) */
#if SCHISM_GNUC_HAS_ATTRIBUTE(__const__, 2, 5, 0)
# define SCHISM_CONST __attribute__((__const__))
#elif SCHISM_HAS_C23_ATTRIBUTE(unsequenced)
# define SCHISM_CONST [[unsequenced]]
#elif SCHISM_MSVC_ATLEAST(8, 0, 0)
# define SCHISM_CONST __declspec(noalias)
#else
# define SCHISM_CONST
#endif

/* Used to declare a function that never returns. */
#if SCHISM_HAS_C23_ATTRIBUTE(noreturn)
# define SCHISM_NORETURN [[noreturn]]
#elif (__STDC_VERSION__ >= 201112L)
# define SCHISM_NORETURN _Noreturn
#elif SCHISM_GNUC_HAS_ATTRIBUTE(__noreturn__, 2, 5, 0)
# define SCHISM_NORETURN __attribute__((__noreturn__))
#elif SCHISM_MSVC_ATLEAST(7, 1, 0)
# define SCHISM_NORETURN __declspec(noreturn)
#else
# define SCHISM_NORETURN
#endif

/* Used for declaring functions that take in a printf-style
 * format string. If there are vararg params, the first index
 * gets specified in the last parameter of this macro, else
 * it should be 0. */
#if SCHISM_GNUC_HAS_ATTRIBUTE(__format__, 2, 3, 0)
# define SCHISM_FORMAT_PRINTF(format_index, first_index) \
	__attribute__((__format__(printf, format_index, first_index)))
#else
# define SCHISM_FORMAT_PRINTF(format_index, first_index)
#endif

/* Used for declaring malloc functions that take in an
 * allocation size in bytes (malloc), or in the case of
 * _EX, an allocation size in objects (calloc). */
#if SCHISM_GNUC_HAS_ATTRIBUTE(__alloc_size__, 9, 1, 0)
# define SCHISM_ALLOC_SIZE(x) __attribute__((__alloc_size__(x)))
# define SCHISM_ALLOC_SIZE_EX(x, y) __attribute__((__alloc_size__(x, y)))
#else
# define SCHISM_ALLOC_SIZE(x)
# define SCHISM_ALLOC_SIZE_EX(x, y)
#endif

/* Use along with "static inline" to make a function that is
 * always inlined by the compiler. */
#if SCHISM_GNUC_HAS_ATTRIBUTE(__always_inline__, 3, 1, 1)
# define SCHISM_ALWAYS_INLINE __attribute__((__always_inline__))
#elif SCHISM_MSVC_ATLEAST(12, 0, 0)
# define SCHISM_ALWAYS_INLINE __forceinline
#else
# define SCHISM_ALWAYS_INLINE
#endif

/* Use this for functions that we'd like to not use anymore,
 * but for some reason or another (for example, a downstream
 * fork using it heavily) we have to keep it. */
#if SCHISM_HAS_C23_ATTRIBUTE(deprecated)
# define SCHISM_DEPRECATED [[deprecated]]
#elif SCHISM_GNUC_HAS_ATTRIBUTE(__deprecated__, 3, 1, 0)
# define SCHISM_DEPRECATED __attribute__((__deprecated__))
#elif SCHISM_MSVC_ATLEAST(6, 0, 0)
# define SCHISM_DEPRECATED __declspec(deprecated)
#else
# define SCHISM_DEPRECATED
#endif

/* Used for functions that are "hot-spots" of the program.
 * For example, the vgamem scanners are a giant hot-spot,
 * taking up lots of precious processing time. As such,
 * those functions, along with the blitter functions that
 * call them, have been marked as HOT. */
#if SCHISM_GNUC_HAS_ATTRIBUTE(__hot__, 4, 3, 0)
# define SCHISM_HOT __attribute__((__hot__))
#else
# define SCHISM_HOT
#endif

/* Use this attribute to generate one or more function
 * variations that use SIMD instructions to process
 * multiple values at once. This is used within the
 * vgamem scanner, though it's not super useful, since
 * the systems with these instructions will likely
 * already run schism fast enough anyway... */
#if SCHISM_GNUC_HAS_ATTRIBUTE(__simd__, 6, 1, 0)
# define SCHISM_SIMD __attribute__((__simd__))
#else
# define SCHISM_SIMD
#endif

/* Wrapped around an expression to hint to the compiler
 * whether it is likely to happen or not. Can often help
 * with branch prediction on newer processors. */
#if SCHISM_GNUC_HAS_BUILTIN(__builtin_expect, 3, 0, 0)
# define SCHISM_LIKELY(x)   __builtin_expect(!!(x), 1)
# define SCHISM_UNLIKELY(x) __builtin_expect(!(x),  1)
#else
# define SCHISM_LIKELY(x)   (x)
# define SCHISM_UNLIKELY(x) (x)
#endif

/* Win32, used for threads */
#if SCHISM_GNUC_HAS_ATTRIBUTE(__force_align_arg_pointer__, 4, 2, 0)
# define SCHISM_FORCE_ALIGN_ARG_POINTER __attribute__((__force_align_arg_pointer__))
#else
# define SCHISM_FORCE_ALIGN_ARG_POINTER
#endif

/* Used to mark a printf format parameter. Currently only MSVC really
 * has this, and GCC has the much more useful "format" attribute */
#if SCHISM_MSVC_ATLEAST(8, 0, 0)
# define SCHISM_PRINTF_FORMAT_PARAM _Printf_format_string_
#else
# define SCHISM_PRINTF_FORMAT_PARAM
#endif

// Static assertion. DO NOT use this within structure definitions!
#if (__STDC_VERSION__ >= 201112L) || (SCHISM_GNUC_HAS_EXTENSION(c11_static_assert, 4, 6, 0))
# define SCHISM_STATIC_ASSERT(x, msg) _Static_assert(x, msg)
#else
/* should work anywhere and shouldn't dump random stack allocations
 * BUT it fails to provide any sort of useful message to the user */
# define SCHISM_STATIC_ASSERT(x, msg) \
	extern int (*schism_static_assert_function_no_touchy_touchy_plz(void)) \
		[!!sizeof (struct { int __error_if_negative: (x) ? 2 : -1; })]
#endif

/* ------------------------------------------------------------------------ */

#ifndef HAVE_ASPRINTF
int asprintf(char **strp, SCHISM_PRINTF_FORMAT_PARAM const char *fmt, ...) SCHISM_FORMAT_PRINTF(2, 3);
#endif
#ifndef HAVE_VASPRINTF
int vasprintf(char **strp, SCHISM_PRINTF_FORMAT_PARAM const char *fmt, va_list ap) SCHISM_FORMAT_PRINTF(2, 0);
#endif
#ifndef HAVE_SNPRINTF
#undef snprintf // stupid windows
int snprintf(char *buffer, size_t count, SCHISM_PRINTF_FORMAT_PARAM const char *fmt, ...) SCHISM_FORMAT_PRINTF(3, 4);
#endif
#ifndef HAVE_VSNPRINTF
#undef vsnprintf // stupid windows
int vsnprintf(char *buffer, size_t count, SCHISM_PRINTF_FORMAT_PARAM const char *fmt, va_list ap) SCHISM_FORMAT_PRINTF(2, 0);
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

/* ------------------------------------------------------------------------ */

#ifdef SCHISM_WIN32
/* TODO We can actually enable long path support on windows in the manifest
 * https://learn.microsoft.com/en-us/windows/win32/fileio/maximum-file-path-limitation */
# define SCHISM_PATH_MAX ((3 + 256 + 1) * 4) // drive letter, colon, name components, NUL, multiplied by 4 for UTF-8
#elif defined(SCHISM_MACOS)
# define SCHISM_PATH_MAX (255 + 1) // 255 bytes in Pascal-string + NUL terminator (encoding conversions do not happen here yet)
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
/* GNU string comparison functions, and charset_stdlib.c replacements for
 * them */

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

/* ------------------------------------------------------------------------ */
/* VLA abstraction; see the definitions below for details */

/* Giant ifdef tower that's literally only for alloca() */
#ifdef alloca
# define SCHISM_USE_ALLOCA
#else
# ifdef HAVE_ALLOCA_H
#  include <alloca.h>
#  define SCHISM_USE_ALLOCA
# elif defined(__NetBSD__) || defined(__DMC__) // untested
#  include <stdlib.h>
#  define SCHISM_USE_ALLOCA
# elif SCHISM_GNUC_HAS_BUILTIN(__builtin_alloca, 2, 95, 3)
#  define alloca __builtin_alloca
#  define SCHISM_USE_ALLOCA
# elif SCHISM_MSVC_ATLEAST(0, 0, 0) // untested
#  include <malloc.h>
#  define alloca _alloca
#  define SCHISM_USE_ALLOCA
# elif defined(__WATCOMC__) || defined(__BORLANDC__) // untested
#  include <malloc.h>
#  define SCHISM_USE_ALLOCA
# elif defined(_AIX) && !defined(__GNUC__) // untested
#  pragma alloca
#  define SCHISM_USE_ALLOCA
# elif defined(__MRC__) // untested
void *alloca(unsigned int size);
#  define SCHISM_USE_ALLOCA
# elif defined(HAVE_ALLOCA)
// we ought to not be assuming this; eh
void *alloca(size_t size);
#  define SCHISM_USE_ALLOCA
# endif
#endif

// This is an abstraction over VLAs that should work everywhere.
// Unfortunately, since C99 VLAs are not required in C11 and newer,
// this is necessary. Most notably MSVC does not have support for
// VLAs whatsoever.
// Note that it is indeed possible that this will result in an out
// of memory crash when using malloc. IMO this is the normal way to
// handle things since VLAs can, will, and do blow up the stack anyway,
// which is completely unrecoverable in portable code.
//
// usage:
//   SCHISM_VLA_ALLOC(int, arr, some_integer);
//   for (int i = 0; i < SCHISM_VLA_LENGTH(arr); i++)
//       arr[i] = i;
//   SCHISM_VLA_FREE(arr);
#ifdef HAVE_C99_VLAS
# define SCHISM_VLA_ALLOC(type, name, size) type name[size]
# define SCHISM_VLA_FREE(name) // no-op
# define SCHISM_VLA_SIZEOF(name) sizeof(name)
#elif defined(SCHISM_USE_ALLOCA)
# define SCHISM_VLA_ALLOC(type, name, size) \
	const size_t _##name##_vla_size = ((size) * sizeof(type)); \
	type *name = alloca(_##name##_vla_size)
# define SCHISM_VLA_FREE(name) // no-op
# define SCHISM_VLA_SIZEOF(name) (_##name##_vla_size)
#else
// fallback to the heap
# include "mem.h"
# define SCHISM_VLA_ALLOC(type, name, size) \
	const size_t _##name##_vla_size = ((size) * sizeof(type)); \
	type *name = mem_alloc(_##name##_vla_size)
# define SCHISM_VLA_FREE(name) free(name)
# define SCHISM_VLA_SIZEOF(name) (_##name##_vla_size)
#endif

// hm :)
#define SCHISM_VLA_LENGTH(name) (SCHISM_VLA_SIZEOF(name) / sizeof(*name))

/* ------------------------------------------------------------------------ */

/* Used in the brackets of a flexible array member. Meant to be used as so:
 *
 * struct foo {
 *   ...
 *   int b[SCHISM_FAM_SIZE];
 * } foo;
 * int *b;
 *
 * // proper ways of accessing `b`:
 * b = &foo.b;
 * b = (int *)((unsigned char *)foo + offsetof(struct foo, b));
 *
 * // undefined behavior:
 * b = (int *)((unsigned char *)foo + sizeof(foo));
 *
 * Flexible array members should be used as defined in the C99 spec (as in,
 * only ever at the end of a struct).
 */
#ifdef HAVE_C99_FAMS
# define SCHISM_FAM_SIZE
#elif SCHISM_GNUC_ATLEAST(2, 7, 0)
# define SCHISM_FAM_SIZE 0
#else
/* Hmph. */
# define SCHISM_FAM_SIZE 1
#endif

#endif /* SCHISM_HEADERS_H_ */
