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
#include "slurp.h"
#include "fmt.h"

#include "sndfile.h"

/* --------------------------------------------------------------------- */

int fmt_stm_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
	char id[8];
	int i;

	/* data[29] is the type: 1 = song, 2 = module (with samples) */
	if (!(length > 28 && (data[28] == 0x1a || data[28] == 0x02) && (data[29] == 1 || data[29] == 2)
		&& data[30] == 2))
		return 0;

	memcpy(id, data + 20, 8);
	for (i = 0; i < 8; i++)
		if (id[i] < 0x20 || id[i] > 0x7E)
			return 0;

	/* I used to check whether it was a 'song' or 'module' and set the description
	accordingly, but it's fairly pointless information :) */
	file->description = "Scream Tracker 2";
	/*file->extension = str_dup("stm");*/
	file->type = TYPE_MODULE_MOD;
	file->title = strn_dup((const char *)data, 20);
	return 1;
}

/* --------------------------------------------------------------------- */

#pragma pack(push, 1)
struct stm_sample {
	char name[12];
	uint8_t zero;
	uint8_t inst_disk; // lol disks
	uint16_t pcmpara; // in the official documentation, this is misleadingly labelled reserved...
	uint16_t length, loop_start, loop_end;
	uint8_t volume;
	uint8_t reserved2;
	uint16_t c5speed;
	uint32_t morejunk;
	uint16_t paragraphs; // what?
};
#pragma pack(pop)


/* ST2 says at startup:
"Remark: the user ID is encoded in over ten places around the file!"
I wonder if this is interesting at all. */
static void load_stm_pattern(song_note_t *note, slurp_t *fp)
{
	int row, chan;
	uint8_t v[4];

	for (row = 0; row < 64; row++, note += 64 - 4) {
		for (chan = 0; chan < 4; chan++) {
			song_note_t* chan_note = note + chan;
			slurp_read(fp, v, 4);

			// mostly copied from modplug...
			if (v[0] < 251)
				chan_note->note = (v[0] >> 4) * 12 + (v[0] & 0xf) + 37;
			chan_note->instrument = v[1] >> 3;
			if (chan_note->instrument > 31)
				chan_note->instrument = 0; // oops never mind, that was crap
			chan_note->volparam = (v[1] & 0x7) + ((v[2] & 0xf0) >> 1);
			if (chan_note->volparam <= 64)
				chan_note->voleffect = VOLFX_VOLUME;
			else
				chan_note->volparam = 0;
			chan_note->param = v[3]; // easy!

			chan_note->effect = stm_effects[v[2] & 0x0f];
			handle_stm_effects(chan_note);
		}

		for (chan = 0; chan < 4; chan++) {
			song_note_t* chan_note = note + chan;
			if (chan_note->effect == FX_SPEED) {
				uint32_t tempo = chan_note->param;
				chan_note->param >>= 4;
				handle_stm_tempo_pattern(note, tempo);
			}
		}

		note += chan;
	}
}

