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

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
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
        TYPE_UNKNOWN = (0),     /* shouldn't ever come up */
        TYPE_IT = (1),
        TYPE_S3M = (2),
        TYPE_XM = (3),
        TYPE_MOD = (4),
        TYPE_OTHER = (5),       /* for non-tracked stuff */
};

typedef struct {
        char *description;      /* "Impulse Tracker" */
        char *extension;        /* "it" (the *expected* extension) */
        char *title;    /* the song title field, or NULL if none */
        enum file_type type;    /* for filename colors */
} file_info;

/* buf can be NULL; it's just for if you've already stat'ed the file.
 * 
 * all of the file_info fields, as well as the file_info pointer itself,
 * should be free'd by the caller. */
file_info *file_info_get(const char *filename, struct stat *buf);

#endif /* ! TITLE_H */
