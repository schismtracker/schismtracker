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
#include "it.h"
#include "disko.h"
#include "player/sndfile.h"
#include "log.h"
#include <stdint.h>

#define WAVE_FORMAT_PCM             0x0001
#define WAVE_FORMAT_IEEE_FLOAT      0x0003 // IEEE float
#define WAVE_FORMAT_ALAW            0x0006 // 8-bit ITU-T G.711 A-law
#define WAVE_FORMAT_MULAW           0x0007 // 8-bit ITU-T G.711 µ-law
#define WAVE_FORMAT_EXTENSIBLE      0xFFFE

// Standard IFF chunks IDs
#define IFFID_FORM              0x4d524f46
#define IFFID_RIFF              0x46464952
#define IFFID_WAVE              0x45564157
#define IFFID_LIST              0x5453494C
#define IFFID_INFO              0x4F464E49

// IFF Info fields
#define IFFID_ICOP              0x504F4349
#define IFFID_IART              0x54524149
#define IFFID_IPRD              0x44525049
#define IFFID_INAM              0x4D414E49
#define IFFID_ICMT              0x544D4349
#define IFFID_IENG              0x474E4549
#define IFFID_ISFT              0x54465349
#define IFFID_ISBJ              0x4A425349
#define IFFID_IGNR              0x524E4749
#define IFFID_ICRD              0x44524349

// Wave IFF chunks IDs
#define IFFID_wave              0x65766177
#define IFFID_fmt               0x20746D66
#define IFFID_wsmp              0x706D7377
#define IFFID_pcm               0x206d6370
#define IFFID_data              0x61746164
#define IFFID_smpl              0x6C706D73
#define IFFID_xtra              0x61727478

typedef struct {
	uint32_t id_RIFF;           // "RIFF"
	uint32_t filesize;          // file length-8
	uint32_t id_WAVE;
} wave_file_header_t;

typedef struct {
	uint16_t format;          // 1
	uint16_t channels;        // 1:mono, 2:stereo
	uint32_t freqHz;          // sampling freq
	uint32_t bytessec;        // bytes/sec=freqHz*samplesize
	uint16_t samplesize;      // sizeof(sample)
	uint16_t bitspersample;   // bits per sample (8/16)
} wave_format_t;

/* --------------------------------------------------------------------------------------------------------- */

