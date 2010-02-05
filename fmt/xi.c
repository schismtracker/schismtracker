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

#ifndef WIN32
#endif

#include "snd_fx.h"
#include "xm_defs.h"

#include <string.h>
#include <stdlib.h>

/* --------------------------------------------------------------------- */
int fmt_xi_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        if (length <= 86) return 0;
        if (memcmp(data, "Extended Instrument: ", 21) != 0) return 0;
        if (data[43] != 26) return 0;
        if (memcmp(data+44, "FastTracker v", 13) != 0) return 0;
        file->description = "FastTracker Instrument";
        file->title = mem_alloc(24);
        memcpy(file->title, ((char*)data)+21, 22);
        file->title[22]='\0';
        file->type = TYPE_INST_XI;
        return 1;
}


int fmt_xi_load_instrument(const uint8_t *data, size_t length, int slot)
{
        //XMINSTRUMENTHEADER xi;
        XMSAMPLEHEADER xmsh;
        XMSAMPLESTRUCT xmss;
        struct instrumentloader ii;
        song_instrument *g;
        song_sample *smp;
        unsigned int nsamples, rs;
        unsigned int samplesize;
        unsigned int ptr;
        unsigned int k, j, n;

        if (length <= 302) return 0;
        if (memcmp(data, "Extended Instrument: ", 21) != 0) return 0;
        if (!slot) return 0;

        g = instrument_loader_init(&ii, slot);
        memcpy((char*)g->name, data+21, 22);
        g->name[22] = '\0';
        memcpy(&xmsh, data+62, sizeof(xmsh)); /* overlap */
        for (k = 0; k < 96; k++) {
                if (xmsh.snum[k] > 15) xmsh.snum[k] = 15;
                xmsh.snum[k] = instrument_loader_sample(&ii, xmsh.snum[k]+1);
                g->note_map[k+12] = k+1+12;
                if (xmsh.snum[k]) g->sample_map[k+12] = xmsh.snum[k];
        }
        for (k = 0; k < 12; k++) {
                g->note_map[k] = 0; g->sample_map[k] = 0;
                g->note_map[k+108] = 0; g->sample_map[k+108] = 0;
        }
        for (k = 0; k < 24; k++) {
                xmsh.venv[k] = bswapLE16(xmsh.venv[k]);
                xmsh.penv[k] = bswapLE16(xmsh.penv[k]);
        }
        xmsh.volfade = bswapLE16(xmsh.volfade);
        xmsh.res = bswapLE16(xmsh.res);
        nsamples = ((int)data[300]) | (((int)data[301])<<8);
        ptr = 298 + (nsamples * sizeof(xmss));
        for (k = 0; k < nsamples; k++) {
                j = 298 + (k * sizeof(xmss));
                if (j >= length) return 0;
                memcpy(&xmss, data + j, sizeof(xmss));
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
                if (xmss.loopstart >= xmss.samplen) xmss.type &= ~3;
                xmss.looplen += xmss.loopstart;
                if (xmss.looplen > xmss.samplen) xmss.looplen = xmss.samplen;
                if (!xmss.looplen) xmss.type &= ~3;

                n = instrument_loader_sample(&ii, k + 1);
                smp = song_get_sample(n);
                smp->flags = 0;
                memcpy(smp->filename, xmss.name, 22);
                smp->filename[21] = '\0';

                samplesize = xmss.samplen;
                smp->length = samplesize;
                smp->loop_start = xmss.loopstart;
                smp->loop_end = xmss.looplen;
                if (smp->loop_end > smp->loop_start) smp->loop_end = smp->length;
                if (smp->loop_start >= smp->loop_end) smp->loop_start = smp->loop_end = 0;
                if (xmss.type & 3) smp->flags |= SAMP_LOOP;
                if (xmss.type & 3) smp->flags |= SAMP_LOOP_PINGPONG;
                smp->volume = xmss.vol << 2;
                if (smp->volume > 256) smp->volume = 256;
                smp->global_volume = 64;
                smp->panning = xmss.pan;
                smp->flags |= SAMP_PANNING;
                smp->vib_type = xmsh.vibtype;
                smp->vib_speed = xmsh.vibsweep;
                smp->vib_depth = xmsh.vibdepth;
                smp->vib_rate = xmsh.vibrate / 4;

                song_sample_set_c5speed(n, transpose_to_frequency(xmss.relnote, xmss.finetune));
                ptr += song_copy_sample_raw(n, rs, data+ptr, length - ptr);
        }
        return 1;
}

