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

/* --------------------------------------------------------------------- */

enum {
	SLURP_OPEN_IGNORE  = -1,
	SLURP_OPEN_FAIL    =  0,
	SLURP_OPEN_SUCCESS =  1,
};

struct slurp_nonseek;

typedef struct slurp_struct_ slurp_t;
struct slurp_struct_ {
	/* stdio-style interfaces:
	 * - seek and tell are not required to be implemented, but DO
	 *   implement them if you can. many functions in schism require it,
	 *   and if you don't, slurp will emulate it, causing a massive
	 *   slowdown.
	 * - if seek is implemented, tell MUST ALSO be implemented, and
	 *   vice versa. otherwise the behavior is totally undefined.
	 * - peek can be NULL if read is implemented, and vice versa.
	 *   (however, if you can implement both, that is preferred)
	 * - peek is a custom schism construct that is like fread, but the
	 *   file position does not change after it is done. */
	int (*seek)(slurp_t *t, int64_t offset, int whence);
	int64_t (*tell)(slurp_t *t);
	size_t (*peek)(slurp_t *t, void *ptr, size_t count);
	size_t (*read)(slurp_t *t, void *ptr, size_t count);
	uint64_t (*length)(slurp_t *t);
	int (*available)(slurp_t *t, size_t x, int whence);
	/* TODO: absolute seek for e.g. memory streams. */

	/* this one is optional, and slurp will emulate stdio behavior if it's NULL */
	int (*eof)(slurp_t *);

	/* clean up after ourselves (optional, can be NULL) */
	void (*closure)(slurp_t *);

	/* receive data in a callback function; keeps away useless allocation when memory mapping.
	 * (optional, can be NULL) */
	int (*receive)(slurp_t *, int (*callback)(const void *, size_t, void *), size_t length, void *userdata);

	/* used internally to mark position for slurp_limit() */
	int64_t limit;

	unsigned int eof_ : 1; /* need THIS to emulate the EOF flag, for impls without it */

	/* only allocated if we're a non-seekable structure. */
	struct slurp_nonseek *nonseek;

	union {
		struct {
			const unsigned char *data;
			const unsigned char *data2; /* for 2mem (this allows us to share tell,seek,length impl) */
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

			/* in lieu of a simple and fast way to get the
			 * length of a stream (have to do gymnastics),
			 * cache this on open. if it gets changed, we'll
			 * probably fail anyway. */
			int64_t length; /* 64-bit for large file support */
		} stdio;

		struct {
			/* only used for sf2 */
			slurp_t *src;

			int64_t pos;

			struct {
				int64_t off;
				uint64_t len;
			} data[2];
			int current; /* which data is currently being used */

			/* original position from before we mutilated it */
			int64_t origpos;
		} sf2;

		struct {
			void *handle;
		} win32;
	} internal;
};

/* --------------------------------------------------------------------- */

/* slurp receives a pointer to a user-allocated structure, returns a negative integer, and sets
errno on error. 'buf' is only meaningful if you've already stat()'d the file; in most cases it
can simply be NULL. If size is nonzero, it overrides the file's size as returned by stat -- this
can be used to read only part of a file, or if the file size is known but a stat structure is not
available. */
int slurp(slurp_t *t, const char *filename, struct stat *buf, uint64_t size);

/* initializes a slurp_t over an existing file */
int slurp_stdio(slurp_t *t, FILE *fp);

/* initializes a slurp_t over an existing memory stream */
int slurp_memstream(slurp_t *t, const uint8_t *mem, size_t memsize);
int slurp_memstream_free(slurp_t *t, uint8_t *mem, size_t memsize);

/* Binds two memory streams together.
 * Both streams must be of the exact same size. */
int slurp_2memstream(slurp_t *t, const uint8_t *mem1, const uint8_t *mem2, size_t memsize);

/* Binds two separate parts of an existing stream together.
 * unslurp() should be called here. */
