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
#include "mem.h"

#include "it.h"
#include "song.h"

#include "player/sndfile.h"

#include <string.h>
#include <stdlib.h>

/* --------------------------------------------------------------------- */

struct xm_sample_header {
	uint32_t samplen;
	uint32_t loopstart;
	uint32_t looplen;
	uint8_t vol;
	int8_t finetune;
	uint8_t type;
	uint8_t pan;
	int8_t relnote;
	uint8_t res;
	char name[22];
};

struct xi_file_header {
	int8_t header[0x15];    // "Extended Instrument: "
	int8_t name[0x16];      // Name of instrument
	uint8_t magic;          // 0x1a, DOS EOF char so you can 'type file.xi'
	int8_t tracker[0x14];   // Name of tracker
	uint16_t version;       // big-endian 0x0102

	// sample header
	struct {
		uint8_t snum[96];
		struct {
			uint16_t ticks;        // Time in tracker ticks
			uint16_t val;        // Value from 0x00 to 0x40.
		} venv[12], penv[12];
		uint8_t vnum, pnum;
		uint8_t vsustain, vloops, vloope, psustain, ploops, ploope;
		uint8_t vtype, ptype;
		uint8_t vibtype, vibsweep, vibdepth, vibrate;
		uint16_t volfade;
		uint8_t reserved1[0x16];
		uint16_t nsamples;
	} xish;
};

static int xm_sample_header_read(struct xm_sample_header *shdr, slurp_t *fp)
{
#define READ_VALUE(name) do { if (slurp_read(fp, &shdr->name, sizeof(shdr->name)) != sizeof(shdr->name)) { return 0; } } while (0)

	READ_VALUE(samplen);
	READ_VALUE(loopstart);
	READ_VALUE(looplen);
	READ_VALUE(vol);
	READ_VALUE(finetune);
	READ_VALUE(type);
	READ_VALUE(pan);
	READ_VALUE(relnote);
	READ_VALUE(res);
	READ_VALUE(name);

#undef READ_VALUE

	return 1;
}

static int xi_file_header_write(struct xi_file_header *hdr, disko_t *fp)
{
#define WRITE_VALUE(name) do { disko_write(fp, &hdr->name, sizeof(hdr->name)); } while (0)

	WRITE_VALUE(header);
	WRITE_VALUE(name);
	WRITE_VALUE(magic);
	WRITE_VALUE(tracker);
	WRITE_VALUE(version);
	WRITE_VALUE(xish.snum);
	for (size_t i = 0; i < 12; i++) {
		WRITE_VALUE(xish.venv[i].ticks);
		WRITE_VALUE(xish.venv[i].val);
	}
	for (size_t i = 0; i < 12; i++) {
		WRITE_VALUE(xish.penv[i].ticks);
		WRITE_VALUE(xish.penv[i].val);
	}
	WRITE_VALUE(xish.vnum);
	WRITE_VALUE(xish.pnum);
	WRITE_VALUE(xish.vsustain);
	WRITE_VALUE(xish.vloops);
	WRITE_VALUE(xish.vloope);
	WRITE_VALUE(xish.psustain);
	WRITE_VALUE(xish.ploops);
	WRITE_VALUE(xish.ploope);
	WRITE_VALUE(xish.vtype);
	WRITE_VALUE(xish.ptype);
	WRITE_VALUE(xish.vibtype);
	WRITE_VALUE(xish.vibsweep);
	WRITE_VALUE(xish.vibdepth);
	WRITE_VALUE(xish.vibrate);
	WRITE_VALUE(xish.volfade);
	WRITE_VALUE(xish.reserved1);
	WRITE_VALUE(xish.nsamples);

#undef WRITE_VALUE

	return 1;
}

static int xm_sample_header_write(struct xm_sample_header *shdr, disko_t *fp)
{
#define WRITE_VALUE(name) do { disko_write(fp, &shdr->name, sizeof(shdr->name)); } while (0)

	WRITE_VALUE(samplen);
	WRITE_VALUE(loopstart);
	WRITE_VALUE(looplen);
	WRITE_VALUE(vol);
	WRITE_VALUE(finetune);
	WRITE_VALUE(type);
	WRITE_VALUE(pan);
	WRITE_VALUE(relnote);
	WRITE_VALUE(res);
	WRITE_VALUE(name);

#undef WRITE_VALUE

	return 1;
}

