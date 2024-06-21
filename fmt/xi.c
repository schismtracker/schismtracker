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

#include "player/sndfile.h"

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
		} sep;
	} env;

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

#define XI_ENV_ENABLED 0x01
#define XI_ENV_SUSTAIN 0x02
#define XI_ENV_LOOP    0x04

static int validate_xi(const struct xi_file_header *xi, size_t length)
{
	if (length <= sizeof(*xi))
		return 0;

	if (memcmp(xi->header, "Extended Instrument: ", 0x15) != 0)
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
	struct xi_file_header *xi = (struct xi_file_header *)data;

	if (!validate_xi(xi, length))
		return 0;

	file->description = "Fasttracker II Instrument";
	file->title = strn_dup((const char *)xi->name, 22);
	file->type = TYPE_INST_XI;
	return 1;
}

int fmt_xi_load_instrument(const uint8_t *data, size_t length, int slot)
{
	const struct xi_file_header *xi = (const struct xi_file_header *) data;
	struct xi_sample_header xmsh;
	struct instrumentloader ii;
	song_instrument_t *g;
	const uint8_t *sampledata, *end;
	int k, prevtick;

	if (!slot)
		return 0;
	if (!validate_xi(xi, length))
		return 0;

	end = data + length;

	song_delete_instrument(slot, 0);

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
		xmsh.env.env[k] = bswapLE16(xmsh.env.env[k]);

	// Set up envelope types in instrument
	if (xmsh.vtype & XI_ENV_ENABLED) g->flags |= ENV_VOLUME;
	if (xmsh.vtype & XI_ENV_SUSTAIN) g->flags |= ENV_VOLSUSTAIN;
	if (xmsh.vtype & XI_ENV_LOOP)    g->flags |= ENV_VOLLOOP;
	if (xmsh.ptype & XI_ENV_ENABLED) g->flags |= ENV_PANNING;
	if (xmsh.ptype & XI_ENV_SUSTAIN) g->flags |= ENV_PANSUSTAIN;
	if (xmsh.ptype & XI_ENV_LOOP)    g->flags |= ENV_PANLOOP;

	prevtick = -1;
	// Copy envelopes into instrument
	for (k = 0; k < xmsh.vnum; k++) {
		if (xmsh.env.sep.venv[k].ticks < prevtick)
			prevtick++;
		else
			prevtick = xmsh.env.sep.venv[k].ticks;
		g->vol_env.ticks[k] = prevtick;
		g->vol_env.values[k] = xmsh.env.sep.venv[k].val;
	}

	prevtick = -1;
	for (k = 0; k < xmsh.pnum; k++) {
		if (xmsh.env.sep.penv[k].ticks < prevtick)
			prevtick++;
		else
			prevtick = xmsh.env.sep.penv[k].ticks;
		g->pan_env.ticks[k] = prevtick;
		g->pan_env.values[k] = xmsh.env.sep.penv[k].val;
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
		if (smp->loop_end < smp->loop_start)
			smp->loop_end = smp->length;
		if (smp->loop_start >= smp->loop_end)
			smp->loop_start = smp->loop_end = 0;
		switch (xmss.type & 3) {
			case 3: case 2: smp->flags |= CHN_PINGPONGLOOP;
			case 1: smp->flags |= CHN_LOOP;
		}
		smp->volume = xmss.vol << 2;
		if (smp->volume > 256)
			smp->volume = 256;
		smp->global_volume = 64;
		smp->panning = xmss.pan;
		smp->flags |= CHN_PANNING;
		smp->vib_type = xmsh.vibtype;
		smp->vib_speed = MIN(xmsh.vibrate, 64);
		smp->vib_depth = MIN(xmsh.vibdepth, 32);
		if (xmsh.vibrate | xmsh.vibdepth) {
			if (xmsh.vibsweep) {
				int s = _muldivr(smp->vib_depth, 256, xmsh.vibsweep);
				smp->vib_rate = CLAMP(s, 0, 255);
			} else {
				smp->vib_rate = 255;
			}
		}

		smp->c5speed = transpose_to_frequency(xmss.relnote, xmss.finetune);
		sampledata += csf_read_sample(current_song->samples + n, rs, sampledata, (end-sampledata));
	}

	return 1;
}

/* ------------------------------------------------------------------------ */

