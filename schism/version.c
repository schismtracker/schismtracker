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
#include "version.h"
#include <inttypes.h>

#include <assert.h>

#define TOP_BANNER_CLASSIC "Impulse Tracker v2.14 Copyright (C) 1995-1998 Jeffrey Lim"

/*
Information at our disposal:

	VERSION
		"" or "YYYYMMDD"
		A date here is the date of the last commit from git
		empty string will happen if git isn't installed, or no .git

	__DATE__        "Jun  3 2009"
	__TIME__        "23:39:19"
	__TIMESTAMP__   "Wed Jun  3 23:39:19 2009"
		These are annoying to manipulate because of the month being in text format -- but at
		least I don't think they're ever localized, which would make it much more annoying.
		Should always exist, especially considering that we require gcc/clang. However, it is a
		poor indicator of the age of the *code*, since it depends on the clock of the computer
		that's building the code, and also there is the possibility that someone was hanging
		onto the code for a really long time before building it.

*/
static const char* top_banner_normal =
#ifndef EMPTY_VERSION
	"Schism Tracker " VERSION
#else
	"Schism Tracker built " __DATE__ " " __TIME__
#endif
	;

/* -------------------------------------------------------- */
/* These are intended to work entirely without time_t to avoid
 * overflow on 32-bit systems, since the way Schism stores the
 * version date will overflow after 32-bit time_t does.
 *
 * I've tested these functions on many different dates and they
 * work fine from what I can tell. */

/* days since schism version epoch (31 Oct 2009) */
typedef uint32_t version_time_t;

/* macros ! */
#define LEAP_YEAR(y) ((year) % 4 == 0 && (year) % 100 != 0)
#define LEAP_YEARS_BEFORE(y) ((((y) - 1) / 4) - (((y) - 1) / 100) + (((y) - 1) / 400))
#define LEAP_YEARS_BETWEEN(start, end) (LEAP_YEARS_BEFORE(end) - LEAP_YEARS_BEFORE(start + 1))

#define EPOCH_YEAR 2009
#define EPOCH_MONTH 9
#define EPOCH_DAY 31

SCHISM_CONST static inline version_time_t get_days_for_month(uint32_t month, uint32_t year)
{
	static const version_time_t days_for_month[12] = {
		31, /* January */
		28, /* February */
		31, /* March */
		30, /* April */
		31, /* May */
		30, /* June */
		31, /* July */
		31, /* August */
		30, /* September */
		31, /* October */
		30, /* November */
		31, /* December */
	};

	version_time_t month_days = days_for_month[month];

	if ((month == 1) && LEAP_YEAR(year))
		month_days++;

	return month_days;
}

SCHISM_CONST static inline version_time_t version_mktime(int y, int m, int d)
{
	uint32_t year = EPOCH_YEAR;
	uint32_t month = EPOCH_MONTH;
	int month_overflow = (m >= EPOCH_MONTH) ? 1 : 0;

	/* sanity check! */
	assert(m < 12 && d <= get_days_for_month(m, y));
	assert((y > EPOCH_YEAR) || (y == EPOCH_YEAR && m >= EPOCH_MONTH && d >= EPOCH_DAY));

	version_time_t ret = 0;

	/* year */
	while (year < y - 1 + month_overflow) {
		ret += (LEAP_YEAR(year) ? 366 : 365);
		year++;
	}

	/* month */
	while (month != m) {
		ret += get_days_for_month(month, year);

		month++;

		if (month > 11) {
			month = 0;
			year++;
		}
	}

	/* day */
	ret += d;
	ret -= EPOCH_DAY;

	return ret;
}

static inline void version_time_format(char buf[11], version_time_t ver)
{
	// FIXME: for some reason, classic mac os interprets this completely
	// wrong. It doesn't necessarily matter that much though, since it can
	// save the version info just fine, but it would be nice if this actually
	// worked over there.
	int64_t year = EPOCH_YEAR, month = EPOCH_MONTH, days = ver + EPOCH_DAY;
	int32_t days_in;

	for (;;) {
		days_in = (LEAP_YEAR(year) ? 366 : 365);
		if (days < days_in)
			break;

		days -= days_in;

		year++;
	}

	for (;;) {
		days_in = get_days_for_month(month, year);
		if (days < days_in)
			break;

		days -= days_in;

		month++;

		if (month > 11) {
			month = 0;
			year++;
		}
	}

	// shut up gcc
	year = CLAMP(year, EPOCH_YEAR, 9999);
	month = CLAMP(month, 0, 11);
	days = CLAMP(days, 1, 31);

	snprintf(buf, 11, "%04" PRId64 "-%02" PRId64 "-%02" PRId64, year, (month + 1), days);
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
	"Copyright (c) 2003-2022 Storlek, Mrs. Brisby et al.";
const char *ver_short_based_on =
	"Based on Impulse Tracker by Jeffrey Lim aka Pulse";

/* SEE ALSO: helptext/copyright (contains full copyright information, credits, and GPL boilerplate) */

const char *schism_banner(int classic)
{
	return (classic
		? TOP_BANNER_CLASSIC
		: top_banner_normal);
}

void ver_decode_cwtv(uint16_t cwtv, uint32_t reserved, char buf[11])
{
	cwtv &= 0xfff;
	if (cwtv > 0x050)
		version_time_format(buf, (cwtv < 0xfff) ? (cwtv - 0x050) : reserved);
	else
		sprintf(buf, "0.%x", cwtv);
}

static inline int get_version_tm(struct tm *version)
{
	// this was wrong for way too long
	memset(version, 0, sizeof(*version));
	// by the time we reach the year 10000 nobody will care that this breaks
	int amt = sscanf(VERSION, "%04d%02d%02d", &version->tm_year, &version->tm_mon, &version->tm_mday);
	if (amt == 3) {
		// fix this
		version->tm_year -= 1900;
		version->tm_mon--;
		return 1;
	}

	memset(version, 0, sizeof(*version));
	char *ret = strptime(__DATE__, "%b %e %Y", version);
	if (ret && !*ret)
		return 1;

	/* give up; we don't know anything */
	return 0;
}

void ver_init(void)
{
	struct tm version;
	version_time_t version_sec;

	if (get_version_tm(&version)) {
		version_sec = version_mktime(version.tm_year + 1900, version.tm_mon, version.tm_mday);
	} else {
		puts("help, I am very confused about myself");
		version_sec = 0;
	}

	ver_cwtv = 0x050 + version_sec;
	ver_reserved = (ver_cwtv < 0xfff) ? 0 : version_sec;
	ver_cwtv = CLAMP(ver_cwtv, 0x050, 0xfff);
}
