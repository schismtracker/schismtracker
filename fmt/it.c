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
#include "charset.h"
#include "slurp.h"
#include "fmt.h"
#include "log.h"
#include "version.h"

#include "player/sndfile.h"
#include "midi.h"

/* --------------------------------------------------------------------- */

struct it_file {
	uint32_t id;                    // 0x4D504D49
	int8_t songname[26];
	uint8_t hilight_minor;
	uint8_t hilight_major;
	uint16_t ordnum;
	uint16_t insnum;
	uint16_t smpnum;
	uint16_t patnum;
	uint16_t cwtv;
	uint16_t cmwt;
	uint16_t flags;
	uint16_t special;
	uint8_t globalvol;
	uint8_t mv;
	uint8_t speed;
	uint8_t tempo;
	uint8_t sep;
	uint8_t pwd;
	uint16_t msglength;
	uint32_t msgoffset;
	uint32_t reserved;
	uint8_t chnpan[64];
	uint8_t chnvol[64];
};

static int it_load_header(struct it_file *hdr, slurp_t *fp)
{
#define LOAD_VALUE(name) do { if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) { return 0; } } while (0)

	LOAD_VALUE(id);
	LOAD_VALUE(songname);
	LOAD_VALUE(hilight_minor);
	LOAD_VALUE(hilight_major);
	LOAD_VALUE(ordnum);
	LOAD_VALUE(insnum);
	LOAD_VALUE(smpnum);
	LOAD_VALUE(patnum);
	LOAD_VALUE(cwtv);
	LOAD_VALUE(cmwt);
	LOAD_VALUE(flags);
	LOAD_VALUE(special);
	LOAD_VALUE(globalvol);
	LOAD_VALUE(mv);
	LOAD_VALUE(speed);
	LOAD_VALUE(tempo);
	LOAD_VALUE(sep);
	LOAD_VALUE(pwd);
	LOAD_VALUE(msglength);
	LOAD_VALUE(msgoffset);
	LOAD_VALUE(reserved);
	LOAD_VALUE(chnpan);
	LOAD_VALUE(chnvol);

#undef LOAD_VALUE

	if (memcmp(&hdr->id, "IMPM", 4))
		return 0;

	hdr->ordnum = bswapLE16(hdr->ordnum);
	hdr->insnum = bswapLE16(hdr->insnum);
	hdr->smpnum = bswapLE16(hdr->smpnum);
	hdr->patnum = bswapLE16(hdr->patnum);
	hdr->cwtv = bswapLE16(hdr->cwtv);
	hdr->cmwt = bswapLE16(hdr->cmwt);
	hdr->flags = bswapLE16(hdr->flags);
	hdr->special = bswapLE16(hdr->special);
	hdr->msglength = bswapLE16(hdr->msglength);
	hdr->msgoffset = bswapLE32(hdr->msgoffset);
	hdr->reserved = bswapLE32(hdr->reserved);

	/* replace NUL bytes with spaces */
	for (int n = 0; n < sizeof(hdr->songname); n++)
		if (!hdr->songname[n])
			hdr->songname[n] = 0x20;

	return 1;
}

static int it_write_header(struct it_file *hdr, disko_t *fp)
{
#define WRITE_VALUE(name) do { disko_write(fp, &hdr->name, sizeof(hdr->name)); } while (0)

	WRITE_VALUE(id);
	WRITE_VALUE(songname);
	WRITE_VALUE(hilight_minor);
	WRITE_VALUE(hilight_major);
	WRITE_VALUE(ordnum);
	WRITE_VALUE(insnum);
	WRITE_VALUE(smpnum);
	WRITE_VALUE(patnum);
	WRITE_VALUE(cwtv);
	WRITE_VALUE(cmwt);
	WRITE_VALUE(flags);
	WRITE_VALUE(special);
	WRITE_VALUE(globalvol);
	WRITE_VALUE(mv);
	WRITE_VALUE(speed);
	WRITE_VALUE(tempo);
	WRITE_VALUE(sep);
	WRITE_VALUE(pwd);
	WRITE_VALUE(msglength);
	WRITE_VALUE(msgoffset);
	WRITE_VALUE(reserved);
	WRITE_VALUE(chnpan);
	WRITE_VALUE(chnvol);

#undef LOAD_VALUE

	return 1;
}

