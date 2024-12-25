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

enum {
	AU_ULAW = 1,                    /* µ-law */
	AU_PCM_8 = 2,                   /* 8-bit linear PCM (RS_PCM8U in Modplug) */
	AU_PCM_16 = 3,                  /* 16-bit linear PCM (RS_PCM16M) */
	AU_PCM_24 = 4,                  /* 24-bit linear PCM */
	AU_PCM_32 = 5,                  /* 32-bit linear PCM */
	AU_IEEE_32 = 6,                 /* 32-bit IEEE floating point */
	AU_IEEE_64 = 7,                 /* 64-bit IEEE floating point */
	AU_ISDN_ULAW_ADPCM = 23,        /* 8-bit ISDN µ-law (CCITT G.721 ADPCM compressed) */
};

struct au_header {
	char magic[4]; /* ".snd" */
	uint32_t data_offset, data_size, encoding, sample_rate, channels;
};

static int read_header_au(struct au_header *hdr, slurp_t *fp)
{
#define READ_VALUE(name) \
	if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) return 0

	READ_VALUE(magic);
	READ_VALUE(data_offset);
	READ_VALUE(data_size);
	READ_VALUE(encoding);
	READ_VALUE(sample_rate);
	READ_VALUE(channels);

#undef READ_VALUE

	if (memcmp(hdr->magic, ".snd", sizeof(hdr->magic)))
		return 0;

	/* now byteswap */
	hdr->data_offset = bswapBE32(hdr->data_offset);
	hdr->data_size = bswapBE32(hdr->data_size);
	hdr->encoding = bswapBE32(hdr->encoding);
	hdr->sample_rate = bswapBE32(hdr->sample_rate);
	hdr->channels = bswapBE32(hdr->channels);

	const size_t memsize = slurp_length(fp);

	if (hdr->data_offset < 24
		|| hdr->data_offset > memsize
		|| hdr->data_size > memsize - hdr->data_offset)
		return 0;

	switch (hdr->channels) {
	case 1:
	case 2:
		break;
	default:
		return 0;
	}

	return 1;
}

/* --------------------------------------------------------------------- */

int fmt_au_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct au_header au;

	if (!read_header_au(&au, fp))
		return 0;

	/* calculate length and flags */
	file->smp_length = au.data_size / au.channels;
	file->smp_flags = 0;
	switch (au.encoding) {
	case AU_PCM_16:
		file->smp_flags |= CHN_16BIT;
		file->smp_length /= 2;
		break;
	case AU_PCM_24:
		file->smp_flags |= CHN_16BIT;
		file->smp_length /= 3;
		break;
	case AU_PCM_32:
	case AU_IEEE_32:
		file->smp_flags |= CHN_16BIT;
		file->smp_length /= 4;
		break;
	case AU_IEEE_64:
		file->smp_flags |= CHN_16BIT;
		file->smp_length /= 8;
		break;
	default:
		return 0;
	}

	if (au.channels == 2)
		file->smp_flags |= CHN_STEREO;

	file->smp_speed = au.sample_rate;
	file->smp_defvol = 64;
	file->smp_gblvol = 64;
	file->description = "AU Sample";
	file->smp_filename = file->base;
	file->type = TYPE_SAMPLE_PLAIN;

	/* now we can grab the title */
	if (au.data_offset > 24) {
		size_t extlen = au.data_offset - 24;

		slurp_seek(fp, 24, SEEK_SET);

		char *title = mem_alloc(extlen + 1);
		if (slurp_read(fp, title, extlen) != extlen)
			return 0;
		title[extlen] = '\0';

		file->title = title;
	}

	return 1;
}

/* --------------------------------------------------------------------- */

int fmt_au_load_sample(slurp_t *fp, song_sample_t *smp)
{
	struct au_header au;
	uint32_t sflags = SF_BE;

	if (!read_header_au(&au, fp))
		return 0;

	smp->c5speed = au.sample_rate;
	smp->volume = 64 * 4;
	smp->global_volume = 64;
	smp->length = au.data_size;

	switch (au.encoding) {
	case AU_PCM_8:
		sflags |= SF_8 | SF_PCMS;
		break;
	case AU_PCM_16:
		sflags |= SF_16 | SF_PCMS;
		smp->length /= 2;
		break;
	case AU_PCM_24:
		sflags |= SF_24 | SF_PCMS;
		smp->length /= 3;
		break;
	case AU_PCM_32:
		sflags |= SF_32 | SF_PCMS;
		smp->length /= 4;
		break;
	case AU_IEEE_32:
		sflags |= SF_32 | SF_IEEE;
		smp->length /= 4;
		break;
	case AU_IEEE_64:
		sflags |= SF_64 | SF_IEEE;
		smp->length /= 8;
		break;
	default:
		return 0;
	}

	switch (au.channels) {
	case 1:
		sflags |= SF_M;
		break;
	case 2:
		sflags |= SF_SI;
		smp->length /= 2;
		break;
	default:
		return 0;
	}

	if (au.data_offset > sizeof(au)) {
		size_t extlen = MIN(au.data_offset - sizeof(au), sizeof(smp->name) - 1);

		if (slurp_read(fp, smp->name, extlen) != extlen)
			return 0;

		smp->name[extlen] = 0;
	}

	return csf_read_sample(smp, sflags, fp);
}

/* --------------------------------------------------------------------------------------------------------- */

int fmt_au_save_sample(disko_t *fp, song_sample_t *smp)
{
	if (smp->flags & CHN_ADLIB)
		return SAVE_UNSUPPORTED;

	struct au_header au;
	uint32_t ln;

	memcpy(au.magic, ".snd", 4);

	au.data_offset = bswapBE32(49); // header is 24 bytes, sample name is 25
	ln = smp->length;
	if (smp->flags & CHN_16BIT) {
		ln *= 2;
		au.encoding = bswapBE32(AU_PCM_16);
	} else {
		au.encoding = bswapBE32(AU_PCM_8);
	}
	au.sample_rate = bswapBE32(smp->c5speed);
	if (smp->flags & CHN_STEREO) {
		ln *= 2;
		au.channels = bswapBE32(2);
	} else {
		au.channels = bswapBE32(1);
	}
	au.data_size = bswapBE32(ln);

	disko_write(fp, &au, sizeof(au));
	disko_write(fp, smp->name, 25);
	csf_write_sample(fp, smp, SF_BE | SF_PCMS
			| ((smp->flags & CHN_16BIT) ? SF_16 : SF_8)
			| ((smp->flags & CHN_STEREO) ? SF_SI : SF_M),
			UINT32_MAX);

	return SAVE_SUCCESS;
}
