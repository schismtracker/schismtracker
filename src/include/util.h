#ifndef _UTIL_H
#define _UTIL_H

#include "_decl.h"

/* FIXME: should include the standard header with all that #ifdef crap */
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

#define RUN_IF(func, ...) G_STMT_START {\
        if (func != NULL) {\
                func(__VA_ARGS__);\
        }\
} G_STMT_END

/* --------------------------------------------------------------------- */
/* functions returning const char * use a static buffer; ones returning
 * char * malloc their return value (thus it needs free'd). */

DECL_BEGIN();

/* formatting */
const char *format_time(int seconds);
const char *format_date(time_t t);
char *format_size(size_t size, bool power_of_two, const char *base_unit);

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

DECL_END();

#endif /* ! _UTIL_H */
