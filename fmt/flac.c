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
#include "it.h"
#include "disko.h"
#include "sndfile.h"
#include "log.h"
#include "util.h"

#include <FLAC/stream_decoder.h>
#include <FLAC/stream_encoder.h>

#include <stdint.h>

struct flac_file {
	FLAC__StreamMetadata_StreamInfo streaminfo;

	struct {
		char name[32];
		uint32_t sample_rate;
		uint8_t pan;
		uint8_t vol;
		struct {
			int32_t type;
			uint32_t start;
			uint32_t end;
		} loop;
	} flags;

	struct {
		const uint8_t* data;
		size_t len;
	} compressed;

	struct {
		uint8_t* data;
		size_t len;

		uint32_t samples_decoded, samples_read;
	} uncompressed;
};

static void on_meta(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data) {
	struct flac_file* flac_file = (struct flac_file*)client_data;
	int32_t loop_start = -1, loop_length = -1;

	switch (metadata->type) {
		case FLAC__METADATA_TYPE_STREAMINFO:
			memcpy(&flac_file->streaminfo, &metadata->data.stream_info, sizeof(flac_file->streaminfo));
			break;
		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
			for (size_t i = 0; i < metadata->data.vorbis_comment.num_comments; i++) {
				const char *tag = (const char*)metadata->data.vorbis_comment.comments[i].entry;
				const FLAC__uint32 length = metadata->data.vorbis_comment.comments[i].length;

				if (length > 6 && !strncasecmp(tag, "TITLE=", 6))
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

			uint32_t chunk_id  = *(uint32_t*)data; data += sizeof(uint32_t);
			uint32_t chunk_len = *(uint32_t*)data; data += sizeof(uint32_t);

			if (chunk_id == 0x61727478 && chunk_len >= 8) { // "xtra"
				uint32_t xtra_flags = *(uint32_t*)data; data += sizeof(uint32_t);

				// panning (0..256)
				if (xtra_flags & 0x20) {
					uint16_t tmp_pan = *(uint16_t*)data;
					if (tmp_pan > 255)
						tmp_pan = 255;

					flac_file->flags.pan = (uint8_t)tmp_pan;
				}
				data += sizeof(uint16_t);

				// volume (0..256)
				uint16_t tmp_vol = *(uint16_t*)data;
				if (tmp_vol > 256)
					tmp_vol = 256;

				flac_file->flags.vol = (uint8_t)((tmp_vol + 2) / 4); // 0..256 -> 0..64 (rounded)
			}

			if (chunk_id == 0x6C706D73 && chunk_len > 52) { // "smpl"
				data += 28; // seek to first wanted byte

				uint32_t num_loops = *(uint32_t *)data; data += sizeof(uint32_t);
				if (num_loops == 1) {
					data += 4 + 4; // skip "samplerData" and "identifier"

					flac_file->flags.loop.type  = *(uint32_t *)data; data += sizeof(uint32_t);
					flac_file->flags.loop.start = *(uint32_t *)data; data += sizeof(uint32_t);
					flac_file->flags.loop.end   = *(uint32_t *)data; data += sizeof(uint32_t);
				}
			}
			break;
		}
		default:
			break;
	}

	(void)decoder, (void)client_data;
}

static FLAC__StreamDecoderReadStatus on_read(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data) {
	struct flac_file* flac_file = (struct flac_file*)client_data;

	if (flac_file->uncompressed.samples_read < flac_file->compressed.len) {
		size_t needed = flac_file->compressed.len - flac_file->uncompressed.samples_read;
		if (*bytes > needed)
			*bytes = needed;

		memcpy(buffer, flac_file->compressed.data + flac_file->uncompressed.samples_read, *bytes);
		flac_file->uncompressed.samples_read += *bytes;

		return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
	} else {
		*bytes = 0;
		return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;
	}

	(void)decoder;
}

static void on_error(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data) {
	log_appendf(4, "Error loading FLAC: %s", FLAC__StreamDecoderErrorStatusString[status]);

	(void)decoder, (void)client_data;
}

