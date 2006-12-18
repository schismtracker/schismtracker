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

/* --------------------------------------------------------------------- */

#define NEED_BYTESWAP
#include "headers.h"
#include "fmt.h"

#include <math.h> /* for ldexp/frexp */

UNUSED static void ConvertToIeeeExtended(double num, unsigned char *bytes);
static double ConvertFromIeeeExtended(const unsigned char *bytes);

/* --------------------------------------------------------------------- */

int fmt_aiff_read_info(dmoz_file_t *file, const byte *data, size_t length)
{
	size_t position, block_length;
	/* these are offsets to various chunks of data in the file */
	size_t comm_chunk = 0, name_chunk = 0, ssnd_chunk = 0;
	short s; /* should be 16 bits */
	unsigned long ul; /* 32 bits */

	/* file structure: "FORM", filesize, "AIFF", chunks */
	if (length < 12 || memcmp(data, "FORM", 4) != 0 || memcmp(data + 8, "AIFF", 4) != 0)
		return false;
	memcpy(&block_length, data + 4, 4);
	block_length = bswapBE32(block_length);
	if (block_length + 8 > length)
		return false;
	
	/* same as below, mostly */
	position = 12;
	while (position + 8 < length) {
		memcpy(&block_length, data + position + 4, 4);
		block_length = bswapBE32(block_length);
		if (comm_chunk == 0 && memcmp(data + position, "COMM", 4) == 0) {
			if (block_length != 18)
				return false;
			comm_chunk = position;
		} else if (name_chunk == 0 && memcmp(data + position, "NAME", 4) == 0) {
			name_chunk = position;
		} else if (ssnd_chunk == 0 && memcmp(data + position, "SSND", 4) == 0) {
			ssnd_chunk = position;
		}
		position += 8 + block_length;
	}
	
	if (comm_chunk == 0 || ssnd_chunk == 0)
		return false;
	
	if (comm_chunk+16 > length) return false;
	memcpy(&s, data + comm_chunk + 8, 2); /* short numChannels; */
	file->smp_flags = 0;
	if (bswapBE16(s) > 1) {
		file->smp_flags |= SAMP_STEREO;
	}

	memcpy(&s, data + comm_chunk + 14, 2); /* short sampleSize; (bits per sample, 1..32) */
	s = bswapBE16(s);
	s = (s + 7) & ~7;
	if (s == 16) {
		file->smp_flags |= SAMP_16_BIT;
	}

	memcpy(&ul, data + comm_chunk + 10, 4); /* unsigned long numSampleFrames; */
	file->smp_length = bswapBE32(ul);

	if (name_chunk) {
		/* should probably save the length as well, and not have to read it twice :P */
		memcpy(&block_length, data + name_chunk + 4, 4);
		block_length = bswapBE32(block_length);
		file->title = mem_alloc(block_length + 1);
		memcpy(file->title, data + name_chunk + 8, block_length);
		file->title[block_length] = '\0';
	}

	file->description = "AIFF Sample";
	file->smp_filename = file->title;
	file->type = TYPE_SAMPLE_PLAIN;
	return true;
}

