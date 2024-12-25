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
#include "it.h" /* for get_effect_char */
#include "log.h"
#include "mem.h"

#include "player/sndfile.h"

#include <math.h> /* for pow */

/* --------------------------------------------------------------------- */

int fmt_ult_read_info(dmoz_file_t *file, slurp_t *fp)
{
	unsigned char magic[14], title[32];

	if (slurp_read(fp, magic, sizeof(magic)) != sizeof(magic)
		|| memcmp(magic, "MAS_UTrack_V00", sizeof(magic)))
		return 0;

	slurp_seek(fp, 15, SEEK_SET);
	if (slurp_read(fp, title, sizeof(title)) != sizeof(title))
		return 0;

	file->description = "UltraTracker Module";
	file->type = TYPE_MODULE_S3M;
	/*file->extension = str_dup("ult");*/
	file->title = strn_dup((const char *)title, sizeof(title));
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

enum {
	ULT_16BIT = 4,
	ULT_LOOP  = 8,
	ULT_PINGPONGLOOP = 16,
};

struct ult_sample {
	char name[32];
	char filename[12];
	uint32_t loop_start;
	uint32_t loop_end;
	uint32_t size_start;
	uint32_t size_end;
	uint8_t volume; // 0-255, apparently prior to 1.4 this was logarithmic?
	uint8_t flags; // above
	uint16_t speed; // only exists for 1.4+
	int16_t finetune;
};

static int read_sample_ult(struct ult_sample *smp, slurp_t *fp, uint8_t ver)
{
#define READ_VALUE(name) \
	if (slurp_read(fp, &smp->name, sizeof(smp->name)) != sizeof(smp->name)) return 0

	READ_VALUE(name);
	READ_VALUE(filename);
	READ_VALUE(loop_start);
	READ_VALUE(loop_end);
	READ_VALUE(size_start);
	READ_VALUE(size_end);
	READ_VALUE(volume);
	READ_VALUE(flags);

	// annoying: v4 added a field before the end of the struct
	if (ver >= 4) {
		READ_VALUE(speed);
		smp->speed = bswapLE16(smp->speed);
	} else {
		smp->speed = 8363;
	}

	READ_VALUE(finetune);

#undef READ_VALUE

	/* now byteswap */
	smp->finetune = bswapLE16(smp->finetune);
	smp->loop_start = bswapLE32(smp->loop_start);
	smp->loop_end = bswapLE32(smp->loop_end);
	smp->size_start = bswapLE32(smp->size_start);
	smp->size_end = bswapLE32(smp->size_end);

	return 1;
}


/* Unhandled effects:
5x1 - do not loop sample (x is unused)
5x2 - play sample backwards
5xC - end loop and finish sample
9xx - set sample offset to xx * 1024
    with 9yy: set sample offset to xxyy * 4
E0x - set vibrato strength (2 is normal)
F00 - reset speed/tempo to 6/125

Apparently 3xx will CONTINUE to slide until it reaches its destination, or
until a 300 effect is encountered. I'm not attempting to handle this (yet).

The logarithmic volume scale used in older format versions here, or pretty
much anywhere for that matter. I don't even think Ultra Tracker tries to
convert them. */

static const uint8_t ult_efftrans[] = {
	FX_ARPEGGIO,
	FX_PORTAMENTOUP,
	FX_PORTAMENTODOWN,
	FX_TONEPORTAMENTO,
	FX_VIBRATO,
	FX_NONE,
	FX_NONE,
	FX_TREMOLO,
	FX_NONE,
	FX_OFFSET,
	FX_VOLUMESLIDE,
	FX_PANNING,
	FX_VOLUME,
	FX_PATTERNBREAK,
	FX_NONE, // extended effects, processed separately
	FX_SPEED,
};

static void translate_fx(uint8_t *pe, uint8_t *pp)
{
	uint8_t e = *pe & 0xf;
	uint8_t p = *pp;

	*pe = ult_efftrans[e];

	switch (e) {
	case 0:
		if (!p)
			*pe = FX_NONE;
		break;
	case 3:
		// 300 apparently stops sliding, which is totally weird
		if (!p)
			p = 1; // close enough?
		break;
	case 0xa:
		// blah, this sucks
		if (p & 0xf0)
			p &= 0xf0;
		break;
	case 0xb:
		// mikmod does this wrong, resulting in values 0-225 instead of 0-255
		p = (p & 0xf) * 0x11;
		break;
	case 0xc: // volume
		p >>= 2;
		break;
	case 0xd: // pattern break
		p = 10 * (p >> 4) + (p & 0xf);
		break;
	case 0xe: // special
		switch (p >> 4) {
		case 1:
			*pe = FX_PORTAMENTOUP;
			p = 0xf0 | (p & 0xf);
			break;
		case 2:
			*pe = FX_PORTAMENTODOWN;
			p = 0xf0 | (p & 0xf);
			break;
		case 8:
			*pe = FX_SPECIAL;
			p = 0x60 | (p & 0xf);
			break;
		case 9:
			*pe = FX_RETRIG;
			p &= 0xf;
			break;
		case 0xa:
			*pe = FX_VOLUMESLIDE;
			p = ((p & 0xf) << 4) | 0xf;
			break;
		case 0xb:
			*pe = FX_VOLUMESLIDE;
			p = 0xf0 | (p & 0xf);
			break;
		case 0xc: case 0xd:
			*pe = FX_SPECIAL;
			break;
		}
		break;
	case 0xf:
		if (p > 0x2f)
			*pe = FX_TEMPO;
		break;
	}

	*pp = p;
}

static int read_ult_event(slurp_t *fp, song_note_t *note, int *lostfx)
{
	uint8_t b, repeat = 1;
	uint32_t off;
	int n;

	b = slurp_getc(fp);
	if (b == 0xfc) {
		repeat = slurp_getc(fp);
		b = slurp_getc(fp);
	}
	note->note = (b > 0 && b < 61) ? b + 36 : NOTE_NONE;
	note->instrument = slurp_getc(fp);
	b = slurp_getc(fp);
	note->voleffect = b & 0xf;
	note->effect = b >> 4;
	note->volparam = slurp_getc(fp);
	note->param = slurp_getc(fp);
	translate_fx(&note->voleffect, &note->volparam);
	translate_fx(&note->effect, &note->param);

	// sample offset -- this is even more special than digitrakker's
	if (note->voleffect == FX_OFFSET && note->effect == FX_OFFSET) {
		off = ((note->volparam << 8) | note->param) >> 6;
		note->voleffect = FX_NONE;
		note->param = MIN(off, 0xff);
	} else if (note->voleffect == FX_OFFSET) {
		off = note->volparam * 4;
		note->volparam = MIN(off, 0xff);
	} else if (note->effect == FX_OFFSET) {
		off = note->param * 4;
		note->param = MIN(off, 0xff);
	} else if (note->voleffect == note->effect) {
		/* don't try to figure out how ultratracker does this, it's quite random */
		note->effect = FX_NONE;
	}
	if (note->effect == FX_VOLUME || (note->effect == FX_NONE && note->voleffect != FX_VOLUME)) {
		swap_effects(note);
	}

	// Do that dance.
	// Maybe I should quit rewriting this everywhere and make a generic version :P
	for (n = 0; n < 4; n++) {
		if (convert_voleffect_of(note, n >> 1)) {
			n = 5;
			break;
		}
		swap_effects(note);
	}
	if (n < 5) {
		if (effect_weight[note->voleffect] > effect_weight[note->effect])
			swap_effects(note);
		(*lostfx)++;
		//log_appendf(4, "Effect dropped: %c%02X < %c%02X", get_effect_char(note->voleffect),
		//        note->volparam, get_effect_char(note->effect), note->param);
		note->voleffect = 0;
	}
	if (!note->voleffect)
		note->volparam = 0;
	if (!note->effect)
		note->param = 0;
	return repeat;
}

/* --------------------------------------------------------------------------------------------------------- */

int fmt_ult_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	char buf[34];
	uint8_t ver;
	int nmsg, nsmp, nchn, npat;
	int n, chn, pat, row;
	int lostfx = 0, gxx = 0;
	struct ult_sample usmp;
	song_sample_t *smp;
	const char *verstr[] = {"<1.4", "1.4", "1.5", "1.6"};

	slurp_read(fp, buf, 14);
	if (memcmp(buf, "MAS_UTrack_V00", 14) != 0)
		return LOAD_UNSUPPORTED;
	ver = slurp_getc(fp);
	if (ver < '1' || ver > '4')
		return LOAD_FORMAT_ERROR;
	ver -= '0';

	slurp_read(fp, buf, 32);
	buf[25] = '\0';
	strcpy(song->title, buf);

	sprintf(song->tracker_id, "Ultra Tracker %s", verstr[ver - 1]);
	song->flags |= SONG_COMPATGXX | SONG_ITOLDEFFECTS;

	nmsg = slurp_getc(fp);
	read_lined_message(song->message, fp, nmsg * 32, 32);

	nsmp = slurp_getc(fp);
	for (n = 0, smp = song->samples + 1; n < nsmp; n++, smp++) {
		if (!read_sample_ult(&usmp, fp, ver))
			return LOAD_FORMAT_ERROR;

		strncpy(smp->name, usmp.name, 25);
		smp->name[25] = '\0';
		strncpy(smp->filename, usmp.filename, 12);
		smp->filename[12] = '\0';
		if (usmp.size_end <= usmp.size_start)
			continue;
		smp->length = usmp.size_end - usmp.size_start;
		smp->loop_start = usmp.loop_start;
		smp->loop_end = MIN(usmp.loop_end, smp->length);
		smp->volume = usmp.volume; //mphack - should be 0-64 not 0-256
		smp->global_volume = 64;

		/* mikmod does some weird integer math here, but it didn't really work for me */
		smp->c5speed = usmp.speed;
		if (usmp.finetune)
			smp->c5speed *= pow(2, (usmp.finetune / (12.0 * 32768)));

		if (usmp.flags & ULT_LOOP)
			smp->flags |= CHN_LOOP;
		if (usmp.flags & ULT_PINGPONGLOOP)
			smp->flags |= CHN_PINGPONGLOOP;
		if (usmp.flags & ULT_16BIT) {
			smp->flags |= CHN_16BIT;
			smp->loop_start >>= 1;
			smp->loop_end >>= 1;
		}
	}

	// ult just so happens to use 255 for its end mark, so there's no need to fiddle with this
	slurp_read(fp, song->orderlist, 256);

	nchn = slurp_getc(fp) + 1;
	npat = slurp_getc(fp) + 1;

	if (nchn > 32 || npat > MAX_PATTERNS)
		return LOAD_FORMAT_ERROR;

	if (ver >= 3) {
		for (n = 0; n < nchn; n++)
			song->channels[n].panning = ((slurp_getc(fp) & 0xf) << 2) + 2;
	} else {
		for (n = 0; n < nchn; n++)
			song->channels[n].panning = (n & 1) ? 48 : 16;
	}
	for (; n < 64; n++) {
		song->channels[n].panning = 32;
		song->channels[n].flags = CHN_MUTE;
	}
	//mphack - fix the pannings
	for (n = 0; n < 64; n++)
		song->channels[n].panning *= 4;

	if ((lflags & (LOAD_NOSAMPLES | LOAD_NOPATTERNS)) == (LOAD_NOSAMPLES | LOAD_NOPATTERNS))
		return LOAD_SUCCESS;

	for (pat = 0; pat < npat; pat++) {
		song->pattern_size[pat] = song->pattern_alloc_size[pat] = 64;
		song->patterns[pat] = csf_allocate_pattern(64);
	}
	for (chn = 0; chn < nchn; chn++) {
		song_note_t evnote;
		song_note_t *note;
		int repeat;

		for (pat = 0; pat < npat; pat++) {
			note = song->patterns[pat] + chn;
			row = 0;
			while (row < 64) {
				repeat = read_ult_event(fp, &evnote, &lostfx);
				if (evnote.effect == FX_TONEPORTAMENTO
				    || evnote.voleffect == VOLFX_TONEPORTAMENTO) {
					gxx |= 1;
				}
				if (repeat + row > 64)
					repeat = 64 - row;
				while (repeat--) {
					*note = evnote;
					note += 64;
					row++;
				}
			}
		}
	}
	if (gxx)
		log_appendf(4, " Warning: Gxx effects may not be suitably imported");
	if (lostfx)
		log_appendf(4, " Warning: %d effect%s dropped", lostfx, lostfx == 1 ? "" : "s");

	if (!(lflags & LOAD_NOSAMPLES)) {
		for (n = 0, smp = song->samples + 1; n < nsmp; n++, smp++) {
			csf_read_sample(smp,
				SF_LE | SF_M | SF_PCMS | ((smp->flags & CHN_16BIT) ? SF_16 : SF_8), fp);
		}
	}
	return LOAD_SUCCESS;
}

