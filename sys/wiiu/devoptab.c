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

/*
	Copyright (c) 2015-present devkitPro, wut Authors

	This software is provided 'as-is', without any express or implied
	warranty. In no event will the authors be held liable for any damages
	arising from the use of this software.

	Permission is granted to anyone to use this software for any purpose,
	including commercial applications, and to alter it and redistribute it
	freely, subject to the following restrictions:

	1. The origin of this software must not be misrepresented; you must not
		claim that you wrote the original software. If you use this software
		in a product, an acknowledgment in the product documentation would be
		appreciated but is not required.
	2. Altered source versions must be plainly marked as such, and must not be
		misrepresented as being the original software.
	3. This notice may not be removed or altered from any source distribution.
*/

/*

Changes from libwut source:
  - ported all C++ to C
  - all devices are different devoptabs

--paper

*/

#include "headers.h"
#include <sys/iosupport.h>
#include <coreinit/mutex.h>
#include <coreinit/filesystem_fsa.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <malloc.h> /* memalign */
#include "log.h"

// The Wii U OSTime epoch is at 2000, so we must map it to 1970 for gettime
#define WIIU_OSTIME_EPOCH_YEAR         (2000)
// The Wii U FSTime epoch is at 1980, so we must map it to 1970 for gettime
#define WIIU_FSTIME_EPOCH_YEAR         (1980)

#define EPOCH_YEAR                     (1970)
#define EPOCH_YEARS_SINCE_LEAP         2
#define EPOCH_YEARS_SINCE_CENTURY      70
#define EPOCH_YEARS_SINCE_LEAP_CENTURY 370

#define EPOCH_DIFF_YEARS(year)         (year - EPOCH_YEAR)
#define EPOCH_DIFF_DAYS(year)                                        \
   ((EPOCH_DIFF_YEARS(year) * 365) +                                 \
    (EPOCH_DIFF_YEARS(year) - 1 + EPOCH_YEARS_SINCE_LEAP) / 4 -      \
    (EPOCH_DIFF_YEARS(year) - 1 + EPOCH_YEARS_SINCE_CENTURY) / 100 + \
    (EPOCH_DIFF_YEARS(year) - 1 + EPOCH_YEARS_SINCE_LEAP_CENTURY) / 400)
#define EPOCH_DIFF_SECS(year) (60ull * 60ull * 24ull * (uint64_t)EPOCH_DIFF_DAYS(year))

#define FSA_DIRITER_MAGIC 0x77696975

struct FSADOTDeviceData {
	devoptab_t device;
#if 0
	bool setup;
	bool mounted;
#endif
	bool isSDCard; /* should be renamed to "supportsStatVFS" */
	char mountPath[255]; /* probably enough */
	char cwd[FS_MAX_PATH + 1];
	FSAClientHandle clientHandle;
	uint64_t deviceSizeInSectors;
	uint32_t deviceSectorSize;
};

struct FSADOTFile {
	//! FSA file handle
	FSAFileHandle fd;

	//! Flags used in open(2)
	int flags;

	//! Current file offset
	uint32_t offset;

	//! Current file path
	char fullPath[FS_MAX_PATH + 1];

	//! Guard file access
	OSMutex mutex;

	//! Current file size (only valid if O_APPEND is set)
	uint32_t appendOffset;
};

struct FSADOTDir {
	//! Should be set to FSA_DIRITER_MAGIC
	uint32_t magic;

	//! FS directory handle
	FSADirectoryHandle fd;

	//! Temporary storage for reading entries
	FSADirectoryEntry entry_data;

	//! Current directory path
	char fullPath[FS_MAX_PATH + 1];

	//! Guard dir access
	OSMutex mutex;
};

#define COMP_MAX      50

#define ispathsep(ch) ((ch) == '/' || (ch) == '\\')
#define iseos(ch)     ((ch) == '\0')
#define ispathend(ch) (ispathsep(ch) || iseos(ch))

static inline FSMode
FSADOT_translate_permission_mode(mode_t mode)
{
   // Convert normal Unix octal permission bits into CafeOS hexadecimal permission bits
   return (FSMode)(((mode & S_IRWXU) << 2) | ((mode & S_IRWXG) << 1) | (mode & S_IRWXO));
}

static inline time_t
FSADOT_translate_time(FSTime timeValue)
{
   return (timeValue / 1000000) + EPOCH_DIFF_SECS(WIIU_FSTIME_EPOCH_YEAR);
}

// https://gist.github.com/starwing/2761647
static char *FSADOT_normpath(char *out, const char *in)
{
	char *pos[COMP_MAX], **top = pos, *head = out;
	int isabs = ispathsep(*in);

	if (isabs) *out++ = '/';
	*top++ = out;

	while (!iseos(*in)) {
		while (ispathsep(*in)) {
			++in;
		}

		if (iseos(*in)) {
			break;
		}

		if (memcmp(in, ".", 1) == 0 && ispathend(in[1])) {
			++in;
			continue;
		}

		if (memcmp(in, "..", 2) == 0 && ispathend(in[2])) {
			in += 2;
			if (top != pos + 1) {
				out = *--top;
			} else if (isabs) {
				out = top[-1];
			} else {
				strcpy(out, "../");
				out += 3;
			}
			continue;
		}

		if (top - pos >= COMP_MAX) {
			return NULL; // path to complicate
		}

		*top++ = out;
		while (!ispathend(*in)) {
			*out++ = *in++;
		}
		if (ispathsep(*in)) {
			*out++ = '/';
		}
	}

	*out = '\0';
	if (*head == '\0') {
		strcpy(head, "./");
	}
	return head;
}

static uint32_t FSADOT_hashstring(const char *str)
{
	uint32_t h;
	uint8_t *p;

	h = 0;
	for (p = (uint8_t *)str; *p != '\0'; p++) {
		h = 37 * h + *p;
	}
	return h;
}

