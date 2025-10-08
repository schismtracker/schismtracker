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

#ifndef SCHISM_FMT_H_
#define SCHISM_FMT_H_

#include "dmoz.h"
#include "slurp.h"
#include "util.h"

#include "disko.h"

#include "player/sndfile.h"

/* --------------------------------------------------------------------------------------------------------- */
/* module loaders */

/* flags to skip loading some data (mainly for scraping titles)
this is only a suggestion in order to speed loading; don't be surprised if the loader ignores these */
#define LOAD_NOSAMPLES  1
#define LOAD_NOPATTERNS 2

/* return codes for module loaders */
enum {
	LOAD_SUCCESS,           /* all's well */
	LOAD_UNSUPPORTED,       /* wrong file type for the loader */
	LOAD_FILE_ERROR,        /* couldn't read the file; check errno */
	LOAD_FORMAT_ERROR,      /* it appears to be the correct type, but there's something wrong */
};

/* return codes for modules savers */
enum {
	SAVE_SUCCESS,           /* all's well */
	SAVE_UNSUPPORTED,       /* unsupported samples, or something */
	SAVE_FILE_ERROR,        /* couldn't write the file; check errno */
	SAVE_INTERNAL_ERROR,    /* something unrelated to disk i/o */
	SAVE_NO_FILENAME,       /* the filename is empty... */
};

/* --------------------------------------------------------------------------------------------------------- */

#define PROTO_READ_INFO         (dmoz_file_t *file, slurp_t *fp)
#define PROTO_LOAD_SONG         (song_t *song, slurp_t *fp, uint32_t lflags)
#define PROTO_SAVE_SONG         (disko_t *fp, song_t *song)
#define PROTO_LOAD_SAMPLE       (slurp_t *fp, song_sample_t *smp)
#define PROTO_SAVE_SAMPLE       (disko_t *fp, song_sample_t *smp)
#define PROTO_LOAD_INSTRUMENT   (slurp_t *fp, int slot)
#define PROTO_SAVE_INSTRUMENT   (disko_t *fp, song_t *song, song_instrument_t *ins)
#define PROTO_EXPORT_HEAD       (disko_t *fp, int bits, int channels, uint32_t rate)
#define PROTO_EXPORT_SILENCE    (disko_t *fp, long bytes)
#define PROTO_EXPORT_BODY       (disko_t *fp, const uint8_t *data, size_t length)
#define PROTO_EXPORT_TAIL       (disko_t *fp)

typedef int (*fmt_read_info_func)       PROTO_READ_INFO;
typedef int (*fmt_load_song_func)       PROTO_LOAD_SONG;
typedef int (*fmt_save_song_func)       PROTO_SAVE_SONG;
typedef int (*fmt_load_sample_func)     PROTO_LOAD_SAMPLE;
typedef int (*fmt_save_sample_func)     PROTO_SAVE_SAMPLE;
typedef int (*fmt_load_instrument_func) PROTO_LOAD_INSTRUMENT;
typedef int (*fmt_save_instrument_func) PROTO_SAVE_INSTRUMENT;
typedef int (*fmt_export_head_func)     PROTO_EXPORT_HEAD;
typedef int (*fmt_export_silence_func)  PROTO_EXPORT_SILENCE;
typedef int (*fmt_export_body_func)     PROTO_EXPORT_BODY;
typedef int (*fmt_export_tail_func)     PROTO_EXPORT_TAIL;

#define READ_INFO(t)            int fmt_##t##_read_info         PROTO_READ_INFO;
#define LOAD_SONG(t)            int fmt_##t##_load_song         PROTO_LOAD_SONG;
#define SAVE_SONG(t)            int fmt_##t##_save_song         PROTO_SAVE_SONG;
#define LOAD_SAMPLE(t)          int fmt_##t##_load_sample       PROTO_LOAD_SAMPLE;
#define SAVE_SAMPLE(t)          int fmt_##t##_save_sample       PROTO_SAVE_SAMPLE;
#define LOAD_INSTRUMENT(t)      int fmt_##t##_load_instrument   PROTO_LOAD_INSTRUMENT;
#define SAVE_INSTRUMENT(t)		int fmt_##t##_save_instrument	PROTO_SAVE_INSTRUMENT;
#define EXPORT(t)               int fmt_##t##_export_head       PROTO_EXPORT_HEAD; \
				int fmt_##t##_export_silence    PROTO_EXPORT_SILENCE; \
				int fmt_##t##_export_body       PROTO_EXPORT_BODY; \
				int fmt_##t##_export_tail       PROTO_EXPORT_TAIL;

#include "fmt-types.h"

/* --------------------------------------------------------------------------------------------------------- */

