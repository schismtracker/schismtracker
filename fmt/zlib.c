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

#include <zlib.h>

static int zlib_isinit = 0;

#define CHUNK_SIZE (4096)

/* private storage */
struct slurp_zlib {
	/* the zlib stream. */
	z_stream zs;

	/* stupid gzip doesn't discard the header? */
	gz_header gz;
};

static const char *(*ZLIB_zlibVersion)(void);
static int (*ZLIB_inflateInit2_)(z_streamp strm, int windowBits, const char *version, int stream_size);
static int (*ZLIB_inflate)(z_streamp strm, int flush);
static int (*ZLIB_inflateEnd)(z_streamp strm);
static int (*ZLIB_inflateGetHeader)(z_streamp strm, gz_headerp head);

static void *slurp_zlib_start(void)
{
	struct slurp_zlib *zl = mem_calloc(1, sizeof(*zl));

	if (ZLIB_inflateInit2_(&zl->zs, 15 + 16, ZLIB_VERSION, sizeof(zl->zs)) != Z_OK) {
		free(zl);
		return NULL;
	}

	ZLIB_inflateGetHeader(&zl->zs, &zl->gz);
	return zl;	
}

static int slurp_zlib_inflate(void *opaque)
{
	struct slurp_zlib *zl = opaque;

	switch (ZLIB_inflate(&zl->zs, Z_NO_FLUSH)) {
	case Z_STREAM_END:
		return SLURP_DEC_DONE;
	case Z_OK:
		return SLURP_DEC_OK;
	default:
		return -1; /* uh oh */
	}
}

static void slurp_zlib_end(void *opaque)
{
	struct slurp_zlib *zl = opaque;

	ZLIB_inflateEnd(&zl->zs);
}

static size_t slurp_zlib_output(void *opaque, void *buf, size_t len)
{
	struct slurp_zlib *zl = opaque;

	if (buf) {
		zl->zs.next_out = buf;
		zl->zs.avail_out = len;
	}

	return zl->zs.avail_out;
}

static size_t slurp_zlib_input(void *opaque, void *buf, size_t len)
{
	struct slurp_zlib *zl = opaque;

	if (buf) {
		zl->zs.next_in = buf;
		zl->zs.avail_in = len;
	}

	return zl->zs.avail_in;
}

static const struct slurp_decompress_vtable vtbl = {
	slurp_zlib_start,
	slurp_zlib_inflate,
	slurp_zlib_end,
	slurp_zlib_output,
	slurp_zlib_input,
};

int slurp_gzip(slurp_t *src)
{
	if (!zlib_isinit)
		return -1;

	return slurp_decompress(src, &vtbl);
}

#ifdef LINK_TO_ZLIB
# define GZIP_GLOBALS
# define GZIP_START
# define GZIP_SYM(x) ZLIB_##x = x
# define GZIP_END
#else
# define GZIP_GLOBALS \
	static void *lib_z;
# define GZIP_START \
	do { lib_z = library_load("z", ZLIB_VERNUM >> 12, 0); if (!lib_z) return -2; } while (0)
# define GZIP_SYM(x) \
	do { ZLIB_##x = loadso_function_load(lib_z, #x); if (!ZLIB_##x) { printf("%s\n", #x); return -1; } } while (0)
# define GZIP_END \
do { \
	loadso_object_unload(lib_z); \
	lib_z = NULL; \
} while (0)
#endif

GZIP_GLOBALS

static int gzip_dlinit(void)
{
	GZIP_START;

	GZIP_SYM(zlibVersion);
	GZIP_SYM(inflateInit2_);
	GZIP_SYM(inflate);
	GZIP_SYM(inflateEnd);
	GZIP_SYM(inflateGetHeader);

	return 0;
}

static void gzip_dlend(void)
{
	GZIP_END;
}

#undef GZIP_GLOBALS
#undef GZIP_START
#undef GZIP_SYM
#undef GZIP_END

int gzip_init(void)
{
	/* "The application can compare zlibVersion and ZLIB_VERSION for
	 * consistency. If the first character differs, the library code actually
	 * used is not compatible with the zlib.h header file used by the
	 * application." */
	int r;

	r = gzip_dlinit();
	if (r < 0)
		return -1;

	if (ZLIB_zlibVersion()[0] != ZLIB_VERSION[0])
		return -1;

	zlib_isinit = 1;
	return 0;
}

void gzip_quit(void)
{
	if (zlib_isinit) {
		gzip_dlend();
		zlib_isinit = 0;
	}
}
