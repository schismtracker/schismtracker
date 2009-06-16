/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

#include "sndfile.h"
#include "snd_fx.h"

//////////////////////////////////////////////////////////
// ProTracker / NoiseTracker MOD/NST file support

void CSoundFile::ConvertModCommand(MODCOMMAND *m, bool from_xm) const
//-----------------------------------------------------
{
	uint32_t command = m->command, param = m->param;

	switch(command)
	{
	case 0x00:	if (param) command = CMD_ARPEGGIO; break;
	case 0x01:	command = CMD_PORTAMENTOUP; break;
	case 0x02:	command = CMD_PORTAMENTODOWN; break;
	case 0x03:	command = CMD_TONEPORTAMENTO; break;
	case 0x04:	command = CMD_VIBRATO; break;
	case 0x05:	command = CMD_TONEPORTAVOL; if (param & 0xF0) param &= 0xF0; break;
	case 0x06:	command = CMD_VIBRATOVOL; if (param & 0xF0) param &= 0xF0; break;
	case 0x07:	command = CMD_TREMOLO; break;
	case 0x08:
		command = CMD_PANNING8;
		if (!from_xm) {
			param *= 2;
			if (param > 0x7f) param = 0xff;
		}
		break;
	case 0x09:	command = CMD_OFFSET; break;
	case 0x0A:	command = CMD_VOLUMESLIDE; if (param & 0xF0) param &= 0xF0; break;
	case 0x0B:	command = CMD_POSITIONJUMP; break;
	case 0x0C:
		if (from_xm) {
			command = CMD_VOLUME;
		} else {
			m->volcmd = VOLCMD_VOLUME;
			m->vol = param;
			if (m->vol > 64)
				m->vol = 64;
			command = param = 0;
		}
		break;
	case 0x0D:	command = CMD_PATTERNBREAK; param = ((param >> 4) * 10) + (param & 0x0F); break;
	case 0x0E:
		command = CMD_S3MCMDEX;
		switch(param & 0xF0) {
			case 0x10: command = CMD_PORTAMENTOUP; param |= 0xF0; break;
			case 0x20: command = CMD_PORTAMENTODOWN; param |= 0xF0; break;
			case 0x30: param = (param & 0x0F) | 0x10; break;
			case 0x40: param = (param & 0x0F) | 0x30; break;
			case 0x50: param = (param & 0x0F) | 0x20; break;
			case 0x60: param = (param & 0x0F) | 0xB0; break;
			case 0x70: param = (param & 0x0F) | 0x40; break;
			case 0x90: command = CMD_RETRIG; param &= 0x0F; break;
			case 0xA0:
				if (param & 0x0F) {
					command = CMD_VOLUMESLIDE;
					param = (param << 4) | 0x0F;
				} else {
					command = param = 0;
				}
				break;
			case 0xB0:
				if (param & 0x0F) {
					command = CMD_VOLUMESLIDE;
					param |= 0xF0;
				} else {
					command=param=0;
				}
				break;
		}
		break;
	case 0x0F:
		command = (param < (from_xm ? 0x21 : 0x20)) ? CMD_SPEED : CMD_TEMPO;
		// I have no idea what this next line is supposed to do.
		if ((param == 0xFF) && (m_nSamples == 15)) command = 0; break;
	// Extension for XM extended effects
	case 'G' - 55:	command = CMD_GLOBALVOLUME; break;
	case 'H' - 55:	command = CMD_GLOBALVOLSLIDE; if (param & 0xF0) param &= 0xF0; break;
	case 'K' - 55:	command = CMD_KEYOFF; break;
	case 'L' - 55:	command = CMD_SETENVPOSITION; break;
	case 'M' - 55:	command = CMD_CHANNELVOLUME; break;
	case 'N' - 55:	command = CMD_CHANNELVOLSLIDE; break;
	case 'P' - 55:	command = CMD_PANNINGSLIDE; if (param & 0xF0) param &= 0xF0; break;
	case 'R' - 55:	command = CMD_RETRIG; break;
	case 'T' - 55:	command = CMD_TREMOR; break;
	case 'X' - 55:	command = CMD_XFINEPORTAUPDOWN;	break;
	case 'Y' - 55:	command = CMD_PANBRELLO; break;
	case 'Z' - 55:	command = CMD_MIDI;	break;
	default:	command = 0;
	}
	m->command = command;
	m->param = param;
}


