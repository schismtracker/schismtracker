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

#define NEED_BYTESWAP
#include "headers.h"
#include "fmt.h"

/* --------------------------------------------------------------------- */

bool fmt_au_read_info(dmoz_file_t *file, const byte *data, size_t length)
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

	file->description = "AU Sample";
	/*file->extension = strdup("au");*/
	if (data_offset > 24) {
		int extlen = data_offset - 24;

		file->title = calloc(extlen + 1, sizeof(char));
		memcpy(file->title, data + 24, extlen);
		file->title[extlen] = 0;
	}
	file->type = TYPE_SAMPLE_PLAIN;
	return true;
}

/* --------------------------------------------------------------------- */

enum {
	AU_ULAW = 1,			/* µ-law */
	AU_PCM_8 = 2,			/* 8-bit linear PCM (RS_PCM8U in Modplug) */
	AU_PCM_16 = 3,			/* 16-bit linear PCM (RS_PCM16M) */
	AU_PCM_24 = 4,			/* 24-bit linear PCM */
	AU_PCM_32 = 5,			/* 32-bit linear PCM */
	AU_IEEE_32 = 6,			/* 32-bit IEEE floating point */
	AU_IEEE_64 = 7,			/* 64-bit IEEE floating point */
	AU_ISDN_ULAW_ADPCM = 23,	/* 8-bit ISDN µ-law (CCITT G.721 ADPCM compressed) */
};

struct au_header {
	char magic[4]; /* ".snd" */
	unsigned long data_offset, data_size, encoding, sample_rate, channels;
};

bool fmt_au_load_sample(const byte *data, size_t length, song_sample *smp, char *title)
{
	struct au_header au;
	
	if (length < 24)
		return false;

	memcpy(&au, data, sizeof(au));
	/* optimization: could #ifdef this out on big-endian machines */
	au.data_offset = bswapBE32(au.data_offset);
	au.data_size = bswapBE32(au.data_size);
	au.encoding = bswapBE32(au.encoding);
	au.sample_rate = bswapBE32(au.sample_rate);
	au.channels = bswapBE32(au.channels);
	
/*#define C__(cond) if (!(cond)) { log_appendf(2, "failed condition: %s", #cond); return false; }*/
#define C__(cond) if (!(cond)) { return false; }
	C__(memcmp(au.magic, ".snd", 4) == 0);
	C__(au.data_offset >= 24);
	C__(au.data_offset < length);
	C__(au.data_size > 0);
	C__(au.data_size <= length - au.data_offset);
	C__(au.encoding == AU_PCM_8 || au.encoding == AU_PCM_16);
	C__(au.channels == 1 || au.channels == 2);
	
	if (au.channels == 2) {
		printf("TODO: stereo sample\n");
		return false;
	}
	
	smp->speed = au.sample_rate;
	smp->volume = 64 * 4;
	smp->global_volume = 64;
	smp->length = au.data_size; /* maybe this should be MIN(...), for files with a wacked out length? */
	if (au.encoding == AU_PCM_16) {
		smp->flags |= SAMP_16_BIT;
		smp->length /= 2;
	}
	
	if (au.data_offset > 24) {
		int extlen = MIN(25, au.data_offset - 24);
		memcpy(title, data + 24, extlen);
		title[extlen] = 0;
	}

	smp->data = song_sample_allocate(au.data_size);
	memcpy(smp->data, data + au.data_offset, au.data_size);

#ifndef WORDS_BIGENDIAN
	/* maybe this could use swab()? */
	if (smp->flags & SAMP_16_BIT) {
		signed short *s = (signed short *) smp->data;
		unsigned long i = smp->length;
		while (i-- > 0) {
			*s = bswapBE16(*s);
			s++;
		}
	}
#endif
	
	return true;
}

/* --------------------------------------------------------------------------------------------------------- */

bool fmt_au_save_sample(FILE *fp, song_sample *smp, char *title)
{
	struct au_header au;

	memcpy(au.magic, ".snd", 4);
	
	au.data_offset = bswapBE32(49); // header is 24 bytes, sample name is 25
	if (smp->flags & SAMP_16_BIT) {
		au.data_size = bswapBE32(smp->length * 2);
		au.encoding = bswapBE32(AU_PCM_16);
	} else {
		au.data_size = bswapBE32(smp->length);
		au.encoding = bswapBE32(AU_PCM_8);
	}
	au.sample_rate = bswapBE32(smp->speed);
	au.channels = bswapBE32(1);
	
	fwrite(&au, sizeof(au), 1, fp);
	fwrite(title, 1, 25, fp);
	save_sample_data_BE(fp, smp);
	
	return true;
}
