/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/

////////////////////////////////////////
// Farandole (FAR) module loader	  //
////////////////////////////////////////
#include "sndfile.h"

//#pragma warning(disable:4244)

#define FARFILEMAGIC	0xFE524146	// "FAR"

#pragma pack(1)

typedef struct FARHEADER1
{
	uint32_t id;				// file magic FAR=
	int8_t songname[40];		// songname
	int8_t magic2[3];			// 13,10,26
	uint16_t headerlen;			// remaining length of header in bytes
	uint8_t version;			// 0xD1
	uint8_t onoff[16];
	uint8_t edit1[9];
	uint8_t speed;
	uint8_t panning[16];
	uint8_t edit2[4];
	uint16_t stlen;
} FARHEADER1;

typedef struct FARHEADER2
{
	uint8_t orders[256];
	uint8_t numpat;
	uint8_t snglen;
	uint8_t loopto;
	uint16_t patsiz[256];
} FARHEADER2;

typedef struct FARSAMPLE
{
	int8_t samplename[32];
	uint32_t length;
	uint8_t finetune;
	uint8_t volume;
	uint32_t reppos;
	uint32_t repend;
	uint8_t type;
	uint8_t loop;
} FARSAMPLE;

#pragma pack()


bool CSoundFile::ReadFAR(const uint8_t *lpStream, uint32_t dwMemLength)
//---------------------------------------------------------------
{
	FARHEADER1 *pmh1 = (FARHEADER1 *)lpStream;
	FARHEADER2 *pmh2;
	uint32_t dwMemPos = sizeof(FARHEADER1);
	uint32_t headerlen;
	uint8_t samplemap[8];

	if ((!lpStream) || (dwMemLength < 1024) || (pmh1->id != FARFILEMAGIC)
	 || (pmh1->magic2[0] != 13) || (pmh1->magic2[1] != 10) || (pmh1->magic2[2] != 26)) return false;
	headerlen = pmh1->headerlen;
	if ((headerlen >= dwMemLength) || (dwMemPos + pmh1->stlen + sizeof(FARHEADER2) >= dwMemLength)) return false;
	// Globals
	m_nType = MOD_TYPE_FAR;
	m_nChannels = 16;
	m_nInstruments = 0;
	m_nSamples = 0;
	m_nSongPreAmp = 0x20;
	m_nDefaultSpeed = pmh1->speed;
	m_nDefaultTempo = 80;
	m_nDefaultGlobalVolume = 128;

	memcpy(song_title, pmh1->songname, 32);
	// Channel Setting
	for (uint32_t nchpan=0; nchpan<16; nchpan++)
	{
		Channels[nchpan].dwFlags = 0;
		Channels[nchpan].nPan = ((pmh1->panning[nchpan] & 0x0F) << 4) + 8;
		Channels[nchpan].nVolume = 64;
	}
	// Reading comment
	if (pmh1->stlen)
	{
		uint32_t szLen = pmh1->stlen;
		if (szLen > dwMemLength - dwMemPos) szLen = dwMemLength - dwMemPos;
		if ((m_lpszSongComments = new char[szLen + 1]) != NULL)
		{
			memcpy(m_lpszSongComments, lpStream+dwMemPos, szLen);
			m_lpszSongComments[szLen] = 0;
		}
		dwMemPos += pmh1->stlen;
	}
	// Reading orders
	pmh2 = (FARHEADER2 *)(lpStream + dwMemPos);
	dwMemPos += sizeof(FARHEADER2);
	if (dwMemPos >= dwMemLength) return true;
	for (uint32_t iorder=0; iorder<MAX_ORDERS; iorder++)
	{
		Orderlist[iorder] = (iorder <= pmh2->snglen) ? pmh2->orders[iorder] : 0xFF;
	}
	//m_nRestartPos = pmh2->loopto;
	// Reading Patterns	
	dwMemPos += headerlen - (869 + pmh1->stlen);
	if (dwMemPos >= dwMemLength) return true;

	uint16_t *patsiz = (uint16_t *)pmh2->patsiz;
	for (uint32_t ipat=0; ipat<256; ipat++) if (patsiz[ipat])
	{
		uint32_t patlen = patsiz[ipat];
		if ((ipat >= MAX_PATTERNS) || (patsiz[ipat] < 2))
		{
			dwMemPos += patlen;
			continue;
		}
		if (dwMemPos + patlen >= dwMemLength) return true;
		uint32_t rows = (patlen - 2) >> 6;
		if (!rows)
		{
			dwMemPos += patlen;
			continue;
		}
		if (rows > 256) rows = 256;
		if (rows < 16) rows = 16;
		PatternSize[ipat] = rows;
		PatternAllocSize[ipat] = rows;
		if ((Patterns[ipat] = csf_allocate_pattern(rows, m_nChannels)) == NULL) return true;
		MODCOMMAND *m = Patterns[ipat];
		uint32_t patbrk = lpStream[dwMemPos];
		const uint8_t *p = lpStream + dwMemPos + 2;
		uint32_t max = rows*16*4;
		if (max > patlen-2) max = patlen-2;
		for (uint32_t len=0; len<max; len += 4, m++)
		{
			uint8_t note = p[len];
			uint8_t ins = p[len+1];
			uint8_t vol = p[len+2];
			uint8_t eff = p[len+3];
			if (note)
			{
				m->instr = ins + 1;
				m->note = note + 36;
			}
			if (vol & 0x0F)
			{
				m->volcmd = VOLCMD_VOLUME;
				m->vol = (vol & 0x0F) << 2;
				if (m->vol <= 4) m->vol = 0;
			}
			switch(eff & 0xF0)
			{
			// 1.x: Portamento Up
			case 0x10:
				m->command = CMD_PORTAMENTOUP;
				m->param = eff & 0x0F;
				break;
			// 2.x: Portamento Down
			case 0x20:
				m->command = CMD_PORTAMENTODOWN;
				m->param = eff & 0x0F;
				break;
			// 3.x: Tone-Portamento
			case 0x30:
				m->command = CMD_TONEPORTAMENTO;
				m->param = (eff & 0x0F) << 2;
				break;
			// 4.x: Retrigger
			case 0x40:
				m->command = CMD_RETRIG;
				m->param = 6 / (1+(eff&0x0F)) + 1;
				break;
			// 5.x: Set Vibrato Depth
			case 0x50:
				m->command = CMD_VIBRATO;
				m->param = (eff & 0x0F);
				break;
			// 6.x: Set Vibrato Speed
			case 0x60:
				m->command = CMD_VIBRATO;
				m->param = (eff & 0x0F) << 4;
				break;
			// 7.x: Vol Slide Up
			case 0x70:
				m->command = CMD_VOLUMESLIDE;
				m->param = (eff & 0x0F) << 4;
				break;
			// 8.x: Vol Slide Down
			case 0x80:
				m->command = CMD_VOLUMESLIDE;
				m->param = (eff & 0x0F);
				break;
			// A.x: Port to vol
			case 0xA0:
				m->volcmd = VOLCMD_VOLUME;
				m->vol = ((eff & 0x0F) << 2) + 4;
				break;
			// B.x: Set Balance
			case 0xB0:
				m->command = CMD_PANNING8;
				m->param = (eff & 0x0F) << 4;
				break;
			// F.x: Set Speed
			case 0xF0:
				m->command = CMD_SPEED;
				m->param = eff & 0x0F;
				break;
			default:
				if ((patbrk) &&	(patbrk+1 == (len >> 6)) && (patbrk+1 != rows-1))
				{
					m->command = CMD_PATTERNBREAK;
					patbrk = 0;
				}
			}
		}
		dwMemPos += patlen;
	}
	// Reading samples
	if (dwMemPos + 8 >= dwMemLength) return true;
	memcpy(samplemap, lpStream+dwMemPos, 8);
	dwMemPos += 8;
	SONGSAMPLE *pins = &Samples[1];
	for (uint32_t ismp=0; ismp<64; ismp++, pins++) if (samplemap[ismp >> 3] & (1 << (ismp & 7)))
	{
		if (dwMemPos + sizeof(FARSAMPLE) > dwMemLength) return true;
		FARSAMPLE *pfs = (FARSAMPLE *)(lpStream + dwMemPos);
		dwMemPos += sizeof(FARSAMPLE);
		m_nSamples = ismp + 1;
		memcpy(pins->name, pfs->samplename, 32);
		pins->nLength = pfs->length;
		pins->nLoopStart = pfs->reppos;
		pins->nLoopEnd = pfs->repend;
		pins->nC5Speed = 8363*2;
		pins->nGlobalVol = 64;
		pins->nVolume = pfs->volume << 4;
		pins->uFlags = 0;
		if ((pins->nLength > 3) && (dwMemPos + 4 < dwMemLength))
		{
			if (pfs->type & 1)
			{
				pins->uFlags |= CHN_16BIT;
				pins->nLength >>= 1;
				pins->nLoopStart >>= 1;
				pins->nLoopEnd >>= 1;
			}
			if ((pfs->loop & 8) && (pins->nLoopEnd > 4)) pins->uFlags |= CHN_LOOP;
			csf_read_sample(pins, (pins->uFlags & CHN_16BIT) ? RS_PCM16S : RS_PCM8S,
						(const char *)(lpStream+dwMemPos), dwMemLength - dwMemPos);
		}
		dwMemPos += pfs->length;
	}
	return true;
}