static FLAC__StreamDecoderWriteStatus on_write(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data) {
	struct flac_file* flac_file = (struct flac_file*)client_data;

	/* invalid? */
	if (!flac_file->streaminfo.total_samples || !flac_file->streaminfo.channels
		|| flac_file->streaminfo.channels > 2)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	if (frame->header.number.sample_number == 0) {
		/* allocate our buffer */
		flac_file->uncompressed.len = (size_t)(flac_file->streaminfo.total_samples * flac_file->streaminfo.channels * flac_file->streaminfo.bits_per_sample/8);
		flac_file->uncompressed.data = (uint8_t*)malloc(flac_file->uncompressed.len * ((flac_file->streaminfo.bits_per_sample == 8) ? sizeof(int8_t) : sizeof(int16_t)));
		if (!flac_file->uncompressed.data)
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

		flac_file->uncompressed.samples_decoded = 0;
	}

	uint32_t block_size = frame->header.blocksize * flac_file->streaminfo.channels;

	const uint32_t samples_allocated = flac_file->streaminfo.total_samples * flac_file->streaminfo.channels;
	if (flac_file->uncompressed.samples_decoded + block_size > samples_allocated)
		block_size = samples_allocated - flac_file->uncompressed.samples_decoded;

	if (flac_file->streaminfo.bits_per_sample <= 8) {
		int8_t* buf_ptr = (int8_t*)flac_file->uncompressed.data + flac_file->uncompressed.samples_decoded;
		uint32_t bit_shift = 8 - flac_file->streaminfo.bits_per_sample;

		size_t i, j, c;
		for (i = 0, j = 0; i < block_size; j++)
			for (c = 0; c < flac_file->streaminfo.channels; c++)
				buf_ptr[i++] = buffer[c][j] << bit_shift;
	} else if (flac_file->streaminfo.bits_per_sample <= 16) {
		int16_t* buf_ptr = (int16_t*)flac_file->uncompressed.data + flac_file->uncompressed.samples_decoded;
		uint32_t bit_shift = 16 - flac_file->streaminfo.bits_per_sample;

		size_t i, j, c;
		for (i = 0, j = 0; i < block_size; j++)
			for (c = 0; c < flac_file->streaminfo.channels; c++)
				buf_ptr[i++] = buffer[c][j] << bit_shift;
	} else { /* >= 16 */
		int16_t* buf_ptr = (int16_t*)flac_file->uncompressed.data + flac_file->uncompressed.samples_decoded;
		uint32_t bit_shift = flac_file->streaminfo.bits_per_sample - 16;

		size_t i, j, c;
		for (i = 0, j = 0; i < block_size; j++)
			for (c = 0; c < flac_file->streaminfo.channels; c++)
				buf_ptr[i++] = buffer[c][j] >> bit_shift;
	}

	flac_file->uncompressed.samples_decoded += block_size;

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

	(void)decoder;
}

#define FLAC_ERROR(x) \
	do { \
		if (!decoder) FLAC__stream_decoder_delete(decoder); \
		return x; \
	} while (0);

static int flac_load(struct flac_file* flac_file, int meta_only) {
	if (flac_file->compressed.len < 4)
		return 0;

	if (memcmp(flac_file->compressed.data, "fLaC", 4))
		return 0;

	FLAC__StreamDecoder* decoder = FLAC__stream_decoder_new();
	if (!decoder)
		return 0;

	FLAC__stream_decoder_set_metadata_respond_all(decoder);

	FLAC__StreamDecoderInitStatus initStatus =
		FLAC__stream_decoder_init_stream(
			decoder,
			on_read, NULL,
			NULL, NULL,
			NULL, on_write,
			on_meta, on_error,
			flac_file
		);

	if (meta_only) {
		if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder))
			FLAC_ERROR(0);
	} else {
		if (!FLAC__stream_decoder_process_until_end_of_stream(decoder))
			FLAC_ERROR(0);
	}

	FLAC__stream_decoder_finish(decoder);
	FLAC__stream_decoder_delete(decoder);

	return 1;
}
#undef FLAC_ERROR

