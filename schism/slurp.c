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

static int slurp_stdio_open_(slurp_t *t, const char *filename);
static int slurp_stdio_open_file_(slurp_t *t, FILE *fp);
static int slurp_stdio_seek_(slurp_t *t, int64_t offset, int whence);
static int64_t slurp_stdio_tell_(slurp_t *t);
static uint64_t slurp_stdio_length_(slurp_t *t);
static size_t slurp_stdio_read_(slurp_t *t, void *ptr, size_t count);
static int slurp_stdio_eof_(slurp_t *t);
static void slurp_stdio_closure_(slurp_t *t);

static int slurp_memory_seek_(slurp_t *t, int64_t offset, int whence);
static int64_t slurp_memory_tell_(slurp_t *t);
static uint64_t slurp_memory_length_(slurp_t *t);
static size_t slurp_memory_peek_(slurp_t *t, void *ptr, size_t count);
static int slurp_memory_receive_(slurp_t *t, int (*callback)(const void *, size_t, void *), size_t count, void *userdata);
static void slurp_memory_closure_free_(slurp_t *t);

static size_t slurp_2mem_peek_(slurp_t *t, void *ptr, size_t count);

/* --------------------------------------------------------------------- */

int slurp(slurp_t *t, const char *filename, struct stat * buf, size_t size)
{
	struct stat st;

	if (!t)
		return -1;

	memset(t, 0, sizeof(*t));

	if (buf) {
		st = *buf;
	} else {
		if (os_stat(filename, &st) < 0)
			return -1;
	}

	if (!size)
		size = st.st_size;

#if (defined(SCHISM_WIN32) || defined(HAVE_MMAP))
	switch (
#ifdef SCHISM_WIN32
		slurp_win32_mmap(t, filename, size)
#elif defined(HAVE_MMAP)
		slurp_mmap(t, filename, size)
#else
# error Where are we now?
#endif
	) {
	case SLURP_OPEN_FAIL:
		return -1;
	case SLURP_OPEN_SUCCESS:
		t->seek = slurp_memory_seek_;
		t->tell = slurp_memory_tell_;
		t->peek = slurp_memory_peek_;
		t->receive = slurp_memory_receive_;
		t->length = slurp_memory_length_;
		goto finished;
	default:
	case SLURP_OPEN_IGNORE:
		break;
	}
#endif

#ifdef SCHISM_WIN32
	switch (slurp_win32(t, filename)) {
	case SLURP_OPEN_FAIL:
		return -1;
	case SLURP_OPEN_SUCCESS:
		/* function pointers already filled in */
		goto finished;
	case SLURP_OPEN_IGNORE:
		break;
	}
#endif

	switch (slurp_stdio_open_(t, filename)) {
	case SLURP_OPEN_FAIL:
		return -1;
	case SLURP_OPEN_SUCCESS:
		goto finished;
	default:
	case SLURP_OPEN_IGNORE:
		break;
	}

	/* fail */
	return -1;

finished: ; /* this semicolon is important because C */
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

/* Initializes a slurp structure on an existing memory stream.
 * Does NOT free the input. */
int slurp_memstream(slurp_t *t, uint8_t *mem, size_t memsize)
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

int slurp_2memstream(slurp_t *t, uint8_t *mem1, uint8_t *mem2, size_t memsize)
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

void unslurp(slurp_t * t)
{
	if (!t)
		return;

	if (t->closure)
		t->closure(t);
}

/* --------------------------------------------------------------------- */
/* stdio implementation */

/* this function does NOT automatically close the file */
int slurp_stdio(slurp_t *t, FILE *fp)
{
	long end;

	t->internal.stdio.fp = fp;

	if (fseek(t->internal.stdio.fp, 0, SEEK_END))
		return SLURP_OPEN_FAIL;

	end = ftell(t->internal.stdio.fp);
	if (end < 0)
		return SLURP_OPEN_FAIL;

	/* return to monke */
	if (fseek(t->internal.stdio.fp, 0, SEEK_SET))
		return SLURP_OPEN_FAIL;

	t->internal.stdio.length = MAX(0, end);

	/* A BARBERSHOP HAIRCUT THAT COSTS A QUARTER */
	t->seek = slurp_stdio_seek_;
	t->tell = slurp_stdio_tell_;
	t->eof  = slurp_stdio_eof_;
	t->read = slurp_stdio_read_;
	t->length = slurp_stdio_length_;

	return SLURP_OPEN_SUCCESS;
}

static int slurp_stdio_open_(slurp_t *t, const char *filename)
{
	FILE *fp;

	if (!strcmp(filename, "-")) {
		fp = stdin;
		t->closure = NULL;
	} else {
		fp = os_fopen(filename, "rb");
		t->closure = slurp_stdio_closure_;
	}

	if (!fp)
		return SLURP_OPEN_FAIL;

	return slurp_stdio(t, fp);
}

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
	long pos, end;

	pos = slurp_stdio_tell_(t);
	if (pos < 0)
		return -1; // what the hell?

	slurp_stdio_seek_(t, 0, SEEK_END);

	end = slurp_stdio_tell_(t);
	if (end < 0)
		return -1;

	slurp_stdio_seek_(t, pos, SEEK_SET);

	return pos >= end;
}