void slurp_sf2(slurp_t *s, slurp_t *in, int64_t off1, size_t len1,
	int64_t off2, size_t len2);

void unslurp(slurp_t *t);

#ifdef SCHISM_WIN32
int slurp_win32_mmap(slurp_t *useme, const char *filename, uint64_t st);
int slurp_win32(slurp_t *s, const char *filename, uint64_t st);
#endif

#if HAVE_MMAP
int slurp_mmap(slurp_t *useme, const char *filename, uint64_t st);
#endif

/* stdio-style file processing */
int slurp_seek(slurp_t *t, int64_t offset, int whence); /* whence => SEEK_SET, SEEK_CUR, SEEK_END */
int64_t slurp_tell(slurp_t *t);
#define slurp_rewind(t) slurp_seek((t), 0, SEEK_SET)

/* these two functions will always fill in the entire buffer pointed to by ptr (any data not covered
 * by the backend will be memset to zero) */
size_t slurp_read(slurp_t *t, void *ptr, size_t count);
size_t slurp_peek(slurp_t *t, void *ptr, size_t count);

int slurp_getc(slurp_t *t); /* returns unsigned char cast to int, or EOF */
int slurp_eof(slurp_t *t);  /* 1 = end of file */
int slurp_receive(slurp_t *t, int (*callback)(const void *, size_t, void *), size_t count, void *userdata);

/* can never fail (hopefully...) */
uint64_t slurp_length(slurp_t *t);

/* creates a wall, relative to the current position
 * any reads that try to go after that point will be filled with zeroes */
void slurp_limit(slurp_t *t, int64_t wall);
void slurp_unlimit(slurp_t *t);
/* does the same as slurp_unlimit, but seeks to the wall itself */
void slurp_unlimit_seek(slurp_t *t);

/* strcspn equivalent */
int slurp_skip_chars(slurp_t *fp, const char *str);
/* strspn equivalent */
int slurp_skip_until_chars(slurp_t *fp, const char *str);

struct disko;

/* stupid nonseek hack */
int slurp_init_nonseek(slurp_t *fp,
	size_t (*read_func)(void *opaque, struct disko *ds, size_t count),
	void (*closure)(void *opaque),
	void *opaque);

#ifdef USE_ZLIB
/* in fmt/gzip.c  .... */
int slurp_gzip(slurp_t *src);
#endif

#ifdef USE_BZIP2
/* fmt/bzip2.c */
int slurp_bzip2(slurp_t *src);
#endif

#ifdef USE_LZMA
/* fmt/xz.c */
int slurp_xz(slurp_t *src);
#endif

#ifdef USE_ZSTD
/* fmt/zstd.c */
int slurp_zstd(slurp_t *src);
#endif

int slurp_available(slurp_t *fp, size_t x, int whence);

/* ------------------------------------------------------------------------ */

#define SLURP_DEC_OK (0)
#define SLURP_DEC_DONE (1)
/* might be either... */
#define SLURP_DEC_OK_OR_DONE (2)

struct slurp_decompress_vtable {
	/* returns an opaque pointer that represents the inflate */
	void * (*start)(void);
	/* negative return value = error */
	int (*inflate)(void *opaque);
	/* kills the opaque pointer off */
	void (*end)(void *opaque);
	/* These actually serve two purposes.
	 * If buf is NULL, then it should return the currently
	 * available buffer length, and `len` is ignored.
	 * Otherwise, it sets the current buffers for input
	 * **without copying them**, i.e. the pointers should
	 * remain valid until the last call to inflate before
	 * calling these functions again. */
	size_t (*output)(void *opaque, void *buf, size_t len);
	size_t (*input)(void *opaque, void *buf, size_t len);

	/* Minimum recommended outbuf size */
	size_t (*outbufsz)(void);
	/* Minimum recommended inbuf size */
	size_t (*inbufsz)(void);
};

int slurp_decompress(slurp_t *fp, const struct slurp_decompress_vtable *vtbl);

#endif /* SCHISM_SLURP_H */
