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

#include "headers.h"

#include "slurp.h"
#include "fmt.h"
#include "util.h"
#include "osdefs.h"
#include "mem.h"

/* --------------------------------------------------------------------- */

static int slurp_stdio_open_(slurp_t *t, const char *filename, uint64_t size);

int slurp(slurp_t *t, const char *filename, struct stat * buf, uint64_t size)
{
	static int (*const init_funcs[])(slurp_t *t, const char *filename, uint64_t size) = {
#ifdef SCHISM_WIN32
		slurp_win32_mmap,
#endif
#ifdef HAVE_MMAP
		slurp_mmap,
#endif
#ifdef SCHISM_WIN32
		slurp_win32,
#endif
		slurp_stdio_open_,
	};
	struct stat st;
	size_t i;

	if (!t)
		return -1;

	memset(t, 0, sizeof(*t));

	if (!strcmp(filename, "-")) {
		slurp_stdio(t, stdin);
	} else {
		if (buf) {
			st = *buf;
		} else {
			if (os_stat(filename, &st) < 0)
				return -1;
		}

		if (!size)
			size = st.st_size;

		for (i = 0; i < ARRAY_SIZE(init_funcs); i++) {
			switch (init_funcs[i](t, filename, size)) {
			case SLURP_OPEN_FAIL:
				return -1;
			case SLURP_OPEN_SUCCESS:
				goto finished;
			default:
			case SLURP_OPEN_IGNORE:
				break;
			}
		}

		/* fail */
		return -1;
	}

finished: ; /* this semicolon is important because C */
#ifdef USE_ZLIB
	/* do this before mmcmp handling, so gzip'd mmcmp'd modules
	 * will load correctly
	 *
	 * TODO is it possible to also have the reverse?
	 * maybe we should be able to handle gzip-in-gzip, but
	 * we'd have to effectively handle the case of infinite
	 * loops (such as zip bombs) */
	slurp_gzip(t);
	slurp_rewind(t);
#endif

#ifdef USE_BZIP2
	slurp_bzip2(t);
	slurp_rewind(t);
#endif

#ifdef USE_LZMA
	slurp_xz(t);
	slurp_rewind(t);
#endif

#ifdef USE_ZSTD
	slurp_zstd(t);
	slurp_rewind(t);
#endif

	uint8_t *mmdata;
	size_t mmlen;

	if (mmcmp_unpack(t, &mmdata, &mmlen)) {
		// clean up the existing data
		if (t->closure)
			t->closure(t);

		// and put the new stuff in
		slurp_memstream_free(t, mmdata, mmlen);
	}

	slurp_rewind(t);

	// TODO re-add PP20 unpacker, possibly also handle other formats?

	return 0;
}

void unslurp(slurp_t * t)
{
	if (!t)
		return;

	if (t->closure)
		t->closure(t);
}

/* --------------------------------------------------------------------- */
/* stdio implementation */

/* -- nonseek */

static size_t slurp_stdio_nonseek_read_(void *opaque, disko_t *ds, size_t z)
{
	size_t rr = 0;

	while (z > 0) {
		/* read into a temporary buffer and then append it to disko */
		char buf[512];

		size_t r = fread(buf, 1, MIN(z, 512), opaque);
		if (!r)
			return rr;

		disko_write(ds, buf, r);

		rr += r;
		z -= r;
	}

	return rr;
}

static void slurp_stdio_nonseek_closure_(void *opaque)
{
	fclose(opaque);
}

/* -- seek */

static int slurp_stdio_seek_(slurp_t *t, int64_t offset, int whence)
{
	// XXX can we use _fseeki64 on Windows?
	return fseek(t->internal.stdio.fp, offset, whence);
}

static int64_t slurp_stdio_tell_(slurp_t *t)
{
	return ftell(t->internal.stdio.fp);
}

static uint64_t slurp_stdio_length_(slurp_t *t)
{
	return t->internal.stdio.length;
}

static size_t slurp_stdio_read_(slurp_t *t, void *ptr, size_t count)
{
	return fread(ptr, 1, count, t->internal.stdio.fp);
}

static int slurp_stdio_eof_(slurp_t *t)
{
	return feof(t->internal.stdio.fp);
}

static void slurp_stdio_closure_(slurp_t *t)
{
	fclose(t->internal.stdio.fp);
}

/* -- public function */

