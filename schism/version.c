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

#define NEED_TIME
#include "headers.h"
#include "it.h"
#include "sdlmain.h"
#include "version.h"


#define TOP_BANNER_CLASSIC "Impulse Tracker v2.14 Copyright (C) 1995-1998 Jeffrey Lim"

/* written by ver_init */
static char top_banner_normal[80];

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
unsigned short ver_cwtv;
unsigned short ver_reserved;

/* these should be 50 characters or shorter, as they are used in the startup dialog */
const char *ver_short_copyright =
	"Copyright (c) 2003-2022 Storlek, Mrs. Brisby et al.";
const char *ver_short_based_on =
	"Based on Impulse Tracker by Jeffrey Lim aka Pulse";

/* SEE ALSO: helptext/copyright (contains full copyright information, credits, and GPL boilerplate) */

static time_t epoch_sec;


const char *schism_banner(int classic)
{
	return (classic
		? TOP_BANNER_CLASSIC
		: top_banner_normal);
}

/*
Information at our disposal:

	VERSION
		"" or "YYYYMMDD"
		A date here is the date of the last commit from git
		empty string will happen if git isn't installed, or no .git

	__DATE__        "Jun  3 2009"
	__TIME__        "23:39:19"
	__TIMESTAMP__   "Wed Jun  3 23:39:19 2009"
		These are annoying to manipulate beacuse of the month being in text format -- but at
		least I don't think they're ever localized, which would make it much more annoying.
		Should always exist, especially considering that we require gcc. However, it is a
		poor indicator of the age of the *code*, since it depends on the clock of the computer
		that's building the code, and also there is the possibility that someone was hanging
		onto the code for a really long time before building it.

*/

static int get_version_tm(struct tm *version)
{
	char *ret;

	memset(version, 0, sizeof(*version));
	ret = strptime(VERSION, "%Y %m %d", version);
	if (ret && !*ret)
		return 1;
	/* Argh. */
	memset(version, 0, sizeof(*version));
	ret = strptime(__DATE__, "%b %e %Y", version);
	if (ret && !*ret)
		return 1;
	/* Give up; we don't know anything. */
	return 0;
}

void ver_init(void)
{
	struct tm version, epoch = { .tm_year = 109, .tm_mon = 9, .tm_mday = 31 }; /* 2009-10-31 */
	time_t version_sec;
	char ver[32] = VERSION;

	if (get_version_tm(&version)) {
		version_sec = mktime(&version);
	} else {
		printf("help, I am very confused about myself\n");
		version_sec = epoch_sec;
	}

	epoch_sec = mktime(&epoch);
	version_sec = mktime(&version);
	ver_cwtv = 0x050 + (version_sec - epoch_sec) / 86400;
	ver_reserved = ver_cwtv < 0xfff ? 0 : (ver_cwtv - 0x050);
	ver_cwtv = CLAMP(ver_cwtv, 0x050, 0xfff);

	/* show build date if we don't know last commit date (no git) */
	if (ver[0]) {
		snprintf(top_banner_normal, sizeof(top_banner_normal) - 1,
			"Schism Tracker %s", ver);
	} else {
		snprintf(top_banner_normal, sizeof(top_banner_normal) - 1,
			"Schism Tracker built %s %s", __DATE__, __TIME__);
	}

	top_banner_normal[sizeof(top_banner_normal) - 1] = '\0'; /* to be sure */
}

void ver_decode_cwtv(uint16_t cwtv, uint32_t reserved, char *buf)
{
	struct tm version;
	time_t version_sec;

	cwtv &= 0xfff;
	if (cwtv > 0x050) {
		// Annoyingly, mktime uses local time instead of UTC. Why etc.
		version_sec = ((cwtv < 0xfff ? (cwtv - 0x050) : reserved) * 86400) + epoch_sec;
		if (localtime_r(&version_sec, &version)) {
			sprintf(buf, "%04d-%02d-%02d",
				version.tm_year + 1900, version.tm_mon + 1, version.tm_mday);
			return;
		}
	}
	sprintf(buf, "0.%x", cwtv);
}

