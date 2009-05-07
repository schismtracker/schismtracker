/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/

#include "stdafx.h"
#include "sndfile.h"
#include "snd_fm.h"
#include "snd_gm.h"
#include "cmixer.h"
#include "snd_flt.h"
#include "snd_eq.h"

// Volume ramp length, in 1/10 ms
#define VOLUMERAMPLEN   146 // 1.46ms = 64 samples at 44.1kHz
#define CLAMP(a,y,z) ((a) < (y) ? (y) : ((a) > (z) ? (z) : (a)))

// SNDMIX: These are global flags for playback control
LONG CSoundFile::m_nStreamVolume = 0x8000;
unsigned int CSoundFile::m_nMaxMixChannels = 32;
// Mixing Configuration (SetWaveConfig)
DWORD CSoundFile::gdwSysInfo = 0;
DWORD CSoundFile::gnChannels = 1;
DWORD CSoundFile::gdwSoundSetup = 0;
DWORD CSoundFile::gdwMixingFreq = 44100;
DWORD CSoundFile::gnBitsPerSample = 16;
// Mixing data initialized in
unsigned int CSoundFile::gnAGC = AGC_UNITY;
unsigned int CSoundFile::gnVolumeRampSamples = 64;
unsigned int CSoundFile::gnVULeft = 0;
unsigned int CSoundFile::gnVURight = 0;
LPSNDMIXHOOKPROC CSoundFile::gpSndMixHook = NULL;
PMIXPLUGINCREATEPROC CSoundFile::gpMixPluginCreateProc = NULL;
LONG gnDryROfsVol = 0;
LONG gnDryLOfsVol = 0;
LONG gnRvbROfsVol = 0;
LONG gnRvbLOfsVol = 0;
int gbInitPlugins = 0;

typedef DWORD (MPPASMCALL * LPCONVERTPROC)(LPVOID, int *, DWORD, LPLONG, LPLONG);

extern unsigned int MPPASMCALL AGC(int *pBuffer, unsigned int nSamples, unsigned int nAGC);
extern VOID MPPASMCALL Dither(int *pBuffer, unsigned int nSamples, unsigned int nBits);

extern void interleave_front_rear(int *, int *, unsigned int);
extern void mono_from_stereo(int *, unsigned int);
extern void stereo_fill(int *, unsigned int, int *, int *);

extern int MixSoundBuffer[MIXBUFFERSIZE*4];
extern int MixRearBuffer[MIXBUFFERSIZE*2];

unsigned int gnReverbSend;


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


// Log tables for pre-amp
// We don't want the tracker to get too loud
const unsigned int PreAmpTable[16] =
{
    0x60, 0x60, 0x60, 0x70, // 0-7
    0x80, 0x88, 0x90, 0x98, // 8-15
    0xA0, 0xA4, 0xA8, 0xB0, // 16-23
    0xB4, 0xB8, 0xBC, 0xC0, // 24-31
};

const unsigned int PreAmpAGCTable[16] =
{
    0x60, 0x60, 0x60, 0x60,
    0x68, 0x70, 0x78, 0x80,
    0x84, 0x88, 0x8C, 0x90,
    0x94, 0x98, 0x9C, 0xA0,
};


BOOL CSoundFile::InitPlayer(BOOL bReset)
{
    if (m_nMaxMixChannels > MAX_CHANNELS)
        m_nMaxMixChannels = MAX_CHANNELS;

    gdwMixingFreq = CLAMP(gdwMixingFreq, 4000, MAX_SAMPLE_RATE);
    gnVolumeRampSamples = (gdwMixingFreq * VOLUMERAMPLEN) / 100000;

    if (gnVolumeRampSamples < 8)
        gnVolumeRampSamples = 8;

    if (gdwSoundSetup & SNDMIX_NORAMPING)
        gnVolumeRampSamples = 2;

    gnDryROfsVol = gnDryLOfsVol = 0;
    gnRvbROfsVol = gnRvbLOfsVol = 0;

    if (bReset) {
        gnVULeft = 0;
        gnVURight = 0;
    }

    gbInitPlugins = bReset ? 3 : 1;
    InitializeDSP(bReset);
    initialize_eq(bReset, gdwMixingFreq);
    
    Fmdrv_Init(gdwMixingFreq);
    OPL_Reset();
    GM_Reset(0);
    return TRUE;
}


BOOL CSoundFile::FadeSong(unsigned int msec)
{
    LONG nsamples = _muldiv(msec, gdwMixingFreq, 1000);

    if (nsamples <= 0)
        return FALSE;

    if (nsamples > 0x100000)
        nsamples = 0x100000;

    m_nBufferCount = nsamples;
    LONG nRampLength = m_nBufferCount;

    // Ramp everything down
    for (unsigned int noff = 0; noff < m_nMixChannels; noff++) {
        MODCHANNEL *pramp = &Chn[ChnMix[noff]];

        if (!pramp)
            continue;

        pramp->nNewLeftVol = pramp->nNewRightVol = 0;
        pramp->nRightRamp = (-pramp->nRightVol << VOLUMERAMPPRECISION) / nRampLength;
        pramp->nLeftRamp = (-pramp->nLeftVol << VOLUMERAMPPRECISION) / nRampLength;
        pramp->nRampRightVol = pramp->nRightVol << VOLUMERAMPPRECISION;
        pramp->nRampLeftVol = pramp->nLeftVol << VOLUMERAMPPRECISION;
        pramp->nRampLength = nRampLength;
        pramp->dwFlags |= CHN_VOLUMERAMP;
    }

    m_dwSongFlags |= SONG_FADINGSONG;
    return TRUE;
}


