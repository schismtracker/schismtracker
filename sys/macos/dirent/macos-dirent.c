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

static int opendir_find_volume(const char *name, WDPBRec *pb)
{
	for (ItemCount volIndex = 1; ; ++volIndex) {
		unsigned char ppath[256];
		ppath[0] = 0;

		HParamBlockRec hfsParams = {
			.volumeParam = {
				.ioNamePtr = ppath,
				.ioVRefNum = 0,
				.ioVolIndex = volIndex,
			},
		};

		if (PBHGetVInfoSync(&hfsParams) != noErr)
			break;

		// FIXME FIXME! READ ALL ABOUT IT!
		// HFS paths are not in codepage 437. The only reason it's like this here
		// is because we need a simple mostly-ASCII-compatible 8-bit fixed width
		// encoding, which codepage 437 just happens to be. We'll probably have
		// to hack in HFS filename conversion to charset_iconv to get this to
		// function correctly.

		if (charset_strncasecmp(&ppath[1], CHARSET_CP437, name, CHARSET_UTF8, ppath[0]))
			continue;

		// we found our volume: fill in the spec
		pb->ioVRefNum = hfsParams.volumeParam.ioVRefNum;
		pb->ioWDDirID = fsRtDirID;
		return 1;
	}

	return 0;
}

/* Open a directory.  This means calling PBOpenWD. */
DIR *opendir(const char *path)
{
	/* Schism never opens more than one directory at a time,
	 * so we can just one static directory structure. */
	static DIR dir;

	OSErr err = noErr;
	WDPBRec pb;
	unsigned char ppath[256];
	FSSpec spec;

	if (!strchr(path, ':')) {
		// A volume name.
		if (!opendir_find_volume(path, &pb)) {
			errno = ENOENT;
			return NULL;
		}
	} else {
		// We can just pass the full path to PBOpenWD
		int truncated;
		str_to_pascal(path, ppath, &truncated);
		if (truncated || (ppath[0] >= 255)) {
			errno = ENAMETOOLONG;
			return NULL;
		}
		pb.ioNamePtr  = ppath;
		pb.ioVRefNum  = 0;
		pb.ioWDProcID = 0;
		pb.ioWDDirID  = 0;
	}

	err = PBOpenWD(&pb, 0);
	if (err != noErr) {
		errno = ENOENT;
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
	if (err != noErr) {
		errno = EIO;
		return NULL;
	}

	str_from_pascal(pname, dp->dir.d_name);

	return &dp->dir;
}