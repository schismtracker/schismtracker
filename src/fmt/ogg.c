/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * URL: http://rigelseven.com/schism/
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

/* This is way more work than it ought to be... */

#include "headers.h"

#include "title.h"

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

/* --------------------------------------------------------------------- */

struct sheep {
        const byte *data;
        size_t length;
        size_t position;
};

/* --------------------------------------------------------------------- */

static size_t fake_read(void *buf, size_t size, size_t nmemb,
                        void *void_data)
{
        struct sheep *file_data = (struct sheep *) void_data;

        off_t length_left = file_data->length - file_data->position;
        off_t read_size = nmemb * size;

        if (read_size > length_left) {
                nmemb = length_left / size;
                read_size = nmemb * size;
        }
        if (nmemb > 0) {
                memcpy(buf, file_data->data + file_data->position,
                       read_size);
                file_data->position += read_size;
        }
        return nmemb;
}

static int fake_seek(void *void_data, ogg_int64_t offset, int whence)
{
        struct sheep *file_data = (struct sheep *) void_data;

        switch (whence) {
        case SEEK_SET:
                break;
        case SEEK_CUR:
                offset += file_data->position;
                break;
        case SEEK_END:
                offset += file_data->length;
                break;
        default:
                return -1;
        }
        if (offset < 0 || offset > file_data->length)
                return -1;
        file_data->position = offset;
        return 0;
}

static int fake_close(UNUSED void *void_data)
{
        return 0;
}

static long fake_tell(void *void_data)
{
        struct sheep *file_data = (struct sheep *) void_data;

        return file_data->position;
}

/* --------------------------------------------------------------------- */

static char *get_title_from_ogg(OggVorbis_File * vf)
{
        char *buf;
        char *key, *value;
        char *artist = NULL, *title = NULL;
        char **ptr = ov_comment(vf, -1)->user_comments;
        int n = -1;

        while (*ptr) {
                key = strdup(*ptr);
                value = strchr(key, '=');
                if (value == NULL) {
                        // buh?
                        free(key);
                        continue;
                }
                // hack? where?
                *value = 0;
                value = strdup(value + 1);

                if (strcmp(key, "artist") == 0)
                        artist = value;
                else if (strcmp(key, "title") == 0)
                        title = value;
                else
                        free(value);
                free(key);
                ptr++;
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

bool fmt_ogg_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_ogg_read_info(const byte * data, size_t length, file_info * fi)
{
        OggVorbis_File vf;
        ov_callbacks cb;
        struct sheep file_data;

        cb.read_func = fake_read;
        cb.seek_func = fake_seek;
        cb.close_func = fake_close;
        cb.tell_func = fake_tell;

        file_data.data = data;
        file_data.length = length;
        file_data.position = 0;

        if (ov_open_callbacks(&file_data, &vf, NULL, 0, cb) < 0)
                return false;

        // song_length = ov_time_total(&vf, -1);

        fi->title = get_title_from_ogg(&vf);
        fi->description = strdup("Ogg Vorbis");
        fi->extension = strdup("ogg");
        fi->type = TYPE_OTHER;

        ov_clear(&vf);

        return true;
}
