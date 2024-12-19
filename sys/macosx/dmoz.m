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

#include "backend/dmoz.h"
#include "loadso.h"
#include "charset.h"
#include "mem.h"
#include "util.h"

#import <Cocoa/Cocoa.h>
#import <Foundation/Foundation.h>

// FIXME we need to also get the Application Support directory through
// NSSearchPathForDirectoriesInDomains() 
static char *macosx_dmoz_get_exe_path(void)
{
	NSBundle *bundle = [NSBundle mainBundle];

	/* this returns the exedir for non-bundled and the resourceDir for bundled */
	const char *base = [[bundle resourcePath] fileSystemRepresentation];
	if (base)
		return str_dup(base);

	return NULL;
}

static int macosx_dmoz_init(void)
{
	// do nothing
	return 1;
}

static void macosx_dmoz_quit(void)
{
	// do nothing
}

const schism_dmoz_backend_t schism_dmoz_backend_macosx = {
	.init = macosx_dmoz_init,
	.quit = macosx_dmoz_quit,

	.get_exe_path = macosx_dmoz_get_exe_path,
};
