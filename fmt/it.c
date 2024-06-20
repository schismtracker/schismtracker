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
#include "charset.h"
#include "slurp.h"
#include "fmt.h"
#include "log.h"
#include "version.h"

#include "sndfile.h"
#include "midi.h"

// TODO: its/iti loaders should be collapsed into here -- no sense duplicating all of this code

/* --------------------------------------------------------------------- */

int fmt_it_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
	/* "Bart just said I-M-P! He's made of pee!" */
	if (length > 30 && memcmp(data, "IMPM", 4) == 0) {
		/* This ought to be more particular; if it's not actually made *with* Impulse Tracker,
		it's probably not compressed, irrespective of what the CMWT says. */
		if (data[42] >= 0x14)
			file->description = "Compressed Impulse Tracker";
		else
			file->description = "Impulse Tracker";
	} else {
		return 0;
	}

	/*file->extension = str_dup("it");*/
	file->title = mem_alloc(26);
	for (int n = 0; n < 25; n++) {
		file->title[n] = data[4 + n] ? data[4 + n] : 32;
	}
	file->title[25] = 0;
	rtrim_string(file->title);
	file->type = TYPE_MODULE_IT;
	return 1;
}

/* --------------------------------------------------------------------- */

#pragma pack(push, 1)

struct it_header {
	char impm[4], title[26];
	uint8_t highlight_minor, highlight_major;
	uint16_t ordnum, insnum, smpnum, patnum;
	uint16_t cwtv, cmwt, flags, special;
	uint8_t gv, mv, is, it, sep, pwd;
	uint16_t msg_length;
	uint32_t msg_offset, reserved;
	uint8_t chan_pan[64], chan_vol[64];
};

struct it_sample {
	char imps[4], filename[13];
	uint8_t gvl, flag, vol;
	char name[26];
	uint8_t cvt, dfp;
	uint32_t length, loop_start, loop_end, c5speed;
	uint32_t susloop_start, susloop_end, sample_pointer;
	uint8_t vis, vid, vir, vit;
};

struct it_envelope  {
	uint8_t flags, num_nodes, loop_start, loop_end;
	uint8_t susloop_start, susloop_end;
	struct {
		int8_t value; // signed (-32 -> 32 for pan and pitch; 0 -> 64 for vol and filter)
		uint16_t tick;
	} nodes[25];
	uint8_t padding;
};

struct it_notetrans {
	uint8_t note;
	uint8_t sample;
};

struct it_instrument {
	char impi[4], filename[13];
	uint8_t nna, dct, dca;
	uint16_t fadeout;
	int8_t pps; // signed!
	uint8_t ppc, gbv, dfp, rv, rp;
	uint16_t trkvers;
	uint8_t num_samples, padding;
	char name[26];
	uint8_t ifc, ifr, mch, mpr;
	uint16_t midibank;
	struct it_notetrans notetrans[120];
	struct it_envelope vol_env, pan_env, pitch_env;
};

struct it_instrument_old {
	char impi[4], filename[13];
	uint8_t flg, vls, vle, sls, sle;
	uint8_t xx[2];
	uint16_t fadeout;
	uint8_t nna, dnc;
	uint16_t trkvers;
	uint8_t nos;
	uint8_t x;
	char name[26];
	uint8_t xxxxxx[6];
	struct it_notetrans notetrans[120];
	uint8_t vol_env[200];
	uint8_t node_points[50];
};

#pragma pack(pop)


/* pattern mask variable bits */
enum {
	ITNOTE_NOTE = 1,
	ITNOTE_SAMPLE = 2,
	ITNOTE_VOLUME = 4,
	ITNOTE_EFFECT = 8,
	ITNOTE_SAME_NOTE = 16,
	ITNOTE_SAME_SAMPLE = 32,
	ITNOTE_SAME_VOLUME = 64,
	ITNOTE_SAME_EFFECT = 128,
};

static const uint8_t autovib_import[] = {VIB_SINE, VIB_RAMP_DOWN, VIB_SQUARE, VIB_RANDOM};

/* --------------------------------------------------------------------- */