static char *FSADOT_fixpath(struct _reent *r, const char *path)
{
	char *p, *fixedPath;
	struct FSADOTDeviceData *deviceData = (struct FSADOTDeviceData *)r->deviceData;

	if (!path) {
		r->_errno = EINVAL;
		return NULL;
	}

	p = strchr(path, ':');
	if (p) {
		p++;
	} else {
		p = (char *)path;
	}

	// wii u softlocks on empty strings so give expected error back
	if (strlen(p) == 0) {
		r->_errno = ENOENT;
		return NULL;
	}

	int maxPathLength = PATH_MAX;
	fixedPath         = memalign(0x40, maxPathLength);
	if (!fixedPath) {
		WUT_DEBUG_REPORT("FSADOT_fixpath: failed to allocate memory for fixedPath\n");
		r->_errno = ENOMEM;
		return NULL;
	}

	// Convert to an absolute path
	if (p[0] != '\0' && p[0] != '\\' && p[0] != '/') {
		if (snprintf(fixedPath, maxPathLength, "%s/%s", deviceData->cwd, p) >= maxPathLength) {
			WUT_DEBUG_REPORT("FSADOT_fixpath: fixedPath snprintf result (absolute) was truncated\n");
		}
	} else {
		if (snprintf(fixedPath, maxPathLength, "%s/%s", deviceData->mountPath, p) >= maxPathLength) {
			WUT_DEBUG_REPORT("FSADOT_fixpath: fixedPath snprintf result (relative) was truncated\n");
		}
	}

	// Normalize path (resolve any ".", "..", or "//")
	// FIXME a user can break this by putting a bunch of .. but currently
	// it doesn't really matter
	char *normalizedPath = strdup(fixedPath);
	if (!normalizedPath) {
		WUT_DEBUG_REPORT("FSADOT_fixpath: failed to allocate memory for normalizedPath\n");
		free(fixedPath);
		r->_errno = ENOMEM;
		return NULL;
	}

	char *resPath = FSADOT_normpath(normalizedPath, fixedPath);
	if (!resPath) {
		WUT_DEBUG_REPORT("FSADOT_fixpath: failed to normalize path\n");
		free(normalizedPath);
		free(fixedPath);
		r->_errno = EIO;
		return NULL;
	}

	if (snprintf(fixedPath, maxPathLength, "%s", resPath) >= maxPathLength) {
		WUT_DEBUG_REPORT("FSADOT_fixpath: fixedPath snprintf result (relative) was truncated\n");
	}

	free(normalizedPath);

	size_t pathLength = strlen(fixedPath);
	if (pathLength > FS_MAX_PATH) {
		free(fixedPath);
		r->_errno = ENAMETOOLONG;
		return NULL;
	}

	return fixedPath;
}

static mode_t FSADOT_translate_stat_mode(FSStat *fsStat)
{
	mode_t retMode = 0;

	if ((fsStat->flags & FS_STAT_LINK) == FS_STAT_LINK) {
		retMode |= S_IFLNK;
	} else if ((fsStat->flags & FS_STAT_DIRECTORY) == FS_STAT_DIRECTORY) {
		retMode |= S_IFDIR;
	} else if ((fsStat->flags & FS_STAT_FILE) == FS_STAT_FILE) {
		retMode |= S_IFREG;
	} else if (fsStat->size == 0) {
		// Mounted paths like /vol/external01 have no flags set.
		// If no flag is set and the size is 0, it's a (root) dir
		retMode |= S_IFDIR;
	} else if (fsStat->size > 0) {
		// Some regular Wii U files have no type info but will have a size
		retMode |= S_IFREG;
	}

	// Convert normal CafeOS hexadecimal permission bits into Unix octal permission bits
	mode_t permissionMode = (((fsStat->mode >> 2) & S_IRWXU) | ((fsStat->mode >> 1) & S_IRWXG) | (fsStat->mode & S_IRWXO));

	return retMode | permissionMode;
}

static void FSADOT_translate_stat(FSAClientHandle clientHandle, FSStat *fsStat, ino_t ino, struct stat *posStat)
{
	memset(posStat, 0, sizeof(struct stat));
	posStat->st_dev     = (dev_t)clientHandle;
	posStat->st_ino     = ino;
	posStat->st_mode    = FSADOT_translate_stat_mode(fsStat);
	posStat->st_nlink   = 1;
	posStat->st_uid     = fsStat->owner;
	posStat->st_gid     = fsStat->group;
	posStat->st_rdev    = posStat->st_dev;
	posStat->st_size    = fsStat->size;
	posStat->st_atime   = FSADOT_translate_time(fsStat->modified);
	posStat->st_ctime   = FSADOT_translate_time(fsStat->created);
	posStat->st_mtime   = FSADOT_translate_time(fsStat->modified);
	posStat->st_blksize = 512;
	posStat->st_blocks  = (posStat->st_size + posStat->st_blksize - 1) / posStat->st_size;
}

static int FSADOT_translate_error(FSError error)
{
	switch (error) {
		case FS_ERROR_END_OF_DIR:
		case FS_ERROR_END_OF_FILE:
			return ENOENT;
		case FS_ERROR_ALREADY_EXISTS:
			return EEXIST;
		case FS_ERROR_MEDIA_ERROR:
			return EIO;
		case FS_ERROR_NOT_FOUND:
			return ENOENT;
		case FS_ERROR_PERMISSION_ERROR:
			return EPERM;
		case FS_ERROR_STORAGE_FULL:
			return ENOSPC;
		case FS_ERROR_BUSY:
			return EBUSY;
		case FS_ERROR_CANCELLED:
			return ECANCELED;
		case FS_ERROR_FILE_TOO_BIG:
			return EFBIG;
		case FS_ERROR_INVALID_PATH:
			return ENAMETOOLONG;
		case FS_ERROR_NOT_DIR:
			return ENOTDIR;
		case FS_ERROR_NOT_FILE:
			return EISDIR;
		case FS_ERROR_OUT_OF_RANGE:
			return EINVAL;
		case FS_ERROR_UNSUPPORTED_COMMAND:
			return ENOTSUP;
		case FS_ERROR_WRITE_PROTECTED:
			return EROFS;
		case FS_ERROR_NOT_INIT:
			return ENODEV;
		case FS_ERROR_MAX_MOUNT_POINTS:
		case FS_ERROR_MAX_VOLUMES:
		case FS_ERROR_MAX_CLIENTS:
		case FS_ERROR_MAX_FILES:
		case FS_ERROR_MAX_DIRS:
			return EMFILE;
		case FS_ERROR_ALREADY_OPEN:
			return EBUSY;
		case FS_ERROR_NOT_EMPTY:
			return ENOTEMPTY;
		case FS_ERROR_ACCESS_ERROR:
			return EACCES;
		case FS_ERROR_DATA_CORRUPTED:
			return EILSEQ;
		case FS_ERROR_JOURNAL_FULL:
			return EBUSY;
		case FS_ERROR_UNAVAILABLE_COMMAND:
			return EBUSY;
		case FS_ERROR_INVALID_PARAM:
			return EBUSY;
		case FS_ERROR_INVALID_BUFFER:
		case FS_ERROR_INVALID_ALIGNMENT:
		case FS_ERROR_INVALID_CLIENTHANDLE:
		case FS_ERROR_INVALID_FILEHANDLE:
		case FS_ERROR_INVALID_DIRHANDLE:
			return EINVAL;
		case FS_ERROR_OUT_OF_RESOURCES:
			return ENOMEM;
		case FS_ERROR_MEDIA_NOT_READY:
			return EIO;
		case FS_ERROR_INVALID_MEDIA:
			return EIO;
		default:
			return EIO;
	}
}

