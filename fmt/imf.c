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
#include "log.h"
#include "fmt.h"
#include "mem.h"

#include "player/sndfile.h"

/* --------------------------------------------------------------------- */

int fmt_imf_read_info(dmoz_file_t *file, slurp_t *fp)
{
	unsigned char magic[4], title[32];

	slurp_seek(fp, 60, SEEK_SET);
	if (slurp_read(fp, magic, sizeof(magic)) != sizeof(magic)
		|| memcmp(magic, "IM10", sizeof(magic)))
		return 0;

	slurp_seek(fp, 0, SEEK_SET);
	slurp_read(fp, title, sizeof(title));

	file->description = "Imago Orpheus";
	/*file->extension = str_dup("imf");*/
	file->title = strn_dup((const char *)title, sizeof(title));
	file->type = TYPE_MODULE_IT;
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

struct imf_channel {
	char name[12];          /* Channelname (ASCIIZ-String, max 11 chars) */
	uint8_t chorus;         /* Default chorus */
	uint8_t reverb;         /* Default reverb */
	uint8_t panning;        /* Pan positions 00-FF */
	uint8_t status;         /* Channel status: 0 = enabled, 1 = mute, 2 = disabled (ignore effects!) */
};

static int imf_read_channel(struct imf_channel *chn, slurp_t *fp)
{
#define READ_VALUE(name) \
	do { if (slurp_read(fp, &chn->name, sizeof(chn->name)) != sizeof(chn->name)) { return 0; } } while (0)

	READ_VALUE(name);
	READ_VALUE(chorus);
	READ_VALUE(reverb);
	READ_VALUE(panning);
	READ_VALUE(status);

#undef READ_VALUE

	return 1;
}

struct imf_header {
	char title[32];         /* Songname (ASCIIZ-String, max. 31 chars) */
	uint16_t ordnum;        /* Number of orders saved */
	uint16_t patnum;        /* Number of patterns saved */
	uint16_t insnum;        /* Number of instruments saved */
	uint16_t flags;         /* Module flags (&1 => linear) */
	//uint8_t unused1[8];
	uint8_t tempo;          /* Default tempo (Axx, 1..255) */
	uint8_t bpm;            /* Default beats per minute (BPM) (Txx, 32..255) */
	uint8_t master;         /* Default mastervolume (Vxx, 0..64) */
	uint8_t amp;            /* Amplification factor (mixing volume, 4..127) */
	//uint8_t unused2[8];
	char im10[4];           /* 'IM10' */
	struct imf_channel channels[32]; /* Channel settings */
	uint8_t orderlist[256]; /* Order list (0xff = +++; blank out anything beyond ordnum) */
};

static int imf_read_header(struct imf_header *hdr, slurp_t *fp)
{
#define READ_VALUE(name) \
	do { if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) { return 0; } } while (0)

	READ_VALUE(title);
	READ_VALUE(ordnum);
	READ_VALUE(patnum);
	READ_VALUE(insnum);
	READ_VALUE(flags);
	slurp_seek(fp, 8, SEEK_CUR);
	READ_VALUE(tempo);
	READ_VALUE(bpm);
	READ_VALUE(master);
	READ_VALUE(amp);
	slurp_seek(fp, 8, SEEK_CUR);
	READ_VALUE(im10);

	for (size_t i = 0; i < ARRAY_SIZE(hdr->channels); i++)
		if (!imf_read_channel(&hdr->channels[i], fp))
			return 0;

	READ_VALUE(orderlist);

#undef READ_VALUE

	if (memcmp(hdr->im10, "IM10", 4))
		return 0;

	hdr->ordnum = bswapLE16(hdr->ordnum);
	hdr->patnum = bswapLE16(hdr->patnum);
	hdr->insnum = bswapLE16(hdr->insnum);
	hdr->flags  = bswapLE16(hdr->flags);

	return 1;
}

enum {
	IMF_ENV_VOL = 0,
	IMF_ENV_PAN = 1,
	IMF_ENV_FILTER = 2,
};