static void it_import_voleffect(song_note_t *note, uint8_t v)
{
	uint8_t adj;

	if (v <= 64)                   { adj =   0; note->voleffect = VOLFX_VOLUME; }
	else if (v >= 128 && v <= 192) { adj = 128; note->voleffect = VOLFX_PANNING; }
	else if (v >= 65 && v <= 74)   { adj =  65; note->voleffect = VOLFX_FINEVOLUP; }
	else if (v >= 75 && v <= 84)   { adj =  75; note->voleffect = VOLFX_FINEVOLDOWN; }
	else if (v >= 85 && v <= 94)   { adj =  85; note->voleffect = VOLFX_VOLSLIDEUP; }
	else if (v >= 95 && v <= 104)  { adj =  95; note->voleffect = VOLFX_VOLSLIDEDOWN; }
	else if (v >= 105 && v <= 114) { adj = 105; note->voleffect = VOLFX_PORTADOWN; }
	else if (v >= 115 && v <= 124) { adj = 115; note->voleffect = VOLFX_PORTAUP; }
	else if (v >= 193 && v <= 202) { adj = 193; note->voleffect = VOLFX_TONEPORTAMENTO; }
	else if (v >= 203 && v <= 212) { adj = 203; note->voleffect = VOLFX_VIBRATODEPTH; }
	else { return; }

	note->volparam = v - adj;
}

static void load_it_notetrans(song_instrument_t *instrument, struct it_notetrans *notetrans)
{
	int note, n;
	for (n = 0; n < 120; n++) {
		note = notetrans[n].note + NOTE_FIRST;
		// map invalid notes to themselves
		if (!NOTE_IS_NOTE(note))
			note = n + NOTE_FIRST;
		instrument->note_map[n] = note;
		instrument->sample_map[n] = notetrans[n].sample;
	}
}



static void load_it_pattern(song_note_t *note, slurp_t *fp, int rows)
{
	song_note_t last_note[64];
	int chan, row = 0;
	uint8_t last_mask[64] = { 0 };
	uint8_t chanvar, maskvar, c;

	while (row < rows) {
		chanvar = slurp_getc(fp);

		if (chanvar == 255 && slurp_eof(fp)) {
			/* truncated file? we might want to complain or something ... eh. */
			return;
		}

		if (chanvar == 0) {
			row++;
			note += 64;
			continue;
		}
		chan = (chanvar - 1) & 63;
		if (chanvar & 128) {
			maskvar = slurp_getc(fp);
			last_mask[chan] = maskvar;
		} else {
			maskvar = last_mask[chan];
		}
		if (maskvar & ITNOTE_NOTE) {
			c = slurp_getc(fp);
			if (c == 255)
				c = NOTE_OFF;
			else if (c == 254)
				c = NOTE_CUT;
			// internally IT uses note 253 as its blank value, but loading it as such is probably
			// undesirable since old Schism Tracker used this value incorrectly for note fade
			//else if (c == 253)
			//      c = NOTE_NONE;
			else if (c > 119)
				c = NOTE_FADE;
			else
				c += NOTE_FIRST;
			note[chan].note = c;
			last_note[chan].note = note[chan].note;
		}
		if (maskvar & ITNOTE_SAMPLE) {
			note[chan].instrument = slurp_getc(fp);
			last_note[chan].instrument = note[chan].instrument;
		}
		if (maskvar & ITNOTE_VOLUME) {
			it_import_voleffect(note + chan, slurp_getc(fp));
			last_note[chan].voleffect = note[chan].voleffect;
			last_note[chan].volparam = note[chan].volparam;
		}
		if (maskvar & ITNOTE_EFFECT) {
			note[chan].effect = slurp_getc(fp) & 0x1f;
			note[chan].param = slurp_getc(fp);
			csf_import_s3m_effect(note + chan, 1);
			last_note[chan].effect = note[chan].effect;
			last_note[chan].param = note[chan].param;
		}
		if (maskvar & ITNOTE_SAME_NOTE)
			note[chan].note = last_note[chan].note;
		if (maskvar & ITNOTE_SAME_SAMPLE)
			note[chan].instrument = last_note[chan].instrument;
		if (maskvar & ITNOTE_SAME_VOLUME) {
			note[chan].voleffect = last_note[chan].voleffect;
			note[chan].volparam = last_note[chan].volparam;
		}
		if (maskvar & ITNOTE_SAME_EFFECT) {
			note[chan].effect = last_note[chan].effect;
			note[chan].param = last_note[chan].param;
		}
	}
}