/* --------------------------------------------------------------------- */

int fmt_it_read_info(dmoz_file_t *file, slurp_t *fp)
{
	int n;
	uint32_t para_smp[MAX_SAMPLES];
	struct it_file hdr;
	
	if (!it_load_header(&hdr, fp))
		return 0;

	if (hdr.smpnum >= MAX_SAMPLES)
		return 0;

	slurp_seek(fp, hdr.ordnum, SEEK_CUR);

	slurp_seek(fp, sizeof(uint32_t) * hdr.insnum, SEEK_CUR);
	slurp_read(fp, para_smp, sizeof(uint32_t) * hdr.smpnum);

	uint32_t para_min = ((hdr.special & 1) && hdr.msglength) ? hdr.msgoffset : slurp_length(fp);
	for (n = 0; n < hdr.smpnum; n++) {
		para_smp[n] = bswapLE32(para_smp[n]);
		if (para_smp[n] < para_min)
			para_min = para_smp[n];
	}

	/* try to find a compressed sample and set
	 * the description accordingly */
	file->description = "Impulse Tracker";
	for (n = 0; n < hdr.smpnum; n++) {
		slurp_seek(fp, para_smp[n], SEEK_SET);

		slurp_seek(fp, 18, SEEK_CUR); // skip to flags
		int flags = slurp_getc(fp);
		if (flags == EOF)
			return 0;

		// compressed ?
		if (flags & 8) {
			file->description = "Compressed Impulse Tracker";
			break;
		}
	}

	/*file->extension = str_dup("it");*/
	file->title = strn_dup((const char *)hdr.songname, sizeof(hdr.songname));
	str_rtrim(file->title);
	file->type = TYPE_MODULE_IT;
	return 1;
}

/* --------------------------------------------------------------------- */

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

static void load_it_pattern(song_note_t *note, slurp_t *fp, int rows, uint16_t cwtv)
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

			if (note[chan].effect == FX_SPECIAL && (note[chan].param & 0xf0) == 0xa0 && cwtv < 0x0200) {
				// IT 1.xx does not support high offset command
				note[chan].effect = FX_NONE;
			} else if (note[chan].effect == FX_GLOBALVOLUME && note[chan].param > 0x80 && cwtv >= 0x1000 && cwtv <= 0x1050) {
				// Fix handling of commands V81-VFF in ITs made with old Schism Tracker versions
				// (fixed in commit ab5517d4730d4c717f7ebffb401445679bd30888 - one of the last versions to identify as v0.50)
				note[chan].param = 0x80;
			}

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

