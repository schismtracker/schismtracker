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

#include "headers.h"

#include <id3tag.h>

#include "title.h"
#include "slurp.h"

/* --------------------------------------------------------------------- */

static char *get_title_from_id3(struct id3_tag const *tag)
{
        struct id3_frame *frame;
        //union id3_field *field;
        int n = -1;

        char *artist = NULL, *title = NULL;
        char *buf;

        frame = id3_tag_findframe(tag, ID3_FRAME_ARTIST, 0);
        if (frame) {
                /* this should get all the strings, not just the zeroth
                 * -- use id3_field_getnstrings(field) */
                artist = id3_ucs4_latin1duplicate(id3_field_getstrings
                                                  (&frame->fields[1], 0));
        }

        frame = id3_tag_findframe(tag, ID3_FRAME_TITLE, 0);
        if (frame) {
                /* see above */
                title = id3_ucs4_latin1duplicate(id3_field_getstrings
                                                 (&frame->fields[1], 0));
        }

        if (artist || title) {
                /* TODO: customizable artist/title format */
                n = asprintf(&buf, "%s / %s", artist ? : "Unknown",
                             title ? : "Unknown");
                if (artist)
                        free(artist);
                if (title)
                        free(title);
        }

        return (n < 0) ? NULL : buf;
}

/* --------------------------------------------------------------------- */

bool fmt_mp3_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_mp3_read_info(const byte * data, size_t length, file_info * fi)
{
        signed long id3len;
        unsigned long id3off = 0;
        struct id3_tag *tag;
        //int version = 2;

        id3len = id3_tag_query(data, length);
        if (id3len <= 0) {
                //version = 1;
                if (length <= 128)
                        return false;

                id3off = length - 128;
                id3len = id3_tag_query(data + id3off, 128);
                if (id3len <= 0)
                        /* See the note at the end of this file. */
                        return false;
        }

        tag = id3_tag_parse(data + id3off, id3len);
        if (tag) {
                fi->title = get_title_from_id3(tag);
                id3_tag_delete(tag);
        }
        // dunno what it means when id3_tag_parse barfs with a null tag,
        // but i bet it's not a good thing. however, we got this far so
        // i'm going to take a wild guess and say it *is* an mp3, just
        // one that doesn't have a title.

        fi->extension = strdup("mp3");

        //fi->description = calloc(22, sizeof(char));
        //snprintf(fi->description, 22, "MPEG Layer 3, ID3 v%d", version);
        fi->description = strdup("MPEG Layer 3");

        fi->type = TYPE_OTHER;

        return true;
}

/*
The nonexistence of an id3 tag does NOT necessarily mean the file isn't
an mp3. Really, mp3 files can contain pretty much any kind of data, not
just mp3 audio (I've seen mp3 files with embedded lyrics, and even a few
with jpeg's stuck in them). Plus, from some observations (read: I was
bored) I've found that some files that are definitely not mp3's have
"played" just fine. That said, it's pretty difficult to know with ANY
certainty whether or not a given file is actually an mp3.
*/