uint16_t CSoundFile::ModSaveCommand(const MODCOMMAND *m, bool bXM) const
//------------------------------------------------------------------
{
	uint32_t command = m->command & 0x3F, param = m->param;

	switch(command)
	{
	case 0:						command = param = 0; break;
	case CMD_ARPEGGIO:			command = 0; break;
	case CMD_PORTAMENTOUP:
		if ((param & 0xF0) == 0xE0) { command=0x0E; param=((param & 0x0F) >> 2)|0x10; break; }
		else if ((param & 0xF0) == 0xF0) { command=0x0E; param &= 0x0F; param|=0x10; break; }
		command = 0x01;
		break;
	case CMD_PORTAMENTODOWN:
		if ((param & 0xF0) == 0xE0) { command=0x0E; param=((param & 0x0F) >> 2)|0x20; break; }
		else if ((param & 0xF0) == 0xF0) { command=0x0E; param &= 0x0F; param|=0x20; break; }
		command = 0x02;
		break;
	case CMD_TONEPORTAMENTO:	command = 0x03; break;
	case CMD_VIBRATO:			command = 0x04; break;
	case CMD_TONEPORTAVOL:		command = 0x05; break;
	case CMD_VIBRATOVOL:		command = 0x06; break;
	case CMD_TREMOLO:			command = 0x07; break;
	case CMD_PANNING8:			
		command = 0x08;
		if (!bXM) param >>= 1;
		break;
	case CMD_OFFSET:			command = 0x09; break;
	case CMD_VOLUMESLIDE:		command = 0x0A; break;
	case CMD_POSITIONJUMP:		command = 0x0B; break;
	case CMD_VOLUME:			command = 0x0C; break;
	case CMD_PATTERNBREAK:		command = 0x0D; param = ((param / 10) << 4) | (param % 10); break;
	case CMD_SPEED:				command = 0x0F; if (param > 0x20) param = 0x20; break;
	case CMD_TEMPO:				if (param > 0x20) { command = 0x0F; break; } return 0;
	case CMD_GLOBALVOLUME:		command = 'G' - 55; break;
	case CMD_GLOBALVOLSLIDE:	command = 'H' - 55; break;
	case CMD_KEYOFF:			command = 'K' - 55; break;
	case CMD_SETENVPOSITION:	command = 'L' - 55; break;
	case CMD_CHANNELVOLUME:		command = 'M' - 55; break;
	case CMD_CHANNELVOLSLIDE:	command = 'N' - 55; break;
	case CMD_PANNINGSLIDE:		command = 'P' - 55; break;
	case CMD_RETRIG:			command = 'R' - 55; break;
	case CMD_TREMOR:			command = 'T' - 55; break;
	case CMD_XFINEPORTAUPDOWN:	command = 'X' - 55; break;
	case CMD_PANBRELLO:			command = 'Y' - 55; break;
	case CMD_MIDI:				command = 'Z' - 55; break;
	case CMD_S3MCMDEX:
		switch(param & 0xF0)
		{
		case 0x10:	command = 0x0E; param = (param & 0x0F) | 0x30; break;
		case 0x20:	command = 0x0E; param = (param & 0x0F) | 0x50; break;
		case 0x30:	command = 0x0E; param = (param & 0x0F) | 0x40; break;
		case 0x40:	command = 0x0E; param = (param & 0x0F) | 0x70; break;
		case 0x90:	command = 'X' - 55; break;
		case 0xB0:	command = 0x0E; param = (param & 0x0F) | 0x60; break;
		case 0xA0:
		case 0x50:
		case 0x70:
		case 0x60:	command = param = 0; break;
		default:	command = 0x0E; break;
		}
		break;
	default:		command = param = 0;
	}
	return (uint16_t)((command << 8) | (param));
}


