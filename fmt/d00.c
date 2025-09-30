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
#include "bits.h"
#include "fmt.h"
#include "util.h"
#include "mem.h"
#include "log.h"

/*
#define D00_ENABLE_BROKEN_LEVELPULS

To enable broken levelpuls support.
This is broken because somewhere in the replayer, effects are ignored
after noteoff, and I don't care enough to fix it right now.
*/

struct d00_header {
	unsigned char type;
	unsigned char version;
	unsigned char speed; // apparently this is in Hz? wtf
	unsigned char subsongs; // ignored for now
	unsigned char soundcard;
	unsigned char title[32], author[32], reserved[32];

	// parapointers
	uint16_t tpoin; // not really sure what this is
	uint16_t sequence_paraptr; // patterns
	uint16_t instrument_paraptr; // adlib instruments
	uint16_t info_paraptr; // song message I guess
	uint16_t spfx_paraptr; // points to levpuls on v2 or spfx on v4
	uint16_t endmark; // what?
};

#define READ_VALUE(name) \
	do { if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) { unslurp(fp); return 0; } } while (0)

static int d00_header_read_v1(struct d00_header *hdr, slurp_t *fp)
{
	/* reads old D00 header. this doesn't have a lot of identifying
	 * info, besides some assumptions we make that I am going to abuse */
	if (!slurp_available(fp, 15, SEEK_CUR) || slurp_available(fp, UINT16_MAX, SEEK_SET))
		return 0;

	READ_VALUE(version);
	READ_VALUE(speed);
	READ_VALUE(subsongs);
	READ_VALUE(tpoin);
	READ_VALUE(sequence_paraptr);
	READ_VALUE(instrument_paraptr);
	READ_VALUE(info_paraptr);
	if (hdr->version > 0) /* v0: no levelpuls */
		READ_VALUE(spfx_paraptr);
	READ_VALUE(endmark);

	return 1;
}

static int d00_header_read_new(struct d00_header *hdr, slurp_t *fp)
{
	// we check if the length is larger than UINT16_MAX because
	// the parapointers wouldn't be able to fit all of the bits
	// otherwise. 119 is just the size of the header.
	unsigned char magic[6];

	if (!slurp_available(fp, 119, SEEK_CUR) || slurp_available(fp, UINT16_MAX, SEEK_SET))
		return 0;

	if ((slurp_read(fp, magic, 6) != 6)
			|| memcmp(magic, "JCH\x26\x02\x66", 6))
		return 0;

	READ_VALUE(type);

	READ_VALUE(version);
	if (hdr->version & 0x80) {
		/* from adplug: "reheadered old-style song" */
		slurp_seek(fp, 0x6B, SEEK_SET);

		return d00_header_read_v1(hdr, fp);
	}

	READ_VALUE(speed);

	/* > EdLib always sets offset 0009h to 01h. You cannot make more than
	 * > one piece of music at a time in the editor. */
	READ_VALUE(subsongs);
	READ_VALUE(soundcard);
	READ_VALUE(title);
	READ_VALUE(author);
	READ_VALUE(reserved);

	READ_VALUE(tpoin);
	READ_VALUE(sequence_paraptr);
	READ_VALUE(instrument_paraptr);
	READ_VALUE(info_paraptr);
	READ_VALUE(spfx_paraptr);
	READ_VALUE(endmark);

	return 1;
}

