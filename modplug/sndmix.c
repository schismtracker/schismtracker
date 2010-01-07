/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/

#include "sndfile.h"
#include "snd_fm.h"
#include "snd_gm.h"
#include "cmixer.h"
#include "snd_flt.h"
#include "snd_eq.h"

#include "util.h" /* for clamp */

// Volume ramp length, in 1/10 ms
#define VOLUMERAMPLEN   146 // 1.46ms = 64 samples at 44.1kHz

// VU meter
#define VUMETER_DECAY 16

// SNDMIX: These are global flags for playback control
unsigned int m_nMaxMixChannels = 32; // ITT it is 1994
// Mixing Configuration (SetWaveConfig)
uint32_t gnChannels = 1;
uint32_t gdwSoundSetup = 0;
uint32_t gdwMixingFreq = 44100;
uint32_t gnBitsPerSample = 16;
// Mixing data initialized in
static unsigned int volume_ramp_samples = 64;
unsigned int gnVULeft = 0;
unsigned int gnVURight = 0;
int32_t gnDryROfsVol = 0;
int32_t gnDryLOfsVol = 0;

typedef uint32_t (* LPCONVERTPROC)(void *, int *, uint32_t, int *, int *);

extern void interleave_front_rear(int *, int *, unsigned int);
extern void mono_from_stereo(int *, unsigned int);
extern void stereo_fill(int *, unsigned int, int *, int *);

extern int MixSoundBuffer[MIXBUFFERSIZE*4];
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




int csf_init_player(CSoundFile *csf, int reset)
{
        if (m_nMaxMixChannels > MAX_VOICES)
                m_nMaxMixChannels = MAX_VOICES;

        gdwMixingFreq = CLAMP(gdwMixingFreq, 4000, MAX_SAMPLE_RATE);
        volume_ramp_samples = (gdwMixingFreq * VOLUMERAMPLEN) / 100000;

        if (volume_ramp_samples < 8)
                volume_ramp_samples = 8;

        if (gdwSoundSetup & SNDMIX_NORAMPING)
                volume_ramp_samples = 2;

        gnDryROfsVol = gnDryLOfsVol = 0;

        if (reset) {
                gnVULeft  = 0;
                gnVURight = 0;
        }

        csf_initialize_dsp(csf, reset);
        initialize_eq(reset, gdwMixingFreq);

        Fmdrv_Init(gdwMixingFreq);
        OPL_Reset();
        GM_Reset(0);
        return true;
}


unsigned int csf_read(CSoundFile *csf, void * lpDestBuffer, unsigned int cbBuffer)
{
        uint8_t * lpBuffer = (uint8_t *)lpDestBuffer;
        LPCONVERTPROC pCvt = clip_32_to_8;
        int32_t vu_min[2];
        int32_t vu_max[2];
        unsigned int lRead, lMax, lSampleSize, lCount, lSampleCount, nStat=0;

        vu_min[0] = vu_min[1] = 0x7FFFFFFF;
        vu_max[0] = vu_max[1] = -0x7FFFFFFF;


        csf->m_nMixStat = 0;
        lSampleSize = gnChannels;

             if (gnBitsPerSample == 16) { lSampleSize *= 2; pCvt = clip_32_to_16; }
        else if (gnBitsPerSample == 24) { lSampleSize *= 3; pCvt = clip_32_to_24; }
        else if (gnBitsPerSample == 32) { lSampleSize *= 4; pCvt = clip_32_to_32; }

        lMax = cbBuffer / lSampleSize;

        if (!lMax || !lpBuffer || !csf->m_nChannels)
                return 0;

        lRead = lMax;

        if (csf->m_dwSongFlags & SONG_ENDREACHED)
                lRead = 0; // skip the loop

        while (lRead > 0) {
                // Update Channel Data
                unsigned int lTotalSampleCount;

                if (!csf->m_nBufferCount) {
                        if (!(gdwSoundSetup & SNDMIX_DIRECTTODISK))
                                csf->m_nBufferCount = lRead;

                        if (!csf_read_note(csf)) {
                                csf->m_dwSongFlags |= SONG_ENDREACHED;

                                if (csf->stop_at_order > -1)
                                        return 0; /* faster */

                                if (lRead == lMax)
                                        break;

                                if (!(gdwSoundSetup & SNDMIX_DIRECTTODISK))
                                        csf->m_nBufferCount = lRead;
                        }

                        if (!csf->m_nBufferCount)
                                break;
                }

                lCount = csf->m_nBufferCount;

                if (lCount > MIXBUFFERSIZE)
                        lCount = MIXBUFFERSIZE;

                if (lCount > lRead)
                        lCount = lRead;

                if (!lCount)
                        break;

                lSampleCount = lCount;

                // Resetting sound buffer
                stereo_fill(MixSoundBuffer, lSampleCount, &gnDryROfsVol, &gnDryLOfsVol);

                if (gnChannels >= 2) {
                        lSampleCount *= 2;
                        csf->m_nMixStat += csf_create_stereo_mix(csf, lCount);
                        csf_process_stereo_dsp(csf, lCount);
                }
                else {
                        csf->m_nMixStat += csf_create_stereo_mix(csf, lCount);
                        mono_from_stereo(MixSoundBuffer, lCount);
                        csf_process_mono_dsp(csf, lCount);
                }

                if (gdwSoundSetup & SNDMIX_EQ) {
                        if (gnChannels >= 2)
                                eq_stereo(MixSoundBuffer, lCount);
                        else
                                eq_mono(MixSoundBuffer, lCount);
                }

                nStat++;

                lTotalSampleCount = lSampleCount;

                // Multichannel
                if (gnChannels > 2) {
                        interleave_front_rear(MixSoundBuffer, MixRearBuffer, lSampleCount);
                        lTotalSampleCount *= 2;
                }

                // Perform clipping + VU-Meter
                lpBuffer += pCvt(lpBuffer, MixSoundBuffer, lTotalSampleCount, vu_min, vu_max);

                // Buffer ready
                lRead -= lCount;
                csf->m_nBufferCount -= lCount;
        }

        if (lRead)
                memset(lpBuffer, (gnBitsPerSample == 8) ? 0x80 : 0, lRead * lSampleSize);

        // VU-Meter
        vu_min[0] >>= 18;
        vu_min[1] >>= 18;
        vu_max[0] >>= 18;
        vu_max[1] >>= 18;

        if (vu_max[0] < vu_min[0])
                vu_max[0] = vu_min[0];

        if (vu_max[1] < vu_min[1])
                vu_max[1] = vu_min[1];

        if ((gnVULeft = (unsigned int)(vu_max[0] - vu_min[0])) > 0xFF)
                gnVULeft = 0xFF;

        if ((gnVURight = (unsigned int)(vu_max[1] - vu_min[1])) > 0xFF)
                gnVURight = 0xFF;

        if (nStat) {
                csf->m_nMixStat += nStat - 1;
                csf->m_nMixStat /= nStat;
        }

        return lMax - lRead;
}



