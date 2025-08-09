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
#include "bits.h"
#include "fmt.h"
#include "it.h"
#include "disko.h"
#include "player/sndfile.h"
#include "log.h"
#include "util.h"
#include "mem.h"

#ifdef SCHISM_WIN32
/* i hate nonportable APIs */
# define off_t _off_t
#endif

#include <FLAC/stream_decoder.h>
#include <FLAC/stream_encoder.h>
#include <FLAC/metadata.h>

/* used by reading functions */
static FLAC__StreamDecoder * (*schism_FLAC_stream_decoder_new)(void);
static FLAC__bool (*schism_FLAC_stream_decoder_set_metadata_respond_all)(FLAC__StreamDecoder *decoder);
static FLAC__StreamDecoderInitStatus (*schism_FLAC_stream_decoder_init_stream)(FLAC__StreamDecoder *decoder, FLAC__StreamDecoderReadCallback read_callback, FLAC__StreamDecoderSeekCallback seek_callback, FLAC__StreamDecoderTellCallback tell_callback, FLAC__StreamDecoderLengthCallback length_callback, FLAC__StreamDecoderEofCallback eof_callback, FLAC__StreamDecoderWriteCallback write_callback, FLAC__StreamDecoderMetadataCallback metadata_callback, FLAC__StreamDecoderErrorCallback error_callback, void *client_data);
static FLAC__bool (*schism_FLAC_stream_decoder_process_until_end_of_metadata)(FLAC__StreamDecoder *decoder);
static FLAC__bool (*schism_FLAC_stream_decoder_process_until_end_of_stream)(FLAC__StreamDecoder *decoder);
static FLAC__bool (*schism_FLAC_stream_decoder_finish)(FLAC__StreamDecoder *decoder);
static void (*schism_FLAC_stream_decoder_delete)(FLAC__StreamDecoder *decoder);
static FLAC__StreamDecoderState (*schism_FLAC_stream_decoder_get_state)(const FLAC__StreamDecoder *decoder);
static const char *const *schism_FLAC_StreamDecoderErrorStatusString;
static const char *const *schism_FLAC_StreamDecoderStateString;

/* used by writing functions */
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
static FLAC__bool (*schism_FLAC_stream_encoder_set_metadata)(FLAC__StreamEncoder *encoder, FLAC__StreamMetadata **metadata, uint32_t num_blocks);
static FLAC__StreamEncoderState (*schism_FLAC_stream_encoder_get_state)(const FLAC__StreamEncoder *encoder);
static const char *const *schism_FLAC_StreamEncoderInitStatusString;
static const char *const *schism_FLAC_StreamEncoderStateString;

static FLAC__StreamMetadata *(*schism_FLAC_metadata_object_new)(FLAC__MetadataType type); 	
static void (*schism_FLAC_metadata_object_delete)(FLAC__StreamMetadata *object); 	
static FLAC__bool (*schism_FLAC_metadata_object_application_set_data)(FLAC__StreamMetadata *object, FLAC__byte *data, uint32_t length, FLAC__bool copy);
static FLAC__bool (*schism_FLAC_metadata_object_vorbiscomment_append_comment)(FLAC__StreamMetadata *object, FLAC__StreamMetadata_VorbisComment_Entry entry, FLAC__bool copy);

static FLAC__bool (*schism_FLAC_format_sample_rate_is_subset)(uint32_t sample_rate);

static int flac_wasinit = 0;

/* ----------------------------------------------------------------------------------- */
/* reading... */

struct flac_readdata {
	slurp_t *fp;

	song_sample_t *smp;

	/* uncompressed sample data and info about it */
	uint8_t *data;

	uint32_t bits; /* 8, 16, or 32 (24 gets rounded up) */
	uint32_t channels; /* channels in the stream */

	/* STREAM INFO */
	uint32_t stream_bits; /* actual bitsize of the stream */
	uint32_t offset; /* used while decoding */
};

/* this should probably be elsewhere
 * note: maybe bswap.h and bshift.h should be merged to a bits.h */
static inline SCHISM_ALWAYS_INLINE uint32_t ceil_pow2_32(uint32_t x)
{
	/* from Bit Twiddling Hacks */
	x--;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	x++;

	return x;
}