static int FSADOT_chdir(struct _reent *r, const char *path)
{
	FSError status;
	struct FSADOTDeviceData *deviceData;

	if (!path) {
		r->_errno = EINVAL;
		return -1;
	}

	char *fixedPath = FSADOT_fixpath(r, path);
	if (!fixedPath) {
		r->_errno = ENOMEM;
		return -1;
	}
	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	status     = FSAChangeDir(deviceData->clientHandle, fixedPath);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSAChangeDir(0x%08X, %s) failed: %s\n", deviceData->clientHandle, fixedPath, FSAGetStatusStr(status));
		free(fixedPath);
		r->_errno = FSADOT_translate_error(status);
		return -1;
	}

	// Remove trailing '/'
	if (fixedPath[0] != '\0') {
		size_t last = strlen(fixedPath) - 1;

		if (fixedPath[last] == '/')
			fixedPath[last] = 0;
	}

	if (snprintf(deviceData->cwd, sizeof(deviceData->cwd), "%s", fixedPath) >= (int)sizeof(deviceData->cwd)) {
		WUT_DEBUG_REPORT("FSADOT_chdir: snprintf result was truncated\n");
	}

	free(fixedPath);

	return 0;
}

static int FSADOT_chmod(struct _reent *r, const char *path, mode_t mode)
{
	FSError status;
	struct FSADOTDeviceData *deviceData;

	if (!path) {
		r->_errno = EINVAL;
		return -1;
	}

	char *fixedPath = FSADOT_fixpath(r, path);
	if (!fixedPath) {
		r->_errno = ENOMEM;
		return -1;
	}

	FSMode translatedMode = FSADOT_translate_permission_mode(mode);

	deviceData            = (struct FSADOTDeviceData *)r->deviceData;

	status                = FSAChangeMode(deviceData->clientHandle, fixedPath, translatedMode);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSAChangeMode(0x%08X, %s, 0x%X) failed: %s\n",
							  deviceData->clientHandle, fixedPath, translatedMode, FSAGetStatusStr(status));
		free(fixedPath);
		r->_errno = FSADOT_translate_error(status);
		return -1;
	}
	free(fixedPath);

	return 0;
}

static int FSADOT_close(struct _reent *r, void *fd)
{
	FSError status;
	struct FSADOTFile *file;
	struct FSADOTDeviceData *deviceData;

	if (!fd) {
		r->_errno = EINVAL;
		return -1;
	}

	file       = (struct FSADOTFile *)fd;

	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	OSLockMutex(&file->mutex);

	status = FSACloseFile(deviceData->clientHandle, file->fd);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSACloseFile(0x%08X, 0x%08X) (%s) failed: %s\n",
							  deviceData->clientHandle, file->fd, file->fullPath, FSAGetStatusStr(status));
		r->_errno = FSADOT_translate_error(status);
		OSUnlockMutex(&file->mutex);
		return -1;
	}

	OSUnlockMutex(&file->mutex);

	return 0;
}

int FSADOT_dirclose(struct _reent *r, DIR_ITER *dirState)
{
	FSError status;
	struct FSADOTDir *dir;
	struct FSADOTDeviceData *deviceData;

	if (!dirState) {
		r->_errno = EINVAL;
		return -1;
	}

	dir        = (struct FSADOTDir *)(dirState->dirStruct);

	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	OSLockMutex(&dir->mutex);

	status = FSACloseDir(deviceData->clientHandle, dir->fd);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSACloseDir(0x%08X, 0x%08X) (%s) failed: %s\n",
							  deviceData->clientHandle, dir->fd, dir->fullPath, FSAGetStatusStr(status));
		r->_errno = FSADOT_translate_error(status);
		OSUnlockMutex(&dir->mutex);
		return -1;
	}

	OSUnlockMutex(&dir->mutex);
	return 0;
}

static int FSADOT_dirnext(struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *filestat)
{
	FSError status;
	struct FSADOTDir *dir;
	struct FSADOTDeviceData *deviceData;

	if (!dirState || !filename || !filestat) {
		r->_errno = EINVAL;
		return -1;
	}

	deviceData = (struct FSADOTDeviceData *)r->deviceData;
	dir        = (struct FSADOTDir *)(dirState->dirStruct);

	OSLockMutex(&dir->mutex);
	memset(&dir->entry_data, 0, sizeof(dir->entry_data));

	status = FSAReadDir(deviceData->clientHandle, dir->fd, &dir->entry_data);
	if (status < 0) {
		if (status != FS_ERROR_END_OF_DIR) {
			WUT_DEBUG_REPORT("FSAReadDir(0x%08X, 0x%08X, %p) (%s) failed: %s\n",
								  deviceData->clientHandle, dir->fd, &dir->entry_data, dir->fullPath, FSAGetStatusStr(status));
		}
		r->_errno = FSADOT_translate_error(status);
		OSUnlockMutex(&dir->mutex);
		return -1;
	}

	ino_t ino;
	size_t fullLen = strlen(dir->fullPath) + 1 + strlen(dir->entry_data.name) + 1;
	char *fullStr  = memalign(0x40, fullLen);
	if (fullStr) {
		if (snprintf(fullStr, fullLen, "%s/%s", dir->fullPath, dir->entry_data.name) >= (int)fullLen) {
			WUT_DEBUG_REPORT("FSADOT_dirnext: snprintf fullStr result was truncated\n");
		}
		ino = FSADOT_hashstring(fullStr);
		free(fullStr);
	} else {
		ino = 0;
		WUT_DEBUG_REPORT("FSADOT_dirnext: Failed to allocate memory for fullStr. st_ino will be set to 0\n");
	}
	FSADOT_translate_stat(deviceData->clientHandle, &dir->entry_data.info, ino, filestat);

	if (snprintf(filename, NAME_MAX, "%s", dir->entry_data.name) >= NAME_MAX) {
		WUT_DEBUG_REPORT("FSADOT_dirnext: snprintf filename result was truncated\n");
	}

	OSUnlockMutex(&dir->mutex);

	return 0;
}

