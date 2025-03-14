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
#include "slurp.h"
#include "fmt.h"
#include "log.h"
#include "mem.h"

#include "player/sndfile.h"

/* --------------------------------------------------------------------------------------------------------- */

/* None of the sfx files on Modland are of the 31-instrument type that xmp recognizes.
However, there are a number of 31-instrument files with a different tag, under "SoundFX 2". */

static struct sfxfmt {
	size_t tagpos;
	const char tag[4];
	int nsmp;
	int dunno;
	const char *id;
} sfxfmts[] = {
	{124, "SO31", 31, 4, "SoundFX 2"},
	{124, "SONG", 31, 0, "SoundFX 2 (?)"},
	{ 60, "SONG", 15, 0, "SoundFX"},
	{  0, ""    ,  0, 0, NULL},
};


int fmt_sfx_read_info(dmoz_file_t *file, slurp_t *fp)
{
	int n;
	unsigned char tag[4];

	for (n = 0; sfxfmts[n].nsmp; n++) {
		slurp_seek(fp, sfxfmts[n].tagpos, SEEK_SET);
		if (slurp_read(fp, tag, sizeof(tag)) == sizeof(tag)
			&& !memcmp(tag, sfxfmts[n].tag, 4)) {
			file->description = sfxfmts[n].id;
			/*file->extension = str_dup("sfx");*/
			file->title = str_dup(""); // whatever
			file->type = TYPE_MODULE_MOD;
			return 1;
		}
	}
	return 0;
}

/* --------------------------------------------------------------------------------------------------------- */

/* Loader taken mostly from XMP.

Why did I write a loader for such an obscure format? That is, besides the fact that neither Modplug nor
Mikmod support SFX (and for good reason; it's a particularly dumb format) */

int fmt_sfx_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	uint8_t tag[4];
	int nord, npat, pat, chan, restart, nsmp = 0, n;
	uint32_t smpsize[31];
	uint16_t tmp;
	song_note_t *note;
	song_sample_t *sample;
	unsigned int effwarn = 0;
	struct sfxfmt *fmt = sfxfmts;

	do {
		slurp_seek(fp, fmt->tagpos, SEEK_SET);
		slurp_read(fp, tag, 4);
		if (memcmp(tag, fmt->tag, 4) == 0) {
			nsmp = fmt->nsmp;
			break;
		}
		fmt++;
	} while (fmt->nsmp);

	if (!nsmp)
		return LOAD_UNSUPPORTED;


	slurp_rewind(fp);
	slurp_read(fp, smpsize, 4 * nsmp);
	slurp_seek(fp, 4, SEEK_CUR); /* the tag again */
	slurp_read(fp, &tmp, 2);
	if (!tmp)
		return LOAD_UNSUPPORTED; // erf
	tmp = 14565 * 122 / bswapBE16(tmp);
	song->initial_tempo = CLAMP(tmp, 31, 255);

	slurp_seek(fp, 14, SEEK_CUR); /* unknown bytes (reserved?) - see below */

	if (lflags & LOAD_NOSAMPLES) {
		slurp_seek(fp, 30 * nsmp, SEEK_CUR);
	} else {
		for (n = 0, sample = song->samples + 1; n < nsmp; n++, sample++) {
			slurp_read(fp, sample->name, 22);
			sample->name[22] = 0;
			slurp_read(fp, &tmp, 2); /* seems to be half the sample size, minus two bytes? */
			tmp = bswapBE16(tmp);
			sample->length = bswapBE32(smpsize[n]);

			song->samples[n].c5speed = MOD_FINETUNE(slurp_getc(fp)); // ?
			sample->volume = slurp_getc(fp);
			if (sample->volume > 64)
				sample->volume = 64;
			sample->volume *= 4; //mphack
			sample->global_volume = 64;
			slurp_read(fp, &tmp, 2);
			sample->loop_start = bswapBE16(tmp);
			slurp_read(fp, &tmp, 2);
			tmp = bswapBE16(tmp) * 2; /* loop length */
			if (tmp > 2) {
				sample->loop_end = sample->loop_start + tmp;
				sample->flags |= CHN_LOOP;
			} else {
				sample->loop_start = sample->loop_end = 0;
			}
		}
	}

	/* pattern/order stuff */
	nord = slurp_getc(fp);
	nord = MIN(nord, 127);
	restart = slurp_getc(fp);
	slurp_read(fp, song->orderlist, nord);
	slurp_seek(fp, 128 - nord, SEEK_CUR);
	npat = 0;
	for (n = 0; n < nord; n++) {
		if (song->orderlist[n] > npat)
			npat = song->orderlist[n];
	}
	npat++;

	/* Not sure what this is about, but skipping a few bytes here seems to make SO31's load right.
	(they all seem to have zero here) */
	slurp_seek(fp, fmt->dunno, SEEK_CUR);

	if (lflags & LOAD_NOPATTERNS) {
		slurp_seek(fp, npat * 1024, SEEK_CUR);
	} else {
		for (pat = 0; pat < npat; pat++) {
			note = song->patterns[pat] = csf_allocate_pattern(64);
			song->pattern_size[pat] = song->pattern_alloc_size[pat] = 64;
			for (n = 0; n < 64; n++, note += 60) {
				for (chan = 0; chan < 4; chan++, note++) {
					uint8_t p[4];
					slurp_read(fp, p, 4);
					mod_import_note(p, note);
					/* Note events starting with FF all seem to be special in some way:
						bytes   apparent use    example file on modland
						-----   ------------    -----------------------
						FF FE   note cut        1st intro.sfx
						FF FD   unknown!        another world (intro).sfx
						FF FC   pattern break   orbit wanderer.sfx2 */
					if (p[0] == 0xff) {
						switch (p[1]) {
						case 0xfc:
							note->note = NOTE_NONE;
							note->instrument = 0;
							// stuff a C00 in channel 5
							note[4 - chan].effect = FX_PATTERNBREAK;
							break;
						case 0xfe:
							note->note = NOTE_CUT;
							note->instrument = 0;
							break;
						}
					}
					switch (note->effect) {
					case 0:
						break;
					case 1: /* arpeggio */
						note->effect = FX_ARPEGGIO;
						break;
					case 2: /* pitch bend */
						if (note->param >> 4) {
							note->effect = FX_PORTAMENTODOWN;
							note->param >>= 4;
						} else if (note->param & 0xf) {
							note->effect = FX_PORTAMENTOUP;
							note->param &= 0xf;
						} else {
							note->effect = 0;
						}
						break;
					case 5: /* volume up */
						note->effect = FX_VOLUMESLIDE;
						note->param = (note->param & 0xf) << 4;
						break;
					case 6: /* set volume */
						if (note->param > 64)
							note->param = 64;
						note->voleffect = VOLFX_VOLUME;
						note->volparam = 64 - note->param;
						note->effect = 0;
						note->param = 0;
						break;
					case 3: /* LED on (wtf!) */
					case 4: /* LED off (ditto) */
					case 7: /* set step up */
					case 8: /* set step down */
					default:
						effwarn |= (1 << note->effect);
						note->effect = FX_UNIMPLEMENTED;
						break;
					}
				}
			}
		}
		for (n = 0; n < 16; n++) {
			if (effwarn & (1 << n))
				log_appendf(4, " Warning: Unimplemented effect %Xxx", n);
		}

		if (restart < npat)
			csf_insert_restart_pos(song, restart);
	}

	/* sample data */
	if (!(lflags & LOAD_NOSAMPLES)) {
		for (n = 0, sample = song->samples + 1; n < fmt->nsmp; n++, sample++) {
			if (sample->length <= 2)
				continue;
			csf_read_sample(sample, SF_8 | SF_LE | SF_PCMS | SF_M, fp);
		}
	}

	/* more header info */
	song->flags = SONG_ITOLDEFFECTS | SONG_COMPATGXX;
	for (n = 0; n < 4; n++)
		song->channels[n].panning = PROTRACKER_PANNING(n); /* ??? */
	for (; n < MAX_CHANNELS; n++)
		song->channels[n].flags = CHN_MUTE;

	strcpy(song->tracker_id, fmt->id);
	song->pan_separation = 64;

