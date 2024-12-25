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
#include "bswap.h"
#include "fmt.h"
#include "mem.h"

#include "song.h"
#include "player/sndfile.h"

enum {
	S3I_TYPE_NONE = 0,
	S3I_TYPE_PCM = 1,
	S3I_TYPE_ADLIB = 2,
};

enum {
	S3I_PCM_FLAG_LOOP   = 0x01,
	S3I_PCM_FLAG_STEREO = 0x02,
	S3I_PCM_FLAG_16BIT  = 0x04,
};

/* TODO: s3m.c should use this */
static int load_s3i_sample(slurp_t *fp, song_sample_t *smp, int with_data)
{
	unsigned char magic[4];
	uint32_t dw;

	if (slurp_length(fp) < 0x50)
		return 0;

	slurp_seek(fp, 0x4C, SEEK_SET);
	if (slurp_read(fp, magic, sizeof(magic)) != sizeof(magic))
		return 0;

	if (memcmp(magic, "SCRS", 4) && memcmp(magic, "SCRI", 4))
		return 0;

	slurp_seek(fp, 0x00, SEEK_SET);
	int type = slurp_getc(fp);
	if (type != S3I_TYPE_PCM && type != S3I_TYPE_ADLIB)
		return 0;

	slurp_seek(fp, 0x1F, SEEK_SET);
	int flags = slurp_getc(fp);
	if (flags < 0)
		return 0;

	slurp_seek(fp, 0x1C, SEEK_SET);
	smp->flags = 0;
	smp->global_volume = 64;
	smp->volume = slurp_getc(fp) * 4; /* mphack */

	slurp_seek(fp, 0x14, SEEK_SET);

	if (slurp_read(fp, &dw, sizeof(dw)) != sizeof(dw))
		return 0;
	smp->loop_start = bswapLE32(dw);

	if (slurp_read(fp, &dw, sizeof(dw)) != sizeof(dw))
		return 0;
	smp->loop_end = bswapLE32(dw);

	slurp_seek(fp, 4, SEEK_CUR);

	if (slurp_read(fp, &dw, sizeof(dw)) != sizeof(dw))
		return 0;
	smp->c5speed = bswapLE32(dw);

	slurp_seek(fp, 0x01, SEEK_SET);
	slurp_read(fp, smp->filename, MIN(sizeof(smp->filename) - 1, 12));

	slurp_seek(fp, 0x30, SEEK_SET);
	slurp_read(fp, smp->name, MIN(sizeof(smp->name) - 1, 28));

	if (type == S3I_TYPE_PCM) {
		int bytes_per_sample = (flags & S3I_PCM_FLAG_STEREO) ? 2 : 1;

		slurp_seek(fp, 0x10, SEEK_SET);
		if (slurp_read(fp, &dw, sizeof(dw)) != sizeof(dw))
			return 0;
		smp->length = bswapLE32(dw);

		if (slurp_length(fp) < 0x50 + smp->length * bytes_per_sample)
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

			slurp_seek(fp, 0x50, SEEK_SET);
			csf_read_sample(smp, format, fp);
		}
	} else if (type == S3I_TYPE_ADLIB) {
		smp->flags |= CHN_ADLIB;
		smp->flags &= ~(CHN_LOOP|CHN_16BIT);

		slurp_seek(fp, 0x10, SEEK_SET);
		if (slurp_read(fp, smp->adlib_bytes, sizeof(smp->adlib_bytes)) != sizeof(smp->adlib_bytes))
			return 0;

		// dumb hackaround that ought to some day be fixed:
		smp->length = 1;
		smp->data = csf_allocate_sample(1);
	}

	return 1;
}

int fmt_s3i_read_info(dmoz_file_t *file, slurp_t *fp)
{
	song_sample_t smp;
	if (!load_s3i_sample(fp, &smp, 0))
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
	return load_s3i_sample(fp, smp, 1);
}

/* ---------------------------------------------------- */

