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

#include <sys/stat.h> /* roundabout way to get time_t */
#include <sys/types.h>
#include <stddef.h> /* wchar_t */

/* --------------------------------------------------------------------- */

#define ARRAY_SIZE(a) ((signed)(sizeof(a) / sizeof(*(a))))


/* macros stolen from glib */
#ifndef MAX
# define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))
#endif
#ifndef MIN
# define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#endif
#ifndef CLAMP
# define CLAMP(N, L, H) (((N) > (H)) ? (H) : (((N) < (L)) ? (L) : (N)))
#endif

#if defined(__has_attribute)
# if __has_attribute(unused)
#  define UNUSED __attribute__((unused))
# endif
# if __has_attribute(packed)
#  define PACKED __attribute__((packed))
# endif
# if __has_attribute(malloc)
#  define MALLOC __attribute__((malloc))
# endif
# if __has_attribute(pure)
#  define PURE __attribute__((pure))
# endif
#endif

#if defined(__has_builtin)
# if __has_builtin(__builtin_expect)
#  define LIKELY(x)   __builtin_expect(!!(x), 1)
#  define UNLIKELY(x) __builtin_expect(!(x), 1)
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

/* Path stuff that differs by platform */
#ifdef SCHISM_WIN32
# define DIR_SEPARATOR       '\\'
# define DIR_SEPARATOR_STR   "\\"
# define IS_DIR_SEPARATOR(c) ((c) == '/' || (c) == '\\')
#else
# define DIR_SEPARATOR       '/'
# define DIR_SEPARATOR_STR   "/"
# define IS_DIR_SEPARATOR(c) ((c) == '/')
#endif

/* --------------------------------------------------------------------- */
/* functions returning const char * use a static buffer; ones returning char * malloc their return value
(thus it needs free'd)... except numtostr, get_time_string, and get_date_string, which return the buffer
passed to them in the 'buf' parameter. */

/* memory */
extern MALLOC void *mem_alloc(size_t);
extern MALLOC void *mem_calloc(size_t, size_t);
extern MALLOC char *str_dup(const char *);
extern MALLOC char *strn_dup(const char *, size_t);
extern void *mem_realloc(void *, size_t);
extern void mem_free(void *);

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

/* formatting */
/* for get_{time,date}_string, buf should be (at least) 27 chars; anything past that isn't used. */
char *get_date_string(time_t when, char *buf);
char *get_time_string(time_t when, char *buf);
char *numtostr(int digits, unsigned int n, char *buf);
char *numtostr_signed(int digits, int n, char *buf);
char *num99tostr(int n, char *buf);

/* string handling */
const char *get_basename(const char *filename);
const char *get_extension(const char *filename); // including dot; "" if no extension (note: used to strip dot)
char *get_parent_directory(const char *dirname);
int ltrim_string(char *s); // return: length of string after trimming
int rtrim_string(char *s);
int trim_string(char *s);
int str_break(const char *s, char c, char **first, char **second);
char *str_escape(const char *source, int space_hack);
char *str_unescape(const char *source);
char *pretty_name(const char *filename);
int get_num_lines(const char *text);
char *str_concat(const char *s, ...);


/* filesystem */
int make_backup_file(const char *filename, int numbered);
unsigned long long file_size(const char *filename);
int is_file(const char *filename);
int is_directory(const char *filename);
/* following functions should free() the resulting strings */
char *get_home_directory(void);    /* "default" directory for user files, i.e. $HOME, My Documents, etc. */
char *get_dot_directory(void);     /* where settings files go (%AppData% on Windows, same as $HOME elsewhere) */
char *get_current_directory(void); /* just a getcwd() wrapper */

/* wrappers around functions for Unicode support */
#ifdef SCHISM_WIN32
int win32_open(const char *path, int flags);
int win32_wstat(const wchar_t *path, struct stat *st);
int win32_stat(const char *path, struct stat *st);
int win32_mktemp(char *template, size_t size);
int win32_mkdir(const char *path, mode_t mode);
FILE *win32_fopen(const char *path, const char *flags);
# define win32_wmkdir(path, mode) _wmkdir(path)
# define os_fopen                 win32_fopen
# define os_stat                  win32_stat
# define os_open                  win32_open
# define os_mkdir                 win32_mkdir
#else
# define os_fopen fopen
# define os_stat  stat
# define os_open  open
# define os_mkdir mkdir
#endif

/* integer sqrt (very fast; 32 bits limited) */
unsigned int i_sqrt(unsigned int r);

/* sleep in msec */
void ms_sleep(unsigned int m);

/* run a hook */
int run_hook(const char *dir, const char *name, const char *maybe_arg);

/* Mostly a glorified rename(), with fixes for certain dumb OSes.
If 'overwrite' is zero, attempts to rename over an existing file will fail with EEXIST. */
int rename_file(const char *old, const char *newf, int overwrite);

#endif /* SCHISM_UTIL_H_ */
