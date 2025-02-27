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
#include "osdefs.h"
#include "mem.h"
#include "log.h"

#define INCL_DOS
#define INCL_PM
#define INCL_WINDIALOGS
#include <os2.h>

int os2_mkdir(const char *path, SCHISM_UNUSED mode_t mode)
{
	USHORT rc;
	char *sys;

	sys = charset_iconv_easy(path, CHARSET_UTF8, CHARSET_DOSCP);
	if (!sys)
		return -1;

	rc = DosMkDir(sys, 0UL);

	free(sys);

	return rc ? -1 : 0;
}

FILE *os2_fopen(const char *path, const char *rw)
{
	FILE *fp;
	char *sys;

	sys = charset_iconv_easy(path, CHARSET_UTF8, CHARSET_DOSCP);
	if (!sys)
		return NULL;

	fp = fopen(sys, rw);

	free(sys);

	return fp;
}

int os2_stat(const char* path, struct stat* st)
{
	int rc;
	char *sys;

	sys = charset_iconv_easy(path, CHARSET_UTF8, CHARSET_DOSCP);
	if (!sys)
		return -1;

	rc = stat(path, st);

	free(sys);

	return rc;
}

/* ------------------------------------------------------------------------ */

// helper function to retrieve profile data (must free result)
//
// For context: OS/2 INI files aren't actually only text, and can contain some
// stupid arbitrary data like structures etc.
// We don't use those, but prepare for them anyway...
static inline void *os2_get_profile_data(PSZ pszAppName, PSZ pszKeyWord, ULONG *psz)
{
	void *res;
	BOOL brc;

	brc = PrfQueryProfileSize(HINI_PROFILE, pszAppName, pszKeyWord, psz);
	if (!brc)
		return NULL;

	res = mem_alloc(*psz);

	brc = PrfQueryProfileData(HINI_PROFILE, pszAppName, pszKeyWord, res, psz);
	if (!brc)
		return NULL;

	return res;
}

static inline int convert_str_to_long(char *str, size_t num, LONG *result)
{
	size_t i;

	/* Fail on empty string */
	if (!str || !*str)
		return 0;

	*result = 0;

	for (i = 0; str[i] && i < num; i++) {
		if (str[i] < '0' || str[i] > '9')
			return 0;

		/* put the fries in the bag */
		*result *= 10;
		*result += str[i] - '0';
	}

	return 1;
}

static inline int os2_get_profile_integer(PSZ pszAppName, PSZ pszKeyWord, LONG *pint)
{
	ULONG sz;
	char *ptr;

	ptr = os2_get_profile_data(pszAppName, pszKeyWord, &sz);
	if (!ptr)
		return 0;

	int suckies = convert_str_to_long(ptr, sz, pint);

	free(ptr);

	return !!suckies;
}

int os2_get_key_repeat(int *pdelay, int *prate)
{
	// This is just an estimate; there's no official documentation on what these
	// values actually *are*, and changing them in the control panel doesn't seem to
	// actually do anything. can someone who knows what they're doing clue me in
	// onto what the translations are really supposed to be?

	if (pdelay) {
		LONG delay;

		if (!os2_get_profile_integer("PM_ControlPanel", "KeyRepeatDelay", &delay))
			return 0;

		// docs say range 0..890
		if (delay < 0 || delay > 890)
			return 0;

		// at value 89, we get a delay of ~500ms.
		*pdelay = (delay * 500L / 89L);
	}

	if (prate) {
		LONG rate;

		if (!os2_get_profile_integer("PM_ControlPanel", "KeyRepeatRate", &rate))
			return 0;

		// docs say range 1..20
		if (rate < 1 || rate > 20)
			return 0;

		// At a value of 18 we get an average of 50ms per repeat;
		// apparently 1 is the slowest while 20 is the fastest,
		// so I inverted the rate here. Hopefully this one is
		// actually linear instead of inverse (AHEM WINDOWS)
		*prate = (20L - rate) * 25L;
	}

	return 1;
}

void os2_show_message_box(const char *title, const char *text)
{
	char *sys_title, *sys_text;

	sys_title = charset_iconv_easy(title, CHARSET_UTF8, CHARSET_DOSCP);
	if (!sys_title)
		return;

	sys_text = charset_iconv_easy(text, CHARSET_UTF8, CHARSET_DOSCP);
	if (!sys_text) {
		free(sys_title);
		return;
	}

	// untested:
	WinMessageBox(HWND_DESKTOP, HWND_DESKTOP, sys_text, sys_title, 0L, MB_OK | MB_INFORMATION);

	free(sys_title);
	free(sys_text);
}