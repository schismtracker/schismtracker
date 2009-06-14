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

/* FIXME - shouldn't be using the Modplug headers here. The only reason I'm doing so is because Modplug
has the IT sample decompression code... */
#include "mplink.h"
#include "it_defs.h"

#pragma pack(push, 1)
/* Note: This struct must match the disk layout struct */
struct scri_header {
	//00
	unsigned char type;
	char dosfn[12];
	unsigned char memseg[3];
	//10
	unsigned int length;  // 32 bits
	unsigned int loopbeg; // 32 bits
	unsigned int loopend; // 32 bits
	unsigned char volume;
	char dummy1;
	unsigned char packed;
	unsigned char flags;
	//20
	unsigned int c2spd;   // 32 bits
	char dummy2[4];
	unsigned short dummy_gp;
	unsigned short dummy_512;
	unsigned int dummy_last;
	//30
	char samplename[28];
	//4C
	char samplesig[4]; /* SCRS or SCRI */
	//50
};
#pragma pack(pop)

static int load_scri_sample(const uint8_t *data, size_t length, song_sample *smp, char *title,
                            bool load_sample_data = true)
{
	const scri_header* header = (const scri_header*) data;
	/*
	fprintf(stderr, "%X-%X-%X-%X-%X\n",
	(((char*)&(header->type     ))-((char*)&(header->type))),
	(((char*)&(header->length   ))-((char*)&(header->type))),
	(((char*)&(header->c2spd    ))-((char*)&(header->type))),
	(((char*)&(header->samplename))-((char*)&(header->type))),
	(((char*)&(header->samplesig))-((char*)&(header->type)))
	);

	fprintf(stderr, "Considering %d byte sample (%.4s), %d\n",
	(int)length,
	header->samplesig,
	header->length);
	*/
	if(length < 0x50)
		return false; // too small
	if (strncmp(header->samplesig, "SCRS", 4) != 0
	    && strncmp(header->samplesig, "SCRI", 4) != 0)
		return false; // It should be either SCRS or SCRI.

	size_t samp_length = bswapLE32(header->length);
	int bytes_per_sample = (header->type == 1 ? ((header->flags & 2) ? 2 : 1) : 0); // no sample data

	if (length < 0x50 + smp->length * bytes_per_sample)
		return false;

	smp->length = samp_length;
	smp->global_volume = 64;
	smp->volume = header->volume*256/64;
	smp->loop_start = header->loopbeg;
	smp->loop_end = header->loopend;
	smp->speed = header->c2spd;
	smp->flags = 0;
	if (header->flags & 1)
		smp->flags |= SAMP_LOOP;
	if (header->flags & 2)
		smp->flags |= SAMP_STEREO;
	if (header->flags & 4)
		smp->flags |= SAMP_16_BIT;

	if (header->type == 2) {
		smp->flags |= SAMP_ADLIB;
		smp->flags &= ~(SAMP_LOOP|SAMP_16_BIT);

		memcpy(smp->AdlibBytes, &header->length, 11);

		smp->length = 1;
		smp->loop_start = 0;
		smp->loop_end = 0;

		smp->data = csf_allocate_sample(1);
	}

	int format = (smp->flags & SAMP_16_BIT) ? RS_PCM16S : RS_PCM8U;

	if (load_sample_data) {
		mp->ReadSample((SONGSAMPLE *) smp, format,
			(const char *) (data + 0x50), (uint32_t) (length - 0x50));
	}

	strncpy(smp->filename, header->dosfn, 11);
	strncpy(title, header->samplename, 28);

	return true;
}


