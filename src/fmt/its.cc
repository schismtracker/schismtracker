/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
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

bool fmt_its_read_info(dmoz_file_t *file, const byte *data, size_t length)
{
	if (!(length > 80 && memcmp(data, "IMPS", 4) == 0))
		return false;

	file->description = "Impulse Tracker Sample";
	/*file->extension = strdup("its");*/
	file->title = (char *) calloc(26, sizeof(char)); /* arghbbq, c++ cast-cakes */
	memcpy(file->title, data + 20, 25);
	file->title[25] = 0;
	file->type = TYPE_SAMPLE_EXTD;
	return true;
}

bool fmt_its_load_sample(const byte *data, size_t length, song_sample *smp, char *title)
{
	ITSAMPLESTRUCT *its = (ITSAMPLESTRUCT *)data;
	UINT format = RS_PCM8U;
	
	if (length < 80 || strncmp((const char *) data, "IMPS", 4) != 0)
		return false;
	if (its->length + 80 > length)
		return false;
	if ((its->flags & 1) == 0) {
		// sample associated with header
		return false;
	}
	if ((its->flags & 4) != 0) {
		// stereo
		printf("TODO: stereo sample\n");
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
	smp->length = its->length;
	smp->loop_start = its->loopbegin;
	smp->loop_end = its->loopend;
	smp->speed = its->C5Speed;
	smp->sustain_start = its->susloopbegin;
	smp->sustain_end = its->susloopend;
	
	int vibs[] = {VIB_SINE, VIB_RAMP_DOWN, VIB_SQUARE, VIB_RANDOM};
	smp->vib_type = vibs[its->vit & 3];
	smp->vib_rate = (its->vir + 3) / 4;
	smp->vib_depth = its->vid;
	smp->vib_speed = its->vis;
	
	// sanity checks
	// (I should probably have more of these in general)
	if (smp->loop_start > smp->length)
		smp->loop_start = smp->length,    smp->flags &= ~(SAMP_LOOP | SAMP_LOOP_PINGPONG);
	if (smp->loop_end > smp->length)
		smp->loop_end = smp->length,      smp->flags &= ~(SAMP_LOOP | SAMP_LOOP_PINGPONG);
	if (smp->sustain_start > smp->length)
		smp->sustain_start = smp->length, smp->flags &= ~(SAMP_SUSLOOP | SAMP_SUSLOOP_PINGPONG);
	if (smp->sustain_end > smp->length)
		smp->sustain_end = smp->length,   smp->flags &= ~(SAMP_SUSLOOP | SAMP_SUSLOOP_PINGPONG);
	
	// dumb casts :P
	return mp->ReadSample((MODINSTRUMENT *) smp, format,
			      (LPCSTR) (data + its->samplepointer),
			      (DWORD) (length - its->samplepointer));
}

void save_its_header(FILE *fp, song_sample *smp, char *title)
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
	its.vis = (smp->vib_rate < 64) ? (smp->vib_rate * 4) : 255;
	its.vid = smp->vib_depth;
	its.vir = smp->vib_speed;
	//its.vit = smp->vib_type; <- Modplug uses different numbers for this. :/
	its.vit = 0;
	
	fwrite(&its, sizeof(its), 1, fp);
}

bool fmt_its_save_sample(FILE *fp, song_sample *smp, char *title)
{
	save_its_header(fp, smp, title);
	save_sample_data_LE(fp, smp);
	
	/* Write the sample pointer. In an ITS file, the sample data is right after the header,
	so its position in the file will be the same as the size of the header. */
	unsigned int tmp = bswapLE32(sizeof(ITSAMPLESTRUCT));
	fseek(fp, 0x48, SEEK_SET);
	fwrite(&tmp, 4, 1, fp);
	
	return true;
}