static DIR_ITER *FSADOT_diropen(struct _reent *r, DIR_ITER *dirState, const char *path)
{
	FSADirectoryHandle fd;
	FSError status;
	struct FSADOTDir *dir;
	struct FSADOTDeviceData *deviceData;

	if (!dirState || !path) {
		r->_errno = EINVAL;
		return NULL;
	}

	char *fixedPath = FSADOT_fixpath(r, path);
	if (!fixedPath) {
		return NULL;
	}
	dir        = (struct FSADOTDir *)(dirState->dirStruct);
	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	// Remove trailing '/'
	if (fixedPath[0] != '\0') {
		if (fixedPath[strlen(fixedPath) - 1] == '/') {
			fixedPath[strlen(fixedPath) - 1] = 0;
		}
	}

	if (snprintf(dir->fullPath, sizeof(dir->fullPath), "%s", fixedPath) >= (int)sizeof(dir->fullPath)) {
		WUT_DEBUG_REPORT("FSADOT_diropen: snprintf result was truncated\n");
	}

	free(fixedPath);

	OSInitMutexEx(&dir->mutex, dir->fullPath);
	OSLockMutex(&dir->mutex);

	status = FSAOpenDir(deviceData->clientHandle, dir->fullPath, &fd);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSAOpenDir(0x%08X, %s, %p) failed: %s\n",
							  deviceData->clientHandle, dir->fullPath, &fd, FSAGetStatusStr(status));
		r->_errno = FSADOT_translate_error(status);
		OSUnlockMutex(&dir->mutex);
		return NULL;
	}

	dir->magic = FSA_DIRITER_MAGIC;
	dir->fd    = fd;
	memset(&dir->entry_data, 0, sizeof(dir->entry_data));
	OSUnlockMutex(&dir->mutex);
	return dirState;
}

static int FSADOT_dirreset(struct _reent *r, DIR_ITER *dirState)
{
	FSError status;
	struct FSADOTDir *dir;
	struct FSADOTDeviceData *deviceData;

	if (!dirState) {
		r->_errno = EINVAL;
		return -1;
	}

	dir        = (struct FSADOTDir *)(dirState->dirStruct);
	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	OSLockMutex(&dir->mutex);

	status = FSARewindDir(deviceData->clientHandle, dir->fd);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSARewindDir(0x%08X, 0x%08X) (%s) failed: %s\n",
							  deviceData->clientHandle, dir->fd, dir->fullPath, FSAGetStatusStr(status));
		r->_errno = FSADOT_translate_error(status);
		OSUnlockMutex(&dir->mutex);
		return -1;
	}

	OSUnlockMutex(&dir->mutex);

	return 0;
}

static int FSADOT_fstat(struct _reent *r, void *fd, struct stat *st)
{
	FSError status;
	FSAStat fsStat;
	struct FSADOTFile *file;
	struct FSADOTDeviceData *deviceData;

	if (!fd || !st) {
		r->_errno = EINVAL;
		return -1;
	}

	file       = (struct FSADOTFile *)fd;
	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	OSLockMutex(&file->mutex);

	status = FSAGetStatFile(deviceData->clientHandle, file->fd, &fsStat);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSAGetStatFile(0x%08X, 0x%08X, %p) (%s) failed: %s\n",
							  deviceData->clientHandle, file->fd, &fsStat,
							  file->fullPath, FSAGetStatusStr(status));
		r->_errno = FSADOT_translate_error(status);
		OSUnlockMutex(&file->mutex);
		return -1;
	}

	ino_t ino = FSADOT_hashstring(file->fullPath);
	FSADOT_translate_stat(deviceData->clientHandle, &fsStat, ino, st);

	OSUnlockMutex(&file->mutex);

	return 0;
}

static int FSADOT_fsync(struct _reent *r, void *fd)
{
	FSError status;
	struct FSADOTFile *file;
	struct FSADOTDeviceData *deviceData;

	if (!fd) {
		r->_errno = EINVAL;
		return -1;
	}

	file       = (struct FSADOTFile *)fd;

	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	OSLockMutex(&file->mutex);

	status = FSAFlushFile(deviceData->clientHandle, file->fd);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSAFlushFile(0x%08X, 0x%08X) (%s) failed: %s\n",
							  deviceData->clientHandle, file->fd, file->fullPath, FSAGetStatusStr(status));
		r->_errno = FSADOT_translate_error(status);
		OSUnlockMutex(&file->mutex);
		return -1;
	}

	OSUnlockMutex(&file->mutex);

	return 0;
}

static int FSADOT_mkdir(struct _reent *r, const char *path, int mode)
{
	FSError status;
	char *fixedPath;
	struct FSADOTDeviceData *deviceData;

	if (!path) {
		r->_errno = EINVAL;
		return -1;
	}

	fixedPath = FSADOT_fixpath(r, path);
	if (!fixedPath) {
		r->_errno = ENOMEM;
		return -1;
	}

	deviceData            = (struct FSADOTDeviceData *)r->deviceData;

	FSMode translatedMode = FSADOT_translate_permission_mode(mode);

	status                = FSAMakeDir(deviceData->clientHandle, fixedPath, translatedMode);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSAMakeDir(0x%08X, %s, 0x%X) failed: %s\n",
							  deviceData->clientHandle, fixedPath, translatedMode, FSAGetStatusStr(status));
		free(fixedPath);
		r->_errno = FSADOT_translate_error(status);
		return -1;
	}
	free(fixedPath);

	return 0;
}

// Extended "magic" value that allows opening files with FS_OPEN_FLAG_UNENCRYPTED in underlying FSOpenFileEx() call similar to O_DIRECTORY
#ifndef O_UNENCRYPTED
#define O_UNENCRYPTED 0x4000000
#endif

