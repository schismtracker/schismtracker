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

/* support code for xz decompression
 *
 * TODO this code is duplicated with minor changes within fmt/zlib.c and
 * fmt/bzip2.c, we should be able to combine the four with some kind of
 * abstraction layer. The APIs are all *extremely* similar. */

#include "headers.h"

#include "fmt.h"
#include "slurp.h"
#include "mem.h"
#include "loadso.h"

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

/* support code for zlib compression (gzip)
 *
 * this is kind of complicated; but the basic gist is, we ONLY want to
 * decompress as much as we have to. */

#include "headers.h"

#include "fmt.h"
#include "slurp.h"
#include "mem.h"
#include "loadso.h"

#ifdef SCHISM_WIN32
typedef long off_t;
#endif

#include <lzma.h>

static int xz_isinit = 0;

#define CHUNK_SIZE (4096)

/* private storage */
struct slurp_xz {
	/* the original file as passed into slurp_xz */
	slurp_t fp;

	/* the liblzma stream. */
	lzma_stream lz;

	unsigned char inbuf[CHUNK_SIZE];
	unsigned char outbuf[CHUNK_SIZE];

	/* error flag */
	unsigned int err : 1;
	unsigned int done : 1;
};

static const char *(*XZ_lzma_version_string)(void);
static lzma_ret (*XZ_lzma_stream_decoder)(lzma_stream *strm, uint64_t memlimit, uint32_t flags);
static lzma_ret (*XZ_lzma_code)(lzma_stream *strm, lzma_action action);
static void (*XZ_lzma_end)(lzma_stream *strm);

static size_t slurp_xz_read(void *opaque, disko_t *ds, size_t size)
{
	struct slurp_xz *zl = opaque;
	size_t i;

	if (zl->err || zl->done)
		return 0; /* FUN! */

	zl->lz.next_out = zl->outbuf;
	zl->lz.avail_out = CHUNK_SIZE;

	while (zl->lz.avail_out > 0) {
		lzma_ret res;

		if (zl->lz.avail_in == 0) {
			/* need more data, or we just started. */
			size_t z = slurp_read(&zl->fp, zl->inbuf, CHUNK_SIZE);
			if (!z)
				return 0; /* EOF? */

			zl->lz.next_in = zl->inbuf;
			zl->lz.avail_in = z;
		}

		/* inflate the mio */
		res = XZ_lzma_code(&zl->lz, LZMA_RUN);
		if (res == LZMA_STREAM_END) {
			zl->done = 1;
			break;
		}

		if (res == LZMA_OK)
			continue;

		/* something has gone totally wrong, OOPS! */
		zl->err = 1;
		return 0;
	}

	disko_write(ds, zl->outbuf, CHUNK_SIZE - zl->lz.avail_out);

	return (CHUNK_SIZE - zl->lz.avail_out);
}

static void slurp_xz_closure(void *opaque)
{
	struct slurp_xz *zl = opaque;

	XZ_lzma_end(&zl->lz);
	unslurp(&zl->fp);
	free(zl);
}

int slurp_xz(slurp_t *src)
{
	int flags;
	unsigned char magic[2];
	struct slurp_xz *xz;
	size_t i;

	if (!xz_isinit)
		return -1;

	xz = mem_calloc(1, sizeof(*xz));

	memset(&xz->lz, 0, sizeof(xz->lz));

	/* ehhhhh */
	if (XZ_lzma_stream_decoder(&xz->lz, UINT64_MAX, 0) != LZMA_OK) {
		free(xz);
		return -1;
	}

	memcpy(&xz->fp, src, sizeof(slurp_t));

	slurp_init_nonseek(src, slurp_xz_read, slurp_xz_closure, xz);

	/* read a bit to ensure we've actually got the right thing.
	 * zlib won't complain if our file Isn't Correct, so we have
	 * to do it ourselves. */
	slurp_available(src, 8096, SEEK_SET);

	/* check the error flag. if it's set, we're toast.
	 * if it was set twice, our whole lives are different than
	 * we would've been otherwise. */
	if (xz->err) {
		/* roll it back */
		memcpy(src, &xz->fp, sizeof(slurp_t));

		XZ_lzma_end(&xz->lz);
		free(xz);
		return -1;
	}

	return 0;
}

#ifndef LZMA_DYNAMIC_LOAD
# define XZ_GLOBALS
# define XZ_START
# define XZ_SYM(x) ZLIB_##x = x
# define XZ_END
#else
# define XZ_GLOBALS \
	static void *lib_lzma;
# define XZ_START \
	do { lib_lzma = library_load("lzma", LZMA_VERSION_MAJOR, 0); if (!lib_lzma) return -2; } while (0)
# define XZ_SYM(x) \
	do { XZ_##x = loadso_function_load(lib_lzma, #x); if (!XZ_##x) { printf("%s\n", #x); return -1; } } while (0)
# define XZ_END \
do { \
	loadso_object_unload(lib_lzma); \
	lib_lzma = NULL; \
} while (0)
#endif

XZ_GLOBALS

static int xz_dlinit(void)
{
	XZ_START;

	XZ_SYM(lzma_version_string);
	XZ_SYM(lzma_stream_decoder);
	XZ_SYM(lzma_code);
	XZ_SYM(lzma_end);

	return 0;
}

static void xz_dlend(void)
{
	XZ_END;
}

#undef XZ_GLOBALS
#undef XZ_START
#undef XZ_SYM
#undef XZ_END

int xz_init(void)
{
	int r;

	r = xz_dlinit();
	if (r < 0)
		return -1;

	/* make sure compile-time version and runtime versions are the same major ver */
	if (XZ_lzma_version_string()[0] != (LZMA_VERSION_MAJOR + '0'))
		return -1;

	xz_isinit = 1;
	return 0;
}

void xz_quit(void)
{
	if (xz_isinit) {
		xz_dlend();
		xz_isinit = 0;
	}
}
