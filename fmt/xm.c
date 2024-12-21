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

#include "it.h" // needed for get_effect_char (purely informational)
#include "log.h"

#include "player/sndfile.h"
#include "player/tables.h"

// gloriously stolen from xmp
struct xm_file_header {
	uint8_t id[17];         // ID text: "Extended module: "
	uint8_t name[20];       // Module name, padded with zeroes
	uint8_t doseof;         // 0x1a
	uint8_t tracker[20];    // Tracker name
	uint16_t version;       // Version number, minor-major
	uint32_t headersz;      // Header size
	uint16_t songlen;       // Song length (in patten order table)
	uint16_t restart;       // Restart position
	uint16_t channels;      // Number of channels (2,4,6,8,10,...,32)
	uint16_t patterns;      // Number of patterns (max 256)
	uint16_t instruments;   // Number of instruments (max 128)
	uint16_t flags;         // bit 0: 0=Amiga freq table, 1=Linear
	uint16_t tempo;         // Default tempo
	uint16_t bpm;           // Default BPM
};

/* --------------------------------------------------------------------- */

static int read_header_xm(struct xm_file_header *hdr, slurp_t *fp)
{
#define READ_VALUE(name) \
	if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) return 0

	READ_VALUE(id);
	READ_VALUE(name);
	READ_VALUE(doseof);
	READ_VALUE(tracker);
	READ_VALUE(version);
	READ_VALUE(headersz);
	READ_VALUE(songlen);
	READ_VALUE(restart);
	READ_VALUE(channels);
	READ_VALUE(patterns);
	READ_VALUE(instruments);
	READ_VALUE(flags);
	READ_VALUE(tempo);
	READ_VALUE(bpm);

#undef READ_VALUE

	if (memcmp(hdr->id, "Extended Module: ", sizeof(hdr->id))
		|| hdr->doseof != 0x1a)
		return 0;

	/* now byteswap */
	hdr->version = bswapLE16(hdr->version);
	hdr->headersz = bswapLE32(hdr->headersz);
	hdr->songlen = bswapLE16(hdr->songlen);
	hdr->restart = bswapLE16(hdr->restart);
	hdr->channels = bswapLE16(hdr->channels);
	hdr->patterns = bswapLE16(hdr->patterns);
	hdr->instruments = bswapLE16(hdr->instruments);
	hdr->flags = bswapLE16(hdr->flags);
	hdr->tempo = bswapLE16(hdr->tempo);
	hdr->bpm = bswapLE16(hdr->bpm);

	if (hdr->channels > MAX_CHANNELS)
		return 0;

	return 1;
}

/* --------------------------------------------------------------------- */

int fmt_xm_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct xm_file_header hdr;

	if (!read_header_xm(&hdr, fp))
		return 0;

	file->description = "Fast Tracker 2 Module";
	file->type = TYPE_MODULE_XM;
	/*file->extension = str_dup("xm");*/
	file->title = strn_dup((const char *)hdr.name, sizeof(hdr.name));
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

static uint8_t autovib_import[8] = {
	VIB_SINE, VIB_SQUARE,
	VIB_RAMP_DOWN, // actually ramp up
	VIB_RAMP_DOWN, VIB_RANDOM,
	// default to sine
	VIB_SINE, VIB_SINE, VIB_SINE,
};



