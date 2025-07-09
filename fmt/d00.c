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
#include "fmt.h"
#include "util.h"
#include "mem.h"
#include "log.h"

struct d00_header {
	unsigned char id[6];
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

// This function, like many of the other read functions, also
// performs sanity checks on the data itself.
static int d00_header_read(struct d00_header *hdr, slurp_t *fp)
{
	// we check if the length is larger than UINT16_MAX because
	// the parapointers wouldn't be able to fit all of the bits
	// otherwise. 119 is just the size of the header.
	const uint64_t fplen = slurp_length(fp);
	if (fplen <= 119 || fplen > UINT16_MAX)
		return 0;

#define READ_VALUE(name) \
	do { if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) { unslurp(fp); return 0; } } while (0)

	READ_VALUE(id);
	if (memcmp(hdr->id, "JCH\x26\x02\x66", sizeof(hdr->id)))
		return 0;

	/* this should always be zero? */
	READ_VALUE(type);
	if (hdr->type)
		return 0;

	/* TODO: handle other versions */
	READ_VALUE(version);
	if (hdr->version != 4)
		return 0;

	READ_VALUE(speed);

	/* > EdLib always sets offset 0009h to 01h. You cannot make more than
	 * > one piece of music at a time in the editor. */
	READ_VALUE(subsongs);
	if (hdr->subsongs != 1)
		return 0;

	READ_VALUE(soundcard);
	if (hdr->soundcard != 0)
		return 0;

	READ_VALUE(title);
	READ_VALUE(author);
	READ_VALUE(reserved);

	READ_VALUE(tpoin);
	hdr->tpoin = bswapLE16(hdr->tpoin);
	READ_VALUE(sequence_paraptr);
	hdr->sequence_paraptr = bswapLE16(hdr->sequence_paraptr);
	READ_VALUE(instrument_paraptr);
	hdr->instrument_paraptr = bswapLE16(hdr->instrument_paraptr);
	READ_VALUE(info_paraptr);
	hdr->info_paraptr = bswapLE16(hdr->info_paraptr);
	READ_VALUE(spfx_paraptr);
	hdr->spfx_paraptr = bswapLE16(hdr->spfx_paraptr);
	READ_VALUE(endmark);
	hdr->endmark = bswapLE16(hdr->endmark);

	// verify the parapointers
	if (hdr->tpoin < 119
		|| hdr->sequence_paraptr < 119
		|| hdr->instrument_paraptr < 119
		|| hdr->info_paraptr < 119
		|| hdr->spfx_paraptr < 119
		|| hdr->endmark < 119)
		return 0;

#undef READ_VALUE

	return 1;
}

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

