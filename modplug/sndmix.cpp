/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/

#include "stdafx.h"
#include "sndfile.h"


// Volume ramp length, in 1/10 ms
#define VOLUMERAMPLEN	146	// 1.46ms = 64 samples at 44.1kHz

// VU-Meter
#define VUMETER_DECAY		16

// SNDMIX: These are global flags for playback control
LONG CSoundFile::m_nStreamVolume = 0x8000;
UINT CSoundFile::m_nMaxMixChannels = 32;
// Mixing Configuration (SetWaveConfig)
DWORD CSoundFile::gdwSysInfo = 0;
DWORD CSoundFile::gnChannels = 1;
DWORD CSoundFile::gdwSoundSetup = 0;
DWORD CSoundFile::gdwMixingFreq = 44100;
DWORD CSoundFile::gnBitsPerSample = 16;
// Mixing data initialized in
UINT CSoundFile::gnAGC = AGC_UNITY;
UINT CSoundFile::gnVolumeRampSamples = 64;
UINT CSoundFile::gnVULeft = 0;
UINT CSoundFile::gnVURight = 0;
UINT CSoundFile::gnCPUUsage = 0;
LPSNDMIXHOOKPROC CSoundFile::gpSndMixHook = NULL;
PMIXPLUGINCREATEPROC CSoundFile::gpMixPluginCreateProc = NULL;
LONG gnDryROfsVol = 0;
LONG gnDryLOfsVol = 0;
LONG gnRvbROfsVol = 0;
LONG gnRvbLOfsVol = 0;
int gbInitPlugins = 0;

typedef DWORD (MPPASMCALL * LPCONVERTPROC)(LPVOID, int *, DWORD, LPLONG, LPLONG);

extern DWORD MPPASMCALL Convert32To8(LPVOID lpBuffer, int *, DWORD nSamples, LONG mins[2], LONG maxs[2]);
extern DWORD MPPASMCALL Convert32To16(LPVOID lpBuffer, int *, DWORD nSamples, LONG mins[2], LONG maxs[2]);
extern DWORD MPPASMCALL Convert32To24(LPVOID lpBuffer, int *, DWORD nSamples, LONG mins[2], LONG maxs[2]);
extern DWORD MPPASMCALL Convert32To32(LPVOID lpBuffer, int *, DWORD nSamples, LONG mins[2], LONG maxs[2]);
extern UINT MPPASMCALL AGC(int *pBuffer, UINT nSamples, UINT nAGC);
extern VOID MPPASMCALL Dither(int *pBuffer, UINT nSamples, UINT nBits);
extern VOID MPPASMCALL InterleaveFrontRear(int *pFrontBuf, int *pRearBuf, DWORD nSamples);
extern VOID MPPASMCALL StereoFill(int *pBuffer, UINT nSamples, LPLONG lpROfs, LPLONG lpLOfs);
extern VOID MPPASMCALL MonoFromStereo(int *pMixBuf, UINT nSamples);

extern short int ModSinusTable[64];
extern short int ModRampDownTable[64];
extern short int ModSquareTable[64];
extern short int ModRandomTable[64];
extern DWORD LinearSlideUpTable[256];
extern DWORD LinearSlideDownTable[256];
extern DWORD FineLinearSlideUpTable[16];
extern DWORD FineLinearSlideDownTable[16];
extern signed char ft2VibratoTable[256];	// -64 .. +64
extern int MixSoundBuffer[MIXBUFFERSIZE*4];
extern int MixRearBuffer[MIXBUFFERSIZE*2];
UINT gnReverbSend;


// Log tables for pre-amp
// We don't want the tracker to get too loud
const UINT PreAmpTable[16] =
{
	0x60, 0x60, 0x60, 0x70,	// 0-7
	0x80, 0x88, 0x90, 0x98,	// 8-15
	0xA0, 0xA4, 0xA8, 0xB0,	// 16-23
	0xB4, 0xB8, 0xBC, 0xC0,	// 24-31
};

const UINT PreAmpAGCTable[16] =
{
	0x60, 0x60, 0x60, 0x60,
	0x68, 0x70, 0x78, 0x80,
	0x84, 0x88, 0x8C, 0x90,
	0x94, 0x98, 0x9C, 0xA0,
};


BOOL CSoundFile::InitPlayer(BOOL bReset)
//--------------------------------------
{
	if (m_nMaxMixChannels > MAX_CHANNELS) m_nMaxMixChannels = MAX_CHANNELS;
	if (gdwMixingFreq < 4000) gdwMixingFreq = 4000;
	if (gdwMixingFreq > MAX_SAMPLE_RATE) gdwMixingFreq = MAX_SAMPLE_RATE;
	gnVolumeRampSamples = (gdwMixingFreq * VOLUMERAMPLEN) / 100000;
	if (gnVolumeRampSamples < 8) gnVolumeRampSamples = 8;
	gnDryROfsVol = gnDryLOfsVol = 0;
	gnRvbROfsVol = gnRvbLOfsVol = 0;
	if (bReset)
	{
		gnVULeft = 0;
		gnVURight = 0;
		gnCPUUsage = 0;
	}
	gbInitPlugins = (bReset) ? 3 : 1;
	InitializeDSP(bReset);
	InitializeEQ(bReset);
	return TRUE;
}


