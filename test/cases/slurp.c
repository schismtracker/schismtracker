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
#include "test-tempfile.h"

#include "slurp.h"

static const char expected_result[] =
	"abc def ghi 123 456 789\n"
	"I live in a giant bucket.\n"
	"bleugh.\n";
SCHISM_STATIC_ASSERT((ARRAY_SIZE(expected_result) - 1) % 2 == 0,
	"need the size to be a multiple of two for slurp_2memstream");

/* the padding between memory segments; used in 2mem and sf2 tests
 * 7 is a completely arbitrary value. */
#define TEST_SLURP_PADDING (7)
#define TEST_SLURP_2SIZE ((ARRAY_SIZE(expected_result) - 1) / 2)

static testresult_t test_slurp_common(slurp_t *fp)
{
	/* TODO: split me up, this is test multiple concerns */
	char buf[ARRAY_SIZE(expected_result) - 1];
	size_t i;
	size_t j;

	ASSERT_EQUAL_LU(slurp_length(fp), sizeof(buf));

	/* go over every possible (legal) combination of reads.
	 * there's probably a simpler way to do this, but oh well. */
	for (i = 0; i < sizeof(buf); i++) {
		for (j = 0; j < (sizeof(buf) - i); j++) {
			ASSERT_EQUAL_D(slurp_seek(fp, i, SEEK_SET), 0);
			ASSERT_EQUAL_LU(slurp_read(fp, buf, sizeof(buf) - i - j), (sizeof(buf) - i - j));

			/*printf("%" PRIuSZ ", %" PRIuSZ ": buf: %.*s", i, (int)(sizeof(buf) - i - j), buf);*/

			/* data should not change */
			ASSERT(!memcmp(buf, expected_result + i, sizeof(buf) - i - j));

			/* we should never trigger EOF here */
			ASSERT_FALSE(slurp_eof(fp));
		}
	}

	/* now, we are at the end of the file.
	 * try reading a bit more, and make sure EOF stays flagged.
	 * do it many times to make sure that extra reads have no effect on EOF. */

	for (i = 0; i < 5; i++) {
		ASSERT_EQUAL_D(slurp_read(fp, buf, sizeof(buf)), 0);
		ASSERT_TRUE(slurp_eof(fp));
	}

	/* TODO what should the behavior be for slurp_peek regarding EOF?
	 * TBH I think it should stay the exact same as it was... */

	/* random operations should have no effect on EOF */
	(void)slurp_tell(fp);
	ASSERT_TRUE(slurp_eof(fp));
	(void)slurp_getc(fp);
	ASSERT_TRUE(slurp_eof(fp));

	/* getting the length should not affect EOF
	 * (i.e., it should not call seek()) */
	(void)slurp_length(fp);
	ASSERT_TRUE(slurp_eof(fp));

	/* seeking to the end should not cause EOF */
	ASSERT_EQUAL_D(slurp_seek(fp, 0, SEEK_END), 0);
	ASSERT_FALSE(slurp_eof(fp));

	/* random operations should have no effect on EOF */
	(void)slurp_tell(fp);
	ASSERT_FALSE(slurp_eof(fp));

	/* getting the length should not affect EOF */
	(void)slurp_length(fp);
	ASSERT_FALSE(slurp_eof(fp));

	/* any reads SHOULD affect EOF */
	(void)slurp_getc(fp);
	ASSERT_TRUE(slurp_eof(fp));

	/* seeking should clear any EOF flag */
	ASSERT_EQUAL_D(slurp_seek(fp, 0, SEEK_SET), 0);
	ASSERT_FALSE(slurp_eof(fp));

	/* TODO what should seeking past EOF do? */
	/* TODO test slurp_limit here as well */

	RETURN_PASS;
}

testresult_t test_slurp_memstream(void)
{
	slurp_t fp;
	uint8_t buf[ARRAY_SIZE(expected_result) - 1];
	testresult_t r;

	memcpy(buf, expected_result, sizeof(buf));

	/* should never happen */
	ASSERT(slurp_memstream(&fp, (uint8_t *)buf, sizeof(buf)) >= 0);

	r = test_slurp_common(&fp);

	/* currently not necessary for memstream, but whatever */
	unslurp(&fp);

	return r;
}

static void test_slurp_setup_padded_buf(const void *src, void *dst, size_t sz, size_t padding)
{
	/* copy the first half of the expected result */
	memcpy(dst, src, sz);
	/* set the next padding bytes to 0 */
	memset((char *)dst + sz, 0, padding);
	/* then, copy the second half of the expected result.
	 * this hopefully makes sure that if there's any buffer overrun within
	 * 2memstream, it can be detected easily. */
	memcpy((char *)dst + sz + padding, (char *)src + sz, sz);
}

testresult_t test_slurp_2memstream(void)
{
	slurp_t fp;
	uint8_t buf[(TEST_SLURP_2SIZE * 2) + TEST_SLURP_PADDING];
	testresult_t r;

	test_slurp_setup_padded_buf(expected_result, buf, TEST_SLURP_2SIZE, TEST_SLURP_PADDING);

	/* should never happen */
	ASSERT(slurp_2memstream(&fp, (uint8_t *)buf, (uint8_t *)buf + TEST_SLURP_2SIZE + TEST_SLURP_PADDING, TEST_SLURP_2SIZE) >= 0);

	r = test_slurp_common(&fp);

	/* currently not necessary for memstream, but whatever */
	unslurp(&fp);

	return r;
}

testresult_t test_slurp_sf2(void)
{
	slurp_t memfp;
	slurp_t sf2fp;
	uint8_t buf[(TEST_SLURP_2SIZE * 2) + TEST_SLURP_PADDING];
	testresult_t r;

	/* this 97 year old NYC diner still serves Coke the old fashioned way */
	test_slurp_setup_padded_buf(expected_result, buf, TEST_SLURP_2SIZE, TEST_SLURP_PADDING);

	ASSERT(slurp_memstream(&memfp, (uint8_t *)buf, sizeof(buf)) >= 0);

	slurp_sf2(&sf2fp, &memfp, 0, TEST_SLURP_2SIZE, TEST_SLURP_2SIZE + TEST_SLURP_PADDING, TEST_SLURP_2SIZE);

	r = test_slurp_common(&sf2fp);

	unslurp(&sf2fp);
	unslurp(&memfp);

	return r;
}

testresult_t test_slurp_stdio(void)
{
	slurp_t fp;
	char tmp[TEST_TEMP_FILE_NAME_LENGTH];
	FILE *stdfp;
	testresult_t r;

	REQUIRE(test_temp_file(tmp, expected_result, ARRAY_SIZE(expected_result) - 1));

	/* XXX we open the file, close the file, and then reopen it.
	 * this is a source for race conditions ... */
	stdfp = fopen(tmp, "rb");

	ASSERT_EQUAL_D_NAMED(slurp_stdio(&fp, stdfp), SLURP_OPEN_SUCCESS);

	r = test_slurp_common(&fp);

	unslurp(&fp);
	fclose(stdfp);

	return r;
}

/* TODO need to add slurp test functions for win32 */

#include "slurp.f"