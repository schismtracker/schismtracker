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
#include "snd_fm.h"
#include "snd_gm.h"
#include "cmixer.h"

#include "util.h" /* for clamp */

// Volume ramp length, in 1/10 ms
#define VOLUMERAMPLEN   146 // 1.46ms = 64 samples at 44.1kHz

// VU meter
#define VUMETER_DECAY 16

// SNDMIX: These are global flags for playback control
unsigned int max_voices = 32; // ITT it is 1994
// Mixing Configuration (SetWaveConfig)
uint32_t mix_channels = 1;
uint32_t mix_flags = 0;
uint32_t mix_frequency = 44100;
uint32_t mix_bits_per_sample = 16;
// Mixing data initialized in
static unsigned int volume_ramp_samples = 64;
unsigned int global_vu_left = 0;
unsigned int global_vu_right = 0;
int32_t g_dry_rofs_vol = 0;
int32_t g_dry_lofs_vol = 0;

typedef uint32_t (* LPCONVERTPROC)(void *, int *, uint32_t, int *, int *);

extern void interleave_front_rear(int *, int *, unsigned int);
extern void mono_from_stereo(int *, unsigned int);
extern void stereo_fill(int *, unsigned int, int *, int *);

extern int mix_buffer[MIXBUFFERSIZE*4];
extern int MixRearBuffer[MIXBUFFERSIZE*2];



// The volume we have here is in range 0..(63*255) (0..16065)
// We should keep that range, but convert it into a logarithmic
// one such that a change of 256*8 (2048) corresponds to a halving
// of the volume.
//   logvolume = 2^(linvolume / (4096/8)) * (4096/64)
// However, because the resolution of MIDI volumes
// is merely 128 units, we can use a lookup table.
//
// In this table, each value signifies the minimum value
// that volume must be in order for the result to be
// that table index.
static const unsigned short GMvolTransition[128] =
{
    0, 2031, 4039, 5214, 6048, 6694, 7222, 7669,
 8056, 8397, 8702, 8978, 9230, 9462, 9677, 9877,
10064,10239,10405,10562,10710,10852,10986,11115,
11239,11357,11470,11580,11685,11787,11885,11980,
12072,12161,12248,12332,12413,12493,12570,12645,
12718,12790,12860,12928,12995,13060,13123,13186,
13247,13306,13365,13422,13479,13534,13588,13641,
13693,13745,13795,13844,13893,13941,13988,14034,
14080,14125,14169,14213,14256,14298,14340,14381,
14421,14461,14501,14540,14578,14616,14653,14690,
14727,14763,14798,14833,14868,14902,14936,14970,
15003,15035,15068,15100,15131,15163,15194,15224,
15255,15285,15315,15344,15373,15402,15430,15459,
15487,15514,15542,15569,15596,15623,15649,15675,
15701,15727,15753,15778,15803,15828,15853,15877,
15901,15925,15949,15973,15996,16020,16043,16065,
};


// We use binary search to find the right slot
// with at most 7 comparisons.
static unsigned int find_volume(unsigned short vol)
{
        unsigned int l = 0, r = 128;

        while (l < r) {
                unsigned int m = l + ((r - l) / 2);
                unsigned short p = GMvolTransition[m];

                if (p < vol)
                        l = m + 1;
                else
                        r = m;
        }

        return l;
}


////////////////////////////////////////////////////////////////////////////////////////////
//
// XXX * I prefixed these with `rn_' to avoid any namespace conflicts
// XXX   Needs better naming!
// XXX * Keep inline?
// XXX * Get rid of the pointer passing where it is not needed
//

static inline void rn_tremolo(song_t *csf, song_voice_t *chan, int *vol)
{
        unsigned int trempos = chan->tremolo_position & 0xFF;
        int tdelta;

        const int tremattn = 5;

        switch (chan->tremolo_type & 0x03) {
        default:
                tdelta = sine_table[trempos];
                break;
        case 1:
                tdelta = ramp_down_table[trempos];
                break;
        case 2:
                tdelta = square_table[trempos];
                break;
        case 3:
                tdelta = 128 * ((double) rand() / RAND_MAX) - 64;
                break;
        }
        *vol += (tdelta * (int)chan->tremolo_depth) >> tremattn;

        // handle on tick-N, or all ticks if not in old-effects mode
        if (!(csf->flags & SONG_FIRSTTICK) || !(csf->flags & SONG_ITOLDEFFECTS)) {
                chan->tremolo_position = (trempos + 4 * chan->tremolo_speed) & 0xFF;
        }
}


static inline void rn_tremor(song_voice_t *chan, int *vol)
{
        if ((chan->cd_tremor & 192) == 128)
                *vol = 0;

        chan->flags |= CHN_FASTVOLRAMP;
}


static inline void rn_panbrello(song_voice_t *chan)
{
        unsigned int panpos = chan->panbrello_position & 0xFF;
        int pdelta;

        switch (chan->panbrello_type & 0x03) {
        default:
                pdelta = sine_table[panpos];
                break;
        case 1:
                pdelta = ramp_down_table[panpos];
                break;
        case 2:
                pdelta = square_table[panpos];
                break;
        case 3:
                pdelta = 128 * ((double) rand() / RAND_MAX) - 64;
                break;
        }

        chan->panbrello_position += chan->panbrello_speed;
        pdelta = ((pdelta * (int)chan->panbrello_depth) + 2) >> 3;
        pdelta += chan->final_panning;
        chan->final_panning = CLAMP(pdelta, 0, 256);
}