int fmt_flac_load_sample(const uint8_t *data, size_t len, song_sample_t *smp) {
	struct flac_file flac_file = {0};
	flac_file.compressed.data = data;
	flac_file.compressed.len = len;
	strncpy(flac_file.flags.name, smp->name, ARRAY_SIZE(flac_file.flags.name));
	flac_file.flags.sample_rate = 0;
	flac_file.flags.loop.type = -1;

	if (!flac_load(&flac_file, 0))
		return 0;

	smp->volume        = 64 * 4;
	smp->global_volume = 64;
	smp->c5speed       = flac_file.streaminfo.sample_rate;
	smp->length        = flac_file.streaminfo.total_samples;
	if (flac_file.flags.loop.type != -1) {
		smp->loop_start    = flac_file.flags.loop.start;
		smp->loop_end      = flac_file.flags.loop.end + 1;
		smp->flags |= (flac_file.flags.loop.type ? (CHN_LOOP | CHN_PINGPONGLOOP) : CHN_LOOP);
	}

	if (flac_file.flags.sample_rate)
		smp->c5speed = flac_file.flags.sample_rate;

	// endianness
	uint32_t flags = SF_LE;

	// channels
	flags |= (flac_file.streaminfo.channels == 2) ? SF_SI : SF_M;

	// bit width
	flags |= (flac_file.streaminfo.bits_per_sample <= 8) ? SF_8 : SF_16;

	// libFLAC always returns signed
	flags |= SF_PCMS;

	int ret = csf_read_sample(smp, flags, flac_file.uncompressed.data, flac_file.uncompressed.len);
	free(flac_file.uncompressed.data);

	return ret;
}

int fmt_flac_read_info(dmoz_file_t *file, const uint8_t *data, size_t len)
{
	struct flac_file flac_file = {0};
	flac_file.compressed.data = data;
	flac_file.compressed.len = len;
	flac_file.flags.loop.type = -1;

	if (!flac_load(&flac_file, 1))
		return 0;

	file->smp_flags = 0;

	/* don't even attempt */
	if (flac_file.streaminfo.channels > 2 || !flac_file.streaminfo.total_samples ||
		!flac_file.streaminfo.channels)
		return 0;

	if (flac_file.streaminfo.bits_per_sample > 8)
		file->smp_flags |= CHN_16BIT;

	if (flac_file.streaminfo.channels == 2)
		file->smp_flags |= CHN_STEREO;

	file->smp_speed      = flac_file.streaminfo.sample_rate;
	file->smp_length     = flac_file.streaminfo.total_samples;

	if (flac_file.flags.loop.type != -1) {
		file->smp_loop_start = flac_file.flags.loop.start;
		file->smp_loop_end   = flac_file.flags.loop.end + 1;
		file->smp_flags    |= (flac_file.flags.loop.type ? (CHN_LOOP | CHN_PINGPONGLOOP) : CHN_LOOP);
	}

	if (flac_file.flags.sample_rate)
		file->smp_speed = flac_file.flags.sample_rate;

	file->description  = "FLAC Audio File";
	file->type         = TYPE_SAMPLE_COMPR;
	file->smp_filename = file->base;

	return 1;
}

/* ------------------------------------------------------------------------ */
/* Now onto the writing stuff */

struct flac_writedata {
	FLAC__StreamEncoder *encoder;

	int bits;
	int channels;
};

