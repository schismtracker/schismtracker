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

/* --------------------------------------------------------------------- */

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
	//uint8_t reserved1[36];
	uint16_t insID; // Instrument ID [0..0xFFFF] [?]
	char insname[16]; // Instrument name (in ASCII)
	uint32_t inssize; // Instrument size
	uint8_t layers;
	uint8_t reserved2[40];
	uint8_t layerduplicate;
	uint8_t layer;
	uint32_t layersize;
	uint8_t smpnum;
	//uint8_t reserved3[40];
};

static int read_pat_header(struct GF1PatchHeader *hdr, slurp_t *fp)
{
#define READ_VALUE(name) \
	do { if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) { return 0; } } while (0)

	READ_VALUE(sig);
	READ_VALUE(ver);
	READ_VALUE(id);
	READ_VALUE(desc);
	READ_VALUE(insnum);
	READ_VALUE(voicenum);
	READ_VALUE(channum);
	READ_VALUE(waveforms);
	READ_VALUE(mastervol);
	READ_VALUE(datasize);
	slurp_seek(fp, 36, SEEK_CUR); // reserved
	READ_VALUE(insID);
	READ_VALUE(insname);
	READ_VALUE(inssize);
	READ_VALUE(layers);
	slurp_seek(fp, 40, SEEK_CUR); // reserved
	READ_VALUE(layerduplicate);
	READ_VALUE(layer);
	READ_VALUE(layersize);
	READ_VALUE(smpnum);
	slurp_seek(fp, 40, SEEK_CUR); // reserved

#undef READ_VALUE

	if ((memcmp(hdr->sig, "GF1PATCH", 8))
	    || (memcmp(hdr->ver, "110\0", 4) && memcmp(hdr->ver, "100\0", 4))
	    || (memcmp(hdr->id, "ID#000002\0", 10)))
		return 0;

	hdr->waveforms = bswapLE16(hdr->waveforms);
	hdr->mastervol = bswapLE16(hdr->mastervol);
	hdr->datasize  = bswapLE32(hdr->datasize);
	hdr->insID     = bswapLE16(hdr->insID);
	hdr->inssize   = bswapLE32(hdr->inssize);
	hdr->layersize = bswapLE32(hdr->layersize);

	return 1;
}

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
	//uint8_t reserved[36];
};

static int read_pat_sample_header(struct GF1PatchSampleHeader *hdr, slurp_t *fp)
{
#define READ_VALUE(name) \
	do { if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) { return 0; } } while (0)

	READ_VALUE(wavename);
	READ_VALUE(fractions);
	READ_VALUE(samplesize);
	READ_VALUE(loopstart);
	READ_VALUE(loopend);
	READ_VALUE(samplerate);
	READ_VALUE(lofreq);
	READ_VALUE(hifreq);
	READ_VALUE(rtfreq);
	READ_VALUE(tune);
	READ_VALUE(panning);
	READ_VALUE(envelopes);
	READ_VALUE(trem_speed);
	READ_VALUE(trem_rate);
	READ_VALUE(trem_depth);
	READ_VALUE(vib_speed);
	READ_VALUE(vib_rate);
	READ_VALUE(vib_depth);
	READ_VALUE(smpmode);
	READ_VALUE(scalefreq);
	READ_VALUE(scalefac);
	slurp_seek(fp, 36, SEEK_CUR); // reserved

#undef READ_VALUE

	hdr->samplesize = bswapLE32(hdr->samplesize);
	hdr->loopstart = bswapLE32(hdr->loopstart);
	hdr->loopend = bswapLE32(hdr->loopend);
	hdr->samplerate = bswapLE16(hdr->samplerate);
	hdr->lofreq = bswapLE32(hdr->lofreq);
	hdr->hifreq = bswapLE32(hdr->hifreq);
	hdr->rtfreq = bswapLE32(hdr->rtfreq);
	hdr->tune = bswapLE16(hdr->tune);
	hdr->scalefreq = bswapLE16(hdr->scalefac);

	return 1;
}

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

int fmt_pat_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct GF1PatchHeader hdr;

	if (slurp_read(fp, &hdr, sizeof(hdr)) != sizeof(hdr))
		return 0;

	if ((memcmp(hdr.sig, "GF1PATCH", 8) != 0)
	    || (memcmp(hdr.ver, "110\0", 4) != 0 && memcmp(hdr.ver, "100\0", 4) != 0)
	    || (memcmp(hdr.id, "ID#000002\0", 10) != 0))
		return 0;

	file->description = "Gravis Patch File";
	file->title = strn_dup(hdr.insname, 16);
	file->type = TYPE_INST_OTHER;
	return 1;
}


int fmt_pat_load_instrument(slurp_t *fp, int slot)
{
	struct GF1PatchHeader header;
	struct GF1PatchSampleHeader gfsamp;
	struct instrumentloader ii;
	song_instrument_t *g;
	song_sample_t *smp;
	unsigned int rs;
	int lo, hi, tmp, i, nsamp, n;

	if (!slot)
		return 0;

	if (!read_pat_header(&header, fp))
		return 0;

	g = instrument_loader_init(&ii, slot);
	memcpy(g->name, header.insname, 16);
	g->name[15] = '\0';

	nsamp = CLAMP(header.smpnum, 1, 16);
	for (i = 0; i < 120; i++) {
		g->sample_map[i] = 0;
		g->note_map[i] = i + 1;
	}
	for (i = 0; i < nsamp; i++) {
		if (!read_pat_sample_header(&gfsamp, fp))
			return 0;

		n = instrument_loader_sample(&ii, i + 1);
		smp = song_get_sample(n);

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

		csf_read_sample(smp, rs, fp);
	}
	return 1;
}
