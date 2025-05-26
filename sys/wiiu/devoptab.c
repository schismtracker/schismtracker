/***************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/

#include "headers.h"

#include <sys/iosupport.h>
#include <sys/statvfs.h>
#include <sys/dirent.h>
#include <malloc.h>
#include <fcntl.h>
#include <sys/iosupport.h>
#include <coreinit/filesystem_fsa.h>
#include <coreinit/debug.h>
#include <coreinit/mutex.h>

#include "devoptab.h"

typedef struct fs_dev_private_t {
	FSAClientHandle fsaFd;
	int mounted;
	OSMutex pMutex;
	char mount_path[SCHISM_FAM_SIZE];
} fs_dev_private_t;

typedef struct fs_dev_file_state_t {
	fs_dev_private_t *dev;
	FSAFileHandle fd;                           /* File descriptor */
	int flags;                                  /* Opening flags */
	int read;                                   /* True if allowed to read from file */
	int write;                                  /* True if allowed to write to file */
	int append;                                 /* True if allowed to append to file */
	uint32_t pos;                               /* Current position within the file (in bytes) */
	uint32_t len;                               /* Total length of the file (in bytes) */
	struct fs_dev_file_state_t *prevOpenFile;   /* The previous entry in a double-linked FILO list of open files */
	struct fs_dev_file_state_t *nextOpenFile;   /* The next entry in a double-linked FILO list of open files */
} fs_dev_file_state_t;

typedef struct fs_dev_dir_entry_t {
	fs_dev_private_t *dev;
	FSADirectoryHandle dirHandle;
} fs_dev_dir_entry_t;

typedef struct fs_devoptab_extra_t {
	devoptab_t devoptab;
	char devname[SCHISM_FAM_SIZE];
} fs_devoptab_extra_t;

static fs_dev_private_t *fs_dev_get_device_data(const char *path)
{
	const devoptab_t *devoptab = NULL;
	char name[128] = {0};
	int i;

	// Get the device name from the path
	strncpy(name, path, 127);
	strtok(name, ":/");

	// Search the devoptab table for the specified device name
	// NOTE: We do this manually due to a 'bug' in GetDeviceOpTab
	//       which ignores names with suffixes and causes names
	//       like "ntfs" and "ntfs1" to be seen as equals
	for (i = 3; i < STD_MAX; i++) {
		devoptab = devoptab_list[i];
		if (devoptab && devoptab->name) {
			if (!strcmp(name, devoptab->name)) {
				return (fs_dev_private_t *)devoptab->deviceData;
			}
		}
	}

	return NULL;
}

static char *fs_dev_real_path (const char *path, fs_dev_private_t *dev)
{
	// Sanity check
	if (!path)
		return NULL;

	// Move the path pointer to the start of the actual path
	if (strchr(path, ':') != NULL) {
		path = strchr(path, ':') + 1;
	}

	size_t mount_len = strlen(dev->mount_path);
	size_t path_len = strlen(path);

	char *new_name = (char*)malloc(mount_len + path_len + 1);
	if (new_name) {
		memcpy(new_name, dev->mount_path, mount_len);
		memcpy(new_name + mount_len, path, path_len + 1); /* copy nul terminator */
		return new_name;
	}
	return new_name;
}

static int fs_dev_open_r (struct _reent *r, void *fileStruct, const char *path, int flags, int mode)
{
	fs_dev_private_t *dev = fs_dev_get_device_data(path);
	if (!dev) {
		r->_errno = ENODEV;
		return -1;
	}

	fs_dev_file_state_t *file = (fs_dev_file_state_t *)fileStruct;

	file->dev = dev;
	// Determine which mode the file is opened for
	file->flags = flags;

	const char *fsMode;
	
	// Map flags to open modes
	if (flags == 0) {
		file->read = 1;
		file->write = 0;
		file->append = 0;
		fsMode = "r";
	} else if (flags == 2) {
		file->read = 1;
		file->write = 1;
		file->append = 0;
		fsMode = "r+";
	} else if (flags == 0x601) {
		file->read = 0;
		file->write = 1;
		file->append = 0;
		fsMode = "w";
	} else if (flags == 0x602) {
		file->read = 1;
		file->write = 1;
		file->append = 0;
		fsMode = "w+";
	} else if (flags == 0x209) {
		file->read = 0;
		file->write = 1;
		file->append = 1;
		fsMode = "a";
	} else if (flags == 0x20A) {
		file->read = 1;
		file->write = 1;
		file->append = 1;
		fsMode = "a+";
	} else {
		r->_errno = EINVAL;
		return -1;
	}
   

	uint32_t fd = -1;

	OSLockMutex(&dev->pMutex);

	char *real_path = fs_dev_real_path(path, dev);
	if (!path) {
		r->_errno = ENOMEM;
		OSUnlockMutex(&dev->pMutex);
		return -1;
	}

	FSError result = FSAOpenFileEx(dev->fsaFd, real_path, fsMode,
		(FSMode)(((mode & S_IRWXU) << 2) | ((mode & S_IRWXG) << 1) | (mode & S_IRWXO)),
		FS_OPEN_FLAG_NONE, 0, &fd);

	free(real_path);

	if (!result) {
		FSAStat stats;
		result = FSAGetStatFile(dev->fsaFd, fd, &stats);
		if (result != 0) {
			FSACloseFile(dev->fsaFd, fd);
			r->_errno = result;
			OSUnlockMutex(&dev->pMutex);
			return -1;
		}
		file->fd = fd;
		file->pos = 0;
		file->len = stats.size;
		OSUnlockMutex(&dev->pMutex);
		return (int)file;
	}

	r->_errno = result;
	OSUnlockMutex(&dev->pMutex);
	return -1;
}


