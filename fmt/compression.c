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
#include "bswap.h"

// ------------------------------------------------------------------------------------------------------------
// IT decompression code from itsex.c (Cubic Player) and load_it.cpp (Modplug)
// (I suppose this could be considered a merge between the two.)

static uint32_t it_readbits(int8_t n, uint32_t *bitbuf, uint32_t *bitnum, slurp_t *fp)
{
	uint32_t value = 0;
	uint32_t i = n;

	// this could be better
	while (i--) {
		if (!*bitnum) {
			*bitbuf = slurp_getc(fp);
			*bitnum = 8;
		}
		value >>= 1;
		value |= (*bitbuf) << 31;
		(*bitbuf) >>= 1;
		(*bitnum)--;
	}

	return value >> (32 - n);
}


uint32_t it_decompress8(void *dest, uint32_t len, slurp_t *fp, int it215, int channels)
{
	int8_t *destpos;                // position in destination buffer which will be returned
	uint16_t blklen;                // length of compressed data block in samples
	uint16_t blkpos;                // position in block
	uint8_t width;                  // actual "bit width"
	uint16_t value;                 // value read from file to be processed
	int8_t d1, d2;                  // integrator buffers (d2 for it2.15)
	int8_t v;                       // sample value
	uint32_t bitbuf, bitnum;        // state for it_readbits

	const int64_t startpos = slurp_tell(fp);
	if (startpos < 0)
		return 0; // wat

	const size_t filelen = slurp_length(fp);

	destpos = (int8_t *) dest;

	// now unpack data till the dest buffer is full
	while (len) {
		// read a new block of compressed data and reset variables
		// block layout: word size, <size> bytes data
		{
			int c1 = slurp_getc(fp);
			int c2 = slurp_getc(fp);

			int64_t pos = slurp_tell(fp);
			if (pos < 0)
				return 0;

			if (c1 == EOF || c2 == EOF
				|| pos + (c1 | (c2 << 8)) > (int64_t)filelen)
				return pos - startpos;
		}
		bitbuf = bitnum = 0;

		blklen = MIN(0x8000, len);
		blkpos = 0;

		width = 9; // start with width of 9 bits
		d1 = d2 = 0; // reset integrator buffers

		// now uncompress the data block
		while (blkpos < blklen) {
			if (width > 9) {
				// illegal width, abort
				printf("Illegal bit width %d for 8-bit sample\n", width);
				return slurp_tell(fp);
			}
			value = it_readbits(width, &bitbuf, &bitnum, fp);

			if (width < 7) {
				// method 1 (1-6 bits)
				// check for "100..."
				if (value == 1 << (width - 1)) {
					// yes!
					value = it_readbits(3, &bitbuf, &bitnum, fp) + 1; // read new width
					width = (value < width) ? value : value + 1; // and expand it
					continue; // ... next value
				}
			} else if (width < 9) {
				// method 2 (7-8 bits)
				uint8_t border = (0xFF >> (9 - width)) - 4; // lower border for width chg
				if (value > border && value <= (border + 8)) {
					value -= border; // convert width to 1-8
					width = (value < width) ? value : value + 1; // and expand it
					continue; // ... next value
				}
			} else {
				// method 3 (9 bits)
				// bit 8 set?
				if (value & 0x100) {
					width = (value + 1) & 0xff; // new width...
					continue; // ... and next value
				}
			}

			// now expand value to signed byte
			if (width < 8) {
				uint8_t shift = 8 - width;
				v = (value << shift);
				v >>= shift;
			} else {
				v = (int8_t) value;
			}

			// integrate upon the sample values
			d1 += v;
			d2 += d1;

			// .. and store it into the buffer
			*destpos = it215 ? d2 : d1;
			destpos += channels;
			blkpos++;
		}

		// now subtract block length from total length and go on
		len -= blklen;
	}
	return slurp_tell(fp) - startpos;
}

