/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/

//////////////////////////////////////////////
// AMS module loader                        //
//////////////////////////////////////////////
#include "sndfile.h"

//#pragma warning(disable:4244)

#pragma pack(1)

typedef struct AMSFILEHEADER
{
	char szHeader[7];	// "Extreme"   // changed from int8_t
	uint8_t verlo, verhi;	// 0x??,0x01
	uint8_t chncfg;
	uint8_t samples;
	uint16_t patterns;
	uint16_t orders;
	uint8_t vmidi;
	uint16_t extra;
} AMSFILEHEADER;

typedef struct AMSSAMPLEHEADER
{
	uint32_t length;
	uint32_t loopstart;
	uint32_t loopend;
	uint8_t finetune_and_pan;
	uint16_t samplerate;	// C-2 = 8363
	uint8_t volume;		// 0-127
	uint8_t infobyte;
} AMSSAMPLEHEADER;


#pragma pack()



bool CSoundFile::ReadAMS(const uint8_t * lpStream, uint32_t dwMemLength)
//-----------------------------------------------------------
{
	uint8_t pkinf[MAX_SAMPLES];
	AMSFILEHEADER *pfh = (AMSFILEHEADER *)lpStream;
	uint32_t dwMemPos;
	uint32_t tmp, tmp2;
	
	if ((!lpStream) || (dwMemLength < 1024)) return false;
	if ((pfh->verhi != 0x01) || (strncmp(pfh->szHeader, "Extreme", 7))
	 || (!pfh->patterns) || (!pfh->orders) || (!pfh->samples) || (pfh->samples > MAX_SAMPLES)
	 || (pfh->patterns > MAX_PATTERNS) || (pfh->orders > MAX_ORDERS))
	{
		return ReadAMS2(lpStream, dwMemLength);
	}
	dwMemPos = sizeof(AMSFILEHEADER) + pfh->extra;
	if (dwMemPos + pfh->samples * sizeof(AMSSAMPLEHEADER) + 256 >= dwMemLength) return false;
	m_nType = MOD_TYPE_AMS;
	m_nInstruments = 0;
	m_nChannels = (pfh->chncfg & 0x1F) + 1;
	m_nSamples = pfh->samples;
	for (uint32_t nSmp=1; nSmp<=m_nSamples; nSmp++, dwMemPos += sizeof(AMSSAMPLEHEADER))
	{
		AMSSAMPLEHEADER *psh = (AMSSAMPLEHEADER *)(lpStream + dwMemPos);
		SONGSAMPLE *pins = &Samples[nSmp];
		pins->nLength = psh->length;
		pins->nLoopStart = psh->loopstart;
		pins->nLoopEnd = psh->loopend;
		pins->nGlobalVol = 64;
		pins->nVolume = psh->volume << 1;
		pins->nC5Speed = psh->samplerate;
		pins->nPan = (psh->finetune_and_pan & 0xF0);
		if (pins->nPan < 0x80) pins->nPan += 0x10;
		//pins->nFineTune = MOD2XMFineTune(psh->finetune_and_pan & 0x0F);
		pins->uFlags = (psh->infobyte & 0x80) ? CHN_16BIT : 0;
		if ((pins->nLoopEnd <= pins->nLength) && (pins->nLoopStart+4 <= pins->nLoopEnd)) pins->uFlags |= CHN_LOOP;
		pkinf[nSmp] = psh->infobyte;
	}
	// Read Song Name
	tmp = lpStream[dwMemPos++];
	if (dwMemPos + tmp + 1 >= dwMemLength) return true;
	tmp2 = (tmp < 32) ? tmp : 31;
	if (tmp2) memcpy(song_title, lpStream+dwMemPos, tmp2);
	song_title[tmp2] = 0;
	dwMemPos += tmp;
	// Read sample names
	for (uint32_t sNam=1; sNam<=m_nSamples; sNam++)
	{
		if (dwMemPos + 32 >= dwMemLength) return true;
		tmp = lpStream[dwMemPos++];
		tmp2 = (tmp < 32) ? tmp : 31;
		if (tmp2) memcpy(Samples[sNam].name, lpStream+dwMemPos, tmp2);
		dwMemPos += tmp;
	}
	// Skip Channel names
	for (uint32_t cNam=0; cNam<m_nChannels; cNam++)
	{
		if (dwMemPos + 32 >= dwMemLength) return true;
		tmp = lpStream[dwMemPos++];
		dwMemPos += tmp;
	}
	// Skip Pattern Names
	for (uint32_t pNam=0; pNam < pfh->patterns; pNam++)
	{
		if (dwMemPos + 32 >= dwMemLength) return true;
		tmp = lpStream[dwMemPos++];
		dwMemPos += tmp;
	}
	if (dwMemPos >= dwMemLength) return true;
	// Read Song Comments
	tmp = *((uint16_t *)(lpStream+dwMemPos));
	dwMemPos += 2;
	if (dwMemPos + tmp >= dwMemLength) return true;
	if (tmp)
	{
		m_lpszSongComments = new char[tmp+1];  // changed from int8_t
		if (!m_lpszSongComments) return true;
		memset(m_lpszSongComments, 0, tmp+1);
		memcpy(m_lpszSongComments, lpStream + dwMemPos, tmp);
		dwMemPos += tmp;
	}
	// Read Order List
	for (uint32_t iOrd=0; iOrd<pfh->orders; iOrd++, dwMemPos += 2)
	{
		uint32_t n = *((uint16_t *)(lpStream+dwMemPos));
		Orderlist[iOrd] = (uint8_t)n;
	}
	// Read Patterns
	for (uint32_t iPat=0; iPat<pfh->patterns; iPat++)
	{
		if (dwMemPos + 4 >= dwMemLength) return true;
		uint32_t len = *((uint32_t *)(lpStream + dwMemPos));
		dwMemPos += 4;
		if ((len >= dwMemLength) || (dwMemPos + len > dwMemLength)) return true;
		PatternSize[iPat] = 64;
		PatternAllocSize[iPat] = 64;
		MODCOMMAND *m = csf_allocate_pattern(PatternSize[iPat], m_nChannels);
		if (!m) return true;
		Patterns[iPat] = m;
		const uint8_t *p = lpStream + dwMemPos;
		uint32_t row = 0, i = 0;
		while ((row < PatternSize[iPat]) && (i+2 < len))
		{
			uint8_t b0 = p[i++];
			uint8_t b1 = p[i++];
			uint8_t b2 = 0;
			uint32_t ch = b0 & 0x3F;
			// Note+Instr
			if (!(b0 & 0x40))
			{
				b2 = p[i++];
				if (ch < m_nChannels)
				{
					if (b1 & 0x7F) m[ch].note = (b1 & 0x7F) + 25;
					m[ch].instr = b2;
				}
				if (b1 & 0x80)
				{
					b0 |= 0x40;
					b1 = p[i++];
				}
			}
			// Effect
			if (b0 & 0x40)
			{
			anothercommand:
				if (b1 & 0x40)
				{
					if (ch < m_nChannels)
					{
						m[ch].volcmd = VOLCMD_VOLUME;
						m[ch].vol = b1 & 0x3F;
					}
				} else
				{
					b2 = p[i++];
					if (ch < m_nChannels)
					{
						uint32_t cmd = b1 & 0x3F;
						if (cmd == 0x0C)
						{
							m[ch].volcmd = VOLCMD_VOLUME;
							m[ch].vol = b2 >> 1;
						} else
						if (cmd == 0x0E)
						{
							if (!m[ch].command)
							{
								uint32_t command = CMD_S3MCMDEX;
								uint32_t param = b2;
								switch(param & 0xF0)
								{
								case 0x00:	if (param & 0x08) { param &= 0x07; param |= 0x90; } else {command=param=0;} break;
								case 0x10:	command = CMD_PORTAMENTOUP; param |= 0xF0; break;
								case 0x20:	command = CMD_PORTAMENTODOWN; param |= 0xF0; break;
								case 0x30:	param = (param & 0x0F) | 0x10; break;
								case 0x40:	param = (param & 0x0F) | 0x30; break;
								case 0x50:	param = (param & 0x0F) | 0x20; break;
								case 0x60:	param = (param & 0x0F) | 0xB0; break;
								case 0x70:	param = (param & 0x0F) | 0x40; break;
								case 0x90:	command = CMD_RETRIG; param &= 0x0F; break;
								case 0xA0:	if (param & 0x0F) { command = CMD_VOLUMESLIDE; param = (param << 4) | 0x0F; } else command=param=0; break;
								case 0xB0:	if (param & 0x0F) { command = CMD_VOLUMESLIDE; param |= 0xF0; } else command=param=0; break;
								}
								m[ch].command = command;
								m[ch].param = param;
							}
						} else
						{
							m[ch].command = cmd;
							m[ch].param = b2;
							ConvertModCommand(&m[ch], 0);
						}
					}
				}
				if (b1 & 0x80)
				{
					b1 = p[i++];
					if (i <= len) goto anothercommand;
				}
			}
			if (b0 & 0x80)
			{
				row++;
				m += m_nChannels;
			}
		}
		dwMemPos += len;
	}
	// Read Samples
	for (uint32_t iSmp=1; iSmp<=m_nSamples; iSmp++) if (Samples[iSmp].nLength)
	{
		if (dwMemPos >= dwMemLength - 9) return true;
		uint32_t flags = (Samples[iSmp].uFlags & CHN_16BIT) ? RS_AMS16 : RS_AMS8;
		dwMemPos += csf_read_sample(&Samples[iSmp], flags, (const char *)(lpStream+dwMemPos), dwMemLength-dwMemPos);
	}
	return true;
}


