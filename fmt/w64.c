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

/* Sonic Foundry / Sony Wave64 sample reader and loader.
 * Largely based on the WAVE code, without the xtra and smpl chunks. */

#include "headers.h"

#include "fmt.h"
#include "bits.h"

/* --------------------------------------------------------------------------------------------------------- */
/* chunk helpers */

struct w64_chunk {
	unsigned char id[16];
	uint64_t size;
	int64_t offset;
};

/* wave64 is NOT iff, so we have to define our own stupid reading functions. sigh. */
static int w64_chunk_peek(struct w64_chunk *chunk, slurp_t *fp)
{
	int64_t pos;

	if (slurp_read(fp, chunk->id, sizeof(chunk->id)) != sizeof(chunk->id))
		return 0;

	if (slurp_read(fp, &chunk->size, sizeof(chunk->size)) != sizeof(chunk->size))
		return 0;

	chunk->size = bswapLE64(chunk->size);

	/* Size includes the size of the header, for whatever reason. */
	if (chunk->size < 24)
		return 0;

	chunk->size -= 24;

	chunk->offset = slurp_tell(fp);
	if (chunk->offset < 0)
		return 0;

	/* w64 sizes are aligned to 64-bit boundaries */
	slurp_seek(fp, (chunk->size + 7) & ~7, SEEK_CUR);

	pos = slurp_tell(fp);
	if (pos < 0)
		return 0;

	return slurp_available(fp, 1, SEEK_CUR);
}

static int w64_chunk_empty(struct w64_chunk *chunk)
{
	static const struct w64_chunk empty_chunk = {0};

	return !memcmp(&empty_chunk, chunk, sizeof(struct w64_chunk));
}

static int w64_chunk_receive(struct w64_chunk *chunk, slurp_t *fp, int (*callback)(const void *, size_t, void *), void *userdata)
{
	int64_t pos = slurp_tell(fp);
	if (pos < 0)
		return 0;

	if (slurp_seek(fp, chunk->offset, SEEK_SET))
		return 0;

	int res = slurp_receive(fp, callback, chunk->size, userdata);

	/* how ? */
	if (slurp_seek(fp, pos, SEEK_SET))
		return 0;

	return res;
}

static int w64_read_sample(struct w64_chunk *chunk, slurp_t *fp, song_sample_t *smp, uint32_t flags)
{
	int64_t pos = slurp_tell(fp);
	if (pos < 0)
		return 0;

	if (slurp_seek(fp, chunk->offset, SEEK_SET))
		return 0;

	int r = csf_read_sample(smp, flags, fp);

	slurp_seek(fp, pos, SEEK_SET);

	return r;
}

/* --------------------------------------------------------------------------------------------------------- */

static int w64_load(song_sample_t *smp, slurp_t *fp, int load_sample)
{
	struct w64_chunk fmt_chunk = {0}, data_chunk = {0};
	struct wave_format fmt;
	uint32_t flags;

	{
		static const unsigned char w64_guid_RIFF[16] = {
			0x72, 0x69, 0x66, 0x66, 0x2E, 0x91, 0xCF, 0x11,
			0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00,
		};

		static const unsigned char w64_guid_WAVE[16] = {
			0x77, 0x61, 0x76, 0x65, 0xF3, 0xAC, 0xD3, 0x11,
			0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A,
		};

		unsigned char guid[16];

		if (slurp_read(fp, guid, sizeof(guid)) != sizeof(guid)
			|| memcmp(guid, w64_guid_RIFF, 16))
			return 0;

		/* skip filesize. */
		slurp_seek(fp, 8, SEEK_CUR);

		if (slurp_read(fp, guid, sizeof(guid)) != sizeof(guid)
			|| memcmp(guid, w64_guid_WAVE, 16))
			return 0;
	}

	{
		static const unsigned char w64_guid_FMT_[16] = {
			0x66, 0x6D, 0x74, 0x20, 0xF3, 0xAC, 0xD3, 0x11,
			0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A,
		};

		static const unsigned char w64_guid_DATA[16] = {
			0x64, 0x61, 0x74, 0x61, 0xF3, 0xAC, 0xD3, 0x11,
			0x8C, 0xD1, 0x00, 0xC0, 0x4F, 0x8E, 0xDB, 0x8A,
		};

		struct w64_chunk c;

		/* go through every chunk in the file. */
		while (w64_chunk_peek(&c, fp)) {
			if (!memcmp(c.id, w64_guid_FMT_, 16)) {
				if (!w64_chunk_empty(&fmt_chunk))
					return 0;

				fmt_chunk = c;
			} else if (!memcmp(c.id, w64_guid_DATA, 16)) {
				if (!w64_chunk_empty(&data_chunk))
					return 0;

				data_chunk = c;
			}
		}
	}

	/* this should never happen */
	if (w64_chunk_empty(&fmt_chunk) || w64_chunk_empty(&data_chunk))
		return 0;

	/* now we have all the chunks we need. */
	if (!w64_chunk_receive(&fmt_chunk, fp, wav_chunk_fmt_read, &fmt))
		return 0;

	// endianness
	flags = SF_LE;

	// channels
	flags |= (fmt.channels == 2) ? SF_SI : SF_M; // interleaved stereo

	// bit width
	switch (fmt.bitspersample) {
	case 8:  flags |= SF_8;  break;
	case 16: flags |= SF_16; break;
	case 24: flags |= SF_24; break;
	case 32: flags |= SF_32; break;
	default: return 0; // unsupported
	}

	// encoding (8-bit wav is unsigned, everything else is signed -- yeah, it's stupid)
	switch (fmt.format) {
	case WAVE_FORMAT_PCM:
		flags |= (fmt.bitspersample == 8) ? SF_PCMU : SF_PCMS;
		break;
	case WAVE_FORMAT_IEEE_FLOAT:
		flags |= SF_IEEE;
		break;
	default: return 0; // unsupported
	}

	smp->flags         = 0; // flags are set by csf_read_sample
	smp->c5speed       = fmt.freqHz;
	smp->length        = data_chunk.size / ((fmt.bitspersample / 8) * fmt.channels);

	if (load_sample) {
		return w64_read_sample(&data_chunk, fp, smp, flags);
	} else {
		if (fmt.channels == 2)
			smp->flags |= CHN_STEREO;

		if (fmt.bitspersample > 8)
			smp->flags |= CHN_16BIT;
	}

	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

int fmt_w64_load_sample(slurp_t *fp, song_sample_t *smp)
{
	return w64_load(smp, fp, 1);
}

int fmt_w64_read_info(dmoz_file_t *file, slurp_t *fp)
{
	song_sample_t smp;

	smp.volume = 64 * 4;
	smp.global_volume = 64;

	if (!w64_load(&smp, fp, 0))
		return 0;

	//fmt_fill_file_from_sample(file, &smp);

	file->description  = "Sony Wave64";
	file->type         = TYPE_SAMPLE_PLAIN;
	file->smp_filename = file->base;

	return 1;
}
