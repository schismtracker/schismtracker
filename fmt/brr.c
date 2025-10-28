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
#include "fmt.h"
#include "bits.h"
#include "mem.h"

/* macros for extracting stuff from the header byte */
#define BRR_HDR_SHIFT(x) ((x) >> 4)
#define BRR_HDR_FILTER(x) (((x) >> 2) & 0x3)
#define BRR_HDR_LOOP(x) (((x) >> 1) & 0x1)
#define BRR_HDR_END(x) ((x) & 1)

/* I'm a *bit* paranoid :) */
#define BRR_TEST_BLOCKS (64)

static inline SCHISM_ALWAYS_INLINE
uint32_t brr_ratio(uint32_t x)
{
	return (((uint64_t)x * 16) / 9);
}

/* shared internal info */
struct brr_info {
	/* number of blocks */
	uint32_t blocks;

	/* loop enabled? */
	unsigned int loop : 1;
	/* loop starting point
	 * loop end is always(?) the length of the sample */
	uint16_t loop_start;
};

static int brr_load(struct brr_info *brr, slurp_t *fp)
{
	uint32_t filesize;
	uint32_t testblocks;
	uint32_t i;
	int hdr;

	memset(brr, 0, sizeof(*brr));

	/* back to the start
	 * this should always be the case anyway, though */
	slurp_rewind(fp);

	if (!slurp_available(fp, 11, SEEK_SET) || slurp_available(fp, UINT16_MAX + 1, SEEK_SET))
		return -1;

	filesize = slurp_length(fp);
	/* initialize this with the file size for now.
	 * since this depends on whether there is a
	 * loop point, we include this in that detection
	 * and then divide by 9. */
	brr->blocks = filesize;

	switch (filesize % 9) {
	case 0:
		break;
	case 2:
		slurp_read(fp, &brr->loop_start, 2);
		brr->loop_start = bswapLE16(brr->loop_start);
		brr->blocks -= 2;
		brr->loop = 1; /* set this flag */
		break;
	default:
		/* Invalid */
		return -1;
	}

	brr->blocks /= 9;

	/* now 'blocks' is actually the number of blocks. */
	hdr = slurp_getc(fp);
	if ((hdr == EOF) || BRR_HDR_END(hdr))
		return -1; /* "if the first block is the last, this is very unlikely to be a real BRR sample" */

	testblocks = MIN(BRR_TEST_BLOCKS, brr->blocks);

	for (i = 0; i < testblocks; i++) {
		if ((hdr == EOF) || BRR_HDR_SHIFT(hdr) > 13)
			return -1;

		slurp_seek(fp, 8, SEEK_CUR); /* skip rest of the block */
		hdr = slurp_getc(fp);
	}

	/* Seek to the start of the blocks */
	slurp_seek(fp, filesize % 9, SEEK_SET);

	return 0;
}

int fmt_brr_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct brr_info brr;
	if (brr_load(&brr, fp) < 0)
		return 0;

	/* ehhhhhhh */
	file->title = str_dup("");
	file->description = "SNES BRR sample";
	file->type = TYPE_SAMPLE_COMPR; /* in a bad way */

	file->smp_flags = CHN_16BIT;
	file->smp_speed = 32000;

	/* this might not be the case if we have a LOOP/END bit
	 * set before the end of the file, but that SHOULD be
	 * rare anyway ?? */
	file->smp_length = brr.blocks * 16;

	if (brr.loop) {
		file->smp_flags |= CHN_LOOP;
		file->smp_loop_start = brr_ratio(brr.loop_start);
		file->smp_loop_end = file->smp_length;
	}

	return 1;
}

static int16_t brr_decode_sample(int32_t nibble, int16_t *psn1, int16_t *psn2,
	uint8_t shift, uint8_t filter)
{
	/* poopy sign extension */
	if (nibble >= 8)
		nibble -= 16;

	nibble = rshift_signed(lshift_signed(nibble, shift), 1);

	/* now we need to apply the filter
	 * for some reason the SNESdev wiki is wrong here, the
	 * 2nd last sample actually gets subtracted, not added. */
	switch (filter) {
	case 1:
		nibble += ((*psn1 * 15) / 16);
		break;
	case 2:
		nibble += ((*psn1 * 61) / 32);
		nibble -= ((*psn2 * 15) / 16);
		break;
	case 3:
		nibble += ((*psn1 * 115) / 64);
		nibble -= ((*psn2 * 13) / 16);
		break;
	}

	/* clamp to 15-bit limits */
	nibble = CLAMP(nibble, -16384, 16383);

	*psn2 = *psn1;
	*psn1 = nibble;

	return lshift_signed(nibble, 1);
}

int fmt_brr_load_sample(slurp_t *fp, song_sample_t *smp)
{
	struct brr_info brr;
	int16_t sn1, sn2;
	uint32_t i;
	uint_fast8_t j;

	if (brr_load(&brr, fp) < 0)
		return 0;

	/* Sample -1, Sample -2 */
	sn1 = 0;
	sn2 = 0;

	/* read directly into the sample
	 * some of our other loaders ought to do this as well */
	smp->flags = CHN_16BIT;
	smp->c5speed = 32000; /* SNES speed */

	smp->length = brr.blocks * 16;
	smp->data = csf_allocate_sample(smp->length * 2);

	if (brr.loop) {
		smp->flags |= CHN_LOOP;
		smp->loop_start = brr_ratio(brr.loop_start);
		smp->loop_end = smp->length;
	}

	for (i = 0; i < brr.blocks; i++) {
		uint8_t shift;
		uint8_t block[9];
		int16_t *data;

		if (slurp_read(fp, block, 9) != 9) {
			csf_free_sample(smp->data);
			return 0;
		}

		shift = BRR_HDR_SHIFT(block[0]);
		if (shift > 12) {
			/* What are we supposed to do here? */
			csf_free_sample(smp->data);
			return 0;
		}

		data = (int16_t *)smp->data + (i * 16);

		for (j = 0; j < 8; j++) {
			uint8_t smpblk = block[1 + j];

			data[2 * j + 0] = brr_decode_sample(smpblk >> 4,  &sn1, &sn2, shift, BRR_HDR_FILTER(block[0]));
			data[2 * j + 1] = brr_decode_sample(smpblk & 0xF, &sn1, &sn2, shift, BRR_HDR_FILTER(block[0]));
		}
	}

	/* Adjust loop points */
	csf_adjust_sample_loop(smp);

	return 1;
}
