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
#include "charset.h"

#include <shlobj.h>
#include <windows.h>
#include <string.h>

void win32_filecreated_callback(const char *filename)
{
	/* let explorer know when we create a file. */
	charset_t explorer_charset;

	SCHISM_ANSI_UNICODE({
		explorer_charset = CHARSET_ANSI;
	}, {
		explorer_charset = CHARSET_WCHAR_T;
	})

	void *wc = charset_iconv_easy(filename, CHARSET_UTF8, explorer_charset);
	if (wc) {
		SHChangeNotify(SHCNE_CREATE, SHCNF_PATH|SHCNF_FLUSHNOWAIT, wc, NULL);
		SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH|SHCNF_FLUSHNOWAIT, wc, NULL);
		free(wc);
	}
}
