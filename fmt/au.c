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

#define NEED_BYTESWAP
#include "headers.h"
#include "fmt.h"


enum {
        AU_ULAW = 1,                    /* µ-law */
        AU_PCM_8 = 2,                   /* 8-bit linear PCM (RS_PCM8U in Modplug) */
        AU_PCM_16 = 3,                  /* 16-bit linear PCM (RS_PCM16M) */
        AU_PCM_24 = 4,                  /* 24-bit linear PCM */
        AU_PCM_32 = 5,                  /* 32-bit linear PCM */
        AU_IEEE_32 = 6,                 /* 32-bit IEEE floating point */
        AU_IEEE_64 = 7,                 /* 64-bit IEEE floating point */
        AU_ISDN_ULAW_ADPCM = 23,        /* 8-bit ISDN µ-law (CCITT G.721 ADPCM compressed) */
};

struct au_header {
        char magic[4]; /* ".snd" */
        unsigned long data_offset, data_size, encoding, sample_rate, channels;
};


/* --------------------------------------------------------------------- */

int fmt_au_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        struct au_header hh;

        if (!(length > 24 && memcmp(data, ".snd", 4) == 0))
                return false;

        memcpy(&hh, data, 24);

        if (!(hh.data_offset < length && hh.data_size > 0 && hh.data_size <= length - hh.data_offset))
                return false;

        file->smp_length = hh.data_size / hh.channels;
        file->smp_flags = 0;
        if (hh.encoding == AU_PCM_16) {
                file->smp_flags |= SAMP_16_BIT;
                file->smp_length /= 2;
        } else if (hh.encoding == AU_PCM_24) {
                file->smp_length /= 3;
        } else if (hh.encoding == AU_PCM_32 || hh.encoding == AU_IEEE_32) {
                file->smp_length /= 4;
        } else if (hh.encoding == AU_IEEE_64) {
                file->smp_length /= 8;
        }
        if (hh.channels >= 2) {
                file->smp_flags |= SAMP_STEREO;
        }
        file->description = "AU Sample";
        if (hh.data_offset > 24) {
                int extlen = hh.data_offset - 24;

                file->title = calloc(extlen + 1, sizeof(char));
                memcpy(file->title, data + 24, extlen);
                file->title[extlen] = 0;
        }
        file->smp_filename = file->title;
        file->type = TYPE_SAMPLE_PLAIN;
        return true;
}

/* --------------------------------------------------------------------- */

int fmt_au_load_sample(const uint8_t *data, size_t length, song_sample *smp, char *title)
{
        struct au_header au;

        if (length < 24)
                return false;

        memcpy(&au, data, sizeof(au));
        /* optimization: could #ifdef this out on big-endian machines */
        au.data_offset = bswapBE32(au.data_offset);
        au.data_size = bswapBE32(au.data_size);
        au.encoding = bswapBE32(au.encoding);
        au.sample_rate = bswapBE32(au.sample_rate);
        au.channels = bswapBE32(au.channels);

/*#define C__(cond) if (!(cond)) { log_appendf(2, "failed condition: %s", #cond); return false; }*/
#define C__(cond) if (!(cond)) { return false; }
        C__(memcmp(au.magic, ".snd", 4) == 0);
        C__(au.data_offset >= 24);
        C__(au.data_offset < length);
        C__(au.data_size > 0);
        C__(au.data_size <= length - au.data_offset);
        C__(au.encoding == AU_PCM_8 || au.encoding == AU_PCM_16);
        C__(au.channels == 1 || au.channels == 2);

        smp->speed = au.sample_rate;
        smp->volume = 64 * 4;
        smp->global_volume = 64;
        smp->length = au.data_size; /* maybe this should be MIN(...), for files with a wacked out length? */
        if (au.encoding == AU_PCM_16) {
                smp->flags |= SAMP_16_BIT;
                smp->length /= 2;
        }
        if (au.channels == 2) {
                smp->flags |= SAMP_STEREO;
                smp->length /= 2;
        }

        if (au.data_offset > 24) {
                int extlen = MIN(25, au.data_offset - 24);
                memcpy(title, data + 24, extlen);
                title[extlen] = 0;
        }

        smp->data = song_sample_allocate(au.data_size);
        memcpy(smp->data, data + au.data_offset, au.data_size);

#ifndef WORDS_BIGENDIAN
        /* maybe this could use swab()? */
        if (smp->flags & SAMP_16_BIT) {
                signed short *s = (signed short *) smp->data;
                unsigned long i = smp->length;
                if (smp->flags & SAMP_STEREO) i *= 2;
                while (i-- > 0) {
                        *s = bswapBE16(*s);
                        s++;
                }
        }
#endif

        return true;
}

/* --------------------------------------------------------------------------------------------------------- */

int fmt_au_save_sample(diskwriter_driver_t *fp, song_sample *smp, char *title)
{
        struct au_header au;
        unsigned long ln;

        memcpy(au.magic, ".snd", 4);

        au.data_offset = bswapBE32(49); // header is 24 bytes, sample name is 25
        ln = smp->length;
        if (smp->flags & SAMP_16_BIT) {
                ln *= 2;
                au.encoding = bswapBE32(AU_PCM_16);
        } else {
                au.encoding = bswapBE32(AU_PCM_8);
        }
        au.sample_rate = bswapBE32(smp->speed);
        if (smp->flags & SAMP_STEREO) {
                ln *= 2;
                au.channels = bswapBE32(2);
        } else {
                au.channels = bswapBE32(1);
        }
        au.data_size = bswapBE32(ln);

        fp->o(fp, (const unsigned char *)&au, sizeof(au));
        fp->o(fp, (const unsigned char *)title, 25);
        save_sample_data_BE(fp, smp, 0);

        return true;
}
