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
#include "bshift.h"
#include "bswap.h"
#include "fmt.h"
#include "it.h"
#include "disko.h"
#include "player/sndfile.h"
#include "log.h"
#include "util.h"

#include <FLAC/stream_decoder.h>
#include <FLAC/stream_encoder.h>

#include <stdint.h>

static FLAC__StreamDecoder * (*schism_FLAC_stream_decoder_new)(void);
static FLAC__bool (*schism_FLAC_stream_decoder_set_metadata_respond_all)(FLAC__StreamDecoder *decoder);
static FLAC__StreamDecoderInitStatus (*schism_FLAC_stream_decoder_init_stream)(FLAC__StreamDecoder *decoder, FLAC__StreamDecoderReadCallback read_callback, FLAC__StreamDecoderSeekCallback seek_callback, FLAC__StreamDecoderTellCallback tell_callback, FLAC__StreamDecoderLengthCallback length_callback, FLAC__StreamDecoderEofCallback eof_callback, FLAC__StreamDecoderWriteCallback write_callback, FLAC__StreamDecoderMetadataCallback metadata_callback, FLAC__StreamDecoderErrorCallback error_callback, void *client_data);
static FLAC__bool (*schism_FLAC_stream_decoder_process_until_end_of_metadata)(FLAC__StreamDecoder *decoder);
static FLAC__bool (*schism_FLAC_stream_decoder_process_until_end_of_stream)(FLAC__StreamDecoder *decoder);
static FLAC__bool (*schism_FLAC_stream_decoder_finish)(FLAC__StreamDecoder *decoder);
static void (*schism_FLAC_stream_decoder_delete)(FLAC__StreamDecoder *decoder);
static const char *const *schism_FLAC_StreamDecoderErrorStatusString;

static FLAC__StreamEncoder * (*schism_FLAC_stream_encoder_new)(void);
static FLAC__bool (*schism_FLAC_stream_encoder_set_channels)(FLAC__StreamEncoder *encoder, uint32_t value);
static FLAC__bool (*schism_FLAC_stream_encoder_set_bits_per_sample)(FLAC__StreamEncoder *encoder, uint32_t value);
static FLAC__bool (*schism_FLAC_stream_encoder_set_streamable_subset)(FLAC__StreamEncoder *encoder, FLAC__bool value);
static FLAC__bool (*schism_FLAC_stream_encoder_set_sample_rate)(FLAC__StreamEncoder *encoder, uint32_t value);
static FLAC__bool (*schism_FLAC_stream_encoder_set_compression_level)(FLAC__StreamEncoder *encoder, uint32_t value);
static FLAC__bool (*schism_FLAC_stream_encoder_set_total_samples_estimate)(FLAC__StreamEncoder *encoder, FLAC__uint64 value);
static FLAC__bool (*schism_FLAC_stream_encoder_set_verify)(FLAC__StreamEncoder *encoder, FLAC__bool value);
static FLAC__StreamEncoderInitStatus (*schism_FLAC_stream_encoder_init_stream)(FLAC__StreamEncoder *encoder, FLAC__StreamEncoderWriteCallback write_callback, FLAC__StreamEncoderSeekCallback seek_callback, FLAC__StreamEncoderTellCallback tell_callback, FLAC__StreamEncoderMetadataCallback metadata_callback, void *client_data);
static FLAC__bool (*schism_FLAC_stream_encoder_process_interleaved)(FLAC__StreamEncoder *encoder, const FLAC__int32 buffer[], uint32_t samples);
static FLAC__bool (*schism_FLAC_stream_encoder_finish)(FLAC__StreamEncoder *encoder);
static void (*schism_FLAC_stream_encoder_delete)(FLAC__StreamEncoder *encoder);
static const char *const *schism_FLAC_StreamEncoderInitStatusString;

static FLAC__bool (*schism_FLAC_format_sample_rate_is_subset)(uint32_t sample_rate);

static int flac_wasinit = 0;

/* ----------------------------------------------------------------------------------- */
/* reading... */

struct flac_readdata {
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

	slurp_t *fp;

	struct {
		uint8_t *data;
		size_t len;

		uint32_t samples_decoded;
	} uncompressed;
};