/////////////////////////////////////////////////////////////////////////////
// Handles navigation/effects

static int increment_row(CSoundFile *csf)
{
        csf->m_nProcessRow = csf->m_nBreakRow; /* [ProcessRow = BreakRow] */
        csf->m_nBreakRow = 0;                  /* [BreakRow = 0] */

        /* some ugly copypasta, this should be less dumb */
        if (csf->m_dwSongFlags & SONG_PATTERNPLAYBACK) {
                if (csf->m_nRepeatCount > 0)
                        csf->m_nRepeatCount--;
                if (!csf->m_nRepeatCount) {
                        csf->m_nProcessRow = PROCESS_NEXT_ORDER;
                        return false;
                }
        } else if (!(csf->m_dwSongFlags & SONG_ORDERLOCKED)) {
                if (csf->m_nLockedOrder < MAX_ORDERS) {
                        csf->m_nProcessOrder = csf->m_nLockedOrder - 1;
                        csf->m_nLockedOrder = MAX_ORDERS;
                }

                /* [Increase ProcessOrder] */
                /* [while Order[ProcessOrder] = 0xFEh, increase ProcessOrder] */
                do {
                        csf->m_nProcessOrder++;
                } while (csf->Orderlist[csf->m_nProcessOrder] == ORDER_SKIP);

                /* [if Order[ProcessOrder] = 0xFFh, ProcessOrder = 0] (... or just stop playing) */
                if (csf->Orderlist[csf->m_nProcessOrder] == ORDER_LAST) {
                        if (csf->m_nRepeatCount > 0)
                                csf->m_nRepeatCount--;
                        if (!csf->m_nRepeatCount) {
                                csf->m_nProcessRow = PROCESS_NEXT_ORDER;
                                return false;
                        }

                        csf->m_nProcessOrder = 0;
                        while (csf->Orderlist[csf->m_nProcessOrder] == ORDER_SKIP)
                                csf->m_nProcessOrder++;
                }
                if (csf->Orderlist[csf->m_nProcessOrder] >= MAX_PATTERNS) {
                        // what the butt?
                        csf->m_nProcessRow = PROCESS_NEXT_ORDER;
                        return false;
                }

                /* [CurrentPattern = Order[ProcessOrder]] */
                csf->m_nCurrentOrder = csf->m_nProcessOrder;
                csf->m_nCurrentPattern = csf->Orderlist[csf->m_nProcessOrder];
        }

        if (!csf->PatternSize[csf->m_nCurrentPattern] || !csf->Patterns[csf->m_nCurrentPattern]) {
                /* okay, this is wrong. allocate the pattern _NOW_ */
                csf->Patterns[csf->m_nCurrentPattern] = csf_allocate_pattern(64, 64);
                csf->PatternSize[csf->m_nCurrentPattern] = 64;
                csf->PatternAllocSize[csf->m_nCurrentPattern] = 64;
        }

        return true;
}


int csf_process_tick(CSoundFile *csf)
{
        csf->m_dwSongFlags &= ~SONG_FIRSTTICK;
        /* [Decrease tick counter. Is tick counter 0?] */
        if (--csf->m_nTickCount == 0) {
                /* [-- Yes --] */

                /* [Tick counter = Tick counter set (the current 'speed')] */
                csf->m_nTickCount = csf->m_nMusicSpeed;

                /* [Decrease row counter. Is row counter 0?] */
                if (--csf->m_nRowCount <= 0) {
                        /* [-- Yes --] */

                        /* [Row counter = 1]
                        this uses zero, in order to simplify SEx effect handling -- SEx has no effect if a
                        channel to its left has already set the delay value. thus we set the row counter
                        there to (value + 1) which is never zero, but 0 and 1 are fundamentally equivalent
                        as far as csf_process_tick is concerned. */
                        csf->m_nRowCount = 0;

                        /* [Increase ProcessRow. Is ProcessRow > NumberOfRows?] */
                        if (++csf->m_nProcessRow >= csf->PatternSize[csf->m_nCurrentPattern]) {
                                /* [-- Yes --] */

                                if (!increment_row(csf))
                                        return false;
                        } /* else [-- No --] */

                        /* [CurrentRow = ProcessRow] */
                        csf->m_nRow = csf->m_nProcessRow;

                        /* [Update Pattern Variables]
                        (this is handled along with update effects) */
                        csf->m_dwSongFlags |= SONG_FIRSTTICK;
                } else {
                        /* [-- No --] */
                        /* Call update-effects for each channel. */
                }


                // Reset channel values
                SONGVOICE *pChn = csf->Voices;
                MODCOMMAND *m = csf->Patterns[csf->m_nCurrentPattern] + csf->m_nRow * csf->m_nChannels;

                for (unsigned int nChn=0; nChn<csf->m_nChannels; pChn++, nChn++, m++) {
                        // this is where we're going to spit out our midi
                        // commands... ALL WE DO is dump raw midi data to
                        // our super-secret "midi buffer"
                        // -mrsb
                        if (csf_midi_out_note)
                                csf_midi_out_note(nChn, m);

                        pChn->nRowNote = m->note;

                        if (m->instr)
                                pChn->nLastInstr = m->instr;

                        pChn->nRowInstr = m->instr;
                        pChn->nRowVolCmd = m->volcmd;
                        pChn->nRowVolume = m->vol;
                        pChn->nRowCommand = m->command;
                        pChn->nRowParam = m->param;

                        pChn->nLeftVol = pChn->nNewLeftVol;
                        pChn->nRightVol = pChn->nNewRightVol;
                        pChn->dwFlags &= ~(CHN_PORTAMENTO | CHN_VIBRATO | CHN_TREMOLO | CHN_PANBRELLO);
                        pChn->nCommand = 0;
                }
        } else {
                /* [-- No --] */
                /* [Update effects for each channel as required.] */

                if (csf_midi_out_note) {
                        MODCOMMAND *m = csf->Patterns[csf->m_nCurrentPattern] + csf->m_nRow * csf->m_nChannels;

                        for (unsigned int nChn=0; nChn<csf->m_nChannels; nChn++, m++) {
                                /* m==NULL allows schism to receive notification of SDx and Scx commands */
                                csf_midi_out_note(nChn, NULL);
                        }
                }
        }

        // Update Effects
        csf_process_effects(csf);

        return true;
}

