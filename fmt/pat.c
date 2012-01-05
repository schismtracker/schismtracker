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
#define NEED_BYTESWAP
#include "headers.h"
#include "fmt.h"

#include "it.h"
#include "song.h"
#include "sndfile.h"

#include <string.h>
#include <stdlib.h>
#include <stdint.h>

/* --------------------------------------------------------------------- */

#pragma pack(push, 1)
struct GF1PatchHeader {
        uint8_t sig[8]; // "GF1PATCH"
        uint8_t ver[4]; // "100\0" or "110\0"
        uint8_t id[10]; // "ID#000002\0"
        char desc[60]; // Discription (in ASCII) [sic]
        uint8_t insnum; // To some patch makers, 0 means 1 [what?]
        uint8_t voicenum; // Voices (Always 14?)
        uint8_t channum; // Channels
        uint16_t waveforms;
        uint16_t mastervol; // 0-127 [then why is it 16-bit? ugh]
        uint32_t datasize;
        uint8_t reserved1[36];
        uint16_t insID; // Instrument ID [0..0xFFFF] [?]
        char insname[16]; // Instrument name (in ASCII)
        uint32_t inssize; // Instrument size
        uint8_t layers;
        uint8_t reserved2[40];
        uint8_t layerduplicate;
        uint8_t layer;
        uint32_t layersize;
        uint8_t smpnum;
        uint8_t reserved3[40];
};

struct GF1PatchSampleHeader {
        char wavename[7]; // Wave name (in ASCII)
        uint8_t fractions; // bits 0-3 loop start frac / 4-7 loop end frac
        uint32_t samplesize; // Sample data size (s)
        uint32_t loopstart;
        uint32_t loopend;
        uint16_t samplerate;
        uint32_t lofreq; // Low frequency
        uint32_t hifreq; // High frequency
        uint32_t rtfreq; // Root frequency
        uint16_t tune; // Tune (Always 1, not used anymore)
        uint8_t panning; // Panning (L=0 -> R=15)
        uint8_t envelopes[12];
        uint8_t trem_speed, trem_rate, trem_depth;
        uint8_t vib_speed, vib_rate, vib_depth;
        uint8_t smpmode; // bit mask: 16, unsigned, loop, pingpong, reverse, sustain, envelope, clamped release
        uint16_t scalefreq; // Scale frequency
        uint16_t scalefac; // Scale factor [0..2048] (1024 is normal)
        uint8_t reserved[36];
};
#pragma pack(pop)

/* --------------------------------------------------------------------- */

static int gusfreq(unsigned int freq)
{
        unsigned int scale_table[109] = {
/*C-0..B-*/
/* Octave 0 */  16351, 17323, 18354, 19445, 20601, 21826,
                23124, 24499, 25956, 27500, 29135, 30867,
/* Octave 1 */  32703, 34647, 36708, 38890, 41203, 43653,
                46249, 48999, 51913, 54999, 58270, 61735,
/* Octave 2 */  65406, 69295, 73416, 77781, 82406, 87306,
                92498, 97998, 103826, 109999, 116540, 123470,
/* Octave 3 */  130812, 138591, 146832, 155563, 164813, 174614,
                184997, 195997, 207652, 219999, 233081, 246941,
/* Octave 4 */  261625, 277182, 293664, 311126, 329627, 349228,
                369994, 391995, 415304, 440000, 466163, 493883,
/* Octave 5 */  523251, 554365, 587329, 622254, 659255, 698456,
                739989, 783991, 830609, 880000, 932328, 987767,
/* Octave 6 */  1046503, 1108731, 1174660, 1244509, 1318511, 1396914,
                1479979, 1567983, 1661220, 1760002, 1864657, 1975536,
/* Octave 7 */  2093007, 2217464, 2349321, 2489019, 2637024, 2793830,
                2959960, 3135968, 3322443, 3520006, 3729316, 3951073,
/* Octave 8 */  4186073, 4434930, 4698645, 4978041, 5274051, 5587663,
                5919922, 6271939, 6644889, 7040015, 7458636, 7902150,
                0xFFFFFFFF,
        };
        int no;

        for (no = 0; scale_table[no] != 0xFFFFFFFF; no++) {
                if (scale_table[no] <= freq && scale_table[no + 1] >= freq) {
                        return no - 12;
                }
        }

        return 4 * 12;
}

/* --------------------------------------------------------------------- */

int fmt_pat_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        const struct GF1PatchHeader *header = (const struct GF1PatchHeader *) data;

        if ((length <= sizeof(struct GF1PatchHeader))
            || (memcmp(header->sig, "GF1PATCH", 8) != 0)
            || (memcmp(header->ver, "110\0", 4) != 0 && memcmp(header->ver, "100\0", 4) != 0)
            || (memcmp(header->id, "ID#000002\0", 10) != 0)) {
                return 0;
        }
        file->description = "Gravis Patch File";
        file->title = malloc(17);
        memcpy(file->title, header->insname, 16);
        file->title[16] = '\0';
        file->type = TYPE_INST_OTHER;
        return 1;
}