static int d00_header_read(struct d00_header *hdr, slurp_t *fp)
{
	/* this function is responsible for reading and verification
	 * most of this verification isn't useful for v2-v4 d00 files,
	 * but v0-v1 d00 files have virtually no identifying information
	 * if they haven't been reheadered. */
	int64_t fppos;
	uint64_t fplen;
	static int (*const hdr_style[])(struct d00_header *hdr, slurp_t *fp) = {
		/* in order of preference */
		d00_header_read_new,
		d00_header_read_v1,
		NULL
	};
	size_t i;

	for (i = 0; /* nothing */; i++) {
		/* end of list? */
		if (!hdr_style[i])
			return 0;

		slurp_rewind(fp);
		memset(hdr, 0, sizeof(*hdr));
		if (hdr_style[i](hdr, fp))
			break; /* SUCCESS! */
	}

	/* should always be zero */
	if (hdr->type)
		return 0;

	if (hdr->version > 4)
		return 0;

	/* ignore files with more than one subsong.
	 * we can't handle them anyway. */
	if (hdr->subsongs != 1)
		return 0;

	hdr->tpoin = bswapLE16(hdr->tpoin);
	hdr->sequence_paraptr = bswapLE16(hdr->sequence_paraptr);
	hdr->instrument_paraptr = bswapLE16(hdr->instrument_paraptr);
	hdr->info_paraptr = bswapLE16(hdr->info_paraptr);
	hdr->spfx_paraptr = bswapLE16(hdr->spfx_paraptr);
	hdr->endmark = bswapLE16(hdr->endmark);

	/* validate the parapointers */
	fppos = slurp_tell(fp);
	if (fppos < 0)
		return 0; /* wut */

	/* verify that parapointers are within range for the file */
#define PARAPTR_VALID(ptr) \
	(fppos <= ptr || slurp_available(fp, ptr, SEEK_CUR))

	/* these can never be invalid */
	if (!PARAPTR_VALID(hdr->tpoin)
		    || !PARAPTR_VALID(hdr->info_paraptr)
		    || !PARAPTR_VALID(hdr->instrument_paraptr)
		    || !PARAPTR_VALID(hdr->sequence_paraptr))
		return 0;

	/* this pointer is used differently depending on the version.
	 * some versions have nothing! */
	switch (hdr->version) {
	case 1:
	case 2:
		/* Levelpuls */
		SCHISM_FALLTHROUGH;
	case 4:
		/* SPFX */
		if (!PARAPTR_VALID(hdr->spfx_paraptr))
			return 0;
		break;
	}

#undef PARAPTR_VALID

	/* endmark should ALWAYS be 0xFFFF
	 * at this point, there's a fairly good chance we're working with
	 * a v0 or v1 d00 file. */
	if (hdr->endmark != 0xFFFF)
		return 0;

	/* AdPlug: overwrite speed with 70hz if version is 0 */
	if (!hdr->version)
		hdr->speed = 70;

	{
		/* do some additional verification of the tpoin stuff.
		 * this is likely a very weird edge case, but... */
		int64_t start;
		uint16_t ptrs[9];
		size_t j;

		start = slurp_tell(fp);
		if (start < 0)
			return 0; /* oops */

		slurp_seek(fp, hdr->tpoin, SEEK_SET);
		if (slurp_read(fp, ptrs, sizeof(ptrs)) != sizeof(ptrs))
			return 0;

		for (j = 0; j < 9; j++) {
			ptrs[j] = bswapLE16(ptrs[j]);
			if (ptrs[j] >= fplen)
				return 0;
		}

		slurp_seek(fp, start, SEEK_SET);
	}

	return 1;
}

#undef READ_VALUE

int fmt_d00_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct d00_header hdr;
	if (!d00_header_read(&hdr, fp))
		return 0;

	file->title = strn_dup((const char *)hdr.title, sizeof(hdr.title));
	file->description = "EdLib Tracker D00";
	file->type = TYPE_MODULE_S3M;
	return 1;
}

/* ------------------------------------------------------------------------ */

/* EdLib D00 loader.
 *
 * Loosely based off the AdPlug code, written by
 * Simon Peter <dn.tlp@gmx.net> */

static void hz_to_speed_tempo(unsigned char ver, uint16_t hz, uint32_t *pspeed, uint32_t *ptempo)
{
	if (ver >= 3) {
		/* have to do a bit more work here...
		 *
		 * this is really just a guesstimate.
		 * "AAAAARGGGHHH" BPM is 131-ish, and the hz value is 32.
		 * hence we just multiple by 131/32. :) */
		double tempo;
		uint32_t speed;

		speed = 3u;
		tempo = (hz * (131.0 / 32.0));

		while (tempo > 255.0) {
			/* divide until we get a valid tempo */
			speed *= 2;
			tempo /= 2;
		}

		*pspeed = speed;
		*ptempo = round(tempo);
	} else {
		/* hz is basically just speed. */
		*pspeed = hz;
		*ptempo = 131;
	}
}

static int uint16_compare(const void *a, const void *b)
{
	int32_t aa = *(uint16_t *)a;
	int32_t bb = *(uint16_t *)b;

	int32_t r = (aa - bb);

	return CLAMP(r, INT_MIN, INT_MAX);
}

