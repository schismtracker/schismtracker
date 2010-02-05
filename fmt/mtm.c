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

#define NEED_BYTESWAP
#include "headers.h"
#include "it.h" /* for feature_check_blahblah */
#include "fmt.h"
#include "song.h"
#include "tables.h"

#include <stdint.h>

/* --------------------------------------------------------------------- */

#pragma pack(push, 1)
typedef struct mtm_header {
        char filever[4]; /* M T M \x10 */
        char title[20]; /* asciz */
        uint16_t ntracks;
        uint8_t last_pattern;
        uint8_t last_order; /* songlength - 1 */
        uint16_t msglen;
        uint8_t nsamples;
        uint8_t flags; /* always 0 */
        uint8_t rows; /* prob. 64 */
        uint8_t nchannels;
        uint8_t panpos[32];
} mtm_header_t;

typedef struct mtm_sample {
        char name[22];
        uint32_t length, loop_start, loop_end;
        uint8_t finetune, volume, flags;
} mtm_sample_t;
#pragma pack(pop)

/* --------------------------------------------------------------------- */

int fmt_mtm_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        if (!(length > 24 && memcmp(data, "MTM", 3) == 0))
                return 0;

        file->description = "MultiTracker Module";
        /*file->extension = str_dup("mtm");*/
        file->title = calloc(21, sizeof(char));
        memcpy(file->title, data + 4, 20);
        file->title[20] = 0;
        file->type = TYPE_MODULE_MOD;
        return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

static void mtm_unpack_track(const uint8_t *b, MODCOMMAND *note, int rows)
{
        int n;

        for (n = 0; n < rows; n++, note++, b += 3) {
                note->note = ((b[0] & 0xfc) ? ((b[0] >> 2) + 36 + 1) : NOTE_NONE);
                note->instr = ((b[0] & 0x3) << 4) | (b[1] >> 4);
                note->volcmd = VOLCMD_NONE;
                note->vol = 0;
                note->command = b[1] & 0xf;
                note->param = b[2];
                /* From mikmod: volume slide up always overrides slide down */
                if (note->command == 0xa && (note->param & 0xf0))
                        note->param &= 0xf0;
                csf_import_mod_effect(note, 0);
        }
}

