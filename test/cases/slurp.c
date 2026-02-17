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
#include "fmt.h"

#define EXPECTEDRESULTP1 "abc def ghi 123 456 789\nI liv"
#define EXPECTEDRESULTP2 "e in a giant bucket.\nbleugh.\n"
#define EXPECTEDRESULTPADDING "\0\0\0\0\0\0\0"

SCHISM_STATIC_ASSERT(sizeof(EXPECTEDRESULTP1) == sizeof(EXPECTEDRESULTP2), "a");

static const char expected_result[] =
	EXPECTEDRESULTP1 EXPECTEDRESULTP2;
static const char expected_result_padded[] =
	EXPECTEDRESULTP1 EXPECTEDRESULTPADDING EXPECTEDRESULTP2;

/* the padding between memory segments; used in 2mem and sf2 tests
 * 7 is a completely arbitrary value. */
#define TEST_SLURP_PADDING (sizeof(EXPECTEDRESULTPADDING) - 1)
#define TEST_SLURP_2SIZE (sizeof(EXPECTEDRESULTP1) - 1)

static testresult_t test_slurp_common(slurp_t *fp)
{
	char buf[ARRAY_SIZE(expected_result) - 1];
	static const char zero[ARRAY_SIZE(buf) >> 1] = {0};
	size_t i;
	size_t j;

	ASSERT(slurp_length(fp) == sizeof(buf));

	/* go over every possible (legal) combination of reads.
	 * there's probably a simpler way to do this, but oh well. */
	for (i = 0; i < sizeof(buf); i++) {
		for (j = 0; j < (sizeof(buf) - i); j++) {
			ASSERT(slurp_seek(fp, i, SEEK_SET) == 0);
			ASSERT(slurp_read(fp, buf, sizeof(buf) - i - j) == (sizeof(buf) - i - j));

			ASSERT(slurp_tell(fp) == (sizeof(buf) - j));

			/*printf("%" PRIuSZ ", %" PRIuSZ ": buf: %.*s", i, (int)(sizeof(buf) - i - j), buf);*/

			/* data should not change */
			ASSERT(!memcmp(buf, expected_result + i, sizeof(buf) - i - j));

			/* we should never trigger EOF here */
			ASSERT(!slurp_eof(fp));
		}
	}

	/* now, we are at the end of the file.
	 * try reading a bit more, and make sure EOF stays flagged.
	 * do it many times to make sure that extra reads have no effect on EOF. */

	for (i = 0; i < 5; i++) {
		size_t x;
		x = slurp_read(fp, buf, sizeof(buf));
		ASSERT_PRINTF(x == 0, "%" PRIuSZ, x);
		ASSERT(slurp_tell(fp) == sizeof(buf));
		ASSERT(slurp_eof(fp));
	}

	/* TODO what should the behavior be for slurp_peek regarding EOF?
	 * TBH I think it should stay the exact same as it was... */

	/* random operations should have no effect on EOF */
	(void)slurp_tell(fp);
	ASSERT(slurp_eof(fp));
	(void)slurp_getc(fp);
	ASSERT(slurp_eof(fp));

	/* getting the length should not affect EOF
	 * (i.e., it should not call seek()) */
	(void)slurp_length(fp);
	ASSERT(slurp_eof(fp));

	/* seeking to the end should not cause EOF */
	ASSERT(slurp_seek(fp, 0, SEEK_END) == 0);
	ASSERT(!slurp_eof(fp));

	/* random operations should have no effect on EOF */
	(void)slurp_tell(fp);
	ASSERT(!slurp_eof(fp));

	/* getting the length should not affect EOF */
	(void)slurp_length(fp);
	ASSERT(!slurp_eof(fp));

	/* any reads should trigger EOF */
	(void)slurp_getc(fp);
	ASSERT(slurp_eof(fp));

	/* read should zero out remaining bytes */
	memset(buf, 0xFF, sizeof(buf));
	ASSERT(slurp_seek(fp, sizeof(buf) >> 1, SEEK_SET) == 0);
	ASSERT(slurp_read(fp, buf, sizeof(buf)) == (sizeof(buf) >> 1));
	ASSERT(memcmp(buf + sizeof(zero), zero, sizeof(zero)) == 0);

	/* verify state */
	ASSERT(slurp_tell(fp) == sizeof(buf));
	ASSERT(slurp_getc(fp) == EOF);
	ASSERT(slurp_eof(fp));

	/* peek should zero out remaining bytes */
	memset(buf, 0xFF, sizeof(buf));
	ASSERT(slurp_seek(fp, sizeof(buf) >> 1, SEEK_SET) == 0);
	ASSERT(slurp_peek(fp, buf, sizeof(buf)) == (sizeof(buf) >> 1));
	ASSERT(memcmp(buf + sizeof(zero), zero, sizeof(zero)) == 0);

	/* verify state */
	ASSERT(slurp_tell(fp) == (sizeof(buf) >> 1));
	ASSERT(slurp_getc(fp) == 101);
	ASSERT(!slurp_eof(fp));

	/* seeking should clear any EOF flag */
	ASSERT(slurp_seek(fp, 0, SEEK_SET) == 0);
	ASSERT(!slurp_eof(fp));

	/* limit should only allow reading X bytes
	 * also test read zero behavior here :) */
	slurp_limit(fp, sizeof(buf) >> 1);
	memset(buf, 0xFF, sizeof(buf));
	ASSERT(slurp_read(fp, buf, sizeof(buf)) == (sizeof(buf) >> 1));
	ASSERT(memcmp(buf + sizeof(zero), zero, sizeof(zero)) == 0);
	ASSERT(slurp_tell(fp) == (sizeof(buf) >> 1));
	/* any subsequent reads should never return any bytes */
	ASSERT(slurp_read(fp, buf, sizeof(buf)) == 0);
	ASSERT(slurp_tell(fp) == (sizeof(buf) >> 1));
	/* XXX slurp_eof should probably return 1 here */
	slurp_unlimit(fp);

	/* seek past EOF behavior */
	ASSERT(slurp_seek(fp, 0, SEEK_SET) == 0);
	ASSERT(slurp_seek(fp, sizeof(buf), SEEK_SET) == 0);
	ASSERT(slurp_tell(fp) == sizeof(buf));
	ASSERT(slurp_seek(fp, sizeof(buf) + 1, SEEK_SET) == -1);
	ASSERT(slurp_tell(fp) == sizeof(buf));

	/* slurp_available */
	ASSERT(slurp_seek(fp, 0, SEEK_SET) == 0);
	ASSERT(slurp_available(fp, sizeof(buf), SEEK_SET));
	ASSERT(!slurp_available(fp, sizeof(buf) + 1, SEEK_SET));

	/* verify state */
	ASSERT(slurp_tell(fp) == 0);
	ASSERT(!slurp_eof(fp));

	RETURN_PASS;
}