static void read_on_meta(SCHISM_UNUSED const FLAC__StreamDecoder *decoder, const FLAC__StreamMetadata *metadata, void *client_data)
{
	struct flac_readdata* read_data = (struct flac_readdata*)client_data;
	song_sample_t *smp = read_data->smp;

	switch (metadata->type) {
	case FLAC__METADATA_TYPE_STREAMINFO: {
		/* supposedly, this is always first */
		const FLAC__StreamMetadata_StreamInfo *streaminfo = &metadata->data.stream_info;

		/* copy */
		smp->c5speed = streaminfo->sample_rate;
		smp->length = streaminfo->total_samples;
		read_data->stream_bits = streaminfo->bits_per_sample;
		read_data->channels = streaminfo->channels;

		read_data->bits = ceil_pow2_32(read_data->stream_bits);

		break;
	}
	case FLAC__METADATA_TYPE_VORBIS_COMMENT: {
		int32_t loop_start = -1, loop_length = -1;
		size_t i;

		for (i = 0; i < metadata->data.vorbis_comment.num_comments; i++) {
			char *s;

			{
				const char *tag = (const char*)metadata->data.vorbis_comment.comments[i].entry;
				const FLAC__uint32 length = metadata->data.vorbis_comment.comments[i].length;

				/* copy, adding a NUL terminator (guarantee nul termination) */
				s = strn_dup(tag, length);
			}

			if (sscanf(s, "TITLE=%25s", smp->name) == 1);
			else if (sscanf(s, "SAMPLERATE=%" SCNu32, &smp->c5speed) == 1);
			else if (sscanf(s, "LOOPSTART=%" SCNd32, &loop_start) == 1);
			else if (sscanf(s, "LOOPLENGTH=%" SCNd32, &loop_length) == 1);
			else {
#if 0
				log_appendf(5, " FLAC: unknown vorbis comment '%s'\n", s);
#endif
			}

			free(s);
		}

		if (loop_start > 0 && loop_length > 1) {
			smp->flags |= CHN_LOOP;

			smp->loop_start = loop_start;
			smp->loop_end = loop_start + loop_length;
		}

		break;
	}
	case FLAC__METADATA_TYPE_APPLICATION: {
		slurp_t app_fp;
		uint32_t chunk_id, chunk_len;

		/* All chunks we read have the application ID "riff" */
		if (memcmp(&metadata->data.application.id, "riff", 4))
			break;

		if (metadata->length < 4)
			break;

		slurp_memstream(&app_fp, metadata->data.application.data, metadata->length - 4);

		if (slurp_read(&app_fp, &chunk_id, 4) != 4)
			break;
		if (slurp_read(&app_fp, &chunk_len, 4) != 4)
			break;

		chunk_id = bswapLE32(chunk_id);
		chunk_len = bswapLE32(chunk_len);

		switch (chunk_id) {
		case UINT32_C(0x61727478): /* "xtra" */
			iff_read_xtra_chunk(&app_fp, read_data->smp);
			break;
		case UINT32_C(0x6C706D73): /* "smpl" */
			iff_read_smpl_chunk(&app_fp, read_data->smp);
			break;
		default:
			break;
		}
		break;
	}
	default:
		break;
	}
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

static FLAC__StreamDecoderSeekStatus read_on_seek(SCHISM_UNUSED const FLAC__StreamDecoder *decoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	slurp_t *fp = ((struct flac_readdata *)client_data)->fp;

	/* how? whatever */
	if (absolute_byte_offset > INT64_MAX)
		return FLAC__STREAM_DECODER_SEEK_STATUS_UNSUPPORTED;

	return (slurp_seek(fp, absolute_byte_offset, SEEK_SET) >= 0)
		? FLAC__STREAM_DECODER_SEEK_STATUS_OK
		: FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
}

static FLAC__StreamDecoderTellStatus read_on_tell(SCHISM_UNUSED const FLAC__StreamDecoder *decoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	slurp_t *fp = ((struct flac_readdata *)client_data)->fp;

	int64_t off = slurp_tell(fp);
	if (off < 0)
		return FLAC__STREAM_DECODER_TELL_STATUS_ERROR;

	*absolute_byte_offset = off;
	return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}

static FLAC__StreamDecoderLengthStatus read_on_length(SCHISM_UNUSED const FLAC__StreamDecoder *decoder, FLAC__uint64 *stream_length, void *client_data)
{
	*stream_length = (FLAC__uint64)slurp_length(((struct flac_readdata*)client_data)->fp);
	return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}

static FLAC__bool read_on_eof(SCHISM_UNUSED const FLAC__StreamDecoder *decoder, void *client_data)
{
	return slurp_eof(((struct flac_readdata*)client_data)->fp);
}

static void read_on_error(SCHISM_UNUSED const FLAC__StreamDecoder *decoder, FLAC__StreamDecoderErrorStatus xx, void *client_data)
{
	log_appendf(4, "Error loading FLAC: %s", schism_FLAC_StreamDecoderErrorStatusString[xx]);

	(void)decoder, (void)client_data;
}

static FLAC__StreamDecoderWriteStatus read_on_write(const FLAC__StreamDecoder *decoder, const FLAC__Frame *frame, const FLAC__int32 *const buffer[], void *client_data)
{
	struct flac_readdata* read_data = (struct flac_readdata*)client_data;
	uint32_t block_size, offset;

	/* invalid?; FIXME: this should probably make sure the total_samples
	 * is less than the max sample constant thing */
	if (!read_data->smp->length || read_data->smp->length > MAX_SAMPLE_LENGTH
		|| read_data->channels > 2
		|| read_data->bits > 32)
		return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;

	if (!frame->header.number.sample_number) {
		/* allocate */
		read_data->data = malloc(read_data->smp->length * read_data->channels * (read_data->bits / 8));
		if (!read_data->data)
			return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

	block_size = MIN(frame->header.blocksize, read_data->smp->length - frame->header.number.sample_number);
	block_size = MIN(block_size, read_data->smp->length - read_data->offset);

	block_size *= read_data->channels;

	offset = frame->header.number.sample_number * read_data->channels;

	/* this is stupid ugly */
#define RESIZE_BUFFER(BITS) \
	do { \
		int##BITS##_t *buf_ptr = (int##BITS##_t*)read_data->data + offset; \
		const uint32_t bit_shift = BITS - read_data->stream_bits; \
		uint32_t i, j, c; \
	\
		for (i = 0, j = 0; i < block_size; j++) \
			for (c = 0; c < read_data->channels; c++) \
				buf_ptr[i++] = lshift_signed(buffer[c][j], bit_shift); \
	} while (0)

	switch (read_data->bits) {
	case 8:  RESIZE_BUFFER(8);  break;
	case 16: RESIZE_BUFFER(16); break;
	case 32: RESIZE_BUFFER(32); break;
	default: return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
	}

#undef RESIZE_BUFFER

	return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;

	(void)decoder;
}

static int flac_load(struct flac_readdata* read_data, int meta_only)
{
	unsigned char magic[4];
	FLAC__StreamDecoder *decoder;
	FLAC__StreamDecoderInitStatus inits;

	// err
	if (!flac_wasinit)
		return 0;

	slurp_rewind(read_data->fp); /* paranoia */

	if (slurp_peek(read_data->fp, magic, sizeof(magic)) != sizeof(magic)
		|| memcmp(magic, "fLaC", sizeof(magic)))
		return 0;

	decoder = schism_FLAC_stream_decoder_new();
	if (!decoder)
		return 0;

	schism_FLAC_stream_decoder_set_metadata_respond_all(decoder);

	inits = schism_FLAC_stream_decoder_init_stream(
		decoder,
		read_on_read, read_on_seek,
		read_on_tell, read_on_length,
		read_on_eof,  read_on_write,
		read_on_meta, read_on_error,
		read_data
	);
	if (inits != FLAC__STREAM_DECODER_INIT_STATUS_OK)
		return 0;

	/* flac function names are such a yapfest */
	if (!(meta_only ? schism_FLAC_stream_decoder_process_until_end_of_metadata(decoder) : schism_FLAC_stream_decoder_process_until_end_of_stream(decoder))) {
		log_appendf(4, " [FLAC] failed to read file: %s", schism_FLAC_StreamDecoderStateString[schism_FLAC_stream_decoder_get_state(decoder)]);
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
		.smp = smp,
	};

	if (!flac_load(&read_data, 0))
		return 0;

	/* libFLAC *always* returns signed */
	uint32_t flags = SF_PCMS;

	// endianness, based on host system
#ifdef WORDS_BIGENDIAN
	flags |= SF_BE;
#else
	flags |= SF_LE;
#endif

	// channels
	flags |= (read_data.channels == 2) ? SF_SI : SF_M;

	// bit width
	switch (read_data.bits) {
	case 8:  flags |= SF_8; break;
	case 16: flags |= SF_16; break;
	case 32: flags |= SF_32; break;
	default: /* csf_read_sample will fail */ break;
	}

	slurp_t fake_fp;
	slurp_memstream(&fake_fp, read_data.data, (read_data.bits / 8) * read_data.channels * smp->length);

	int ret = csf_read_sample(smp, flags, &fake_fp);

	free(read_data.data);

	return ret;
}

int fmt_flac_read_info(dmoz_file_t *file, slurp_t *fp)
{
	song_sample_t smp = {0};

	smp.volume = 64 * 4;
	smp.global_volume = 64;

	struct flac_readdata read_data = {
		.fp = fp,
		.smp = &smp,
	};

	if (!flac_load(&read_data, 1))
		return 0;

	fmt_fill_file_from_sample(file, &smp);

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

static FLAC__StreamEncoderWriteStatus write_on_write(SCHISM_UNUSED const FLAC__StreamEncoder *encoder, const FLAC__byte buffer[],
	size_t bytes, SCHISM_UNUSED uint32_t samples, SCHISM_UNUSED uint32_t current_frame, void *client_data)
{
	disko_t* fp = (disko_t*)client_data;

	disko_write(fp, buffer, bytes);
	return FLAC__STREAM_ENCODER_WRITE_STATUS_OK;
}

static FLAC__StreamEncoderSeekStatus write_on_seek(SCHISM_UNUSED const FLAC__StreamEncoder *encoder, FLAC__uint64 absolute_byte_offset, void *client_data)
{
	disko_t* fp = (disko_t*)client_data;

	disko_seek(fp, absolute_byte_offset, SEEK_SET);
	return FLAC__STREAM_ENCODER_SEEK_STATUS_OK;
}

static FLAC__StreamEncoderTellStatus write_on_tell(SCHISM_UNUSED const FLAC__StreamEncoder *encoder, FLAC__uint64 *absolute_byte_offset, void *client_data)
{
	disko_t* fp = (disko_t*)client_data;

	long b = disko_tell(fp);
	if (b < 0)
		return FLAC__STREAM_ENCODER_TELL_STATUS_ERROR;

	if (absolute_byte_offset) *absolute_byte_offset = (FLAC__uint64)b;
	return FLAC__STREAM_ENCODER_TELL_STATUS_OK;
}

static int flac_save_init_head(disko_t *fp, int bits, int channels, int rate, int estimate_num_samples)
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

	if (rate > (int)FLAC__MAX_SAMPLE_RATE)
		rate = (int)FLAC__MAX_SAMPLE_RATE;

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

	fp->userdata = fwd;

	return 0;
}

static inline int flac_save_init_tail(disko_t *fp)
{
	struct flac_writedata *fwd = fp->userdata;

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

	return 0;
}

static int flac_save_init(disko_t *fp, int bits, int channels, int rate, int estimate_num_samples)
{
	if (flac_save_init_head(fp, bits, channels, rate, estimate_num_samples) || flac_save_init_tail(fp))
		return -1;

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

	if (length % bytes_per_sample) {
		log_appendf(4, "FLAC export: received uneven length");
		return DW_ERROR;
	}

	length /= bytes_per_sample;

	if (bytes_per_sample == 4) {
		if (!schism_FLAC_stream_encoder_process_interleaved(fwd->encoder, (FLAC__int32 *)data, length / fwd->channels))
			return DW_ERROR;
	} else {
		FLAC__int32 *pcm = mem_alloc(length * 4);

		/* 8-bit/16-bit PCM -> 32-bit PCM */
		size_t i;
		for (i = 0; i < length; i++) {
			switch (bytes_per_sample) {
			case 1: pcm[i] = (FLAC__int32)(((const int8_t*)data)[i]); break;
			case 2: pcm[i] = (FLAC__int32)(((const int16_t*)data)[i]); break;
			case 3: {
				union { int32_t s; uint32_t u; } x;

				x.u = (
#ifdef WORDS_BIGENDIAN
					((uint32_t)data[i*3+0] << 24)
					| ((uint32_t)data[i*3+1] << 16)
					| ((uint32_t)data[i*3+2] << 8)
#else
					((uint32_t)data[i*3+2] << 24)
					| ((uint32_t)data[i*3+1] << 16)
					| ((uint32_t)data[i*3+0] << 8)
#endif
				);

				/* sign extend */
				pcm[i] = rshift_signed(x.s, 8);
				break;
			}
			default: free(pcm); return DW_ERROR;
			}
		}

		if (!schism_FLAC_stream_encoder_process_interleaved(fwd->encoder, pcm, length / fwd->channels)) {
			free(pcm);
			return DW_ERROR;
		}

		free(pcm);
	}

	return DW_OK;
}

int fmt_flac_export_silence(disko_t *fp, long bytes)
{
	/* actually have to generate silence here */
	uint8_t *silence = mem_calloc(1, bytes);

	int res = fmt_flac_export_body(fp, silence, bytes);

	free(silence);

	return res;
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

static SCHISM_FORMAT_PRINTF(2, 3) int flac_append_vorbis_comment(FLAC__StreamMetadata *metadata, const char *format, ...)
{
	char *s;
	int x;

	{
		va_list ap;

		va_start(ap, format);

		x = vasprintf(&s, format, ap);

		va_end(ap);
	}

	if (x > 0) {
		FLAC__StreamMetadata_VorbisComment_Entry e;

		e.length = x;
		e.entry = (FLAC__byte *)s;

		schism_FLAC_metadata_object_vorbiscomment_append_comment(metadata, e, 1);

		free(s);

		return 1;
	}

	return 0;
}

int fmt_flac_save_sample(disko_t *fp, song_sample_t *smp)
{
	struct flac_writedata *fwd;

	/* metadata structures & the amount */
	FLAC__StreamMetadata *metadata_ptrs[3] = {0};
	uint32_t num_metadata = 0, i;

	if (smp->flags & CHN_ADLIB)
		return SAVE_UNSUPPORTED;

	if (flac_save_init_head(fp, (smp->flags & CHN_16BIT) ? 16 : 8, (smp->flags & CHN_STEREO) ? 2 : 1, smp->c5speed, smp->length))
		return SAVE_INTERNAL_ERROR;

	/* okay, now we have to hijack the writedata, so we can set metadata */
	fwd = fp->userdata;

	metadata_ptrs[num_metadata] = schism_FLAC_metadata_object_new(FLAC__METADATA_TYPE_APPLICATION);
	if (metadata_ptrs[num_metadata]) {
		uint32_t length;
		unsigned char xtra[IFF_XTRA_CHUNK_SIZE];

		iff_fill_xtra_chunk(smp, xtra, &length);

		/* now shove it into the metadata */
		if (schism_FLAC_metadata_object_application_set_data(metadata_ptrs[num_metadata], xtra, length, 1)) {
			memcpy(metadata_ptrs[num_metadata]->data.application.id, "riff", 4);
			num_metadata++;
		}
	}

	metadata_ptrs[num_metadata] = schism_FLAC_metadata_object_new(FLAC__METADATA_TYPE_APPLICATION);
	if (metadata_ptrs[num_metadata]) {
		uint32_t length;
		unsigned char smpl[IFF_SMPL_CHUNK_SIZE];

		iff_fill_smpl_chunk(smp, smpl, &length);

		if (schism_FLAC_metadata_object_application_set_data(metadata_ptrs[num_metadata], smpl, length, 1)) {
			memcpy(metadata_ptrs[num_metadata]->data.application.id, "riff", 4);
			num_metadata++;
		}
	}

	/* this shouldn't be needed for export */
	metadata_ptrs[num_metadata] = schism_FLAC_metadata_object_new(FLAC__METADATA_TYPE_VORBIS_COMMENT);
	if (metadata_ptrs[num_metadata]) {
		if (flac_append_vorbis_comment(metadata_ptrs[num_metadata], "SAMPLERATE=%" PRIu32, smp->c5speed)
			&& flac_append_vorbis_comment(metadata_ptrs[num_metadata], "TITLE=%.*s", (int)sizeof(smp->name), smp->name))
			num_metadata++;
	}

	if (!schism_FLAC_stream_encoder_set_metadata(fwd->encoder, metadata_ptrs, num_metadata)) {
		log_appendf(4, " FLAC__stream_encoder_set_metadata: %s", schism_FLAC_StreamEncoderStateString[schism_FLAC_stream_encoder_get_state(fwd->encoder)]);
		return SAVE_INTERNAL_ERROR;
	}

	if (flac_save_init_tail(fp)) {
		log_appendf(4, " flac_save_init_tail: %s", schism_FLAC_StreamEncoderStateString[schism_FLAC_stream_encoder_get_state(fwd->encoder)]);
		return SAVE_INTERNAL_ERROR;
	}

	/* buffer this */
	size_t offset;
	const size_t total_bytes = smp->length * ((smp->flags & CHN_16BIT) ? 2 : 1) * ((smp->flags & CHN_STEREO) ? 2 : 1);
	for (offset = 0; offset < total_bytes; offset += SAMPLE_BUFFER_LENGTH) {
		size_t needed = total_bytes - offset;
		if (fmt_flac_export_body(fp, (uint8_t*)smp->data + offset, MIN(needed, SAMPLE_BUFFER_LENGTH)) != DW_OK) {
			log_appendf(4, " fmt_flac_export_body: %s", schism_FLAC_StreamEncoderStateString[schism_FLAC_stream_encoder_get_state(fwd->encoder)]);
			return SAVE_INTERNAL_ERROR;
		}
	}

	if (fmt_flac_export_tail(fp) != DW_OK)
		return SAVE_INTERNAL_ERROR;

	/* cleanup the metadata crap */
	for (i = 0; i < num_metadata; i++)
		schism_FLAC_metadata_object_delete(metadata_ptrs[i]);

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
	int retval;

	// already have it?
	if (flac_dltrick_handle_)
		return 0;

	flac_dltrick_handle_ = library_load("FLAC", FLAC_API_VERSION_CURRENT, FLAC_API_VERSION_AGE);
	if (!flac_dltrick_handle_)
		return -1;

	retval = load_flac_syms();
	if (retval < 0)
		flac_dlend();

	return retval;
}

// this is always true under SDL but I'm paranoid
SCHISM_STATIC_ASSERT(sizeof(void (*)(void)) == sizeof(void *), "dynamic loading code assumes function pointer and void pointer are of equivalent size");

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

#define SCHISM_FLAC_SYM_ARR(x) SCHISM_FLAC_SYM(x)

#else

/* need to do this differently because C is archaic */
#define SCHISM_FLAC_SYM(x) schism_FLAC_##x = &FLAC__##x
#define SCHISM_FLAC_SYM_ARR(x) schism_FLAC_##x = FLAC__##x

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
	SCHISM_FLAC_SYM(stream_decoder_get_state);
	SCHISM_FLAC_SYM_ARR(StreamDecoderStateString);
	SCHISM_FLAC_SYM_ARR(StreamDecoderErrorStatusString);

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
	SCHISM_FLAC_SYM(stream_encoder_set_metadata);
	SCHISM_FLAC_SYM(stream_encoder_get_state);
	SCHISM_FLAC_SYM_ARR(StreamEncoderInitStatusString);
	SCHISM_FLAC_SYM_ARR(StreamEncoderStateString);

	SCHISM_FLAC_SYM(metadata_object_new);
	SCHISM_FLAC_SYM(metadata_object_delete);
	SCHISM_FLAC_SYM(metadata_object_application_set_data);
	SCHISM_FLAC_SYM(metadata_object_vorbiscomment_append_comment);

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
	flac_wasinit = 0;
	return 1;
}