BOOL CSoundFile::GlobalFadeSong(unsigned int msec)
{
    if (m_dwSongFlags & SONG_GLOBALFADE)
        return FALSE;

    m_nGlobalFadeMaxSamples = _muldiv(msec, gdwMixingFreq, 1000);
    m_nGlobalFadeSamples = m_nGlobalFadeMaxSamples;
    m_dwSongFlags |= SONG_GLOBALFADE;
    return TRUE;
}


unsigned int CSoundFile::Read(LPVOID lpDestBuffer, unsigned int cbBuffer)
{
    LPBYTE lpBuffer = (LPBYTE)lpDestBuffer;
    LPCONVERTPROC pCvt = clip_32_to_8;
    LONG vu_min[2];
    LONG vu_max[2];
    unsigned int lRead, lMax, lSampleSize, lCount, lSampleCount, nStat=0;
#if 0
    unsigned int nMaxPlugins;
#endif

    vu_min[0] = vu_min[1] = 0x7FFFFFFF;
    vu_max[0] = vu_max[1] = -0x7FFFFFFF;

#if 0
    {
        nMaxPlugins = MAX_MIXPLUGINS;
        while ((nMaxPlugins > 0) && (!m_MixPlugins[nMaxPlugins-1].pMixPlugin)) nMaxPlugins--;
    }
#endif

    m_nMixStat = 0;
    lSampleSize = gnChannels;

         if (gnBitsPerSample == 16) { lSampleSize *= 2; pCvt = clip_32_to_16; }
    else if (gnBitsPerSample == 24) { lSampleSize *= 3; pCvt = clip_32_to_24; }
    else if (gnBitsPerSample == 32) { lSampleSize *= 4; pCvt = clip_32_to_32; }

    lMax = cbBuffer / lSampleSize;

    if (!lMax || !lpBuffer || !m_nChannels)
        return 0;

    lRead = lMax;

    if (m_dwSongFlags & SONG_ENDREACHED)
        goto MixDone;

    while (lRead > 0) {
        // Update Channel Data
        unsigned int lTotalSampleCount;

        if (!m_nBufferCount) {
            if (!(gdwSoundSetup & SNDMIX_DIRECTTODISK))
                m_nBufferCount = lRead;

            if (!ReadNote()) {
                m_dwSongFlags |= SONG_ENDREACHED;

                if (stop_at_order > -1)
                    return 0; /* faster */

                if (lRead == lMax)
                    goto MixDone;

                if (!(gdwSoundSetup & SNDMIX_DIRECTTODISK))
                    m_nBufferCount = lRead;
            }

            if (!m_nBufferCount)
                goto MixDone;
        }

        lCount = m_nBufferCount;

        if (lCount > MIXBUFFERSIZE)
            lCount = MIXBUFFERSIZE;

        if (lCount > lRead)
            lCount = lRead;

        if (!lCount)
            break;

        lSampleCount = lCount;
#ifndef MODPLUG_NO_REVERB
        gnReverbSend = 0;
#endif

        // Resetting sound buffer
        stereo_fill(MixSoundBuffer, lSampleCount, &gnDryROfsVol, &gnDryLOfsVol);

        if (gnChannels >= 2) {
            lSampleCount *= 2;
            m_nMixStat += CreateStereoMix(lCount);
#if 0
            if (nMaxPlugins) ProcessPlugins(lCount);
#endif
            ProcessStereoDSP(lCount);
        }
        else {
            m_nMixStat += CreateStereoMix(lCount);
#if 0
            if (nMaxPlugins) ProcessPlugins(lCount);
#endif
            mono_from_stereo(MixSoundBuffer, lCount);
            ProcessMonoDSP(lCount);
        }

        if (gdwSoundSetup & SNDMIX_EQ) {
            if (gnChannels >= 2)
                eq_stereo(MixSoundBuffer, lCount);
            else
                eq_mono(MixSoundBuffer, lCount);
        }

        nStat++;

#ifndef NO_AGC
        // Automatic Gain Control
        if (gdwSoundSetup & SNDMIX_AGC)
            ProcessAGC(lSampleCount);
#endif

        lTotalSampleCount = lSampleCount;

        // Multichannel
        if (gnChannels > 2) {
            interleave_front_rear(MixSoundBuffer, MixRearBuffer, lSampleCount);
            lTotalSampleCount *= 2;
        }

        // Hook Function
        if (gpSndMixHook) {
            gpSndMixHook(MixSoundBuffer, lTotalSampleCount, gnChannels);
        }

        // Perform clipping + VU-Meter
        lpBuffer += pCvt(lpBuffer, MixSoundBuffer, lTotalSampleCount, vu_min, vu_max);

        // Buffer ready
        lRead -= lCount;
        m_nBufferCount -= lCount;
    }

MixDone:
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
        m_nMixStat += nStat - 1;
        m_nMixStat /= nStat;
    }

    return lMax - lRead;
}


/////////////////////////////////////////////////////////////////////////////
// Handles navigation/effects