static void load_it_instrument_old(song_instrument_t *instrument, slurp_t *fp)
{
	struct it_instrument_old ihdr;
	int n;

	slurp_read(fp, &ihdr, sizeof(ihdr));

	memcpy(instrument->name, ihdr.name, 25);
	instrument->name[25] = '\0';
	memcpy(instrument->filename, ihdr.filename, 12);
	ihdr.filename[12] = '\0';

	instrument->nna = ihdr.nna % 4;
	if (ihdr.dnc) {
		// XXX is this right?
		instrument->dct = DCT_NOTE;
		instrument->dca = DCA_NOTECUT;
	}

	instrument->fadeout = bswapLE16(ihdr.fadeout) << 6;
	instrument->pitch_pan_separation = 0;
	instrument->pitch_pan_center = NOTE_MIDC;
	instrument->global_volume = 128;
	instrument->panning = 32 * 4; //mphack

	load_it_notetrans(instrument, ihdr.notetrans);

	if (ihdr.flg & 1)
		instrument->flags |= ENV_VOLUME;
	if (ihdr.flg & 2)
		instrument->flags |= ENV_VOLLOOP;
	if (ihdr.flg & 4)
		instrument->flags |= ENV_VOLSUSTAIN;

	instrument->vol_env.loop_start = ihdr.vls;
	instrument->vol_env.loop_end = ihdr.vle;
	instrument->vol_env.sustain_start = ihdr.sls;
	instrument->vol_env.sustain_end = ihdr.sle;
	instrument->vol_env.nodes = 25;
	// this seems totally wrong... why isn't this using ihdr.vol_env at all?
	// apparently it works, though.
	for (n = 0; n < 25; n++) {
		int node = ihdr.node_points[2 * n];
		if (node == 0xff) {
			instrument->vol_env.nodes = n;
			break;
		}
		instrument->vol_env.ticks[n] = node;
		instrument->vol_env.values[n] = ihdr.node_points[2 * n + 1];
	}
}


static const uint32_t env_flags[3][4] = {
	{ENV_VOLUME,  ENV_VOLLOOP,   ENV_VOLSUSTAIN,   ENV_VOLCARRY},
	{ENV_PANNING, ENV_PANLOOP,   ENV_PANSUSTAIN,   ENV_PANCARRY},
	{ENV_PITCH,   ENV_PITCHLOOP, ENV_PITCHSUSTAIN, ENV_PITCHCARRY},
};

static uint32_t load_it_envelope(song_envelope_t *env, struct it_envelope *itenv, int envtype, int adj)
{
	uint32_t flags = 0;
	int n;

	env->nodes = CLAMP(itenv->num_nodes, 2, 25);
	env->loop_start = MIN(itenv->loop_start, env->nodes);
	env->loop_end = CLAMP(itenv->loop_end, env->loop_start, env->nodes);
	env->sustain_start = MIN(itenv->susloop_start, env->nodes);
	env->sustain_end = CLAMP(itenv->susloop_end, env->sustain_start, env->nodes);

	for (n = 0; n < env->nodes; n++) {
		int v = itenv->nodes[n].value + adj;
		env->values[n] = CLAMP(v, 0, 64);
		env->ticks[n] = bswapLE16(itenv->nodes[n].tick);
	}
	env->ticks[0] = 0; // sanity check

	for (n = 0; n < 4; n++) {
		if (itenv->flags & (1 << n))
			flags |= env_flags[envtype][n];
	}
	if (envtype == 2 && (itenv->flags & 0x80))
		flags |= ENV_FILTER;
	return flags;
}


static void load_it_instrument(song_instrument_t *instrument, slurp_t *fp)
{
	struct it_instrument ihdr;

	slurp_read(fp, &ihdr, sizeof(ihdr));

	memcpy(instrument->name, ihdr.name, 25);
	instrument->name[25] = '\0';
	memcpy(instrument->filename, ihdr.filename, 12);
	ihdr.filename[12] = '\0';

	instrument->nna = ihdr.nna % 4;
	instrument->dct = ihdr.dct % 4;
	instrument->dca = ihdr.dca % 3;
	instrument->fadeout = bswapLE16(ihdr.fadeout) << 5;
	instrument->pitch_pan_separation = CLAMP(ihdr.pps, -32, 32);
	instrument->pitch_pan_center = MIN(ihdr.ppc, 119); // I guess
	instrument->global_volume = MIN(ihdr.gbv, 128);
	instrument->panning = MIN((ihdr.dfp & 127), 64) * 4; //mphack
	if (!(ihdr.dfp & 128))
		instrument->flags |= ENV_SETPANNING;
	instrument->vol_swing = MIN(ihdr.rv, 100);
	instrument->pan_swing = MIN(ihdr.rp, 64);

	instrument->ifc = ihdr.ifc;
	instrument->ifr = ihdr.ifr;

	// (blah... this isn't supposed to be a mask according to the
	// spec. where did this code come from? and what is 0x10000?)
	instrument->midi_channel_mask =
			((ihdr.mch > 16)
			 ? (0x10000 + ihdr.mch)
			 : ((ihdr.mch > 0)
			    ? (1 << (ihdr.mch - 1))
			    : 0));
	instrument->midi_program = ihdr.mpr;
	instrument->midi_bank = bswapLE16(ihdr.midibank);

	load_it_notetrans(instrument, ihdr.notetrans);

	instrument->flags |= load_it_envelope(&instrument->vol_env, &ihdr.vol_env, 0, 0);
	instrument->flags |= load_it_envelope(&instrument->pan_env, &ihdr.pan_env, 1, 32);
	instrument->flags |= load_it_envelope(&instrument->pitch_env, &ihdr.pitch_env, 2, 32);
}


