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

/* FIXME this file is redundant and needs to go away -- s3m.c already does all of this */

#include "headers.h"
#include "bswap.h"
#include "fmt.h"

#include "song.h"
#include "player/sndfile.h"

enum {
	S3I_TYPE_PCM   = 1,
	S3I_TYPE_ADLIB = 2,
};

enum {
	S3I_PCM_FLAG_LOOP   = 0x01,
	S3I_PCM_FLAG_STEREO = 0x02,
	S3I_PCM_FLAG_16BIT  = 0x04,
};

/* XXX maybe this should be in fmt.h */
#define READ_UINT(var, endian, bits, data, offset) \
	do { \
		/* ensure we can represent the entire number safely */ \
		SCHISM_STATIC_ASSERT(sizeof(var) >= sizeof(uint ## bits ## _t), "mismatched read size"); \
	\
		uint ## bits ## _t x; \
		memcpy(&x, (data) + (offset), sizeof(x)); \
		(var) = bswap ## endian ## bits(x); \
	} while (0)

/* nonzero on success */
static int load_s3i_sample(const uint8_t *data, size_t length, song_sample_t *smp, int with_data)
{
	if (length < 0x50)
		return 0;

	/* verify signature; offset 0x4C */
	if (memcmp(data + 0x4C, "SCRS", 4) != 0
		&& memcmp(data + 0x4C, "SCRI", 4) != 0)
		return 0;

	uint8_t type = data[0x00];
	if (type != S3I_TYPE_PCM && type != S3I_TYPE_ADLIB)
		return 0;

	uint8_t flags = data[0x1F];

	smp->flags = 0;
	smp->global_volume = 64;
	smp->volume = ((uint_fast16_t)data[0x1C]) * 4; /* mphack */
	READ_UINT(smp->loop_start, LE, 32, data, 0x14);
	READ_UINT(smp->loop_end, LE, 32, data, 0x18);
	READ_UINT(smp->c5speed, LE, 32, data, 0x20);

	memcpy(smp->filename, data + 0x01, sizeof(smp->filename));
	memcpy(smp->name, data + 0x30, sizeof(smp->name));

	if (type == S3I_TYPE_PCM) {
		/* PCM audio */
		int bytes_per_sample = (flags & S3I_PCM_FLAG_STEREO) ? 2 : 1;

		READ_UINT(smp->length, LE, 32, data, 0x10);

		if (length < 0x50 + smp->length * bytes_per_sample)
			return 0;

		/* convert flags */
		if (flags & S3I_PCM_FLAG_LOOP)
			smp->flags |= CHN_LOOP;

		if (flags & S3I_PCM_FLAG_STEREO)
			smp->flags |= CHN_STEREO;

		if (flags & S3I_PCM_FLAG_16BIT)
			smp->flags |= CHN_16BIT;

		if (with_data) {
			int format = SF_M | SF_LE; // endianness; channels
			format |= (smp->flags & CHN_16BIT) ? (SF_16 | SF_PCMS) : (SF_8 | SF_PCMU); // bits; encoding

			csf_read_sample((song_sample_t *) smp, format,
				(const char *)(data + 0x50), (uint32_t)(length - 0x50));
		}
	} else if (type == S3I_TYPE_ADLIB) {
		/* AdLib */
		smp->flags |= CHN_ADLIB;
		smp->flags &= ~(CHN_LOOP|CHN_16BIT);

		memcpy(smp->adlib_bytes, data + 0x10, 11);

		smp->length = 1;
		smp->loop_start = 0;
		smp->loop_end = 0;

		smp->data = csf_allocate_sample(1);
	}

	return 1;
}

/* FIXME update this stuff to not use fp->data */
int fmt_s3i_read_info(dmoz_file_t *file, slurp_t *fp)
{
	song_sample_t smp;
	if (!load_s3i_sample(fp->data, fp->length, &smp, 0))
		return 0;

	file->smp_length = smp.length;
	file->smp_flags = smp.flags;
	file->smp_defvol = smp.volume;
	file->smp_gblvol = smp.global_volume;
	file->smp_loop_start = smp.loop_start;
	file->smp_loop_end = smp.loop_end;
	file->smp_speed = smp.c5speed;
	file->smp_filename = strn_dup(smp.filename, 12);

	file->description = "Scream Tracker Sample";
	file->title = strn_dup(smp.name, 25);
	file->type = TYPE_SAMPLE_EXTD | TYPE_INST_OTHER;
	return 1;
}

int fmt_s3i_load_sample(slurp_t *fp, song_sample_t *smp)
{
	// what the crap?
	return load_s3i_sample(fp->data, fp->length, smp, 1);
}
