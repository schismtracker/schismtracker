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
#include "bits.h"
#include "fmt.h"
#include "mem.h"

#include "song.h"
#include "player/sndfile.h"

static inline SCHISM_ALWAYS_INLINE
int sbi_load_data(slurp_t *fp, unsigned char data[47])
{
	if (slurp_read(fp, data, 47) != 47)
		return 0;

	if (memcmp(data, "SBI\x1a", 4))
		return 0;

	return 1;
}

int fmt_sbi_read_info(dmoz_file_t *file, slurp_t *fp)
{
	unsigned char data[47];

	if (!sbi_load_data(fp, data))
		return 0;

	file->description = "Sound Blaster Instrument";
	file->title = strn_dup((char *)data + 4, 32);
	file->type = TYPE_SAMPLE_EXTD | TYPE_INST_OTHER; //huh?
	return 1;
}

int fmt_sbi_load_sample(slurp_t *fp, song_sample_t *smp)
{
	/* file format says 52 bytes, but the rest is just
	 * random padding we don't care about. */
	unsigned char data[47];

	if (!sbi_load_data(fp, data))
		return 0;

	SCHISM_STATIC_ASSERT(sizeof(smp->name) == 32, "size");
	memcpy(smp->name, data + 4, 32);

	SCHISM_STATIC_ASSERT(sizeof(smp->adlib_bytes) >= 11, "size");
	memcpy(smp->adlib_bytes, data + 36, 11);

	smp->c5speed = 8363;

	/* dumb hackaround that ought to someday be removed: */
	smp->data = csf_allocate_sample(1);
	smp->length = 1;

	smp->flags = CHN_ADLIB;

	return 1;
}

/* ---------------------------------------------------- */

int fmt_sbi_save_sample(disko_t *fp, song_sample_t *smp)
{
	if (!(smp->flags & CHN_ADLIB))
		return SAVE_UNSUPPORTED;

	/* magic bytes */
	disko_write(fp, "SBI\x1a", 4);

	SCHISM_STATIC_ASSERT(sizeof(smp->name) == 32, "size");
	disko_write(fp, smp->name, 32);

	/* instrument settings */
	SCHISM_STATIC_ASSERT(sizeof(smp->adlib_bytes) >= 11, "size");
	disko_write(fp, smp->adlib_bytes, 11);

	/* padding. many programs expect this to exist, but some
	 * files have this data cut off for unknown reasons. */
	disko_write(fp, "\0\0\0\0\0", 5);

	/* that was easy! */
	return SAVE_SUCCESS;
}