struct imf_env {
	uint8_t points;         /* Number of envelope points */
	uint8_t sustain;        /* Envelope sustain point */
	uint8_t loop_start;     /* Envelope loop start point */
	uint8_t loop_end;       /* Envelope loop end point */
	uint8_t flags;          /* Envelope flags */
	//uint8_t unused[3];
};

static int imf_read_env(struct imf_env *env, slurp_t *fp)
{
#define READ_VALUE(name) \
	do { if (slurp_read(fp, &env->name, sizeof(env->name)) != sizeof(env->name)) { return 0; } } while (0)

	READ_VALUE(points);
	READ_VALUE(sustain);
	READ_VALUE(loop_start);
	READ_VALUE(loop_end);
	READ_VALUE(flags);
	slurp_seek(fp, 3, SEEK_CUR);

#undef READ_VALUE

	return 1;
}

struct imf_envnodes {
	uint16_t tick;
	uint16_t value;
};

static int imf_read_envnodes(struct imf_envnodes *envn, slurp_t *fp)
{
#define READ_VALUE(name) \
	do { if (slurp_read(fp, &envn->name, sizeof(envn->name)) != sizeof(envn->name)) { return 0; } } while (0)

	READ_VALUE(tick);
	READ_VALUE(value);

#undef READ_VALUE

	envn->tick  = bswapLE16(envn->tick);
	envn->value = bswapLE16(envn->value);

	return 1;
}

struct imf_instrument {
	char name[32];          /* Inst. name (ASCIIZ-String, max. 31 chars) */
	uint8_t map[120];       /* Multisample settings */
	//uint8_t unused[8];
	struct imf_envnodes nodes[3][16];
	struct imf_env env[3];
	uint16_t fadeout;       /* Fadeout rate (0...0FFFH) */
	uint16_t smpnum;        /* Number of samples in instrument */
	char ii10[4];           /* 'II10' */
};

static int imf_read_instrument(struct imf_instrument *inst, slurp_t *fp)
{
#define READ_VALUE(name) \
	do { if (slurp_read(fp, &inst->name, sizeof(inst->name)) != sizeof(inst->name)) { return 0; } } while (0)

	READ_VALUE(name);
	READ_VALUE(map);

	slurp_seek(fp, 8, SEEK_CUR);

	for (size_t i = 0; i < ARRAY_SIZE(inst->nodes); i++)
		for (size_t j = 0; j < ARRAY_SIZE(inst->nodes[i]); j++)
			if (!imf_read_envnodes(&inst->nodes[i][j], fp))
				return 0;

	for (size_t i = 0; i < ARRAY_SIZE(inst->env); i++)
		if (!imf_read_env(&inst->env[i], fp))
			return 0;

	READ_VALUE(fadeout);
	READ_VALUE(smpnum);
	READ_VALUE(ii10);

#undef READ_VALUE

	if (memcmp(inst->ii10, "II10", 4))
		return 0;

	inst->fadeout = bswapLE16(inst->fadeout);
	inst->smpnum  = bswapLE16(inst->smpnum);

	return 1;
}

struct imf_sample {
	char name[13];          /* Sample filename (12345678.ABC) */
	//uint8_t unused1[3];
	uint32_t length;        /* Length */
	uint32_t loop_start;    /* Loop start */
	uint32_t loop_end;      /* Loop end */
	uint32_t c5speed;       /* Samplerate */
	uint8_t volume;         /* Default volume (0..64) */
	uint8_t panning;        /* Default pan (00h = Left / 80h = Middle) */
	uint8_t unused2[14];
	uint8_t flags;          /* Sample flags */
	uint8_t unused3[5];
	uint16_t ems;           /* Reserved for internal usage */
	uint32_t dram;          /* Reserved for internal usage */
	char is10[4];           /* 'IS10' or 'IW10' */
};

