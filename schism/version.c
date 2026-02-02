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
#include "it.h"
#include "mem.h"
#include "version.h"

#define TOP_BANNER_CLASSIC "Impulse Tracker v2.14 Copyright (C) 1995-1998 Jeffrey Lim"

/*
The schism "version" is simply a count of the days elapsed since
Oct 31, 2009. Note that there have been a few bugs that crept in,
for example:

Mar 3, 2025  -- added commit 0026465 which had versions off by
                roughly a month
Mar 5, 2025  -- version 20250305 is released, this version saved
                files as "20250205"
Mar 7, 2025  -- add commit d0ffb96 which fixed the bug introduced
                by the commit on Mar 3, 2025
Mar 13, 2025 -- version 20250313 is released

So, any file with a version indicating a mythical "20250205" version
is likely version 20250305 in disguise. However, it could also be
a development build actually from the 5th of February.

Another bug was introduced with the Mar 3 commit, in that any dates
in January would have its year/day be incremented/decremented
respectfully. One example of a module affected by this bug is
	"v39.it" -- https://s3m.it/file/8419
which was created in a development version 2026-01-31, but was saved
with a version indicating 2027-01-30. Such files are likely rare to
come across unless someone used a dev build from January 2026. This
bug was fixed on Feb 1, 2026 with commit be04324.

*/

/*
Information at our disposal:

	VERSION
		"" or "YYYYMMDD"
		A date here is the date of the last commit from git.
		If there is no version, then VERSION is an empty string, and EMPTY_VERSION is defined.

	__DATE__        "Jun  3 2009"
	__TIME__        "23:39:19"
		These are the current date and time of the machine running the compilation. These
		don't really say anything about the age of the code itself, so we only use it as a
		last resort. It's standard though, so it should work everywhere.

	__TIMESTAMP__   "Wed Jun  3 23:39:19 2009"
		This is a timestamp of when the compilation unit was last edited. This is much more
		useful than __DATE__ and __TIME__, but is specific to GNU C, so we can only use it
		on gcc-like compilers. (not like anyone's really using schism with anything else...)

*/
#if !defined(EMPTY_VERSION) && defined(VERSION)
# define TOP_BANNER_NORMAL "Schism Tracker " VERSION
#else
# define TOP_BANNER_NORMAL "Schism Tracker built " __DATE__ " " __TIME__
#endif
	;

/* -------------------------------------------------------- */
/* These are intended to work entirely without time_t to avoid
 * overflow on 32-bit systems, since the way Schism stores the
 * version date will overflow after 32-bit time_t does.
 *
 * I've tested these functions on many different dates and they
 * work fine from what I can tell. */

/* macros ! */
#define LEAP_YEAR(y) ((year) % 4 == 0 && (year) % 100 != 0)
#define LEAP_YEARS_BEFORE(y) ((((y) - 1) / 4) - (((y) - 1) / 100) + (((y) - 1) / 400))
#define LEAP_YEARS_BETWEEN(start, end) (LEAP_YEARS_BEFORE(end) - LEAP_YEARS_BEFORE(start + 1))

#define EPOCH_YEAR 2009
#define EPOCH_MONTH 10
#define EPOCH_DAY 31

/* -------------------------------------------------------------- */

// only used by ver_mktime, do not use directly
// see https://alcor.concordia.ca/~gpkatch/gdate-algorithm.html
// for a description of the algorithm used here
static inline SCHISM_ALWAYS_INLINE SCHISM_CONST
int64_t ver_date_encode(uint32_t y, uint32_t m, uint32_t d)
{
	int64_t mm, yy;

	mm = (m + 9LL) % 12LL;
	yy = y - (mm / 10LL);

	return (yy * 365LL) + (yy / 4LL) - (yy / 100LL) + (yy / 400LL) + ((mm * 306LL + 5LL) / 10LL) + (d - 1LL);
}

static inline SCHISM_ALWAYS_INLINE
void ver_date_decode(int64_t date, uint32_t *py, uint32_t *pm, uint32_t *pd)
{
	int64_t y, ddd, mi;

	y = ((date * 10000LL) + 14780LL) / 3652425LL;
	ddd = date - ((365LL * y) + (y / 4LL) - (y / 100LL) + (y / 400LL));
	if (ddd < 0) {
		y--;
		ddd = date - ((365LL * y) + (y / 4LL) - (y / 100LL) + (y / 400LL));
	}
	mi = ((100LL * ddd) + 52LL) / 3060LL;

	*py = (y + ((mi + 2LL) / 12LL));
	*pm = ((mi + 2LL) % 12LL) + 1LL;
	*pd = ddd - ((mi * 306LL + 5LL) / 10LL) + 1LL;
}