#define D00_PATTERN_ROWS 64

static song_note_t *d00_get_note(song_t *song, uint32_t pattern, uint32_t row,
	uint32_t chn)
{
	if (!song->patterns[pattern]) {
		/* allocate the pattern data, if it's not there already */
		song->patterns[pattern] = csf_allocate_pattern(D00_PATTERN_ROWS);
		song->pattern_size[pattern] = song->pattern_alloc_size[pattern]
			= D00_PATTERN_ROWS;
	}

	return song->patterns[pattern] + (row * MAX_CHANNELS) + chn;
}

static void d00_fix_row(uint32_t *pattern, uint32_t *row)
{
	while (*row >= D00_PATTERN_ROWS) {
		(*pattern)++;
		(*row) -= D00_PATTERN_ROWS;
	}
}

enum {
	D00_WARN_EXPERIMENTAL,
	D00_WARN_SPFX,
#ifndef D00_ENABLE_BROKEN_LEVELPULS
	D00_WARN_LEVELPULS,
#endif
	D00_WARN_FINETUNE,
	D00_WARN_ORDERSPEED,
	/* ... */

	D00_WARN_MAX_
};

const char *d00_warn_names[D00_WARN_MAX_] = {
	[D00_WARN_EXPERIMENTAL] = "D00 loader is experimental at best",
	[D00_WARN_SPFX] = "SPFX not implemented",
#ifndef D00_ENABLE_BROKEN_LEVELPULS
	[D00_WARN_LEVELPULS] = "Levelpuls not implemented",
#endif
	[D00_WARN_FINETUNE] = "Instrument finetune ignored",
	[D00_WARN_ORDERSPEED] = "Order speed ignored",
};

#define EDLIBVOLTOITVOL(x) \
	((63 - ((x) & 63)) * 64 / 63)

#ifdef D00_ENABLE_BROKEN_LEVELPULS
/* return of 0 means catastrophic failure */
static int d00_load_levelpuls(uint16_t paraptr, int tunelev,
	song_instrument_t *ins, song_sample_t *smp, slurp_t *fp)
{
	/* FIXME: The first iteration of the tunelev takes in a "timer",
	 * which is weird-speak for sustain for given amount of ticks. */
	BITARRAY_DECLARE(loop, 255);
	uint32_t i;
	int32_t x = 0;
	int64_t start = slurp_tell(fp);

	BITARRAY_ZERO(loop);

	for (i = 0; i < ARRAY_SIZE(ins->vol_env.ticks); i++) {
		int level, duration;

		if (tunelev == 0xFF)
			break; /* end of levelpuls. FIXME i think we need to loop this point */

		slurp_seek(fp, paraptr + tunelev * 4, SEEK_SET);

		BITARRAY_SET(loop, tunelev);

		level = slurp_getc(fp);
		if (level == EOF)
			return 0;

		/* int8_t voladd; -- ignored */

		duration = slurp_getc(fp);
		if (duration == EOF)
			return 0;

		ins->vol_env.ticks[i] = x;

		if (level != 0xFF) {
			ins->vol_env.values[i] = EDLIBVOLTOITVOL(level);
		} else if (i > 0) {
			/* total guesstimate */
			ins->vol_env.values[i] = ins->vol_env.values[i - 1];
		} else {
			ins->vol_env.values[i] = 64;
		}

		x += MAX(duration / 20, 1) + 1;

		tunelev = slurp_getc(fp);
		if (tunelev == EOF)
			return 0;

		if (BITARRAY_ISSET(loop, tunelev))
			/* XXX set loop here */
			break;
	}

	ins->vol_env.nodes = MAX(i, 2);

	slurp_seek(fp, start, SEEK_SET);

	/* I guess */
	return 1;
}
#endif