////////////////////////////////////////////////////////////////////////////////////////////
//
// XXX * I prefixed these with `rn_' to avoid any namespace conflicts
// XXX   Needs better naming!
// XXX * Keep inline?
// XXX * Get rid of the pointer passing where it is not needed
//

static inline void rn_tremolo(CSoundFile *csf, SONGVOICE *chan, int *vol)
{
        unsigned int trempos = chan->nTremoloPos & 0xFF;
        int tdelta;

        const int tremattn = 5;

        switch (chan->nTremoloType & 0x03) {
        default:
                tdelta = FineSineData[trempos];
                break;
        case 1:
                tdelta = FineRampDownData[trempos];
                break;
        case 2:
                tdelta = FineSquareWave[trempos];
                break;
        case 3:
                tdelta = 128 * ((double) rand() / RAND_MAX) - 64;
                break;
        }
        *vol += (tdelta * (int)chan->nTremoloDepth) >> tremattn;

        // handle on tick-N, or all ticks if not in old-effects mode
        if (!(csf->m_dwSongFlags & SONG_FIRSTTICK) || !(csf->m_dwSongFlags & SONG_ITOLDEFFECTS)) {
                chan->nTremoloPos = (trempos + 4 * chan->nTremoloSpeed) & 0xFF;
        }
}


static inline void rn_tremor(SONGVOICE *chan, int *vol)
{
        if ((chan->nTremorCount & 192) == 128)
                *vol = 0;

        chan->dwFlags |= CHN_FASTVOLRAMP;
}


static inline void rn_panbrello(SONGVOICE *chan)
{
        unsigned int panpos = chan->nPanbrelloPos & 0xFF;
        int pdelta;

        switch (chan->nPanbrelloType & 0x03) {
        default:
                pdelta = FineSineData[panpos];
                break;
        case 1:
                pdelta = FineRampDownData[panpos];
                break;
        case 2:
                pdelta = FineSquareWave[panpos];
                break;
        case 3:
                pdelta = 128 * ((double) rand() / RAND_MAX) - 64;
                break;
        }

        chan->nPanbrelloPos += chan->nPanbrelloSpeed;
        pdelta = ((pdelta * (int)chan->nPanbrelloDepth) + 2) >> 3;
        pdelta += chan->nRealPan;
        chan->nRealPan = CLAMP(pdelta, 0, 256);
}


static inline void rn_vibrato(CSoundFile *csf, SONGVOICE *chan, int *nperiod)
{
        unsigned int vibpos = chan->nVibratoPos & 0xFF;
        int vdelta;
        unsigned int vdepth;
        int period = *nperiod;

        switch (chan->nVibratoType & 0x03) {
        default:
                vdelta = FineSineData[vibpos];
                break;
        case 1:
                vdelta = FineRampDownData[vibpos];
                break;
        case 2:
                vdelta = FineSquareWave[vibpos];
                break;
        case 3:
                vdelta = 128 * ((double) rand() / RAND_MAX) - 64;
                break;
        }

        if (csf->m_dwSongFlags & SONG_ITOLDEFFECTS) {
                vdepth = 5;
                vdelta = -vdelta; // yes, IT does vibrato backwards in old-effects mode. try it.
        } else {
                vdepth = 6;
        }
        vdelta = (vdelta * (int)chan->nVibratoDepth) >> vdepth;

        if (csf->m_dwSongFlags & SONG_LINEARSLIDES) {
                int l = abs(vdelta);

                if (vdelta < 0) {
                        vdelta = _muldiv(period, LinearSlideDownTable[l >> 2], 0x10000) - period;

                        if (l & 0x03)
                                vdelta += _muldiv(period, FineLinearSlideDownTable[l & 0x03], 0x10000) - period;
                } else {
                        vdelta = _muldiv(period, LinearSlideUpTable[l >> 2], 0x10000) - period;

                        if (l & 0x03)
                                vdelta += _muldiv(period, FineLinearSlideUpTable[l & 0x03], 0x10000) - period;
                }
        }

        period -= vdelta;

        // handle on tick-N, or all ticks if not in old-effects mode
        if (!(csf->m_dwSongFlags & SONG_FIRSTTICK) || !(csf->m_dwSongFlags & SONG_ITOLDEFFECTS)) {
                chan->nVibratoPos = (vibpos + 4 * chan->nVibratoSpeed) & 0xFF;
        }

        *nperiod = period;
}