#pragma pack(1)

typedef struct _MODSAMPLE
{
	int8_t name[22];
	uint16_t length;
	uint8_t finetune;
	uint8_t volume;
	uint16_t loopstart;
	uint16_t looplen;
} MODSAMPLE, *PMODSAMPLE;

typedef struct _MODMAGIC
{
	uint8_t nOrders;
	uint8_t nRestartPos;
	uint8_t Orders[128];
        char Magic[4];          // changed from int8_t
} MODMAGIC, *PMODMAGIC;

#pragma pack()

bool IsMagic(const char * s1, const char * s2)
{
	return ((*(uint32_t *)s1) == (*(uint32_t *)s2)) ? true : false;
}


bool CSoundFile::ReadMod(const uint8_t *lpStream, uint32_t dwMemLength)
//---------------------------------------------------------------
{
        char s[1024];          // changed from int8_t
	uint32_t dwMemPos, dwTotalSampleLen;
	PMODMAGIC pMagic;
	uint32_t nErr;

	if ((!lpStream) || (dwMemLength < 0x600)) return false;
	dwMemPos = 20;
	m_nSamples = 31;
	m_nChannels = 4;
	pMagic = (PMODMAGIC)(lpStream+dwMemPos+sizeof(MODSAMPLE)*31);
	// Check Mod Magic
	memcpy(s, pMagic->Magic, 4);
	if ((IsMagic(s, "M.K.")) || (IsMagic(s, "M!K!"))
	 || (IsMagic(s, "M&K!")) || (IsMagic(s, "N.T."))) m_nChannels = 4; else
	if ((IsMagic(s, "CD81")) || (IsMagic(s, "OKTA"))) m_nChannels = 8; else
	if ((s[0]=='F') && (s[1]=='L') && (s[2]=='T') && (s[3]>='4') && (s[3]<='9')) m_nChannels = s[3] - '0'; else
	if ((s[0]>='4') && (s[0]<='9') && (s[1]=='C') && (s[2]=='H') && (s[3]=='N')) m_nChannels = s[0] - '0'; else
	if ((s[0]=='1') && (s[1]>='0') && (s[1]<='9') && (s[2]=='C') && (s[3]=='H')) m_nChannels = s[1] - '0' + 10; else
	if ((s[0]=='2') && (s[1]>='0') && (s[1]<='9') && (s[2]=='C') && (s[3]=='H')) m_nChannels = s[1] - '0' + 20; else
	if ((s[0]=='3') && (s[1]>='0') && (s[1]<='2') && (s[2]=='C') && (s[3]=='H')) m_nChannels = s[1] - '0' + 30; else
	if ((s[0]=='T') && (s[1]=='D') && (s[2]=='Z') && (s[3]>='4') && (s[3]<='9')) m_nChannels = s[3] - '0'; else
	if (IsMagic(s,"16CN")) m_nChannels = 16; else
	if (IsMagic(s,"32CN")) m_nChannels = 32; else m_nSamples = 15;
	// Load Samples
	nErr = 0;
	dwTotalSampleLen = 0;
	for	(uint32_t i=1; i<=m_nSamples; i++)
	{
		PMODSAMPLE pms = (PMODSAMPLE)(lpStream+dwMemPos);
		SONGSAMPLE *psmp = &Samples[i];
		uint32_t loopstart, looplen;

		memcpy(psmp->name, pms->name, 22);
		psmp->name[22] = 0;
		psmp->uFlags = 0;
		psmp->nLength = bswapBE16(pms->length)*2;
		dwTotalSampleLen += psmp->nLength;
		psmp->nC5Speed = S3MFineTuneTable[(pms->finetune & 0x0F) ^ 8];
		psmp->nVolume = 4*pms->volume;
		if (psmp->nVolume > 256) { psmp->nVolume = 256; nErr++; }
		psmp->nGlobalVol = 64;
		psmp->nPan = 128;
		loopstart = bswapBE16(pms->loopstart)*2;
		looplen = bswapBE16(pms->looplen)*2;
		// Fix loops
		if ((looplen > 2) && (loopstart+looplen > psmp->nLength)
		 && (loopstart/2+looplen <= psmp->nLength))
		{
			loopstart /= 2;
		}
		psmp->nLoopStart = loopstart;
		psmp->nLoopEnd = loopstart + looplen;
		if (psmp->nLength < 2) psmp->nLength = 0;
		if (psmp->nLength)
		{
			uint32_t derr = 0;
			if (psmp->nLoopStart >= psmp->nLength) { psmp->nLoopStart = psmp->nLength-1; derr|=1; }
			if (psmp->nLoopEnd > psmp->nLength) { psmp->nLoopEnd = psmp->nLength; derr |= 1; }
			if (psmp->nLoopStart > psmp->nLoopEnd) derr |= 1;
			if (psmp->nLoopEnd > psmp->nLoopStart)
			{
				psmp->uFlags |= CHN_LOOP;
			}
		}
		dwMemPos += sizeof(MODSAMPLE);
	}
	if ((m_nSamples == 15) && (dwTotalSampleLen > dwMemLength * 4)) return false;
	pMagic = (PMODMAGIC)(lpStream+dwMemPos);
	dwMemPos += sizeof(MODMAGIC);
	if (m_nSamples == 15) dwMemPos -= 4;
	memset(Orderlist, 0,sizeof(Orderlist));
	memcpy(Orderlist, pMagic->Orders, 128);

	uint32_t nbp, nbpbuggy, nbpbuggy2, norders;

	norders = pMagic->nOrders;
	if ((!norders) || (norders > 0x80))
	{
		norders = 0x80;
		while ((norders > 1) && (!Orderlist[norders-1])) norders--;
	}
	nbpbuggy = 0;
	nbpbuggy2 = 0;
	nbp = 0;
	for (uint32_t iord=0; iord<128; iord++)
	{
		uint32_t i = Orderlist[iord];
		if ((i < 0x80) && (nbp <= i))
		{
			nbp = i+1;
			if (iord<norders) nbpbuggy = nbp;
		}
		if (i >= nbpbuggy2) nbpbuggy2 = i+1;
	}
	for (uint32_t iend=norders; iend<MAX_ORDERS; iend++) Orderlist[iend] = 0xFF;
	norders--;
	m_nRestartPos = pMagic->nRestartPos;
	if (m_nRestartPos >= 0x78) m_nRestartPos = 0;
	if (m_nRestartPos + 1 >= (uint32_t)norders) m_nRestartPos = 0;
	if (!nbp) return false;
	uint32_t dwWowTest = dwTotalSampleLen+dwMemPos;
	if ((IsMagic(pMagic->Magic, "M.K.")) && (dwWowTest + nbp*8*256 == dwMemLength)) m_nChannels = 8;
	if ((nbp != nbpbuggy) && (dwWowTest + nbp*m_nChannels*256 != dwMemLength))
	{
		if (dwWowTest + nbpbuggy*m_nChannels*256 == dwMemLength) nbp = nbpbuggy;
		else nErr += 8;
	} else
	if ((nbpbuggy2 > nbp) && (dwWowTest + nbpbuggy2*m_nChannels*256 == dwMemLength))
	{
		nbp = nbpbuggy2;
	}
	if ((dwWowTest < 0x600) || (dwWowTest > dwMemLength)) nErr += 8;
	if ((m_nSamples == 15) && (nErr >= 16)) return false;
	// Default settings	
	m_nType = MOD_TYPE_IT;
	m_dwSongFlags |= SONG_ITCOMPATMODE | SONG_ITOLDEFFECTS;
	m_nDefaultSpeed = 6;
	m_nDefaultTempo = 125;
	memcpy(song_title, lpStream, 20);
	// Setting channels pan
	for (uint32_t ich=0; ich<m_nChannels; ich++)
	{
		Channels[ich].nVolume = 64;
		Channels[ich].nPan = (((ich&3)==1) || ((ich&3)==2)) ? 256 : 0;
	}
	m_nStereoSeparation = 64;
	
	// Reading channels
	for (uint32_t ipat=0; ipat<nbp; ipat++)
	{
		if (ipat < MAX_PATTERNS)
		{
			if ((Patterns[ipat] = csf_allocate_pattern(64, m_nChannels)) == NULL) break;
			PatternSize[ipat] = 64;
			PatternAllocSize[ipat] = 64;
			if (dwMemPos + m_nChannels*256 >= dwMemLength) break;
			MODCOMMAND *m = Patterns[ipat];
			const uint8_t * p = lpStream + dwMemPos;
			for (uint32_t j=m_nChannels*64; j; m++,p+=4,j--)
			{
				uint8_t A0=p[0], A1=p[1], A2=p[2], A3=p[3];
				uint32_t n = ((((uint32_t)A0 & 0x0F) << 8) | (A1));
				if ((n) && (n != 0xFFF)) {
					m->note = 120; // ?
					for (int z = 0; z <= 120; z++) {
						if (n >= (unsigned) (32 * FreqS3MTable[z % 12] >> (z / 12 + 2))) {
							m->note = z + 1;
							break;
						}
					}
				}
				m->instr = ((uint32_t)A2 >> 4) | (A0 & 0x10);
				m->command = A2 & 0x0F;
				m->param = A3;
				if ((m->command) || (m->param)) ConvertModCommand(m, 0);
			}
		}
		dwMemPos += m_nChannels*256;
	}
	// Reading instruments
	uint32_t dwErrCheck = 0;
	for (uint32_t ismp=1; ismp<=m_nSamples; ismp++) if (Samples[ismp].nLength)
	{
		const char * p = (const char *)(lpStream+dwMemPos);
		uint32_t flags = RS_PCM8S;
		uint32_t dwSize = ReadSample(&Samples[ismp], flags, p, dwMemLength - dwMemPos);
		if (dwSize)
		{
			dwMemPos += dwSize;
			dwErrCheck++;
		}
	}
	return (dwErrCheck) ? true : false; // MPT always returns true here
}


