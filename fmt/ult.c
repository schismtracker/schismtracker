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
#include "it.h" /* for get_effect_char */

#include "sndfile.h"

#include <math.h> /* for pow */

/* --------------------------------------------------------------------- */

int fmt_ult_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        if (!(length > 48 && memcmp(data, "MAS_UTrack_V00", 14) == 0))
                return 0;

        file->description = "UltraTracker Module";
        file->type = TYPE_MODULE_S3M;
        /*file->extension = str_dup("ult");*/
        file->title = calloc(33, sizeof(char));
        memcpy(file->title, data + 15, 32);
        file->title[32] = 0;
        return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

enum {
        ULT_16BIT = 4,
        ULT_LOOP  = 8,
        ULT_PINGPONGLOOP = 16,
};

#pragma pack(push, 1)
struct ult_sample {
        char name[32];
        char filename[12];
        uint32_t loop_start;
        uint32_t loop_end;
        uint32_t size_start;
        uint32_t size_end;
        uint8_t volume; // 0-255, apparently prior to 1.4 this was logarithmic?
        uint8_t flags; // above
        uint16_t speed; // only exists for 1.4+
        int16_t finetune;
};
#pragma pack(pop)


/* Unhandled effects:
5x1 - do not loop sample (x is unused)
5x2 - play sample backwards
5xC - end loop and finish sample
9xx - set sample offset to xx * 1024
    with 9yy: set sample offset to xxyy * 4
E0x - set vibrato strength (2 is normal)
F00 - reset speed/tempo to 6/125

Apparently 3xx will CONTINUE to slide until it reaches its destination, or
until a 300 effect is encountered. I'm not attempting to handle this (yet).

The logarithmic volume scale used in older format versions here, or pretty
much anywhere for that matter. I don't even think Ultra Tracker tries to
convert them. */

static const uint8_t ult_efftrans[] = {
        CMD_ARPEGGIO,
        CMD_PORTAMENTOUP,
        CMD_PORTAMENTODOWN,
        CMD_TONEPORTAMENTO,
        CMD_VIBRATO,
        CMD_NONE,
        CMD_NONE,
        CMD_TREMOLO,
        CMD_NONE,
        CMD_OFFSET,
        CMD_VOLUMESLIDE,
        CMD_PANNING,
        CMD_VOLUME,
        CMD_PATTERNBREAK,
        CMD_NONE, // extended effects, processed separately
        CMD_SPEED,
};

static void translate_fx(uint8_t *pe, uint8_t *pp)
{
        uint8_t e = *pe & 0xf;
        uint8_t p = *pp;

        *pe = ult_efftrans[e];

        switch (e) {
        case 0:
                if (!p)
                        *pe = CMD_NONE;
                break;
        case 3:
                // 300 apparently stops sliding, which is totally weird
                if (!p)
                        p = 1; // close enough?
                break;
        case 0xa:
                // blah, this sucks
                if (p & 0xf0)
                        p &= 0xf0;
                break;
        case 0xb:
                // mikmod does this wrong, resulting in values 0-225 instead of 0-255
                p = (p & 0xf) * 0x11;
                break;
        case 0xc: // volume
                p >>= 2;
                break;
        case 0xd: // pattern break
                p = 10 * (p >> 4) + (p & 0xf);
        case 0xe: // special
                switch (p >> 4) {
                case 1:
                        *pe = CMD_PORTAMENTOUP;
                        p = 0xf0 | (p & 0xf);
                        break;
                case 2:
                        *pe = CMD_PORTAMENTODOWN;
                        p = 0xf0 | (p & 0xf);
                        break;
                case 8:
                        *pe = CMD_S3MCMDEX;
                        p = 0x60 | (p & 0xf);
                        break;
                case 9:
                        *pe = CMD_RETRIG;
                        p &= 0xf;
                        break;
                case 0xa:
                        *pe = CMD_VOLUMESLIDE;
                        p = ((p & 0xf) << 4) | 0xf;
                        break;
                case 0xb:
                        *pe = CMD_VOLUMESLIDE;
                        p = 0xf0 | (p & 0xf);
                        break;
                case 0xc: case 0xd:
                        *pe = CMD_S3MCMDEX;
                        break;
                }
                break;
        case 0xf:
                if (p > 0x2f)
                        *pe = CMD_TEMPO;
                break;
        }

        *pp = p;
}