static int imf_read_sample(struct imf_sample *smpl, slurp_t *fp)
{
#define READ_VALUE(name) \
	do { if (slurp_read(fp, &smpl->name, sizeof(smpl->name)) != sizeof(smpl->name)) { return 0; } } while (0)

	READ_VALUE(name);
	slurp_seek(fp, 3, SEEK_CUR);
	READ_VALUE(length);
	READ_VALUE(loop_start);
	READ_VALUE(loop_end);
	READ_VALUE(c5speed);
	READ_VALUE(volume);
	READ_VALUE(panning);
	slurp_seek(fp, 14, SEEK_CUR);
	READ_VALUE(flags);
	slurp_seek(fp, 5, SEEK_CUR);
	READ_VALUE(ems);
	READ_VALUE(dram);
	READ_VALUE(is10);

#undef READ_VALUE

	if (memcmp(smpl->is10, "IS10", 4))
		return 0;

	smpl->length     = bswapLE32(smpl->length);
	smpl->loop_start = bswapLE32(smpl->loop_start);
	smpl->loop_end   = bswapLE32(smpl->loop_end);
	smpl->c5speed    = bswapLE32(smpl->c5speed);

	smpl->ems  = bswapLE16(smpl->ems);
	smpl->dram = bswapLE32(smpl->dram);

	return 1;
}

static uint8_t imf_efftrans[] = {
	FX_NONE,
	FX_SPEED, // 0x01 1xx Set Tempo
	FX_TEMPO, // 0x02 2xx Set BPM
	FX_TONEPORTAMENTO, // 0x03 3xx Tone Portamento                  (*)
	FX_TONEPORTAVOL, // 0x04 4xy Tone Portamento + Volume Slide   (*)
	FX_VIBRATO, // 0x05 5xy Vibrato                          (*)
	FX_VIBRATOVOL, // 0x06 6xy Vibrato + Volume Slide           (*)
	FX_FINEVIBRATO, // 0x07 7xy Fine Vibrato                     (*)
	FX_TREMOLO, // 0x08 8xy Tremolo                          (*)
	FX_ARPEGGIO, // 0x09 9xy Arpeggio                         (*)
	FX_PANNING, // 0x0A Axx Set Pan Position
	FX_PANNINGSLIDE, // 0x0B Bxy Pan Slide                        (*)
	FX_VOLUME, // 0x0C Cxx Set Volume
	FX_VOLUMESLIDE, // 0x0D Dxy Volume Slide                     (*)
	FX_VOLUMESLIDE, // 0x0E Exy Fine Volume Slide                (*)
	FX_SPECIAL, // 0x0F Fxx Set Finetune
	FX_NOTESLIDEUP, // 0x10 Gxy Note Slide Up                    (*)
	FX_NOTESLIDEDOWN, // 0x11 Hxy Note Slide Down                  (*)
	FX_PORTAMENTOUP, // 0x12 Ixx Slide Up                         (*)
	FX_PORTAMENTODOWN, // 0x13 Jxx Slide Down                       (*)
	FX_PORTAMENTOUP, // 0x14 Kxx Fine Slide Up                    (*)
	FX_PORTAMENTODOWN, // 0x15 Lxx Fine Slide Down                  (*)
	FX_MIDI, // 0x16 Mxx Set Filter Cutoff - XXX
	FX_NONE, // 0x17 Nxy Filter Slide + Resonance - XXX
	FX_OFFSET, // 0x18 Oxx Set Sample Offset                (*)
	FX_NONE, // 0x19 Pxx Set Fine Sample Offset - XXX
	FX_KEYOFF, // 0x1A Qxx Key Off
	FX_RETRIG, // 0x1B Rxy Retrig                           (*)
	FX_TREMOR, // 0x1C Sxy Tremor                           (*)
	FX_POSITIONJUMP, // 0x1D Txx Position Jump
	FX_PATTERNBREAK, // 0x1E Uxx Pattern Break
	FX_GLOBALVOLUME, // 0x1F Vxx Set Mastervolume
	FX_GLOBALVOLSLIDE, // 0x20 Wxy Mastervolume Slide               (*)
	FX_SPECIAL, // 0x21 Xxx Extended Effect
	//      X1x Set Filter
	//      X3x Glissando
	//      X5x Vibrato Waveform
	//      X8x Tremolo Waveform
	//      XAx Pattern Loop
	//      XBx Pattern Delay
	//      XCx Note Cut
	//      XDx Note Delay
	//      XEx Ignore Envelope
	//      XFx Invert Loop
	FX_NONE, // 0x22 Yxx Chorus - XXX
	FX_NONE, // 0x23 Zxx Reverb - XXX
};

