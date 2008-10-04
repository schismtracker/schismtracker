/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

#include "headers.h"

#include "stdafx.h"
#include "sndfile.h"
#include "midi.h"

//#pragma warning(disable:4244)

extern WORD S3MFineTuneTable[16];

//////////////////////////////////////////////////////
// ScreamTracker S3M file support

typedef struct tagS3MSAMPLESTRUCT
{
	BYTE type;
	CHAR dosname[12];
	BYTE hmem;
	WORD memseg;
	DWORD length;
	DWORD loopbegin;
	DWORD loopend;
	BYTE vol;
	BYTE bReserved;
	BYTE pack;
	BYTE flags;
	DWORD finetune;
	DWORD dwReserved;
	WORD intgp;
	WORD int512;
	DWORD lastused;
	CHAR name[28];
	CHAR scrs[4];
} S3MSAMPLESTRUCT;


typedef struct tagS3MFILEHEADER
{
	CHAR name[28];
	BYTE b1A;
	BYTE type;
	WORD reserved1;
	WORD ordnum;
	WORD insnum;
	WORD patnum;
	WORD flags;
	WORD cwtv;
	WORD version;
	DWORD scrm;	// "SCRM" = 0x4D524353
	BYTE globalvol;
	BYTE speed;
	BYTE tempo;
	BYTE mastervol;
	BYTE ultraclicks;
	BYTE panning_present;
	BYTE reserved2[8];
	WORD special;
	BYTE channels[32];
} S3MFILEHEADER;


void CSoundFile::S3MConvert(MODCOMMAND *m, BOOL bIT) const
//--------------------------------------------------------
{
	UINT command = m->command;
	UINT param = m->param;
	switch (command + 0x40)
	{
	case 'A':	command = CMD_SPEED; break;
	case 'B':	command = CMD_POSITIONJUMP; break;
	case 'C':	command = CMD_PATTERNBREAK; if (!bIT) param = (param >> 4) * 10 + (param & 0x0F); break;
	case 'D':	command = CMD_VOLUMESLIDE; break;
	case 'E':	command = CMD_PORTAMENTODOWN; break;
	case 'F':	command = CMD_PORTAMENTOUP; break;
	case 'G':	command = CMD_TONEPORTAMENTO; break;
	case 'H':	command = CMD_VIBRATO; break;
	case 'I':	command = CMD_TREMOR; break;
	case 'J':	command = CMD_ARPEGGIO; break;
	case 'K':	command = CMD_VIBRATOVOL; break;
	case 'L':	command = CMD_TONEPORTAVOL; break;
	case 'M':	command = CMD_CHANNELVOLUME; break;
	case 'N':	command = CMD_CHANNELVOLSLIDE; break;
	case 'O':	command = CMD_OFFSET; break;
	case 'P':	command = CMD_PANNINGSLIDE; break;
	case 'Q':	command = CMD_RETRIG; break;
	case 'R':	command = CMD_TREMOLO; break;
	case 'S':	command = CMD_S3MCMDEX; break;
	case 'T':	command = CMD_TEMPO; break;
	case 'U':	command = CMD_FINEVIBRATO; break;
	case 'V':	command = CMD_GLOBALVOLUME; if (!bIT) param *= 2; break;
	case 'W':	command = CMD_GLOBALVOLSLIDE; break;
	case 'X':	command = CMD_PANNING8; break;
	case 'Y':	command = CMD_PANBRELLO; break;
	case 'Z':	command = CMD_MIDI; break;
	default:	command = 0;
	}
	m->command = command;
	m->param = param;
}


