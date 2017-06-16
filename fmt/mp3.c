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

/* TODO: compile and test this... */

#include "headers.h"
#include "fmt.h"

#include <id3tag.h>

/* --------------------------------------------------------------------- */

static void get_title_from_id3(struct id3_tag const *tag, char **artist_ptr, char **title_ptr)
{
	struct id3_frame *frame;
	/*union id3_field *field;*/
	int n = -1;
	char *artist = NULL, *title = NULL, *buf;

	frame = id3_tag_findframe(tag, ID3_FRAME_ARTIST, 0);
	if (frame) {
		/* this should get all the strings, not just the zeroth -- use id3_field_getnstrings(field) */
		*artist_ptr = id3_ucs4_latin1duplicate(id3_field_getstrings(&frame->fields[1], 0));
	}

	frame = id3_tag_findframe(tag, ID3_FRAME_TITLE, 0);
	if (frame) {
		/* see above */
		*title_ptr = id3_ucs4_latin1duplicate(id3_field_getstrings(&frame->fields[1], 0));
	}
}

/* --------------------------------------------------------------------- */

int fmt_mp3_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
	signed long id3len;
	unsigned long id3off = 0;
	struct id3_tag *tag;
	/*int version = 2;*/

	id3len = id3_tag_query(data, length);
	if (id3len <= 0) {
		/*version = 1;*/
		if (length <= 128)
			return 0;

		id3off = length - 128;
		id3len = id3_tag_query(data + id3off, 128);
		if (id3len <= 0)
			/* See the note at the end of this file. */
			return 0;
	}

	tag = id3_tag_parse(data + id3off, id3len);
	if (tag) {
		get_title_from_id3(tag, &file->artist, &file->title);
		id3_tag_delete(tag);
	}
	/* Dunno what it means when id3_tag_parse barfs with a NULL tag, but I bet it's not a good
	thing. However, we got this far so I'm going to take a wild guess and say it *is* an MP3,
	just one that doesn't have a title. */

	/*file->extension = str_dup("mp3");*/
	/*file->description = mem_calloc(22, sizeof(char));*/
	/*snprintf(file->description, 22, "MPEG Layer 3, ID3 v%d", version);*/
	file->description = "MPEG Layer 3";
	file->type = TYPE_SAMPLE_COMPR;
	return 1;
}

/* The nonexistence of an ID3 tag does NOT necessarily mean the file isn't an MP3. Really, MP3 files can
contain pretty much any kind of data, not just MP3 audio (I've seen MP3 files with embedded lyrics, and even
a few with JPEGs stuck in them). Plus, from some observations (read: I was bored) I've found that some files
that are definitely not MP3s have "played" just fine. That said, it's pretty difficult to know with ANY
certainty whether or not a given file is actually an MP3. */