/* TODO better name */
uint32_t ver_mktime(uint32_t y, uint32_t m, uint32_t d)
{
	int64_t date, res, epoch;

	epoch = ver_date_encode(EPOCH_YEAR, EPOCH_MONTH, EPOCH_DAY);

	date = ver_date_encode(y, m, d);

	if (date < epoch)
		return 0;

	res = date - epoch;

	return (uint32_t)MIN(res, UINT32_MAX);
}

/* convert a version number to date */
int ver_to_date(uint32_t ver, uint32_t *py, uint32_t *pm, uint32_t *pd)
{
	int64_t epoch = ver_date_encode(EPOCH_YEAR, EPOCH_MONTH, EPOCH_DAY);
	ver_date_decode(epoch + ver, py, pm, pd);
	return 0;
}

/* ----------------------------------------------------------------- */

/*
Lower 12 bits of the CWTV field in IT and S3M files.

"Proper" version numbers went the way of the dodo, but we can't really fit an eight-digit date stamp directly
into a twelve-bit number. Since anything < 0x50 already carries meaning (even though most of them weren't
used), we have 0xfff - 0x50 = 4015 possible values. By encoding the date as an offset from a rather arbitrarily
chosen epoch, there can be plenty of room for the foreseeable future.

  < 0x020: a proper version (files saved by such versions are likely very rare)
  = 0x020: any version between the 0.2a release (2005-04-29?) and 2007-04-17
  = 0x050: anywhere from 2007-04-17 to 2009-10-31 (version was updated to 0x050 in hg changeset 2f6bd40c0b79)
  > 0x050: the number of days since 2009-10-31, for example:
	0x051 = (0x051 - 0x050) + 2009-10-31 = 2009-11-01
	0x052 = (0x052 - 0x050) + 2009-10-31 = 2009-11-02
	0x14f = (0x14f - 0x050) + 2009-10-31 = 2010-07-13
	0xffe = (0xfff - 0x050) + 2009-10-31 = 2020-10-27
  = 0xfff: a non-value indicating a date after 2020-10-27. in this case, the full version number is stored in a reserved header field.
		   this field follows the same format, using the same epoch, but without adding 0x50. */
uint16_t ver_cwtv;
uint32_t ver_reserved;

/* these should be 50 characters or shorter, as they are used in the startup dialog */
const char *ver_short_copyright =
	"Copyright (c) 2003-2026 Storlek, Mrs. Brisby et al.";
const char *ver_short_based_on =
	"Based on Impulse Tracker by Jeffrey Lim aka Pulse";

/* SEE ALSO: helptext/copyright (contains full copyright information, credits, and GPL boilerplate) */

const char *schism_banner(int classic)
{
	return (classic
		? TOP_BANNER_CLASSIC
		: TOP_BANNER_NORMAL);
}

void ver_decode_cwtv(uint16_t cwtv, uint32_t reserved, char buf[11])
{
	cwtv &= 0xfff;
	if (cwtv > 0x050) {
		uint32_t y, m, d;

		ver_to_date((cwtv < 0xFFF) ? ((uint32_t)cwtv - 0x050) : reserved, &y, &m, &d);

		y = CLAMP(y, EPOCH_YEAR, 9999);
		/* these two don't necessarily need to be clamped,
		 * but we're doing it for good measure */
		m = CLAMP(m, 1, 12);
		d = CLAMP(d, 1, 31);

		/* make gcc shut up */
		if (y < EPOCH_YEAR || y > 9999 || m < 1 || m > 12 || d < 1 || d > 31)
			SCHISM_UNREACHABLE;

		// Classic Mac OS's snprintf is not C99 compliant (duh) so we need
		// to cast our integers to unsigned long first.
		// We should probably have a replacement snprintf in case we don't
		// actually have a standard one.
		snprintf(buf, 11, "%04" PRIu32 "-%02" PRIu32 "-%02" PRIu32, y, m, d);
	} else {
		snprintf(buf, 11, "0.%x", cwtv);
	}
}

/* ----------------------------------------------------------------- */
/* parsers */

static inline SCHISM_ALWAYS_INLINE
int lookup_short_month(const char *name)
{
	static const char month_names[12][3] = {
		"Jan",
		"Feb",
		"Mar",
		"Apr",
		"May",
		"Jun",
		"Jul",
		"Aug",
		"Sep",
		"Oct",
		"Nov",
		"Dec",
	};
	size_t i;

	if (strlen(name) < 3)
		return -1;

	for (i = 0; i < ARRAY_SIZE(month_names); i++)
		if (!memcmp(month_names[i], name, 3))
			return i + 1;

	return -1;
}

/* The return value of this isn't really used anywhere,
 * it's just to make sure timestamps strings are of the
 * correct format. */
