/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
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
#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

#include "title.h"
#include "slurp.h"
#include "util.h"

#include "fmt.h"

/* --------------------------------------------------------------------- */
/* NULL => the file type is unknown. (or malloc died...) */

static file_info *_file_info_get_mem(const byte * data, size_t length)
{
        file_info *fi;
        const fmt_read_info_func *func = types;

        fi = malloc(sizeof(file_info));
        if (fi == NULL)
                return NULL;

        while (*func) {
                if ((*func) (data, length, fi)) {
                        if (fi->title)
                                trim_string(fi->title);
                        else
                                fi->title = strdup("");
                        return fi;
                } else {
                        func++;
                }
        }
        return NULL;
}

/* --------------------------------------------------------------------- */
/* if this returns NULL, check errno for the problem. if it failed with
 * EPROTO, there wasn't a handler for the file type; otherwise use
 * perror() or something.
 * hopefully none of the system calls here will fail with EPROTO ;) */

file_info *file_info_get(const char *filename, struct stat * buf)
{
        slurp_t *t;
        file_info *fi;
        int old_errno;

        if (buf == NULL) {
                struct stat buf2;
                if (stat(filename, &buf2) != 0)
                        return NULL;
                return file_info_get(filename, &buf2);
        }

        t = slurp(filename, buf);
        if (t == NULL)
                return NULL;
        if (t->length == 0) {
                unslurp(t);
                errno = ENODATA;
                return NULL;
        }

        fi = _file_info_get_mem(t->data, t->length);
        if (!fi) {
                errno = EPROTO;
        }

        old_errno = errno;
        unslurp(t);
        errno = old_errno;
        return fi;
}
