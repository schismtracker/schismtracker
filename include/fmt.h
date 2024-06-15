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

#include <stdint.h>
#include "dmoz.h"
#include "slurp.h"
#include "util.h"

#include "disko.h"

#include "sndfile.h"

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
	SAVE_FILE_ERROR,        /* couldn't write the file; check errno */
	SAVE_INTERNAL_ERROR,    /* something unrelated to disk i/o */
};

/* --------------------------------------------------------------------------------------------------------- */

#define PROTO_READ_INFO         (dmoz_file_t *file, const uint8_t *data, size_t length)
#define PROTO_LOAD_SONG         (song_t *song, slurp_t *fp, unsigned int lflags)
#define PROTO_SAVE_SONG         (disko_t *fp, song_t *song)
#define PROTO_LOAD_SAMPLE       (const uint8_t *data, size_t length, song_sample_t *smp)
#define PROTO_SAVE_SAMPLE       (disko_t *fp, song_sample_t *smp)
#define PROTO_LOAD_INSTRUMENT   (const uint8_t *data, size_t length, int slot)
#define PROTO_EXPORT_HEAD       (disko_t *fp, int bits, int channels, int rate)
#define PROTO_EXPORT_SILENCE    (disko_t *fp, long bytes)
#define PROTO_EXPORT_BODY       (disko_t *fp, const uint8_t *data, size_t length)
#define PROTO_EXPORT_TAIL       (disko_t *fp)

typedef int (*fmt_read_info_func)       PROTO_READ_INFO;
typedef int (*fmt_load_song_func)       PROTO_LOAD_SONG;
typedef int (*fmt_save_song_func)       PROTO_SAVE_SONG;
typedef int (*fmt_load_sample_func)     PROTO_LOAD_SAMPLE;
typedef int (*fmt_save_sample_func)     PROTO_SAVE_SAMPLE;
typedef int (*fmt_load_instrument_func) PROTO_LOAD_INSTRUMENT;
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
		struct {
			fmt_export_head_func head;
			fmt_export_silence_func silence;
			fmt_export_body_func body;
			fmt_export_tail_func tail;
			int multi;
		} export;
	} f;
};

extern const struct save_format song_save_formats[];
extern const struct save_format song_export_formats[];
extern const struct save_format sample_save_formats[];

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

uint32_t it_decompress8(void *dest, uint32_t len, const void *file, uint32_t filelen, int it215, int channels);
uint32_t it_decompress16(void *dest, uint32_t len, const void *file, uint32_t filelen, int it215, int channels);

uint16_t mdl_read_bits(uint32_t *bitbuf, uint32_t *bitnum, uint8_t **ibuf, int8_t n);

/* --------------------------------------------------------------------------------------------------------- */

/* shared by the .it, .its, and .iti saving functions */
void save_its_header(disko_t *fp, song_sample_t *smp);
int load_its_sample(const uint8_t *header, const uint8_t *data, size_t length, song_sample_t *smp);

/* --------------------------------------------------------------------------------------------------------- */
// other misc functions...

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

// get L-R-R-L panning value from a (zero-based!) channel number
#define PROTRACKER_PANNING(n) (((((n) + 1) >> 1) & 1) * 256)

// convert .mod finetune byte value to c5speed
#define MOD_FINETUNE(b) (finetune_table[((b) & 0xf) ^ 8])

/* --------------------------------------------------------------------------------------------------------- */

int win32mf_init(void);
void win32mf_quit(void);

#endif /* SCHISM_FMT_H_ */