BOOL CSoundFile::FadeSong(UINT msec)
//----------------------------------
{
	LONG nsamples = _muldiv(msec, gdwMixingFreq, 1000);
	if (nsamples <= 0) return FALSE;
	if (nsamples > 0x100000) nsamples = 0x100000;
	m_nBufferCount = nsamples;
	LONG nRampLength = m_nBufferCount;
	// Ramp everything down
	for (UINT noff=0; noff < m_nMixChannels; noff++)
	{
		MODCHANNEL *pramp = &Chn[ChnMix[noff]];
		if (!pramp) continue;
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


BOOL CSoundFile::GlobalFadeSong(UINT msec)
//----------------------------------------
{
	if (m_dwSongFlags & SONG_GLOBALFADE) return FALSE;
	m_nGlobalFadeMaxSamples = _muldiv(msec, gdwMixingFreq, 1000);
	m_nGlobalFadeSamples = m_nGlobalFadeMaxSamples;
	m_dwSongFlags |= SONG_GLOBALFADE;
	return TRUE;
}


UINT CSoundFile::Read(LPVOID lpDestBuffer, UINT cbBuffer)
//-------------------------------------------------------
{
	LPBYTE lpBuffer = (LPBYTE)lpDestBuffer;
	LPCONVERTPROC pCvt = Convert32To8;
	LONG vu_min[2];
	LONG vu_max[2];
	UINT lRead, lMax, lSampleSize, lCount, lSampleCount, nStat=0;
#if 0
	UINT nMaxPlugins;
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
	if (gnBitsPerSample == 16) { lSampleSize *= 2; pCvt = Convert32To16; }
	else if (gnBitsPerSample == 24) { lSampleSize *= 3; pCvt = Convert32To24; }
	else if (gnBitsPerSample == 32) { lSampleSize *= 4; pCvt = Convert32To32; }
	lMax = cbBuffer / lSampleSize;
	if ((!lMax) || (!lpBuffer) || (!m_nChannels)) return 0;
	lRead = lMax;
	if (m_dwSongFlags & SONG_ENDREACHED) goto MixDone;
	while (lRead > 0)
	{
		// Update Channel Data
		UINT lTotalSampleCount;
		if (!m_nBufferCount)
		{
			if (!(gdwSoundSetup & SNDMIX_DIRECTTODISK))
				m_nBufferCount = lRead;
			if (!ReadNote()) {
				m_dwSongFlags |= SONG_ENDREACHED;
				if (stop_at_order > -1) return 0; /* faster */
				if (lRead == lMax) goto MixDone;
				if (!(gdwSoundSetup & SNDMIX_DIRECTTODISK))
					m_nBufferCount = lRead;
			}
			if (!m_nBufferCount) goto MixDone;
		}
		lCount = m_nBufferCount;
		if (lCount > MIXBUFFERSIZE) lCount = MIXBUFFERSIZE;
		if (lCount > lRead) lCount = lRead;
		if (!lCount) break;
		lSampleCount = lCount;
#ifndef MODPLUG_NO_REVERB
		gnReverbSend = 0;
#endif

		// Resetting sound buffer
		StereoFill(MixSoundBuffer, lSampleCount, &gnDryROfsVol, &gnDryLOfsVol);
		if (gnChannels >= 2)
		{
			lSampleCount *= 2;
			m_nMixStat += CreateStereoMix(lCount);
#if 0
			if (nMaxPlugins) ProcessPlugins(lCount);
#endif
			ProcessStereoDSP(lCount);
		} else
		{
			m_nMixStat += CreateStereoMix(lCount);
#if 0
			if (nMaxPlugins) ProcessPlugins(lCount);
#endif
			MonoFromStereo(MixSoundBuffer, lCount);
			ProcessMonoDSP(lCount);
		}

		if (gdwSoundSetup & SNDMIX_EQ)
		{
			if (gnChannels >= 2)
				EQStereo(MixSoundBuffer, lCount);
			else
				EQMono(MixSoundBuffer, lCount);
		}


		nStat++;
#ifndef NO_AGC
		// Automatic Gain Control
		if (gdwSoundSetup & SNDMIX_AGC) ProcessAGC(lSampleCount);
#endif
		lTotalSampleCount = lSampleCount;
		// Multichannel
		if (gnChannels > 2)
		{
			InterleaveFrontRear(MixSoundBuffer, MixRearBuffer, lSampleCount);
			lTotalSampleCount *= 2;
		}
		// Hook Function
		if (gpSndMixHook)
		{
			gpSndMixHook(MixSoundBuffer, lTotalSampleCount, gnChannels);
		}
		// Perform clipping + VU-Meter
		lpBuffer += pCvt(lpBuffer, MixSoundBuffer, lTotalSampleCount, vu_min, vu_max);
		// Buffer ready
		lRead -= lCount;
		m_nBufferCount -= lCount;
	}
MixDone:
	if (lRead) memset(lpBuffer, (gnBitsPerSample == 8) ? 0x80 : 0, lRead * lSampleSize);
	// VU-Meter
	vu_min[0] >>= (24-MIXING_ATTENUATION);
	vu_min[1] >>= (24-MIXING_ATTENUATION);
	vu_max[0] >>= (24-MIXING_ATTENUATION);
	vu_max[1] >>= (24-MIXING_ATTENUATION);
	if (vu_max[0] < vu_min[0]) vu_max[0] = vu_min[0];
	if (vu_max[1] < vu_min[1]) vu_max[1] = vu_min[1];
	if ((gnVULeft = (UINT)(vu_max[0] - vu_min[0])) > 0xFF)
		gnVULeft = 0xFF;
	if ((gnVURight = (UINT)(vu_max[1] - vu_min[1])) > 0xFF)
		gnVURight = 0xFF;
	if (nStat) { m_nMixStat += nStat-1; m_nMixStat /= nStat; }
	return lMax - lRead;
}



/////////////////////////////////////////////////////////////////////////////
// Handles navigation/effects

BOOL CSoundFile::ProcessRow()
//---------------------------
{
	if (++m_nTickCount >= m_nMusicSpeed * (m_nPatternDelay+1) + m_nFrameDelay)
        {
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
			} else {
				m_nCurrentPattern = m_nNextPattern;
			}

			// Check if pattern is valid
			if (!(m_dwSongFlags & SONG_PATTERNLOOP))
			{
				m_nPattern = (m_nCurrentPattern < MAX_ORDERS) ? Order[m_nCurrentPattern] : 0xFF;
				if ((m_nPattern < MAX_PATTERNS) && (!Patterns[m_nPattern])) m_nPattern = 0xFE;
				while (m_nPattern >= MAX_PATTERNS)
				{
					// End of song ?
					if ((m_nPattern == 0xFF) || (m_nCurrentPattern >= MAX_ORDERS))
					{
						if (m_nRepeatCount > 0) m_nRepeatCount--;
						if (!m_nRepeatCount) return FALSE;
						m_nCurrentPattern = m_nRestartPos;
						if ((Order[m_nCurrentPattern] >= MAX_PATTERNS)
						    || (!Patterns[Order[m_nCurrentPattern]]))
							return FALSE;
					} else {
						m_nCurrentPattern++;
					}
					m_nPattern = (m_nCurrentPattern < MAX_ORDERS) ? Order[m_nCurrentPattern] : 0xFF;
					if ((m_nPattern < MAX_PATTERNS) && (!Patterns[m_nPattern])) m_nPattern = 0xFE;
				}
				m_nNextPattern = m_nCurrentPattern;
			} else if (m_nCurrentPattern < 255) {
				if (m_nRepeatCount > 0) m_nRepeatCount--;
				if (!m_nRepeatCount) return FALSE;
			}
		}
#ifdef MODPLUG_TRACKER
		if (m_dwSongFlags & SONG_STEP)
		{
			m_dwSongFlags &= ~SONG_STEP;
			m_dwSongFlags |= SONG_PAUSED;
		}
#endif // MODPLUG_TRACKER
		// Weird stuff?
		if ((m_nPattern >= MAX_PATTERNS) || (!Patterns[m_nPattern])) return FALSE;
		// Should never happen
		// ... sure it should: suppose there's a C70 effect before a 64-row pattern.
		// It's in fact very easy to make this happen ;)
		//       - chisel
		if (m_nRow >= PatternSize[m_nPattern]) m_nRow = 0;
                m_nNextRow = m_nRow + 1;
		if (m_nNextRow >= PatternSize[m_nPattern])
		{
			if (!(m_dwSongFlags & SONG_PATTERNLOOP)) m_nNextPattern = m_nCurrentPattern + 1;
			else if (m_nRepeatCount > 0) return FALSE;
			m_nNextRow = 0;
		}
		// Reset channel values
		MODCHANNEL *pChn = Chn;
		MODCOMMAND *m = Patterns[m_nPattern] + m_nRow * m_nChannels;
		for (UINT nChn=0; nChn<m_nChannels; pChn++, nChn++, m++)
		{
			/* skip realtime copyin */
			if (pChn->nRealtime) continue;

			// this is where we're going to spit out our midi
			// commands... ALL WE DO is dump raw midi data to
			// our super-secret "midi buffer"
			// -mrsb
			if (_midi_out_note)
				_midi_out_note(nChn, m);

			pChn->nRowNote = m->note;
			if (m->instr) pChn->nLastInstr = m->instr;
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
				
	} else if (_midi_out_note) {
		MODCOMMAND *m = Patterns[m_nPattern] + m_nRow * m_nChannels;
		for (UINT nChn=0; nChn<m_nChannels; nChn++, m++)
		{
			/* m==NULL allows schism to receive notification of SDx and Scx commands */
			_midi_out_note(nChn, 0);
		}
	}
	// Should we process tick0 effects?
	if (!m_nMusicSpeed) m_nMusicSpeed = 1;
	m_dwSongFlags |= SONG_FIRSTTICK;
	if (m_nTickCount)
	{
		m_dwSongFlags &= ~SONG_FIRSTTICK;
		if ((!(m_nType & MOD_TYPE_XM)) && (m_nTickCount < m_nMusicSpeed * (1 + m_nPatternDelay)))
		{
			if (!(m_nTickCount % m_nMusicSpeed)) m_dwSongFlags |= SONG_FIRSTTICK;
		}

	}
	// Update Effects
	return ProcessEffects();
}


////////////////////////////////////////////////////////////////////////////////////////////
// Handles envelopes & mixer setup

BOOL CSoundFile::ReadNote()
//-------------------------
{
	// Checking end of row ?
	if (m_dwSongFlags & SONG_PAUSED)
	{
		/*m_nTickCount = 0;*/
		if (!m_nMusicSpeed) m_nMusicSpeed = 6;
		if (!m_nMusicTempo) m_nMusicTempo = 125;
		m_nPatternDelay = 0;
		m_nFrameDelay = 0;

		m_dwSongFlags |= SONG_FIRSTTICK;
		if (m_nTickCount) {
			m_dwSongFlags &= ~SONG_FIRSTTICK;
		}

		ProcessEffects();
		m_nTickCount++;
		if (m_nTickCount >= m_nMusicSpeed) {
			m_nTickCount = 0;
		}
		if (!ProcessEffects()) return FALSE;
	} else
	{
		if (!ProcessRow()) return FALSE;
	}

	{ /* handle realtime closures */
		MODCHANNEL *pChn = Chn;
		for (UINT nChn=0; nChn<m_nChannels; pChn++, nChn++) {
			/* reset end of "row" */
			if (pChn->nRealtime && pChn->nRowNote && (pChn->nTickStart % m_nMusicSpeed) == (m_nTickCount % m_nMusicSpeed)) {
				pChn->nRealtime = 0;
				pChn->nRowNote = 0;
				pChn->nRowInstr = 0;
				//pChn->nMaster
				pChn->nRowVolCmd = 0;
				pChn->nRowVolume = 0;
				pChn->nRowCommand = 0;
				pChn->nRowParam = 0;
				pChn->nTickStart = 0;
			}
		}
	};

	////////////////////////////////////////////////////////////////////////////////////
	m_nTotalCount++;
	if (!m_nMusicTempo) return FALSE;
	m_nBufferCount = (gdwMixingFreq * 5 * m_nTempoFactor) / (m_nMusicTempo << 8);
#ifdef MODPLUG_TRACKER
	if (m_dwSongFlags & SONG_PAUSED)
	{
		m_nBufferCount = gdwMixingFreq / 64; // 1/64 seconds
	}
#endif

	// chaseback hoo hah
	if (stop_at_order > -1 && stop_at_row > -1) {
		if (stop_at_order <= (signed) m_nCurrentPattern && stop_at_row <= (signed) m_nRow) {
			return FALSE;
		}
	}

	// Master Volume + Pre-Amplification / Attenuation setup
	DWORD nMasterVol;
	{
		int nchn32 = 0;
		MODCHANNEL *pChn = Chn;	
		for (UINT nChn=0; nChn<m_nChannels; nChn++, pChn++) {
			nchn32++;
		}
		if (nchn32 < 1) nchn32 = 1;
		if (nchn32 > 31) nchn32 = 31;

#if 0
		int nchn32 = (m_nChannels < 32) ? m_nChannels : 31;
		if ((m_nType & MOD_TYPE_IT) && (m_dwSongFlags & SONG_INSTRUMENTMODE) && (nchn32 < 6)) nchn32 = 6;
#endif

		int realmastervol = m_nMasterVolume;
		if (realmastervol > 0x80)
		{
			realmastervol = 0x80 + ((realmastervol - 0x80) * (nchn32+4)) / 16;
		}

		DWORD mastervol = (realmastervol * (m_nSongPreAmp)) >> 6;
//		if (mastervol > 0x200) mastervol = 0x200;
		if ((m_dwSongFlags & SONG_GLOBALFADE) && (m_nGlobalFadeMaxSamples))
		{
			mastervol = _muldiv(mastervol, m_nGlobalFadeSamples, m_nGlobalFadeMaxSamples);
		}

		UINT attenuation = (gdwSoundSetup & SNDMIX_AGC) ? PreAmpAGCTable[nchn32>>1] : PreAmpTable[nchn32>>1];
		if (attenuation < 1) attenuation = 1;

		nMasterVol = (mastervol << 7) / attenuation;
		if (nMasterVol > 0x180) nMasterVol = 0x180;
	}
	////////////////////////////////////////////////////////////////////////////////////
	// Update channels data
	if (CSoundFile::gdwSoundSetup & SNDMIX_NOMIXING) return TRUE;
	m_nMixChannels = 0;
	MODCHANNEL *pChn = Chn;
	for (UINT nChn=0; nChn<MAX_CHANNELS; nChn++,pChn++)
	{
		if ((pChn->dwFlags & CHN_NOTEFADE) && (!(pChn->nFadeOutVol|pChn->nRightVol|pChn->nLeftVol)))
		{
			pChn->nLength = 0;
			pChn->nROfs = pChn->nLOfs = 0;
		}
		// Check for unused channel
		if ((nChn >= m_nChannels) && (!pChn->nLength))
		{
			pChn->nLeftVU = pChn->nRightVU = 0;
			continue;
		}
		// Reset channel data
		pChn->nInc = 0;
		pChn->nRealVolume = 0;
		pChn->nRealPan = pChn->nPan + pChn->nPanSwing;
		if (pChn->nRealPan < 0) pChn->nRealPan = 0;
		if (pChn->nRealPan > 256) pChn->nRealPan = 256;
		pChn->nRampLength = 0;
		// Calc Frequency
		if ((pChn->nPeriod)	&& (pChn->nLength))
		{
			int vol = pChn->nVolume + pChn->nVolSwing;

			if (vol < 0) vol = 0;
			if (vol > 256) vol = 256;
			// Tremolo
			if (pChn->dwFlags & CHN_TREMOLO)
			{
				UINT trempos = pChn->nTremoloPos & 0x3F;
				if (vol > 0)
				{
					int tremattn = (m_nType & MOD_TYPE_XM) ? 5 : 6;
					switch (pChn->nTremoloType & 0x03)
					{
					case 1:
						vol += (ModRampDownTable[trempos] * (int)pChn->nTremoloDepth) >> tremattn;
						break;
					case 2:
						vol += (ModSquareTable[trempos] * (int)pChn->nTremoloDepth) >> tremattn;
						break;
					case 3:
						vol += (ModRandomTable[trempos] * (int)pChn->nTremoloDepth) >> tremattn;
						break;
					default:
						vol += (ModSinusTable[trempos] * (int)pChn->nTremoloDepth) >> tremattn;
					}
				}
				if ((m_nTickCount) || ((m_nType & (MOD_TYPE_STM|MOD_TYPE_S3M|MOD_TYPE_IT)) && (!(m_dwSongFlags & SONG_ITOLDEFFECTS))))
				{
					pChn->nTremoloPos = (trempos + pChn->nTremoloSpeed) & 0x3F;
				}
			}
			// Tremor
			if (pChn->nCommand == CMD_TREMOR)
			{
				UINT n = (pChn->nTremorParam >> 4) + (pChn->nTremorParam & 0x0F);
				UINT ontime = pChn->nTremorParam >> 4;
				if ((!(m_nType & MOD_TYPE_IT)) || (m_dwSongFlags & SONG_ITOLDEFFECTS)) { n += 2; ontime++; }
				UINT tremcount = (UINT)pChn->nTremorCount;
				if (tremcount >= n) tremcount = 0;
				if ((m_nTickCount) || (m_nType & (MOD_TYPE_S3M|MOD_TYPE_IT)))
				{
					if (tremcount >= ontime) vol = 0;
					pChn->nTremorCount = (BYTE)(tremcount + 1);
				}
				pChn->dwFlags |= CHN_FASTVOLRAMP;
			}
			// Clip volume
			if (vol < 0) vol = 0;
			if (vol > 0x100) vol = 0x100;
			vol <<= 6;
			// Process Envelopes
			if ((m_dwSongFlags & SONG_INSTRUMENTMODE) && pChn->pHeader)
			{
				INSTRUMENTHEADER *penv = pChn->pHeader;
				// Volume Envelope
				if ((pChn->dwFlags & CHN_VOLENV) && (penv->VolEnv.nNodes))
				{
					int envpos = pChn->nVolEnvPosition;
					UINT pt = penv->VolEnv.nNodes - 1;
					for (UINT i=0; i<(UINT)(penv->VolEnv.nNodes-1); i++)
					{
						if (envpos <= penv->VolEnv.Ticks[i])
						{
							pt = i;
							break;
						}
					}
					int x2 = penv->VolEnv.Ticks[pt];
					int x1, envvol;
					if (envpos >= x2)
					{
						envvol = penv->VolEnv.Values[pt] << 2;
						x1 = x2;
					} else
					if (pt)
					{
						envvol = penv->VolEnv.Values[pt-1] << 2;
						x1 = penv->VolEnv.Ticks[pt-1];
					} else
					{
						envvol = 0;
						x1 = 0;
					}
					if (envpos > x2) envpos = x2;
					if ((x2 > x1) && (envpos > x1))
					{
						envvol += ((envpos - x1) * (((int)penv->VolEnv.Values[pt]<<2) - envvol)) / (x2 - x1);
					}
					if (envvol < 0) envvol = 0;
					if (envvol > 256) envvol = 256;
					vol = (vol * envvol) >> 8;
				}
				// Panning Envelope
				if ((pChn->dwFlags & CHN_PANENV) && (penv->PanEnv.nNodes))
				{
					int envpos = pChn->nPanEnvPosition;
					UINT pt = penv->PanEnv.nNodes - 1;
					for (UINT i=0; i<(UINT)(penv->PanEnv.nNodes-1); i++)
					{
						if (envpos <= penv->PanEnv.Ticks[i])
						{
							pt = i;
							break;
						}
					}
					int x2 = penv->PanEnv.Ticks[pt], y2 = penv->PanEnv.Values[pt];
					int x1, envpan;
					if (envpos >= x2)
					{
						envpan = y2;
						x1 = x2;
					} else
					if (pt)
					{
						envpan = penv->PanEnv.Values[pt-1];
						x1 = penv->PanEnv.Ticks[pt-1];
					} else
					{
						envpan = 128;
						x1 = 0;
					}
					if ((x2 > x1) && (envpos > x1))
					{
						envpan += ((envpos - x1) * (y2 - envpan)) / (x2 - x1);
					}
					if (envpan < 0) envpan = 0;
					if (envpan > 64) envpan = 64;
					int pan = pChn->nPan;
					if (pan >= 128)
					{
						pan += ((envpan - 32) * (256 - pan)) / 32;
					} else
					{
						pan += ((envpan - 32) * (pan)) / 32;
					}
					if (pan < 0) pan = 0;
					if (pan > 256) pan = 256;
					pChn->nRealPan = pan;
				}
				// FadeOut volume
				if (pChn->dwFlags & CHN_NOTEFADE)
				{
					UINT fadeout = penv->nFadeOut;
					if (fadeout)
					{
						pChn->nFadeOutVol -= fadeout << 1;
						if (pChn->nFadeOutVol <= 0) pChn->nFadeOutVol = 0;
						vol = (vol * pChn->nFadeOutVol) >> 16;
					} else
					if (!pChn->nFadeOutVol)
					{
						vol = 0;
					}
				}
				// Pitch/Pan separation
				if ((penv->nPPS) && (pChn->nRealPan) && (pChn->nNote))
				{
					int pandelta = (int)pChn->nRealPan + (int)((int)(pChn->nNote - penv->nPPC - 1) * (int)penv->nPPS) / (int)8;
					if (pandelta < 0) pandelta = 0;
					if (pandelta > 256) pandelta = 256;
					pChn->nRealPan = pandelta;
				}
			} else
			{
				// No Envelope: key off => note cut
				if (pChn->dwFlags & CHN_NOTEFADE) // 1.41-: CHN_KEYOFF|CHN_NOTEFADE
				{
					pChn->nFadeOutVol = 0;
					vol = 0;
				}
			}
			// vol is 14-bits
			if (vol)
			{
				// IMPORTANT: pChn->nRealVolume is 14 bits !!!
				// -> _muldiv( 14+8, 6+6, 18); => RealVolume: 14-bit result (22+12-20)
				pChn->nRealVolume = _muldiv(vol * m_nGlobalVolume, pChn->nGlobalVol * pChn->nInsVol, 1 << 20);
			}
			if (pChn->nPeriod < m_nMinPeriod) pChn->nPeriod = m_nMinPeriod;
			int period = pChn->nPeriod;
			if ((pChn->dwFlags & (CHN_GLISSANDO|CHN_PORTAMENTO)) ==	(CHN_GLISSANDO|CHN_PORTAMENTO))
			{
				period = GetPeriodFromNote(GetNoteFromPeriod(period), pChn->nFineTune, pChn->nC4Speed);
			}

			// Arpeggio ?
			if (pChn->nCommand == CMD_ARPEGGIO)
			{
				switch(m_nTickCount % 3)
				{
#if 0
				case 1:	period = GetPeriodFromNote(pChn->nNote + (pChn->nArpeggio >> 4), pChn->nFineTune, pChn->nC4Speed); break;
				case 2:	period = GetPeriodFromNote(pChn->nNote + (pChn->nArpeggio & 0x0F), pChn->nFineTune, pChn->nC4Speed); break;
#else
				case 1:	period = GetLinearPeriodFromNote(GetNoteFromPeriod(period) + (pChn->nArpeggio >> 4), pChn->nFineTune, pChn->nC4Speed); break;
				case 2:	period = GetLinearPeriodFromNote(GetNoteFromPeriod(period) + (pChn->nArpeggio & 0x0F), pChn->nFineTune, pChn->nC4Speed); break;
#endif
				}
			}

			if (m_dwSongFlags & SONG_AMIGALIMITS)
			{
				if (period < 113*4) period = 113*4;
				if (period > 856*4) period = 856*4;
			}

			// Pitch/Filter Envelope
			int envpitch = 0;
			if ((m_dwSongFlags & SONG_INSTRUMENTMODE) && (pChn->pHeader)
			    && (pChn->dwFlags & CHN_PITCHENV) && (pChn->pHeader->PitchEnv.nNodes))
			{
				INSTRUMENTHEADER *penv = pChn->pHeader;
				int envpos = pChn->nPitchEnvPosition;
				UINT pt = penv->PitchEnv.nNodes - 1;
				for (UINT i=0; i<(UINT)(penv->PitchEnv.nNodes-1); i++)
				{
					if (envpos <= penv->PitchEnv.Ticks[i])
					{
						pt = i;
						break;
					}
				}
				int x2 = penv->PitchEnv.Ticks[pt];
				int x1;
				if (envpos >= x2)
				{
					envpitch = (((int)penv->PitchEnv.Values[pt]) - 32) * 8;
					x1 = x2;
				} else
				if (pt)
				{
					envpitch = (((int)penv->PitchEnv.Values[pt-1]) - 32) * 8;
					x1 = penv->PitchEnv.Ticks[pt-1];
				} else
				{
					envpitch = 0;
					x1 = 0;
				}
				if (envpos > x2) envpos = x2;
				if ((x2 > x1) && (envpos > x1))
				{
					int envpitchdest = (((int)penv->PitchEnv.Values[pt]) - 32) * 8;
					envpitch += ((envpos - x1) * (envpitchdest - envpitch)) / (x2 - x1);
				}
				if (envpitch < -256) envpitch = -256;
				if (envpitch > 256) envpitch = 256;
				// Pitch Envelope
				if (!(penv->dwFlags & ENV_FILTER))
				{
					int l = envpitch;
					if (l < 0)
					{
						l = -l;
						if (l > 255) l = 255;
						period = _muldiv(period, LinearSlideUpTable[l], 0x10000);
					} else
					{
						if (l > 255) l = 255;
						period = _muldiv(period, LinearSlideDownTable[l], 0x10000);
					}
				}
			}

			// Vibrato
			if (pChn->dwFlags & CHN_VIBRATO)
			{
				UINT vibpos = pChn->nVibratoPos;
				LONG vdelta;
				switch (pChn->nVibratoType & 0x03)
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
				UINT vdepth = ((m_nType != MOD_TYPE_IT) || (m_dwSongFlags & SONG_ITOLDEFFECTS)) ? 6 : 7;
				vdelta = (vdelta * (int)pChn->nVibratoDepth) >> vdepth;
				if ((m_dwSongFlags & SONG_LINEARSLIDES) && (m_nType & MOD_TYPE_IT))
				{
					LONG l = vdelta;
					if (l < 0)
					{
						l = -l;
						vdelta = _muldiv(period, LinearSlideDownTable[l >> 2], 0x10000) - period;
						if (l & 0x03) vdelta += _muldiv(period, FineLinearSlideDownTable[l & 0x03], 0x10000) - period;

					} else
					{
						vdelta = _muldiv(period, LinearSlideUpTable[l >> 2], 0x10000) - period;
						if (l & 0x03) vdelta += _muldiv(period, FineLinearSlideUpTable[l & 0x03], 0x10000) - period;

					}
				}
				period += vdelta;
				if ((m_nTickCount) || ((m_nType & MOD_TYPE_IT) && (!(m_dwSongFlags & SONG_ITOLDEFFECTS))))
				{
					pChn->nVibratoPos = (vibpos + pChn->nVibratoSpeed) & 0x3F;
				}
			}
			// Panbrello
			if (pChn->dwFlags & CHN_PANBRELLO)
			{
				UINT panpos = ((pChn->nPanbrelloPos+0x10) >> 2) & 0x3F;
				LONG pdelta;
				switch (pChn->nPanbrelloType & 0x03)
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
				pChn->nPanbrelloPos += pChn->nPanbrelloSpeed;
				pdelta = ((pdelta * (int)pChn->nPanbrelloDepth) + 2) >> 3;
				pdelta += pChn->nRealPan;
				if (pdelta < 0) pdelta = 0;
				if (pdelta > 256) pdelta = 256;
				pChn->nRealPan = pdelta;
			}
			int nPeriodFrac = 0;
			// Instrument Auto-Vibrato
			if ((pChn->pInstrument) && (pChn->pInstrument->nVibDepth))
			{
				MODINSTRUMENT *pins = pChn->pInstrument;
				/* this isn't correct, but its better... */

				if (pins->nVibSweep == 0) {
					pChn->nAutoVibDepth = pins->nVibDepth << 8;
				} else {
					pChn->nAutoVibDepth += pins->nVibSweep;
					if ((pChn->nAutoVibDepth >> 8) > (int)pins->nVibDepth)
						pChn->nAutoVibDepth = pins->nVibDepth << 8;
				}
#if 0
				if (pins->nVibSweep == 0)
				{
					pChn->nAutoVibDepth = pins->nVibDepth << 8;
				} else
				{
					if (m_nType & MOD_TYPE_IT)
					{
						pChn->nAutoVibDepth += pins->nVibSweep;
					} else
					if (!(pChn->dwFlags & CHN_KEYOFF))
					{
						pChn->nAutoVibDepth += (pins->nVibDepth << 8) /	pins->nVibSweep;
					}
					if ((pChn->nAutoVibDepth >> 8) > pins->nVibDepth)
						pChn->nAutoVibDepth = pins->nVibDepth << 8;
				}
#endif
				pChn->nAutoVibPos += ((int)pins->nVibRate);
				int val;
				switch(pins->nVibType)
				{
				case 4:	// Random
					val = ModRandomTable[pChn->nAutoVibPos & 0x3F];
					pChn->nAutoVibPos++;
					break;
				case 3:	// Ramp Down
					val = ((0x40 - (pChn->nAutoVibPos >> 1)) & 0x7F) - 0x40;
					break;
				case 2:	// Ramp Up
					val = ((0x40 + (pChn->nAutoVibPos >> 1)) & 0x7f) - 0x40;
					break;
				case 1:	// Square
					val = (pChn->nAutoVibPos & 128) ? +64 : -64;
					break;
				default:	// Sine
					val = ft2VibratoTable[pChn->nAutoVibPos & 255];
				}
				int n =	((val * pChn->nAutoVibDepth) >> 8);
				// is this right? -mrsb
				if (!(m_dwSongFlags & SONG_ITOLDEFFECTS))
					n >>= 1;

				if (m_nType & MOD_TYPE_IT)
				{
					int df1, df2;
					if (n < 0)
					{
						n = -n;
						UINT n1 = n >> 8;
						df1 = LinearSlideUpTable[n1];
						df2 = LinearSlideUpTable[n1+1];
					} else
					{
						UINT n1 = n >> 8;
						df1 = LinearSlideDownTable[n1];
						df2 = LinearSlideDownTable[n1+1];
					}
					n >>= 2;
					period = _muldiv(period, df1 + ((df2-df1)*(n&0x3F)>>6), 256);
					nPeriodFrac = period & 0xFF;
					period >>= 8;
				} else
				{
					period += (n >> 6);
				}
			}
			// Final Period
			if (period <= m_nMinPeriod)
			{
				if (m_nType & MOD_TYPE_S3M) pChn->nLength = 0;
				period = m_nMinPeriod;
			}
			if (period > m_nMaxPeriod)
			{
				if ((m_nType & MOD_TYPE_IT) || (period >= 0x100000))
				{
					pChn->nFadeOutVol = 0;
					pChn->dwFlags |= CHN_NOTEFADE;
					pChn->nRealVolume = 0;
				}
				period = m_nMaxPeriod;
				nPeriodFrac = 0;
			}
			UINT freq = GetFreqFromPeriod(period, pChn->nC4Speed, nPeriodFrac);

			// Filter Envelope: controls cutoff frequency
			if (pChn && pChn->pHeader && pChn->pHeader->dwFlags & ENV_FILTER)
			{
#ifndef NO_FILTER
				SetupChannelFilter(pChn, (pChn->dwFlags & CHN_FILTER) ? FALSE : TRUE, envpitch, freq);
#endif // NO_FILTER
			}

#if 0
			if ((m_nType & MOD_TYPE_IT) && (freq < 256))
			{
				pChn->nFadeOutVol = 0;
				pChn->dwFlags |= CHN_NOTEFADE;
				pChn->nRealVolume = 0;
			}
#endif
			pChn->sample_freq = freq;

			UINT ninc = _muldiv(freq, 0x10000, gdwMixingFreq);
			if ((ninc >= 0xFFB0) && (ninc <= 0x10090)) ninc = 0x10000;
			if (m_nFreqFactor != 128) ninc = (ninc * m_nFreqFactor) >> 7;
			if (ninc > 0xFF0000) ninc = 0xFF0000;
			pChn->nInc = (ninc+1) & ~3;
		}

		// Increment envelope position
		if ((m_dwSongFlags & SONG_INSTRUMENTMODE) && pChn->pHeader)
		{
			INSTRUMENTHEADER *penv = pChn->pHeader;
			// Volume Envelope
			if (pChn->dwFlags & CHN_VOLENV)
			{
				// Increase position
				pChn->nVolEnvPosition++;
				// Volume Loop ?
				if (penv->dwFlags & ENV_VOLLOOP)
				{
					int volloopend = penv->VolEnv.Ticks[penv->VolEnv.nLoopEnd];
					if (m_nType != MOD_TYPE_XM) volloopend++;
					if (pChn->nVolEnvPosition == volloopend)
					{
						pChn->nVolEnvPosition = penv->VolEnv.Ticks[penv->VolEnv.nLoopStart];
						if ((penv->VolEnv.nLoopEnd == penv->VolEnv.nLoopStart) && (!penv->VolEnv.Values[penv->VolEnv.nLoopStart])
						 && ((!(m_nType & MOD_TYPE_XM)) || (penv->VolEnv.nLoopEnd+1 == penv->VolEnv.nNodes)))
						{
							pChn->dwFlags |= CHN_NOTEFADE;
							pChn->nFadeOutVol = 0;
						}
					}
				}
				// Volume Sustain ?
				if ((penv->dwFlags & ENV_VOLSUSTAIN) && (!(pChn->dwFlags & CHN_KEYOFF)))
				{
					if (pChn->nVolEnvPosition == (int)penv->VolEnv.Ticks[penv->VolEnv.nSustainEnd]+1)
						pChn->nVolEnvPosition = penv->VolEnv.Ticks[penv->VolEnv.nSustainStart];
				} else
				// End of Envelope ?
				if (pChn->nVolEnvPosition > penv->VolEnv.Ticks[penv->VolEnv.nNodes - 1])
				{
					if ((m_nType & MOD_TYPE_IT) || (pChn->dwFlags & CHN_KEYOFF)) pChn->dwFlags |= CHN_NOTEFADE;
					pChn->nVolEnvPosition = penv->VolEnv.Ticks[penv->VolEnv.nNodes - 1];
					if ((!penv->VolEnv.Values[penv->VolEnv.nNodes-1]) && ((nChn >= m_nChannels) || (m_nType & MOD_TYPE_IT)))
					{
						pChn->dwFlags |= CHN_NOTEFADE;
						pChn->nFadeOutVol = 0;

						pChn->nRealVolume = 0;
					}
				}
			}
			// Panning Envelope
			if (pChn->dwFlags & CHN_PANENV)
			{
				pChn->nPanEnvPosition++;
				if (penv->dwFlags & ENV_PANLOOP)
				{
					int panloopend = penv->PanEnv.Ticks[penv->PanEnv.nLoopEnd];
					if (m_nType != MOD_TYPE_XM) panloopend++;
					if (pChn->nPanEnvPosition == panloopend)
						pChn->nPanEnvPosition = penv->PanEnv.Ticks[penv->PanEnv.nLoopStart];
				}
				// Panning Sustain ?
				if ((penv->dwFlags & ENV_PANSUSTAIN) && (pChn->nPanEnvPosition == (int)penv->PanEnv.Ticks[penv->PanEnv.nSustainEnd]+1)
				 && (!(pChn->dwFlags & CHN_KEYOFF)))
				{
					// Panning sustained
					pChn->nPanEnvPosition = penv->PanEnv.Ticks[penv->PanEnv.nSustainStart];
				} else
				{
					if (pChn->nPanEnvPosition > penv->PanEnv.Ticks[penv->PanEnv.nNodes - 1])
						pChn->nPanEnvPosition = penv->PanEnv.Ticks[penv->PanEnv.nNodes - 1];
				}
			}
			// Pitch Envelope
			if (pChn->dwFlags & CHN_PITCHENV)
			{
				// Increase position
				pChn->nPitchEnvPosition++;
				// Pitch Loop ?
				if (penv->dwFlags & ENV_PITCHLOOP)
				{
					if (pChn->nPitchEnvPosition >= penv->PitchEnv.Ticks[penv->PitchEnv.nLoopEnd])
						pChn->nPitchEnvPosition = penv->PitchEnv.Ticks[penv->PitchEnv.nLoopStart];
				}
				// Pitch Sustain ?
				if ((penv->dwFlags & ENV_PITCHSUSTAIN) && (!(pChn->dwFlags & CHN_KEYOFF)))
				{
					if (pChn->nPitchEnvPosition == (int)penv->PitchEnv.Ticks[penv->PitchEnv.nSustainEnd]+1)
						pChn->nPitchEnvPosition = penv->PitchEnv.Ticks[penv->PitchEnv.nSustainStart];
				} else
				{
					if (pChn->nPitchEnvPosition > penv->PitchEnv.Ticks[penv->PitchEnv.nNodes - 1])
						pChn->nPitchEnvPosition = penv->PitchEnv.Ticks[penv->PitchEnv.nNodes - 1];
				}
			}
		}
#if 0
		// Limit CPU -> > 80% -> don't ramp
		if ((gnCPUUsage >= 80) && (!pChn->nRealVolume))
		{
			pChn->nLeftVol = pChn->nRightVol = 0;
		}
#endif // MODPLUG_PLAYER
		// Volume ramping
		pChn->dwFlags &= ~CHN_VOLUMERAMP;
		if ((pChn->nRealVolume) || (pChn->nLeftVol) || (pChn->nRightVol))
			pChn->dwFlags |= CHN_VOLUMERAMP;
		// Decrease VU-Meter
		if (pChn->nVUMeter > VUMETER_DECAY)	pChn->nVUMeter -= VUMETER_DECAY; else pChn->nVUMeter = 0;
		if (pChn->nLeftVU > VUMETER_DECAY) pChn->nLeftVU -= VUMETER_DECAY; else pChn->nLeftVU = 0;
		if (pChn->nRightVU > VUMETER_DECAY) pChn->nRightVU -= VUMETER_DECAY; else pChn->nRightVU = 0;
		// Check for too big nInc
		if (((pChn->nInc >> 16) + 1) >= (LONG)(pChn->nLoopEnd - pChn->nLoopStart)) pChn->dwFlags &= ~CHN_LOOP;
		pChn->nNewRightVol = pChn->nNewLeftVol = 0;
		pChn->pCurrentSample = ((pChn->pSample) && (pChn->nLength) && (pChn->nInc)) ? pChn->pSample : NULL;
		if (pChn->pCurrentSample)
		{
			// Update VU-Meter (nRealVolume is 14-bit)
			UINT vutmp = pChn->nRealVolume >> (14 - 8);
			if (vutmp > 0xFF) vutmp = 0xFF;
			if (pChn->nVUMeter >= 0x100) pChn->nVUMeter = vutmp;
			vutmp >>= 1;
			if (pChn->nVUMeter < vutmp)	pChn->nVUMeter = vutmp;
			UINT vul = (pChn->nRealVolume * pChn->nRealPan) >> 14;
			if (vul > 127) vul = 127;
			if (pChn->nLeftVU > 127) pChn->nLeftVU = (BYTE)vul;
			vul >>= 1;
			if (pChn->nLeftVU < vul) pChn->nLeftVU = (BYTE)vul;
			UINT vur = (pChn->nRealVolume * (256-pChn->nRealPan)) >> 14;
			if (vur > 127) vur = 127;
			if (pChn->nRightVU > 127) pChn->nRightVU = (BYTE)vur;
			vur >>= 1;
			if (pChn->nRightVU < vur) pChn->nRightVU = (BYTE)vur;
#ifdef MODPLUG_TRACKER
			UINT kChnMasterVol = (pChn->dwFlags & CHN_EXTRALOUD) ? 0x100 : nMasterVol;
#else
#define		kChnMasterVol	nMasterVol
#endif // MODPLUG_TRACKER
			// Adjusting volumes
			if (gnChannels >= 2)
			{
				int pan = ((int)pChn->nRealPan) - 128;
				pan *= (int)m_nStereoSeparation;
				pan /= 128;
				pan += 128;

				if (pan < 0) pan = 0;
				if (pan > 256) pan = 256;
				if (gdwSoundSetup & SNDMIX_REVERSESTEREO) pan = 256 - pan;
				if (m_dwSongFlags & SONG_NOSTEREO) pan = 128;
				LONG realvol = (pChn->nRealVolume * kChnMasterVol) >> (8-1);
				if (gdwSoundSetup & SNDMIX_SOFTPANNING)
				{
					if (pan < 128)
					{
						pChn->nNewLeftVol = (realvol * pan) >> 8;
						pChn->nNewRightVol = (realvol * 128) >> 8;
					} else
					{
						pChn->nNewLeftVol = (realvol * 128) >> 8;
						pChn->nNewRightVol = (realvol * (256 - pan)) >> 8;
					}
				} else
				{
					pChn->nNewLeftVol = (realvol * pan) >> 8;
					pChn->nNewRightVol = (realvol * (256 - pan)) >> 8;
				}
			} else
			{
				pChn->nNewRightVol = (pChn->nRealVolume * kChnMasterVol) >> 8;
				pChn->nNewLeftVol = pChn->nNewRightVol;
			}
			// Clipping volumes
			if (pChn->nNewRightVol > 0xFFFF) pChn->nNewRightVol = 0xFFFF;
			if (pChn->nNewLeftVol > 0xFFFF) pChn->nNewLeftVol = 0xFFFF;
			// Check IDO
			if (gdwSoundSetup & SNDMIX_NORESAMPLING)
			{
				pChn->dwFlags &= ~(CHN_HQSRC);
				pChn->dwFlags |= CHN_NOIDO;
			} else
			{
				pChn->dwFlags &= ~(CHN_NOIDO|CHN_HQSRC);
				if( pChn->nInc == 0x10000 )
				{	pChn->dwFlags |= CHN_NOIDO;
				}
				else
				{	if( ((gdwSoundSetup & SNDMIX_HQRESAMPLER) == 0) && ((gdwSoundSetup & SNDMIX_ULTRAHQSRCMODE) == 0) )
					{	if (pChn->nInc >= 0xFF00) pChn->dwFlags |= CHN_NOIDO;
					}
				}
			}
			pChn->nNewRightVol >>= MIXING_ATTENUATION;
			pChn->nNewLeftVol >>= MIXING_ATTENUATION;
			pChn->nRightRamp = pChn->nLeftRamp = 0;
			// Dolby Pro-Logic Surround
			if ((pChn->dwFlags & CHN_SURROUND) && (gnChannels <= 2) && (gdwSoundSetup & SNDMIX_NOSURROUND) == 0)
				pChn->nNewLeftVol = -pChn->nNewLeftVol;
			// Checking Ping-Pong Loops
			if (pChn->dwFlags & CHN_PINGPONGFLAG) pChn->nInc = -pChn->nInc;
			// Setting up volume ramp
			if (!(gdwSoundSetup & SNDMIX_NORAMPING)
			 && (pChn->dwFlags & CHN_VOLUMERAMP)
			 && ((pChn->nRightVol != pChn->nNewRightVol)
			  || (pChn->nLeftVol != pChn->nNewLeftVol)))
			{
				LONG nRampLength = gnVolumeRampSamples;
				LONG nRightDelta = ((pChn->nNewRightVol - pChn->nRightVol) << VOLUMERAMPPRECISION);
				LONG nLeftDelta = ((pChn->nNewLeftVol - pChn->nLeftVol) << VOLUMERAMPPRECISION);
#if 0
				if ((gdwSoundSetup & SNDMIX_DIRECTTODISK)
				 || ((gdwSysInfo & (SYSMIX_ENABLEMMX|SYSMIX_FASTCPU))
				  && (gdwSoundSetup & SNDMIX_HQRESAMPLER) && (gnCPUUsage <= 20)))
#else
				if (gdwSoundSetup & SNDMIX_HQRESAMPLER)
#endif
				{
					if ((pChn->nRightVol|pChn->nLeftVol) && (pChn->nNewRightVol|pChn->nNewLeftVol) && (!(pChn->dwFlags & CHN_FASTVOLRAMP)))
					{
						nRampLength = m_nBufferCount;
						if (nRampLength > (1 << (VOLUMERAMPPRECISION-1))) nRampLength = (1 << (VOLUMERAMPPRECISION-1));
						if (nRampLength < (LONG)gnVolumeRampSamples) nRampLength = gnVolumeRampSamples;
					}
				}
				pChn->nRightRamp = nRightDelta / nRampLength;
				pChn->nLeftRamp = nLeftDelta / nRampLength;
				pChn->nRightVol = pChn->nNewRightVol - ((pChn->nRightRamp * nRampLength) >> VOLUMERAMPPRECISION);
				pChn->nLeftVol = pChn->nNewLeftVol - ((pChn->nLeftRamp * nRampLength) >> VOLUMERAMPPRECISION);
				if (pChn->nRightRamp|pChn->nLeftRamp)
				{
					pChn->nRampLength = nRampLength;
				} else
				{
					pChn->dwFlags &= ~CHN_VOLUMERAMP;
					pChn->nRightVol = pChn->nNewRightVol;
					pChn->nLeftVol = pChn->nNewLeftVol;
				}
			} else
			{
				pChn->dwFlags &= ~CHN_VOLUMERAMP;
				pChn->nRightVol = pChn->nNewRightVol;
				pChn->nLeftVol = pChn->nNewLeftVol;
			}
			pChn->nRampRightVol = pChn->nRightVol << VOLUMERAMPPRECISION;
			pChn->nRampLeftVol = pChn->nLeftVol << VOLUMERAMPPRECISION;
			// Adding the channel in the channel list
			if (!(pChn->dwFlags & CHN_MUTE)) {
				ChnMix[m_nMixChannels++] = nChn;
				if (m_nMixChannels >= MAX_CHANNELS) break;
			}
		} else
		{
			// Note change but no sample
			if (pChn->nLeftVU > 128) pChn->nLeftVU = 0;
			if (pChn->nRightVU > 128) pChn->nRightVU = 0;
			if (pChn->nVUMeter > 0xFF) pChn->nVUMeter = 0;
			pChn->nLeftVol = pChn->nRightVol = 0;
			pChn->nLength = 0;
		}
	}
	// Checking Max Mix Channels reached: ordering by volume
	if ((m_nMixChannels >= m_nMaxMixChannels) && (!(gdwSoundSetup & SNDMIX_DIRECTTODISK)))
	{
		for (UINT i=0; i<m_nMixChannels; i++)
		{
			UINT j=i;
			while ((j+1<m_nMixChannels) && (Chn[ChnMix[j]].nRealVolume < Chn[ChnMix[j+1]].nRealVolume))
			{
				UINT n = ChnMix[j];
				ChnMix[j] = ChnMix[j+1];
				ChnMix[j+1] = n;
				j++;
			}
		}
	}
	if (m_dwSongFlags & SONG_GLOBALFADE)
	{
		if (!m_nGlobalFadeSamples)
		{
			m_dwSongFlags |= SONG_ENDREACHED;
			return FALSE;
		}
		if (m_nGlobalFadeSamples > m_nBufferCount)
			m_nGlobalFadeSamples -= m_nBufferCount;
		else
			m_nGlobalFadeSamples = 0;
	}
	return TRUE;
}