static int wav_chunk_fmt_read(const void *data, size_t size, void *void_fmt)
{
	wave_format_t *fmt = (wave_format_t *)void_fmt;

	slurp_t fp;
	slurp_memstream(&fp, (uint8_t *)data, size);

#define READ_VALUE(name) \
	do { if (slurp_read(&fp, &name, sizeof(name)) != sizeof(name)) { unslurp(&fp); return 0; } } while (0)

	READ_VALUE(fmt->format);
	READ_VALUE(fmt->channels);
	READ_VALUE(fmt->freqHz);
	READ_VALUE(fmt->bytessec);
	READ_VALUE(fmt->samplesize);
	READ_VALUE(fmt->bitspersample);

	fmt->format        = bswapLE16(fmt->format);
	fmt->channels      = bswapLE16(fmt->channels);
	fmt->freqHz        = bswapLE32(fmt->freqHz);
	fmt->bytessec      = bswapLE32(fmt->bytessec);
	fmt->samplesize    = bswapLE16(fmt->samplesize);
	fmt->bitspersample = bswapLE16(fmt->bitspersample);

	/* BUT I'M NOT DONE YET */
	if (fmt->format == WAVE_FORMAT_EXTENSIBLE) {
		uint16_t ext_size;
		READ_VALUE(ext_size);
		ext_size = bswapLE16(ext_size);

		if (ext_size < 22)
			return 0;

		slurp_seek(&fp, 6, SEEK_CUR);

		static const unsigned char subformat_base_check[12] = {
			0x00, 0x00, 0x10, 0x00,
			0x80, 0x00, 0x00, 0xAA,
			0x00, 0x38, 0x9B, 0x71,
		};

		uint32_t subformat;
		unsigned char subformat_base[12];

		READ_VALUE(subformat);
		READ_VALUE(subformat_base);

		if (memcmp(subformat_base, subformat_base_check, 12))
			return 0;

		fmt->format = bswapLE32(subformat);
	}

#undef READ_VALUE

	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

static int wav_load(song_sample_t *smp, slurp_t *fp, int load_sample)
{
	iff_chunk_t fmt_chunk = {0}, data_chunk = {0};
	wave_format_t fmt;
	wave_file_header_t phdr;

	if (slurp_read(fp, &phdr.id_RIFF, sizeof(phdr.id_RIFF)) != sizeof(phdr.id_RIFF)
		|| slurp_read(fp, &phdr.filesize, sizeof(phdr.filesize)) != sizeof(phdr.filesize)
		|| slurp_read(fp, &phdr.id_WAVE, sizeof(phdr.id_WAVE)) != sizeof(phdr.id_WAVE))
		return 0;

	phdr.id_RIFF  = bswapLE32(phdr.id_RIFF);
	phdr.filesize = bswapLE32(phdr.filesize);
	phdr.id_WAVE  = bswapLE32(phdr.id_WAVE);

	if (phdr.id_RIFF != IFFID_RIFF ||
		phdr.id_WAVE != IFFID_WAVE)
		return 0;

	iff_chunk_t c;
	while (riff_chunk_peek(&c, fp)) {
		switch (bswapBE32(c.id)) {
		case IFFID_fmt:
			if (fmt_chunk.id)
				return 0;

			fmt_chunk = c;
			break;
		case IFFID_data:
			if (data_chunk.id)
				return 0;

			data_chunk = c;
			break;
		default:
			break;
		}
	}

	if (!fmt_chunk.id || !data_chunk.id)
		return 0;

	if (!iff_chunk_receive(&fmt_chunk, fp, wav_chunk_fmt_read, &fmt))
		return 0;

	uint32_t flags = 0;

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
	smp->volume        = 64 * 4;
	smp->global_volume = 64;
	smp->c5speed       = fmt.freqHz;
	smp->length        = data_chunk.size / ((fmt.bitspersample / 8) * fmt.channels);

	if (load_sample) {
		return iff_read_sample(&data_chunk, fp, smp, flags, 0);
	} else {
		if (fmt.channels == 2)
			smp->flags |= CHN_STEREO;

		if (fmt.bitspersample > 8)
			smp->flags |= CHN_16BIT;
	}

	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

int fmt_wav_load_sample(slurp_t *fp, song_sample_t *smp)
{
	return wav_load(smp, fp, 1);
}

int fmt_wav_read_info(dmoz_file_t *file, slurp_t *fp)
{
	song_sample_t smp;
	if (!wav_load(&smp, fp, 0))
		return 0;

	file->smp_flags  = smp.flags;
	file->smp_speed  = smp.c5speed;
	file->smp_length = smp.length;

	file->description  = "IBM/Microsoft RIFF Audio";
	file->type         = TYPE_SAMPLE_PLAIN;
	file->smp_filename = file->base;

	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */
/* wav is like aiff's retarded cousin */

struct wav_writedata {
	long data_size; // seek position for writing data size (in bytes)
	size_t numbytes; // how many bytes have been written
	int bps; // bytes per sample
	int swap; // should be byteswapped?
};

static int wav_header(disko_t *fp, int bits, int channels, int rate, size_t length,
	struct wav_writedata *wwd /* out */)
{
	int16_t s;
	uint32_t ul;
	int bps = 1;

	bps *= ((bits + 7) / 8) * channels;

	/* write a very large size for now */
	disko_write(fp, "RIFF\377\377\377\377WAVEfmt ", 16);
	ul = bswapLE32(16); // fmt chunk size
	disko_write(fp, &ul, 4);
	s = bswapLE16(1); // linear pcm
	disko_write(fp, &s, 2);
	s = bswapLE16(channels); // number of channels
	disko_write(fp, &s, 2);
	ul = bswapLE32(rate); // sample rate
	disko_write(fp, &ul, 4);
	ul = bswapLE32(bps * rate); // "byte rate" (why?! I have no idea)
	disko_write(fp, &ul, 4);
	s = bswapLE16(bps); // (oh, come on! the format already stores everything needed to calculate this!)
	disko_write(fp, &s, 2);
	s = bswapLE16(bits); // bits per sample
	disko_write(fp, &s, 2);

	disko_write(fp, "data", 4);
	if (wwd)
		wwd->data_size = disko_tell(fp);
	ul = bswapLE32(bps * length);
	disko_write(fp, &ul, 4);

	return bps;
}

int fmt_wav_save_sample(disko_t *fp, song_sample_t *smp)
{
	if (smp->flags & CHN_ADLIB)
		return SAVE_UNSUPPORTED;

	int bps;
	uint32_t ul;
	uint32_t flags = SF_LE;
	flags |= (smp->flags & CHN_16BIT) ? (SF_16 | SF_PCMS) : (SF_8 | SF_PCMU);
	flags |= (smp->flags & CHN_STEREO) ? SF_SI : SF_M;

	bps = wav_header(fp, (smp->flags & CHN_16BIT) ? 16 : 8, (smp->flags & CHN_STEREO) ? 2 : 1,
		smp->c5speed, smp->length, NULL);

	if (csf_write_sample(fp, smp, flags, UINT32_MAX) != smp->length * bps) {
		log_appendf(4, "WAV: unexpected data size written");
		return SAVE_INTERNAL_ERROR;
	}

	/* fix the length in the file header */
	ul = disko_tell(fp) - 8;
	ul = bswapLE32(ul);
	disko_seek(fp, 4, SEEK_SET);
	disko_write(fp, &ul, 4);

	return SAVE_SUCCESS;
}


int fmt_wav_export_head(disko_t *fp, int bits, int channels, int rate)
{
	struct wav_writedata *wwd = malloc(sizeof(struct wav_writedata));
	if (!wwd)
		return DW_ERROR;
	fp->userdata = wwd;
	wwd->bps = wav_header(fp, bits, channels, rate, ~0, wwd);
	wwd->numbytes = 0;
#if WORDS_BIGENDIAN
	wwd->swap = (bits > 8);
#else
	wwd->swap = 0;
#endif

	return DW_OK;
}

int fmt_wav_export_body(disko_t *fp, const uint8_t *data, size_t length)
{
	struct wav_writedata *wwd = fp->userdata;

	if (length % wwd->bps) {
		log_appendf(4, "WAV export: received uneven length");
		return DW_ERROR;
	}

	wwd->numbytes += length;

	if (wwd->swap) {
		const int16_t *ptr = (const int16_t *) data;
		uint16_t v;

		length /= 2;
		while (length--) {
			v = *ptr;
			v = bswapLE16(v);
			disko_write(fp, &v, 2);
			ptr++;
		}
	} else {
		disko_write(fp, data, length);
	}

	return DW_OK;
}

int fmt_wav_export_silence(disko_t *fp, long bytes)
{
	struct wav_writedata *wwd = fp->userdata;
	wwd->numbytes += bytes;

	disko_seek(fp, bytes, SEEK_CUR);
	return DW_OK;
}

int fmt_wav_export_tail(disko_t *fp)
{
	struct wav_writedata *wwd = fp->userdata;
	uint32_t ul;

	/* fix the length in the file header */
	ul = disko_tell(fp) - 8;
	ul= bswapLE32(ul);
	disko_seek(fp, 4, SEEK_SET);
	disko_write(fp, &ul, 4);

	/* write the other lengths */
	disko_seek(fp, wwd->data_size, SEEK_SET);
	ul = bswapLE32(wwd->numbytes);
	disko_write(fp, &ul, 4);

	free(wwd);

	return DW_OK;
}

