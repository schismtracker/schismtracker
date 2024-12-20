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
#include "song.h"
#include "it.h" // need for kbd_get_alnum
#include "log.h"
#include "dmoz.h"
#include "util.h"

#include <di/di.h>
#include <fat.h>
#include <ogc/machine/processor.h>
#include <ogc/system.h>
#include <ogc/es.h>
#include <ogc/ios.h>
#include <errno.h>
#include <sys/dir.h>
#include <dirent.h>
#include "isfs.h"
#define CACHE_PAGES 8

// cargopasta'd from libogc git __di_check_ahbprot
static u32 _check_ahbprot(void) {
	s32 res;
	u64 title_id;
	u32 tmd_size;
	STACK_ALIGN(u32, tmdbuf, 1024, 32);

	res = ES_GetTitleID(&title_id);
	if (res < 0) {
		log_appendf(4, "ES_GetTitleID() failed: %d", res);
		return res;
	}

	res = ES_GetStoredTMDSize(title_id, &tmd_size);
	if (res < 0) {
		log_appendf(4, "ES_GetStoredTMDSize() failed: %d", res);
		return res;
	}

	if (tmd_size > 4096) {
		log_appendf(4, "TMD too big: %d", tmd_size);
		return -EINVAL;
	}

	res = ES_GetStoredTMD(title_id, tmdbuf, tmd_size);
	if (res < 0) {
		log_appendf(4, "ES_GetStoredTMD() failed: %d", res);
		return -EINVAL;
	}

	if ((tmdbuf[0x76] & 3) == 3) {
		return 1;
	}

	return 0;
}

void wii_sysinit(int *pargc, char ***pargv)
{
	char *ptr = NULL;

	log_appendf(1, "[Wii] This is IOS%d v%X, and AHBPROT is %s",
		IOS_GetVersion(), IOS_GetRevision(), _check_ahbprot() > 0 ? "enabled" : "disabled");
	if (*pargc == 0 && *pargv == NULL) {
		// I don't know if any other loaders provide similarly broken environments
		log_appendf(1, "[Wii] Was I just bannerbombed? Prepare for crash at exit...");
	} else if (memcmp((void *) 0x80001804, "STUBHAXX", 8) == 0) {
		log_appendf(1, "[Wii] Hello, HBC user!");
	} else {
		log_appendf(1, "[Wii] Where am I?!");
	}

	ISFS_SU();
	if (ISFS_Initialize() == IPC_OK)
		ISFS_Mount();
	fatInit(CACHE_PAGES, 0);

	// Attempt to locate a suitable home directory.
	if (!*pargc || !*pargv) {
		// loader didn't bother setting these
		*pargc = 1;
		*pargv = malloc(sizeof(char **));
		*pargv[0] = str_dup("?");
	} else if (strchr(*pargv[0], '/') != NULL) {
		// presumably launched from hbc menu - put stuff in the boot dir
		// (does get_parent_directory do what I want here?)
		ptr = dmoz_path_get_parent_directory(*pargv[0]);
	}
	if (!ptr) {
		// Make a guess anyway
		ptr = str_dup("sd:/apps/schismtracker");
	}
	if (chdir(ptr) != 0) {
		DIR* dir = opendir("sd:/");
		free(ptr);
		if (dir) {
			// Ok at least the sd card works, there's some other dysfunction
			closedir(dir);
			ptr = str_dup("sd:/");
		} else {
			// Safe (but useless) default
			ptr = str_dup("isfs:/");
		}
		chdir(ptr); // Hope that worked, otherwise we're hosed
	}
	setenv("HOME", ptr, 1);
	free(ptr);
}

void wii_sysexit(void)
{
	ISFS_Deinitialize();
}
