/*

libisfs -- a NAND filesystem devoptab library for the Wii

Copyright (C) 2008 Joseph Jordan <joe.ftpii@psychlaw.com.au>

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from
the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1.The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software in a
product, an acknowledgment in the product documentation would be
appreciated but is not required.

2.Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3.This notice may not be removed or altered from any source distribution.


[In compliance with the above: I patched this code up somewhat so that it
builds with all warnings. -- Storlek]
*/
#include "headers.h"

#include <errno.h>
#include <ogc/isfs.h>
#include <ogc/lwp_watchdog.h>
#include <ogcsys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <sys/iosupport.h>

#include "isfs.h"

#define DEVICE_NAME "isfs"

#define FLAG_DIR 1
#define DIR_SEPARATOR '/'
#define SECTOR_SIZE 0x800
#define BUFFER_SIZE 0x8000

typedef struct DIR_ENTRY_STRUCT {
    char *name;
    char *abspath;
    u32 size;
    u8 flags;
    u32 fileCount;
    struct DIR_ENTRY_STRUCT *children;
} DIR_ENTRY;

typedef struct {
    DIR_ENTRY *entry;
    s32 isfs_fd;
    bool inUse;
} FILE_STRUCT;

typedef struct {
    DIR_ENTRY *entry;
    u32 index;
    bool inUse;
} DIR_STATE_STRUCT;

static char read_buffer[BUFFER_SIZE] __attribute__((aligned(32)));

static DIR_ENTRY *root = NULL;
static DIR_ENTRY *current = NULL;
static s32 dotab_device = -1;

static bool is_dir(DIR_ENTRY *entry) {
    return entry->flags & FLAG_DIR;
}

static bool invalid_drive_specifier(const char *path) {
    if (strchr(path, ':') == NULL) return false;
    int namelen = strlen(DEVICE_NAME);
    if (!strncmp(DEVICE_NAME, path, namelen) && path[namelen] == ':') return false;
    return true;
}

static DIR_ENTRY *entry_from_path(const char *path) {
    if (invalid_drive_specifier(path)) return NULL;
    if (strchr(path, ':') != NULL) path = strchr(path, ':') + 1;
    DIR_ENTRY *entry;
    bool found = false;
    bool notFound = false;
    const char *pathPosition = path;
    const char *pathEnd = strchr(path, '\0');
    if (pathPosition[0] == DIR_SEPARATOR) {
	entry = root;
	while (pathPosition[0] == DIR_SEPARATOR) pathPosition++;
	if (pathPosition >= pathEnd) found = true;
    } else {
	entry = current;
    }
    if (entry == root && !strcmp(".", pathPosition)) found = true;
    DIR_ENTRY *dir = entry;
    while (!found && !notFound) {
	const char *nextPathPosition = strchr(pathPosition, DIR_SEPARATOR);
	size_t dirnameLength;
	if (nextPathPosition != NULL) dirnameLength = nextPathPosition - pathPosition;
	else dirnameLength = strlen(pathPosition);
	if (dirnameLength >= ISFS_MAXPATHLEN) return NULL;

	u32 fileIndex = 0;
	while (fileIndex < dir->fileCount && !found && !notFound) {
	    entry = &dir->children[fileIndex];
	    if (dirnameLength == strnlen(entry->name, ISFS_MAXPATHLEN - 1)
		&& !strncasecmp(pathPosition, entry->name, dirnameLength)) found = true;
	    if (found && !is_dir(entry) && nextPathPosition) found = false;
	    if (!found) fileIndex++;
	}

	if (fileIndex >= dir->fileCount) {
	    notFound = true;
	    found = false;
	} else if (!nextPathPosition || nextPathPosition >= pathEnd) {
	    found = true;
	} else if (is_dir(entry)) {
	    dir = entry;
	    pathPosition = nextPathPosition;
	    while (pathPosition[0] == DIR_SEPARATOR) pathPosition++;
	    if (pathPosition >= pathEnd) found = true;
	    else found = false;
	}
    }

    if (found && !notFound) return entry;
    return NULL;
}

static ssize_t _ISFS_open_r(struct _reent *r, void *fd, const char *path,
			SCHISM_UNUSED int flags, SCHISM_UNUSED int mode) {
    FILE_STRUCT *file = (FILE_STRUCT *)fd;
    DIR_ENTRY *entry = entry_from_path(path);
    if (!entry) {
	r->_errno = ENOENT;
	return -1;
    } else if (is_dir(entry)) {
	r->_errno = EISDIR;
	return -1;
    }

    file->entry = entry;
    file->inUse = true;
    file->isfs_fd = ISFS_Open(entry->abspath, ISFS_OPEN_READ);
    if (file->isfs_fd < 0) {
	if (file->isfs_fd == ISFS_EINVAL) r->_errno = EACCES;
	else r->_errno = -file->isfs_fd;
	return -1;
    }

    return (int)file;
}