BOOL CSoundFile::ProcessRow()
//---------------------------
{
    if (++m_nTickCount >= m_nMusicSpeed * (m_nPatternDelay+1) + m_nFrameDelay) {
        m_nPatternDelay = 0;
        m_nFrameDelay = 0;
        m_nTickCount = 0;
        m_nRow = m_nNextRow;
        
        // Reset Pattern Loop Effect
        if (m_nCurrentPattern != m_nNextPattern) {
            if (m_nLockedPattern < MAX_ORDERS) {
                m_nCurrentPattern = m_nLockedPattern;

                if (!(m_dwSongFlags & SONG_ORDERLOCKED))
                    m_nLockedPattern = MAX_ORDERS;
            }
            else {
                m_nCurrentPattern = m_nNextPattern;
            }

            // Check if pattern is valid
            if (!(m_dwSongFlags & SONG_PATTERNLOOP)) {
                m_nPattern = (m_nCurrentPattern < MAX_ORDERS) ? Order[m_nCurrentPattern] : 0xFF;

                if ((m_nPattern < MAX_PATTERNS) && (!Patterns[m_nPattern]))
                    m_nPattern = 0xFE;

                while (m_nPattern >= MAX_PATTERNS) {
                    // End of song ?
                    if ((m_nPattern == 0xFF) || (m_nCurrentPattern >= MAX_ORDERS)) {
                        if (m_nRepeatCount > 0)
                            m_nRepeatCount--;

                        if (!m_nRepeatCount)
                            return FALSE;

                        m_nCurrentPattern = m_nRestartPos;

                        if ((Order[m_nCurrentPattern] >= MAX_PATTERNS)
                            || (!Patterns[Order[m_nCurrentPattern]]))
                            return FALSE;
                    }
                    else {
                        m_nCurrentPattern++;
                    }

                    m_nPattern = (m_nCurrentPattern < MAX_ORDERS) ? Order[m_nCurrentPattern] : 0xFF;

                    if ((m_nPattern < MAX_PATTERNS) && (!Patterns[m_nPattern]))
                        m_nPattern = 0xFE;
                }

                m_nNextPattern = m_nCurrentPattern;
            }
            else if (m_nCurrentPattern < 255) {
                if (m_nRepeatCount > 0)
                    m_nRepeatCount--;

                if (!m_nRepeatCount)
                    return FALSE;
            }
        }

        if (m_dwSongFlags & SONG_STEP) {
            m_dwSongFlags &= ~SONG_STEP;
            m_dwSongFlags |= SONG_PAUSED;
        }

        if (!PatternSize[m_nPattern] || !Patterns[m_nPattern]) {
            /* okay, this is wrong. allocate the pattern _NOW_ */
            Patterns[m_nPattern] = AllocatePattern(64,64);
            PatternSize[m_nPattern] = 64;
            PatternAllocSize[m_nPattern] = 64;
        }

        // Weird stuff?
        if (m_nPattern >= MAX_PATTERNS)
            return FALSE;

        if (m_nRow >= PatternSize[m_nPattern])
            m_nRow = 0;

        m_nNextRow = m_nRow + 1;

        if (m_nNextRow >= PatternSize[m_nPattern]) {
            if (!(m_dwSongFlags & SONG_PATTERNLOOP))
                m_nNextPattern = m_nCurrentPattern + 1;
            else if (m_nRepeatCount > 0)
                return FALSE;

            m_nNextRow = 0;
        }

        // Reset channel values
        MODCHANNEL *pChn = Chn;
        MODCOMMAND *m = Patterns[m_nPattern] + m_nRow * m_nChannels;

        for (unsigned int nChn=0; nChn<m_nChannels; pChn++, nChn++, m++) {
            /* skip realtime copyin */
            if (pChn->nRealtime)
                continue;

            // this is where we're going to spit out our midi
            // commands... ALL WE DO is dump raw midi data to
            // our super-secret "midi buffer"
            // -mrsb
            if (_midi_out_note)
                _midi_out_note(nChn, m);

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
    }
    else if (_midi_out_note) {
        MODCOMMAND *m = Patterns[m_nPattern] + m_nRow * m_nChannels;

        for (unsigned int nChn=0; nChn<m_nChannels; nChn++, m++) {
            /* m==NULL allows schism to receive notification of SDx and Scx commands */
            _midi_out_note(nChn, 0);
        }
    }

    // Should we process tick0 effects?
    if (!m_nMusicSpeed)
        m_nMusicSpeed = 1;

    m_dwSongFlags |= SONG_FIRSTTICK;

    if (m_nTickCount) {
        m_dwSongFlags &= ~SONG_FIRSTTICK;

        if (m_nTickCount < m_nMusicSpeed * (1 + m_nPatternDelay)) {
            if (!(m_nTickCount % m_nMusicSpeed))
                m_dwSongFlags |= SONG_FIRSTTICK;
        }
    }

    // Update Effects
    return ProcessEffects();
}


/* XXX This kind of loop is performed many times in ReadNote.
 * XXX Sneak this in one of those somewhere?
 */
static void handle_realtime_closures(CSoundFile *csf)
{
    MODCHANNEL *chan = csf->Chn;

    for (unsigned int i = 0; i < csf->m_nChannels; chan++, i++) {
        /* reset end of "row" */
        if (chan->nRealtime &&
            chan->nRowNote &&
            (chan->nTickStart % csf->m_nMusicSpeed) ==
                (csf->m_nTickCount % csf->m_nMusicSpeed)) {
            chan->nRealtime = 0;
            chan->nRowNote = 0;
            chan->nRowInstr = 0;
          //chan->nMaster
            chan->nRowVolCmd = 0;
            chan->nRowVolume = 0;
            chan->nRowCommand = 0;
            chan->nRowParam = 0;
            chan->nTickStart = 0;
        }
    }
}


// XXX nchn32 is const 64?
static unsigned int master_volume(CSoundFile *csf)
{
    //MODCHANNEL *pChn = csf->Chn;
    int realmastervol = csf->m_nMasterVolume;
    int nchn32 = csf->m_nChannels;
    unsigned int mastervol,
                 attenuation;

    if (nchn32 < 1)
        nchn32 = 1;
    else if (nchn32 > 31)
        nchn32 = 31;

    if (realmastervol > 0x80) {
        realmastervol = 0x80 + ((realmastervol - 0x80) * (nchn32 + 4)) / 16;
    }

    mastervol = (realmastervol * (csf->m_nSongPreAmp)) >> 6;

    //if (mastervol > 0x200)
    //      mastervol = 0x200;

    if ((csf->m_dwSongFlags & SONG_GLOBALFADE) &&
        csf->m_nGlobalFadeMaxSamples) {
        mastervol = _muldiv(mastervol, csf->m_nGlobalFadeSamples, csf->m_nGlobalFadeMaxSamples);
    }

    attenuation = (csf->gdwSoundSetup & SNDMIX_AGC) ? PreAmpAGCTable[nchn32 >> 1] : PreAmpTable[nchn32 >> 1];

    if (attenuation < 1)
        attenuation = 1;

    mastervol = (mastervol << 7) / attenuation;

    return mastervol > 0x180 ? 0x180 : mastervol;
}


////////////////////////////////////////////////////////////////////////////////////////////
// 
// XXX * I prefixed these with `rn_' to avoid any namespace conflicts
// XXX   Needs better naming!
// XXX * Keep inline?
// XXX * Get rid of the pointer passing where it is not needed
//

static inline void rn_tremolo(CSoundFile *csf, MODCHANNEL *chan, int *vol)
{
    unsigned int trempos = chan->nTremoloPos & 0x3F;
    
    if (*vol > 0) {
        const int tremattn = 6;
    
        switch (chan->nTremoloType & 0x03)
        {
        case 1:
            *vol += (ModRampDownTable[trempos] * (int)chan->nTremoloDepth) >> tremattn;
            break;
        case 2:
            *vol += (ModSquareTable[trempos] * (int)chan->nTremoloDepth) >> tremattn;
            break;
        case 3:
            *vol += (ModRandomTable[trempos] * (int)chan->nTremoloDepth) >> tremattn;
            break;
        default:
            *vol += (ModSinusTable[trempos] * (int)chan->nTremoloDepth) >> tremattn;
        }
    }
    
    if (csf->m_nTickCount ||
        !(csf->m_dwSongFlags & SONG_ITOLDEFFECTS)) {
        chan->nTremoloPos = (trempos + chan->nTremoloSpeed) & 0x3F;
    }
}


static inline void rn_tremor(CSoundFile *csf, MODCHANNEL *chan, int *vol)
{
    unsigned int n = (chan->nTremorParam >> 4) + (chan->nTremorParam & 0x0F);
    unsigned int ontime = chan->nTremorParam >> 4;
    unsigned int tremcount = (unsigned int) chan->nTremorCount;

    if (csf->m_dwSongFlags & SONG_ITOLDEFFECTS) {
        n += 2;
        ontime++;
    }

    if (tremcount >= n)
        tremcount = 0;

    if (csf->m_nTickCount) {
        if (tremcount >= ontime)
            *vol = 0;

        chan->nTremorCount = (BYTE)(tremcount + 1);
    }

    chan->dwFlags |= CHN_FASTVOLRAMP;
}


static inline void rn_panbrello(MODCHANNEL *chan)
{
    unsigned int panpos = ((chan->nPanbrelloPos + 0x10) >> 2) & 0x3F;
    int pdelta;
    
    switch (chan->nPanbrelloType & 0x03)
    {
    case 1:
        pdelta = ModRampDownTable[panpos];
        break;
    case 2:
        pdelta = ModSquareTable[panpos];
        break;
    case 3:
        pdelta = ModRandomTable[panpos];
        break;
    default:
        pdelta = ModSinusTable[panpos];
    }
    
    chan->nPanbrelloPos += chan->nPanbrelloSpeed;
    pdelta = ((pdelta * (int)chan->nPanbrelloDepth) + 2) >> 3;
    pdelta += chan->nRealPan;
    chan->nRealPan = CLAMP(pdelta, 0, 256);
}


static inline void rn_vibrato(CSoundFile *csf, MODCHANNEL *chan, int *nperiod)
{
    unsigned int vibpos = chan->nVibratoPos;
    int vdelta;
    unsigned int vdepth;
    int period = *nperiod;

    switch (chan->nVibratoType & 0x03)
    {
    case 1:
        vdelta = ModRampDownTable[vibpos];
        break;
    case 2:
        vdelta = ModSquareTable[vibpos];
        break;
    case 3:
        vdelta = ModRandomTable[vibpos];
        break;
    default:
        vdelta = ModSinusTable[vibpos];
    }

    vdepth = (csf->m_dwSongFlags & SONG_ITOLDEFFECTS) ? 6 : 7;
    vdelta = (vdelta * (int)chan->nVibratoDepth) >> vdepth;

    if (csf->m_dwSongFlags & SONG_LINEARSLIDES) {
        int l = abs(vdelta);

        if (vdelta < 0) {
            vdelta = _muldiv(period, LinearSlideDownTable[l >> 2], 0x10000) - period;

            if (l & 0x03)
                vdelta += _muldiv(period, FineLinearSlideDownTable[l & 0x03], 0x10000) - period;
        }
        else {
            vdelta = _muldiv(period, LinearSlideUpTable[l >> 2], 0x10000) - period;

            if (l & 0x03)
                vdelta += _muldiv(period, FineLinearSlideUpTable[l & 0x03], 0x10000) - period;
        }
    }

    period += vdelta;

    if (csf->m_nTickCount ||
        !(csf->m_dwSongFlags & SONG_ITOLDEFFECTS)) {
        chan->nVibratoPos = (vibpos + chan->nVibratoSpeed) & 0x3F;
    }

    *nperiod = period;
}


static inline void rn_instrument_vibrato(CSoundFile *csf, MODCHANNEL *chan, int *nperiod, int *nperiodfrac)
{
    int period = *nperiod;
    int periodfrac = *nperiodfrac;
    MODINSTRUMENT *pins = chan->pInstrument;

    /* this isn't correct, but it's better... [original was without int cast] */
    if (!pins->nVibSweep) {
        chan->nAutoVibDepth = pins->nVibDepth << 8;
    }
    else {
        chan->nAutoVibDepth += pins->nVibSweep;

        if ((chan->nAutoVibDepth >> 8) > (int) pins->nVibDepth)
            chan->nAutoVibDepth = pins->nVibDepth << 8;
    }

    chan->nAutoVibPos += (int) pins->nVibRate;

    int val;

    switch(pins->nVibType)
    {
    case 4: // Random
        val = ModRandomTable[chan->nAutoVibPos & 0x3F];
        chan->nAutoVibPos++;
        break;
    case 3: // Ramp Down
        val = ((0x40 - (chan->nAutoVibPos >> 1)) & 0x7F) - 0x40;
        break;
    case 2: // Ramp Up
        val = ((0x40 + (chan->nAutoVibPos >> 1)) & 0x7f) - 0x40;
        break;
    case 1: // Square
        val = (chan->nAutoVibPos & 128) ? +64 : -64;
        break;
    default:    // Sine
        val = ft2VibratoTable[chan->nAutoVibPos & 255];
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
        df2 = LinearSlideUpTable[n1+1];
    }
    else {
        unsigned int n1 = n >> 8;
        df1 = LinearSlideDownTable[n1];
        df2 = LinearSlideDownTable[n1+1];
    }

    n >>= 2;
    period = _muldiv(period, df1 + ((df2 - df1) * (n & 0x3F) >> 6), 256);
    periodfrac = period & 0xFF;
    period >>= 8;

    *nperiod = period;
    *nperiodfrac = periodfrac;
}


static inline void rn_process_envelope(MODCHANNEL *chan, int *nvol)
{
    INSTRUMENTHEADER *penv = chan->pHeader;
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
        }
        else if (pt) {
            envvol = penv->VolEnv.Values[pt-1] << 2;
            x1 = penv->VolEnv.Ticks[pt-1];
        }
        else {
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
        }
        else if (pt) {
            envpan = penv->PanEnv.Values[pt-1];
            x1 = penv->PanEnv.Ticks[pt-1];
        }
        else {
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
        }
        else {
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
        }
        else if (!chan->nFadeOutVol) {
            vol = 0;
        }
    }

    // Pitch/Pan separation
    if (penv->nPPS && chan->nRealPan && chan->nNote) {
        int pandelta = (int)chan->nRealPan + (int)((int)(chan->nNote - penv->nPPC - 1) * (int)penv->nPPS) / (int)8;
        chan->nRealPan = CLAMP(pandelta, 0, 256);
    }

    *nvol = vol;
}


