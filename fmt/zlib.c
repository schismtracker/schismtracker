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
	/* the original file as passed into slurp_zlib */
	slurp_t fp;

	/* the zlib stream. */
	z_stream zs;

	/* stupid gzip doesn't discard the header? */
	gz_header gz;

	unsigned char buf[4096];

	/* error flag */
	unsigned int err : 1;
	unsigned int done : 1;
};

static const char *(*ZLIB_zlibVersion)(void);
static int (*ZLIB_inflateInit2_)(z_streamp strm, int windowBits, const char *version, int stream_size);
static int (*ZLIB_inflate)(z_streamp strm, int flush);
static int (*ZLIB_inflateEnd)(z_streamp strm);
static int (*ZLIB_inflateGetHeader)(z_streamp strm, gz_headerp head);

static size_t slurp_zlib_read(void *opaque, disko_t *ds, size_t size)
{
	struct slurp_zlib *zl = opaque;
	size_t i;

	if (zl->err || zl->done)
		return 0; /* FUN! */

	zl->zs.next_out = disko_memstart(ds, size);
	zl->zs.avail_out = size;

	if (!zl->zs.next_out) {
		zl->err = 1;
		return 0; /* memory error */
	}

	while (zl->zs.avail_out > 0) {
		int res;

		if (zl->zs.avail_in == 0) {
			/* need more data, or we just started. */
			size_t z = slurp_read(&zl->fp, zl->buf, sizeof(zl->buf));
			if (!z) {
				zl->err = 1;
				goto ZL_end;
			}

			zl->zs.next_in = zl->buf;
			zl->zs.avail_in = z;
		}

		/* inflate the mio */
		res = ZLIB_inflate(&zl->zs, Z_NO_FLUSH);
		if (res == Z_STREAM_END) {
			zl->done = 1;
			break;
		}

		if (res == Z_OK)
			continue;

		/* something has gone totally wrong, OOPS! */
		zl->err = 1;
		goto ZL_end;
	}

ZL_end:
	disko_memend(ds, zl->zs.next_out, size - zl->zs.avail_out);

	return size - zl->zs.avail_out;
}

static void slurp_zlib_closure(void *opaque)
{
	struct slurp_zlib *zl = opaque;

	ZLIB_inflateEnd(&zl->zs);
	unslurp(&zl->fp);
	free(zl);
}

int slurp_gzip(slurp_t *src)
{
	int flags;
	unsigned char magic[2];
	struct slurp_zlib *zl;
	size_t i;

	if (!zlib_isinit)
		return -1;

	zl = mem_calloc(1, sizeof(*zl));

	zl->zs.next_in = Z_NULL;
	zl->zs.avail_in = 0;
	zl->zs.zalloc = Z_NULL;
	zl->zs.zfree = Z_NULL;
	zl->zs.opaque = Z_NULL;

	if (ZLIB_inflateInit2_(&zl->zs, 15 + 16, ZLIB_VERSION, sizeof(zl->zs)) != Z_OK) {
		free(zl);
		return -1;
	}

	ZLIB_inflateGetHeader(&zl->zs, &zl->gz);

	memcpy(&zl->fp, src, sizeof(slurp_t));

	slurp_init_nonseek(src, slurp_zlib_read, slurp_zlib_closure, zl);

	/* read a bit to ensure we've actually got the right thing.
	 * zlib won't complain if our file Isn't Correct, so we have
	 * to do it ourselves. */
	slurp_available(src, 8096, SEEK_SET);

	/* check the error flag. if it's set, we're toast.
	 * if it was set twice, our whole lives are different than
	 * we would've been otherwise. */
	if (zl->err) {
		/* TODO please find a better way to do this.
		 * this prevents memleaks, but is utterly deranged */
		slurp_t tmp;

		memcpy(&tmp, &zl->fp, sizeof(slurp_t));

		/* prevent original fp from actually being closed */
		zl->fp.closure = NULL;
		unslurp(src);

		/* roll it back */
		memcpy(src, &tmp, sizeof(slurp_t));

		return -1;
	}

	return 0;
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