int slurp_stdio(slurp_t *t, FILE *fp)
{
	/* stdio streams have a chance of being nonseekable. if that's true,
	 * then initialize it as a nonseekable stream. */

	if (fseek(fp, 0, SEEK_END)) {
		slurp_init_nonseek(t, slurp_stdio_nonseek_read_, slurp_stdio_nonseek_closure_, fp);
	} else {
		long end;

		memset(t, 0, sizeof(*t));

		end = ftell(fp);
		if (end < 0)
			return SLURP_OPEN_FAIL;

		/* return to monke */
		if (fseek(fp, 0, SEEK_SET))
			return SLURP_OPEN_FAIL;

		t->internal.stdio.fp = fp;
		t->internal.stdio.length = end;

		/* A BARBERSHOP HAIRCUT THAT COSTS A QUARTER */
		t->seek = slurp_stdio_seek_;
		t->tell = slurp_stdio_tell_;
		t->eof  = slurp_stdio_eof_;
		t->read = slurp_stdio_read_;
		t->length = slurp_stdio_length_;
	}

	return SLURP_OPEN_SUCCESS;
}

/* helper function for slurp() */
static int slurp_stdio_open_(slurp_t *t, const char *filename, SCHISM_UNUSED uint64_t size)
{
	FILE *fp;
	int r;

	fp = os_fopen(filename, "rb");
	if (!fp)
		return SLURP_OPEN_FAIL;

	r = slurp_stdio(t, fp);
	if (r != SLURP_OPEN_SUCCESS)
		return r;

	t->closure = slurp_stdio_closure_;
	return SLURP_OPEN_SUCCESS;
}

/* --------------------------------------------------------------------- */
/* impl for memory streams */

static int slurp_memory_seek_(slurp_t *t, int64_t offset, int whence)
{
	uint64_t len = slurp_length(t);

	switch (whence) {
	default:
	case SEEK_SET:
		break;
	case SEEK_CUR:
		offset += t->internal.memory.pos;
		break;
	case SEEK_END:
		offset += len;
		break;
	}

	if (offset < 0 || (size_t)offset > len)
		return -1;

	t->internal.memory.pos = offset;
	return 0;
}

static int64_t slurp_memory_tell_(slurp_t *t)
{
	return t->internal.memory.pos;
}

static uint64_t slurp_memory_length_(slurp_t *t)
{
	return t->internal.memory.length;
}

static size_t slurp_memory_peek_(slurp_t *t, void *ptr, size_t count)
{
	ptrdiff_t bytesleft = (ptrdiff_t)t->internal.memory.length - t->internal.memory.pos;
	if (bytesleft < 0)
		return 0;

	if (count > (size_t)bytesleft)
		count = bytesleft;

	if (count)
		memcpy(ptr, t->internal.memory.data + t->internal.memory.pos, count);

	return count;
}

static int slurp_memory_receive_(slurp_t *t, int (*callback)(const void *, size_t, void *), size_t count, void *userdata)
{
	/* xd */
	ptrdiff_t bytesleft = (ptrdiff_t)t->internal.memory.length - t->internal.memory.pos;
	if (bytesleft < 0)
		return -1;

	return callback(t->internal.memory.data + t->internal.memory.pos, MIN(bytesleft, count), userdata);
}

static void slurp_memory_closure_free_(slurp_t *t)
{
	/* data ptr is const but it was malloc'd */
	free((void *)t->internal.memory.data);
}

/* Initializes a slurp structure on an existing memory stream.
 * Does NOT free the input. */
int slurp_memstream(slurp_t *t, const uint8_t *mem, size_t memsize)
{
	memset(t, 0, sizeof(*t));

	t->seek = slurp_memory_seek_;
	t->tell = slurp_memory_tell_;
	t->peek = slurp_memory_peek_;
	t->receive = slurp_memory_receive_;
	t->length = slurp_memory_length_;

	t->internal.memory.length = memsize;
	t->internal.memory.data = mem;
	t->closure = NULL; // haha

	return 0;
}

int slurp_memstream_free(slurp_t *t, uint8_t *mem, size_t memsize)
{
	slurp_memstream(t, mem, memsize);

	t->closure = slurp_memory_closure_free_;

	return 0;
}

/* --------------------------------------------------------------------- */

/* 2mem puts two separate memory streams next to each other, and
 * acts as if they were one stream.
 *
 * This is useful for AVFoundation, as well as SoundFont2, and
 * likely others as well, as more stuff gets added. */

