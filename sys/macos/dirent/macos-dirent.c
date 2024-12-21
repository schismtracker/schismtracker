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

/* Based off code by Guido van Rossum in Python. */

#include "headers.h"
#include "str.h"
#include "dmoz.h"
#include "osdefs.h"
#include "charset.h"
#include "util.h"
#include "macos-dirent.h"

#include <errno.h>

#include <Files.h>
#include <TextUtils.h>

/* values for DIR */
struct dir_ {
	long dirid;
	int nextfile;

	struct dirent dir;
};

/* Open a directory.  This means calling PBOpenWD. */
DIR *opendir(const char *path)
{
	/* Schism never opens more than one directory at a time,
	 * so we can just one static directory structure. */
	static DIR dir;

	if (dir.nextfile) {
		errno = ENFILE;
		return NULL;
	}

	OSErr err = noErr;
	WDPBRec pb;
	unsigned char ppath[256];
	FSSpec spec;

	{
		// We can just pass the full path to PBOpenWD
		int truncated;
		str_to_pascal(path, ppath, &truncated);
		if (truncated) {
			errno = ENAMETOOLONG;
			return NULL;
		}

		// Append a separator on the end if one isn't there already; I don't
		// know if this is strictly necessary, but every macos path I've seen
		// that goes to a folder has an explicit path separator on the end.
		if (ppath[ppath[0]] != ':') {
			if (ppath[0] >= 255) {
				errno = ENAMETOOLONG;
				return NULL;
			}
			ppath[++ppath[0]] = ':';
		}

		pb.ioNamePtr  = ppath;
		pb.ioVRefNum  = 0;
		pb.ioWDProcID = 0;
		pb.ioWDDirID  = 0;
	}

	err = PBOpenWD(&pb, 0);
	switch (err) {
	case noErr:
		break;
	case nsvErr:
	case fnfErr:
		errno = ENOENT;
		return NULL;
	case tmwdoErr:
		errno = ENFILE;
		return NULL;
	case afpAccessDenied:
		errno = EACCES;
		return NULL;
	default:
		return NULL;
	}

	dir.dirid    = pb.ioVRefNum;
	dir.nextfile = 1;

	return &dir;
}

/* Close an open directory. */
void closedir(DIR *dirp)
{
	WDPBRec pb;
	
	pb.ioVRefNum = dirp->dirid;
	(void) PBCloseWD(&pb, 0);
	dirp->dirid = 0;
	dirp->nextfile = 0;
}

/* Read the next directory entry. */
struct dirent *readdir(DIR *dp)
{
	union {
		DirInfo d;
		FileParam f;
		HFileInfo hf;
	} pb;
	unsigned char pname[256];
	pname[0] = '\0';

	pb.d.ioNamePtr = pname;
	pb.d.ioVRefNum = dp->dirid;
	pb.d.ioDrDirID = 0;
	pb.d.ioFDirIndex = dp->nextfile++;

	short err = PBGetCatInfo((CInfoPBPtr)&pb, 0);
	switch (err) {
	case noErr:
		break;
	case nsvErr:
	case fnfErr:
		errno = ENOENT;
		return NULL;
	case ioErr:
		errno = EIO;
		return NULL;
	case bdNamErr:
		errno = EILSEQ;
		return NULL;
	case paramErr:
	case dirNFErr:
	case afpObjectTypeErr:
		errno = ENOTDIR;
		return NULL;
	case afpAccessDenied:
		errno = EACCES;
		return NULL;
	default:
		return NULL;
	}

	str_from_pascal(pname, dp->dir.d_name);

	return &dp->dir;
}