static int read_ult_event(slurp_t *fp, MODCOMMAND *note, int *lostfx)
{
        uint8_t b, repeat = 1;
        uint32_t off;
        int n;

        b = slurp_getc(fp);
        if (b == 0xfc) {
                repeat = slurp_getc(fp);
                b = slurp_getc(fp);
        }
        note->note = (b > 0 && b < 61) ? b + 36 : NOTE_NONE;
        note->instr = slurp_getc(fp);
        b = slurp_getc(fp);
        note->volcmd = b & 0xf;
        note->command = b >> 4;
        note->vol = slurp_getc(fp);
        note->param = slurp_getc(fp);
        translate_fx(&note->volcmd, &note->vol);
        translate_fx(&note->command, &note->param);

        // sample offset -- this is even more special than digitrakker's
        if (note->volcmd == CMD_OFFSET && note->command == CMD_OFFSET) {
                off = ((note->vol << 8) | note->param) >> 6;
                note->volcmd = CMD_NONE;
                note->param = MIN(off, 0xff);
        } else if (note->volcmd == CMD_OFFSET) {
                off = note->vol * 4;
                note->vol = MIN(off, 0xff);
        } else if (note->command == CMD_OFFSET) {
                off = note->param * 4;
                note->param = MIN(off, 0xff);
        } else if (note->volcmd == note->command) {
                /* don't try to figure out how ultratracker does this, it's quite random */
                note->command = CMD_NONE;
        }
        if (note->command == CMD_VOLUME || (note->command == CMD_NONE && note->volcmd != CMD_VOLUME)) {
                swap_effects(note);
        }

        // Do that dance.
        // Maybe I should quit rewriting this everywhere and make a generic version :P
        for (n = 0; n < 4; n++) {
                if (convert_voleffect_of(note, n >> 1)) {
                        n = 5;
                        break;
                }
                swap_effects(note);
        }
        if (n < 5) {
                if (effect_weight[note->volcmd] > effect_weight[note->command])
                        swap_effects(note);
                (*lostfx)++;
                //log_appendf(4, "Effect dropped: %c%02X < %c%02X", get_effect_char(note->volcmd), note->vol,
                //        get_effect_char(note->command), note->param);
                note->volcmd = 0;
        }
        if (!note->volcmd)
                note->vol = 0;
        if (!note->command)
                note->param = 0;
        return repeat;
}

/* --------------------------------------------------------------------------------------------------------- */

