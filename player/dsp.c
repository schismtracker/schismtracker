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

#include "sndfile.h"
#include "util.h" // for UNUSED
#include "cmixer.h"


void (*csf_midi_out_note)(int chan, const song_note_t *m) = NULL;
void (*csf_midi_out_raw)(const unsigned char *,unsigned int, unsigned int) = NULL;

////////////////////////////////////////////////////////////////////
// DSP Effects internal state

// Noise Reduction: simple low-pass filter
static int32_t left_nr = 0;
static int32_t right_nr = 0;


void csf_initialize_dsp(UNUSED song_t *csf, int reset)
{
        if (reset) {
                // Noise Reduction
                left_nr = right_nr = 0;
        }
}


void csf_process_stereo_dsp(UNUSED song_t *csf, int count)
{
        // Noise Reduction
        if (mix_flags & SNDMIX_NOISEREDUCTION) {
                int n1 = left_nr, n2 = right_nr;
                int *pnr = mix_buffer;

                for (int nr=count; nr; nr--) {
                        int vnr = pnr[0] >> 1;
                        pnr[0] = vnr + n1;
                        n1 = vnr;
                        vnr = pnr[1] >> 1;
                        pnr[1] = vnr + n2;
                        n2 = vnr;
                        pnr += 2;
                }

                left_nr = n1;
                right_nr = n2;
        }
}


void csf_process_mono_dsp(UNUSED song_t *csf, int count)
//----------------------------------------
{
        // Noise Reduction
        if (mix_flags & SNDMIX_NOISEREDUCTION) {
                int n = left_nr;
                int *pnr = mix_buffer;

                for (int nr = count; nr; pnr++, nr--) {
                        int vnr = *pnr >> 1;
                        *pnr = vnr + n;
                        n = vnr;
                }

                left_nr = n;
        }
}


/////////////////////////////////////////////////////////////////
// Clean DSP Effects interface

int csf_set_wave_config_ex(song_t *csf, int hqido, int bNR, int bEQ)
{
        uint32_t d = mix_flags & ~(SNDMIX_HQRESAMPLER | SNDMIX_NOISEREDUCTION | SNDMIX_EQ);
        if (hqido) d |= SNDMIX_HQRESAMPLER;
        if (bNR) d |= SNDMIX_NOISEREDUCTION;
        if (bEQ) d |= SNDMIX_EQ;

        mix_flags = d;
        csf_init_player(csf, 0);
        return 1;
}