static void load_xm_patterns(song_t *song, struct xm_file_header *hdr, slurp_t *fp)
{
	int pat, row, chan;
	uint32_t patlen;
	uint8_t b;
	uint16_t rows;
	uint16_t bytes;
	size_t end; // should be same data type as slurp_t's length
	song_note_t *note;
	unsigned int lostpat = 0;
	unsigned int lostfx = 0;

	for (pat = 0; pat < hdr->patterns; pat++) {
		slurp_read(fp, &patlen, 4); // = 8/9
		patlen = bswapLE32(patlen);
		b = slurp_getc(fp); // = 0
		if (hdr->version == 0x0102) {
			rows = slurp_getc(fp) + 1;
			patlen++; // fake it so that alignment works properly.
		} else {
			slurp_read(fp, &rows, 2);
			rows = bswapLE16(rows);
		}
		slurp_read(fp, &bytes, 2);
		bytes = bswapLE16(bytes); // if 0, pattern is empty

		slurp_seek(fp, patlen - 9, SEEK_CUR); // probably a no-op

		if (!rows)
			continue;

		if (pat >= MAX_PATTERNS) {
			if (bytes)
				lostpat++;
			slurp_seek(fp, bytes, SEEK_CUR);
			continue;
		}

		note = song->patterns[pat] = csf_allocate_pattern(rows);
		song->pattern_size[pat] = song->pattern_alloc_size[pat] = rows;

		if (!bytes)
			continue;

		// hack to avoid having to count bytes when reading
		end = slurp_tell(fp) + bytes;
		end = MIN(end, slurp_length(fp));

		for (row = 0; row < rows; row++, note += MAX_CHANNELS - hdr->channels) {
			for (chan = 0; slurp_tell(fp) < end && chan < hdr->channels; chan++, note++) {
				b = slurp_getc(fp);
				if (b & 128) {
					if (b & 1) note->note = slurp_getc(fp);
					if (b & 2) note->instrument = slurp_getc(fp);
					if (b & 4) note->volparam = slurp_getc(fp);
					if (b & 8) note->effect = slurp_getc(fp);
					if (b & 16) note->param = slurp_getc(fp);
				} else {
					note->note = b;
					note->instrument = slurp_getc(fp);
					note->volparam = slurp_getc(fp);
					note->effect = slurp_getc(fp);
					note->param = slurp_getc(fp);
				}
				// translate everything
				if (note->note > 0 && note->note < 97) {
					note->note += 12;
				} else if (note->note == 97) {
					/* filter out instruments on noteoff;
					 * this is what IT's importer does, because
					 * hanging the note is *definitely* not
					 * intended behavior
					 *
					 * see: MPT test case noteoff3.it */
					note->note = NOTE_OFF;
					note->instrument = 0;
				} else {
					note->note = NOTE_NONE;
				}

				if (note->effect || note->param)
					csf_import_mod_effect(note, 1);
				if (note->instrument == 0xff)
					note->instrument = 0;

				// now that the mundane stuff is over with... NOW IT'S TIME TO HAVE SOME FUN!

				// the volume column is initially imported as "normal" effects, juggled around
				// in order to make it more IT-like, and then converted into volume-effects

				/* IT puts all volume column effects into the effect column if there's not an
				effect there already; in the case of two set-volume effects, the one in the
				effect column takes precedence.
				set volume with values > 64 are clipped to 64
				pannings are imported as S8x, unless there's an effect in which case it's
				translated to a volume-column panning value.
				volume and panning slides with zero value (+0, -0, etc.) still translate to
				an effect -- even though volslides don't have effect memory in FT2. */

				switch (note->volparam >> 4) {
				case 5: // 0x50 = volume 64, 51-5F = nothing
					if (note->volparam == 0x50) {
				case 1: case 2:
				case 3: case 4: // Set volume Value-$10
						note->voleffect = FX_VOLUME;
						note->volparam -= 0x10;
						break;
					} // NOTE: falls through from case 5 when vol != 0x50
				case 0: // Do nothing
					note->voleffect = FX_NONE;
					note->volparam = 0;
					break;
				case 6: // Volume slide down
					note->volparam &= 0xf;
					if (note->volparam)
						note->voleffect = FX_VOLUMESLIDE;
					break;
				case 7: // Volume slide up
					note->volparam = (note->volparam & 0xf) << 4;
					if (note->volparam)
						note->voleffect = FX_VOLUMESLIDE;
					break;
				case 8: // Fine volume slide down
					note->volparam &= 0xf;
					if (note->volparam) {
						if (note->volparam == 0xf)
							note->volparam = 0xe; // DFF is fine slide up...
						note->volparam |= 0xf0;
						note->voleffect = FX_VOLUMESLIDE;
					}
					break;
				case 9: // Fine volume slide up
					note->volparam = (note->volparam & 0xf) << 4;
					if (note->volparam) {
						note->volparam |= 0xf;
						note->voleffect = FX_VOLUMESLIDE;
					}
					break;
				case 10: // Set vibrato speed
					/* ARGH. this doesn't actually CAUSE vibrato - it only sets the value!
					i don't think there's a way to handle this correctly and sanely, so
					i'll just do what impulse tracker and mpt do...
					(probably should write a warning saying the song might not be
					played correctly) */
					note->volparam = (note->volparam & 0xf) << 4;
					note->voleffect = FX_VIBRATO;
					break;
				case 11: // Vibrato
					note->volparam &= 0xf;
					note->voleffect = FX_VIBRATO;
					break;
				case 12: // Set panning
					note->voleffect = FX_SPECIAL;
					note->volparam = 0x80 | (note->volparam & 0xf);
					break;
				case 13: // Panning slide left
					// in FT2, <0 sets the panning to far left on the SECOND tick
					// this is "close enough" (except at speed 1)
					note->volparam &= 0xf;
					if (note->volparam) {
						note->volparam <<= 4;
						note->voleffect = FX_PANNINGSLIDE;
					} else {
						note->volparam = 0x80;
						note->voleffect = FX_SPECIAL;
					}
					break;
				case 14: // Panning slide right
					note->volparam &= 0xf;
					if (note->volparam)
						note->voleffect = FX_PANNINGSLIDE;
					break;
				case 15: // Tone porta
					note->volparam = (note->volparam & 0xf) << 4;
					note->voleffect = FX_TONEPORTAMENTO;
					break;
				}

				if (note->effect == FX_KEYOFF && note->param == 0) {
					// FT2 ignores notes and instruments next to a K00
					note->note = NOTE_NONE;
					note->instrument = 0;
				} else if (note->note == NOTE_OFF && note->effect == FX_SPECIAL
					   && (note->param >> 4) == 0xd) {
					// note off with a delay ignores the note off, and also
					// ignores set-panning (but not other effects!)
					// (actually the other vol. column effects happen on the
					// first tick with ft2, but this is "close enough" i think)
					note->note = NOTE_NONE;
					note->instrument = 0;
					// note: haven't fixed up volumes yet
					if (note->voleffect == FX_PANNING) {
						note->voleffect = FX_NONE;
						note->volparam = 0;
						note->effect = FX_NONE;
						note->param = 0;
					}
				}

				if (note->effect == FX_NONE && note->voleffect != FX_NONE) {
					// put the lotion in the basket
					swap_effects(note);
				} else if (note->effect == note->voleffect) {
					// two of the same kind of effect => ignore the volume column
					// (note that ft2 behaves VERY strangely with Mx + 3xx combined --
					// but i'll ignore that nonsense and just go by xm.txt here because
					// it's easier :)
					note->voleffect = note->volparam = 0;
				}
				if (note->effect == FX_VOLUME) {
					// try to move set-volume into the volume column
					swap_effects(note);
				}
				// now try to rewrite the volume column, if it's not possible then see if we
				// can do so after swapping them.
				// this is a terrible hack -- don't write code like this, kids :)
				int n;
				for (n = 0; n < 4; n++) {
					// (n >> 1) will be 0/1, indicating our desire to j... j... jam it in
					if (convert_voleffect_of(note, n >> 1)) {
						n = 5; // it'd be nice if c had a for...else like python
						break;
					}
					// nope that didn't work, switch them around
					swap_effects(note);
				}
				if (n < 5) {
					// Need to throw one out.
					if (effect_weight[note->voleffect] > effect_weight[note->effect]) {
						note->effect = note->voleffect;
						note->param = note->volparam;
					}
					//log_appendf(4, "Warning: pat%u row%u chn%u: lost effect %c%02X",
					//      pat, row, chan + 1, get_effect_char(note->voleffect), note->volparam);
					note->voleffect = note->volparam = 0;
					lostfx++;
				}

				/* some XM effects that schism probably won't handle decently:
				0xy / Jxy
				  - this one is *totally* screwy, see milkytracker source for details :)
				    (NOT documented -- in fact, all the documentation claims that it should
				    simply play note -> note+x -> note+y -> note like any other tracker, but
				    that sure isn't what FT2 does...)
				Axy / Dxy
				  - it's probably not such a good idea to move these between the volume and
				    effect column, since there's a chance it might screw stuff up since the
				    volslides don't share memory (in either .it or .xm) -- e.g.
					... .. .. DF0
					... .. .. D04
					... .. .. D00
				    is quite different from
					... .. .. DF0
					... .. D4 .00
					... .. .. D00
				    But oh well. Works "enough" for now.
				    [Note: IT doesn't even try putting volslide into the volume column.]
				E6x / SBx
				  - ridiculously broken; it screws up the pattern break row if E60 isn't at
				    the start of the pattern -- this is fairly well known by FT2 users, but
				    curiously absent from its "known bugs" list
				E9x / Q0x
				  - actually E9x isn't like Q0x at all... it's really stupid, I give up.
				    hope no one wants to listen to XM files with retrig.
				ECx / SCx
				  - doesn't actually CUT the note, it just sets volume to zero at tick x
				    (this is documented) */
			}
		}
	}

	if (lostfx)
		log_appendf(4, " Warning: %u effect%s dropped", lostfx, lostfx == 1 ? "" : "s");

	if (lostpat)
		log_appendf(4, " Warning: Too many patterns in song (%u skipped)", lostpat);
}

