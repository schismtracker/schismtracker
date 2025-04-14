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
#include "charset.h"
#include "osdefs.h"
#include "log.h"

#include <windows.h>
#include <hal/video.h>
#include <hal/debug.h>
#include <nxdk/mount.h>

/* converts FILETIME to unix time_t */
static inline int64_t xbox_filetime_to_unix_time(FILETIME *ft) {
	uint64_t ul = ((uint64_t)ft->dwHighDateTime << 32) | ft->dwLowDateTime;
	return ((int64_t)(ul - 116444736000000000ULL) / 10000000);
}

void xbox_sysinit(SCHISM_UNUSED int *pargc, SCHISM_UNUSED char ***pargv)
{
	XBOX_REFURB_INFO refurb;

	XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);

	log_appendf(1, "[XBOX] This is kernel version %u.%u.%u",
		XboxKrnlVersion.Major, XboxKrnlVersion.Minor, XboxKrnlVersion.Build);
	/* There is also a Qfe member of the kernel version, which I'm guessing
	 * is for quick security fixes? */
	log_appendf(1, "[XBOX] GPU revision is %u, MCP revision is %u",
		XboxHardwareInfo.GpuRevision, XboxHardwareInfo.McpRevision);

	log_nl();

	if (XboxHardwareInfo.Flags & XBOX_HW_FLAG_INTERNAL_USB_HUB)
		log_appendf(1, "[XBOX] I have an internal USB hub!");
	if (XboxHardwareInfo.Flags & XBOX_HW_FLAG_DEVKIT_KERNEL)
		log_appendf(1, "[XBOX] I am a devkit!");
	if (XboxHardwareInfo.Flags & XBOX_480P_MACROVISION_ENABLED)
		log_appendf(1, "[XBOX] I have Macrovision support enabled!");
	if (XboxHardwareInfo.Flags & XBOX_HW_FLAG_ARCADE)
		log_appendf(1, "[XBOX] I am an arcade model!");

	log_nl();

	if (NT_SUCCESS(ExReadWriteRefurbInfo(&refurb, sizeof(refurb), FALSE))) {
		TIME_FIELDS st;

#if 0
		/* this seems to always be the ASCII text "RFRB" */
		log_appendf(1, "[XBOX] My refurbishment signature is %lx.", refurb.Signature);
#endif

		log_appendf(1, "[XBOX] I have been power cycled %lu times.", refurb.PowerCycleCount);

		if (refurb.FirstSetTime.QuadPart) {
			RtlTimeToTimeFields(&refurb.FirstSetTime, &st);
			log_appendf(1, "[XBOX] I was first booted on %d-%d-%d.", st.Year, st.Month, st.Day);
		} else {
			/* probably an emulator? XEMU always sets this value to 0. */
			log_appendf(1, "[XBOX] Support emulator developers!");
		}
	}

	log_nl();

	/* More interesting things:
	 *  * All of the keys are stored in memory, 16 bytes each
	 *  * There are quite a lot of settings that can be queried with
	 *    ExQueryNonVolatileSetting. None of these are particularly
	 *    interesting though :) */

#define MOUNT_PARTITION(letter, num) \
	do { \
		if (!nxMountDrive(#letter[0], "\\Device\\Harddisk0\\Partition" #num "\\")) \
			log_appendf(1, "[XBOX] Failed to mount " #letter " drive\n"); \
	} while (0)

	/* Partition 0 is the config area, which has a fixed
	 * size of 524288 (0x80000) and is not FATX. */

	MOUNT_PARTITION(C, 2); /* System */
	MOUNT_PARTITION(E, 1); /* Data -- this is likely the only useful one */
	MOUNT_PARTITION(X, 3); /* Game Cache */
	MOUNT_PARTITION(Y, 4); /* Game Cache */
	MOUNT_PARTITION(Z, 5); /* Game Cache */

#undef MOUNT_PARTITION
}

int xbox_stat(const char *path, struct stat *st)
{
	LPSTR wpath;

	wpath = charset_iconv_easy(path, CHARSET_UTF8, CHARSET_ANSI);
	if (!wpath)
		return -1;

	{
		DWORD dw;

		st->st_mode = 0;

		dw = GetFileAttributesA(wpath);
		if (dw == INVALID_FILE_ATTRIBUTES) {
			free(wpath);
			return -1;
		} else if (dw & FILE_ATTRIBUTE_DIRECTORY) {
			st->st_mode |= S_IFDIR;
		} else {
			st->st_mode |= S_IFREG;
		}
	}

	/* CreateFileA returns INVALID_HANDLE_VALUE if file is not actually a file */
	if (S_ISREG(st->st_mode)) {
		HANDLE fh;
		FILETIME ctime, atime, wtime;
		LARGE_INTEGER li;
		int fail = 0;

		/* we could possibly be more lenient with the access rights here */
		fh = CreateFileA(wpath, 0, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (fh == INVALID_HANDLE_VALUE) {
			free(wpath);
			return -1;
		}

		fail |= !GetFileSizeEx(fh, &li);
		fail |= !GetFileTime(fh, &ctime, &atime, &wtime);

		CloseHandle(fh);

		if (fail) {
			free(wpath);
			return -1;
		}

		/* now, copy everything into the stat structure. */
		st->st_mtime = xbox_filetime_to_unix_time(&wtime);
		st->st_atime = xbox_filetime_to_unix_time(&atime);
		st->st_ctime = xbox_filetime_to_unix_time(&ctime);

		st->st_size = li.QuadPart;
	}

	free(wpath);
	return 0;
}

int xbox_mkdir(const char *path, mode_t mode)
{
	LPSTR wpath;
	BOOL b;

	wpath = charset_iconv_easy(path, CHARSET_UTF8, CHARSET_ANSI);
	if (!wpath)
		return -1;

	b = CreateDirectoryA(wpath, NULL);

	free(wpath);

	return (b ? 0 : -1);
}

FILE *xbox_fopen(const char *path, const char *mode)
{
	LPSTR wpath;
	FILE *f;

	wpath = charset_iconv_easy(path, CHARSET_UTF8, CHARSET_ANSI);
	if (!wpath)
		return NULL;

	f = fopen(wpath, mode);

	free(wpath);

	return f;
}
