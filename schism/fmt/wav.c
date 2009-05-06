/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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

#include "diskwriter.h"

#define WAVE_FORMAT_PCM 1
#define WAVE_FORMAT_EXTENSIBLE  0xFFFE

// Standard IFF chunks IDs
#define IFFID_FORM              0x4d524f46
#define IFFID_RIFF              0x46464952
#define IFFID_WAVE              0x45564157
#define IFFID_LIST              0x5453494C
#define IFFID_INFO              0x4F464E49

// IFF Info fields
#define IFFID_ICOP              0x504F4349
#define IFFID_IART              0x54524149
#define IFFID_IPRD              0x44525049
#define IFFID_INAM              0x4D414E49
#define IFFID_ICMT              0x544D4349
#define IFFID_IENG              0x474E4549
#define IFFID_ISFT              0x54465349
#define IFFID_ISBJ              0x4A425349
#define IFFID_IGNR              0x524E4749
#define IFFID_ICRD              0x44524349

// Wave IFF chunks IDs
#define IFFID_wave              0x65766177
#define IFFID_fmt               0x20746D66
#define IFFID_wsmp              0x706D7377
#define IFFID_pcm               0x206d6370
#define IFFID_data              0x61746164
#define IFFID_smpl              0x6C706D73
#define IFFID_xtra              0x61727478


#pragma pack(push, 1)
typedef struct
{
    unsigned int id_RIFF;           // "RIFF"
    unsigned int filesize;          // file length-8
    unsigned int id_WAVE;
} WAVEFILEHEADER;


typedef struct
{
    unsigned int id_fmt;            // "fmt "
    unsigned int hdrlen;            // 16
    unsigned short format;          // 1
    unsigned short channels;        // 1:mono, 2:stereo
    unsigned int freqHz;            // sampling freq
    unsigned int bytessec;          // bytes/sec=freqHz*samplesize
    unsigned short samplesize;      // sizeof(sample)
    unsigned short bitspersample;   // bits per sample (8/16)
} WAVEFORMATHEADER;


typedef struct
{
    unsigned int id_data;           // "data"
    unsigned int length;            // length of data
} WAVEDATAHEADER;

#pragma pack(pop)


static int wav_read_fileheader(const byte *data, size_t len, WAVEFILEHEADER *hdr)
{
    if (!data || !hdr || len < sizeof(WAVEFILEHEADER))
        return false;

    memcpy(hdr, data, sizeof(WAVEFILEHEADER));
    hdr->id_RIFF  = bswapLE32(hdr->id_RIFF);
    hdr->filesize = bswapLE32(hdr->filesize);
    hdr->id_WAVE  = bswapLE32(hdr->id_WAVE);
    return true;
}


static int wav_read_formatheader(const byte *data, size_t len, WAVEFORMATHEADER *fmt)
{
    if (!data || !fmt || len < sizeof(WAVEFILEHEADER) + sizeof(WAVEFORMATHEADER))
        return false;

    memcpy(fmt, data + sizeof(WAVEFILEHEADER), sizeof(WAVEFORMATHEADER));
    fmt->id_fmt        = bswapLE32(fmt->id_fmt);
    fmt->hdrlen        = bswapLE32(fmt->hdrlen);
    fmt->format        = bswapLE16(fmt->format);
    fmt->channels      = bswapLE16(fmt->channels);
    fmt->freqHz        = bswapLE32(fmt->freqHz);
    fmt->bytessec      = bswapLE32(fmt->bytessec);
    fmt->samplesize    = bswapLE16(fmt->samplesize);
    fmt->bitspersample = bswapLE16(fmt->bitspersample);
    return true;
}


