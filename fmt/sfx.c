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
#include "slurp.h"
#include "fmt.h"

#include "sndfile.h"

/* --------------------------------------------------------------------------------------------------------- */

int fmt_sfx_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        if (!(length > 64 && memcmp(data + 60, "SONG", 4) == 0))
                return 0;

        file->description = "Sound FX";
        /*file->extension = str_dup("sfx");*/
        file->title = strdup(""); // whatever
        file->type = TYPE_MODULE_MOD;
        return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

/* Loader taken mostly from XMP.
TODO: also handle 30-instrument files (no actual change in the structure, just read more instruments)

Why did I write a loader for such an obscure format? That is, besides the fact that neither Modplug nor
Mikmod support SFX (and for good reason; it's a particularly dumb format) */

int fmt_sfx_load_song(CSoundFile *song, slurp_t *fp, unsigned int lflags)
{
        uint8_t tag[4];
        int n, nord, npat, pat, chan;
        uint32_t smpsize[15];
        uint16_t tmp;
        MODCOMMAND *note;
        SONGSAMPLE *sample;

        slurp_seek(fp, 60, SEEK_SET);
        slurp_read(fp, tag, 4);
        if (memcmp(tag, "SONG", 4) != 0) // how ... generic
                return LOAD_UNSUPPORTED;

        slurp_rewind(fp);

        slurp_read(fp, smpsize, 4 * 15);
        slurp_seek(fp, 4, SEEK_CUR); /* the tag again */
        slurp_read(fp, &tmp, 2);
        if (!tmp)
                return LOAD_UNSUPPORTED; // erf
        tmp = 14565 * 122 / bswapBE16(tmp);
        song->m_nDefaultTempo = CLAMP(tmp, 31, 255);
        slurp_seek(fp, 14, SEEK_CUR); /* unknown bytes (reserved?) */
        for (n = 0, sample = song->Samples + 1; n < 15; n++, sample++) {
                slurp_read(fp, sample->name, 22);
                sample->name[22] = 0;
                slurp_read(fp, &tmp, 2); /* seems to be half the sample size, minus two bytes? */
                sample->nLength = bswapBE32(smpsize[n]);

                song->Samples[n].nC5Speed = MOD_FINETUNE(slurp_getc(fp)); // ?
                sample->nVolume = slurp_getc(fp);
                if (sample->nVolume > 64)
                        sample->nVolume = 64;
                sample->nVolume *= 4; //mphack
                sample->nGlobalVol = 64;
                slurp_read(fp, &tmp, 2);
                sample->nLoopStart = bswapBE16(tmp);
                slurp_read(fp, &tmp, 2);
                tmp = bswapBE16(tmp) * 2; /* loop length */
                if (tmp > 2)
                        sample->uFlags |= CHN_LOOP;
                sample->nLoopEnd = sample->nLoopStart + tmp;
                sample->nVibType = 0;
                sample->nVibSweep = 0;
                sample->nVibDepth = 0;
                sample->nVibRate = 0;
        }

        /* pattern/order stuff */
        nord = slurp_getc(fp);
        slurp_getc(fp); /* restart position? */
        slurp_read(fp, song->Orderlist, 128);
        npat = 0;
        for (n = 0; n < 128; n++) {
                if (song->Orderlist[n] > npat)
                        npat = song->Orderlist[n];
        }
        /* set all the extra orders to the end-of-song marker */
        memset(song->Orderlist + nord, ORDER_LAST, MAX_ORDERS - nord);

        for (pat = 0; pat <= npat; pat++) {
                note = song->Patterns[pat] = csf_allocate_pattern(64, 64);
                song->PatternSize[pat] = song->PatternAllocSize[pat] = 64;
                for (n = 0; n < 64; n++, note += 60) {
                        for (chan = 0; chan < 4; chan++, note++) {
                                uint8_t p[4];
                                slurp_read(fp, p, 4);
                                mod_import_note(p, note);
                                switch (note->command) {
                                case 1: /* arpeggio */
                                        note->command = CMD_ARPEGGIO;
                                        break;
                                case 2: /* pitch bend */
                                        if (note->param >> 4) {
                                                note->command = CMD_PORTAMENTODOWN;
                                                note->param >>= 4;
                                        } else if (note->param & 0xf) {
                                                note->command = CMD_PORTAMENTOUP;
                                                note->param &= 0xf;
                                        } else {
                                                note->command = 0;
                                        }
                                        break;
                                case 5: /* volume up */
                                        note->command = CMD_VOLUMESLIDE;
                                        note->param = (note->param & 0xf) << 4;
                                        break;
                                case 6: /* set volume */
                                        if (note->param > 64)
                                                note->param = 64;
                                        note->volcmd = VOLCMD_VOLUME;
                                        note->vol = 64 - note->param;
                                        note->command = 0;
                                        note->param = 0;
                                        break;
                                case 7: /* set step up */
                                case 8: /* set step down */
                                        printf("TODO: set step up/down - what is this?");
                                        break;
                                case 3: /* LED on (wtf!) */
                                case 4: /* LED off (ditto) */
                                default:
                                        note->command = 0;
                                        note->param = 0;
                                        break;
                                }
                        }
                }
        }

        /* sample data */
        if (!(lflags & LOAD_NOSAMPLES)) {
                for (n = 0, sample = song->Samples + 1; n < 31; n++, sample++) {
                        int8_t *ptr;

                        if (!sample->nLength)
                                continue;
                        ptr = csf_allocate_sample(sample->nLength);
                        slurp_read(fp, ptr, sample->nLength);
                        sample->pSample = ptr;
                }
        }

        /* more header info */
        song->m_dwSongFlags = SONG_ITOLDEFFECTS | SONG_COMPATGXX;
        for (n = 0; n < 4; n++)
                song->Channels[n].nPan = PROTRACKER_PANNING(n); /* ??? */
        for (; n < MAX_CHANNELS; n++)
                song->Channels[n].dwFlags = CHN_MUTE;

        strcpy(song->tracker_id, "Sound FX");
        song->m_nStereoSeparation = 64;

//      if (ferror(fp)) {
//              return LOAD_FILE_ERROR;
//      }

        /* done! */
        return LOAD_SUCCESS;
}