int fmt_pat_load_instrument(const uint8_t *data, size_t length, int slot)
{
        struct GF1PatchHeader header;
        struct GF1PatchSampleHeader gfsamp;
        struct instrumentloader ii;
        song_instrument_t *g;
        song_sample_t *smp;
        unsigned int pos, rs;
        int lo, hi, tmp, i, nsamp, n;

        if (length < sizeof(header) || !slot) return 0;
        memcpy(&header, data, sizeof(header));
        if ((memcmp(header.sig, "GF1PATCH", 8) != 0)
            || (memcmp(header.ver, "110\0", 4) != 0 && memcmp(header.ver, "100\0", 4) != 0)
            || (memcmp(header.id, "ID#000002\0", 10) != 0)) {
                return 0;
        }

        header.waveforms = bswapLE16(header.waveforms);
        header.mastervol = bswapLE16(header.mastervol);
        header.datasize  = bswapLE32(header.datasize);
        header.insID     = bswapLE16(header.insID);
        header.inssize   = bswapLE32(header.inssize);
        header.layersize = bswapLE32(header.layersize);

        g = instrument_loader_init(&ii, slot);
        memcpy(g->name, header.insname, 16);
        g->name[15] = '\0';

        nsamp = CLAMP(header.smpnum, 1, 16);
        pos = sizeof(header);
        for (i = 0; i < 120; i++) {
                g->sample_map[i] = 0;
                g->note_map[i] = i + 1;
        }
        for (i = 0; i < nsamp; i++) {
                memcpy(&gfsamp, data + pos, sizeof(gfsamp));
                pos += sizeof(gfsamp);

                n = instrument_loader_sample(&ii, i + 1) - 1;
                smp = song_get_sample(n);

                gfsamp.samplesize = bswapLE32(gfsamp.samplesize);
                gfsamp.loopstart = bswapLE32(gfsamp.loopstart);
                gfsamp.loopend = bswapLE32(gfsamp.loopend);
                gfsamp.samplerate = bswapLE16(gfsamp.samplerate);
                gfsamp.lofreq = bswapLE32(gfsamp.lofreq);
                gfsamp.hifreq = bswapLE32(gfsamp.hifreq);
                gfsamp.rtfreq = bswapLE32(gfsamp.rtfreq);
                gfsamp.tune = bswapLE16(gfsamp.tune);
                gfsamp.scalefreq = bswapLE16(gfsamp.scalefac);

                lo = CLAMP(gusfreq(gfsamp.lofreq), 0, 95);
                hi = CLAMP(gusfreq(gfsamp.hifreq), 0, 95);
                if (lo > hi) {
                        tmp = lo;
                        lo = hi;
                        hi = tmp;
                }
                for (; lo < hi; lo++) {
                        g->sample_map[lo + 12] = n;
                }

                if (gfsamp.smpmode & 1) {
                        gfsamp.samplesize >>= 1;
                        gfsamp.loopstart >>= 1;
                        gfsamp.loopend >>= 1;
                }
                smp->length = gfsamp.samplesize;
                smp->loop_start = smp->sustain_start = gfsamp.loopstart;
                smp->loop_end = smp->sustain_end = gfsamp.loopend;
                smp->c5speed = gfsamp.samplerate;

                smp->flags = 0;
                rs = SF_M | SF_LE; // channels; endianness
                rs |= (gfsamp.smpmode & 1) ? SF_16 : SF_8; // bit width
                rs |= (gfsamp.smpmode & 2) ? SF_PCMU : SF_PCMS; // encoding
                if (gfsamp.smpmode & 32) {
                        if (gfsamp.smpmode & 4)
                                smp->flags |= CHN_SUSTAINLOOP;
                        if (gfsamp.smpmode & 8)
                                smp->flags |= CHN_PINGPONGSUSTAIN;
                } else {
                        if (gfsamp.smpmode & 4)
                                smp->flags |= CHN_LOOP;
                        if (gfsamp.smpmode & 8)
                                smp->flags |= CHN_PINGPONGLOOP;
                }
                memcpy(smp->filename, gfsamp.wavename, 7);
                smp->filename[8] = '\0';
                strcpy(smp->name, smp->filename);
                smp->vib_speed = gfsamp.vib_speed;
                smp->vib_rate = gfsamp.vib_rate;
                smp->vib_depth = gfsamp.vib_depth;

                pos += csf_read_sample(current_song->samples + n, rs, data + pos, length - pos);
        }
        return 1;
}

