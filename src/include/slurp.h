/*
 * slurp - General-purpose file reader
 * copyright (c) 2003-2005 chisel <someguy@here.is> <http://here.is/someguy/>
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

#ifndef SLURP_H
#define SLURP_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <unistd.h>

/* --------------------------------------------------------------------- */

enum { SLURP_MMAP, SLURP_MALLOC };

typedef struct {
        size_t length;
        const byte *data;
        int type;       /* SLURP_MMAP or SLURP_MALLOC */
} slurp_t;

/* --------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* slurp returns NULL and sets errno on error. in most cases buf can be
 * NULL; it's only useful if you've already done a stat on the file. */
slurp_t *slurp(const char *filename, struct stat *buf);

void unslurp(slurp_t * t);

#ifdef __cplusplus
}
#endif

#endif /* ! SLURP_H */