static inline void rn_instrument_vibrato(CSoundFile *csf, SONGVOICE *chan, int *nperiod, int *nperiodfrac)
{
        int period = *nperiod;
        int periodfrac = *nperiodfrac;
        SONGSAMPLE *pins = chan->pInstrument;

        /* this isn't correct, but it's better... [original was without int cast] */
        if (!pins->nVibSweep) {
                chan->nAutoVibDepth = pins->nVibDepth << 8;
        } else {
                chan->nAutoVibDepth += pins->nVibSweep;

                if ((chan->nAutoVibDepth >> 8) > (int) pins->nVibDepth)
                        chan->nAutoVibDepth = pins->nVibDepth << 8;
        }

        chan->nAutoVibPos += (int) pins->nVibRate;

        int val;

        // XXX why is this so completely different from the other vibrato code?
        switch(pins->nVibType) {
        case VIB_SINE:
        default:
                val = ft2VibratoTable[chan->nAutoVibPos & 255];
                break;
        case VIB_RAMP_DOWN:
                val = ((0x40 - (chan->nAutoVibPos >> 1)) & 0x7F) - 0x40;
                break;
        case VIB_SQUARE:
                val = (chan->nAutoVibPos & 128) ? +64 : -64;
                break;
        case VIB_RANDOM:
                val = 128 * ((double) rand() / RAND_MAX) - 64;
                break;
        }

        int n = ((val * chan->nAutoVibDepth) >> 8);

        // is this right? -mrsb
        if (!(csf->m_dwSongFlags & SONG_ITOLDEFFECTS))
                n >>= 1;

        int df1, df2;

        if (n < 0) {
                n = -n;
                unsigned int n1 = n >> 8;
                df1 = LinearSlideUpTable[n1];
                df2 = LinearSlideUpTable[n1 + 1];
        }
        else {
                unsigned int n1 = n >> 8;
                df1 = LinearSlideDownTable[n1];
                df2 = LinearSlideDownTable[n1 + 1];
        }

        n >>= 2;
        period = _muldiv(period, df1 + ((df2 - df1) * (n & 0x3F) >> 6), 256);
        periodfrac = period & 0xFF;
        period >>= 8;

        *nperiod = period;
        *nperiodfrac = periodfrac;
}


static inline void rn_process_envelope(SONGVOICE *chan, int *nvol)
{
        SONGINSTRUMENT *penv = chan->pHeader;
        int vol = *nvol;

        // Volume Envelope
        if (chan->dwFlags & CHN_VOLENV && penv->VolEnv.nNodes) {
                int envpos = chan->nVolEnvPosition;
                unsigned int pt = penv->VolEnv.nNodes - 1;

                for (unsigned int i = 0; i < (unsigned int)(penv->VolEnv.nNodes - 1); i++) {
                        if (envpos <= penv->VolEnv.Ticks[i]) {
                                pt = i;
                                break;
                        }
                }

                int x2 = penv->VolEnv.Ticks[pt];
                int x1, envvol;

                if (envpos >= x2) {
                        envvol = penv->VolEnv.Values[pt] << 2;
                        x1 = x2;
                } else if (pt) {
                        envvol = penv->VolEnv.Values[pt-1] << 2;
                        x1 = penv->VolEnv.Ticks[pt-1];
                } else {
                        envvol = 0;
                        x1 = 0;
                }

                if (envpos > x2)
                        envpos = x2;

                if (x2 > x1 && envpos > x1) {
                        envvol += ((envpos - x1) * (((int)penv->VolEnv.Values[pt]<<2) - envvol)) / (x2 - x1);
                }

                envvol = CLAMP(envvol, 0, 256);
                vol = (vol * envvol) >> 8;
        }

        // Panning Envelope
        if ((chan->dwFlags & CHN_PANENV) && (penv->PanEnv.nNodes)) {
                int envpos = chan->nPanEnvPosition;
                unsigned int pt = penv->PanEnv.nNodes - 1;

                for (unsigned int i=0; i<(unsigned int)(penv->PanEnv.nNodes-1); i++) {
                        if (envpos <= penv->PanEnv.Ticks[i]) {
                                pt = i;
                                break;
                        }
                }

                int x2 = penv->PanEnv.Ticks[pt], y2 = penv->PanEnv.Values[pt];
                int x1, envpan;

                if (envpos >= x2) {
                        envpan = y2;
                        x1 = x2;
                } else if (pt) {
                        envpan = penv->PanEnv.Values[pt-1];
                        x1 = penv->PanEnv.Ticks[pt-1];
                } else {
                        envpan = 128;
                        x1 = 0;
                }

                if (x2 > x1 && envpos > x1) {
                        envpan += ((envpos - x1) * (y2 - envpan)) / (x2 - x1);
                }

                envpan = CLAMP(envpan, 0, 64);

                int pan = chan->nPan;

                if (pan >= 128) {
                        pan += ((envpan - 32) * (256 - pan)) / 32;
                } else {
                        pan += ((envpan - 32) * (pan)) / 32;
                }

                chan->nRealPan = CLAMP(pan, 0, 256);
        }

        // FadeOut volume
        if (chan->dwFlags & CHN_NOTEFADE) {
                unsigned int fadeout = penv->nFadeOut;

                if (fadeout) {
                        chan->nFadeOutVol -= fadeout << 1;

                        if (chan->nFadeOutVol <= 0)
                                chan->nFadeOutVol = 0;

                        vol = (vol * chan->nFadeOutVol) >> 16;
                } else if (!chan->nFadeOutVol) {
                        vol = 0;
                }
        }

        // Pitch/Pan separation
        if (penv->nPPS && chan->nRealPan && chan->nNote) {
                // PPS value is 1/512, i.e. PPS=1 will adjust by 8/512 = 1/64 for each 8 semitones
                // with PPS = 32 / PPC = C-5, E-6 will pan hard right (and D#6 will not)
                int pandelta = (int) chan->nRealPan
                             + (int) ((int) (chan->nNote - penv->nPPC - 1) * (int) penv->nPPS) / (int) 4;
                chan->nRealPan = CLAMP(pandelta, 0, 256);
        }

        *nvol = vol;
}


static inline int rn_arpeggio(CSoundFile *csf, SONGVOICE *chan, int period)
{
        int a;
        switch ((csf->m_nMusicSpeed - csf->m_nTickCount) % 3) {
        case 1:
                a = chan->nArpeggio >> 4;
                break;
        case 2:
                a = chan->nArpeggio & 0xf;
                break;
        default:
                a = 0;
        }
        if (!a)
                return period;
        return get_period_from_note(a + get_note_from_period(period), 8363, 0);
}


