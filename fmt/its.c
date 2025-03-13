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

#include "player/sndfile.h"
#include "song.h"

/* --------------------------------------------------------------------- */

// IT Sample Format
struct it_sample {
	uint32_t id;            // 0x53504D49
	int8_t filename[12];
	uint8_t zero;
	uint8_t gvl;
	uint8_t flags;
	uint8_t vol;
	int8_t name[26];
	uint8_t cvt;
	uint8_t dfp;
	uint32_t length;
	uint32_t loopbegin;
	uint32_t loopend;
	uint32_t c5speed;
	uint32_t susloopbegin;
	uint32_t susloopend;
	uint32_t samplepointer;
	uint8_t vis;
	uint8_t vid;
	uint8_t vir;
	uint8_t vit;
};

/* --------------------------------------------------------------------- */

int fmt_its_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct it_sample its;
	unsigned char title[25];

	if (slurp_read(fp, &its, sizeof(its)) != sizeof(its)
		|| memcmp(&its.id, "IMPS", 4))
		return 0;

	file->smp_length = bswapLE32(its.length);
	file->smp_flags = 0;

	if (its.flags & 2)
		file->smp_flags |= CHN_16BIT;

	if (its.flags & 16) {
		file->smp_flags |= CHN_LOOP;
		if (its.flags & 64)
			file->smp_flags |= CHN_PINGPONGLOOP;
	}

	if (its.flags & 32) {
		file->smp_flags |= CHN_SUSTAINLOOP;
		if (its.flags & 128)
			file->smp_flags |= CHN_PINGPONGSUSTAIN;
	}

	if (its.dfp & 128) file->smp_flags |= CHN_PANNING;
	if (its.flags & 4) file->smp_flags |= CHN_STEREO;

	file->smp_defvol = its.vol;
	file->smp_gblvol = its.gvl;
	file->smp_vibrato_speed = its.vis;
	file->smp_vibrato_depth = its.vid & 0x7f;
	file->smp_vibrato_rate = its.vir;

	file->smp_loop_start = bswapLE32(its.loopbegin);
	file->smp_loop_end = bswapLE32(its.loopend);
	file->smp_speed = bswapLE32(its.c5speed);
	file->smp_sustain_start = bswapLE32(its.susloopbegin);
	file->smp_sustain_end = bswapLE32(its.susloopend);

	file->smp_filename = strn_dup((const char *)its.filename, sizeof(its.filename));
	file->description = "Impulse Tracker Sample";
	file->type = TYPE_SAMPLE_EXTD;

	slurp_seek(fp, 20, SEEK_SET);
	if (slurp_read(fp, title, sizeof(title)) != sizeof(title))
		return 0;

	file->title = strn_dup((const char *)title, sizeof(title));

	return 1;
}

