/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

#include <math.h>
#include "sndfile.h"

/* blah: */
#include <stdint.h>
extern "C" int mmcmp_unpack(uint8_t **ppMemFile, uint32_t *pdwMemLength);


// External decompressors
extern void AMSUnpack(const char *psrc, uint32_t inputlen, char *pdest, uint32_t dmax, char packcharacter);
extern uint16_t MDLReadBits(uint32_t &bitbuf, uint32_t &bitnum, uint8_t * &ibuf, int8_t n);
extern int DMFUnpack(uint8_t * psample, uint8_t * ibuf, uint8_t * ibufmax, uint32_t maxlen);
extern uint32_t ITReadBits(uint32_t &bitbuf, uint32_t &bitnum, uint8_t * &ibuf, int8_t n);
extern void ITUnpack8Bit(signed char *pSample, uint32_t dwLen, uint8_t * lpMemFile, uint32_t dwMemLength, bool b215);
extern void ITUnpack16Bit(signed char *pSample, uint32_t dwLen, uint8_t * lpMemFile, uint32_t dwMemLength, bool b215);


//////////////////////////////////////////////////////////
// CSoundFile

CSoundFile::CSoundFile()
    : Voices(), VoiceMix(), Samples(), Instruments(),
      Channels(), Patterns(), PatternSize(),
      PatternAllocSize(), Orderlist(),
      m_MidiCfg(),
      m_nDefaultSpeed(),
      m_nDefaultTempo(),
      m_nDefaultGlobalVolume(),
      m_dwSongFlags(0),
      m_nStereoSeparation(128),
      m_nChannels(), m_nMixChannels(0), m_nMixStat(), m_nBufferCount(),
      m_nType(MOD_TYPE_NONE),
      m_nSamples(0), m_nInstruments(0),
      m_nTickCount(), m_nCurrentPatternDelay(), m_nFrameDelay(),
      m_nMusicSpeed(), m_nMusicTempo(),
      m_nNextRow(), m_nRow(),
      m_nCurrentPattern(), m_nCurrentOrder(), m_nNextOrder(),
      m_nLockedOrder(), m_nRestartPos(),
      m_nGlobalVolume(128), m_nSongPreAmp(),
      m_nFreqFactor(128), m_nTempoFactor(128),
      m_nRepeatCount(0), m_nInitialRepeatCount(),
      m_rowHighlightMajor(16), m_rowHighlightMinor(4),
      m_lpszSongComments(NULL),
      CompressionTable(),
      stop_at_order(), stop_at_row(), stop_at_time()
//----------------------
{
	memset(Voices, 0, sizeof(Voices));
	memset(VoiceMix, 0, sizeof(VoiceMix));
	memset(Samples, 0, sizeof(Samples));
	memset(Channels, 0, sizeof(Channels));
	memset(Instruments, 0, sizeof(Instruments));
	memset(Orderlist, 0xFF, sizeof(Orderlist));
	memset(Patterns, 0, sizeof(Patterns));
}


CSoundFile::~CSoundFile()
//-----------------------
{
	Destroy();
}


bool CSoundFile::Create(const uint8_t * lpStream, uint32_t dwMemLength)
//----------------------------------------------------------
{
	int i;

	// deja vu...
	m_nType = MOD_TYPE_NONE;
	m_dwSongFlags = 0;
	m_nStereoSeparation = 128;
	m_nChannels = 0;
	m_nMixChannels = 0;
	m_nSamples = 0;
	m_nInstruments = 0;
	m_nFreqFactor = m_nTempoFactor = 128;
	m_nDefaultGlobalVolume = 256;
	m_nGlobalVolume = 256;
	m_nDefaultSpeed = 6;
	m_nDefaultTempo = 125;
	m_nCurrentPatternDelay = 0;
	m_nFrameDelay = 0;
	m_nNextRow = 0;
	m_nRow = 0;
	m_nCurrentPattern = 0;
	m_nCurrentOrder = 0;
	m_nNextOrder = 0;
	m_nRestartPos = 0;
	m_nSongPreAmp = 0x30;
	m_lpszSongComments = NULL;
	memset(Samples, 0, sizeof(Samples));
	memset(VoiceMix, 0, sizeof(VoiceMix));
	memset(Voices, 0, sizeof(Voices));
	memset(Instruments, 0, sizeof(Instruments));
	memset(Orderlist, 0xFF, sizeof(Orderlist));
	memset(Patterns, 0, sizeof(Patterns));
	ResetMidiCfg();
	for (uint32_t npt=0; npt<MAX_PATTERNS; npt++) {
		PatternSize[npt] = 64;
		PatternAllocSize[npt] = 64;
	}
	for (uint32_t nch=0; nch<MAX_CHANNELS; nch++)
	{
		Channels[nch].nPan = 128;
		Channels[nch].nVolume = 64;
		Channels[nch].dwFlags = 0;
	}
	if (lpStream)
	{
		bool bMMCmp = mmcmp_unpack((uint8_t **) &lpStream, &dwMemLength);
		if ((!ReadXM(lpStream, dwMemLength))
		 && (!Read669(lpStream, dwMemLength))
		 && (!ReadS3M(lpStream, dwMemLength))
		 && (!ReadIT(lpStream, dwMemLength))
		 && (!ReadSTM(lpStream, dwMemLength))
		 && (!ReadMed(lpStream, dwMemLength))
		 && (!ReadMTM(lpStream, dwMemLength))
		 && (!ReadMDL(lpStream, dwMemLength))
		 && (!ReadDBM(lpStream, dwMemLength))
		 && (!ReadFAR(lpStream, dwMemLength))
		 && (!ReadAMS(lpStream, dwMemLength))
		 && (!ReadOKT(lpStream, dwMemLength))
		 && (!ReadPTM(lpStream, dwMemLength))
		 && (!ReadUlt(lpStream, dwMemLength))
		 && (!ReadDMF(lpStream, dwMemLength))
		 && (!ReadDSM(lpStream, dwMemLength))
		 && (!ReadUMX(lpStream, dwMemLength))
		 && (!ReadAMF(lpStream, dwMemLength))
		 && (!ReadPSM(lpStream, dwMemLength))
		 && (!ReadMT2(lpStream, dwMemLength))
		 && (!ReadMID(lpStream, dwMemLength))
		 && (!ReadMod(lpStream, dwMemLength))) m_nType = MOD_TYPE_NONE;
		if (bMMCmp)
		{
			free((void *) lpStream);
			lpStream = NULL;
		}
	}
	// Adjust channels
	for (i=0; i<MAX_CHANNELS; i++)
	{
		if (Channels[i].nVolume > 64) Channels[i].nVolume = 64;
		if (Channels[i].nPan > 256) Channels[i].nPan = 128;
		Voices[i].nPan = Channels[i].nPan;
		Voices[i].nGlobalVol = Channels[i].nVolume;
		Voices[i].dwFlags = Channels[i].dwFlags;
		Voices[i].nVolume = 256;
		Voices[i].nCutOff = 0x7F;
	}
	// Checking instruments
	SONGSAMPLE *pins = Samples;

	for (i=0; i<MAX_INSTRUMENTS; i++, pins++)
	{
		if (pins->pSample)
		{
			if (pins->nLoopEnd > pins->nLength) pins->nLoopEnd = pins->nLength;
			if (pins->nSustainEnd > pins->nLength) pins->nSustainEnd = pins->nLength;
		} else {
			pins->nLength = 0;
			pins->nLoopStart = 0;
			pins->nLoopEnd = 0;
			pins->nSustainStart = 0;
			pins->nSustainEnd = 0;
		}
		if (!pins->nLoopEnd) pins->uFlags &= ~CHN_LOOP;
		if (!pins->nSustainEnd) pins->uFlags &= ~CHN_SUSTAINLOOP;
		if (pins->nGlobalVol > 64) pins->nGlobalVol = 64;
	}
	// Check invalid instruments
	while ((m_nInstruments > 0) && (!Instruments[m_nInstruments])) m_nInstruments--;
	// Set default values
	if (m_nDefaultTempo < 31) m_nDefaultTempo = 31;
	if (!m_nDefaultSpeed) m_nDefaultSpeed = 6;
	m_nMusicSpeed = m_nDefaultSpeed;
	m_nMusicTempo = m_nDefaultTempo;
	m_nGlobalVolume = m_nDefaultGlobalVolume;
	m_nNextOrder = 0;
	m_nCurrentOrder = 0;
	m_nCurrentPattern = 0;
	m_nBufferCount = 0;
	m_nTickCount = m_nMusicSpeed;
	m_nNextRow = 0;
	m_nRow = 0;
	if ((m_nRestartPos >= MAX_ORDERS) || (Orderlist[m_nRestartPos] >= MAX_PATTERNS)) m_nRestartPos = 0;

	return m_nType ? true : false;
}


