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

#include "it.h"
#include "song.h"

#include "sndfile.h"

#include <string.h>
#include <stdlib.h>

/* --------------------------------------------------------------------- */

#pragma pack(push, 1)

struct xm_point {
        uint16_t ticks;        // Time in tracker ticks
        uint16_t val;        // Value from 0x00 to 0x40.
};

struct xm_sample_header {
        uint32_t samplen;
        uint32_t loopstart;
        uint32_t looplen;
        uint8_t vol;
        signed char finetune;
        uint8_t type;
        uint8_t pan;
        signed char relnote;
        uint8_t res;
        char name[22];
};

struct xi_sample_header {
        uint8_t snum[96];

        union {
                uint16_t env[48];        // Occupies same mem as venv,penv
                struct {
                        struct xm_point venv[12], penv[12];
                };
        };

        uint8_t vnum, pnum;
        uint8_t vsustain, vloops, vloope, psustain, ploops, ploope;
        uint8_t vtype, ptype;
        uint8_t vibtype, vibsweep, vibdepth, vibrate;
        uint16_t volfade;
        uint8_t reserved1[0x16];
        uint16_t nsamples;
};

struct xi_file_header {
        int8_t header[0x15];    // "Extended Instrument: "
        int8_t name[0x16];      // Name of instrument
        uint8_t magic;          // 0x1a, DOS EOF char so you can 'type file.xi'
        int8_t tracker[0x14];   // Name of tracker
        uint16_t version;       // big-endian 0x0102
        struct xi_sample_header xish;
        struct xm_sample_header sheader[];
};
#pragma pack(pop)

static int validate_xi(const struct xi_file_header *xi, size_t length)
{
        if (length <= sizeof(struct xi_file_header))
                return 0;
        if (memcmp(xi->header, "Extended Instrument: ", 21) != 0)
                return 0;
        if (xi->magic != 0x1a)
                return 0;
        if (bswapLE16(xi->version) != 0x0102)
                return(0);
        return(1);
}

/* --------------------------------------------------------------------- */
int fmt_xi_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        struct xi_file_header *xi = (struct xi_file_header *) data;

        if (!validate_xi(xi, length))
                return 0;

        file->description = "FastTracker Instrument";
        file->title = mem_alloc(24);
        memcpy(file->title, xi->name, 22);
        file->title[22]='\0';
        file->type = TYPE_INST_XI;
        return 1;
}

