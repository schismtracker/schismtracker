/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
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

/* this is my ~3k png writer -mrsb */
#include "pngw.h"

/* manual string compression; gcc doesn't see this for some reason */
#define H5 "\010\3\0\0\0"
#define Z3 (H5+2)

static void _make_crc32_tab(unsigned int *crc32_tab)
{
	int n, k;
	unsigned int c, p;

	p = 0xedb88320L;
	for (n = 0; n < 256; n++) {
		c = (unsigned int)n;
		for (k = 0; k < 8; k++)
			c = (c & 1) ? p ^ (c >> 1) : (c >> 1);
		crc32_tab[n] = c;
	}
}

static void out_crc(unsigned int *crc32_tab,
			unsigned int *crc, struct pngw_arg *p,
			const void *data, int len)
{
	const unsigned char *s;
	int i;

	s = (void*)data;
	for (i = 0; i < len; i++) {
		*crc = crc32_tab[ ((*crc) ^ s[i]) & 255 ]
			^ ((*crc) >> 8);
	}
	p->output(p, data, len);
}

void pngw(struct pngw_arg *p)
{
	unsigned char u16[2];
	unsigned char u32[4];
#define make_u16le(n) u16[0] = (n)&255, u16[1] = ((n)>>8)&255
#define make_u32be(n) \
		u32[3] = (n)&255, \
		u32[2] = ((n)>>8)&255, \
		u32[1] = ((n)>>16)&255, \
		u32[0] = ((n)>>24)&255
	unsigned char triple[3];
	unsigned int crc;
	int data_size;
	int check;
	unsigned int crc32_tab[256]; /*alloca*/
#define NEWCHUNK(n,z)	{check=(n);make_u32be(check);crc=0xffffffff; \
			p->output(p,u32,4); out_crc(crc32_tab,&crc,p,z,4); }
#define OUT(a,b)	{out_crc(crc32_tab,&crc,p,(const void*)a,b);check-=b;}
#define OUT_u16le(n)	{make_u16le(n);OUT(u16,2);}
#define OUT_u32be(n)	{make_u32be(n);OUT(u32,4);}
#define ENDCHUNK()	{make_u32be(~crc);p->output(p,u32,4);}
	int i, j, k;
	int s1, s2, n;
	int x, y;

	_make_crc32_tab(crc32_tab);

	/* initial header */
	p->output(p, "\211PNG\r\n\032\n", 8);

	/* write the IHDR */
	NEWCHUNK(4+4+5, "IHDR");
	OUT_u32be(p->width);
	OUT_u32be(p->height);
	OUT(H5,5);
	ENDCHUNK();

	/* write the PLTE */
	j = p->pal_size;
	if (j < 16) j = 16;
	else if (j > 256) j = 256;
	NEWCHUNK(3*j, "PLTE");
	for (i = 0; i < j && i < p->pal_size; i++) {
		k = p->pal[i];
		triple[2] = k & 255;
		triple[1] = (k>>8) & 255;
		triple[0] = (k>>16) & 255;
		OUT(triple,3);
	}
	for (; i < j; i++) {
		OUT(Z3, 3);
	}
	ENDCHUNK();

	/* write the tRNS */
	NEWCHUNK(j, "tRNS");
	for (i = 0; i < j && i < p->pal_size; i++) {
		k = p->pal[i];
		triple[0] = (k>>24) & 255;
		OUT(triple,1);
	}
	for (; i < j; i++) {
		OUT(Z3, 1);
	}
	ENDCHUNK();

	/* write the IDAT */
	j = (p->height * (p->width+1));
	data_size = 2 + j + 5 * ((j+65534)/65535) + 4;
	NEWCHUNK(data_size, "IDAT");
	OUT("\170\332", 2); /* ZLIB header */

	/* adler32 */
#define BASE 65521
#define NMAX 5552
	s1 = 1;
	s2 = 0;
	n = NMAX;
#define HEADER() {i=0;\
	if(j>65535){ \
		OUT(Z3,1);\
		OUT_u16le(65535);\
		OUT_u16le(~65535); \
	} else { \
		OUT("\1", 1); \
		OUT_u16le(j); \
		OUT_u16le(~j); \
	} \
}
#define WRITE_PIXEL(c) \
	{ \
		triple[0] = c; OUT(triple,1); \
		i++;j--; if (i == 65535) { HEADER(); } }

#define MINUS_N() if ((n-=1) == 0) { s1 %= BASE; s2 %= BASE; n = NMAX; }
	HEADER();
	for (y = 0; y < p->height; y++) {
		WRITE_PIXEL(0); /* filter "pixel" */
		s2 += s1;

		MINUS_N();
		for (x = 0; x < p->width; x++) {
			k = p->read_pixel(p);
			WRITE_PIXEL(k);

			s1 += k;
			s2 += s1;
			MINUS_N();
		}
	}
	s1 %= BASE; s2 %= BASE;
	OUT_u32be((s2<<16)|s1);
	ENDCHUNK();

	NEWCHUNK(0, "IEND");
	ENDCHUNK();
}
