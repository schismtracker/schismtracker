/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://rigelseven.com/schism/
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
#include "fmt.h"

/* FIXME - shouldn't be using the Modplug headers here. The only reason I'm doing so is because Modplug
has the IT sample decompression code... */
#include "mplink.h"
#include "it_defs.h"

/* --------------------------------------------------------------------- */
int fmt_its_read_info(dmoz_file_t *file, const byte *data, size_t length)
{
	ITSAMPLESTRUCT *its;

	if (!(length > 80 && memcmp(data, "IMPS", 4) == 0))
		return false;

	its = (ITSAMPLESTRUCT *)data;
	file->smp_length = bswapLE32(its->length);
	file->smp_flags = 0;

	if (its->flags & 2) {
		file->smp_flags |= SAMP_16_BIT;
	}
	if (its->flags & 16) {
		file->smp_flags |= SAMP_LOOP;
		if (its->flags & 64)
			file->smp_flags |= SAMP_LOOP_PINGPONG;
	}
	if (its->flags & 32) {
		file->smp_flags |= SAMP_SUSLOOP;
		if (its->flags & 128)
			file->smp_flags |= SAMP_SUSLOOP_PINGPONG;
	}

	if (its->dfp & 128) file->smp_flags |= SAMP_PANNING;
	if (its->flags & 4) file->smp_flags |= SAMP_STEREO;

	file->smp_defvol = its->vol;
	file->smp_gblvol = its->gvl;
	file->smp_vibrato_speed = its->vis;
	file->smp_vibrato_depth = its->vid & 0x7f;
	file->smp_vibrato_rate = its->vir;

	file->smp_loop_start = bswapLE32(its->loopbegin);
	file->smp_loop_end = bswapLE32(its->loopend);
	file->smp_speed = bswapLE32(its->C5Speed);
	file->smp_sustain_start = bswapLE32(its->susloopbegin);
	file->smp_sustain_end = bswapLE32(its->susloopend);

	file->smp_filename = (char *)mem_alloc(13);
	memcpy(file->smp_filename, its->filename, 12);
	file->smp_filename[12] = 0;

	file->description = "Impulse Tracker Sample";
	file->title = (char *)mem_alloc(26);
	memcpy(file->title, data + 20, 25);
	file->title[25] = 0;
	file->type = TYPE_SAMPLE_EXTD;

	return true;
}

int load_its_sample(const byte *header, const byte *data, size_t length, song_sample *smp, char *title)
{
	ITSAMPLESTRUCT *its = (ITSAMPLESTRUCT *)header;
	UINT format = RS_PCM8U;
	UINT bp, bl;
	
	if (length < 80 || strncmp((const char *) header, "IMPS", 4) != 0)
		return false;
	/* alright, let's get started */
	smp->length = bswapLE32(its->length);
	if (smp->length + 80 > length)
		return false;
	if ((its->flags & 1) == 0) {
		// sample associated with header
		return false;
	}
	if (its->flags & 8) {
		// compressed
		format = (its->flags & 2) ? RS_IT21416 : RS_IT2148;
	} else {
		if (its->flags & 2) {
			// 16 bit
			format = (its->cvt & 1) ? RS_PCM16S : RS_PCM16U;
		} else {
			// 8 bit
			format = (its->cvt & 1) ? RS_PCM8S : RS_PCM8U;
		}
	}
	smp->global_volume = its->gvl;
	if (its->flags & 16) {
		smp->flags |= SAMP_LOOP;
		if (its->flags & 64)
			smp->flags |= SAMP_LOOP_PINGPONG;
	}
	if (its->flags & 32) {
		smp->flags |= SAMP_SUSLOOP;
		if (its->flags & 128)
			smp->flags |= SAMP_SUSLOOP_PINGPONG;
	}
	smp->volume = its->vol * 4;
	strncpy(title, (const char *) its->name, 25);
	smp->panning = (its->dfp & 127) * 4;
	if (its->dfp & 128)
		smp->flags |= SAMP_PANNING;
	if (its->flags & 4) {
		// stereo
		format |= RSF_STEREO;
		smp->flags |= SAMP_STEREO;
	}
	smp->loop_start = bswapLE32(its->loopbegin);
	smp->loop_end = bswapLE32(its->loopend);
	smp->speed = bswapLE32(its->C5Speed);
	smp->sustain_start = bswapLE32(its->susloopbegin);
	smp->sustain_end = bswapLE32(its->susloopend);
	
	int vibs[] = {VIB_SINE, VIB_RAMP_DOWN, VIB_SQUARE, VIB_RANDOM};
	smp->vib_type = vibs[its->vit & 3];
	smp->vib_rate = its->vir;
	smp->vib_depth = its->vid;
	smp->vib_speed = its->vis;
	
	// sanity checks
	// (I should probably have more of these in general)
	if (smp->loop_start > smp->length) {
		smp->loop_start = smp->length;
		smp->flags &= ~(SAMP_LOOP | SAMP_LOOP_PINGPONG);
	}
	if (smp->loop_end > smp->length) {
		smp->loop_end = smp->length;
		smp->flags &= ~(SAMP_LOOP | SAMP_LOOP_PINGPONG);
	}
	if (smp->sustain_start > smp->length) {
		smp->sustain_start = smp->length;
		smp->flags &= ~(SAMP_SUSLOOP | SAMP_SUSLOOP_PINGPONG);
	}
	if (smp->sustain_end > smp->length) {
		smp->sustain_end = smp->length;
		smp->flags &= ~(SAMP_SUSLOOP | SAMP_SUSLOOP_PINGPONG);
	}

	// use length correctly
	bp = bswapLE32(its->samplepointer);
	bl = smp->length;
	if (smp->flags & SAMP_STEREO) bl *= 2;
	if (smp->flags & SAMP_16_BIT) bl *= 2; /* 16bit */

	if (bl + bp > length) {
		/* wrong length */
		return false;
	}
	// dumb casts :P
	return mp->ReadSample((MODINSTRUMENT *) smp, format,
			(LPCSTR) (data + bp),
			(DWORD) (length - bp));
}