static size_t slurp_2mem_peek_(slurp_t *t, void *ptr, size_t count)
{
	size_t leneach, which, pos;
	ptrdiff_t bytesleft;

	bytesleft = (ptrdiff_t)t->internal.memory.length - t->internal.memory.pos;
	if (bytesleft < 0)
		return 0;

	if (count > (size_t)bytesleft)
		count = (size_t)bytesleft;

	if (!count)
		return 0;

	/* okay -- now we have to do our magic :) */

	leneach = t->internal.memory.length / 2;

	which = t->internal.memory.pos / leneach;
	pos = t->internal.memory.pos % leneach;

	if (pos + count <= leneach) {
		const unsigned char *data = (which == 0) ? t->internal.memory.data : t->internal.memory.data2;

		memcpy(ptr, data + pos, count);
	} else {
		/* XXX this branch desperately needs more testing */
		ptrdiff_t left1 = leneach - pos;

		/* this is a bug */
		SCHISM_RUNTIME_ASSERT(left1 >= 0, "logic error in 2mem implementation");

		if (left1)
			memcpy(ptr, t->internal.memory.data + pos, left1);

		memcpy((char *)ptr + left1, t->internal.memory.data2, count - left1);
	}

	return count;
}

int slurp_2memstream(slurp_t *t, const uint8_t *mem1, const uint8_t *mem2, size_t memsize)
{
	memset(t, 0, sizeof(*t));

	t->seek = slurp_memory_seek_;
	t->tell = slurp_memory_tell_;
	t->peek = slurp_2mem_peek_;
	t->length = slurp_memory_length_;

	t->internal.memory.length = memsize * 2;
	t->internal.memory.data = mem1;
	t->internal.memory.data2 = mem2;
	t->closure = NULL;

	return 0;
}

/* --------------------------------------------------------------------- */
/* implementation specialized for sf2 stuff
 * it allows reading from two different places in a file as if they
 * were sequential, since sf2 allows for stereo samples to not be
 * in the split stereo format schism likes to have. */

static inline uint64_t sf2_slurp_length(slurp_t *s)
{
	return s->internal.sf2.data[0].len + s->internal.sf2.data[1].len;
}

static inline int64_t sf2_slurp_tell(slurp_t *s)
{
	int64_t len;
	int i;

	for (i = 0, len = 0; i < s->internal.sf2.current; i++)
		len += s->internal.sf2.data[i].len;

	return len + slurp_tell(s->internal.sf2.src) - s->internal.sf2.data[s->internal.sf2.current].off;
}

static inline int sf2_slurp_seek(slurp_t *s, int64_t off, int whence)
{
	size_t i, len;

	len = sf2_slurp_length(s);

	switch (whence) {
	default:
	case SEEK_SET:
		break;
	case SEEK_CUR:
		off += sf2_slurp_tell(s);
		break;
	case SEEK_END:
		off += len;
		break;
	}

	if (off < 0 || (size_t)off > len)
		return -1;

	for (i = 0; i < ARRAY_SIZE(s->internal.sf2.data); i++) {
		if (off < (int64_t)s->internal.sf2.data[i].len) {
			s->internal.sf2.current = i;
			return slurp_seek(s->internal.sf2.src, s->internal.sf2.data[i].off + off, SEEK_SET);
		}

		off -= s->internal.sf2.data[i].len;
	}

	/* likely EOF */
	s->internal.sf2.current = ARRAY_SIZE(s->internal.sf2.data) - 1;
	/* fix this up */
	off += s->internal.sf2.data[s->internal.sf2.current].len;
	/* seek */
	return slurp_seek(s->internal.sf2.src, s->internal.sf2.data[s->internal.sf2.current].off + off, SEEK_SET);
}

static size_t sf2_slurp_read(slurp_t *s, void *data, size_t count)
{
	size_t read = 0;

	while (s->internal.sf2.current < (int)(ARRAY_SIZE(s->internal.sf2.data) - 1)) {
		int64_t off_current = slurp_tell(s->internal.sf2.src) - s->internal.sf2.data[s->internal.sf2.current].off;
		int64_t left = s->internal.sf2.data[s->internal.sf2.current].len - off_current;

		if (left < 0)
			return 0; /* ??? */

		if ((size_t)left >= count)
			break;

		size_t tread = slurp_read(s->internal.sf2.src, (char *)data + read, left);
		if (tread != (size_t)left)
			return tread;

		read += tread;
		count -= tread;

		/* start over at the new offset */
		slurp_seek(s->internal.sf2.src, s->internal.sf2.data[++s->internal.sf2.current].off, SEEK_SET);
	}

	if (count > s->internal.sf2.data[s->internal.sf2.current].len)
		count = s->internal.sf2.data[s->internal.sf2.current].len;

	if (count)
		read += slurp_read(s->internal.sf2.src, (char *)data + read, count);

	return read;
}

