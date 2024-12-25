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
#include "str.h"

#include "player/sndfile.h"

/* --------------------------------------------------------------------- */

/* MDL is nice, but it's a pain to read the title... */

int fmt_mdl_read_info(dmoz_file_t *file, slurp_t *fp)
{
	unsigned char magic[4];

	if (slurp_read(fp, magic, sizeof(magic)) != sizeof(magic)
		|| memcmp(magic, "DMDL", sizeof(magic)))
		return 0;

	/* major version number (accept 0 or 1) */
	int version = slurp_getc(fp);
	if (version == EOF)
		return 0;

	if ((((unsigned char)version & 0xf0) >> 4) > 1)
		return 0;

	for (;;) {
		unsigned char id[2];
		uint32_t block_length;

		if (slurp_read(fp, &id, sizeof(id)) != sizeof(id)
			|| slurp_read(fp, &block_length, sizeof(block_length)) != sizeof(block_length))
			return 0;

		block_length = bswapLE32(block_length);

		if (!memcmp(id, "IN", 2)) {
			/* hey! we have a winner */
			unsigned char title[32], artist[20];

			if (slurp_read(fp, &title, sizeof(title)) != sizeof(title)
				|| slurp_read(fp, &artist, sizeof(artist)) != sizeof(artist))
				return 0;

			file->title = strn_dup((const char *)title, sizeof(title));
			file->artist = strn_dup((const char *)artist, sizeof(artist));
			file->description = "Digitrakker";
			/*file->extension = str_dup("mdl");*/
			file->type = TYPE_MODULE_XM;
			return 1;
		} else {
			slurp_seek(fp, SEEK_CUR, block_length);
		}
	}

	return 0;
}

/* --------------------------------------------------------------------------------------------------------- */
/* Structs and stuff for the loader */

#define MDL_BLOCK(a,b)          (((a) << 8) | (b))
#define MDL_BLK_INFO            MDL_BLOCK('I','N')
#define MDL_BLK_MESSAGE         MDL_BLOCK('M','E')
#define MDL_BLK_PATTERNS        MDL_BLOCK('P','A')
#define MDL_BLK_PATTERNNAMES    MDL_BLOCK('P','N')
#define MDL_BLK_TRACKS          MDL_BLOCK('T','R')
#define MDL_BLK_INSTRUMENTS     MDL_BLOCK('I','I')
#define MDL_BLK_VOLENVS         MDL_BLOCK('V','E')
#define MDL_BLK_PANENVS         MDL_BLOCK('P','E')
#define MDL_BLK_FREQENVS        MDL_BLOCK('F','E')
#define MDL_BLK_SAMPLEINFO      MDL_BLOCK('I','S')
#define MDL_BLK_SAMPLEDATA      MDL_BLOCK('S','A')

#define MDL_FADE_CUT 0xffff

enum {
	MDLNOTE_NOTE            = 1 << 0,
	MDLNOTE_SAMPLE          = 1 << 1,
	MDLNOTE_VOLUME          = 1 << 2,
	MDLNOTE_EFFECTS         = 1 << 3,
	MDLNOTE_PARAM1          = 1 << 4,
	MDLNOTE_PARAM2          = 1 << 5,
};

struct mdl_infoblock {
	char title[32];
	char composer[20];
	uint16_t numorders;
	uint16_t repeatpos;
	uint8_t globalvol;
	uint8_t speed;
	uint8_t tempo;
	uint8_t chanpan[32];
};

/* This is actually a part of the instrument (II) block */
struct mdl_samplehdr {
	uint8_t smpnum;
	uint8_t lastnote;
	uint8_t volume;
	uint8_t volenv_flags; // 6 bits env #, 2 bits flags
	uint8_t panning;
	uint8_t panenv_flags;
	uint16_t fadeout;
	uint8_t vibspeed;
	uint8_t vibdepth;
	uint8_t vibsweep;
	uint8_t vibtype;
	uint8_t freqenv_flags;
};

struct mdl_sampleinfo {
	uint8_t smpnum;
	char name[32];
	char filename[8];
	uint32_t c4speed; // c4, c5, whatever
	uint32_t length;
	uint32_t loopstart;
	uint32_t looplen;
	uint8_t volume; // volume in v0.0, unused after
	uint8_t flags;
};

struct mdl_envelope {
	uint8_t envnum;
	struct {
		uint8_t x; // delta-value from last point, 0 means no more points defined
		uint8_t y; // 0-63
	} nodes[15];
	uint8_t flags;
	uint8_t loop; // lower 4 bits = start, upper 4 bits = end
};

/* --------------------------------------------------------------------------------------------------------- */
/* Internal definitions */

struct mdlpat {
	int track; // which track to put here
	int rows; // 1-256
	song_note_t *note; // first note -- add 64 for next note, etc.
	struct mdlpat *next;
};

struct mdlenv {
	uint32_t flags;
	song_envelope_t data;
};

enum {
	MDL_HAS_INFO            = 1 << 0,
	MDL_HAS_MESSAGE         = 1 << 1,
	MDL_HAS_PATTERNS        = 1 << 2,
	MDL_HAS_TRACKS          = 1 << 3,
	MDL_HAS_INSTRUMENTS     = 1 << 4,
	MDL_HAS_VOLENVS         = 1 << 5,
	MDL_HAS_PANENVS         = 1 << 6,
	MDL_HAS_FREQENVS        = 1 << 7,
	MDL_HAS_SAMPLEINFO      = 1 << 8,
	MDL_HAS_SAMPLEDATA      = 1 << 9,
};

