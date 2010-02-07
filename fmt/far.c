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
                return 0;

        file->description = "Farandole Module";
        /*file->extension = str_dup("far");*/
        file->title = calloc(41, sizeof(char));
        memcpy(file->title, data + 4, 40);
        file->title[40] = 0;
        file->type = TYPE_MODULE_S3M;
        return 1;
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
        FX_NONE,
        FX_PORTAMENTOUP,
        FX_PORTAMENTODOWN,
        FX_TONEPORTAMENTO,
        FX_RETRIG,
        FX_VIBRATO, // depth
        FX_VIBRATO, // speed
        FX_VOLUMESLIDE, // up
        FX_VOLUMESLIDE, // down
        FX_VIBRATO, // sustained (?)
        FX_NONE, // actually slide-to-volume
        FX_PANNING,
        FX_S3MCMDEX, // note offset => note delay?
        FX_NONE, // fine tempo down
        FX_NONE, // fine tempo up
        FX_SPEED,
};

static void far_import_note(song_note_t *note, const uint8_t data[4])
{
        if (data[0] > 0 && data[0] < 85) {
                note->note = data[0] + 36;
                note->instrument = data[1] + 1;
        }
        if (data[2] & 0x0F) {
                note->voleffect = VOLFX_VOLUME;
                note->volparam = (data[2] & 0x0F) << 2; // askjdfjasdkfjasdf
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
                note->voleffect = VOLFX_VOLUME;
                note->volparam = (note->param << 2) + 4;
                break;
        case 0xc: // note offset
                note->param = 6 / (1 + (note->param & 0xf)) + 1;
                note->param |= 0xd;
        }
        note->effect = far_effects[data[3] >> 4];
}


int fmt_far_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
        struct far_header fhdr;
        struct far_sample fsmp;
        song_sample_t *smp;
        int nord, restartpos;
        int n, pat, row, chn;
        uint8_t orderlist[256];
        uint16_t pattern_size[256];
        uint8_t data[8];

        slurp_read(fp, &fhdr, sizeof(fhdr));
        if (memcmp(fhdr.magic, "FAR\xfe", 4) != 0 || memcmp(fhdr.eof, "\x0d\x0a\x1a", 3) != 0)
                return LOAD_UNSUPPORTED;

        fhdr.title[25] = '\0';
        strcpy(song->title, fhdr.title);
        fhdr.header_len = bswapLE16(fhdr.header_len);
        fhdr.message_len = bswapLE16(fhdr.message_len);

        for (n = 0; n < 16; n++) {
                /* WHAT A GREAT WAY TO STORE THIS INFORMATION */
                song->channels[n].panning = short_panning_table[fhdr.chn_panning[n] & 0xf];
                song->channels[n].panning *= 4; //mphack
                if (!fhdr.onoff[n])
                        song->channels[n].flags |= CHN_MUTE;
        }
        for (; n < 64; n++)
                song->channels[n].flags |= CHN_MUTE;

        song->initial_speed = fhdr.default_speed;
        song->initial_tempo = 80;

        // to my knowledge, no other program is insane enough to save in this format
        strcpy(song->tracker_id, "Farandole Composer");

        /* Farandole's song message doesn't have line breaks, and the tracker runs in
        some screwy ultra-wide text mode, so this displays more or less like crap. */
        read_lined_message(song->message, fp, fhdr.message_len, 132);


        slurp_seek(fp, sizeof(fhdr) + fhdr.message_len, SEEK_SET);

        if ((lflags & (LOAD_NOSAMPLES | LOAD_NOPATTERNS)) == (LOAD_NOSAMPLES | LOAD_NOPATTERNS))
                return LOAD_SUCCESS;

        slurp_read(fp, orderlist, 256);
        slurp_getc(fp); // supposed to be "number of patterns stored in the file"; apparently that's wrong
        nord = slurp_getc(fp);
        restartpos = slurp_getc(fp);

        nord = MIN(nord, MAX_ORDERS);
        memcpy(song->orderlist, orderlist, nord);
        memset(song->orderlist + nord, ORDER_LAST, MAX_ORDERS - nord);

        slurp_read(fp, pattern_size, 256 * 2); // byteswapped later
        slurp_seek(fp, fhdr.header_len - (869 + fhdr.message_len), SEEK_CUR);

        for (pat = 0; pat < 256; pat++) {
                int breakpos, rows;
                song_note_t *note;

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
                note = song->patterns[pat] = csf_allocate_pattern(rows);
                song->pattern_size[pat] = song->pattern_alloc_size[pat] = rows;
                breakpos = breakpos && breakpos < rows - 2 ? breakpos + 1 : -1;
                for (row = 0; row < rows; row++, note += 48) {
                        for (chn = 0; chn < 16; chn++, note++) {
                                slurp_read(fp, data, 4);
                                far_import_note(note, data);
                        }
                        if (row == breakpos)
                                note->effect = FX_PATTERNBREAK;
                }
        }
        csf_insert_restart_pos(song, restartpos);

        if (lflags & LOAD_NOSAMPLES)
                return LOAD_SUCCESS;

        slurp_read(fp, data, 8);
        smp = song->samples + 1;
        for (n = 0; n < 64; n++, smp++) {
                if (!(data[n / 8] & (1 << (n % 8)))) /* LOLWHAT */
                        continue;
                slurp_read(fp, &fsmp, sizeof(fsmp));
                fsmp.name[25] = '\0';
                strcpy(smp->name, fsmp.name);
                smp->length = fsmp.length = bswapLE32(fsmp.length);
                smp->loop_start = bswapLE32(fsmp.loopstart);
                smp->loop_end = bswapLE32(fsmp.loopend);
                smp->volume = fsmp.volume << 4; // "not supported", but seems to exist anyway
                if (fsmp.type & 1) {
                        smp->length >>= 1;
                        smp->loop_start >>= 1;
                        smp->loop_end >>= 1;
                }
                if (smp->loop_end > smp->loop_start && (fsmp.loop & 8))
                        smp->flags |= CHN_LOOP;
                smp->c5speed = 16726;
                smp->global_volume = 64;
                csf_read_sample(smp, SF_LE | SF_M | SF_PCMS | ((fsmp.type & 1) ? SF_16 : SF_8),
                        fp->data + fp->pos, fp->length - fp->pos);
                slurp_seek(fp, fsmp.length, SEEK_CUR);
        }

        return LOAD_SUCCESS;
}

