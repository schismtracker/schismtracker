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

/* This is way more work than it ought to be... */

/* TODO: test this again since I rearranged the artist/title stuff.
Can't be bothered actually compiling it now to make sure it works. :P */

/* read info and load sample arguments have changed too, btw. this
 * will not compile and I honestly don't even know how to compile
 * it anyway. I'll probably remove this file... */

#include "headers.h"
#include "fmt.h"

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

/* --------------------------------------------------------------------- */

struct sheep {
	const uint8_t *data;
	size_t length;
	size_t position;
};

/* --------------------------------------------------------------------- */

static size_t fake_read(void *buf, size_t size, size_t nmemb, void *void_data)
{
	struct sheep *file_data = (struct sheep *) void_data;

	off_t length_left = file_data->length - file_data->position;
	off_t read_size = nmemb * size;

	if (read_size > length_left) {
		nmemb = length_left / size;
		read_size = nmemb * size;
	}
	if (nmemb > 0) {
		memcpy(buf, file_data->data + file_data->position, read_size);
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

static int fake_close(SCHISM_UNUSED void *void_data)
{
	return 0;
}

static long fake_tell(void *void_data)
{
	struct sheep *file_data = (struct sheep *) void_data;

	return file_data->position;
}

/* --------------------------------------------------------------------- */

static void get_title_from_ogg(OggVorbis_File * vf, char **artist_ptr, char **title_ptr)
{
	char *buf, *key, *value;
	char **ptr = ov_comment(vf, -1)->user_comments;
	int n = -1;

	while (*ptr) {
		key = str_dup(*ptr);
		value = strchr(key, '=');
		if (value == NULL) {
			/* buh? */
			free(key);
			continue;
		}
		/* hack? where? */
		*value = 0;
		value = str_dup(value + 1);

		if (strcmp(key, "artist") == 0)
			*artist_ptr = value;
		else if (strcmp(key, "title") == 0)
			*title_ptr = value;
		else
			free(value);
		free(key);
		ptr++;
	}
}

/* --------------------------------------------------------------------- */

int fmt_ogg_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
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
		return 0;

	/* song_length = ov_time_total(&vf, -1); */

	get_title_from_ogg(&vf, &file->artist, &file->title);
	file->description = "Ogg Vorbis";
	/*file->extension = str_dup("ogg");*/
	file->type = TYPE_SAMPLE_COMPR;

	ov_clear(&vf);

	return 1;
}
