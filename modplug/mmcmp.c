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

/* this function declaration should ideally be in a header file */
int mmcmp_unpack(uint8_t **ppMemFile, uint32_t *pdwMemLength);


typedef struct MMCMPFILEHEADER
{
        uint32_t id_ziRC;       /* "ziRC" */
        uint32_t id_ONia;       /* "ONia" */
        uint16_t hdrsize;
} MMCMPFILEHEADER, *LPMMCMPFILEHEADER;

typedef struct MMCMPHEADER
{
        uint16_t version;
        uint16_t nblocks;
        uint32_t filesize;
        uint32_t blktable;
        uint8_t glb_comp;
        uint8_t fmt_comp;
} MMCMPHEADER, *LPMMCMPHEADER;

typedef struct MMCMPBLOCK
{
        uint32_t unpk_size;
        uint32_t pk_size;
        uint32_t xor_chk;
        uint16_t sub_blk;
        uint16_t flags;
        uint16_t tt_entries;
        uint16_t num_bits;
} MMCMPBLOCK, *LPMMCMPBLOCK;

typedef struct MMCMPSUBBLOCK
{
        uint32_t unpk_pos;
        uint32_t unpk_size;
} MMCMPSUBBLOCK, *LPMMCMPSUBBLOCK;

#define mmcmp_COMP      0x0001
#define mmcmp_DELTA     0x0002
#define mmcmp_16BIT     0x0004
#define mmcmp_STEREO    0x0100
#define mmcmp_ABS16     0x0200
#define mmcmp_ENDIAN    0x0400

typedef struct MMCMPBITBUFFER
{
        uint32_t bitcount;
        uint32_t bitbuffer;
        uint8_t * pSrc;
        uint8_t * pEnd;

} MMCMPBITBUFFER;


static uint32_t GetBits(struct MMCMPBITBUFFER *bb, uint32_t nBits)
/*--------------------------------------- */
{
        uint32_t d;
        if (!nBits) return 0;
        while (bb->bitcount < 24)
        {
                bb->bitbuffer |= ((bb->pSrc < bb->pEnd) ? *bb->pSrc++ : 0) << bb->bitcount;
                bb->bitcount += 8;
        }
        d = bb->bitbuffer & ((1 << nBits) - 1);
        bb->bitbuffer >>= nBits;
        bb->bitcount -= nBits;
        return d;
}

/*#define mmcmp_LOG*/

#ifdef mmcmp_LOG
extern void Log(const char * s, uint32 d1=0, uint32 d2=0, uint32 d3=0);
#endif

static const uint32_t MMCMP8BitCommands[8] =
{
        0x01, 0x03,     0x07, 0x0F,     0x1E, 0x3C,     0x78, 0xF8
};

static const uint32_t MMCMP8BitFetch[8] =
{
        3, 3, 3, 3, 2, 1, 0, 0
};

static const uint32_t MMCMP16BitCommands[16] =
{
        0x01, 0x03,     0x07, 0x0F,     0x1E, 0x3C,     0x78, 0xF0,
        0x1F0, 0x3F0, 0x7F0, 0xFF0, 0x1FF0, 0x3FF0, 0x7FF0, 0xFFF0
};

static const uint32_t MMCMP16BitFetch[16] =
{
        4, 4, 4, 4, 3, 2, 1, 0,
        0, 0, 0, 0, 0, 0, 0, 0
};