static int FSADOT_open(struct _reent *r, void *fileStruct, const char *path, int flags, int mode)
{
	FSAFileHandle fd;
	FSError status;
	const char *fsMode;
	struct FSADOTFile *file;
	struct FSADOTDeviceData *deviceData;

	if (!fileStruct || !path) {
		r->_errno = EINVAL;
		return -1;
	}

	bool createFileIfNotFound = false;
	bool failIfFileNotFound   = false;
	// Map flags to open modes
	int commonFlagMask        = O_CREAT | O_TRUNC | O_APPEND;
	if (((flags & O_ACCMODE) == O_RDONLY) && !(flags & commonFlagMask)) {
		fsMode = "r";
	} else if (((flags & O_ACCMODE) == O_RDWR) && !(flags & commonFlagMask)) {
		fsMode = "r+";
	} else if (((flags & O_ACCMODE) == O_WRONLY) && ((flags & commonFlagMask) == (O_CREAT | O_TRUNC))) {
		fsMode = "w";
	} else if (((flags & O_ACCMODE) == O_RDWR) && ((flags & commonFlagMask) == (O_CREAT | O_TRUNC))) {
		fsMode = "w+";
	} else if (((flags & O_ACCMODE) == O_WRONLY) && ((flags & commonFlagMask) == O_CREAT) && (flags & O_EXCL) == O_EXCL) {
		// if O_EXCL is set, we don't need O_TRUNC
		fsMode = "w";
	} else if (((flags & O_ACCMODE) == O_RDWR) && ((flags & commonFlagMask) == O_CREAT) && (flags & O_EXCL) == O_EXCL) {
		// if O_EXCL is set, we don't need O_TRUNC
		fsMode = "w+";
	} else if (((flags & O_ACCMODE) == O_WRONLY) && ((flags & commonFlagMask) == (O_CREAT | O_APPEND))) {
		fsMode = "a";
	} else if (((flags & O_ACCMODE) == O_RDWR) && ((flags & commonFlagMask) == (O_CREAT | O_APPEND))) {
		fsMode = "a+";
	} else if (((flags & O_ACCMODE) == O_WRONLY) && ((flags & commonFlagMask) == (O_CREAT))) {
		// Cafe OS doesn't have a matching mode for this, so we have to be creative and create the file.
		createFileIfNotFound = true;
		// It's not possible to open a file with write only mode which doesn't truncate the file
		// Technically we could read from the file, but our read implementation is blocking this.
		fsMode               = "r+";
	} else if (((flags & O_ACCMODE) == O_RDWR) && ((flags & commonFlagMask) == (O_CREAT))) {
		// Cafe OS doesn't have a matching mode for this, so we have to be creative and create the file.
		createFileIfNotFound = true;
		fsMode               = "r+";
	} else if (((flags & O_ACCMODE) == O_WRONLY) && ((flags & commonFlagMask) == (O_APPEND))) {
		// Cafe OS doesn't have a matching mode for this, so we have to check if the file exists.
		failIfFileNotFound = true;
		fsMode             = "a";
	} else if (((flags & O_ACCMODE) == O_WRONLY) && ((flags & commonFlagMask) == (O_TRUNC))) {
		// As above
		failIfFileNotFound = true;
		fsMode             = "w";
	} else {
		r->_errno = EINVAL;
		return -1;
	}

	char *fixedPath = FSADOT_fixpath(r, path);
	if (!fixedPath) {
		r->_errno = ENOMEM;
		return -1;
	}


	file       = (struct FSADOTFile *)fileStruct;
	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	if (snprintf(file->fullPath, sizeof(file->fullPath), "%s", fixedPath) >= (int)sizeof(file->fullPath)) {
		WUT_DEBUG_REPORT("FSADOT_open: snprintf result was truncated\n");
	}
	free(fixedPath);

	// Prepare flags
	FSOpenFileFlags openFlags = (flags & O_UNENCRYPTED) ? FS_OPEN_FLAG_UNENCRYPTED : FS_OPEN_FLAG_NONE;
	FSMode translatedMode     = FSADOT_translate_permission_mode(mode);
	uint32_t preAllocSize     = 0;

	// Init mutex and lock
	OSInitMutexEx(&file->mutex, file->fullPath);
	OSLockMutex(&file->mutex);

	if (createFileIfNotFound || failIfFileNotFound || (flags & (O_EXCL | O_CREAT)) == (O_EXCL | O_CREAT)) {
		// Check if file exists
		FSAStat stat;
		status = FSAGetStat(deviceData->clientHandle, file->fullPath, &stat);
		if (status == FS_ERROR_NOT_FOUND) {
			if (createFileIfNotFound) { // Create new file if needed
				status = FSAOpenFileEx(deviceData->clientHandle, file->fullPath, "w", translatedMode,
											  openFlags, preAllocSize, &fd);
				if (status == FS_ERROR_OK) {
					if (FSACloseFile(deviceData->clientHandle, fd) != FS_ERROR_OK) {
						WUT_DEBUG_REPORT("FSACloseFile(0x%08X, 0x%08X) (%s) failed: %s\n",
											  deviceData->clientHandle, fd, file->fullPath, FSAGetStatusStr(status));
					}
					fd = -1;
				} else {
					WUT_DEBUG_REPORT("FSAOpenFileEx(0x%08X, %s, %s, 0x%X, 0x%08X, 0x%08X, %p) failed: %s\n",
										  deviceData->clientHandle, file->fullPath, "w", translatedMode, openFlags, preAllocSize, &fd,
										  FSAGetStatusStr(status));
					r->_errno = FSADOT_translate_error(status);
					OSUnlockMutex(&file->mutex);
					return -1;
				}
			} else if (failIfFileNotFound) { // Return an error if we don't we create new files
				r->_errno = FSADOT_translate_error(status);
				OSUnlockMutex(&file->mutex);
				return -1;
			}
		} else if (status == FS_ERROR_OK) {
			// If O_CREAT and O_EXCL are set, open() shall fail if the file exists.
			if ((flags & (O_EXCL | O_CREAT)) == (O_EXCL | O_CREAT)) {
				r->_errno = EEXIST;
				OSUnlockMutex(&file->mutex);
				return -1;
			}
		}
	}

	status = FSAOpenFileEx(deviceData->clientHandle, file->fullPath, fsMode, translatedMode, openFlags, preAllocSize, &fd);
	if (status < 0) {
		if (status != FS_ERROR_NOT_FOUND) {
			WUT_DEBUG_REPORT("FSAOpenFileEx(0x%08X, %s, %s, 0x%X, 0x%08X, 0x%08X, %p) failed: %s\n",
								  deviceData->clientHandle, file->fullPath, fsMode, translatedMode, openFlags, preAllocSize, &fd,
								  FSAGetStatusStr(status));
		}
		r->_errno = FSADOT_translate_error(status);
		OSUnlockMutex(&file->mutex);
		return -1;
	}

	file->fd     = fd;
	file->flags  = (flags & (O_ACCMODE | O_APPEND | O_SYNC));
	// Is always 0, even if O_APPEND is set.
	file->offset = 0;

	if (flags & O_APPEND) {
		FSAStat stat;
		status = FSAGetStatFile(deviceData->clientHandle, fd, &stat);
		if (status < 0) {
			WUT_DEBUG_REPORT("FSAGetStatFile(0x%08X, 0x%08X, %p) (%s) failed: %s\n",
								  deviceData->clientHandle, fd, &stat, file->fullPath, FSAGetStatusStr(status));

			r->_errno = FSADOT_translate_error(status);
			if (FSACloseFile(deviceData->clientHandle, fd) < 0) {
				WUT_DEBUG_REPORT("FSACloseFile(0x%08X, 0x%08X) (%s) failed: %s\n",
									  deviceData->clientHandle, fd, file->fullPath, FSAGetStatusStr(status));
			}
			OSUnlockMutex(&file->mutex);
			return -1;
		}
		file->appendOffset = stat.size;
	}
	OSUnlockMutex(&file->mutex);
	return 0;
}

