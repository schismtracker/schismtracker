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
#include "mem.h"

#include <assert.h>

/* -------------------------------------------------------- */

struct it_envelope {
	uint8_t flags;
	uint8_t num;
	uint8_t lpb;
	uint8_t lpe;
	uint8_t slb;
	uint8_t sle;
	struct {
		int8_t value; // signed (-32 -> 32 for pan and pitch; 0 -> 64 for vol and filter)
		uint16_t tick;
	} nodes[25];
	uint8_t reserved;
};

// Old Impulse Instrument Format (cmwt < 0x200)
struct it_instrument_old {
	uint32_t id;                    // IMPI = 0x49504D49
	int8_t filename[12];    // DOS file name
	uint8_t zero;
	uint8_t flags;
	uint8_t vls;
	uint8_t vle;
	uint8_t sls;
	uint8_t sle;
	uint16_t reserved1;
	uint16_t fadeout;
	uint8_t nna;
	uint8_t dnc;
	uint16_t trkvers;
	uint8_t nos;
	uint8_t reserved2;
	int8_t name[26];
	uint16_t reserved3[3];
	//struct it_notetrans keyboard[120];
	uint8_t volenv[200];
	uint8_t nodes[50];
};

// Impulse Instrument Format
struct it_instrument {
	uint32_t id;
	int8_t filename[12];
	uint8_t zero;
	uint8_t nna;
	uint8_t dct;
	uint8_t dca;
	uint16_t fadeout;
	signed char pps;
	uint8_t ppc;
	uint8_t gbv;
	uint8_t dfp;
	uint8_t rv;
	uint8_t rp;
	uint16_t trkvers;
	uint8_t nos;
	uint8_t reserved1;
	int8_t name[26];
	uint8_t ifc;
	uint8_t ifr;
	uint8_t mch;
	uint8_t mpr;
	uint16_t mbank;
	//struct it_notetrans keyboard[120];
	struct it_envelope volenv;
	struct it_envelope panenv;
	struct it_envelope pitchenv;
	uint8_t dummy[4]; // was 7, but IT v2.17 saves 554 bytes
};

/* --------------------------------------------------------------------- */
int fmt_iti_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct it_instrument iti;

	if (slurp_read(fp, &iti, sizeof(iti)) != sizeof(iti)
		|| memcmp(&iti.id, "IMPI", sizeof(iti.id)))
		return 0;

	file->description = "Impulse Tracker Instrument";
	file->title = strn_dup((const char *)iti.name, sizeof(iti.name));
	file->type = TYPE_INST_ITI;

	return 1;
}

static const uint32_t env_flags[3][4] = {
	{ENV_VOLUME,  ENV_VOLLOOP,   ENV_VOLSUSTAIN,   ENV_VOLCARRY},
	{ENV_PANNING, ENV_PANLOOP,   ENV_PANSUSTAIN,   ENV_PANCARRY},
	{ENV_PITCH,   ENV_PITCHLOOP, ENV_PITCHSUSTAIN, ENV_PITCHCARRY},
};

static int load_it_notetrans(struct instrumentloader *ii, song_instrument_t *instrument, slurp_t *fp)
{
	int n;

	for (n = 0; n < 120; n++) {
		int note = slurp_getc(fp);
		int smp = slurp_getc(fp);
		if (note == EOF || smp == EOF)
			return 0;

		note += NOTE_FIRST;
		// map invalid notes to themselves
		if (!NOTE_IS_NOTE(note))
			note = n + NOTE_FIRST;

		instrument->note_map[n] = note;
		instrument->sample_map[n] = (ii ? instrument_loader_sample(ii, smp) : smp);
	}

	return 1;
}

