/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010 Storlek
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

/* --------------------------------------------------------------------- */

#define NEED_BYTESWAP
#include "headers.h"
#include "log.h"
#include "fmt.h"

#include <stdint.h>
#include <unistd.h> /* swab */
#include <math.h> /* for ldexp/frexp */

#ifdef WIN32
# define swab(a,b,c) swab((const char*)(a),(char*)(b),(size_t)(c))
#endif

static void ConvertToIeeeExtended(double num, unsigned char *bytes);
static double ConvertFromIeeeExtended(const unsigned char *bytes);

/* --------------------------------------------------------------------- */

#pragma pack(push, 1)
typedef union chunkdata {
        struct {
                uint32_t filetype; // AIFF, 8SVX, etc.
                uint8_t data[0]; // rest of file is encapsulated in here, also chunked
        } FORM;

        // 8SVX
        struct {
                uint32_t smp_highoct_1shot;
                uint32_t smp_highoct_repeat;
                uint32_t smp_cycle_highoct;
                uint16_t smp_per_sec;
                uint8_t num_octaves;
                uint8_t compression; // 0 = none, 1 = fibonacci-delta
                uint32_t volume; // fixed point, 65536 = 1.0
        } VHDR;

        // AIFF
        struct {
                uint16_t num_channels;
                uint32_t num_frames;
                uint16_t sample_size;
                uint8_t sample_rate[80]; // IEEE-extended
        } COMM;

        uint8_t bytes[0];
} chunkdata_t;
#pragma pack(pop)

typedef struct chunk {
        uint32_t id;
        uint32_t size;
        const chunkdata_t *data;
} chunk_t;

// other chunks that might exist: "NAME", "AUTH", "ANNO", "(c) "


// 'chunk' is filled in with the chunk header
// return: 0 if chunk overflows EOF, 1 if it was successfully read
// pos is updated to point to the beginning of the next chunk
static int iff_chunk_read(chunk_t *chunk, const uint8_t *data, size_t length, size_t *pos)
{
        if (*pos + 8 > length)
                return 0;
        memcpy(&chunk->id, data + *pos, 4);
        memcpy(&chunk->size, data + *pos + 4, 4);
        chunk->id = bswapBE32(chunk->id);
        chunk->size = bswapBE32(chunk->size);
        chunk->data = (chunkdata_t *) (data + *pos + 8);
        *pos += 8 + chunk->size;
        return (*pos <= length);
}

// Wish I could do this:
//#define ID(x) ((0[#x] << 24) | (1[#x] << 16) | (2[#x] << 8) | (3[#x]))
// It works great, but gcc doesn't think it's a constant, so it can't be used in a switch.
// (although with -O, it definitely does optimize it into a constant...)

#define ID_FORM 0x464F524D
#define ID_8SVX 0x38535658
#define ID_VHDR 0x56484452
#define ID_BODY 0x424F4459
#define ID_NAME 0x4E414D45
#define ID_AUTH 0x41555448
#define ID_ANNO 0x414E4E4F
#define ID__C__ 0x28632920 /* "(c) " */
#define ID_AIFF 0x41494646
#define ID_COMM 0x434F4D4D
#define ID_SSND 0x53534E44

/* --------------------------------------------------------------------- */

#define ZEROIZE(x) memset(&(x), 0, sizeof(x))