/////////////////////////////////////////////////////////////////////
// AMS 2.2 loader

#pragma pack(1)

typedef struct AMS2FILEHEADER
{
	uint32_t dwHdr1;		// AMShdr
	uint16_t wHdr2;
	uint8_t b1A;			// 0x1A
	uint8_t titlelen;		// 30-bytes max
	int8_t szTitle[30];	// [titlelen]
} AMS2FILEHEADER;

typedef struct AMS2SONGHEADER
{
	uint16_t version;
	uint8_t instruments;
	uint16_t patterns;
	uint16_t orders;
	uint16_t bpm;
	uint8_t speed;
	uint8_t channels;
	uint8_t commands;
	uint8_t rows;
	uint16_t flags;
} AMS2SONGHEADER;

typedef struct AMS2INSTRUMENT
{
	uint8_t samples;
	uint8_t notemap[120];
} AMS2INSTRUMENT;

typedef struct AMS2ENVELOPE
{
	uint8_t speed;
	uint8_t sustain;
	uint8_t loopbegin;
	uint8_t loopend;
	uint8_t points;
	uint8_t info[3];
} AMS2ENVELOPE;

typedef struct AMS2SAMPLE
{
	uint32_t length;
	uint32_t loopstart;
	uint32_t loopend;
	uint16_t frequency;
	uint8_t finetune;
	uint16_t c4speed;
	int8_t transpose;
	uint8_t volume;
	uint8_t flags;
} AMS2SAMPLE;