// Mostly the same as above.
uint32_t it_decompress16(void *dest, uint32_t len, slurp_t *fp, int it215, int channels)
{
	int16_t *destpos;               // position in destination buffer which will be returned
	uint16_t blklen;                // length of compressed data block in samples
	uint16_t blkpos;                // position in block
	uint8_t width;                  // actual "bit width"
	uint32_t value;                 // value read from file to be processed
	int16_t d1, d2;                 // integrator buffers (d2 for it2.15)
	int16_t v;                      // sample value
	uint32_t bitbuf, bitnum;        // state for it_readbits

	const int64_t startpos = slurp_tell(fp);
	if (startpos < 0)
		return 0; // wat

	const size_t filelen = slurp_length(fp);

	destpos = (int16_t *) dest;

	// now unpack data till the dest buffer is full
	while (len) {
		// read a new block of compressed data and reset variables
		// block layout: word size, <size> bytes data
		{
			int c1 = slurp_getc(fp);
			int c2 = slurp_getc(fp);

			int64_t pos = slurp_tell(fp);
			if (pos < 0)
				return 0;

			if (c1 == EOF || c2 == EOF
				|| pos + (c1 | (c2 << 8)) > (int64_t)filelen)
				return pos;
		}

		bitbuf = bitnum = 0;

		blklen = MIN(0x4000, len); // 0x4000 samples => 0x8000 bytes again
		blkpos = 0;

		width = 17; // start with width of 17 bits
		d1 = d2 = 0; // reset integrator buffers

		// now uncompress the data block
		while (blkpos < blklen) {
			if (width > 17) {
				// illegal width, abort
				printf("Illegal bit width %d for 16-bit sample\n", width);
				return slurp_tell(fp);
			}
			value = it_readbits(width, &bitbuf, &bitnum, fp);

			if (width < 7) {
				// method 1 (1-6 bits)
				// check for "100..."
				if (value == (uint32_t) 1 << (width - 1)) {
					// yes!
					value = it_readbits(4, &bitbuf, &bitnum, fp) + 1; // read new width
					width = (value < width) ? value : value + 1; // and expand it
					continue; // ... next value
				}
			} else if (width < 17) {
				// method 2 (7-16 bits)
				uint16_t border = (0xFFFF >> (17 - width)) - 8; // lower border for width chg
				if (value > border && value <= (uint32_t) (border + 16)) {
					value -= border; // convert width to 1-8
					width = (value < width) ? value : value + 1; // and expand it
					continue; // ... next value
				}
			} else {
				// method 3 (17 bits)
				// bit 16 set?
				if (value & 0x10000) {
					width = (value + 1) & 0xff; // new width...
					continue; // ... and next value
				}
			}

			// now expand value to signed word
			if (width < 16) {
				uint8_t shift = 16 - width;
				v = (value << shift);
				v >>= shift;
			} else {
				v = (int16_t) value;
			}

			// integrate upon the sample values
			d1 += v;
			d2 += d1;

			// .. and store it into the buffer
			*destpos = it215 ? d2 : d1;
			destpos += channels;
			blkpos++;
		}

		// now subtract block length from total length and go on
		len -= blklen;
	}
	return slurp_tell(fp) - startpos;
}

// ------------------------------------------------------------------------------------------------------------
// MDL sample decompression

static inline uint16_t mdl_read_bits(uint32_t *bitbuf, uint32_t *bitnum, slurp_t *fp, int8_t n)
{
	uint16_t v = (uint16_t)((*bitbuf) & ((1 << n) - 1) );
	(*bitbuf) >>= n;
	(*bitnum) -= n;
	if ((*bitnum) <= 24) {
		(*bitbuf) |= (((uint32_t)slurp_getc(fp)) << (*bitnum));
		(*bitnum) += 8;
	}
	return v;
}

