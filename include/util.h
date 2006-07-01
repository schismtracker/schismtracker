/*
 * util.h - Various useful functions
 * copyright (c) 2003-2005 chisel <storlek@chisel.cjb.net>
 * copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
 * URL: http://rigelseven.com/
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
#ifndef UNUSED
# define UNUSED __attribute__((unused))
#endif
#ifndef NORETURN
# define NORETURN __attribute__((noreturn))
#endif
#else
#ifndef UNUSED
# define UNUSED
#endif
#ifndef NORETURN
# define NORETURN
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
extern void *mem_alloc(size_t);
extern void *mem_realloc(void *,size_t);
extern void mem_free(void *);

/* formatting */
/* for get_{time,date}_string, buf should be (at least) 27 chars; anything past that isn't used. */
unsigned char *get_date_string(time_t when, unsigned char *buf);
unsigned char *get_time_string(time_t when, unsigned char *buf);
unsigned char *numtostr(int digits, int n, unsigned char *buf);
unsigned char *num99tostr(int n, unsigned char *buf);

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
int make_backup_file(const char *filename);
long file_size(const char *filename);
long file_size_fd(int fd);
int is_directory(const char *filename);
char *get_home_directory(void); /* should free() the resulting string */

void put_env_var(const char *key, const char *value);

/* integer sqrt (very fast; 32 bits limited) */
unsigned int i_sqrt(unsigned int r);

/* sleep in msec */
void ms_sleep(unsigned int m);

#ifdef __cplusplus
}
#endif

#endif /* ! UTIL_H */
