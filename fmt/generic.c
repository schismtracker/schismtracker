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

#include "headers.h"
#include "fmt.h"

/* --------------------------------------------------------------------------------------------------------- */

static int _mod_period_to_note(int period)
{
        int n;

        if (period)
                for (n = 0; n <= NOTE_LAST; n++)
                        if (period >= (32 * period_table[n % 12] >> (n / 12 + 2)))
                                return n + 1;
        return NOTE_NONE;
}

void mod_import_note(const uint8_t p[4], song_note_t *note)
{
        note->note = _mod_period_to_note(((p[0] & 0xf) << 8) + p[1]);
        note->instrument = (p[0] & 0xf0) + (p[2] >> 4);
        note->voleffect = VOLFX_NONE;
        note->volparam = 0;
        note->effect = p[2] & 0xf;
        note->param = p[3];
}

/* --------------------------------------------------------------------------------------------------------- */

const uint8_t effect_weight[FX_MAX] = {
        [FX_PATTERNBREAK]       = 248,
        [FX_POSITIONJUMP]       = 240,
        [FX_SPEED]              = 232,
        [FX_TEMPO]              = 224,
        [FX_GLOBALVOLUME]       = 216,
        [FX_GLOBALVOLSLIDE]     = 208,
        [FX_CHANNELVOLUME]      = 200,
        [FX_CHANNELVOLSLIDE]    = 192,
        [FX_TONEPORTAVOL]       = 184,
        [FX_TONEPORTAMENTO]     = 176,
        [FX_ARPEGGIO]           = 168,
        [FX_RETRIG]             = 160,
        [FX_TREMOR]             = 152,
        [FX_OFFSET]             = 144,
        [FX_VOLUME]             = 136,
        [FX_VIBRATOVOL]         = 128,
        [FX_VOLUMESLIDE]        = 120,
        [FX_PORTAMENTODOWN]     = 112,
        [FX_PORTAMENTOUP]       = 104,
        [FX_NOTESLIDEDOWN]      =  96, // IMF Hxy
        [FX_NOTESLIDEUP]        =  88, // IMF Gxy
        [FX_PANNING]            =  80,
        [FX_PANNINGSLIDE]       =  72,
        [FX_MIDI]               =  64,
        [FX_S3MCMDEX]           =  56,
        [FX_PANBRELLO]          =  48,
        [FX_VIBRATO]            =  40,
        [FX_FINEVIBRATO]        =  32,
        [FX_TREMOLO]            =  24,
        [FX_KEYOFF]             =  16,
        [FX_SETENVPOSITION]     =   8,
        [FX_NONE]               =   0,
};

void swap_effects(song_note_t *note)
{
        song_note_t tmp = {
                .note = note->note,
                .instrument = note->instrument,
                .voleffect = note->effect,
                .volparam = note->param,
                .effect = note->voleffect,
                .param = note->volparam,
        };
        *note = tmp;
}

int convert_voleffect(uint8_t *e, uint8_t *p, int force)
{
        switch (*e) {
        case FX_NONE:
                return 1;
        case FX_VOLUME:
                *e = VOLFX_VOLUME;
                *p = MIN(*p, 64);
                break;
        case FX_PORTAMENTOUP:
                if (force)
                        *p = MIN(*p, 9);
                else if (*p > 9)
                        return 0;
                *e = VOLFX_PORTAUP;
                break;
        case FX_PORTAMENTODOWN:
                if (force)
                        *p = MIN(*p, 9);
                else if (*p > 9)
                        return 0;
                *e = VOLFX_PORTADOWN;
                break;
        case FX_TONEPORTAMENTO:
                if (*p >= 0xf0) {
                        // hack for people who can't type F twice :)
                        *e = VOLFX_TONEPORTAMENTO;
                        *p = 0xff;
                        return 1;
                }
                for (int n = 0; n < 10; n++) {
                        if (force
                            ? (*p <= vc_portamento_table[n])
                            : (*p == vc_portamento_table[n])) {
                                *e = VOLFX_TONEPORTAMENTO;
                                *p = n;
                                return 1;
                        }
                }
                return 0;
        case FX_VIBRATO:
                if (force)
                        *p = MIN(*p, 9);
                else if (*p > 9)
                        return 0;
                *e = VOLFX_VIBRATODEPTH;
                break;
        case FX_FINEVIBRATO:
                if (force)
                        *p = 0;
                else if (*p)
                        return 0;
                *e = VOLFX_VIBRATODEPTH;
                break;
        case FX_PANNING:
                *p = MIN(64, *p * 64 / 255);
                *e = VOLFX_PANNING;
                break;
        case FX_VOLUMESLIDE:
                // ugh
                // (IT doesn't even attempt to do this, presumably since it'd screw up the effect memory)
                if (*p == 0)
                        return 0;
                if ((*p & 0xf) == 0) { // Dx0 / Cx
                        if (force)
                                *p = MIN(*p >> 4, 9);
                        else if ((*p >> 4) > 9)
                                return 0;
                        else
                                *p >>= 4;
                        *e = VOLFX_VOLSLIDEUP;
                } else if ((*p & 0xf0) == 0) { // D0x / Dx
                        if (force)
                                *p = MIN(*p, 9);
                        else if (*p > 9)
                                return 0;
                        *e = VOLFX_VOLSLIDEDOWN;
                } else if ((*p & 0xf) == 0xf) { // DxF / Ax
                        if (force)
                                *p = MIN(*p >> 4, 9);
                        else if ((*p >> 4) > 9)
                                return 0;
                        else
                                *p >>= 4;
                        *e = VOLFX_FINEVOLUP;
                } else if ((*p & 0xf0) == 0xf0) { // DFx / Bx
                        if (force)
                                *p = MIN(*p, 9);
                        else if ((*p & 0xf) > 9)
                                return 0;
                        else
                                *p &= 0xf;
                        *e = VOLFX_FINEVOLDOWN;
                } else { // ???
                        return 0;
                }
                break;
        case FX_S3MCMDEX:
                switch (*p >> 4) {
                case 8:
                        /* Impulse Tracker imports XM volume-column panning very weirdly:
                                XM = P0 P1 P2 P3 P4 P5 P6 P7 P8 P9 PA PB PC PD PE PF
                                IT = 00 05 10 15 20 21 30 31 40 45 42 47 60 61 62 63
                        I'll be um, not duplicating that behavior. :) */
                        *e = VOLFX_PANNING;
                        *p = short_panning_table[*p & 0xf];
                        return 1;
                case 0: case 1: case 2: case 0xf:
                        if (force) {
                                *e = *p = 0;
                                return 1;
                        }
                        break;
                default:
                        break;
                }
                return 0;
        default:
                return 0;
        }
        return 1;
}


void read_lined_message(char *msg, slurp_t *fp, int len, int linelen)
{
        int msgsize = 0, linesize;

        while (len) {
                linesize = MIN(len, linelen);
                if (msgsize + linesize + 1 >= MAX_MESSAGE) {
                        /* Skip the rest */
                        slurp_seek(fp, len, SEEK_CUR);
                        break;
                }

                slurp_read(fp, msg, linesize);
                len -= linesize;

                msg[linesize] = '\0';
                linesize = rtrim_string(msg);
                msgsize += linesize + 1;
                msg += linesize;
                *msg++ = '\n';
        }
        *msg = '\0';
}