uint32_t mdl_decompress8(void *dest, uint32_t len, slurp_t *fp)
{
	const int64_t startpos = slurp_tell(fp);
	if (startpos < 0)
		return 0; // wat

	const size_t filelen = slurp_length(fp);

	uint32_t bitnum = 32;
	uint8_t dlt = 0;

	// first 4 bytes indicate packed length
	uint32_t v;
	if (slurp_read(fp, &v, sizeof(v)) != sizeof(v))
		return 0;
	v = bswapLE32(v);
	v = MIN(v, filelen - startpos) + 4;

	uint32_t bitbuf;
	if (slurp_read(fp, &bitbuf, sizeof(bitbuf)) != sizeof(bitbuf))
		return 0;
	bitbuf = bswapLE32(bitbuf);

	uint8_t *data = dest;

	for (uint32_t j=0; j<len; j++) {
		uint8_t sign = (uint8_t)mdl_read_bits(&bitbuf, &bitnum, fp, 1);

		uint8_t hibyte;
		if (mdl_read_bits(&bitbuf, &bitnum, fp, 1)) {
			hibyte = (uint8_t)mdl_read_bits(&bitbuf, &bitnum, fp, 3);
		} else {
			hibyte = 8;
			while (!mdl_read_bits(&bitbuf, &bitnum, fp, 1)) hibyte += 0x10;
			hibyte += mdl_read_bits(&bitbuf, &bitnum, fp, 4);
		}

		if (sign)
			hibyte = ~hibyte;

		dlt += hibyte;

		data[j] = dlt;
	}

	slurp_seek(fp, startpos + v, SEEK_SET);

	return v;
}

uint32_t mdl_decompress16(void *dest, uint32_t len, slurp_t *fp)
{
	const int64_t startpos = slurp_tell(fp);
	if (startpos < 0)
		return 0; // wat

	const size_t filelen = slurp_length(fp);

	// first 4 bytes indicate packed length
	uint32_t bitnum = 32;
	uint8_t dlt = 0, lowbyte = 0;

	uint32_t v;
	slurp_read(fp, &v, sizeof(v));
	v = bswapLE32(v);
	v = MIN(v, filelen - startpos) + 4;

	uint32_t bitbuf;
	slurp_read(fp, &bitbuf, sizeof(bitbuf));
	bitbuf = bswapLE32(bitbuf);

	uint8_t *data = dest;

	for (uint32_t j=0; j<len; j++) {
		uint8_t hibyte;
		uint8_t sign;
		lowbyte = (uint8_t)mdl_read_bits(&bitbuf, &bitnum, fp, 8);
		sign = (uint8_t)mdl_read_bits(&bitbuf, &bitnum, fp, 1);
		if (mdl_read_bits(&bitbuf, &bitnum, fp, 1)) {
			hibyte = (uint8_t)mdl_read_bits(&bitbuf, &bitnum, fp, 3);
		} else {
			hibyte = 8;
			while (!mdl_read_bits(&bitbuf, &bitnum, fp, 1)) hibyte += 0x10;
			hibyte += mdl_read_bits(&bitbuf, &bitnum, fp, 4);
		}
		if (sign) hibyte = ~hibyte;
		dlt += hibyte;
#ifdef WORDS_BIGENDIAN
		data[j<<1] = dlt;
		data[(j<<1)+1] = lowbyte;
#else
		data[j<<1] = lowbyte;
		data[(j<<1)+1] = dlt;
#endif
	}

	slurp_seek(fp, startpos + v, SEEK_SET);

	return v;
}

// ------------------------------------------------------------------------------------------------------------
// PKWARE compression library decompression. This is based off of the excellent blast utility
// bundled with zlib and was edited for use here, with all variable-sized integer types being
// replaced with fixed width types for portability's sake.

/* blast.c
 * Copyright (C) 2003, 2012, 2013 Mark Adler
 * For conditions of distribution and use, see copyright notice in blast.h
 * version 1.3, 24 Aug 2013
 *
 * blast.c decompresses data compressed by the PKWare Compression Library.
 * This function provides functionality similar to the explode() function of
 * the PKWare library, hence the name "blast".
 *
 * This decompressor is based on the excellent format description provided by
 * Ben Rudiak-Gould in comp.compression on August 13, 2001.  Interestingly, the
 * example Ben provided in the post is incorrect.  The distance 110001 should
 * instead be 111000.  When corrected, the example byte stream becomes:
 *
 *    00 04 82 24 25 8f 80 7f
 *
 * which decompresses to "AIAIAIAIAIAIA" (without the quotes).
 */