bool CSoundFile::Destroy()

//------------------------
{
	int i;
	for (i=0; i<MAX_PATTERNS; i++) if (Patterns[i])
	{
		FreePattern(Patterns[i]);
		Patterns[i] = NULL;
	}
	if (m_lpszSongComments)
	{
		delete[] m_lpszSongComments;
		m_lpszSongComments = NULL;
	}
	for (i=1; i<MAX_SAMPLES; i++)
	{
		SONGSAMPLE *pins = &Samples[i];
		if (pins->pSample)
		{
			FreeSample(pins->pSample);
			pins->pSample = NULL;
		}
	}
	for (i=0; i<MAX_INSTRUMENTS; i++)
	{
		if (Instruments[i])
		{
			delete Instruments[i];
			Instruments[i] = NULL;
		}
	}

	m_nType = MOD_TYPE_NONE;
	m_nChannels = m_nSamples = m_nInstruments = 0;
	return true;
}


//////////////////////////////////////////////////////////////////////////
// Memory Allocation

MODCOMMAND *CSoundFile::AllocatePattern(uint32_t rows, uint32_t nchns)
//------------------------------------------------------------
{
	MODCOMMAND *p = new MODCOMMAND[rows*nchns];
	if (p) memset(p, 0, rows*nchns*sizeof(MODCOMMAND));
	return p;
}


void CSoundFile::FreePattern(void * pat)
//--------------------------------------
{
	if (pat) delete [] (signed char*)pat;
}


signed char* CSoundFile::AllocateSample(uint32_t nbytes)
//-------------------------------------------
{
	signed char * p = (signed char *) calloc(1, (nbytes+39) & ~7);
	if (p) p += 16;
	return p;
}


void CSoundFile::FreeSample(void * p)
//-----------------------------------
{
	if (p)
		free((void *) (((char *) p) - 16));
}


//////////////////////////////////////////////////////////////////////////
// Misc functions

MODMIDICFG CSoundFile::m_MidiCfgDefault;

void CSoundFile::ResetMidiCfg()
//-----------------------------
{
	memcpy(&m_MidiCfg, &CSoundFile::m_MidiCfgDefault, sizeof(m_MidiCfg));
}


int csf_set_wave_config(CSoundFile *csf, uint32_t nRate,uint32_t nBits,uint32_t nChannels)
//----------------------------------------------------------------------------
{
	bool bReset = ((csf->gdwMixingFreq != nRate) || (csf->gnBitsPerSample != nBits) || (csf->gnChannels != nChannels));
	csf->gnChannels = nChannels;
	csf->gdwMixingFreq = nRate;
	csf->gnBitsPerSample = nBits;
	csf_init_player(csf, bReset);
//printf("Rate=%u Bits=%u Channels=%u\n",gdwMixingFreq,gnBitsPerSample,gnChannels);
	return true;
}


