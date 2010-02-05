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

#include "song.h"
#include "it.h"

#include <string.h>

void feature_check_instruments(const char *fmt,
                int limit, unsigned int flags_mask)
{
        unsigned int did_mask, m;
        int did_lim;
        song_instrument *s;
        int i;

        did_lim = 0;
        did_mask = 0;
        for (i = 1; i < SCHISM_MAX_INSTRUMENTS; i++) {
                if (song_instrument_is_empty(i)) continue;
                s = song_get_instrument(i);
                if (!s) continue;
                if (i > limit) {
                        if (!did_lim) {
                                did_lim++;
                                if (!limit) {
                                        did_mask |= (~0);
                                        log_appendf(4, "Warning: %s doesn't support instruments", fmt);
                                } else {
                                        log_appendf(4, "Warning: %s supports max %d instruments", fmt, limit);
                                }
                        }
                }
                m = (s->flags & ~(flags_mask));
                if (!m) continue;
                m &= (~did_mask);
                did_mask |= m;
                if (m & (ENV_VOLUME|ENV_VOLSUSTAIN|ENV_VOLLOOP|ENV_VOLCARRY)) {
                        log_appendf(4, "Warning: %s doesn't support volume envelopes", fmt);
                }
                if (m & (ENV_PANNING|ENV_PANSUSTAIN|ENV_PANLOOP|ENV_PANCARRY)) {
                        log_appendf(4, "Warning: %s doesn't support panning envelopes", fmt);
                }
                if (m & (ENV_PITCH|ENV_PITCHSUSTAIN|ENV_PITCHLOOP|ENV_PITCHCARRY)) {
                        log_appendf(4, "Warning: %s doesn't support pitch envelopes", fmt);
                }
                if (m & (ENV_SETPANNING)) {
                        log_appendf(4, "Warning: %s doesn't support default panning for instruments", fmt);
                }
                if (m & (ENV_FILTER)) {
                        log_appendf(4, "Warning: %s doesn't support filtered envelopes", fmt);
                }
        }
}
void feature_check_samples(const char *fmt, int limit, unsigned int flags_mask)
{
        unsigned int did_mask, m;
        int did_lim;
        song_sample *s;
        int i;

        did_lim = 0;
        did_mask = 0;
        for (i = 1; i < SCHISM_MAX_SAMPLES; i++) {
                if (song_sample_is_empty(i)) continue;
                s = song_get_sample(i);
                if (!s) continue;
                if (i > limit) {
                        if (!did_lim) {
                                did_lim++;
                                if (!limit) {
                                        did_mask |= (~0);
                                        log_appendf(4, "Warning: %s doesn't support samples", fmt);
                                } else {
                                        log_appendf(4, "Warning: %s supports max %d samples", fmt, limit);
                                }
                        }
                }
                if (s->global_volume != 64) s->flags |= SAMP_GLOBALVOL;

                m = (s->flags & ~(flags_mask));
                if (!m) continue;
                m &= (~did_mask);
                did_mask |= m;
                if (m & (SAMP_16_BIT)) {
                        log_appendf(4, "Warning: %s doesn't support 16-bit samples", fmt);
                }
                if (m & (SAMP_LOOP)) {
                        log_appendf(4, "Warning: %s doesn't support looping samples", fmt);
                } else if (m & (SAMP_LOOP_PINGPONG)) {
                        log_appendf(4, "Warning: %s doesn't support pingpong-looped samples", fmt);
                }
                if (m & (SAMP_SUSLOOP)) {
                        log_appendf(4, "Warning: %s doesn't support sustained looping samples", fmt);
                } else if (m & (SAMP_SUSLOOP_PINGPONG)) {
                        log_appendf(4, "Warning: %s doesn't support sustained, pingpong-looped samples", fmt);
                }
                if (m & (SAMP_PANNING)) {
                        log_appendf(4, "Warning: %s doesn't support a default panning for samples", fmt);
                }
                if (m & (SAMP_STEREO)) {
                        log_appendf(4, "Warning: %s doesn't support stereo samples", fmt);
                }
                if (m & (SAMP_ADLIB)) {
                        log_appendf(4, "Warning: %s doesn't support AdLib (YM3812) samples", fmt);
                }
                if (m & (SAMP_GLOBALVOL)) {
                        log_appendf(4, "Warning: %s doesn't support a global volume", fmt);
                }
        }
}
void feature_check_notes(const char *fmt,
                int min_note, int max_note,
                int min_inst,int max_inst,
                const char *volcmd, const char *fxcmd)
{
        song_note *n;
        int c, i, j, k, rows, pats;
        unsigned int did_limit_msg;
        int fx_limit[64];
        int vol_limit[32];
        char n1[4], n2[4];

        did_limit_msg = 0;
        pats = song_get_num_patterns();
        if (!fxcmd) fxcmd = ".";
        if (!volcmd) volcmd = ".";
        memset(fx_limit, 0, sizeof(fx_limit));
        for (i = 0; i < pats; i++) {
                rows = song_get_pattern(i, &n);
                if (!rows) continue;
                for (j = 0; j < rows; j++) {
                        for (k = 0; k < 64; k++) {
                                if (n->note && !(did_limit_msg & 1)) {
                                        if (n->note == NOTE_CUT) {
                                                if (!(did_limit_msg & 4) && !strchr(fxcmd, '1')) {
                                                        did_limit_msg |= 4;
                                                        log_appendf(4, "Warning: %s doesn't support note-cut ^^^", fmt);
                                                }
                                        } else if (n->note == NOTE_FADE) {
                                                if (!(did_limit_msg & 8) && !strchr(fxcmd, '~')) {
                                                        did_limit_msg |= 8;
                                                        log_appendf(4, "Warning: %s doesn't support note-fade ~~~", fmt);
                                                }
                                        } else if (n->note == NOTE_OFF) {
                                                if (!(did_limit_msg & 16) && !strchr(fxcmd, '`')) {
                                                        did_limit_msg |= 16;
                                                        log_appendf(4, "Warning: %s doesn't support note-off ===", fmt);
                                                }
                                        } else if (n->note < min_note || n->note > max_note) {
                                                did_limit_msg |= 1;
                                                log_appendf(4, "Warning: %s is limited to notes between %s and %s", fmt,
                                                                get_note_string(min_note, n1),
                                                                get_note_string(max_note, n2));
                                        }
                                }
                                if (n->instrument && !(did_limit_msg & 2)) {
                                        if (n->instrument < min_inst || n->instrument > max_inst) {
                                                did_limit_msg |= 2;
                                                log_appendf(4, "Warning: %s is limited to instrument numbers between %d and %d", fmt, min_inst, max_inst);
                                        }
                                }
                                if (n->effect) {
                                        c = get_effect_char(n->effect);
                                        if (!strchr(fxcmd, c)) {
                                                if (!fx_limit[ n->effect & 63 ]) {
                                                        log_appendf(4, "Warning: %s doesn't support effect %cxx", fmt, (int)c);
                                                        fx_limit[ n->effect & 63 ] |= 1;
                                                }
                                        }
                                }
                                if (n->volume_effect) {
                                        switch ((c=n->volume_effect)) {
                                        case VOL_EFFECT_VOLUME: c = 'v'; break;
                                        case VOL_EFFECT_PANNING: c = 'p'; break;
                                        default:
                                                get_volume_string(0, c, n1);
                                                c = n1[0];
                                                break;
                                        };
                                        if (!strchr(volcmd, c)) {
                                                if (!vol_limit[ n->volume_effect & 31 ]) {
                                                        if (c == 'v') {
                                                                log_appendf(4, "Warning: %s doesn't support the volume column", fmt);
                                                        } else if (c == 'p') {
                                                                log_appendf(4, "Warning: %s doesn't support the panning column", fmt);
                                                        } else {
                                                                log_appendf(4, "Warning: %s doesn't support volume-effect %cx", fmt, (int)c);
                                                        }
                                                        vol_limit[ n->volume_effect & 31 ] |= 1;
                                                }
                                        }
                                }
                                n++;
                        }
                }
        }
}