static ssize_t FSADOT_read(struct _reent *r, void *fd, char *ptr, size_t len)
{
	FSError status;
	struct FSADOTFile *file;
	struct FSADOTDeviceData *deviceData;
	if (!fd || !ptr) {
		r->_errno = EINVAL;
		return -1;
	}

	// Check that the file was opened with read access
	file = (struct FSADOTFile *)fd;
	if ((file->flags & O_ACCMODE) == O_WRONLY) {
		r->_errno = EBADF;
		return -1;
	}

	// cache-aligned, cache-line-sized
	__attribute__((aligned(0x40))) uint8_t alignedBuffer[0x40];

	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	OSLockMutex(&file->mutex);

	size_t bytesRead = 0;
	while (bytesRead < len) {
		// only use input buffer if cache-aligned and read size is a multiple of cache line size
		// otherwise read into alignedBuffer
		uint8_t *tmp = (uint8_t *)ptr;
		size_t size  = len - bytesRead;

		if (size < 0x40) {
			// read partial cache-line back-end
			tmp = alignedBuffer;
		} else if ((uintptr_t)ptr & 0x3F) {
			// read partial cache-line front-end
			tmp  = alignedBuffer;
			size = MIN(size, 0x40 - ((uintptr_t)ptr & 0x3F));
		} else {
			// read whole cache lines
			size &= ~0x3F;
		}

		// Limit each request to 1 MiB
		if (size > 0x100000) {
			size = 0x100000;
		}

		status = FSAReadFile(deviceData->clientHandle, tmp, 1, size, file->fd, 0);

		if (status < 0) {
			WUT_DEBUG_REPORT("FSAReadFile(0x%08X, %p, 1, 0x%08X, 0x%08X, 0) (%s) failed: %s\n",
								  deviceData->clientHandle, tmp, size, file->fd, file->fullPath, FSAGetStatusStr(status));

			if (bytesRead != 0) {
				OSUnlockMutex(&file->mutex);
				return bytesRead; // error after partial read
			}

			r->_errno = FSADOT_translate_error(status);
			OSUnlockMutex(&file->mutex);
			return -1;
		}

		if (tmp == alignedBuffer) {
			memcpy(ptr, alignedBuffer, status);
		}

		file->offset += status;
		bytesRead += status;
		ptr += status;

		if ((size_t)status != size) {
			OSUnlockMutex(&file->mutex);
			return bytesRead; // partial read
		}
	}

	OSUnlockMutex(&file->mutex);
	return bytesRead;
}

static int FSADOT_rename(struct _reent *r, const char *oldName, const char *newName)
{
	FSError status;
	char *fixedOldPath, *fixedNewPath;
	struct FSADOTDeviceData *deviceData;

	if (!oldName || !newName) {
		r->_errno = EINVAL;
		return -1;
	}

	fixedOldPath = FSADOT_fixpath(r, oldName);
	if (!fixedOldPath) {
		r->_errno = ENOMEM;
		return -1;
	}

	fixedNewPath = FSADOT_fixpath(r, newName);
	if (!fixedNewPath) {
		free(fixedOldPath);
		r->_errno = ENOMEM;
		return -1;
	}

	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	status     = FSARename(deviceData->clientHandle, fixedOldPath, fixedNewPath);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSARename(0x%08X, %s, %s) failed: %s\n",
							  deviceData->clientHandle, fixedOldPath, fixedNewPath, FSAGetStatusStr(status));
		free(fixedOldPath);
		free(fixedNewPath);
		r->_errno = FSADOT_translate_error(status);
		return -1;
	}
	free(fixedOldPath);
	free(fixedNewPath);

	return 0;
}

static int FSADOT_rmdir(struct _reent *r, const char *name)
{
	FSError status;
	struct FSADOTDeviceData *deviceData;

	if (!name) {
		r->_errno = EINVAL;
		return -1;
	}

	char *fixedPath = FSADOT_fixpath(r, name);
	if (!fixedPath) {
		r->_errno = ENOMEM;
		return -1;
	}

	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	status     = FSARemove(deviceData->clientHandle, fixedPath);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSARemove(0x%08X, %s) failed: %s\n",
							  deviceData->clientHandle, fixedPath, FSAGetStatusStr(status));
		free(fixedPath);
		r->_errno = FSADOT_translate_error(status);
		return -1;
	}

	free(fixedPath);

	return 0;
}

static off_t FSADOT_seek(struct _reent *r, void *fd, off_t pos, int whence)
{
	FSError status;
	FSAStat fsStat;
	uint64_t offset;
	struct FSADOTFile *file;
	struct FSADOTDeviceData *deviceData;

	if (!fd) {
		r->_errno = EINVAL;
		return -1;
	}

	file       = (struct FSADOTFile *)fd;

	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	OSLockMutex(&file->mutex);

	// Find the offset to see from
	switch (whence) {
		case SEEK_SET: { // Set absolute position; start offset is 0
			offset = 0;
			break;
		}
		case SEEK_CUR: { // Set position relative to the current position
			offset = file->offset;
			break;
		}
		case SEEK_END: { // Set position relative to the end of the file
			status = FSAGetStatFile(deviceData->clientHandle, file->fd, &fsStat);
			if (status < 0) {
				WUT_DEBUG_REPORT("FSAGetStatFile(0x%08X, 0x%08X, %p) (%s) failed: %s\n",
									  deviceData->clientHandle, file->fd, &fsStat, file->fullPath, FSAGetStatusStr(status));
				r->_errno = FSADOT_translate_error(status);
				OSUnlockMutex(&file->mutex);
				return -1;
			}
			offset = fsStat.size;
			break;
		}
		default: { // An invalid option was provided
			r->_errno = EINVAL;
			OSUnlockMutex(&file->mutex);
			return -1;
		}
	}

	if (pos < 0 && (off_t)offset < -pos) {
		// Don't allow seek to before the beginning of the file
		r->_errno = EINVAL;
		OSUnlockMutex(&file->mutex);
		return -1;
	} else if (offset + pos > UINT32_MAX) {
		// Check for overflow
		r->_errno = EINVAL;
		OSUnlockMutex(&file->mutex);
		return -1;
	}

	if ((uint32_t)(offset + pos) == file->offset) {
		OSUnlockMutex(&file->mutex);
		return file->offset;
	}

	uint32_t old_pos = file->offset;
	file->offset     = offset + pos;

	status           = FSASetPosFile(deviceData->clientHandle, file->fd, file->offset);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSASetPosFile(0x%08X, 0x%08X, 0x%08X) (%s) failed: %s\n",
							  deviceData->clientHandle, file->fd, file->offset, file->fullPath, FSAGetStatusStr(status));
		file->offset = old_pos;
		r->_errno    = FSADOT_translate_error(status);
		OSUnlockMutex(&file->mutex);
		return -1;
	}

	OSUnlockMutex(&file->mutex);
	return file->offset;
}