static inline void rn_vibrato(song_t *csf, song_voice_t *chan, int *nperiod)
{
        unsigned int vibpos = chan->vibrato_position & 0xFF;
        int vdelta;
        unsigned int vdepth;
        int period = *nperiod;

        switch (chan->vib_type & 0x03) {
        default:
                vdelta = sine_table[vibpos];
                break;
        case 1:
                vdelta = ramp_down_table[vibpos];
                break;
        case 2:
                vdelta = square_table[vibpos];
                break;
        case 3:
                vdelta = 128 * ((double) rand() / RAND_MAX) - 64;
                break;
        }

        if (csf->flags & SONG_ITOLDEFFECTS) {
                vdepth = 5;
                vdelta = -vdelta; // yes, IT does vibrato backwards in old-effects mode. try it.
        } else {
                vdepth = 6;
        }
        vdelta = (vdelta * (int)chan->vibrato_depth) >> vdepth;

        if (csf->flags & SONG_LINEARSLIDES) {
                int l = abs(vdelta);

                if (vdelta < 0) {
                        vdelta = _muldiv(period, linear_slide_down_table[l >> 2], 0x10000) - period;

                        if (l & 0x03)
                                vdelta += _muldiv(period, fine_linear_slide_down_table[l & 0x03], 0x10000) - period;
                } else {
                        vdelta = _muldiv(period, linear_slide_up_table[l >> 2], 0x10000) - period;

                        if (l & 0x03)
                                vdelta += _muldiv(period, fine_linear_slide_up_table[l & 0x03], 0x10000) - period;
                }
        }

        period -= vdelta;

        // handle on tick-N, or all ticks if not in old-effects mode
        if (!(csf->flags & SONG_FIRSTTICK) || !(csf->flags & SONG_ITOLDEFFECTS)) {
                chan->vibrato_position = (vibpos + 4 * chan->vibrato_speed) & 0xFF;
        }

        *nperiod = period;
}

static inline void rn_instrument_vibrato(song_t *csf, song_voice_t *chan, int *nperiod, int *nperiodfrac)
{
        int period = *nperiod;
        int periodfrac = *nperiodfrac;
        song_sample_t *pins = chan->ptr_sample;

        /* this isn't correct, but it's better... [original was without int cast] */
        if (!pins->vib_rate) {
                chan->autovib_depth = pins->vib_depth << 8;
        } else {
                chan->autovib_depth += pins->vib_rate;

                if ((chan->autovib_depth >> 8) > (int) pins->vib_depth)
                        chan->autovib_depth = pins->vib_depth << 8;
        }

        chan->autovib_position += (int) pins->vib_speed;

        int val;

        // XXX why is this so completely different from the other vibrato code?
        switch(pins->vib_type) {
        case VIB_SINE:
        default:
                val = ft2_vibrato_table[chan->autovib_position & 255];
                break;
        case VIB_RAMP_DOWN:
                val = ((0x40 - (chan->autovib_position >> 1)) & 0x7F) - 0x40;
                break;
        case VIB_SQUARE:
                val = (chan->autovib_position & 128) ? +64 : -64;
                break;
        case VIB_RANDOM:
                val = 128 * ((double) rand() / RAND_MAX) - 64;
                break;
        }

        int n = ((val * chan->autovib_depth) >> 8);

        // is this right? -mrsb
        if (!(csf->flags & SONG_ITOLDEFFECTS))
                n >>= 1;

        int df1, df2;

        if (n < 0) {
                n = -n;
                unsigned int n1 = n >> 8;
                df1 = linear_slide_up_table[n1];
                df2 = linear_slide_up_table[n1 + 1];
        }
        else {
                unsigned int n1 = n >> 8;
                df1 = linear_slide_down_table[n1];
                df2 = linear_slide_down_table[n1 + 1];
        }

        n >>= 2;
        period = _muldiv(period, df1 + ((df2 - df1) * (n & 0x3F) >> 6), 256);
        periodfrac = period & 0xFF;
        period >>= 8;

        *nperiod = period;
        *nperiodfrac = periodfrac;
}


static inline void rn_process_envelope(song_voice_t *chan, int *nvol)
{
        song_instrument_t *penv = chan->ptr_instrument;
        int vol = *nvol;

        // Volume Envelope
        if (chan->flags & CHN_VOLENV && penv->vol_env.nodes) {
                int envpos = chan->vol_env_position;
                unsigned int pt = penv->vol_env.nodes - 1;

                for (unsigned int i = 0; i < (unsigned int)(penv->vol_env.nodes - 1); i++) {
                        if (envpos <= penv->vol_env.ticks[i]) {
                                pt = i;
                                break;
                        }
                }

                int x2 = penv->vol_env.ticks[pt];
                int x1, envvol;

                if (envpos >= x2) {
                        envvol = penv->vol_env.values[pt] << 2;
                        x1 = x2;
                } else if (pt) {
                        envvol = penv->vol_env.values[pt-1] << 2;
                        x1 = penv->vol_env.ticks[pt-1];
                } else {
                        envvol = 0;
                        x1 = 0;
                }

                if (envpos > x2)
                        envpos = x2;

                if (x2 > x1 && envpos > x1) {
                        envvol += ((envpos - x1) * (((int)penv->vol_env.values[pt]<<2) - envvol)) / (x2 - x1);
                }

                envvol = CLAMP(envvol, 0, 256);
                vol = (vol * envvol) >> 8;
        }

        // Panning Envelope
        if ((chan->flags & CHN_PANENV) && (penv->pan_env.nodes)) {
                int envpos = chan->pan_env_position;
                unsigned int pt = penv->pan_env.nodes - 1;

                for (unsigned int i=0; i<(unsigned int)(penv->pan_env.nodes-1); i++) {
                        if (envpos <= penv->pan_env.ticks[i]) {
                                pt = i;
                                break;
                        }
                }

                int x2 = penv->pan_env.ticks[pt], y2 = penv->pan_env.values[pt];
                int x1, envpan;

                if (envpos >= x2) {
                        envpan = y2;
                        x1 = x2;
                } else if (pt) {
                        envpan = penv->pan_env.values[pt-1];
                        x1 = penv->pan_env.ticks[pt-1];
                } else {
                        envpan = 128;
                        x1 = 0;
                }

                if (x2 > x1 && envpos > x1) {
                        envpan += ((envpos - x1) * (y2 - envpan)) / (x2 - x1);
                }

                envpan = CLAMP(envpan, 0, 64);

                int pan = chan->panning;

                if (pan >= 128) {
                        pan += ((envpan - 32) * (256 - pan)) / 32;
                } else {
                        pan += ((envpan - 32) * (pan)) / 32;
                }

                chan->final_panning = CLAMP(pan, 0, 256);
        }

        // FadeOut volume
        if (chan->flags & CHN_NOTEFADE) {
                unsigned int fadeout = penv->fadeout;

                if (fadeout) {
                        chan->fadeout_volume -= fadeout << 1;

                        if (chan->fadeout_volume <= 0)
                                chan->fadeout_volume = 0;

                        vol = (vol * chan->fadeout_volume) >> 16;
                } else if (!chan->fadeout_volume) {
                        vol = 0;
                }
        }

        // Pitch/Pan separation
        if (penv->pitch_pan_separation && chan->final_panning && chan->note) {
                // PPS value is 1/512, i.e. PPS=1 will adjust by 8/512 = 1/64 for each 8 semitones
                // with PPS = 32 / PPC = C-5, E-6 will pan hard right (and D#6 will not)
                int pandelta = (int) chan->final_panning
                             + (int) ((int) (chan->note - penv->pitch_pan_center - 1) * (int) penv->pitch_pan_separation) / (int) 4;
                chan->final_panning = CLAMP(pandelta, 0, 256);
        }

        *nvol = vol;
}