struct save_format {
	const char *label; // label for the button on the save page
	const char *name; // long name of format
	const char *ext; // no dot
	union {
		fmt_save_song_func save_song;
		fmt_save_sample_func save_sample;
		fmt_save_instrument_func save_instrument;
		struct {
			fmt_export_head_func head;
			fmt_export_silence_func silence;
			fmt_export_body_func body;
			fmt_export_tail_func tail;
			int multi;
		} export;
	} f;

	// TODO need a way to filter out sample formats that cannot
	// be saved in a specific format (i.e. pcm in sbi or adlib in wav)

	// for files that can only be loaded with an external library
	// that is loaded at runtime (or linked to)
	int (*enabled)(void);
};

extern const struct save_format song_save_formats[];
extern const struct save_format song_export_formats[];
extern const struct save_format sample_save_formats[];
extern const struct save_format instrument_save_formats[];

/* --------------------------------------------------------------------------------------------------------- */
struct instrumentloader {
	song_instrument_t *inst;
	int sample_map[MAX_SAMPLES];
	int basex, slot, expect_samples;
};
song_instrument_t *instrument_loader_init(struct instrumentloader *ii, int slot);
int instrument_loader_abort(struct instrumentloader *ii);
int instrument_loader_sample(struct instrumentloader *ii, int slot);

/* --------------------------------------------------------------------------------------------------------- */

uint32_t it_decompress8(void *dest, uint32_t len, slurp_t *fp, int it215, int channels);
uint32_t it_decompress16(void *dest, uint32_t len, slurp_t *fp, int it215, int channels);

uint32_t mdl_decompress8(void *dest, uint32_t len, slurp_t *fp);
uint32_t mdl_decompress16(void *dest, uint32_t len, slurp_t *fp);

/* returns 0 on success */
int32_t huffman_decompress(slurp_t *slurp, disko_t *disko);

/* --------------------------------------------------------------------------------------------------------- */

/* shared by the .it, .its, and .iti saving functions */
void save_its_header(disko_t *fp, song_sample_t *smp);
void save_iti_instrument(disko_t *fp, song_t *song, song_instrument_t *ins, int iti_file);
int load_its_sample(slurp_t *fp, song_sample_t *smp, uint16_t cwtv);
int load_it_instrument(struct instrumentloader* ii, song_instrument_t *instrument, slurp_t *fp);
int load_it_instrument_old(song_instrument_t *instrument, slurp_t *fp);
uint32_t it_decode_edit_timer(uint16_t cwtv, uint32_t runtime);
uint32_t it_get_song_elapsed_dos_time(song_t *song);

/* shared by .IT and .XM */
int it_read_midi_config(midi_config_t *midi, slurp_t *fp);

/* s3i, called from s3m saver */
void s3i_write_header(disko_t *fp, song_sample_t *smp, uint32_t sdata);

/* --------------------------------------------------------------------------------------------------------- */

/* handle dos timestamps */
timer_ticks_t dos_time_to_ms(uint32_t dos_time);
uint32_t ms_to_dos_time(timer_ticks_t ms);

void fat_date_time_to_tm(struct tm *tm, uint16_t fat_date, uint16_t fat_time);
void tm_to_fat_date_time(const struct tm *tm, uint16_t *fat_date, uint16_t *fat_time);

/* --------------------------------------------------------------------------------------------------------- */

/* [R]IFF helper functions */

typedef struct iff_chunk {
	uint32_t id; /* the ID, as a big endian integer. e.g. "wave" == 0x77617665 */
	uint32_t size;
	int64_t offset; /* where in the file the data actually starts */
} iff_chunk_t;

/* flags to iff_chunk_peek_ex */
enum {
	IFF_CHUNK_SIZE_LE = (1 << 0), /* for RIFF */
	IFF_CHUNK_ALIGNED = (1 << 1), /* are the structures word aligned? */
};

int iff_chunk_peek_ex(iff_chunk_t *chunk, slurp_t *fp, uint32_t flags);

/* provided for convenience, and backwards compatibility. */
#define iff_chunk_peek(chunk, fp) iff_chunk_peek_ex(chunk, fp, IFF_CHUNK_ALIGNED)
#define riff_chunk_peek(chunk, fp) iff_chunk_peek_ex(chunk, fp, IFF_CHUNK_ALIGNED | IFF_CHUNK_SIZE_LE)

int iff_chunk_read(iff_chunk_t *chunk, slurp_t *fp, void *data, size_t size);
int iff_read_sample(iff_chunk_t *chunk, slurp_t *fp, song_sample_t *smp, uint32_t flags, size_t offset);
int iff_chunk_receive(iff_chunk_t *chunk, slurp_t *fp, int (*callback)(const void *, size_t, void *), void *userdata);

