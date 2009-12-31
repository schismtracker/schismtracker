/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010 Storlek
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "slurp.h"
#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>

/* The dup's are because fclose closes its file descriptor even if the FILE* was acquired with fdopen, and when
the control gets back to slurp, it closes the fd (again). It doesn't seem to exist on Amiga OS though, so... */
#ifndef HAVE_DUP
# define dup(fd) fd
#endif

/* I hate this... */
#ifndef O_BINARY
# ifdef O_RAW
#  define O_BINARY O_RAW
# else
#  define O_BINARY 0
# endif
#endif

static void _slurp_closure_free(slurp_t *t)
{
        free(t->data);
}

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
 * (so maybe this should grab the block size from stat() instead...) */
#define CHUNK 65536

static int _slurp_stdio_pipe(slurp_t * t, int fd)
{
        int old_errno;
        FILE *fp;
        uint8_t *read_buf, *realloc_buf;
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
                        free(t->data);
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
                                free(t->data);
                                errno = old_errno;
                                return 0;
                        }
                }
                t->length += this_len;
        } while (this_len);
        fclose(fp);
        t->closure = _slurp_closure_free;
        return 1;
}

static int _slurp_stdio(slurp_t * t, int fd)
{
        int old_errno;
        FILE *fp;
        size_t got = 0, need, len;

        if (t->length == 0) {
                /* Hrmph. Probably a pipe or something... gotta do it the REALLY ugly way. */
                return _slurp_stdio_pipe(t, fd);
        }

        fp = fdopen(dup(fd), "rb");

        if (!fp)
                return 0;

        t->data = (uint8_t *) malloc(t->length);
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
                len = fread(t->data + got, 1, need, fp);
                if (len <= 0) {
                        if (ferror(fp)) {
                                old_errno = errno;
                                fclose(fp);
                                free(t->data);
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
        t->closure = _slurp_closure_free;
        return 1;
}


/* --------------------------------------------------------------------- */

static slurp_t *_slurp_open(const char *filename, struct stat * buf, size_t size)
{
        slurp_t *t;
        int fd, old_errno;

        if (buf && S_ISDIR(buf->st_mode)) {
                errno = EISDIR;
                return NULL;
        }

        t = (slurp_t *) mem_alloc(sizeof(slurp_t));
        if (t == NULL)
                return NULL;
        t->pos = 0;

        if (strcmp(filename, "-") == 0) {
                if (_slurp_stdio(t, STDIN_FILENO))
                        return t;
                free(t);
                return NULL;
        }

        if (size <= 0) {
                size = (buf ? buf->st_size : file_size(filename));
        }

#ifdef WIN32
        switch (slurp_win32(t, filename, size)) {
        case 0: free(t); return NULL;
        case 1: return t;
        };
#endif

#if HAVE_MMAP
        switch (slurp_mmap(t, filename, size)) {
        case 0: free(t); return NULL;
        case 1: return t;
        };
#endif

        fd = open(filename, O_RDONLY | O_BINARY);

        if (fd < 0) {
                free(t);
                return NULL;
        }

        t->length = size;

        if (_slurp_stdio(t, fd)) {
                close(fd);
                return t;
        }

        old_errno = errno;
        close(fd);
        free(t);
        errno = old_errno;
        return NULL;
}

slurp_t *slurp(const char *filename, struct stat * buf, size_t size)
{
        slurp_t *t = _slurp_open(filename, buf, size);
        uint8_t *mmdata;
        size_t mmlen;

        if (!t) {
                return NULL;
        }

        mmdata = t->data;
        mmlen = t->length;
        if (mmcmp_unpack(&mmdata, &mmlen)) {
                // clean up the existing data
                if (t->data && t->closure) {
                        t->closure(t);
                }
                // and put the new stuff in
                t->length = mmlen;
                t->data = mmdata;
                t->closure = _slurp_closure_free;
        }

        // TODO re-add PP20 unpacker, possibly also handle other formats?

        return t;
}


void unslurp(slurp_t * t)
{
        if (!t)
                return;
        if (t->data && t->closure) {
                t->closure(t);
        }
        free(t);
}

/* --------------------------------------------------------------------- */

int slurp_seek(slurp_t *t, long offset, int whence)
{
        switch (whence) {
        default:
        case SEEK_SET:
                break;
        case SEEK_CUR:
                offset += t->pos;
                break;
        case SEEK_END:
                offset += t->length;
                break;
        }
        if (offset < 0 || (size_t) offset > t->length)
                return -1;
        t->pos = offset;
        return 0;
}

long slurp_tell(slurp_t *t)
{
        return (long) t->pos;
}

size_t slurp_read(slurp_t *t, void *ptr, size_t count)
{
        size_t bytesleft = t->length - t->pos;
        if (count > bytesleft) {
                // short read -- fill in any extra bytes with zeroes
                size_t tail = count - bytesleft;
                count = bytesleft;
                memset(ptr + count, 0, tail);
        }
        if (count)
                memcpy(ptr, t->data + t->pos, count);
        t->pos += count;
        return count;
}

int slurp_getc(slurp_t *t)
{
        return (t->pos < t->length) ? t->data[t->pos++] : EOF;
}

int slurp_eof(slurp_t *t)
{
        return t->pos >= t->length;
}

