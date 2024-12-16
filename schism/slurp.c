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

#include <sys/types.h>
#include <sys/stat.h>
#include <stddef.h>

#include <errno.h>
#include <fcntl.h>

static int slurp_stdio_open_(slurp_t *t, const char *filename);
static int slurp_stdio_open_file_(slurp_t *t, FILE *fp);
static int slurp_stdio_seek_(slurp_t *t, int64_t offset, int whence);
static int64_t slurp_stdio_tell_(slurp_t *t);
static size_t slurp_stdio_peek_(slurp_t *t, void *ptr, size_t count);
static size_t slurp_stdio_read_(slurp_t *t, void *ptr, size_t count);
static int slurp_stdio_eof_(slurp_t *t);
static int slurp_stdio_receive_(slurp_t *t, int (*callback)(const void *, size_t, void *), size_t count, void *userdata);
static void slurp_stdio_closure_(slurp_t *t);

static int slurp_memory_seek_(slurp_t *t, int64_t offset, int whence);
static int64_t slurp_memory_tell_(slurp_t *t);
static size_t slurp_memory_peek_(slurp_t *t, void *ptr, size_t count);
static size_t slurp_memory_read_(slurp_t *t, void *ptr, size_t count);
static int slurp_memory_receive_(slurp_t *t, int (*callback)(const void *, size_t, void *), size_t count, void *userdata);
static int slurp_memory_eof_(slurp_t *t);
static void slurp_memory_closure_free_(slurp_t *t);

/* --------------------------------------------------------------------- */

int slurp(slurp_t *t, const char *filename, struct stat * buf, size_t size)
{
	struct stat st;

	if (!t)
		return -1;

	*t = (slurp_t){0};

	if (buf) {
		st = *buf;
	} else {
		if (os_stat(filename, &st) < 0)
			return -1;
	}

	if (!size)
		size = st.st_size;

	switch (
#ifdef SCHISM_WIN32
		slurp_win32(t, filename, size)
#elif defined(HAVE_MMAP)
		slurp_mmap(t, filename, size)
#else
		SLURP_OPEN_IGNORE
#endif
	) {
	case SLURP_OPEN_FAIL:
		return -1;
	case SLURP_OPEN_SUCCESS:
		t->seek = slurp_memory_seek_;
		t->tell = slurp_memory_tell_;
		t->eof  = slurp_memory_eof_;
		t->peek = slurp_memory_peek_;
		t->read = slurp_memory_read_;
		t->receive = slurp_memory_receive_;
		goto finished;
	default:
	case SLURP_OPEN_IGNORE:
		break;
	}

	switch (slurp_stdio_open_(t, filename)) {
	case SLURP_OPEN_FAIL:
		return -1;
	case SLURP_OPEN_SUCCESS:
		t->seek = slurp_stdio_seek_;
		t->tell = slurp_stdio_tell_;
		t->eof  = slurp_stdio_eof_;
		t->peek = slurp_stdio_peek_;
		t->read = slurp_stdio_read_;
		t->receive = slurp_stdio_receive_;
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
		t->seek = slurp_memory_seek_;
		t->tell = slurp_memory_tell_;
		t->eof  = slurp_memory_eof_;
		t->peek = slurp_memory_peek_;
		t->read = slurp_memory_read_;
		t->receive = slurp_memory_receive_;

		t->internal.memory.length = mmlen;
		t->internal.memory.data = mmdata;
		t->closure = slurp_memory_closure_free_;
	}

	slurp_rewind(t);

	// TODO re-add PP20 unpacker, possibly also handle other formats?

	return 0;
}

/* Initializes a slurp structure on an existing memory stream.
 * Does NOT free the input. */