void CSoundFile::S3MSaveConvert(UINT *pcmd, UINT *pprm, BOOL bIT) const
//---------------------------------------------------------------------
{
	UINT command = *pcmd;
	UINT param = *pprm;
	switch(command)
	{
	case CMD_SPEED:				command = 'A'; break;
	case CMD_POSITIONJUMP:		command = 'B'; break;
	case CMD_PATTERNBREAK:		command = 'C'; if (!bIT) param = ((param / 10) << 4) + (param % 10); break;
	case CMD_VOLUMESLIDE:		command = 'D'; break;
	case CMD_PORTAMENTODOWN:	command = 'E'; if ((param >= 0xE0) && (m_nType & (MOD_TYPE_MOD|MOD_TYPE_XM))) param = 0xDF; break;
	case CMD_PORTAMENTOUP:		command = 'F'; if ((param >= 0xE0) && (m_nType & (MOD_TYPE_MOD|MOD_TYPE_XM))) param = 0xDF; break;
	case CMD_TONEPORTAMENTO:	command = 'G'; break;
	case CMD_VIBRATO:			command = 'H'; break;
	case CMD_TREMOR:			command = 'I'; break;
	case CMD_ARPEGGIO:			command = 'J'; break;
	case CMD_VIBRATOVOL:		command = 'K'; break;
	case CMD_TONEPORTAVOL:		command = 'L'; break;
	case CMD_CHANNELVOLUME:		command = 'M'; break;
	case CMD_CHANNELVOLSLIDE:	command = 'N'; break;
	case CMD_OFFSET:			command = 'O'; break;
	case CMD_PANNINGSLIDE:		command = 'P'; break;
	case CMD_RETRIG:			command = 'Q'; break;
	case CMD_TREMOLO:			command = 'R'; break;
	case CMD_S3MCMDEX:			command = 'S'; break;
	case CMD_TEMPO:				command = 'T'; break;
	case CMD_FINEVIBRATO:		command = 'U'; break;
	case CMD_GLOBALVOLUME:		command = 'V'; if (!bIT) param >>= 1;break;
	case CMD_GLOBALVOLSLIDE:	command = 'W'; break;
	case CMD_PANNING8:			
		command = 'X';
		if ((bIT) && (m_nType != MOD_TYPE_IT) && (m_nType != MOD_TYPE_XM))
		{
			if (param == 0xA4) { command = 'S'; param = 0x91; }	else
			if (param <= 0x80) { param <<= 1; if (param > 255) param = 255; } else
			command = param = 0;
		} else
		if ((!bIT) && ((m_nType == MOD_TYPE_IT) || (m_nType == MOD_TYPE_XM)))
		{
			param >>= 1;
		}
		break;
	case CMD_PANBRELLO:			command = 'Y'; break;
	case CMD_MIDI:				command = 'Z'; break;
	case CMD_XFINEPORTAUPDOWN:
		if (param & 0x0F) switch(param & 0xF0)
		{
		case 0x10:	command = 'F'; param = (param & 0x0F) | 0xE0; break;
		case 0x20:	command = 'E'; param = (param & 0x0F) | 0xE0; break;
		case 0x90:	command = 'S'; break;
		default:	command = param = 0;
		} else command = param = 0;
		break;
	case CMD_MODCMDEX:
		command = 'S';
		switch(param & 0xF0)
		{
		case 0x00:	command = param = 0; break;
		case 0x10:	command = 'F'; param |= 0xF0; break;
		case 0x20:	command = 'E'; param |= 0xF0; break;
		case 0x30:	param = (param & 0x0F) | 0x10; break;
		case 0x40:	param = (param & 0x0F) | 0x30; break;
		case 0x50:	param = (param & 0x0F) | 0x20; break;
		case 0x60:	param = (param & 0x0F) | 0xB0; break;
		case 0x70:	param = (param & 0x0F) | 0x40; break;
		case 0x90:	command = 'Q'; param &= 0x0F; break;
		case 0xA0:	if (param & 0x0F) { command = 'D'; param = (param << 4) | 0x0F; } else command=param=0; break;
		case 0xB0:	if (param & 0x0F) { command = 'D'; param |= 0xF0; } else command=param=0; break;
		}
		break;
	default:	command = param = 0;
	}
	command &= ~0x40;
	*pcmd = command;
	*pprm = param;
}

static bool MidiS3M_Read(INSTRUMENTHEADER& Header, int iSmp, char name[32], int& scale)
{
//    fprintf(stderr, "Name(%s)\n", name);

    if(name[0] == 'G') // GM=General MIDI
    {
        bool is_percussion = false;
        if(name[1] == 'M') {}
        else if(name[1] == 'P') is_percussion = true;
        else return false;
        char*s = name+2;
        int GM = 0;      // midi program
        int ft = 0;      // finetuning
        /**/scale = 63;  // automatic volume scaling
        int autoSDx = 0; // automatic randomized SDx effect
        int bank = 0;    // midi bank
        while(isdigit_safe(*s)) GM = GM*10 + (*s++)-'0';
        for(;;)
        {
            int sign=0;
            if(*s == '-') sign=1;
            if(sign || *s=='+')
            {
                for(ft=0; isdigit_safe(*++s); ft=ft*10+(*s-'0')) {}
                if(sign)ft=-ft;
                continue;
            }
            if(*s=='/')
            {
                for(scale=0; isdigit_safe(*++s); scale=scale*10+(*s-'0')) {}
                continue;
            }
            if(*s=='&')
            {
                for(autoSDx=0; isdigit_safe(*++s); autoSDx=autoSDx*10+(*s-'0')) {}
                if(autoSDx > 15) autoSDx &= 15;
                continue;
            }
            if(*s=='%')
            {
                for(bank=0; isdigit_safe(*++s); bank=bank*10+(*s-'0')) {}
                continue;
            }
            break;
        }
        // wMidiBank, nMidiProgram, nMidiChannel, nMidiDrumKey
        Header.wMidiBank = bank;
        if(is_percussion)
            { Header.nMidiDrumKey = GM;
              Header.nMidiChannelMask = 1 << 9;
              Header.nMidiProgram = 128+(GM); }
        else
            { Header.nMidiProgram = GM-1;
              Header.nMidiChannelMask = 0xFFFF &~ (1 << 9); // any channel except percussion
            }
        /* TODO: Apply autoSDx,
         * FIXME: Channel note changes don't affect MIDI notes
         */
        strncpy((char*)Header.name, (const char*)name, 32);

		for(unsigned a=0; a<128; ++a)
		{
			Header.Keyboard[a] = iSmp;
			Header.NoteMap[a] = a+1+ft;
		}
        return true;
    }
    return false;
}