static ssize_t _ISFS_close_r(struct _reent *r, void* fd) {
    FILE_STRUCT *file = (FILE_STRUCT *)fd;
    if (!file->inUse) {
	r->_errno = EBADF;
	return -1;
    }
    file->inUse = false;

    s32 ret = ISFS_Close(file->isfs_fd);
    if (ret < 0) {
	r->_errno = -ret;
	return -1;
    }

    return 0;
}

static ssize_t _ISFS_read_r(struct _reent *r, void* fd, char *ptr, size_t len) {
    FILE_STRUCT *file = (FILE_STRUCT *)fd;
    if (!file->inUse) {
	r->_errno = EBADF;
	return -1;
    }
    if (len <= 0) {
	return 0;
    }

    s32 ret = ISFS_Read(file->isfs_fd, read_buffer, len);
    if (ret < 0) {
	r->_errno = -ret;
	return -1;
    } else if ((size_t) ret < len) {
	r->_errno = EOVERFLOW;
    }
    
    memcpy(ptr, read_buffer, ret);
    return ret;
}

static off_t _ISFS_seek_r(struct _reent *r, void* fd, off_t pos, int dir) {
    FILE_STRUCT *file = (FILE_STRUCT *)fd;
    if (!file->inUse) {
	r->_errno = EBADF;
	return -1;
    }

    s32 ret = ISFS_Seek(file->isfs_fd, pos, dir);
    if (ret < 0) {
	r->_errno = -ret;
	return -1;
    }
    return ret;
}