testresult_t test_slurp_memstream(void)
{
	slurp_t fp;
	testresult_t r;

	/* should never happen */
	ASSERT(slurp_memstream(&fp, expected_result, sizeof(expected_result) - 1) >= 0);

	r = test_slurp_common(&fp);

	/* currently not necessary for memstream, but whatever */
	unslurp(&fp);

	return r;
}

testresult_t test_slurp_2memstream(void)
{
	slurp_t fp;
	testresult_t r;

	/* should never happen */
	ASSERT(slurp_2memstream(&fp, (const uint8_t *)expected_result_padded,
			(const uint8_t *)expected_result_padded + TEST_SLURP_2SIZE + TEST_SLURP_PADDING, TEST_SLURP_2SIZE) >= 0);

	r = test_slurp_common(&fp);

	/* currently not necessary for memstream, but whatever */
	unslurp(&fp);

	return r;
}

testresult_t test_slurp_sf2(void)
{
	slurp_t memfp;
	slurp_t sf2fp;
	testresult_t r;

	/* this 97 year old NYC diner still serves Coke the old fashioned way */
	ASSERT(slurp_memstream(&memfp, (const uint8_t *)expected_result_padded, sizeof(expected_result_padded)) >= 0);

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

	stdfp = test_temp_file2(tmp, expected_result, ARRAY_SIZE(expected_result) - 1);
	REQUIRE(stdfp);

	REQUIRE(slurp_stdio(&fp, stdfp) == SLURP_OPEN_SUCCESS);

	r = test_slurp_common(&fp);

	unslurp(&fp);
	fclose(stdfp);

	return r;
}

