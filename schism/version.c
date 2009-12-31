/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010 Storlek
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
  = 0xfff: a non-value indicating a date after 2020-10-27 (assuming Schism Tracker still exists then) */
short ver_cwtv;

/* this should be 50 characters or shorter, as they are used in the startup dialog */
const char *ver_short_copyright =
        "Copyright (c) 2003-2009 Storlek & Mrs. Brisby";
const char *ver_short_based_on =
        "Based on Impulse Tracker by Jeffrey Lim aka Pulse";

/* and these should be no more than 74 chars per line */
const char *ver_copyright_credits[] = {
        /* same as the boilerplate for each .c file */
        "Copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>",
        "Copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>",
        "Copyright (c) 2009 Storlek & Mrs. Brisby",
        "",
        "Based on Impulse Tracker which is copyright (C) 1995-1998 Jeffrey Lim.",
        "Contains code by Olivier Lapicque, Markus Fick, Adam Goode, Ville Jokela,",
        "Juan Linietsky, Juha Niemim\x84ki, and others. See the file AUTHORS in",
        "the source distribution for details.",
        NULL,
};
const char *ver_license[] = {
        "This program is free software; you can redistribute it and/or modify it",
        "under the terms of the GNU General Public License as published by the",
        "Free Software Foundation; either version 2 of the License, or (at your",
        "option) any later version.",
        "",
        "You should have received a copy of the GNU General Public License along",
        "with this program; if not, write to the Free Software Foundation, Inc.,",
        "59 Temple Place, Suite 330, Boston, MA 02111-1307  USA",
        NULL,
};

static time_t epoch_sec;


const char *schism_banner(int classic)
{
        return (classic
                ? TOP_BANNER_CLASSIC
                : top_banner_normal);
}

/*
Information at our disposal:

        HG_VERSION
                "YYYY-MM-DD"
                Only defined if .hg directory existed and hg client was installed when configuring.
                This is the best indicator of the version if it's defined.
        VERSION
                "hg" or "YYYYMMDD"
                A date here indicates that the source is from one of the periodically-buit packages
                on the homepage. Less reliable, but will probably be usable when HG_VERSION isn't.

        __DATE__        "Jun  3 2009"
        __TIME__        "23:39:19"
        __TIMESTAMP__   "Wed Jun  3 23:39:19 2009"
                These are annoying to manipulate beacuse of the month being in text format -- but at
                least I don't think they're ever localized, which would make it much more annoying.
                Should always exist, especially considering that we require gcc. However, it is a
                poor indicator of the age of the *code*, since it depends on the clock of the computer
                that's building the code, and also there is the possibility that someone was hanging
                onto the code for a really long time before building it.

If VERSION is "hg", but HG_VERSION is undefined or blank, then we are being built from one of the
snapshot packages, or the build system is somehow broken. (This might happen if hg was uninstalled
between cloning the repo and configuring, or the .hg directory was removed.)

Note: this is a hack, it'd be great to have something that doesn't require runtime logic.
Fortunately, most of this should be able to be optimized down to static assignment.
*/

static int get_version_tm(struct tm *version)
{
        char *ret;
#ifdef HG_VERSION
        memset(version, 0, sizeof(*version));
        /* Try this first, but make sure it's not broken (can happen if 'hg' is removed after configure) */
        if (HG_VERSION[0] >= '0' && HG_VERSION[0] <= '9') {
                ret = strptime(HG_VERSION, "%Y-%m-%d", version);
                if (ret && !*ret)
                        return 1;
        }
#endif
        /* Ok how about VERSION? (Note this has spaces only because strptime is dumb) */
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
        ver_cwtv = CLAMP(ver_cwtv, 0x050, 0xfff);

#ifdef HG_VERSION
        if (strcmp(VERSION, "hg") == 0) {
                strcat(ver, ":");
                strcat(ver, HG_VERSION);
        }
#endif
        snprintf(top_banner_normal, sizeof(top_banner_normal) - 1,
                "Schism Tracker %s built %s %s",
                ver, __DATE__, __TIME__);
        top_banner_normal[sizeof(top_banner_normal) - 1] = '\0'; /* to be sure */
}

void ver_decode_cwtv(uint16_t cwtv, char *buf)
{
        struct tm version;
        time_t version_sec;

        cwtv &= 0xfff;
        if (cwtv > 0x050) {
                // Annoyingly, mktime uses local time instead of UTC. Why etc.
                version_sec = ((cwtv - 0x050) * 86400) + epoch_sec;
                if (localtime_r(&version_sec, &version)) {
                        sprintf(buf, "%04d-%02d-%02d",
                                version.tm_year + 1900, version.tm_mon + 1, version.tm_mday);
                        return;
                }
        }
        sprintf(buf, "0.%x", cwtv);
}

