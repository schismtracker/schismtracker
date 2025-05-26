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

/* This file, along with devoptab.c, has been retrieved from IOSUHAX,
 * and was fairly heavily edited from the original.
 *
 * The implementation, when it was imported, was unfinished; fsync
 * and ftruncate were not implemented. Since they easily map to FSA
 * functions, they were trivial to add.
 *
 * Additionally, it has been edited to use C99-like flexible array members,
 * rather than appending data to the end of the structure and then copying
 * the pointer to the start (dumb)
 *
 * The documentation has also been edited to match wut APIs.
 *   --paper */

#ifndef IOSUHAX_DEVOPTAB_H_
#define IOSUHAX_DEVOPTAB_H_

#include "headers.h"

//! virtual name example:   sd or odd (for sd:/ or odd:/ access)
//! fsaFd:                  fd received by FSAAddClient()
//! dev_path:               (optional) if a device should be mounted to the mount_path. If NULL, FSAMount is not executed.
//! mount_path:             path to map to virtual device name
//! mount_flag:             FSA_MOUNT_FLAG_LOCAL_MOUNT, FSA_MOUNT_FLAG_BIND_MOUNT, ...
int wiiu_mount_fs(const char *virt_name, FSAClientHandle fsaFd, const char *dev_path, const char *mount_path, int mount_flag);
int wiiu_unmount_fs(const char *virt_name);

#endif // IOSUHAX_DEVOPTAB_H_