static void sf2_slurp_closure(slurp_t *s)
{
	slurp_seek(s->internal.sf2.src, s->internal.sf2.origpos, SEEK_SET);
}

void slurp_sf2(slurp_t *s, slurp_t *in, int64_t off1, size_t len1,
	int64_t off2, size_t len2)
{
	memset(s, 0, sizeof(slurp_t));

	s->internal.sf2.src = in;
	s->internal.sf2.data[0].off = off1;
	s->internal.sf2.data[0].len = len1;
	s->internal.sf2.data[1].off = off2;
	s->internal.sf2.data[1].len = len2;
	s->internal.sf2.origpos = slurp_tell(in);

	/* now, fill in the functions :) */
	s->length = sf2_slurp_length;
	s->seek = sf2_slurp_seek;
	s->tell = sf2_slurp_tell;
	s->read = sf2_slurp_read;
	s->closure = sf2_slurp_closure;

	slurp_rewind(s);
}

/* --------------------------------------------------------------------- */
/* Replacement for seek() behavior for things that don't support
 * seeking, such as stdin or whatever
 *
 * FIXME: need to override slurp_seek, because if someone calls it with
 * SEEK_END we will totally fail. */

struct slurp_nonseek {
	void *opaque;

	/* read function. NOTE that size is merely a suggestion.
	 * the user (i.e. slurp_nonseek_available) should call it
	 * however many times required to fill the size it needs.
	 * this function could also add More than requested.
	 * it's designed this way to simplify buffering, hence why
	 * this function does not instead take a void pointer. */
	size_t (*read)(void *opaque, disko_t *ds, size_t size);
	void (*closure)(void *opaque);

	/* disko memory buffer (note that pos should always equal length) */
	disko_t ds;
};

static int slurp_nonseek_available(slurp_t *fp, size_t x, int whence)
{
	struct slurp_nonseek *ns = fp->nonseek;
	int64_t pos = x;

	switch (whence) {
	case SEEK_SET: break;
	case SEEK_CUR: pos += fp->internal.memory.pos; break;
	case SEEK_END: return !x;
	}

	while (pos > (int64_t)ns->ds.length) {
		size_t r = ns->read(ns->opaque, &ns->ds, pos - ns->ds.length);
		if (!r)
			return 0;
	}

	/* buffer may have changed */
	fp->internal.memory.data = ns->ds.data;
	fp->internal.memory.length = ns->ds.length;

	return 1;
}

static size_t slurp_nonseek_peek(slurp_t *fp, void *buf, size_t size)
{
	/* available() will load up any data we're missing and update the buffer */
	slurp_nonseek_available(fp, size, SEEK_CUR);

	return slurp_memory_peek_(fp, buf, size);
}

static void slurp_nonseek_closure(slurp_t *fp)
{
	struct slurp_nonseek *ns = fp->nonseek;

	ns->closure(ns->opaque);
	disko_memclose(&ns->ds, 0);
	free(ns);
}

static uint64_t slurp_nonseek_length(slurp_t *fp)
{
	/* Call available with maximum size to load the entire buffer;
	 * allows us to read in the whole thing :) */
	slurp_nonseek_available(fp, SIZE_MAX, SEEK_SET);

	return fp->internal.memory.length;
}

int slurp_init_nonseek(slurp_t *fp,
	size_t (*read_func)(void *opaque, disko_t *ds, size_t count),
	void (*closure)(void *opaque),
	void *opaque)
{
	struct slurp_nonseek *ns = mem_calloc(1, sizeof(*ns));

	ns->opaque = opaque;
	ns->read = read_func;
	ns->closure = closure;

	if (disko_memopen(&ns->ds) < 0)
		return -1;

	/* initialize with bogus values */
	slurp_memstream(fp, NULL, 0);

	fp->peek = slurp_nonseek_peek;
	fp->closure = slurp_nonseek_closure;
	fp->nonseek = ns;
	fp->length = slurp_nonseek_length;
	fp->available = slurp_nonseek_available;

	return 0;
}

/* --------------------------------------------------------------------- */
/* and now, the slurp interface */