#ifdef SCHISM_WIN32
testresult_t test_slurp_win32(void)
{
	slurp_t fp;
	char tmp[TEST_TEMP_FILE_NAME_LENGTH];
	testresult_t r;

	REQUIRE(test_temp_file(tmp, expected_result, ARRAY_SIZE(expected_result) - 1));

	REQUIRE(slurp_win32(&fp, tmp, 0) == SLURP_OPEN_SUCCESS);

	r = test_slurp_common(&fp);

	unslurp(&fp);

	return r;
}

testresult_t test_slurp_win32_mmap(void)
{
	slurp_t fp;
	char tmp[TEST_TEMP_FILE_NAME_LENGTH];
	testresult_t r;

	REQUIRE(test_temp_file(tmp, expected_result, ARRAY_SIZE(expected_result) - 1));

	REQUIRE(slurp_win32_mmap(&fp, tmp, ARRAY_SIZE(expected_result) - 1) == SLURP_OPEN_SUCCESS);

	r = test_slurp_common(&fp);

	unslurp(&fp);

	return r;
}
#endif

#ifdef HAVE_MMAP
testresult_t test_slurp_mmap(void)
{
	slurp_t fp;
	char tmp[TEST_TEMP_FILE_NAME_LENGTH];
	testresult_t r;

	REQUIRE(test_temp_file(tmp, expected_result, ARRAY_SIZE(expected_result) - 1));

	REQUIRE(slurp_mmap(&fp, tmp, ARRAY_SIZE(expected_result) - 1) == SLURP_OPEN_SUCCESS);

	r = test_slurp_common(&fp);

	unslurp(&fp);

	return r;
}
#endif

static testresult_t test_slurp_decompress(int (*init)(void), int (*start)(slurp_t *fp), void (*quit)(void),
	const unsigned char *data, size_t datalen)
{
	testresult_t r;
	slurp_t fp;
	static const unsigned char expected_result_gz[] = {
		'\037', '\213', '\010', '\010', '\234', '\107', '\333', '\150', '\002', '\003', '\145', '\170', '\160', '\145', '\143', '\164',
		'\145', '\144', '\137', '\162', '\145', '\163', '\165', '\154', '\164', '\000', '\113', '\114', '\112', '\126', '\110', '\111',
		'\115', '\123', '\110', '\317', '\310', '\124', '\060', '\064', '\062', '\126', '\060', '\061', '\065', '\123', '\060', '\267',
		'\260', '\344', '\362', '\124', '\310', '\311', '\054', '\113', '\125', '\310', '\314', '\123', '\110', '\124', '\110', '\317',
		'\114', '\314', '\053', '\121', '\110', '\052', '\115', '\316', '\116', '\055', '\321', '\343', '\112', '\312', '\111', '\055',
		'\115', '\317', '\320', '\343', '\002', '\000', '\240', '\062', '\045', '\375', '\072', '\000', '\000', '\000'
	};

	/* should never happen */
	ASSERT(slurp_memstream(&fp, data, datalen) >= 0);

	/* -1 is an error (e.g. lib failed to load) */
	REQUIRE(init() >= 0);
	/* -1 is an error (e.g. lib failed to decompress) */
	REQUIRE(start(&fp) >= 0);

	r = test_slurp_common(&fp);

	unslurp(&fp);
	quit();

	return r;
}

#ifdef USE_ZLIB
testresult_t test_slurp_gzip(void)
{
	static const unsigned char expected_result_gz[] = {
		'\037', '\213', '\010', '\010', '\234', '\107', '\333', '\150', '\002', '\003', '\145', '\170', '\160', '\145', '\143', '\164',
		'\145', '\144', '\137', '\162', '\145', '\163', '\165', '\154', '\164', '\000', '\113', '\114', '\112', '\126', '\110', '\111',
		'\115', '\123', '\110', '\317', '\310', '\124', '\060', '\064', '\062', '\126', '\060', '\061', '\065', '\123', '\060', '\267',
		'\260', '\344', '\362', '\124', '\310', '\311', '\054', '\113', '\125', '\310', '\314', '\123', '\110', '\124', '\110', '\317',
		'\114', '\314', '\053', '\121', '\110', '\052', '\115', '\316', '\116', '\055', '\321', '\343', '\112', '\312', '\111', '\055',
		'\115', '\317', '\320', '\343', '\002', '\000', '\240', '\062', '\045', '\375', '\072', '\000', '\000', '\000'
	};

	return test_slurp_decompress(gzip_init, slurp_gzip, gzip_quit, expected_result_gz, sizeof(expected_result_gz));
}
#endif

