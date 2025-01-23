/*
 * This program is  free software; you can redistribute it  and modify it
 * under the terms of the GNU  General Public License as published by the
 * Free Software Foundation; either version 2  of the license or (at your
 * option) any later version.
 *
 * Author: Olivier Lapicque <olivierl@jps.net>
 * Modified by Claudio Matsuoka for xmp
 * Modified again by Storlek to de-xmp-ify it.
 */

#include "headers.h"

#include "bswap.h"

#include "slurp.h" // for declaration of mmcmp_unpack

typedef struct mm_header {
	char zirconia[8]; // "ziRCONia"
	uint16_t hdrsize;

	uint16_t version;
	uint16_t blocks;
	uint32_t filesize;
	uint32_t blktable;
	uint8_t glb_comp;
	uint8_t fmt_comp;
} mm_header_t;

typedef struct mm_block {
	uint32_t unpk_size;
	uint32_t pk_size;
	uint32_t xor_chk;
	uint16_t sub_blk;
	uint16_t flags;
	uint16_t tt_entries;
	uint16_t num_bits;
} mm_block_t;

typedef struct mm_subblock {
	uint32_t unpk_pos;
	uint32_t unpk_size;
} mm_subblock_t;

static int read_mmcmp_header(mm_header_t *hdr, slurp_t *fp)
{
	const size_t memsize = slurp_length(fp);

#define READ_VALUE(name) \
	do { if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) { return 0; } } while (0)

	READ_VALUE(zirconia);
	READ_VALUE(hdrsize);

	READ_VALUE(version);
	READ_VALUE(blocks);
	READ_VALUE(filesize);
	READ_VALUE(blktable);
	READ_VALUE(glb_comp);
	READ_VALUE(fmt_comp);

#undef READ_VALUE

	hdr->hdrsize  = bswapLE16(hdr->hdrsize);
	hdr->version  = bswapLE16(hdr->version);
	hdr->blocks   = bswapLE16(hdr->blocks);
	hdr->filesize = bswapLE32(hdr->filesize);
	hdr->blktable = bswapLE32(hdr->blktable);

	if (memcmp(hdr->zirconia, "ziRCONia", 8)
	    || hdr->hdrsize < 14
	    || hdr->blocks == 0
	    || hdr->filesize < 16
	    || hdr->filesize > 0x8000000
	    || hdr->blktable >= memsize
	    || hdr->blktable + 4 * hdr->blocks > memsize)
		return 0;

	return 1;
}

static int read_mmcmp_block(mm_block_t *pblk, slurp_t *fp)
{
#define READ_VALUE(name) \
	do { if (slurp_read(fp, &pblk->name, sizeof(pblk->name)) != sizeof(pblk->name)) { return 0; } } while (0)

	READ_VALUE(unpk_size);
	READ_VALUE(pk_size);
	READ_VALUE(xor_chk);
	READ_VALUE(sub_blk);
	READ_VALUE(flags);
	READ_VALUE(tt_entries);
	READ_VALUE(num_bits);

#undef READ_VALUE

	pblk->unpk_size  = bswapLE32(pblk->unpk_size);
	pblk->pk_size    = bswapLE32(pblk->pk_size);
	pblk->xor_chk    = bswapLE32(pblk->xor_chk);
	pblk->sub_blk    = bswapLE16(pblk->sub_blk);
	pblk->flags      = bswapLE16(pblk->flags);
	pblk->tt_entries = bswapLE16(pblk->tt_entries);
	pblk->num_bits   = bswapLE16(pblk->num_bits);

	return 1;
}

static int read_mmcmp_subblocks(size_t size, mm_subblock_t *subblks, slurp_t *fp)
{
#define READ_VALUE(name) \
	do { if (slurp_read(fp, &name, sizeof(name)) != sizeof(name)) { return 0; } } while (0)

	for (size_t i = 0; i < size; i++) {
		READ_VALUE(subblks[i].unpk_pos);
		subblks[i].unpk_pos = bswapLE32(subblks[i].unpk_pos);
		READ_VALUE(subblks[i].unpk_size);
		subblks[i].unpk_size = bswapLE32(subblks[i].unpk_size);
	}
#undef READ_VALUE

	return 1;
}

// only used internally
typedef struct mm_bit_buffer {
	uint32_t bits, buffer;
	uint8_t *src, *end;
} mm_bit_buffer_t;