//      if (ferror(fp)) {
//              return LOAD_FILE_ERROR;
//      }

	/* done! */
	return LOAD_SUCCESS;
}


/*
most of modland's sfx files have all zeroes for those 14 "unknown" bytes, with the following exceptions:

64 00 00 00 00 00 00 00 00 00 00 00 00 00  d............. - unknown/antitrax.sfx
74 63 68 33 00 00 00 00 00 00 00 00 00 00  tch3.......... - unknown/axel f.sfx
61 6c 6b 00 00 00 00 00 00 00 00 00 00 00  alk........... Andreas Hommel/cyberblast-intro.sfx
21 00 00 00 00 00 00 00 00 00 00 00 00 00  !............. - unknown/dugger.sfx
00 00 00 00 00 0d 00 00 00 00 00 00 00 00  .............. Jean Baudlot/future wars - time travellers - dugger (title).sfx
00 00 00 00 00 00 00 00 0d 00 00 00 00 00  .............. Jean Baudlot/future wars - time travellers - escalator.sfx
6d 65 31 34 00 00 00 00 00 00 00 00 00 00  me14.......... - unknown/melodious.sfx
0d 0d 0d 53 46 58 56 31 2e 38 00 00 00 00  ...SFXV1.8.... AM-FM/sunday morning.sfx
61 6c 6b 00 00 00 00 00 00 00 00 00 00 00  alk........... - unknown/sunday morning.sfx
6f 67 00 00 00 00 00 00 00 00 00 00 00 00  og............ Philip Jespersen/supaplex.sfx
6e 74 20 73 6f 6e 67 00 00 00 00 00 00 00  nt song....... - unknown/sweety.sfx
61 6c 6b 00 00 00 00 00 00 00 00 00 00 00  alk........... - unknown/thrust.sfx
*/