int fmt_aiff_load_sample(const byte *data, size_t length, song_sample *smp, char *title)
{
	size_t position, block_length;
	unsigned long byte_length; /* size of the sample data */
	/* these are offsets to various chunks of data in the file */
	size_t comm_chunk = 0, name_chunk = 0, ssnd_chunk = 0;
	/* temporary variables to read stuff into */
	short s; /* should be 16 bits */
	unsigned long ul; /* 32 bits */

	/* file structure: "FORM", filesize, "AIFF", chunks */
	if (length < 12 || memcmp(data, "FORM", 4) != 0 || memcmp(data + 8, "AIFF", 4) != 0)
		return false;
	memcpy(&block_length, data + 4, 4);
	block_length = bswapBE32(block_length);
	if (block_length > length) {
		printf("aiff: file claims to be bigger than it really is!");
		return false;
	}
	
	/* same as above, mostly */
	position = 12;
	while (position + 8 < length) {
		memcpy(&block_length, data + position + 4, 4);
		block_length = bswapBE32(block_length);
		if (comm_chunk == 0 && memcmp(data + position, "COMM", 4) == 0) {
			if (block_length != 18) {
				printf("aiff: weird %ld-byte COMM chunk; bailing", block_length);
				return false;
			}
			comm_chunk = position;
		} else if (name_chunk == 0 && memcmp(data + position, "NAME", 4) == 0) {
			name_chunk = position;
		} else if (ssnd_chunk == 0 && memcmp(data + position, "SSND", 4) == 0) {
			ssnd_chunk = position;
		}
		position += 8 + block_length;
	}
	
	if (comm_chunk == 0 || ssnd_chunk == 0)
		return false;
	
	/* COMM chunk: sample format information */
	memcpy(&s, data + comm_chunk + 8, 2); /* short numChannels; */
	s = bswapBE16(s);
	if (s == 2) {
		smp->flags |= SAMP_STEREO;
	} else if (s != 1) {
		return false;
	}
	memcpy(&ul, data + comm_chunk + 10, 4); /* unsigned long numSampleFrames; */
	smp->length = bswapBE32(ul);
	memcpy(&s, data + comm_chunk + 14, 2); /* short sampleSize; (bits per sample, 1..32) */
	s = bswapBE16(s);
	s = (s + 7) & ~7;
	if (s != 8 && s != 16) {
		printf("aiff: %d-bit samples not supported; bailing", s);
		return false;
	}
	if (s == 16)
		smp->flags |= SAMP_16_BIT;
	byte_length = s / 8 * smp->length;
	if (smp->flags & SAMP_STEREO) byte_length *= 2;
	smp->data = song_sample_allocate(byte_length);
	smp->speed = ConvertFromIeeeExtended(data + comm_chunk + 16); /* extended sampleRate; */
	
	/* SSND chunk: the actual sample data */
	memcpy(&ul, data + ssnd_chunk + 8, 4); /* unsigned long offset; (bytes to skip, for block alignment) */
	ul = bswapBE32(ul);
	/* unsigned long blockSize; (skipping this) */
	memcpy(smp->data, data + ssnd_chunk + 16 + ul, byte_length); /* unsigned char soundData[]; */

#ifndef WORDS_BIGENDIAN
	/* maybe this could use swab()? */
	if (smp->flags & SAMP_16_BIT) {
		signed short *p = (signed short *) smp->data;
		unsigned long i = smp->length;
		if (smp->flags & SAMP_STEREO) i *= 2;
		while (i-- > 0) {
			*p = bswapBE16(*p);
			p++;
		}
	}
#endif

	/* NAME chunk: title (optional) */
	if (name_chunk) {
		memcpy(&block_length, data + name_chunk + 4, 4);
		block_length = bswapBE32(block_length);
		block_length = MIN(block_length, 25);
		memcpy(title, data + name_chunk + 8, block_length);
		title[block_length] = '\0';
	}
	
	smp->volume = 64 * 4;
	smp->global_volume = 64;
	
	return true;
}

int fmt_aiff_save_sample(diskwriter_driver_t *fp, song_sample *smp, char *title)
{
	short s;
	unsigned long ul;
	int tlen, bps = (smp->flags & SAMP_16_BIT) ? 2 : 1;
	unsigned char b[10];
	
	/* File header
	       ID ckID;
	       long ckSize;
	       ID formType; */
	fp->o(fp, (const unsigned char *)"FORM\1\1\1\1AIFFNAME", 16);

	/* NAME chunk
	       ID ckID;
	       long ckSize;
	       char text[]; */
	tlen = strlen(title);
	if (tlen & 1)
		tlen++; /* must be even */
	ul = bswapBE32(tlen);
	fp->o(fp, (const unsigned char *)&ul, 4);
	fp->o(fp, (const unsigned char *)title, tlen);

	/* COMM chunk
	       ID ckID;
	       long ckSize;
	       short numChannels;
	       unsigned long numSampleFrames;
	       short sampleSize;
	       extended sampleRate; */
	fp->o(fp, (const unsigned char *)"COMM", 4);
	ul = bswapBE32(18);
	fp->o(fp, (const unsigned char *)&ul, 4);
	s = bswapBE16(1);
	fp->o(fp, (const unsigned char *)&s, 2);
	ul = bswapBE32(smp->length);
	fp->o(fp, (const unsigned char *)&ul, 4);
	s = 8 * bps;
	s = bswapBE16(s);
	fp->o(fp, (const unsigned char *)&s, 2);
	ConvertToIeeeExtended(smp->speed, b);
	fp->o(fp, (const unsigned char *)b, 10);

	/* SSND chunk:
	       char ckID[4];
	       long ckSize;
	       unsigned long offset;
	       unsigned long blockSize;
	       unsigned char soundData[]; */
	fp->o(fp, (const unsigned char *)"SSND", 4);
	ul = smp->length * bps;
	ul = bswapBE32(ul);
	fp->o(fp, (const unsigned char *)&ul, 4);
	ul = bswapBE32(0);
	fp->o(fp, (const unsigned char *)&ul, 4);
	fp->o(fp, (const unsigned char *)&ul, 4);
	fp->o(fp, (const unsigned char *)smp->data, smp->length * bps);
	
	/* fix the length in the file header */
	ul = fp->pos - 8;
	ul = bswapBE32(ul);
	fp->l(fp, 4);
	fp->o(fp, (const unsigned char *)&ul, 4);
	
	return true;
}

