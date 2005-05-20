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

#include "headers.h"
#include "fmt.h"

/* --------------------------------------------------------------------- */

/* does IT's raw sample loader use signed or unsigned samples? */

bool fmt_raw_load_sample(const byte *data, size_t length, song_sample *smp, UNUSED char *title)
{
	smp->speed = 8363;
	smp->volume = 64 * 4;
	smp->global_volume = 64;
	
	/* log_appendf(2, "Loading as raw."); */
	
	smp->data = song_sample_allocate(length);
	memcpy(smp->data, data, length);
	smp->length = length;
	
	return true;
}

bool fmt_raw_save_sample(FILE *fp, song_sample *smp, UNUSED char *title)
{
	fwrite(smp->data, (smp->flags & SAMP_16_BIT) ? 2 : 1, smp->length, fp);
	return true;
}