static void import_imf_effect(song_note_t *note)
{
	uint8_t n;
	// fix some of them
	switch (note->effect) {
	case 0xe: // fine volslide
		// hackaround to get almost-right behavior for fine slides (i think!)
		if (note->param == 0)
			/* nothing */;
		else if (note->param == 0xf0)
			note->param = 0xef;
		else if (note->param == 0x0f)
			note->param = 0xfe;
		else if (note->param & 0xf0)
			note->param |= 0xf;
		else
			note->param |= 0xf0;
		break;
	case 0xf: // set finetune
		// we don't implement this, but let's at least import the value
		note->param = 0x20 | MIN(note->param >> 4, 0xf);
		break;
	case 0x14: // fine slide up
	case 0x15: // fine slide down
		// this is about as close as we can do...
		if (note->param >> 4)
			note->param = 0xf0 | MIN(note->param >> 4, 0xf);
		else
			note->param |= 0xe0;
		break;
	case 0x16: // filter
		note->param = (255 - note->param) / 2; // TODO: cutoff range in IMF is 125...8000 Hz
		break;
	case 0x1f: // set global volume
		note->param = MIN(note->param << 1, 0xff);
		break;
	case 0x21:
		n = 0;
		switch (note->param >> 4) {
		case 0:
			/* undefined, but since S0x does nothing in IT anyway, we won't care.
			this is here to allow S00 to pick up the previous value (assuming IMF
			even does that -- I haven't actually tried it) */
			break;
		default: // undefined
		case 0x1: // set filter
		case 0xf: // invert loop
			note->effect = 0;
			break;
		case 0x3: // glissando
			n = 0x20;
			break;
		case 0x5: // vibrato waveform
			n = 0x30;
			break;
		case 0x8: // tremolo waveform
			n = 0x40;
			break;
		case 0xa: // pattern loop
			n = 0xb0;
			break;
		case 0xb: // pattern delay
			n = 0xe0;
			break;
		case 0xc: // note cut
		case 0xd: // note delay
			// no change
			break;
		case 0xe: // ignore envelope
			switch (note->param & 0x0F) {
				/* predicament: we can only disable one envelope at a time.
				volume is probably most noticeable, so let's go with that. */
			case 0: note->param = 0x77; break;
				// Volume
			case 1: note->param = 0x77; break;
				// Panning
			case 2: note->param = 0x79; break;
				// Filter
			case 3: note->param = 0x7B; break;
			}
			break;
		case 0x18: // sample offset
			// O00 doesn't pick up the previous value
			if (!note->param)
				note->effect = 0;
			break;
		}
		if (n)
			note->param = n | (note->param & 0xf);
		break;
	}
	note->effect = (note->effect < 0x24) ? imf_efftrans[note->effect] : FX_NONE;
	if (note->effect == FX_VOLUME && note->voleffect == VOLFX_NONE) {
		note->voleffect = VOLFX_VOLUME;
		note->volparam = note->param;
		note->effect = FX_NONE;
		note->param = 0;
	}
}

