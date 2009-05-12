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
#include "it.h"
#include "diskwriter.h"
#include <stdint.h>

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
typedef struct {
    uint32_t id_RIFF;           // "RIFF"
    uint32_t filesize;          // file length-8
    uint32_t id_WAVE;
} wave_file_header_t;



typedef struct {
    uint16_t format;          // 1
    uint16_t channels;        // 1:mono, 2:stereo
    uint32_t freqHz;          // sampling freq
    uint32_t bytessec;        // bytes/sec=freqHz*samplesize
    uint16_t samplesize;      // sizeof(sample)
    uint16_t bitspersample;   // bits per sample (8/16)
} wave_format_t;


typedef struct {
    uint32_t id;
    uint32_t length;
} wave_chunk_prefix_t;


typedef struct {
    wave_format_t fmt;        // Format
    wave_chunk_prefix_t data; // Data header
    uint8_t *buf;
} wave_file_t;
#pragma pack(pop)


static int wave_transform_data(const wave_format_t *fmt, const uint8_t *data, uint8_t *out, size_t len)
{
#if BYTE_ORDER == BIG_ENDIAN
        if (fmt->bitspersample == 8) {
#else
        if (fmt->bitspersample == 8 || fmt->bitspersample == 16) {
#endif
                memcpy(out, data, len);
                return 1;
        }

        switch (fmt->bitspersample) {
        case 16:
                swab(data, out, len);
                return 1;

        default:
                log_appendf(4, "Warning: %u bps WAV files are not supported\n", fmt->bitspersample);
                return 0;
        }
}


static int wav_load(wave_file_t *f, const uint8_t *data, size_t len)
{
        wave_file_header_t phdr;
        size_t offset;
        int have_format = 0;

        if (len < sizeof(wave_file_header_t)) {
                return 0;
        }

        memcpy(&phdr, data, sizeof(wave_file_header_t));
#if WORDS_BIGENDIAN
        phdr.id_RIFF  = bswapLE32(phdr.id_RIFF);
        phdr.filesize = bswapLE32(phdr.filesize);
        phdr.id_WAVE  = bswapLE32(phdr.id_WAVE);
#endif

        if (phdr.id_RIFF != IFFID_RIFF ||
            phdr.id_WAVE != IFFID_WAVE) {
                return 0;
        }

        offset = sizeof(wave_file_header_t);

        while (1) {
                wave_chunk_prefix_t c;
                memcpy(&c, data + offset, sizeof(wave_chunk_prefix_t));

#if WORDS_BIGENDIAN
                c.id     = bswapLE32(c.id);
                c.length = bswapLE32(c.length);
#endif
                offset  += sizeof(wave_chunk_prefix_t);

                if (offset + c.length > len) {
                        log_appendf(4, "Corrupt WAV file. Chunk points outside of WAV file [%lu + %u > %lu]\n",
                            offset, c.length, len);
                        return 0;
                }

                switch (c.id) {
                case IFFID_fmt: {
                        if (have_format) {
                                log_appendf(4, "Corrupt WAV file. Found multiple format headers.\n");
                                return 0;
                        }

                        have_format = 1;
                        memcpy(&f->fmt, data + offset, sizeof(wave_format_t));
#if WORDS_BIGENDIAN
                        f->fmt.format        = bswapLE16(f->fmt.format);
                        f->fmt.channels      = bswapLE16(f->fmt.channels);
                        f->fmt.freqHz        = bswapLE32(f->fmt.freqHz);
                        f->fmt.bytessec      = bswapLE32(f->fmt.bytessec);
                        f->fmt.samplesize    = bswapLE16(f->fmt.samplesize);
                        f->fmt.bitspersample = bswapLE16(f->fmt.bitspersample);
#endif
                        break;
                }

                case IFFID_data:
                        if (!have_format) {
                                log_appendf(4, "WAV file did not specify format before data\n");
                                return 0;
                        }

                        memcpy(&f->data, &c, sizeof(wave_chunk_prefix_t));
                        f->buf = (uint8_t *)(data + offset);
                        return 1;
                }
        
            offset += c.length;

            if (offset == len)
                    break;
        }

        return 1;
}


int fmt_wav_load_sample(const byte *data, size_t len, song_sample *smp, UNUSED char *title)
{
        wave_file_t f;

        if (!wav_load(&f, data, len))
                return false;

        if (f.fmt.format != WAVE_FORMAT_PCM ||
            !f.fmt.freqHz ||
            (f.fmt.channels != 1 && f.fmt.channels != 2) ||
            (f.fmt.bitspersample != 8 && f.fmt.bitspersample != 16))
                return false;

        smp->flags  = 0;

        if (f.fmt.channels == 2)
                smp->flags |= SAMP_STEREO;

        if (f.fmt.bitspersample == 16)
                smp->flags |= SAMP_16_BIT;

        smp->volume        = 64 * 4;
        smp->global_volume = 64;
        smp->speed         = f.fmt.freqHz;
        smp->length        = f.data.length / ((f.fmt.bitspersample / 8) * f.fmt.channels);
        smp->data          = song_sample_allocate(f.data.length);

        if (!smp->data)
                return false;

        if (!wave_transform_data(&f.fmt, f.buf, (uint8_t *)smp->data, f.data.length))
                return false;

        return true;
}


int fmt_wav_read_info(dmoz_file_t *file, const byte *data, size_t length)
{
        wave_file_t f;

        if (!wav_load(&f, data, length))
                return false;
        else if (f.fmt.format != WAVE_FORMAT_PCM ||
                !f.fmt.freqHz ||
                (f.fmt.channels != 1 && f.fmt.channels != 2) ||
                (f.fmt.bitspersample != 8 && f.fmt.bitspersample != 16))
                return false;

        file->smp_flags  = 0;

        if (f.fmt.channels == 2)
                file->smp_flags |= SAMP_STEREO;

        if (f.fmt.bitspersample == 16)
                file->smp_flags |= SAMP_16_BIT;

        file->smp_speed  = f.fmt.freqHz;
        file->smp_length = f.data.length / ((f.fmt.bitspersample / 8) * f.fmt.channels);

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
        wave_file_header_t  hdr;
        wave_format_t       fmt;
        wave_chunk_prefix_t pfx;

        hdr.id_RIFF  = bswapLE32(IFFID_RIFF);
        hdr.filesize = 0x0BBC0DE0;
        hdr.id_WAVE  = bswapLE32(IFFID_WAVE);
        x->o(x, (const uint8_t*) &hdr, sizeof(hdr));

        pfx.id     = bswapLE32(IFFID_fmt);
        pfx.length = bswapLE32(sizeof(wave_format_t));
        x->o(x, (const uint8_t*) &pfx, sizeof(pfx));

        fmt.format        = bswapLE16(WAVE_FORMAT_PCM);
        fmt.channels      = bswapLE16(x->channels);
        fmt.freqHz        = bswapLE32(x->rate);
        fmt.bytessec      = bswapLE32(x->rate * x->channels * (x->bits / 8));
        fmt.samplesize    = bswapLE16(x->bits / 8);
        fmt.bitspersample = bswapLE16(x->bits);
        x->o(x, (const uint8_t*) &fmt, sizeof(fmt));

        pfx.id     = bswapLE32(IFFID_data);
        pfx.length = 0x0BBC0DE0;
        x->o(x, (const uint8_t*) &pfx, sizeof(pfx));
}


static void _wavout_tail(diskwriter_driver_t *x)
{
        off_t tt;
        uint32_t tmp;

        // File size after RIFF id
        tt = x->pos;
        tt -= sizeof(wave_chunk_prefix_t);
        tmp = bswapLE32(tt);

        // Skip RIFF
        x->l(x, sizeof(uint32_t));
        x->o(x, (const uint8_t*) &tmp, sizeof(uint32_t));

        // File size after format header and so on
        tt -= sizeof(wave_format_t) + (2 * sizeof(wave_chunk_prefix_t)) + sizeof(uint32_t);
        tmp = bswapLE32(tt);

        x->l(x, sizeof(wave_file_header_t) +
                sizeof(wave_chunk_prefix_t) +
                sizeof(wave_format_t) +
                sizeof(uint32_t));
        x->o(x, (const uint8_t*) &tmp, sizeof(uint32_t));
}


static void _wavout_data(diskwriter_driver_t *x, const uint8_t *buf, uint32_t len)
{
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


int fmt_wav_save_sample(diskwriter_driver_t *fp, song_sample *smp, UNUSED char *title)
{
        fp->rate     = smp->speed;
        fp->channels = (smp->flags & SAMP_STEREO) ? 2 : 1;
        fp->bits     = (smp->flags & SAMP_16_BIT) ? 16 : 8;

        _wavout_header(fp);

#if WORDS_BIGENDIAN
        if (fp->bits == 8) {
                /* no swapping required */
                _wavout_data(fp, (uint8_t *) smp->data, smp->length * fp->channels);
        }
        else {
#define BUFS 4096
                uint16_t buffer[BUFS];
                uint16_t *q, *p, *end;
                uint32_t length;

                q      = (uint16_t *)smp->data;
                length = smp->length;
                end    = &buffer[BUFS - 2];
                p      = buffer;

                while (length > 0) {
                        if (p >= end) {
                                _wavout_data(fp, (uint8_t *)buffer,
                                                ((char*)p)-((char*)buffer));
                                p = buffer;
                        }

                        *p = bswap_16(*q);
                        q++;
                        p++;

                        if (smp->flags & SAMP_STEREO) {
                                *p = bswap_16(*q);
                                q++;
                                p++;
                        }

                        length--;
                }

                if (p != buffer) {
                printf("Failed to read file header.\n");
                        _wavout_data(fp, (uint8_t *) buffer, ((char *) p) - ((char *)buffer));
                }
        }
#else
        _wavout_data(fp, (uint8_t *) smp->data, smp->length * (fp->bits / 8) * fp->channels);
#endif
        _wavout_tail(fp);
        return true;
}