static const uint8_t mdl_efftrans[] = {
	/* 0 */ FX_NONE,
	/* 1st column only */
	/* 1 */ FX_PORTAMENTOUP,
	/* 2 */ FX_PORTAMENTODOWN,
	/* 3 */ FX_TONEPORTAMENTO,
	/* 4 */ FX_VIBRATO,
	/* 5 */ FX_ARPEGGIO,
	/* 6 */ FX_NONE,
	/* Either column */
	/* 7 */ FX_TEMPO,
	/* 8 */ FX_PANNING,
	/* 9 */ FX_SETENVPOSITION,
	/* A */ FX_NONE,
	/* B */ FX_POSITIONJUMP,
	/* C */ FX_GLOBALVOLUME,
	/* D */ FX_PATTERNBREAK,
	/* E */ FX_SPECIAL,
	/* F */ FX_SPEED,
	/* 2nd column only */
	/* G */ FX_VOLUMESLIDE, // up
	/* H */ FX_VOLUMESLIDE, // down
	/* I */ FX_RETRIG,
	/* J */ FX_TREMOLO,
	/* K */ FX_TREMOR,
	/* L */ FX_NONE,
};

static const uint8_t autovib_import[] = {VIB_SINE, VIB_RAMP_DOWN, VIB_SQUARE, VIB_SINE};

/* --------------------------------------------------------------------------------------------------------- */
/* a highly overcomplicated mess to import effects */

// receive an MDL effect, give back a 'normal' one.
static void translate_fx(uint8_t *pe, uint8_t *pp)
{
	uint8_t e = *pe;
	uint8_t p = *pp;

	if (e > 21)
		e = 0; // (shouldn't ever happen)
	*pe = mdl_efftrans[e];

	switch (e) {
	case 7: // tempo
		// MDL supports any nonzero tempo value, but we don't
		p = MAX(p, 0x20);
		break;
	case 8: // panning
		p = MIN(p << 1, 0xff);
		break;
	case 0xd: // pattern break
		// convert from stupid decimal-hex
		p = 10 * (p >> 4) + (p & 0xf);
		break;
	case 0xe: // special
		switch (p >> 4) {
		case 0: // unused
		case 3: // unused
		case 5: // set finetune
		case 8: // set samplestatus (what?)
			*pe = FX_NONE;
			break;
		case 1: // pan slide left
			*pe = FX_PANNINGSLIDE;
			p = (MAX(p & 0xf, 0xe) << 4) | 0xf;
			break;
		case 2: // pan slide right
			*pe = FX_PANNINGSLIDE;
			p = 0xf0 | MAX(p & 0xf, 0xe);
			break;
		case 4: // vibrato waveform
			p = 0x30 | (p & 0xf);
			break;
		case 6: // pattern loop
			p = 0xb0 | (p & 0xf);
			break;
		case 7: // tremolo waveform
			p = 0x40 | (p & 0xf);
			break;
		case 9: // retrig
			*pe = FX_RETRIG;
			p &= 0xf;
			break;
		case 0xa: // global vol slide up
			*pe = FX_GLOBALVOLSLIDE;
			p = 0xf0 & (((p & 0xf) + 1) << 3);
			break;
		case 0xb: // global vol slide down
			*pe = FX_GLOBALVOLSLIDE;
			p = ((p & 0xf) + 1) >> 1;
			break;
		case 0xc: // note cut
		case 0xd: // note delay
		case 0xe: // pattern delay
			// nothing to change here
			break;
		case 0xf: // offset -- further mangled later.
			*pe = FX_OFFSET;
			break;
		}
		break;
	case 0x10: // volslide up
		if (p < 0xE0) { // Gxy -> Dz0 (z=(xy>>2))
			p >>= 2;
			if (p > 0x0F)
				p = 0x0F;
			p <<= 4;
		} else if (p < 0xF0) { // GEy -> DzF (z=(y>>2))
			p = (((p & 0x0F) << 2) | 0x0F);
		} else { // GFy -> DyF
			p = ((p << 4) | 0x0F);
		}
		break;
	case 0x11: // volslide down
		if (p < 0xE0) { // Hxy -> D0z (z=(xy>>2))
			p >>= 2;
			if(p > 0x0F)
				p = 0x0F;
		} else if (p < 0xF0) { // HEy -> DFz (z=(y>>2))
			p = (((p & 0x0F) >> 2) | 0xF0);
		} else { // HFy -> DFy
			// Nothing to do
		}
		break;
	}

	*pp = p;
}

