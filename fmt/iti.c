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

#include "headers.h"
#include "bswap.h"
#include "fmt.h"

#include "it.h"
#include "song.h"
#include "log.h"
#include "version.h"

#include "it_defs.h"

#include <assert.h>

/* --------------------------------------------------------------------- */
int fmt_iti_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct it_instrument iti;

	if (slurp_read(fp, &iti, sizeof(iti)) != sizeof(iti)
		|| memcmp(&iti.id, "IMPI", sizeof(iti.id)))
		return 0;

	file->description = "Impulse Tracker Instrument";
	file->title = strn_dup(iti.name, sizeof(iti.name));
	file->type = TYPE_INST_ITI;

	return 1;
}

static const uint32_t env_flags[3][4] = {
	{ENV_VOLUME,  ENV_VOLLOOP,   ENV_VOLSUSTAIN,   ENV_VOLCARRY},
	{ENV_PANNING, ENV_PANLOOP,   ENV_PANSUSTAIN,   ENV_PANCARRY},
	{ENV_PITCH,   ENV_PITCHLOOP, ENV_PITCHSUSTAIN, ENV_PITCHCARRY},
};

static void load_it_notetrans(song_instrument_t *instrument, struct it_notetrans *notetrans)
{
	int note, n;
	for (n = 0; n < 120; n++) {
		note = notetrans[n].note + NOTE_FIRST;
		// map invalid notes to themselves
		if (!NOTE_IS_NOTE(note))
			note = n + NOTE_FIRST;
		instrument->note_map[n] = note;
		instrument->sample_map[n] = notetrans[n].sample;
	}
}

static uint32_t load_it_envelope(song_envelope_t *env, struct it_envelope *itenv, int envtype, int adj)
{
	uint32_t flags = 0;
	int n;

	env->nodes = CLAMP(itenv->num, 2, 25);
	env->loop_start = MIN(itenv->lpb, env->nodes);
	env->loop_end = CLAMP(itenv->lpe, env->loop_start, env->nodes);
	env->sustain_start = MIN(itenv->slb, env->nodes);
	env->sustain_end = CLAMP(itenv->sle, env->sustain_start, env->nodes);

	for (n = 0; n < env->nodes; n++) {
		int v = itenv->nodes[n].value + adj;
		env->values[n] = CLAMP(v, 0, 64);
		env->ticks[n] = bswapLE16(itenv->nodes[n].tick);
	}

	env->ticks[0] = 0; // sanity check

	for (n = 0; n < 4; n++) {
		if (itenv->flags & (1 << n))
			flags |= env_flags[envtype][n];
	}

	if (envtype == 2 && (itenv->flags & 0x80))
		flags |= ENV_FILTER;

	return flags;
}

int load_it_instrument_old(song_instrument_t *instrument, slurp_t *fp)
{
	struct it_instrument_old ihdr;
	int n;

	if (slurp_read(fp, &ihdr, sizeof(ihdr)) != sizeof(ihdr))
		return 0;

	/* err */
	if (ihdr.id != bswapLE32(0x49504D49))
		return 0;

	memcpy(instrument->name, ihdr.name, 25);
	instrument->name[25] = '\0';
	memcpy(instrument->filename, ihdr.filename, 12);
	instrument->filename[12] = '\0';

	instrument->nna = ihdr.nna % 4;
	if (ihdr.dnc) {
		// XXX is this right?
		instrument->dct = DCT_NOTE;
		instrument->dca = DCA_NOTECUT;
	}

	instrument->fadeout = bswapLE16(ihdr.fadeout) << 6;
	instrument->pitch_pan_separation = 0;
	instrument->pitch_pan_center = NOTE_MIDC;
	instrument->global_volume = 128;
	instrument->panning = 32 * 4; //mphack

	load_it_notetrans(instrument, ihdr.keyboard);

	if (ihdr.flags & 1)
		instrument->flags |= ENV_VOLUME;
	if (ihdr.flags & 2)
		instrument->flags |= ENV_VOLLOOP;
	if (ihdr.flags & 4)
		instrument->flags |= ENV_VOLSUSTAIN;

	instrument->vol_env.loop_start = ihdr.vls;
	instrument->vol_env.loop_end = ihdr.vle;
	instrument->vol_env.sustain_start = ihdr.sls;
	instrument->vol_env.sustain_end = ihdr.sle;
	instrument->vol_env.nodes = 25;
	// this seems totally wrong... why isn't this using ihdr.vol_env at all?
	// apparently it works, though.
	for (n = 0; n < 25; n++) {
		int node = ihdr.nodes[2 * n];
		if (node == 0xff) {
			instrument->vol_env.nodes = n;
			break;
		}
		instrument->vol_env.ticks[n] = node;
		instrument->vol_env.values[n] = ihdr.nodes[2 * n + 1];
	}
}