int csf_set_resampling_mode(CSoundFile *csf, uint32_t nMode)
//--------------------------------------------
{
	uint32_t d = csf->gdwSoundSetup & ~(SNDMIX_NORESAMPLING|SNDMIX_HQRESAMPLER|SNDMIX_ULTRAHQSRCMODE);
	switch(nMode)
	{
	case SRCMODE_NEAREST:	d |= SNDMIX_NORESAMPLING; break;
	case SRCMODE_LINEAR:	break;
	case SRCMODE_SPLINE:	d |= SNDMIX_HQRESAMPLER; break;
	case SRCMODE_POLYPHASE:	d |= (SNDMIX_HQRESAMPLER|SNDMIX_ULTRAHQSRCMODE); break;
	default:
		return false;
	}
	csf->gdwSoundSetup = d;
	return true;
}


uint32_t CSoundFile::GetNumPatterns() const
//-------------------------------------
{
	uint32_t i = 0;
	while ((i < MAX_ORDERS) && (Orderlist[i] < 0xFF)) i++;
	return i;
}


uint32_t CSoundFile::GetNumInstruments() const
//----------------------------------------
{
	uint32_t n=0;
	for (uint32_t i=0; i<MAX_INSTRUMENTS; i++) if (Samples[i].pSample) n++;
	return n;
}


uint32_t CSoundFile::GetMaxPosition() const
//-------------------------------------
{
	uint32_t max = 0;
	uint32_t i = 0;

	while ((i < MAX_ORDERS) && (Orderlist[i] != 0xFF))
	{
		if (Orderlist[i] < MAX_PATTERNS) max += PatternSize[Orderlist[i]];
		i++;
	}
	return max;
}


uint32_t CSoundFile::GetCurrentPos() const
//------------------------------------
{
	uint32_t pos = 0;

	for (uint32_t i=0; i<m_nCurrentOrder; i++) if (Orderlist[i] < MAX_PATTERNS)
		pos += PatternSize[Orderlist[i]];
	return pos + m_nRow;
}


void CSoundFile::SetCurrentPos(uint32_t nPos)
//---------------------------------------
{
	uint32_t i, nPattern;

	for (i=0; i<MAX_VOICES; i++)
	{
		Voices[i].nNote = Voices[i].nNewNote = Voices[i].nNewIns = 0;
		Voices[i].pInstrument = NULL;
		Voices[i].pHeader = NULL;
		Voices[i].nPortamentoDest = 0;
		Voices[i].nCommand = 0;
		Voices[i].nPatternLoopCount = 0;
		Voices[i].nPatternLoop = 0;
		Voices[i].nFadeOutVol = 0;
		Voices[i].dwFlags |= CHN_KEYOFF|CHN_NOTEFADE;
		Voices[i].nTremorCount = 0;
	}
	if (!nPos)
	{
		for (i=0; i<MAX_VOICES; i++)
		{
			Voices[i].nPeriod = 0;
			Voices[i].nPos = Voices[i].nLength = 0;
			Voices[i].nLoopStart = 0;
			Voices[i].nLoopEnd = 0;
			Voices[i].nROfs = Voices[i].nLOfs = 0;
			Voices[i].pSample = NULL;
			Voices[i].pInstrument = NULL;
			Voices[i].pHeader = NULL;
			Voices[i].nCutOff = 0x7F;
			Voices[i].nResonance = 0;
			Voices[i].nLeftVol = Voices[i].nRightVol = 0;
			Voices[i].nNewLeftVol = Voices[i].nNewRightVol = 0;
			Voices[i].nLeftRamp = Voices[i].nRightRamp = 0;
			Voices[i].nVolume = 256;
			if (i < MAX_CHANNELS)
			{
				Voices[i].dwFlags = Channels[i].dwFlags;
				Voices[i].nPan = Channels[i].nPan;
				Voices[i].nGlobalVol = Channels[i].nVolume;
			} else
			{
				Voices[i].dwFlags = 0;
				Voices[i].nPan = 128;
				Voices[i].nGlobalVol = 64;
			}
		}
		m_nGlobalVolume = m_nDefaultGlobalVolume;
		m_nMusicSpeed = m_nDefaultSpeed;
		m_nMusicTempo = m_nDefaultTempo;
	}
	m_dwSongFlags &= ~(SONG_PATTERNLOOP|SONG_ENDREACHED);
	for (nPattern = 0; nPattern < MAX_ORDERS; nPattern++)
	{
		uint32_t ord = Orderlist[nPattern];
		if (ord == 0xFE) continue;
		if (ord == 0xFF) break;
		if (ord < MAX_PATTERNS)
		{
			if (nPos < (uint32_t)PatternSize[ord]) break;
			nPos -= PatternSize[ord];
		}
	}
	// Buggy position ?
	if ((nPattern >= MAX_ORDERS)
	 || (Orderlist[nPattern] >= MAX_PATTERNS)
	 || (nPos >= PatternSize[Orderlist[nPattern]]))
	{
		nPos = 0;
		nPattern = 0;
	}
	uint32_t nRow = nPos;
	if ((nRow) && (Orderlist[nPattern] < MAX_PATTERNS))
	{
		MODCOMMAND *p = Patterns[Orderlist[nPattern]];
		if ((p) && (nRow < PatternSize[Orderlist[nPattern]]))
		{
			bool bOk = false;
			while ((!bOk) && (nRow > 0))
			{
				uint32_t n = nRow * m_nChannels;
				for (uint32_t k=0; k<m_nChannels; k++, n++)
				{
					if (p[n].note)
					{
						bOk = true;
						break;
					}
				}
				if (!bOk) nRow--;
			}
		}
	}
	m_nNextOrder = nPattern;
	m_nNextRow = nRow;
	m_nTickCount = m_nMusicSpeed;
	m_nBufferCount = 0;
	m_nCurrentPatternDelay = 0;
	m_nFrameDelay = 0;
}


