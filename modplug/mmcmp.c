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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdlib.h>
#ifdef __EMX__
#include <sys/types.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>

#include "slurp.h"


#pragma pack(push, 1)
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
#pragma pack(pop)


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


int mmcmp_unpack(uint8_t **data, size_t *length)
{
        size_t memlength;
        uint8_t *memfile;
        uint8_t *buffer;
        mm_header_t *hdr;
        uint32_t *pblk_table;
        size_t filesize;
        uint32_t block, i;

        if (!data || !length || !*data) {
                return 0;
        }
        memlength = *length;
        memfile = *data;
        hdr = (mm_header_t *) memfile;

        if (memlength < 256
            || memcmp(hdr->zirconia, "ziRCONia", 8) != 0
            || hdr->hdrsize < 14
            || !hdr->blocks
            || hdr->filesize < 16
            || hdr->filesize > 0x8000000
            || hdr->blktable >= memlength
            || hdr->blktable + 4 * hdr->blocks > memlength) {
                return 0;
        }
        filesize = hdr->filesize;
        if ((buffer = calloc(1, (filesize + 31) & ~15)) == NULL)
                return 0;

        pblk_table = (uint32_t *) (memfile + hdr->blktable);

        for (block = 0; block < hdr->blocks; block++) {
                uint32_t pos = pblk_table[block];
                mm_block_t *pblk = (mm_block_t *) (memfile + pos);
                mm_subblock_t *psubblk = (mm_subblock_t *) (memfile + pos + 20);

                if ((pos + 20 >= memlength)
                    || (pos + 20 + pblk->sub_blk * 8 >= memlength)) {
                        break;
                }
                pos += 20 + pblk->sub_blk * 8;

                if (!(pblk->flags & MM_COMP)) {
                        /* Data is not packed */
                        for (i = 0; i < pblk->sub_blk; i++) {
                                if ((psubblk->unpk_pos > filesize)
                                    || (psubblk->unpk_pos + psubblk->unpk_size > filesize)) {
                                        break;
                                }
                                memcpy(buffer + psubblk->unpk_pos, memfile + pos, psubblk->unpk_size);
                                pos += psubblk->unpk_size;
                                psubblk++;
                        }
                } else if (pblk->flags & MM_16BIT) {
                        /* Data is 16-bit packed */
                        uint16_t *dest = (uint16_t *) (buffer + psubblk->unpk_pos);
                        uint32_t size = psubblk->unpk_size >> 1;
                        uint32_t destpos = 0;
                        uint32_t numbits = pblk->num_bits;
                        uint32_t subblk = 0, oldval = 0;

                        mm_bit_buffer_t bb = {
                                .bits = 0,
                                .buffer = 0,
                                .src = memfile + pos + pblk->tt_entries,
                                .end = memfile + pos + pblk->pk_size,
                        };

                        while (subblk < pblk->sub_blk) {
                                uint32_t newval = 0x10000;
                                uint32_t d = get_bits(&bb, numbits + 1);

                                if (d >= mm_16bit_commands[numbits]) {
                                        uint32_t nFetch = mm_16bit_fetch[numbits];
                                        uint32_t newbits = get_bits(&bb, nFetch)
                                                + ((d - mm_16bit_commands[numbits]) << nFetch);
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
                                        if (pblk->flags & MM_DELTA) {
                                                newval += oldval;
                                                oldval = newval;
                                        } else if (!(pblk->flags & MM_ABS16)) {
                                                newval ^= 0x8000;
                                        }
                                        dest[destpos++] = (uint16_t) newval;
                                }
                                if (destpos >= size) {
                                        subblk++;
                                        destpos = 0;
                                        size = psubblk[subblk].unpk_size >> 1;
                                        dest = (uint16_t *)(buffer + psubblk[subblk].unpk_pos);
                                }
                        }
                } else {
                        /* Data is 8-bit packed */
                        uint8_t *dest = buffer + psubblk->unpk_pos;
                        uint32_t size = psubblk->unpk_size;
                        uint32_t destpos = 0;
                        uint32_t numbits = pblk->num_bits;
                        uint32_t subblk = 0, oldval = 0;
                        uint8_t *ptable = memfile + pos;

                        mm_bit_buffer_t bb = {
                                .bits = 0,
                                .buffer = 0,
                                .src = memfile + pos + pblk->tt_entries,
                                .end = memfile + pos + pblk->pk_size,
                        };

                        while (subblk < pblk->sub_blk) {
                                uint32_t newval = 0x100;
                                uint32_t d = get_bits(&bb, numbits + 1);

                                if (d >= mm_8bit_commands[numbits]) {
                                        uint32_t nFetch = mm_8bit_fetch[numbits];
                                        uint32_t newbits = get_bits(&bb, nFetch)
                                                + ((d - mm_8bit_commands[numbits]) << nFetch);
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
                                        if (pblk->flags & MM_DELTA) {
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
                }
        }
        *data = buffer;
        *length = filesize;
        return 1;
}

