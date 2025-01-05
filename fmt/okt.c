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

#include "player/sndfile.h"

/* --------------------------------------------------------------------- */

int fmt_okt_read_info(dmoz_file_t *file, slurp_t *fp)
{
	unsigned char magic[8];

	if (slurp_length(fp) < 16)
		return 0;

	if (slurp_read(fp, magic, sizeof(magic)) != sizeof(magic)
		|| memcmp(magic, "OKTASONG", sizeof(magic)))
		return 0;

	file->description = "Amiga Oktalyzer";
	/* okts don't have names? */
	file->title = strdup("");
	file->type = TYPE_MODULE_MOD;
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

#define OKT_BLOCK(a,b,c,d) (((a) << 24) | ((b) << 16) | ((c) << 8) | (d))
#define OKT_BLK_CMOD    OKT_BLOCK('C','M','O','D')
#define OKT_BLK_SAMP    OKT_BLOCK('S','A','M','P')
#define OKT_BLK_SPEE    OKT_BLOCK('S','P','E','E')
#define OKT_BLK_SLEN    OKT_BLOCK('S','L','E','N')
#define OKT_BLK_PLEN    OKT_BLOCK('P','L','E','N')
#define OKT_BLK_PATT    OKT_BLOCK('P','A','T','T')
#define OKT_BLK_PBOD    OKT_BLOCK('P','B','O','D')
#define OKT_BLK_SBOD    OKT_BLOCK('S','B','O','D')

struct okt_sample {
	char name[20];
	uint32_t length;
	uint16_t loop_start;
	uint16_t loop_len;
	uint16_t volume;
	uint16_t mode;
};

enum {
	OKT_HAS_CMOD = 1 << 0,
	OKT_HAS_SAMP = 1 << 1,
	OKT_HAS_SPEE = 1 << 2,
	OKT_HAS_PLEN = 1 << 3,
	OKT_HAS_PATT = 1 << 4,
};


/* return: number of channels */
static int okt_read_cmod(song_t *song, slurp_t *fp)
{
	song_channel_t *cs = song->channels;
	int t, cn = 0;

	for (t = 0; t < 4; t++) {
		if (slurp_getc(fp) || slurp_getc(fp))
			cs[cn++].panning = PROTRACKER_PANNING(t);
		cs[cn++].panning = PROTRACKER_PANNING(t);
	}
	for (t = cn; t < 64; t++)
		cs[t].flags |= CHN_MUTE;
	return cn;
}


static void okt_read_samp(song_t *song, slurp_t *fp, uint32_t len, uint32_t smpflag[])
{
	unsigned int n;
	struct okt_sample osmp;
	song_sample_t *ssmp = song->samples + 1;

	if (len % 32)
		log_appendf(4, " Warning: Sample data is misaligned");
	len /= 32;
	if (len >= MAX_SAMPLES) {
		log_appendf(4, " Warning: Too many samples in file");
		len = MAX_SAMPLES - 1;
	}

	for (n = 1; n <= len; n++, ssmp++) {
		slurp_read(fp, &osmp.name, sizeof(osmp.name));
		slurp_read(fp, &osmp.length, sizeof(osmp.length));
		slurp_read(fp, &osmp.loop_start, sizeof(osmp.loop_start));
		slurp_read(fp, &osmp.loop_len, sizeof(osmp.loop_len));
		slurp_read(fp, &osmp.volume, sizeof(osmp.volume));
		slurp_read(fp, &osmp.mode, sizeof(osmp.mode));

		osmp.length = bswapBE32(osmp.length);
		osmp.loop_start = bswapBE16(osmp.loop_start);
		osmp.loop_len = bswapBE16(osmp.loop_len);
		osmp.volume = bswapBE16(osmp.volume);
		osmp.mode = bswapBE16(osmp.mode);

		strncpy(ssmp->name, osmp.name, 20);
		ssmp->name[20] = '\0';
		ssmp->length = osmp.length & ~1; // round down
		if (osmp.loop_len > 2 && osmp.loop_len + osmp.loop_start < ssmp->length) {
			ssmp->sustain_start = osmp.loop_start;
			ssmp->sustain_end = osmp.loop_start + osmp.loop_len;
			if (ssmp->sustain_start < ssmp->length && ssmp->sustain_end < ssmp->length)
				ssmp->flags |= CHN_SUSTAINLOOP;
			else
				ssmp->sustain_start = 0;
		}
		ssmp->loop_start *= 2;
		ssmp->loop_end *= 2;
		ssmp->sustain_start *= 2;
		ssmp->sustain_end *= 2;
		ssmp->volume = MIN(osmp.volume, 64) * 4; //mphack
		smpflag[n] = (osmp.mode == 0 || osmp.mode == 2) ? SF_7 : SF_8;

		ssmp->c5speed = 8287;
		ssmp->global_volume = 64;
	}
}


/* Octalyzer effects list, straight from the internal help (acquired by running "strings octalyzer1.57") --
	- Effects Help Page --------------------------
	1 Portamento Down (4) (Period)
	2 Portamento Up   (4) (Period)
	A Arpeggio 1      (B) (down, orig,   up)
	B Arpeggio 2      (B) (orig,   up, orig, down)
	C Arpeggio 3      (B) (  up,   up, orig)
	D Slide Down      (B) (Notes)
	U Slide Up        (B) (Notes)
	L Slide Down Once (B) (Notes)
	H Slide Up   Once (B) (Notes)
	F Set Filter      (B) <>00:ON
	P Pos Jump        (B)
	S Speed           (B)
	V Volume          (B) <=40:DIRECT
	O Old Volume      (4)   4x:Vol Down      (VO)
				5x:Vol Up        (VO)
				6x:Vol Down Once (VO)
				7x:Vol Up   Once (VO)
Note that 1xx/2xx are apparently inverted from Protracker.
I'm not sure what "Old Volume" does -- continue a slide? reset to the sample's volume? */

/* return: mask indicating effects that aren't implemented/recognized */
static uint32_t okt_read_pbod(song_t *song, slurp_t *fp, int nchn, int pat)
{
	int row, chn, e;
	uint16_t rows;
	song_note_t *note;
	// bitset for effect warnings: (effwarn & (1 << (okteffect - 1)))
	// bit 1 is set if out of range values are encountered (Xxx, Yxx, Zxx, or garbage data)
	uint32_t effwarn = 0;

	slurp_read(fp, &rows, 2);
	rows = bswapBE16(rows);
	rows = CLAMP(rows, 1, 200);

	song->pattern_alloc_size[pat] = song->pattern_size[pat] = rows;
	note = song->patterns[pat] = csf_allocate_pattern(rows);

	for (row = 0; row < rows; row++, note += 64 - nchn) {
		for (chn = 0; chn < nchn; chn++, note++) {
			note->note = slurp_getc(fp);
			note->instrument = slurp_getc(fp);
			e = slurp_getc(fp);
			note->param = slurp_getc(fp);

			if (note->note && note->note <= 36) {
				note->note += 48;
				note->instrument++;
			} else {
				note->instrument = 0; // ?
			}

			/* blah -- check for read error */
			if (e < 0)
				return effwarn;

			switch (e) {
			case 0: // Nothing
				break;

			/* 1/2 apparently are backwards from .mod? */
			case 1: // 1 Portamento Down (Period)
				note->effect = FX_PORTAMENTODOWN;
				note->param &= 0xf;
				break;
			case 2: // 2 Portamento Up (Period)
				note->effect = FX_PORTAMENTOUP;
				note->param &= 0xf;
				break;

#if 0
			/* these aren't like Jxx: "down" means to *subtract* the offset from the note.
			For now I'm going to leave these unimplemented. */
			case 10: // A Arpeggio 1 (down, orig, up)
			case 11: // B Arpeggio 2 (orig, up, orig, down)
				if (note->param)
					note->effect = FX_WEIRDOKTARP;
				break;
#endif

			/* This one is close enough to "standard" arpeggio -- I think! */
			case 12: // C Arpeggio 3 (up, up, orig)
				if (note->param)
					note->effect = FX_ARPEGGIO;
				break;

			case 13: // D Slide Down (Notes)
				if (note->param) {
					note->effect = FX_NOTESLIDEDOWN;
					note->param = 0x10 | MIN(0xf, note->param);
				}
				break;

			case 30: // U Slide Up (Notes)
				if (note->param) {
					note->effect = FX_NOTESLIDEUP;
					note->param = 0x10 | MIN(0xf, note->param);
				}
				break;

			case 21: // L Slide Down Once (Notes)
				/* We don't have fine note slide, but this is supposed to happen once
				per row. Sliding every 5 (non-note) ticks kind of works (at least at
				speed 6), but implementing fine slides would of course be better. */
				if (note->param) {
					note->effect = FX_NOTESLIDEDOWN;
					note->param = 0x50 | MIN(0xf, note->param);
				}
				break;

			case 17: // H Slide Up Once (Notes)
				if (note->param) {
					note->effect = FX_NOTESLIDEUP;
					note->param = 0x50 | MIN(0xf, note->param);
				}
				break;

			case 15: // F Set Filter <>00:ON
				// Not implemented, but let's import it anyway...
				note->effect = FX_SPECIAL;
				note->param = !!note->param;
				break;

			case 25: // P Pos Jump
				note->effect = FX_POSITIONJUMP;
				break;

			case 27: // R Release sample (apparently not listed in the help!)
				note->note = NOTE_OFF;
				note->instrument = note->effect = note->param = 0;
				break;

			case 28: // S Speed
				note->effect = FX_SPEED; // or tempo?
				break;

			case 31: // V Volume
				note->effect = FX_VOLUMESLIDE;
				switch (note->param >> 4) {
				case 4:
					if (note->param != 0x40) {
						note->param &= 0xf; // D0x
						break;
					}
					// 0x40 is set volume -- fall through
				case 0: case 1:
				case 2: case 3:
					note->voleffect = VOLFX_VOLUME;
					note->volparam = note->param;
					note->effect = FX_NONE;
					note->param = 0;
					break;
				case 5:
					note->param = (note->param & 0xf) << 4; // Dx0
					break;
				case 6:
					note->param = 0xf0 | MIN(note->param & 0xf, 0xe); // DFx
					break;
				case 7:
					note->param = (MIN(note->param & 0xf, 0xe) << 4) | 0xf; // DxF
					break;
				default:
					// Junk.
					note->effect = note->param = 0;
					break;
				}
				break;

#if 0
			case 24: // O Old Volume
				/* ? */
				note->effect = FX_VOLUMESLIDE;
				note->param = 0;
				break;
#endif

			default:
				//log_appendf(2, " Pattern %d, row %d: effect %d %02X",
				//        pat, row, e, note->param);
				effwarn |= (e > 32) ? 1 : (1 << (e - 1));
				note->effect = FX_UNIMPLEMENTED;
				break;
			}
		}
	}

	return effwarn;
}

/* --------------------------------------------------------------------------------------------------------- */

int fmt_okt_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	uint8_t tag[8];
	unsigned int readflags = 0;
	uint16_t w; // temp for reading
	int plen = 0; // how many positions in the orderlist are valid
	int npat = 0; // next pattern to read
	int nsmp = 1; // next sample (data, not header)
	int pat, sh, sd, e; // iterators (pattern, sample header, sample data, effect warnings
	int nchn = 0; // how many channels does this song use?
	size_t patseek[MAX_PATTERNS] = {0};
	size_t smpseek[MAX_SAMPLES + 1] = {0}; // where the sample's data starts
	uint32_t smpsize[MAX_SAMPLES + 2] = {0}; // data size (one element bigger to simplify loop condition)
	uint32_t smpflag[MAX_SAMPLES + 1] = {0}; // bit width
	uint32_t effwarn = 0; // effect warning mask

	slurp_read(fp, tag, 8);
	if (memcmp(tag, "OKTASONG", 8) != 0)
		return LOAD_UNSUPPORTED;

	while (!slurp_eof(fp)) {
		uint32_t blklen; // length of this block
		size_t nextpos; // ... and start of next one

		slurp_read(fp, tag, 4);
		slurp_read(fp, &blklen, 4);
		blklen = bswapBE32(blklen);
		nextpos = slurp_tell(fp) + blklen;

		switch (OKT_BLOCK(tag[0], tag[1], tag[2], tag[3])) {
		case OKT_BLK_CMOD:
			if (!(readflags & OKT_HAS_CMOD)) {
				readflags |= OKT_HAS_CMOD;
				nchn = okt_read_cmod(song, fp);
			}
			break;
		case OKT_BLK_SAMP:
			if (!(readflags & OKT_HAS_SAMP)) {
				readflags |= OKT_HAS_SAMP;
				okt_read_samp(song, fp, blklen, smpflag);
			}
			break;
		case OKT_BLK_SPEE:
			if (!(readflags & OKT_HAS_SPEE)) {
				readflags |= OKT_HAS_SPEE;
				slurp_read(fp, &w, 2);
				w = bswapBE16(w);
				song->initial_speed = CLAMP(w, 1, 255);
				song->initial_tempo = 125;
			}
			break;
		case OKT_BLK_SLEN:
			// Don't care.
			break;
		case OKT_BLK_PLEN:
			if (!(readflags & OKT_HAS_PLEN)) {
				readflags |= OKT_HAS_PLEN;
				slurp_read(fp, &w, 2);
				plen = bswapBE16(w);
			}
			break;
		case OKT_BLK_PATT:
			if (!(readflags & OKT_HAS_PATT)) {
				readflags |= OKT_HAS_PATT;
				slurp_read(fp, song->orderlist, MIN(blklen, MAX_ORDERS));
			}
			break;
		case OKT_BLK_PBOD:
			/* Need the channel count (in CMOD) in order to read these */
			if (npat < MAX_PATTERNS) {
				if (blklen > 0)
					patseek[npat] = slurp_tell(fp);
				npat++;
			}
			break;
		case OKT_BLK_SBOD:
			if (nsmp < MAX_SAMPLES) {
				smpseek[nsmp] = slurp_tell(fp);
				smpsize[nsmp] = blklen;
				if (smpsize[nsmp])
					nsmp++;
			}
			break;

		default:
			//log_appendf(4, " Warning: Unknown block of type '%c%c%c%c' at 0x%lx",
			//        tag[0], tag[1], tag[2], tag[3], fp->pos - 8);
			break;
		}

		if (slurp_seek(fp, nextpos, SEEK_SET) != 0) {
			log_appendf(4, " Warning: Failed to seek (file truncated?)");
			break;
		}
	}

	if ((readflags & (OKT_HAS_CMOD | OKT_HAS_SPEE)) != (OKT_HAS_CMOD | OKT_HAS_SPEE))
		return LOAD_FORMAT_ERROR;

	if (!(lflags & LOAD_NOPATTERNS)) {
		for (pat = 0; pat < npat; pat++) {
			slurp_seek(fp, patseek[pat], SEEK_SET);
			effwarn |= okt_read_pbod(song, fp, nchn, pat);
		}

		if (effwarn) {
			if (effwarn & 1)
				log_appendf(4, " Warning: Out-of-range effects (junk data?)");
			for (e = 2; e <= 32; e++) {
				if (effwarn & (1 << (e - 1))) {
					log_appendf(4, " Warning: Unimplemented effect %cxx",
						e + (e < 10 ? '0' : ('A' - 10)));
				}
			}
		}
	}

	if (!(lflags & LOAD_NOSAMPLES)) {
		for (sh = sd = 1; sh < MAX_SAMPLES && smpsize[sd]; sh++) {
			song_sample_t *ssmp = song->samples + sh;
			if (!ssmp->length)
				continue;

			if (ssmp->length != smpsize[sd]) {
				log_appendf(4, " Warning: Sample %d: header/data size mismatch (%" PRIu32 "/%" PRIu32 ")", sh,
					ssmp->length, smpsize[sd]);
				ssmp->length = MIN(smpsize[sd], ssmp->length);
			}

			slurp_seek(fp, smpseek[sd], SEEK_SET);
			csf_read_sample(ssmp, SF_BE | SF_M | SF_PCMS | smpflag[sh], fp);
			sd++;
		}
		// Make sure there's nothing weird going on
		for (; sh < MAX_SAMPLES; sh++) {
			if (song->samples[sh].length) {
				log_appendf(4, " Warning: Sample %d: file truncated", sh);
				song->samples[sh].length = 0;
			}
		}
	}

	song->pan_separation = 64;
	memset(song->orderlist + plen, ORDER_LAST, MAX(0, MAX_ORDERS - plen));
	strcpy(song->tracker_id, "Amiga Oktalyzer");

	return LOAD_SUCCESS;
}

