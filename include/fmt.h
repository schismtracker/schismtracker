/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
 * URL: http://rigelseven.com/schism/
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

#include "song.h"
#include "dmoz.h"
#include "util.h"

#include "diskwriter.h"

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------------------------------------- */

typedef int (*fmt_read_info_func) (dmoz_file_t *file, const byte *data, size_t length);
typedef int (*fmt_load_sample_func) (const byte *data, size_t length, song_sample *smp, char *title);
typedef int (*fmt_save_sample_func) (diskwriter_driver_t *fp, song_sample *smp, char *title);
typedef int (*fmt_load_instrument_func) (const byte *data, size_t length, int slot);

#define READ_INFO(t) int fmt_##t##_read_info(dmoz_file_t *file, const byte *data, size_t length)
#define LOAD_SAMPLE(t) int fmt_##t##_load_sample(const byte *data, size_t length, song_sample *smp, char *title)
#define SAVE_SAMPLE(t) int fmt_##t##_save_sample(diskwriter_driver_t *fp, song_sample *smp, char *title)
#define LOAD_INSTRUMENT(t) int fmt_##t##_load_instrument(const byte *data, size_t length, int slot)
#define SAVE_SONG(t) void fmt_##t##_save_song(diskwriter_driver_t *fp)

READ_INFO(669);
READ_INFO(ams);
READ_INFO(dtm);
READ_INFO(f2r);
READ_INFO(far);
READ_INFO(imf);
READ_INFO(it);
READ_INFO(liq);
READ_INFO(mdl);
READ_INFO(mod);
READ_INFO(mt2);
READ_INFO(mtm);
READ_INFO(ntk);
READ_INFO(s3m);
READ_INFO(stm);
READ_INFO(ult);
READ_INFO(xm);

#ifdef USE_NON_TRACKED_TYPES
READ_INFO(sid);
READ_INFO(mp3);
# ifdef HAVE_VORBIS
READ_INFO(ogg);
# endif
#endif

READ_INFO(iti);  LOAD_INSTRUMENT(iti);
READ_INFO(xi);   LOAD_INSTRUMENT(xi);
READ_INFO(pat);  LOAD_INSTRUMENT(pat);

READ_INFO(aiff); LOAD_SAMPLE(aiff);    SAVE_SAMPLE(aiff);
READ_INFO(au);   LOAD_SAMPLE(au);      SAVE_SAMPLE(au);
READ_INFO(its);  LOAD_SAMPLE(its);     SAVE_SAMPLE(its);
                 LOAD_SAMPLE(raw);     SAVE_SAMPLE(raw);
READ_INFO(wav);	 LOAD_SAMPLE(wav);     SAVE_SAMPLE(wav);
READ_INFO(psm);
READ_INFO(mid);						   SAVE_SONG(mid);


#undef READ_INFO
#undef LOAD_SAMPLE
#undef SAVE_SAMPLE
#undef SAVE_SONG
#undef LOAD_INSTRUMENT

/* --------------------------------------------------------------------------------------------------------- */

/* save the sample's data in little- or big- endian byte order (defined in audio_loadsave.cc)

noe == no interleave (IT214)

should probably return something, but... meh :P */
void save_sample_data_LE(diskwriter_driver_t *fp, song_sample *smp, int noe);
void save_sample_data_BE(diskwriter_driver_t *fp, song_sample *smp, int noe);

/* shared by the .it, .its, and .iti saving functions */
void save_its_header(diskwriter_driver_t *fp, song_sample *smp, char *title);
int load_its_sample(const byte *header, const byte *data,
		size_t length, song_sample *smp, char *title);

/* --------------------------------------------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* ! FMT_H */
