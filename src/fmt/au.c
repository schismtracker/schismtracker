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

#define NEED_BYTESWAP
#include "headers.h"

#include "title.h"

/* --------------------------------------------------------------------- */

bool fmt_au_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_au_read_info(const byte * data, size_t length, file_info * fi)
{
	unsigned long data_offset, data_size;
	
        if (!(length > 24 && memcmp(data, ".snd", 4) == 0))
        	return false;

	memcpy(&data_offset, data + 4, 4);
	memcpy(&data_size, data + 8, 4);
	data_offset = bswapBE32(data_offset);
	data_size = bswapBE32(data_size);

	if (!(data_offset < length && data_size > 0 && data_size <= length - data_offset))
		return false;

	fi->description = strdup("AU Sample");
	fi->extension = strdup("au");
	if (data_offset > 24) {
		int extlen = data_offset - 24;

		fi->title = calloc(extlen + 1, sizeof(char));
		memcpy(fi->title, data + 24, extlen);
		fi->title[extlen] = 0;
	}
	fi->type = TYPE_SAMPLE;
	return true;
}
