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

#include "backend/clippy.h"
#include "loadso.h"
#include "charset.h"
#include "mem.h"
#include "util.h"

#include <Scrap.h>
#include <Memory.h>

/* Since I've implemented clipboard support between instances,
 * we now officially implement the clipboard better than the
 * Finder does.  --paper */

/* selection stubs (these are stupid and should go away) */
static void macos_clippy_set_selection(const char *text)
{
	/* nonexistent */
}

static char *macos_clippy_get_selection(void)
{
	/* doesn't exist, ever */
	return str_dup("");
}

static int macos_clippy_have_selection(void)
{
	return 0;
}

static void macos_clippy_set_clipboard(const char *text)
{
	char *sysscript;
	size_t i;
	size_t len;

	/* UTF-8 -> system script */
	sysscript = charset_iconv_easy(text, CHARSET_UTF8, CHARSET_SYSTEMSCRIPT);

	len = strlen(sysscript);

	/* convert newlines to macintosh style */
	for (i = 0; i < len; i++)
		if (sysscript[i] == '\n')
			sysscript[i] = '\r';

	/* need to zero-out the scrap BEFORE putting new things in */
	ZeroScrap();
	PutScrap(len, 'TEXT', sysscript);

	free(sysscript);
}

static char *macos_clippy_get_clipboard(void)
{
	long offset = 0;
	long size;
	Handle hdata;
	char *data;

	data = NULL;
	hdata = NewHandle(0);
	size = GetScrap(hdata, 'TEXT', &offset);
	if (size > 0 && hdata) {
		SetHandleSize(hdata, size + 1); /* NUL terminator */
		if (MemError() == noErr) {
			long i;

			HLock(hdata);
			((unsigned char *)hdata)[size] = 0;
			/* convert system script to UTF-8 */
			data = charset_iconv_easy(*hdata, CHARSET_SYSTEMSCRIPT,
				CHARSET_UTF8);
			/* convert macintosh newlines to unix newlines */
			for (i = 0; i < size; i++)
				if (data[i] == '\r')
					data[i] = '\n';
			HUnlock(hdata);
		}
	}
	DisposeHandle(hdata);

	return str_dup(data ? data : "");
}

static int macos_clippy_have_clipboard(void)
{
	long offset = 0;
	long size;

	size = GetScrap(NULL, 'TEXT', &offset);

	return (size > 0);
}

static int macos_clippy_init(void)
{
	// nothing to do
	return 1;
}

static void macos_clippy_quit(void)
{
	// nothing to do
}

const schism_clippy_backend_t schism_clippy_backend_macos = {
	.init = macos_clippy_init,
	.quit = macos_clippy_quit,

	.have_selection = macos_clippy_have_selection,
	.get_selection = macos_clippy_get_selection,
	.set_selection = macos_clippy_set_selection,

	.have_clipboard = macos_clippy_have_clipboard,
	.get_clipboard = macos_clippy_get_clipboard,
	.set_clipboard = macos_clippy_set_clipboard,
};
