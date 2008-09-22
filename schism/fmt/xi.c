/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2008 Mrs. Brisby <mrs.brisby@nimh.org>
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
#define NEED_BYTESWAP

#include "headers.h"
#include "fmt.h"

#include "it.h"
#include "song.h"

#include <string.h>
#include <stdlib.h>

/* --------------------------------------------------------------------- */
int fmt_xi_read_info(dmoz_file_t *file, const byte *data, size_t length)
{
	if (length <= 86) return false;
	if (memcmp(data, "Extended Instrument: ", 21) != 0) return false;
	if (data[43] != 26) return false;
	if (data[42] != 0) return false;
	if (memcmp(data+44, "FastTracker v", 13) != 0) return false;
	file->description = "FastTracker Instrument";
	file->title = strdup(((char*)data)+21);
	file->type = TYPE_INST_XI;
	return true;
}


int fmt_xi_load_instrument(const byte *data, size_t length, int slot)
{
	if (length <= 86) return false;
	if (memcmp(data, "Extended Instrument: ", 21) != 0) return false;
	if (!slot) return false;
	/* TODO actually implement it :) */
	return false;
}