// return: 1 if an effect was lost, 0 if not.
static int cram_mdl_effects(song_note_t *note, uint8_t vol, uint8_t e1, uint8_t e2, uint8_t p1, uint8_t p2)
{
	int lostfx = 0;
	int n;
	uint8_t tmp;

	// map second effect values 1-6 to effects G-L
	if (e2 >= 1 && e2 <= 6)
		e2 += 15;

	translate_fx(&e1, &p1);
	translate_fx(&e2, &p2);
	/* From the Digitrakker documentation:
		* EFx -xx - Set Sample Offset
		This  is a  double-command.  It starts the
		sample at adress xxx*256.
		Example: C-5 01 -- EF1 -23 ->starts sample
		01 at address 12300 (in hex).
	Kind of screwy, but I guess it's better than the mess required to do it with IT (which effectively
	requires 3 rows in order to set the offset past 0xff00). If we had access to the entire track, we
	*might* be able to shove the high offset SAy into surrounding rows, but it wouldn't always be possible,
	it'd make the loader a lot uglier, and generally would be more trouble than it'd be worth to implement.

	What's more is, if there's another effect in the second column, it's ALSO processed in addition to the
	offset, and the second data byte is shared between the two effects.
	And: an offset effect without a note will retrigger the previous note, but I'm not even going to try to
	handle that behavior. */
	if (e1 == FX_OFFSET) {
		// EFy -xx => offset yxx00
		p1 = (p1 & 0xf) ? 0xff : p2;
		if (e2 == FX_OFFSET)
			e2 = FX_NONE;
	} else if (e2 == FX_OFFSET) {
		// --- EFy => offset y0000 (best we can do without doing a ton of extra work is 0xff00)
		p2 = (p2 & 0xf) ? 0xff : 0;
	}

	if (vol) {
		note->voleffect = VOLFX_VOLUME;
		note->volparam = (vol + 2) >> 2;
	}

	/* If we have Dxx + G00, or Dxx + H00, combine them into Lxx/Kxx.
	(Since pitch effects only "fit" in the first effect column, and volume effects only work in the
	second column, we don't have to check every combination here.) */
	if (e2 == FX_VOLUMESLIDE && p1 == 0) {
		if (e1 == FX_TONEPORTAMENTO) {
			e1 = FX_NONE;
			e2 = FX_TONEPORTAVOL;
		} else if (e1 == FX_VIBRATO) {
			e1 = FX_NONE;
			e2 = FX_VIBRATOVOL;
		}
	}

	/* Try to fit the "best" effect into e2. */
	if (e1 == FX_NONE) {
		// easy
	} else if (e2 == FX_NONE) {
		// almost as easy
		e2 = e1;
		p2 = p1;
		e1 = FX_NONE;
	} else if (e1 == e2 && e1 != FX_SPECIAL) {
		/* Digitrakker processes the effects left-to-right, so if both effects are the same, the
		second essentially overrides the first. */
		e1 = FX_NONE;
	} else if (!vol) {
		// The volume column is free, so try to shove one of them into there.

		// See also xm.c.
		// (Just because I'm using the same sort of code twice doesn't make it any less of a hack)
		for (n = 0; n < 4; n++) {
			if (convert_voleffect(&e1, &p1, n >> 1)) {
				note->voleffect = e1;
				note->volparam = p1;
				e1 = FX_NONE;
				break;
			} else {
				// swap them
				tmp = e2; e2 = e1; e1 = tmp;
				tmp = p2; p2 = p1; p1 = tmp;
			}
		}
	}

	// If we still have two effects, pick the 'best' one
	if (e1 != FX_NONE && e2 != FX_NONE) {
		lostfx++;
		if (effect_weight[e1] < effect_weight[e2]) {
			e2 = e1;
			p2 = p1;
		}
	}

	note->effect = e2;
	note->param = p2;

	return lostfx;
}

/* --------------------------------------------------------------------------------------------------------- */
/* block reading */

// return: repeat position.
static int mdl_read_info(song_t *song, slurp_t *fp)
{
	struct mdl_infoblock info;
	int n, songlen;
	uint8_t b;

#define READ_VALUE(name) \
	do { if (slurp_read(fp, &info.name, sizeof(info.name)) != sizeof(info.name)) { return 0; } } while (0)

	READ_VALUE(title);
	READ_VALUE(composer);
	READ_VALUE(numorders);
	READ_VALUE(repeatpos);
	READ_VALUE(globalvol);
	READ_VALUE(speed);
	READ_VALUE(tempo);
	READ_VALUE(chanpan);

#undef READ_VALUE

	info.numorders = bswapLE16(info.numorders);
	info.repeatpos = bswapLE16(info.repeatpos);

	// title is space-padded
	info.title[31] = '\0';
	str_trim(info.title);
	strncpy(song->title, info.title, 25);
	song->title[25] = '\0';

	song->initial_global_volume = (info.globalvol + 1) >> 1;
	song->initial_speed = info.speed ? info.speed : 1;
	song->initial_tempo = MAX(info.tempo, 31); // MDL tempo range is actually 4-255

	// channel pannings
	for (n = 0; n < 32; n++) {
		song->channels[n].panning = (info.chanpan[n] & 127) << 1; // ugh
		if (info.chanpan[n] & 128)
			song->channels[n].flags |= CHN_MUTE;
	}
	for (; n < 64; n++) {
		song->channels[n].panning = 128;
		song->channels[n].flags |= CHN_MUTE;
	}

	songlen = MIN(info.numorders, MAX_ORDERS - 1);
	for (n = 0; n < songlen; n++) {
		b = slurp_getc(fp);
		song->orderlist[n] = (b < MAX_PATTERNS) ? b : ORDER_SKIP;
	}

	return info.repeatpos;
}

static void mdl_read_message(song_t *song, slurp_t *fp, uint32_t blklen)
{
	char *ptr = song->message;

	blklen = MIN(blklen, MAX_MESSAGE);
	slurp_read(fp, ptr, blklen);
	ptr[blklen] = '\0';

	while ((ptr = strchr(ptr, '\r')) != NULL)
		*ptr = '\n';
}

static struct mdlpat *mdl_read_patterns(song_t *song, slurp_t *fp)
{
	struct mdlpat pat_head = { .next = NULL }; // only exists for .next
	struct mdlpat *patptr = &pat_head;
	song_note_t *note;
	int npat, nchn, rows, pat, chn;
	uint16_t trknum;