static void hz_to_speed_tempo(uint32_t hz, uint32_t *pspeed, uint32_t *ptempo)
{
	/* "close enough" calculation; based on known values ;)
	 *
	 * "AAAAARGGGHHH" BPM is 131-ish, and the Hz is 32.
	 * 131/32 is a little over 4. */
	*pspeed = 3;
	*ptempo = (hz * 4);

	while (*ptempo > 255) {
		/* eh... */
		*pspeed *= 2;
		*ptempo /= 2;
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
	/* ... */

	D00_WARN_MAX_
};

const char *d00_warn_names[D00_WARN_MAX_] = {
	[D00_WARN_EXPERIMENTAL] = "D00 loader is experimental at best",
	[D00_WARN_SPFX] = "SPFX effects not implemented",
};

int fmt_d00_load_song(song_t *song, slurp_t *fp,
	SCHISM_UNUSED unsigned int lflags)
{
	int c;
	int ninst = 0;
	uint16_t speeds[10];
	uint32_t warn = (1 << D00_WARN_EXPERIMENTAL);
	struct d00_header hdr;

	if (!d00_header_read(&hdr, fp))
		return LOAD_UNSUPPORTED;

	memcpy(song->title, hdr.title, 31);
	song->title[31] = '\0';

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
			int ord_transpose[MAX_ORDERS] = {0};
			int transpose_set = 0; /* stupid hack */
			uint16_t ords[MAX_ORDERS];
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
				//song->channels[c].volume = volume[c];
			} else {
				song->channels[c].flags |= CHN_MUTE;
			}

			slurp_seek(fp, ptrs[c], SEEK_SET);

			if (slurp_read(fp, &speeds[c + 1], 2) != 2)
				continue;

			speeds[c + 1] = bswapLE32(speeds[c + 1]);

			for (nords = 0; nords < ARRAY_SIZE(ords); /* nothing */) {
				if (slurp_read(fp, &ords[nords], 2) != 2)
					break;

				ords[nords] = bswapLE16(ords[nords]);

				if (ords[nords] == 0xFFFF || ords[nords] == 0xFFFE) {
					break;
				} else if (ords[nords] >= 0x9000) {
					/* set speed -- IGNORED for now */
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
						int count = (event >> 8);
						int r;

						/* note event; data is stored in the low byte */
						switch (note) {
						case 0: /* "REST" */
						case 0x80: /* "REST" & 0x80 */
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
							/* 0x80 flag == ignore channel transpose */
							if (note & 0x80) {
								note -= 0x80;
							} else {
								note += ord_transpose[n % nords];
							}

							mem_effect = FX_NONE;
							mem_param = 0;

							sn->note = note + NOTE_FIRST + 12;
							sn->instrument = mem_instr;
							sn->voleffect = mem_volfx;
							sn->volparam = mem_volparam;

							if (count >= 0x20) {
								/* "tied note" */
								if (sn->effect == FX_NONE) {
									sn->effect = FX_TONEPORTAMENTO;
									sn->param = 0xFF;
								}
								count -= 0x20;
							}

							row += count + 1;

							break;
						}
						continue;
					} else {
						uint8_t fx = (event >> 12);
						uint16_t fxop = (event & 0x0FFF);

						switch (fx) {
						case 6: /* Cut/Stop Voice */
							sn->note = NOTE_CUT;
							continue;
						case 7: /* Vibrato */
							mem_effect = FX_VIBRATO;
							/* these are flipped in the fxop */
							{
								/* this is a total guess, mostly just based
								 * on what sounds "correct" */
								uint8_t depth = ((fxop >> 8) & 0xFF) * 2 / 3;
								uint8_t speed = (fxop & 0xFF) * 2 / 3;

								depth = MIN(depth, 0xF);
								speed = MIN(speed, 0xF);

								mem_param = (speed << 4) | depth;
							}
							break;
						case 9: /* New Level (in layman's terms, volume) */
							mem_volfx = VOLFX_VOLUME;
							/* volume is backwards, WTF */
							mem_volparam = (63 - (fxop & 63)) * 64 / 63;
							break;
						case 0xB: /* Set spfx (need to handle this appropriately...) */
							{
								/* SPFX is a linked list.
								 *
								 * Yep; there's a `ptr` value within the structure, which
								 * points to the next spfx structure to process. This is
								 * terrible for us, but we can at least haphazardly
								 * grab the instrument number from the first one, and
								 * hope it fits...
								 *
								 * FIXME: The other things in */
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
								 *  - uint16_t ptr; (seriously?) */

								slurp_seek(fp, oldpos, SEEK_SET);
							}
							warn |= (1 << D00_WARN_SPFX);
							break;
						case 0xC: /* Set instrument */
							mem_instr = fxop + 1;
							ninst = MAX(ninst, fxop + 1);
							break;
						case 0xD: /* Pitch slide up */
							mem_effect = FX_PORTAMENTOUP;
							mem_param = fxop;
							break;
						case 0xE: /* Pitch slide down */
							mem_effect = FX_PORTAMENTODOWN;
							mem_param = fxop;
							break;
						default:
							break;
						}

						/* if we're here, the event is incomplete */
						goto D00_readnote;
					}
				}

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

		/* now, clean up the giant mess we've made.
		 *
		 * FIXME: don't make a giant mess to begin with :) */

		if (max_pattern + 1 < MAX_PATTERNS) {
			for (c = max_pattern + 1; c < MAX_PATTERNS; c++) {
				csf_free_pattern(song->patterns[c]);
				song->patterns[c] = NULL;
				song->pattern_size[c] = 64;
			}
		}

		if (song->pattern_size[max_pattern] != max_row) {
			/* insert an effect to jump back to the start */
			song_note_t *row = song->patterns[max_pattern] + (max_row * MAX_CHANNELS);

			for (c = 0; c < MAX_CHANNELS; c++) {
				if (row[c].effect != FX_NONE)
					continue;

				row[c].effect = FX_POSITIONJUMP;
				row[c].param = 0;
			}
		}

		for (c = 0; c < (int)max_pattern; c++)
			song->orderlist[c] = c;
		song->orderlist[max_pattern] = ORDER_LAST;
	}

	/* -------------------------------------------------------------------- */
	/* find the most common speed, and use it */

	{
		/* FIXME: this isn't very good, we should be doing per-channel
		 * speed stuff or else we get broken modules */
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

		//log_appendf(1, "mode: %u", mode);
		//for (c = 0; c < 10; c++)
		//	log_appendf(1, "speeds[%d] = %u", c, speeds[c]);

		hz_to_speed_tempo(mode, &song->initial_speed, &song->initial_tempo);
	}

	/* start reading instrument data */

	if (slurp_seek(fp, hdr.instrument_paraptr, SEEK_SET))
		return LOAD_UNSUPPORTED;

	for (c = 0; c < ninst; c++) {
		unsigned char bytes[11];
		song_sample_t *smp = &song->samples[c + 1];

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

#if 0
		/* I don't think this is right... */
		smp->c5speed += slurp_getc(fp);
#else
		slurp_seek(fp, 1, SEEK_CUR);
#endif

		smp->volume = 64 * 4; //mphack

		/* It's probably safe to ignore these */
#if 0
		log_appendf(1, "timer: %d", slurp_getc(fp));
		log_appendf(1, "sr: %d", slurp_getc(fp));
		log_appendf(1, "unknown bytes: %d, %d", slurp_getc(fp), slurp_getc(fp));
#else
		slurp_seek(fp, 1, SEEK_CUR); /* "timer" */
		slurp_seek(fp, 1, SEEK_CUR); /* "sr" */
		slurp_seek(fp, 2, SEEK_CUR); /* unknown bytes (padding, probably) */
#endif
	}

	for (c = 9; c < MAX_CHANNELS; c++)
		song->channels[c].flags |= CHN_MUTE;

	snprintf(song->tracker_id, sizeof(song->tracker_id),
		(hdr.version < 4) ? "Unknown AdLib tracker" : "EdLib Tracker");

	for (c = 0; c < D00_WARN_MAX_; c++)
		if (warn & (1 << c))
			log_appendf(4, " Warning: %s", d00_warn_names[c]);

	return LOAD_SUCCESS;
}