static void load_xm_samples(song_sample_t *first, int total, slurp_t *fp)
{
	song_sample_t *smp = first;
	int ns;

	// dontyou: 20 samples starting at 26122
	// trnsmix: 31 samples starting at 61946
	for (ns = 0; ns < total; ns++, smp++) {
		if (!smp->length)
			continue;
		if (smp->flags & CHN_16BIT) {
			smp->length >>= 1;
			smp->loop_start >>= 1;
			smp->loop_end >>= 1;
		}
		if (smp->flags & CHN_STEREO) {
			smp->length >>= 1;
			smp->loop_start >>= 1;
			smp->loop_end >>= 1;
		}
		if (smp->adlib_bytes[0] != 0xAD) {
			csf_read_sample(smp, SF_LE | ((smp->flags & CHN_STEREO) ? SF_SS : SF_M) | SF_PCMD | ((smp->flags & CHN_16BIT) ? SF_16 : SF_8), fp);
		} else {
			smp->adlib_bytes[0] = 0;
			csf_read_sample(smp, SF_8 | SF_M | SF_LE | SF_PCMD16, fp);
		}
	}
}

// Volume/panning envelope loop fix
// FT2 leaves out the final tick of the envelope loop causing a slight discrepancy when loading XI instruments directly
// from an XM file into Schism
// Works by adding a new end node one tick behind the previous loop end by linearly interpolating (or selecting 
// an existing node there).
// Runs generally the same for either type of envelope (vol/pan), pointed to by s_env.
static void fix_xm_envelope_loop(song_envelope_t *s_env, int sustain_flag)
{
	int n;
	float v;

	if (s_env->ticks[s_env->loop_end - 1] == s_env->ticks[s_env->loop_end] - 1) {
		// simplest case: prior node is one tick behind already, set envelope end index
		s_env->loop_end--;
		return;
	}

	// shift each node from loop_end right one index to insert a new node
	// on first iteration n is one more that existing # of nodes
	for (n = s_env->nodes; n > s_env->loop_end; n--) {
		s_env->ticks[n] = s_env->ticks[n - 1];
		s_env->values[n] = s_env->values[n - 1];
	}

	// increment the node count for the previous shift
	s_env->nodes++;

	// define the new node at the previous loop_end index, one tick behind and interpolated correctly
	s_env->ticks[s_env->loop_end]--;
	v = (float)(s_env->values[s_env->loop_end + 1] - s_env->values[s_env->loop_end - 1]);
	v *= (float)(s_env->ticks[s_env->loop_end] - s_env->ticks[s_env->loop_end - 1]);
	v /= (float)(s_env->ticks[s_env->loop_end + 1] - s_env->ticks[s_env->loop_end - 1]);
	// alter the float so it rounds to the nearest integer when type casted
	v = (v >= 0.0f) ? v + 0.5f : v - 0.5f;
	s_env->values[s_env->loop_end] = (uint8_t)v + s_env->values[s_env->loop_end - 1];

	// adjust the sustain loop as needed
	if (sustain_flag && s_env->sustain_start >= s_env->loop_end) {
		s_env->sustain_start++;
		s_env->sustain_end++;
	}
}