static void load_it_sample(song_sample_t *sample, slurp_t *fp, uint16_t cwtv)
{
	struct it_sample shdr;

	slurp_read(fp, &shdr, sizeof(shdr));

	/* Fun fact: Impulse Tracker doesn't check any of the header data for consistency when loading samples
	(or instruments, for that matter). If some other data is stored in place of the IMPS/IMPI, it'll
	happily load it anyway -- and in fact, since the song is manipulated in memory in the same format as
	on disk, this data is even preserved when the file is saved! */

	memcpy(sample->name, shdr.name, 25);
	sample->name[25] = '\0';

	memcpy(sample->filename, shdr.filename, 12);
	sample->filename[12] = '\0';

	if (shdr.dfp & 128) {
		sample->flags |= CHN_PANNING;
		shdr.dfp &= 127;
	}

	sample->global_volume = MIN(shdr.gvl, 64);
	sample->volume = MIN(shdr.vol, 64) * 4; //mphack
	sample->panning = MIN(shdr.dfp, 64) * 4; //mphack
	sample->length = bswapLE32(shdr.length);
	sample->length = MIN(sample->length, MAX_SAMPLE_LENGTH);
	sample->loop_start = bswapLE32(shdr.loop_start);
	sample->loop_end = bswapLE32(shdr.loop_end);
	sample->c5speed = bswapLE32(shdr.c5speed);
	sample->sustain_start = bswapLE32(shdr.susloop_start);
	sample->sustain_end = bswapLE32(shdr.susloop_end);

	sample->vib_speed = MIN(shdr.vis, 64);
	sample->vib_depth = MIN(shdr.vid, 32);
	sample->vib_rate = shdr.vir;
	sample->vib_type = autovib_import[shdr.vit % 4];

	if (shdr.flag & 16)
		sample->flags |= CHN_LOOP;
	if (shdr.flag & 32)
		sample->flags |= CHN_SUSTAINLOOP;
	if (shdr.flag & 64)
		sample->flags |= CHN_PINGPONGLOOP;
	if (shdr.flag & 128)
		sample->flags |= CHN_PINGPONGSUSTAIN;

	/* IT sometimes didn't clear the flag after loading a stereo sample. This appears to have
	been fixed sometime before IT 2.14, which is fortunate because that's what a lot of other
	programs annoyingly identify themselves as. */
	if (cwtv < 0x0214)
		shdr.flag &= ~4;

	if ((shdr.flag & 1) && shdr.cvt == 64 && sample->length == 12) {
		// OPL instruments in OpenMPT MPTM files (which are essentially extended IT files)
		slurp_seek(fp, bswapLE32(shdr.sample_pointer), SEEK_SET);
		slurp_read(fp, sample->adlib_bytes, 12);
		sample->flags |= CHN_ADLIB;
		// dumb hackaround that ought to some day be fixed:
		sample->length = 1;
		sample->data = csf_allocate_sample(1);
	} else if (shdr.flag & 1) {
		slurp_seek(fp, bswapLE32(shdr.sample_pointer), SEEK_SET);

		uint32_t flags = SF_LE;
		flags |= (shdr.flag & 4) ? SF_SS : SF_M;
		if (shdr.flag & 8) {
			flags |= (shdr.cvt & 4) ? SF_IT215 : SF_IT214;
		} else {
			// XXX for some reason I had a note in pm/fmt/it.c saying that I had found some
			// .it files with the signed flag set incorrectly and to assume unsigned when
			// hdr.cwtv < 0x0202. Why, and for what files?
			// Do any other players use the header for deciding sample data signedness?
			flags |= (shdr.cvt & 4) ? SF_PCMD : (shdr.cvt & 1) ? SF_PCMS : SF_PCMU;
		}
		flags |= (shdr.flag & 2) ? SF_16 : SF_8;
		csf_read_sample(sample, flags, fp->data + fp->pos, fp->length - fp->pos);
	} else {
		sample->length = 0;
	}
}