int fmt_xi_load_instrument(const uint8_t *data, size_t length, int slot)
{
        const struct xi_file_header *xi = (const struct xi_file_header *) data;
        struct xi_sample_header xmsh;
        struct instrumentloader ii;
        song_instrument_t *g;
        const uint8_t *sampledata, *eof;
        int k, prevtick;

        if (!slot)
                return 0;
        if (!validate_xi(xi, length))
                return 0;

        eof = data + length;

        song_delete_instrument(slot);

        g = instrument_loader_init(&ii, slot);
        memcpy(g->name, xi->name, 22);
        g->name[22] = '\0';

        xmsh = xi->xish;

        for (k = 0; k < 96; k++) {
                if (xmsh.snum[k] > 15)
                        xmsh.snum[k] = 15;
                xmsh.snum[k] = instrument_loader_sample(&ii, xmsh.snum[k] + 1);
                g->note_map[k + 12] = k + 1 + 12;
                if (xmsh.snum[k])
                        g->sample_map[k + 12] = xmsh.snum[k];
        }

        for (k = 0; k < 12; k++) {
                g->note_map[k] = 0;
                g->sample_map[k] = 0;
                g->note_map[k + 108] = 0;
                g->sample_map[k + 108] = 0;
        }

        // bswap all volume and panning envelope points
        for (k = 0; k < 48; k++)
                xmsh.env[k] = bswapLE16(xmsh.env[k]);

        // Set up envelope types in instrument
        if (xmsh.vtype & 0x01) g->flags |= ENV_VOLUME;
        if (xmsh.vtype & 0x02) g->flags |= ENV_VOLSUSTAIN;
        if (xmsh.vtype & 0x04) g->flags |= ENV_VOLLOOP;
        if (xmsh.ptype & 0x01) g->flags |= ENV_PANNING;
        if (xmsh.ptype & 0x02) g->flags |= ENV_PANSUSTAIN;
        if (xmsh.ptype & 0x04) g->flags |= ENV_PANLOOP;

        prevtick = -1;
        // Copy envelopes into instrument
        for (k = 0; k < xmsh.vnum; k++) {
                if (xmsh.venv[k].ticks < prevtick)
                        prevtick++;
                else
                        prevtick = xmsh.venv[k].ticks;
                g->vol_env.ticks[k] = prevtick;
                g->vol_env.values[k] = xmsh.venv[k].val;
        }

        prevtick = -1;
        for (k = 0; k < xmsh.pnum; k++) {
                if (xmsh.penv[k].ticks < prevtick)
                        prevtick++;
                else
                        prevtick = xmsh.penv[k].ticks;
                g->pan_env.ticks[k] = prevtick;
                g->pan_env.values[k] = xmsh.penv[k].val;
        }

        g->vol_env.loop_start = xmsh.vloops;
        g->vol_env.loop_end = xmsh.vloope;
        g->vol_env.sustain_start = xmsh.vsustain;
        g->vol_env.nodes = xmsh.vnum;

        g->pan_env.loop_start = xmsh.ploops;
        g->pan_env.loop_end = xmsh.ploope;
        g->pan_env.sustain_start = xmsh.psustain;
        g->pan_env.nodes = xmsh.pnum;

        xmsh.volfade = bswapLE16(xmsh.volfade);
        xmsh.nsamples = bswapLE16(xmsh.nsamples);

        // Sample data begins at the end of the sample headers
        sampledata = (const uint8_t *) (xi->sheader + xmsh.nsamples);

        for (k = 0; k < xmsh.nsamples; k++) {
                struct xm_sample_header xmss = xi->sheader[k];
                song_sample_t *smp;
                unsigned int rs, samplesize, n;

                xmss.samplen = bswapLE32(xmss.samplen);
                xmss.loopstart = bswapLE32(xmss.loopstart);
                xmss.looplen = bswapLE32(xmss.looplen);

                rs = SF_LE | SF_PCMD; // endianness; encoding
                rs |= (xmss.type & 0x20) ? SF_SS : SF_M; // channels
                rs |= (xmss.type & 0x10) ? SF_16 : SF_8; // bits

                if (xmss.type & 0x10) {
                        xmss.looplen >>= 1;
                        xmss.loopstart >>= 1;
                        xmss.samplen >>= 1;
                }
                if (xmss.type & 0x20) {
                        xmss.looplen >>= 1;
                        xmss.loopstart >>= 1;
                        xmss.samplen >>= 1;
                }
                if (xmss.loopstart >= xmss.samplen)
                        xmss.type &= ~3;
                xmss.looplen += xmss.loopstart;
                if (xmss.looplen > xmss.samplen)
                        xmss.looplen = xmss.samplen;
                if (!xmss.looplen)
                        xmss.type &= ~3;

                n = instrument_loader_sample(&ii, k + 1);
                smp = song_get_sample(n);
                smp->flags = 0;
                memcpy(smp->name, xmss.name, 22);
                smp->name[21] = '\0';

                samplesize = xmss.samplen;
                smp->length = samplesize;
                smp->loop_start = xmss.loopstart;
                smp->loop_end = xmss.looplen;
                if (smp->loop_end > smp->loop_start)
                        smp->loop_end = smp->length;
                if (smp->loop_start >= smp->loop_end)
                        smp->loop_start = smp->loop_end = 0;
                if ((xmss.type & 0x03) == 0x01)
                        smp->flags |= CHN_LOOP;
                if ((xmss.type & 0x03) == 0x02)
                        smp->flags |= CHN_PINGPONGLOOP;
                smp->volume = xmss.vol << 2;
                if (smp->volume > 256)
                        smp->volume = 256;
                smp->global_volume = 64;
                smp->panning = xmss.pan;
                smp->flags |= CHN_PANNING;
                smp->vib_type = xmsh.vibtype;
                smp->vib_speed = xmsh.vibsweep;
                smp->vib_depth = xmsh.vibdepth;
                smp->vib_rate = xmsh.vibrate / 4; // XXX xm.c does not divide here, which is wrong?

                song_sample_set_c5speed(n, transpose_to_frequency(xmss.relnote, xmss.finetune));
                sampledata += csf_read_sample(current_song->samples + n, rs, sampledata, (eof-sampledata));
        }

        return 1;
}

