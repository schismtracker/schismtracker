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

#ifndef UTIL_H
#define UTIL_H

/* FIXME: should include the standard header with all that #ifdef crap.
 * (the only reason time.h is here is for the time_t definition) */
#include <time.h>

/* --------------------------------------------------------------------- */

/* I don't like all caps for these 'cuz they don't get highlighted. */
#ifndef __cplusplus
#define false (0)
#define true (!0)
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

#ifdef __GNUC__
#ifndef LIKELY
#define LIKELY(x) __builtin_expect(!!(x),1)
#endif
#ifndef UNLIKELY
#define UNLIKELY(x) __builtin_expect(!!(x),0)
#endif
#ifndef UNUSED
# define UNUSED __attribute__((unused))
#endif
#ifndef NORETURN
# define NORETURN __attribute__((noreturn))
#endif
#ifndef PACKED
# define PACKED __attribute__((packed))
#endif
#else
#ifndef UNUSED
# define UNUSED
#endif
#ifndef PACKED
# define PACKED
#endif
#ifndef NORETURN
# define NORETURN
#endif
#ifndef LIKELY
#define LIKELY(x)
#endif
#ifndef UNLIKELY
#define UNLIKELY(x)
#endif
#endif

/* Path stuff that differs by platform */
#ifdef WIN32
# define DIR_SEPARATOR '\\'
# define DIR_SEPARATOR_STR "\\"
#else
# define DIR_SEPARATOR '/'
# define DIR_SEPARATOR_STR "/"
#endif

/* --------------------------------------------------------------------- */
/* functions returning const char * use a static buffer; ones returning char * malloc their return value
(thus it needs free'd)... except numtostr, get_time_string, and get_date_string, which return the buffer
passed to them in the 'buf' parameter. */

#ifdef __cplusplus
extern "C" {
#endif

/* memory */
#ifdef __GNUC__
extern __attribute__ ((malloc)) void *mem_alloc(size_t);
extern __attribute__ ((malloc)) char *str_dup(const char *);
#else
extern void *mem_alloc(size_t);
extern char *str_dup(const char *);
#endif
extern void *mem_realloc(void *,size_t);
extern void mem_free(void *);

/* formatting */
/* for get_{time,date}_string, buf should be (at least) 27 chars; anything past that isn't used. */
char *get_date_string(time_t when, char *buf);
char *get_time_string(time_t when, char *buf);
char *numtostr(int digits, unsigned int n, char *buf);
char *numtostr_signed(int digits, int n, char *buf);
char *num99tostr(int n, char *buf);

/* string handling */
const char *get_basename(const char *filename);
const char *get_extension(const char *filename);
char *get_parent_directory(const char *dirname);
void trim_string(char *s);
int str_break(const char *s, char c, char **first, char **second);
char *str_escape(const char *source, int space_hack);
char *str_unescape(const char *source);
char *pretty_name(const char *filename);
int get_num_lines(const char *text);
char *str_concat(const char *s, ...);


/* filesystem */
int make_backup_file(const char *filename, int numbered);
long file_size(const char *filename);
int is_directory(const char *filename);
char *get_home_directory(void); /* should free() the resulting string */
char *get_current_directory(void); /* should free() the resulting string */

void put_env_var(const char *key, const char *value);
void unset_env_var(const char *key);

/* integer sqrt (very fast; 32 bits limited) */
unsigned int i_sqrt(unsigned int r);

/* sleep in msec */
void ms_sleep(unsigned int m);

/* runs a hook */
int run_hook(const char *dir, const char *name, const char *maybe_arg);

#ifdef __cplusplus
}
#endif

#endif /* ! UTIL_H */