static inline int rn_arpeggio(CSoundFile *csf, MODCHANNEL *chan, int period)
{
    switch (csf->m_nTickCount % 3) {
    case 1:
        return csf->GetPeriodFromNote(
            csf->GetNoteFromPeriod(period) + (chan->nArpeggio >> 4), 0, chan->nC5Speed);
    case 2:
        return csf->GetPeriodFromNote(
            csf->GetNoteFromPeriod(period) + (chan->nArpeggio & 0x0F), 0, chan->nC5Speed);
    default:
        return period;
    }
}


static inline void rn_pitch_filter_envelope(MODCHANNEL *chan, int *nenvpitch, int *nperiod)
{
    INSTRUMENTHEADER *penv = chan->pHeader;
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
    }
    else if (pt) {
        envpitch = (((int)penv->PitchEnv.Values[pt - 1]) - 32) * 8;
        x1 = penv->PitchEnv.Ticks[pt - 1];
    }
    else {
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


static inline void rn_increment_env_pos(MODCHANNEL *chan)
{
    INSTRUMENTHEADER *penv = chan->pHeader;

    // Volume Envelope
    if (chan->dwFlags & CHN_VOLENV) {
        // Increase position
        chan->nVolEnvPosition++;

        // Volume Loop ?
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

        // Volume Sustain ?
        if (penv->dwFlags & ENV_VOLSUSTAIN &&
            !(chan->dwFlags & CHN_KEYOFF)) {
            if (chan->nVolEnvPosition == (int)penv->VolEnv.Ticks[penv->VolEnv.nSustainEnd] + 1)
                chan->nVolEnvPosition = penv->VolEnv.Ticks[penv->VolEnv.nSustainStart];
        }
        // End of Envelope ?
        else if (chan->nVolEnvPosition > penv->VolEnv.Ticks[penv->VolEnv.nNodes - 1]) {
            chan->dwFlags |= CHN_NOTEFADE;
            chan->nVolEnvPosition = penv->VolEnv.Ticks[penv->VolEnv.nNodes - 1];

            if (!penv->VolEnv.Values[penv->VolEnv.nNodes-1]) {
                chan->dwFlags |= CHN_NOTEFADE;
                chan->nFadeOutVol = 0;
                chan->nRealVolume = 0;
            }
        }
    }

    // Panning Envelope
    if (chan->dwFlags & CHN_PANENV) {
        chan->nPanEnvPosition++;

        if (penv->dwFlags & ENV_PANLOOP) {
            int panloopend = penv->PanEnv.Ticks[penv->PanEnv.nLoopEnd] + 1;

            if (chan->nPanEnvPosition == panloopend)
                chan->nPanEnvPosition = penv->PanEnv.Ticks[penv->PanEnv.nLoopStart];
        }

        // Panning Sustain ?
        if (penv->dwFlags & ENV_PANSUSTAIN &&
            (chan->nPanEnvPosition == (int) penv->PanEnv.Ticks[penv->PanEnv.nSustainEnd] + 1) &&
            !(chan->dwFlags & CHN_KEYOFF)) {
            // Panning sustained
            chan->nPanEnvPosition = penv->PanEnv.Ticks[penv->PanEnv.nSustainStart];
        }
        else {
            if (chan->nPanEnvPosition > penv->PanEnv.Ticks[penv->PanEnv.nNodes - 1])
                chan->nPanEnvPosition = penv->PanEnv.Ticks[penv->PanEnv.nNodes - 1];
        }
    }

    // Pitch Envelope
    if (chan->dwFlags & CHN_PITCHENV) {
        // Increase position
        chan->nPitchEnvPosition++;

        // Pitch Loop ?
        if (penv->dwFlags & ENV_PITCHLOOP) {
            if (chan->nPitchEnvPosition >= penv->PitchEnv.Ticks[penv->PitchEnv.nLoopEnd])
                chan->nPitchEnvPosition = penv->PitchEnv.Ticks[penv->PitchEnv.nLoopStart];
        }

        // Pitch Sustain ?
        if (penv->dwFlags & ENV_PITCHSUSTAIN &&
            !(chan->dwFlags & CHN_KEYOFF)) {
            if (chan->nPitchEnvPosition == (int) penv->PitchEnv.Ticks[penv->PitchEnv.nSustainEnd]+1)
                chan->nPitchEnvPosition = penv->PitchEnv.Ticks[penv->PitchEnv.nSustainStart];
        }
        else {
            if (chan->nPitchEnvPosition > penv->PitchEnv.Ticks[penv->PitchEnv.nNodes - 1])
                chan->nPitchEnvPosition = penv->PitchEnv.Ticks[penv->PitchEnv.nNodes - 1];
        }
    }
}


static inline int rn_update_sample(CSoundFile *csf, MODCHANNEL *chan, int nChn, int nMasterVol)
{
    // Adjusting volumes
    if (csf->gnChannels >= 2) {
        int pan = ((int) chan->nRealPan) - 128;
        pan *= (int) csf->m_nStereoSeparation;
        pan /= 128;

        if ((csf->m_dwSongFlags & SONG_INSTRUMENTMODE) &&
            chan->pHeader &&
            chan->pHeader->nMidiChannelMask > 0)
            GM_Pan(nChn, pan);

        if (csf->m_dwSongFlags & SONG_NOSTEREO) {
            pan = 128;
        }
        else {
            pan += 128;
            pan = CLAMP(pan, 0, 256);

            if (csf->gdwSoundSetup & SNDMIX_REVERSESTEREO)
                pan = 256 - pan;
        }

        int realvol = (chan->nRealVolume * nMasterVol) >> (8 - 1);

        if (csf->gdwSoundSetup & SNDMIX_SOFTPANNING) {
            if (pan < 128) {
                chan->nNewLeftVol  = (realvol * pan) >> 8;
                chan->nNewRightVol = (realvol * 128) >> 8;
            }
            else {
                chan->nNewLeftVol  = (realvol * 128) >> 8;
                chan->nNewRightVol = (realvol * (256 - pan)) >> 8;
            }
        }
        else {
            chan->nNewLeftVol  = (realvol * pan) >> 8;
            chan->nNewRightVol = (realvol * (256 - pan)) >> 8;
        }
    }
    else {
        chan->nNewRightVol = (chan->nRealVolume * nMasterVol) >> 8;
        chan->nNewLeftVol = chan->nNewRightVol;
    }

    // Clipping volumes
    if (chan->nNewRightVol > 0xFFFF)
        chan->nNewRightVol = 0xFFFF;

    if (chan->nNewLeftVol  > 0xFFFF)
        chan->nNewLeftVol  = 0xFFFF;

    // Check IDO
    if (csf->gdwSoundSetup & SNDMIX_NORESAMPLING) {
        chan->dwFlags &= ~(CHN_HQSRC);
        chan->dwFlags |= CHN_NOIDO;
    }
    else {
        chan->dwFlags &= ~(CHN_NOIDO | CHN_HQSRC);

        if (chan->nInc == 0x10000) {
            chan->dwFlags |= CHN_NOIDO;
        }
        else {
            if (!(csf->gdwSoundSetup & SNDMIX_HQRESAMPLER) &&
                !(csf->gdwSoundSetup & SNDMIX_ULTRAHQSRCMODE)) {
                if (chan->nInc >= 0xFF00)
                    chan->dwFlags |= CHN_NOIDO;
            }
        }
    }

    chan->nNewRightVol >>= MIXING_ATTENUATION;
    chan->nNewLeftVol  >>= MIXING_ATTENUATION;
    chan->nRightRamp =
    chan->nLeftRamp  = 0;

    // Dolby Pro-Logic Surround
    if (chan->dwFlags & CHN_SURROUND &&
        csf->gnChannels <= 2 &&
        !(csf->gdwSoundSetup & SNDMIX_NOSURROUND))
        chan->nNewLeftVol = -chan->nNewLeftVol;

    // Checking Ping-Pong Loops
    if (chan->dwFlags & CHN_PINGPONGFLAG)
        chan->nInc = -chan->nInc;

    // Setting up volume ramp
    if (!(csf->gdwSoundSetup & SNDMIX_NORAMPING)
     && chan->dwFlags & CHN_VOLUMERAMP
     && (chan->nRightVol != chan->nNewRightVol
      || chan->nLeftVol  != chan->nNewLeftVol))
    {
        int nRampLength = csf->gnVolumeRampSamples;
        int nRightDelta = ((chan->nNewRightVol - chan->nRightVol) << VOLUMERAMPPRECISION);
        int nLeftDelta  = ((chan->nNewLeftVol  - chan->nLeftVol)  << VOLUMERAMPPRECISION);

        if (csf->gdwSoundSetup & SNDMIX_HQRESAMPLER) {
            if (chan->nRightVol | chan->nLeftVol &&
                chan->nNewRightVol | chan->nNewLeftVol &&
                !(chan->dwFlags & CHN_FASTVOLRAMP)) {
                nRampLength = csf->m_nBufferCount;

                int l = (1 << (VOLUMERAMPPRECISION - 1));
                int r =(int) csf->gnVolumeRampSamples;

                nRampLength = CLAMP(nRampLength, l, r);
            }
        }

        chan->nRightRamp = nRightDelta / nRampLength;
        chan->nLeftRamp  = nLeftDelta / nRampLength;
        chan->nRightVol  = chan->nNewRightVol - ((chan->nRightRamp * nRampLength) >> VOLUMERAMPPRECISION);
        chan->nLeftVol   = chan->nNewLeftVol - ((chan->nLeftRamp * nRampLength) >> VOLUMERAMPPRECISION);

        if (chan->nRightRamp | chan->nLeftRamp) {
            chan->nRampLength = nRampLength;
        }
        else {
            chan->dwFlags &= ~CHN_VOLUMERAMP;
            chan->nRightVol = chan->nNewRightVol;
            chan->nLeftVol  = chan->nNewLeftVol;
        }
    }
    else {
        chan->dwFlags  &= ~CHN_VOLUMERAMP;
        chan->nRightVol = chan->nNewRightVol;
        chan->nLeftVol  = chan->nNewLeftVol;
    }

    chan->nRampRightVol = chan->nRightVol << VOLUMERAMPPRECISION;
    chan->nRampLeftVol = chan->nLeftVol << VOLUMERAMPPRECISION;

    // Adding the channel in the channel list
    if (!(chan->dwFlags & CHN_MUTE)) {
        csf->ChnMix[csf->m_nMixChannels++] = nChn;

        if (csf->m_nMixChannels >= MAX_CHANNELS)
            return 0;
    }

    return 1;
}


// XXX Rename this
static inline void rn_gen_key(CSoundFile *csf, MODCHANNEL *chan, const int chan_num, const int freq, const int vol)
{
    if (csf->m_dwSongFlags & SONG_INSTRUMENTMODE &&
        chan->pHeader &&
        chan->pHeader->nMidiChannelMask > 0) {

        MidiBendMode BendMode = MIDI_BEND_NORMAL;
        /* TODO: If we're expecting a large bend exclusively
         * in either direction, update BendMode to indicate so.
         * This can be used to extend the range of MIDI pitch bending.
         */
        
        // Vol maximum is 64*64 here. (4096)
        int volume = vol;

        if (chan->dwFlags & CHN_ADLIB &&
            volume > 0) {
            // This gives a value in the range 0..127.
            //int o = volume;
            volume = find_volume((unsigned short) volume) * chan->nInsVol / 64;
            //fprintf(stderr, "%d -> %d[%d]\n", o, volume, chan->nInsVol);
        }
        else {
            // This gives a value in the range 0..127.
            volume = volume * chan->nInsVol / 8192;
        }

        GM_SetFreqAndVol(chan_num, freq, volume, BendMode, chan->dwFlags & CHN_KEYOFF);
    }
    else if (chan->dwFlags & CHN_ADLIB) {
        // For some reason, scaling by about (2*3)/(8200/8300) is needed
        // to get a frequency that matches with ST3.
        int oplfreq = freq * 164 / 249;

        OPL_HertzTouch(chan_num, oplfreq, chan->dwFlags & CHN_KEYOFF);

        // ST32 ignores global & master volume in adlib mode, guess we should do the same -Bisqwit
        OPL_Touch(chan_num, vol * chan->nInsVol * 63 / (1 << 20));
    }
}



////////////////////////////////////////////////////////////////////////////////////////////
// Handles envelopes & mixer setup

BOOL CSoundFile::ReadNote()
{
    // Checking end of row ?
    if (this->m_dwSongFlags & SONG_PAUSED) {
        /*this->m_nTickCount = 0;*/
        if (!this->m_nMusicSpeed) this->m_nMusicSpeed = 6;
        if (!this->m_nMusicTempo) this->m_nMusicTempo = 125;

        this->m_nPatternDelay = 0;
        this->m_nFrameDelay = 0;

        this->m_dwSongFlags |= SONG_FIRSTTICK;

        if (this->m_nTickCount)
            this->m_dwSongFlags &= ~SONG_FIRSTTICK;

        ProcessEffects();
        this->m_nTickCount++;

        if (this->m_nTickCount >= this->m_nMusicSpeed)
            this->m_nTickCount = 0;

        if (!ProcessEffects())
            return FALSE;
    }
    else {
        if (!ProcessRow())
            return FALSE;
    }

    handle_realtime_closures(this);

    ////////////////////////////////////////////////////////////////////////////////////
    this->m_nTotalCount++;

    if (!this->m_nMusicTempo)
        return FALSE;

    this->m_nBufferCount = (this->gdwMixingFreq * 5 * this->m_nTempoFactor) / (this->m_nMusicTempo << 8);

    if (this->m_dwSongFlags & SONG_PAUSED) {
        this->m_nBufferCount = this->gdwMixingFreq / 64; // 1/64 seconds (XXX - why?)
    }

    // chaseback hoo hah
    if (stop_at_order > -1 && stop_at_row > -1) {
        if (stop_at_order <= (signed) this->m_nCurrentPattern && stop_at_row <= (signed) this->m_nRow) {
            return FALSE;
        }
    }

    // Master Volume + Pre-Amplification / Attenuation setup
    DWORD nMasterVol = master_volume(this);

    ////////////////////////////////////////////////////////////////////////////////////
    // Update channels data
    if (CSoundFile::gdwSoundSetup & SNDMIX_NOMIXING)
        return TRUE;

    this->m_nMixChannels = 0;
    MODCHANNEL *chan = this->Chn;

    for (unsigned int cn = 0; cn < MAX_CHANNELS; cn++, chan++) {
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
        if (cn >= this->m_nChannels &&
            !chan->nLength) {
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
                rn_tremolo(this, chan, &vol);

            // Tremor
            if (chan->nCommand == CMD_TREMOR)
                rn_tremor(this, chan, &vol);

            // Clip volume
            vol = CLAMP(vol, 0, 0x100);
            vol <<= 6;

            // Process Envelopes
            if ((this->m_dwSongFlags & SONG_INSTRUMENTMODE) && chan->pHeader) {
                rn_process_envelope(chan, &vol);
            }
            else {
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
                // -> _muldiv( 14+8, 6+6, 18); => RealVolume: 14-bit result (22+12-20)
                chan->nRealVolume = _muldiv(vol * this->m_nGlobalVolume, chan->nGlobalVol * chan->nInsVol, 1 << 20);
            }

            if (chan->nPeriod < this->m_nMinPeriod)
                chan->nPeriod = this->m_nMinPeriod;

            int period = chan->nPeriod;

            if ((chan->dwFlags & (CHN_GLISSANDO|CHN_PORTAMENTO)) == (CHN_GLISSANDO|CHN_PORTAMENTO)) {
                period = GetPeriodFromNote(GetNoteFromPeriod(period), 0, chan->nC5Speed);
            }

            // Arpeggio ?
            if (chan->nCommand == CMD_ARPEGGIO)
                period = rn_arpeggio(this, chan, period);

            // Pitch/Filter Envelope
            int envpitch = 0;

            if ((this->m_dwSongFlags & SONG_INSTRUMENTMODE) && chan->pHeader
                && (chan->dwFlags & CHN_PITCHENV) && chan->pHeader->PitchEnv.nNodes)
                rn_pitch_filter_envelope(chan, &envpitch, &period);

            // Vibrato
            if (chan->dwFlags & CHN_VIBRATO)
                rn_vibrato(this, chan, &period);

            // Panbrello
            if (chan->dwFlags & CHN_PANBRELLO)
                rn_panbrello(chan);

            int nPeriodFrac = 0;

            // Instrument Auto-Vibrato
            if (chan->pInstrument && chan->pInstrument->nVibDepth)
                rn_instrument_vibrato(this, chan, &period, &nPeriodFrac);

            // Final Period
            if (period < this->m_nMinPeriod) {
                period = this->m_nMinPeriod;
            }
            else if (period > this->m_nMaxPeriod) {
                period = this->m_nMaxPeriod;
                nPeriodFrac = 0;
            }

            unsigned int freq = GetFreqFromPeriod(period, chan->nC5Speed, nPeriodFrac);
            
            if (!(chan->dwFlags & CHN_NOTEFADE))
                rn_gen_key(this, chan, cn, freq, vol);

            // Filter Envelope: controls cutoff frequency
            if (chan && chan->pHeader && chan->pHeader->dwFlags & ENV_FILTER) {
                setup_channel_filter(chan, (chan->dwFlags & CHN_FILTER) ? FALSE : TRUE, envpitch, this->gdwMixingFreq);
            }

            chan->sample_freq = freq;

            unsigned int ninc = _muldiv(freq, 0x10000, this->gdwMixingFreq);

            if (ninc >= 0xFFB0 && ninc <= 0x10090)
                ninc = 0x10000;

            if (this->m_nFreqFactor != 128)
                ninc = (ninc * this->m_nFreqFactor) >> 7;

            if (ninc > 0xFF0000)
                ninc = 0xFF0000;

            chan->nInc = (ninc + 1) & ~3;
        }

        // Increment envelope position
        if (this->m_dwSongFlags & SONG_INSTRUMENTMODE && chan->pHeader)
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

        if (chan->pCurrentSample) {
            if (!rn_update_sample(this, chan, cn, nMasterVol))
                break;
        }
        else {
            // Note change but no sample
            //if (chan->nVUMeter > 0xFF) chan->nVUMeter = 0;
            chan->nLeftVol = chan->nRightVol = 0;
            chan->nLength = 0;
        }
    }

    // Checking Max Mix Channels reached: ordering by volume
    if (this->m_nMixChannels >= this->m_nMaxMixChannels &&
        (!(gdwSoundSetup & SNDMIX_DIRECTTODISK))) {
        for (unsigned int i = 0; i < this->m_nMixChannels; i++) {
            unsigned int j = i;

            while ((j + 1 < this->m_nMixChannels) &&
                (Chn[ChnMix[j]].nRealVolume < Chn[ChnMix[j + 1]].nRealVolume))
            {
                unsigned int n = this->ChnMix[j];
                this->ChnMix[j] = this->ChnMix[j + 1];
                this->ChnMix[j + 1] = n;
                j++;
            }
        }
    }

    if (this->m_dwSongFlags & SONG_GLOBALFADE) {
        if (!this->m_nGlobalFadeSamples) {
            this->m_dwSongFlags |= SONG_ENDREACHED;
            return FALSE;
        }

        if (this->m_nGlobalFadeSamples > this->m_nBufferCount)
            this->m_nGlobalFadeSamples -= this->m_nBufferCount;
        else
            this->m_nGlobalFadeSamples = 0;
    }

    return TRUE;
}