/* return: number of lost effects */
static int load_imf_pattern(song_t *song, int pat, uint32_t ignore_channels, slurp_t *fp)
{
	uint16_t length, nrows;
	uint8_t mask, channel;
	int row;
	unsigned int lostfx = 0;
	song_note_t *row_data, *note, junk_note;

	//int startpos = slurp_tell(fp);

	slurp_read(fp, &length, 2);
	length = bswapLE16(length);
	slurp_read(fp, &nrows, 2);
	nrows = bswapLE16(nrows);

	row_data = song->patterns[pat] = csf_allocate_pattern(nrows);
	song->pattern_size[pat] = song->pattern_alloc_size[pat] = nrows;

	row = 0;
	while (row < nrows) {
		mask = slurp_getc(fp);
		if (mask == 0) {
			row++;
			row_data += MAX_CHANNELS;
			continue;
		}

		channel = mask & 0x1f;

		if (ignore_channels & (1 << channel)) {
			/* should do this better, i.e. not go through the whole process of deciding
			what to do with the effects since they're just being thrown out */
			//printf("disabled channel %d contains data\n", channel + 1);
			note = &junk_note;
		} else {
			note = row_data + channel;
		}

		if (mask & 0x20) {
			/* read note/instrument */
			note->note = slurp_getc(fp);
			note->instrument = slurp_getc(fp);
			if (note->note == 160) {
				note->note = NOTE_OFF; /* ??? */
			} else if (note->note == 255) {
				note->note = NOTE_NONE; /* ??? */
			} else {
				note->note = (note->note >> 4) * 12 + (note->note & 0xf) + 12 + 1;
				if (!NOTE_IS_NOTE(note->note)) {
					//printf("%d.%d.%d: funny note 0x%02x\n",
					//      pat, row, channel, fp->data[fp->pos - 1]);
					note->note = NOTE_NONE;
				}
			}
		}
		if ((mask & 0xc0) == 0xc0) {
			uint8_t e1c, e1d, e2c, e2d;

			/* read both effects and figure out what to do with them */
			e1c = slurp_getc(fp);
			e1d = slurp_getc(fp);
			e2c = slurp_getc(fp);
			e2d = slurp_getc(fp);
			if (e1c == 0xc) {
				note->volparam = MIN(e1d, 0x40);
				note->voleffect = VOLFX_VOLUME;
				note->effect = e2c;
				note->param = e2d;
			} else if (e2c == 0xc) {
				note->volparam = MIN(e2d, 0x40);
				note->voleffect = VOLFX_VOLUME;
				note->effect = e1c;
				note->param = e1d;
			} else if (e1c == 0xa) {
				note->volparam = e1d * 64 / 255;
				note->voleffect = VOLFX_PANNING;
				note->effect = e2c;
				note->param = e2d;
			} else if (e2c == 0xa) {
				note->volparam = e2d * 64 / 255;
				note->voleffect = VOLFX_PANNING;
				note->effect = e1c;
				note->param = e1d;
			} else {
				/* check if one of the effects is a 'global' effect
				-- if so, put it in some unused channel instead.
				otherwise pick the most important effect. */
				lostfx++;
				note->effect = e2c;
				note->param = e2d;
			}
		} else if (mask & 0xc0) {
			/* there's one effect, just stick it in the effect column */
			note->effect = slurp_getc(fp);
			note->param = slurp_getc(fp);
		}
		if (note->effect)
			import_imf_effect(note);
	}

	return lostfx;
}


static unsigned int envflags[3][3] = {
	{ENV_VOLUME,             ENV_VOLSUSTAIN,   ENV_VOLLOOP},
	{ENV_PANNING,            ENV_PANSUSTAIN,   ENV_PANLOOP},
	{ENV_PITCH | ENV_FILTER, ENV_PITCHSUSTAIN, ENV_PITCHLOOP},
};