// cwtv should be 0x214 when loading from its or iti
int load_its_sample(slurp_t *fp, song_sample_t *smp, uint16_t cwtv)
{
	struct it_sample its;

#define READ_VALUE(name) do { if (slurp_read(fp, &its.name, sizeof(its.name)) != sizeof(its.name)) { return 0; } } while (0)

	READ_VALUE(id);
	READ_VALUE(filename);
	READ_VALUE(zero);
	READ_VALUE(gvl);
	READ_VALUE(flags);
	READ_VALUE(vol);
	READ_VALUE(name);
	READ_VALUE(cvt);
	READ_VALUE(dfp);
	READ_VALUE(length);
	READ_VALUE(loopbegin);
	READ_VALUE(loopend);
	READ_VALUE(c5speed);
	READ_VALUE(susloopbegin);
	READ_VALUE(susloopend);
	READ_VALUE(samplepointer);
	READ_VALUE(vis);
	READ_VALUE(vid);
	READ_VALUE(vir);
	READ_VALUE(vit);

#undef READ_VALUE

	if (its.id != bswapLE32(0x53504D49))
		return 0;

	/* alright, let's get started */
	smp->length = bswapLE32(its.length);
	if ((its.flags & 1) == 0) {
		// sample associated with header
		return 0;
	}

	smp->global_volume = its.gvl;
	if (its.flags & 16) {
		smp->flags |= CHN_LOOP;
		if (its.flags & 64)
			smp->flags |= CHN_PINGPONGLOOP;
	}
	if (its.flags & 32) {
		smp->flags |= CHN_SUSTAINLOOP;
		if (its.flags & 128)
			smp->flags |= CHN_PINGPONGSUSTAIN;
	}

	/* IT sometimes didn't clear the flag after loading a stereo sample. This appears to have
	 * been fixed sometime before IT 2.14, which is fortunate because that's what a lot of other
	 * programs annoyingly identify themselves as. */
	if (cwtv < 0x0214)
		its.flags &= ~4;

	memcpy(smp->name, (const char *)its.name, MIN(sizeof(smp->name), sizeof(its.name)));
	smp->name[ARRAY_SIZE(smp->name) - 1] = '\0';
	memcpy(smp->filename, (const char *)its.filename, MIN(sizeof(smp->filename), sizeof(its.filename)));
	smp->filename[ARRAY_SIZE(smp->filename) - 1] = '\0';

	smp->volume = its.vol * 4;
	smp->panning = (its.dfp & 127) * 4;
	if (its.dfp & 128)
		smp->flags |= CHN_PANNING;
	smp->loop_start = bswapLE32(its.loopbegin);
	smp->loop_end = bswapLE32(its.loopend);
	smp->c5speed = bswapLE32(its.c5speed);
	smp->sustain_start = bswapLE32(its.susloopbegin);
	smp->sustain_end = bswapLE32(its.susloopend);

	int vibs[] = {VIB_SINE, VIB_RAMP_DOWN, VIB_SQUARE, VIB_RANDOM};
	smp->vib_type = vibs[its.vit & 3];
	smp->vib_rate = its.vir;
	smp->vib_depth = its.vid;
	smp->vib_speed = its.vis;

	// sanity checks purged, csf_adjust_sample_loop already does them  -paper

	its.samplepointer = bswapLE32(its.samplepointer);

	const int64_t pos = slurp_tell(fp);
	if (pos < 0)
		return 0;

	int r;

	if ((its.flags & 1) && its.cvt == 64 && its.length == 12) {
		// OPL instruments in OpenMPT MPTM files (which are essentially extended IT files)
		slurp_seek(fp, its.samplepointer, SEEK_SET);
		r = slurp_read(fp, smp->adlib_bytes, 12);
		smp->flags |= CHN_ADLIB;
		// dumb hackaround that ought to some day be fixed:
		smp->length = 1;
		smp->data = csf_allocate_sample(1);
	} else if (its.flags & 1) {
		slurp_seek(fp, its.samplepointer, SEEK_SET);

		// endianness (always zero)
		uint32_t flags = SF_LE;
		// channels
		flags |= (its.flags & 4) ? SF_SS : SF_M;
		if (its.flags & 8) {
			// compression algorithm
			flags |= (its.cvt & 4) ? SF_IT215 : SF_IT214;
		} else {
			// signedness (or delta?)

			// XXX for some reason I had a note in pm/fmt/it.c saying that I had found some
			// .it files with the signed flag set incorrectly and to assume unsigned when
			// hdr.cwtv < 0x0202. Why, and for what files?
			// Do any other players use the header for deciding sample data signedness?
			flags |= (its.cvt & 4) ? SF_PCMD : (its.cvt & 1) ? SF_PCMS : SF_PCMU;
		}
		// bit width
		flags |= (its.flags & 2) ? SF_16 : SF_8;

		r = csf_read_sample(smp, flags, fp);
	} else {
		r = smp->length = 0;
	}

	slurp_seek(fp, pos, SEEK_SET);

	return r;
}