struct s3i_header {
	uint8_t type;
	char filename[12];
	union {
		struct {
			uint8_t memseg[3];
			uint32_t length;
			uint32_t loop_start;
			uint32_t loop_end;
		} pcm;
		struct {
			//uint8_t zero[3];
			uint8_t data[12];
		} admel;
	} spec;
	uint8_t vol;
	uint8_t x; // "dsk" for adlib
	uint8_t pack; // 0
	uint8_t flags; // 1=loop 2=stereo 4=16-bit / zero for adlib
	uint32_t c5speed;
	uint8_t junk[12];
	char name[28];
	char tag[4]; // SCRS/SCRI/whatever
};

void s3i_write_header(disko_t *fp, song_sample_t *smp, uint32_t sdata)
{
	struct s3i_header hdr = {0};
	int n;

	if (smp->flags & CHN_ADLIB) {
		hdr.type = S3I_TYPE_ADLIB;
		memcpy(hdr.spec.admel.data, smp->adlib_bytes, 11);
		memcpy(hdr.tag, "SCRI", 4);
	} else if (smp->data != NULL) {
		hdr.type = S3I_TYPE_PCM;
		hdr.spec.pcm.memseg[0] = (sdata >> 20) & 0xff;
		hdr.spec.pcm.memseg[1] = (sdata >> 4) & 0xff;
		hdr.spec.pcm.memseg[2] = (sdata >> 12) & 0xff;
		hdr.spec.pcm.length = bswapLE32(smp->length);
		hdr.spec.pcm.loop_start = bswapLE32(smp->loop_start);
		hdr.spec.pcm.loop_end = bswapLE32(smp->loop_end);
		hdr.flags = ((smp->flags & CHN_LOOP) ? 1 : 0)
			| ((smp->flags & CHN_STEREO) ? 2 : 0)
			| ((smp->flags & CHN_16BIT) ? 4 : 0);
		memcpy(hdr.tag, "SCRS", 4);
	} else {
		hdr.type = S3I_TYPE_NONE;
	}

	memcpy(hdr.filename, smp->filename, 12);
	hdr.vol = smp->volume / 4; //mphack
	hdr.c5speed = bswapLE32(smp->c5speed);

	for (n = 25; n >= 0; n--)
		if ((smp->name[n] ? smp->name[n] : 32) != 32)
			break;
	for (; n >= 0; n--)
		hdr.name[n] = smp->name[n] ? smp->name[n] : 32;

#define WRITE_VALUE(x) do { disko_write(fp, &hdr.x, sizeof(hdr.x)); } while (0)

	WRITE_VALUE(type);
	WRITE_VALUE(filename);

	switch (hdr.type) {
	case S3I_TYPE_ADLIB:
		disko_seek(fp, 3, SEEK_CUR);
		WRITE_VALUE(spec.admel.data);
		break;
	case S3I_TYPE_PCM:
		WRITE_VALUE(spec.pcm.memseg);
		WRITE_VALUE(spec.pcm.length);
		WRITE_VALUE(spec.pcm.loop_start);
		WRITE_VALUE(spec.pcm.loop_end);
		break;
	default:
	case S3I_TYPE_NONE:
		disko_seek(fp, 15, SEEK_CUR);
		break;
	}

	WRITE_VALUE(vol);
	WRITE_VALUE(x);
	WRITE_VALUE(pack);
	WRITE_VALUE(flags);
	WRITE_VALUE(c5speed);
	disko_seek(fp, 12, SEEK_CUR);
	WRITE_VALUE(name);
	WRITE_VALUE(tag); // SCRS/SCRI/whatever

#undef WRITE_VALUE
}

int fmt_s3i_save_sample(disko_t *fp, song_sample_t *smp)
{
	s3i_write_header(fp, smp, 0);

	if (smp->flags & CHN_ADLIB) {
		return SAVE_SUCCESS; // already done
	} else if (smp->data != NULL) {
		uint32_t format = SF_M | SF_LE; // endianness; channels
		format |= (smp->flags & CHN_16BIT) ? (SF_16 | SF_PCMS) : (SF_8 | SF_PCMU); // bits; encoding

		csf_write_sample(fp, smp, format, UINT32_MAX);
	}

	return SAVE_SUCCESS;
}