static int FSADOT_stat(struct _reent *r, const char *path, struct stat *st)
{
	FSError status;
	FSAStat fsStat;
	struct FSADOTDeviceData *deviceData;

	if (!path || !st) {
		r->_errno = EINVAL;
		return -1;
	}

	char *fixedPath = FSADOT_fixpath(r, path);
	if (!fixedPath) {
		r->_errno = ENOMEM;
		return -1;
	}

	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	status     = FSAGetStat(deviceData->clientHandle, fixedPath, &fsStat);
	if (status < 0) {
		if (status != FS_ERROR_NOT_FOUND) {
			WUT_DEBUG_REPORT("FSAGetStat(0x%08X, %s, %p) failed: %s\n",
								  deviceData->clientHandle, fixedPath, &fsStat, FSAGetStatusStr(status));
		}
		free(fixedPath);
		r->_errno = FSADOT_translate_error(status);
		return -1;
	}
	ino_t ino = FSADOT_hashstring(fixedPath);
	free(fixedPath);

	FSADOT_translate_stat(deviceData->clientHandle, &fsStat, ino, st);

	return 0;
}

static int FSADOT_statvfs(struct _reent *r, const char *path, struct statvfs *buf)
{
	FSError status;
	uint64_t freeSpace;
	struct FSADOTDeviceData *deviceData;

	deviceData = (struct FSADOTDeviceData *)r->deviceData;
	if (deviceData->isSDCard) {
		r->_errno = ENOSYS;
		return -1;
	}

	memset(buf, 0, sizeof(struct statvfs));

	char *fixedPath = FSADOT_fixpath(r, path);
	if (!fixedPath) {
		r->_errno = ENOMEM;
		return -1;
	}

	status = FSAGetFreeSpaceSize(deviceData->clientHandle, fixedPath, &freeSpace);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSAGetFreeSpaceSize(0x%08X, %s, %p) failed: %s\n",
							  deviceData->clientHandle, fixedPath, &freeSpace, FSAGetStatusStr(status));
		free(fixedPath);
		r->_errno = FSADOT_translate_error(status);
		return -1;
	}
	free(fixedPath);

	// File system block size
	buf->f_bsize  = deviceData->deviceSectorSize;
	// Fundamental file system block size
	buf->f_frsize = deviceData->deviceSectorSize;
	// Total number of blocks on file system in units of f_frsize
	buf->f_blocks = deviceData->deviceSizeInSectors;
	// Free blocks available for all and for non-privileged processes
	buf->f_bfree = buf->f_bavail = (uint32_t)(freeSpace / buf->f_frsize);
	// Number of inodes at this point in time
	buf->f_files                 = 0xFFFFFFFF;
	// Free inodes available for all and for non-privileged processes
	buf->f_ffree                 = 0xFFFFFFFF;
	// File system id
	buf->f_fsid                  = (unsigned long)deviceData->clientHandle;
	// Bit mask of f_flag values.
	buf->f_flag                  = 0;
	// Maximum length of filenames
	buf->f_namemax               = 255;

	return 0;
}

static int FSADOT_ftruncate(struct _reent *r, void *fd, off_t len)
{
	FSError status;
	struct FSADOTFile *file;
	struct FSADOTDeviceData *deviceData;

	// Make sure length is non-negative
	if (!fd || len < 0) {
		r->_errno = EINVAL;
		return -1;
	}

	file       = (struct FSADOTFile *)fd;

	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	OSLockMutex(&file->mutex);

	// Set the new file size
	status = FSASetPosFile(deviceData->clientHandle, file->fd, len);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSASetPosFile(0x%08X, 0x%08X, 0x%08llX) failed: %s\n",
							  deviceData->clientHandle, file->fd, len, FSAGetStatusStr(status));
		r->_errno = FSADOT_translate_error(status);
		OSUnlockMutex(&file->mutex);
		return -1;
	}

	status = FSATruncateFile(deviceData->clientHandle, file->fd);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSATruncateFile(0x%08X, 0x%08X) failed: %s\n",
							  deviceData->clientHandle, file->fd, FSAGetStatusStr(status));
		r->_errno = FSADOT_translate_error(status);
		OSUnlockMutex(&file->mutex);
		return -1;
	}

	OSUnlockMutex(&file->mutex);

	return 0;
}

static int FSADOT_unlink(struct _reent *r, const char *name)
{
	FSError status;
	char *fixedPath;
	struct FSADOTDeviceData *deviceData;

	if (!name) {
		r->_errno = EINVAL;
		return -1;
	}

	fixedPath = FSADOT_fixpath(r, name);
	if (!fixedPath) {
		r->_errno = ENOMEM;
		return -1;
	}
	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	status     = FSARemove(deviceData->clientHandle, fixedPath);
	if (status < 0) {
		WUT_DEBUG_REPORT("FSARemove(0x%08X, %s) failed: %s\n",
							  deviceData->clientHandle, fixedPath, FSAGetStatusStr(status));
		free(fixedPath);
		r->_errno = FSADOT_translate_error(status);
		return -1;
	}

	free(fixedPath);

	return 0;
}