int fmt_its_load_sample(slurp_t *fp, song_sample_t *smp)
{
	return load_its_sample(fp, smp, 0x0214);
}

void save_its_header(disko_t *fp, song_sample_t *smp)
{
	struct it_sample its = {0};

	its.id = bswapLE32(0x53504D49); // IMPS
	strncpy((char *) its.filename, smp->filename, 12);
	its.gvl = smp->global_volume;
	if (smp->data && smp->length)
		its.flags |= 1;
	if (smp->flags & CHN_16BIT)
		its.flags |= 2;
	if (smp->flags & CHN_STEREO)
		its.flags |= 4;
	if (smp->flags & CHN_LOOP)
		its.flags |= 16;
	if (smp->flags & CHN_SUSTAINLOOP)
		its.flags |= 32;
	if (smp->flags & CHN_PINGPONGLOOP)
		its.flags |= 64;
	if (smp->flags & CHN_PINGPONGSUSTAIN)
		its.flags |= 128;
	its.vol = smp->volume / 4;
	strncpy((char *) its.name, smp->name, 25);
	its.name[25] = 0;
	its.cvt = 1;                    // signed samples
	its.dfp = smp->panning / 4;
	if (smp->flags & CHN_PANNING)
		its.dfp |= 0x80;
	its.length = bswapLE32(smp->length);
	its.loopbegin = bswapLE32(smp->loop_start);
	its.loopend = bswapLE32(smp->loop_end);
	its.c5speed = bswapLE32(smp->c5speed);
	its.susloopbegin = bswapLE32(smp->sustain_start);
	its.susloopend = bswapLE32(smp->sustain_end);
	//its.samplepointer = 42; - this will be filled in later
	its.vis = smp->vib_speed;
	its.vir = smp->vib_rate;
	its.vid = smp->vib_depth;
	switch (smp->vib_type) {
		case VIB_RANDOM:    its.vit = 3; break;
		case VIB_SQUARE:    its.vit = 2; break;
		case VIB_RAMP_DOWN: its.vit = 1; break;
		default:
		case VIB_SINE:      its.vit = 0; break;
	}

#define WRITE_VALUE(name) do { disko_write(fp, &its.name, sizeof(its.name)); } while (0)

	WRITE_VALUE(id);
	WRITE_VALUE(filename);
	WRITE_VALUE(zero);
	WRITE_VALUE(gvl);
	WRITE_VALUE(flags);
	WRITE_VALUE(vol);
	WRITE_VALUE(name);
	WRITE_VALUE(cvt);
	WRITE_VALUE(dfp);
	WRITE_VALUE(length);
	WRITE_VALUE(loopbegin);
	WRITE_VALUE(loopend);
	WRITE_VALUE(c5speed);
	WRITE_VALUE(susloopbegin);
	WRITE_VALUE(susloopend);
	WRITE_VALUE(samplepointer);
	WRITE_VALUE(vis);
	WRITE_VALUE(vid);
	WRITE_VALUE(vir);
	WRITE_VALUE(vit);

#undef WRITE_VALUE
}

int fmt_its_save_sample(disko_t *fp, song_sample_t *smp)
{
	if (smp->flags & CHN_ADLIB)
		return SAVE_UNSUPPORTED;

	save_its_header(fp, smp);
	csf_write_sample(fp, smp, SF_LE | SF_PCMS
			| ((smp->flags & CHN_16BIT) ? SF_16 : SF_8)
			| ((smp->flags & CHN_STEREO) ? SF_SS : SF_M),
			UINT32_MAX);

	/* Write the sample pointer. In an ITS file, the sample data is right after the header,
	 * so its position in the file will be the same as the size of the header. */
	unsigned int tmp = bswapLE32(sizeof(struct it_sample));
	disko_seek(fp, 0x48, SEEK_SET);
	disko_write(fp, &tmp, 4);

	return SAVE_SUCCESS;
}