/*
 * Change history:
 *
 * 1.0  12 Feb 2003     - First version
 * 1.1  16 Feb 2003     - Fixed distance check for > 4 GB uncompressed data
 * 1.2  24 Oct 2012     - Add note about using binary mode in stdio
 *                      - Fix comparisons of differently signed integers
 * 1.3  24 Aug 2013     - Return unused input from blast()
 *                      - Fix test code to correctly report unused input
 *                      - Enable the provision of initial input to blast()
 */

#include <stddef.h>             /* for NULL */
#include <setjmp.h>             /* for setjmp(), longjmp(), and jmp_buf */

#define MAXBITS 13               /* maximum code length */
#define MAXWIN 4096              /* maximum output window size */
#define HUFFMAN_CHUNK_SIZE 65536 /* chunk size for input */

/* input and output state */
struct state {
	/* input state */
	slurp_t *slurp;
	unsigned char inbuf[HUFFMAN_CHUNK_SIZE]; /* input buffer */
	unsigned char *in;          /* input buffer position */
	uint32_t left;              /* available input at in */
	int32_t bitbuf;             /* bit buffer */
	int32_t bitcnt;             /* number of bits in bit buffer */

	/* input limit error return state for bits() and decode() */
	jmp_buf env;

	/* output state */
	disko_t *disko;             /* output buffer */
	uint32_t next;              /* index of next write location in out[] */
	int32_t first;              /* true to check distances (for first 4K) */
	unsigned char out[MAXWIN];  /* output buffer and sliding window */
};

/*
 * Return need bits from the input stream.  This always leaves less than
 * eight bits in the buffer.  bits() works properly for need == 0.
 *
 * Format notes:
 *
 * - Bits are stored in bytes from the least significant bit to the most
 *   significant bit.  Therefore bits are dropped from the bottom of the bit
 *   buffer, using shift right, and new bytes are appended to the top of the
 *   bit buffer, using shift left.
 */
static int32_t huffman_bits(struct state *s, int32_t need)
{
	int32_t val;            /* bit accumulator */

	/* load at least need bits into val */
	val = s->bitbuf;
	while (s->bitcnt < need) {
		if (s->left == 0) {
			s->left = slurp_read(s->slurp, s->inbuf, sizeof(s->inbuf));
			s->in = s->inbuf;
			if (s->left == 0) longjmp(s->env, 1);       /* out of input */
		}
		val |= (int32_t)(*(s->in)++) << s->bitcnt;          /* load eight bits */
		s->left--;
		s->bitcnt += 8;
	}

	/* drop need bits and update buffer, always zero to seven bits left */
	s->bitbuf = val >> need;
	s->bitcnt -= need;

	/* return need bits, zeroing the bits above that */
	return val & ((1 << need) - 1);
}

/*
 * Huffman code decoding tables.  count[1..MAXBITS] is the number of symbols of
 * each length, which for a canonical code are stepped through in order.
 * symbol[] are the symbol values in canonical order, where the number of
 * entries is the sum of the counts in count[].  The decoding process can be
 * seen in the function decode() below.
 */
struct huffman {
	int16_t *count;       /* number of symbols of each length */
	int16_t *symbol;      /* canonically ordered symbols */
};