static inline void rn_pitch_filter_envelope(SONGVOICE *chan, int *nenvpitch, int *nperiod)
{
        SONGINSTRUMENT *penv = chan->pHeader;
        int envpos = chan->nPitchEnvPosition;
        unsigned int pt = penv->PitchEnv.nNodes - 1;
        int period = *nperiod;
        int envpitch = *nenvpitch;

        for (unsigned int i = 0; i < (unsigned int)(penv->PitchEnv.nNodes - 1); i++) {
                if (envpos <= penv->PitchEnv.Ticks[i]) {
                        pt = i;
                        break;
                }
        }

        int x2 = penv->PitchEnv.Ticks[pt];
        int x1;

        if (envpos >= x2) {
                envpitch = (((int)penv->PitchEnv.Values[pt]) - 32) * 8;
                x1 = x2;
        } else if (pt) {
                envpitch = (((int)penv->PitchEnv.Values[pt - 1]) - 32) * 8;
                x1 = penv->PitchEnv.Ticks[pt - 1];
        } else {
                envpitch = 0;
                x1 = 0;
        }

        if (envpos > x2)
                envpos = x2;

        if (x2 > x1 && envpos > x1) {
                int envpitchdest = (((int)penv->PitchEnv.Values[pt]) - 32) * 8;
                envpitch += ((envpos - x1) * (envpitchdest - envpitch)) / (x2 - x1);
        }

        // clamp to -255/255?
        envpitch = CLAMP(envpitch, -256, 256);

        // Pitch Envelope
        if (!(penv->dwFlags & ENV_FILTER)) {
                int l = abs(envpitch);

                if (l > 255)
                        l = 255;

                period = _muldiv(period, (envpitch < 0 ?
                        LinearSlideUpTable : LinearSlideDownTable)[l], 0x10000);
        }

        *nperiod = period;
        *nenvpitch = envpitch;
}


static inline void rn_increment_env_pos(SONGVOICE *chan)
{
        SONGINSTRUMENT *penv = chan->pHeader;

        if (chan->dwFlags & CHN_VOLENV) {
                chan->nVolEnvPosition++;

                if (penv->dwFlags & ENV_VOLLOOP) {
                        int volloopend = penv->VolEnv.Ticks[penv->VolEnv.nLoopEnd] + 1;

                        if (chan->nVolEnvPosition == volloopend) {
                                chan->nVolEnvPosition = penv->VolEnv.Ticks[penv->VolEnv.nLoopStart];
                                if (penv->VolEnv.nLoopEnd == penv->VolEnv.nLoopStart
                                    && !penv->VolEnv.Values[penv->VolEnv.nLoopStart]) {
                                        chan->dwFlags |= CHN_NOTEFADE;
                                        chan->nFadeOutVol = 0;
                                }
                        }
                }

                if (penv->dwFlags & ENV_VOLSUSTAIN
                    && (chan->nVolEnvPosition == (int)penv->VolEnv.Ticks[penv->VolEnv.nSustainEnd] + 1)
                    && !(chan->dwFlags & CHN_KEYOFF)) {
                        // Volume sustained
                        chan->nVolEnvPosition = penv->VolEnv.Ticks[penv->VolEnv.nSustainStart];
                } else if (chan->nVolEnvPosition > penv->VolEnv.Ticks[penv->VolEnv.nNodes - 1]) {
                        // End of Envelope
                        chan->nVolEnvPosition = penv->VolEnv.Ticks[penv->VolEnv.nNodes - 1];
                        chan->dwFlags |= CHN_NOTEFADE;
                        if (!penv->VolEnv.Values[penv->VolEnv.nNodes-1]) {
                                chan->nFadeOutVol = 0;
                                chan->nRealVolume = 0;
                        }
                }
        }

        if (chan->dwFlags & CHN_PANENV) {
                chan->nPanEnvPosition++;

                if (penv->dwFlags & ENV_PANLOOP) {
                        int panloopend = penv->PanEnv.Ticks[penv->PanEnv.nLoopEnd] + 1;

                        if (chan->nPanEnvPosition == panloopend) {
                                chan->nPanEnvPosition = penv->PanEnv.Ticks[penv->PanEnv.nLoopStart];
                        }
                }

                if (penv->dwFlags & ENV_PANSUSTAIN
                    && (chan->nPanEnvPosition == (int) penv->PanEnv.Ticks[penv->PanEnv.nSustainEnd] + 1)
                    && !(chan->dwFlags & CHN_KEYOFF)) {
                        // Panning sustained
                        chan->nPanEnvPosition = penv->PanEnv.Ticks[penv->PanEnv.nSustainStart];
                } else if (chan->nPanEnvPosition > penv->PanEnv.Ticks[penv->PanEnv.nNodes - 1]) {
                        // End of envelope
                        chan->nPanEnvPosition = penv->PanEnv.Ticks[penv->PanEnv.nNodes - 1];
                }
        }

        if (chan->dwFlags & CHN_PITCHENV) {
                chan->nPitchEnvPosition++;

                if (penv->dwFlags & ENV_PITCHLOOP) {
                        int pitchloopend = penv->PitchEnv.Ticks[penv->PitchEnv.nLoopEnd] + 1;

                        if (chan->nPitchEnvPosition == pitchloopend) {
                                chan->nPitchEnvPosition = penv->PitchEnv.Ticks[penv->PitchEnv.nLoopStart];
                        }
                }

                if (penv->dwFlags & ENV_PITCHSUSTAIN
                    && (chan->nPitchEnvPosition == (int) penv->PitchEnv.Ticks[penv->PitchEnv.nSustainEnd]+1)
                    && !(chan->dwFlags & CHN_KEYOFF)) {
                        // Pitch sustained
                        chan->nPitchEnvPosition = penv->PitchEnv.Ticks[penv->PitchEnv.nSustainStart];
                } else if (chan->nPitchEnvPosition > penv->PitchEnv.Ticks[penv->PitchEnv.nNodes - 1]) {
                        // End of envelope
                        chan->nPitchEnvPosition = penv->PitchEnv.Ticks[penv->PitchEnv.nNodes - 1];
                }
        }
}