static void read_on_meta(const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	struct flac_readdata* read_data = (struct flac_readdata*)client_data;
	int32_t loop_start = -1, loop_length = -1;

	switch (metadata->type) {
		case FLAC__METADATA_TYPE_STREAMINFO:
			memcpy(&read_data->streaminfo, &metadata->data.stream_info, sizeof(read_data->streaminfo));
			break;
		case FLAC__METADATA_TYPE_VORBIS_COMMENT:
			for (size_t i = 0; i < metadata->data.vorbis_comment.num_comments; i++) {
				const char *tag = (const char*)metadata->data.vorbis_comment.comments[i].entry;
				const FLAC__uint32 length = metadata->data.vorbis_comment.comments[i].length;

/* "name" must be a string literal for both of these */
#define CHECK_TAG_SIZE(name) \
	if (length > (sizeof(name "=")) && !strncasecmp(tag, name "=", sizeof(name "=")))

#define STRING_TAG(name, outvar, outvarsize) \
	CHECK_TAG_SIZE(name) { \
		strncpy(outvar, tag + sizeof(name "="), outvarsize); \
		continue; \
	}

#define INTEGER_TAG(name, outvar) \
	CHECK_TAG_SIZE(name) { \
		outvar = strtoll(tag + sizeof(name "="), NULL, 10); \
		continue; \
	}

				STRING_TAG("TITLE", read_data->flags.name, sizeof(read_data->flags.name));
				INTEGER_TAG("SAMPLERATE", read_data->flags.sample_rate);
				INTEGER_TAG("LOOPSTART", loop_start);
				INTEGER_TAG("LOOPLENGTH", loop_length);

#undef INTEGER_TAG
#undef STRING_TAG
#undef CHECK_TAG_SIZE
			}

			if (loop_start > 0 && loop_length > 1) {
				read_data->flags.loop.type = 0;
				read_data->flags.loop.start = loop_start;
				read_data->flags.loop.end = loop_start + loop_length - 1;
			}

			break;
		case FLAC__METADATA_TYPE_APPLICATION: {
			const uint8_t *data = (const uint8_t *)metadata->data.application.data;

			uint32_t chunk_id  = *(uint32_t*)data; data += sizeof(uint32_t);
			uint32_t chunk_len = *(uint32_t*)data; data += sizeof(uint32_t);

			if (chunk_id == bswapLE32(0x61727478) && chunk_len >= 8) { // "xtra"
				uint32_t xtra_flags = *(uint32_t*)data; data += sizeof(uint32_t);

				// panning (0..256)
				if (xtra_flags & 0x20) {
					uint16_t tmp_pan = *(uint16_t*)data;
					if (tmp_pan > 255)
						tmp_pan = 255;

					read_data->flags.pan = (uint8_t)tmp_pan;
				}
				data += sizeof(uint16_t);

				// volume (0..256)
				uint16_t tmp_vol = *(uint16_t*)data;
				if (tmp_vol > 256)
					tmp_vol = 256;

				read_data->flags.vol = (uint8_t)((tmp_vol + 2) / 4); // 0..256 -> 0..64 (rounded)
			}

			if (chunk_id == bswapLE32(0x6C706D73) && chunk_len > 52) { // "smpl"
				data += 28; // seek to first wanted byte

				uint32_t num_loops = *(uint32_t *)data; data += sizeof(uint32_t);
				if (num_loops == 1) {
					data += 4 + 4; // skip "samplerData" and "identifier"

					read_data->flags.loop.type  = *(uint32_t *)data; data += sizeof(uint32_t);
					read_data->flags.loop.start = *(uint32_t *)data; data += sizeof(uint32_t);
					read_data->flags.loop.end   = *(uint32_t *)data; data += sizeof(uint32_t);
				}
			}
			break;
		}
		default:
			break;
	}

	(void)decoder, (void)client_data;
}

static FLAC__StreamDecoderReadStatus read_on_read(const FLAC__StreamDecoder *decoder, FLAC__byte buffer[], size_t *bytes, void *client_data)
{
	slurp_t *fp = ((struct flac_readdata *)client_data)->fp;

	if (*bytes > 0) {
		*bytes = slurp_read(fp, buffer, *bytes);

		return (*bytes)
			? FLAC__STREAM_DECODER_READ_STATUS_CONTINUE
			: (slurp_eof(fp))
				? FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM
				: FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	} else {
		return FLAC__STREAM_DECODER_READ_STATUS_ABORT;
	}

	(void)decoder;
}

