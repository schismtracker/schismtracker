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

#define INCL_DOS
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
	// Windows 9x
	int rc;
	char *sys;

	sys = charset_iconv_easy(path, CHARSET_UTF8, CHARSET_DOSCP);
	if (!sys)
		return -1;

	rc = stat(path, st);

	free(sys);

	return rc;
}
