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

/* support code for xz decompression */

#include "headers.h"

#include "fmt.h"
#include "loadso.h"
#include "mem.h"
#include "slurp.h"

#ifdef SCHISM_WIN32
typedef long off_t;
#endif

#include <lzma.h>

static int xz_isinit = 0;

#define CHUNK_SIZE (4096)

/* private storage */
struct slurp_xz {
	/* the liblzma stream. */
	lzma_stream lz;
};

static const char *(*XZ_lzma_version_string)(void);
static lzma_ret (*XZ_lzma_stream_decoder)(lzma_stream *strm, uint64_t memlimit, uint32_t flags);
static lzma_ret (*XZ_lzma_code)(lzma_stream *strm, lzma_action action);
static void (*XZ_lzma_end)(lzma_stream *strm);

static void *slurp_xz_start(void)
{
	struct slurp_xz *zl = mem_calloc(1, sizeof(*zl));

	if (XZ_lzma_stream_decoder(&zl->lz, UINT64_MAX, 0) != LZMA_OK) {
		free(zl);
		return NULL;
	}

	return zl;
}

static int slurp_xz_inflate(void *opaque)
{
	struct slurp_xz *zl = opaque;

	switch (XZ_lzma_code(&zl->lz, LZMA_RUN)) {
	case LZMA_STREAM_END:
		return SLURP_DEC_DONE;
	case LZMA_OK:
		return SLURP_DEC_OK;
	default:
		return -1; /* uh oh */
	}
}

static void slurp_xz_end(void *opaque)
{
	struct slurp_xz *zl = opaque;

	XZ_lzma_end(&zl->lz);
	free(zl);
}

static size_t slurp_xz_output(void *opaque, void *buf, size_t len)
{
	struct slurp_xz *zl = opaque;

	if (buf) {
		zl->lz.next_out = buf;
		zl->lz.avail_out = len;
	}

	return zl->lz.avail_out;
}

static size_t slurp_xz_input(void *opaque, void *buf, size_t len)
{
	struct slurp_xz *zl = opaque;

	if (buf) {
		zl->lz.next_in = buf;
		zl->lz.avail_in = len;
	}

	return zl->lz.avail_in;
}

static const struct slurp_decompress_vtable vtbl = {
	slurp_xz_start,
	slurp_xz_inflate,
	slurp_xz_end,
	slurp_xz_output,
	slurp_xz_input,
};

int slurp_xz(slurp_t *src)
{
	if (!xz_isinit)
		return -1;

	return slurp_decompress(src, &vtbl);
}

#ifndef LZMA_DYNAMIC_LOAD
# define XZ_GLOBALS
# define XZ_START
# define XZ_SYM(x) XZ_##x = x
# define XZ_END
#else
# define XZ_GLOBALS static void *lib_lzma;
# define XZ_START \
	 do { \
		 lib_lzma = library_load("lzma", LZMA_VERSION_MAJOR, 0); \
		 if (!lib_lzma) \
			 return -2; \
	 } while (0)
# define XZ_SYM(x) \
	 do { \
		 XZ_##x = loadso_function_load(lib_lzma, #x); \
		 if (!XZ_##x) { \
			 printf("%s\n", #x); \
			 return -1; \
		 } \
	 } while (0)
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