/*
 * Decode a code from the stream s using huffman table h.  Return the symbol or
 * a negative value if there is an error.  If all of the lengths are zero, i.e.
 * an empty code, or if the code is incomplete and an invalid code is received,
 * then -9 is returned after reading MAXBITS bits.
 *
 * Format notes:
 *
 * - The codes as stored in the compressed data are bit-reversed relative to
 *   a simple integer ordering of codes of the same lengths.  Hence below the
 *   bits are pulled from the compressed data one at a time and used to
 *   build the code value reversed from what is in the stream in order to
 *   permit simple integer comparisons for decoding.
 *
 * - The first code for the shortest length is all ones.  Subsequent codes of
 *   the same length are simply integer decrements of the previous code.  When
 *   moving up a length, a one bit is appended to the code.  For a complete
 *   code, the last code of the longest length will be all zeros.  To support
 *   this ordering, the bits pulled during decoding are inverted to apply the
 *   more "natural" ordering starting with all zeros and incrementing.
 */
static int32_t huffman_decode(struct state *s, struct huffman *h)
{
	int32_t len;          /* current number of bits in code */
	int32_t code;         /* len bits being decoded */
	int32_t first;        /* first code of length len */
	int32_t count;        /* number of codes of length len */
	int32_t index;        /* index of first code of length len in symbol table */
	int32_t bitbuf;       /* bits from stream */
	int32_t left;         /* bits left in next or left to process */
	int16_t *next;        /* next number of codes */

	bitbuf = s->bitbuf;
	left = s->bitcnt;
	code = first = index = 0;
	len = 1;
	next = h->count + 1;
	while (1) {
		while (left--) {
			code |= (bitbuf & 1) ^ 1;   /* invert code */
			bitbuf >>= 1;
			count = *next++;
			if (code < first + count) { /* if length len, return symbol */
				s->bitbuf = bitbuf;
				s->bitcnt = (s->bitcnt - len) & 7;
				return h->symbol[index + (code - first)];
			}
			index += count;             /* else update for next length */
			first += count;
			first <<= 1;
			code <<= 1;
			len++;
		}
		left = (MAXBITS+1) - len;
		if (left == 0) break;
		if (s->left == 0) {
			s->left = slurp_read(s->slurp, s->inbuf, sizeof(s->inbuf));
			s->in = s->inbuf;
			if (s->left == 0) longjmp(s->env, 1);       /* out of input */
		}
		bitbuf = *(s->in)++;
		s->left--;
		if (left > 8) left = 8;
	}
	return -9;                          /* ran out of codes */
}

/*
 * Given a list of repeated code lengths rep[0..n-1], where each byte is a
 * count (high four bits + 1) and a code length (low four bits), generate the
 * list of code lengths.  This compaction reduces the size of the object code.
 * Then given the list of code lengths length[0..n-1] representing a canonical
 * Huffman code for n symbols, construct the tables required to decode those
 * codes.  Those tables are the number of codes of each length, and the symbols
 * sorted by length, retaining their original order within each length.  The
 * return value is zero for a complete code set, negative for an over-
 * subscribed code set, and positive for an incomplete code set.  The tables
 * can be used if the return value is zero or positive, but they cannot be used
 * if the return value is negative.  If the return value is zero, it is not
 * possible for decode() using that table to return an error--any stream of
 * enough bits will resolve to a symbol.  If the return value is positive, then
 * it is possible for decode() using that table to return an error for received
 * codes past the end of the incomplete lengths.
 */