BOOL CSoundFile::ReadS3M(const BYTE *lpStream, DWORD dwMemLength)
//---------------------------------------------------------------
{
	UINT insnum,patnum,nins,npat;
	DWORD insfile[128];
	WORD ptr[256];
	BYTE s[1024];
	DWORD dwMemPos;
	BYTE insflags[128], inspack[128];
	S3MFILEHEADER psfh = *(S3MFILEHEADER *)lpStream;

	psfh.reserved1 = bswapLE16(psfh.reserved1);
	psfh.ordnum = bswapLE16(psfh.ordnum);
	psfh.insnum = bswapLE16(psfh.insnum);
	psfh.patnum = bswapLE16(psfh.patnum);
	psfh.flags = bswapLE16(psfh.flags);
	psfh.cwtv = bswapLE16(psfh.cwtv);
	psfh.version = bswapLE16(psfh.version);
	psfh.scrm = bswapLE32(psfh.scrm);
	psfh.special = bswapLE16(psfh.special);

	if ((!lpStream) || (dwMemLength <= sizeof(S3MFILEHEADER)+sizeof(S3MSAMPLESTRUCT)+64)) return FALSE;
	if (psfh.scrm != 0x4D524353) return FALSE;
	dwMemPos = 0x60;
	m_nType = MOD_TYPE_S3M;
	memset(m_szNames,0,sizeof(m_szNames));
	memcpy(m_szNames[0], psfh.name, 28);
	// Speed
	m_nDefaultSpeed = psfh.speed;
	if (m_nDefaultSpeed < 1) m_nDefaultSpeed = 6;
	if (m_nDefaultSpeed > 0x1F) m_nDefaultSpeed = 0x1F;
	// Tempo
	m_nDefaultTempo = psfh.tempo;
	if (m_nDefaultTempo < 40) m_nDefaultTempo = 40;
	if (m_nDefaultTempo > 240) m_nDefaultTempo = 240;
	// Global Volume
	m_nDefaultGlobalVolume = psfh.globalvol << 2;
	if ((!m_nDefaultGlobalVolume) || (m_nDefaultGlobalVolume > 256)) m_nDefaultGlobalVolume = 256;
	m_nSongPreAmp = psfh.mastervol & 0x7F;
	// Channels
	m_nChannels = 4;
	for (UINT ich=0; ich<32; ich++)
	{
		ChnSettings[ich].nPan = 128;
		ChnSettings[ich].nVolume = 64;

		ChnSettings[ich].dwFlags = CHN_MUTE;
		if (psfh.channels[ich] != 0xFF)
		{
			m_nChannels = ich+1;
			UINT b = psfh.channels[ich] & 0x0F;
			ChnSettings[ich].nPan = (b & 8) ? 0xC0 : 0x40;
			ChnSettings[ich].dwFlags = 0;
		}
	}
	if (m_nChannels < 4) m_nChannels = 4;
	if ((psfh.cwtv < 0x1320) || (psfh.flags & 0x40)) m_dwSongFlags |= SONG_FASTVOLSLIDES;
	// Reading pattern order
	UINT iord = psfh.ordnum;
	if (iord<1) iord = 1;
	if (iord > MAX_ORDERS) iord = MAX_ORDERS;
	if (iord)
	{
		memcpy(Order, lpStream+dwMemPos, iord);
		dwMemPos += iord;
	}
	if ((iord & 1) && (lpStream[dwMemPos] == 0xFF)) dwMemPos++;
	// Reading file pointers
	insnum = nins = psfh.insnum;
	if (insnum >= MAX_SAMPLES) insnum = MAX_SAMPLES-1;
	m_nSamples = insnum;
	patnum = npat = psfh.patnum;
	if (patnum > MAX_PATTERNS) patnum = MAX_PATTERNS;
	memset(ptr, 0, sizeof(ptr));
	if (nins+npat)
	{
		memcpy(ptr, lpStream+dwMemPos, 2*(nins+npat));
		dwMemPos += 2*(nins+npat);
		for (UINT j = 0; j < (nins+npat); ++j) {
		        ptr[j] = bswapLE16(ptr[j]);
		}
		if (psfh.panning_present == 252)
		{
			const BYTE *chnpan = lpStream+dwMemPos;
			for (UINT i=0; i<32; i++) if (chnpan[i] & 0x20)
			{
				ChnSettings[i].nPan = ((chnpan[i] & 0x0F) << 4) + 8;
			}
		}
	}
	if (!m_nChannels) return TRUE;
	// Reading instrument headers
	memset(insfile, 0, sizeof(insfile));
	
	bool has_adlib_samples = false;
	
	for (UINT iSmp=1; iSmp<=insnum; iSmp++)
	{
		UINT nInd = ((DWORD)ptr[iSmp-1])*16;
		if ((!nInd) || (nInd + 0x50 > dwMemLength)) continue;
		memcpy(s, lpStream+nInd, 0x50);
		memcpy(Ins[iSmp].name, s+1, 12);
		insflags[iSmp-1] = s[0x1F];
		inspack[iSmp-1] = s[0x1E];
		s[0x4C] = 0;
		lstrcpy(m_szNames[iSmp], (LPCSTR)&s[0x30]);
		
		if ((s[0]==1) && (s[0x4E]=='R') && (s[0x4F]=='S'))
		{
			UINT j = bswapLE32(*((LPDWORD)(s+0x10)));
			if (j > MAX_SAMPLE_LENGTH) j = MAX_SAMPLE_LENGTH;
			if (j < 2) j = 0;
			Ins[iSmp].nLength = j;
			j = bswapLE32(*((LPDWORD)(s+0x14)));
			if (j >= Ins[iSmp].nLength) j = Ins[iSmp].nLength - 1;
			Ins[iSmp].nLoopStart = j;
			j = bswapLE32(*((LPDWORD)(s+0x18)));
			if (j > MAX_SAMPLE_LENGTH) j = MAX_SAMPLE_LENGTH;
			if (j < 2) j = 0;
			if (j > Ins[iSmp].nLength) j = Ins[iSmp].nLength;
			Ins[iSmp].nLoopEnd = j;
			j = s[0x1C];
			if (j > 64) j = 64;
			Ins[iSmp].nVolume = j << 2;
			Ins[iSmp].nGlobalVol = 64;
			if (s[0x1F]&1) Ins[iSmp].uFlags |= CHN_LOOP;
			j = bswapLE32(*((LPDWORD)(s+0x20)));
			if (!j) j = 8363;
			if (j < 1024) j = 1024;
			Ins[iSmp].nC4Speed = j;
			insfile[iSmp] = ((DWORD)bswapLE16(*((LPWORD)(s+0x0E)))) << 4;
			insfile[iSmp] += ((DWORD)(BYTE)s[0x0D]) << 20;
			if (insfile[iSmp] > dwMemLength) insfile[iSmp] &= 0xFFFF;
			if ((Ins[iSmp].nLoopStart >= Ins[iSmp].nLoopEnd) || (Ins[iSmp].nLoopEnd - Ins[iSmp].nLoopStart < 8))
				Ins[iSmp].nLoopStart = Ins[iSmp].nLoopEnd = 0;
			Ins[iSmp].nPan = 0x80;
		}
		/* TODO: Add support for the following configurations:
		 *
		 * s[0] == 3: adlib bd     (4C..4F = "SCRI") \
		 * s[0] == 4: adlib snare  (4C..4F = "SCRI")  \
		 * s[0] == 5: adlib tom    (4C..4F = "SCRI")   > incredibly rarely used, though!
		 * s[0] == 6: adlib cymbal (4C..4F = "SCRI")  /  -Bisqwit
		 * s[0] == 7: adlib hihat  (4C..4F = "SCRI") /
		 */
		if ((s[0]==2) && (s[0x4E]=='R') && (s[0x4F]=='I' || s[0x4F]=='S'))
		{
            memcpy(Ins[iSmp].AdlibBytes, s+0x10, 11);

			int j = s[0x1C];
			if (j > 64) j = 64;
			Ins[iSmp].nVolume = j << 2;
			Ins[iSmp].nGlobalVol = 64;
			j = bswapLE32(*((LPDWORD)(s+0x20)));
			if (!j) j = 8363;
			Ins[iSmp].nC4Speed = j;
			Ins[iSmp].nPan = 0x80;
			Ins[iSmp].uFlags |= CHN_ADLIB;
			Ins[iSmp].uFlags &= ~(CHN_LOOP | CHN_16BIT);
			Ins[iSmp].nLength = 1;
			// Because most of the code in modplug requires
			// the presence of pSample when nLength is given,
			// we must have an at least 1-byte sample to make
			// it work. The actual contents of the sample don't
			// matter, since it will never be digitized. -Bisqwit
			Ins[iSmp].pSample = AllocateSample(1);
			has_adlib_samples = true;
		}
		
		Headers[iSmp] = new INSTRUMENTHEADER;
		memset(Headers[iSmp], 0, sizeof(INSTRUMENTHEADER));
		
		Headers[iSmp]->nNNA = NNA_NOTEOFF;
		Headers[iSmp]->nDNA = DNA_NOTEOFF;
		Headers[iSmp]->nDCT = DCT_INSTRUMENT;
		Headers[iSmp]->dwFlags = Ins[iSmp].uFlags;
		int scale;
		if(MidiS3M_Read(*Headers[iSmp], iSmp, m_szNames[iSmp], scale))
		{
		    m_dwSongFlags |= SONG_INSTRUMENTMODE;
			Headers[iSmp]->nGlobalVol = scale*128/63;
		}
		else
		{
		    delete Headers[iSmp];
		    Headers[iSmp] = 0;
		}
		
		//fprintf(stderr, "loaded uflags = %X, length = %d\n", Ins[iSmp].uFlags, Ins[iSmp].nLength);
	}
	
	bool has_noteoff_commands = false;
	
	// Reading patterns
	for (UINT iPat=0; iPat<patnum; iPat++)
	{
                UINT nInd = ((DWORD)ptr[nins+iPat]) << 4;
                // if the parapointer is zero, the pattern is blank (so ignore it)
                if (nInd == 0)
                        continue;
                if (nInd + 0x40 > dwMemLength) continue;
		WORD len = bswapLE16(*((WORD *)(lpStream+nInd)));
		nInd += 2;
		
		LPBYTE src = (LPBYTE)(lpStream+nInd);
		
		if(len < 2 || (nInd + (len-2) > dwMemLength)) continue;
		len -= 2;
		
		/* Figure out how many rows this pattern has (normal is 64, but we never know) */
		PatternSize[iPat] = 0;
		for(UINT j=0; j<len; )
		{
			BYTE b = src[j++];
			if(!b)
				++PatternSize[iPat];
			else
			{
				if(b&0x20) j+=2;
				if(b&0x40) j+=1;
				if(b&0x80) j+=2;
			}
		}
		
		PatternAllocSize[iPat] = PatternSize[iPat];
		
		if(PatternSize[iPat] != 64)
			fprintf(stderr, "Warning: Pattern %u has %u rows\n", iPat, PatternSize[iPat]);
		
		if ((Patterns[iPat] = AllocatePattern(PatternSize[iPat], m_nChannels)) == NULL)
		{
			continue;
		}
		
		// Unpacking pattern
		MODCOMMAND *p = Patterns[iPat];
		for (UINT row=0, j=0; j < len; )
		{
			BYTE b = src[j++];
			if (!b)
			{
				if (++row >= PatternSize[iPat]) break;
			} else
			{
				UINT chn = b & 0x1F;
				if (chn < m_nChannels)
				{
					MODCOMMAND *m = &p[row*m_nChannels+chn];
					if (b & 0x20)
					{
						m->note = src[j++];
						if (m->note < 0xF0) m->note = (m->note & 0x0F) + 12*(m->note >> 4) + 13;
						else if (m->note == 0xFF) m->note = 0;
						else if (m->note == 0xFE)
						{
							// S3M's ^^^.
							// When used with Adlib samples, works like NOTE_OFF.
							// When used with digital samples, works like NOTE_CUT.
							/* From S3M official documentation:
							  byte 0 - Note; hi=oct, lo=note, 255=empty note,
							           254=key off (used with adlib, with samples stops smp)
							*/
							has_noteoff_commands = true;
							m->note = 0xFE; // assume NOTE_CUT
						}
						m->instr = src[j++];
					}
					if (b & 0x40)
					{
						UINT vol = src[j++];
						if ((vol >= 128) && (vol <= 192))
						{
							vol -= 128;
							m->volcmd = VOLCMD_PANNING;
						} else
						{
							if (vol > 64) vol = 64;
							m->volcmd = VOLCMD_VOLUME;
						}
						m->vol = vol;
					}
					if (b & 0x80)
					{
						m->command = src[j++];
						m->param = src[j++];
						if (m->command) S3MConvert(m, FALSE);
					}
				} else
				{
					if (b & 0x20) j += 2;
					if (b & 0x40) j += 1;
					if (b & 0x80) j += 2;
				}
			}
		}
	}
	
	if(has_adlib_samples && has_noteoff_commands)
	{
		/* If the song contains 254-bytes, and it contained adlib samples,
		 * parse through the song once to see when the 254 is applied to
		 * adlib samples. Those 254s should be changed into ===s.
		 * They were produced as ^^^ earlier.
		 */
		unsigned LastInstrNo[32] = {0};
		
		/* First, check all patterns, so we won't miss patterns & rows
		 * that aren't part of the song */
		for(unsigned pat=0; pat<patnum; ++pat)
		{
			if(!Patterns[pat]) continue;
			for(unsigned row=0; row<PatternSize[pat]; ++row)
				for(unsigned chn=0; chn<m_nChannels && chn<32; ++chn)
				{
					MODCOMMAND& m = Patterns[pat][row*m_nChannels+chn];
					if(m.instr) LastInstrNo[chn] = m.instr;
					if(m.note == 0xFE || m.note == 0xFF) // NOTE_CUT or NOTE_OFF
					{
						unsigned ins = LastInstrNo[chn];
						if(ins >= 1 && ins <= insnum
						&& (Ins[ins].uFlags & CHN_ADLIB))
						{
							m.note = 0xFF; // Change into NOTE_OFF
						}
						else
							m.note = 0xFE; // Change into NOTE_CUT
					}
				}
		}
		
		/* Then, run through orders to get the correct ordering for the rows */
		unsigned ord=0, row=0;
		bool* visited = new bool[MAX_ORDERS*256];
		for(;;)
		{
			unsigned pat = Order[ord];
			if(pat == 0xFE) { ++ord; continue; }
			if(pat == 0xFF) break;
			if(pat >= patnum || !Patterns[pat] || row >= PatternSize[pat])
			    { row=0; ++ord; continue; }

			if(visited[ord*256+row]) break;
			visited[ord*256+row]=true;

			int nextrow=row+1, nextord=ord, patbreak=0;
			for(unsigned chn=0; chn<m_nChannels && chn<32; ++chn)
			{
				MODCOMMAND& m = Patterns[pat][row*m_nChannels+chn];
				if(m.instr) LastInstrNo[chn] = m.instr;
				if(m.command == CMD_PATTERNBREAK) { nextrow=m.param; if(!patbreak) patbreak=1; }
				if(m.command == CMD_POSITIONJUMP) { nextord=m.param; if(!patbreak) nextrow=0; patbreak=2; }
				if(m.note == 0xFE || m.note == 0xFF) // NOTE_CUT or NOTE_OFF
				{
					unsigned ins = LastInstrNo[chn];
					if(ins >= 1 && ins <= insnum
					&& (Ins[ins].uFlags & CHN_ADLIB))
					{
						m.note = 0xFF; // Change into NOTE_OFF
					}
					else
						m.note = 0xFE; // Change into NOTE_CUT
				}
			}
			if(patbreak==1) nextord=ord+1;
			ord=nextord; row=nextrow;
		}
		delete[] visited;
		/* Note: This can still fail, if you have the following setup:
		 *
		 * Pattern 0:  C-5 01 C00 (01 is a AME)
		 * Pattern 1:  C-5 02 C00 (02 is a SMP)
		 * Pattern 2:  254 .. C00
		 *
		 * Orders: 00 02 01 02 ...
		 *
		 * In this setup, 254 should mean === the first time around,
		 * and ^^^ the second time around. Our parser puts ^^^ here,
		 * because SMP was the last one encountered. However, this
		 * example is very contrived and unlikely to appear in any
		 * production S3M in the world. --Bisqwit
		 */
	}
		
	// Reading samples
	for (UINT iRaw=1; iRaw<=insnum; iRaw++) if ((Ins[iRaw].nLength) && (insfile[iRaw]))
	{
		UINT flags;
		if (insflags[iRaw-1] & 4)
			flags = (psfh.version == 1) ? RS_PCM16S : RS_PCM16U;
		else
			flags = (psfh.version == 1) ? RS_PCM8S : RS_PCM8U;
		if (insflags[iRaw-1] & 2) flags |= RSF_STEREO;
		if (inspack[iRaw-1] == 4) flags = RS_ADPCM4;
		dwMemPos = insfile[iRaw];
		dwMemPos += ReadSample(&Ins[iRaw], flags, (LPSTR)(lpStream + dwMemPos), dwMemLength - dwMemPos);
	}
	
	m_nMinPeriod = 64;
	m_nMaxPeriod = 32767;
	if (psfh.flags & 0x10) m_dwSongFlags |= SONG_AMIGALIMITS;
	return TRUE;
}


