/*
 * util.h - Various useful functions
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
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

#ifndef UTIL_H
#define UTIL_H

/* FIXME: should include the standard header with all that #ifdef crap.
 * (the only reason time.h is here is for the time_t definition) */
#include <time.h>

/* --------------------------------------------------------------------- */

/* I don't like all caps for these 'cuz they don't get highlighted. */
#ifndef __cplusplus
typedef enum { false, true } bool;
#endif

/* This is just for the sake of typing 4 chars rather than 13. */
#ifndef byte
#define byte byte
typedef unsigned char byte;
#endif

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

/* these should be around an #if gcc or something */
#ifndef UNUSED
# define UNUSED __attribute__((unused))
#endif
#ifndef NORETURN
# define NORETURN __attribute__((noreturn))
#endif
#ifndef BREAKPOINT
# define BREAKPOINT() asm volatile("int3")
#endif

/* borrowed from glib, adapted from perl */
#if !(defined(G_STMT_START) && defined(G_STMT_END))
# if defined(__GNUC__) && !defined(__STRICT_ANSI__) && !defined(__cplusplus)
#  define G_STMT_START        (void)(
#  define G_STMT_END          )
# else
#  if (defined (sun) || defined (__sun__))
#   define G_STMT_START      if (1)
#   define G_STMT_END        else (void)0
#  else
#   define G_STMT_START      do
#   define G_STMT_END        while (0)
#  endif
# endif
#endif

/* well then. #define RUN_IF(func, ...) is the iso c99 conformant way to define a varargs macro, but it seems
to cause problems on freebsd for some reason. RUN_IF(func, args...) is a gcc-ism that precedes the iso spec,
but it at least works. perhaps some day I'll get around to writing a configure check for this. or not. */
#if 0
# define RUN_IF(func, ...) G_STMT_START {\
        if (func != NULL) {\
                func(__VA_ARGS__);\
        }\
} G_STMT_END
#else
# define RUN_IF(func, args...) G_STMT_START {\
        if (func != NULL) {\
                func(args);\
        }\
} G_STMT_END
#endif

/* --------------------------------------------------------------------- */
/* functions returning const char * use a static buffer; ones returning
 * char * malloc their return value (thus it needs free'd). */

#ifdef __cplusplus
extern "C" {
#endif

/* formatting */
const char *format_time(int seconds);
const char *format_date(time_t t);
char *format_size(size_t size, bool power_of_two, const char *base_unit);
char *numtostr(int digits, int n, char *buf);

/* string handling */
const char *get_basename(const char *filename);
const char *get_extension(const char *filename);
char *clean_path(const char *path);
char *get_parent_directory(const char *dirname);
void trim_string(char *s);
char *pretty_name(const char *filename);
int get_num_lines(const char *text);

/* filesystem */
long file_size(const char *filename);
long file_size_fd(int fd);
bool is_directory(const char *filename);
bool has_subdirectories(const char *dirname);

/* TODO: strreplace(str, from, to) */

#ifdef __cplusplus
}
#endif

#endif /* ! UTIL_H */
