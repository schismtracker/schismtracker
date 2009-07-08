/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

//////////////////////////////////////////////
// Oktalyzer (OKT) module loader            //
//////////////////////////////////////////////
#include "sndfile.h"

//#pragma warning(disable:4244)

typedef struct OKTFILEHEADER
{
	uint32_t okta;		// "OKTA"
	uint32_t song;		// "SONG"
	uint32_t cmod;		// "CMOD"
	uint32_t fixed8;
	uint8_t chnsetup[8];
	uint32_t samp;		// "SAMP"
	uint32_t samplen;
} OKTFILEHEADER;


typedef struct OKTSAMPLE
{
	int8_t name[20];
	uint32_t length;
	uint16_t loopstart;
	uint16_t looplen;
	uint8_t pad1;
	uint8_t volume;
	uint8_t pad2;
	uint8_t pad3;
} OKTSAMPLE;


bool CSoundFile::ReadOKT(const uint8_t *lpStream, uint32_t dwMemLength)
//---------------------------------------------------------------
{
	OKTFILEHEADER *pfh = (OKTFILEHEADER *)lpStream;
	uint32_t dwMemPos = sizeof(OKTFILEHEADER);
	uint32_t nsamples = 0, npatterns = 0, norders = 0;

	if ((!lpStream) || (dwMemLength < 1024)) return false;
	if ((pfh->okta != 0x41544B4F) || (pfh->song != 0x474E4F53)
	 || (pfh->cmod != 0x444F4D43) || (pfh->chnsetup[0]) || (pfh->chnsetup[2])
	 || (pfh->chnsetup[4]) || (pfh->chnsetup[6]) || (pfh->fixed8 != 0x08000000)
	 || (pfh->samp != 0x504D4153)) return false;
	m_nType = MOD_TYPE_OKT;
	m_nChannels = 4 + pfh->chnsetup[1] + pfh->chnsetup[3] + pfh->chnsetup[5] + pfh->chnsetup[7];
	if (m_nChannels > MAX_VOICES) m_nChannels = MAX_VOICES;
	nsamples = bswapBE32(pfh->samplen) >> 5;
	m_nSamples = nsamples;
	if (m_nSamples >= MAX_SAMPLES) m_nSamples = MAX_SAMPLES-1;
	// Reading samples
	for (uint32_t smp=1; smp <= nsamples; smp++)
	{
		if (dwMemPos >= dwMemLength) return true;
		if (smp < MAX_SAMPLES)
		{
			OKTSAMPLE *psmp = (OKTSAMPLE *)(lpStream + dwMemPos);
			SONGSAMPLE *pins = &Samples[smp];

			memcpy(Samples[smp].name, psmp->name, 20);
			pins->uFlags = 0;
			pins->nLength = bswapBE32(psmp->length) & ~1;
			pins->nLoopStart = bswapBE16(psmp->loopstart);
			pins->nLoopEnd = pins->nLoopStart + bswapBE16(psmp->looplen);
			if (pins->nLoopStart + 2 < pins->nLoopEnd) pins->uFlags |= CHN_LOOP;
			pins->nGlobalVol = 64;
			pins->nVolume = psmp->volume << 2;
			pins->nC5Speed = 8363;
		}
		dwMemPos += sizeof(OKTSAMPLE);
	}
	// SPEE
	if (dwMemPos >= dwMemLength) return true;
	if (*((uint32_t *)(lpStream + dwMemPos)) == 0x45455053)
	{
		m_nDefaultSpeed = lpStream[dwMemPos+9];
		dwMemPos += bswapBE32(*((uint32_t *)(lpStream + dwMemPos + 4))) + 8;
	}
	// SLEN
	if (dwMemPos >= dwMemLength) return true;
	if (*((uint32_t *)(lpStream + dwMemPos)) == 0x4E454C53)
	{
		npatterns = lpStream[dwMemPos+9];
		dwMemPos += bswapBE32(*((uint32_t *)(lpStream + dwMemPos + 4))) + 8;
	}
	// PLEN
	if (dwMemPos >= dwMemLength) return true;
	if (*((uint32_t *)(lpStream + dwMemPos)) == 0x4E454C50)
	{
		norders = lpStream[dwMemPos+9];
		dwMemPos += bswapBE32(*((uint32_t *)(lpStream + dwMemPos + 4))) + 8;
	}
	// PATT
	if (dwMemPos >= dwMemLength) return true;
	if (*((uint32_t *)(lpStream + dwMemPos)) == 0x54544150)
	{
		uint32_t orderlen = norders;
		if (orderlen >= MAX_ORDERS) orderlen = MAX_ORDERS-1;
		for (uint32_t i=0; i<orderlen; i++) Orderlist[i] = lpStream[dwMemPos+10+i];
		for (uint32_t j=orderlen; j>1; j--) { if (Orderlist[j-1]) break; Orderlist[j-1] = 0xFF; }
		dwMemPos += bswapBE32(*((uint32_t *)(lpStream + dwMemPos + 4))) + 8;
	}
	// PBOD
	uint32_t npat = 0;
	while ((dwMemPos+10 < dwMemLength) && (*((uint32_t *)(lpStream + dwMemPos)) == 0x444F4250))
	{
		uint32_t dwPos = dwMemPos + 10;
		uint32_t rows = lpStream[dwMemPos+9];
		if (!rows) rows = 64;
		if (npat < MAX_PATTERNS)
		{
			if ((Patterns[npat] = csf_allocate_pattern(rows, m_nChannels)) == NULL) return true;
			MODCOMMAND *m = Patterns[npat];
			PatternSize[npat] = rows;
			PatternAllocSize[npat] = rows;
			uint32_t imax = m_nChannels*rows;
			for (uint32_t i=0; i<imax; i++, m++, dwPos+=4)
			{
				if (dwPos+4 > dwMemLength) break;
				const uint8_t *p = lpStream+dwPos;
				uint32_t note = p[0];
				if (note)
				{
					m->note = note + 48;
					m->instr = p[1] + 1;
				}
				uint32_t command = p[2];
				uint32_t param = p[3];
				m->param = param;
				switch(command)
				{
				// 0: no effect
				case 0:
					break;
				// 1: Portamento Up
				case 1:
				case 17:
				case 30:
					if (param) m->command = CMD_PORTAMENTOUP;
					break;
				// 2: Portamento Down
				case 2:
				case 13:
				case 21:
					if (param) m->command = CMD_PORTAMENTODOWN;
					break;
				// 10: Arpeggio
				case 10:
				case 11:
				case 12:
					m->command = CMD_ARPEGGIO;
					break;
				// 15: Filter
				case 15:
					m->command = CMD_S3MCMDEX;
					m->param = param & 0x0F;
					break;
				// 25: Position Jump
				case 25:
					m->command = CMD_POSITIONJUMP;
					break;
				// 28: Set Speed
				case 28:
					m->command = CMD_SPEED;
					break;
				// 31: Volume Control
				case 31:
					if (param <= 0x40) {
						// 00-3F -> volume
						m->volcmd = VOLCMD_VOLUME;
						m->vol = param;
						m->param = 0;
					} else if (param <= 0x50) {
						// 4x -> D0x
						m->command = CMD_VOLUMESLIDE;
						m->param &= 0x0F;
						if (!m->param)
							m->param = 0x0F;
					} else if (param <= 0x60) {
						// 5x -> Dx0
						m->command = CMD_VOLUMESLIDE;
						m->param = (param & 0x0F) << 4;
						if (!m->param)
							m->param = 0xF0;
					} else if (param <= 0x70) {
						// 6x -> DFx
						m->command = CMD_VOLUMESLIDE;
						m->param = 0xF0 | (param & 0x0F);
						if (!(param & 0x0F))
							m->param = 0xFE;
					} else if (param <= 0x80) {
						// 7x -> DxF
						m->command = CMD_VOLUMESLIDE;
						m->param = 0x0F | ((param & 0x0F) << 4);
						if (!(param & 0xF0))
							m->param = 0xEF;
					}
					break;
				}
			}
		}
		npat++;
		dwMemPos += bswapBE32(*((uint32_t *)(lpStream + dwMemPos + 4))) + 8;
	}
	// SBOD
	uint32_t nsmp = 1;
	while ((dwMemPos+10 < dwMemLength) && (*((uint32_t *)(lpStream + dwMemPos)) == 0x444F4253))
	{
		if (nsmp < MAX_SAMPLES) csf_read_sample(&Samples[nsmp], RS_PCM8S, (const char *)(lpStream+dwMemPos+8), dwMemLength-dwMemPos-8);
		dwMemPos += bswapBE32(*((uint32_t *)(lpStream + dwMemPos + 4))) + 8;
		nsmp++;
	}
	return true;
}