#pragma pack()


bool CSoundFile::ReadAMS2(const uint8_t * lpStream, uint32_t dwMemLength)
//------------------------------------------------------------
{
	AMS2FILEHEADER *pfh = (AMS2FILEHEADER *)lpStream;
	AMS2SONGHEADER *psh;
	uint32_t dwMemPos;
	uint8_t smpmap[16];
	uint8_t packedsamples[MAX_SAMPLES];

	if ((pfh->dwHdr1 != 0x68534D41) || (pfh->wHdr2 != 0x7264)
	 || (pfh->b1A != 0x1A) || (pfh->titlelen > 30)) return false;
	dwMemPos = pfh->titlelen + 8;
	psh = (AMS2SONGHEADER *)(lpStream + dwMemPos);
	if (((psh->version & 0xFF00) != 0x0200) || (!psh->instruments)
	 || (psh->instruments > MAX_INSTRUMENTS) || (!psh->patterns) || (!psh->orders)) return false;
	dwMemPos += sizeof(AMS2SONGHEADER);
	if (pfh->titlelen)
	{
		memcpy(song_title, pfh->szTitle, pfh->titlelen);
		song_title[pfh->titlelen] = 0;
	}
	m_nType = MOD_TYPE_AMS;
	m_nChannels = 32;
	m_nDefaultTempo = psh->bpm >> 8;
	m_nDefaultSpeed = psh->speed;
	m_nInstruments = psh->instruments;
	m_nSamples = 0;
	m_dwSongFlags |= SONG_INSTRUMENTMODE;
	if (psh->flags & 0x40) m_dwSongFlags |= SONG_LINEARSLIDES;
	for (uint32_t nIns=1; nIns<=m_nInstruments; nIns++)
	{
		uint32_t insnamelen = lpStream[dwMemPos];
		int8_t *pinsname = (int8_t *)(lpStream+dwMemPos+1);
		dwMemPos += insnamelen + 1;
		AMS2INSTRUMENT *pins = (AMS2INSTRUMENT *)(lpStream + dwMemPos);
		dwMemPos += sizeof(AMS2INSTRUMENT);
		if (dwMemPos + 1024 >= dwMemLength) return true;
		AMS2ENVELOPE *volenv, *panenv, *pitchenv;
		volenv = (AMS2ENVELOPE *)(lpStream+dwMemPos);
		dwMemPos += 5 + volenv->points*3;
		panenv = (AMS2ENVELOPE *)(lpStream+dwMemPos);
		dwMemPos += 5 + panenv->points*3;
		pitchenv = (AMS2ENVELOPE *)(lpStream+dwMemPos);
		dwMemPos += 5 + pitchenv->points*3;
		SONGINSTRUMENT *penv = new SONGINSTRUMENT;
		if (!penv) return true;
		memset(smpmap, 0, sizeof(smpmap));
		memset(penv, 0, sizeof(SONGINSTRUMENT));
		for (uint32_t ismpmap=0; ismpmap<pins->samples; ismpmap++)
		{
			if ((ismpmap >= 16) || (m_nSamples+1 >= MAX_SAMPLES)) break;
			m_nSamples++;
			smpmap[ismpmap] = m_nSamples;
		}
		penv->nGlobalVol = 64;
		penv->nPan = 128;
		penv->nPPC = 60;
		Instruments[nIns] = penv;
		if (insnamelen)
		{
			if (insnamelen > 31) insnamelen = 31;
			memcpy(penv->name, pinsname, insnamelen);
			penv->name[insnamelen] = 0;
		}
		for (uint32_t inotemap=0; inotemap<120; inotemap++)
		{
			penv->NoteMap[inotemap] = inotemap+1;
			penv->Keyboard[inotemap] = smpmap[pins->notemap[inotemap] & 0x0F];
		}
		// Volume Envelope
		{
			uint32_t pos = 0;
			penv->VolEnv.nNodes = (volenv->points > 16) ? 16 : volenv->points;
			penv->VolEnv.nSustainStart = penv->VolEnv.nSustainEnd = volenv->sustain;
			penv->VolEnv.nLoopStart = volenv->loopbegin;
			penv->VolEnv.nLoopEnd = volenv->loopend;
			for (int i=0; i<penv->VolEnv.nNodes; i++)
			{
				penv->VolEnv.Values[i] = (uint8_t)((volenv->info[i*3+2] & 0x7F) >> 1);
				pos += volenv->info[i*3] + ((volenv->info[i*3+1] & 1) << 8);
				penv->VolEnv.Ticks[i] = (uint16_t)pos;
			}
		}
		penv->nFadeOut = (((lpStream[dwMemPos+2] & 0x0F) << 8) | (lpStream[dwMemPos+1])) << 3;
		uint32_t envflags = lpStream[dwMemPos+3];
		if (envflags & 0x01) penv->dwFlags |= ENV_VOLLOOP;
		if (envflags & 0x02) penv->dwFlags |= ENV_VOLSUSTAIN;
		if (envflags & 0x04) penv->dwFlags |= ENV_VOLUME;
		dwMemPos += 5;
		// Read Samples
		for (uint32_t ismp=0; ismp<pins->samples; ismp++)
		{
			SONGSAMPLE *psmp = ((ismp < 16) && (smpmap[ismp])) ? &Samples[smpmap[ismp]] : NULL;
			uint32_t smpnamelen = lpStream[dwMemPos];
			if ((psmp) && (smpnamelen) && (smpnamelen <= 22))
			{
				memcpy(Samples[smpmap[ismp]].name, lpStream+dwMemPos+1, smpnamelen);
			}
			dwMemPos += smpnamelen + 1;
			if (psmp)
			{
				AMS2SAMPLE *pams = (AMS2SAMPLE *)(lpStream+dwMemPos);
				psmp->nGlobalVol = 64;
				psmp->nPan = 128;
				psmp->nLength = pams->length;
				psmp->nLoopStart = pams->loopstart;
				psmp->nLoopEnd = pams->loopend;
				psmp->nC5Speed = pams->c4speed;
				//psmp->RelativeTone = pams->transpose;
				psmp->nVolume = pams->volume / 2;
				packedsamples[smpmap[ismp]] = pams->flags;
				if (pams->flags & 0x04) psmp->uFlags |= CHN_16BIT;
				if (pams->flags & 0x08) psmp->uFlags |= CHN_LOOP;
				if (pams->flags & 0x10) psmp->uFlags |= CHN_PINGPONGLOOP;
			}
			dwMemPos += sizeof(AMS2SAMPLE);
		}
	}
	if (dwMemPos + 256 >= dwMemLength) return true;
	// Comments
	{
		uint32_t composernamelen = lpStream[dwMemPos];
		if (composernamelen)
		{
			m_lpszSongComments = new char[composernamelen+1]; // changed from int8_t
			if (m_lpszSongComments)
			{
				memcpy(m_lpszSongComments, lpStream+dwMemPos+1, composernamelen);
				m_lpszSongComments[composernamelen] = 0;
			}
		}
		dwMemPos += composernamelen + 1;
		// channel names
		for (uint32_t i=0; i<32; i++)
		{
			uint32_t chnnamlen = lpStream[dwMemPos];
			dwMemPos += chnnamlen + 1;
			if (dwMemPos + chnnamlen + 256 >= dwMemLength) return true;
		}
		// packed comments (ignored)
		uint32_t songtextlen = *((uint32_t *)(lpStream+dwMemPos));
		dwMemPos += songtextlen;
		if (dwMemPos + 256 >= dwMemLength) return true;
	}
	// Order List
	{
		for (uint32_t i=0; i<MAX_ORDERS; i++)
		{
			Orderlist[i] = 0xFF;
			if (dwMemPos + 2 >= dwMemLength) return true;
			if (i < psh->orders)
			{
				Orderlist[i] = lpStream[dwMemPos];
				dwMemPos += 2;
			}
		}
	}
	// Pattern Data
	for (uint32_t ipat=0; ipat<psh->patterns; ipat++)
	{
		if (dwMemPos+8 >= dwMemLength) return true;
		uint32_t packedlen = *((uint32_t *)(lpStream+dwMemPos));
		uint32_t numrows = 1 + (uint32_t)(lpStream[dwMemPos+4]);
		//uint32_t patchn = 1 + (uint32_t)(lpStream[dwMemPos+5] & 0x1F);
		//uint32_t patcmds = 1 + (uint32_t)(lpStream[dwMemPos+5] >> 5);
		uint32_t patnamlen = lpStream[dwMemPos+6];
		dwMemPos += 4;
		if ((ipat < MAX_PATTERNS) && (packedlen < dwMemLength-dwMemPos) && (numrows >= 8))
		{
			PatternSize[ipat] = numrows;
			PatternAllocSize[ipat] = numrows;
			Patterns[ipat] = csf_allocate_pattern(numrows, m_nChannels);
			if (!Patterns[ipat]) return true;
			// Unpack Pattern Data
			const uint8_t * psrc = lpStream + dwMemPos;
			uint32_t pos = 3 + patnamlen;
			uint32_t row = 0;
			while ((pos < packedlen) && (row < numrows))
			{
				MODCOMMAND *m = Patterns[ipat] + row * m_nChannels;
				uint32_t byte1 = psrc[pos++];
				uint32_t ch = byte1 & 0x1F;
				// Read Note + Instr
				if (!(byte1 & 0x40))
				{
					uint32_t byte2 = psrc[pos++];
					uint32_t note = byte2 & 0x7F;
					if (note) m[ch].note = (note > 1) ? (note-1) : 0xFF;
					m[ch].instr = psrc[pos++];
					// Read Effect
					while (byte2 & 0x80)
					{
						byte2 = psrc[pos++];
						if (byte2 & 0x40)
						{
							m[ch].volcmd = VOLCMD_VOLUME;
							m[ch].vol = byte2 & 0x3F;
						} else
						{
							uint32_t command = byte2 & 0x3F;
							uint32_t param = psrc[pos++];
							if (command == 0x0C)
							{
								m[ch].volcmd = VOLCMD_VOLUME;
								m[ch].vol = param / 2;
							} else
							if (command < 0x10)
							{
								m[ch].command = command;
								m[ch].param = param;
								ConvertModCommand(&m[ch], 0);
							} else
							{
								// TODO: AMS effects
							}
						}
					}
				}
				if (byte1 & 0x80) row++;
			}
		}
		dwMemPos += packedlen;
	}
	// Read Samples
	for (uint32_t iSmp=1; iSmp<=m_nSamples; iSmp++) if (Samples[iSmp].nLength)
	{
		if (dwMemPos >= dwMemLength - 9) return true;
		uint32_t flags;
		if (packedsamples[iSmp] & 0x03)
		{
			flags = (Samples[iSmp].uFlags & CHN_16BIT) ? RS_AMS16 : RS_AMS8;
		} else
		{
			flags = (Samples[iSmp].uFlags & CHN_16BIT) ? RS_PCM16S : RS_PCM8S;
		}
		dwMemPos += csf_read_sample(&Samples[iSmp], flags, (const char *)(lpStream+dwMemPos), dwMemLength-dwMemPos);
	}
	return true;
}


