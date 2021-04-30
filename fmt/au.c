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

#define NEED_BYTESWAP
#include "headers.h"
#include "fmt.h"


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


/* --------------------------------------------------------------------- */

int fmt_au_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
	struct au_header au;

	if (!(length > 24 && memcmp(data, ".snd", 4) == 0))
		return 0;

	memcpy(&au, data, 24);
	au.data_offset = bswapBE32(au.data_offset);
	au.data_size = bswapBE32(au.data_size);
	au.encoding = bswapBE32(au.encoding);
	au.sample_rate = bswapBE32(au.sample_rate);
	au.channels = bswapBE32(au.channels);

	if (!(au.data_offset < length && au.data_size > 0 && au.data_size <= length - au.data_offset))
		return 0;

	file->smp_length = au.data_size / au.channels;
	file->smp_flags = 0;
	if (au.encoding == AU_PCM_16) {
		file->smp_flags |= CHN_16BIT;
		file->smp_length /= 2;
	} else if (au.encoding == AU_PCM_24) {
		file->smp_length /= 3;
	} else if (au.encoding == AU_PCM_32 || au.encoding == AU_IEEE_32) {
		file->smp_length /= 4;
	} else if (au.encoding == AU_IEEE_64) {
		file->smp_length /= 8;
	}
	if (au.channels >= 2) {
		file->smp_flags |= CHN_STEREO;
	}
	file->description = "AU Sample";
	if (au.data_offset > 24) {
		int extlen = au.data_offset - 24;

		file->title = strn_dup((const char *)data + 24, extlen);
	}
	file->smp_filename = file->title;
	file->type = TYPE_SAMPLE_PLAIN;
	return 1;
}

/* --------------------------------------------------------------------- */

int fmt_au_load_sample(const uint8_t *data, size_t length, song_sample_t *smp)
{
	struct au_header au;
	uint32_t sflags = SF_BE | SF_PCMS;

	if (length < 24)
		return 0;

	memcpy(&au, data, sizeof(au));
	/* optimization: could #ifdef this out on big-endian machines */
	au.data_offset = bswapBE32(au.data_offset);
	au.data_size = bswapBE32(au.data_size);
	au.encoding = bswapBE32(au.encoding);
	au.sample_rate = bswapBE32(au.sample_rate);
	au.channels = bswapBE32(au.channels);

/*#define C__(cond) if (!(cond)) { log_appendf(2, "failed condition: %s", #cond); return 0; }*/
#define C__(cond) if (!(cond)) { return 0; }
	C__(memcmp(au.magic, ".snd", 4) == 0);
	C__(au.data_offset >= 24);
	C__(au.data_offset < length);
	C__(au.data_size > 0);
	C__(au.data_size <= length - au.data_offset);
	C__(au.encoding == AU_PCM_8 || au.encoding == AU_PCM_16);
	C__(au.channels == 1 || au.channels == 2);

	smp->c5speed = au.sample_rate;
	smp->volume = 64 * 4;
	smp->global_volume = 64;
	smp->length = au.data_size;
	if (au.encoding == AU_PCM_16) {
		sflags |= SF_16;
		smp->length /= 2;
	} else {
		sflags |= SF_8;
	}
	if (au.channels == 2) {
		sflags |= SF_SI;
		smp->length /= 2;
	} else {
		sflags |= SF_M;
	}

	if (au.data_offset > 24) {
		int extlen = MIN(25, au.data_offset - 24);
		memcpy(smp->name, data + 24, extlen);
		smp->name[extlen] = 0;
	}

	csf_read_sample(smp, sflags, data + au.data_offset, length - au.data_offset);

	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

int fmt_au_save_sample(disko_t *fp, song_sample_t *smp)
{
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