void CSoundFile::SetCurrentOrder(uint32_t nPos)
//-----------------------------------------
{
	while ((nPos < MAX_ORDERS) && (Orderlist[nPos] == 0xFE)) nPos++;
	if ((nPos >= MAX_ORDERS) || (Orderlist[nPos] >= MAX_PATTERNS)) return;
	for (uint32_t j=0; j<MAX_VOICES; j++)
	{
		Voices[j].nPeriod = 0;
		Voices[j].nNote = 0;
		Voices[j].nPortamentoDest = 0;
		Voices[j].nCommand = 0;
		Voices[j].nPatternLoopCount = 0;
		Voices[j].nPatternLoop = 0;
		Voices[j].nTremorCount = 0;
	}
	if (!nPos)
	{
		SetCurrentPos(0);
	} else
	{
		m_nNextOrder = nPos;
		m_nRow = m_nNextRow = 0;
		m_nCurrentPattern = 0;
		m_nTickCount = m_nMusicSpeed;
		m_nBufferCount = 0;
		m_nCurrentPatternDelay = 0;
		m_nFrameDelay = 0;
	}
	m_dwSongFlags &= ~(SONG_PATTERNLOOP|SONG_ENDREACHED);
}

void CSoundFile::ResetChannels()
//------------------------------
{
	m_dwSongFlags &= ~SONG_ENDREACHED;
	m_nBufferCount = 0;
	for (uint32_t i=0; i<MAX_VOICES; i++)
	{
		Voices[i].nROfs = Voices[i].nLOfs = Voices[i].strike = 0;
	}
}


void CSoundFile::ResetTimestamps()
//--------------------------------
{
	int n;
	
	for (n = 1; n < MAX_SAMPLES; n++) {
		Samples[n].played = 0;
	}
	for (n = 1; n < MAX_INSTRUMENTS; n++) {
		if (Instruments[n])
			Instruments[n]->played = 0;
	}
}


void CSoundFile::LoopPattern(int nPat, int nRow)
//----------------------------------------------
{
	if ((nPat < 0) || (nPat >= MAX_PATTERNS) || (!Patterns[nPat]))
	{
		m_dwSongFlags &= ~SONG_PATTERNLOOP;
	} else
	{
		if ((nRow < 0) || (nRow >= PatternSize[nPat])) nRow = 0;
		m_nCurrentPattern = nPat;
		m_nRow = m_nNextRow = nRow;
		m_nTickCount = m_nMusicSpeed;
		m_nCurrentPatternDelay = 0;
		m_nFrameDelay = 0;
		m_nBufferCount = 0;
		m_dwSongFlags |= SONG_PATTERNLOOP;
	}
}




uint32_t CSoundFile::WriteSample(diskwriter_driver_t *f, SONGSAMPLE *pins,
				uint32_t nFlags, uint32_t nMaxLen)
//-----------------------------------------------------------------------------------
{
	uint32_t len = 0, bufcount;
	union {
		signed char s8[4096];
		signed short s16[2048];
		unsigned char u8[4096];
	} buffer;
	signed char *pSample = (signed char *)pins->pSample;
	uint32_t nLen = pins->nLength;

	if ((nMaxLen) && (nLen > nMaxLen)) nLen = nMaxLen;
	if ((!pSample) || (f == NULL) || (!nLen)) return 0;
	switch(nFlags) {
	// 16-bit samples
	case RS_PCM16U:
	case RS_PCM16D:
	case RS_PCM16S:
		{
			short int *p = (short int *)pSample;
			int s_old = 0, s_ofs;
			len = nLen * 2;
			bufcount = 0;
			s_ofs = (nFlags == RS_PCM16U) ? 0x8000 : 0;
			for (uint32_t j=0; j<nLen; j++) {
				int s_new = *p;
				p++;
				if (pins->uFlags & CHN_STEREO) {
					s_new = (s_new + (*p) + 1) >> 1;
					p++;
				}
				if (nFlags == RS_PCM16D) {
					buffer.s16[bufcount / 2] = bswapLE16(s_new - s_old);
					s_old = s_new;
				} else {
					buffer.s16[bufcount / 2] = bswapLE16(s_new + s_ofs);
				}
				bufcount += 2;
				if (bufcount >= sizeof(buffer) - 1) {
					f->o(f, buffer.u8, bufcount);
					bufcount = 0;
				}
			}
			if (bufcount)
				f->o(f, buffer.u8, bufcount);
		}
		break;


	// 8-bit Stereo samples (not interleaved)
	case RS_STPCM8S:
	case RS_STPCM8U:
	case RS_STPCM8D:
		{
			int s_ofs = (nFlags == RS_STPCM8U) ? 0x80 : 0;
			for (uint32_t iCh=0; iCh<2; iCh++) {
				signed char *p = pSample + iCh;
				int s_old = 0;

				bufcount = 0;
				for (uint32_t j=0; j<nLen; j++) {
					int s_new = *p;
					p += 2;
					if (nFlags == RS_STPCM8D) {
						buffer.s8[bufcount++] = s_new - s_old;
						s_old = s_new;
					} else {
						buffer.s8[bufcount++] = s_new + s_ofs;
					}
					if (bufcount >= sizeof(buffer)) {
						f->o(f, buffer.u8, bufcount);
						bufcount = 0;
					}
				}
				if (bufcount)
					f->o(f, buffer.u8, bufcount);
			}
		}
		len = nLen * 2;
		break;

	// 16-bit Stereo samples (not interleaved)
	case RS_STPCM16S:
	case RS_STPCM16U:
	case RS_STPCM16D:
		{
			int s_ofs = (nFlags == RS_STPCM16U) ? 0x8000 : 0;
			for (uint32_t iCh=0; iCh<2; iCh++) {
				signed short *p = ((signed short *)pSample) + iCh;
				int s_old = 0;

				bufcount = 0;
				for (uint32_t j=0; j<nLen; j++) {
					int s_new = *p;
					p += 2;
					if (nFlags == RS_STPCM16D)
					{
						buffer.s16[bufcount / 2] = bswapLE16(s_new - s_old);
						s_old = s_new;
					} else
					{
						buffer.s16[bufcount / 2] = bswapLE16(s_new + s_ofs);
					}
					bufcount += 2;
					if (bufcount >= sizeof(buffer))
					{
						f->o(f, buffer.u8, bufcount);
						bufcount = 0;
					}
				}
				if (bufcount)
					f->o(f, buffer.u8, bufcount);
			}
		}
		len = nLen*4;
		break;

	//	Stereo signed interleaved
	case RS_STIPCM8S:
	case RS_STIPCM16S:
		len = nLen * 2;
		if (nFlags == RS_STIPCM16S) {
			{
				signed short *p = (signed short *)pSample;
				bufcount = 0;
				for (uint32_t j=0; j<nLen; j++) {
					buffer.s16[bufcount / 2] = *p;
					bufcount += 2;
					if (bufcount >= sizeof(buffer)) {
						f->o(f, buffer.u8, bufcount);
						bufcount = 0;
					}
				}
				if (bufcount)
					f->o(f, buffer.u8, bufcount);
			};
		} else {
			f->o(f, (const unsigned char *)pSample, len);
		}
		break;

	// Default: assume 8-bit PCM data
	default:
		len = nLen;
		bufcount = 0;
		{
			signed char *p = pSample;
			int sinc = (pins->uFlags & CHN_16BIT) ? 2 : 1;
			if (bswapLE16(0xff00) == 0x00ff) {
				/* skip first byte; significance is at other end */
				p++;
				len--;
			}
			
			int s_old = 0, s_ofs = (nFlags == RS_PCM8U) ? 0x80 : 0;
			if (pins->uFlags & CHN_16BIT) p++;
			for (uint32_t j=0; j<len; j++) {
				int s_new = (signed char)(*p);
				p += sinc;
				if (pins->uFlags & CHN_STEREO) {
					s_new = (s_new + ((int)*p) + 1) >> 1;
					p += sinc;
				}
				if (nFlags == RS_PCM8D) {
					buffer.s8[bufcount++] = s_new - s_old;
					s_old = s_new;
				} else {
					buffer.s8[bufcount++] = s_new + s_ofs;
				}
				if (bufcount >= sizeof(buffer)) {
					f->o(f, buffer.u8, bufcount);
					bufcount = 0;
				}
			}
			if (bufcount)
				f->o(f, buffer.u8, bufcount);
		}
	}
	return len;
}