int slurp_memstream(slurp_t *t, uint8_t *mem, size_t memsize)
{
	*t = (slurp_t){0};

	t->seek = slurp_memory_seek_;
	t->tell = slurp_memory_tell_;
	t->eof  = slurp_memory_eof_;
	t->peek = slurp_memory_peek_;
	t->read = slurp_memory_read_;
	t->receive = slurp_memory_receive_;

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

void unslurp(slurp_t * t)
{
	if (!t)
		return;

	if (t->closure)
		t->closure(t);
}

/* --------------------------------------------------------------------- */
/* stdio implementation */

static int slurp_stdio_open_(slurp_t *t, const char *filename)
{
	FILE *fp = !strcmp(filename, "-") ? stdin : os_fopen(filename, "rb");
	if (!fp)
		return SLURP_OPEN_FAIL;

	t->closure = slurp_stdio_closure_;

	return slurp_stdio_open_file_(t, fp);
}

static int slurp_stdio_open_file_(slurp_t *t, FILE *fp)
{
	t->internal.stdio.fp = fp;
	return SLURP_OPEN_SUCCESS;
}

static int slurp_stdio_seek_(slurp_t *t, int64_t offset, int whence)
{
	return fseek(t->internal.stdio.fp, offset, whence);
}

static int64_t slurp_stdio_tell_(slurp_t *t)
{
	return ftell(t->internal.stdio.fp);
}

static size_t slurp_stdio_peek_(slurp_t *t, void *ptr, size_t count)
{
	/* cache current position */
	int64_t pos = slurp_stdio_tell_(t);
	if (pos < 0)
		return 0;

	count = slurp_stdio_read_(t, ptr, count);

	slurp_stdio_seek_(t, pos, SEEK_SET);

	return count;
}

static size_t slurp_stdio_read_(slurp_t *t, void *ptr, size_t count)
{
	size_t read = fread(ptr, 1, count, t->internal.stdio.fp);
	if (read < count)
		memset((unsigned char*)ptr + read, 0, count - read);

	return read;
}

static int slurp_stdio_eof_(slurp_t *t)
{
	return feof(t->internal.stdio.fp);
}

static void slurp_stdio_closure_(slurp_t *t)
{
	fclose(t->internal.stdio.fp);
}

static int slurp_stdio_receive_(slurp_t *t, int (*callback)(const void *, size_t, void *), size_t count, void *userdata)
{
	unsigned char *buf = malloc(count);
	if (!buf)
		return -1;

	count = slurp_stdio_peek_(t, buf, count);

	int r = callback(buf, count, userdata);

	free(buf);

	return r;
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

static size_t slurp_memory_peek_(slurp_t *t, void *ptr, size_t count)
{
	ptrdiff_t bytesleft = (ptrdiff_t)t->internal.memory.length - t->internal.memory.pos;
	if (bytesleft < 0)
		return 0;

	if (count > bytesleft) {
		// short read -- fill in any extra bytes with zeroes
		size_t tail = count - bytesleft;
		count = bytesleft;
		memset((unsigned char*)ptr + count, 0, tail);
	}

	if (count)
		memcpy(ptr, t->internal.memory.data + t->internal.memory.pos, count);

	return count;
}

static size_t slurp_memory_read_(slurp_t *t, void *ptr, size_t count)
{
	count = slurp_memory_peek_(t, ptr, count);
	slurp_memory_seek_(t, count, SEEK_CUR);
	return count;
}

static int slurp_memory_eof_(slurp_t *t)
{
	return t->internal.memory.pos >= t->internal.memory.length;
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
/* these just forward directly to the function pointers */

int slurp_seek(slurp_t *t, int64_t offset, int whence)
{
	return t->seek(t, offset, whence);
}

int64_t slurp_tell(slurp_t *t)
{
	return t->tell(t);
}

size_t slurp_peek(slurp_t *t, void *ptr, size_t count)
{
	return t->peek(t, ptr, count);
}

size_t slurp_read(slurp_t *t, void *ptr, size_t count)
{
	return t->read(t, ptr, count);
}

/* actual implementations */

size_t slurp_length(slurp_t *t)
{
	int64_t pos = slurp_tell(t);
	if (pos < 0)
		return 0;

	slurp_seek(t, 0, SEEK_END);

	int64_t end = slurp_tell(t);

	/* return to monke */
	slurp_seek(t, pos, SEEK_SET);

	return MAX(0, end);
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
	return t->eof(t);
}

int slurp_receive(slurp_t *t, int (*callback)(const void *, size_t, void *), size_t count, void *userdata)
{
	return t->receive(t, callback, count, userdata);
}