enum {
	ID_CONFIRMED = 0x01, // confirmed with inst/sample header sizes
	ID_FT2GENERIC = 0x02, // "FastTracker v2.00", but fasttracker has NOT been ruled out
	ID_OLDMODPLUG = 0x04, // "FastTracker v 2.00"
	ID_OTHER = 0x08, // something we don't know, testing for digitrakker.
	ID_FT2CLONE = 0x10, // NOT FT2: itype changed between instruments, or \0 found in song title
	ID_MAYBEMODPLUG = 0x20, // some FT2-ish thing, possibly MPT.
	ID_DIGITRAK = 0x40, // probably digitrakker
	ID_UNKNOWN = 0x80 | ID_CONFIRMED, // ?????
};

// TODO: try to identify packers (boobiesqueezer?)

// this also does some tracker detection
// return value is the number of samples that need to be loaded later (for old xm files)
static int load_xm_instruments(song_t *song, struct xm_file_header *hdr, slurp_t *fp)
{
	int n, ni, ns;
	int abssamp = 1; // "real" sample
	int32_t ihdr, shdr; // instrument/sample header size (yes these should be signed)
	uint8_t b;
	uint16_t w;
	uint32_t d;
	int detected;
	int itype = -1;
	uint8_t srsvd_or = 0; // bitwise-or of all sample reserved bytes

	if (strncmp(song->tracker_id, "FastTracker ", 12) == 0) {
		if (hdr->headersz == 276 && strncmp(song->tracker_id + 12, "v2.00   ", 8) == 0) {
			detected = ID_FT2GENERIC | ID_MAYBEMODPLUG;
			/* there is very little change between different versions of FT2, making it
			 * very difficult (maybe even impossible) to detect them, so here we just
			 * say it's either FT2 or a compatible tracker */
			strcpy(song->tracker_id + 12, "2 or compatible");
		} else if (strncmp(song->tracker_id + 12, "v 2.00  ", 8) == 0) {
			/* alpha and beta are handled later */
			detected = ID_OLDMODPLUG | ID_CONFIRMED;
			strcpy(song->tracker_id, "ModPlug Tracker 1.0");
		} else {
			// definitely NOT FastTracker, so let's clear up that misconception
			detected = ID_UNKNOWN;
		}
	} else if (strncmp(song->tracker_id, "*Converted ", 11) == 0 || strspn(song->tracker_id, " ") == 20) {
		// this doesn't catch any cases where someone typed something into the field :(
		detected = ID_OTHER | ID_DIGITRAK;
	} else {
		detected = ID_OTHER;
	}

	// FT2 pads the song title with spaces, some other trackers don't
	if (detected & ID_FT2GENERIC && memchr(song->title, '\0', 20) != NULL)
		detected = ID_FT2CLONE | ID_MAYBEMODPLUG;

	for (ni = 1; ni <= hdr->instruments; ni++) {
		int vtype, vsweep, vdepth, vrate;
		song_instrument_t *ins;
		uint16_t nsmp;

		slurp_read(fp, &ihdr, 4);
		ihdr = bswapLE32(ihdr);

		if (ni >= MAX_INSTRUMENTS) {
			// TODO: try harder
			log_appendf(4, " Warning: Too many instruments in file");
			break;
		}
		song->instruments[ni] = ins = csf_allocate_instrument();

		slurp_read(fp, ins->name, 22);
		ins->name[22] = '\0';
		if ((detected & ID_DIGITRAK) && memchr(ins->name, '\0', 22) != NULL)
			detected &= ~ID_DIGITRAK;

		b = slurp_getc(fp);
		if (itype == -1) {
			itype = b;
		} else if (itype != b && (detected & ID_FT2GENERIC)) {
			// FT2 writes some random junk for the instrument type field,
			// but it's always the SAME junk for every instrument saved.
			detected = (detected & ~ID_FT2GENERIC) | ID_FT2CLONE | ID_MAYBEMODPLUG;
		}
		slurp_read(fp, &nsmp, 2);
		nsmp = bswapLE16(nsmp);
		slurp_read(fp, &shdr, 4);
		shdr = bswapLE32(shdr);

		if (detected == ID_OLDMODPLUG) {
			detected = ID_CONFIRMED;
			if (ihdr == 245) {
				strcat(song->tracker_id, " alpha");
			} else if (ihdr == 263) {
				strcat(song->tracker_id, " beta");
			} else {
				// WEIRD!!
				detected = ID_UNKNOWN;
			}
		}

		if (!nsmp) {
			// lucky day! it's pretty easy to identify tracker if there's a blank instrument
			if (!(detected & ID_CONFIRMED)) {
				if ((detected & ID_MAYBEMODPLUG) && ihdr == 263 && shdr == 0) {
					detected = ID_CONFIRMED;
					strcpy(song->tracker_id, "Modplug Tracker");
				} else if ((detected & ID_DIGITRAK) && ihdr != 29) {
					detected &= ~ID_DIGITRAK;
				} else if ((detected & (ID_FT2CLONE | ID_FT2GENERIC)) && ihdr != 33) {
					// Sure isn't FT2.
					// note: FT2 NORMALLY writes shdr=40 for all samples, but sometimes it
					// just happens to write random garbage there instead. surprise!
					detected = ID_UNKNOWN;
				}
			}
			// some adjustment hack from xmp.
			slurp_seek(fp, ihdr - 33, SEEK_CUR);
			continue;
		}

		for (n = 0; n < 12; n++)
			ins->note_map[n] = n + 1;
		for (; n < 96 + 12; n++) {
			ins->note_map[n] = n + 1;
			ins->sample_map[n] = slurp_getc(fp) + abssamp;
		}
		for (; n < 120; n++)
			ins->note_map[n] = n + 1;

		// envelopes. XM stores this in a hilariously bad format
		const struct {
			song_envelope_t *env;
			uint32_t envflag;
			uint32_t envsusloopflag;
			uint32_t envloopflag;
		} envs[] = {
			{&ins->vol_env, ENV_VOLUME,  ENV_VOLSUSTAIN, ENV_VOLLOOP},
			{&ins->pan_env, ENV_PANNING, ENV_PANSUSTAIN, ENV_PANLOOP},
		};

		for (int i = 0; i < ARRAY_SIZE(envs); i++) {
			uint16_t prevtick;
			for (n = 0; n < 12; n++) {
				slurp_read(fp, &w, 2); // tick
				w = bswapLE16(w);
				if (n > 0 && w < prevtick & !(w & 0xFF00)) {
					// libmikmod code says: "Some broken XM editing program will only save the low byte of the position
					// value. Try to compensate by adding the missing high byte."
					// Note: MPT 1.07's XI instrument saver omitted the high byte of envelope nodes.
					// This might be the source for some broken envelopes in IT and XM files.
					w |= (prevtick & 0xFF00U);
					if (w < prevtick)
						w += 0x100;
				}
				envs[i].env->ticks[n] = prevtick = w;
				slurp_read(fp, &w, 2); // value
				w = bswapLE16(w);
				envs[i].env->values[n] = MIN(w, 64);
			}
		}
		for (int i = 0; i < ARRAY_SIZE(envs); i++) {
			b = slurp_getc(fp);
			envs[i].env->nodes = CLAMP(b, 2, 12);
		}
		for (int i = 0; i < ARRAY_SIZE(envs); i++) {
			envs[i].env->sustain_start = envs[i].env->sustain_end = slurp_getc(fp);
			envs[i].env->loop_start = slurp_getc(fp);
			envs[i].env->loop_end = slurp_getc(fp);
		}
		for (int i = 0; i < ARRAY_SIZE(envs); i++) {
			b = slurp_getc(fp);
			if ((b & 1) && envs[i].env->nodes > 0) ins->flags |= envs[i].envflag;
			if (b & 2) ins->flags |= envs[i].envsusloopflag;
			if (b & 4) ins->flags |= envs[i].envloopflag;
		}

		vtype = autovib_import[slurp_getc(fp) & 0x7];
		vsweep = slurp_getc(fp);
		vdepth = slurp_getc(fp);
		vdepth = MIN(vdepth, 32);
		vrate = slurp_getc(fp);
		vrate = MIN(vrate, 64);

		/* translate the sweep value */
		if (vrate | vdepth) {
			if (vsweep) {
				int s = _muldivr(vdepth, 256, vsweep);
				vsweep = CLAMP(s, 0, 255);
			} else {
				vsweep = 255;
			}
		}

		slurp_read(fp, &w, 2);
		ins->fadeout = bswapLE16(w);

		if (ins->flags & ENV_VOLUME) {
			// fix note-fade if either volume loop is disabled or both end nodes are equal
			if (!(ins->flags & ENV_VOLLOOP) || ins->vol_env.loop_start == ins->vol_env.loop_end) {
				ins->vol_env.loop_start = ins->vol_env.loop_end = ins->vol_env.nodes - 1;
			} else {
				// fix volume envelope
				fix_xm_envelope_loop(&ins->vol_env, ins->flags & ENV_VOLSUSTAIN);
			}

			if (!(ins->flags & ENV_VOLSUSTAIN))
				ins->vol_env.sustain_start = ins->vol_env.sustain_end = ins->vol_env.nodes - 1;
			ins->flags |= ENV_VOLLOOP | ENV_VOLSUSTAIN;
		} else {
			// fix note-off
			ins->vol_env.ticks[0] = 0;
			ins->vol_env.ticks[1] = 1;
			ins->vol_env.values[0] = 64;
			ins->vol_env.values[1] = 0;
			ins->vol_env.nodes = 2;
			ins->vol_env.sustain_start = ins->vol_env.sustain_end = 0;
			ins->flags |= ENV_VOLUME | ENV_VOLSUSTAIN;
		}

		if ((ins->flags & ENV_PANNING) && (ins->flags & ENV_PANLOOP)) {
			if (ins->pan_env.loop_start == ins->pan_env.loop_end) {
				// panning is unused in XI
				ins->flags &= ~ENV_PANLOOP;
			} else {
				// fix panning envelope
				fix_xm_envelope_loop(&ins->pan_env, ins->flags & ENV_PANSUSTAIN);
			}
		}


		// some other things...
		ins->panning = 128;
		ins->global_volume = 128;
		ins->pitch_pan_center = 60; // C-5?

		/* here we're looking at what the ft2 spec SAYS are two reserved bytes.
		most programs blindly follow ft2's saving and add 22 zero bytes at the end (making
		the instrument header size 263 bytes), but ft2 is really writing the midi settings
		there, at least in the first 7 bytes. (as far as i can tell, the rest of the bytes
		are always zero) */
		int midi_enabled = slurp_getc(fp); // instrument midi enable = 0/1
		b = slurp_getc(fp); // midi transmit channel = 0-15
		ins->midi_channel_mask = (midi_enabled == 1) ? 1 << MIN(b, 15) : 0;
		slurp_read(fp, &w, 2); // midi program = 0-127
		w = bswapLE16(w);
		ins->midi_program = MIN(w, 127);
		slurp_read(fp, &w, 2); // bender range (halftones) = 0-36
		if (slurp_getc(fp) == 1)
			ins->global_volume = 0; // mute computer = 0/1

		slurp_seek(fp, ihdr - 248, SEEK_CUR);

		for (ns = 0; ns < nsmp; ns++) {
			int8_t relnote, finetune;
			song_sample_t *smp;

			if (abssamp + ns >= MAX_SAMPLES) {
				// TODO: try harder (fill unused sample slots)
				log_appendf(4, " Warning: Too many samples in file");
				break;
			}
			smp = song->samples + abssamp + ns;

			slurp_read(fp, &d, 4);
			smp->length = bswapLE32(d);
			slurp_read(fp, &d, 4);
			smp->loop_start = bswapLE32(d);
			slurp_read(fp, &d, 4);
			smp->loop_end = bswapLE32(d) + smp->loop_start;
			smp->volume = slurp_getc(fp);
			smp->volume = MIN(64, smp->volume);
			smp->volume *= 4; //mphack
			smp->global_volume = 64;
			smp->flags = CHN_PANNING;
			finetune = slurp_getc(fp);
			b = slurp_getc(fp); // flags
			if (smp->loop_start >= smp->loop_end)
				b &= ~3; // that loop sucks, turn it off
			switch (b & 3) {
				/* NOTE: all cases fall through here.
				In FT2, type 3 is played as pingpong, but the GUI doesn't show any selected
				loop type. Apparently old MPT versions wrote 3 for pingpong loops, but that
				doesn't seem to be reliable enough to declare "THIS WAS MPT" because it seems
				FT2 would also SAVE that broken data after loading an instrument with loop
				type 3 was set. I have no idea. */
				case 3: case 2: smp->flags |= CHN_PINGPONGLOOP;
				case 1: smp->flags |= CHN_LOOP;
			}
			if (b & 0x10) {
				smp->flags |= CHN_16BIT;
				// NOTE length and loop start/end are adjusted later
			}
			if (b & 0x20) {
				smp->flags |= CHN_STEREO;
				// NOTE length and loop start/end are adjusted later
			}
			smp->panning = slurp_getc(fp); //mphack, should be adjusted to 0-64
			relnote = slurp_getc(fp);
			smp->c5speed = transpose_to_frequency(relnote, finetune);
			uint8_t reserved = slurp_getc(fp);
			srsvd_or |= reserved;
			if (reserved == 0xAD && !(b & 0x10) && !(b & 0x20)) {
				smp->adlib_bytes[0] = 0xAD; // temp storage
			}
			slurp_read(fp, smp->name, 22);
			smp->name[22] = '\0';
			if (detected & ID_DIGITRAK && memchr(smp->name, '\0', 22) != NULL)
				detected &= ~ID_DIGITRAK;

			smp->vib_type = vtype;
			smp->vib_rate = vsweep;
			smp->vib_depth = vdepth;
			smp->vib_speed = vrate;
		}
		if (hdr->version == 0x0104)
			load_xm_samples(song->samples + abssamp, ns, fp);
		abssamp += ns;
		// if we ran out of samples, stop trying to load instruments
		// (note this will break things with xm format ver < 0x0104!)
		if (ns != nsmp)
			break;
	}

	if (detected & ID_FT2CLONE) {
		if (srsvd_or == 0) {
			strcpy(song->tracker_id, "Modplug Tracker");
		} else {
			// PlayerPro: itype and smp rsvd are both always zero
			// no idea how to identify it elsewise.
			strcpy(song->tracker_id, "FastTracker clone");
		}
	} else if ((detected & ID_DIGITRAK) && srsvd_or == 0 && (itype ? itype : -1) == -1) {
		strcpy(song->tracker_id, "Digitrakker");
	} else if (detected == ID_UNKNOWN) {
		strcpy(song->tracker_id, "Unknown tracker");
	}

	return (hdr->version < 0x0104) ? abssamp : 0;
}