int fmt_its_load_sample(const byte *data, size_t length, song_sample *smp, char *title)
{
	return load_its_sample(data,data,length,smp,title);
}

void save_its_header(diskwriter_driver_t *fp, song_sample *smp, char *title)
{
	ITSAMPLESTRUCT its;
	
	its.id = bswapLE32(0x53504D49); // IMPS
	strncpy((char *) its.filename, (char *) smp->filename, 12);
	its.zero = 0;
	its.gvl = smp->global_volume;
	its.flags = 0; // uFlags
	if (smp->data && smp->length)
		its.flags |= 1;
	if (smp->flags & SAMP_16_BIT)
		its.flags |= 2;
	if (smp->flags & SAMP_STEREO)
		its.flags |= 4;
	if (smp->flags & SAMP_LOOP)
		its.flags |= 16;
	if (smp->flags & SAMP_SUSLOOP)
		its.flags |= 32;
	if (smp->flags & SAMP_LOOP_PINGPONG)
		its.flags |= 64;
	if (smp->flags & SAMP_SUSLOOP_PINGPONG)
		its.flags |= 128;
	its.vol = smp->volume / 4;
	strncpy((char *) its.name, title, 25);
	its.name[25] = 0;
	its.cvt = 1;			// signed samples
	its.dfp = smp->panning / 4;
	if (smp->flags & SAMP_PANNING)
		its.dfp |= 0x80;
	its.length = bswapLE32(smp->length);
	its.loopbegin = bswapLE32(smp->loop_start);
	its.loopend = bswapLE32(smp->loop_end);
	its.C5Speed = bswapLE32(smp->speed);
	its.susloopbegin = bswapLE32(smp->sustain_start);
	its.susloopend = bswapLE32(smp->sustain_end);
	//its.samplepointer = 42; - this will be filled in later
	its.vis = smp->vib_speed;
	its.vir = smp->vib_rate;
	its.vid = smp->vib_depth;
	//its.vit = smp->vib_type; <- Modplug uses different numbers for this. :/
	its.vit = 0;
	
	fp->o(fp, (const unsigned char *)&its, sizeof(its));
}

int fmt_its_save_sample(diskwriter_driver_t *fp, song_sample *smp, char *title)
{
	save_its_header(fp, smp, title);
	save_sample_data_LE(fp, smp, 1);
	
	/* Write the sample pointer. In an ITS file, the sample data is right after the header,
	so its position in the file will be the same as the size of the header. */
	unsigned int tmp = bswapLE32(sizeof(ITSAMPLESTRUCT));
	fp->l(fp, 0x48);
	fp->o(fp, (const unsigned char *)&tmp, 4);
	
	return true;
}