int slurp_seek(slurp_t *t, int64_t offset, int whence)
{
	int r;
	int64_t offcheck = offset;

	switch (whence) {
	case SEEK_SET: break;
	case SEEK_CUR: {
		int64_t pos = slurp_tell(t);

		if (pos < 0)
			return -1;

		offcheck += pos;
		break;
	}
	case SEEK_END:
		offcheck += slurp_length(t);
		break;
	}

	if (offcheck < 0 || !slurp_available(t, offcheck, SEEK_SET))
		return -1;

	r = t->seek(t, offset, whence);
	if (r == 0)
		t->eof_ = 0;

	return r;
}

int64_t slurp_tell(slurp_t *t)
{
	return t->tell(t);
}

static inline SCHISM_ALWAYS_INLINE
void slurp_fill_remaining(slurp_t *t, void *ptr, size_t read, size_t count)
{
	if (count > read) {
		/* short read -- fill in any extra bytes with zeroes */
		memset((unsigned char *)ptr + read, 0, count - read);
		t->eof_ = 1;
	}
}

static inline SCHISM_ALWAYS_INLINE
size_t slurp_limit_count(slurp_t *t, size_t count)
{
	int64_t pos;

	if (slurp_eof(t))
		return 0;

	if (!t->limit)
		return count;

	pos = slurp_tell(t);

	return MIN(count, t->limit - pos);
}

size_t slurp_peek(slurp_t *t, void *ptr, size_t count)
{
	size_t read_bytes = slurp_limit_count(t, count);

	if (read_bytes > 0) {
		if (t->peek) {
			read_bytes = t->peek(t, ptr, read_bytes);
		} else {
			/* cache current position */
			int64_t pos = slurp_tell(t);
			if (pos < 0)
				return 0;

			read_bytes = t->read(t, ptr, read_bytes);

			slurp_seek(t, pos, SEEK_SET);
		}
	}

	slurp_fill_remaining(t, ptr, read_bytes, count);
	t->eof_ = 0; /* dont clobber */

	return read_bytes;
}

size_t slurp_read(slurp_t *t, void *ptr, size_t count)
{
	size_t read_bytes = slurp_limit_count(t, count);

	if (read_bytes > 0) {
		if (t->read) {
			read_bytes = t->read(t, ptr, read_bytes);
		} else {
			read_bytes = t->peek(t, ptr, read_bytes);
			slurp_seek(t, read_bytes, SEEK_CUR);
		}
	}

	slurp_fill_remaining(t, ptr, read_bytes, count);

	return read_bytes;
}

uint64_t slurp_length(slurp_t *t)
{
	return t->length(t);
}

int slurp_getc(slurp_t *t)
{
	/* just a wrapper around slurp_read() */
	unsigned char byte;
	size_t count = slurp_read(t, &byte, 1);

	return (count) ? (int)byte : EOF;
}

int slurp_eof(slurp_t *t)
{
	if (t->eof) {
		return t->eof(t);
	} else {
		/* emulate */
		return t->eof_;
	}
}

int slurp_receive(slurp_t *t, int (*callback)(const void *, size_t, void *), size_t count, void *userdata)
{
	if (t->receive) {
		return t->receive(t, callback, count, userdata);
	} else {
		unsigned char *buf = mem_alloc(count);
		int r;

		if (!buf)
			return -1;

		count = slurp_peek(t, buf, count);

		r = callback(buf, count, userdata);

		free(buf);

		return r;
	}
}

/* TODO actually test this function within slurp crap */
int slurp_available(slurp_t *fp, size_t x, int whence)
{
	if (!x)
		return 1; /* ... */

	if (fp->available) {
		/* prefer this one */
		return fp->available(fp, x, whence);
	} else if (fp->length) {
		int64_t pos = 0;

		switch (whence) {
		case SEEK_SET: break;
		case SEEK_CUR: pos += slurp_tell(fp); break;
		case SEEK_END: return 0; /* ??? */
		}

		if (pos < 0)
			return 0;

		return (pos + x) <= fp->length(fp);
	} else {
		SCHISM_RUNTIME_ASSERT(0, "slurp: available or length is required");
	}
}

void slurp_limit(slurp_t *t, int64_t wall)
{
	/* creates a wall, relative to the current position
	 * any reads that try to go after that point will fail */

	if (t->limit)
		slurp_unlimit(t);

	t->limit = slurp_tell(t) + wall;
}

void slurp_unlimit(slurp_t *t)
{
	t->limit = 0;
}

void slurp_unlimit_seek(slurp_t *t)
{
	if (t->limit) {
		slurp_seek(t, t->limit, SEEK_SET);

		t->limit = 0;
	}
}

