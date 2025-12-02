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

/* --------------------------------------------------------------------- */

#include "headers.h"
#include "bits.h"
#include "ieee-float.h"
#include "log.h"
#include "fmt.h"
#include "mem.h"

/* --------------------------------------------------------------------- */

struct aiff_chunk_vhdr {
	uint32_t smp_highoct_1shot;
	uint32_t smp_highoct_repeat;
	uint32_t smp_cycle_highoct;
	uint16_t smp_per_sec;
	uint8_t num_octaves;
	uint8_t compression; // 0 = none, 1 = fibonacci-delta
	uint32_t volume; // fixed point, 65536 = 1.0
};

struct aiff_chunk_comm {
	uint16_t num_channels;
	uint32_t num_frames;
	uint16_t sample_size;
	unsigned char sample_rate[10]; // IEEE-extended
};

static int aiff_chunk_vhdr_read(const void *data, size_t size, void *void_vhdr)
{
	struct aiff_chunk_vhdr *vhdr = (struct aiff_chunk_vhdr *)void_vhdr;

	slurp_t fp;
	slurp_memstream(&fp, (uint8_t *)data, size);

#define READ_VALUE(name) \
	do { if (slurp_read(&fp, &vhdr->name, sizeof(vhdr->name)) != sizeof(vhdr->name)) { unslurp(&fp); return 0; } } while (0)

	READ_VALUE(smp_highoct_1shot);
	READ_VALUE(smp_highoct_repeat);
	READ_VALUE(smp_cycle_highoct);
	READ_VALUE(smp_per_sec);
	READ_VALUE(num_octaves);
	READ_VALUE(compression);
	READ_VALUE(volume);

#undef READ_VALUE

	unslurp(&fp);

	return 1;
}

static int aiff_chunk_comm_read(const void *data, size_t size, void *void_comm)
{
	struct aiff_chunk_comm *comm = (struct aiff_chunk_comm *)void_comm;

	slurp_t fp;
	slurp_memstream(&fp, (uint8_t *)data, size);

#define READ_VALUE(name) \
	do { if (slurp_read(&fp, &comm->name, sizeof(comm->name)) != sizeof(comm->name)) { unslurp(&fp); return 0; } } while (0)

	READ_VALUE(num_channels);
	READ_VALUE(num_frames);
	READ_VALUE(sample_size);
	READ_VALUE(sample_rate);

#undef READ_VALUE

	unslurp(&fp);

	return 1;
}

// other chunks that might exist: "NAME", "AUTH", "ANNO", "(c) "

// Wish I could do this:
//#define ID(x) ((0[#x] << 24) | (1[#x] << 16) | (2[#x] << 8) | (3[#x]))
// It works great, but gcc doesn't think it's a constant, so it can't be used in a switch.
// (although with -O, it definitely does optimize it into a constant...)

#define ID_FORM UINT32_C(0x464F524D)
#define ID_8SVX UINT32_C(0x38535658)
#define ID_16SV UINT32_C(0x31365356)
#define ID_VHDR UINT32_C(0x56484452)
#define ID_BODY UINT32_C(0x424F4459)
#define ID_NAME UINT32_C(0x4E414D45)
#define ID_AUTH UINT32_C(0x41555448)
#define ID_ANNO UINT32_C(0x414E4E4F)
#define ID__C__ UINT32_C(0x28632920) /* "(c) " */
#define ID_AIFF UINT32_C(0x41494646)
#define ID_COMM UINT32_C(0x434F4D4D)
#define ID_SSND UINT32_C(0x53534E44)
#define ID_MARK UINT32_C(0x4D41524B)
#define ID_INST UINT32_C(0x494E5354)
#define ID_APPL UINT32_C(0x4150504C)
#define ID_Schm UINT32_C(0x5363686D)

#define IFFID_xtra UINT32_C(0x78747261)

/* --------------------------------------------------------------------- */

