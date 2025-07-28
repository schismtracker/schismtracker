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

#include "headers.h"
#include "disko.h"

/* global memory buffer for the test output */
static disko_t ds;

void test_output_clear(void)
{
	disko_memclose(&ds, 0);

	SCHISM_RUNTIME_ASSERT(disko_memopen(&ds) >= 0, "disko can't fail");
}

void test_outputn(const char *str, int len)
{
	disko_write(&ds, str, len);
}

void test_output(const char *str)
{
	test_outputn(str, strlen(str));
}

void test_vprintf(const char *fmt, va_list ap)
{
	char *s;
	int n;

	n = vasprintf(&s, fmt, ap);
	if (n < 0)
		return;

	test_outputn(s, n);

	free(s);
}

void test_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	test_vprintf(fmt, ap);
	va_end(ap);
}

void test_dump_output(void)
{
	if (!ds.length)
		return;

	printf("\nTEST OUTPUT: %.*s\n\n", (int)ds.length, ds.data);
}

