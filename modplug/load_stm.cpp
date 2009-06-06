/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/

#include "sndfile.h"

//#pragma warning(disable:4244)

#pragma pack(1)

typedef struct tagSTMNOTE
{
	uint8_t note;
	uint8_t insvol;
	uint8_t volcmd;
	uint8_t cmdinf;
} STMNOTE;


// Raw STM sampleinfo struct:
typedef struct tagSTMSAMPLE
{
	int8_t filename[14];	// Can't have long comments - just filename comments :)
	uint16_t reserved;		// ISA in memory when in ST 2
	uint16_t length;		// Sample length
	uint16_t loopbeg;		// Loop start point
	uint16_t loopend;		// Loop end point
	uint8_t volume;		// Volume
	uint8_t reserved2;		// More reserved crap
	uint16_t c2spd;			// Good old c2spd
	uint8_t reserved3[6];	// Yet more of PSi's reserved crap
} STMSAMPLE;


// Raw STM header struct:
typedef struct tagSTMHEADER
{
        char songname[20];      // changed from int8_t
	char trackername[8];	// !SCREAM! for ST 2.xx  // changed from int8_t
	int8_t unused;			// 0x1A
	int8_t filetype;			// 1=song, 2=module (only 2 is supported, of course) :)
	int8_t ver_major;			// Like 2
	int8_t ver_minor;			// "ditto"
	uint8_t inittempo;			// initspeed= stm inittempo>>4
	uint8_t numpat;			// number of patterns
	uint8_t globalvol;			// <- WoW! a RiGHT TRiANGLE =8*)
	uint8_t reserved[13];		// More of PSi's internal crap
	STMSAMPLE sample[31];	// STM sample data
	uint8_t patorder[128];		// Docs say 64 - actually 128
} STMHEADER;

#pragma pack()



