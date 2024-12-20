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
#include "dmoz.h"

#include <whb/proc.h>
#include <sysapp/launch.h>

#include <dirent.h>

/* fixup HOME envvar */
void wiiu_sysinit(int *pargc, char ***pargv)
{
	/* tell the wii u about us */
	WHBProcInit();

	char *ptr = NULL;

	// Attempt to locate a suitable home directory.
	if (strchr(*pargv[0], '/') != NULL) {
		// presumably launched from hbc menu - put stuff in the boot dir
		// (does get_parent_directory do what I want here?)
		ptr = dmoz_path_get_parent_directory(*pargv[0]);
	}

	if (!ptr)
		ptr = str_dup("fs:/vol/external01"); // Make a guess anyway

	if (chdir(ptr) != 0) {
		DIR* dir = opendir("fs:/vol/external01");
		free(ptr);
		if (dir) {
			// Ok at least the sd card works, there's some other dysfunction
			closedir(dir);
			ptr = str_dup("fs:/vol/external01");
		} else {
			// What?
			ptr = str_dup("fs:/");
		}
		chdir(ptr); // Hope that worked, otherwise we're hosed
	}
	setenv("HOME", ptr, 1);
	free(ptr);
}

void wiiu_sysexit(void)
{
	/* hmph */
	WHBProcShutdown();

	/* if WHCProcShutdown didn't return us to the Homebrew Launcher
	 * or similar, return to the system menu */
	SYSLaunchMenu();
}
