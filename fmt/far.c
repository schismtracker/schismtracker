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

/* --------------------------------------------------------------------- */

int fmt_far_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        /* The magic for this format is truly weird (which I suppose is good, as the chance of it
        being "accidentally" correct is pretty low) */
        if (!(length > 47 && memcmp(data + 44, "\x0d\x0a\x1a", 3) == 0 && memcmp(data, "FAR\xfe", 4) == 0))
                return false;

        file->description = "Farandole Module";
        /*file->extension = str_dup("far");*/
        file->title = calloc(41, sizeof(char));
        memcpy(file->title, data + 4, 40);
        file->title[40] = 0;
        file->type = TYPE_MODULE_S3M;
        return true;
}

/* --------------------------------------------------------------------------------------------------------- */
/* This loader sucks. Mostly it was implemented based on what Modplug does, which is
kind of counterproductive, but I can't get Farandole to run in Dosbox to test stuff */

#pragma pack(push, 1)
struct far_header {
        uint8_t magic[4];
        char title[40];
        uint8_t eof[3];
        uint16_t header_len;
        uint8_t version;
        uint8_t onoff[16];
        uint8_t editing_state[9]; // stuff we don't care about
        uint8_t default_speed;
        uint8_t chn_panning[16];
        uint8_t pattern_state[4]; // more stuff we don't care about
        uint16_t message_len;
};

struct far_sample {
        char name[32];
        uint32_t length;
        uint8_t finetune;
        uint8_t volume;
        uint32_t loopstart;
        uint32_t loopend;
        uint8_t type;
        uint8_t loop;
};
#pragma pack(pop)

static uint8_t far_effects[] = {
        CMD_NONE,
        CMD_PORTAMENTOUP,
        CMD_PORTAMENTODOWN,
        CMD_TONEPORTAMENTO,
        CMD_RETRIG,
        CMD_VIBRATO, // depth
        CMD_VIBRATO, // speed
        CMD_VOLUMESLIDE, // up
        CMD_VOLUMESLIDE, // down
        CMD_VIBRATO, // sustained (?)
        CMD_NONE, // actually slide-to-volume
        CMD_PANNING,
        CMD_S3MCMDEX, // note offset => note delay?
        CMD_NONE, // fine tempo down
        CMD_NONE, // fine tempo up
        CMD_SPEED,
};

static void far_import_note(MODCOMMAND *note, const uint8_t data[4])
{
        if (data[0] > 0 && data[0] < 85) {
                note->note = data[0] + 36;
                note->instr = data[1] + 1;
        }
        if (data[2] & 0x0F) {
                note->volcmd = VOLCMD_VOLUME;
                note->vol = (data[2] & 0x0F) << 2; // askjdfjasdkfjasdf
        }
        note->param = data[3] & 0xf;
        switch (data[3] >> 4) {
        case 3: // porta to note
                note->param <<= 2;
                break;
        case 4: // retrig
                note->param = 6 / (1 + (note->param & 0xf)) + 1; // ugh?
                break;
        case 6: // vibrato speed
        case 7: // volume slide up
        case 0xb: // panning
                note->param <<= 4;
                break;
        case 0xa: // volume-portamento (what!)
                note->volcmd = VOLCMD_VOLUME;
                note->vol = (note->param << 2) + 4;
                break;
        case 0xc: // note offset
                note->param = 6 / (1 + (note->param & 0xf)) + 1;
                note->param |= 0xd;
        }
        note->command = far_effects[data[3] >> 4];
}