static void load_imf_envelope(song_instrument_t *ins, song_envelope_t *env, struct imf_instrument *imfins, int e)
{
	int n, t, v;
	int min = 0; // minimum tick value for next node
	int shift = (e == IMF_ENV_VOL ? 0 : 2);
	int mirror = (e == IMF_ENV_FILTER) ? 0xff : 0x00;

	env->nodes = CLAMP(imfins->env[e].points, 2, 25);
	env->loop_start = imfins->env[e].loop_start;
	env->loop_end = imfins->env[e].loop_end;
	env->sustain_start = env->sustain_end = imfins->env[e].sustain;

	for (n = 0; n < env->nodes; n++) {
		t = bswapLE16(imfins->nodes[e][n].tick);
		v = ((bswapLE16(imfins->nodes[e][n].value) & 0xff) ^ mirror) >> shift;
		env->ticks[n] = MAX(min, t);
		env->values[n] = v = MIN(v, 64);
		min = t + 1;
	}
	// this would be less retarded if the envelopes all had their own flags...
	if (imfins->env[e].flags & 1)
		ins->flags |= envflags[e][0];
	if (imfins->env[e].flags & 2)
		ins->flags |= envflags[e][1];
	if (imfins->env[e].flags & 4)
		ins->flags |= envflags[e][2];
}