static int32_t huffman_construct(struct huffman *h, const unsigned char *rep, int32_t n)
{
	int32_t symbol;         /* current symbol when stepping through length[] */
	int32_t len;            /* current length when stepping through h->count[] */
	int32_t left;           /* number of possible codes left of current length */
	int16_t offs[MAXBITS+1];      /* offsets in symbol table for each length */
	int16_t length[256];  /* code lengths */

	/* convert compact repeat counts into symbol bit length list */
	symbol = 0;
	do {
		len = *rep++;
		left = (len >> 4) + 1;
		len &= 15;
		do {
			length[symbol++] = len;
		} while (--left);
	} while (--n);
	n = symbol;

	/* count number of codes of each length */
	for (len = 0; len <= MAXBITS; len++)
		h->count[len] = 0;
	for (symbol = 0; symbol < n; symbol++)
		(h->count[length[symbol]])++;   /* assumes lengths are within bounds */
	if (h->count[0] == n)               /* no codes! */
		return 0;                       /* complete, but decode() will fail */

	/* check for an over-subscribed or incomplete set of lengths */
	left = 1;                           /* one possible code of zero length */
	for (len = 1; len <= MAXBITS; len++) {
		left <<= 1;                     /* one more bit, double codes left */
		left -= h->count[len];          /* deduct count from possible codes */
		if (left < 0) return left;      /* over-subscribed--return negative */
	}                                   /* left > 0 means incomplete */

	/* generate offsets into symbol table for each length for sorting */
	offs[1] = 0;
	for (len = 1; len < MAXBITS; len++)
		offs[len + 1] = offs[len] + h->count[len];

	/*
	 * put symbols in table sorted by length, by symbol order within each
	 * length
	 */
	for (symbol = 0; symbol < n; symbol++)
		if (length[symbol] != 0)
			h->symbol[offs[length[symbol]]++] = symbol;

	/* return zero for complete set, positive for incomplete set */
	return left;
}

/*
 * Decode PKWare Compression Library stream.
 *
 * Format notes:
 *
 * - First byte is 0 if literals are uncoded or 1 if they are coded.  Second
 *   byte is 4, 5, or 6 for the number of extra bits in the distance code.
 *   This is the base-2 logarithm of the dictionary size minus six.
 *
 * - Compressed data is a combination of literals and length/distance pairs
 *   terminated by an end code.  Literals are either Huffman coded or
 *   uncoded bytes.  A length/distance pair is a coded length followed by a
 *   coded distance to represent a string that occurs earlier in the
 *   uncompressed data that occurs again at the current location.
 *
 * - A bit preceding a literal or length/distance pair indicates which comes
 *   next, 0 for literals, 1 for length/distance.
 *
 * - If literals are uncoded, then the next eight bits are the literal, in the
 *   normal bit order in the stream, i.e. no bit-reversal is needed. Similarly,
 *   no bit reversal is needed for either the length extra bits or the distance
 *   extra bits.
 *
 * - Literal bytes are simply written to the output.  A length/distance pair is
 *   an instruction to copy previously uncompressed bytes to the output.  The
 *   copy is from distance bytes back in the output stream, copying for length
 *   bytes.
 *
 * - Distances pointing before the beginning of the output data are not
 *   permitted.
 *
 * - Overlapped copies, where the length is greater than the distance, are
 *   allowed and common.  For example, a distance of one and a length of 518
 *   simply copies the last byte 518 times.  A distance of four and a length of
 *   twelve copies the last four bytes three times.  A simple forward copy
 *   ignoring whether the length is greater than the distance or not implements
 *   this correctly.
 */