static void stat_entry(DIR_ENTRY *entry, struct stat *st) {
    st->st_dev = 0x4957;
    st->st_ino = 0;
    st->st_mode = ((is_dir(entry)) ? S_IFDIR : S_IFREG) | (S_IRUSR | S_IRGRP | S_IROTH);
    st->st_nlink = 1;
    st->st_uid = 1;
    st->st_gid = 2;
    st->st_rdev = st->st_dev;
    st->st_size = entry->size;
    st->st_atime = 0;
    st->st_mtime = 0;
    st->st_ctime = 0;
    st->st_blksize = SECTOR_SIZE;
    st->st_blocks = (entry->size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    st->st_spare4[0] = 0;
    st->st_spare4[1] = 0;
}

static int _ISFS_fstat_r(struct _reent *r, void* fd, struct stat *st) {
    FILE_STRUCT *file = (FILE_STRUCT *)fd;
    if (!file->inUse) {
	r->_errno = EBADF;
	return -1;
    }
    stat_entry(file->entry, st);
    return 0;
}

static int _ISFS_stat_r(struct _reent *r, const char *path, struct stat *st) {
    DIR_ENTRY *entry = entry_from_path(path);
    if (!entry) {
	r->_errno = ENOENT;
	return -1;
    }
    stat_entry(entry, st);
    return 0;
}

static int _ISFS_chdir_r(struct _reent *r, const char *path) {
    DIR_ENTRY *entry = entry_from_path(path);
    if (!entry) {
	r->_errno = ENOENT;
	return -1;
    } else if (!is_dir(entry)) {
	r->_errno = ENOTDIR;
	return -1;
    }
    return 0;
}

static DIR_ITER *_ISFS_diropen_r(struct _reent *r, DIR_ITER *dirState, const char *path) {
    DIR_STATE_STRUCT *state = (DIR_STATE_STRUCT *)(dirState->dirStruct);
    state->entry = entry_from_path(path);
    if (!state->entry) {
	r->_errno = ENOENT;
	return NULL;
    } else if (!is_dir(state->entry)) {
	r->_errno = ENOTDIR;
	return NULL;
    }
    state->index = 0;
    state->inUse = true;
    return dirState;
}

static int _ISFS_dirreset_r(struct _reent *r, DIR_ITER *dirState) {
    DIR_STATE_STRUCT *state = (DIR_STATE_STRUCT *)(dirState->dirStruct);
    if (!state->inUse) {
	r->_errno = EBADF;
	return -1;
    }
    state->index = 0;
    return 0;
}

static int _ISFS_dirnext_r(struct _reent *r, DIR_ITER *dirState, char *filename, struct stat *st) {
    DIR_STATE_STRUCT *state = (DIR_STATE_STRUCT *)(dirState->dirStruct);
    if (!state->inUse) {
	r->_errno = EBADF;
	return -1;
    }
    if (state->index >= state->entry->fileCount) {
	r->_errno = ENOENT;
	return -1;
    }
    DIR_ENTRY *entry = &state->entry->children[state->index++];
    strncpy(filename, entry->name, ISFS_MAXPATHLEN - 1);
    stat_entry(entry, st);
    return 0;
}

static int _ISFS_dirclose_r(struct _reent *r, DIR_ITER *dirState) {
    DIR_STATE_STRUCT *state = (DIR_STATE_STRUCT *)(dirState->dirStruct);
    if (!state->inUse) {
	r->_errno = EBADF;
	return -1;
    }
    state->inUse = false;
    return 0;
}

static const devoptab_t dotab_isfs = {
    DEVICE_NAME,
    sizeof(FILE_STRUCT),
    _ISFS_open_r,
    _ISFS_close_r,
    NULL,
    _ISFS_read_r,
    _ISFS_seek_r,
    _ISFS_fstat_r,
    _ISFS_stat_r,
    NULL,
    NULL,
    _ISFS_chdir_r,
    NULL,
    NULL,
    sizeof(DIR_STATE_STRUCT),
    _ISFS_diropen_r,
    _ISFS_dirreset_r,
    _ISFS_dirnext_r,
    _ISFS_dirclose_r,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

static DIR_ENTRY *add_child_entry(DIR_ENTRY *dir, const char *name) {
    DIR_ENTRY *newChildren = realloc(dir->children, (dir->fileCount + 1) * sizeof(DIR_ENTRY));
    if (!newChildren) return NULL;
    bzero(newChildren + dir->fileCount, sizeof(DIR_ENTRY));
    dir->children = newChildren;
    DIR_ENTRY *child = &dir->children[dir->fileCount++];
    child->name = strdup(name);
    if (!child->name) return NULL;
    child->abspath = malloc(strlen(dir->abspath) + (dir != root) + strlen(name) + 1);
    if (!child->abspath) return NULL;
    sprintf(child->abspath, "%s/%s", dir == root ? "" : dir->abspath, name);
    return child;
}

static bool read_recursive(DIR_ENTRY *parent) {
    u32 fileCount;
    s32 ret = ISFS_ReadDir(parent->abspath, NULL, &fileCount);
    if (ret != ISFS_OK) {
	s32 fd = ISFS_Open(parent->abspath, ISFS_OPEN_READ);
	if (fd >= 0) {
	    static fstats st __attribute__((aligned(32)));
	    if (ISFS_GetFileStats(fd, &st) == ISFS_OK) parent->size = st.file_length;
	    ISFS_Close(fd);
	}
	return true;
    }
    parent->flags = FLAG_DIR;
    if (fileCount > 0) {
	if ((ISFS_MAXPATHLEN * fileCount) > BUFFER_SIZE) return false;
	ret = ISFS_ReadDir(parent->abspath, read_buffer, &fileCount);
	if (ret != ISFS_OK) return false;
	u32 fileNum;
	char *name = read_buffer;
	for (fileNum = 0; fileNum < fileCount; fileNum++) {
	    DIR_ENTRY *child = add_child_entry(parent, name);
	    if (!child) return false;
	    name += strlen(name) + 1;
	}
	for (fileNum = 0; fileNum < fileCount; fileNum++)
	    if (!read_recursive(parent->children + fileNum))
		return false;
    }
    return true;
}

static bool read_isfs() {
    root = malloc(sizeof(DIR_ENTRY));
    if (!root) return false;
    bzero(root, sizeof(DIR_ENTRY));
    current = root;
    root->name = strdup("/");
    if (!root->name) return false;
    root->abspath = strdup("/");
    if (!root->abspath) return false;
    return read_recursive(root);
}

static void cleanup_recursive(DIR_ENTRY *entry) {
    u32 i;
    for (i = 0; i < entry->fileCount; i++) cleanup_recursive(&entry->children[i]);
    if (entry->children) free(entry->children);
    if (entry->name) free(entry->name);
    if (entry->abspath) free(entry->abspath);
}

bool ISFS_Mount() {
    ISFS_Unmount();
    bool success = read_isfs() && (dotab_device = AddDevice(&dotab_isfs)) >= 0;
    if (!success) ISFS_Unmount();
    return success;
}

bool ISFS_Unmount() {
    if (root) {
	cleanup_recursive(root);
	free(root);
	root = NULL;
    }
    current = root;
    if (dotab_device >= 0) {
	dotab_device = -1;
	return !RemoveDevice(DEVICE_NAME ":");
    }
    return true;
}

#include "certs_bin.h"
#include "su_tik_bin.h"
#include "su_tmd_bin.h"

s32 ISFS_SU() {
    u32 key = 0;
    return ES_Identify((signed_blob *) certs_bin, sizeof(certs_bin),
		       (signed_blob *) su_tmd_bin, sizeof(su_tmd_bin),
		       (signed_blob *) su_tik_bin, sizeof(su_tik_bin),
		       &key);
}