int fmt_mtm_load_song(CSoundFile *song, slurp_t *fp, unsigned int lflags)
{
        uint8_t b[192];
        uint16_t ntrk, nchan, nord, npat, nsmp;
        uint16_t comment_len;
        int n, pat, chan, smp, rows;
        MODCOMMAND *note;
        uint16_t tmp;
        uint32_t tmplong;
        MODCOMMAND **trackdata, *tracknote;
        SONGSAMPLE *sample;

        slurp_read(fp, b, 3);
        if (memcmp(b, "MTM", 3) != 0)
                return LOAD_UNSUPPORTED;
        n = slurp_getc(fp);
        sprintf(song->tracker_id, "MultiTracker %d.%d", n >> 4, n & 0xf);
        slurp_read(fp, song->song_title, 20);
        song->song_title[20] = 0;
        slurp_read(fp, &ntrk, 2);
        ntrk = bswapLE16(ntrk);
        npat = slurp_getc(fp);
        nord = slurp_getc(fp) + 1;

        slurp_read(fp, &comment_len, 2);
        comment_len = bswapLE16(comment_len);

        nsmp = slurp_getc(fp);
        slurp_getc(fp); /* attribute byte (unused) */
        rows = slurp_getc(fp); /* beats per track (translation: number of rows in every pattern) */
        if (rows != 64) {
                printf("TODO: test this file with other players (beats per track != 64)");
        }
        nchan = slurp_getc(fp);
        for (n = 0; n < 32; n++) {
                song->Channels[n].nPan = SHORT_PANNING[slurp_getc(fp) & 0xf];
                song->Channels[n].nPan *= 4; //mphack
        }
        for (n = nchan; n < MAX_CHANNELS; n++)
                song->Channels[n].dwFlags = CHN_MUTE;

        for (n = 1, sample = song->Samples + 1; n <= nsmp; n++, sample++) {
                slurp_read(fp, sample->name, 22);
                sample->name[22] = 0;
                slurp_read(fp, &tmplong, 4);
                sample->nLength = bswapLE32(tmplong);
                slurp_read(fp, &tmplong, 4);
                sample->nLoopStart = bswapLE32(tmplong);
                slurp_read(fp, &tmplong, 4);
                sample->nLoopEnd = bswapLE32(tmplong);
                if ((sample->nLoopEnd - sample->nLoopStart) > 2) {
                        sample->uFlags |= CHN_LOOP;
                } else {
                        /* Both Impulse Tracker and Modplug do this */
                        sample->nLoopStart = 0;
                        sample->nLoopEnd = 0;
                }
                song->Samples[n].nC5Speed = MOD_FINETUNE(slurp_getc(fp));
                sample->nVolume = slurp_getc(fp);
                sample->nVolume *= 4; //mphack
                sample->nGlobalVol = 64;
                if (slurp_getc(fp) & 1) {
                        printf("TODO: double check 16 bit sample loading");
                        sample->uFlags |= CHN_16BIT;
                        sample->nLength >>= 1;
                        sample->nLoopStart >>= 1;
                        sample->nLoopEnd >>= 1;
                }
                song->Samples[n].nVibType = 0;
                song->Samples[n].nVibSweep = 0;
                song->Samples[n].nVibDepth = 0;
                song->Samples[n].nVibRate = 0;
        }

        /* orderlist */
        slurp_read(fp, song->Orderlist, 128);
        memset(song->Orderlist + nord, ORDER_LAST, MAX_ORDERS - nord);

        /* tracks */
        trackdata = calloc(ntrk, sizeof(MODCOMMAND *));
        for (n = 0; n < ntrk; n++) {
                slurp_read(fp, b, 3 * rows);
                trackdata[n] = calloc(rows, sizeof(MODCOMMAND));
                mtm_unpack_track(b, trackdata[n], rows);
        }

        /* patterns */
        for (pat = 0; pat <= npat; pat++) {
                song->Patterns[pat] = csf_allocate_pattern(MAX(rows, 32), 64);
                song->PatternSize[pat] = song->PatternAllocSize[pat] = 64;
                tracknote = trackdata[n];
                for (chan = 0; chan < 32; chan++) {
                        slurp_read(fp, &tmp, 2);
                        tmp = bswapLE16(tmp);
                        if (tmp == 0)
                                continue;
                        note = song->Patterns[pat] + chan;
                        tracknote = trackdata[tmp - 1];
                        for (n = 0; n < rows; n++, tracknote++, note += MAX_CHANNELS)
                                *note = *tracknote;
                }
                if (rows < 32) {
                        /* stick a pattern break on the first channel with an empty effect column
                         * (XXX don't do this if there's already one in another column) */
                        note = song->Patterns[pat] + 64 * (rows - 1);
                        while (note->command || note->param)
                                note++;
                        note->command = CMD_PATTERNBREAK;
                }
        }

        /* free willy */
        for (n = 0; n < ntrk; n++)
                free(trackdata[n]);
        free(trackdata);

        read_lined_message(song->m_lpszSongComments, fp, comment_len, 40);

        /* sample data */
        if (!(lflags & LOAD_NOSAMPLES)) {
                for (smp = 1; smp <= nsmp; smp++) {
                        int8_t *ptr;
                        int bps = 1;    /* bytes per sample (i.e. bits / 8) */

                        if (song->Samples[smp].nLength == 0)
                                continue;
                        if (song->Samples[smp].uFlags & CHN_16BIT)
                                bps = 2;
                        ptr = csf_allocate_sample(bps * song->Samples[smp].nLength);
                        slurp_read(fp, ptr, bps * song->Samples[smp].nLength);
                        song->Samples[smp].pSample = ptr;

                        /* convert to signed */
                        n = song->Samples[smp].nLength;
                        while (n-- > 0)
                                ptr[n] += 0x80;
                }
        }

        /* set the rest of the stuff */
        song->m_dwSongFlags = SONG_ITOLDEFFECTS | SONG_COMPATGXX;

//      if (ferror(fp)) {
//              return LOAD_FILE_ERROR;
//      }

        /* done! */
        return LOAD_SUCCESS;
}