static int _read_iff(dmoz_file_t *file, song_sample *smp, char *title, const uint8_t *data, size_t length)
{
        chunk_t chunk;
        size_t pos = 0;
        chunk_t vhdr, body, name, comm, auth, anno, ssnd; // butt

        if (!iff_chunk_read(&chunk, data, length, &pos))
                return 0;
        if (chunk.id != ID_FORM)
                return 0;

        // jump "into" the FORM chunk
        // if (pos < length), there's more data after the FORM chunk -- but I don't care about this scenario
        pos = 0;
        length = chunk.size;
        data = chunk.data->FORM.data;

        /* the header is already byteswapped, but anything in 'chunk' will need to be swapped as needed
        because the structure is a const pointing into the data itself */
        switch (bswapBE32(chunk.data->FORM.filetype)) {
        case ID_8SVX:
                // shut up, gcc
                ZEROIZE(vhdr);
                ZEROIZE(body);
                ZEROIZE(name);
                ZEROIZE(auth);
                ZEROIZE(anno);

                while (iff_chunk_read(&chunk, data, length, &pos)) {
                        switch (chunk.id) {
                                case ID_VHDR: vhdr = chunk; break;
                                case ID_BODY: body = chunk; break;
                                case ID_NAME: name = chunk; break;
                                case ID_AUTH: auth = chunk; break;
                                case ID_ANNO: anno = chunk; break;
                                default: break;
                        }
                }
                if (!(vhdr.id && body.id))
                        return 0;

                if (vhdr.data->VHDR.compression) {
                        log_appendf(4, "error: compressed 8SVX files are unsupported");
                        return 0;
                }
                if (vhdr.data->VHDR.num_octaves != 1) {
                        log_appendf(4, "warning: 8SVX file contains %d octaves",
                                vhdr.data->VHDR.num_octaves);
                }

                if (file) {
                        file->description = "8SVX sample";
                        file->type = TYPE_SAMPLE_PLAIN;
                }
                if (!name.id) name = auth;
                if (!name.id) name = anno;
                if (name.id) {
                        if (file) {
                                file->title = calloc(1, name.size + 1);
                                memcpy(file->title, name.data->bytes, name.size);
                                file->title[name.size] = '\0';
                        }
                        if (title) {
                                int len = MIN(25, name.size);
                                memcpy(title, name.data->bytes, len);
                                title[len] = 0;
                        }
                }

                if (smp) {
                        smp->speed = bswapBE16(vhdr.data->VHDR.smp_per_sec);
                        smp->length = body.size;
                        smp->data = song_sample_allocate(body.size);
                        memcpy(smp->data, body.data->bytes, body.size);
                        smp->volume = 64*4;
                        smp->global_volume = 64;

                        // this is done kinda weird
                        smp->loop_end = bswapBE32(vhdr.data->VHDR.smp_highoct_repeat);
                        if (smp->loop_end) {
                                smp->loop_start = bswapBE32(vhdr.data->VHDR.smp_highoct_1shot);
                                smp->loop_end += smp->loop_start;
                                if (smp->loop_start > smp->length)
                                        smp->loop_start = 0;
                                if (smp->loop_end > smp->length)
                                        smp->loop_end = smp->length;
                                if (smp->loop_start + 2 < smp->loop_end)
                                        smp->flags |= SAMP_LOOP;
                        }
                        // TODO vhdr.data->VHDR.volume ?
                }

                return 1;

        case ID_AIFF:
                ZEROIZE(comm);
                ZEROIZE(ssnd);
                ZEROIZE(name);
                ZEROIZE(auth);
                ZEROIZE(anno);

                while (iff_chunk_read(&chunk, data, length, &pos)) {
                        switch (chunk.id) {
                                case ID_COMM: comm = chunk; break;
                                case ID_SSND: ssnd = chunk; break;
                                case ID_NAME: name = chunk; break;
                                default: break;
                        }
                }
                if (!(comm.id && ssnd.id))
                        return 0;

                if (file) {
                        file->description = "Audio IFF sample";
                        file->type = TYPE_SAMPLE_PLAIN;
                }
                if (!name.id) name = auth;
                if (!name.id) name = anno;
                if (name.id) {
                        if (file) {
                                file->title = calloc(1, name.size + 1);
                                memcpy(file->title, name.data->bytes, name.size);
                                file->title[name.size] = '\0';
                        }
                        if (title) {
                                int len = MIN(25, name.size);
                                memcpy(title, name.data->bytes, len);
                                title[len] = 0;
                        }
                }

                if (smp) {
                        switch (bswapBE16(comm.data->COMM.num_channels)) {
                        default:
                                log_appendf(4, "warning: multichannel AIFF is unsupported");
                        case 1:
                                break;
                        case 2:
                                smp->flags |= SAMP_STEREO;
                                break;
                        }

                        switch ((bswapBE16(comm.data->COMM.sample_size) + 7) & ~7) {
                        default:
                                log_appendf(4, "warning: AIFF has unsupported bit-width");
                        case 8:
                                break;
                        case 16:
                                smp->flags |= SAMP_16_BIT;
                                break;
                        }

                        // TODO: data checking; make sure sample count and byte size agree
                        // (and if not, cut to shorter of the two)

                        smp->speed = ConvertFromIeeeExtended(comm.data->COMM.sample_rate);
                        smp->length = bswapBE32(comm.data->COMM.num_frames);
                        smp->data = song_sample_allocate(ssnd.size);
                        smp->volume = 64*4;
                        smp->global_volume = 64;

#ifdef WORDS_BIGENDIAN
                        memcpy(smp->data, ssnd.data, ssnd.size);
#else
                        if (smp->flags & SAMP_16_BIT) {
                                swab(ssnd.data, smp->data, ssnd.size & ~1);
                        } else {
                                memcpy(smp->data, ssnd.data, ssnd.size);
                        }
#endif
                }

                return 1;
        }

        return 0;
}