int mmcmp_unpack(uint8_t **ppMemFile, uint32_t *pdwMemLength)
{
        uint32_t dwMemLength = *pdwMemLength;
        uint8_t *lpMemFile = *ppMemFile;
        uint8_t *pBuffer;
        LPMMCMPFILEHEADER pmfh = (LPMMCMPFILEHEADER)(lpMemFile);
        LPMMCMPHEADER pmmh = (LPMMCMPHEADER)(lpMemFile+10);
        uint32_t *pblk_table;
        uint32_t dwFileSize;
        uint32_t nBlock, i;

        // TODO re-add PP20 unpacker. (This is completely the wrong place for such a thing,
        // but it's where Modplug called the unpacker)

        if ((dwMemLength < 256) || (!pmfh) || (pmfh->id_ziRC != 0x4352697A) || (pmfh->id_ONia != 0x61694e4f) || (pmfh->hdrsize < 14)
         || (!pmmh->nblocks) || (pmmh->filesize < 16) || (pmmh->filesize > 0x8000000)
         || (pmmh->blktable >= dwMemLength) || (pmmh->blktable + 4*pmmh->nblocks > dwMemLength)) return 0;
        dwFileSize = pmmh->filesize;
        if ((pBuffer = calloc(1, (dwFileSize + 31) & ~15)) == NULL) return 0;
        pblk_table = (uint32_t *) (lpMemFile+pmmh->blktable);
        for (nBlock=0; nBlock<pmmh->nblocks; nBlock++)
        {
                uint32_t dwMemPos = pblk_table[nBlock];
                LPMMCMPBLOCK pblk = (LPMMCMPBLOCK)(lpMemFile+dwMemPos);
                LPMMCMPSUBBLOCK psubblk = (LPMMCMPSUBBLOCK)(lpMemFile+dwMemPos+20);

                if ((dwMemPos + 20 >= dwMemLength) || (dwMemPos + 20 + pblk->sub_blk*8 >= dwMemLength)) break;
                dwMemPos += 20 + pblk->sub_blk*8;
#ifdef mmcmp_LOG
                Log("block %d: flags=%04X sub_blocks=%d", nBlock, (uint32_t)pblk->flags, (uint32_t)pblk->sub_blk);
                Log(" pksize=%d unpksize=%d", pblk->pk_size, pblk->unpk_size);
                Log(" tt_entries=%d num_bits=%d\n", pblk->tt_entries, pblk->num_bits);
#endif
                /* Data is not packed */
                if (!(pblk->flags & mmcmp_COMP))
                {
                        for (i=0; i<pblk->sub_blk; i++)
                        {
                                if ((psubblk->unpk_pos > dwFileSize) || (psubblk->unpk_pos + psubblk->unpk_size > dwFileSize)) break;
#ifdef mmcmp_LOG
                                Log("  Unpacked sub-block %d: offset %d, size=%d\n", i, psubblk->unpk_pos, psubblk->unpk_size);
#endif
                                memcpy(pBuffer+psubblk->unpk_pos, lpMemFile+dwMemPos, psubblk->unpk_size);
                                dwMemPos += psubblk->unpk_size;
                                psubblk++;
                        }
                } else
                /* Data is 16-bit packed */
                if (pblk->flags & mmcmp_16BIT)
                {
                        MMCMPBITBUFFER bb;
                        uint16_t * pDest = (uint16_t *)(pBuffer + psubblk->unpk_pos);
                        uint32_t dwSize = psubblk->unpk_size >> 1;
                        uint32_t dwPos = 0;
                        uint32_t numbits = pblk->num_bits;
                        uint32_t subblk = 0, oldval = 0;

#ifdef mmcmp_LOG
                        Log("  16-bit block: pos=%d size=%d ", psubblk->unpk_pos, psubblk->unpk_size);
                        if (pblk->flags & mmcmp_DELTA) Log("DELTA ");
                        if (pblk->flags & mmcmp_ABS16) Log("ABS16 ");
                        Log("\n");
#endif
                        bb.bitcount = 0;
                        bb.bitbuffer = 0;
                        bb.pSrc = lpMemFile+dwMemPos+pblk->tt_entries;
                        bb.pEnd = lpMemFile+dwMemPos+pblk->pk_size;
                        while (subblk < pblk->sub_blk)
                        {
                                uint32_t newval = 0x10000;
                                uint32_t d = GetBits(&bb, numbits+1);

                                if (d >= MMCMP16BitCommands[numbits])
                                {
                                        uint32_t nFetch = MMCMP16BitFetch[numbits];
                                        uint32_t newbits = GetBits(&bb, nFetch) + ((d - MMCMP16BitCommands[numbits]) << nFetch);
                                        if (newbits != numbits)
                                        {
                                                numbits = newbits & 0x0F;
                                        } else
                                        {
                                                if ((d = GetBits(&bb, 4)) == 0x0F)
                                                {
                                                        if (GetBits(&bb,1)) break;
                                                        newval = 0xFFFF;
                                                } else
                                                {
                                                        newval = 0xFFF0 + d;
                                                }
                                        }
                                } else
                                {
                                        newval = d;
                                }
                                if (newval < 0x10000)
                                {
                                        newval = (newval & 1) ? (uint32_t)(-(int32_t)((newval+1) >> 1)) : (uint32_t)(newval >> 1);
                                        if (pblk->flags & mmcmp_DELTA)
                                        {
                                                newval += oldval;
                                                oldval = newval;
                                        } else
                                        if (!(pblk->flags & mmcmp_ABS16))
                                        {
                                                newval ^= 0x8000;
                                        }
                                        pDest[dwPos++] = (uint16_t)newval;
                                }
                                if (dwPos >= dwSize)
                                {
                                        subblk++;
                                        dwPos = 0;
                                        dwSize = psubblk[subblk].unpk_size >> 1;
                                        pDest = (uint16_t *)(pBuffer + psubblk[subblk].unpk_pos);
                                }
                        }
                } else
                /* Data is 8-bit packed */
                {
                        MMCMPBITBUFFER bb;
                        uint8_t * pDest = pBuffer + psubblk->unpk_pos;
                        uint32_t dwSize = psubblk->unpk_size;
                        uint32_t dwPos = 0;
                        uint32_t numbits = pblk->num_bits;
                        uint32_t subblk = 0, oldval = 0;
                        uint8_t * ptable = lpMemFile+dwMemPos;

                        bb.bitcount = 0;
                        bb.bitbuffer = 0;
                        bb.pSrc = lpMemFile+dwMemPos+pblk->tt_entries;
                        bb.pEnd = lpMemFile+dwMemPos+pblk->pk_size;
                        while (subblk < pblk->sub_blk)
                        {
                                uint32_t newval = 0x100;
                                uint32_t d = GetBits(&bb,numbits+1);

                                if (d >= MMCMP8BitCommands[numbits])
                                {
                                        uint32_t nFetch = MMCMP8BitFetch[numbits];
                                        uint32_t newbits = GetBits(&bb,nFetch) + ((d - MMCMP8BitCommands[numbits]) << nFetch);
                                        if (newbits != numbits)
                                        {
                                                numbits = newbits & 0x07;
                                        } else
                                        {
                                                if ((d = GetBits(&bb,3)) == 7)
                                                {
                                                        if (GetBits(&bb,1)) break;
                                                        newval = 0xFF;
                                                } else
                                                {
                                                        newval = 0xF8 + d;
                                                }
                                        }
                                } else
                                {
                                        newval = d;
                                }
                                if (newval < 0x100)
                                {
                                        int n = ptable[newval];
                                        if (pblk->flags & mmcmp_DELTA)
                                        {
                                                n += oldval;
                                                oldval = n;
                                        }
                                        pDest[dwPos++] = (uint8_t)n;
                                }
                                if (dwPos >= dwSize)
                                {
                                        subblk++;
                                        dwPos = 0;
                                        dwSize = psubblk[subblk].unpk_size;
                                        pDest = pBuffer + psubblk[subblk].unpk_pos;
                                }
                        }
                }
        }
        *ppMemFile = pBuffer;
        *pdwMemLength = dwFileSize;
        return 1;
}

#if 0
int decrunch_mmcmp (FILE *f, FILE *fo)
{
        struct stat st;
        uint8_t *buf;
        uint32_t s;

        if (fo == NULL)
                return -1;

        if (fstat (fileno (f), &st))
                return -1;

        buf = malloc (s = st.st_size);
        fread (buf, 1, s, f);

        mmcmp_unpack (&buf, &s);

        fwrite (buf, 1, s, fo);

        free (buf);

        return 0;
}
#endif

