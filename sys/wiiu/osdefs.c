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
#include "log.h"

#include <whb/proc.h>
#include <sysapp/launch.h>
#include <sys/iosupport.h>
#include <coreinit/filesystem_fsa.h>

#include "devoptab.h"

#include <dirent.h>

static struct fsadevice {
	/* ---- READ-ONLY */
	const char *drive;
	const char *devpath;
	const int mountflag;

	/* ---- WRITE */
	int mounted;
} devices[] = {
	{"sd",   "/dev/sdcard01",      FSA_MOUNT_FLAG_LOCAL_MOUNT},
	{"usb0", "/vol/storage_usb01", FSA_MOUNT_FLAG_BIND_MOUNT},
	{"usb1", "/vol/storage_usb02", FSA_MOUNT_FLAG_BIND_MOUNT},
	//{"slc",  "/dev/slc01",         FSA_MOUNT_FLAG_LOCAL_MOUNT},
};

/* I think this will never be zero? */
static FSAClientHandle fs_handle = 0;

static int wiiu_mount_all_fs(void)
{
	int r;
	size_t i;

	if (!fs_handle) {
		/* initialize filesystem handle */
		fs_handle = FSAAddClient(NULL);
		if (!fs_handle)
			return -1;

#if 0
		{
			/* this is ancient, and only really relevant to users
			 * on Mocha, who we don't support anyway. */
			__attribute__((__aligned__(0x40))) char x[0x40];

			IOS_Ioctl(fs_handle, 0x28, x, 0x40, x, 0x40);
		}
#endif
	}

	r = 0;

	for (i = 0; i < ARRAY_SIZE(devices); i++) {
		int rc;
		char *s;

		if (devices[i].mounted)
			continue; // YARR

		if (asprintf(&s, "/vol/schism%02d", i) < 0)
			continue;

		if ((rc = wiiu_mount_fs(devices[i].drive, fs_handle, devices[i].devpath, s, devices[i].mountflag)) != 0) {
			free(s);
			log_appendf(1, "[Wii U] Failed to mount device %s with error %d", devices[i].drive, rc);
			r = -1;
			continue;
		}

		free(s);

		devices[i].mounted = 1;
	}

	return r;
}

static void wiiu_unmount_all_fs(void)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(devices); i++) {
		char *s;

		if (devices[i].mounted)
			continue; // YARR

		wiiu_unmount_fs(devices[i].drive);
	}

	FSADelClient(fs_handle);
}

/* fixup HOME envvar */
void wiiu_sysinit(int *pargc, char ***pargv)
{
	/* tell the wii u about us */
	WHBProcInit();

	/* initialize filesystem */
	FSAInit();
	wiiu_mount_all_fs();

	char *ptr = NULL;

	// Attempt to locate a suitable home directory.
	if (strchr(*pargv[0], '/') != NULL) {
		// presumably launched from hbc menu - put stuff in the boot dir
		// (does get_parent_directory do what I want here?)
		ptr = dmoz_path_get_parent_directory(*pargv[0]);
	}

	if (!ptr)
		ptr = str_dup("sd:/"); // Make a guess anyway

	if (chdir(ptr) != 0) {
		DIR* dir = opendir("sd:/");
		free(ptr);
		if (dir) {
			// Ok at least the sd card works, there's some other dysfunction
			closedir(dir);
			ptr = str_dup("sd:/");
		} else {
			// What?
			ptr = str_dup("fs:/vol/external01");
		}
		chdir(ptr); // Hope that worked, otherwise we're hosed
	}
	setenv("HOME", ptr, 1);
	free(ptr);
}

void wiiu_sysexit(void)
{
	wiiu_unmount_all_fs();

	/* hmph */
	WHBProcShutdown();

	/* if WHCProcShutdown didn't return us to the Homebrew Launcher
	 * or similar, return to the system menu */
	SYSLaunchMenu();
}
