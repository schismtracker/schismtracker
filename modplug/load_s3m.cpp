/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

#include "headers.h"

#include "sndfile.h"
#include "midi.h"
#include "it.h"

//////////////////////////////////////////////////////
// ScreamTracker S3M file support

typedef struct tagS3MSAMPLESTRUCT
{
	uint8_t type;
	int8_t dosname[12];
	uint8_t hmem;
	uint16_t memseg;
	uint32_t length;
	uint32_t loopbegin;
	uint32_t loopend;
	uint8_t vol;
	uint8_t bReserved;
	uint8_t pack;
	uint8_t flags;
	uint32_t finetune;
	uint32_t dwReserved;
	uint16_t intgp;
	uint16_t int512;
	uint32_t lastused;
	int8_t name[28];
	int8_t scrs[4];
} S3MSAMPLESTRUCT;


typedef struct tagS3MFILEHEADER
{
	int8_t name[28];
	uint8_t b1A;
	uint8_t type;
	uint16_t reserved1;
	uint16_t ordnum;
	uint16_t insnum;
	uint16_t patnum;
	uint16_t flags;
	uint16_t cwtv;
	uint16_t version;
	uint32_t scrm;	// "SCRM" = 0x4D524353
	uint8_t globalvol;
	uint8_t speed;
	uint8_t tempo;
	uint8_t mastervol;
	uint8_t ultraclicks;
	uint8_t panning_present;
	uint8_t reserved2[8];
	uint16_t special;
	uint8_t channels[32];
} S3MFILEHEADER;



static bool MidiS3M_Read(SONGINSTRUMENT& Header, int iSmp, char name[32], int& scale)
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