// Locate the data header
static int wav_read_data(const byte *data, size_t len, size_t *offset, WAVEDATAHEADER *dat)
{
    size_t pos = *offset;

    if (!data || !dat || !offset || len < (pos + sizeof(WAVEDATAHEADER)))
        return false;

    while (true) {
        memcpy(dat, data + pos, sizeof(WAVEDATAHEADER));

        dat->id_data = bswapLE32(dat->id_data);
        dat->length  = bswapLE32(dat->length);
        pos         += sizeof(WAVEDATAHEADER);

        if (pos + dat->length > len)
            return false;
        else if (dat->id_data == IFFID_data)
            break;

        pos += dat->length;
    }

    if (dat->length > len - pos)
        return false;

    *offset = pos;
    return true;
}


static int wav_load(const byte *data, size_t len, size_t *offset, WAVEFILEHEADER *hdr,
    WAVEFORMATHEADER *fmt, WAVEDATAHEADER *dat)
{
    if (!wav_read_fileheader(data, len, hdr))
        return false;
    else if (!wav_read_formatheader(data, len, fmt))
        return false;

    if (hdr->id_RIFF != IFFID_RIFF ||
        hdr->id_WAVE != IFFID_WAVE || 
        fmt->id_fmt != IFFID_fmt)
            return false;

    *offset = sizeof(WAVEFILEHEADER) + sizeof(WAVEFORMATHEADER);

    if (!wav_read_data(data, len, offset, dat))
        return false;

    return true;
}


int fmt_wav_load_sample(const byte *data, size_t len, song_sample *smp, UNUSED char *title)
{
    WAVEFILEHEADER   phdr;
    WAVEFORMATHEADER pfmt;
    WAVEDATAHEADER   pdata;
    size_t offset = 0;
    unsigned short bpp = 1;

    if (!wav_load(data, len, &offset, &phdr, &pfmt, &pdata))
        return false;

    if (pfmt.format != WAVE_FORMAT_PCM ||
        !pfmt.freqHz ||
        (pfmt.channels != 1 && pfmt.channels != 2) ||
        (pfmt.bitspersample != 8 && pfmt.bitspersample != 16))
            return false;

    smp->flags  = 0;

    if (pfmt.channels == 2)
        smp->flags |= SAMP_STEREO;

    if (pfmt.bitspersample == 16) {
        bpp = 2;
        smp->flags |= SAMP_16_BIT;
    }

    smp->speed  = pfmt.freqHz;
    smp->length = pdata.length / (bpp * pfmt.channels);
    smp->data   = song_sample_allocate(pdata.length);
    smp->volume = 64 * 4;
    smp->global_volume = 64;

    if (!smp->data)
        return false;

    switch (bpp) {
    case 1:
        memcpy(smp->data, data + offset, pdata.length);
        break;

    case 2: {
            signed short *src = (signed short *)(data + offset);
            signed short *dst = (signed short *)(smp->data);
            unsigned int range = pdata.length / 2;
 
            for (unsigned int i = 0; i < range; i++) {
                dst[i] = bswapLE16(src[i]);
            }

            break;
        }

    default:
        // Should never happen
        return false;
    }

    return true;
}


int fmt_wav_read_info(dmoz_file_t *file, const byte *data, size_t length)
{
    WAVEFILEHEADER   phdr;
    WAVEFORMATHEADER pfmt;
    WAVEDATAHEADER   pdata;
    size_t offset = 0;

    if (!wav_load(data, length, &offset, &phdr, &pfmt, &pdata))
        return false;

    file->smp_flags  = 0;

    if (pfmt.channels == 2)
        file->smp_flags |= SAMP_STEREO;

    if (pfmt.bitspersample == 16)
        file->smp_flags |= SAMP_16_BIT;

    file->smp_speed  = pfmt.freqHz;
    file->smp_length = pdata.length / ((pfmt.bitspersample == 16 ? 2 : 1) * pfmt.channels);

    file->description  = "IBM/Microsoft RIFF Audio";
    file->type         = TYPE_SAMPLE_PLAIN;
    file->smp_filename = file->base;
    return true;
}