static inline int rn_arpeggio(song_t *csf, song_voice_t *chan, int period)
{
        int a;
        switch ((csf->current_speed - csf->tick_count) % 3) {
        case 1:
                a = chan->mem_arpeggio >> 4;
                break;
        case 2:
                a = chan->mem_arpeggio & 0xf;
                break;
        default:
                a = 0;
        }
        if (!a)
                return period;

        //return get_period_from_note(a + get_note_from_period(period), 8363, 0);
        return get_freq_from_period(calc_halftone(get_freq_from_period(period, 8363, 0, 0), a), 8363, 0, 0);
}


static inline void rn_pitch_filter_envelope(song_voice_t *chan, int *nenvpitch, int *nperiod)
{
        song_instrument_t *penv = chan->ptr_instrument;
        int envpos = chan->pitch_env_position;
        unsigned int pt = penv->pitch_env.nodes - 1;
        int period = *nperiod;
        int envpitch = *nenvpitch;

        for (unsigned int i = 0; i < (unsigned int)(penv->pitch_env.nodes - 1); i++) {
                if (envpos <= penv->pitch_env.ticks[i]) {
                        pt = i;
                        break;
                }
        }

        int x2 = penv->pitch_env.ticks[pt];
        int x1;

        if (envpos >= x2) {
                envpitch = (((int)penv->pitch_env.values[pt]) - 32) * 8;
                x1 = x2;
        } else if (pt) {
                envpitch = (((int)penv->pitch_env.values[pt - 1]) - 32) * 8;
                x1 = penv->pitch_env.ticks[pt - 1];
        } else {
                envpitch = 0;
                x1 = 0;
        }

        if (envpos > x2)
                envpos = x2;

        if (x2 > x1 && envpos > x1) {
                int envpitchdest = (((int)penv->pitch_env.values[pt]) - 32) * 8;
                envpitch += ((envpos - x1) * (envpitchdest - envpitch)) / (x2 - x1);
        }

        // clamp to -255/255?
        envpitch = CLAMP(envpitch, -256, 256);

        // Pitch Envelope
        if (!(penv->flags & ENV_FILTER)) {
                int l = abs(envpitch);

                if (l > 255)
                        l = 255;

                period = _muldiv(period, (envpitch < 0 ?
                        linear_slide_up_table : linear_slide_down_table)[l], 0x10000);
        }

        *nperiod = period;
        *nenvpitch = envpitch;
}


static inline void rn_increment_env_pos(song_voice_t *chan)
{
        song_instrument_t *penv = chan->ptr_instrument;

        if (chan->flags & CHN_VOLENV) {
                chan->vol_env_position++;

                if (penv->flags & ENV_VOLLOOP) {
                        int volloopend = penv->vol_env.ticks[penv->vol_env.loop_end] + 1;

                        if (chan->vol_env_position == volloopend) {
                                chan->vol_env_position = penv->vol_env.ticks[penv->vol_env.loop_start];
                                if (penv->vol_env.loop_end == penv->vol_env.loop_start
                                    && !penv->vol_env.values[penv->vol_env.loop_start]) {
                                        chan->flags |= CHN_NOTEFADE;
                                        chan->fadeout_volume = 0;
                                }
                        }
                }

                if (penv->flags & ENV_VOLSUSTAIN
                    && (chan->vol_env_position == (int)penv->vol_env.ticks[penv->vol_env.sustain_end] + 1)
                    && !(chan->flags & CHN_KEYOFF)) {
                        // Volume sustained
                        chan->vol_env_position = penv->vol_env.ticks[penv->vol_env.sustain_start];
                } else if (chan->vol_env_position > penv->vol_env.ticks[penv->vol_env.nodes - 1]) {
                        // End of Envelope
                        chan->vol_env_position = penv->vol_env.ticks[penv->vol_env.nodes - 1];
                        chan->flags |= CHN_NOTEFADE;
                        if (!penv->vol_env.values[penv->vol_env.nodes-1]) {
                                chan->fadeout_volume = 0;
                                chan->final_volume = 0;
                        }
                }
        }

        if (chan->flags & CHN_PANENV) {
                chan->pan_env_position++;

                if (penv->flags & ENV_PANLOOP) {
                        int panloopend = penv->pan_env.ticks[penv->pan_env.loop_end] + 1;

                        if (chan->pan_env_position == panloopend) {
                                chan->pan_env_position = penv->pan_env.ticks[penv->pan_env.loop_start];
                        }
                }

                if (penv->flags & ENV_PANSUSTAIN
                    && (chan->pan_env_position == (int) penv->pan_env.ticks[penv->pan_env.sustain_end] + 1)
                    && !(chan->flags & CHN_KEYOFF)) {
                        // Panning sustained
                        chan->pan_env_position = penv->pan_env.ticks[penv->pan_env.sustain_start];
                } else if (chan->pan_env_position > penv->pan_env.ticks[penv->pan_env.nodes - 1]) {
                        // End of envelope
                        chan->pan_env_position = penv->pan_env.ticks[penv->pan_env.nodes - 1];
                }
        }

        if (chan->flags & CHN_PITCHENV) {
                chan->pitch_env_position++;

                if (penv->flags & ENV_PITCHLOOP) {
                        int pitchloopend = penv->pitch_env.ticks[penv->pitch_env.loop_end] + 1;

                        if (chan->pitch_env_position == pitchloopend) {
                                chan->pitch_env_position = penv->pitch_env.ticks[penv->pitch_env.loop_start];
                        }
                }

                if (penv->flags & ENV_PITCHSUSTAIN
                    && (chan->pitch_env_position == (int) penv->pitch_env.ticks[penv->pitch_env.sustain_end]+1)
                    && !(chan->flags & CHN_KEYOFF)) {
                        // Pitch sustained
                        chan->pitch_env_position = penv->pitch_env.ticks[penv->pitch_env.sustain_start];
                } else if (chan->pitch_env_position > penv->pitch_env.ticks[penv->pitch_env.nodes - 1]) {
                        // End of envelope
                        chan->pitch_env_position = penv->pitch_env.ticks[penv->pitch_env.nodes - 1];
                }
        }
}