int fmt_xm_load_song(song_t *song, slurp_t *fp, SCHISM_UNUSED unsigned int lflags)
{
	struct xm_file_header hdr;
	int n;
	uint8_t b;

	if (!read_header_xm(&hdr, fp))
		return LOAD_UNSUPPORTED;

	memcpy(song->title, hdr.name, 20);
	song->title[20] = '\0';
	memcpy(song->tracker_id, hdr.tracker, 20);
	song->tracker_id[20] = '\0';

	if (hdr.flags & 1)
		song->flags |= SONG_LINEARSLIDES;

	song->flags |= SONG_ITOLDEFFECTS | SONG_COMPATGXX | SONG_INSTRUMENTMODE;

	song->initial_speed = MIN(hdr.tempo, 255);
	if (!song->initial_speed)
		song->initial_speed = 255;

	song->initial_tempo = CLAMP(hdr.bpm, 31, 255);
	song->initial_global_volume = 128;
	song->mixing_volume = 48;

	for (n = 0; n < hdr.channels; n++)
		song->channels[n].panning = 32 * 4; //mphack
	for (; n < MAX_CHANNELS; n++)
		song->channels[n].flags |= CHN_MUTE;

	hdr.songlen = MIN(MAX_ORDERS, hdr.songlen);
	for (n = 0; n < hdr.songlen; n++) {
		b = slurp_getc(fp);
		song->orderlist[n] = (b >= MAX_PATTERNS) ? ORDER_SKIP : b;
	}

	slurp_seek(fp, 60 + hdr.headersz, SEEK_SET);

	if (hdr.version == 0x0104) {
		load_xm_patterns(song, &hdr, fp);
		load_xm_instruments(song, &hdr, fp);
	} else {
		int nsamp = load_xm_instruments(song, &hdr, fp);
		load_xm_patterns(song, &hdr, fp);
		load_xm_samples(song->samples + 1, nsamp, fp);
	}
	csf_insert_restart_pos(song, hdr.restart);

	// ModPlug song message
	char text[4];
	if (slurp_read(fp, text, sizeof(text)) == sizeof(text) && !memcmp(text, "text", 4)) {
		uint32_t len = 0;
		slurp_read(fp, &len, 4);
		len = bswapLE32(len);
		len = MIN(MAX_MESSAGE, len);
		slurp_read(fp, song->message, len);
		song->message[len] = '\0';
	}

	return LOAD_SUCCESS;
}