static inline int rn_update_sample(CSoundFile *csf, SONGVOICE *chan, int nChn, int nMasterVol)
{
        // Adjusting volumes
        if (gnChannels >= 2) {
                int pan = ((int) chan->nRealPan) - 128;
                pan *= (int) csf->m_nStereoSeparation;
                pan /= 128;

                if ((csf->m_dwSongFlags & SONG_INSTRUMENTMODE)
                    && chan->pHeader
                    && chan->pHeader->nMidiChannelMask > 0)
                        GM_Pan(nChn, pan);

                if (csf->m_dwSongFlags & SONG_NOSTEREO) {
                        pan = 128;
                } else {
                        pan += 128;
                        pan = CLAMP(pan, 0, 256);

                        if (gdwSoundSetup & SNDMIX_REVERSESTEREO)
                                pan = 256 - pan;
                }

                int realvol = (chan->nRealVolume * nMasterVol) >> (8 - 1);

                chan->nNewLeftVol  = (realvol * pan) >> 8;
                chan->nNewRightVol = (realvol * (256 - pan)) >> 8;
        } else {
                chan->nNewRightVol = (chan->nRealVolume * nMasterVol) >> 8;
                chan->nNewLeftVol = chan->nNewRightVol;
        }

        // Clipping volumes
        if (chan->nNewRightVol > 0xFFFF)
                chan->nNewRightVol = 0xFFFF;

        if (chan->nNewLeftVol  > 0xFFFF)
                chan->nNewLeftVol  = 0xFFFF;

        // Check IDO
        if (gdwSoundSetup & SNDMIX_NORESAMPLING) {
                chan->dwFlags &= ~(CHN_HQSRC);
                chan->dwFlags |= CHN_NOIDO;
        } else {
                chan->dwFlags &= ~(CHN_NOIDO | CHN_HQSRC);

                if (chan->nInc == 0x10000) {
                        chan->dwFlags |= CHN_NOIDO;
                } else {
                        if (!(gdwSoundSetup & SNDMIX_HQRESAMPLER) &&
                            !(gdwSoundSetup & SNDMIX_ULTRAHQSRCMODE)) {
                                if (chan->nInc >= 0xFF00)
                                        chan->dwFlags |= CHN_NOIDO;
                        }
                }
        }

        chan->nNewRightVol >>= MIXING_ATTENUATION;
        chan->nNewLeftVol  >>= MIXING_ATTENUATION;
        chan->nRightRamp =
        chan->nLeftRamp  = 0;

        // Dolby Pro-Logic Surround (S91)
        if (chan->dwFlags & CHN_SURROUND && gnChannels <= 2
            && !(gdwSoundSetup & SNDMIX_NOSURROUND)
            && !(csf->m_dwSongFlags & SONG_NOSTEREO))
                chan->nNewLeftVol = -chan->nNewLeftVol;

        // Checking Ping-Pong Loops
        if (chan->dwFlags & CHN_PINGPONGFLAG)
                chan->nInc = -chan->nInc;

        if (chan->dwFlags & CHN_MUTE) {
                chan->nLeftVol = chan->nRightVol = 0;
        } else if (!(gdwSoundSetup & SNDMIX_NORAMPING) &&
            chan->dwFlags & CHN_VOLUMERAMP &&
            (chan->nRightVol != chan->nNewRightVol ||
             chan->nLeftVol  != chan->nNewLeftVol)) {
                // Setting up volume ramp
                int nRampLength = volume_ramp_samples;
                int nRightDelta = ((chan->nNewRightVol - chan->nRightVol) << VOLUMERAMPPRECISION);
                int nLeftDelta  = ((chan->nNewLeftVol  - chan->nLeftVol)  << VOLUMERAMPPRECISION);

                if (gdwSoundSetup & SNDMIX_HQRESAMPLER) {
                        if (chan->nRightVol | chan->nLeftVol &&
                            chan->nNewRightVol | chan->nNewLeftVol &&
                            !(chan->dwFlags & CHN_FASTVOLRAMP)) {
                                nRampLength = csf->m_nBufferCount;

                                int l = (1 << (VOLUMERAMPPRECISION - 1));
                                int r =(int) volume_ramp_samples;

                                nRampLength = CLAMP(nRampLength, l, r);
                        }
                }

                chan->nRightRamp = nRightDelta / nRampLength;
                chan->nLeftRamp  = nLeftDelta / nRampLength;
                chan->nRightVol  = chan->nNewRightVol - ((chan->nRightRamp * nRampLength) >> VOLUMERAMPPRECISION);
                chan->nLeftVol   = chan->nNewLeftVol - ((chan->nLeftRamp * nRampLength) >> VOLUMERAMPPRECISION);

                if (chan->nRightRamp | chan->nLeftRamp) {
                        chan->nRampLength = nRampLength;
                } else {
                        chan->dwFlags &= ~CHN_VOLUMERAMP;
                        chan->nRightVol = chan->nNewRightVol;
                        chan->nLeftVol  = chan->nNewLeftVol;
                }
        } else {
                chan->dwFlags  &= ~CHN_VOLUMERAMP;
                chan->nRightVol = chan->nNewRightVol;
                chan->nLeftVol  = chan->nNewLeftVol;
        }

        chan->nRampRightVol = chan->nRightVol << VOLUMERAMPPRECISION;
        chan->nRampLeftVol = chan->nLeftVol << VOLUMERAMPPRECISION;

        // Adding the channel in the channel list
        csf->VoiceMix[csf->m_nMixChannels++] = nChn;

        if (csf->m_nMixChannels >= MAX_VOICES)
                return 0;

        return 1;
}