int fmt_ult_load_song(CSoundFile *song, slurp_t *fp, unsigned int lflags)
{
        char buf[34];
        uint8_t ver;
        int nmsg, nsmp, nchn, npat;
        int n, chn, pat, row;
        int lostfx = 0, gxx = 0;
        struct ult_sample usmp;
        SONGSAMPLE *smp;
        const char *verstr[] = {"<1.4", "1.4", "1.5", "1.6"};

        slurp_read(fp, buf, 14);
        if (memcmp(buf, "MAS_UTrack_V00", 14) != 0)
                return LOAD_UNSUPPORTED;
        ver = slurp_getc(fp);
        if (ver < '1' || ver > '4')
                return LOAD_FORMAT_ERROR;
        ver -= '0';

        slurp_read(fp, buf, 32);
        buf[25] = '\0';
        strcpy(song->song_title, buf);

        sprintf(song->tracker_id, "Ultra Tracker %s", verstr[ver - 1]);
        song->m_dwSongFlags |= SONG_COMPATGXX | SONG_ITOLDEFFECTS;

        nmsg = slurp_getc(fp);
        read_lined_message(song->m_lpszSongComments, fp, nmsg * 32, 32);

        nsmp = slurp_getc(fp);
        for (n = 0, smp = song->Samples + 1; n < nsmp; n++, smp++) {
                // annoying: v4 added a field before the end of the struct
                if (ver >= 4) {
                        slurp_read(fp, &usmp, sizeof(usmp));
                        usmp.speed = bswapLE16(usmp.speed);
                } else {
                        slurp_read(fp, &usmp, 64);
                        usmp.finetune = usmp.speed;
                        usmp.speed = 8363;
                }
                usmp.finetune = bswapLE16(usmp.finetune);
                usmp.loop_start = bswapLE32(usmp.loop_start);
                usmp.loop_end = bswapLE32(usmp.loop_end);
                usmp.size_start = bswapLE32(usmp.size_start);
                usmp.size_end = bswapLE32(usmp.size_end);

                strncpy(smp->name, usmp.name, 25);
                smp->name[25] = '\0';
                strncpy(smp->filename, usmp.filename, 12);
                smp->filename[12] = '\0';
                if (usmp.size_end <= usmp.size_start)
                        continue;
                smp->nLength = usmp.size_end - usmp.size_start;
                smp->nLoopStart = usmp.loop_start;
                smp->nLoopEnd = MIN(usmp.loop_end, smp->nLength);
                smp->nVolume = usmp.volume; //mphack - should be 0-64 not 0-256
                smp->nGlobalVol = 64;

                /* mikmod does some weird integer math here, but it didn't really work for me */
                smp->nC5Speed = usmp.speed;
                if (usmp.finetune)
                        smp->nC5Speed *= pow(2, (usmp.finetune / (12.0 * 32768)));

                if (usmp.flags & ULT_LOOP)
                        smp->uFlags |= CHN_LOOP;
                if (usmp.flags & ULT_PINGPONGLOOP)
                        smp->uFlags |= CHN_PINGPONGLOOP;
                if (usmp.flags & ULT_16BIT) {
                        smp->uFlags |= CHN_16BIT;
                        smp->nLoopStart >>= 1;
                        smp->nLoopEnd >>= 1;
                }
        }

        // ult just so happens to use 255 for its end mark, so there's no need to fiddle with this
        slurp_read(fp, song->Orderlist, 256);

        nchn = slurp_getc(fp) + 1;
        npat = slurp_getc(fp) + 1;

        if (nchn > 32 || npat > MAX_PATTERNS)
                return LOAD_FORMAT_ERROR;

        if (ver >= 3) {
                for (n = 0; n < nchn; n++)
                        song->Channels[n].nPan = ((slurp_getc(fp) & 0xf) << 2) + 2;
        } else {
                for (n = 0; n < nchn; n++)
                        song->Channels[n].nPan = (n & 1) ? 48 : 16;
        }
        for (; n < 64; n++) {
                song->Channels[n].nPan = 32;
                song->Channels[n].dwFlags = CHN_MUTE;
        }
        //mphack - fix the pannings
        for (n = 0; n < 64; n++)
                song->Channels[n].nPan *= 4;

        if ((lflags & (LOAD_NOSAMPLES | LOAD_NOPATTERNS)) == (LOAD_NOSAMPLES | LOAD_NOPATTERNS))
                return LOAD_SUCCESS;

        for (pat = 0; pat < npat; pat++) {
                song->PatternSize[pat] = song->PatternAllocSize[pat] = 64;
                song->Patterns[pat] = csf_allocate_pattern(64, 64);
        }
        for (chn = 0; chn < nchn; chn++) {
                MODCOMMAND evnote;
                MODCOMMAND *note;
                int repeat;

                for (pat = 0; pat < npat; pat++) {
                        note = song->Patterns[pat] + chn;
                        row = 0;
                        while (row < 64) {
                                repeat = read_ult_event(fp, &evnote, &lostfx);
                                if (evnote.command == CMD_TONEPORTAMENTO
                                    || evnote.volcmd == VOLCMD_TONEPORTAMENTO) {
                                        gxx |= 1;
                                }
                                if (repeat + row > 64)
                                        repeat = 64 - row;
                                while (repeat--) {
                                        *note = evnote;
                                        note += 64;
                                        row++;
                                }
                        }
                }
        }
        if (gxx)
                log_appendf(4, " Warning: Gxx effects may not be suitably imported");
        if (lostfx)
                log_appendf(4, " Warning: %d effect%s dropped", lostfx, lostfx == 1 ? "" : "s");

        if (!(lflags & LOAD_NOSAMPLES)) {
                for (n = 0, smp = song->Samples + 1; n < nsmp; n++, smp++) {
                        uint32_t ssize = csf_read_sample(smp,
                                SF_LE | SF_M | SF_PCMS | ((smp->uFlags & CHN_16BIT) ? SF_16 : SF_8),
                                fp->data + fp->pos, fp->length - fp->pos);
                        slurp_seek(fp, ssize, SEEK_CUR);
                }
        }
        return LOAD_SUCCESS;
}