	npat = slurp_getc(fp);
	npat = MIN(npat, MAX_PATTERNS);
	for (pat = 0; pat < npat; pat++) {

		nchn = slurp_getc(fp);
		rows = slurp_getc(fp) + 1;
		slurp_seek(fp, 16, SEEK_CUR); // skip the name

		note = song->patterns[pat] = csf_allocate_pattern(rows);
		song->pattern_size[pat] = song->pattern_alloc_size[pat] = rows;
		for (chn = 0; chn < nchn; chn++, note++) {
			slurp_read(fp, &trknum, 2);
			trknum = bswapLE16(trknum);
			if (!trknum)
				continue;

			patptr->next = mem_alloc(sizeof(struct mdlpat));
			patptr = patptr->next;
			patptr->track = trknum;
			patptr->rows = rows;
			patptr->note = note;
			patptr->next = NULL;
		}
	}

	return pat_head.next;
}

// mostly the same as above
static struct mdlpat *mdl_read_patterns_v0(song_t *song, slurp_t *fp)
{
	struct mdlpat pat_head = { .next = NULL };
	struct mdlpat *patptr = &pat_head;
	song_note_t *note;
	int npat, pat, chn;
	uint16_t trknum;

	npat = slurp_getc(fp);
	npat = MIN(npat, MAX_PATTERNS);
	for (pat = 0; pat < npat; pat++) {

		note = song->patterns[pat] = csf_allocate_pattern(64);
		song->pattern_size[pat] = song->pattern_alloc_size[pat] = 64;
		for (chn = 0; chn < 32; chn++, note++) {
			slurp_read(fp, &trknum, 2);
			trknum = bswapLE16(trknum);
			if (!trknum)
				continue;

			patptr->next = mem_alloc(sizeof(struct mdlpat));
			patptr = patptr->next;
			patptr->track = trknum;
			patptr->rows = 64;
			patptr->note = note;
			patptr->next = NULL;
		}
	}

	return pat_head.next;
}

struct receive_userdata {
	song_note_t *track;
	int *lostfx;
};

static int mdl_receive_track(const void *data, size_t len, void *userdata)
{
	struct receive_userdata *rec_userdata = userdata;
	int row = 0;
	uint8_t b, x, y;
	uint8_t vol, e1, e2, p1, p2;
	song_note_t *track = rec_userdata->track;
	int *lostfx = rec_userdata->lostfx;

	slurp_t fake_fp = {0};
	slurp_memstream(&fake_fp, (uint8_t *)data, len);

	while (row < 256 && !slurp_eof(&fake_fp)) {
		b = slurp_getc(&fake_fp);
		x = b >> 2;
		y = b & 3;
		switch (y) {
		case 0: // (x+1) empty notes follow
			row += x + 1;
			break;
		case 1: // Repeat previous note (x+1) times
			if (row > 0) {
				do {
					track[row] = track[row - 1];
				} while (++row < 256 && x--);
			}
			break;
		case 2: // Copy note from row x
			if (row > x)
				track[row] = track[x];
			row++;
			break;
		case 3: // New note data
			if (x & MDLNOTE_NOTE) {
				b = slurp_getc(&fake_fp);
				// convenient! :)
				// (I don't know what DT does for out of range notes, might be worth
				// checking some time)
				track[row].note = (b > 120) ? NOTE_OFF : b;
			}
			if (x & MDLNOTE_SAMPLE) {
				b = slurp_getc(&fake_fp);
				if (b >= MAX_INSTRUMENTS)
					b = 0;
				track[row].instrument = b;
			}
			vol = (x & MDLNOTE_VOLUME) ? slurp_getc(&fake_fp) : 0;
			if (x & MDLNOTE_EFFECTS) {
				b = slurp_getc(&fake_fp);
				e1 = b & 0xf;
				e2 = b >> 4;
			} else {
				e1 = e2 = 0;
			}
			p1 = (x & MDLNOTE_PARAM1) ? slurp_getc(&fake_fp) : 0;
			p2 = (x & MDLNOTE_PARAM2) ? slurp_getc(&fake_fp) : 0;
			*lostfx += cram_mdl_effects(&track[row], vol, e1, e2, p1, p2);
			row++;
			break;
		}
	}

	return slurp_tell(&fake_fp);
}

static int mdl_read_tracks(slurp_t *fp, song_note_t *tracks[65536])
{
	/* why are we allocating so many of these ? */
	uint16_t ntrks, trk;
	int lostfx = 0;

	slurp_read(fp, &ntrks, 2);
	ntrks = bswapLE16(ntrks);

	// track 0 is always blank
	for (trk = 1; trk <= ntrks; trk++) {
		int64_t startpos = slurp_tell(fp);
		if (startpos < 0)
			return 0; /* what ? */

		uint16_t bytesleft;
		slurp_read(fp, &bytesleft, sizeof(bytesleft));
		bytesleft = bswapLE16(bytesleft);

		tracks[trk] = mem_calloc(256, sizeof(song_note_t));

		struct receive_userdata data = {
			.track = tracks[trk],
			.lostfx = &lostfx,
		};

		int c = slurp_receive(fp, mdl_receive_track, bytesleft, &data);

		slurp_seek(fp, startpos + c + 2, SEEK_SET);
	}
	if (lostfx)
		log_appendf(4, " Warning: %d effect%s dropped", lostfx, lostfx == 1 ? "" : "s");

	return 1;
}


