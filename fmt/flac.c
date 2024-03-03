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

#ifdef USE_FLAC
#include <FLAC/stream_decoder.h>
#define NEED_BYTESWAP
#include "headers.h"
#include "fmt.h"
#include "it.h"
#include "disko.h"
#include "sndfile.h"
#include "log.h"
#include <stdint.h>

struct flac_file {
	FLAC__StreamMetadata_StreamInfo streaminfo;
	struct {
		char *name;
		int32_t sample_rate;
		uint8_t pan;
		uint8_t vol;
		struct {
			uint32_t type;
			uint32_t start;
			uint32_t end;
		} loop;
	} flags;
	struct {
		const uint8_t* data;
		size_t len;
	} raw_data;
	uint8_t* uncomp_buf;
};
static uint32_t samples_decoded = 0, samples_read = 0;
static size_t total_size = 0;

static void on_meta(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data) {
	struct flac_file* flac_file = (struct flac_file*)client_data;
	int32_t loop_start = -1, loop_length = -1;
	switch (metadata->type) {
		case FLAC__METADATA_TYPE_STREAMINFO:
			flac_file->streaminfo = metadata->data.stream_info;
			break;
		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
			for (FLAC__uint32 i = 0; i < metadata->data.vorbis_comment.num_comments; i++) {
				const char *tag = (const char*)metadata->data.vorbis_comment.comments[i].entry;
				const FLAC__uint32 length = metadata->data.vorbis_comment.comments[i].length;
				if (length > 6 && !strncasecmp(tag, "TITLE=", 6) && flac_file->flags.name)
					strncpy(flac_file->flags.name, tag + 6, 32);
				else if (length > 11 && !strncasecmp(tag, "SAMPLERATE=", 11))
					flac_file->flags.sample_rate = strtol(tag + 11, NULL, 10);
				else if (length > 10 && !strncasecmp(tag, "LOOPSTART=", 10))
					loop_start = strtol(tag + 10, NULL, 10);
				else if (length > 11 && !strncasecmp(tag, "LOOPLENGTH=", 11))
					loop_length = strtol(tag + 11, NULL, 10);
			}
			if (loop_start > 0 && loop_length > 1) {
				flac_file->flags.loop.type = 0;
				flac_file->flags.loop.start = loop_start;
				flac_file->flags.loop.end = loop_start + loop_length - 1;
			}
			break;
		case FLAC__METADATA_TYPE_APPLICATION: {
			const uint8_t *data = (const uint8_t *)metadata->data.application.data;

			uint32_t chunk_id  = *(uint32_t *)data; data += 4;
			uint32_t chunk_len = *(uint32_t *)data; data += 4;

			if (chunk_id == 0x61727478 && chunk_len >= 8) { // "xtra"
				uint32_t xtra_flags = *(uint32_t *)data; data += 4;

				// panning (0..256)
				if (xtra_flags & 0x20) {
					uint16_t tmp_pan = *(uint16_t *)data;
					if (tmp_pan > 255)
						tmp_pan = 255;

					flac_file->flags.pan = (uint8_t)tmp_pan;
				}
				data += 2;

				// volume (0..256)
				uint16_t tmp_vol = *(uint16_t *)data;
				if (tmp_vol > 256)
					tmp_vol = 256;

				flac_file->flags.vol = (uint8_t)((tmp_vol + 2) / 4); // 0..256 -> 0..64 (rounded)
			}

			if (chunk_id == 0x6C706D73 && chunk_len > 52) { // "smpl"
				data += 28; // seek to first wanted byte

				uint32_t num_loops = *(uint32_t *)data; data += 4;
				if (num_loops == 1) {
					data += 4+4; // skip "samplerData" and "identifier"

					flac_file->flags.loop.type  = *(uint32_t *)data; data += 4;
					flac_file->flags.loop.start = *(uint32_t *)data; data += 4;
					flac_file->flags.loop.end   = *(uint32_t *)data; data += 4;
				}
			}
			break;
		}
		default:
			break;
	}

	(void)client_data;
	(void)decoder;
}

