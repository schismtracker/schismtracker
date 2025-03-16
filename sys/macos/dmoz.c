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
#include "dmoz.h"
#include "log.h"

#include <Folders.h>
#include <Processes.h>

static char *macos_dmoz_get_exe_path(void)
{
	ProcessSerialNumber process;
	ProcessInfoRec process_info;
	FSSpec process_fsp;
	char *path, *parent;

	process.highLongOfPSN = 0;
	process.lowLongOfPSN = kCurrentProcess;
	process_info.processInfoLength = sizeof(process_info);
	process_info.processName = NULL;
	process_info.processAppSpec = &process_fsp;

	if (GetProcessInformation(&process, &process_info) != noErr)
		return NULL;

	if (!dmoz_path_from_fsspec(&process_fsp, &path))
		return NULL;

	parent = dmoz_path_get_parent_directory(path);

	free(path);

	return parent;
}

static int macos_dmoz_init(void)
{
	// do nothing
	return 1;
}

static void macos_dmoz_quit(void)
{
	// do nothing
}

const schism_dmoz_backend_t schism_dmoz_backend_macos = {
	.init = macos_dmoz_init,
	.quit = macos_dmoz_quit,

	.get_exe_path = macos_dmoz_get_exe_path,
};