// XXX need to check slurp return values
static uint32_t load_it_envelope(song_envelope_t *env, slurp_t *fp, int envtype, int adj)
{
	struct it_envelope itenv;

	uint32_t flags = 0;
	int n;

	slurp_read(fp, &itenv.flags, sizeof(itenv.flags));
	slurp_read(fp, &itenv.num, sizeof(itenv.num));
	slurp_read(fp, &itenv.lpb, sizeof(itenv.lpb));
	slurp_read(fp, &itenv.lpe, sizeof(itenv.lpe));
	slurp_read(fp, &itenv.slb, sizeof(itenv.slb));
	slurp_read(fp, &itenv.sle, sizeof(itenv.sle));
	for (size_t i = 0; i < ARRAY_SIZE(itenv.nodes); i++) {
		slurp_read(fp, &itenv.nodes[i].value, sizeof(itenv.nodes[i].value));
		slurp_read(fp, &itenv.nodes[i].tick, sizeof(itenv.nodes[i].tick));
	}
	slurp_read(fp, &itenv.reserved, sizeof(itenv.reserved));

	env->nodes = CLAMP(itenv.num, 2, 25);
	env->loop_start = MIN(itenv.lpb, env->nodes);
	env->loop_end = CLAMP(itenv.lpe, env->loop_start, env->nodes);
	env->sustain_start = MIN(itenv.slb, env->nodes);
	env->sustain_end = CLAMP(itenv.sle, env->sustain_start, env->nodes);

	for (n = 0; n < env->nodes; n++) {
		int v = itenv.nodes[n].value + adj;
		env->values[n] = CLAMP(v, 0, 64);
		env->ticks[n] = bswapLE16(itenv.nodes[n].tick);
	}

	env->ticks[0] = 0; // sanity check

	for (n = 0; n < 4; n++) {
		if (itenv.flags & (1 << n))
			flags |= env_flags[envtype][n];
	}

	if (envtype == 2 && (itenv.flags & 0x80))
		flags |= ENV_FILTER;

	return flags;
}

int load_it_instrument_old(song_instrument_t *instrument, slurp_t *fp)
{
	struct it_instrument_old ihdr;
	int n;

#define READ_VALUE(name) do { if (slurp_read(fp, &ihdr.name, sizeof(ihdr.name)) != sizeof(ihdr.name)) { return 0; } } while (0)

	READ_VALUE(id);
	READ_VALUE(filename);
	READ_VALUE(zero);
	READ_VALUE(flags);
	READ_VALUE(vls);
	READ_VALUE(vle);
	READ_VALUE(sls);
	READ_VALUE(sle);
	READ_VALUE(reserved1);
	READ_VALUE(fadeout);
	READ_VALUE(nna);
	READ_VALUE(dnc);
	READ_VALUE(trkvers);
	READ_VALUE(nos);
	READ_VALUE(reserved2);
	READ_VALUE(name);
	READ_VALUE(reserved3);

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

	if (!load_it_notetrans(NULL, instrument, fp))
		return 0;

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

	READ_VALUE(volenv);
	READ_VALUE(nodes);

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

#undef READ_VALUE

	return 1;
}

int load_it_instrument(struct instrumentloader* ii, song_instrument_t *instrument, slurp_t *fp)
{
	struct it_instrument ihdr;

#define READ_VALUE(name) do { if (slurp_read(fp, &ihdr.name, sizeof(ihdr.name)) != sizeof(ihdr.name)) { return 0; } } while (0)

	READ_VALUE(id);
	READ_VALUE(filename);
	READ_VALUE(zero);
	READ_VALUE(nna);
	READ_VALUE(dct);
	READ_VALUE(dca);
	READ_VALUE(fadeout);
	READ_VALUE(pps);
	READ_VALUE(ppc);
	READ_VALUE(gbv);
	READ_VALUE(dfp);
	READ_VALUE(rv);
	READ_VALUE(rp);
	READ_VALUE(trkvers);
	READ_VALUE(nos);
	READ_VALUE(reserved1);
	READ_VALUE(name);
	READ_VALUE(ifc);
	READ_VALUE(ifr);
	READ_VALUE(mch);
	READ_VALUE(mpr);
	READ_VALUE(mbank);

#undef READ_VALUE

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

	if (!load_it_notetrans(ii, instrument, fp))
		return 0;

	instrument->flags |= load_it_envelope(&instrument->vol_env, fp, 0, 0);
	instrument->flags |= load_it_envelope(&instrument->pan_env, fp, 1, 32);
	instrument->flags |= load_it_envelope(&instrument->pitch_env, fp, 2, 32);

	slurp_seek(fp, 4, SEEK_CUR);

	return 1;
}