static int xi_file_header_read(struct xi_file_header *hdr, slurp_t *fp)
{
#define READ_VALUE(name) do { if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) { return 0; } } while (0)

	READ_VALUE(header);
	READ_VALUE(name);
	READ_VALUE(magic);
	READ_VALUE(tracker);
	READ_VALUE(version);
	READ_VALUE(xish.snum);
	for (size_t i = 0; i < 12; i++) {
		READ_VALUE(xish.venv[i].ticks);
		READ_VALUE(xish.venv[i].val);
	}
	for (size_t i = 0; i < 12; i++) {
		READ_VALUE(xish.penv[i].ticks);
		READ_VALUE(xish.penv[i].val);
	}
	READ_VALUE(xish.vnum);
	READ_VALUE(xish.pnum);
	READ_VALUE(xish.vsustain);
	READ_VALUE(xish.vloops);
	READ_VALUE(xish.vloope);
	READ_VALUE(xish.psustain);
	READ_VALUE(xish.ploops);
	READ_VALUE(xish.ploope);
	READ_VALUE(xish.vtype);
	READ_VALUE(xish.ptype);
	READ_VALUE(xish.vibtype);
	READ_VALUE(xish.vibsweep);
	READ_VALUE(xish.vibdepth);
	READ_VALUE(xish.vibrate);
	READ_VALUE(xish.volfade);
	READ_VALUE(xish.reserved1);
	READ_VALUE(xish.nsamples);

#undef READ_VALUE

	if (memcmp(hdr->header, "Extended Instrument: ", 0x15))
		return 0;

	if (hdr->magic != 0x1a)
		return 0;

	if (bswapLE16(hdr->version) != 0x0102)
		return 0;

	return 1;
}

#define XI_ENV_ENABLED 0x01
#define XI_ENV_SUSTAIN 0x02
#define XI_ENV_LOOP    0x04

/* --------------------------------------------------------------------- */

int fmt_xi_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct xi_file_header xi;
	if (!xi_file_header_read(&xi, fp))
		return 0;

	file->description = "Fasttracker II Instrument";
	file->title = strn_dup((const char *)xi.name, sizeof(xi.name));
	file->type = TYPE_INST_XI;
	return 1;
}