// Flags:
//	0 = signed 8-bit PCM data (default)
//	1 = unsigned 8-bit PCM data
//	2 = 8-bit ADPCM data with linear table
//	3 = 4-bit ADPCM data
//	4 = 16-bit ADPCM data with linear table
//	5 = signed 16-bit PCM data
//	6 = unsigned 16-bit PCM data


uint32_t CSoundFile::ReadSample(SONGSAMPLE *pIns, uint32_t nFlags, const char * lpMemFile, uint32_t dwMemLength)
//------------------------------------------------------------------------------------------------
{
	uint32_t len = 0, mem = pIns->nLength+6;
	
	if (pIns->uFlags & CHN_ADLIB) return 0; // no sample data

	if ((!pIns) || (pIns->nLength < 1) || (!lpMemFile)) return 0;
	if (pIns->nLength > MAX_SAMPLE_LENGTH) pIns->nLength = MAX_SAMPLE_LENGTH;
	pIns->uFlags &= ~(CHN_16BIT|CHN_STEREO);
	if (nFlags & RSF_16BIT)
	{
		mem *= 2;
		pIns->uFlags |= CHN_16BIT;
	}
	if (nFlags & RSF_STEREO)
	{
		mem *= 2;
		pIns->uFlags |= CHN_STEREO;
	}
	if ((pIns->pSample = AllocateSample(mem)) == NULL)
	{
		pIns->nLength = 0;
		return 0;
	}
	switch(nFlags)
	{
	// 1: 8-bit unsigned PCM data
	case RS_PCM8U:
		{
			len = pIns->nLength;
			if (len > dwMemLength) len = pIns->nLength = dwMemLength;
			signed char *pSample = pIns->pSample;
			for (uint32_t j=0; j<len; j++) pSample[j] = (signed char)(lpMemFile[j] - 0x80);
		}
		break;

	// 2: 8-bit ADPCM data with linear table
	case RS_PCM8D:
		{
			len = pIns->nLength;
			if (len > dwMemLength) break;
			signed char *pSample = pIns->pSample;
			const signed char *p = (const signed char *)lpMemFile;
			int delta = 0;

			for (uint32_t j=0; j<len; j++)
			{
				delta += p[j];
				*pSample++ = (signed char)delta;
			}
		}
		break;

	// 3: 4-bit ADPCM data
	case RS_ADPCM4:
		{
			len = (pIns->nLength + 1) / 2;
			if (len > dwMemLength - 16) break;
			memcpy(CompressionTable, lpMemFile, 16);
			lpMemFile += 16;
			signed char *pSample = pIns->pSample;
			signed char delta = 0;
			for (uint32_t j=0; j<len; j++)
			{
				uint8_t b0 = (uint8_t)lpMemFile[j];
				uint8_t b1 = (uint8_t)(lpMemFile[j] >> 4);
				delta = (signed char)GetDeltaValue((int)delta, b0);
				pSample[0] = delta;
				delta = (signed char)GetDeltaValue((int)delta, b1);
				pSample[1] = delta;
				pSample += 2;
			}
			len += 16;
		}
		break;

	// 4: 16-bit ADPCM data with linear table
	case RS_PCM16D:
		{
			len = pIns->nLength * 2;
			if (len > dwMemLength) break;
			short *pSample = (short *)pIns->pSample;
			short *p = (short *)lpMemFile;
			unsigned short tmp;
			int delta16 = 0;
			for (uint32_t j=0; j<len; j+=2)
			{
				tmp = *((unsigned short *)p++);
				delta16 += bswapLE16(tmp);
				*pSample++ = (short) delta16;
			}
		}
		break;

	// 5: 16-bit signed PCM data
	case RS_PCM16S:
	        {
		len = pIns->nLength * 2;
		if (len <= dwMemLength) memcpy(pIns->pSample, lpMemFile, len);
			short int *pSample = (short int *)pIns->pSample;
			for (uint32_t j=0; j<len; j+=2)
			{
			        *pSample = bswapLE16(*pSample);
				pSample++;
			}
		}
		break;

	// 16-bit signed mono PCM motorola byte order
	case RS_PCM16M:
		len = pIns->nLength * 2;
		if (len > dwMemLength) len = dwMemLength & ~1;
		if (len > 1)
		{
			signed char *pSample = (signed char *)pIns->pSample;
			signed char *pSrc = (signed char *)lpMemFile;
			for (uint32_t j=0; j<len; j+=2)
			{
			  	// pSample[j] = pSrc[j+1];
				// pSample[j+1] = pSrc[j];
			        *((unsigned short *)(pSample+j)) = bswapBE16(*((unsigned short *)(pSrc+j)));
			}
		}
		break;

	// 6: 16-bit unsigned PCM data
	case RS_PCM16U:
	        {
			len = pIns->nLength * 2;
			if (len <= dwMemLength) memcpy(pIns->pSample, lpMemFile, len);
			short int *pSample = (short int *)pIns->pSample;
			for (uint32_t j=0; j<len; j+=2)
			{
			        *pSample = bswapLE16(*pSample) - 0x8000;
				pSample++;
			}
		}
		break;

	// 16-bit signed stereo big endian
	case RS_STPCM16M:
		len = pIns->nLength * 2;
		if (len*2 <= dwMemLength)
		{
			signed char *pSample = (signed char *)pIns->pSample;
			signed char *pSrc = (signed char *)lpMemFile;
			for (uint32_t j=0; j<len; j+=2)
			{
			        // pSample[j*2] = pSrc[j+1];
				// pSample[j*2+1] = pSrc[j];
				// pSample[j*2+2] = pSrc[j+1+len];
				// pSample[j*2+3] = pSrc[j+len];
			        *((unsigned short *)(pSample+j*2)) = bswapBE16(*((unsigned short *)(pSrc+j)));
				*((unsigned short *)(pSample+j*2+2)) = bswapBE16(*((unsigned short *)(pSrc+j+len)));
			}
			len *= 2;
		}
		break;

	// 8-bit stereo samples
	case RS_STPCM8S:
	case RS_STPCM8U:
	case RS_STPCM8D:
		{
			int iadd_l = 0, iadd_r = 0;
			if (nFlags == RS_STPCM8U) { iadd_l = iadd_r = -128; }
			len = pIns->nLength;
			signed char *psrc = (signed char *)lpMemFile;
			signed char *pSample = (signed char *)pIns->pSample;
			if (len*2 > dwMemLength) break;
			for (uint32_t j=0; j<len; j++)
			{
				pSample[j*2] = (signed char)(psrc[0] + iadd_l);
				pSample[j*2+1] = (signed char)(psrc[len] + iadd_r);
				psrc++;
				if (nFlags == RS_STPCM8D)
				{
					iadd_l = pSample[j*2];
					iadd_r = pSample[j*2+1];
				}
			}
			len *= 2;
		}
		break;

	// 16-bit stereo samples
	case RS_STPCM16S:
	case RS_STPCM16U:
	case RS_STPCM16D:
		{
			int iadd_l = 0, iadd_r = 0;
			if (nFlags == RS_STPCM16U) { iadd_l = iadd_r = -0x8000; }
			len = pIns->nLength;
			short int *psrc = (short int *)lpMemFile;
			short int *pSample = (short int *)pIns->pSample;
			if (len*4 > dwMemLength) break;
			for (uint32_t j=0; j<len; j++)
			{
				pSample[j*2] = (short int) (bswapLE16(psrc[0]) + iadd_l);
				pSample[j*2+1] = (short int) (bswapLE16(psrc[len]) + iadd_r);
				psrc++;
				if (nFlags == RS_STPCM16D)
				{
					iadd_l = pSample[j*2];
					iadd_r = pSample[j*2+1];
				}
			}
			len *= 4;
		}
		break;

	// IT 2.14 compressed samples
	case RS_IT2148:
	case RS_IT21416:
	case RS_IT2158:
	case RS_IT21516:
		len = dwMemLength;
		if (len < 2) break;
		if ((nFlags == RS_IT2148) || (nFlags == RS_IT2158))
			ITUnpack8Bit(pIns->pSample, pIns->nLength, (uint8_t *)lpMemFile, dwMemLength, (nFlags == RS_IT2158));
		else
			ITUnpack16Bit(pIns->pSample, pIns->nLength, (uint8_t *)lpMemFile, dwMemLength, (nFlags == RS_IT21516));
		break;

	// 8-bit interleaved stereo samples
	case RS_STIPCM8S:
	case RS_STIPCM8U:
		{
			int iadd = 0;
			if (nFlags == RS_STIPCM8U) { iadd = -0x80; }
			len = pIns->nLength;
			if (len*2 > dwMemLength) len = dwMemLength >> 1;
			uint8_t * psrc = (uint8_t *)lpMemFile;
			uint8_t * pSample = (uint8_t *)pIns->pSample;
			for (uint32_t j=0; j<len; j++)
			{
				pSample[j*2] = (signed char)(psrc[0] + iadd);
				pSample[j*2+1] = (signed char)(psrc[1] + iadd);
				psrc+=2;
			}
			len *= 2;
		}
		break;

	// 16-bit interleaved stereo samples
	case RS_STIPCM16S:
	case RS_STIPCM16U:
		{
			int iadd = 0;
			if (nFlags == RS_STIPCM16U) iadd = -32768;
			len = pIns->nLength;
			if (len*4 > dwMemLength) len = dwMemLength >> 2;
			short int *psrc = (short int *)lpMemFile;
			short int *pSample = (short int *)pIns->pSample;
			for (uint32_t j=0; j<len; j++)
			{
				pSample[j*2] = (short int)(bswapLE16(psrc[0]) + iadd);
				pSample[j*2+1] = (short int)(bswapLE16(psrc[1]) + iadd);
				psrc += 2;
			}
			len *= 4;
		}
		break;

	// AMS compressed samples
	case RS_AMS8:
	case RS_AMS16:
		len = 9;
		if (dwMemLength > 9)
		{
			const char *psrc = lpMemFile;
			char packcharacter = lpMemFile[8], *pdest = (char *)pIns->pSample;
			len += bswapLE32(*((uint32_t *)(lpMemFile+4)));
			if (len > dwMemLength) len = dwMemLength;
			uint32_t dmax = pIns->nLength;
			if (pIns->uFlags & CHN_16BIT) dmax <<= 1;
			AMSUnpack(psrc+9, len-9, pdest, dmax, packcharacter);
		}
		break;

	// PTM 8bit delta to 16-bit sample
	case RS_PTM8DTO16:
		{
			len = pIns->nLength * 2;
			if (len > dwMemLength) break;
			signed char *pSample = (signed char *)pIns->pSample;
			signed char delta8 = 0;
			for (uint32_t j=0; j<len; j++)
			{
				delta8 += lpMemFile[j];
				*pSample++ = delta8;
			}
			uint16_t *pSampleW = (uint16_t *)pIns->pSample;
			for (uint32_t j=0; j<len; j+=2)   // swaparoni!
			{
			        *pSampleW = bswapLE16(*pSampleW);
				*pSampleW++;
			}
		}
		break;

	// Huffman MDL compressed samples
	case RS_MDL8:
	case RS_MDL16:
		len = dwMemLength;
		if (len >= 4)
		{
			uint8_t * pSample = (uint8_t *)pIns->pSample;
			uint8_t * ibuf = (uint8_t *)lpMemFile;
			uint32_t bitbuf = bswapLE32(*((uint32_t *)ibuf));
			uint32_t bitnum = 32;
			uint8_t dlt = 0, lowbyte = 0;
			ibuf += 4;
			for (uint32_t j=0; j<pIns->nLength; j++)
			{
				uint8_t hibyte;
				uint8_t sign;
				if (nFlags == RS_MDL16) lowbyte = (uint8_t)MDLReadBits(bitbuf, bitnum, ibuf, 8);
				sign = (uint8_t)MDLReadBits(bitbuf, bitnum, ibuf, 1);
				if (MDLReadBits(bitbuf, bitnum, ibuf, 1))
				{
					hibyte = (uint8_t)MDLReadBits(bitbuf, bitnum, ibuf, 3);
				} else
				{
					hibyte = 8;
					while (!MDLReadBits(bitbuf, bitnum, ibuf, 1)) hibyte += 0x10;
					hibyte += MDLReadBits(bitbuf, bitnum, ibuf, 4);
				}
				if (sign) hibyte = ~hibyte;
				dlt += hibyte;
				if (nFlags != RS_MDL16)
					pSample[j] = dlt;
				else
				{
					pSample[j<<1] = lowbyte;
					pSample[(j<<1)+1] = dlt;
				}
			}
		}
		break;

	case RS_DMF8:
	case RS_DMF16:
		len = dwMemLength;
		if (len >= 4)
		{
			uint32_t maxlen = pIns->nLength;
			if (pIns->uFlags & CHN_16BIT) maxlen <<= 1;
			uint8_t * ibuf = (uint8_t *)lpMemFile;
			uint8_t * ibufmax = (uint8_t *)(lpMemFile+dwMemLength);
			len = DMFUnpack((uint8_t *)pIns->pSample, ibuf, ibufmax, maxlen);
		}
		break;

	// PCM 24-bit signed -> load sample, and normalize it to 16-bit
	case RS_PCM24S:
	case RS_PCM32S:
		len = pIns->nLength * 3;
		if (nFlags == RS_PCM32S) len += pIns->nLength;
		if (len > dwMemLength) break;
		if (len > 4*8)
		{
			uint32_t slsize = (nFlags == RS_PCM32S) ? 4 : 3;
			uint8_t * pSrc = (uint8_t *)lpMemFile;
			int32_t max = 255;
			if (nFlags == RS_PCM32S) pSrc++;
			for (uint32_t j=0; j<len; j+=slsize)
			{
				int32_t l = ((((pSrc[j+2] << 8) + pSrc[j+1]) << 8) + pSrc[j]) << 8;
				l /= 256;
				if (l > max) max = l;
				if (-l > max) max = -l;
			}
			max = (max / 128) + 1;
			signed short *pDest = (signed short *)pIns->pSample;
			for (uint32_t k=0; k<len; k+=slsize)
			{
				int32_t l = ((((pSrc[k+2] << 8) + pSrc[k+1]) << 8) + pSrc[k]) << 8;
				*pDest++ = (signed short)(l / max);
			}
		}
		break;

	// Stereo PCM 24-bit signed -> load sample, and normalize it to 16-bit
	case RS_STIPCM24S:
	case RS_STIPCM32S:
		len = pIns->nLength * 6;
		if (nFlags == RS_STIPCM32S) len += pIns->nLength * 2;
		if (len > dwMemLength) break;
		if (len > 8*8)
		{
			uint32_t slsize = (nFlags == RS_STIPCM32S) ? 4 : 3;
			uint8_t * pSrc = (uint8_t *)lpMemFile;
			int32_t max = 255;
			if (nFlags == RS_STIPCM32S) pSrc++;
			for (uint32_t j=0; j<len; j+=slsize)
			{
				int32_t l = ((((pSrc[j+2] << 8) + pSrc[j+1]) << 8) + pSrc[j]) << 8;
				l /= 256;
				if (l > max) max = l;
				if (-l > max) max = -l;
			}
			max = (max / 128) + 1;
			signed short *pDest = (signed short *)pIns->pSample;
			for (uint32_t k=0; k<len; k+=slsize)
			{
				int32_t lr = ((((pSrc[k+2] << 8) + pSrc[k+1]) << 8) + pSrc[k]) << 8;
				k += slsize;
				int32_t ll = ((((pSrc[k+2] << 8) + pSrc[k+1]) << 8) + pSrc[k]) << 8;
				pDest[0] = (signed short)ll;
				pDest[1] = (signed short)lr;
				pDest += 2;
			}
		}
		break;

	// 16-bit signed big endian interleaved stereo
	case RS_STIPCM16M:
		{
			len = pIns->nLength;
			if (len*4 > dwMemLength) len = dwMemLength >> 2;
			const uint8_t * psrc = (const uint8_t *)lpMemFile;
			short int *pSample = (short int *)pIns->pSample;
			for (uint32_t j=0; j<len; j++)
			{
				pSample[j*2] = (signed short)(((uint32_t)psrc[0] << 8) | (psrc[1]));
				pSample[j*2+1] = (signed short)(((uint32_t)psrc[2] << 8) | (psrc[3]));
				psrc += 4;
			}
			len *= 4;
		}
		break;

	// Default: 8-bit signed PCM data
	default:
		len = pIns->nLength;
		if (len > dwMemLength) len = pIns->nLength = dwMemLength;
		memcpy(pIns->pSample, lpMemFile, len);
	}
	if (len > dwMemLength)
	{
		if (pIns->pSample)
		{
			pIns->nLength = 0;
			FreeSample(pIns->pSample);
			pIns->pSample = NULL;
		}
		return 0;
	}
	AdjustSampleLoop(pIns);
	return len;
}