static ssize_t FSADOT_write(struct _reent *r, void *fd, const char *ptr, size_t len)
{
	FSError status;
	struct FSADOTFile *file;
	struct FSADOTDeviceData *deviceData;

	if (!fd || !ptr) {
		r->_errno = EINVAL;
		return -1;
	}

	// Check that the file was opened with write access
	file = (struct FSADOTFile *)fd;
	if ((file->flags & O_ACCMODE) == O_RDONLY) {
		r->_errno = EBADF;
		return -1;
	}

	// cache-aligned, cache-line-sized
	__attribute__((aligned(0x40))) uint8_t alignedBuffer[0x40];

	deviceData = (struct FSADOTDeviceData *)r->deviceData;

	OSLockMutex(&file->mutex);

	// If O_APPEND is set, we always write to the end of the file.
	// When writing we file->offset to the file size to keep in sync.
	if (file->flags & O_APPEND) {
		file->offset = file->appendOffset;
	}

	size_t bytesWritten = 0;
	while (bytesWritten < len) {
		// only use input buffer if cache-aligned and write size is a multiple of cache line size
		// otherwise write from alignedBuffer
		uint8_t *tmp = (uint8_t *)ptr;
		size_t size  = len - bytesWritten;

		if (size < 0x40) {
			// write partial cache-line back-end
			tmp = alignedBuffer;
		} else if ((uintptr_t)ptr & 0x3F) {
			// write partial cache-line front-end
			tmp  = alignedBuffer;
			size = MIN(size, 0x40 - ((uintptr_t)ptr & 0x3F));
		} else {
			// write whole cache lines
			size &= ~0x3F;
		}

		// Limit each request to 256 KiB
		if (size > 0x40000) {
			size = 0x40000;
		}

		if (tmp == alignedBuffer) {
			memcpy(tmp, ptr, size);
		}

		status = FSAWriteFile(deviceData->clientHandle, tmp, 1, size, file->fd, 0);
		if (status < 0) {
			WUT_DEBUG_REPORT("FSAWriteFile(0x%08X, %p, 1, 0x%08X, 0x%08X, 0) (%s) failed: %s\n",
								  deviceData->clientHandle, tmp, size, file->fd, file->fullPath, FSAGetStatusStr(status));
			if (bytesWritten != 0) {
				OSUnlockMutex(&file->mutex);
				return bytesWritten; // error after partial write
			}

			r->_errno = FSADOT_translate_error(status);
			OSUnlockMutex(&file->mutex);
			return -1;
		}

		file->appendOffset += status;
		file->offset += status;
		bytesWritten += status;
		ptr += status;

		if ((size_t)status != size) {
			OSUnlockMutex(&file->mutex);
			return bytesWritten; // partial write
		}
	}

	OSUnlockMutex(&file->mutex);
	return bytesWritten;
}

/* lookup of device tabs */
static struct FSADOTDeviceData dot_data_tab[] = {
#define DEVICE(id, name, path, sdcard) {.isSDCard = (sdcard)},
#include "devoptab-devs.h"
};

static devoptab_t dot_tab[] = {
#define DEVICE(id, name_p, path, sdcard) \
	{ \
		.name         = (name_p), \
		.structSize   = sizeof(struct FSADOTFile), \
		.open_r       = FSADOT_open, \
		.close_r      = FSADOT_close, \
		.write_r      = FSADOT_write, \
		.read_r       = FSADOT_read, \
		.seek_r       = FSADOT_seek, \
		.fstat_r      = FSADOT_fstat, \
		.stat_r       = FSADOT_stat, \
		.unlink_r     = FSADOT_unlink, \
		.chdir_r      = FSADOT_chdir, \
		.rename_r     = FSADOT_rename, \
		.mkdir_r      = FSADOT_mkdir, \
		.dirStateSize = sizeof(struct FSADOTDir), \
		.diropen_r    = FSADOT_diropen, \
		.dirreset_r   = FSADOT_dirreset, \
		.dirnext_r    = FSADOT_dirnext, \
		.dirclose_r   = FSADOT_dirclose, \
		.statvfs_r    = FSADOT_statvfs, \
		.ftruncate_r  = FSADOT_ftruncate, \
		.fsync_r      = FSADOT_fsync, \
		.deviceData   = dot_data_tab + (id), \
		.chmod_r      = FSADOT_chmod, \
		.rmdir_r      = FSADOT_rmdir, \
		.lstat_r      = FSADOT_stat, \
	},
#include "devoptab-devs.h"
};

int FSADOT_Init(void)
{
	devoptab_t *op;
	struct FSADOTDeviceData *opd;
	FSError rc;
	int r;

	/* Initialize FSA */
	if (FSAInit() < 0)
		return -1;

	r = -1;

#define DEVICE(id, name, path, sdcard) \
	{ \
		op = dot_tab + id; \
		opd = dot_data_tab + id; \
	\
		opd->clientHandle = FSAAddClient(NULL); \
		if (opd->clientHandle) { \
			if ((rc = FSAMount(opd->clientHandle, (path), "/vol/schism_" #name, FSA_MOUNT_FLAG_LOCAL_MOUNT, NULL, 0)) >= 0) { \
				FSADeviceInfo devinfo; \
	\
				if (FSAGetDeviceInfo(opd->clientHandle, "/vol/schism_" #name, &devinfo) >= 0) { \
					opd->deviceSizeInSectors = devinfo.deviceSizeInSectors; \
					opd->deviceSectorSize    = devinfo.deviceSectorSize; \
				} else { \
					/* okay assumption */ \
					opd->deviceSizeInSectors = 0xFFFFFFFF; \
					opd->deviceSectorSize    = 512; \
				} \
	\
				if (AddDevice(op) >= 0) { \
					strncpy(opd->mountPath, "/vol/schism_" #name, sizeof(opd->mountPath) - 1); \
					opd->mountPath[sizeof(opd->mountPath) - 1] = 0; \
					strcpy(opd->cwd, opd->mountPath); \
					r = 0; \
				} else { \
					FSAUnmount(opd->clientHandle, "/vol/schism_" #name, FSA_UNMOUNT_FLAG_BIND_MOUNT); \
					FSADelClient(opd->clientHandle); \
				} \
			} else { \
				log_appendf(4, "[WiiU] FSADOT/%s: FSAMount returned %d!\n", (name), rc); \
				/* fail; should probably log this */ \
				FSADelClient(opd->clientHandle); \
			} \
		} else { \
			log_appendf(4, "[WiiU] FSADOT/%s: FSAAddClient failed!\n", (name)); \
		} \
	}
#include "devoptab-devs.h"

	return r; /* ok */
}

int FSADOT_Quit(void)
{
	/* Kill them all */
#define DEVICE(id, name, path, sdcard) \
	{ \
		const devoptab_t *dev = GetDeviceOpTab(name); \
		if (dev) { \
			struct FSADOTDeviceData *dot = dev->deviceData; \
			/* Sanity check */ \
			if (dot >= dot_data_tab && dot < (dot_data_tab + ARRAY_SIZE(dot_data_tab))) { \
				/* Ok, this is our device; remove it from the devoptab */ \
				RemoveDevice(name); \
	\
				/* Now kill off everything */ \
				FSAUnmount(dot->clientHandle, dot->mountPath, FSA_UNMOUNT_FLAG_BIND_MOUNT); \
	\
				FSADelClient(dot->clientHandle); \
			} \
		} \
	}
#include "devoptab-devs.h"

	return 0;
}