static void slurp_stdio_closure_(slurp_t *t)
{
	fclose(t->internal.stdio.fp);
}

/* --------------------------------------------------------------------- */

static int slurp_memory_seek_(slurp_t *t, int64_t offset, int whence)
{
	switch (whence) {
	default:
	case SEEK_SET:
		break;
	case SEEK_CUR:
		offset += t->internal.memory.pos;
		break;
	case SEEK_END:
		offset += t->internal.memory.length;
		break;
	}

	if (offset < 0 || (size_t)offset > t->internal.memory.length)
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
	free(t->internal.memory.data);
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
		unsigned char *data = (which == 0) ? t->internal.memory.data : t->internal.memory.data2;

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

	for (i = 0, len = 0; i < ARRAY_SIZE(s->internal.sf2.data); i++) {
		if (off >= len && off < len + s->internal.sf2.data[i].len) {
			s->internal.sf2.current = i;
			return slurp_seek(s->internal.sf2.src, s->internal.sf2.data[i].off + off - len, SEEK_SET);
		}

		len += s->internal.sf2.data[i].len;
	}

	/* ? */
	s->internal.sf2.current = ARRAY_SIZE(s->internal.sf2.data) - 1;
	return slurp_seek(s->internal.sf2.src, s->internal.sf2.data[s->internal.sf2.current].off + off - len, SEEK_SET);
}

static size_t sf2_slurp_read(slurp_t *s, void *data, size_t count)
{
	size_t read = 0;

	while (s->internal.sf2.current < (ARRAY_SIZE(s->internal.sf2.data) - 1)) {
		int64_t off_current = slurp_tell(s->internal.sf2.src) - s->internal.sf2.data[s->internal.sf2.current].off;
		int64_t left = s->internal.sf2.data[s->internal.sf2.current].len - off_current;

		if (left < 0)
			return 0; /* ??? */

		if ((size_t)left >= count)
			break;

		size_t tread = slurp_read(s->internal.sf2.src, (char *)data + read, left);
		if (tread != left)
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
/* these just forward directly to the function pointers */

int slurp_seek(slurp_t *t, int64_t offset, int whence)
{
	int r = t->seek(t, offset, whence);
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

	if (!t->limit)
		return count;

	pos = slurp_tell(t);

	return MIN(count, t->limit - pos);
}

size_t slurp_peek(slurp_t *t, void *ptr, size_t count)
{
	size_t read_bytes;

	if (!count)
		return 0;

	read_bytes = slurp_limit_count(t, count);

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

	slurp_fill_remaining(t, ptr, read_bytes, count);

	return read_bytes;
}

size_t slurp_read(slurp_t *t, void *ptr, size_t count)
{
	size_t read_bytes;

	if (!count)
		return 0;

	count = slurp_limit_count(t, count);

	if (t->read) {
		read_bytes = t->read(t, ptr, count);
	} else {
		read_bytes = t->peek(t, ptr, count);
		slurp_seek(t, count, SEEK_CUR);
	}

	slurp_fill_remaining(t, ptr, read_bytes, count);

	return read_bytes;
}

uint64_t slurp_length(slurp_t *t)
{
	return t->length(t);
}

/* actual implementations */

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

/* */
void slurp_unlimit_seek(slurp_t *t)
{
	if (t->limit) {
		slurp_seek(t, t->limit, SEEK_SET);

		t->limit = 0;
	}
}
