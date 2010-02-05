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

#include "it_defs.h"


/* --------------------------------------------------------------------- */
int fmt_iti_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        if (!(length > 554 && memcmp(data, "IMPI",4) == 0)) return 0;
        file->description = "Impulse Tracker Instrument";
        file->title = (char *)calloc(26,sizeof(char *));
        memcpy(file->title, data+32, 25);
        file->title[25] = 0;
        file->type = TYPE_INST_ITI;

        return 1;
}

int fmt_iti_load_instrument(const uint8_t *data, size_t length, int slot)
{
        ITINSTRUMENT iti;
        struct instrumentloader ii;
        song_instrument *g;
        unsigned int q;
        song_sample *smp;
        int j;

        if (!(length > 554 && memcmp(data, "IMPI",4) == 0)) return 0;

        memcpy(&iti, data, sizeof(iti));

        g = instrument_loader_init(&ii, slot);
        strncpy((char *)g->filename, (char *)iti.filename, 12);

        g->nna = iti.nna;
        g->dct = iti.dct;
        g->dca = iti.dca;
        g->fadeout = (bswapLE16(iti.fadeout) << 5);
        g->pitch_pan_separation = iti.pps;
        g->pitch_pan_center = iti.ppc;
        g->global_volume = iti.gbv;
        g->panning = (iti.dfp & 0x7F) << 2;
        if (g->panning > 256) g->panning = 128;
        g->flags = 0;
        if (iti.dfp & 0x80) g->flags = ENV_SETPANNING;
        g->volume_swing = iti.rv;
        g->pan_swing = iti.rp;

        strncpy((char *)g->name, (char *)iti.name, 25);
        g->name[25] = 0;
        g->filter_cutoff = iti.ifc;
        g->filter_resonance = iti.ifr;
        g->midi_channel_mask = iti.mch > 16 ? (0x10000 + iti.mch)
                             : iti.mch == 0 ? (0)
                             :                (1 << (iti.mch-1));
        g->midi_program = iti.mpr;
        g->midi_bank = bswapLE16(iti.mbank);

        for (j = 0; j < 120; j++) {
                g->sample_map[j] = instrument_loader_sample(&ii, iti.keyboard[2*j + 1]);
                g->note_map[j] = iti.keyboard[2 * j]+1;
        }
        if (iti.volenv.flags & 1) g->flags |= ENV_VOLUME;
        if (iti.volenv.flags & 2) g->flags |= ENV_VOLLOOP;
        if (iti.volenv.flags & 4) g->flags |= ENV_VOLSUSTAIN;
        if (iti.volenv.flags & 8) g->flags |= ENV_VOLCARRY;
        g->vol_env.nodes = iti.volenv.num;
        g->vol_env.loop_start = iti.volenv.lpb;
        g->vol_env.loop_end = iti.volenv.lpe;
        g->vol_env.sustain_start = iti.volenv.slb;
        g->vol_env.sustain_end = iti.volenv.sle;
        if (iti.panenv.flags & 1) g->flags |= ENV_PANNING;
        if (iti.panenv.flags & 2) g->flags |= ENV_PANLOOP;
        if (iti.panenv.flags & 4) g->flags |= ENV_PANSUSTAIN;
        if (iti.panenv.flags & 8) g->flags |= ENV_PANCARRY;
        g->pan_env.nodes = iti.panenv.num;
        g->pan_env.loop_start = iti.panenv.lpb;
        g->pan_env.loop_end = iti.panenv.lpe;
        g->pan_env.sustain_start = iti.panenv.slb;
        g->pan_env.sustain_end = iti.panenv.sle;
        if (iti.pitchenv.flags & 1) g->flags |= ENV_PITCH;
        if (iti.pitchenv.flags & 2) g->flags |= ENV_PITCHLOOP;
        if (iti.pitchenv.flags & 4) g->flags |= ENV_PITCHSUSTAIN;
        if (iti.pitchenv.flags & 8) g->flags |= ENV_PITCHCARRY;
        if (iti.pitchenv.flags & 0x80) g->flags |= ENV_FILTER;
        g->pitch_env.nodes = iti.pitchenv.num;
        g->pitch_env.loop_start = iti.pitchenv.lpb;
        g->pitch_env.loop_end = iti.pitchenv.lpe;
        g->pitch_env.sustain_start = iti.pitchenv.slb;
        g->pitch_env.sustain_end = iti.pitchenv.sle;

        for (j = 0; j < 25; j++) {
                g->vol_env.values[j] = iti.volenv.data[3 * j];
                g->vol_env.ticks[j] = iti.volenv.data[3 * j + 1]
                        | (iti.volenv.data[3 * j + 2] << 8);

                g->pan_env.values[j] = iti.panenv.data[3 * j] + 32;
                g->pan_env.ticks[j] = iti.panenv.data[3 * j + 1]
                        | (iti.panenv.data[3 * j + 2] << 8);

                g->pitch_env.values[j] = iti.pitchenv.data[3 * j] + 32;
                g->pitch_env.ticks[j] = iti.pitchenv.data[3 * j + 1]
                        | (iti.pitchenv.data[3 * j + 2] << 8);
        }

        /* okay, on to samples */
        q = 554;
        for (j = 0; j < ii.expect_samples; j++) {
                smp = song_get_sample(ii.sample_map[j+1]);
                if (!smp) break;
                if (!load_its_sample(data+q, data, length, smp)) {
                        status_text_flash("Could not load sample %d from ITI file", j);
                        return instrument_loader_abort(&ii);
                }
                q += 80; /* length if ITS header */
        }
        return 1;
}
