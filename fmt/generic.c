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
                        if (period >= (32 * FreqS3MTable[n % 12] >> (n / 12 + 2)))
                                return n + 1;
        return NOTE_NONE;
}

void mod_import_note(const uint8_t p[4], MODCOMMAND *note)
{
        note->note = _mod_period_to_note(((p[0] & 0xf) << 8) + p[1]);
        note->instr = (p[0] & 0xf0) + (p[2] >> 4);
        note->volcmd = VOLCMD_NONE;
        note->vol = 0;
        note->command = p[2] & 0xf;
        note->param = p[3];
}

/* --------------------------------------------------------------------------------------------------------- */

const uint8_t effect_weight[CMD_MAX] = {
        [CMD_PATTERNBREAK]       = 248,
        [CMD_POSITIONJUMP]       = 240,
        [CMD_SPEED]              = 232,
        [CMD_TEMPO]              = 224,
        [CMD_GLOBALVOLUME]       = 216,
        [CMD_GLOBALVOLSLIDE]     = 208,
        [CMD_CHANNELVOLUME]      = 200,
        [CMD_CHANNELVOLSLIDE]    = 192,
        [CMD_TONEPORTAVOL]       = 184,
        [CMD_TONEPORTAMENTO]     = 176,
        [CMD_ARPEGGIO]           = 168,
        [CMD_RETRIG]             = 160,
        [CMD_TREMOR]             = 152,
        [CMD_OFFSET]             = 144,
        [CMD_VOLUME]             = 136,
        [CMD_VIBRATOVOL]         = 128,
        [CMD_VOLUMESLIDE]        = 120,
        [CMD_PORTAMENTODOWN]     = 112,
        [CMD_PORTAMENTOUP]       = 104,
        [CMD_NOTESLIDEDOWN]      =  96, // IMF Hxy
        [CMD_NOTESLIDEUP]        =  88, // IMF Gxy
        [CMD_PANNING]            =  80,
        [CMD_PANNINGSLIDE]       =  72,
        [CMD_MIDI]               =  64,
        [CMD_S3MCMDEX]           =  56,
        [CMD_PANBRELLO]          =  48,
        [CMD_VIBRATO]            =  40,
        [CMD_FINEVIBRATO]        =  32,
        [CMD_TREMOLO]            =  24,
        [CMD_KEYOFF]             =  16,
        [CMD_SETENVPOSITION]     =   8,
        [CMD_NONE]               =   0,
};

void swap_effects(MODCOMMAND *note)
{
        MODCOMMAND tmp = {
                .note = note->note,
                .instr = note->instr,
                .volcmd = note->command,
                .vol = note->param,
                .command = note->volcmd,
                .param = note->vol,
        };
        *note = tmp;
}

int convert_voleffect(uint8_t *e, uint8_t *p, int force)
{
        switch (*e) {
        case CMD_NONE:
                return 1;
        case CMD_VOLUME:
                *e = VOLCMD_VOLUME;
                *p = MIN(*p, 64);
                break;
        case CMD_PORTAMENTOUP:
                if (force)
                        *p = MIN(*p, 9);
                else if (*p > 9)
                        return 0;
                *e = VOLCMD_PORTAUP;
                break;
        case CMD_PORTAMENTODOWN:
                if (force)
                        *p = MIN(*p, 9);
                else if (*p > 9)
                        return 0;
                *e = VOLCMD_PORTADOWN;
                break;
        case CMD_TONEPORTAMENTO:
                if (*p >= 0xf0) {
                        // hack for people who can't type F twice :)
                        *e = VOLCMD_TONEPORTAMENTO;
                        *p = 0xff;
                        return 1;
                }
                for (int n = 0; n < 10; n++) {
                        if (force
                            ? (*p <= ImpulseTrackerPortaVolCmd[n])
                            : (*p == ImpulseTrackerPortaVolCmd[n])) {
                                *e = VOLCMD_TONEPORTAMENTO;
                                *p = n;
                                return 1;
                        }
                }
                return 0;
        case CMD_VIBRATO:
                if (force)
                        *p = MIN(*p, 9);
                else if (*p > 9)
                        return 0;
                *e = VOLCMD_VIBRATODEPTH;
                break;
        case CMD_FINEVIBRATO:
                if (force)
                        *p = 0;
                else if (*p)
                        return 0;
                *e = VOLCMD_VIBRATODEPTH;
                break;
        case CMD_PANNING:
                *p = MIN(64, *p * 64 / 255);
                *e = VOLCMD_PANNING;
                break;
        case CMD_VOLUMESLIDE:
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
                        *e = VOLCMD_VOLSLIDEUP;
                } else if ((*p & 0xf0) == 0) { // D0x / Dx
                        if (force)
                                *p = MIN(*p, 9);
                        else if (*p > 9)
                                return 0;
                        *e = VOLCMD_VOLSLIDEDOWN;
                } else if ((*p & 0xf) == 0xf) { // DxF / Ax
                        if (force)
                                *p = MIN(*p >> 4, 9);
                        else if ((*p >> 4) > 9)
                                return 0;
                        else
                                *p >>= 4;
                        *e = VOLCMD_FINEVOLUP;
                } else if ((*p & 0xf0) == 0xf0) { // DFx / Bx
                        if (force)
                                *p = MIN(*p, 9);
                        else if ((*p & 0xf) > 9)
                                return 0;
                        else
                                *p &= 0xf;
                        *e = VOLCMD_FINEVOLDOWN;
                } else { // ???
                        return 0;
                }
                break;
        case CMD_S3MCMDEX:
                switch (*p >> 4) {
                case 8:
                        /* Impulse Tracker imports XM volume-column panning very weirdly:
                                XM = P0 P1 P2 P3 P4 P5 P6 P7 P8 P9 PA PB PC PD PE PF
                                IT = 00 05 10 15 20 21 30 31 40 45 42 47 60 61 62 63
                        I'll be um, not duplicating that behavior. :) */
                        *e = VOLCMD_PANNING;
                        *p = SHORT_PANNING[*p & 0xf];
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

