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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "slurp.h"
#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>

#if HAVE_MMAP
# include <sys/mman.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

/* --------------------------------------------------------------------- */

/* CHUNK is how much memory is allocated at once. Too large a number is a
 * waste of memory; too small means constantly realloc'ing.
 * 
 * <mml> also, too large a number might take the OS more than an efficient number of reads to read in one
 *       hit -- which you could be processing/reallocing while waiting for the next bit
 * <mml> we had something for some proggy on the server that was sucking data off stdin
 * <mml> and had our resident c programmer and resident perl programmer competing for the fastest code
 * <mml> but, the c coder found that after a bunch of test runs with time, 64k worked out the best case
 * ...
 * <mml> but, on another system with a different block size, 64 blocks may still be efficient, but 64k
 *       might not be 64 blocks
 * (so maybe this should grab the block size from stat() insetad...) */
#define CHUNK 65536

static int _slurp_stdio_pipe(slurp_t * t, int fd)
{
        int old_errno;
        FILE *fp;
        byte *read_buf, *realloc_buf;
        size_t this_len;
        int chunks = 0;

        t->data = NULL;
        fp = fdopen(dup(fd), "rb");
        if (fp == NULL)
                return 0;

        do {
                chunks++;
		/* Have to cast away the const... */
                realloc_buf = realloc((void *) t->data, CHUNK * chunks);
                if (realloc_buf == NULL) {
                        old_errno = errno;
                        fclose(fp);
                        free((void *) t->data);
                        errno = old_errno;
                        return 0;
                }
                t->data = realloc_buf;
                read_buf = (void *) (t->data + (CHUNK * (chunks - 1)));
                this_len = fread(read_buf, 1, CHUNK, fp);
                if (this_len <= 0) {
                        if (ferror(fp)) {
                                old_errno = errno;
                                fclose(fp);
                                free((void *) t->data);
                                errno = old_errno;
                                return 0;
                        }
                }
                t->length += this_len;
        } while (this_len);
        fclose(fp);
        t->type = SLURP_MALLOC;
        return 1;
}

static int _slurp_stdio(slurp_t * t, int fd)
{
        int old_errno;
        FILE *fp;
        size_t got = 0, need, len;

        if (t->length == 0) {
                /* Hrmph. Probably a pipe or something...
                 * gotta do it the REALLY ugly way. */
                return _slurp_stdio_pipe(t, fd);
        }

        fp = fdopen(dup(fd), "rb");

        if (!fp)
                return 0;

        t->data = (byte *) calloc(t->length, 1);
        if (t->data == NULL) {
                old_errno = errno;
                fclose(fp);
                errno = old_errno;
                return 0;
        }

        /* Read the WHOLE thing -- fread might not get it all at once,
         * so keep trying until it returns zero. */
        need = t->length;
        do {
                len = fread((void *) (t->data + got), 1, need, fp);
                if (len <= 0) {
                        if (ferror(fp)) {
                                old_errno = errno;
                                fclose(fp);
                                free((void *) t->data);
                                errno = old_errno;
                                return 0;
                        }

                        if (need > 0) {
                                /* short file */
                                need = 0;
                                t->length = got;
                        }
                } else {
                        got += len;
                        need -= len;
                }
        } while (need > 0);

        fclose(fp);
        t->type = SLURP_MALLOC;
        return 1;
}

static int _slurp_mmap(slurp_t * t, int fd)
{
#if HAVE_MMAP
        /* /me loves mmap */
        t->data = mmap(0, t->length, PROT_READ, MAP_SHARED, fd, 0);
        if (t->data == MAP_FAILED)
                return 0;
        t->type = SLURP_MMAP;
        return 1;
#else
        return 0;
#endif
}

/* --------------------------------------------------------------------- */

slurp_t *slurp(const char *filename, struct stat * buf)
{
        slurp_t *t;
        int fd, old_errno;

        if (buf && S_ISDIR(buf->st_mode)) {
                errno = EISDIR;
                return NULL;
        }

        /* TODO | add a third param for flags, and make this optional.
         * TODO | (along with decompression once that gets written) */
        if (strcmp(filename, "-") == 0) {
                fd = STDIN_FILENO;
        } else {
                fd = open(filename, O_RDONLY);
                if (fd < 0)
                        return NULL;
        }

        t = (slurp_t *) malloc(sizeof(slurp_t));
        if (t == NULL)
                return NULL;

        t->length = (size_t) (buf ? buf->st_size : file_size_fd(fd));
        /* TODO: bz2, gz, etc. */
        if (_slurp_mmap(t, fd) || _slurp_stdio(t, fd)) {
                close(fd);
                return t;
        }

        old_errno = errno;
        close(fd);
        free(t);
        errno = old_errno;
        return NULL;
}

void unslurp(slurp_t * t)
{
        if (!t)
                return;
        switch (t->type) {
#if HAVE_MMAP
        case SLURP_MMAP:
                munmap((void *) t->data, t->length);
                break;
#endif
        case SLURP_MALLOC:
                free((void *) t->data);
                break;
        default:
                /* should never, ever happen. */
                break;
        }
        free(t);
}
