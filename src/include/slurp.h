#ifndef _SLURP_H
#define _SLURP_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/stat.h>
#include <unistd.h>

#include "_decl.h"
#include "util.h"


enum { SLURP_MMAP, SLURP_MALLOC };

typedef struct {
        size_t length;
        byte *data;
        int type;       /* SLURP_MMAP or SLURP_MALLOC */
} slurp_t;

/* --------------------------------------------------------------------- */

DECL_BEGIN();

/* slurp returns NULL and sets errno on error. in most cases buf can be
 * NULL; it's only useful if you've already done a stat on the file. */
slurp_t *slurp(const char *filename, struct stat *buf);

void unslurp(slurp_t * t);

DECL_END();

#endif /* ! _SLURP_H */