static int read_iff_(dmoz_file_t *file, song_sample_t *smp, slurp_t *fp)
{
	uint32_t filetype = 0;
	iff_chunk_t chunk = {0};
	iff_chunk_t vhdr = {0}, body = {0}, name = {0}, auth = {0}, anno = {0},
	            ssnd = {0}, comm = {0}, mark = {0}, inst = {0}, applschm = {0};

	if (!iff_chunk_peek(&chunk, fp))
		return 0;

	if (chunk.id != ID_FORM)
		return 0;

	if (iff_chunk_read(&chunk, fp, &filetype, sizeof(filetype)) != sizeof(filetype))
		return 0;

	// jump "into" the FORM chunk
	slurp_seek(fp, chunk.offset + sizeof(filetype), SEEK_SET);

	filetype = bswapBE32(filetype);

	switch (filetype) {
	case ID_16SV: /* 16-bit */
	case ID_8SVX: {
		struct aiff_chunk_vhdr chunk_vhdr;

		while (iff_chunk_peek(&chunk, fp)) {
			switch (chunk.id) {
				case ID_VHDR: vhdr = chunk; break;
				case ID_BODY: body = chunk; break;
				case ID_NAME: name = chunk; break;
				case ID_AUTH: auth = chunk; break;
				case ID_ANNO: anno = chunk; break;
				default: break;
			}
		}
		if (!(vhdr.id && body.id))
			return 0;

		if (!iff_chunk_receive(&vhdr, fp, aiff_chunk_vhdr_read, &chunk_vhdr))
			return 0;

		if (chunk_vhdr.compression) {
			log_appendf(4, "error: compressed IFF files are unsupported");
			return 0;
		}
		if (chunk_vhdr.num_octaves != 1) {
			log_appendf(4, "warning: IFF file contains %d octaves",
				chunk_vhdr.num_octaves);
		}

		if (file) {
			file->smp_speed = bswapBE16(chunk_vhdr.smp_per_sec);
			file->smp_length = body.size;

			file->description = (filetype == ID_16SV)
				? "16SV sample"
				: "8SVX sample";
			file->type = TYPE_SAMPLE_PLAIN;
		}
		if (!name.id) name = auth;
		if (!name.id) name = anno;
		if (name.id) {
			if (file) {
				file->title = mem_alloc(name.size + 1);
				iff_chunk_read(&name, fp, file->title, name.size);
				file->title[name.size] = '\0';
			}
			if (smp) {
				iff_chunk_read(&name, fp, smp->name, sizeof(smp->name));
			}
		}

		if (smp) {
			uint32_t flags = SF_BE | SF_PCMS | SF_M;

			smp->c5speed = bswapBE16(chunk_vhdr.smp_per_sec);
			smp->length = body.size;

			/* volume (should this be global volume, or just smp volume?) */
			chunk_vhdr.volume = bswapBE32(chunk_vhdr.volume);
			chunk_vhdr.volume = MIN(chunk_vhdr.volume, 0x10000);

			/* round to 0..64 -- 1024 == (0x10000 / 64) */
			chunk_vhdr.volume = (chunk_vhdr.volume + 512) / 1024;

			smp->volume = chunk_vhdr.volume * 4; //mphack

			// this is done kinda weird
			smp->loop_end = bswapBE32(chunk_vhdr.smp_highoct_repeat);
			if (smp->loop_end) {
				smp->loop_start = bswapBE32(chunk_vhdr.smp_highoct_1shot);
				smp->loop_end += smp->loop_start;
				if (smp->loop_start > smp->length)
					smp->loop_start = 0;
				if (smp->loop_end > smp->length)
					smp->loop_end = smp->length;
				if (smp->loop_start + 2 < smp->loop_end)
					smp->flags |= CHN_LOOP;
			}

			if (filetype == ID_16SV) {
				flags |= SF_16;

				smp->length >>= 1;
				smp->loop_start >>= 1;
				smp->loop_end >>= 1;
			} else {
				flags |= SF_8;
			}

			iff_read_sample(&body, fp, smp, flags, 0);
		}

		return 1;
	}
	case ID_AIFF: {
		struct aiff_chunk_comm chunk_comm;
		/* this is an ugly hack for getting sample loops etc in the sample library */
		song_sample_t tmpsmp = {0};
		song_sample_t *psmp = (smp) ? smp : &tmpsmp;

		while (iff_chunk_peek(&chunk, fp)) {
			switch (chunk.id) {
				case ID_COMM: comm = chunk; break;
				case ID_SSND: ssnd = chunk; break;
				case ID_NAME: name = chunk; break;
				case ID_MARK: mark = chunk; break;
				case ID_INST: inst = chunk; break;
				case ID_APPL: {
					int64_t pos = slurp_tell(fp);
					uint32_t id;

					/* figure out which type it is.
					 * if it's from us, we want to read it in */
					slurp_seek(fp, chunk.offset, SEEK_SET);

					chunk.offset += 4;
					chunk.size -= 4;

					slurp_read(fp, &id, sizeof(id));
					id = bswapBE32(id);

					switch (id) {
					case ID_Schm:
						applschm = chunk;
						break;
					}

					/* return back to the original pos */
					slurp_seek(fp, pos, SEEK_SET);

					break;
				}
				default: break;
			}
		}
		if (!(comm.id && ssnd.id))
			return 0;

		if (!iff_chunk_receive(&comm, fp, aiff_chunk_comm_read, &chunk_comm))
			return 0;

		psmp->c5speed = float_decode_ieee_80(chunk_comm.sample_rate);
		psmp->length = bswapBE32(chunk_comm.num_frames);

		if (name.id) {
			if (file) {
				file->title = mem_alloc(name.size + 1);
				iff_chunk_read(&name, fp, file->title, name.size);
				file->title[name.size] = '\0';
			}
			if (smp) {
				iff_chunk_read(&name, fp, smp->name, sizeof(smp->name));
			}
		}

		if (inst.id && mark.id) {
			/* wow this sucks */
			uint16_t i, num_markers;
			uint16_t loop_type, sustain_type;
			uint16_t loop_start_id, loop_end_id, sustain_start_id, sustain_end_id;

			/* seek to the stuff we actually care about (loop points) */
			slurp_seek(fp, inst.offset + 8, SEEK_SET);

			slurp_read(fp, &sustain_type, 2);
			sustain_type = bswapBE16(sustain_type);

			slurp_read(fp, &sustain_start_id, 2);
			sustain_start_id = bswapBE16(sustain_start_id);

			slurp_read(fp, &sustain_end_id, 2);
			sustain_end_id = bswapBE16(sustain_end_id);

			slurp_read(fp, &loop_type, 2);
			loop_type = bswapBE16(loop_type);

			slurp_read(fp, &loop_start_id, 2);
			loop_start_id = bswapBE16(loop_start_id);

			slurp_read(fp, &loop_end_id, 2);
			loop_end_id = bswapBE16(loop_end_id);

			slurp_seek(fp, mark.offset, SEEK_SET);

			slurp_read(fp, &num_markers, 2);
			num_markers = bswapBE16(num_markers);

			for (i = 0; i < num_markers; i++) {
				int c;
				uint16_t marker_id;
				uint32_t offset;

				slurp_read(fp, &marker_id, 2);
				marker_id = bswapBE16(marker_id);

				slurp_read(fp, &offset, 4);
				offset = bswapBE32(offset);

				/* loops can share markers! */
				if (marker_id == loop_start_id)
					psmp->loop_start = offset;
				if (marker_id == loop_end_id)
					psmp->loop_end = offset;
				if (marker_id == sustain_start_id)
					psmp->sustain_start = offset;
				if (marker_id == sustain_end_id)
					psmp->sustain_end = offset;

				/* skip name */
				c = slurp_getc(fp);
				if (c == EOF)
					break; /* what */

				slurp_seek(fp, c, SEEK_CUR);
			}

			/* okay, now we have everything; now find the markers */
			switch (loop_type) {
			case 2: psmp->flags |= CHN_PINGPONGLOOP; SCHISM_FALLTHROUGH;
			case 1: psmp->flags |= CHN_LOOP;
			}

			switch (sustain_type) {
			case 2: psmp->flags |= CHN_PINGPONGSUSTAIN; SCHISM_FALLTHROUGH;
			case 1: psmp->flags |= CHN_SUSTAINLOOP;
			}
		}

		if (applschm.id) {
			iff_chunk_t xtra = {0};

			slurp_seek(fp, applschm.offset, SEEK_SET);

			while (iff_chunk_peek(&chunk, fp)) {
				switch (chunk.id) {
				case IFFID_xtra:
					xtra = chunk;
					break;
				}

				if (xtra.id)
					break; /* we have everything we want */
			}

			if (xtra.id) {
				slurp_seek(fp, xtra.offset, SEEK_SET);

				iff_read_xtra_chunk(fp, psmp);
			}
		}

		if (file) {
			fmt_fill_file_from_sample(file, psmp);

			file->description = "Audio IFF sample";
			file->type = TYPE_SAMPLE_PLAIN;

			switch ((bswapBE32(chunk_comm.sample_size) + 7) & ~7) {
			case 16: case 24: case 32:
				file->smp_flags |= CHN_16BIT;
			}
		}

		if (smp) {
			uint32_t flags = SF_BE | SF_PCMS;

			switch (bswapBE16(chunk_comm.num_channels)) {
			default:
				log_appendf(4, "warning: multichannel AIFF is unsupported");
				SCHISM_FALLTHROUGH;
			case 1:
				flags |= SF_M;
				break;
			case 2:
				flags |= SF_SI;
				break;
			}

			switch ((bswapBE16(chunk_comm.sample_size) + 7) & ~7) {
			default:
				log_appendf(4, "warning: AIFF has unsupported bit-width");
				SCHISM_FALLTHROUGH;
			case 8:
				flags |= SF_8;
				break;
			case 16:
				flags |= SF_16;
				break;
			case 24:
				flags |= SF_24;
				break;
			case 32:
				flags |= SF_32;
				break;
			}

			// TODO: data checking; make sure sample count and byte size agree
			// (and if not, cut to shorter of the two)

			// the audio data starts 8 bytes into the chunk
			// (don't care about the block alignment stuff)
			iff_read_sample(&ssnd, fp, smp, flags, 8);
		}

		return 1;
	}
	default:
		break;
	}

	return 0;
}