/* This is actually somewhat horrible.
Digitrakker's envelopes are actually properties of the *sample*, not the instrument -- that is, the only thing
an instrument is actually doing is providing a keyboard split and grouping a bunch of samples together.

This is handled here by importing the instrument names and note/sample mapping into a "master" instrument,
but NOT writing the envelope data there -- instead, that stuff is placed into whatever instrument matches up
with the sample number. Then, when building the tracks into patterns, we'll actually *remap* all the numbers
and rewrite each instrument's sample map as a 1:1 mapping with the sample.
In the end, the song will play back correctly (well, at least hopefully it will ;) though the instrument names
won't always line up. */
static void mdl_read_instruments(song_t *song, slurp_t *fp)
{
	struct mdl_samplehdr shdr; // Etaoin shrdlu
	song_instrument_t *ins; // 'master' instrument
	song_instrument_t *sins; // other instruments created to track each sample's individual envelopes
	song_sample_t *smp;
	int nins, nsmp;
	int insnum;
	int firstnote, note;

	nins = slurp_getc(fp);
	while (nins--) {
		insnum = slurp_getc(fp);
		firstnote = 0;
		nsmp = slurp_getc(fp);
		// if it's out of range, or if the same instrument was already loaded (weird), don't read it
		if (insnum == 0 || insnum > MAX_INSTRUMENTS) {
			// skip it (32 bytes name, plus 14 bytes per sample)
			slurp_seek(fp, 32 + 14 * nsmp, SEEK_SET);
			continue;
		}
		// ok, make an instrument
		if (!song->instruments[insnum])
			song->instruments[insnum] = csf_allocate_instrument();
		ins = song->instruments[insnum];

		slurp_read(fp, ins->name, 25);
		slurp_seek(fp, 7, SEEK_CUR); // throw away the rest
		ins->name[25] = '\0';

		while (nsmp--) {
			// read a sample
#define READ_VALUE(name) slurp_read(fp, &shdr.name, sizeof(shdr.name))

			READ_VALUE(smpnum);
			READ_VALUE(lastnote);
			READ_VALUE(volume);
			READ_VALUE(volenv_flags);
			READ_VALUE(panning);
			READ_VALUE(panenv_flags);
			READ_VALUE(fadeout);
			READ_VALUE(vibspeed);
			READ_VALUE(vibdepth);
			READ_VALUE(vibsweep);
			READ_VALUE(vibtype);
			slurp_seek(fp, 1, SEEK_CUR); // reserved, zero
			READ_VALUE(freqenv_flags);

#undef READ_VALUE

			shdr.fadeout = bswapLE16(shdr.fadeout);

			if (shdr.smpnum == 0 || shdr.smpnum > MAX_SAMPLES)
				continue;

			if (!song->instruments[shdr.smpnum])
				song->instruments[shdr.smpnum] = csf_allocate_instrument();

			sins = song->instruments[shdr.smpnum];

			smp = song->samples + shdr.smpnum;

			// Write this sample's instrument mapping
			// (note: test "jazz 2 jazz.mdl", it uses a multisampled piano)
			shdr.lastnote = MIN(shdr.lastnote, 119);
			for (note = firstnote; note <= shdr.lastnote; note++)
				ins->sample_map[note] = shdr.smpnum;
			firstnote = shdr.lastnote + 1; // get ready for the next sample

			// temporarily hijack the envelope "nodes" field to write the envelope number
			sins->vol_env.nodes = shdr.volenv_flags & 63;
			sins->pan_env.nodes = shdr.panenv_flags & 63;
			sins->pitch_env.nodes = shdr.freqenv_flags & 63;

			if (shdr.volenv_flags & 128)
				sins->flags |= ENV_VOLUME;
			if (shdr.panenv_flags & 128)
				sins->flags |= ENV_PANNING;
			if (shdr.freqenv_flags & 128)
				sins->flags |= ENV_PITCH;

			// DT fadeout = 0000-1fff, or 0xffff for "cut"
			// assuming DT uses 'cut' behavior for anything past 0x1fff, too lazy to bother
			// hex-editing a file at the moment to find out :P
			sins->fadeout = (shdr.fadeout < 0x2000)
				? (shdr.fadeout + 1) >> 1 // this seems about right
				: MDL_FADE_CUT; // temporary

			// for the volume envelope / flags:
			//      "bit 6   -> flags, if volume is used"
			// ... huh? what happens if the volume isn't used?
			smp->volume = shdr.volume; //mphack (range 0-255, s/b 0-64)
			smp->panning = ((MIN(shdr.panning, 127) + 1) >> 1) * 4; //mphack
			if (shdr.panenv_flags & 64)
				smp->flags |= CHN_PANNING;

			smp->vib_speed = shdr.vibspeed; // XXX bother checking ranges for vibrato
			smp->vib_depth = shdr.vibdepth;
			smp->vib_rate = shdr.vibsweep;
			smp->vib_type = autovib_import[shdr.vibtype & 3];
		}
	}
}