int fmt_xi_load_instrument(slurp_t *fp, int slot)
{
	struct xi_file_header xi;
	struct instrumentloader ii;
	song_instrument_t *g;
	int k, prevtick;

	if (!xi_file_header_read(&xi, fp))
		return 0;

	if (!slot)
		return 0;

	//song_delete_instrument(slot, 0);

	g = instrument_loader_init(&ii, slot);
	memcpy(g->name, xi.name, 22);
	g->name[22] = '\0';

	for (k = 0; k < 96; k++) {
		if (xi.xish.snum[k] > 15)
			xi.xish.snum[k] = 15;
		xi.xish.snum[k] = instrument_loader_sample(&ii, xi.xish.snum[k] + 1);
		g->note_map[k + 12] = k + 1 + 12;
		if (xi.xish.snum[k])
			g->sample_map[k + 12] = xi.xish.snum[k];
	}

	for (k = 0; k < 12; k++) {
		g->note_map[k] = 0;
		g->sample_map[k] = 0;
		g->note_map[k + 108] = 0;
		g->sample_map[k + 108] = 0;
	}

	// bswap all volume and panning envelope points
	for (k = 0; k < 12; k++) {
		xi.xish.venv[k].ticks = bswapLE16(xi.xish.venv[k].ticks);
		xi.xish.venv[k].val = bswapLE16(xi.xish.venv[k].val);
		xi.xish.penv[k].ticks = bswapLE16(xi.xish.penv[k].ticks);
		xi.xish.penv[k].val = bswapLE16(xi.xish.penv[k].val);
	}

	// Set up envelope types in instrument
	if (xi.xish.vtype & XI_ENV_ENABLED) g->flags |= ENV_VOLUME;
	if (xi.xish.vtype & XI_ENV_SUSTAIN) g->flags |= ENV_VOLSUSTAIN;
	if (xi.xish.vtype & XI_ENV_LOOP)    g->flags |= ENV_VOLLOOP;
	if (xi.xish.ptype & XI_ENV_ENABLED) g->flags |= ENV_PANNING;
	if (xi.xish.ptype & XI_ENV_SUSTAIN) g->flags |= ENV_PANSUSTAIN;
	if (xi.xish.ptype & XI_ENV_LOOP)    g->flags |= ENV_PANLOOP;

	prevtick = -1;
	// Copy envelopes into instrument
	for (k = 0; k < xi.xish.vnum; k++) {
		if (xi.xish.venv[k].ticks < prevtick)
			prevtick++;
		else
			prevtick = xi.xish.venv[k].ticks;
		g->vol_env.ticks[k] = prevtick;
		g->vol_env.values[k] = xi.xish.venv[k].val;
	}

	prevtick = -1;
	for (k = 0; k < xi.xish.pnum; k++) {
		if (xi.xish.penv[k].ticks < prevtick)
			prevtick++;
		else
			prevtick = xi.xish.penv[k].ticks;
		g->pan_env.ticks[k] = prevtick;
		g->pan_env.values[k] = xi.xish.penv[k].val;
	}

	g->vol_env.loop_start = xi.xish.vloops;
	g->vol_env.loop_end = xi.xish.vloope;
	g->vol_env.sustain_start = xi.xish.vsustain;
	g->vol_env.nodes = xi.xish.vnum;

	g->pan_env.loop_start = xi.xish.ploops;
	g->pan_env.loop_end = xi.xish.ploope;
	g->pan_env.sustain_start = xi.xish.psustain;
	g->pan_env.nodes = xi.xish.pnum;

	xi.xish.volfade = bswapLE16(xi.xish.volfade);
	xi.xish.nsamples = bswapLE16(xi.xish.nsamples);

	for (k = 0; k < xi.xish.nsamples; k++) {
		struct xm_sample_header xmss;
		song_sample_t *smp;
		unsigned int rs, samplesize, n;

		if (!xm_sample_header_read(&xmss, fp))
			break;

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
		smp->vib_type = xi.xish.vibtype;
		smp->vib_speed = MIN(xi.xish.vibrate, 64);
		smp->vib_depth = MIN(xi.xish.vibdepth, 32);
		if (xi.xish.vibrate | xi.xish.vibdepth) {
			if (xi.xish.vibsweep) {
				int s = _muldivr(smp->vib_depth, 256, xi.xish.vibsweep);
				smp->vib_rate = CLAMP(s, 0, 255);
			} else {
				smp->vib_rate = 255;
			}
		}

		smp->c5speed = transpose_to_frequency(xmss.relnote, xmss.finetune);
		slurp_seek(fp, csf_read_sample(current_song->samples + n, rs, fp), SEEK_CUR);
	}

	return 1;
}

/* ------------------------------------------------------------------------ */

int fmt_xi_save_instrument(disko_t *fp, song_t *song, song_instrument_t *ins)
{
	struct xi_file_header xi = {0};
	song_sample_t *smp;
	int k;

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
	strncpy((char *)xi.name, ins->name, sizeof(xi.name));
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
		xi.xish.venv[k].ticks = ins->vol_env.ticks[k];
		xi.xish.venv[k].val = ins->vol_env.values[k];
	}

	/* envelope nodes */
	for (k = 0; k < xi.xish.pnum; k++) {
		xi.xish.penv[k].ticks = ins->pan_env.ticks[k];
		xi.xish.penv[k].val = ins->pan_env.values[k];
	}

	// bswap all volume and panning envelope points
	for (k = 0; k < 12; k++) {
		xi.xish.venv[k].ticks = bswapLE16(xi.xish.venv[k].ticks);
		xi.xish.venv[k].val = bswapLE16(xi.xish.venv[k].val);
		xi.xish.penv[k].ticks = bswapLE16(xi.xish.penv[k].ticks);
		xi.xish.penv[k].val = bswapLE16(xi.xish.penv[k].val);
	}

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
	xi_file_header_write(&xi, fp);

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

		xm_sample_header_write(&xmss, fp);
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

