/* This is probably overkill, but it's consistent this way. */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

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

#if HAVE_UNISTD_H
# include <sys/types.h>
# include <unistd.h>
#endif

/* --------------------------------------------------------------------- */

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