/* ------------------------------------------------------------------------ */
/* slurp support for decompression */

#define DEF_CHUNK_SIZE (4096)

struct slurp_decompress {
	/* the original file as passed into slurp_decompress */
	slurp_t fp;

	struct slurp_decompress_vtable vtbl;

	/* error flag */
	unsigned int err : 1;
	unsigned int done : 1;
	/* only for zstd; the format doesn't HAVE a "EOF" flag, which means
	 * we might be done if the frame finished; if so, set the EOF flag
	 * instead of dying */
	unsigned int maybe_done : 1;

	void *opaque;

	/* minimum output buffer size */
	size_t minoutputbufsz;

	/* input buffer size */
	size_t bufsz;
	unsigned char buf[SCHISM_FAM_SIZE];
};

static size_t slurp_decompress_read(void *opaque, disko_t *ds, size_t size)
{
	struct slurp_decompress *zl = opaque;
	void *buf;

	if (zl->err || zl->done)
		return 0; /* Uh oh */

	size = MAX(zl->minoutputbufsz, size);

	buf = disko_memstart(ds, size);
	if (!buf) {
		zl->err = 1;
		return 0;
	}
	zl->vtbl.output(zl->opaque, buf, size);

	while (zl->vtbl.output(zl->opaque, NULL, 0) > 0) {
		int res;

		if (zl->vtbl.input(zl->opaque, NULL, 0) == 0) {
			size_t z = slurp_read(&zl->fp, zl->buf, zl->bufsz);
			if (!z) {
				if (zl->maybe_done) {
					zl->done = 1;
					zl->maybe_done = 0;
				} else {
					zl->err = 1;
				}
				goto ZL_end;
			}

			zl->vtbl.input(zl->opaque, zl->buf, z);
		}

		res = zl->vtbl.inflate(zl->opaque);
		if (res == SLURP_DEC_OK)
			continue;

		if (res == SLURP_DEC_DONE) {
			zl->done = 1;
			break;
		}

		if (res == SLURP_DEC_OK_OR_DONE) {
			zl->maybe_done = 1;
			continue;
		}

		zl->err = 1;
		goto ZL_end;
	}

ZL_end:
	size -= zl->vtbl.output(zl->opaque, NULL, 0);
	disko_memend(ds, buf, size);
	return size;
}

static void slurp_decompress_closure(void *opaque)
{
	struct slurp_decompress *zl = opaque;

	zl->vtbl.end(zl->opaque);
	unslurp(&zl->fp);
	free(zl);
}

int slurp_decompress(slurp_t *fp, const struct slurp_decompress_vtable *vtbl)
{
	struct slurp_decompress *zl;
	size_t inbufsz;

	inbufsz = vtbl->inbufsz ? vtbl->inbufsz() : DEF_CHUNK_SIZE;

	zl = mem_calloc(1, sizeof(*zl) + inbufsz);
	zl->bufsz = inbufsz;

	memcpy(&zl->vtbl, vtbl, sizeof(struct slurp_decompress_vtable));

	zl->opaque = zl->vtbl.start();
	if (!zl->opaque) {
		free(zl);
		return -1;
	}

	memcpy(&zl->fp, fp, sizeof(slurp_t));

	slurp_init_nonseek(fp, slurp_decompress_read, slurp_decompress_closure, zl);

	/* read a bit to ensure we've actually got the right thing.
	 * zlib won't complain if our file Isn't Correct, so we have
	 * to do it ourselves. */
	slurp_available(fp, 8096, SEEK_SET);

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
		unslurp(fp);

		/* roll it back */
		memcpy(fp, &tmp, sizeof(slurp_t));

		return -1;
	}

	return 0;
}

/* ------------------------------------------------------------------------ */

/* strcspn equivalent.
 * if this function returns -1, then it's hit EOF. */
int slurp_skip_chars(slurp_t *fp, const char *str)
{
	for (;;) {
		unsigned char c;

		if (slurp_peek(fp, &c, 1) != 1)
			return -1;

		if (strchr(str, c))
			break;

		/* keep going */
		slurp_seek(fp, 1, SEEK_CUR);
	}

	return 0;
}

/* strspn equivalent */
int slurp_skip_until_chars(slurp_t *fp, const char *str)
{
	for (;;) {
		unsigned char c;

		if (slurp_peek(fp, &c, 1) != 1)
			return -1;

		if (!strchr(str, c))
			break;

		/* keep going */
		slurp_seek(fp, 1, SEEK_CUR);
	}

	return 0;
}