// wavewriter
//
// Filesize and data length are updated by _wavout_tail
static void _wavout_header(diskwriter_driver_t *x)
{
        WAVEFILEHEADER hdr;
        WAVEFORMATHEADER fmt;
        WAVEDATAHEADER dat;

        hdr.id_RIFF  = bswapLE32(IFFID_RIFF);
        hdr.filesize = 0xFFFFFFFF;
        hdr.id_WAVE  = bswapLE32(IFFID_WAVE);
        x->o(x, (const byte*)&hdr, sizeof(hdr));

        fmt.id_fmt        = bswapLE32(IFFID_fmt);
        fmt.hdrlen        = bswapLE32(sizeof(WAVEFORMATHEADER) - 2 * sizeof(unsigned int));
        fmt.format        = bswapLE16(WAVE_FORMAT_PCM);
        fmt.channels      = bswapLE16(x->channels);
        fmt.freqHz        = bswapLE32(x->rate);
        fmt.bytessec      = bswapLE32(x->rate * x->channels * (x->bits / 8));
        fmt.samplesize    = bswapLE16(x->bits / 8);
        fmt.bitspersample = bswapLE16(x->bits);

        x->o(x, (const byte*)&fmt, sizeof(fmt));

        dat.id_data = bswapLE32(IFFID_data);
        dat.length  = 0xFFFFFFFF;
        x->o(x, (const byte*)&dat, sizeof(dat));
}


static void _wavout_tail(diskwriter_driver_t *x)
{
        off_t tt;
        unsigned int tmp;

        tt = x->pos;
        tt -= sizeof(WAVEDATAHEADER);

        x->l(x, sizeof(unsigned int));
        tmp = bswapLE32(tt);
        x->o(x, (const byte *) &tmp, sizeof(unsigned int));

        tt -= sizeof(unsigned int) + sizeof(WAVEFORMATHEADER) + sizeof(WAVEDATAHEADER);
        x->l(x, sizeof(WAVEFILEHEADER) + sizeof(WAVEFORMATHEADER) + sizeof(unsigned int));
        tmp = bswapLE32(tt);
        x->o(x, (const byte *) &tmp, sizeof(unsigned int));
}


static void _wavout_data(diskwriter_driver_t *x, const unsigned char *buf, unsigned int len)
{
        // endianess?
        x->o(x, buf, len);
}


diskwriter_driver_t wavewriter = {
    "WAV", "wav", 1,
    _wavout_header,
    _wavout_data,
    NULL, /* no midi data */
    _wavout_tail,
    NULL, NULL, NULL,
    NULL, /* setup page */
    NULL,
    44100, 16, 2, 1,
    0 /* pos */
};

/* --------------------------------------------------------------------------------- */

int fmt_wav_save_sample(diskwriter_driver_t *fp, song_sample *smp, UNUSED char *title)
{
        fp->rate = smp->speed;
        fp->channels = (smp->flags & SAMP_STEREO) ? 2 : 1;
        fp->bits = (smp->flags & SAMP_16_BIT) ? 16 : 8;
        _wavout_header(fp);
#if WORDS_BIGENDIAN
        if (fp->bits == 8) {
                /* no swapping required */
                _wavout_data(fp, (unsigned char *)smp->data,
                                smp->length * fp->channels);
        } else {
#define BUFS    4096
                unsigned short buffer[BUFS];
                unsigned short *q, *p, *end;
                unsigned int length;

                q = (unsigned short *)smp->data;
                length = smp->length;
                end = &buffer[BUFS-2];
                p = buffer;
                while (length > 0) {
                        if (p >= end) {
                                _wavout_data(fp, (unsigned char *)buffer,
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
                        _wavout_data(fp, (unsigned char *)buffer, ((char*)p)-((char*)buffer));
                }
        }
#else
        _wavout_data(fp, (unsigned char*)smp->data,
                        smp->length * (fp->bits / 8) * fp->channels);
#endif
        _wavout_tail(fp);
        return true;
}