// XXX Rename this
static inline void rn_gen_key(CSoundFile *csf, SONGVOICE *chan, const int chan_num, const int freq, const int vol)
{
        if (chan->dwFlags & CHN_MUTE) {
                // don't do anything
        } else if (csf->m_dwSongFlags & SONG_INSTRUMENTMODE &&
            chan->pHeader &&
            chan->pHeader->nMidiChannelMask > 0) {
                MidiBendMode BendMode = MIDI_BEND_NORMAL;
                /* TODO: If we're expecting a large bend exclusively
                 * in either direction, update BendMode to indicate so.
                 * This can be used to extend the range of MIDI pitch bending.
                 */

                // Vol maximum is 64*64 here. (4096)
                int volume = vol;

                if (chan->dwFlags & CHN_ADLIB && volume > 0) {
                        // This gives a value in the range 0..127.
                        //int o = volume;
                        volume = find_volume((unsigned short) volume) * chan->nInsVol / 64;
                        //fprintf(stderr, "%d -> %d[%d]\n", o, volume, chan->nInsVol);
                } else {
                        // This gives a value in the range 0..127.
                        volume = volume * chan->nInsVol / 8192;
                }

                GM_SetFreqAndVol(chan_num, freq, volume, BendMode, chan->dwFlags & CHN_KEYOFF);
        } else if (chan->dwFlags & CHN_ADLIB) {
                // For some reason, scaling by about (2*3)/(8200/8300) is needed
                // to get a frequency that matches with ST3.
                int oplfreq = freq * 164 / 249;

                OPL_HertzTouch(chan_num, oplfreq, chan->dwFlags & CHN_KEYOFF);

                // ST32 ignores global & master volume in adlib mode, guess we should do the same -Bisqwit
                OPL_Touch(chan_num, NULL, vol * chan->nInsVol * 63 / (1 << 20));
        }
}


static inline void update_vu_meter(SONGVOICE *chan)
{
        // Update VU-Meter (nRealVolume is 14-bit)
        // TODO: missing background channels by doing it this way.
        // need to use nMasterCh, add the vu meters for each physical voice, and bit shift.
        uint32_t vutmp = chan->nRealVolume >> (14 - 8);
        if (vutmp > 0xFF) vutmp = 0xFF;
        if (chan->dwFlags & CHN_ADLIB) {
                // fake VU decay (intentionally similar to ST3)
                if (chan->nVUMeter > VUMETER_DECAY) {
                        chan->nVUMeter -= VUMETER_DECAY;
                } else {
                        chan->nVUMeter = 0;
                }
                if (chan->nVUMeter >= 0x100) {
                        chan->nVUMeter = vutmp;
                }
        } else if (vutmp && chan->pCurrentSample) {
                // can't fake the funk
                int n;
                int pos = chan->nPos; // necessary on 64-bit systems (sometimes pos == -1, weird)
                if (chan->dwFlags & CHN_16BIT) {
                        const signed short *p = (signed short *)(chan->pCurrentSample);
                        if (chan->dwFlags & CHN_STEREO)
                                n = p[2 * pos];
                        else
                                n = p[pos];
                        n >>= 8;
                } else {
                        const signed char *p = (signed char *)(chan->pCurrentSample);
                        if (chan->dwFlags & CHN_STEREO)
                                n = p[2 * pos];
                        else
                                n = p[pos];
                }
                if (n < 0)
                        n = -n;
                vutmp *= n;
                vutmp >>= 7; // 0..255
                if (vutmp)
                        chan->nVUMeter = vutmp;
        } else {
                chan->nVUMeter = 0;
        }
}


////////////////////////////////////////////////////////////////////////////////////////////
// Handles envelopes & mixer setup

