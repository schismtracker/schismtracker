/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010 Storlek
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

#ifndef FMT_H
#define FMT_H

#include <stdint.h>
#include "song.h"
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

typedef int (*fmt_read_info_func) (dmoz_file_t *file, const uint8_t *data, size_t length);
typedef int (*fmt_load_song_func) (song_t *song, slurp_t *fp, unsigned int lflags);
typedef int (*fmt_save_song_func) (disko_t *fp, song_t *song);
typedef int (*fmt_load_sample_func) (const uint8_t *data, size_t length, song_sample_t *smp);
typedef int (*fmt_save_sample_func) (disko_t *fp, song_sample_t *smp);
typedef int (*fmt_load_instrument_func) (const uint8_t *data, size_t length, int slot);

#define READ_INFO(t) int fmt_##t##_read_info(dmoz_file_t *file, const uint8_t *data, size_t length);
#define LOAD_SONG(t) int fmt_##t##_load_song(song_t *song, slurp_t *fp, unsigned int lflags);
#define SAVE_SONG(t) void fmt_##t##_save_song(disko_t *fp, song_t *song);
#define LOAD_SAMPLE(t) int fmt_##t##_load_sample(const uint8_t *data, size_t length, song_sample_t *smp);
#define SAVE_SAMPLE(t) int fmt_##t##_save_sample(disko_t *fp, song_sample_t *smp);
#define LOAD_INSTRUMENT(t) int fmt_##t##_load_instrument(const uint8_t *data, size_t length, int slot);

#include "fmt-types.h"

#undef READ_INFO
#undef LOAD_SONG
#undef SAVE_SONG
#undef LOAD_SAMPLE
#undef SAVE_SAMPLE
#undef LOAD_INSTRUMENT

/* --------------------------------------------------------------------------------------------------------- */
struct instrumentloader {
        song_instrument_t *inst;
        int sample_map[SCHISM_MAX_SAMPLES];
        int basex, slot, expect_samples;
};
song_instrument_t *instrument_loader_init(struct instrumentloader *ii, int slot);
int instrument_loader_abort(struct instrumentloader *ii);
int instrument_loader_sample(struct instrumentloader *ii, int slot);

/* --------------------------------------------------------------------------------------------------------- */

void it_decompress8(void *dest, uint32_t len, const void *file, uint32_t filelen, int it215);
void it_decompress16(void *dest, uint32_t len, const void *file, uint32_t filelen, int it215);

uint16_t mdl_read_bits(uint32_t *bitbuf, uint32_t *bitnum, uint8_t **ibuf, int8_t n);

/* --------------------------------------------------------------------------------------------------------- */

/* save the sample's data in little- or big- endian byte order (defined in audio_loadsave.cc)

noe == no interleave (IT214)

should probably return something, but... meh :P */
void save_sample_data_LE(disko_t *fp, song_sample_t *smp, int noe);
void save_sample_data_BE(disko_t *fp, song_sample_t *smp, int noe);

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


// get L-R-R-L panning value from a (zero-based!) channel number
#define PROTRACKER_PANNING(n) (((((n) + 1) >> 1) & 1) * 256)

// convert .mod finetune byte value to c5speed
#define MOD_FINETUNE(b) (finetune_table[((b) & 0xf) ^ 8])

/* --------------------------------------------------------------------------------------------------------- */

#endif /* ! FMT_H */