void CSoundFile::AdjustSampleLoop(SONGSAMPLE *pIns)
//----------------------------------------------------
{
	if (!pIns->pSample) return;
	if (pIns->nLoopEnd > pIns->nLength) pIns->nLoopEnd = pIns->nLength;
	if (pIns->nLoopStart+2 >= pIns->nLoopEnd)
	{
		pIns->nLoopStart = pIns->nLoopEnd = 0;
		pIns->uFlags &= ~CHN_LOOP;
	}
	
	// poopy, removing all that loop-hacking code has produced... very nasty sounding loops!
	// so I guess I should rewrite the crap at the end of the sample at least.
	uint32_t len = pIns->nLength;
	if (pIns->uFlags & CHN_16BIT) {
		short int *pSample = (short int *)pIns->pSample;
		// Adjust end of sample
		if (pIns->uFlags & CHN_STEREO)
		{
			pSample[len*2+6] = pSample[len*2+4] = pSample[len*2+2] = pSample[len*2] = pSample[len*2-2];
			pSample[len*2+7] = pSample[len*2+5] = pSample[len*2+3] = pSample[len*2+1] = pSample[len*2-1];
		} else
		{
			pSample[len+4] = pSample[len+3] = pSample[len+2] = pSample[len+1] = pSample[len] = pSample[len-1];
		}
	} else {
		signed char *pSample = pIns->pSample;
		// Adjust end of sample
		if (pIns->uFlags & CHN_STEREO) {
			pSample[len*2+6] = pSample[len*2+4] = pSample[len*2+2] = pSample[len*2] = pSample[len*2-2];
			pSample[len*2+7] = pSample[len*2+5] = pSample[len*2+3] = pSample[len*2+1] = pSample[len*2-1];
		} else {
			pSample[len+4] = pSample[len+3] = pSample[len+2] = pSample[len+1] = pSample[len] = pSample[len-1];
		}
	}
}