int fmt_stm_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	char id[8];
	uint8_t tmp[4];
	int npat, n;
	uint16_t para_sdata[MAX_SAMPLES] = { 0 };

	slurp_seek(fp, 20, SEEK_SET);
	slurp_read(fp, id, 8);
	slurp_read(fp, tmp, 4);

	if (!(
		// this byte is *usually* guaranteed to be 0x1a,
		// however putup10.stm and putup11.stm are outliers
		// for some reason?...
		(tmp[0] == 0x1a || tmp[0] == 0x02)
		// from the doc:
		//      1 - song (contains no samples)
		//      2 - module (contains samples)
		// I'm not going to care about "songs".
		&& tmp[1] == 2
		// do version 1 STM's even exist?...
		&& tmp[2] == 2
	)) {
		return LOAD_UNSUPPORTED;
	}
	// check the file tag for printable ASCII
	for (n = 0; n < 8; n++)
		if (id[n] < 0x20 || id[n] > 0x7E)
			return LOAD_FORMAT_ERROR;

	// and the next two bytes are the tracker version.
	sprintf(song->tracker_id, "Scream Tracker %d.%02d", tmp[2], tmp[3]);

	slurp_seek(fp, 0, SEEK_SET);
	slurp_read(fp, song->title, 20);
	song->title[20] = '\0';
	slurp_seek(fp, 12, SEEK_CUR); // skip the tag and stuff

	size_t tempo = slurp_getc(fp);

	if (tmp[3] < 21) {
		tempo = ((tempo / 10) << 4) + tempo % 10;
	}

	song->initial_speed = (tempo >> 4) ? (tempo >> 4) : 1;
	song->initial_tempo = convert_stm_tempo_to_bpm(tempo);

	npat = slurp_getc(fp);
	song->initial_global_volume = 2 * slurp_getc(fp);
	slurp_seek(fp, 13, SEEK_CUR); // junk

	if (npat > 64)
		return LOAD_FORMAT_ERROR;

	for (n = 1; n <= 31; n++) {
		struct stm_sample stmsmp;
		uint16_t blen;
		song_sample_t *sample = song->samples + n;

		slurp_read(fp, &stmsmp, sizeof(stmsmp));

		for (int i = 0; i < 12; i++) {
			if ((uint8_t)stmsmp.name[i] == 0xFF)
				stmsmp.name[i] = 0x20;
		}
		// the strncpy here is intentional -- ST2 doesn't show the '3' after the \0 bytes in the first
		// sample of pm_fract.stm, for example
		strncpy(sample->filename, stmsmp.name, 12);
		memcpy(sample->name, sample->filename, 12);
		blen = sample->length = bswapLE16(stmsmp.length);
		sample->loop_start = bswapLE16(stmsmp.loop_start);
		sample->loop_end = bswapLE16(stmsmp.loop_end);
		sample->c5speed = bswapLE16(stmsmp.c5speed);
		sample->volume = stmsmp.volume * 4; //mphack
		if (sample->loop_start < blen
			&& sample->loop_end != 0xffff
			&& sample->loop_start < sample->loop_end) {
			sample->flags |= CHN_LOOP;
			sample->loop_end = CLAMP(sample->loop_end, sample->loop_start, blen);
		}
		para_sdata[n] = bswapLE16(stmsmp.pcmpara);
	}

	size_t orderlist_size = tmp[3] ? 128 : 64;

	slurp_read(fp, song->orderlist, orderlist_size);
	for (n = 0; n < orderlist_size; n++) {
		if (song->orderlist[n] >= 64)
			song->orderlist[n] = ORDER_LAST;
	}

	if (lflags & LOAD_NOPATTERNS) {
		slurp_seek(fp, npat * 64 * 4 * 4, SEEK_CUR);
	} else {
		for (n = 0; n < npat; n++) {
			song->patterns[n] = csf_allocate_pattern(64);
			song->pattern_size[n] = song->pattern_alloc_size[n] = 64;
			load_stm_pattern(song->patterns[n], fp);
		}
	}

	if (!(lflags & LOAD_NOSAMPLES)) {
		for (n = 1; n <= 31; n++) {
			song_sample_t *sample = song->samples + n;

			if (sample->length < 3) {
				// Garbage?
				sample->length = 0;
			} else {
				slurp_seek(fp, para_sdata[n] << 4, SEEK_SET);
				csf_read_sample(sample, SF_LE | SF_PCMS | SF_8 | SF_M,
					(const char *) (fp->data + fp->pos), sample->length);
			}
		}
	}

	for (n = 0; n < 4; n++)
		song->channels[n].panning = ((n & 1) ? 64 : 0) * 4; //mphack
	for (; n < 64; n++)
		song->channels[n].flags |= CHN_MUTE;
	song->pan_separation = 64;
	song->flags = SONG_ITOLDEFFECTS | SONG_COMPATGXX | SONG_NOSTEREO;

	return LOAD_SUCCESS;
}