enum {
	MM_COMP   = 0x0001,
	MM_DELTA  = 0x0002,
	MM_16BIT  = 0x0004,
	MM_STEREO = 0x0100, // unused?
	MM_ABS16  = 0x0200,
	MM_ENDIAN = 0x0400, // unused?
};


static const uint32_t mm_8bit_commands[8] = { 0x01, 0x03, 0x07, 0x0F, 0x1E, 0x3C, 0x78, 0xF8 };

static const uint32_t mm_8bit_fetch[8] = { 3, 3, 3, 3, 2, 1, 0, 0 };

static const uint32_t mm_16bit_commands[16] = {
	0x0001, 0x0003, 0x0007, 0x000F, 0x001E, 0x003C, 0x0078, 0x00F0,
	0x01F0, 0x03F0, 0x07F0, 0x0FF0, 0x1FF0, 0x3FF0, 0x7FF0, 0xFFF0,
};

static const uint32_t mm_16bit_fetch[16] = { 4, 4, 4, 4, 3, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0 };



static uint32_t get_bits(mm_bit_buffer_t *bb, uint32_t bits)
{
	uint32_t d;
	if (!bits) return 0;
	while (bb->bits < 24) {
		bb->buffer |= ((bb->src < bb->end) ? *bb->src++ : 0) << bb->bits;
		bb->bits += 8;
	}
	d = bb->buffer & ((1 << bits) - 1);
	bb->buffer >>= bits;
	bb->bits -= bits;
	return d;
}