int fmt_iti_load_instrument(slurp_t *fp, int slot)
{
	struct instrumentloader ii;
	song_instrument_t *ins = instrument_loader_init(&ii, slot);

	if (!load_it_instrument(&ii, ins, fp))
		return 0;

	/* okay, on to samples */
	for (int j = 0; j < ii.expect_samples; j++) {
		song_sample_t *smp = song_get_sample(ii.sample_map[j+1]);
		if (!smp)
			break;

		if (!load_its_sample(fp, smp, 0x214)) {
			log_appendf(4, "Could not load sample %d from ITI file", j);
			return instrument_loader_abort(&ii);
		}
	}
	return 1;
}

static int save_iti_envelope(disko_t *fp, struct it_envelope itenv)
{
	disko_write(fp, &itenv.flags, sizeof(itenv.flags));
	disko_write(fp, &itenv.num, sizeof(itenv.num));
	disko_write(fp, &itenv.lpb, sizeof(itenv.lpb));
	disko_write(fp, &itenv.lpe, sizeof(itenv.lpe));
	disko_write(fp, &itenv.slb, sizeof(itenv.slb));
	disko_write(fp, &itenv.sle, sizeof(itenv.sle));
	for (size_t i = 0; i < ARRAY_SIZE(itenv.nodes); i++) {
		disko_write(fp, &itenv.nodes[i].value, sizeof(itenv.nodes[i].value));
		itenv.nodes[i].tick = bswapLE16(itenv.nodes[i].tick);
		disko_write(fp, &itenv.nodes[i].tick, sizeof(itenv.nodes[i].tick));
	}
	disko_write(fp, &itenv.reserved, sizeof(itenv.reserved));

	return 1;
}

