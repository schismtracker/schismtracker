/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
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

#ifndef TITLE_H
#define TITLE_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>

#include <stdio.h>
#include <unistd.h>

#include "util.h"


/* These roughly correspond to what Modplug would save a file loaded with
 * a given type as. Not quite like Impulse Tracker does it, but maybe a
 * bit more informative... if ya know what the colors mean, that is ;)
 * 
 * This isn't completely accurate, though, partly to keep a few types
 * lined up with how IT displays 'em, and partly to make the directory
 * list more colorful. ^_^ */
enum file_type {
	/* for internal use only (file_info_get returns FINF_UNSUPPORTED and doesn't fill in the structure,
	so no other functions will see TYPE_UNKNOWN) */
	TYPE_UNKNOWN = (0),
	TYPE_IT,
	TYPE_S3M,
	TYPE_XM,
	TYPE_MOD,
	TYPE_OTHER,		/* for non-tracked stuff */
	TYPE_SAMPLE = (6),	/* drawn in color 3 in IT */
};

enum {
	FINF_SUCCESS = (0),	/* nothing wrong */
	FINF_UNSUPPORTED = (1),	/* unsupported file type */
	FINF_EMPTY = (2),	/* zero-byte-long file */
	FINF_ERRNO = (-1),	/* check errno */
};

typedef struct {
        char *description;	/* "Impulse Tracker" */
        char *extension;	/* "it" (the *expected* extension) */
        char *title;		/* the song title field, or NULL if none */
        enum file_type type;	/* for filename colors */
} file_info;

/* buf can be NULL; it's just for if you've already stat'ed the file.
all of the file_info fields, as well as the file_info pointer itself,
should be free'd by the caller.
if this function succeeds, 
return status: one of the FINF_ codes (0 = success)
short usage example:
	file_info *fi;
	switch (file_info_get(filename, NULL, &fi)) {
	case FINF_SUCCESS:
		printf("title: %s\n", fi->title);
		free(fi);
		break;
	case FINF_ERRNO:
		perror(filename);
		break;
	default:
		printf("unsupported file type\n");
	}
*/
int file_info_get(const char *filename, struct stat *buf, file_info **fi);

#endif /* ! TITLE_H */