static int fs_dev_close_r (struct _reent *r, void *fd)
{
	fs_dev_file_state_t *file = (fs_dev_file_state_t *)fd;
	if (!file->dev) {
		r->_errno = ENODEV;
		return -1;
	}

	OSLockMutex(&file->dev->pMutex);

	FSError result = FSACloseFile(file->dev->fsaFd, file->fd);

	OSUnlockMutex(&file->dev->pMutex);

	if (result < 0) {
		r->_errno = result;
		return -1;
	}
	return 0;
}

static off_t fs_dev_seek_r (struct _reent *r, void *fd, off_t pos, int dir)
{
	fs_dev_file_state_t *file = (fs_dev_file_state_t *)fd;
	if (!file->dev) {
		r->_errno = ENODEV;
		return 0;
	}

	OSLockMutex(&file->dev->pMutex);

	switch(dir) {
	case SEEK_SET:
		file->pos = pos;
		break;
	case SEEK_CUR:
		file->pos += pos;
		break;
	case SEEK_END:
		file->pos = file->len + pos;
		break;
	default:
		r->_errno = EINVAL;
		return -1;
	}

	FSError result = FSASetPosFile(file->dev->fsaFd, file->fd, file->pos);

	OSUnlockMutex(&file->dev->pMutex);

	if (!result) {
		return file->pos;
	}

	return result;
}

static ssize_t fs_dev_write_r (struct _reent *r, void *fd, const char *ptr, size_t len)
{
	fs_dev_file_state_t *file = (fs_dev_file_state_t *)fd;
	if (!file->dev) {
		r->_errno = ENODEV;
		return 0;
	}

	if (!file->write) {
		r->_errno = EACCES;
		return 0;
	}

	OSLockMutex(&file->dev->pMutex);

	size_t done = 0;

	while (done < len) {
		size_t write_size = len - done;

		FSError result = FSAWriteFile(file->dev->fsaFd, (char *)ptr + done, 0x01, write_size, file->fd, 0);
		if (result < 0) {
			r->_errno = result;
			break;
		} else if (result == 0) {
			if (write_size > 0)
				done = 0;
			break;
		} else {
			done += result;
			file->pos += result;
		}
	}

	OSUnlockMutex(&file->dev->pMutex);
	return done;
}

static ssize_t fs_dev_read_r (struct _reent *r, void *fd, char *ptr, size_t len)
{
	fs_dev_file_state_t *file = (fs_dev_file_state_t *)fd;
	if (!file->dev) {
		r->_errno = ENODEV;
		return 0;
	}

	if (!file->read) {
		r->_errno = EACCES;
		return 0;
	}

	OSLockMutex(&file->dev->pMutex);

	size_t done = 0;

	while (done < len) {
		size_t read_size = len - done;

		FSError result = FSAReadFile(file->dev->fsaFd, ptr + done, 0x01, read_size, file->fd, 0);

		if (result < 0) {
			r->_errno = result;
			done = 0;
			break;
		} else if (result == 0) {
			//! TODO: error on read_size > 0
			break;
		} else {
			done += result;
			file->pos += result;
		}
	}

	OSUnlockMutex(&file->dev->pMutex);
	return done;
}