int fmt_it_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	struct it_file hdr;
	uint32_t para_smp[MAX_SAMPLES], para_ins[MAX_INSTRUMENTS], para_pat[MAX_PATTERNS], para_min;
	int n;
	int modplug = 0;
	int ignoremidi = 0;
	song_channel_t *channel;
	song_sample_t *sample;
	uint16_t hist = 0; // save history (for IT only)
	const char *tid = NULL;

	if (!it_load_header(&hdr, fp))
		return LOAD_UNSUPPORTED;

	// Screwy limits?
	if (hdr.insnum > MAX_INSTRUMENTS || hdr.smpnum > MAX_SAMPLES
		|| hdr.patnum > MAX_PATTERNS) {
		return LOAD_FORMAT_ERROR;
	}

	strncpy(song->title, (char *)hdr.songname, sizeof(song->title));

	str_rtrim(song->title);

	if (hdr.cmwt < 0x0214 && hdr.cwtv < 0x0214)
		ignoremidi = 1;
	if (hdr.special & 4) {
		/* "reserved" bit, experimentally determined to indicate presence of otherwise-documented row
		highlight information - introduced in IT 2.13. Formerly checked cwtv here, but that's lame :)
		XXX does any tracker save highlight but *not* set this bit? (old Schism versions maybe?) */
		song->row_highlight_minor = hdr.hilight_minor;
		song->row_highlight_major = hdr.hilight_major;
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

	song->initial_global_volume = MIN(hdr.globalvol, 128);
	song->mixing_volume = MIN(hdr.mv, 128);
	song->initial_speed = hdr.speed ? hdr.speed : 6;
	song->initial_tempo = MAX(hdr.tempo, 31);
	song->pan_separation = hdr.sep;

	for (n = 0, channel = song->channels; n < 64; n++, channel++) {
		int pan = hdr.chnpan[n];
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
		channel->volume = MIN(hdr.chnvol[n], 64);
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

	para_min = ((hdr.special & 1) && hdr.msglength) ? hdr.msgoffset : slurp_length(fp);
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
		song->history = mem_calloc(song->histlen, sizeof(*song->history));
		for (size_t i = 0; i < song->histlen; i++) {
			// handle the date
			uint16_t fat_date;
			uint16_t fat_time;

			slurp_read(fp, &fat_date, sizeof(fat_date));
			fat_date = bswapLE16(fat_date);
			slurp_read(fp, &fat_time, sizeof(fat_time));
			fat_time = bswapLE16(fat_time);

			if (fat_date && fat_time) {
				fat_date_time_to_tm(&song->history[i].time, fat_date, fat_time);
				song->history[i].time_valid = 1;
			}

			// now deal with the runtime
			uint32_t run_time;

			slurp_read(fp, &run_time, sizeof(run_time));
			run_time = bswapLE32(run_time);

			song->history[i].runtime = dos_time_to_ms(run_time);
		}
	}
	if (ignoremidi) {
		if (hdr.special & 8) {
			log_appendf(4, " Warning: ignoring embedded MIDI data (CWTV/CMWT is too old)");
			slurp_seek(fp, sizeof(midi_config_t), SEEK_CUR);
		}
		memset(&song->midi_config, 0, sizeof(midi_config_t));
	} else if ((hdr.special & 8) && slurp_tell(fp) + sizeof(midi_config_t) <= slurp_length(fp)) {
		slurp_read(fp, &song->midi_config, sizeof(midi_config_t));
	}
	if (!hist) {
		// berotracker check
		unsigned char modu[4];
		slurp_read(fp, modu, 4);
		if (!memcmp(modu, "MODU", 4))
			tid = "BeRoTracker";
	}

	if ((hdr.special & 1) && hdr.msglength && hdr.msgoffset + hdr.msglength < slurp_length(fp)) {
		int msg_len = MIN(MAX_MESSAGE, hdr.msglength);
		slurp_seek(fp, hdr.msgoffset, SEEK_SET);
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
			
			if (hdr.cmwt >= 0x0200)
				load_it_instrument(NULL, inst, fp);
			else
				load_it_instrument_old(inst, fp);
		}

		for (n = 0, sample = song->samples + 1; n < hdr.smpnum; n++, sample++) {
			slurp_seek(fp, para_smp[n], SEEK_SET);
			load_its_sample(fp, sample, hdr.cwtv);
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
			load_it_pattern(song->patterns[n], fp, rows, hdr.cwtv);
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
		   && hdr.hilight_major == 0 && hdr.hilight_minor == 0
		   && hdr.insnum == 0 && hdr.patnum + 1 == hdr.ordnum
		   && hdr.globalvol == 128 && hdr.mv == 100 && hdr.speed == 1 && hdr.sep == 128 && hdr.pwd == 0
		   && hdr.msglength == 0 && hdr.msgoffset == 0 && hdr.reserved == 0) {
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
		if (!ompt && (memchr(hdr.chnpan, 0xff, 64) == NULL)) {
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
		} else if (hdr.cwtv >= 0x0215 && hdr.cwtv <= 0x0217) {
			tid = NULL;
			const char *versions[] = { "1-2", "3", "4-5" };
			sprintf(song->tracker_id, "Impulse Tracker 2.14p%s", versions[hdr.cwtv - 0x0215]);
		}

		if (hdr.cwtv >= 0x0207 && !song->histlen && hdr.reserved) {
			// Starting from version 2.07, IT stores the total edit
			// time of a module in the "reserved" field
			song->histlen = 1;
			song->history = mem_calloc(1, sizeof(*song->history));

			uint32_t runtime = it_decode_edit_timer(hdr.cwtv, hdr.reserved);
			song->history[0].runtime = dos_time_to_ms(runtime);
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
		char *tmp;

#define CONVERT(x, size)  \
	do { \
		if (!charset_iconv(x, &tmp, CHARSET_WINDOWS1252, CHARSET_CP437, size)) { \
			strncpy(x, tmp, size - 1); \
			tmp[size] = 0; \
			free(tmp); \
		} \
	} while (0)

		CONVERT(song->title, ARRAY_SIZE(song->title));

		for (n = 0; n < hdr.insnum; n++) {
			song_instrument_t *inst = song->instruments[n + 1];
			if (!inst)
				continue;

			CONVERT(inst->name, ARRAY_SIZE(inst->name));
			CONVERT(inst->filename, ARRAY_SIZE(inst->filename));
		}

		for (n = 0, sample = song->samples + 1; n < hdr.smpnum; n++, sample++) {
			CONVERT(sample->name, ARRAY_SIZE(sample->name));
			CONVERT(sample->filename, ARRAY_SIZE(sample->filename));
		}

		CONVERT(song->message, ARRAY_SIZE(song->message));

#undef CONVERT
	}

//      if (ferror(fp)) {
//              return LOAD_FILE_ERROR;
//      }

	return LOAD_SUCCESS;
}

/* ---------------------------------------------------------------------- */
/* saving routines */

enum {
	WARN_ADLIB,

	MAX_WARN,
};

const char *it_warnings[] = {
	[WARN_ADLIB] = "AdLib samples",
};

// NOBODY expects the Spanish Inquisition!
static void save_it_pattern(disko_t *fp, song_note_t *pat, int patsize)
{
	song_note_t *noteptr = pat;
	song_note_t lastnote[64] = {0};
	uint8_t initmask[64] = {0};
	uint8_t lastmask[64];
	uint16_t pos = 0;
	uint8_t data[65536];

	memset(lastmask, 0xff, 64);

	for (int row = 0; row < patsize; row++) {
		for (int chan = 0; chan < 64; chan++, noteptr++) {
			uint8_t m = 0;  // current mask
			int vol = -1;
			unsigned int note = noteptr->note;
			uint8_t effect = noteptr->effect, param = noteptr->param;

			if (note) {
				m |= 1;
				if (note < 0x80)
					note--;
			}
			if (noteptr->instrument) m |= 2;
			switch (noteptr->voleffect) {
			default:                                                       break;
			case VOLFX_VOLUME:         vol = MIN(noteptr->volparam, 64);       break;
			case VOLFX_FINEVOLUP:      vol = MIN(noteptr->volparam,  9) +  65; break;
			case VOLFX_FINEVOLDOWN:    vol = MIN(noteptr->volparam,  9) +  75; break;
			case VOLFX_VOLSLIDEUP:     vol = MIN(noteptr->volparam,  9) +  85; break;
			case VOLFX_VOLSLIDEDOWN:   vol = MIN(noteptr->volparam,  9) +  95; break;
			case VOLFX_PORTADOWN:      vol = MIN(noteptr->volparam,  9) + 105; break;
			case VOLFX_PORTAUP:        vol = MIN(noteptr->volparam,  9) + 115; break;
			case VOLFX_PANNING:        vol = MIN(noteptr->volparam, 64) + 128; break;
			case VOLFX_VIBRATODEPTH:   vol = MIN(noteptr->volparam,  9) + 203; break;
			case VOLFX_VIBRATOSPEED:   vol = 203;                         break;
			case VOLFX_TONEPORTAMENTO: vol = MIN(noteptr->volparam,  9) + 193; break;
			}
			if (vol != -1) m |= 4;
			csf_export_s3m_effect(&effect, &param, 1);
			if (effect || param) m |= 8;
			if (!m) continue;

			if (m & 1) {
				if ((note == lastnote[chan].note) && (initmask[chan] & 1)) {
					m &= ~1;
					m |= 0x10;
				} else {
					lastnote[chan].note = note;
					initmask[chan] |= 1;
				}
			}

			if (m & 2) {
				if ((noteptr->instrument == lastnote[chan].instrument) && (initmask[chan] & 2)) {
					m &= ~2;
					m |= 0x20;
				} else {
					lastnote[chan].instrument = noteptr->instrument;
					initmask[chan] |= 2;
				}
			}

			if (m & 4) {
				if ((vol == lastnote[chan].volparam) && (initmask[chan] & 4)) {
					m &= ~4;
					m |= 0x40;
				} else {
					lastnote[chan].volparam = vol;
					initmask[chan] |= 4;
				}
			}

			if (m & 8) {
				if ((effect == lastnote[chan].effect) && (param == lastnote[chan].param)
				    && (initmask[chan] & 8)) {
					m &= ~8;
					m |= 0x80;
				} else {
					lastnote[chan].effect = effect;
					lastnote[chan].param = param;
					initmask[chan] |= 8;
				}
			}

			if (m == lastmask[chan]) {
				data[pos++] = chan + 1;
			} else {
				lastmask[chan] = m;
				data[pos++] = (chan + 1) | 0x80;
				data[pos++] = m;
			}

			if (m & 1) data[pos++] = note;
			if (m & 2) data[pos++] = noteptr->instrument;
			if (m & 4) data[pos++] = vol;
			if (m & 8) { data[pos++] = effect; data[pos++] = param; }
		} // end channel
		data[pos++] = 0;
	} // end row

	// write the data to the file (finally!)
	uint16_t h[4] = {0};
	h[0] = bswapLE16(pos);
	h[1] = bswapLE16(patsize);
	// h[2] and h[3] are meaningless
	disko_write(fp, &h, 8);
	disko_write(fp, data, pos);
}

int fmt_it_save_song(disko_t *fp, song_t *song)
{
	struct it_file hdr = {0};
	int n;
	int nord, nins, nsmp, npat;
	int msglen = strlen(song->message);
	uint32_t para_ins[256], para_smp[256], para_pat[256];
	// how much extra data is stuffed between the parapointers and the rest of the file
	// (2 bytes for edit history length, and 8 per entry including the current session)
	uint32_t extra = 2 + 8 * song->histlen + 8;
	// warnings for unsupported features
	uint32_t warn = 0;

	// TODO complain about nonstandard stuff? or just stop saving it to begin with

	/* IT always saves at least two orders -- and requires an extra order at the end (which gets chopped!)
	However, the loader refuses to load files with too much data in the orderlist, so in the pathological
	case where order 255 has data, writing an extra 0xFF at the end will result in a file that can't be
	loaded back (for now). Eventually this can be fixed, but at least for a while it's probably a great
	idea not to save things that other versions won't load. */
	nord = csf_get_num_orders(song);
	nord = CLAMP(nord + 1, 2, MAX_ORDERS);

	nins = csf_get_num_instruments(song);
	nsmp = csf_get_num_samples(song);

	// IT always saves at least one pattern.
	npat = csf_get_num_patterns(song);
	if (!npat)
		npat = 1;

	hdr.id = bswapLE32(0x4D504D49); // IMPM
	strncpy((char *) hdr.songname, song->title, 25);
	hdr.songname[25] = 0; // why ?
	hdr.hilight_major = song->row_highlight_major;
	hdr.hilight_minor = song->row_highlight_minor;
	hdr.ordnum = bswapLE16(nord);
	hdr.insnum = bswapLE16(nins);
	hdr.smpnum = bswapLE16(nsmp);
	hdr.patnum = bswapLE16(npat);
	// No one else seems to be using the cwtv's tracker id number, so I'm gonna take 1. :)
	hdr.cwtv = bswapLE16(0x1000 | ver_cwtv); // cwtv 0xtxyy = tracker id t, version x.yy
	// compat:
	//     really simple IT files = 1.00 (when?)
	//     "normal" = 2.00
	//     vol col effects = 2.08
	//     pitch wheel depth = 2.13
	//     embedded midi config = 2.13
	//     row highlight = 2.13 (doesn't necessarily affect cmwt)
	//     compressed samples = 2.14
	//     instrument filters = 2.17
	hdr.cmwt = bswapLE16(0x0214);   // compatible with IT 2.14
	for (n = 1; n < nins; n++) {
		song_instrument_t *i = song->instruments[n];
		if (!i) continue;
		if (i->flags & ENV_FILTER) {
			hdr.cmwt = bswapLE16(0x0217);
			break;
		}
	}

	hdr.flags = 0;
	hdr.special = 2 | 4;            // 2 = edit history, 4 = row highlight

	if (!(song->flags & SONG_NOSTEREO))    hdr.flags |= 1;
	if (song->flags & SONG_INSTRUMENTMODE) hdr.flags |= 4;
	if (song->flags & SONG_LINEARSLIDES)   hdr.flags |= 8;
	if (song->flags & SONG_ITOLDEFFECTS)   hdr.flags |= 16;
	if (song->flags & SONG_COMPATGXX)      hdr.flags |= 32;
	if (midi_flags & MIDI_PITCHBEND) {
		hdr.flags |= 64;
		hdr.pwd = midi_pitch_depth;
	}
	if (song->flags & SONG_EMBEDMIDICFG) {
		hdr.flags |= 128;
		hdr.special |= 8;
		extra += sizeof(midi_config_t);
	}
	hdr.flags = bswapLE16(hdr.flags);
	if (msglen) {
		hdr.special |= 1;
		msglen++;
	}
	hdr.special = bswapLE16(hdr.special);

	// 16+ = reserved (always off?)
	hdr.globalvol = song->initial_global_volume;
	hdr.mv = song->mixing_volume;
	hdr.speed = song->initial_speed;
	hdr.tempo = song->initial_tempo;
	hdr.sep = song->pan_separation;
	if (msglen) {
		hdr.msgoffset = bswapLE32(extra + 0xc0 + nord + 4 * (nins + nsmp + npat));
		hdr.msglength = bswapLE16(msglen);
	}
	hdr.reserved = bswapLE32(ver_reserved);

	for (n = 0; n < 64; n++) {
		hdr.chnpan[n] = ((song->channels[n].flags & CHN_SURROUND)
				 ? 100 : (song->channels[n].panning / 4));
		hdr.chnvol[n] = song->channels[n].volume;
		if (song->channels[n].flags & CHN_MUTE)
			hdr.chnpan[n] += 128;
	}

	it_write_header(&hdr, fp);
	disko_write(fp, song->orderlist, nord);

	// we'll get back to these later
	disko_write(fp, para_ins, 4*nins);
	disko_write(fp, para_smp, 4*nsmp);
	disko_write(fp, para_pat, 4*npat);


	uint16_t h;

	// edit history (see scripts/timestamp.py)
	// Should™ be fully compatible with Impulse Tracker.

	// item count
	h = bswapLE16(song->histlen + 1);
	disko_write(fp, &h, 2);

	// old data
	for (size_t i = 0; i < song->histlen; i++) {
		uint16_t fat_date = 0, fat_time = 0;

		if (song->history[i].time_valid)
			tm_to_fat_date_time(&song->history[i].time, &fat_date, &fat_time);

		fat_date = bswapLE16(fat_date);
		disko_write(fp, &fat_date, sizeof(fat_date));
		fat_time = bswapLE16(fat_time);
		disko_write(fp, &fat_time, sizeof(fat_time));

		uint32_t run_time = bswapLE32(ms_to_dos_time(song->history[i].runtime));
		disko_write(fp, &run_time, sizeof(run_time));
	}

	{
		uint16_t fat_date, fat_time;

		tm_to_fat_date_time(&song->editstart.time, &fat_date, &fat_time);

		fat_date = bswapLE16(fat_date);
		disko_write(fp, &fat_date, sizeof(fat_date));
		fat_time = bswapLE16(fat_time);
		disko_write(fp, &fat_time, sizeof(fat_time));

		uint32_t ticks = it_get_song_elapsed_dos_time(song);
		ticks = bswapLE32(ticks);
		disko_write(fp, &ticks, sizeof(ticks));
	}

	// here comes MIDI configuration
	// here comes MIDI configuration
	// right down MIDI configuration lane
	if (song->flags & SONG_EMBEDMIDICFG)
		disko_write(fp, &song->midi_config, sizeof(song->midi_config));

	disko_write(fp, song->message, msglen);

	// instruments, samples, and patterns
	for (n = 0; n < nins; n++) {
		para_ins[n] = disko_tell(fp);
		para_ins[n] = bswapLE32(para_ins[n]);
		save_iti_instrument(fp, song, song->instruments[n + 1], 0);
	}

	for (n = 0; n < nsmp; n++) {
		// the sample parapointers are byte-swapped later
		para_smp[n] = disko_tell(fp);
		save_its_header(fp, song->samples + n + 1);
	}

	for (n = 0; n < npat; n++) {
		if (csf_pattern_is_empty(song, n)) {
			para_pat[n] = 0;
		} else {
			para_pat[n] = disko_tell(fp);
			para_pat[n] = bswapLE32(para_pat[n]);
			save_it_pattern(fp, song->patterns[n], song->pattern_size[n]);
		}
	}

	// sample data
	for (n = 0; n < nsmp; n++) {
		unsigned int tmp, op;
		song_sample_t *smp = song->samples + (n + 1);

		// Always save the data pointer, even if there's not actually any data being pointed to
		op = disko_tell(fp);
		tmp = bswapLE32(op);
		disko_seek(fp, para_smp[n]+0x48, SEEK_SET);
		disko_write(fp, &tmp, 4);
		disko_seek(fp, op, SEEK_SET);
		if (smp->data)
			csf_write_sample(fp, smp, SF_LE | SF_PCMS
					| ((smp->flags & CHN_16BIT) ? SF_16 : SF_8)
					| ((smp->flags & CHN_STEREO) ? SF_SS : SF_M),
					UINT32_MAX);
		// done using the pointer internally, so *now* swap it
		para_smp[n] = bswapLE32(para_smp[n]);

		if (smp->flags & CHN_ADLIB)
			warn |= (1 << WARN_ADLIB);
	}

	for (int i = 0; i < ARRAY_SIZE(it_warnings); i++)
		if (warn & (1 << i))
			log_appendf(4, " Warning: %s unsupported in IT format", it_warnings[i]);

	// rewrite the parapointers
	disko_seek(fp, 0xc0 + nord, SEEK_SET);
	disko_write(fp, para_ins, 4*nins);
	disko_write(fp, para_smp, 4*nsmp);
	disko_write(fp, para_pat, 4*npat);

	return SAVE_SUCCESS;
}