static FLAC__StreamDecoderReadStatus on_read(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data) {
	struct flac_file* flac_file = (struct flac_file*)client_data;

	if (samples_read < flac_file->raw_data.len) {
		if (*bytes > (flac_file->raw_data.len-samples_read))
			*bytes = flac_file->raw_data.len-samples_read;
		memcpy(buffer, flac_file->raw_data.data+samples_read, *bytes);
		samples_read += *bytes;
		return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
	} else {
		*bytes = 0;
		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	}

	(void)decoder;
}

static void on_error(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	(void)decoder, (void)client_data;

	log_appendf(4, "Error loading FLAC: %s", FLAC__StreamDecoderErrorStatusString[status]);
}

static FLAC__StreamDecoderWriteStatus on_write(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data) {
	struct flac_file* flac_file = (struct flac_file*)client_data;

	if (!flac_file->streaminfo.total_samples || !flac_file->streaminfo.channels)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	if (flac_file->streaminfo.channels > 2)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	switch (flac_file->streaminfo.bits_per_sample) {
		case 8:  case 12: case 16:
		case 20: case 24: case 32:
			break;
		default:
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	if (frame->header.number.sample_number == 0)
	{
		total_size = (size_t)(flac_file->streaminfo.total_samples * flac_file->streaminfo.channels * flac_file->streaminfo.bits_per_sample/8);
		flac_file->uncomp_buf = (uint8_t*)malloc(total_size * ((flac_file->streaminfo.bits_per_sample == 8) ? sizeof(int8_t) : sizeof(int16_t)));
		if (flac_file->uncomp_buf == NULL) {
			log_appendf(4, "Error loading FLAC: Out of memory!");
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
		}

		samples_decoded = 0;
	}

	/* this isn't true, but it works fine for what we do */
	uint32_t block_size = frame->header.blocksize*flac_file->streaminfo.channels;

	const uint32_t samples_allocated = flac_file->streaminfo.total_samples*flac_file->streaminfo.channels;
	if (samples_decoded+block_size > samples_allocated)
		block_size = samples_allocated - samples_decoded;

	uint32_t i = 0, j = 0;
	switch (flac_file->streaminfo.bits_per_sample) {
		case 8: {
			int8_t *buf_ptr = flac_file->uncomp_buf + samples_decoded;
			for (i = 0, j = 0; i < block_size; j++) {
				buf_ptr[i++] = (int8_t)buffer[0][j];
				if (flac_file->streaminfo.channels == 2)
					buf_ptr[i++] = (int8_t)buffer[1][j];
			}
			break;
		}
		/* here we cheat by just increasing 12-bit to 16-bit */
		case 12: case 16: {
			int16_t *buf_ptr = (int16_t*)flac_file->uncomp_buf + samples_decoded;
			uint32_t bit_shift = 16 - flac_file->streaminfo.bits_per_sample;
			for (i = 0, j = 0; i < block_size; j++) {
				buf_ptr[i++] = buffer[0][j] << bit_shift;
				if (flac_file->streaminfo.channels == 2)
					buf_ptr[i++] = buffer[1][j] << bit_shift;
			}
			break;
		}
		/* here we just make everything 16-bit, we don't support
		   24-bit anyway */
		case 20: case 24: case 32: {
			int16_t *buf_ptr = (int16_t*)flac_file->uncomp_buf + samples_decoded;
			uint32_t bit_shift = flac_file->streaminfo.bits_per_sample - 16;
			for (i = 0, j = 0; i < block_size; j++) {
				buf_ptr[i++] = buffer[0][j] >> bit_shift;
				if (flac_file->streaminfo.channels == 2)
					buf_ptr[i++] = buffer[1][j] >> bit_shift;
			}
			break;
		}
	}

	samples_decoded += block_size;

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

	(void)decoder;
}

#define FLAC_ERROR(x) \
	if (decoder != NULL) FLAC__stream_decoder_delete(decoder); \
	return x
static int flac_load(struct flac_file* flac_file, size_t len, int meta_only) {
	if (memcmp(flac_file->raw_data.data, "fLaC", 4))
		return 0;
	FLAC__StreamDecoder *decoder = FLAC__stream_decoder_new();
	if (decoder == NULL)
		return 0;

	FLAC__stream_decoder_set_metadata_respond_all(decoder);

	FLAC__StreamDecoderInitStatus initStatus =
		FLAC__stream_decoder_init_stream
		(
			decoder,
			on_read, NULL,
			NULL, NULL,
			NULL, on_write,
			on_meta, on_error,
			flac_file
		);

	if (meta_only) {
		if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder)) {
			FLAC_ERROR(0);
		}
	} else {
		if (!FLAC__stream_decoder_process_until_end_of_stream(decoder)) {
			FLAC_ERROR(0);
		}
	}

	FLAC__stream_decoder_finish(decoder);
	FLAC__stream_decoder_delete(decoder);
	return 1;
}
#undef FLAC_ERROR

