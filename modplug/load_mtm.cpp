/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/
#define NEED_BYTESWAP

#include "sndfile.h"

//#pragma warning(disable:4244)

//////////////////////////////////////////////////////////
// MTM file support (import only)

#pragma pack(1)


typedef struct tagMTMSAMPLE
{
        char samplename[22];      // changed from int8_t
	uint32_t length;
	uint32_t reppos;
	uint32_t repend;
	int8_t finetune;
	uint8_t volume;
	uint8_t attribute;
} MTMSAMPLE;


typedef struct tagMTMHEADER
{
	char id[4];	        // MTM file marker + version // changed from int8_t
	char songname[20];	// ASCIIZ songname  // changed from int8_t
	uint16_t numtracks;		// number of tracks saved
	uint8_t lastpattern;	// last pattern number saved
	uint8_t lastorder;		// last order number to play (songlength-1)
	uint16_t commentsize;	// length of comment field
	uint8_t numsamples;	// number of samples saved
	uint8_t attribute;		// attribute byte (unused)
	uint8_t beatspertrack;
	uint8_t numchannels;	// number of channels used
	uint8_t panpos[32];	// voice pan positions
} MTMHEADER;


#pragma pack()


bool CSoundFile::ReadMTM(const uint8_t * lpStream, uint32_t dwMemLength)
//-----------------------------------------------------------
{
	MTMHEADER *pmh = (MTMHEADER *)lpStream;
	uint32_t dwMemPos = 66;

	if ((!lpStream) || (dwMemLength < 0x100)) return false;
	if ((strncmp(pmh->id, "MTM", 3)) || (pmh->numchannels > 32)
	 || (pmh->numsamples >= MAX_SAMPLES) || (!pmh->numsamples)
	 || (!pmh->numtracks) || (!pmh->numchannels)
	 || (!pmh->lastpattern) || (pmh->lastpattern > MAX_PATTERNS)) return false;
	strncpy(song_title, pmh->songname, 20);
	song_title[20] = 0;
	if (dwMemPos + 37*pmh->numsamples + 128 + 192*bswapLE16(pmh->numtracks)
	 + 64 * (pmh->lastpattern+1) + bswapLE16(pmh->commentsize) >= dwMemLength) return false;
	m_nType = MOD_TYPE_MTM;
	m_nSamples = pmh->numsamples;
	m_nChannels = pmh->numchannels;
	// Reading instruments
	for	(uint32_t i=1; i<=m_nSamples; i++)
	{
		MTMSAMPLE *pms = (MTMSAMPLE *)(lpStream + dwMemPos);
		strncpy(m_szNames[i], pms->samplename, 22);
		m_szNames[i][22] = 0;
		Samples[i].nVolume = pms->volume << 2;
		Samples[i].nGlobalVol = 64;
		uint32_t len = bswapLE32(pms->length);
		if ((len > 4) && (len <= MAX_SAMPLE_LENGTH))
		{
			Samples[i].nLength = len;
			Samples[i].nLoopStart = bswapLE32(pms->reppos);
			Samples[i].nLoopEnd = bswapLE32(pms->repend);
			if (Samples[i].nLoopEnd > Samples[i].nLength) Samples[i].nLoopEnd = Samples[i].nLength;
			if (Samples[i].nLoopStart + 4 >= Samples[i].nLoopEnd) Samples[i].nLoopStart = Samples[i].nLoopEnd = 0;
			if (Samples[i].nLoopEnd) Samples[i].uFlags |= CHN_LOOP;
			Samples[i].nC5Speed = S3MFineTuneTable[(pms->finetune & 0x0F) ^ 8];
			if (pms->attribute & 0x01)
			{
				Samples[i].uFlags |= CHN_16BIT;
				Samples[i].nLength >>= 1;
				Samples[i].nLoopStart >>= 1;
				Samples[i].nLoopEnd >>= 1;
			}
			Samples[i].nPan = 128;
		}
		dwMemPos += 37;
	}
	// Setting Channel Pan Position
	for (uint32_t ich=0; ich<m_nChannels; ich++)
	{
		Channels[ich].nPan = ((pmh->panpos[ich] & 0x0F) << 4) + 8;
		Channels[ich].nVolume = 64;
	}
	// Reading pattern order
	memcpy(Orderlist, lpStream + dwMemPos, pmh->lastorder+1);
	dwMemPos += 128;
	// Reading Patterns
	const uint8_t * pTracks = lpStream + dwMemPos;
	dwMemPos += 192 * bswapLE16(pmh->numtracks);
	uint32_t * pSeq = (uint32_t *)(lpStream + dwMemPos);
	for (uint32_t pat=0; pat<=pmh->lastpattern; pat++)
	{
		PatternSize[pat] = 64;
		PatternAllocSize[pat] = 64;
		if ((Patterns[pat] = AllocatePattern(64, m_nChannels)) == NULL) break;
		for (uint32_t n=0; n<32; n++) if ((pSeq[n]) && (pSeq[n] <= bswapLE16(pmh->numtracks)) && (n < m_nChannels))
		{
			const uint8_t * p = pTracks + 192 * (pSeq[n]-1);
			MODCOMMAND *m = Patterns[pat] + n;
			for (uint32_t i=0; i<64; i++, m+=m_nChannels, p+=3)
			{
				if (p[0] & 0xFC) m->note = (p[0] >> 2) + 37;
				m->instr = ((p[0] & 0x03) << 4) | (p[1] >> 4);
				uint32_t cmd = p[1] & 0x0F;
				uint32_t param = p[2];
				if (cmd == 0x0A)
				{
					if (param & 0xF0) param &= 0xF0; else param &= 0x0F;
				}
				m->command = cmd;
				m->param = param;
				if ((cmd) || (param)) ConvertModCommand(m, 0);
			}
		}
		pSeq += 32;
	}
	dwMemPos += 64*(pmh->lastpattern+1);
	if (bswapLE16(pmh->commentsize) && (dwMemPos + bswapLE16(pmh->commentsize) < dwMemLength))
	{
		uint32_t n = bswapLE16(pmh->commentsize);
		m_lpszSongComments = new char[n+1];
		if (m_lpszSongComments)
		{
			memcpy(m_lpszSongComments, lpStream+dwMemPos, n);
			m_lpszSongComments[n] = 0;
			for (uint32_t i=0; i<n; i++)
			{
				if (!m_lpszSongComments[i])
				{
					m_lpszSongComments[i] = ((i+1) % 40) ? 0x20 : 0x0D;
				}
			}
		}
	}
	dwMemPos += bswapLE16(pmh->commentsize);
	// Reading Samples
	for (uint32_t ismp=1; ismp<=m_nSamples; ismp++)
	{
		if (dwMemPos >= dwMemLength) break;
		dwMemPos += ReadSample(&Samples[ismp], (Samples[ismp].uFlags & CHN_16BIT) ? RS_PCM16U : RS_PCM8U,
								(const char *)(lpStream + dwMemPos), dwMemLength - dwMemPos);
	}
	return true;
}

