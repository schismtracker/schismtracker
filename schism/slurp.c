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

#ifdef SLURP_BUFFERED
/* wraps a slurp_t in a buffer */
static void slurp_buffer(slurp_t *t, size_t bufsz);
#endif

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
#ifdef SLURP_BUFFERED
	size_t bufsz = 4096;
#endif

	if (!t)
		return -1;

	memset(t, 0, sizeof(*t));

	if (!strcmp(filename, "-")) {
		slurp_stdio(t, stdin);
	} else {
		size_t i;
		struct stat st;

		if (!buf) {
			if (os_stat(filename, &st) < 0)
				return -1;

			buf = &st;
		}

#if defined(HAVE_STRUCT_STAT_ST_BLKSIZE) && defined(SLURP_BUFFERED)
		/* Adjust buffer size according to stat hints */
		bufsz = buf->st_blksize;
#endif
		size = buf->st_size;

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

	{
		uint8_t *mmdata;
		size_t mmlen;

		if (mmcmp_unpack(t, &mmdata, &mmlen)) {
			// clean up the existing data
			unslurp(t);

			// and put the new stuff in
			slurp_memstream_free(t, mmdata, mmlen);
		}
	}

	slurp_rewind(t);

#ifdef SLURP_BUFFERED
	slurp_buffer(t, bufsz);
#endif

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

#ifdef SLURP_BUFFERED
	/* it's dumb to have two layers of buffering */
	setbuf(fp, NULL);
#endif

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
 * in the split stereo format schism likes to have.
 * FIXME: */

static inline uint64_t sf2_slurp_length(slurp_t *s)
{
	int i;
	uint64_t len;

	SCHISM_RUNTIME_ASSERT(s->internal.sf2.current < s->internal.sf2.num, "a");

	len = 0;
	for (i = 0; i < s->internal.sf2.num; i++)
		len += s->internal.sf2.data[i].len;

	return len;
}

static inline int64_t sf2_slurp_tell(slurp_t *s)
{
	int64_t len;
	int i;

	SCHISM_RUNTIME_ASSERT(s->internal.sf2.current < s->internal.sf2.num, "a");

	len = 0;
	for (i = 0; i < s->internal.sf2.current; i++)
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

	for (i = 0; i < s->internal.sf2.num; i++) {
		if (off < (int64_t)s->internal.sf2.data[i].len) {
			s->internal.sf2.current = i;
			return slurp_seek(s->internal.sf2.src, s->internal.sf2.data[i].off + off, SEEK_SET);
		}

		off -= s->internal.sf2.data[i].len;
	}

	/* likely EOF */
	s->internal.sf2.current = s->internal.sf2.num - 1;
	/* fix this up */
	off += s->internal.sf2.data[s->internal.sf2.current].len;
	/* seek */
	return slurp_seek(s->internal.sf2.src, s->internal.sf2.data[s->internal.sf2.current].off + off, SEEK_SET);
}

static size_t sf2_slurp_cap(slurp_t *s, size_t count)
{
	int64_t off_current, left;

	SCHISM_RUNTIME_ASSERT(s->internal.sf2.current < s->internal.sf2.num, "a");

	off_current = slurp_tell(s->internal.sf2.src) - s->internal.sf2.data[s->internal.sf2.current].off;
	left = s->internal.sf2.data[s->internal.sf2.current].len - off_current;

	if (left <= 0)
		return 0; /* ??? */

	if ((size_t)left < count)
		return left;

	return count;
}

static size_t sf2_slurp_read(slurp_t *s, void *data, size_t count)
{
	size_t read = 0;

	SCHISM_RUNTIME_ASSERT(s->internal.sf2.current < s->internal.sf2.num, "a");

	for (;;) {
		size_t l = sf2_slurp_cap(s, count);		
		if (!l)
			break;

		size_t tread = slurp_read(s->internal.sf2.src, (char *)data + read, l);

		read += tread;
		count -= tread;

		/* do we want to read anymore? */
		if (!count)
			break;

		/* do we want to read? */
		if (tread != (size_t)l)
			break;

		/* EOF? */
		if ((s->internal.sf2.current + 1) >= s->internal.sf2.num)
			return read;

		/* start over at the new offset */
		slurp_seek(s->internal.sf2.src, s->internal.sf2.data[++s->internal.sf2.current].off, SEEK_SET);
	}

	return read;
}

static void sf2_slurp_closure(slurp_t *s)
{
	slurp_seek(s->internal.sf2.src, s->internal.sf2.origpos, SEEK_SET);
}

/* usage: slurp_sf2v2(&sf2, in, 2, off1, len1, off2, len2, off3, len3 ...) */
int slurp_sf2v2(slurp_t *s, slurp_t *in, size_t num, int64_t off1, int64_t len1, ...)
{
	size_t i;
	va_list ap;

	if (!num || (num > ARRAY_SIZE(s->internal.sf2.data)))
		return -1;

	memset(s, 0, sizeof(slurp_t));

	s->internal.sf2.src = in;
	s->internal.sf2.num = num;
	s->internal.sf2.current = 0;
	s->internal.sf2.data[0].off = off1;
	s->internal.sf2.data[0].len = len1;
	va_start(ap, len1);
	for (i = 1; i < num; i++) {
		s->internal.sf2.data[i].off = va_arg(ap, int64_t);
		s->internal.sf2.data[i].len = va_arg(ap, int64_t);
	}
	va_end(ap);

	/* now, fill in the functions :) */
	s->length = sf2_slurp_length;
	s->seek = sf2_slurp_seek;
	s->tell = sf2_slurp_tell;
	s->read = sf2_slurp_read;
	s->closure = sf2_slurp_closure;

	slurp_rewind(s);

	return 0;
}

void slurp_sf2(slurp_t *s, slurp_t *in, int64_t off1, size_t len1,
	int64_t off2, size_t len2)
{
	slurp_sf2v2(s, in, 2, off1, len1, off2, len2);
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

/* return: 0 on success, -1 on error */
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

	if (offcheck < 0 || !slurp_could_seek(t, offcheck, SEEK_SET))
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

static size_t slurp_read_nozero(slurp_t *t, void *ptr, size_t count)
{
	count = slurp_limit_count(t, count);

	if (count > 0) {
		if (t->read) {
			count = t->read(t, ptr, count);
		} else {
			count = t->peek(t, ptr, count);
			slurp_seek(t, count, SEEK_CUR);
		}
	}

	return count;
}

size_t slurp_read(slurp_t *t, void *ptr, size_t count)
{
	size_t read_bytes = slurp_read_nozero(t, ptr, count);

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

int slurp_could_seek(slurp_t *fp, int64_t x, int whence)
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
		case SEEK_END: return (x <= 0); /* file pointers past the end are not valid */
		}

		if (pos < 0)
			return 0;

		return (pos + x) <= fp->length(fp);
	} else {
		SCHISM_RUNTIME_ASSERT(0, "slurp: available or length is required");
	}
}