int load_it_instrument(song_instrument_t *instrument, slurp_t *fp)
{
	struct it_instrument ihdr;

	if (slurp_read(fp, &ihdr, sizeof(ihdr)) != sizeof(ihdr))
		return 0;

	/* err */
	if (ihdr.id != bswapLE32(0x49504D49))
		return 0;

	memcpy(instrument->name, ihdr.name, 25);
	instrument->name[25] = '\0';
	memcpy(instrument->filename, ihdr.filename, 12);
	instrument->filename[12] = '\0';

	instrument->nna = ihdr.nna % 4;
	instrument->dct = ihdr.dct % 4;
	instrument->dca = ihdr.dca % 3;
	instrument->fadeout = bswapLE16(ihdr.fadeout) << 5;
	instrument->pitch_pan_separation = CLAMP(ihdr.pps, -32, 32);
	instrument->pitch_pan_center = MIN(ihdr.ppc, 119); // I guess
	instrument->global_volume = MIN(ihdr.gbv, 128);
	instrument->panning = MIN((ihdr.dfp & 127), 64) * 4; //mphack
	if (!(ihdr.dfp & 128))
		instrument->flags |= ENV_SETPANNING;
	instrument->vol_swing = MIN(ihdr.rv, 100);
	instrument->pan_swing = MIN(ihdr.rp, 64);

	instrument->ifc = ihdr.ifc;
	instrument->ifr = ihdr.ifr;

	// (blah... this isn't supposed to be a mask according to the
	// spec. where did this code come from? and what is 0x10000?)
	instrument->midi_channel_mask =
			((ihdr.mch > 16)
			 ? (0x10000 + ihdr.mch)
			 : ((ihdr.mch > 0)
			    ? (1 << (ihdr.mch - 1))
			    : 0));
	instrument->midi_program = ihdr.mpr;
	instrument->midi_bank = bswapLE16(ihdr.mbank);

	load_it_notetrans(instrument, ihdr.keyboard);

	instrument->flags |= load_it_envelope(&instrument->vol_env, &ihdr.volenv, 0, 0);
	instrument->flags |= load_it_envelope(&instrument->pan_env, &ihdr.panenv, 1, 32);
	instrument->flags |= load_it_envelope(&instrument->pitch_env, &ihdr.pitchenv, 2, 32);
}

int fmt_iti_load_instrument(slurp_t *fp, int slot)
{
	struct instrumentloader ii;
	song_instrument_t *ins = instrument_loader_init(&ii, slot);

	if (!load_it_instrument(ins, fp))
		return 0;

	/* okay, on to samples */
	size_t pos = 554;
	for (int j = 0; j < ii.expect_samples; j++) {
		song_sample_t *smp = song_get_sample(ii.sample_map[j+1]);
		if (!smp)
			break;

		struct it_sample its;
		slurp_read(fp, &its, sizeof(its));

		if (!load_its_sample(&its, fp, smp)) {
			log_appendf(4, "Could not load sample %d from ITI file", j);
			return instrument_loader_abort(&ii);
		}
		pos += 80; /* length of ITS header */
	}
	return 1;
}

