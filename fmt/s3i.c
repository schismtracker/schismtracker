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

/* FIXME this file is redundant and needs to go away -- s3m.c already does all of this */

#define NEED_BYTESWAP
#include "headers.h"
#include "fmt.h"

#include "song.h"
#include "sndfile.h"

#pragma pack(push, 1)
/* Note: This struct must match the disk layout struct */
struct s3i_header {
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

static int load_s3i_sample(const uint8_t *data, size_t length, song_sample_t *smp)
{
	const struct s3i_header* header = (const struct s3i_header*) data;
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
		return 0; // too small
	if (strncmp(header->samplesig, "SCRS", 4) != 0
	    && strncmp(header->samplesig, "SCRI", 4) != 0)
		return 0; // It should be either SCRS or SCRI.

	size_t samp_length = bswapLE32(header->length);
	int bytes_per_sample = (header->type == 1 ? ((header->flags & 2) ? 2 : 1) : 0); // no sample data

	if (length < 0x50 + smp->length * bytes_per_sample)
		return 0;

	smp->length = samp_length;
	smp->global_volume = 64;
	smp->volume = header->volume*256/64;
	smp->loop_start = header->loopbeg;
	smp->loop_end = header->loopend;
	smp->c5speed = header->c2spd;
	smp->flags = 0;
	if (header->flags & 1)
		smp->flags |= CHN_LOOP;
	if (header->flags & 2)
		smp->flags |= CHN_STEREO;
	if (header->flags & 4)
		smp->flags |= CHN_16BIT;

	if (header->type == 2) {
		smp->flags |= CHN_ADLIB;
		smp->flags &= ~(CHN_LOOP|CHN_16BIT);

		memcpy(smp->adlib_bytes, &header->length, 11);

		smp->length = 1;
		smp->loop_start = 0;
		smp->loop_end = 0;

		smp->data = csf_allocate_sample(1);
	}

	int format = SF_M | SF_LE; // endianness; channels
	format |= (smp->flags & CHN_16BIT) ? (SF_16 | SF_PCMS) : (SF_8 | SF_PCMU); // bits; encoding

	csf_read_sample((song_sample_t *) smp, format,
		(const char *) (data + 0x50), (uint32_t) (length - 0x50));

	strncpy(smp->filename, header->dosfn, 11);
	strncpy(smp->name, header->samplename, 25);

	return 1;
}


int fmt_s3i_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
	song_sample_t tmp;
	song_sample_t *smp = &tmp;
	if (!load_s3i_sample(data, length, smp))
		return 0;

	file->smp_length = smp->length;
	file->smp_flags = smp->flags;
	file->smp_defvol = smp->volume;
	file->smp_gblvol = smp->global_volume;
	file->smp_loop_start = smp->loop_start;
	file->smp_loop_end = smp->loop_end;
	file->smp_speed = smp->c5speed;
	file->smp_filename = strn_dup(smp->filename, 12);

	file->description = "Scream Tracker Sample";
	file->title = strn_dup(smp->name, 25);
	file->type = TYPE_SAMPLE_EXTD | TYPE_INST_OTHER;
	return 1;
}

int fmt_s3i_load_sample(const uint8_t *data, size_t length, song_sample_t *smp)
{
	// what the crap?
	return load_s3i_sample(data, length, smp);
}