/* --------------------------------------------------------------------- */

int fmt_aiff_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        return _read_iff(file, NULL, NULL, data, length);
}

int fmt_aiff_load_sample(const uint8_t *data, size_t length, song_sample *smp, char *title)
{
        return _read_iff(NULL, smp, title, data, length);
}

/* --------------------------------------------------------------------- */

int fmt_aiff_save_sample(diskwriter_driver_t *fp, song_sample *smp, char *title)
{
        int16_t s;
        uint32_t ul;
        int tlen, bps = (smp->flags & SAMP_16_BIT) ? 2 : 1;
        unsigned char b[10];

        /* File header
               ID ckID;
               int32_t ckSize;
               ID formType; */
        fp->o(fp, (const unsigned char *)"FORM\1\1\1\1AIFFNAME", 16);

        /* NAME chunk
               ID ckID;
               int32_t ckSize;
               char text[]; */
        tlen = strlen(title);
        if (tlen & 1)
                tlen++; /* must be even */
        ul = bswapBE32(tlen);
        fp->o(fp, (const unsigned char *)&ul, 4);
        fp->o(fp, (const unsigned char *)title, tlen);

        /* COMM chunk
               ID ckID;
               int32_t ckSize;
               int16_t numChannels;
               uint32_t numSampleFrames;
               int16_t sampleSize;
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
               int32_t ckSize;
               uint32_t offset;
               uint32_t blockSize;
               uint8_t soundData[]; */
        fp->o(fp, (const unsigned char *)"SSND", 4);
        ul = smp->length * bps;
        ul = bswapBE32(ul);
        fp->o(fp, (const unsigned char *)&ul, 4);
        ul = bswapBE32(0);
        fp->o(fp, (const unsigned char *)&ul, 4);
        fp->o(fp, (const unsigned char *)&ul, 4);

#if !WORDS_BIGENDIAN
        if (fp->bits == 8) {
                /* no swapping required */
                fp->o(fp, (const unsigned char *)smp->data, smp->length * bps);
        } else {
#define BUFS    4096
                uint16_t buffer[BUFS];
                uint16_t *q, *p, *end;
                uint32_t length;

                q = (uint16_t *)smp->data;
                length = smp->length;
                end = &buffer[BUFS-2];
                p = buffer;
                while (length > 0) {
                        if (p >= end) {
                                fp->o(fp, (unsigned char *)buffer,
                                                ((char*)p)-((char*)buffer));
                                p = buffer;
                        }
                        *p = bswap_16(*q);
                        q++; p++;
                        if (smp->flags & SAMP_STEREO) {
                                *p = bswap_16(*q);
                                q++; p++;
                        }
                        length--;
                }
                if (p != buffer) {
                        fp->o(fp, (unsigned char *)buffer, ((char*)p)-((char*)buffer));
                }
        }
#else
        fp->o(fp, (const unsigned char *)smp->data, smp->length * bps);
#endif

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

#define FloatToUnsigned(f) ((uint32_t) (((int32_t) (f - 2147483648.0)) + 2147483647L + 1))
#define UnsignedToFloat(u) (((double) ((int32_t) (u - 2147483647L - 1))) + 2147483648.0)

static void ConvertToIeeeExtended(double num, unsigned char *bytes)
{
        int sign, expon;
        double fMant, fsMant;
        uint32_t hiMant, loMant;

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
        uint32_t hiMant, loMant;

        expon = ((bytes[0] & 0x7F) << 8) | (bytes[1] & 0xFF);
        hiMant = ((uint32_t) (bytes[2] & 0xFF) << 24)
                | ((uint32_t) (bytes[3] & 0xFF) << 16)
                | ((uint32_t) (bytes[4] & 0xFF) << 8)
                | ((uint32_t) (bytes[5] & 0xFF));
        loMant = ((uint32_t) (bytes[6] & 0xFF) << 24)
                | ((uint32_t) (bytes[7] & 0xFF) << 16)
                | ((uint32_t) (bytes[8] & 0xFF) << 8)
                | ((uint32_t) (bytes[9] & 0xFF));

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