int mmcmp_unpack(slurp_t *fp, uint8_t **data, size_t *length)
{
	if (slurp_length(fp) < 256)
		return 0;

	mm_header_t hdr;
	if (!read_mmcmp_header(&hdr, fp))
		return 0;

	size_t filesize = hdr.filesize;

	uint8_t *buffer = calloc(1, (filesize + 31) & ~15);
	if (!buffer)
		return 0;

	SCHISM_VLA_ALLOC(uint32_t, pblk_table, hdr.blocks);
	slurp_seek(fp, hdr.blktable, SEEK_SET);
	if (slurp_read(fp, pblk_table, SCHISM_VLA_SIZEOF(pblk_table)) != SCHISM_VLA_SIZEOF(pblk_table)) {
		SCHISM_VLA_FREE(pblk_table);
		free(buffer);
		return 0;
	}

	for (uint32_t block = 0; block < hdr.blocks; block++) {
		uint32_t pos = bswapLE32(pblk_table[block]);

		slurp_seek(fp, pos, SEEK_SET);

		mm_block_t pblk;
		if (!read_mmcmp_block(&pblk, fp)) {
			SCHISM_VLA_FREE(pblk_table);
			free(buffer);
			return 0;
		}

		SCHISM_VLA_ALLOC(mm_subblock_t, psubblk, pblk.sub_blk);
		if (!read_mmcmp_subblocks(pblk.sub_blk, psubblk, fp)) {
			SCHISM_VLA_FREE(pblk_table);
			SCHISM_VLA_FREE(psubblk);
			free(buffer);
			return 0;
		}

		if (!(pblk.flags & MM_COMP)) {
			/* Data is not packed */
			for (uint32_t i = 0; i < pblk.sub_blk; i++) {
				if ((psubblk[i].unpk_pos > filesize) || (psubblk[i].unpk_pos + psubblk[i].unpk_size > filesize))
					break;

				if (slurp_read(fp, buffer + psubblk[i].unpk_pos, psubblk[i].unpk_size) != psubblk[i].unpk_size) {
					SCHISM_VLA_FREE(pblk_table);
					SCHISM_VLA_FREE(psubblk);
					free(buffer);
					return 0;
				}
			}
		} else if (pblk.flags & MM_16BIT) {
			/* Data is 16-bit packed */
			uint16_t *dest = (uint16_t *)(buffer + psubblk->unpk_pos);
			uint32_t size = psubblk->unpk_size >> 1;
			uint32_t destpos = 0;
			uint32_t numbits = pblk.num_bits;
			uint32_t subblk = 0, oldval = 0;

			SCHISM_VLA_ALLOC(unsigned char, buf, pblk.pk_size - pblk.tt_entries);

			slurp_seek(fp, pblk.tt_entries, SEEK_CUR);
			if (slurp_read(fp, buf, SCHISM_VLA_SIZEOF(buf)) != SCHISM_VLA_SIZEOF(buf)) {
				SCHISM_VLA_FREE(pblk_table);
				SCHISM_VLA_FREE(psubblk);
				SCHISM_VLA_FREE(buf);
				free(buffer);
				return 0;
			}

			mm_bit_buffer_t bb = {
				.bits = 0,
				.buffer = 0,
				.src = buf,
				.end = buf + pblk.pk_size,
			};

			while (subblk < pblk.sub_blk) {
				uint32_t newval = 0x10000;
				uint32_t d = get_bits(&bb, numbits + 1);

				if (d >= mm_16bit_commands[numbits]) {
					uint32_t fetch = mm_16bit_fetch[numbits];
					uint32_t newbits = get_bits(&bb, fetch)
						+ ((d - mm_16bit_commands[numbits]) << fetch);
					if (newbits != numbits) {
						numbits = newbits & 0x0F;
					} else {
						if ((d = get_bits(&bb, 4)) == 0x0F) {
							if (get_bits(&bb, 1))
								break;
							newval = 0xFFFF;
						} else {
							newval = 0xFFF0 + d;
						}
					}
				} else {
					newval = d;
				}
				if (newval < 0x10000) {
					newval = (newval & 1)
						? (uint32_t) (-(int32_t)((newval + 1) >> 1))
						: (uint32_t) (newval >> 1);
					if (pblk.flags & MM_DELTA) {
						newval += oldval;
						oldval = newval;
					} else if (!(pblk.flags & MM_ABS16)) {
						newval ^= 0x8000;
					}
					dest[destpos++] = bswapLE16((uint16_t) newval);
				}
				if (destpos >= size) {
					subblk++;
					destpos = 0;
					size = psubblk[subblk].unpk_size >> 1;
					dest = (uint16_t *)(buffer + psubblk[subblk].unpk_pos);
				}
			}

			SCHISM_VLA_FREE(buf);
		} else {
			/* Data is 8-bit packed */
			uint8_t *dest = buffer + psubblk->unpk_pos;
			uint32_t size = psubblk->unpk_size;
			uint32_t destpos = 0;
			uint32_t numbits = pblk.num_bits;
			uint32_t subblk = 0, oldval = 0;
			uint8_t ptable[0x100];

			slurp_peek(fp, ptable, sizeof(ptable));

			SCHISM_VLA_ALLOC(unsigned char, buf, pblk.pk_size - pblk.tt_entries);

			slurp_seek(fp, pblk.tt_entries, SEEK_CUR);
			if (slurp_read(fp, buf, SCHISM_VLA_SIZEOF(buf)) != SCHISM_VLA_SIZEOF(buf)) {
				SCHISM_VLA_FREE(pblk_table);
				SCHISM_VLA_FREE(psubblk);
				SCHISM_VLA_FREE(buf);
				free(buffer);
				return 0;
			}

			mm_bit_buffer_t bb = {
				.bits = 0,
				.buffer = 0,
				.src = buf,
				.end = buf + pblk.pk_size,
			};

			while (subblk < pblk.sub_blk) {
				uint32_t newval = 0x100;
				uint32_t d = get_bits(&bb, numbits + 1);

				if (d >= mm_8bit_commands[numbits]) {
					uint32_t fetch = mm_8bit_fetch[numbits];
					uint32_t newbits = get_bits(&bb, fetch)
						+ ((d - mm_8bit_commands[numbits]) << fetch);
					if (newbits != numbits) {
						numbits = newbits & 0x07;
					} else {
						if ((d = get_bits(&bb, 3)) == 7) {
							if (get_bits(&bb, 1))
								break;
							newval = 0xFF;
						} else {
							newval = 0xF8 + d;
						}
					}
				} else {
					newval = d;
				}
				if (newval < 0x100) {
					int n = ptable[newval];
					if (pblk.flags & MM_DELTA) {
						n += oldval;
						oldval = n;
					}
					dest[destpos++] = (uint8_t) n;
				}
				if (destpos >= size) {
					subblk++;
					destpos = 0;
					size = psubblk[subblk].unpk_size;
					dest = buffer + psubblk[subblk].unpk_pos;
				}
			}

			SCHISM_VLA_FREE(buf);
		}

		SCHISM_VLA_FREE(psubblk);
	}

	SCHISM_VLA_FREE(pblk_table);

	*data = buffer;
	*length = filesize;

	return 1;
}