/* functions to deal with IFF chunks containing sample info */

#define IFF_XTRA_CHUNK_SIZE 24 /* 8 + 16 */
#define IFF_SMPL_CHUNK_SIZE 92 /* 8 + 36 + (2 * 24) */

void iff_fill_xtra_chunk(song_sample_t *smp, unsigned char xtra_data[IFF_XTRA_CHUNK_SIZE], uint32_t *length);
void iff_fill_smpl_chunk(song_sample_t *smp, unsigned char smpl_data[IFF_SMPL_CHUNK_SIZE], uint32_t *length);

int iff_read_xtra_chunk(slurp_t *fp, song_sample_t *smp);
int iff_read_smpl_chunk(slurp_t *fp, song_sample_t *smp);

/* --------------------------------------------------------------------------------------------------------- */
/* .wav crap. Shared between Wave64 and regular .wav loaders */

struct wave_format {
	uint16_t format;          // 1
	uint16_t channels;        // 1:mono, 2:stereo
	uint32_t freqHz;          // sampling freq
	uint32_t bytessec;        // bytes/sec=freqHz*samplesize
	uint16_t samplesize;      // sizeof(sample)
	uint16_t bitspersample;   // bits per sample (8/16)
};

#define WAVE_FORMAT_PCM        UINT16_C(0x0001)
#define WAVE_FORMAT_IEEE_FLOAT UINT16_C(0x0003) // IEEE float
#define WAVE_FORMAT_ALAW       UINT16_C(0x0006) // 8-bit ITU-T G.711 A-law
#define WAVE_FORMAT_MULAW      UINT16_C(0x0007) // 8-bit ITU-T G.711 µ-law
#define WAVE_FORMAT_EXTENSIBLE UINT16_C(0xFFFE)

int wav_chunk_fmt_read(const void *data, size_t size, void *void_fmt /* struct wave_format * */);

/* --------------------------------------------------------------------------------------------------------- */
// other misc functions...

/* fills player quirks from a schism version
 * we could do this for old modplug/openmpt versions too */
void fmt_fill_schism_quirks(song_t *csf, uint32_t ver);

/* fills a dmoz_file_t from a csndfile sample */
void fmt_fill_file_from_sample(dmoz_file_t *file, const song_sample_t *smp);

/* writes raw pcm to `fp`
 * bpf: bytes per frame
 * bps: bytes per sample
 * swap: nonzero to byteswap */
int fmt_write_pcm(disko_t *fp, const uint8_t *data, size_t length, int bpf,
	int bps, int swap, const char *name);

/* effect_weight[FX_something] => how "important" the effect is. */
extern const uint8_t effect_weight[];

/* Shuffle the effect and volume-effect values around.
Note: this does NOT convert between volume and 'normal' effects, it only exchanges them.
(This function is most useful in conjunction with convert_voleffect in order to try to
cram ten pounds of crap into a five pound container) */
void swap_effects(song_note_t *note);

/* Convert volume column data from FX_* to VOLFX_*, if possible.
Return: 1 = it was properly converted, 0 = couldn't do so without loss of information. */
int convert_voleffect(uint8_t *effect, uint8_t *param, int force);
#define convert_voleffect_of(note,force) convert_voleffect(&((note)->voleffect), &((note)->volparam), (force))

// load a .mod-style 4-byte packed note
void mod_import_note(const uint8_t p[4], song_note_t *note);

// Read a message with fixed-size line lengths
void read_lined_message(char *msg, slurp_t *fp, int len, int linelen);

// STM specific tools
uint8_t convert_stm_tempo_to_bpm(size_t tempo);
void handle_stm_tempo_pattern(song_note_t *note, size_t tempo);
void handle_stm_effects(song_note_t *chan_note);
extern const uint8_t stm_effects[16];

/* used internally by slurp only. nothing else should need this */
int mmcmp_unpack(slurp_t *fp, uint8_t **data, size_t *length);

// get L-R-R-L panning value from a (zero-based!) channel number
#define PROTRACKER_PANNING(n) (((((n) + 1) >> 1) & 1) * 256)

// convert .mod finetune byte value to c5speed
#define MOD_FINETUNE(b) (finetune_table[((b) & 0xf) ^ 8])

/* --------------------------------------------------------------------------------------------------------- */

int win32mf_init(void);
void win32mf_quit(void);

int flac_init(void);
int flac_quit(void);
void audio_enable_flac(int enabled); // should be called by flac_init()

int avformat_init(void);
void avformat_quit(void);

int gzip_init(void);
void gzip_quit(void);

int bzip2_init(void);
void bzip2_quit(void);

#endif /* SCHISM_FMT_H_ */