int fmt_imf_load_song(song_t *song, slurp_t *fp, SCHISM_UNUSED unsigned int lflags)
{
	struct imf_header hdr;
	int n, s;
	song_sample_t *sample = song->samples + 1;
	int firstsample = 1; // first sample for the current instrument
	uint32_t ignore_channels = 0; /* bit set for each channel that's completely disabled */
	int lostfx = 0;

	if (!imf_read_header(&hdr, fp))
		return LOAD_UNSUPPORTED;

	if (hdr.ordnum > MAX_ORDERS || hdr.patnum > MAX_PATTERNS || hdr.insnum > MAX_INSTRUMENTS)
		return LOAD_FORMAT_ERROR;

	memcpy(song->title, hdr.title, 25);
	song->title[25] = 0;
	strcpy(song->tracker_id, "Imago Orpheus");

	if (hdr.flags & 1)
		song->flags |= SONG_LINEARSLIDES;
	song->flags |= SONG_INSTRUMENTMODE;
	song->initial_speed = hdr.tempo;
	song->initial_tempo = hdr.bpm;
	song->initial_global_volume = 2 * hdr.master;
	song->mixing_volume = hdr.amp;

	for (n = 0; n < 32; n++) {
		song->channels[n].panning = hdr.channels[n].panning * 64 / 255;
		song->channels[n].panning *= 4; //mphack
		/* TODO: reverb/chorus??? */
		switch (hdr.channels[n].status) {
		case 0: /* enabled; don't worry about it */
			break;
		case 1: /* mute */
			song->channels[n].flags |= CHN_MUTE;
			break;
		case 2: /* disabled */
			song->channels[n].flags |= CHN_MUTE;
			ignore_channels |= (1 << n);
			break;
		default: /* uhhhh.... freak out */
			//fprintf(stderr, "imf: channel %d has unknown status %d\n", n, hdr.channels[n].status);
			return LOAD_FORMAT_ERROR;
		}
	}
	for (; n < MAX_CHANNELS; n++)
		song->channels[n].flags |= CHN_MUTE;
	/* From mikmod: work around an Orpheus bug */
	if (hdr.channels[0].status == 0) {
		for (n = 1; n < 16; n++)
			if (hdr.channels[n].status != 1)
				break;
		if (n == 16)
			for (n = 1; n < 16; n++)
				song->channels[n].flags &= ~CHN_MUTE;
	}

	for (n = 0; n < hdr.ordnum; n++)
		song->orderlist[n] = ((hdr.orderlist[n] == 0xff) ? ORDER_SKIP : hdr.orderlist[n]);

	for (n = 0; n < hdr.patnum; n++)
		lostfx += load_imf_pattern(song, n, ignore_channels, fp);

	if (lostfx)
		log_appendf(4, " Warning: %d effect%s dropped", lostfx, lostfx == 1 ? "" : "s");

	for (n = 0; n < hdr.insnum; n++) {
		// read the ins header
		struct imf_instrument imfins;
		song_instrument_t *ins;

		if (!imf_read_instrument(&imfins, fp)) {
			printf("readins failed\n");
			break;
		}

		ins = song->instruments[n + 1] = csf_allocate_instrument();
		strncpy(ins->name, imfins.name, 25);
		ins->name[25] = 0;

		if (imfins.smpnum) {
			for (s = 12; s < 120; s++) {
				ins->note_map[s] = s + 1;
				ins->sample_map[s] = firstsample + imfins.map[s - 12];
			}
		}

		/* Fadeout:
		IT1 - 64
		IT2 - 256
		FT2 - 4095
		IMF - 4095
		MPT - god knows what, all the loaders are inconsistent
		Schism - 256 presented; 8192? internal

		IMF and XM have the same range and modplug's XM loader doesn't do any bit shifting with it,
		so I'll do the same here for now. I suppose I should get this nonsense straightened
		out at some point, though. */
		ins->fadeout = imfins.fadeout;
		ins->global_volume = 128;

		load_imf_envelope(ins, &ins->vol_env, &imfins, IMF_ENV_VOL);
		load_imf_envelope(ins, &ins->pan_env, &imfins, IMF_ENV_PAN);
		load_imf_envelope(ins, &ins->pitch_env, &imfins, IMF_ENV_FILTER);

		/* I'm not sure if XM's envelope hacks apply here or not, but Orpheus *does* at least stop
		samples upon note-off when no envelope is active. Whether or not this depends on the fadeout
		value, I don't know, and since the fadeout doesn't even seem to be implemented in the gui,
		I might never find out. :P */
		if (!(ins->flags & ENV_VOLUME)) {
			ins->vol_env.ticks[0] = 0;
			ins->vol_env.ticks[1] = 1;
			ins->vol_env.values[0] = 64;
			ins->vol_env.values[1] = 0;
			ins->vol_env.nodes = 2;
			ins->vol_env.sustain_start = ins->vol_env.sustain_end = 0;
			ins->flags |= ENV_VOLUME | ENV_VOLSUSTAIN;
		}

		for (s = 0; s < imfins.smpnum; s++) {
			struct imf_sample imfsmp;
			uint32_t sflags = SF_LE | SF_M | SF_PCMS;

			if (!imf_read_sample(&imfsmp, fp)) {
				printf("readsmp failed\n");
				break;
			}

			strncpy(sample->filename, imfsmp.name, 12);
			sample->filename[12] = 0;
			strcpy(sample->name, sample->filename);
			sample->length = imfsmp.length;
			sample->loop_start = imfsmp.loop_start;
			sample->loop_end = imfsmp.loop_end;
			sample->c5speed = imfsmp.c5speed;
			sample->volume = imfsmp.volume * 4; //mphack
			sample->panning = imfsmp.panning; //mphack (IT uses 0-64, IMF uses the full 0-255)
			if (imfsmp.flags & 1)
				sample->flags |= CHN_LOOP;
			if (imfsmp.flags & 2)
				sample->flags |= CHN_PINGPONGLOOP;
			if (imfsmp.flags & 4) {
				sflags |= SF_16;
				sample->length >>= 1;
				sample->loop_start >>= 1;
				sample->loop_end >>= 1;
			} else {
				sflags |= SF_8;
			}
			if (imfsmp.flags & 8)
				sample->flags |= CHN_PANNING;

			if (!(lflags & LOAD_NOSAMPLES))
				csf_read_sample(sample, sflags, fp);
			else
				slurp_seek(fp, imfsmp.length * ((sflags & SF_16) ? 2 : 1), SEEK_CUR);

			sample++;
		}
		firstsample += imfins.smpnum;
	}

	// Fix the MIDI settings, because IMF files might include Zxx effects
	memset(&song->midi_config, 0, sizeof(song->midi_config));
	strcpy(song->midi_config.sfx[0], "F0F000z");
	song->flags |= SONG_EMBEDMIDICFG;

	return LOAD_SUCCESS;
}