static int fs_dev_fstat_r (struct _reent *r, void *fd, struct stat *st)
{
	fs_dev_file_state_t *file = (fs_dev_file_state_t *)fd;
	if (!file->dev) {
		r->_errno = ENODEV;
		return -1;
	}

	OSLockMutex(&file->dev->pMutex);

	// Zero out the stat buffer
	memset(st, 0, sizeof(struct stat));

	FSAStat stats;
	FSError result = FSAGetStatFile(file->dev->fsaFd, (int)fd, &stats);
	if (result != 0) {
		r->_errno = result;
		OSUnlockMutex(&file->dev->pMutex);
		return -1;
	}

	st->st_mode = S_IFREG;
	st->st_size = stats.size;
	st->st_blocks = (stats.size + 511) >> 9;
	st->st_nlink = 1;
	st->st_dev = stats.entryId;
	st->st_uid = stats.owner;
	st->st_gid = stats.group;
	st->st_ino = stats.entryId;
	st->st_atime = stats.modified;
	st->st_ctime = stats.created;
	st->st_mtime = stats.modified;

	OSUnlockMutex(&file->dev->pMutex);
	return 0;
}

static int fs_dev_ftruncate_r (struct _reent *r, void *fd, off_t len)
{
	int status;
	fs_dev_file_state_t *file = (fs_dev_file_state_t *)fd;

	if (!file->dev) {
		r->_errno = ENODEV;
		return -1;
	}

	status = FSASetPosFile(file->dev->fsaFd, file->fd, len);
	if (status < 0) {
		r->_errno = status;
		return -1;
	}

	status = FSATruncateFile(file->dev->fsaFd, file->fd);
	if (status < 0) {
		r->_errno = status;
		return -1;
	}

	return 0;
}

static int fs_dev_fsync_r (struct _reent *r, void *fd)
{
	int status;
	fs_dev_file_state_t *file = (fs_dev_file_state_t *)fd;
	if (!file->dev) {
		r->_errno = ENODEV;
		return -1;
	}

	status = FSAFlushFile(file->dev->fsaFd, file->fd);
	if (status < 0) {
		r->_errno = status;
		return -1;
	}

	return 0;
}

static int fs_dev_stat_r (struct _reent *r, const char *path, struct stat *st)
{
	fs_dev_private_t *dev = fs_dev_get_device_data(path);
	if (!dev) {
		r->_errno = ENODEV;
		return -1;
	}

	OSLockMutex(&dev->pMutex);

	// Zero out the stat buffer
	memset(st, 0, sizeof(struct stat));

	char *real_path = fs_dev_real_path(path, dev);
	if (!real_path) {
		r->_errno = ENOMEM;
		OSUnlockMutex(&dev->pMutex);
		return -1;
	}

	FSAStat stats;

	FSError result = FSAGetStat(dev->fsaFd, real_path, &stats);

	free(real_path);

	if (result < 0) {
		r->_errno = result;
		OSUnlockMutex(&dev->pMutex);
		return -1;
	}

	st->st_mode = (
		(stats.flags & FS_STAT_DIRECTORY) ||
		/* mark root also as directory */
		(strlen(dev->mount_path) + 1 == strlen(real_path))
	) ? S_IFDIR : S_IFREG;
	st->st_size = stats.size;
	st->st_blocks = (stats.size + 511) >> 9;
	st->st_nlink = 1;
	st->st_dev = stats.entryId;
	st->st_uid = stats.owner;
	st->st_gid = stats.group;
	st->st_ino = stats.entryId;
	st->st_atime = stats.modified;
	st->st_ctime = stats.created;
	st->st_mtime = stats.modified;

	OSUnlockMutex(&dev->pMutex);

	return 0;
}

static int fs_dev_link_r (struct _reent *r, const char *existing, const char *newLink)
{
	r->_errno = ENOTSUP;
	return -1;
}

static int fs_dev_unlink_r (struct _reent *r, const char *name)
{
	fs_dev_private_t *dev = fs_dev_get_device_data(name);
	if (!dev) {
		r->_errno = ENODEV;
		return -1;
	}

	OSLockMutex(&dev->pMutex);

	char *real_path = fs_dev_real_path(name, dev);
	if (!real_path) {
		r->_errno = ENOMEM;
		OSUnlockMutex(&dev->pMutex);
		return -1;
	}

	FSError result = FSARemove(dev->fsaFd, real_path);

	free(real_path);

	OSUnlockMutex(&dev->pMutex);

	if (result < 0) {
		r->_errno = result;
		return -1;
	}

	return result;
}

