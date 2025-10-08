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
#include "slurp.h"
#include "mem.h"
#include "loadso.h"

#include <bzlib.h>

/* we expect major version 1 */
#define BZIP2_LIB_MAJOR_VERSION (1)

#define CHUNK_SIZE (4096)

struct slurp_bzip2 {
	/* the original file as passed into slurp_bzip2 */
	slurp_t fp;

	/* the bzip2 stream */
	bz_stream bz;

	unsigned char inbuf[CHUNK_SIZE];
	unsigned char outbuf[CHUNK_SIZE];

	unsigned int err : 1;
	unsigned int done : 1;
};

static int bzip2_isinit = 0;

static const char *(*BZIP2_bzlibVersion)(void) = NULL;
static int (*BZIP2_bzDecompressInit)(bz_stream *strm, int verbosity, int small);
static int (*BZIP2_bzDecompress)(bz_stream *strm);
static int (*BZIP2_bzDecompressEnd)(bz_stream *strm);

static size_t slurp_bzip2_read(void *opaque, disko_t *ds, size_t size)
{
	struct slurp_bzip2 *bz = opaque;

	if (bz->err || bz->done)
		return 0;

	bz->bz.next_out = bz->outbuf;
	bz->bz.avail_out = CHUNK_SIZE;

	while (bz->bz.avail_out > 0) {
		int res;

		if (bz->bz.avail_in == 0) {
			/* need more data, or we just started. */
			size_t z = slurp_read(&bz->fp, bz->inbuf, CHUNK_SIZE);
			if (!z)
				return 0; /* Truncated? */

			bz->bz.next_in = bz->inbuf;
			bz->bz.avail_in = z;
		}

		res = BZIP2_bzDecompress(&bz->bz);
		if (res == BZ_STREAM_END) {
			bz->done = 1;
			break;
		}

		if (res == BZ_OK)
			continue;

		/* we've hit some kind of error, punt */
		bz->err = 1;
		return 0;
	}

	disko_write(ds, bz->outbuf, CHUNK_SIZE - bz->bz.avail_out);

	return (CHUNK_SIZE - bz->bz.avail_out);
}

static void slurp_bzip2_closure(void *opaque)
{
	struct slurp_bzip2 *bz = opaque;

	BZIP2_bzDecompressEnd(&bz->bz);
	unslurp(&bz->fp);
	free(bz);
}

int slurp_bzip2(slurp_t *src)
{
	int flags;
	unsigned char magic[2];
	struct slurp_bzip2 *bz;
	size_t i;

	if (!bzip2_isinit)
		return -1;

	bz = mem_calloc(1, sizeof(*bz));

	if (BZIP2_bzDecompressInit(&bz->bz, 0, 0) != BZ_OK) {
		free(bz);
		return -1;
	}

	memcpy(&bz->fp, src, sizeof(slurp_t));

	slurp_init_nonseek(src, slurp_bzip2_read, slurp_bzip2_closure, bz);

	/* read a bit to ensure we've actually got the right thing.
	 * bzip2 won't complain if our file Isn't Correct, so we have
	 * to do it ourselves. */
	slurp_available(src, 8096, SEEK_SET);

	/* check the error flag. if it's set, we're toast.
	 * if it was set twice, our whole lives are different than
	 * we would've been otherwise. */
	if (bz->err) {
		/* roll it back */
		memcpy(src, &bz->fp, sizeof(slurp_t));

		BZIP2_bzDecompressEnd(&bz->bz);
		free(bz);
		return -1;
	}

	return 0;
}

#ifdef LINK_TO_ZLIB
# define BZIP2_GLOBALS
# define BZIP2_START
# define BZIP2_SYM(x) BZIP2_##x = x
# define BZIP2_END
#else
# define BZIP2_GLOBALS \
	static void *lib_bz2;
# define BZIP2_START \
	do { lib_bz2 = library_load("bz2", 1, 0); if (!lib_bz2) return -2; } while (0)
# define BZIP2_SYM(x) \
	do { BZIP2_##x = loadso_function_load(lib_bz2, "BZ2_" #x); if (!BZIP2_##x) { printf("%s\n", #x); return -1; } } while (0)
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
	if (r < 0) {
		printf("failed to load...\n");
		return -1;
	}

	if (BZIP2_bzlibVersion()[0] != '1')
		return -1;

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
