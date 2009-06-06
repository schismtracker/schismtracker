/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

////////////////////////////////////////////////////////////
// 669 Composer / UNIS 669 module loader
////////////////////////////////////////////////////////////

#include "sndfile.h"

//#pragma warning(disable:4244)

typedef struct tagFILEHEADER669
{
	uint16_t sig;				// 'if' or 'JN'
        signed char songmessage[108];	// Song Message
	uint8_t samples;			// number of samples (1-64)
	uint8_t patterns;			// number of patterns (1-128)
	uint8_t restartpos;
	uint8_t orders[128];
	uint8_t tempolist[128];
	uint8_t breaks[128];
} FILEHEADER669;


typedef struct tagSAMPLE669
{
	uint8_t filename[13];
	unsigned int length, loopstart, loopend;
} SAMPLE669;


bool CSoundFile::Read669(const uint8_t *lpStream, uint32_t dwMemLength)
//---------------------------------------------------------------
{
	bool b669Ext;
	const FILEHEADER669 *pfh = (const FILEHEADER669 *)lpStream;
	const SAMPLE669 *psmp = (const SAMPLE669 *)(lpStream + 0x1F1);
	uint32_t dwMemPos = 0;

	if ((!lpStream) || (dwMemLength < sizeof(FILEHEADER669))) return false;
	if ((bswapLE16(pfh->sig) != 0x6669) && (bswapLE16(pfh->sig) != 0x4E4A)) return false;
	b669Ext = (bswapLE16(pfh->sig) == 0x4E4A) ? true : false;
	if ((!pfh->samples) || (pfh->samples > 64) || (pfh->restartpos >= 128)
	 || (!pfh->patterns) || (pfh->patterns > 128)) return false;
	uint32_t dontfuckwithme = 0x1F1 + pfh->samples * sizeof(SAMPLE669) + pfh->patterns * 0x600;
	if (dontfuckwithme > dwMemLength) return false;
        for (int n = 0; n < 128; n++)
                if (pfh->breaks[n] > 0x3f)
                        return false;
	// That should be enough checking: this must be a 669 module.
	m_nType = MOD_TYPE_669;
	m_dwSongFlags |= SONG_LINEARSLIDES;
	m_nDefaultTempo = 125;
	m_nDefaultSpeed = 6;
	m_nChannels = 8;
	memcpy(m_szNames[0], pfh->songmessage, 31);
	m_szNames[0][31] = 0;
	m_nSamples = pfh->samples;
	for (uint32_t nins=1; nins<=m_nSamples; nins++, psmp++)
	{
		uint32_t len = bswapLE32(psmp->length);
		uint32_t loopstart = bswapLE32(psmp->loopstart);
		uint32_t loopend = bswapLE32(psmp->loopend);
		if (len > MAX_SAMPLE_LENGTH) len = MAX_SAMPLE_LENGTH;
		if ((loopend > len) && (!loopstart)) loopend = 0;
		if (loopend > len) loopend = len;
		if (loopstart + 4 >= loopend) loopstart = loopend = 0;
		Ins[nins].nLength = len;
		Ins[nins].nLoopStart = loopstart;
		Ins[nins].nLoopEnd = loopend;
		if (loopend) Ins[nins].uFlags |= CHN_LOOP;
		memcpy(m_szNames[nins], psmp->filename, 13);
		Ins[nins].nVolume = 256;
		Ins[nins].nGlobalVol = 64;
		Ins[nins].nPan = 128;
		Ins[nins].nC5Speed = 8363;
	}
	// Song Message
	m_lpszSongComments = new char[114];
	memcpy(m_lpszSongComments, pfh->songmessage, 36);
	m_lpszSongComments[36] = '\015';
	m_lpszSongComments[37] = '\012';
	memcpy(m_lpszSongComments + 38, pfh->songmessage + 36, 36);
	m_lpszSongComments[74] = '\015';
	m_lpszSongComments[75] = '\012';
	memcpy(m_lpszSongComments + 76, pfh->songmessage + 72, 36);
	m_lpszSongComments[112] = 0;
	// Reading Orders
	memcpy(Order, pfh->orders, 128);
	m_nRestartPos = pfh->restartpos;
	if (Order[m_nRestartPos] >= pfh->patterns) m_nRestartPos = 0;
	// Reading Pattern Break Locations
	for (uint32_t npan=0; npan<8; npan++)
	{
		ChnSettings[npan].nPan = (npan & 1) ? 0x30 : 0xD0;
		ChnSettings[npan].nVolume = 64;
	}
	// Reading Patterns
	dwMemPos = 0x1F1 + pfh->samples * 25;
	for (uint32_t npat=0; npat<pfh->patterns; npat++)
	{
		Patterns[npat] = AllocatePattern(64, m_nChannels);
		if (!Patterns[npat]) break;
		PatternSize[npat] = 64;
		PatternAllocSize[npat] = 64;
		MODCOMMAND *m = Patterns[npat];
		const uint8_t *p = lpStream + dwMemPos;
		for (uint32_t row=0; row<64; row++)
		{
			MODCOMMAND *mspeed = m;
			if ((row == pfh->breaks[npat]) && (row != 63))
			{
				for (uint32_t i=0; i<8; i++)
				{
					m[i].command = CMD_PATTERNBREAK;
					m[i].param = 0;
				}
			}
			for (uint32_t n=0; n<8; n++, m++, p+=3)
			{
				uint32_t note = p[0] >> 2;
				uint32_t instr = ((p[0] & 0x03) << 4) | (p[1] >> 4);
				uint32_t vol = p[1] & 0x0F;
				if (p[0] < 0xFE)
				{
					m->note = note + 37;
					m->instr = instr + 1;
				}
				if (p[0] <= 0xFE)
				{
					m->volcmd = VOLCMD_VOLUME;
					m->vol = (vol << 2) + 2;
				}
				if (p[2] != 0xFF)
				{
					uint32_t command = p[2] >> 4;
					uint32_t param = p[2] & 0x0F;
					switch(command)
					{
					case 0x00:
						command = CMD_PORTAMENTOUP;
						break;
					case 0x01:
						command = CMD_PORTAMENTODOWN;
						break;
					case 0x02:
						command = CMD_TONEPORTAMENTO;
						break;
					case 0x03: // set finetune
						command = CMD_S3MCMDEX;
						param |= 0x20;
						break;
					case 0x04:
						command = CMD_VIBRATO;
						param |= 0x40;
						break;
					case 0x05:
						if (param) {
							command = CMD_SPEED;
							param += 2;
						} else {
							command = param = 0;
						}
						break;
					case 0x06:
						if (param == 0) {
							command = CMD_PANNINGSLIDE;
							param = 0xFE;
						} else if (param == 1) {
							command = CMD_PANNINGSLIDE;
							param = 0xEF;
						} else {
							command = 0;
						}
						break;
					default:
						command = 0;
					}
					if (command)
					{
						if (command == CMD_SPEED) mspeed = NULL;
						m->command = command;
						m->param = param;
					}
				}
			}
			if ((!row) && (mspeed))
			{
				for (uint32_t i=0; i<8; i++) if (!mspeed[i].command)
				{
					mspeed[i].command = CMD_SPEED;
					mspeed[i].param = pfh->tempolist[npat] + 2;
					break;
				}
			}
		}
		dwMemPos += 0x600;
	}
	// Reading Samples
	for (uint32_t n=1; n<=m_nSamples; n++)
	{
		uint32_t len = Ins[n].nLength;
		if (dwMemPos >= dwMemLength) break;
		if (len > 4) ReadSample(&Ins[n], RS_PCM8U, (const char *)(lpStream+dwMemPos), dwMemLength - dwMemPos);
		dwMemPos += len;
	}
	return true;
}


