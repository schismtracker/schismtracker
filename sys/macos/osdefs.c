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

#include "it.h"
#include "osdefs.h"
#include "events.h"
#include "song.h"
#include "page.h"
#include "widget.h"
#include "dmoz.h"
#include "errno.h"

#include <Files.h>
#include <Folders.h>
#include <Dialogs.h>

/* ------------------------------------------------------------------------ */

void macos_show_message_box(const char *title, const char *text)
{
	unsigned char err[256], explanation[256];
	int16_t hit;

	str_to_pascal(title, err, NULL);
	str_to_pascal(text, explanation, NULL);

	StandardAlert(kAlertDefaultOKText, err, explanation, NULL, &hit);
}

/* ------------------------------------------------------------------------ */

int macos_mkdir(const char *path, mode_t mode)
{
	// This won't work if a path contains ..
	// or some crap, but I don't care. We don't
	// pass anything like that to this function
	// anyway.
	HParamBlockRec pb = {0};
	unsigned char mpath[256]; // Str255 ?
	int truncated;

	str_to_pascal(path, mpath, &truncated);

	if (truncated) {
		errno = ENAMETOOLONG;
		return -1;
	}

	pb.fileParam.ioNamePtr = mpath;

	return (PBDirCreateSync(&pb) == noErr) ? 0 : -1;
}

int macos_stat(const char *file, struct stat *st)
{
	CInfoPBRec pb = {0};
	unsigned char path[256];
	int result = 0;

	const size_t len = strlen(file);
	if (len > 255)
		return -1; // whaaat?

	// stupid pascal string
	path[0] = len;
	memcpy(&path[1], file, len);

	if (strcmp(file, ".") == 0) {
		*st = (struct stat){
			.st_mode = S_IFDIR,
			.st_ino = -1,
		};
	} else {		
		pb.hFileInfo.ioNamePtr = path;

		if (PBGetCatInfoSync(&pb) != noErr) {
			result = -1;
		} else {
			memset(st, 0, sizeof(struct stat));

			st->st_mode = (pb.hFileInfo.ioFlAttrib & ioDirMask) ? S_IFDIR : S_IFREG;
			st->st_ino = pb.hFileInfo.ioFlStBlk;
			st->st_dev = pb.hFileInfo.ioVRefNum;
			st->st_nlink = 1;
			st->st_size = pb.hFileInfo.ioFlLgLen;
			st->st_atime = pb.hFileInfo.ioFlMdDat;
			st->st_mtime = pb.hFileInfo.ioFlMdDat;
			st->st_ctime = pb.hFileInfo.ioFlCrDat;
		}
	}

	return result;
}
