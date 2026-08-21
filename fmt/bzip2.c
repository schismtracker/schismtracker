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

/* support code for bzip2 compression
 *
 * this is fairly similar to the gzip code, mostly because the libraries
 * are extremely similar in general.
 *
 * Also, as I'm writing this, the US government has shut down and the
 * bzip2 documentation has an annoying banner saying that the information
 * on the website may not be up to date. Nice! */

#include "headers.h"

#include "fmt.h"
#include "loadso.h"
#include "mem.h"
#include "slurp.h"

#include <bzlib.h>

/* we expect major version 1 */
#define BZIP2_LIB_MAJOR_VERSION (1)

#define CHUNK_SIZE (4096)

struct slurp_bzip2 {
	/* the bzip2 stream */
	bz_stream bz;
};

static int bzip2_isinit = 0;

static const char *(*BZIP2_bzlibVersion)(void) = NULL;
static int (*BZIP2_bzDecompressInit)(bz_stream *strm, int verbosity, int small);
static int (*BZIP2_bzDecompress)(bz_stream *strm);
static int (*BZIP2_bzDecompressEnd)(bz_stream *strm);

static void *slurp_bzip2_start(void)
{
	struct slurp_bzip2 *zl = mem_calloc(1, sizeof(*zl));

	if (BZIP2_bzDecompressInit(&zl->bz, 0, 0) != BZ_OK) {
		free(zl);
		return NULL;
	}

	return zl;
}

static int slurp_bzip2_inflate(void *opaque)
{
	struct slurp_bzip2 *zl = opaque;

	switch (BZIP2_bzDecompress(&zl->bz)) {
	case BZ_STREAM_END:
		return SLURP_DEC_DONE;
	case BZ_OK:
		return SLURP_DEC_OK;
	default:
		return -1; /* uh oh */
	}
}

static void slurp_bzip2_end(void *opaque)
{
	struct slurp_bzip2 *zl = opaque;

	BZIP2_bzDecompressEnd(&zl->bz);
	free(zl);
}

static size_t slurp_bzip2_output(void *opaque, void *buf, size_t len)
{
	struct slurp_bzip2 *zl = opaque;

	if (buf) {
		zl->bz.next_out = buf;
		zl->bz.avail_out = len;
	}

	return zl->bz.avail_out;
}

static size_t slurp_bzip2_input(void *opaque, void *buf, size_t len)
{
	struct slurp_bzip2 *zl = opaque;

	if (buf) {
		zl->bz.next_in = buf;
		zl->bz.avail_in = len;
	}

	return zl->bz.avail_in;
}

static const struct slurp_decompress_vtable vtbl = {
	slurp_bzip2_start,
	slurp_bzip2_inflate,
	slurp_bzip2_end,
	slurp_bzip2_output,
	slurp_bzip2_input,
};

int slurp_bzip2(slurp_t *src)
{
	if (!bzip2_isinit)
		return -1;

	return slurp_decompress(src, &vtbl);
}

#ifdef LINK_TO_ZLIB
# define BZIP2_GLOBALS
# define BZIP2_START
# define BZIP2_SYM(x) BZIP2_##x = x
# define BZIP2_END
#else
# define BZIP2_GLOBALS static void *lib_bz2;
# define BZIP2_START \
	 do { \
		 lib_bz2 = library_load("bz2", 1, 0); \
		 if (!lib_bz2) \
			 return -2; \
	 } while (0)
# define BZIP2_SYM(x) \
	 do { \
		 BZIP2_##x = loadso_function_load(lib_bz2, "BZ2_" #x); \
		 if (!BZIP2_##x) { \
			 printf("%s\n", #x); \
			 return -1; \
		 } \
	 } while (0)
# define BZIP2_END \
	 do { \
		 loadso_object_unload(lib_bz2); \
		 lib_bz2 = NULL; \
	 } while (0)
#endif

BZIP2_GLOBALS

static int bzip2_dlinit(void)
{
	BZIP2_START;

	BZIP2_SYM(bzlibVersion);
	BZIP2_SYM(bzDecompressInit);
	BZIP2_SYM(bzDecompress);
	BZIP2_SYM(bzDecompressEnd);

	return 0;
}

static void bzip2_dlend(void)
{
	BZIP2_END;
}

#undef BZIP2_GLOBALS
#undef BZIP2_START
#undef BZIP2_SYM
#undef BZIP2_END

int bzip2_init(void)
{
	int r;

	r = bzip2_dlinit();
	if (r < 0)
		return -1;

	if ((BZIP2_bzlibVersion()[0] - '0') != BZIP2_LIB_MAJOR_VERSION) {
		bzip2_dlend();
		return -1;
	}

	bzip2_isinit = 1;
	return 0;
}

void bzip2_quit(void)
{
	if (bzip2_isinit) {
		bzip2_dlend();
		bzip2_isinit = 0;
	}
}