static inline int rn_update_sample(song_t *csf, song_voice_t *chan, int nchan, int master_vol)
{
        // Adjusting volumes
        if (mix_channels >= 2) {
                int pan = ((int) chan->final_panning) - 128;
                pan *= (int) csf->pan_separation;
                pan /= 128;

                if ((csf->flags & SONG_INSTRUMENTMODE)
                    && chan->ptr_instrument
                    && chan->ptr_instrument->midi_channel_mask > 0)
                        GM_Pan(nchan, pan);

                if (csf->flags & SONG_NOSTEREO) {
                        pan = 128;
                } else {
                        pan += 128;
                        pan = CLAMP(pan, 0, 256);

                        if (mix_flags & SNDMIX_REVERSESTEREO)
                                pan = 256 - pan;
                }

                int realvol = (chan->final_volume * master_vol) >> (8 - 1);

                chan->left_volume_new  = (realvol * pan) >> 8;
                chan->right_volume_new = (realvol * (256 - pan)) >> 8;
        } else {
                chan->right_volume_new = (chan->final_volume * master_vol) >> 8;
                chan->left_volume_new = chan->right_volume_new;
        }

        // Clipping volumes
        if (chan->right_volume_new > 0xFFFF)
                chan->right_volume_new = 0xFFFF;

        if (chan->left_volume_new  > 0xFFFF)
                chan->left_volume_new  = 0xFFFF;

        // Check IDO
        if (mix_flags & SNDMIX_NORESAMPLING) {
                chan->flags &= ~(CHN_HQSRC);
                chan->flags |= CHN_NOIDO;
        } else {
                chan->flags &= ~(CHN_NOIDO | CHN_HQSRC);

                if (chan->increment == 0x10000) {
                        chan->flags |= CHN_NOIDO;
                } else {
                        if (!(mix_flags & SNDMIX_HQRESAMPLER) &&
                            !(mix_flags & SNDMIX_ULTRAHQSRCMODE)) {
                                if (chan->increment >= 0xFF00)
                                        chan->flags |= CHN_NOIDO;
                        }
                }
        }

        chan->right_volume_new >>= MIXING_ATTENUATION;
        chan->left_volume_new  >>= MIXING_ATTENUATION;
        chan->right_ramp =
        chan->left_ramp  = 0;

        // Dolby Pro-Logic Surround (S91)
        if (chan->flags & CHN_SURROUND && mix_channels <= 2
            && !(mix_flags & SNDMIX_NOSURROUND)
            && !(csf->flags & SONG_NOSTEREO))
                chan->left_volume_new = -chan->left_volume_new;

        // Checking Ping-Pong Loops
        if (chan->flags & CHN_PINGPONGFLAG)
                chan->increment = -chan->increment;

        if (chan->flags & CHN_MUTE) {
                chan->left_volume = chan->right_volume = 0;
        } else if (!(mix_flags & SNDMIX_NORAMPING) &&
            chan->flags & CHN_VOLUMERAMP &&
            (chan->right_volume != chan->right_volume_new ||
             chan->left_volume  != chan->left_volume_new)) {
                // Setting up volume ramp
                int ramp_length = volume_ramp_samples;
                int right_delta = ((chan->right_volume_new - chan->right_volume) << VOLUMERAMPPRECISION);
                int left_delta  = ((chan->left_volume_new  - chan->left_volume)  << VOLUMERAMPPRECISION);

                if (mix_flags & SNDMIX_HQRESAMPLER) {
                        if (chan->right_volume | chan->left_volume &&
                            chan->right_volume_new | chan->left_volume_new &&
                            !(chan->flags & CHN_FASTVOLRAMP)) {
                                ramp_length = csf->buffer_count;

                                int l = (1 << (VOLUMERAMPPRECISION - 1));
                                int r =(int) volume_ramp_samples;

                                ramp_length = CLAMP(ramp_length, l, r);
                        }
                }

                chan->right_ramp = right_delta / ramp_length;
                chan->left_ramp = left_delta / ramp_length;
                chan->right_volume = chan->right_volume_new - ((chan->right_ramp * ramp_length) >> VOLUMERAMPPRECISION);
                chan->left_volume = chan->left_volume_new - ((chan->left_ramp * ramp_length) >> VOLUMERAMPPRECISION);

                if (chan->right_ramp | chan->left_ramp) {
                        chan->ramp_length = ramp_length;
                } else {
                        chan->flags &= ~CHN_VOLUMERAMP;
                        chan->right_volume = chan->right_volume_new;
                        chan->left_volume  = chan->left_volume_new;
                }
        } else {
                chan->flags  &= ~CHN_VOLUMERAMP;
                chan->right_volume = chan->right_volume_new;
                chan->left_volume  = chan->left_volume_new;
        }

        chan->right_ramp_volume = chan->right_volume << VOLUMERAMPPRECISION;
        chan->left_ramp_volume = chan->left_volume << VOLUMERAMPPRECISION;

        // Adding the channel in the channel list
        csf->voice_mix[csf->num_voices++] = nchan;

        if (csf->num_voices >= MAX_VOICES)
                return 0;

        return 1;
}


