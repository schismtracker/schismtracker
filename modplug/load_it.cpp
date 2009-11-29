/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

#include "sndfile.h"
#include "util.h"

//////////////////////////////////////////////////////////////////////////////
// IT 2.14 compression

uint32_t ITReadBits(uint32_t *bitbuf, uint32_t *bitnum, uint8_t **ibuf, int8_t n)
{
        uint32_t retval = 0;
        uint32_t i = n;

        if (n > 0) {
                do {
                        if (!*bitnum) {
                                *bitbuf = *(*ibuf)++;
                                *bitnum = 8;
                        }
                        retval >>= 1;
                        retval |= (*bitbuf) << 31;
                        (*bitbuf) >>= 1;
                        (*bitnum)--;
                        i--;
                } while (i);
                i = n;
        }
        return (retval >> (32-i));
}

void ITUnpack8Bit(signed char *pSample, uint32_t dwLen, uint8_t * lpMemFile, uint32_t dwMemLength, int b215)
//-------------------------------------------------------------------------------------------
{
        signed char *pDst = pSample;
        uint8_t * pSrc = lpMemFile;
        uint32_t wHdr = 0;
        uint32_t wCount = 0;
        uint32_t bitbuf = 0;
        uint32_t bitnum = 0;
        uint8_t bLeft = 0, bTemp = 0, bTemp2 = 0;

        while (dwLen)
        {
                if (!wCount)
                {
                        wCount = 0x8000;
                        wHdr = bswapLE16(*((uint16_t *)pSrc));
                        pSrc += 2;
                        bLeft = 9;
                        bTemp = bTemp2 = 0;
                        bitbuf = bitnum = 0;
                }
                uint32_t d = wCount;
                if (d > dwLen) d = dwLen;
                // Unpacking
                uint32_t dwPos = 0;
                do
                {
                        uint16_t wBits = (uint16_t)ITReadBits(&bitbuf, &bitnum, &pSrc, bLeft);
                        if (bLeft < 7)
                        {
                                uint32_t i = 1 << (bLeft-1);
                                uint32_t j = wBits & 0xFFFF;
                                if (i != j) goto UnpackByte;
                                wBits = (uint16_t)(ITReadBits(&bitbuf, &bitnum, &pSrc, 3) + 1) & 0xFF;
                                bLeft = ((uint8_t)wBits < bLeft) ? (uint8_t)wBits : (uint8_t)((wBits+1) & 0xFF);
                                goto Next;
                        }
                        if (bLeft < 9)
                        {
                                uint16_t i = (0xFF >> (9 - bLeft)) + 4;
                                uint16_t j = i - 8;
                                if ((wBits <= j) || (wBits > i)) goto UnpackByte;
                                wBits -= j;
                                bLeft = ((uint8_t)(wBits & 0xFF) < bLeft) ? (uint8_t)(wBits & 0xFF) : (uint8_t)((wBits+1) & 0xFF);
                                goto Next;
                        }
                        if (bLeft >= 10) goto SkipByte;
                        if (wBits >= 256)
                        {
                                bLeft = (uint8_t)(wBits + 1) & 0xFF;
                                goto Next;
                        }
                UnpackByte:
                        if (bLeft < 8)
                        {
                                uint8_t shift = 8 - bLeft;
                                signed char c = (signed char)(wBits << shift);
                                c >>= shift;
                                wBits = (uint16_t)c;
                        }
                        wBits += bTemp;
                        bTemp = (uint8_t)wBits;
                        bTemp2 += bTemp;
                        pDst[dwPos] = (b215) ? bTemp2 : bTemp;
                SkipByte:
                        dwPos++;
                Next:
                        if (pSrc >= lpMemFile+dwMemLength+1) return;
                } while (dwPos < d);
                // Move On
                wCount -= d;
                dwLen -= d;
                pDst += d;
        }
}


void ITUnpack16Bit(signed char *pSample, uint32_t dwLen, uint8_t * lpMemFile, uint32_t dwMemLength, int b215)
//--------------------------------------------------------------------------------------------
{
        signed short *pDst = (signed short *)pSample;
        uint8_t * pSrc = lpMemFile;
        uint32_t wHdr = 0;
        uint32_t wCount = 0;
        uint32_t bitbuf = 0;
        uint32_t bitnum = 0;
        uint8_t bLeft = 0;
        signed short wTemp = 0, wTemp2 = 0;

        while (dwLen)
        {
                if (!wCount)
                {
                        wCount = 0x4000;
                        wHdr = bswapLE16(*((uint16_t *)pSrc));
                        pSrc += 2;
                        bLeft = 17;
                        wTemp = wTemp2 = 0;
                        bitbuf = bitnum = 0;
                }
                uint32_t d = wCount;
                if (d > dwLen) d = dwLen;
                // Unpacking
                uint32_t dwPos = 0;
                do
                {
                        uint32_t dwBits = ITReadBits(&bitbuf, &bitnum, &pSrc, bLeft);
                        if (bLeft < 7)
                        {
                                uint32_t i = 1 << (bLeft-1);
                                uint32_t j = dwBits;
                                if (i != j) goto UnpackByte;
                                dwBits = ITReadBits(&bitbuf, &bitnum, &pSrc, 4) + 1;
                                bLeft = ((uint8_t)(dwBits & 0xFF) < bLeft) ? (uint8_t)(dwBits & 0xFF) : (uint8_t)((dwBits+1) & 0xFF);
                                goto Next;
                        }
                        if (bLeft < 17)
                        {
                                uint32_t i = (0xFFFF >> (17 - bLeft)) + 8;
                                uint32_t j = (i - 16) & 0xFFFF;
                                if ((dwBits <= j) || (dwBits > (i & 0xFFFF))) goto UnpackByte;
                                dwBits -= j;
                                bLeft = ((uint8_t)(dwBits & 0xFF) < bLeft) ? (uint8_t)(dwBits & 0xFF) : (uint8_t)((dwBits+1) & 0xFF);
                                goto Next;
                        }
                        if (bLeft >= 18) goto SkipByte;
                        if (dwBits >= 0x10000)
                        {
                                bLeft = (uint8_t)(dwBits + 1) & 0xFF;
                                goto Next;
                        }
                UnpackByte:
                        if (bLeft < 16)
                        {
                                uint8_t shift = 16 - bLeft;
                                signed short c = (signed short)(dwBits << shift);
                                c >>= shift;
                                dwBits = (uint32_t)c;
                        }
                        dwBits += wTemp;
                        wTemp = (signed short)dwBits;
                        wTemp2 += wTemp;
                        pDst[dwPos] = (b215) ? wTemp2 : wTemp;
                SkipByte:
                        dwPos++;
                Next:
                        if (pSrc >= lpMemFile+dwMemLength+1) return;
                } while (dwPos < d);
                // Move On
                wCount -= d;
                dwLen -= d;
                pDst += d;
                if (pSrc >= lpMemFile+dwMemLength) break;
        }
}