/* --------------------------------------------------------------------- */

int fmt_aiff_read_info(dmoz_file_t *file, slurp_t *fp)
{
	return read_iff_(file, NULL, fp);
}

int fmt_aiff_load_sample(slurp_t *fp, song_sample_t *smp)
{
	return read_iff_(NULL, smp, fp);
}

/* --------------------------------------------------------------------- */

struct aiff_writedata {
	long comm_frames, ssnd_size; // seek positions for writing header data
	size_t numbytes; // how many bytes have been written
	int bps; // bytes per sample
	int swap; // should be byteswapped?
	int bpf; // bytes per frame
};

static int aiff_header(disko_t *fp, int bits, int channels, uint32_t rate,
	const char *name, size_t length, song_sample_t *smp, struct aiff_writedata *awd /* out */)
{
	int16_t s;
	uint32_t ul;
	int tlen, bps = 1;
	uint8_t b[10];
	int bpf; /* bytes per frame */

	bps *= ((bits + 7) / 8);
	/* note: channel multiply is done below -- need single-channel value for the COMM chunk */

	/* write a very large size for now */
	disko_write(fp, "FORM\377\377\377\377AIFF", 12);

	if (name && *name) {
		disko_write(fp, "NAME", 4);
		tlen = strlen(name);
		ul = (tlen + 1) & ~1; /* must be even */
		ul = bswapBE32(ul);
		disko_write(fp, &ul, 4);
		disko_write(fp, name, tlen);
		if (tlen & 1)
			disko_putc(fp, '\0');
	}

	if (smp) {
		/* Marker Chunk

		typedef short MarkerId;

		typedef struct {
			MarkerId id;
			unsigned long position;
			pstring markerName;
		} Marker;

		typedef struct {
			ID ckID;
			long ckSize;
			unsigned short numMarkers;
			Marker Markers[];
		} MarkerChunk; */
		uint32_t ckSize, dw;
		uint16_t mid, w;

		disko_write(fp, "MARK", 4);

		ckSize = 2 + 14 + 14;

		dw = bswapBE32(ckSize);
		disko_write(fp, &dw, 4);

		w = bswapBE16(4);
		disko_write(fp, &w, 2);

		enum {
			MID_LOOP_START = 1,
			MID_LOOP_END = 2,
			MID_SUSTAIN_START = 3,
			MID_SUSTAIN_END = 4,
		};

		/* now start writing the markers. */
#define WRITE_MARKER(id, x) \
do { \
	w = bswapBE16(id); \
	disko_write(fp, &w, 2); \
\
	dw = bswapBE32(x); \
	disko_write(fp, &dw, 4); \
\
	/* no name... */ \
	disko_write(fp, "\0", 1); \
} while (0)

		WRITE_MARKER(MID_LOOP_START,    smp->loop_start);
		WRITE_MARKER(MID_LOOP_END,      smp->loop_end);
		WRITE_MARKER(MID_SUSTAIN_START, smp->sustain_start);
		WRITE_MARKER(MID_SUSTAIN_END,   smp->sustain_end);

#undef WRITE_MARKER

		/* Instrument Chunk
			The Instrument Chunk defines basic parameters that an instrument, such as a sampler,
			could use to play back the sound data.

		typedef struct {
		#define NoLooping 0
		#define ForwardLooping 1
		#define ForwardBackwardLooping 2
			short playMode;
			MarkerId beginLoop;
			MarkerId endLoop;
		} Loop; // size: 6

		typedef struct {
			ID ckID;         // 'INST'
			long ckSize;
			char baseNote;
			char detune;
			char lowNote;
			char highNote;
			char lowVelocity;
			char highVelocity;
			short gain;
			Loop sustainLoop;
			Loop releaseLoop;
		} InstrumentChunk; // size: 20 */

		disko_write(fp, "INST", 4);

		dw = bswapBE32(20);
		disko_write(fp, &dw, sizeof(dw));

		/* base note (60 == C-5 ?) */
		disko_putc(fp, 60);

		/* detune in cents [-50, 50] (TODO) */
		disko_putc(fp, 0);

		disko_putc(fp, 0); /* lowNote */
		disko_putc(fp, 127); /* highNote */

		disko_putc(fp, 1); /* lowVelocity */
		disko_putc(fp, 127); /* highVelocity */

		/* gain (0 db) -- could possibly write volume here? I don't know */
		disko_write(fp, "\0\0", 2);

		/* finally, to the loops */

#define WRITE_LOOP(LOOPFLAG, PINGPONGFLAG, MIDSTART, MIDEND) \
do { \
	w = bswapBE16(!!(smp->flags & (LOOPFLAG)) + !!(smp->flags & (PINGPONGFLAG))); \
	disko_write(fp, &w, 2); \
\
	w = bswapBE16(MIDSTART); \
	disko_write(fp, &w, 2); \
\
	w = bswapBE16(MIDEND); \
	disko_write(fp, &w, 2); \
} while (0)

		WRITE_LOOP(CHN_SUSTAINLOOP, CHN_PINGPONGSUSTAIN, MID_SUSTAIN_START, MID_SUSTAIN_END);
		WRITE_LOOP(CHN_LOOP, CHN_PINGPONGLOOP, MID_LOOP_START, MID_LOOP_END);

#undef WRITE_LOOP
	}

	/* Apple II style information chunk.
	 * This is the only standard way I can find to store the encoder name... */
	{
		static const char name[] = "Schism Tracker " VERSION;
		uint32_t dw;

		disko_write(fp, "APPL", 4);

		dw = bswapBE32(4 + sizeof(name));
		disko_write(fp, &dw, sizeof(dw));

		disko_write(fp, "pdos", 4);

		/* write pascal string -- name of the application */
		disko_putc(fp, sizeof(name) - 1);
		disko_write(fp, name, sizeof(name) - 1);
	}

	/* Our information chunk -- stores Modplug-style 'xtra' data */
	if (smp) {
		unsigned char xtra[IFF_XTRA_CHUNK_SIZE];
		uint32_t dw, length;

		iff_fill_xtra_chunk(smp, xtra, &length);

		disko_write(fp, "APPL", 4);

		dw = bswapBE32(4 + length);
		disko_write(fp, &dw, 4);

		/* Reusing the Mac OS X bundle identifier here */
		disko_write(fp, "Schm", 4);

		/* This is stored WITH the 'xtra' ID and size */
		disko_write(fp, xtra, length);
	}

	/* Common Chunk
		The Common Chunk describes fundamental parameters of the sampled sound.
	typedef struct {
		ID              ckID;           // 'COMM'
		long            ckSize;         // 18
		short           numChannels;
		unsigned long   numSampleFrames;
		short           sampleSize;
		extended        sampleRate;
	} CommonChunk; */
	disko_write(fp, "COMM", 4);
	ul = bswapBE32(18); /* chunk size -- won't change */
	disko_write(fp, &ul, 4);
	s = bswapBE16(channels);
	disko_write(fp, &s, 2);
	if (awd)
		awd->comm_frames = disko_tell(fp);
	ul = bswapBE32(length); /* num sample frames */
	disko_write(fp, &ul, 4);
	s = bswapBE16(bits);
	disko_write(fp, &s, 2);
	float_encode_ieee_80(rate, b);
	disko_write(fp, b, 10);

	/* NOW do this (sample size in AIFF is indicated per channel, not per frame) */
	bpf = bps * channels; /* == number of bytes per (stereo) sample */

	/* Sound Data Chunk
		The Sound Data Chunk contains the actual sample frames.
	typedef struct {
		ID              ckID;           // 'SSND'
		long            ckSize;         // data size in bytes, *PLUS EIGHT* (for offset and blockSize)
		unsigned long   offset;         // just set this to 0...
		unsigned long   blockSize;      // likewise
		unsigned char   soundData[];
	} SoundDataChunk; */
	disko_write(fp, "SSND", 4);
	if (awd) {
		awd->ssnd_size = disko_tell(fp);
		awd->bps = bps;
		awd->bpf = bpf;
	}
	ul = bswapBE32(length * bpf + 8);
	disko_write(fp, &ul, 4);
	ul = bswapBE32(0);
	disko_write(fp, &ul, 4);
	disko_write(fp, &ul, 4);

	return bpf;
}

