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

#include "title.h"
#include "slurp.h"
#include "util.h"

#include "fmt.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>

/* --------------------------------------------------------------------- */
/* 0 => the file type is unknown */

static int _file_info_get_mem(const byte * data, size_t length, file_info *fi)
{
        const fmt_read_info_func *func = types;

        while (*func) {
                if ((*func) (data, length, fi)) {
                        if (fi->title)
                                trim_string(fi->title);
                        else
                                fi->title = strdup("");
                        return 1;
                } else {
                        func++;
                }
        }
        return 0;
}

/* --------------------------------------------------------------------- */

int file_info_get(const char *filename, struct stat * buf, file_info **fi)
{
        slurp_t *t;
        int n;

        if (buf == NULL) {
                struct stat buf2;
                if (stat(filename, &buf2) != 0)
                        return FINF_ERRNO;
                /* recurse */
                return file_info_get(filename, &buf2, fi);
        }

        *fi = malloc(sizeof(file_info));
        if (*fi == NULL)
        	/* eep, out of memory! */
                return FINF_ERRNO;
        memset(*fi, 0, sizeof(file_info));

        t = slurp(filename, buf);
        if (t == NULL)
                return FINF_ERRNO;
        if (t->length == 0) {
                unslurp(t);
                return FINF_EMPTY;
        }

        n = _file_info_get_mem(t->data, t->length, *fi);
        unslurp(t);
        if (n)
        	return FINF_SUCCESS;
       	free(*fi);
       	return FINF_UNSUPPORTED;
}