int fmt_it_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	struct it_header hdr;
	uint32_t para_smp[MAX_SAMPLES], para_ins[MAX_INSTRUMENTS], para_pat[MAX_PATTERNS], para_min;
	int n;
	int modplug = 0;
	int ignoremidi = 0;
	song_channel_t *channel;
	song_sample_t *sample;
	uint16_t hist = 0; // save history (for IT only)
	const char *tid = NULL;

	slurp_read(fp, &hdr, sizeof(hdr));

	if (memcmp(hdr.impm, "IMPM", 4) != 0)
		return LOAD_UNSUPPORTED;

	hdr.ordnum = bswapLE16(hdr.ordnum);
	hdr.insnum = bswapLE16(hdr.insnum);
	hdr.smpnum = bswapLE16(hdr.smpnum);
	hdr.patnum = bswapLE16(hdr.patnum);
	hdr.cwtv = bswapLE16(hdr.cwtv);
	hdr.cmwt = bswapLE16(hdr.cmwt);
	hdr.flags = bswapLE16(hdr.flags);
	hdr.special = bswapLE16(hdr.special);
	hdr.msg_length = bswapLE16(hdr.msg_length);
	hdr.msg_offset = bswapLE32(hdr.msg_offset);
	hdr.reserved = bswapLE32(hdr.reserved);

	// Screwy limits?
	if (hdr.insnum > MAX_INSTRUMENTS || hdr.smpnum > MAX_SAMPLES
		|| hdr.patnum > MAX_PATTERNS) {
		return LOAD_FORMAT_ERROR;
	}

	for (n = 0; n < 25; n++) {
		song->title[n] = hdr.title[n] ? hdr.title[n] : 32;
	}
	song->title[25] = 0;
	rtrim_string(song->title);

	if (hdr.cmwt < 0x0214 && hdr.cwtv < 0x0214)
		ignoremidi = 1;
	if (hdr.special & 4) {
		/* "reserved" bit, experimentally determined to indicate presence of otherwise-documented row
		highlight information - introduced in IT 2.13. Formerly checked cwtv here, but that's lame :)
		XXX does any tracker save highlight but *not* set this bit? (old Schism versions maybe?) */
		song->row_highlight_minor = hdr.highlight_minor;
		song->row_highlight_major = hdr.highlight_major;
	} else {
		song->row_highlight_minor = 4;
		song->row_highlight_major = 16;
	}

	if (!(hdr.flags & 1))
		song->flags |= SONG_NOSTEREO;
	// (hdr.flags & 2) no longer used (was vol0 optimizations)
	if (hdr.flags & 4)
		song->flags |= SONG_INSTRUMENTMODE;
	if (hdr.flags & 8)
		song->flags |= SONG_LINEARSLIDES;
	if (hdr.flags & 16)
		song->flags |= SONG_ITOLDEFFECTS;
	if (hdr.flags & 32)
		song->flags |= SONG_COMPATGXX;
	if (hdr.flags & 64) {
		midi_flags |= MIDI_PITCHBEND;
		midi_pitch_depth = hdr.pwd;
	}
	if ((hdr.flags & 128) && !ignoremidi)
		song->flags |= SONG_EMBEDMIDICFG;
	else
		song->flags &= ~SONG_EMBEDMIDICFG;

	song->initial_global_volume = MIN(hdr.gv, 128);
	song->mixing_volume = MIN(hdr.mv, 128);
	song->initial_speed = hdr.is ? hdr.is : 6;
	song->initial_tempo = MAX(hdr.it, 31);
	song->pan_separation = hdr.sep;

	for (n = 0, channel = song->channels; n < 64; n++, channel++) {
		int pan = hdr.chan_pan[n];
		if (pan & 128) {
			channel->flags |= CHN_MUTE;
			pan &= ~128;
		}
		if (pan == 100) {
			channel->flags |= CHN_SURROUND;
			channel->panning = 32;
		} else {
			channel->panning = MIN(pan, 64);
		}
		channel->panning *= 4; //mphack
		channel->volume = MIN(hdr.chan_vol[n], 64);
	}

	/* only read what we can and ignore the rest */
	slurp_read(fp, song->orderlist, MIN(hdr.ordnum, MAX_ORDERS));

	/* show a warning in the message log if there's too many orders */
	if (hdr.ordnum > MAX_ORDERS) {
		const int lostord = hdr.ordnum - MAX_ORDERS;
		int show_warning = 1;

		/* special exception: ordnum == 257 is valid ONLY if the final order is ORDER_LAST */
		if (lostord == 1) {
			uint8_t ord;
			slurp_read(fp, &ord, sizeof(ord));

			show_warning = (ord != ORDER_LAST);
		} else {
			slurp_seek(fp, SEEK_CUR, lostord);
		}

		if (show_warning)
			log_appendf(4, " Warning: Too many orders in the order list (%d skipped)", lostord);
	}

	slurp_read(fp, para_ins, 4 * hdr.insnum);
	slurp_read(fp, para_smp, 4 * hdr.smpnum);
	slurp_read(fp, para_pat, 4 * hdr.patnum);

	para_min = ((hdr.special & 1) && hdr.msg_length) ? hdr.msg_offset : fp->length;
	for (n = 0; n < hdr.insnum; n++) {
		para_ins[n] = bswapLE32(para_ins[n]);
		if (para_ins[n] < para_min)
			para_min = para_ins[n];
	}
	for (n = 0; n < hdr.smpnum; n++) {
		para_smp[n] = bswapLE32(para_smp[n]);
		if (para_smp[n] < para_min)
			para_min = para_smp[n];
	}
	for (n = 0; n < hdr.patnum; n++) {
		para_pat[n] = bswapLE32(para_pat[n]);
		if (para_pat[n] && para_pat[n] < para_min)
			para_min = para_pat[n];
	}

	if (hdr.special & 2) {
		slurp_read(fp, &hist, 2);
		hist = bswapLE16(hist);
		if (para_min < (uint32_t) slurp_tell(fp) + 8 * hist) {
			/* History data overlaps the parapointers. Discard it, it's probably broken.
			Some programs, notably older versions of Schism Tracker, set the history flag
			but didn't actually write any data, so the "length" we just read is actually
			some other data in the file. */
			hist = 0;
		}
	} else {
		// History flag isn't even set. Probably an old version of Impulse Tracker.
		hist = 0;
	}
	if (hist) {
		song->histlen = hist;
		song->histdata = mem_alloc(8 * song->histlen);
		slurp_read(fp, song->histdata, 8 * song->histlen);
	}
	if (ignoremidi) {
		if (hdr.special & 8) {
			log_appendf(4, " Warning: ignoring embedded MIDI data (CWTV/CMWT is too old)");
			slurp_seek(fp, sizeof(midi_config_t), SEEK_CUR);
		}
		memset(&song->midi_config, 0, sizeof(midi_config_t));
	} else if ((hdr.special & 8) && fp->pos + sizeof(midi_config_t) <= fp->length) {
		slurp_read(fp, &song->midi_config, sizeof(midi_config_t));
	}
	if (!hist) {
		// berotracker check
		char modu[4];
		slurp_read(fp, modu, 4);
		if (memcmp(modu, "MODU", 4) == 0) {
			tid = "BeroTracker";
		}
	}

	if ((hdr.special & 1) && hdr.msg_length && hdr.msg_offset + hdr.msg_length < fp->length) {
		int msg_len = MIN(MAX_MESSAGE, hdr.msg_length);
		slurp_seek(fp, hdr.msg_offset, SEEK_SET);
		slurp_read(fp, song->message, msg_len);
		song->message[msg_len] = '\0';
	}


	if (!(lflags & LOAD_NOSAMPLES)) {
		for (n = 0; n < hdr.insnum; n++) {
			song_instrument_t *inst;

			if (!para_ins[n])
				continue;
			slurp_seek(fp, para_ins[n], SEEK_SET);
			inst = song->instruments[n + 1] = csf_allocate_instrument();
			(hdr.cmwt >= 0x0200 ? load_it_instrument : load_it_instrument_old)(inst, fp);
		}

		for (n = 0, sample = song->samples + 1; n < hdr.smpnum; n++, sample++) {
			slurp_seek(fp, para_smp[n], SEEK_SET);
			load_it_sample(sample, fp, hdr.cwtv);
		}
	}

	if (!(lflags & LOAD_NOPATTERNS)) {
		for (n = 0; n < hdr.patnum; n++) {
			uint16_t rows, bytes;
			size_t got;

			if (!para_pat[n])
				continue;
			slurp_seek(fp, para_pat[n], SEEK_SET);
			slurp_read(fp, &bytes, 2);
			bytes = bswapLE16(bytes);
			slurp_read(fp, &rows, 2);
			rows = bswapLE16(rows);
			slurp_seek(fp, 4, SEEK_CUR);
			song->patterns[n] = csf_allocate_pattern(rows);
			song->pattern_size[n] = song->pattern_alloc_size[n] = rows;
			load_it_pattern(song->patterns[n], fp, rows);
			got = slurp_tell(fp) - para_pat[n] - 8;
			if (bytes != got)
				log_appendf(4, " Warning: Pattern %d: size mismatch"
					" (expected %d bytes, got %lu)",
					n, bytes, (unsigned long) got);
		}
	}


	// XXX 32 CHARACTER MAX XXX

	if (tid) {
		// BeroTracker (detected above)
	} else if ((hdr.cwtv >> 12) == 1) {
		tid = NULL;
		strcpy(song->tracker_id, "Schism Tracker ");
		ver_decode_cwtv(hdr.cwtv, hdr.reserved, song->tracker_id + strlen(song->tracker_id));
	} else if ((hdr.cwtv >> 12) == 0 && hist != 0 && hdr.reserved != 0) {
		// early catch to exclude possible false positives without repeating a bunch of stuff.
	} else if (hdr.cwtv == 0x0214 && hdr.cmwt == 0x0200 && hdr.flags == 9 && hdr.special == 0
		   && hdr.highlight_major == 0 && hdr.highlight_minor == 0
		   && hdr.insnum == 0 && hdr.patnum + 1 == hdr.ordnum
		   && hdr.gv == 128 && hdr.mv == 100 && hdr.is == 1 && hdr.sep == 128 && hdr.pwd == 0
		   && hdr.msg_length == 0 && hdr.msg_offset == 0 && hdr.reserved == 0) {
		// :)
		tid = "OpenSPC conversion";
	} else if ((hdr.cwtv >> 12) == 5) {
		if (hdr.reserved == 0x54504d4f)
			tid = "OpenMPT %d.%02x";
		else if (hdr.cwtv < 0x5129 || !(hdr.reserved & 0xffff))
			tid = "OpenMPT %d.%02x (compat.)";
		else
			sprintf(song->tracker_id, "OpenMPT %d.%02x.%02x.%02x (compat.)", (hdr.cwtv & 0xf00) >> 8, hdr.cwtv & 0xff, (hdr.reserved >> 8) & 0xff, (hdr.reserved & 0xff));
		modplug = 1;
	} else if (hdr.cwtv == 0x0888 && hdr.cmwt == 0x0888 && hdr.reserved == 0/* && hdr.ordnum == 256*/) {
		// erh.
		// There's a way to identify the exact version apparently, but it seems too much trouble
		// (ordinarily ordnum == 256, but I have encountered at least one file for which this is NOT
		// the case (trackit_r2.it by dsck) and no other trackers I know of use 0x0888)
		tid = "OpenMPT 1.17+";
		modplug = 1;
	} else if (hdr.cwtv == 0x0300 && hdr.cmwt == 0x0300 && hdr.reserved == 0 && hdr.ordnum == 256 && hdr.sep == 128 && hdr.pwd == 0) {
		tid = "OpenMPT 1.17.02.20 - 1.17.02.25";
		modplug = 1;
	} else if (hdr.cwtv == 0x0217 && hdr.cmwt == 0x0200 && hdr.reserved == 0) {
		int ompt = 0;
		if (hdr.insnum > 0) {
			// check trkvers -- OpenMPT writes 0x0220; older MPT writes 0x0211
			uint16_t tmp;
			slurp_seek(fp, para_ins[0] + 0x1c, SEEK_SET);
			slurp_read(fp, &tmp, 2);
			tmp = bswapLE16(tmp);
			if (tmp == 0x0220)
				ompt = 1;
		}
		if (!ompt && (memchr(hdr.chan_pan, 0xff, 64) == NULL)) {
			// MPT 1.16 writes 0xff for unused channels; OpenMPT never does this
			// XXX this is a false positive if all 64 channels are actually in use
			// -- but then again, who would use 64 channels and not instrument mode?
			ompt = 1;
		}
		tid = (ompt
			? "OpenMPT (compatibility mode)"
			: "Modplug Tracker 1.09 - 1.16");
		modplug = 1;
	} else if (hdr.cwtv == 0x0214 && hdr.cmwt == 0x0200 && hdr.reserved == 0) {
		// instruments 560 bytes apart
		tid = "Modplug Tracker 1.00a5";
		modplug = 1;
	} else if (hdr.cwtv == 0x0214 && hdr.cmwt == 0x0202 && hdr.reserved == 0) {
		// instruments 557 bytes apart
		tid = "Modplug Tracker b3.3 - 1.07";
		modplug = 1;
	} else if (hdr.cwtv == 0x0214 && hdr.cmwt == 0x0214 && hdr.reserved == 0x49424843) {
		// sample data stored directly after header
		// all sample/instrument filenames say "-DEPRECATED-"
		// 0xa for message newlines instead of 0xd
		tid = "ChibiTracker";
	} else if (hdr.cwtv == 0x0214 && hdr.cmwt == 0x0214 && (hdr.flags & 0x10C6) == 4 && hdr.special <= 1 && hdr.reserved == 0) {
		// sample data stored directly after header
		// all sample/instrument filenames say "XXXXXXXX.YYY"
		tid = "CheeseTracker?";
	} else if ((hdr.cwtv >> 12) == 0) {
		// Catch-all. The above IT condition only works for newer IT versions which write something
		// into the reserved field; older IT versions put zero there (which suggests that maybe it
		// really is being used for something useful)
		// (handled below)
	} else {
		tid = "Unknown tracker";
	}

	// argh
	if (!tid && (hdr.cwtv >> 12) == 0) {
		tid = "Impulse Tracker %d.%02x";
		if (hdr.cmwt > 0x0214) {
			hdr.cwtv = 0x0215;
		} else if (hdr.cwtv > 0x0214) {
			// Patched update of IT 2.14 (0x0215 - 0x0217 == p1 - p3)
			// p4 (as found on modland) adds the ITVSOUND driver, but doesn't seem to change
			// anything as far as file saving is concerned.
			tid = NULL;
			sprintf(song->tracker_id, "Impulse Tracker 2.14p%d", hdr.cwtv - 0x0214);
		}
		//"saved %d time%s", hist, (hist == 1) ? "" : "s"
	}
	if (tid) {
		sprintf(song->tracker_id, tid, (hdr.cwtv & 0xf00) >> 8, hdr.cwtv & 0xff);
	}

	if (modplug) {
		/* The encoding of songs saved by Modplug (and OpenMPT) is dependent
		 * on the current system encoding, which will be Windows-1252 in 99%
		 * of cases. However, some modules made in other (usually Asian) countries
		 * will be encoded in other character sets like Shift-JIS or UHC (Korean).
		 *
		 * There really isn't a good way to detect this aside from some heuristics
		 * (JIS is fairly easy to detect, for example) and honestly it's a bit
		 * more trouble than its really worth to deal with those edge cases when
		 * we can't even display those characters correctly anyway.
		 *
		 *  - paper */
		uint8_t* tmp;

#define CONVERT(x, size) \
	if (!charset_iconv(x, &tmp, CHARSET_WINDOWS1252, CHARSET_CP437)) { \
		strncpy(x, tmp, size); \
		free(tmp); \
	}

		CONVERT(song->title, ARRAY_SIZE(song->title))

		for (n = 0; n < hdr.insnum; n++) {
			song_instrument_t *inst = song->instruments[n + 1];
			if (!inst)
				continue;

			CONVERT(inst->name, ARRAY_SIZE(inst->name))
			CONVERT(inst->filename, ARRAY_SIZE(inst->filename))
		}

		for (n = 0, sample = song->samples + 1; n < hdr.smpnum; n++, sample++) {
			CONVERT(sample->name, ARRAY_SIZE(sample->name))
			CONVERT(sample->filename, ARRAY_SIZE(sample->filename))
		}

		CONVERT(song->message, ARRAY_SIZE(song->message))

#undef CONVERT
	}

//      if (ferror(fp)) {
//              return LOAD_FILE_ERROR;
//      }

	return LOAD_SUCCESS;
}