#ifdef USE_BZIP2
testresult_t test_slurp_bzip2(void)
{
	static const unsigned char expected_result_bz2[] = {
		'\102', '\132', '\150', '\071', '\061', '\101', '\131', '\046', '\123', '\131', '\203', '\024', '\220', '\211', '\000', '\000',
		'\015', '\135', '\200', '\000', '\020', '\100', '\001', '\077', '\340', '\000', '\040', '\077', '\355', '\007', '\000', '\040',
		'\000', '\110', '\212', '\157', '\112', '\075', '\103', '\106', '\203', '\311', '\250', '\323', '\152', '\024', '\000', '\015',
		'\000', '\001', '\266', '\062', '\174', '\347', '\157', '\225', '\325', '\333', '\324', '\343', '\236', '\207', '\152', '\032',
		'\265', '\071', '\055', '\001', '\275', '\304', '\132', '\360', '\310', '\205', '\004', '\322', '\031', '\171', '\064', '\000',
		'\155', '\352', '\062', '\104', '\135', '\311', '\024', '\341', '\102', '\102', '\014', '\122', '\102', '\044',
	};

	return test_slurp_decompress(bzip2_init, slurp_bzip2, bzip2_quit, expected_result_bz2, sizeof(expected_result_bz2));
}
#endif

#ifdef USE_LZMA
testresult_t test_slurp_xz(void)
{
	static const unsigned char expected_result_xz[] = {
		'\375', '\067', '\172', '\130', '\132', '\000', '\000', '\004', '\346', '\326', '\264', '\106', '\004', '\300', '\076', '\072',
		'\041', '\001', '\034', '\000', '\000', '\000', '\000', '\000', '\000', '\000', '\000', '\000', '\030', '\077', '\025', '\236',
		'\001', '\000', '\071', '\141', '\142', '\143', '\040', '\144', '\145', '\146', '\040', '\147', '\150', '\151', '\040', '\061',
		'\062', '\063', '\040', '\064', '\065', '\066', '\040', '\067', '\070', '\071', '\012', '\111', '\040', '\154', '\151', '\166',
		'\145', '\040', '\151', '\156', '\040', '\141', '\040', '\147', '\151', '\141', '\156', '\164', '\040', '\142', '\165', '\143',
		'\153', '\145', '\164', '\056', '\012', '\142', '\154', '\145', '\165', '\147', '\150', '\056', '\012', '\000', '\000', '\000',
		'\324', '\341', '\365', '\105', '\203', '\376', '\146', '\206', '\000', '\001', '\132', '\072', '\107', '\331', '\336', '\246',
		'\037', '\266', '\363', '\175', '\001', '\000', '\000', '\000', '\000', '\004', '\131', '\132',
	};

	return test_slurp_decompress(xz_init, slurp_xz, xz_quit, expected_result_xz, sizeof(expected_result_xz));
}
#endif

#ifdef USE_ZSTD
testresult_t test_slurp_zstd(void)
{
	static unsigned char expected_result_zstd[] = {
		'\050', '\265', '\057', '\375', '\044', '\072', '\321', '\001', '\000', '\141', '\142', '\143', '\040', '\144', '\145', '\146',
		'\040', '\147', '\150', '\151', '\040', '\061', '\062', '\063', '\040', '\064', '\065', '\066', '\040', '\067', '\070', '\071',
		'\012', '\111', '\040', '\154', '\151', '\166', '\145', '\040', '\151', '\156', '\040', '\141', '\040', '\147', '\151', '\141',
		'\156', '\164', '\040', '\142', '\165', '\143', '\153', '\145', '\164', '\056', '\012', '\142', '\154', '\145', '\165', '\147',
		'\150', '\056', '\012', '\367', '\153', '\060', '\247',
	};

	return test_slurp_decompress(zstd_init, slurp_zstd, zstd_quit, expected_result_zstd, sizeof(expected_result_zstd));
}
#endif