/////////////////////////////////////////////////////////////////////
// AMS Sample unpacking

void AMSUnpack(const char *psrc, uint32_t inputlen, char *pdest, uint32_t dmax, char packcharacter)
{
	uint32_t tmplen = dmax;
	signed char *amstmp = new signed char[tmplen];
	
	if (!amstmp) return;
	// Unpack Loop
	{
		signed char *p = amstmp;
		uint32_t i=0, j=0;
		while ((i < inputlen) && (j < tmplen))
		{
			signed char ch = psrc[i++];
			if (ch == packcharacter)
			{
				uint8_t ch2 = psrc[i++];
				if (ch2)
				{
					ch = psrc[i++];
					while (ch2--)
					{
						p[j++] = ch;
						if (j >= tmplen) break;
					}
				} else p[j++] = packcharacter;
			} else p[j++] = ch;
		}
	}
	// Bit Unpack Loop
	{
		signed char *p = amstmp;
		uint32_t bitcount = 0x80, dh;
		uint32_t k=0;
		for (uint32_t i=0; i<dmax; i++)
		{
			uint8_t al = *p++;
			dh = 0;
			for (uint32_t count=0; count<8; count++)
			{
				uint32_t bl = al & bitcount;
				bl = ((bl|(bl<<8)) >> ((dh+8-count) & 7)) & 0xFF;
				bitcount = ((bitcount|(bitcount<<8)) >> 1) & 0xFF;
				pdest[k++] |= bl;
				if (k >= dmax)
				{
					k = 0;
					dh++;
				}
			}
			bitcount = ((bitcount|(bitcount<<8)) >> dh) & 0xFF;
		}
	}
	// Delta Unpack
	{
		signed char old = 0;
		for (uint32_t i=0; i<dmax; i++)
		{
			int pos = ((uint8_t *)pdest)[i];
			if ((pos != 128) && (pos & 0x80)) pos = -(pos & 0x7F);
			old -= (signed char)pos;
			pdest[i] = old;
		}
	}
	delete amstmp;
}

