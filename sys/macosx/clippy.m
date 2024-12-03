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

#import <Cocoa/Cocoa.h>

static char *macosx_clippy_get_clipboard(void);

static int macosx_clippy_have_selection(void)
{
	return 0;
}

static int macosx_clippy_have_clipboard(void)
{
	char *text = macosx_clippy_get_clipboard();
	int res = (text && text[0]);
	free(text);
	return res;
}

static void macosx_clippy_set_selection(const char *text)
{
	// nonexistent
}

static void macosx_clippy_set_clipboard(const char *text)
{
	NSString *contents = [NSString stringWithUTF8String: text];
	NSPasteboard *pb = [NSPasteboard generalPasteboard];
	[pb declareTypes:[NSArray arrayWithObject: NSStringPboardType] owner:nil];
	[pb setString:contents forType:NSStringPboardType];
}

static char *macosx_clippy_get_selection(void)
{
	// doesn't exist, ever
	return str_dup("");
}

static char *macosx_clippy_get_clipboard(void)
{
	NSPasteboard *pb = [NSPasteboard generalPasteboard];
	NSString *type = [pb availableTypeFromArray:[NSArray arrayWithObject: NSStringPboardType]];

	if (type != nil) {
		NSString *contents = [pb stringForType: type];
		if (contents != nil) {
			const char *po = [contents UTF8String];
			if (po)
				return str_dup(po);
		}
	}

	return str_dup("");
}

static int macosx_clippy_init(void)
{
	// nothing to do
	return 1;
}

static void macosx_clippy_quit(void)
{
	// nothing to do
}

const schism_clippy_backend_t schism_clippy_backend_macosx = {
	.init = macosx_clippy_init,
	.quit = macosx_clippy_quit,

	.have_selection = macosx_clippy_have_selection,
	.get_selection = macosx_clippy_get_selection,
	.set_selection = macosx_clippy_set_selection,

	.have_clipboard = macosx_clippy_have_clipboard,
	.get_clipboard = macosx_clippy_get_clipboard,
	.set_clipboard = macosx_clippy_set_clipboard,
};