// set iti_file if saving an instrument to disk by itself
void save_iti_instrument(disko_t *fp, song_t *song, song_instrument_t *ins, int iti_file)
{
	song_instrument_t blank_instrument = {0};
	struct it_instrument iti = {0};

	/* don't have an instrument? make one up! */
	if (!ins)
		ins = &blank_instrument;

	// envelope: flags num lpb lpe slb sle data[25*3] reserved

	iti.id = bswapLE32(0x49504D49); // IMPI
	strncpy((char *) iti.filename, (char *) ins->filename, 12);
	iti.zero = 0;
	iti.nna = ins->nna;
	iti.dct = ins->dct;
	iti.dca = ins->dca;
	iti.fadeout = bswapLE16(ins->fadeout >> 5);
	iti.pps = ins->pitch_pan_separation;
	iti.ppc = ins->pitch_pan_center;
	iti.gbv = ins->global_volume;

	iti.dfp = ins->panning / 4;
	if (!(ins->flags & ENV_SETPANNING))
		iti.dfp |= 0x80;

	iti.rv = ins->vol_swing;
	iti.rp = ins->pan_swing;

	if (iti_file)
		iti.trkvers = bswapLE16(0x1000 | ver_cwtv);

	// reserved1
	strncpy((char *) iti.name, (char *) ins->name, 25);
	iti.name[25] = 0;
	iti.ifc = ins->ifc;
	iti.ifr = ins->ifr;
	iti.mch = 0;
	if(ins->midi_channel_mask >= 0x10000)
	{
	    iti.mch = ins->midi_channel_mask - 0x10000;
	    if(iti.mch <= 16) iti.mch = 16;
	}
	else if(ins->midi_channel_mask & 0xFFFF)
	{
	    iti.mch = 1;
	    while(!(ins->midi_channel_mask & (1 << (iti.mch-1)))) ++iti.mch;
	}
	iti.mpr = ins->midi_program;
	iti.mbank = bswapLE16(ins->midi_bank);

	int iti_map[255];
	int iti_invmap[255];
	int iti_nalloc = 0;

	for (int j = 0; j < 255; j++)
		iti_map[j] = -1;

	for (int j = 0; j < 120; j++) {
		if (iti_file) {
			int o = ins->sample_map[j];
			if (o > 0 && o < 255 && iti_map[o] == -1) {
				iti_map[o] = iti_nalloc;
				iti_invmap[iti_nalloc] = o;
				iti_nalloc++;
			}
			iti.keyboard[j].sample = iti_map[o]+1;
		} else {
			iti.keyboard[j].sample = ins->sample_map[j];
		}
		iti.keyboard[j].note = ins->note_map[j] - 1;
	}

	if (iti_file)
		iti.nos = (uint8_t)iti_nalloc;

	// envelope stuff from modplug
	iti.volenv.flags = 0;
	iti.panenv.flags = 0;
	iti.pitchenv.flags = 0;
	if (ins->flags & ENV_VOLUME) iti.volenv.flags |= 0x01;
	if (ins->flags & ENV_VOLLOOP) iti.volenv.flags |= 0x02;
	if (ins->flags & ENV_VOLSUSTAIN) iti.volenv.flags |= 0x04;
	if (ins->flags & ENV_VOLCARRY) iti.volenv.flags |= 0x08;
	iti.volenv.num = ins->vol_env.nodes;
	iti.volenv.lpb = ins->vol_env.loop_start;
	iti.volenv.lpe = ins->vol_env.loop_end;
	iti.volenv.slb = ins->vol_env.sustain_start;
	iti.volenv.sle = ins->vol_env.sustain_end;
	if (ins->flags & ENV_PANNING) iti.panenv.flags |= 0x01;
	if (ins->flags & ENV_PANLOOP) iti.panenv.flags |= 0x02;
	if (ins->flags & ENV_PANSUSTAIN) iti.panenv.flags |= 0x04;
	if (ins->flags & ENV_PANCARRY) iti.panenv.flags |= 0x08;
	iti.panenv.num = ins->pan_env.nodes;
	iti.panenv.lpb = ins->pan_env.loop_start;
	iti.panenv.lpe = ins->pan_env.loop_end;
	iti.panenv.slb = ins->pan_env.sustain_start;
	iti.panenv.sle = ins->pan_env.sustain_end;
	if (ins->flags & ENV_PITCH) iti.pitchenv.flags |= 0x01;
	if (ins->flags & ENV_PITCHLOOP) iti.pitchenv.flags |= 0x02;
	if (ins->flags & ENV_PITCHSUSTAIN) iti.pitchenv.flags |= 0x04;
	if (ins->flags & ENV_PITCHCARRY) iti.pitchenv.flags |= 0x08;
	if (ins->flags & ENV_FILTER) iti.pitchenv.flags |= 0x80;
	iti.pitchenv.num = ins->pitch_env.nodes;
	iti.pitchenv.lpb = ins->pitch_env.loop_start;
	iti.pitchenv.lpe = ins->pitch_env.loop_end;
	iti.pitchenv.slb = ins->pitch_env.sustain_start;
	iti.pitchenv.sle = ins->pitch_env.sustain_end;
	for (int j = 0; j < 25; j++) {
		iti.volenv.nodes[j].value = ins->vol_env.values[j];
		iti.volenv.nodes[j].tick = bswapLE16(ins->vol_env.ticks[j]);

		iti.panenv.nodes[j].value = ins->pan_env.values[j] - 32;
		iti.panenv.nodes[j].tick = bswapLE16(ins->pan_env.ticks[j]);

		iti.pitchenv.nodes[j].value = ins->pitch_env.values[j] - 32;
		iti.pitchenv.nodes[j].tick = bswapLE16(ins->pitch_env.ticks[j]);
	}

	// ITI files *need* to write 554 bytes due to alignment, but in a song it doesn't matter
	disko_write(fp, &iti, sizeof(iti));
	if (iti_file) {
		if (sizeof(iti) < 554) {
			for (int j = sizeof(iti); j < 554; j++) {
				disko_write(fp, "\x0", 1);
			}
		}
		assert(sizeof(iti) <= 554);

		unsigned int qp = 554;
		/* okay, now go through samples */
		for (int j = 0; j < iti_nalloc; j++) {
			int o = iti_invmap[ j ];

			iti_map[o] = qp;
			qp += 80; /* header is 80 bytes */
			save_its_header(fp, song->samples + o);
		}
		for (int j = 0; j < iti_nalloc; j++) {
			unsigned int op, tmp;

			int o = iti_invmap[ j ];

			song_sample_t *smp = song->samples + o;

			op = disko_tell(fp);
			tmp = bswapLE32(op);
			disko_seek(fp, iti_map[o]+0x48, SEEK_SET);
			disko_write(fp, &tmp, 4);
			disko_seek(fp, op, SEEK_SET);
			csf_write_sample(fp, smp, SF_LE | SF_PCMS
					| ((smp->flags & CHN_16BIT) ? SF_16 : SF_8)
					| ((smp->flags & CHN_STEREO) ? SF_SS : SF_M),
					UINT32_MAX);
		}
	}
}

int fmt_iti_save_instrument(disko_t *fp, song_t *song, song_instrument_t *ins)
{
	save_iti_instrument(fp, song, ins, 1);
	return SAVE_SUCCESS;
}