int fmt_scri_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
	song_sample tmp;
	song_sample *smp = &tmp;
	char title[32] = "";
	if (!load_scri_sample(data, length, smp, title))
		return false;

	file->smp_length = smp->length;
	file->smp_flags = smp->flags;
	file->smp_defvol = smp->volume;
	file->smp_gblvol = smp->global_volume;
	file->smp_loop_start = smp->loop_start;
	file->smp_loop_end = smp->loop_end;
	file->smp_speed = smp->speed;
	file->smp_filename = (char*) mem_alloc(13);
	memcpy(file->smp_filename, smp->filename, 12);
	file->smp_filename[12] = 0;

	file->description = "ST32 Sample/Instrument";
	file->title = (char *) mem_alloc(32);
	memcpy(file->title, title, 32);
	file->title[31] = 0;
	file->type = TYPE_SAMPLE_EXTD | TYPE_INST_OTHER;
	return true;
}

int fmt_scri_load_sample(const uint8_t *data, size_t length, song_sample *smp, char *title)
{
	return load_scri_sample(data, length, smp, title);
}

static bool MidiS3M_Read(song_instrument* g, const char* name, int& ft)
{
	//fprintf(stderr, "Name(%s)\n", name);

	// GM=General MIDI
	if(name[0] == 'G') {
		bool is_percussion = false;
		if(name[1] == 'M') {
			/* nothing */
		} else if (name[1] == 'P') {
			is_percussion = true;
		} else {
			return false;
		}
		const char*s = name+2;
		int GM = 0;      // midi program
		ft = 0;          // finetuning
		int scale = 63;  // automatic volume scaling
		int autoSDx = 0; // automatic randomized SDx effect
		int bank = 0;    // midi bank
		while (isdigit_safe(*s))
			GM = GM*10 + (*s++)-'0';
		for(;;) {
			int sign=0;
			if (*s == '-')
				sign = 1;
			if (sign || *s == '+') {
				for (ft = 0; isdigit_safe(*++s); ft = ft * 10 + (*s - '0')) {
					/* nothing */
				}
				if (sign)
					ft = -ft;
				continue;
			}
			if (*s == '/') {
				for (scale = 0; isdigit_safe(*++s); scale = scale * 10 + (*s - '0')) {
					/* continue */
				}
				continue;
			}
			if (*s == '&') {
				for (autoSDx = 0; isdigit_safe(*++s); autoSDx = autoSDx * 10 + (*s - '0')) {
					/* nothing */
				}
				if (autoSDx > 15)
					autoSDx &= 15;
				continue;
			}
			if (*s == '%') {
				for (bank = 0; isdigit_safe(*++s); bank = bank * 10 + (*s - '0')) {
					/* nothing */
				}
				continue;
			}
			break;
		}
		// wMidiBank, nMidiProgram, nMidiChannel, nMidiDrumKey
		g->midi_bank = bank;
		if(is_percussion) {
			g->midi_drum_key = GM;
			g->midi_channel_mask = 1 << 9;
			g->midi_program = 128 + GM;
		} else {
			g->midi_program = GM - 1;
			g->midi_channel_mask = 0xFFFF &~ (1 << 9); // any channel except percussion
		}

		/* TODO: Apply ft, apply scale, apply autoSDx,
		 * FIXME: Channel note changes don't affect MIDI notes */
		return true;
	}
	return false;
}


int fmt_scri_load_instrument(const uint8_t *data, size_t length, int slot)
{
	if(length < 0x50)
		return false;

	song_instrument *g = song_get_instrument(slot, NULL);

	char *np;
	song_sample *smp = song_get_sample(slot, &np);

	if (!load_scri_sample(data, length, smp, np)) {
		return false;
	}

	strncpy(g->name, np, 25);
	strncpy(g->filename, smp->filename, 11);

	int ft = 0;
	g->global_volume = smp->global_volume * 2;

	g->midi_bank = 0;
	g->midi_program = 0;
	g->midi_channel_mask = 0;
	g->midi_drum_key = 0;

	MidiS3M_Read(g, np, ft);

	for(int n = 0; n < 128; ++n) {
		g->note_map[n] = n+1+ft;
		g->sample_map[n] = slot;
	}

	return true;
}