/* --------------------------------------------------------------------- */
/* Copyright (C) 1988-1991 Apple Computer, Inc.
 * All rights reserved.
 *
 * Machine-independent I/O routines for IEEE floating-point numbers.
 *
 * NaN's and infinities are converted to HUGE_VAL or HUGE, which
 * happens to be infinity on IEEE machines.  Unfortunately, it is
 * impossible to preserve NaN's in a machine-independent way.
 * Infinities are, however, preserved on IEEE machines.
 *
 * These routines have been tested on the following machines:
 *    Apple Macintosh, MPW 3.1 C compiler
 *    Apple Macintosh, THINK C compiler
 *    Silicon Graphics IRIS, MIPS compiler
 *    Cray X/MP and Y/MP
 *    Digital Equipment VAX
 *
 *
 * Implemented by Malcolm Slaney and Ken Turkowski.
 *
 * Malcolm Slaney contributions during 1988-1990 include big- and little-
 * endian file I/O, conversion to and from Motorola's extended 80-bit
 * floating-point format, and conversions to and from IEEE single-
 * precision floating-point format.
 *
 * In 1991, Ken Turkowski implemented the conversions to and from
 * IEEE double-precision format, added more precision to the extended
 * conversions, and accommodated conversions involving +/- infinity,
 * NaN's, and denormalized numbers.
 */

#ifndef HUGE_VAL
# define HUGE_VAL HUGE
#endif /* HUGE_VAL */

#define FloatToUnsigned(f) ((unsigned long) (((long) (f - 2147483648.0)) + 2147483647L + 1))
#define UnsignedToFloat(u) (((double) ((long) (u - 2147483647L - 1))) + 2147483648.0)

static void ConvertToIeeeExtended(double num, unsigned char *bytes)
{
	int sign, expon;
	double fMant, fsMant;
	unsigned long hiMant, loMant;

	if (num < 0) {
		sign = 0x8000;
		num *= -1;
	} else {
		sign = 0;
	}

	if (num == 0) {
		expon = 0;
		hiMant = 0;
		loMant = 0;
	} else {
		fMant = frexp(num, &expon);
		if ((expon > 16384) || !(fMant < 1)) {
			/* Infinity or NaN */
			expon = sign | 0x7FFF;
			hiMant = 0;
			loMant = 0; /* infinity */
		} else {
			/* Finite */
			expon += 16382;
			if (expon < 0) {
				/* denormalized */
				fMant = ldexp(fMant, expon);
				expon = 0;
			}
			expon |= sign;
			fMant = ldexp(fMant, 32);          
			fsMant = floor(fMant); 
			hiMant = FloatToUnsigned(fsMant);
			fMant = ldexp(fMant - fsMant, 32); 
			fsMant = floor(fMant); 
			loMant = FloatToUnsigned(fsMant);
		}
	}

	bytes[0] = expon >> 8;
	bytes[1] = expon;
	bytes[2] = hiMant >> 24;
	bytes[3] = hiMant >> 16;
	bytes[4] = hiMant >> 8;
	bytes[5] = hiMant;
	bytes[6] = loMant >> 24;
	bytes[7] = loMant >> 16;
	bytes[8] = loMant >> 8;
	bytes[9] = loMant;
}

static double ConvertFromIeeeExtended(const unsigned char *bytes)
{
	double f;
	int expon;
	unsigned long hiMant, loMant;

	expon = ((bytes[0] & 0x7F) << 8) | (bytes[1] & 0xFF);
	hiMant = ((unsigned long) (bytes[2] & 0xFF) << 24)
		| ((unsigned long) (bytes[3] & 0xFF) << 16)
		| ((unsigned long) (bytes[4] & 0xFF) << 8)
		| ((unsigned long) (bytes[5] & 0xFF));
	loMant = ((unsigned long) (bytes[6] & 0xFF) << 24)
		| ((unsigned long) (bytes[7] & 0xFF) << 16)
		| ((unsigned long) (bytes[8] & 0xFF) << 8)
		| ((unsigned long) (bytes[9] & 0xFF));

	if (expon == 0 && hiMant == 0 && loMant == 0) {
		f = 0;
	} else if (expon == 0x7FFF) {
		/* Infinity or NaN */
		f = HUGE_VAL;
	} else {
		expon -= 16383;
		f = ldexp(UnsignedToFloat(hiMant), expon -= 31);
		f += ldexp(UnsignedToFloat(loMant), expon -= 32);
	}

	if (bytes[0] & 0x80)
		return -f;
	else
		return f;
}