static void slurp_fill_nonseek_buffer(slurp_t *t, size_t count)
{
	/* If we are a nonseek slurp, then a custom available() has been
	 * installed which reads forward and buffers bytes to ensure that
	 * they are available.
	 *
	 * If we are not a nonseek slurp, then there is no buffer to fill.
	 */
	if (t->available) {
		t->available(t, count, SEEK_CUR);
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
	slurp_fill_nonseek_buffer(fp, 8096);

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
/* buffered slurp() */

#ifdef SLURP_BUFFERED
# define SLURP_SEEK_PRESERVE_BUFFER 1

#if 0 /* This is for testing */
# define SLURP_BUFFER_OVERRIDE 2048
#endif

/* optimize allocations: only one here */
struct buffer_alloc {
	slurp_t fp;
	char buf[];
};

static int slurp_buffered_seek(slurp_t *t, int64_t off, int whence)
{
	int64_t n;
	uint64_t len;
	int64_t newbufferedoff;

	len = slurp_length(t->internal.buffered.fp);

	switch (whence) {
	case SEEK_SET: n = 0; break;
	case SEEK_CUR: n = t->internal.buffered.off; break;
	case SEEK_END: n = len; break;
	default: {
		t->internal.buffered.bufptr = NULL;
		t->internal.buffered.bufcnt = 0;
		return -1;
	}
	}

	n += off;

	if (n > len) {
		t->internal.buffered.bufptr = NULL;
		t->internal.buffered.bufcnt = 0;
		return -1;
	}

	newbufferedoff = n;

#ifdef SLURP_SEEK_PRESERVE_BUFFER
	/* okay, now check if we can keep the same buffer
	 *
	 * bufcnt == 0 means that there is nothing left in the buffer, BUT
	 * bufptr == NULL means there is no buffer in the first place
	 *
	 * this is an important distinction here -- if we have a valid buffer
	 * even at bufcnt == 0 we ought to use it
	 *
	 * TODO: the crux of this logic should really be in read(), as we
	 * could seek twice, the first being outside of the buffer, and
	 * the second being inside of it, and we'll lose our buffer. */
	if (t->internal.buffered.bufptr) {
		int64_t bufstart;
		size_t bufcnt;
		ptrdiff_t ptroff;

		/* how far were we? */
		ptroff = t->internal.buffered.bufptr - t->internal.buffered.buf;

		/* the initial value of bufcnt (may be smaller than bufsz) */
		bufcnt = t->internal.buffered.bufcnt + ptroff;

		/* where the file position is relative to the buffer start */
		bufstart = n - (t->internal.buffered.off - ptroff);

		/* <= is not an off-by-one -- the bufptr will just point at the end */
		if (bufstart >= 0 && bufstart <= bufcnt) {
			t->internal.buffered.bufptr = t->internal.buffered.buf + bufstart;
			t->internal.buffered.bufcnt = bufcnt - bufstart;

			/* :3 */
			n += bufcnt - bufstart;
		} else {
			/* invalidate */
			t->internal.buffered.bufptr = NULL;
			t->internal.buffered.bufcnt = 0;
		}
	} /* else we don't even HAVE a buffer (never read?) */
#else
	/* kill it */
	t->internal.buffered.bufptr = NULL;
	t->internal.buffered.bufcnt = 0;
#endif

	t->internal.buffered.off = newbufferedoff;

	return slurp_seek(t->internal.buffered.fp, n, SEEK_SET);
}

static int64_t slurp_buffered_tell(slurp_t *t)
{
	return t->internal.buffered.off;
}

static size_t slurp_buffered_empty_buffer(slurp_t *t, void **ptr, size_t *sz)
{
	size_t tocpy;

	/* do nothing */
	if (!t->internal.buffered.bufcnt)
		return 0;

	/* handle the buffer */
	tocpy = MIN(*sz, t->internal.buffered.bufcnt);
	memcpy(*ptr, t->internal.buffered.bufptr, tocpy);
	t->internal.buffered.bufcnt -= tocpy;
	t->internal.buffered.bufptr += tocpy;
	*sz -= tocpy;
	*ptr = (char *)*ptr + tocpy;

	return tocpy;
}

static size_t slurp_buffered_fill_buffer(slurp_t *t)
{
	if (t->internal.buffered.bufcnt)
		return 0;

	/* fill the buffer with next bufsz bytes
	 * we do actually need to use the nozero implementation, as if
	 * this returns 0, the data still in there will remain valid
	 * and can be useful later */
	t->internal.buffered.bufcnt = slurp_read_nozero(t->internal.buffered.fp,
			t->internal.buffered.buf, t->internal.buffered.bufsz);
	if (!t->internal.buffered.bufcnt)
		return 0;

	t->internal.buffered.bufptr = t->internal.buffered.buf;

	return t->internal.buffered.bufcnt;
}

static size_t slurp_buffered_read(slurp_t *t, void *ptr, size_t sz)
{
	/* fill up the buffer */
	size_t r, szunbuffered;

	/* this holds the return value */
	r = 0;

	if (!sz)
		goto done;

	/* empty anything thats left over in the buffer */
	r += slurp_buffered_empty_buffer(t, &ptr, &sz);
	if (!sz)
		goto done;

	/* if sz is large enough, bypass the buffer and fill it directly.
	 * we fill enough to where the last N bytes that are less than the
	 * buffer size are filled in the buffer. */
	szunbuffered = sz - (sz % t->internal.buffered.bufsz);
	if (szunbuffered > 0) {
		size_t re;

		/* go! */
		re = slurp_read(t->internal.buffered.fp, ptr, szunbuffered);

		r += re;
		ptr = (char *)ptr + re;
		sz -= re;

		/* we don't need to invalidate the buffer here -- it's already
		 * empty if we had space left over after the first call */

		if (!sz) /* already finished? */
			goto done;
	}

	/* refill the buffer */
	slurp_buffered_fill_buffer(t);

	/* ... and empty what we need */
	r += slurp_buffered_empty_buffer(t, &ptr, &sz);

done:
	t->internal.buffered.off += r;
	return r;
}

static uint64_t slurp_buffered_length(slurp_t *t)
{
	return slurp_length(t->internal.buffered.fp);
}

/* this really should not take a size_t */
static int slurp_buffered_available(slurp_t *t, size_t x, int whence)
{
	/* fake it til u make it */
	switch (whence) {
	case SEEK_CUR:
		x += t->internal.buffered.off;
		whence = SEEK_SET;
		break;
	}

	return slurp_could_seek(t->internal.buffered.fp, x, SEEK_SET);
}

static void slurp_buffered_closure(slurp_t *t)
{
	unslurp(t->internal.buffered.fp);
	/* also frees the buffer */
	free(t->internal.buffered.fp);
}

/* forward to fp implementation
 *
 * note that because this is optional this pointer is only filled if
 * the child pointer is not NULL
 *
 * otherwise, we just malloc a buffer and send it off */
static int slurp_buffered_receive(slurp_t *t, int (*callback)(const void *, size_t, void *), size_t length, void *userdata)
{
	return t->internal.buffered.fp->receive(t, callback, length, userdata);
}

/* wraps a slurp_t in a buffer */
static void slurp_buffer(slurp_t *t, size_t bufsz)
{
	struct buffer_alloc *a;

	if (!bufsz) return; /* ok */

#ifdef SLURP_BUFFER_OVERRIDE
	bufsz = SLURP_BUFFER_OVERRIDE;
#endif

	a = malloc(sizeof(*a) + bufsz);
	if (!a)
		return;

	memcpy(&a->fp, t, sizeof(slurp_t));
	memset(t, 0, sizeof(slurp_t));

	t->seek = slurp_buffered_seek;
	t->tell = slurp_buffered_tell;
	t->read = slurp_buffered_read;
	t->length = slurp_buffered_length;
	t->closure = slurp_buffered_closure;
	t->available = slurp_buffered_available;
	if (a->fp.receive)
		t->receive = slurp_buffered_receive;

	t->internal.buffered.bufptr = NULL;
	t->internal.buffered.buf = a->buf;
	t->internal.buffered.bufcnt = 0;
	t->internal.buffered.bufsz = bufsz;

	t->internal.buffered.fp = &a->fp;

	/* WHAT'LL IT BE FELLAS? */
	slurp_buffered_fill_buffer(t);
}
#endif

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