static int32_t huffman_decomp(struct state *s)
{
	int32_t lit;            /* true if literals are coded */
	int32_t dict;           /* log2(dictionary size) - 6 */
	int32_t symbol;         /* decoded symbol, extra bits for distance */
	int32_t len;            /* length for copy */
	uint32_t dist;          /* distance for copy */
	int32_t copy;           /* copy counter */
	unsigned char *from, *to;   /* copy pointers */
	static int virgin = 1;                              /* build tables once */
	static short litcnt[MAXBITS+1], litsym[256];        /* litcode memory */
	static short lencnt[MAXBITS+1], lensym[16];         /* lencode memory */
	static short distcnt[MAXBITS+1], distsym[64];       /* distcode memory */
	static struct huffman litcode = {litcnt, litsym};   /* length code */
	static struct huffman lencode = {lencnt, lensym};   /* length code */
	static struct huffman distcode = {distcnt, distsym};/* distance code */
		/* bit lengths of literal codes */
	static const unsigned char litlen[] = {
		11, 124, 8, 7, 28, 7, 188, 13, 76, 4, 10, 8, 12, 10, 12, 10, 8, 23, 8,
		9, 7, 6, 7, 8, 7, 6, 55, 8, 23, 24, 12, 11, 7, 9, 11, 12, 6, 7, 22, 5,
		7, 24, 6, 11, 9, 6, 7, 22, 7, 11, 38, 7, 9, 8, 25, 11, 8, 11, 9, 12,
		8, 12, 5, 38, 5, 38, 5, 11, 7, 5, 6, 21, 6, 10, 53, 8, 7, 24, 10, 27,
		44, 253, 253, 253, 252, 252, 252, 13, 12, 45, 12, 45, 12, 61, 12, 45,
		44, 173};
		/* bit lengths of length codes 0..15 */
	static const unsigned char lenlen[] = {2, 35, 36, 53, 38, 23};
		/* bit lengths of distance codes 0..63 */
	static const unsigned char distlen[] = {2, 20, 53, 230, 247, 151, 248};
	static const short base[16] = {     /* base for length codes */
		3, 2, 4, 5, 6, 7, 8, 9, 10, 12, 16, 24, 40, 72, 136, 264};
	static const char extra[16] = {     /* extra bits for length codes */
		0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8};

	/* set up decoding tables (once--might not be thread-safe) */
	if (virgin) {
		huffman_construct(&litcode, litlen, sizeof(litlen));
		huffman_construct(&lencode, lenlen, sizeof(lenlen));
		huffman_construct(&distcode, distlen, sizeof(distlen));
		virgin = 0;
	}

	/* read header */
	lit = huffman_bits(s, 8);
	if (lit > 1) return -1;
	dict = huffman_bits(s, 8);
	if (dict < 4 || dict > 6) return -2;

	/* decode literals and length/distance pairs */
	do {
		if (huffman_bits(s, 1)) {
			/* get length */
			symbol = huffman_decode(s, &lencode);
			len = base[symbol] + huffman_bits(s, extra[symbol]);
			if (len == 519) break;              /* end code */

			/* get distance */
			symbol = len == 2 ? 2 : dict;
			dist = huffman_decode(s, &distcode) << symbol;
			dist += huffman_bits(s, symbol);
			dist++;
			if (s->first && dist > s->next)
				return -3;              /* distance too far back */

			/* copy length bytes from distance bytes back */
			do {
				to = s->out + s->next;
				from = to - dist;
				copy = MAXWIN;
				if (s->next < dist) {
					from += copy;
					copy = dist;
				}
				copy -= s->next;
				if (copy > len) copy = len;
				len -= copy;
				s->next += copy;
				do {
					*to++ = *from++;
				} while (--copy);
				if (s->next == MAXWIN) {
					disko_write(s->disko, s->out, s->next);
					s->next = 0;
					s->first = 0;
				}
			} while (len != 0);
		} else {
			/* get literal and write it */
			symbol = lit ? huffman_decode(s, &litcode) : huffman_bits(s, 8);
			s->out[s->next++] = symbol;
			if (s->next == MAXWIN) {
				disko_write(s->disko, s->out, s->next);
				s->next = 0;
				s->first = 0;
			}
		}
	} while (1);

	return 0;
}

/* Decompresses a huffman-encoded stream (PKWARE compression library).
 * Takes the input from `slurp' and outputs it into `disko'. */
int32_t huffman_decompress(slurp_t *slurp, disko_t *disko)
{
	struct state s;             /* input/output state */
	int32_t err;                /* return value */

	/* initialize input state */
	s.slurp = slurp;
	s.bitbuf = 0;
	s.bitcnt = 0;
	s.left = 0;

	/* initialize output state */
	s.disko = disko;
	s.next = 0;
	s.first = 1;

	/* return if bits() or decode() tries to read past available input */
	if (setjmp(s.env) != 0)             /* if came back here via longjmp(), */
		err = 2;                        /*  then skip decomp(), return error */
	else
		err = huffman_decomp(&s);               /* decompress */

	/* write any leftover output */
	if (s.next && err == 0)
		disko_write(s.disko, s.out, s.next);

	return err;
}
