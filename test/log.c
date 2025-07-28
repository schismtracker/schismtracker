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

/* global memory buffer for the test log */
static disko_t ds = {0};
static int ds_init = 0;

void test_log_clear(void)
{
	if (ds_init)
		disko_memclose(&ds, 0);

	SCHISM_RUNTIME_ASSERT(disko_memopen(&ds) >= 0, "disko can't fail");
	ds_init = 1;
}

void test_logn(const char *str, int len)
{
	disko_write(&ds, str, len);
}

void test_log(const char *str)
{
	test_logn(str, strlen(str));
}

void test_log_vprintf(const char *fmt, va_list ap)
{
	char *s;
	int n;

	n = vasprintf(&s, fmt, ap);
	if (n < 0)
		return;

	test_logn(s, n);

	free(s);
}

void test_log_printf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	test_log_vprintf(fmt, ap);
	va_end(ap);
}

void test_log_dump(void)
{
	if (!ds.length)
		return;

	printf("\nTEST LOG: %.*s\n\n", (int)ds.length, ds.data);
}