static int fs_dev_chdir_r (struct _reent *r, const char *name)
{
	fs_dev_private_t *dev = fs_dev_get_device_data(name);
	if (!dev) {
		r->_errno = ENODEV;
		return -1;
	}

	OSLockMutex(&dev->pMutex);

	char *real_path = fs_dev_real_path(name, dev);
	if (!real_path) {
		r->_errno = ENOMEM;
		OSUnlockMutex(&dev->pMutex);
		return -1;
	}

	FSError result = FSAChangeDir(dev->fsaFd, real_path);

	free(real_path);

	OSUnlockMutex(&dev->pMutex);

	if (result < 0) {
		r->_errno = result;
		return -1;
	}

	return 0;
}

static int fs_dev_rename_r (struct _reent *r, const char *oldName, const char *newName)
{
	fs_dev_private_t *dev = fs_dev_get_device_data(oldName);
	if (!dev) {
		r->_errno = ENODEV;
		return -1;
	}

	OSLockMutex(&dev->pMutex);

	char *real_oldpath = fs_dev_real_path(oldName, dev);
	if (!real_oldpath) {
		r->_errno = ENOMEM;
		OSUnlockMutex(&dev->pMutex);
		return -1;
	}
	char *real_newpath = fs_dev_real_path(newName, dev);
	if (!real_newpath) {
		r->_errno = ENOMEM;
		free(real_oldpath);
		OSUnlockMutex(&dev->pMutex);
		return -1;
	}

	FSError result = FSARename(dev->fsaFd, real_oldpath, real_newpath);

	free(real_oldpath);
	free(real_newpath);

	OSUnlockMutex(&dev->pMutex);

	if (result < 0) {
		r->_errno = result;
		return -1;
	}

	return 0;

}

static int fs_dev_mkdir_r (struct _reent *r, const char *path, int mode)
{
	fs_dev_private_t *dev = fs_dev_get_device_data(path);
	if (!dev) {
		r->_errno = ENODEV;
		return -1;
	}

	OSLockMutex(&dev->pMutex);

	char *real_path = fs_dev_real_path(path, dev);
	if (!real_path) {
		r->_errno = ENOMEM;
		OSUnlockMutex(&dev->pMutex);
		return -1;
	}

	FSError result = FSAMakeDir(dev->fsaFd, real_path, mode);

	free(real_path);

	OSUnlockMutex(&dev->pMutex);

	if (result < 0) {
		r->_errno = result;
		return -1;
	}

	return 0;
}

static int fs_dev_chmod_r (struct _reent *r, const char *path, int mode)
{
	fs_dev_private_t *dev = fs_dev_get_device_data(path);
	if (!dev) {
		r->_errno = ENODEV;
		return -1;
	}

	OSLockMutex(&dev->pMutex);

	char *real_path = fs_dev_real_path(path, dev);
	if (!real_path) {
		r->_errno = ENOMEM;
		OSUnlockMutex(&dev->pMutex);
		return -1;
	}

	FSError result = FSAChangeMode(dev->fsaFd, real_path, mode);

	free(real_path);

	OSUnlockMutex(&dev->pMutex);

	if (result < 0) {
		r->_errno = result;
		return -1;
	}

	return 0;
}

static int fs_dev_statvfs_r (struct _reent *r, const char *path, struct statvfs *buf)
{
	FSADeviceInfo devinfo;

	fs_dev_private_t *dev = fs_dev_get_device_data(path);
	if (!dev) {
		r->_errno = ENODEV;
		return -1;
	}

	OSLockMutex(&dev->pMutex);

	// Zero out the stat buffer
	memset(buf, 0, sizeof(struct statvfs));

	char *real_path = fs_dev_real_path(path, dev);
	if (!real_path) {
		r->_errno = ENOMEM;
		OSUnlockMutex(&dev->pMutex);
		return -1;
	}

	uint64_t size;

	FSError result = FSAGetDeviceInfo(dev->fsaFd, real_path, &devinfo);

	free(real_path);

	if (result < 0) {
		r->_errno = result;
		OSUnlockMutex(&dev->pMutex);
		return -1;
	}

	// File system block size
	buf->f_bsize = 512;

	// Fundamental file system block size
	buf->f_frsize = 512;

	// Total number of blocks on file system in units of f_frsize
	buf->f_blocks = size >> 9; // this is unknown

	// Free blocks available for all and for non-privileged processes
	buf->f_bfree = buf->f_bavail = size >> 9;

	// Number of inodes at this point in time
	buf->f_files = 0xffffffff;

	// Free inodes available for all and for non-privileged processes
	buf->f_ffree = 0xffffffff;

	// File system id
	buf->f_fsid = (int)dev;

	// Bit mask of f_flag values.
	buf->f_flag = 0;

	// Maximum length of filenames
	buf->f_namemax = 255;

	OSUnlockMutex(&dev->pMutex);

	return 0;
}