/* --------------------------------------------------------------------- */

int fmt_aiff_save_sample(disko_t *fp, song_sample_t *smp)
{
	if (smp->flags & CHN_ADLIB)
		return SAVE_UNSUPPORTED;

	int bpf;
	uint32_t ul;
	uint32_t flags = SF_BE | SF_PCMS;
	flags |= (smp->flags & CHN_16BIT) ? SF_16 : SF_8;
	flags |= (smp->flags & CHN_STEREO) ? SF_SI : SF_M;

	bpf = aiff_header(fp, (smp->flags & CHN_16BIT) ? 16 : 8, (smp->flags & CHN_STEREO) ? 2 : 1,
		smp->c5speed, smp->name, smp->length, smp, NULL);

	if (csf_write_sample(fp, smp, flags, UINT32_MAX) != smp->length * bpf) {
		log_appendf(4, "AIFF: unexpected data size written");
		return SAVE_INTERNAL_ERROR;
	}

	/* fix the length in the file header */
	ul = disko_tell(fp) - 8;
	ul = bswapBE32(ul);
	disko_seek(fp, 4, SEEK_SET);
	disko_write(fp, &ul, 4);

	return SAVE_SUCCESS;
}


int fmt_aiff_export_head(disko_t *fp, int bits, int channels, uint32_t rate, const char *title)
{
	struct aiff_writedata *awd = malloc(sizeof(struct aiff_writedata));
	if (!awd)
		return DW_ERROR;
	fp->userdata = awd;
	aiff_header(fp, bits, channels, rate, title, ~0, NULL, awd);
	awd->numbytes = 0;
#if WORDS_BIGENDIAN
	awd->swap = 0;
#else
	awd->swap = (bits > 8);
#endif

	return DW_OK;
}