static void mdl_read_sampleinfo(song_t *song, slurp_t *fp, uint8_t *packtype)
{
	struct mdl_sampleinfo sinfo;
	song_sample_t *smp;
	int nsmp;

	nsmp = slurp_getc(fp);
	while (nsmp--) {
#define READ_VALUE(name) slurp_read(fp, &sinfo.name, sizeof(sinfo.name))

		READ_VALUE(smpnum);
		READ_VALUE(name);
		READ_VALUE(filename);
		READ_VALUE(c4speed);
		READ_VALUE(length);
		READ_VALUE(loopstart);
		READ_VALUE(looplen);
		READ_VALUE(volume);
		READ_VALUE(flags);

#undef READ_VALUE

		if (sinfo.smpnum == 0 || sinfo.smpnum > MAX_SAMPLES) {
			continue;
		}

		smp = song->samples + sinfo.smpnum;
		strncpy(smp->name, sinfo.name, 25);
		smp->name[25] = '\0';
		strncpy(smp->filename, sinfo.filename, 8);
		smp->filename[8] = '\0';

		// MDL has ten octaves like IT, but they're not the *same* ten octaves -- dropping
		// perfectly good note data is stupid so I'm adjusting the sample tunings instead
		smp->c5speed = bswapLE32(sinfo.c4speed) * 2;
		smp->length = bswapLE32(sinfo.length);
		smp->loop_start = bswapLE32(sinfo.loopstart);
		smp->loop_end = bswapLE32(sinfo.looplen);
		if (smp->loop_end) {
			smp->loop_end += smp->loop_start;
			smp->flags |= CHN_LOOP;
		}
		if (sinfo.flags & 1) {
			smp->flags |= CHN_16BIT;
			smp->length >>= 1;
			smp->loop_start >>= 1;
			smp->loop_end >>= 1;
		}
		if (sinfo.flags & 2)
			smp->flags |= CHN_PINGPONGLOOP;
		packtype[sinfo.smpnum] = ((sinfo.flags >> 2) & 3);

		smp->global_volume = 64;
	}
}

// (ughh)
static void mdl_read_sampleinfo_v0(song_t *song, slurp_t *fp, uint8_t *packtype)
{
	struct mdl_sampleinfo sinfo;
	song_sample_t *smp;
	int nsmp;

	nsmp = slurp_getc(fp);
	while (nsmp--) {
#define READ_VALUE(name) slurp_read(fp, &sinfo.name, sizeof(sinfo.name))

		READ_VALUE(smpnum);
		READ_VALUE(name);
		READ_VALUE(filename);
		READ_VALUE(c4speed);
		READ_VALUE(length);
		READ_VALUE(loopstart);
		READ_VALUE(looplen);
		READ_VALUE(volume);
		READ_VALUE(flags);

#undef READ_VALUE

		if (sinfo.smpnum == 0 || sinfo.smpnum > MAX_SAMPLES) {
			continue;
		}

		smp = song->samples + sinfo.smpnum;
		strncpy(smp->name, sinfo.name, 25);
		smp->name[25] = '\0';
		strncpy(smp->filename, sinfo.filename, 8);
		smp->filename[8] = '\0';

		smp->c5speed = bswapLE16(sinfo.c4speed) * 2;
		smp->length = bswapLE32(sinfo.length);
		smp->loop_start = bswapLE32(sinfo.loopstart);
		smp->loop_end = bswapLE32(sinfo.looplen);
		smp->volume = sinfo.volume; //mphack (range 0-255, I think?)
		if (smp->loop_end) {
			smp->loop_end += smp->loop_start;
			smp->flags |= CHN_LOOP;
		}
		if (sinfo.flags & 1) {
			smp->flags |= CHN_16BIT;
			smp->length >>= 1;
			smp->loop_start >>= 1;
			smp->loop_end >>= 1;
		}
		if (sinfo.flags & 2)
			smp->flags |= CHN_PINGPONGLOOP;
		packtype[sinfo.smpnum] = ((sinfo.flags >> 2) & 3);

		smp->global_volume = 64;
	}
}

static void mdl_read_envelopes(slurp_t *fp, struct mdlenv **envs, uint32_t flags)
{
	struct mdl_envelope ehdr;
	song_envelope_t *env;
	uint8_t nenv;
	int n, tick;

	nenv = slurp_getc(fp);
	while (nenv--) {
#define READ_VALUE(name) slurp_read(fp, &ehdr.name, sizeof(ehdr.name))

		READ_VALUE(envnum);

		for (size_t i = 0; i < ARRAY_SIZE(ehdr.nodes); i++) {
			READ_VALUE(nodes[i].x);
			READ_VALUE(nodes[i].y);
		}

		READ_VALUE(flags);
		READ_VALUE(loop);

#undef READ_VALUE

		if (ehdr.envnum > 63)
			continue;

		if (!envs[ehdr.envnum])
			envs[ehdr.envnum] = mem_calloc(1, sizeof(struct mdlenv));
		env = &envs[ehdr.envnum]->data;

		env->nodes = 15;
		tick = -ehdr.nodes[0].x; // adjust so it starts at zero
		for (n = 0; n < 15; n++) {
			if (!ehdr.nodes[n].x) {
				env->nodes = MAX(n, 2);
				break;
			}
			tick += ehdr.nodes[n].x;
			env->ticks[n] = tick;
			env->values[n] = MIN(ehdr.nodes[n].y, 64); // actually 0-63
		}

		env->loop_start = ehdr.loop & 0xf;
		env->loop_end = ehdr.loop >> 4;
		env->sustain_start = env->sustain_end = ehdr.flags & 0xf;

		envs[ehdr.envnum]->flags = 0;
		if (ehdr.flags & 16)
			envs[ehdr.envnum]->flags
				|= (flags & (ENV_VOLSUSTAIN | ENV_PANSUSTAIN | ENV_PITCHSUSTAIN));
		if (ehdr.flags & 32)
			envs[ehdr.envnum]->flags
				|= (flags & (ENV_VOLLOOP | ENV_PANLOOP | ENV_PITCHLOOP));
	}
}

/* --------------------------------------------------------------------------------------------------------- */