int fmt_xi_save_instrument(disko_t *fp, song_t *song, song_instrument_t *ins)
{
	struct xi_file_header xi;
	struct xi_sample_header xmsh;
	struct instrumentloader ii;
	song_instrument_t *g;
	song_sample_t *smp;
	int k, prevtick;

	/* fill in sample numbers, epicly stolen from the iti code */
	int xi_map[255];
	int xi_invmap[255];
	int xi_nalloc = 0;

	for (int j = 0; j < 255; j++)
		xi_map[j] = -1;

	for (int j = 0; j < 96; j++) {
		int o = ins->sample_map[j + 12];

		if (o > 0 && o < 255 && xi_map[o] == -1) {
			xi_map[o] = xi_nalloc;
			xi_invmap[xi_nalloc] = o;
			xi_nalloc++;
		}

		xi.xish.snum[j] = xi_map[o]+1;
	}

	if (xi_nalloc < 1)
		return SAVE_FILE_ERROR;

	/* now add header things */
	memcpy(xi.header, "Extended Instrument: ", sizeof(xi.header));
	strncpy(xi.name, ins->name, sizeof(ins->name));
	xi.magic = 0x1A;
	memcpy(xi.tracker, "Schism Tracker", 14);
	xi.version = bswapLE16(0x0102);

	/* envelope type */
	if (ins->flags & ENV_VOLUME)     xi.xish.vtype |= XI_ENV_ENABLED;
	if (ins->flags & ENV_VOLSUSTAIN) xi.xish.vtype |= XI_ENV_SUSTAIN;
	if (ins->flags & ENV_VOLLOOP)    xi.xish.vtype |= XI_ENV_LOOP;
	if (ins->flags & ENV_PANNING)    xi.xish.ptype |= XI_ENV_ENABLED;
	if (ins->flags & ENV_PANSUSTAIN) xi.xish.ptype |= XI_ENV_SUSTAIN;
	if (ins->flags & ENV_PANLOOP)    xi.xish.ptype |= XI_ENV_LOOP;

	xi.xish.vloops = ins->vol_env.loop_start;
	xi.xish.vloope = ins->vol_env.loop_end;
	xi.xish.vsustain = ins->vol_env.sustain_start;
	xi.xish.vnum = ins->vol_env.nodes;
	xi.xish.vnum = MIN(xi.xish.vnum, 12);

	xi.xish.ploops = ins->pan_env.loop_start;
	xi.xish.ploope = ins->pan_env.loop_end;
	xi.xish.psustain = ins->pan_env.sustain_start;
	xi.xish.pnum = ins->pan_env.nodes;
	xi.xish.pnum = MIN(xi.xish.pnum, 12);

	/* envelope nodes */
	for (k = 0; k < xi.xish.vnum; k++) {
		xi.xish.env.sep.venv[k].ticks = ins->vol_env.ticks[k];
		xi.xish.env.sep.venv[k].val = ins->vol_env.values[k];
	}

	/* envelope nodes */
	for (k = 0; k < xi.xish.pnum; k++) {
		xi.xish.env.sep.penv[k].ticks = ins->pan_env.ticks[k];
		xi.xish.env.sep.penv[k].val = ins->pan_env.values[k];
	}

	for (k = 0; k < 48; k++)
		xi.xish.env.env[k] = bswapLE16(xi.xish.env.env[k]);

	/* XXX volfade */

	/* Tuesday's coming! Did you bring your coat? */
	memcpy(xi.xish.reserved1, "ILiveInAGiantBucket", 19);

	xi.xish.nsamples = xi_nalloc;

	if (xi_nalloc > 0) {
		/* nab the first sample's info */
		smp = song->samples + xi_invmap[0];

		xi.xish.vibtype = smp->vib_type;
		xi.xish.vibrate = MIN(smp->vib_speed, 63);
		xi.xish.vibdepth = MIN(smp->vib_depth, 15);
		if (xi.xish.vibrate | xi.xish.vibdepth) {
			if (smp->vib_rate) {
				int s = _muldivr(smp->vib_depth, 256, smp->vib_rate);
				xi.xish.vibsweep = CLAMP(s, 0, 255);
			} else {
				xi.xish.vibsweep = 255;
			}
		}
	}

	/* now write the data... */
	disko_write(fp, &xi, sizeof(xi));

	for (k = 0; k < xi_nalloc; k++) {
		int o = xi_invmap[k];
		struct xm_sample_header xmss;
		smp = song->samples + o;
		strncpy(xmss.name, smp->name, sizeof(xmss.name));

		const uint32_t samplesize = smp->length * ((smp->flags & CHN_16BIT) ? 2 : 1) * ((smp->flags & CHN_STEREO) ? 2 : 1);
		xmss.samplen = bswapLE32(samplesize);

		xmss.loopstart = smp->loop_start;
		xmss.looplen = smp->loop_end - smp->loop_start;

		xmss.loopstart = bswapLE32(xmss.loopstart);
		xmss.looplen = bswapLE32(xmss.looplen);

		if (smp->flags & CHN_PINGPONGLOOP) {
			xmss.type |= 0x03;
		} else if (smp->flags & CHN_LOOP) {
			xmss.type |= 0x01;
		}

		if (smp->flags & CHN_16BIT)
			xmss.type |= 0x10;

		if (smp->flags & CHN_STEREO)
			xmss.type |= 0x20;

		xmss.vol = smp->volume >> 2;
		xmss.pan = smp->panning;

		int transp = frequency_to_transpose(smp->c5speed);

		log_appendf(4, "%d", transp);

		xmss.relnote = transp / 128;
		xmss.finetune = transp % 128;

		disko_write(fp, &xmss, sizeof(xmss));
	}

	for (k = 0; k < xi_nalloc; k++) {
		int o = xi_invmap[k];
		smp = song->samples + o;
		csf_write_sample(fp, smp, SF_LE | SF_PCMD
							| ((smp->flags & CHN_16BIT) ? SF_16 : SF_8)
							| ((smp->flags & CHN_STEREO) ? SF_SS : SF_M),
							UINT32_MAX);
	}

	return SAVE_SUCCESS;
}

