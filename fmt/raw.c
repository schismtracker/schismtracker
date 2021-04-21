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
#include "fmt.h"

#include <errno.h>

/* --------------------------------------------------------------------- */

// Impulse Tracker handles raw sample data as unsigned, EXCEPT when saving a 16-bit sample as raw.

int fmt_raw_load_sample(const uint8_t *data, size_t length, song_sample_t *smp)
{
	/* we'll uphold IT's limit of 4mb */
	length = MIN(length, 4 * 1048576);

	smp->c5speed = 8363;
	smp->volume = 64 * 4;
	smp->global_volume = 64;
	smp->length = length;
	csf_read_sample(smp, SF_LE | SF_8 | SF_PCMU | SF_M, data, length);

	return 1;
}

int fmt_raw_save_sample(disko_t *fp, song_sample_t *smp)
{
	csf_write_sample(fp, smp, SF_LE
		| ((smp->flags & CHN_16BIT) ? SF_16 | SF_PCMS : SF_8 | SF_PCMU)
		| ((smp->flags & CHN_STEREO) ? SF_SI : SF_M),
		UINT32_MAX);
	return SAVE_SUCCESS;
}