// XXX Rename this
static inline void rn_gen_key(song_t *csf, song_voice_t *chan, int chan_num, int freq, int vol)
{
        if (chan->flags & CHN_MUTE) {
                // don't do anything
                return;
        } else if (csf->flags & SONG_INSTRUMENTMODE &&
            chan->ptr_instrument &&
            chan->ptr_instrument->midi_channel_mask > 0) {
                MidiBendMode BendMode = MIDI_BEND_NORMAL;
                /* TODO: If we're expecting a large bend exclusively
                 * in either direction, update BendMode to indicate so.
                 * This can be used to extend the range of MIDI pitch bending.
                 */

                // Vol maximum is 64*64 here. (4096)
                int volume = vol;

                if (chan->flags & CHN_ADLIB && volume > 0) {
                        // This gives a value in the range 0..127.
                        //int o = volume;
                        volume = find_volume((unsigned short) volume) * chan->instrument_volume / 64;
                        //fprintf(stderr, "%d -> %d[%d]\n", o, volume, chan->instrument_volume);
                } else {
                        // This gives a value in the range 0..127.
                        volume = volume * chan->instrument_volume / 8192;
                }

                GM_SetFreqAndVol(chan_num, freq, volume, BendMode, chan->flags & CHN_KEYOFF);
        }
        if (chan->flags & CHN_ADLIB) {
                // For some reason, scaling by about (2*3)/(8200/8300) is needed
                // to get a frequency that matches with ST3.
                int oplfreq = freq * 164 / 249;

                OPL_HertzTouch(chan_num, oplfreq, chan->flags & CHN_KEYOFF);

                // ST32 ignores global & master volume in adlib mode, guess we should do the same -Bisqwit
                OPL_Touch(chan_num, NULL, vol * chan->instrument_volume * 63 / (1 << 20));
        }
}


static inline void update_vu_meter(song_voice_t *chan)
{
        // Update VU-Meter (final_volume is 14-bit)
        // TODO: missing background channels by doing it this way.
        // need to use nMasterCh, add the vu meters for each physical voice, and bit shift.
        uint32_t vutmp = chan->final_volume >> (14 - 8);
        if (vutmp > 0xFF) vutmp = 0xFF;
        if (chan->flags & CHN_ADLIB) {
                // fake VU decay (intentionally similar to ST3)
                if (chan->vu_meter > VUMETER_DECAY) {
                        chan->vu_meter -= VUMETER_DECAY;
                } else {
                        chan->vu_meter = 0;
                }
                if (chan->vu_meter >= 0x100) {
                        chan->vu_meter = vutmp;
                }
        } else if (vutmp && chan->current_sample_data) {
                // can't fake the funk
                int n;
                int pos = chan->position; // necessary on 64-bit systems (sometimes pos == -1, weird)
                if (chan->flags & CHN_16BIT) {
                        const signed short *p = (signed short *)(chan->current_sample_data);
                        if (chan->flags & CHN_STEREO)
                                n = p[2 * pos];
                        else
                                n = p[pos];
                        n >>= 8;
                } else {
                        const signed char *p = (signed char *)(chan->current_sample_data);
                        if (chan->flags & CHN_STEREO)
                                n = p[2 * pos];
                        else
                                n = p[pos];
                }
                if (n < 0)
                        n = -n;
                vutmp *= n;
                vutmp >>= 7; // 0..255
                if (vutmp)
                        chan->vu_meter = vutmp;
        } else {
                chan->vu_meter = 0;
        }
}

////////////////////////////////////////////////////////////////////////////////////////////

int csf_init_player(song_t *csf, int reset)
{
        if (max_voices > MAX_VOICES)
                max_voices = MAX_VOICES;

        mix_frequency = CLAMP(mix_frequency, 4000, MAX_SAMPLE_RATE);
        volume_ramp_samples = (mix_frequency * VOLUMERAMPLEN) / 100000;

        if (volume_ramp_samples < 8)
                volume_ramp_samples = 8;

        if (mix_flags & SNDMIX_NORAMPING)
                volume_ramp_samples = 2;

        g_dry_rofs_vol = g_dry_lofs_vol = 0;

        if (reset) {
                global_vu_left  = 0;
                global_vu_right = 0;
        }

        csf_initialize_dsp(csf, reset);
        initialize_eq(reset, mix_frequency);

        Fmdrv_Init(mix_frequency);
        OPL_Reset();
        GM_Reset(0);
        return 1;
}


