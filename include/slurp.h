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

#ifndef SCHISM_SLURP_H
#define SCHISM_SLURP_H

#include "headers.h"

#include <stdint.h>
#include <sys/stat.h> /* struct stat */

/* --------------------------------------------------------------------- */

enum {
	SLURP_OPEN_IGNORE  = -1,
	SLURP_OPEN_FAIL    =  0,
	SLURP_OPEN_SUCCESS =  1,
};

typedef struct slurp_struct_ slurp_t;
struct slurp_struct_ {
	/* stdio-style interfaces */
	int (*seek)(slurp_t *, int64_t, int);
	int64_t (*tell)(slurp_t *);
	size_t (*peek)(slurp_t *, void *, size_t);
	size_t (*read)(slurp_t *, void *, size_t);
	int (*eof)(slurp_t *);

	/* clean up after ourselves */
	void (*closure)(slurp_t *);

	/* receive data in a callback function; keeps away useless allocation for memory mapping */
	int (*receive)(slurp_t *, int (*callback)(const void *, size_t, void *), size_t length, void *userdata);

	union {
		struct {
			unsigned char *data;
			size_t length;
			size_t pos;

			/* for specific interfaces that are all "memory-based" */
			union {
				struct {
					void *file;
					void *mapping;
				} win32;

				struct {
					int fd;
				} mmap;
			} interfaces;
		} memory;

		struct {
			/* only contains this (for now i guess) */
			FILE *fp;
		} stdio;
	} internal;
};

/* --------------------------------------------------------------------- */

/* slurp receives a pointer to a user-allocated structure, returns a negative integer, and sets
errno on error. 'buf' is only meaningful if you've already stat()'d the file; in most cases it
can simply be NULL. If size is nonzero, it overrides the file's size as returned by stat -- this
can be used to read only part of a file, or if the file size is known but a stat structure is not
available. */
int slurp(slurp_t *t, const char *filename, struct stat *buf, size_t size);

/* initializes a slurp_t over an existing memory stream */
int slurp_memstream(slurp_t *t, uint8_t *mem, size_t memsize);
int slurp_memstream_free(slurp_t *t, uint8_t *mem, size_t memsize);

void unslurp(slurp_t *t);

#ifdef SCHISM_WIN32
int slurp_win32(slurp_t *useme, const char *filename, size_t st);
#endif

#if HAVE_MMAP
int slurp_mmap(slurp_t *useme, const char *filename, size_t st);
#endif

/* stdio-style file processing */
int slurp_seek(slurp_t *t, int64_t offset, int whence); /* whence => SEEK_SET, SEEK_CUR, SEEK_END */
int64_t slurp_tell(slurp_t *t);
#define slurp_rewind(t) slurp_seek((t), 0, SEEK_SET)

size_t slurp_read(slurp_t *t, void *ptr, size_t count); /* i never really liked fread */
size_t slurp_peek(slurp_t *t, void *ptr, size_t count);
int slurp_getc(slurp_t *t); /* returns unsigned char cast to int, or EOF */
int slurp_eof(slurp_t *t);  /* 1 = end of file */
int slurp_receive(slurp_t *t, int (*callback)(const void *, size_t, void *), size_t count, void *userdata);

size_t slurp_length(slurp_t *t);

#endif /* SCHISM_SLURP_H */