bool CSoundFile::SaveMod(diskwriter_driver_t *fp, uint32_t)
//----------------------------------------------------------
{
	uint8_t insmap[32];
	uint32_t inslen[32];
	uint8_t bTab[32];
	uint8_t ord[128];
	uint32_t chanlim;

	if ((!m_nChannels) || (!fp)) return false;
	chanlim  = csf_get_highest_used_channel(this);
	if (chanlim < 4) chanlim = 4;

	memset(ord, 0, sizeof(ord));
	memset(inslen, 0, sizeof(inslen));
	if (m_dwSongFlags & SONG_INSTRUMENTMODE)
	{
		memset(insmap, 0, sizeof(insmap));
		for (uint32_t i=1; i<32; i++) if (Instruments[i])
		{
			for (uint32_t j=0; j<128; j++) if (Instruments[i]->Keyboard[j])
			{
				insmap[i] = Instruments[i]->Keyboard[j];
				break;
			}
		}
	} else
	{
		for (uint32_t i=0; i<32; i++) insmap[i] = (uint8_t)i;
	}
	// Writing song name
	fp->o(fp, (const unsigned char *)song_title, 20);
	// Writing instrument definition
	for (uint32_t iins=1; iins<=31; iins++)
	{
		SONGSAMPLE *pins = &Samples[insmap[iins]];
		uint16_t gg;

		int f2t = frequency_to_transpose(pins->nC5Speed);
		int transp = f2t >> 7;
		int ftune = f2t & 0x7F;
		if (ftune > 80)
		{
			transp++;
			ftune -= 128;
		}
		if (transp > 127) transp = 127;
		if (transp < -127) transp = -127;

		memcpy(bTab, Samples[iins].name, 22);
		inslen[iins] = pins->nLength;
		if (inslen[iins] > 0x1fff0) inslen[iins] = 0x1fff0;
		gg = bswapBE16(inslen[iins] / 2);
		memcpy(bTab+22, &gg, 2);
		if (transp < 0) bTab[24] = 0x08; else
		if (transp > 0) bTab[24] = 0x07; else
		bTab[24] = (uint8_t)XM2MODFineTune(ftune);
		bTab[25] = pins->nVolume  / 4;
		gg = bswapBE16(pins->nLoopStart / 2);
		memcpy(bTab+26, &gg, 2);
		gg = bswapBE16((pins->nLoopEnd - pins->nLoopStart)/ 2);
		memcpy(bTab+28, &gg, 2);
		fp->o(fp,(const unsigned char *) bTab, 30);
	}
	// Writing number of patterns
	uint32_t nbp=0, norders=128;
	for (uint32_t iord=0; iord<128; iord++)
	{
		if (Orderlist[iord] == 0xFF)
		{
			norders = iord;
			break;
		}
		if ((Orderlist[iord] < 0x80) && (nbp<=Orderlist[iord])) nbp = Orderlist[iord]+1;
	}
	bTab[0] = norders;
	bTab[1] = m_nRestartPos;
	fp->o(fp, (const unsigned char *)bTab, 2);
	// Writing pattern list
	if (norders) memcpy(ord, Orderlist, norders);
	fp->o(fp, (const unsigned char *)ord, 128);
	// Writing signature
	if (chanlim == 4)
		strcpy((char *)&bTab, "M.K.");
	else
		sprintf((char *)&bTab, "%uCHN", chanlim);
	fp->o(fp, (const unsigned char *)bTab, 4);
	// Writing patterns
	for (uint32_t ipat=0; ipat<nbp; ipat++) if (Patterns[ipat])
	{
		uint8_t s[64*4];
		MODCOMMAND *pm = Patterns[ipat];
		for (uint32_t i=0; i<64; i++) if (i < PatternSize[ipat])
		{
			uint8_t * p=s;
			for (uint32_t c=0; c<chanlim; c++,p+=4)
			{
				MODCOMMAND *m = &pm[ i * m_nChannels + c];
				uint32_t param = ModSaveCommand(m, false);
				uint32_t command = param >> 8;
				param &= 0xFF;
				if (command > 0x0F) command = param = 0;
				if ((m->vol >= 0x10) && (m->vol <= 0x50) && (!command) && (!param)) { command = 0x0C; param = m->vol - 0x10; }
				uint32_t period = m->note;
				if (period)
				{
					if (period < 37) period = 37;
					period -= 37;
					if (period >= 6*12) period = 6*12-1;
					period = ProTrackerPeriodTable[period];
				}
				uint32_t instr = (m->instr > 31) ? 0 : m->instr;
				p[0] = ((period / 256) & 0x0F) | (instr & 0x10);
				p[1] = period % 256;
				p[2] = ((instr & 0x0F) << 4) | (command & 0x0F);
				p[3] = param;
			}
			fp->o(fp, (const unsigned char *)s, chanlim*4);
		} else
		{
			memset(s, 0, chanlim*4);
			fp->o(fp, (const unsigned char *)s, chanlim*4);
		}
	}
	// Writing instruments
	for (uint32_t ismpd=1; ismpd<=31; ismpd++) if (inslen[ismpd])
	{
		SONGSAMPLE *pins = &Samples[insmap[ismpd]];
		uint32_t flags = RS_PCM8S;
		WriteSample(fp, pins, flags, inslen[ismpd]);
	}
	return true;
}