static DIR_ITER *fs_dev_diropen_r (struct _reent *r, DIR_ITER *dirState, const char *path)
{
	FSADirectoryHandle dirHandle;
	FSError result;

	fs_dev_private_t *dev = fs_dev_get_device_data(path);
	if (!dev) {
		r->_errno = ENODEV;
		return NULL;
	}

	fs_dev_dir_entry_t *dirIter = (fs_dev_dir_entry_t *)dirState->dirStruct;

	OSLockMutex(&dev->pMutex);

	{
		char *real_path = fs_dev_real_path(path, dev);
		if (!real_path) {
			r->_errno = ENOMEM;
			OSUnlockMutex(&dev->pMutex);
			return NULL;
		}

		result = FSAOpenDir(dev->fsaFd, real_path, &dirHandle);

		free(real_path);
	}

	OSUnlockMutex(&dev->pMutex);

	if (result < 0) {
		r->_errno = result;
		return NULL;
	}

	dirIter->dev = dev;
	dirIter->dirHandle = dirHandle;

	return dirState;
}

static int fs_dev_dirclose_r (struct _reent *r, DIR_ITER *dirState)
{
	fs_dev_dir_entry_t *dirIter = (fs_dev_dir_entry_t *)dirState->dirStruct;
	if (!dirIter->dev) {
		r->_errno = ENODEV;
		return -1;
	}

	OSLockMutex(&dirIter->dev->pMutex);

	FSError result = FSACloseDir(dirIter->dev->fsaFd, dirIter->dirHandle);

	OSUnlockMutex(&dirIter->dev->pMutex);

	if (result < 0) {
		r->_errno = result;
		return -1;
	}

	return 0;
}

static int fs_dev_dirreset_r (struct _reent *r, DIR_ITER *dirState)
{
	fs_dev_dir_entry_t *dirIter = (fs_dev_dir_entry_t *)dirState->dirStruct;
	if (!dirIter->dev) {
		r->_errno = ENODEV;
		return -1;
	}

	OSLockMutex(&dirIter->dev->pMutex);

	FSError result = FSARewindDir(dirIter->dev->fsaFd, dirIter->dirHandle);

	OSUnlockMutex(&dirIter->dev->pMutex);

	if (result < 0) {
		r->_errno = result;
		return -1;
	}

	return 0;
}

static int fs_dev_dirnext_r (struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *st)
{
	fs_dev_dir_entry_t *dirIter = (fs_dev_dir_entry_t *)dirState->dirStruct;
	if (!dirIter->dev) {
		r->_errno = ENODEV;
		return -1;
	}

	OSLockMutex(&dirIter->dev->pMutex);

	FSADirectoryEntry * dir_entry = malloc(sizeof(FSADirectoryEntry));

	FSError result = FSAReadDir(dirIter->dev->fsaFd, dirIter->dirHandle, dir_entry);
	if (result < 0) {
		free(dir_entry);
		r->_errno = result;
		OSUnlockMutex(&dirIter->dev->pMutex);
		return -1;
	}

	// Fetch the current entry (no overflow!)
	strncpy(filename, dir_entry->name, NAME_MAX - 1);
	filename[NAME_MAX - 1] = '\0';

	if (st) {
		memset(st, 0, sizeof(struct stat));
		st->st_mode = (dir_entry->info.flags & FS_STAT_DIRECTORY) ? S_IFDIR : S_IFREG;
		st->st_nlink = 1;
		st->st_size = dir_entry->info.size;
		st->st_blocks = (dir_entry->info.size + 511) >> 9;
		st->st_dev = dir_entry->info.entryId;
		st->st_uid = dir_entry->info.owner;
		st->st_gid = dir_entry->info.group;
		st->st_ino = dir_entry->info.entryId;
		st->st_atime = dir_entry->info.modified;
		st->st_ctime = dir_entry->info.created;
		st->st_mtime = dir_entry->info.modified;
	}

	free(dir_entry);
	OSUnlockMutex(&dirIter->dev->pMutex);
	return 0;
}

