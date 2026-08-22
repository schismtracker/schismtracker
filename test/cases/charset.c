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

#include "test.h"
#include "test-assertions.h"

#include "charset.h"

#define TEST_CHARSET_ICONV_V2_BEGIN(name, inset, outset, str, expectoutput) \
	testresult_t test_charset_iconv_v2##name(void) \
	{ \
		static const char *expect = expectoutput; \
		struct charset_iconv_v2 *x; \
		char inbuf[sizeof(str)] = str; \
		char *inbufptr = inbuf; \
		size_t inbufsz = sizeof(inbuf); \
		char outbuf[sizeof(expectoutput)]; \
		char *outbufptr = outbuf; \
		size_t outbufsz = sizeof(outbuf); \
	\
		x = charset_iconv_v2_open(inset, outset, 0); \
		REQUIRE(x);

#define TEST_CHARSET_ICONV_V2_END \
		ASSERT(inbufptr == (inbuf + sizeof(inbuf))); \
		ASSERT(inbufsz == 0); \
		ASSERT(outbufptr == (outbuf + sizeof(outbuf))); \
		ASSERT(outbufsz == 0); \
	\
		ASSERT(memcmp(expect, outbuf, sizeof(outbuf)) == 0); \
	\
		charset_iconv_v2_close(x); \
	\
		RETURN_PASS; \
	}

#define TEST_STRING \
	"Hello, this is my test string in UTF-8 encoding\n" \
	"\0That is a NUL terminator to make sure we can properly" \
	"convert even after a NUL byte is encountered"

TEST_CHARSET_ICONV_V2_BEGIN(, CHARSET_UTF8, CHARSET_CP437, TEST_STRING, TEST_STRING)
{
	charset_error_t r;

	r = charset_iconv_v2(x, &inbufptr, &inbufsz, &outbufptr, &outbufsz);
	ASSERT_PRINTF(r >= 0, "%d", (int)r);
}
TEST_CHARSET_ICONV_V2_END

/* Like the above, but also tests calling between each char */
TEST_CHARSET_ICONV_V2_BEGIN(_partialbyte, CHARSET_UTF8, CHARSET_CP437, TEST_STRING, TEST_STRING)
{
	charset_error_t rc;
	size_t sz1 = outbufsz >> 1;

	outbufsz -= sz1;

	rc = charset_iconv_v2(x, &inbufptr, &inbufsz, &outbufptr, &sz1);
	ASSERT_PRINTF(rc == CHARSET_ERROR_NOTENOUGHSPACE, "%d", (int)rc);

	/* may have not filled the whole output buffer */
	outbufsz += sz1;

	rc = charset_iconv_v2(x, &inbufptr, &inbufsz, &outbufptr, &outbufsz);
	ASSERT_PRINTF(rc >= 0, "%zu %d", inbufsz, (int)rc);
}
TEST_CHARSET_ICONV_V2_END

/* Stress tests the case where we can decode a character, but there is
 * not enough space to store it. This is basically the same as above
 * but also tests when there *is* space in the buffer; just not enough
 * of it */
TEST_CHARSET_ICONV_V2_BEGIN(_partialcode, CHARSET_UTF8, CHARSET_UTF16BE, "\xC3\xA2", "\x00\xE2\0")
{
	/* ehhh */
	charset_error_t rc;
	size_t sz1 = 1;

	outbufsz -= sz1;

	rc = charset_iconv_v2(x, &inbufptr, &inbufsz, &outbufptr, &sz1);
	ASSERT_PRINTF(rc == CHARSET_ERROR_NOTENOUGHSPACE, "%d", (int)rc);

	outbufsz += sz1;

	rc = charset_iconv_v2(x, &inbufptr, &inbufsz, &outbufptr, &outbufsz);
	ASSERT_PRINTF(rc >= 0, "%zu %d %zu", inbufsz, (int)rc, outbufsz);
}
TEST_CHARSET_ICONV_V2_END

#ifdef SCHISM_WIN32
/* FIXME: Find some way to tell the ANSI code that we're running
 * a test, and to always assume Windows-1252; this would allow
 * testing the non-ASCII chars */

TEST_CHARSET_ICONV_V2_BEGIN(_ansi, CHARSET_ANSI, CHARSET_UTF8, "himynameiscarmenwinstead", "himynameiscarmenwinstead")
{
	charset_error_t r;

	r = charset_iconv_v2(x, &inbufptr, &inbufsz, &outbufptr, &outbufsz);
	ASSERT_PRINTF(r >= 0, "%d", (int)r);
}
TEST_CHARSET_ICONV_V2_END

TEST_CHARSET_ICONV_V2_BEGIN(_ansiout, CHARSET_UTF8, CHARSET_ANSI, "himynameiscarmenwinstead", "himynameiscarmenwinstead")
{
	charset_error_t r;

	r = charset_iconv_v2(x, &inbufptr, &inbufsz, &outbufptr, &outbufsz);
	ASSERT_PRINTF(r >= 0, "%d", (int)r);
}
TEST_CHARSET_ICONV_V2_END
#endif