static inline SCHISM_ALWAYS_INLINE
int lookup_weekday(const char *name)
{
	static const char weekday_names[7][3] = {
		"Sun",
		"Mon",
		"Tue",
		"Wed",
		"Thu",
		"Fri",
		"Sat",
	};
	size_t i;

	if (strlen(name) < 3)
		return -1;

	for (i = 0; i < ARRAY_SIZE(weekday_names); i++)
		if (!memcmp(weekday_names[i], name, 3))
			return i;

	return -1;
}

static int ver_parse_num(const char *x, size_t len, uint32_t *res)
{
	size_t i;

	*res = 0;

	for (i = 0; i < len; i++) {
		if (!isdigit(x[i]))
			return -1;

		*res *= 10;
		*res += (x[i] - '0');
	}

	return 0;
}

static int ver_parse_cday(const char x[2], uint32_t *res)
{
	uint32_t day;

	if (!isdigit(x[1]))
		return -1;

	if (x[0] >= '1' && x[0] <= '3') {
		day = (x[0] - '0') * 10;
	} else if (x[0] != ' ') {
		/* fail */
		return -1;
	} else {
		day = 0;
	}

	day += (x[1] - '0');

	*res = day;

	return 0;
}

/* Parse a schism version "YYYYMMDD" */
int ver_parse_schism_version(const char *ver, uint32_t *pyear, uint32_t *pmonth, uint32_t *pday)
{
	uint32_t year, month, day;

	if (strlen(ver) != 8
			|| ver_parse_num(ver, 4, &year) != 0
			|| ver_parse_num(ver + 4, 2, &month) != 0
			|| ver_parse_num(ver + 6, 2, &day) != 0)
		return -1;

	*pyear = year;
	*pmonth = month;
	*pday = day;
	return 0;
}

int ver_parse_ctimestamp(const char *timestamp, uint32_t *pyear, uint32_t *pmonth, uint32_t *pday)
{
	uint32_t year, day;
	int m;

	if (strlen(timestamp) != 24)
		return -1;

	/* get these out of the way first */
	if (!isdigit(timestamp[11]) || !isdigit(timestamp[12]) || timestamp[13] != ':'
			|| !isdigit(timestamp[14]) || !isdigit(timestamp[15]) || timestamp[16] != ':'
			|| !isdigit(timestamp[17]) || !isdigit(timestamp[18])
			|| timestamp[3] != ' ' || timestamp[7] != ' ' || timestamp[19] != ' ')
		return -1;

	if (lookup_weekday(timestamp) == -1)
		return -1;

	m = lookup_short_month(timestamp + 4);
	if (m == -1)
		return -1;

	if (ver_parse_cday(timestamp + 8, &day) == -1)
		return -1;

	if (ver_parse_num(timestamp + 20, 4, &year) == -1)
		return -1;

	*pyear = year;
	*pmonth = m;
	*pday = day;

	return 0;
}

int ver_parse_cdate(const char *cdate, uint32_t *pyear, uint32_t *pmonth, uint32_t *pday)
{
	uint32_t year, day;
	int m;

	if (strlen(cdate) != 11)
		return -1; /* this will break in the year 10000 */

	/* lookup_short_month will only account for the first 3 characters
	 * in the buffer, which allows us to avoid unnecessary copying */
	m = lookup_short_month(cdate);
	if (m == -1)
		return -1;

	if (cdate[3] != ' ')
		return -1; /* invalid */

	if (ver_parse_cday(cdate + 4, &day) == -1)
		return -1;

	if (cdate[6] != ' ')
		return -1;

	if (ver_parse_num(cdate + 7, 4, &year) != 0)
		return -1;

	*pyear = year;
	*pmonth = m;
	*pday = day;

	return 0;
}

// Tries multiple methods to get a reasonable date to start with.
static int get_version_date(uint32_t *pyear, uint32_t *pmonth, uint32_t *pday)
{
#if !defined(EMPTY_VERSION) && defined(VERSION)
	if (ver_parse_schism_version(VERSION, pyear, pmonth, pday) == 0)
		return 0;
#endif

#ifdef __TIMESTAMP__
	/* The last time THIS source file was actually edited */
	if (ver_parse_ctimestamp(__TIMESTAMP__, pyear, pmonth, pday) == 0)
		return 0;
#endif

	if (ver_parse_cdate(__DATE__, pyear, pmonth, pday) == 0)
		return 0;

	/* give up... */
	return -1;
}

void ver_init(void)
{
	uint32_t year, month, day;
	uint32_t version_sec;

	if (get_version_date(&year, &month, &day) == 0) {
		version_sec = ver_mktime(year, month, day);
	} else {
		puts("help, I am very confused about myself");
		version_sec = 0;
	}

	ver_cwtv = 0x050 + version_sec;
	ver_reserved = (ver_cwtv < 0xfff) ? 0 : version_sec;
	ver_cwtv = CLAMP(ver_cwtv, 0x050, 0xfff);
}
