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

/* sanity.c: these don't test Schism, and actually just make sure
 * that everything works right. if not, there is probably a bug in
 * the OS or the libc. */

static char *ex_strlcpy(char *dst, const char *src, size_t len)
{
	/* This is trivial to implement off of strncpy */
	switch (len) {
	default: /* len > 1 */
		strncpy(dst, src, len-1);
		SCHISM_FALLTHROUGH;
	case 1:
		dst[len-1] = 0;
		SCHISM_FALLTHROUGH;
	case 0:
		return dst;
	}
}

/* test_sanity_time: makes sure that
 *  time() -> localtime() -> mktime()
 * always returns back the same value for time_t */
testresult_t test_sanity_time(void)
{
	time_t x, r;
	struct tm t;

	time(&x);

	/* Note: this can actually fuck up if someone happens to
	 * change the time zone between these two calls (race-y) */
	localtime_r(&x, &t);

	r = mktime(&t);

	if (x != r) {
		/* 64 chars ought to be enough for anybody */
		char buf1[64], buf2[64], buf3[64];

		ex_strlcpy(buf1, ctime(&x), sizeof(buf1));
		ex_strlcpy(buf2, ctime(&r), sizeof(buf2));
		ex_strlcpy(buf3, asctime(&t), sizeof(buf3));

		test_log_printf(
			" time(NULL) and mktime(localtime((time_t[]){ time(NULL); })) have different values!\n"
			" something is very wrong.\n"
			"\n"
			" time(...): %s"
			" localtime(...): %s"
			" mktime(...): %s",
			buf1, buf3, buf2
		);
		RETURN_FAIL;
	}

	RETURN_PASS;
}