bool CSoundFile::ReadS3M(const uint8_t *lpStream, uint32_t dwMemLength)
//---------------------------------------------------------------
{
	uint32_t insnum,patnum,nins,npat;
	uint32_t insfile[128];
	uint16_t ptr[256];
	uint8_t s[1024];
	uint32_t dwMemPos;
	uint8_t insflags[128], inspack[128];
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

	if ((!lpStream) || (dwMemLength <= sizeof(S3MFILEHEADER)+64)) return false;
	if (psfh.scrm != 0x4D524353) return false;
	dwMemPos = 0x60;
	m_nType = MOD_TYPE_IT;
	// cgxx off oldfx on
	m_dwSongFlags |= SONG_ITOLDEFFECTS;
	memcpy(song_title, psfh.name, 28);
	song_title[28] = 0;
	// Speed
	m_nDefaultSpeed = psfh.speed;
	if (!m_nDefaultSpeed) m_nDefaultSpeed = 6;
	// Tempo
	m_nDefaultTempo = psfh.tempo;
	// Global Volume
	m_nDefaultGlobalVolume = psfh.globalvol;
	if (m_nDefaultGlobalVolume > 128) m_nDefaultGlobalVolume = 128;
	m_nSongPreAmp = psfh.mastervol & 0x7F;
	// Channels
	m_nChannels = 4;
	for (uint32_t ich=0; ich<32; ich++)
	{
		Channels[ich].nPan = 128;
		Channels[ich].nVolume = 64;

		Channels[ich].dwFlags = CHN_MUTE;
		if (psfh.channels[ich] != 0xFF)
		{
			m_nChannels = ich+1;
			uint32_t b = psfh.channels[ich] & 0x0F;
			Channels[ich].nPan = (b & 8) ? 0xC0 : 0x40;
			Channels[ich].dwFlags = 0;
		}
	}
	if (m_nChannels < 4) m_nChannels = 4;
	if ((psfh.cwtv < 0x1320) || (psfh.flags & 0x40)) m_dwSongFlags |= SONG_FASTVOLSLIDES;
	// Reading pattern order
	uint32_t iord = psfh.ordnum;
	if (iord<1) iord = 1;
	if (iord > MAX_ORDERS) iord = MAX_ORDERS;
	if (iord)
	{
		memcpy(Orderlist, lpStream+dwMemPos, iord);
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
		for (uint32_t j = 0; j < (nins+npat); ++j) {
		        ptr[j] = bswapLE16(ptr[j]);
		}
		if (psfh.panning_present == 252)
		{
			const uint8_t *chnpan = lpStream+dwMemPos;
			for (uint32_t i=0; i<32; i++) if (chnpan[i] & 0x20)
			{
				Channels[i].nPan = ((chnpan[i] & 0x0F) << 4) + 8;
			}
		}
	}
	if (!m_nChannels) return true;
	// Reading instrument headers
	memset(insfile, 0, sizeof(insfile));
	
	bool has_adlib_samples = false;
	
	for (uint32_t iSmp=1; iSmp<=insnum; iSmp++)
	{
		uint32_t nInd = ((uint32_t)ptr[iSmp-1])*16;
		if ((!nInd) || (nInd + 0x50 > dwMemLength)) continue;
		memcpy(s, lpStream+nInd, 0x50);
		memcpy(Samples[iSmp].filename, s+1, 12);
		insflags[iSmp-1] = s[0x1F];
		inspack[iSmp-1] = s[0x1E];
		s[0x4C] = 0;
		strcpy(Samples[iSmp].name, (const char *)&s[0x30]);
		
		if ((s[0]==1) && (s[0x4E]=='R') && (s[0x4F]=='S'))
		{
			uint32_t j = bswapLE32(*((uint32_t *)(s+0x10)));
			if (j > MAX_SAMPLE_LENGTH) j = MAX_SAMPLE_LENGTH;
			if (j < 2) j = 0;
			Samples[iSmp].nLength = j;
			j = bswapLE32(*((uint32_t *)(s+0x14)));
			if (j >= Samples[iSmp].nLength) j = Samples[iSmp].nLength - 1;
			Samples[iSmp].nLoopStart = j;
			j = bswapLE32(*((uint32_t *)(s+0x18)));
			if (j > MAX_SAMPLE_LENGTH) j = MAX_SAMPLE_LENGTH;
			if (j < 2) j = 0;
			if (j > Samples[iSmp].nLength) j = Samples[iSmp].nLength;
			Samples[iSmp].nLoopEnd = j;
			j = s[0x1C];
			if (j > 64) j = 64;
			Samples[iSmp].nVolume = j << 2;
			Samples[iSmp].nGlobalVol = 64;
			if (s[0x1F]&1) Samples[iSmp].uFlags |= CHN_LOOP;
			j = bswapLE32(*((uint32_t *)(s+0x20)));
			if (!j) j = 8363;
			if (j < 1024) j = 1024;
			Samples[iSmp].nC5Speed = j;
			insfile[iSmp] = ((uint32_t)bswapLE16(*((uint16_t *)(s+0x0E)))) << 4;
			insfile[iSmp] += ((uint32_t)(uint8_t)s[0x0D]) << 20;
			if (insfile[iSmp] > dwMemLength) insfile[iSmp] &= 0xFFFF;
			if ((Samples[iSmp].nLoopStart >= Samples[iSmp].nLoopEnd) || (Samples[iSmp].nLoopEnd - Samples[iSmp].nLoopStart < 8))
				Samples[iSmp].nLoopStart = Samples[iSmp].nLoopEnd = 0;
			Samples[iSmp].nPan = 0x80;
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
            memcpy(Samples[iSmp].AdlibBytes, s+0x10, 11);

			int j = s[0x1C];
			if (j > 64) j = 64;
			Samples[iSmp].nVolume = j << 2;
			Samples[iSmp].nGlobalVol = 64;
			j = bswapLE32(*((uint32_t *)(s+0x20)));
			if (!j) j = 8363;
			Samples[iSmp].nC5Speed = j;
			Samples[iSmp].nPan = 0x80;
			Samples[iSmp].uFlags |= CHN_ADLIB;
			Samples[iSmp].uFlags &= ~(CHN_LOOP | CHN_16BIT);
			Samples[iSmp].nLength = 1;
			// Because most of the code in modplug requires
			// the presence of pSample when nLength is given,
			// we must have an at least 1-byte sample to make
			// it work. The actual contents of the sample don't
			// matter, since it will never be digitized. -Bisqwit
			Samples[iSmp].pSample = csf_allocate_sample(1);
			has_adlib_samples = true;
		}
		
		Instruments[iSmp] = csf_allocate_instrument();
		Instruments[iSmp]->nNNA = NNA_NOTEOFF;
		Instruments[iSmp]->nDCA = DCA_NOTEOFF;
		Instruments[iSmp]->nDCT = DCT_INSTRUMENT;
		Instruments[iSmp]->dwFlags = Samples[iSmp].uFlags;
		int scale;
		if(MidiS3M_Read(*Instruments[iSmp], iSmp, Samples[iSmp].name, scale))
		{
		    m_dwSongFlags |= SONG_INSTRUMENTMODE;
			Instruments[iSmp]->nGlobalVol = scale*128/63;
		}
		else
		{
		    csf_free_instrument(Instruments[iSmp]);
		    Instruments[iSmp] = NULL;
		}
		
		//fprintf(stderr, "loaded uflags = %X, length = %d\n", Samples[iSmp].uFlags, Samples[iSmp].nLength);
	}
	
	bool has_noteoff_commands = false;
	
	// Reading patterns
	for (uint32_t iPat=0; iPat<patnum; iPat++)
	{
                uint32_t nInd = ((uint32_t)ptr[nins+iPat]) << 4;
                // if the parapointer is zero, the pattern is blank (so ignore it)
                if (nInd == 0)
                        continue;
                if (nInd + 0x40 > dwMemLength) continue;
		uint16_t len = bswapLE16(*((uint16_t *)(lpStream+nInd)));
		nInd += 2;
		
		uint8_t * src = (uint8_t *)(lpStream+nInd);
		
		if(len < 2 || (nInd + (len-2) > dwMemLength)) continue;
		len -= 2;
		
		PatternAllocSize[iPat] = PatternSize[iPat] = 64;
		if ((Patterns[iPat] = csf_allocate_pattern(PatternSize[iPat], m_nChannels)) == NULL)
		{
			continue;
		}
		
		// Unpacking pattern
		MODCOMMAND *p = Patterns[iPat];
		for (uint32_t row=0, j=0; j < len; )
		{
			uint8_t b = src[j++];
			if (!b)
			{
				if (++row >= PatternSize[iPat]) {
					if (j != len)
						log_appendf(4, "Warning: Pattern %u is too long (has %u extra bytes)", iPat, len-j);
					break;
				}
			} else
			{
				uint32_t chn = b & 0x1F;
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
						uint32_t vol = src[j++];
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
						if (m->command) csf_import_s3m_effect(m, false);
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
						&& (Samples[ins].uFlags & CHN_ADLIB))
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
			unsigned pat = Orderlist[ord];
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
					&& (Samples[ins].uFlags & CHN_ADLIB))
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
	for (uint32_t iRaw=1; iRaw<=insnum; iRaw++) if ((Samples[iRaw].nLength) && (insfile[iRaw]))
	{
		uint32_t flags;
		if (insflags[iRaw-1] & 4)
			flags = (psfh.version == 1) ? RS_PCM16S : RS_PCM16U;
		else
			flags = (psfh.version == 1) ? RS_PCM8S : RS_PCM8U;
		if (insflags[iRaw-1] & 2) flags |= RSF_STEREO;
		dwMemPos = insfile[iRaw];
		dwMemPos += csf_read_sample(&Samples[iRaw], flags, (const char *)(lpStream + dwMemPos), dwMemLength - dwMemPos);
	}
	
	return true;
}


static uint8_t S3MFiller[16] =
{
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
	0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
};


bool CSoundFile::SaveS3M(diskwriter_driver_t *fp, uint32_t)
//----------------------------------------------------------
{
	uint8_t header[0x60];
	uint32_t nbo,nbi,nbp,i;
	uint32_t chanlim;
	uint16_t patptr[128];
	uint16_t insptr[128];
	uint8_t buffer[8+20*1024];
	S3MSAMPLESTRUCT insex[128];

	if ((!m_nChannels) || (!fp)) return false;
	// Writing S3M header
	memset(header, 0, sizeof(header));
	memset(insex, 0, sizeof(insex));
	memcpy(header, song_title, 0x1C);
	header[0x1B] = 0;
	header[0x1C] = 0x1A;
	header[0x1D] = 0x10;
        nbo = csf_get_num_orders(this);
        if (nbo < 2)
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
	for (i=0; i<MAX_ORDERS; i++) if ((Orderlist[i] < MAX_PATTERNS) && (Orderlist[i] >= nbp)) nbp = Orderlist[i] + 1;
	header[0x24] = nbp & 0xFF;
	header[0x25] = nbp >> 8;
	if (m_dwSongFlags & SONG_FASTVOLSLIDES) header[0x26] |= 0x40;
	//if ((m_nMaxPeriod < 20000) || (m_dwSongFlags & SONG_AMIGALIMITS)) header[0x26] |= 0x10;
	/* CWT/V identifiers:
	    STx.yy  = 1xyy
	    Orpheus = 2xyy
	    Impulse = 3xyy
	    So we'll use 4
	reference: http://xmp.cvs.sf.net/viewvc/xmp/xmp2/src/loaders/s3m_load.c?view=markup */
	header[0x28] = 0x50; // ST3.20 = 0x1320
	header[0x29] = 0x40;
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

	chanlim = csf_get_highest_used_channel(this) + 1;
	if (chanlim < 4) chanlim = 4;
	if (chanlim > 32) chanlim = 32;

	for (i=0; i<32; i++)
	{
		if (i < chanlim)
		{
			uint32_t tmp = (i & 0x0F) >> 1;
			header[0x40+i] = (i & 0x10) | ((i & 1) ? 8+tmp : tmp);
		} else header[0x40+i] = 0xFF;
	}
	fp->o(fp, (const unsigned char *)header, 0x60);
	fp->o(fp, (const unsigned char *)Orderlist, nbo);
	memset(patptr, 0, sizeof(patptr));
	memset(insptr, 0, sizeof(insptr));
	uint32_t ofs0 = 0x60 + nbo;
	uint32_t ofs1 = ((0x60 + nbo + nbi*2 + nbp*2 + 15) & 0xFFF0);
	uint32_t ofs = ofs1;
	if (header[0x35] == 0xFC) {
		ofs += 0x20;
		ofs1 += 0x20;
	}

	for (i=0; i<nbi; i++) insptr[i] = bswapLE16((uint16_t)((ofs + i*0x50) / 16));
	for (i=0; i<nbp; i++) patptr[i] = bswapLE16((uint16_t)((ofs + nbi*0x50) / 16));
	fp->o(fp, (const unsigned char *)insptr, nbi*2);
	fp->o(fp, (const unsigned char *)patptr, nbp*2);
	if (header[0x35] == 0xFC)
	{
		uint8_t chnpan[32];
		for (i=0; i<32; i++)
		{
			uint32_t nPan = ((Channels[i].nPan+7) < 0xF0) ? Channels[i].nPan+7 : 0xF0;
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
		uint16_t len = 64;
		memset(buffer, 0, sizeof(buffer));
		patptr[i] = bswapLE16(ofs / 16);
		if (Patterns[i])
		{
			len = 2;
			MODCOMMAND *p = Patterns[i];
			if(PatternSize[i] < 64)
				log_appendf(4, "Warning: Pattern %u has %u rows, padding", i, PatternSize[i]);
			else if (PatternSize[i] > 64)
				log_appendf(4, "Warning: Pattern %u has %u rows, truncating", i, PatternSize[i]);
			
			int row;
			for (row=0; row<PatternSize[i] && row < 64; row++)
			{
				for (uint32_t j=0; j < 32 && j<chanlim; j++)
				{
					uint32_t b = j;
					MODCOMMAND *m = &p[row*m_nChannels+j];
					uint32_t note = m->note;
					uint32_t volcmd = m->volcmd;
					uint32_t vol = m->vol;
					uint32_t command = m->command;
					uint32_t param = m->param;
					uint32_t inst = m->instr;

					if (m_dwSongFlags & SONG_INSTRUMENTMODE
					&& note && inst) {
						uint32_t nn = Instruments[inst]->Keyboard[note];
						uint32_t nm = Instruments[inst]->NoteMap[note];
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
						csf_export_s3m_effect(&command, &param, false);
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
			/* pad to 64 rows */
			for (; row<64; row++)
			{
				buffer[len++] = 0;
				if (len > sizeof(buffer) - 20) break;
			}
		}
		buffer[0] = (len) & 0xFF;
		buffer[1] = (len) >> 8;
		len = (len+15) & (~0x0F);
		
		fp->o(fp, (const unsigned char *)buffer, len);
		ofs += len;
	}
	// Writing samples
	for (i=1; i<=nbi; i++)
	{
		SONGSAMPLE *pins = &Samples[i];
		memcpy(insex[i-1].dosname, pins->filename, 12);
		memcpy(insex[i-1].name, pins->name, 28);
		memcpy(insex[i-1].scrs, "SCRS", 4);
		insex[i-1].hmem = (uint8_t)((uint32_t)ofs >> 20);
		insex[i-1].memseg = bswapLE16((uint16_t)((uint32_t)ofs >> 4));

		insex[i-1].vol = pins->nVolume / 4;
		insex[i-1].finetune = bswapLE32(pins->nC5Speed);

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
			uint32_t flags = RS_PCM8U;
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
			uint32_t len = csf_write_sample(fp, pins, flags, 0);
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
	return true;
}

