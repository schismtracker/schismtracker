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

/* support code for zstd compression (zstd)
 *
 * this is kind of complicated; but the basic gist is, we ONLY want to
 * decompress as much as we have to. */

#include "headers.h"

#include "fmt.h"
#include "loadso.h"
#include "mem.h"
#include "slurp.h"

#include <zstd.h>

static int zstd_isinit = 0;

#define CHUNK_SIZE (4096)

/* private storage */
struct slurp_zstd {
	/* the zstd stream. */
	ZSTD_DStream *dstr;

	ZSTD_inBuffer inbuf;
	ZSTD_outBuffer outbuf;
};

static ZSTD_DStream *(*pZSTD_createDStream)(void);
static size_t (*pZSTD_freeDStream)(ZSTD_DStream *zds);
static size_t (*pZSTD_decompressStream)(ZSTD_DStream *zds, ZSTD_outBuffer *output, ZSTD_inBuffer *input);
static size_t (*pZSTD_DStreamInSize)(void);
static size_t (*pZSTD_DStreamOutSize)(void);
static unsigned (*pZSTD_versionNumber)(void);
static unsigned (*pZSTD_isError)(size_t result);

static void *slurp_zstd_start(void)
{
	struct slurp_zstd *zl = mem_calloc(1, sizeof(*zl));

	zl->dstr = pZSTD_createDStream();
	if (!zl->dstr) {
		free(zl);
		return NULL;
	}

	return zl;
}

static int slurp_zstd_inflate(void *opaque)
{
	struct slurp_zstd *zl = opaque;
	size_t r;

	r = pZSTD_decompressStream(zl->dstr, &zl->outbuf, &zl->inbuf);

	if (pZSTD_isError(r)) {
		return -1;
	} else if (r > 0) {
		return SLURP_DEC_OK;
	} /* else... */

	return SLURP_DEC_OK_OR_DONE;
}

static void slurp_zstd_end(void *opaque)
{
	struct slurp_zstd *zl = opaque;

	pZSTD_freeDStream(zl->dstr);
	free(zl);
}

static size_t slurp_zstd_output(void *opaque, void *buf, size_t len)
{
	struct slurp_zstd *zl = opaque;

	if (buf) {
		zl->outbuf.dst = buf;
		zl->outbuf.size = len;
		zl->outbuf.pos = 0;
	}

	return (zl->outbuf.size - zl->outbuf.pos);
}

static size_t slurp_zstd_input(void *opaque, void *buf, size_t len)
{
	struct slurp_zstd *zl = opaque;

	if (buf) {
		zl->inbuf.src = buf;
		zl->inbuf.size = len;
		zl->inbuf.pos = 0;
	}

	return (zl->inbuf.size - zl->inbuf.pos);
}

static size_t slurp_zstd_inbufsz(void)
{
	return pZSTD_DStreamInSize();
}

static size_t slurp_zstd_outbufsz(void)
{
	return pZSTD_DStreamOutSize();
}

static const struct slurp_decompress_vtable vtbl = {
	slurp_zstd_start,
	slurp_zstd_inflate,
	slurp_zstd_end,
	slurp_zstd_output,
	slurp_zstd_input,
	slurp_zstd_inbufsz,
	slurp_zstd_outbufsz,
};

int slurp_zstd(slurp_t *src)
{
	unsigned char x[4];

	if (!zstd_isinit)
		return -1;

	/* check for magic bytes */
	if (slurp_peek(src, x, 4) != 4)
		return -1;

	if (memcmp(x, "\x28\xB5\x2F\xFD", 4) != 0)
		return -1;

	return slurp_decompress(src, &vtbl);
}

#ifdef LINK_TO_ZSTD
# define ZSTD_GLOBALS
# define ZSTD_START
# define ZSTD_SYM(x) pZSTD_##x = x
# define ZSTD_END
#else
# define ZSTD_GLOBALS static void *lib_zstd;
# define ZSTD_START \
	 do { \
		 lib_zstd = library_load("zstd", ZSTD_VERSION_MAJOR, 0); \
		 if (!lib_zstd) \
			 return -2; \
	 } while (0)
# define ZSTD_SYM(x) \
	 do { \
		 pZSTD_##x = loadso_function_load(lib_zstd, "ZSTD_" #x); \
		 if (!pZSTD_##x) { \
			 printf("%s\n", #x); \
			 return -1; \
		 } \
	 } while (0)
# define ZSTD_END \
	 do { \
		 loadso_object_unload(lib_zstd); \
		 lib_zstd = NULL; \
	 } while (0)
#endif

ZSTD_GLOBALS

static int zstd_dlinit(void)
{
	ZSTD_START;

	ZSTD_SYM(createDStream);
	ZSTD_SYM(freeDStream);
	ZSTD_SYM(decompressStream);
	ZSTD_SYM(DStreamInSize);
	ZSTD_SYM(DStreamOutSize);
	ZSTD_SYM(versionNumber);
	ZSTD_SYM(isError);

	return 0;
}

static void zstd_dlend(void)
{
	ZSTD_END;
}

#undef ZSTD_GLOBALS
#undef ZSTD_START
#undef ZSTD_SYM
#undef ZSTD_END

int zstd_init(void)
{
	/* "The application can compare zstdVersion and ZSTD_VERSION for
	 * consistency. If the first character differs, the library code actually
	 * used is not compatible with the zstd.h header file used by the
	 * application." */
	int r;

	r = zstd_dlinit();
	if (r < 0)
		return -1;

	if ((pZSTD_versionNumber() / 10000) != ZSTD_VERSION_MAJOR)
		return -1;

	zstd_isinit = 1;
	return 0;
}

void zstd_quit(void)
{
	if (zstd_isinit) {
		zstd_dlend();
		zstd_isinit = 0;
	}
}