int fmt_d00_load_song(song_t *song, slurp_t *fp,
	SCHISM_UNUSED uint32_t lflags)
{
	int c;
	int ninst = 0;
	uint16_t speeds[10];
	BITARRAY_DECLARE(warn, D00_WARN_MAX_);
	struct d00_header hdr;
	int msgp;

	if (!d00_header_read(&hdr, fp))
		return LOAD_UNSUPPORTED;

	BITARRAY_ZERO(warn);
	BITARRAY_SET(warn, D00_WARN_EXPERIMENTAL);

	speeds[0] = hdr.speed;

	memcpy(song->title, hdr.title, 31);
	song->title[31] = '\0';

	/* read in song info (message).
	 * this is USUALLY simply "\xFF\xFF" (end marking) but in some
	 * old songs (e.g. "the alibi.d00") it contains real data */
	slurp_seek(fp, hdr.info_paraptr, SEEK_SET);
	for (msgp = 0; msgp < MAX_MESSAGE; ) {
		int ch;

		ch = slurp_getc(fp);
		if (ch == EOF)
			/* some old files actually have this case.
			 * (platest.d00) */
			break;

		if (ch == 0xFF) {
			int ch2;

			ch2 = slurp_getc(fp);
			if (ch2 == EOF)
				break;

			if (ch2 == 0xFF) {
				break; /* message end */
			} else {
				song->message[msgp++] = ch;
				if (msgp >= MAX_MESSAGE) break;
				song->message[msgp++] = ch2;
			}
		} else {
			song->message[msgp++] = ch;
		}
	}

	{
		/* handle pattern/order stuff
		 *
		 * EdLib tracker is sort of odd, in which patterns (and orders) can
		 * really be any arbitrary length. What I've decided to do to "combat"
		 * this is to make each pattern 64 rows long, and just keep pasting
		 * the pattern data. This will probably result in very weird looking
		 * patterns from an editor standpoint, but for many mods it will
		 * probably work fine. */
		uint16_t ptrs[9];
		uint8_t volume[9];
		uint32_t max_pattern = 0, max_row = 0;

		slurp_seek(fp, hdr.tpoin, SEEK_SET);

		if (slurp_read(fp, ptrs, sizeof(ptrs)) != sizeof(ptrs))
			return LOAD_UNSUPPORTED;
		if (slurp_read(fp, volume, sizeof(volume)) != sizeof(volume))
			return LOAD_UNSUPPORTED;

		for (c = 0; c < 9; c++) {
			/* ... sigh */
			int16_t ord_transpose[MAX_ORDERS] = {0};
			int transpose_set = 0; /* stupid hack */
			uint16_t ords[MAX_ORDERS - 2]; /* need space for ORDER_LAST */
			uint16_t patt_paraptr;
			uint16_t mem_instr = 0; /* current instrument for the channel */
			uint16_t mem_volfx = VOLFX_NONE;
			uint16_t mem_volparam = 0;
			uint8_t mem_effect = FX_NONE;
			uint8_t mem_param = 0;
			uint32_t n, nords;
			uint32_t pattern, row;

			ptrs[c] = bswapLE16(ptrs[c]);

			if (ptrs[c] != 0) {
				// I think this actually just adds onto the existing volume,
				// instead of averaging them together ???????
				song->channels[c].volume = EDLIBVOLTOITVOL(volume[c]);
			} else {
				song->channels[c].flags |= CHN_MUTE;
				continue; /* wut */
			}

			slurp_seek(fp, ptrs[c], SEEK_SET);

			if (slurp_read(fp, &speeds[c + 1], 2) != 2)
				continue;

			speeds[c + 1] = bswapLE16(speeds[c + 1]);

			for (nords = 0; nords < ARRAY_SIZE(ords); /* nothing */) {
				if (slurp_read(fp, &ords[nords], 2) != 2)
					break;

				ords[nords] = bswapLE16(ords[nords]);

				if (ords[nords] == 0xFFFF || ords[nords] == 0xFFFE) {
					break;
				} else if (ords[nords] >= 0x9000) {
					/* set speed -- IGNORED for now */
					BITARRAY_SET(warn, D00_WARN_ORDERSPEED);
					continue;
				} else if (ords[nords] >= 0x8000) {
					ord_transpose[nords] = (ords[nords] & 0xff);

					if (ords[nords] & 0x100) // sign bit
						ord_transpose[nords] = -ord_transpose[nords];

					transpose_set = 1;

					continue;
				} else {
					/* this is a real order! */
					//log_appendf(1, "ord[%d] = %d", nords, ord_transpose[nords]);
					if (!transpose_set && nords > 0)
						ord_transpose[nords] = ord_transpose[nords - 1];
					transpose_set = 0;
					nords++;
				}
			}

			/* avoid possible divide-by-zero (phase.d00) */
			if (!nords)
				continue;

			pattern = 0;
			row = 0;

			for (n = 0; pattern < MAX_PATTERNS; /* WHATS IN THE BOX? */) {
				/* mental gymnastics to find the pattern paraptr */
				slurp_seek(fp, hdr.sequence_paraptr + (ords[n % nords] * 2), SEEK_SET);

				if (slurp_read(fp, &patt_paraptr, 2) != 2)
					return LOAD_UNSUPPORTED;

				patt_paraptr = bswapLE16(patt_paraptr);

				slurp_seek(fp, patt_paraptr, SEEK_SET);

				for (; pattern < MAX_PATTERNS; d00_fix_row(&pattern, &row)) {
					uint16_t event;
					song_note_t *sn;
					uint16_t count = 0;

D00_readnote: /* this goto is kind of ugly... */
					if (slurp_read(fp, &event, 2) != 2)
						break;

					event = bswapLE16(event);

					/* end of pattern? */
					if (event == 0xFFFF) {
						n++;
						break;
					}

					sn = d00_get_note(song, pattern, row, c);

					if (event < 0x4000) {
						int note = (event & 0xFF);
						int r;

						if (hdr.version > 0)
							count = (event >> 8);

						/* note event; data is stored in the low byte */
						switch (note & 0x7F) {
						case 0: /* "REST" */
							sn->note = NOTE_OFF;
							row += count + 1;
							break;
						case 0x7E: /* "HOLD" */
							/* copy the last effect... */
							for (r = 0; pattern < MAX_PATTERNS && r <= count; r++, row++, d00_fix_row(&pattern, &row)) {
								sn = d00_get_note(song, pattern, row, c);

								sn->effect = mem_effect;
								sn->param = mem_param;
							}
							break;
						default:
							/* reset fx (does v0 do this?)
							 * ALSO: is this even right ?? lol */
							mem_effect = FX_NONE;
							mem_param = 0;

							if (hdr.version > 0) {
								/* 0x80 flag == ignore channel transpose */
								if (note & 0x80) {
									note -= 0x80;
								} else {
									note += ord_transpose[n % nords];
								}

								if (count >= 0x20) {
									/* "tied note" */
									if (sn->effect == FX_NONE) {
										sn->effect = FX_TONEPORTAMENTO;
										sn->param = 0xFF;
									}
									count -= 0x20;
								}
							} else {
								/* note handling for v0 */
								if (count < 2) /* unlocked note */
									note += ord_transpose[n % nords];
							}

							sn->note = note + NOTE_FIRST + 12;
							sn->instrument = mem_instr;
							sn->voleffect = mem_volfx;
							sn->volparam = mem_volparam;

							row += count + 1;

							break;
						}
						continue;
					} else {
						/* it's probably possible to have multiple effects
						 * on one track. we should be able to handle this! */
						uint8_t fx = (event >> 12);
						uint16_t fxop = (event & 0x0FFF);

						switch (fx) {
						case 6: /* Cut/Stop Voice */
							sn->note = NOTE_CUT;
							row += fxop + 1;
							continue;
						case 7: /* Vibrato */
							mem_effect = FX_VIBRATO;
							/* these are flipped in the fxop */
							{
								/* this is a total guess, mostly just based
								 * on what sounds "correct" */
								uint8_t depth = ((fxop >> 8) & 0xFF) * 4 / 3;
								uint8_t speed = (fxop & 0xFF) * 4 / 3;

								depth = MIN(depth, 0xF);
								speed = MIN(speed, 0xF);

								mem_param = (speed << 4) | depth;
							}
							break;
						case 8: /* Duration (v0) */
							if (hdr.version == 0)
								count = fxop;
							break;
						case 9: /* New Level (in layman's terms, volume) */
							mem_volfx = VOLFX_VOLUME;
							/* volume is backwards, WTF */
							mem_volparam = EDLIBVOLTOITVOL(fxop);
							break;
						case 0xB: /* Set spfx (need to handle this appropriately...) */
							if (hdr.version == 4) {
								/* SPFX is a linked list.
								 *
								 * Yep; there's a `ptr` value within the structure, which
								 * points to the next spfx structure to process. This is
								 * terrible for us, but we can at least haphazardly
								 * grab the instrument number from the first one, and
								 * hope it fits... */
								int64_t oldpos = slurp_tell(fp);

								slurp_seek(fp, hdr.spfx_paraptr + fxop, SEEK_SET);

								slurp_read(fp, &mem_instr, 2);
								mem_instr = bswapLE16(mem_instr);
								ninst = MAX(ninst, mem_instr);

								/* other values:
								 *  - int8_t halfnote;
								 *  - uint8_t modlev;
								 *  - int8_t modlevadd;
								 *  - uint8_t duration;
								 *  - uint16_t ptr; (seriously?)
								 * it's likely that we can transform these into an instrument. */

								slurp_seek(fp, oldpos, SEEK_SET);
							}
							BITARRAY_SET(warn, D00_WARN_SPFX);
							break;
						case 0xC: /* Set instrument */
							mem_instr = fxop + 1;
							ninst = MAX(ninst, fxop + 1);
							break;
						case 0xD: /* Pitch slide up */
							mem_effect = FX_PORTAMENTOUP;
							mem_param = fxop * 5 / 2;
							break;
						case 0xE: /* Pitch slide down */
							mem_effect = FX_PORTAMENTODOWN;
							mem_param = fxop * 5 / 2;
							break;
						default:
							break;
						}

						/* if we're here, the event is incomplete */
						goto D00_readnote;
					}
				}

				/* FIXME: this isn't perfect; "like galway.d00" adds some
				 * weird stuff on the end */
				if (n == nords) {
					if (max_pattern < pattern) {
						max_pattern = pattern;
						max_row = row;
					} else if (max_pattern == pattern && max_row < row) {
						max_row = row;
					}
				}
			}
		}

		//log_appendf(1, "pat: %d, row: %d", max_pattern, max_row);

		/* fixup max patterns / rows (off by one?) */
		if (!max_row) {
			max_pattern--;
			max_row = D00_PATTERN_ROWS - 1;
		} else {
			max_row--;
		}

		/* now, clean up the giant mess we've made.
		 *
		 * FIXME: don't make a giant mess to begin with :) */

		if (max_pattern + 1 < MAX_PATTERNS) {
			for (c = max_pattern + 1; c < MAX_PATTERNS; c++) {
				csf_free_pattern(song->patterns[c]);
				song->patterns[c] = NULL;
				song->pattern_size[c] = song->pattern_alloc_size[c] = 64;
			}
		}

#if 0
		if (song->pattern_size[max_pattern] != max_row) {
			/* insert an effect to jump back to the start
			 * this effect may be on the 10th channel if we can't
			 * fit it anywhere else. */
			song_note_t *row = song->patterns[max_pattern] + (max_row * MAX_CHANNELS);

			for (c = 0; c < MAX_CHANNELS; c++) {
				if (row[c].effect != FX_NONE)
					continue;

				row[c].effect = FX_POSITIONJUMP;
				row[c].param = 0;
				break;
			}
		}
#else
		song->pattern_size[max_pattern] = max_row + 1;
#endif

		for (c = 0; c <= (int)max_pattern; c++)
			song->orderlist[c] = c;
		song->orderlist[max_pattern + 1] = ORDER_LAST;
	}

	/* -------------------------------------------------------------------- */
	/* find the most common speed, and use it */

	{
		/* FIXME: this isn't very good, we should be doing per-channel
		 * speed stuff or else we get broken modules
		 *
		 * basically each channel ought to have an increment. if each speed
		 * is the same this is okay, and we can probably just ignore the
		 * "song speed" altogether. though i don't know how many songs
		 * actually use different speeds for each channel, and such
		 * songs are likely incredibly rare. */
		int max_count = 1, count = 1;
		uint16_t mode = speeds[0];

		qsort(speeds, 10, sizeof(uint16_t), uint16_compare);

		for (c = 1; c < 10; c++) {
			count = (speeds[c] == speeds[c - 1])
				? (count + 1)
				: 1;

			if (count > max_count) {
				max_count = count;
				mode = speeds[c];
			}
		}


#if 0
		log_appendf(1, "mode: %u", mode);
		for (c = 0; c < 10; c++)
			log_appendf(1, "speeds[%d] = %u", c, speeds[c]);
#endif

		hz_to_speed_tempo(hdr.version, mode, &song->initial_speed, &song->initial_tempo);
	}

	/* start reading instrument data */

	if (slurp_seek(fp, hdr.instrument_paraptr, SEEK_SET))
		return LOAD_UNSUPPORTED;

	ninst = MIN(ninst, MAX_SAMPLES);

	/* enable instrument mode so we can read in levelpuls info */
#ifdef D00_ENABLE_BROKEN_LEVELPULS
	if (hdr.version == 1 || hdr.version == 2)
		song->flags |= SONG_INSTRUMENTMODE;
#endif

	for (c = 0; c < ninst; c++) {
		unsigned char bytes[11];
		song_sample_t *smp = song->samples + c + 1;
#ifdef D00_ENABLE_BROKEN_LEVELPULS
		song_instrument_t *inst;
#endif
		int tunelev;

		if (slurp_read(fp, bytes, 11) != 11)
			continue; /* wut? */

		/* Internally, we expect a different order for the bytes than
		 * what D00 files provide. Shift them around accordingly. */

		smp->adlib_bytes[0] = bytes[8];
		smp->adlib_bytes[1] = bytes[3];
		/* NOTE: AdPlug doesn't use these two bytes. */
		smp->adlib_bytes[2] = bytes[7];
		smp->adlib_bytes[3] = bytes[2];
		smp->adlib_bytes[4] = bytes[5];
		smp->adlib_bytes[5] = bytes[0];
		smp->adlib_bytes[6] = bytes[6];
		smp->adlib_bytes[7] = bytes[1];
		smp->adlib_bytes[8] = bytes[9];
		smp->adlib_bytes[9] = bytes[4];
		smp->adlib_bytes[10] = bytes[10];

		smp->flags |= CHN_ADLIB;

		/* dumb hackaround that ought to some day be fixed */
		smp->data = csf_allocate_sample(1);
		smp->length = 1;

#ifdef D00_ENABLE_BROKEN_LEVELPULS
		song->instruments[c + 1] = inst = csf_allocate_instrument();
		csf_init_instrument(inst, c + 1);
		inst->fadeout = 256ul << 5;
#endif

		tunelev = slurp_getc(fp);
		if (tunelev == EOF)
			break; /* truncated file?? */

		switch (hdr.version) {
		case 1:
		case 2:
			if (tunelev == 0xFF)
				break;
#ifdef D00_ENABLE_BROKEN_LEVELPULS
			if (d00_load_levelpuls(hdr.spfx_paraptr, tunelev, inst, smp, fp) == 0)
				return LOAD_FORMAT_ERROR; /* WUT */
#else
			BITARRAY_SET(warn, D00_WARN_LEVELPULS);
#endif
			break;
		case 4:
			BITARRAY_SET(warn, D00_WARN_FINETUNE);
			break;
		default:
			/* just padding? */
			break;
		}


		smp->volume = 64 * 4; //mphack

		/* It's probably safe to ignore these (?)
		 * I think Adplug also ignores these values if SPFX isn't used ... */
#if 0
		log_appendf(1, "timer: %d", slurp_getc(fp));
		log_appendf(1, "sr: %d", slurp_getc(fp));
		log_appendf(1, "unknown bytes: %d, %d", slurp_getc(fp), slurp_getc(fp));
		log_nl();
#else
		slurp_seek(fp, 1, SEEK_CUR); /* "timer" */
		slurp_seek(fp, 1, SEEK_CUR); /* "sr" */
		slurp_seek(fp, 2, SEEK_CUR); /* unknown bytes (padding, probably) */
#endif
	}

	/* TODO for older D00 files we can use the levelpuls structure
	 * to create an instrument that plays stuff properly */

	for (c = 9; c < MAX_CHANNELS; c++)
		song->channels[c].flags |= CHN_MUTE;

	if (hdr.version == 4)
		strncpy(song->tracker_id, "EdLib Tracker", sizeof(song->tracker_id) - 1);
	else
		snprintf(song->tracker_id, sizeof(song->tracker_id), "Unknown AdLib Tracker, file version %d", (int)hdr.version);
	/* otherwise... it's probably some random tracker */

	for (c = 0; c < D00_WARN_MAX_; c++)
		if (BITARRAY_ISSET(warn, c))
			log_appendf(4, " Warning: %s", d00_warn_names[c]);

	return LOAD_SUCCESS;
}