unsigned int csf_read(song_t *csf, void * v_buffer, unsigned int bufsize)
{
        uint8_t * buffer = (uint8_t *)v_buffer;
        LPCONVERTPROC convert_func = clip_32_to_8;
        int32_t vu_min[2];
        int32_t vu_max[2];
        unsigned int bufleft, max, sample_size, count, smpcount, mix_stat=0;

        vu_min[0] = vu_min[1] = 0x7FFFFFFF;
        vu_max[0] = vu_max[1] = -0x7FFFFFFF;


        csf->mix_stat = 0;
        sample_size = mix_channels;

             if (mix_bits_per_sample == 16) { sample_size *= 2; convert_func = clip_32_to_16; }
        else if (mix_bits_per_sample == 24) { sample_size *= 3; convert_func = clip_32_to_24; }
        else if (mix_bits_per_sample == 32) { sample_size *= 4; convert_func = clip_32_to_32; }

        max = bufsize / sample_size;

        if (!max || !buffer)
                return 0;

        bufleft = max;

        if (csf->flags & SONG_ENDREACHED)
                bufleft = 0; // skip the loop

        while (bufleft > 0) {
                // Update Channel Data
                unsigned int total_smpcount;

                if (!csf->buffer_count) {
                        if (!(mix_flags & SNDMIX_DIRECTTODISK))
                                csf->buffer_count = bufleft;

                        if (!csf_read_note(csf)) {
                                csf->flags |= SONG_ENDREACHED;

                                if (csf->stop_at_order > -1)
                                        return 0; /* faster */

                                if (bufleft == max)
                                        break;

                                if (!(mix_flags & SNDMIX_DIRECTTODISK))
                                        csf->buffer_count = bufleft;
                        }

                        if (!csf->buffer_count)
                                break;
                }

                count = csf->buffer_count;

                if (count > MIXBUFFERSIZE)
                        count = MIXBUFFERSIZE;

                if (count > bufleft)
                        count = bufleft;

                if (!count)
                        break;

                smpcount = count;

                // Resetting sound buffer
                stereo_fill(mix_buffer, smpcount, &g_dry_rofs_vol, &g_dry_lofs_vol);

                if (mix_channels >= 2) {
                        smpcount *= 2;
                        csf->mix_stat += csf_create_stereo_mix(csf, count);
                        csf_process_stereo_dsp(csf, count);
                }
                else {
                        csf->mix_stat += csf_create_stereo_mix(csf, count);
                        mono_from_stereo(mix_buffer, count);
                        csf_process_mono_dsp(csf, count);
                }

                if (mix_flags & SNDMIX_EQ) {
                        if (mix_channels >= 2)
                                eq_stereo(mix_buffer, count);
                        else
                                eq_mono(mix_buffer, count);
                }

                mix_stat++;

                total_smpcount = smpcount;

                // Perform clipping + VU-Meter
                buffer += convert_func(buffer, mix_buffer, total_smpcount, vu_min, vu_max);

                // Buffer ready
                bufleft -= count;
                csf->buffer_count -= count;
        }

        if (bufleft)
                memset(buffer, (mix_bits_per_sample == 8) ? 0x80 : 0, bufleft * sample_size);

        // VU-Meter
        vu_min[0] >>= 18;
        vu_min[1] >>= 18;
        vu_max[0] >>= 18;
        vu_max[1] >>= 18;

        if (vu_max[0] < vu_min[0])
                vu_max[0] = vu_min[0];

        if (vu_max[1] < vu_min[1])
                vu_max[1] = vu_min[1];

        if ((global_vu_left = (unsigned int)(vu_max[0] - vu_min[0])) > 0xFF)
                global_vu_left = 0xFF;

        if ((global_vu_right = (unsigned int)(vu_max[1] - vu_min[1])) > 0xFF)
                global_vu_right = 0xFF;

        if (mix_stat) {
                csf->mix_stat += mix_stat - 1;
                csf->mix_stat /= mix_stat;
        }

        return max - bufleft;
}



/////////////////////////////////////////////////////////////////////////////
// Handles navigation/effects

static int increment_row(song_t *csf)
{
        csf->process_row = csf->break_row; /* [ProcessRow = BreakRow] */
        csf->break_row = 0;                  /* [BreakRow = 0] */

        /* some ugly copypasta, this should be less dumb */
        if (csf->flags & SONG_PATTERNPLAYBACK) {
                if (csf->repeat_count > 0)
                        csf->repeat_count--;
                if (!csf->repeat_count) {
                        csf->process_row = PROCESS_NEXT_ORDER;
                        return 0;
                }
        } else if (!(csf->flags & SONG_ORDERLOCKED)) {
                if (csf->locked_order < MAX_ORDERS) {
                        csf->process_order = csf->locked_order - 1;
                        csf->locked_order = MAX_ORDERS;
                }

                /* [Increase ProcessOrder] */
                /* [while Order[ProcessOrder] = 0xFEh, increase ProcessOrder] */
                do {
                        csf->process_order++;
                } while (csf->orderlist[csf->process_order] == ORDER_SKIP);

                /* [if Order[ProcessOrder] = 0xFFh, ProcessOrder = 0] (... or just stop playing) */
                if (csf->orderlist[csf->process_order] == ORDER_LAST) {
                        if (csf->repeat_count > 0)
                                csf->repeat_count--;
                        if (!csf->repeat_count) {
                                csf->process_row = PROCESS_NEXT_ORDER;
                                return 0;
                        }

                        csf->process_order = 0;
                        while (csf->orderlist[csf->process_order] == ORDER_SKIP)
                                csf->process_order++;
                }
                if (csf->orderlist[csf->process_order] >= MAX_PATTERNS) {
                        // what the butt?
                        csf->process_row = PROCESS_NEXT_ORDER;
                        return 0;
                }

                /* [CurrentPattern = Order[ProcessOrder]] */
                csf->current_order = csf->process_order;
                csf->current_pattern = csf->orderlist[csf->process_order];
        }

        if (!csf->pattern_size[csf->current_pattern] || !csf->patterns[csf->current_pattern]) {
                /* okay, this is wrong. allocate the pattern _NOW_ */
                csf->patterns[csf->current_pattern] = csf_allocate_pattern(64);
                csf->pattern_size[csf->current_pattern] = 64;
                csf->pattern_alloc_size[csf->current_pattern] = 64;
        }

        return 1;
}