int fmt_flac_load_sample(const uint8_t *data, size_t len, song_sample_t *smp) {
	samples_read = 0;
	samples_decoded = 0;
	total_size = 0;
	struct flac_file flac_file;
	flac_file.raw_data.data = data;
	flac_file.raw_data.len = len;
	flac_file.flags.name = smp->name;
	flac_file.flags.sample_rate = -1;
	flac_file.flags.loop.type = -1;
	if (!flac_load(&flac_file, len, 0))
		return 0;

	smp->volume        = 64 * 4;
	smp->global_volume = 64;
	smp->c5speed       = flac_file.streaminfo.sample_rate;
	smp->length        = flac_file.streaminfo.total_samples;
	if (flac_file.flags.loop.type != -1) {
		smp->loop_start    = flac_file.flags.loop.start;
		smp->loop_end      = flac_file.flags.loop.end+1;
		smp->flags |= (flac_file.flags.loop.type ? (CHN_LOOP | CHN_PINGPONGLOOP) : CHN_LOOP);
	}
	if (flac_file.flags.sample_rate > 0) {
		smp->c5speed = flac_file.flags.sample_rate;
	}

	// endianness
	uint32_t flags = SF_LE;
	// channels
	flags |= (flac_file.streaminfo.channels == 2) ? SF_SI : SF_M;
	// bit width
	switch (flac_file.streaminfo.bits_per_sample) {
	case 8:  flags |= SF_8;  break;
	case 12:
	case 16:
	case 20:
	case 24:
	case 32: flags |= SF_16; break;
	default:
		free(flac_file.uncomp_buf);
		return 0;
	}
	// libFLAC always returns signed
	flags |= SF_PCMS;
	int ret = csf_read_sample(smp, flags, flac_file.uncomp_buf, total_size);
	free(flac_file.uncomp_buf);

	return ret;
}

int fmt_flac_read_info(dmoz_file_t *file, const uint8_t *data, size_t len)
{
	samples_read = 0;
	samples_decoded = 0;
	total_size = 0;
	struct flac_file flac_file;
	flac_file.raw_data.data = data;
	flac_file.raw_data.len = len;
	flac_file.flags.name = NULL;
	flac_file.flags.loop.type = -1;
	if (!flac_load(&flac_file, len, 1)) {
		return 0;
	}

	file->smp_flags = 0;

	/* don't even attempt */
	if (flac_file.streaminfo.channels > 2 || !flac_file.streaminfo.total_samples ||
		!flac_file.streaminfo.channels)
		return 0;

	switch (flac_file.streaminfo.bits_per_sample) {
		case 12:
		case 16:
		case 20:
		case 24:
		case 32:
			file->smp_flags |= CHN_16BIT;
		case 8:
			break;
		default:
			return 0;
	}

	if (flac_file.streaminfo.channels == 2)
		file->smp_flags |= CHN_STEREO;

	file->smp_speed      = flac_file.streaminfo.sample_rate;
	file->smp_length     = flac_file.streaminfo.total_samples;
	if (flac_file.flags.loop.type != -1) {
		file->smp_loop_start = flac_file.flags.loop.start;
		file->smp_loop_end   = flac_file.flags.loop.end+1;
		file->smp_flags    |= (flac_file.flags.loop.type ? (CHN_LOOP | CHN_PINGPONGLOOP) : CHN_LOOP);
	}
	if (flac_file.flags.sample_rate > 0) {
		file->smp_speed = flac_file.flags.sample_rate;
	}

	file->description  = "FLAC Audio File";
	file->type         = TYPE_SAMPLE_COMPR;
	file->smp_filename = file->base;

	return 1;
}
#endif