int fmt_aiff_export_body(disko_t *fp, const uint8_t *data, size_t length)
{
	struct aiff_writedata *awd = fp->userdata;

	if (fmt_write_pcm(fp, data, length, awd->bpf, awd->bps,
			awd->swap, "AIFF") < 0)
		return DW_ERROR;

	awd->numbytes += length;

	return DW_OK;
}

int fmt_aiff_export_silence(disko_t *fp, long bytes)
{
	struct aiff_writedata *awd = fp->userdata;
	awd->numbytes += bytes;

	disko_seek(fp, bytes, SEEK_CUR);
	return DW_OK;
}

int fmt_aiff_export_tail(disko_t *fp)
{
	struct aiff_writedata *awd = fp->userdata;
	uint32_t ul;

	/* fix the length in the file header */
	ul = disko_tell(fp) - 8;
	ul = bswapBE32(ul);
	disko_seek(fp, 4, SEEK_SET);
	disko_write(fp, &ul, 4);

	/* write the other lengths */
	disko_seek(fp, awd->comm_frames, SEEK_SET);
	ul = bswapBE32(awd->numbytes / awd->bpf);
	disko_write(fp, &ul, 4);
	disko_seek(fp, awd->ssnd_size, SEEK_SET);
	ul = bswapBE32(awd->numbytes + 8);
	disko_write(fp, &ul, 4);

	free(awd);

	return DW_OK;
}