bool CSoundFile::ReadSTM(const uint8_t *lpStream, uint32_t dwMemLength)
//---------------------------------------------------------------
{
	STMHEADER *phdr = (STMHEADER *)lpStream;
	uint32_t dwMemPos = 0;
	
	if ((!lpStream) || (dwMemLength < sizeof(STMHEADER))) return false;
	if ((phdr->filetype != 2) || (phdr->unused != 0x1A)
	 || ((strncasecmp(phdr->trackername, "!SCREAM!", 8))
	  && (strncasecmp(phdr->trackername, "BMOD2STM", 8)))) return false;
	memcpy(m_szNames[0], phdr->songname, 20);
	// Read STM header
	m_nType = MOD_TYPE_STM;
	m_nSamples = 31;
	m_nChannels = 4;
	m_nInstruments = 0;
	m_nDefaultSpeed = phdr->inittempo >> 4;
	if (m_nDefaultSpeed < 1) m_nDefaultSpeed = 1;
	m_nDefaultTempo = 125;
	m_nDefaultGlobalVolume = phdr->globalvol << 2;
	if (m_nDefaultGlobalVolume > 256) m_nDefaultGlobalVolume = 256;
	memcpy(Order, phdr->patorder, 128);
	// Setting up channels
	for (uint32_t nSet=0; nSet<4; nSet++)
	{
		ChnSettings[nSet].dwFlags = 0;
		ChnSettings[nSet].nVolume = 64;
		ChnSettings[nSet].nPan = (nSet & 1) ? 0x40 : 0xC0;
	}
	// Reading samples
	for (uint32_t nIns=0; nIns<31; nIns++)
	{
		MODINSTRUMENT *pIns = &Ins[nIns+1];
		STMSAMPLE *pStm = &phdr->sample[nIns];  // STM sample data
		memcpy(pIns->name, pStm->filename, 13);
		memcpy(m_szNames[nIns+1], pStm->filename, 12);
		pIns->nC5Speed = pStm->c2spd;
		pIns->nGlobalVol = 64;
		pIns->nVolume = pStm->volume << 2;
		if (pIns->nVolume > 256) pIns->nVolume = 256;
		pIns->nLength = pStm->length;
		if ((pIns->nLength < 2) || (!pIns->nVolume)) pIns->nLength = 0;
		pIns->nLoopStart = pStm->loopbeg;
		pIns->nLoopEnd = pStm->loopend;
		if ((pIns->nLoopEnd > pIns->nLoopStart) && (pIns->nLoopEnd != 0xFFFF)) pIns->uFlags |= CHN_LOOP;
	}
	dwMemPos = sizeof(STMHEADER);
	for (uint32_t nOrd=0; nOrd<MAX_ORDERS; nOrd++) if (Order[nOrd] >= 99) Order[nOrd] = 0xFF;
	uint32_t nPatterns = phdr->numpat;
	for (uint32_t nPat=0; nPat<nPatterns; nPat++)
	{
		if (dwMemPos + 64*4*4 > dwMemLength) return true;
		PatternSize[nPat] = 64;
		PatternAllocSize[nPat] = 64;
		if ((Patterns[nPat] = AllocatePattern(64, m_nChannels)) == NULL) return true;
		MODCOMMAND *m = Patterns[nPat];
		STMNOTE *p = (STMNOTE *)(lpStream + dwMemPos);
		for (uint32_t n=0; n<64*4; n++, p++, m++)
		{
			uint32_t note,ins,vol,cmd;
			// extract the various information from the 4 bytes that
			// make up a single note
			note = p->note;
			ins = p->insvol >> 3;
			vol = (p->insvol & 0x07) + (p->volcmd >> 1);
			cmd = p->volcmd & 0x0F;
			if ((ins) && (ins < 32)) m->instr = ins;
			// special values of [SBYTE0] are handled here ->
			// we have no idea if these strange values will ever be encountered
			// but it appears as though stms sound correct.
			if ((note == 0xFE) || (note == 0xFC)) m->note = 0xFE; else
			// if note < 251, then all three bytes are stored in the file
			if (note < 0xFC) m->note = (note >> 4)*12 + (note&0xf) + 37;
			if (vol <= 64) { m->volcmd = VOLCMD_VOLUME; m->vol = vol; }
			m->param = p->cmdinf;
			switch(cmd)
			{
			// Axx set speed to xx
			case 1:	m->command = CMD_SPEED; m->param >>= 4; break;
			// Bxx position jump
			case 2:	m->command = CMD_POSITIONJUMP; break;
			// Cxx patternbreak to row xx
			case 3:	m->command = CMD_PATTERNBREAK; m->param = (m->param & 0xF0) * 10 + (m->param & 0x0F);	break;
			// Dxy volumeslide
			case 4:	m->command = CMD_VOLUMESLIDE; break;
			// Exy toneslide down
			case 5:	m->command = CMD_PORTAMENTODOWN; break;
			// Fxy toneslide up
			case 6:	m->command = CMD_PORTAMENTOUP; break;
			// Gxx Tone portamento,speed xx
			case 7:	m->command = CMD_TONEPORTAMENTO; break;
			// Hxy vibrato
			case 8:	m->command = CMD_VIBRATO; break;
			// Ixy tremor, ontime x, offtime y
			case 9:	m->command = CMD_TREMOR; break;
			// Jxy arpeggio
			case 10: m->command = CMD_ARPEGGIO; break;
			// Kxy Dual command H00 & Dxy
			case 11: m->command = CMD_VIBRATOVOL; break;
			// Lxy Dual command G00 & Dxy
			case 12: m->command = CMD_TONEPORTAVOL; break;
			// Xxx amiga command 8xx
			case 0x18:	m->command = CMD_PANNING8; break;
			default:
				m->command = m->param = 0;
			}
		}
		dwMemPos += 64*4*4;
	}
	// Reading Samples
	for (uint32_t nSmp=1; nSmp<=31; nSmp++)
	{
		MODINSTRUMENT *pIns = &Ins[nSmp];
		dwMemPos = (dwMemPos + 15) & (~15);
		if (pIns->nLength)
		{
			uint32_t nPos = ((uint32_t)phdr->sample[nSmp-1].reserved) << 4;
			if ((nPos >= sizeof(STMHEADER)) && (nPos+pIns->nLength <= dwMemLength)) dwMemPos = nPos;
			if (dwMemPos < dwMemLength)
			{
				dwMemPos += ReadSample(pIns, RS_PCM8S, (const char *)(lpStream+dwMemPos),dwMemLength-dwMemPos);
			}
		}
	}
	return true;
}