int csf_read_note(CSoundFile *csf)
{
        SONGVOICE *chan;
        unsigned int cn;

        // Checking end of row ?
        if (csf->m_dwSongFlags & SONG_PAUSED) {
                if (!csf->m_nMusicSpeed)
                        csf->m_nMusicSpeed = 6;
                if (!csf->m_nMusicTempo)
                        csf->m_nMusicTempo = 125;

                csf->m_dwSongFlags &= ~SONG_FIRSTTICK;

                if (--csf->m_nTickCount == 0) {
                        csf->m_nTickCount = csf->m_nMusicSpeed;
                        if (--csf->m_nRowCount <= 0) {
                                csf->m_nRowCount = 0;
                                //csf->m_dwSongFlags |= SONG_FIRSTTICK;
                        }
                        // clear channel values (similar to csf_process_tick)
                        for (cn = 0, chan = csf->Voices; cn < MAX_CHANNELS; cn++, chan++) {
                                chan->nRowNote = 0;
                                chan->nRowInstr = 0;
                                chan->nRowVolCmd = 0;
                                chan->nRowVolume = 0;
                                chan->nRowCommand = 0;
                                chan->nRowParam = 0;
                                chan->nCommand = 0;
                        }
                }
                csf_process_effects(csf);
        } else {
                if (!csf_process_tick(csf))
                        return false;
        }

        ////////////////////////////////////////////////////////////////////////////////////

        if (!csf->m_nMusicTempo)
                return false;

        csf->m_nBufferCount = (gdwMixingFreq * 5 * csf->m_nTempoFactor) / (csf->m_nMusicTempo << 8);

        // chaseback hoo hah
        if (csf->stop_at_order > -1 && csf->stop_at_row > -1) {
                if (csf->stop_at_order <= (signed) csf->m_nCurrentOrder &&
                    csf->stop_at_row <= (signed) csf->m_nRow) {
                        return false;
                }
        }

        ////////////////////////////////////////////////////////////////////////////////////
        // Update channels data
        if (gdwSoundSetup & SNDMIX_NOMIXING)
                return true;

        // Master Volume + Pre-Amplification / Attenuation setup
        // m_nSongPreAmp is the 'mixing volume' setting
        // Modplug's master volume calculation limited the volume to 0x180, whereas this yields
        // a maximum of 0x200. Try *3 here instead of <<2 if this proves to be problematic.
        // I think this is a closer match to Impulse Tracker, though.
        uint32_t nMasterVol = csf->m_nSongPreAmp << 2;

        csf->m_nMixChannels = 0;

        for (cn = 0, chan = csf->Voices; cn < MAX_VOICES; cn++, chan++) {
                /*if(cn == 0 || cn == 1)
                fprintf(stderr, "considering channel %d (per %d, pos %d/%d, flags %X)\n",
                        (int)cn, chan->nPeriod, chan->nPos, chan->nLength, chan->dwFlags);*/

                if (chan->dwFlags & CHN_NOTEFADE &&
                    !(chan->nFadeOutVol | chan->nRightVol | chan->nLeftVol)) {
                        chan->nLength = 0;
                        chan->nROfs =
                        chan->nLOfs = 0;
                        continue;
                }

                // Check for unused channel
                if (cn >= csf->m_nChannels && !chan->nLength) {
                        continue;
                }

                // Reset channel data
                chan->nInc = 0;
                chan->nRealVolume = 0;
                chan->nRealPan = chan->nPan + chan->nPanSwing;
                chan->nRealPan = CLAMP(chan->nRealPan, 0, 256);
                chan->nRampLength = 0;

                // Calc Frequency
                if (chan->nPeriod && chan->nLength) {
                        int vol = chan->nVolume + chan->nVolSwing;

                        vol = CLAMP(vol, 0, 256);

                        // Tremolo
                        if (chan->dwFlags & CHN_TREMOLO)
                                rn_tremolo(csf, chan, &vol);

                        // Tremor
                        if (chan->nCommand == CMD_TREMOR)
                                rn_tremor(chan, &vol);

                        // Clip volume
                        vol = CLAMP(vol, 0, 0x100);
                        vol <<= 6;

                        // Process Envelopes
                        if ((csf->m_dwSongFlags & SONG_INSTRUMENTMODE) && chan->pHeader) {
                                rn_process_envelope(chan, &vol);
                        } else {
                                // No Envelope: key off => note cut
                                // 1.41-: CHN_KEYOFF|CHN_NOTEFADE
                                if (chan->dwFlags & CHN_NOTEFADE) {
                                        chan->nFadeOutVol = 0;
                                        vol = 0;
                                }
                        }

                        // vol is 14-bits
                        if (vol) {
                                // IMPORTANT: chan->nRealVolume is 14 bits !!!
                                // -> _muldiv( 14+7, 6+6, 18); => RealVolume: 14-bit result (21+12-19)
                                chan->nRealVolume = _muldiv(vol * csf->m_nGlobalVolume, chan->nGlobalVol * chan->nInsVol, 1 << 19);
                        }

                        int period = chan->nPeriod;

                        if ((chan->dwFlags & (CHN_GLISSANDO|CHN_PORTAMENTO)) == (CHN_GLISSANDO|CHN_PORTAMENTO)) {
                                period = get_period_from_note(get_note_from_period(period),
                                        chan->nC5Speed, csf->m_dwSongFlags & SONG_LINEARSLIDES);
                        }

                        // Arpeggio ?
                        if (chan->nCommand == CMD_ARPEGGIO)
                                period = rn_arpeggio(csf, chan, period);

                        // Pitch/Filter Envelope
                        int envpitch = 0;

                        if ((csf->m_dwSongFlags & SONG_INSTRUMENTMODE) && chan->pHeader
                                && (chan->dwFlags & CHN_PITCHENV) && chan->pHeader->PitchEnv.nNodes)
                                rn_pitch_filter_envelope(chan, &envpitch, &period);

                        // Vibrato
                        if (chan->dwFlags & CHN_VIBRATO)
                                rn_vibrato(csf, chan, &period);

                        // Panbrello
                        if (chan->dwFlags & CHN_PANBRELLO)
                                rn_panbrello(chan);

                        int nPeriodFrac = 0;

                        // Instrument Auto-Vibrato
                        if (chan->pInstrument && chan->pInstrument->nVibDepth)
                                rn_instrument_vibrato(csf, chan, &period, &nPeriodFrac);

                        unsigned int freq = get_freq_from_period(period, chan->nC5Speed, nPeriodFrac,
                                csf->m_dwSongFlags & SONG_LINEARSLIDES);

                        if (!(chan->dwFlags & CHN_NOTEFADE))
                                rn_gen_key(csf, chan, cn, freq, vol);

                        // Filter Envelope: controls cutoff frequency
                        if (chan && chan->pHeader && chan->pHeader->dwFlags & ENV_FILTER) {
                                setup_channel_filter(chan, (chan->dwFlags & CHN_FILTER) ? false : true, envpitch, gdwMixingFreq);
                        }

                        chan->sample_freq = freq;

                        unsigned int ninc = _muldiv(freq, 0x10000, gdwMixingFreq);

                        if (ninc >= 0xFFB0 && ninc <= 0x10090)
                                ninc = 0x10000;

                        if (csf->m_nFreqFactor != 128)
                                ninc = (ninc * csf->m_nFreqFactor) >> 7;

                        if (ninc > 0xFF0000)
                                ninc = 0xFF0000;

                        chan->nInc = (ninc + 1) & ~3;
                }

                // Increment envelope position
                if (csf->m_dwSongFlags & SONG_INSTRUMENTMODE && chan->pHeader)
                        rn_increment_env_pos(chan);

                // Volume ramping
                chan->dwFlags &= ~CHN_VOLUMERAMP;

                if (chan->nRealVolume || chan->nLeftVol || chan->nRightVol)
                        chan->dwFlags |= CHN_VOLUMERAMP;

                if (chan->strike)
                        chan->strike--;

                // Check for too big nInc
                if (((chan->nInc >> 16) + 1) >= (int)(chan->nLoopEnd - chan->nLoopStart))
                        chan->dwFlags &= ~CHN_LOOP;

                chan->nNewRightVol = chan->nNewLeftVol = 0;
                chan->pCurrentSample = (chan->pSample && chan->nLength && chan->nInc) ? chan->pSample : NULL;

                update_vu_meter(chan);

                if (chan->pCurrentSample) {
                        if (!rn_update_sample(csf, chan, cn, nMasterVol))
                                break;
                } else {
                        // Note change but no sample
                        //if (chan->nVUMeter > 0xFF) chan->nVUMeter = 0;
                        chan->nLeftVol = chan->nRightVol = 0;
                        chan->nLength = 0;
                }
        }

        // Checking Max Mix Channels reached: ordering by volume
        if (csf->m_nMixChannels >= m_nMaxMixChannels && (!(gdwSoundSetup & SNDMIX_DIRECTTODISK))) {
                for (unsigned int i = 0; i < csf->m_nMixChannels; i++) {
                        unsigned int j = i;

                        while ((j + 1 < csf->m_nMixChannels) &&
                            (csf->Voices[csf->VoiceMix[j]].nRealVolume < csf->Voices[csf->VoiceMix[j + 1]].nRealVolume))
                        {
                                unsigned int n = csf->VoiceMix[j];
                                csf->VoiceMix[j] = csf->VoiceMix[j + 1];
                                csf->VoiceMix[j + 1] = n;
                                j++;
                        }
                }
        }

        return true;
}