int fmt_far_load_song(CSoundFile *song, slurp_t *fp, unsigned int lflags)
{
        struct far_header fhdr;
        struct far_sample fsmp;
        SONGSAMPLE *smp;
        int nord, restartpos;
        int n, pat, row, chn;
        uint8_t orderlist[256];
        uint16_t pattern_size[256];
        uint8_t data[8];

        slurp_read(fp, &fhdr, sizeof(fhdr));
        if (memcmp(fhdr.magic, "FAR\xfe", 4) != 0 || memcmp(fhdr.eof, "\x0d\x0a\x1a", 3) != 0)
                return LOAD_UNSUPPORTED;

        fhdr.title[25] = '\0';
        strcpy(song->song_title, fhdr.title);
        fhdr.header_len = bswapLE16(fhdr.header_len);
        fhdr.message_len = bswapLE16(fhdr.message_len);

        for (n = 0; n < 16; n++) {
                /* WHAT A GREAT WAY TO STORE THIS INFORMATION */
                song->Channels[n].nPan = SHORT_PANNING[fhdr.chn_panning[n] & 0xf];
                song->Channels[n].nPan *= 4; //mphack
                if (!fhdr.onoff[n])
                        song->Channels[n].dwFlags |= CHN_MUTE;
        }
        for (; n < 64; n++)
                song->Channels[n].dwFlags |= CHN_MUTE;

        song->m_nDefaultSpeed = fhdr.default_speed;
        song->m_nDefaultTempo = 80;

        // to my knowledge, no other program is insane enough to save in this format
        strcpy(song->tracker_id, "Farandole Composer");

        /* Farandole's song message doesn't have line breaks, and the tracker runs in
        some screwy ultra-wide text mode, so this displays more or less like crap. */
        n = MIN(fhdr.message_len, MAX_MESSAGE - 1);
        slurp_read(fp, song->m_lpszSongComments, n);
        song->m_lpszSongComments[n] = '\0';

        slurp_seek(fp, sizeof(fhdr) + fhdr.message_len, SEEK_SET);

        if ((lflags & (LOAD_NOSAMPLES | LOAD_NOPATTERNS)) == (LOAD_NOSAMPLES | LOAD_NOPATTERNS))
                return LOAD_SUCCESS;

        slurp_read(fp, orderlist, 256);
        slurp_getc(fp); // supposed to be "number of patterns stored in the file"; apparently that's wrong
        nord = slurp_getc(fp);
        restartpos = slurp_getc(fp);

        nord = MIN(nord, MAX_ORDERS);
        memcpy(song->Orderlist, orderlist, nord);
        memset(song->Orderlist + nord, ORDER_LAST, MAX_ORDERS - nord);

        slurp_read(fp, pattern_size, 256 * 2); // byteswapped later
        slurp_seek(fp, fhdr.header_len - (869 + fhdr.message_len), SEEK_CUR);

        for (pat = 0; pat < 256; pat++) {
                int breakpos, rows;
                MODCOMMAND *note;

                pattern_size[pat] = bswapLE16(pattern_size[pat]);

                if (pat >= MAX_PATTERNS || pattern_size[pat] < (2 + 16 * 4)) {
                        slurp_seek(fp, pattern_size[pat], SEEK_CUR);
                        continue;
                }
                breakpos = slurp_getc(fp);
                slurp_getc(fp); // apparently, this value is *not* used anymore!!! I will not support it!!

                rows = (pattern_size[pat] - 2) / (16 * 4);
                if (!rows)
                        continue;
                note = song->Patterns[pat] = csf_allocate_pattern(rows, 64);
                song->PatternSize[pat] = song->PatternAllocSize[pat] = rows;
                breakpos = breakpos && breakpos < rows - 2 ? breakpos + 1 : -1;
                for (row = 0; row < rows; row++, note += 48) {
                        for (chn = 0; chn < 16; chn++, note++) {
                                slurp_read(fp, data, 4);
                                far_import_note(note, data);
                        }
                        if (row == breakpos)
                                note->command = CMD_PATTERNBREAK;
                }
        }
        csf_insert_restart_pos(song, restartpos);

        if (lflags & LOAD_NOSAMPLES)
                return LOAD_SUCCESS;

        slurp_read(fp, data, 8);
        smp = song->Samples + 1;
        for (n = 0; n < 64; n++, smp++) {
                if (!(data[n / 8] & (1 << (n % 8)))) /* LOLWHAT */
                        continue;
                slurp_read(fp, &fsmp, sizeof(fsmp));
                fsmp.name[25] = '\0';
                strcpy(smp->name, fsmp.name);
                smp->nLength = fsmp.length = bswapLE32(fsmp.length);
                smp->nLoopStart = bswapLE32(fsmp.loopstart);
                smp->nLoopEnd = bswapLE32(fsmp.loopend);
                smp->nVolume = fsmp.volume << 4; // "not supported", but seems to exist anyway
                if (fsmp.type & 1) {
                        smp->nLength >>= 1;
                        smp->nLoopStart >>= 1;
                        smp->nLoopEnd >>= 1;
                }
                if (smp->nLoopEnd > smp->nLoopStart && (fsmp.loop & 8))
                        smp->uFlags |= CHN_LOOP;
                smp->nC5Speed = 16726;
                smp->nGlobalVol = 64;
                csf_read_sample(smp, SF_LE | SF_M | SF_PCMS | ((fsmp.type & 1) ? SF_16 : SF_8),
                        fp->data + fp->pos, fp->length - fp->pos);
                slurp_seek(fp, fsmp.length, SEEK_CUR);
        }

        return LOAD_SUCCESS;
}