static FLAC__StreamDecoderSeekStatus read_on_seek(const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	slurp_t *fp = ((struct flac_readdata *)client_data)->fp;

	/* how? whatever */
	if (absolute_byte_offset > INT64_MAX)
		return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;

	return (slurp_seek(fp, absolute_byte_offset, SEEK_SET) >= 0)
		? FLAC__STREAM_DECODER_SEEK_STATUS_OK
		: FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
}

static FLAC__StreamDecoderTellStatus read_on_tell(const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	slurp_t *fp = ((struct flac_readdata *)client_data)->fp;

	int64_t off = slurp_tell(fp);
	if (off < 0)
		return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;

	*absolute_byte_offset = off;
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus read_on_length(const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
	/* XXX need a slurp_length() */
	*stream_length = (FLAC__uint64)slurp_length(((struct flac_readdata*)client_data)->fp);
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool read_on_eof(const FLAC__StreamDecoder *decoder, void *client_data)
{
	return slurp_eof(((struct flac_readdata*)client_data)->fp);
}

static void read_on_error(const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus status, void *client_data)
{
	log_appendf(4, "Error loading FLAC: %s", schism_FLAC_StreamDecoderErrorStatusString[status]);

	(void)decoder, (void)client_data;
}

static FLAC__StreamDecoderWriteStatus read_on_write(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data)
{
	struct flac_readdata* read_data = (struct flac_readdata*)client_data;

	/* invalid?; FIXME: this should probably make sure the total_samples
	 * is less than the max sample constant thing */
	if (!read_data->streaminfo.total_samples || !read_data->streaminfo.channels
		|| read_data->streaminfo.channels > 2)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	if (frame->header.number.sample_number == 0) {
		/* allocate our buffer. for some reason the length isn't the real
		 * length of the buffer in bytes but I can't be bothered to figure
		 * out why. */
		read_data->uncompressed.len = ((size_t)read_data->streaminfo.total_samples * read_data->streaminfo.channels * read_data->streaminfo.bits_per_sample/8);
		read_data->uncompressed.data = (uint8_t*)malloc(read_data->uncompressed.len * ((read_data->streaminfo.bits_per_sample == 8) ? sizeof(int8_t) : sizeof(int16_t)));
		if (!read_data->uncompressed.data)
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

		read_data->uncompressed.samples_decoded = 0;
	}

	uint32_t block_size = frame->header.blocksize * read_data->streaminfo.channels;

	const uint32_t samples_allocated = read_data->streaminfo.total_samples * read_data->streaminfo.channels;
	if (read_data->uncompressed.samples_decoded + block_size > samples_allocated)
		block_size = samples_allocated - read_data->uncompressed.samples_decoded;

	if (read_data->streaminfo.bits_per_sample <= 8) {
		int8_t *buf_ptr = (int8_t*)read_data->uncompressed.data + read_data->uncompressed.samples_decoded;
		uint32_t bit_shift = 8 - read_data->streaminfo.bits_per_sample;

		size_t i, j, c;
		for (i = 0, j = 0; i < block_size; j++)
			for (c = 0; c < read_data->streaminfo.channels; c++)
				buf_ptr[i++] = lshift_signed(buffer[c][j], bit_shift);
	} else if (read_data->streaminfo.bits_per_sample <= 16) {
		int16_t *buf_ptr = (int16_t*)read_data->uncompressed.data + read_data->uncompressed.samples_decoded;
		uint32_t bit_shift = 16 - read_data->streaminfo.bits_per_sample;

		size_t i, j, c;
		for (i = 0, j = 0; i < block_size; j++)
			for (c = 0; c < read_data->streaminfo.channels; c++)
				buf_ptr[i++] = lshift_signed(buffer[c][j], bit_shift);
	} else { /* >= 16 */
		int16_t *buf_ptr = (int16_t*)read_data->uncompressed.data + read_data->uncompressed.samples_decoded;
		uint32_t bit_shift = read_data->streaminfo.bits_per_sample - 16;

		size_t i, j, c;
		for (i = 0, j = 0; i < block_size; j++)
			for (c = 0; c < read_data->streaminfo.channels; c++)
				buf_ptr[i++] = rshift_signed(buffer[c][j], bit_shift);
	}

	read_data->uncompressed.samples_decoded += block_size;

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

	(void)decoder;
}

static int flac_load(struct flac_readdata* read_data, int meta_only)
{
	// err
	if (!flac_wasinit)
		return 0;

	unsigned char magic[4];

	slurp_rewind(read_data->fp); /* paranoia */

	if (slurp_peek(read_data->fp, magic, sizeof(magic)) != sizeof(magic)
		|| memcmp(magic, "fLaC", sizeof(magic)))
		return 0;

	FLAC__StreamDecoder *decoder = schism_FLAC_stream_decoder_new();
	if (!decoder)
		return 0;

	schism_FLAC_stream_decoder_set_metadata_respond_all(decoder);

	FLAC__StreamDecoderInitStatus status =
		schism_FLAC_stream_decoder_init_stream(
			decoder,
			read_on_read, read_on_seek,
			read_on_tell, read_on_length,
			read_on_eof,  read_on_write,
			read_on_meta, read_on_error,
			read_data
		);
	if (status != FLAC__STREAM_DECODER_INIT_STATUS_OK)
		return 0;

	/* flac function names are such a yapfest */
	if (!(meta_only ? schism_FLAC_stream_decoder_process_until_end_of_metadata(decoder) : schism_FLAC_stream_decoder_process_until_end_of_stream(decoder))) {
		schism_FLAC_stream_decoder_delete(decoder);
		return 0;
	}

	schism_FLAC_stream_decoder_finish(decoder);
	schism_FLAC_stream_decoder_delete(decoder);

	return 1;
}
#undef FLAC_ERROR

int fmt_flac_load_sample(slurp_t *fp, song_sample_t *smp)
{
	struct flac_readdata read_data = {
		.fp = fp,
		.flags = {
			.sample_rate = 0,
			.loop = {
				.type = -1,
			},
		},
	};

	if (!flac_load(&read_data, 0))
		return 0;

	smp->volume        = 64 * 4;
	smp->global_volume = 64;
	smp->c5speed       = read_data.streaminfo.sample_rate;
	smp->length        = read_data.streaminfo.total_samples;
	if (read_data.flags.loop.type != -1) {
		smp->loop_start = read_data.flags.loop.start;
		smp->loop_end   = read_data.flags.loop.end + 1;
		smp->flags |= (read_data.flags.loop.type ? (CHN_LOOP | CHN_PINGPONGLOOP) : CHN_LOOP);
	}

	if (read_data.flags.sample_rate)
		smp->c5speed = read_data.flags.sample_rate;

	// endianness, based on host system
	uint32_t flags = 0;
#ifdef WORDS_BIGENDIAN
	flags |= SF_BE;
#else
	flags |= SF_LE;
#endif

	// channels
	flags |= (read_data.streaminfo.channels == 2) ? SF_SI : SF_M;

	// bit width
	flags |= (read_data.streaminfo.bits_per_sample <= 8) ? SF_8 : SF_16;

	// libFLAC always returns signed
	flags |= SF_PCMS;

	slurp_t fake_fp;
	slurp_memstream(&fake_fp, read_data.uncompressed.data, read_data.uncompressed.len);

	int ret = csf_read_sample(smp, flags, &fake_fp);

	free(read_data.uncompressed.data);

	return ret;
}

int fmt_flac_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct flac_readdata read_data = {
		.fp = fp,
		.flags = {
			.sample_rate = 0,
			.loop = {
				.type = -1,
			},
		},
	};

	if (!flac_load(&read_data, 1))
		return 0;

	file->smp_flags = 0;

	/* don't even attempt */
	if (read_data.streaminfo.channels > 2 || !read_data.streaminfo.total_samples ||
		!read_data.streaminfo.channels)
		return 0;

	if (read_data.streaminfo.bits_per_sample > 8)
		file->smp_flags |= CHN_16BIT;

	if (read_data.streaminfo.channels == 2)
		file->smp_flags |= CHN_STEREO;

	file->smp_speed  = read_data.streaminfo.sample_rate;
	file->smp_length = read_data.streaminfo.total_samples;

	/* stupid magic numbers... */
	if (read_data.flags.loop.type >= 0) {
		file->smp_loop_start = read_data.flags.loop.start;
		file->smp_loop_end   = read_data.flags.loop.end + 1;
		file->smp_flags    |= (read_data.flags.loop.type ? (CHN_LOOP | CHN_PINGPONGLOOP) : CHN_LOOP);
	}

	if (read_data.flags.sample_rate)
		file->smp_speed = read_data.flags.sample_rate;

	file->description  = "FLAC Audio File";
	file->type         = TYPE_SAMPLE_COMPR;
	file->smp_filename = file->base;

	return 1;
}

/* ------------------------------------------------------------------------ */
/* Now onto the writing stuff */

struct flac_writedata {
	/* TODO would be nice to save some metadata */
	FLAC__StreamEncoder *encoder;

	int bits;
	int channels;
};

static FLAC__StreamEncoderWriteStatus write_on_write(const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[],
	size_t bytes, uint32_t samples, uint32_t current_frame, void *client_data)
{
	disko_t* fp = (disko_t*)client_data;

	disko_write(fp, buffer, bytes);
	return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

static FLAC__StreamEncoderSeekStatus write_on_seek(const FLAC__StreamEncoder *encoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	disko_t* fp = (disko_t*)client_data;

	disko_seek(fp, absolute_byte_offset, SEEK_SET);
	return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

static FLAC__StreamEncoderTellStatus write_on_tell(const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	disko_t* fp = (disko_t*)client_data;

	long b = disko_tell(fp);
	if (b < 0)
		return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;

	if (absolute_byte_offset) *absolute_byte_offset = (FLAC__uint64)b;
	return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

static int flac_save_init(disko_t *fp, int bits, int channels, int rate, int estimate_num_samples)
{
	if (!flac_wasinit)
		return -9;

	struct flac_writedata *fwd = malloc(sizeof(*fwd));
	if (!fwd)
		return -8;

	fwd->channels = channels;
	fwd->bits = bits;

	fwd->encoder = schism_FLAC_stream_encoder_new();
	if (!fwd->encoder)
		return -1;

	if (!schism_FLAC_stream_encoder_set_channels(fwd->encoder, channels))
		return -2;

	if (!schism_FLAC_stream_encoder_set_bits_per_sample(fwd->encoder, bits))
		return -3;

	if (rate > FLAC__MAX_SAMPLE_RATE)
		rate = FLAC__MAX_SAMPLE_RATE;

	// FLAC only supports 10 Hz granularity for frequencies above 65535 Hz if the streamable subset is chosen, and only a maximum frequency of 655350 Hz.
	if (!schism_FLAC_format_sample_rate_is_subset(rate))
		schism_FLAC_stream_encoder_set_streamable_subset(fwd->encoder, false);

	if (!schism_FLAC_stream_encoder_set_sample_rate(fwd->encoder, rate))
		return -4;

	if (!schism_FLAC_stream_encoder_set_compression_level(fwd->encoder, 5))
		return -5;

	if (!schism_FLAC_stream_encoder_set_total_samples_estimate(fwd->encoder, estimate_num_samples))
		return -6;

	if (!schism_FLAC_stream_encoder_set_verify(fwd->encoder, false))
		return -7;

	FLAC__StreamEncoderInitStatus init_status = schism_FLAC_stream_encoder_init_stream(
		fwd->encoder,
		write_on_write,
		write_on_seek,
		write_on_tell,
		NULL, /* metadata callback */
		fp
	);

	if (init_status != FLAC__STREAM_ENCODER_INIT_STATUS_OK) {
		log_appendf(4, "ERROR: initializing FLAC encoder: %s\n", schism_FLAC_StreamEncoderInitStatusString[init_status]);
		fprintf(stderr, "ERROR: initializing FLAC encoder: %s\n", schism_FLAC_StreamEncoderInitStatusString[init_status]);
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

	if (!schism_FLAC_stream_encoder_process_interleaved(fwd->encoder, pcm, length / (bytes_per_sample * fwd->channels)))
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

	schism_FLAC_stream_encoder_finish(fwd->encoder);
	schism_FLAC_stream_encoder_delete(fwd->encoder);

	free(fwd);

	return DW_OK;
}

/* need this because convering huge buffers in memory is KIND OF bad.
 * currently this is the same size as the buffer length in disko.c */
#define SAMPLE_BUFFER_LENGTH 65536

int fmt_flac_save_sample(disko_t *fp, song_sample_t *smp)
{
	if (smp->flags & CHN_ADLIB)
		return SAVE_UNSUPPORTED;

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

/* --------------------------------------------------------------- */

static int load_flac_syms(void);

#ifdef FLAC_DYNAMIC_LOAD

#include "loadso.h"

void *flac_dltrick_handle_ = NULL;

static void flac_dlend(void)
{
	if (flac_dltrick_handle_) {
		loadso_object_unload(flac_dltrick_handle_);
		flac_dltrick_handle_ = NULL;
	}
}

static int flac_dlinit(void)
{
	int i;

	// already have it?
	if (flac_dltrick_handle_)
		return 0;

	flac_dltrick_handle_ = library_load("FLAC", FLAC_API_VERSION_CURRENT, FLAC_API_VERSION_AGE);
	if (!flac_dltrick_handle_)
		return -1;

	int retval = load_flac_syms();
	if (retval < 0)
		flac_dlend();

	return retval;
}

// this is always true under SDL but I'm paranoid
SCHISM_STATIC_ASSERT(sizeof(void (*)) == sizeof(void *), "dynamic loading code assumes function pointer and void pointer are of equivalent size");

static int load_flac_sym(const char *fn, void *addr)
{
	void *func = loadso_function_load(flac_dltrick_handle_, fn);
	if (!func)
		return 0;

	memcpy(addr, &func, sizeof(void *));

	return 1;
}

#define SCHISM_FLAC_SYM(x) \
	if (!load_flac_sym("FLAC__" #x, &schism_FLAC_##x)) return -1

#else

#define SCHISM_FLAC_SYM(x) schism_FLAC_##x = FLAC__##x

static int flac_dlinit(void)
{
	load_flac_syms();
	return 0;
}

#endif

static int load_flac_syms(void)
{
	SCHISM_FLAC_SYM(stream_decoder_new);
	SCHISM_FLAC_SYM(stream_decoder_set_metadata_respond_all);
	SCHISM_FLAC_SYM(stream_decoder_init_stream);
	SCHISM_FLAC_SYM(stream_decoder_process_until_end_of_metadata);
	SCHISM_FLAC_SYM(stream_decoder_process_until_end_of_stream);
	SCHISM_FLAC_SYM(stream_decoder_finish);
	SCHISM_FLAC_SYM(stream_decoder_delete);
	SCHISM_FLAC_SYM(StreamDecoderErrorStatusString);

	SCHISM_FLAC_SYM(stream_encoder_new);
	SCHISM_FLAC_SYM(stream_encoder_set_channels);
	SCHISM_FLAC_SYM(stream_encoder_set_bits_per_sample);
	SCHISM_FLAC_SYM(stream_encoder_set_streamable_subset);
	SCHISM_FLAC_SYM(stream_encoder_set_sample_rate);
	SCHISM_FLAC_SYM(stream_encoder_set_compression_level);
	SCHISM_FLAC_SYM(stream_encoder_set_total_samples_estimate);
	SCHISM_FLAC_SYM(stream_encoder_set_verify);
	SCHISM_FLAC_SYM(stream_encoder_init_stream);
	SCHISM_FLAC_SYM(stream_encoder_process_interleaved);
	SCHISM_FLAC_SYM(stream_encoder_finish);
	SCHISM_FLAC_SYM(stream_encoder_delete);
	SCHISM_FLAC_SYM(StreamEncoderInitStatusString);

	SCHISM_FLAC_SYM(format_sample_rate_is_subset);

	return 0;
}

/* ------------------------------------------------- */

int flac_init(void)
{
#ifdef FLAC_DYNAMIC_LOAD
	if (!flac_dltrick_handle_)
#endif
		if (flac_dlinit() < 0)
			return 0;

	audio_enable_flac(1);
	flac_wasinit = 1;
	return 1;
}

int flac_quit(void)
{
#ifdef FLAC_DYNAMIC_LOAD
	if (flac_wasinit)
		flac_dlend();
#endif
	return 1;
}
