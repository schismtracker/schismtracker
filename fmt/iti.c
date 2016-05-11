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
	struct it_instrument iti;
	struct instrumentloader ii;
	song_instrument_t *ins;
	song_sample_t *smp;
	int j;

	if (!(length > 554 && memcmp(data, "IMPI",4) == 0)) return 0;

	memcpy(&iti, data, sizeof(iti));

	ins = instrument_loader_init(&ii, slot);
	strncpy(ins->filename, (char *)iti.filename, 12);
	ins->filename[12] = 0;

	ins->nna = iti.nna;
	ins->dct = iti.dct;
	ins->dca = iti.dca;
	ins->fadeout = (bswapLE16(iti.fadeout) << 5);
	ins->pitch_pan_separation = iti.pps;
	ins->pitch_pan_center = iti.ppc;
	ins->global_volume = iti.gbv;
	ins->panning = (iti.dfp & 0x7F) << 2;
	if (ins->panning > 256) ins->panning = 128;
	ins->flags = 0;
	if (iti.dfp & 0x80) ins->flags = ENV_SETPANNING;
	ins->vol_swing = iti.rv;
	ins->pan_swing = iti.rp;

	strncpy(ins->name, (char *)iti.name, 25);
	ins->name[25] = 0;
	ins->ifc = iti.ifc;
	ins->ifr = iti.ifr;
	ins->midi_channel_mask = iti.mch > 16 ? (0x10000 + iti.mch)
			     : iti.mch == 0 ? (0)
			     :                (1 << (iti.mch-1));
	ins->midi_program = iti.mpr;
	ins->midi_bank = bswapLE16(iti.mbank);

	for (j = 0; j < 120; j++) {
		ins->sample_map[j] = instrument_loader_sample(&ii, iti.keyboard[2*j + 1]);
		ins->note_map[j] = iti.keyboard[2 * j]+1;
	}
	if (iti.volenv.flags & 1) ins->flags |= ENV_VOLUME;
	if (iti.volenv.flags & 2) ins->flags |= ENV_VOLLOOP;
	if (iti.volenv.flags & 4) ins->flags |= ENV_VOLSUSTAIN;
	if (iti.volenv.flags & 8) ins->flags |= ENV_VOLCARRY;
	ins->vol_env.nodes = iti.volenv.num;
	ins->vol_env.loop_start = iti.volenv.lpb;
	ins->vol_env.loop_end = iti.volenv.lpe;
	ins->vol_env.sustain_start = iti.volenv.slb;
	ins->vol_env.sustain_end = iti.volenv.sle;
	if (iti.panenv.flags & 1) ins->flags |= ENV_PANNING;
	if (iti.panenv.flags & 2) ins->flags |= ENV_PANLOOP;
	if (iti.panenv.flags & 4) ins->flags |= ENV_PANSUSTAIN;
	if (iti.panenv.flags & 8) ins->flags |= ENV_PANCARRY;
	ins->pan_env.nodes = iti.panenv.num;
	ins->pan_env.loop_start = iti.panenv.lpb;
	ins->pan_env.loop_end = iti.panenv.lpe;
	ins->pan_env.sustain_start = iti.panenv.slb;
	ins->pan_env.sustain_end = iti.panenv.sle;
	if (iti.pitchenv.flags & 1) ins->flags |= ENV_PITCH;
	if (iti.pitchenv.flags & 2) ins->flags |= ENV_PITCHLOOP;
	if (iti.pitchenv.flags & 4) ins->flags |= ENV_PITCHSUSTAIN;
	if (iti.pitchenv.flags & 8) ins->flags |= ENV_PITCHCARRY;
	if (iti.pitchenv.flags & 0x80) ins->flags |= ENV_FILTER;
	ins->pitch_env.nodes = iti.pitchenv.num;
	ins->pitch_env.loop_start = iti.pitchenv.lpb;
	ins->pitch_env.loop_end = iti.pitchenv.lpe;
	ins->pitch_env.sustain_start = iti.pitchenv.slb;
	ins->pitch_env.sustain_end = iti.pitchenv.sle;

	for (j = 0; j < 25; j++) {
		ins->vol_env.values[j] = iti.volenv.data[3 * j];
		ins->vol_env.ticks[j] = iti.volenv.data[3 * j + 1]
			| (iti.volenv.data[3 * j + 2] << 8);

		ins->pan_env.values[j] = iti.panenv.data[3 * j] + 32;
		ins->pan_env.ticks[j] = iti.panenv.data[3 * j + 1]
			| (iti.panenv.data[3 * j + 2] << 8);

		ins->pitch_env.values[j] = iti.pitchenv.data[3 * j] + 32;
		ins->pitch_env.ticks[j] = iti.pitchenv.data[3 * j + 1]
			| (iti.pitchenv.data[3 * j + 2] << 8);
	}

	/* okay, on to samples */
	unsigned int pos = 554;
	for (j = 0; j < ii.expect_samples; j++) {
		smp = song_get_sample(ii.sample_map[j+1]);
		if (!smp) break;
		if (!load_its_sample(data + pos, data, length, smp)) {
			log_appendf(4, "Could not load sample %d from ITI file", j);
			return instrument_loader_abort(&ii);
		}
		pos += 80; /* length of ITS header */
	}
	return 1;
}