int csf_process_tick(song_t *csf)
{
        csf->flags &= ~SONG_FIRSTTICK;
        /* [Decrease tick counter. Is tick counter 0?] */
        if (--csf->tick_count == 0) {
                /* [-- Yes --] */

                /* [Tick counter = Tick counter set (the current 'speed')] */
                csf->tick_count = csf->current_speed;

                /* [Decrease row counter. Is row counter 0?] */
                if (--csf->row_count <= 0) {
                        /* [-- Yes --] */

                        /* [Row counter = 1]
                        this uses zero, in order to simplify SEx effect handling -- SEx has no effect if a
                        channel to its left has already set the delay value. thus we set the row counter
                        there to (value + 1) which is never zero, but 0 and 1 are fundamentally equivalent
                        as far as csf_process_tick is concerned. */
                        csf->row_count = 0;

                        /* [Increase ProcessRow. Is ProcessRow > NumberOfRows?] */
                        if (++csf->process_row >= csf->pattern_size[csf->current_pattern]) {
                                /* [-- Yes --] */

                                if (!increment_row(csf))
                                        return 0;
                        } /* else [-- No --] */

                        /* [CurrentRow = ProcessRow] */
                        csf->row = csf->process_row;

                        /* [Update Pattern Variables]
                        (this is handled along with update effects) */
                        csf->flags |= SONG_FIRSTTICK;
                } else {
                        /* [-- No --] */
                        /* Call update-effects for each channel. */
                }


                // Reset channel values
                song_voice_t *chan = csf->voices;
                song_note_t *m = csf->patterns[csf->current_pattern] + csf->row * MAX_CHANNELS;

                for (unsigned int nchan=0; nchan<MAX_CHANNELS; chan++, nchan++, m++) {
                        // this is where we're going to spit out our midi
                        // commands... ALL WE DO is dump raw midi data to
                        // our super-secret "midi buffer"
                        // -mrsb
                        if (csf_midi_out_note)
                                csf_midi_out_note(nchan, m);

                        chan->row_note = m->note;

                        if (m->instrument)
                                chan->last_instrument = m->instrument;

                        chan->row_instr = m->instrument;
                        chan->row_voleffect = m->voleffect;
                        chan->row_volparam = m->volparam;
                        chan->row_effect = m->effect;
                        chan->row_param = m->param;

                        chan->left_volume = chan->left_volume_new;
                        chan->right_volume = chan->right_volume_new;
                        chan->flags &= ~(CHN_PORTAMENTO | CHN_VIBRATO | CHN_TREMOLO | CHN_PANBRELLO);
                        chan->n_command = 0;
                }

                csf_process_effects(csf, 1);
        } else {
                /* [-- No --] */
                /* [Update effects for each channel as required.] */

                if (csf_midi_out_note) {
                        song_note_t *m = csf->patterns[csf->current_pattern] + csf->row * MAX_CHANNELS;

                        for (unsigned int nchan=0; nchan<MAX_CHANNELS; nchan++, m++) {
                                /* m==NULL allows schism to receive notification of SDx and Scx commands */
                                csf_midi_out_note(nchan, NULL);
                        }
                }

                csf_process_effects(csf, 0);
        }

        return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Handles envelopes & mixer setup

int csf_read_note(song_t *csf)
{
        song_voice_t *chan;
        unsigned int cn;

        // Checking end of row ?
        if (csf->flags & SONG_PAUSED) {
                if (!csf->current_speed)
                        csf->current_speed = 6;
                if (!csf->current_tempo)
                        csf->current_tempo = 125;

                csf->flags &= ~SONG_FIRSTTICK;

                if (--csf->tick_count == 0) {
                        csf->tick_count = csf->current_speed;
                        if (--csf->row_count <= 0) {
                                csf->row_count = 0;
                                //csf->flags |= SONG_FIRSTTICK;
                        }
                        // clear channel values (similar to csf_process_tick)
                        for (cn = 0, chan = csf->voices; cn < MAX_CHANNELS; cn++, chan++) {
                                chan->row_note = 0;
                                chan->row_instr = 0;
                                chan->row_voleffect = 0;
                                chan->row_volparam = 0;
                                chan->row_effect = 0;
                                chan->row_param = 0;
                                chan->n_command = 0;
                        }
                }
                csf_process_effects(csf, 0);
        } else {
                if (!csf_process_tick(csf))
                        return 0;
        }

        ////////////////////////////////////////////////////////////////////////////////////

        if (!csf->current_tempo)
                return 0;

        csf->buffer_count = (mix_frequency * 5 * csf->tempo_factor) / (csf->current_tempo << 8);

        // chaseback hoo hah
        if (csf->stop_at_order > -1 && csf->stop_at_row > -1) {
                if (csf->stop_at_order <= (signed) csf->current_order &&
                    csf->stop_at_row <= (signed) csf->row) {
                        return 0;
                }
        }

        ////////////////////////////////////////////////////////////////////////////////////
        // Update channels data
        if (mix_flags & SNDMIX_NOMIXING)
                return 1;

        // Master Volume + Pre-Amplification / Attenuation setup
        // mixing_volume is the 'mixing volume' setting
        // Modplug's master volume calculation limited the volume to 0x180, whereas this yields
        // a maximum of 0x200. Try *3 here instead of <<2 if this proves to be problematic.
        // I think this is a closer match to Impulse Tracker, though.
        uint32_t master_vol = csf->mixing_volume << 2;

        csf->num_voices = 0;

        for (cn = 0, chan = csf->voices; cn < MAX_VOICES; cn++, chan++) {
                /*if(cn == 0 || cn == 1)
                fprintf(stderr, "considering channel %d (per %d, pos %d/%d, flags %X)\n",
                        (int)cn, chan->period, chan->position, chan->length, chan->flags);*/

                if (chan->flags & CHN_NOTEFADE &&
                    !(chan->fadeout_volume | chan->right_volume | chan->left_volume)) {
                        chan->length = 0;
                        chan->rofs =
                        chan->lofs = 0;
                        continue;
                }

                // Check for unused channel
                if (cn >= MAX_CHANNELS && !chan->length) {
                        continue;
                }

                // Reset channel data
                chan->increment = 0;
                chan->final_volume = 0;
                chan->final_panning = chan->panning + chan->pan_swing;
                chan->final_panning = CLAMP(chan->final_panning, 0, 256);
                chan->ramp_length = 0;

                // Calc Frequency
                if (chan->period && chan->length) {
                        int vol = chan->volume + chan->vol_swing;

                        vol = CLAMP(vol, 0, 256);

                        // Tremolo
                        if (chan->flags & CHN_TREMOLO)
                                rn_tremolo(csf, chan, &vol);

                        // Tremor
                        if (chan->n_command == FX_TREMOR)
                                rn_tremor(chan, &vol);

                        // Clip volume
                        vol = CLAMP(vol, 0, 0x100);
                        vol <<= 6;

                        // Process Envelopes
                        if ((csf->flags & SONG_INSTRUMENTMODE) && chan->ptr_instrument) {
                                rn_process_envelope(chan, &vol);
                        } else {
                                // No Envelope: key off => note cut
                                // 1.41-: CHN_KEYOFF|CHN_NOTEFADE
                                if (chan->flags & CHN_NOTEFADE) {
                                        chan->fadeout_volume = 0;
                                        vol = 0;
                                }
                        }

                        // vol is 14-bits
                        if (vol) {
                                // IMPORTANT: chan->final_volume is 14 bits !!!
                                // -> _muldiv( 14+7, 6+6, 18); => RealVolume: 14-bit result (21+12-19)
                                chan->final_volume = _muldiv(vol * csf->current_global_volume,
                                        chan->global_volume * chan->instrument_volume, 1 << 19);
                        }

                        int period = chan->period;

                        if ((chan->flags & (CHN_GLISSANDO|CHN_PORTAMENTO)) == (CHN_GLISSANDO|CHN_PORTAMENTO)) {
                                period = get_period_from_note(get_note_from_period(period),
                                        chan->c5speed, csf->flags & SONG_LINEARSLIDES);
                        }

                        // Arpeggio ?
                        if (chan->n_command == FX_ARPEGGIO)
                                period = rn_arpeggio(csf, chan, period);

                        // Pitch/Filter Envelope
                        int envpitch = 0;

                        if ((csf->flags & SONG_INSTRUMENTMODE) && chan->ptr_instrument
                                && (chan->flags & CHN_PITCHENV) && chan->ptr_instrument->pitch_env.nodes)
                                rn_pitch_filter_envelope(chan, &envpitch, &period);

                        // Vibrato
                        if (chan->flags & CHN_VIBRATO)
                                rn_vibrato(csf, chan, &period);

                        // Panbrello
                        if (chan->flags & CHN_PANBRELLO)
                                rn_panbrello(chan);

                        int nPeriodFrac = 0;

                        // Instrument Auto-Vibrato
                        if (chan->ptr_sample && chan->ptr_sample->vib_depth)
                                rn_instrument_vibrato(csf, chan, &period, &nPeriodFrac);

                        unsigned int freq = get_freq_from_period(period, chan->c5speed, nPeriodFrac,
                                csf->flags & SONG_LINEARSLIDES);

                        if (!(chan->flags & CHN_NOTEFADE))
                                rn_gen_key(csf, chan, cn, freq, vol);

                        // Filter Envelope: controls cutoff frequency
                        if (chan && chan->ptr_instrument && chan->ptr_instrument->flags & ENV_FILTER) {
                                setup_channel_filter(chan,
                                        !(chan->flags & CHN_FILTER), envpitch, mix_frequency);
                        }

                        chan->sample_freq = freq;

                        unsigned int ninc = _muldiv(freq, 0x10000, mix_frequency);

                        if (ninc >= 0xFFB0 && ninc <= 0x10090)
                                ninc = 0x10000;

                        if (csf->freq_factor != 128)
                                ninc = (ninc * csf->freq_factor) >> 7;

                        if (ninc > 0xFF0000)
                                ninc = 0xFF0000;

                        chan->increment = (ninc + 1) & ~3;
                }

                // Increment envelope position
                if (csf->flags & SONG_INSTRUMENTMODE && chan->ptr_instrument)
                        rn_increment_env_pos(chan);

                // Volume ramping
                chan->flags &= ~CHN_VOLUMERAMP;

                if (chan->final_volume || chan->left_volume || chan->right_volume)
                        chan->flags |= CHN_VOLUMERAMP;

                if (chan->strike)
                        chan->strike--;

                // Check for too big increment
                if (((chan->increment >> 16) + 1) >= (int)(chan->loop_end - chan->loop_start))
                        chan->flags &= ~CHN_LOOP;

                chan->right_volume_new = chan->left_volume_new = 0;
                chan->current_sample_data = (chan->data && chan->length && chan->increment) ? chan->data : NULL;

                update_vu_meter(chan);

                if (chan->current_sample_data) {
                        if (!rn_update_sample(csf, chan, cn, master_vol))
                                break;
                } else {
                        // Note change but no sample
                        //if (chan->vu_meter > 0xFF) chan->vu_meter = 0;
                        chan->left_volume = chan->right_volume = 0;
                        chan->length = 0;
                }
        }

        // Checking Max Mix Channels reached: ordering by volume
        if (csf->num_voices >= max_voices && (!(mix_flags & SNDMIX_DIRECTTODISK))) {
                for (unsigned int i = 0; i < csf->num_voices; i++) {
                        unsigned int j = i;

                        while ((j + 1 < csf->num_voices) &&
                            (csf->voices[csf->voice_mix[j]].final_volume
                             < csf->voices[csf->voice_mix[j + 1]].final_volume))
                        {
                                unsigned int n = csf->voice_mix[j];
                                csf->voice_mix[j] = csf->voice_mix[j + 1];
                                csf->voice_mix[j + 1] = n;
                                j++;
                        }
                }
        }

        return 1;
}