static int save_iti_envelopes(disko_t *fp, song_instrument_t *ins)
{
	struct it_envelope vol = {0}, pan = {0}, pitch = {0};

	vol.flags = ((ins->flags & ENV_VOLUME) ? 0x01 : 0)
		| ((ins->flags & ENV_VOLLOOP) ? 0x02 : 0)
		| ((ins->flags & ENV_VOLSUSTAIN) ? 0x04 : 0)
		| ((ins->flags & ENV_VOLCARRY) ? 0x08 : 0);
	vol.num = ins->vol_env.nodes;
	vol.lpb = ins->vol_env.loop_start;
	vol.lpe = ins->vol_env.loop_end;
	vol.slb = ins->vol_env.sustain_start;
	vol.sle = ins->vol_env.sustain_end;

	pan.flags = ((ins->flags & ENV_PANNING) ? 0x01 : 0)
		| ((ins->flags & ENV_PANLOOP) ? 0x02 : 0)
		| ((ins->flags & ENV_PANSUSTAIN) ? 0x04 : 0)
		| ((ins->flags & ENV_PANCARRY) ? 0x08 : 0);
	pan.num = ins->pan_env.nodes;
	pan.lpb = ins->pan_env.loop_start;
	pan.lpe = ins->pan_env.loop_end;
	pan.slb = ins->pan_env.sustain_start;
	pan.sle = ins->pan_env.sustain_end;

	pitch.flags = ((ins->flags & ENV_PITCH) ? 0x01 : 0)
		| ((ins->flags & ENV_PITCHLOOP) ? 0x02 : 0)
		| ((ins->flags & ENV_PITCHSUSTAIN) ? 0x04 : 0)
		| ((ins->flags & ENV_PITCHCARRY) ? 0x08 : 0)
		| ((ins->flags & ENV_FILTER) ? 0x80 : 0);
	pitch.num = ins->pan_env.nodes;
	pitch.lpb = ins->pan_env.loop_start;
	pitch.lpe = ins->pan_env.loop_end;
	pitch.slb = ins->pan_env.sustain_start;
	pitch.sle = ins->pan_env.sustain_end;

	for (int j = 0; j < 25; j++) {
		vol.nodes[j].value = ins->vol_env.values[j];
		vol.nodes[j].tick = ins->vol_env.ticks[j];

		pan.nodes[j].value = ins->pan_env.values[j] - 32;
		pan.nodes[j].tick = ins->pan_env.ticks[j];

		pitch.nodes[j].value = ins->pitch_env.values[j] - 32;
		pitch.nodes[j].tick = ins->pitch_env.ticks[j];
	}

	if (!save_iti_envelope(fp, vol))   return 0;
	if (!save_iti_envelope(fp, pan))   return 0;
	if (!save_iti_envelope(fp, pitch)) return 0;

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
	memcpy((char *) iti.filename, (char *) ins->filename, MIN(sizeof(ins->filename), sizeof(iti.filename)));
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
	memcpy((char *) iti.name, (char *) ins->name, MIN(sizeof(ins->name), sizeof(iti.name)));
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

	struct {
		uint8_t note;
		uint8_t smp;
	} notetrans[120];

	int iti_map[255];
	int iti_invmap[255];
	int iti_nalloc = 0;

	for (int j = 0; j < 255; j++)
		iti_map[j] = -1;

	for (int j = 0; j < 120; j++) {
		notetrans[j].note = ins->note_map[j] - 1;

		if (iti_file) {
			int o = ins->sample_map[j];
			if (o > 0 && o < 255 && iti_map[o] == -1) {
				iti_map[o] = iti_nalloc;
				iti_invmap[iti_nalloc] = o;
				iti_nalloc++;
			}
			notetrans[j].smp = iti_map[o]+1;
		} else {
			notetrans[j].smp = ins->sample_map[j];
		}
	}

	if (iti_file)
		iti.nos = (uint8_t)iti_nalloc;

	disko_write(fp, &iti.id, sizeof(iti.id));
	disko_write(fp, &iti.filename, sizeof(iti.filename));
	disko_write(fp, &iti.zero, sizeof(iti.zero));
	disko_write(fp, &iti.nna, sizeof(iti.nna));
	disko_write(fp, &iti.dct, sizeof(iti.dct));
	disko_write(fp, &iti.dca, sizeof(iti.dca));
	disko_write(fp, &iti.fadeout, sizeof(iti.fadeout));
	disko_write(fp, &iti.pps, sizeof(iti.pps));
	disko_write(fp, &iti.ppc, sizeof(iti.ppc));
	disko_write(fp, &iti.gbv, sizeof(iti.gbv));
	disko_write(fp, &iti.dfp, sizeof(iti.dfp));
	disko_write(fp, &iti.rv, sizeof(iti.rv));
	disko_write(fp, &iti.rp, sizeof(iti.rp));
	disko_write(fp, &iti.trkvers, sizeof(iti.trkvers));
	disko_write(fp, &iti.nos, sizeof(iti.nos));
	disko_write(fp, &iti.reserved1, sizeof(iti.reserved1));
	disko_write(fp, &iti.name, sizeof(iti.name));
	disko_write(fp, &iti.ifc, sizeof(iti.ifc));
	disko_write(fp, &iti.ifr, sizeof(iti.ifr));
	disko_write(fp, &iti.mch, sizeof(iti.mch));
	disko_write(fp, &iti.mpr, sizeof(iti.mpr));
	disko_write(fp, &iti.mbank, sizeof(iti.mbank));

	for (int i = 0; i < 120; i++) {
		disko_write(fp, &notetrans[i].note, sizeof(notetrans[i].note));
		disko_write(fp, &notetrans[i].smp, sizeof(notetrans[i].smp));
	}

	save_iti_envelopes(fp, ins);

	/* unused padding */
	disko_write(fp, "\0\0\0\0", 4);

	// ITI files *need* to write 554 bytes due to alignment, but in a song it doesn't matter
	if (iti_file) {
		int64_t pos = disko_tell(fp);

		// ack
		assert(pos == 554);

		/* okay, now go through samples */
		for (int j = 0; j < iti_nalloc; j++) {
			int o = iti_invmap[j];

			iti_map[o] = pos;
			pos += 80; /* header is 80 bytes */
			save_its_header(fp, song->samples + o);
		}

		for (int j = 0; j < iti_nalloc; j++) {
			unsigned int op, tmp;

			int o = iti_invmap[j];

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