// NTFS device driver devoptab
static const devoptab_t devops_fs = {
	NULL, /* Device name */
	sizeof (fs_dev_file_state_t),
	fs_dev_open_r,
	fs_dev_close_r,
	fs_dev_write_r,
	fs_dev_read_r,
	fs_dev_seek_r,
	fs_dev_fstat_r,
	fs_dev_stat_r,
	fs_dev_link_r,
	fs_dev_unlink_r,
	fs_dev_chdir_r,
	fs_dev_rename_r,
	fs_dev_mkdir_r,
	sizeof (fs_dev_dir_entry_t),
	fs_dev_diropen_r,
	fs_dev_dirreset_r,
	fs_dev_dirnext_r,
	fs_dev_dirclose_r,
	fs_dev_statvfs_r,
	fs_dev_ftruncate_r,
	fs_dev_fsync_r,
	fs_dev_chmod_r,
	NULL, /* fs_dev_fchmod_r */
	NULL  /* Device data */
};

static int fs_dev_add_device (const char *name, const char *mount_path, FSAClientHandle fsaFd, int isMounted)
{
	fs_devoptab_extra_t *dev = NULL;
	char *devname = NULL;
	char *devpath = NULL;
	size_t namelen, mountpathlen;
	int i;

	// Sanity check
	if (!name || !mount_path) {
		errno = EINVAL;
		return -1;
	}

	namelen = strlen(name);

	// Allocate a devoptab for this device
	dev = malloc(sizeof(*dev) + namelen + 1);
	if (!dev) {
		errno = ENOMEM;
		return -1;
	}

	// Use the space allocated at the end of the devoptab for storing the device name
	memcpy(dev->devname, name, namelen + 1);

	mountpathlen = strlen(mount_path);

	// create private data
	fs_dev_private_t *priv = (fs_dev_private_t *)malloc(sizeof(fs_dev_private_t) + mountpathlen + 1);
	if (!priv) {
		free(dev);
		errno = ENOMEM;
		return -1;
	}

	memcpy(priv->mount_path, mount_path, mountpathlen + 1);

	// setup private data
	priv->fsaFd = fsaFd;
	priv->mounted = isMounted;
	memset(&priv->pMutex, 0, sizeof(priv->pMutex));

	OSInitMutex(&priv->pMutex);

	// Setup the devoptab
	memcpy(&dev->devoptab, &devops_fs, sizeof(devoptab_t));
	dev->devoptab.name = devname;
	dev->devoptab.deviceData = priv;

	// Add the device to the devoptab table (if there is a free slot)
	for (i = 3; i < STD_MAX; i++) {
		if (devoptab_list[i] == devoptab_list[0]) {
			devoptab_list[i] = &dev->devoptab;
			return 0;
		}
	}

	// failure, free all memory
	free(priv);
	free(dev);

	// If we reach here then there are no free slots in the devoptab table for this device
	errno = EADDRNOTAVAIL;
	return -1;
}

static int fs_dev_remove_device (const char *path)
{
	const devoptab_t *devoptab = NULL;
	char name[128];
	int i;

	// Get the device name from the path
	strncpy(name, path, 127);
	name[127] = '\0';
	strtok(name, ":/");

	// Find and remove the specified device from the devoptab table
	// NOTE: We do this manually due to a 'bug' in RemoveDevice
	//       which ignores names with suffixes and causes names
	//       like "ntfs" and "ntfs1" to be seen as equals
	for (i = 3; i < STD_MAX; i++) {
		devoptab = devoptab_list[i];
		if (devoptab && devoptab->name) {
			if (!strcmp(name, devoptab->name)) {
				devoptab_list[i] = devoptab_list[0];

				if (devoptab->deviceData) {
					fs_dev_private_t *priv = (fs_dev_private_t *)devoptab->deviceData;

					if (priv->mounted)
						FSAUnmount(priv->fsaFd, priv->mount_path, 2);

					free(devoptab->deviceData);
				}

				free((devoptab_t*)devoptab);
				return 0;
			}
		}
	}

	return -1;
}

int wiiu_mount_fs(const char *virt_name, FSAClientHandle fsaFd, const char *dev_path, const char *mount_path, int mount_flag)
{
	int isMounted = 0;

	if (dev_path) {
		isMounted = 1;

		int res = FSAMount(fsaFd, dev_path, mount_path, mount_flag, NULL, 0);
		if (res != 0)
			return res;
	}

	return fs_dev_add_device(virt_name, mount_path, fsaFd, isMounted);
}

int wiiu_unmount_fs(const char *virt_name)
{
	return fs_dev_remove_device(virt_name);
}
