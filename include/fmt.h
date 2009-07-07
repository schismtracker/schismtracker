/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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

#include "diskwriter.h"

#include "sndfile.h"

#ifdef __cplusplus
extern "C" {
#endif

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

/* --------------------------------------------------------------------------------------------------------- */

typedef int (*fmt_read_info_func) (dmoz_file_t *file, const uint8_t *data, size_t length);
typedef int (*fmt_load_song_func) (CSoundFile *song, slurp_t *fp, unsigned int lflags);
typedef int (*fmt_load_sample_func) (const uint8_t *data, size_t length, song_sample *smp, char *title);
typedef int (*fmt_save_sample_func) (diskwriter_driver_t *fp, song_sample *smp, char *title);
typedef int (*fmt_load_instrument_func) (const uint8_t *data, size_t length, int slot);

#define READ_INFO(t) int fmt_##t##_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
#define LOAD_SONG(t) int fmt_##t##_load_song(CSoundFile *song, slurp_t *fp, unsigned int lflags)
#define SAVE_SONG(t) void fmt_##t##_save_song(diskwriter_driver_t *fp)
#define LOAD_SAMPLE(t) int fmt_##t##_load_sample(const uint8_t *data, size_t length, song_sample *smp, char *title)
#define SAVE_SAMPLE(t) int fmt_##t##_save_sample(diskwriter_driver_t *fp, song_sample *smp, char *title)
#define LOAD_INSTRUMENT(t) int fmt_##t##_load_instrument(const uint8_t *data, size_t length, int slot)

/* module types (and some other things that act like modules) */
READ_INFO(669); LOAD_SONG(669);
READ_INFO(ams);
READ_INFO(dtm);
READ_INFO(f2r);
READ_INFO(far);
READ_INFO(imf);
READ_INFO(it);
READ_INFO(liq);
READ_INFO(mdl);
// mid med mod... and i suppose mt2 is mad, now we just need a mud format!
READ_INFO(mid);                 SAVE_SONG(mid);
READ_INFO(med);
READ_INFO(mod); LOAD_SONG(mod);
READ_INFO(mt2);
READ_INFO(mtm); LOAD_SONG(mtm); SAVE_SONG(mtm);
READ_INFO(ntk);
READ_INFO(okt);
READ_INFO(psm);
READ_INFO(s3m); LOAD_SONG(s3m);
READ_INFO(sfx); LOAD_SONG(sfx);
READ_INFO(stm);
READ_INFO(ult);
READ_INFO(xm);

/* things that don't really act like modules or samples, and which we also don't use in any way */
#ifdef USE_NON_TRACKED_TYPES
READ_INFO(sid);
READ_INFO(mp3);
# ifdef HAVE_VORBIS
READ_INFO(ogg);
# endif
#endif

/* sample types */
READ_INFO(aiff); LOAD_SAMPLE(aiff);    SAVE_SAMPLE(aiff);
READ_INFO(au);   LOAD_SAMPLE(au);      SAVE_SAMPLE(au);
READ_INFO(iti);  LOAD_INSTRUMENT(iti);
READ_INFO(its);  LOAD_SAMPLE(its);     SAVE_SAMPLE(its);
READ_INFO(pat);  LOAD_INSTRUMENT(pat);
                 LOAD_SAMPLE(raw);     SAVE_SAMPLE(raw);
READ_INFO(scri); LOAD_SAMPLE(scri);                       LOAD_INSTRUMENT(scri);
READ_INFO(wav);  LOAD_SAMPLE(wav);     SAVE_SAMPLE(wav);
READ_INFO(xi);                                            LOAD_INSTRUMENT(xi);

#undef READ_INFO
#undef LOAD_SONG
#undef SAVE_SONG
#undef LOAD_SAMPLE
#undef SAVE_SAMPLE
#undef LOAD_INSTRUMENT

/* --------------------------------------------------------------------------------------------------------- */
struct instrumentloader {
	song_instrument *inst;
	int sample_map[SCHISM_MAX_SAMPLES];
	int basex, slot, expect_samples;
};
song_instrument *instrument_loader_init(struct instrumentloader *ii, int slot);
int instrument_loader_abort(struct instrumentloader *ii);
int instrument_loader_sample(struct instrumentloader *ii, int slot);


/* --------------------------------------------------------------------------------------------------------- */

/* save the sample's data in little- or big- endian byte order (defined in audio_loadsave.cc)

noe == no interleave (IT214)

should probably return something, but... meh :P */
void save_sample_data_LE(diskwriter_driver_t *fp, song_sample *smp, int noe);
void save_sample_data_BE(diskwriter_driver_t *fp, song_sample *smp, int noe);

/* shared by the .it, .its, and .iti saving functions */
void save_its_header(diskwriter_driver_t *fp, song_sample *smp, char *title);
int load_its_sample(const uint8_t *header, const uint8_t *data,
		size_t length, song_sample *smp, char *title);

/* --------------------------------------------------------------------------------------------------------- */
// other misc functions...

// load a .mod-style 4-byte packed note
void mod_import_note(const uint8_t p[4], MODCOMMAND *note);

// get L-R-R-L panning value from a (zero-based!) channel number
#define PROTRACKER_PANNING(c) ((((c) & 3) == 1 || ((c) & 3) == 2) ? 256 : 0)

// convert .mod finetune byte value to c5speed
#define MOD_FINETUNE(b) (S3MFineTuneTable[((b) & 0xf) ^ 8])

/* --------------------------------------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* ! FMT_H */