static FLAC__StreamEncoderWriteStatus write_on_write(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[],
	size_t bytes, uint32_t samples, uint32_t current_frame, void *client_data) {
	disko_t* fp = (disko_t*)client_data;

	disko_write(fp, buffer, bytes);
	return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

static FLAC__StreamEncoderSeekStatus write_on_seek(const FLAC__StreamEncoder *encoder, FLAC__uint64 absolute_byte_offset, void *client_data) {
	disko_t* fp = (disko_t*)client_data;

	disko_seek(fp, absolute_byte_offset, SEEK_SET);
	return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

static FLAC__StreamEncoderTellStatus write_on_tell(const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset, void *client_data) {
	disko_t* fp = (disko_t*)client_data;

	long b = disko_tell(fp);
	if (b < 0)
		return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;

	if (absolute_byte_offset) *absolute_byte_offset = (FLAC__uint64)b;
	return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

static int flac_save_init(disko_t *fp, int bits, int channels, int rate, int estimate_num_samples)
{
	struct flac_writedata *fwd = malloc(sizeof(*fwd));
	if (!fwd)
		return -8;

	fwd->channels = channels;
	fwd->bits = bits;

	fwd->encoder = FLAC__stream_encoder_new();
	if (!fwd->encoder)
		return -1;

	if (!FLAC__stream_encoder_set_channels(fwd->encoder, channels))
		return -2;

	if (!FLAC__stream_encoder_set_bits_per_sample(fwd->encoder, bits))
		return -3;

	if (rate > FLAC__MAX_SAMPLE_RATE)
		rate = FLAC__MAX_SAMPLE_RATE;

	// FLAC only supports 10 Hz granularity for frequencies above 65535 Hz if the streamable subset is chosen, and only a maximum frequency of 655350 Hz.
	if (!FLAC__format_sample_rate_is_subset(rate))
		FLAC__stream_encoder_set_streamable_subset(fwd->encoder, false);

	if (!FLAC__stream_encoder_set_sample_rate(fwd->encoder, rate))
		return -4;

	if (!FLAC__stream_encoder_set_compression_level(fwd->encoder, 5))
		return -5;

	if (!FLAC__stream_encoder_set_total_samples_estimate(fwd->encoder, estimate_num_samples))
		return -6;

	if (!FLAC__stream_encoder_set_verify(fwd->encoder, false))
		return -7;

	FLAC__StreamEncoderInitStatus init_status = FLAC__stream_encoder_init_stream(
		fwd->encoder,
		write_on_write,
		write_on_seek,
		write_on_tell,
		NULL,
		fp
	);

	if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
		log_appendf(4, "ERROR: initializing FLAC encoder: %s\n", FLAC__StreamEncoderInitStatusString[init_status]);
		fprintf(stderr, "ERROR: initializing FLAC encoder: %s\n", FLAC__StreamEncoderInitStatusString[init_status]);
		return -8;
	}

	fp->userdata = fwd;

	return 0;
}

int fmt_flac_export_head(disko_t *fp, int bits, int channels, int rate)
{
	if (flac_save_init(fp, bits, channels, rate, 0))
		return DW_ERROR;

	return DW_OK;
}

int fmt_flac_export_body(disko_t *fp, const uint8_t *data, size_t length)
{
	struct flac_writedata *fwd = fp->userdata;
	const int bytes_per_sample = (fwd->bits / 8);

	FLAC__int32 pcm[length / bytes_per_sample];

	/* 8-bit/16-bit PCM -> 32-bit PCM */
	size_t i;
	for (i = 0; i < length / bytes_per_sample; i++) {
		if (bytes_per_sample == 2)
			pcm[i] = (FLAC__int32)(((const int16_t*)data)[i]);
		else if (bytes_per_sample == 1)
			pcm[i] = (FLAC__int32)(((const int8_t*)data)[i]);
		else
			return DW_ERROR;
	}

	if (!FLAC__stream_encoder_process_interleaved(fwd->encoder, pcm, length / (bytes_per_sample * fwd->channels)))
		return DW_ERROR;

	return DW_OK;
}

int fmt_flac_export_silence(disko_t *fp, long bytes)
{
	/* actually have to generate silence here */
	uint8_t silence[bytes];
	memset(silence, 0, sizeof(silence));

	return fmt_flac_export_body(fp, silence, bytes);
}

int fmt_flac_export_tail(disko_t *fp)
{
	struct flac_writedata *fwd = fp->userdata;

	FLAC__stream_encoder_finish(fwd->encoder);
	FLAC__stream_encoder_delete(fwd->encoder);

	free(fwd);

	return DW_OK;
}

/* need this because convering huge buffers in memory is KIND OF bad.
 * currently this is the same size as the buffer length in disko.c */
#define SAMPLE_BUFFER_LENGTH 65536

int fmt_flac_save_sample(disko_t *fp, song_sample_t *smp)
{
	if (flac_save_init(fp, (smp->flags & CHN_16BIT) ? 16 : 8, (smp->flags & CHN_STEREO) ? 2 : 1, smp->c5speed, smp->length))
		return SAVE_INTERNAL_ERROR;

	/* need to buffer this or else we'll make a HUGE array when
	 * saving huge samples */
	size_t offset;
	const size_t total_bytes = smp->length * ((smp->flags & CHN_16BIT) ? 2 : 1) * ((smp->flags & CHN_STEREO) ? 2 : 1);
	for (offset = 0; offset < total_bytes; offset += SAMPLE_BUFFER_LENGTH) {
		size_t needed = total_bytes - offset;
		if (fmt_flac_export_body(fp, (uint8_t*)smp->data + offset, MIN(needed, SAMPLE_BUFFER_LENGTH)) != DW_OK)
			return SAVE_INTERNAL_ERROR;
	}

	if (fmt_flac_export_tail(fp) != DW_OK)
		return SAVE_INTERNAL_ERROR;

	return SAVE_SUCCESS;
}