static void copy_envelope(song_instrument_t *ins, song_envelope_t *ienv, struct mdlenv **envs, uint32_t enable)
{
	// nodes temporarily indicates which envelope to load
	struct mdlenv *env = envs[ienv->nodes];
	if (env) {
		ins->flags |= env->flags;
		memcpy(ienv, &env->data, sizeof(song_envelope_t));
	} else {
		ins->flags &= ~enable;
		ienv->nodes = 2;
	}
}

int fmt_mdl_load_song(song_t *song, slurp_t *fp, SCHISM_UNUSED unsigned int lflags)
{
	struct mdlpat *pat, *patptr = NULL;
	struct mdlenv *volenvs[64] = {0}, *panenvs[64] = {0}, *freqenvs[64] = {0};
	uint8_t packtype[MAX_SAMPLES] = {0};
	song_note_t *tracks[65536] = {0};
	long datapos = 0; // where to seek for the sample data
	int restartpos = -1;
	int trk, n;
	uint32_t readflags = 0;
	uint8_t tag[4];
	uint8_t fmtver; // file format version, e.g. 0x11 = v1.1

	slurp_read(fp, tag, 4);
	if (memcmp(tag, "DMDL", 4) != 0)
		return LOAD_UNSUPPORTED;

	fmtver = slurp_getc(fp);

	// Read the next block
	while (!slurp_eof(fp)) {
		uint32_t blklen; // length of this block
		size_t nextpos; // ... and start of next one

		slurp_read(fp, tag, 2);
		slurp_read(fp, &blklen, 4);
		blklen = bswapLE32(blklen);
		nextpos = slurp_tell(fp) + blklen;

		switch (MDL_BLOCK(tag[0], tag[1])) {
		case MDL_BLK_INFO:
			if (!(readflags & MDL_HAS_INFO)) {
				readflags |= MDL_HAS_INFO;
				restartpos = mdl_read_info(song, fp);
			}
			break;
		case MDL_BLK_MESSAGE:
			if (!(readflags & MDL_HAS_MESSAGE)) {
				readflags |= MDL_HAS_MESSAGE;
				mdl_read_message(song, fp, blklen);
			}
			break;
		case MDL_BLK_PATTERNS:
			if (!(readflags & MDL_HAS_PATTERNS)) {
				readflags |= MDL_HAS_PATTERNS;
				patptr = ((fmtver >> 4) ? mdl_read_patterns : mdl_read_patterns_v0)(song, fp);
			}
			break;
		case MDL_BLK_TRACKS:
			if (!(readflags & MDL_HAS_TRACKS)) {
				readflags |= MDL_HAS_TRACKS;
				mdl_read_tracks(fp, tracks);
			}
			break;
		case MDL_BLK_INSTRUMENTS:
			if (!(readflags & MDL_HAS_INSTRUMENTS)) {
				readflags |= MDL_HAS_INSTRUMENTS;
				mdl_read_instruments(song, fp);
			}
			break;
		case MDL_BLK_VOLENVS:
			if (!(readflags & MDL_HAS_VOLENVS)) {
				readflags |= MDL_HAS_VOLENVS;
				mdl_read_envelopes(fp, volenvs, ENV_VOLLOOP | ENV_VOLSUSTAIN);
			}
			break;
		case MDL_BLK_PANENVS:
			if (!(readflags & MDL_HAS_PANENVS)) {
				readflags |= MDL_HAS_PANENVS;
				mdl_read_envelopes(fp, panenvs, ENV_PANLOOP | ENV_PANSUSTAIN);
			}
			break;
		case MDL_BLK_FREQENVS:
			if (!(readflags & MDL_HAS_FREQENVS)) {
				readflags |= MDL_HAS_FREQENVS;
				mdl_read_envelopes(fp, freqenvs, ENV_PITCHLOOP | ENV_PITCHSUSTAIN);
			}
			break;
		case MDL_BLK_SAMPLEINFO:
			if (!(readflags & MDL_HAS_SAMPLEINFO)) {
				readflags |= MDL_HAS_SAMPLEINFO;
				((fmtver >> 4) ? mdl_read_sampleinfo : mdl_read_sampleinfo_v0)
					(song, fp, packtype);
			}
			break;
		case MDL_BLK_SAMPLEDATA:
			// Can't do anything until we have the sample info block loaded, since the sample
			// lengths and packing information is stored there.
			// Best we can do at the moment is to remember where this block was so we can jump
			// back to it later.
			if (!(readflags & MDL_HAS_SAMPLEDATA)) {
				readflags |= MDL_HAS_SAMPLEDATA;
				datapos = slurp_tell(fp);
			}
			break;

		case MDL_BLK_PATTERNNAMES:
			// don't care
			break;

		default:
			//log_appendf(4, " Warning: Unknown block of type '%c%c' (0x%04X) at %ld",
			//        tag[0], tag[1], MDL_BLOCK(tag[0], tag[1]), slurp_tell(fp));
			break;
		}

		if (slurp_seek(fp, nextpos, SEEK_SET) != 0) {
			log_appendf(4, " Warning: Failed to seek (file truncated?)");
			break;
		}
	}

	if (!(readflags & MDL_HAS_INSTRUMENTS)) {
		// Probably a v0 file, fake an instrument
		for (n = 1; n < MAX_SAMPLES; n++) {
			if (song->samples[n].length) {
				song->instruments[n] = csf_allocate_instrument();
				strcpy(song->instruments[n]->name, song->samples[n].name);
			}
		}
	}

	if (readflags & MDL_HAS_SAMPLEINFO) {
		// Sample headers loaded!
		// if the sample data was encountered, load it now
		// otherwise, clear out the sample lengths so Bad Things don't happen later
		if (datapos) {
			slurp_seek(fp, datapos, SEEK_SET);
			for (n = 1; n < MAX_SAMPLES; n++) {
				if (!packtype[n] && !song->samples[n].length)
					continue;
				uint32_t flags;
				if (packtype[n] > 2) {
					log_appendf(4, " Warning: Sample %d: unknown packing type %d",
						    n, packtype[n]);
					packtype[n] = 0; // ?
				} else if (packtype[n] == ((song->samples[n].flags & CHN_16BIT) ? 1 : 2)) {
					log_appendf(4, " Warning: Sample %d: bit width / pack type mismatch",
						    n);
				}
				flags = SF_LE | SF_M;
				flags |= packtype[n] ? SF_MDL : SF_PCMS;
				flags |= (song->samples[n].flags & CHN_16BIT) ? SF_16 : SF_8;
				csf_read_sample(song->samples + n, flags, fp);
			}
		} else {
			for (n = 1; n < MAX_SAMPLES; n++)
				song->samples[n].length = 0;
		}
	}

	if (readflags & MDL_HAS_TRACKS) {
		song_note_t *patnote, *trknote;

		// first off, fix all the instrument numbers to compensate
		// for the screwy envelope craziness
		if (fmtver >> 4) {
			for (trk = 1; trk < 65536 && tracks[trk]; trk++) {
				uint8_t cnote = NOTE_FIRST; // current/last used data

				for (n = 0, trknote = tracks[trk]; n < 256; n++, trknote++) {
					if (NOTE_IS_NOTE(trknote->note)) {
						cnote = trknote->note;
					}
					if (trknote->instrument) {
						// translate it
						trknote->instrument = song->instruments[trknote->instrument]
							? (song->instruments[trknote->instrument]
							   ->sample_map[cnote - 1])
							: 0;
					}
				}
			}
		}

		// "paste" the tracks into the channels
		for (pat = patptr; pat; pat = pat->next) {
			trknote = tracks[pat->track];
			if (!trknote)
				continue;
			patnote = pat->note;
			for (n = 0; n < pat->rows; n++, trknote++, patnote += 64) {
				*patnote = *trknote;
			}
		}
		// and clean up
		for (trk = 1; trk < 65536 && tracks[trk]; trk++)
			free(tracks[trk]);
	}
	while (patptr) {
		pat = patptr;
		patptr = patptr->next;
		free(pat);
	}

	// Finish fixing up the instruments
	for (n = 1; n < MAX_INSTRUMENTS; n++) {
		song_instrument_t *ins = song->instruments[n];
		if (ins) {
			copy_envelope(ins, &ins->vol_env, volenvs, ENV_VOLUME);
			copy_envelope(ins, &ins->pan_env, panenvs, ENV_PANNING);
			copy_envelope(ins, &ins->pitch_env, freqenvs, ENV_PITCH);

			if (ins->flags & ENV_VOLUME) {
				// fix note-fade
				if (!(ins->flags & ENV_VOLLOOP))
					ins->vol_env.loop_start = ins->vol_env.loop_end = ins->vol_env.nodes - 1;
				if (!(ins->flags & ENV_VOLSUSTAIN))
					ins->vol_env.sustain_start = ins->vol_env.sustain_end
						= ins->vol_env.nodes - 1;
				ins->flags |= ENV_VOLLOOP | ENV_VOLSUSTAIN;
			}
			if (ins->fadeout == MDL_FADE_CUT) {
				// fix note-off
				if (!(ins->flags & ENV_VOLUME)) {
					ins->vol_env.ticks[0] = 0;
					ins->vol_env.values[0] = 64;
					ins->vol_env.sustain_start = ins->vol_env.sustain_end = 0;
					ins->flags |= ENV_VOLUME | ENV_VOLSUSTAIN;
					// (the rest is set below)
				}
				int se = ins->vol_env.sustain_end;
				ins->vol_env.nodes = se + 2;
				ins->vol_env.ticks[se + 1] = ins->vol_env.ticks[se] + 1;
				ins->vol_env.values[se + 1] = 0;
				ins->fadeout = 0;
			}

			// set a 1:1 map for each instrument with a corresponding sample,
			// and a blank map for each one that doesn't.
			int note, smp = song->samples[n].data ? n : 0;
			for (note = 0; note < 120; note++) {
				ins->sample_map[note] = smp;
				ins->note_map[note] = note + 1;
			}
		}
	}

	if (readflags & MDL_HAS_VOLENVS) {
		for (n = 0; n < 64; n++)
			free(volenvs[n]);
	}
	if (readflags & MDL_HAS_PANENVS) {
		for (n = 0; n < 64; n++)
			free(panenvs[n]);
	}
	if (readflags & MDL_HAS_FREQENVS) {
		for (n = 0; n < 64; n++)
			free(freqenvs[n]);
	}

	if (restartpos > 0)
		csf_insert_restart_pos(song, restartpos);

	song->flags |= SONG_ITOLDEFFECTS | SONG_COMPATGXX | SONG_INSTRUMENTMODE | SONG_LINEARSLIDES;

	sprintf(song->tracker_id, "Digitrakker %s",
		(fmtver == 0x11) ? "3" // really could be 2.99b -- but close enough for me
		: (fmtver == 0x10) ? "2.3"
		: (fmtver == 0x00) ? "2.0 - 2.2b" // there was no 1.x release
		: "v?.?");

	return LOAD_SUCCESS;
}