#ifndef MODPLUG_NO_FILESAVE

#ifdef MSC_VER
#pragma warning(disable:4100)
#endif

static BYTE S3MFiller[16] =
{
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
};


BOOL CSoundFile::SaveS3M(diskwriter_driver_t *fp, UINT nPacking)
//----------------------------------------------------------
{
	BYTE header[0x60];
	UINT nbo,nbi,nbp,i;
	UINT chanlim;
	WORD patptr[128];
	WORD insptr[128];
	BYTE buffer[5*1024];
	S3MSAMPLESTRUCT insex[128];

	if ((!m_nChannels) || (!fp)) return FALSE;
	// Writing S3M header
	memset(header, 0, sizeof(header));
	memset(insex, 0, sizeof(insex));
	memcpy(header, m_szNames[0], 0x1C);
	header[0x1B] = 0;
	header[0x1C] = 0x1A;
	header[0x1D] = 0x10;
        nbo = (GetNumPatterns());
        if (nbo == 0)
                nbo = 2;
        else if (nbo & 1)
                nbo++;
        header[0x20] = nbo & 0xFF;
	header[0x21] = nbo >> 8;
	
	nbi = m_nSamples+1;
	if (nbi > 99) nbi = 99;
	header[0x22] = nbi & 0xFF;
	header[0x23] = nbi >> 8;
	nbp = 0;
	for (i=0; Patterns[i]; i++) { nbp = i+1; if (nbp >= MAX_PATTERNS) break; }
	for (i=0; i<MAX_ORDERS; i++) if ((Order[i] < MAX_PATTERNS) && (Order[i] >= nbp)) nbp = Order[i] + 1;
	header[0x24] = nbp & 0xFF;
	header[0x25] = nbp >> 8;
	if (m_dwSongFlags & SONG_FASTVOLSLIDES) header[0x26] |= 0x40;
	if ((m_nMaxPeriod < 20000) || (m_dwSongFlags & SONG_AMIGALIMITS)) header[0x26] |= 0x10;
	header[0x28] = 0x20; // ST3.20 = 0x1320
	header[0x29] = 0x13;
	header[0x2A] = 0x02; // Version = 1 => Signed samples
	header[0x2B] = 0x00;
	header[0x2C] = 'S';
	header[0x2D] = 'C';
	header[0x2E] = 'R';
	header[0x2F] = 'M';
	header[0x30] = m_nDefaultGlobalVolume >> 2;
	header[0x31] = m_nDefaultSpeed;
	header[0x32] = m_nDefaultTempo;
	header[0x33] = ((m_nSongPreAmp < 0x20) ? 0x20 : m_nSongPreAmp) | 0x80;	// Stereo
	header[0x35] = 0xFC;

	chanlim = GetHighestUsedChannel()+1;
	if (chanlim < 4) chanlim = 4;
	if (chanlim > 32) chanlim = 32;

	for (i=0; i<32; i++)
	{
		if (i < chanlim)
		{
			UINT tmp = (i & 0x0F) >> 1;
			header[0x40+i] = (i & 0x10) | ((i & 1) ? 8+tmp : tmp);
		} else header[0x40+i] = 0xFF;
	}
	fp->o(fp, (const unsigned char *)header, 0x60);
	fp->o(fp, (const unsigned char *)Order, nbo);
	memset(patptr, 0, sizeof(patptr));
	memset(insptr, 0, sizeof(insptr));
	UINT ofs0 = 0x60 + nbo;
	UINT ofs1 = ((0x60 + nbo + nbi*2 + nbp*2 + 15) & 0xFFF0);
	UINT ofs = ofs1;
	if (header[0x35] == 0xFC) {
		ofs += 0x20;
		ofs1 += 0x20;
	}

	for (i=0; i<nbi; i++) insptr[i] = bswapLE16((WORD)((ofs + i*0x50) / 16));
	for (i=0; i<nbp; i++) patptr[i] = bswapLE16((WORD)((ofs + nbi*0x50) / 16));
	fp->o(fp, (const unsigned char *)insptr, nbi*2);
	fp->o(fp, (const unsigned char *)patptr, nbp*2);
	if (header[0x35] == 0xFC)
	{
		BYTE chnpan[32];
		for (i=0; i<32; i++)
		{
			UINT nPan = ((ChnSettings[i].nPan+7) < 0xF0) ? ChnSettings[i].nPan+7 : 0xF0;
			chnpan[i] = (i<chanlim) ? 0x20 | (nPan >> 4) : 0x08;
		}
		fp->o(fp, (const unsigned char *)chnpan, 0x20);
	}
	if ((nbi*2+nbp*2) & 0x0F)
	{
		fp->o(fp, (const unsigned char *)S3MFiller, 0x10 - ((nbi*2+nbp*2) & 0x0F));
	}
	fp->l(fp, ofs1);
	ofs1 = fp->pos;
	fp->o(fp, (const unsigned char *)insex, nbi*0x50);
	// Packing patterns
	ofs += nbi*0x50;
	fp->l(fp,ofs);
	for (i=0; i<nbp; i++)
	{
		WORD len = 64;
		memset(buffer, 0, sizeof(buffer));
		patptr[i] = bswapLE16(ofs / 16);
		if (Patterns[i])
		{
			len = 2;
			MODCOMMAND *p = Patterns[i];
			if(PatternSize[i] != 64)
				fprintf(stderr, "Warning: Pattern %u has %u rows\n", i, PatternSize[i]);
			
			for (int row=0; row<PatternSize[i]; row++)
			{
				for (UINT j=0; j < 32 && j<chanlim; j++)
				{
					UINT b = j;
					MODCOMMAND *m = &p[row*m_nChannels+j];
					UINT note = m->note;
					UINT volcmd = m->volcmd;
					UINT vol = m->vol;
					UINT command = m->command;
					UINT param = m->param;
					UINT inst = m->instr;

					if (m_dwSongFlags & SONG_INSTRUMENTMODE
					&& note && inst) {
						UINT nn = Headers[inst]->Keyboard[note];
						UINT nm = Headers[inst]->NoteMap[note];
						/* translate on save */
						note = nm;
						inst = nn;
					}


					if ((note) || (inst)) b |= 0x20;
					switch(note)
					{
					    case 0: // no note
					        note = 0xFF; break;
					    case 0xFF: // NOTE_OFF ('===')
					    case 0xFE: // NOTE_CUT ('^^^')
					    case 0xFD: // NOTE_FADE ('~~~)
					    {
					        note = 0xFE; // Create ^^^
					        // From S3M official documentation:
					        // 254=key off (used with adlib, with samples stops smp)
					        //
					        // In fact, with AdLib S3M, notecut does not even exist.
					        // The "SCx" opcode is a complete NOP in adlib.
					        // There are only two ways to cut a note:
					        // set volume to 0, or use keyoff.
					        // With digital S3M, notecut is accomplished by ^^^.
					        // So is notefade (except it doesn't fade),
					        // and noteoff (except there are no volume
					        // envelopes, so it cuts immediately).
					        break;
					    }
					    case 1: case 2: case 3: case 4: case 5: case 6:
					    case 7: case 8: case 9: case 10:case 11:case 12:
					        note = 0; break; // too low
					    default:
					        note -= 13;
					        // Convert into S3M format
					        note = (note % 12) + ((note / 12) << 4);
					        break;
					}
					if (command == CMD_VOLUME)
					{
						command = 0;
						if (param > 64) param = 64;
						volcmd = VOLCMD_VOLUME;
						vol = param;
					}
					if (volcmd == VOLCMD_VOLUME) b |= 0x40; else
					if (volcmd == VOLCMD_PANNING) { vol |= 0x80; b |= 0x40; }
					if (command)
					{
						S3MSaveConvert(&command, &param, FALSE);
						if (command) b |= 0x80;
					}
					if (b & 0xE0)
					{
						buffer[len++] = b;
						if (b & 0x20)
						{
							buffer[len++] = note;
							buffer[len++] = inst;
						}
						if (b & 0x40)
						{
							buffer[len++] = vol;
						}
						if (b & 0x80)
						{
							buffer[len++] = command;
							buffer[len++] = param;
						}
						if (len > sizeof(buffer) - 20) break;
					}
				}
				buffer[len++] = 0;
				if (len > sizeof(buffer) - 20) break;
			}
		}
		buffer[0] = (len - 2) & 0xFF;
		buffer[1] = (len - 2) >> 8;
		len = (len+15) & (~0x0F);
		
		fp->o(fp, (const unsigned char *)buffer, len);
		ofs += len;
	}
	// Writing samples
	for (i=1; i<=nbi; i++)
	{
		MODINSTRUMENT *pins = &Ins[i];
		memcpy(insex[i-1].dosname, pins->name, 12);
		memcpy(insex[i-1].name, m_szNames[i], 28);
		memcpy(insex[i-1].scrs, "SCRS", 4);
		insex[i-1].hmem = (BYTE)((DWORD)ofs >> 20);
		insex[i-1].memseg = bswapLE16((WORD)((DWORD)ofs >> 4));

		insex[i-1].vol = pins->nVolume / 4;
		if (pins->nC4Speed)
			insex[i-1].finetune = bswapLE32(pins->nC4Speed);
		else
			insex[i-1].finetune = bswapLE32(TransposeToFrequency(pins->RelativeTone, pins->nFineTune));

		if (pins->uFlags & CHN_ADLIB)
		{
		    insex[i-1].type = 2;
		    memcpy(&insex[i-1].length, pins->AdlibBytes, 12);
		    // AdlibBytes occupies length, loopbegin and loopend
		}
		else if (pins->pSample)
		{
			insex[i-1].type = 1;
			insex[i-1].length = bswapLE32(pins->nLength);
			insex[i-1].loopbegin = bswapLE32(pins->nLoopStart);
			insex[i-1].loopend = bswapLE32(pins->nLoopEnd);
			insex[i-1].flags = (pins->uFlags & CHN_LOOP) ? 1 : 0;
			UINT flags = RS_PCM8U;
#ifndef NO_PACKING
			if (nPacking)
			{
			    /* NOTE: Packed samples are not supported by ST32 */
				if ((!(pins->uFlags & (CHN_16BIT|CHN_STEREO)))
				 && (CanPackSample((char *)pins->pSample, pins->nLength, nPacking)))
				{
					insex[i-1].pack = 4;
					flags = RS_ADPCM4;
				}
			} else
#endif // NO_PACKING
			{
				if (pins->uFlags & CHN_16BIT)
				{
					insex[i-1].flags |= 4;
					flags = RS_PCM16U;
				}
				if (pins->uFlags & CHN_STEREO)
				{
					insex[i-1].flags |= 2;
					flags = (pins->uFlags & CHN_16BIT) ? RS_STPCM16U : RS_STPCM8U;
				}
			}
			DWORD len = WriteSample(fp, pins, flags);
			if (len & 0x0F)
			{
				fp->o(fp, (const unsigned char *)S3MFiller, 0x10 - (len & 0x0F));
			}
			ofs += (len + 15) & (~0x0F);
		} else {
			insex[i-1].length = 0;
		}
	}
	// Updating parapointers
	fp->l(fp, ofs0);
	fp->o(fp, (const unsigned char *)insptr, nbi*2);
	fp->o(fp, (const unsigned char *)patptr, nbp*2);
	fp->l(fp, ofs1);
	fp->o(fp, (const unsigned char *)insex, 0x50*nbi);
	return TRUE;
}

#ifdef MSC_VER
#pragma warning(default:4100)
#endif

#endif // MODPLUG_NO_FILESAVE

