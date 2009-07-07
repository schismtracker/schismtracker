/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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

#include <string.h>
#include <stdlib.h>

/* --------------------------------------------------------------------- */
#pragma pack(1)

struct GF1PatchHeader
{
	unsigned char	sig[22];
	unsigned char	desc[60];
	unsigned char	insnum;
	unsigned char	voicenum;
	unsigned char	channum;
	unsigned short	waveforms;
	unsigned short	mastervol;
	unsigned int	datasize;
	unsigned char	reserved1[36];
	unsigned int	insID;
	unsigned char	insname[16];
	unsigned int	inssize;
	unsigned char	layers;
	unsigned char	reserved2[40];
	unsigned char	layerduplicate;
	unsigned char	layer;
	unsigned int	layersize;
	unsigned char	smpnum;
	unsigned char	reserved3[40];
};

struct GF1PatchSampleHeader
{
	unsigned char	wavename[7];
	unsigned char	fractions;
	unsigned int	samplesize;
	int	loopstart;
	int	loopend;
	unsigned short	samplerate;
	unsigned int	lofreq;
	unsigned int	hifreq;
	unsigned int	rtfreq;
	unsigned short	tune;
	unsigned char	panning;

	unsigned char	envelopes[12];
	unsigned char	tremolo[3];
	unsigned char	vibrato[3];

	unsigned char	smpmode;
	unsigned short	scalefreq;
	unsigned short	scalefac;
	unsigned char	reserved[36];
};
#pragma pack()

/* --------------------------------------------------------------------- */

static int gusfreq(unsigned int freq)
{
	static const unsigned int scale_table[109] = 
	{ 
/*C-0..B-*/
/* Octave 0 */  16351, 17323, 18354, 19445, 20601, 21826, 23124, 24499, 25956, 27500, 29135, 30867,
/* Octave 1 */  32703, 34647, 36708, 38890, 41203, 43653, 46249, 48999, 51913, 54999, 58270, 61735,
/* Octave 2 */  65406, 69295, 73416, 77781, 82406, 87306, 92498, 97998, 103826, 109999, 116540, 123470,
/* Octave 3 */  130812, 138591, 146832, 155563, 164813, 174614, 184997, 195997, 207652, 219999, 233081, 246941,
/* Octave 4 */  261625, 277182, 293664, 311126, 329627, 349228, 369994, 391995, 415304, 440000, 466163, 493883,
/* Octave 5 */  523251, 554365, 587329, 622254, 659255, 698456, 739989, 783991, 830609, 880000, 932328, 987767,
/* Octave 6 */  1046503, 1108731, 1174660, 1244509, 1318511, 1396914, 1479979, 1567983, 1661220, 1760002, 1864657, 1975536,
/* Octave 7 */  2093007, 2217464, 2349321, 2489019, 2637024, 2793830, 2959960, 3135968, 3322443, 3520006, 3729316, 3951073,
/* Octave 8 */  4186073, 4434930, 4698645, 4978041, 5274051, 5587663, 5919922, 6271939, 6644889, 7040015, 7458636, 7902150, 0xFFFFFFFF
	};
	unsigned int no;

	for (no = 0; no < sizeof(scale_table)/sizeof(int)-1; no++)
	{
		if (scale_table[no] <= freq && scale_table[no+1] >= freq) return (no-12);
	}

	return 4*12;
}




/* --------------------------------------------------------------------- */
int fmt_pat_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
	if (length <= 23) return false;
	if (memcmp(data, "GF1PATCH", 8) != 0) return false;
	if (memcmp(data+8, "110", 3) != 0
	&&  memcmp(data+8, "100", 3) != 0) return false;
	if (data[11] != 0) return false;
	file->description = "Gravis Patch File";
	file->title = strdup(((char*)data)+12);
	file->type = TYPE_INST_OTHER;
	return true;
}


int fmt_pat_load_instrument(const uint8_t *data, size_t length, int slot)
{
	struct GF1PatchHeader header;
	struct GF1PatchSampleHeader gfsamp;
        struct instrumentloader ii;
        song_instrument *g;
	song_sample *smp;
	unsigned int pos, lo, hi, tmp, rs;
	int i, nsamp, n;

	if (length < sizeof(header)) return false;
	if (memcmp(data, "GF1PATCH", 8) != 0) return false;
	if (!slot) return false;

	memcpy(&header, data, sizeof(header));
	header.waveforms = bswapLE16(header.waveforms);
	header.mastervol = bswapLE16(header.mastervol);
	header.datasize  = bswapLE32(header.datasize);
	header.insID     = bswapLE32(header.insID);
	header.inssize   = bswapLE32(header.inssize);
	header.layersize = bswapLE32(header.layersize);

	g = instrument_loader_init(&ii, slot);
	memcpy(g->name, header.insname, 16);
	g->name[15] = '\0';

	nsamp = header.smpnum <= 16 ? header.smpnum : 16;
	pos = sizeof(header);
	for (i = 0; i < 120; i++) {
		g->sample_map[i] = 0;
		g->note_map[i] = i+1;
	}
	for (i = 0; i < nsamp; i++) {
		memcpy(&gfsamp, data+pos, sizeof(gfsamp));

		pos += sizeof(gfsamp);

		n = instrument_loader_sample(&ii, i+1);
		smp = song_get_sample(n, NULL);

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
			tmp = lo; lo = hi; hi = tmp;
		}
		for (; lo < hi; lo++) g->sample_map[lo+12] = n;

		smp->length = gfsamp.samplesize;
		smp->loop_start = gfsamp.loopstart;
		smp->loop_end = gfsamp.loopend;
		smp->speed = gfsamp.samplerate;
		
		smp->flags = 0;
		if (gfsamp.smpmode & 2) {
			rs = (gfsamp.smpmode & 1) ? RS_PCM16S : RS_PCM8S;
		} else {
			rs = (gfsamp.smpmode & 1) ? RS_PCM16U : RS_PCM8U;
		}
		if (gfsamp.smpmode & 4) smp->flags |= SAMP_LOOP;
		if (gfsamp.smpmode & 8) smp->flags |= SAMP_LOOP_PINGPONG;
		if (gfsamp.smpmode & 32) smp->flags |= SAMP_SUSLOOP;
		memcpy(smp->filename, gfsamp.wavename, 7);
		smp->filename[8] = '\0';
		smp->vib_speed = gfsamp.vibrato[0];
		smp->vib_rate  = gfsamp.vibrato[1];
		smp->vib_depth = gfsamp.vibrato[2];

printf("reading len=%d\n",smp->length);
		pos += song_copy_sample_raw(n, rs, data+pos, length - pos);
	}
	return true;
}