uint32_t CSoundFile::GetHighestUsedChannel()
//------------------------------
{
	uint32_t highchan = 0;

	for (uint32_t ipat=0; ipat<MAX_PATTERNS; ipat++)
	{
		MODCOMMAND *p = Patterns[ipat];
		if (p)
		{
			uint32_t jmax = PatternSize[ipat] * m_nChannels;
			for (uint32_t j=0; j<jmax; j++, p++)
			{
				if ((p->note) && (p->note <= 120))
				{
					if ((j % m_nChannels) > highchan)
						highchan = j % m_nChannels;
				}
			}
		}
	}

	return highchan;
}



uint32_t CSoundFile::DetectUnusedSamples(bool *pbIns)
//-----------------------------------------------
{
	uint32_t nExt = 0;

	if (!pbIns) return 0;
	if (m_dwSongFlags & SONG_INSTRUMENTMODE)
	{
		memset(pbIns, 0, MAX_SAMPLES * sizeof(bool));
		for (uint32_t ipat=0; ipat<MAX_PATTERNS; ipat++)
		{
			MODCOMMAND *p = Patterns[ipat];
			if (p)
			{
				uint32_t jmax = PatternSize[ipat] * m_nChannels;
				for (uint32_t j=0; j<jmax; j++, p++)
				{
					if ((p->note) && (p->note <= 120))
					{
						if ((p->instr) && (p->instr < MAX_INSTRUMENTS))
						{
							SONGINSTRUMENT *penv = Instruments[p->instr];
							if (penv)
							{
								uint32_t n = penv->Keyboard[p->note-1];
								if (n < MAX_SAMPLES) pbIns[n] = true;
							}
						} else
						{
							for (uint32_t k=1; k<=m_nInstruments; k++)
							{
								SONGINSTRUMENT *penv = Instruments[k];
								if (penv)
								{
									uint32_t n = penv->Keyboard[p->note-1];
									if (n < MAX_SAMPLES) pbIns[n] = true;
								}
							}
						}
					}
				}
			}
		}
		for (uint32_t ichk=1; ichk<=m_nSamples; ichk++)
		{
			if ((!pbIns[ichk]) && (Samples[ichk].pSample)) nExt++;
		}
	}
	return nExt;
}


bool CSoundFile::DestroySample(uint32_t nSample)
//------------------------------------------
{
	if ((!nSample) || (nSample >= MAX_SAMPLES)) return false;
	if (!Samples[nSample].pSample) return true;
	SONGSAMPLE *pins = &Samples[nSample];
	signed char *pSample = pins->pSample;
	pins->pSample = NULL;
	pins->nLength = 0;
	pins->uFlags &= ~(CHN_16BIT);
	for (uint32_t i=0; i<MAX_VOICES; i++)
	{
		if (Voices[i].pSample == pSample)
		{
			Voices[i].nPos = Voices[i].nLength = 0;
			Voices[i].pSample = Voices[i].pCurrentSample = NULL;
		}
	}
	FreeSample(pSample);
	return true;
}

