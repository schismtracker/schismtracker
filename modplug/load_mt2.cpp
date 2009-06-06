#include "sndfile.h"
#include "snd_fx.h"

//#define MT2DEBUG

#pragma pack(1)

typedef struct _MT2FILEHEADER
{
	uint32_t dwMT20;	// 0x3032544D "MT20"
	uint32_t dwSpecial;
	uint16_t wVersion;
	int8_t szTrackerName[32];	// "MadTracker 2.0"
	int8_t szSongName[64];
	uint16_t nOrders;
	uint16_t wRestart;
	uint16_t wPatterns;
	uint16_t wChannels;
	uint16_t wSamplesPerTick;
	uint8_t bTicksPerLine;
	uint8_t bLinesPerBeat;
	uint32_t fulFlags; // b0=packed patterns
	uint16_t wInstruments;
	uint16_t wSamples;
	uint8_t Orders[256];
} MT2FILEHEADER;

typedef struct _MT2PATTERN
{
	uint16_t wLines;
	uint32_t wDataLen;
} MT2PATTERN;

typedef struct _MT2COMMAND
{
	uint8_t note;	// 0=nothing, 97=note off
	uint8_t instr;
	uint8_t vol;
	uint8_t pan;
	uint8_t fxcmd;
	uint8_t fxparam1;
	uint8_t fxparam2;
} MT2COMMAND;

typedef struct _MT2DRUMSDATA
{
	uint16_t wDrumPatterns;
	uint16_t wDrumSamples[8];
	uint8_t DrumPatternOrder[256];
} MT2DRUMSDATA;

typedef struct _MT2AUTOMATION
{
	uint32_t dwFlags;
	uint32_t dwEffectId;
	uint32_t nEnvPoints;
} MT2AUTOMATION;

typedef struct _MT2INSTRUMENT
{
	int8_t szName[32];
	uint32_t dwDataLen;
	uint16_t wSamples;
	uint8_t GroupsMapping[96];
	uint8_t bVibType;
	uint8_t bVibSweep;
	uint8_t bVibDepth;
	uint8_t bVibRate;
	uint16_t wFadeOut;
	uint16_t wNNA;
	uint16_t wInstrFlags;
	uint16_t wEnvFlags1;
	uint16_t wEnvFlags2;
} MT2INSTRUMENT;

typedef struct _MT2ENVELOPE
{
	uint8_t nFlags;
	uint8_t nPoints;
	uint8_t nSustainPos;
	uint8_t nLoopStart;
	uint8_t nLoopEnd;
	uint8_t bReserved[3];
	uint8_t EnvData[64];
} MT2ENVELOPE;

typedef struct _MT2SYNTH
{
	uint8_t nSynthId;
	uint8_t nFxId;
	uint16_t wCutOff;
	uint8_t nResonance;
	uint8_t nAttack;
	uint8_t nDecay;
	uint8_t bReserved[25];
} MT2SYNTH;

typedef struct _MT2SAMPLE
{
	int8_t szName[32];
	uint32_t dwDataLen;
	uint32_t dwLength;
	uint32_t dwFrequency;
	uint8_t nQuality;
	uint8_t nChannels;
	uint8_t nFlags;
	uint8_t nLoop;
	uint32_t dwLoopStart;
	uint32_t dwLoopEnd;
	uint16_t wVolume;
	uint8_t nPan;
	uint8_t nBaseNote;
	uint16_t wSamplesPerBeat;
} MT2SAMPLE;

typedef struct _MT2GROUP
{
	uint8_t nSmpNo;
	uint8_t nVolume;	// 0-128
	uint8_t nFinePitch;
	uint8_t Reserved[5];
} MT2GROUP;

#pragma pack()


static void ConvertMT2Command(CSoundFile *that, MODCOMMAND *m, MT2COMMAND *p)
//---------------------------------------------------------------------------
{
	// Note
	m->note = 0;
	if (p->note) m->note = (p->note > 96) ? 0xFF : p->note+12;
	// Instrument
	m->instr = p->instr;
	// Volume Column
	if ((p->vol >= 0x10) && (p->vol <= 0x90))
	{
		m->volcmd = VOLCMD_VOLUME;
		m->vol = (p->vol - 0x10) >> 1;
	} else
	if ((p->vol >= 0xA0) && (p->vol <= 0xAF))
	{
		m->volcmd = VOLCMD_VOLSLIDEDOWN;
		m->vol = (p->vol & 0x0f);
	} else
	if ((p->vol >= 0xB0) && (p->vol <= 0xBF))
	{
		m->volcmd = VOLCMD_VOLSLIDEUP;
		m->vol = (p->vol & 0x0f);
	} else
	if ((p->vol >= 0xC0) && (p->vol <= 0xCF))
	{
		m->volcmd = VOLCMD_FINEVOLDOWN;
		m->vol = (p->vol & 0x0f);
	} else
	if ((p->vol >= 0xD0) && (p->vol <= 0xDF))
	{
		m->volcmd = VOLCMD_FINEVOLUP;
		m->vol = (p->vol & 0x0f);
	} else
	{
		m->volcmd = 0;
		m->vol = 0;
	}
	// Effects
	m->command = 0;
	m->param = 0;
	if ((p->fxcmd) || (p->fxparam1) || (p->fxparam2))
	{
		if (!p->fxcmd)
		{
			m->command = p->fxparam2;
			m->param = p->fxparam1;
			that->ConvertModCommand(m, 1);
		} else
		{
			// TODO: MT2 Effects
		}
	}
}


bool CSoundFile::ReadMT2(const uint8_t * lpStream, uint32_t dwMemLength)
//-----------------------------------------------------------
{
	MT2FILEHEADER *pfh = (MT2FILEHEADER *)lpStream;
	uint32_t dwMemPos, dwDrumDataPos, dwExtraDataPos;
	uint32_t nDrumDataLen, nExtraDataLen;
	MT2DRUMSDATA *pdd;
	MT2INSTRUMENT *InstrMap[255];
	MT2SAMPLE *SampleMap[256];

	if ((!lpStream) || (dwMemLength < sizeof(MT2FILEHEADER))
	 || (pfh->dwMT20 != 0x3032544D)
	 || (pfh->wVersion < 0x0200) || (pfh->wVersion >= 0x0300)
	 || (pfh->wChannels < 4) || (pfh->wChannels > 64)) return false;
	pdd = NULL;
	m_nType = MOD_TYPE_MT2;
	m_nChannels = pfh->wChannels;
	m_nRestartPos = pfh->wRestart;
	m_nDefaultSpeed = pfh->bTicksPerLine;
	m_nDefaultTempo = 125;
	if ((pfh->wSamplesPerTick > 100) && (pfh->wSamplesPerTick < 5000))
	{
		m_nDefaultTempo = 110250 / pfh->wSamplesPerTick;
	}
	for (uint32_t iOrd=0; iOrd<MAX_ORDERS; iOrd++)
	{
		Order[iOrd] = (uint8_t)((iOrd < pfh->nOrders) ? pfh->Orders[iOrd] : 0xFF);
	}
	memcpy(m_szNames[0], pfh->szSongName, 32);
	m_szNames[0][31] = 0;
	dwMemPos = sizeof(MT2FILEHEADER);
	nDrumDataLen = *(uint16_t *)(lpStream + dwMemPos);
	dwDrumDataPos = dwMemPos + 2;
	if (nDrumDataLen >= 2) pdd = (MT2DRUMSDATA *)(lpStream+dwDrumDataPos);
	dwMemPos += 2 + nDrumDataLen;
#ifdef MT2DEBUG

	Log("MT2 v%03X: \"%s\" (flags=%04X)\n", pfh->wVersion, m_szNames[0], pfh->fulFlags);
	Log("%d Channels, %d Patterns, %d Instruments, %d Samples\n", pfh->wChannels, pfh->wPatterns, pfh->wInstruments, pfh->wSamples);
	Log("Drum Data: %d bytes @%04X\n", nDrumDataLen, dwDrumDataPos);
#endif
	if (dwMemPos >= dwMemLength-12) return true;
	if (!*(uint32_t *)(lpStream+dwMemPos)) dwMemPos += 4;
	if (!*(uint32_t *)(lpStream+dwMemPos)) dwMemPos += 4;
	nExtraDataLen = *(uint32_t *)(lpStream+dwMemPos);
	dwExtraDataPos = dwMemPos + 4;
	dwMemPos += 4;
#ifdef MT2DEBUG
	Log("Extra Data: %d bytes @%04X\n", nExtraDataLen, dwExtraDataPos);
#endif
	if (dwMemPos + nExtraDataLen >= dwMemLength) return true;
	while (dwMemPos+8 < dwExtraDataPos + nExtraDataLen)
	{
		uint32_t dwId = *(uint32_t *)(lpStream+dwMemPos);
		uint32_t dwLen = *(uint32_t *)(lpStream+dwMemPos+4);
		dwMemPos += 8;
		if (dwMemPos + dwLen > dwMemLength) return true;
#ifdef MT2DEBUG
		int8_t s[5];
		memcpy(s, &dwId, 4);
		s[4] = 0;
		Log("pos=0x%04X: %s: %d bytes\n", dwMemPos-8, s, dwLen);
#endif
		switch(dwId)
		{
		// MSG
		case 0x0047534D:
			if ((dwLen > 3) && (!m_lpszSongComments))
			{
				uint32_t nTxtLen = dwLen;
				if (nTxtLen > 32000) nTxtLen = 32000;
				m_lpszSongComments = new char[nTxtLen];  // changed from int8_t
				if (m_lpszSongComments)
				{
					memcpy(m_lpszSongComments, lpStream+dwMemPos+1, nTxtLen-1);
					m_lpszSongComments[nTxtLen-1] = 0;
				}
			}
			break;
		// SUM -> author name (or "Unregistered")
		// TMAP
		// TRKS
		case 0x534b5254:
			break;
		}
		dwMemPos += dwLen;
	}
	// Load Patterns
	dwMemPos = dwExtraDataPos + nExtraDataLen;
	for (uint32_t iPat=0; iPat<pfh->wPatterns; iPat++) if (dwMemPos < dwMemLength-6)
	{
		MT2PATTERN *pmp = (MT2PATTERN *)(lpStream+dwMemPos);
		uint32_t wDataLen = (pmp->wDataLen + 1) & ~1;
		dwMemPos += 6;
		if (dwMemPos + wDataLen > dwMemLength) break;
		uint32_t nLines = pmp->wLines;
		if ((iPat < MAX_PATTERNS) && (nLines > 0) && (nLines <= 256))
		{
	#ifdef MT2DEBUG
			Log("Pattern #%d @%04X: %d lines, %d bytes\n", iPat, dwMemPos-6, nLines, pmp->wDataLen);
	#endif
			PatternSize[iPat] = nLines;
			PatternAllocSize[iPat] = nLines;
			Patterns[iPat] = AllocatePattern(nLines, m_nChannels);
			if (!Patterns[iPat]) return true;
			MODCOMMAND *m = Patterns[iPat];
			uint32_t len = wDataLen;
			if (pfh->fulFlags & 1) // Packed Patterns
			{
				uint8_t *p = (uint8_t *)(lpStream+dwMemPos);
				uint32_t pos = 0, row=0, ch=0;
				while (pos < len)
				{
					MT2COMMAND cmd;
					uint32_t infobyte = p[pos++];
					uint32_t rptcount = 0;
					if (infobyte == 0xff)
					{
						rptcount = p[pos++];
						infobyte = p[pos++];
				#if 0
						Log("(%d.%d) FF(%02X).%02X\n", row, ch, rptcount, infobyte);
					} else
					{
						Log("(%d.%d) %02X\n", row, ch, infobyte);
				#endif
					}
					if (infobyte & 0x7f)
					{
						uint32_t patpos = row*m_nChannels+ch;
						cmd.note = cmd.instr = cmd.vol = cmd.pan = cmd.fxcmd = cmd.fxparam1 = cmd.fxparam2 = 0;
						if (infobyte & 1) cmd.note = p[pos++];
						if (infobyte & 2) cmd.instr = p[pos++];
						if (infobyte & 4) cmd.vol = p[pos++];
						if (infobyte & 8) cmd.pan = p[pos++];
						if (infobyte & 16) cmd.fxcmd = p[pos++];
						if (infobyte & 32) cmd.fxparam1 = p[pos++];
						if (infobyte & 64) cmd.fxparam2 = p[pos++];
					#ifdef MT2DEBUG
						if (cmd.fxcmd)
						{
							Log("(%d.%d) MT2 FX=%02X.%02X.%02X\n", row, ch, cmd.fxcmd, cmd.fxparam1, cmd.fxparam2);
						}
					#endif
						ConvertMT2Command(this, &m[patpos], &cmd);
					}
					row += rptcount+1;
					while (row >= nLines) { row-=nLines; ch++; }
					if (ch >= m_nChannels) break;
				}
			} else
			{
				MT2COMMAND *p = (MT2COMMAND *)(lpStream+dwMemPos);
				uint32_t n = 0;
				while ((len > sizeof(MT2COMMAND)) && (n < m_nChannels*nLines))
				{
					ConvertMT2Command(this, m, p);
					len -= sizeof(MT2COMMAND);
					n++;
					p++;
					m++;
				}
			}
		}
		dwMemPos += wDataLen;
	}
	// Skip Drum Patterns
	if (pdd)
	{
	#ifdef MT2DEBUG
		Log("%d Drum Patterns at offset 0x%08X\n", pdd->wDrumPatterns, dwMemPos);
	#endif
		for (uint32_t iDrm=0; iDrm<pdd->wDrumPatterns; iDrm++)
		{
			if (dwMemPos > dwMemLength-2) return true;
			uint32_t nLines = *(uint16_t *)(lpStream+dwMemPos);
		#ifdef MT2DEBUG
			if (nLines != 64) Log("Drum Pattern %d: %d Lines @%04X\n", iDrm, nLines, dwMemPos);
		#endif
			dwMemPos += 2 + nLines * 32;
		}
	}
	// Automation
	if (pfh->fulFlags & 2)
	{
	#ifdef MT2DEBUG
		Log("Automation at offset 0x%08X\n", dwMemPos);
	#endif
		uint32_t nAutoCount = m_nChannels;
		if (pfh->fulFlags & 0x10) nAutoCount++; // Master Automation
		if ((pfh->fulFlags & 0x08) && (pdd)) nAutoCount += 8; // Drums Automation
		nAutoCount *= pfh->wPatterns;
		for (uint32_t iAuto=0; iAuto<nAutoCount; iAuto++)
		{
			if (dwMemPos+12 >= dwMemLength) return true;
			MT2AUTOMATION *pma = (MT2AUTOMATION *)(lpStream+dwMemPos);
			dwMemPos += (pfh->wVersion <= 0x201) ? 4 : 8;
			for (uint32_t iEnv=0; iEnv<14; iEnv++)
			{
				if (pma->dwFlags & (1 << iEnv))
				{
				#ifdef MT2DEBUG
					uint32_t nPoints = *(uint32_t *)(lpStream+dwMemPos);
					Log("  Env[%d/%d] %04X @%04X: %d points\n", iAuto, nAutoCount, 1 << iEnv, dwMemPos-8, nPoints);
				#endif
					dwMemPos += 260;
				}
			}
		}
	}
	// Load Instruments
#ifdef MT2DEBUG
	Log("Loading instruments at offset 0x%08X\n", dwMemPos);
#endif
	memset(InstrMap, 0, sizeof(InstrMap));
	m_nInstruments = (pfh->wInstruments < MAX_INSTRUMENTS) ? pfh->wInstruments : MAX_INSTRUMENTS-1;
	m_dwSongFlags |= SONG_INSTRUMENTMODE;
	for (uint32_t iIns=1; iIns<=255; iIns++)
	{
		if (dwMemPos+36 > dwMemLength) return true;
		MT2INSTRUMENT *pmi = (MT2INSTRUMENT *)(lpStream+dwMemPos);
		INSTRUMENTHEADER *penv = NULL;
		if (iIns <= m_nInstruments)
		{
			penv = new INSTRUMENTHEADER;
			Headers[iIns] = penv;
			if (penv)
			{
				memset(penv, 0, sizeof(INSTRUMENTHEADER));
				memcpy(penv->name, pmi->szName, 32);
				penv->nGlobalVol = 128;
				penv->nPan = 128;
				for (uint32_t i=0; i<120; i++)
				{
					penv->NoteMap[i] = i+1;
				}
			}
		}
	#ifdef MT2DEBUG
		if (iIns <= pfh->wInstruments) Log("  Instrument #%d at offset %04X: %d bytes\n", iIns, dwMemPos, pmi->dwDataLen);
	#endif
		if (((int32_t)pmi->dwDataLen > 0) && (dwMemPos + pmi->dwDataLen + 40 <= dwMemLength))
		{
			InstrMap[iIns-1] = pmi;
			if (penv)
			{
				penv->nFadeOut = pmi->wFadeOut;
				penv->nNNA = pmi->wNNA & 3;
				penv->nDCT = (pmi->wNNA>>8) & 3;
				penv->nDNA = (pmi->wNNA>>12) & 3;
				MT2ENVELOPE *pehdr[4];
				uint16_t *pedata[4];
				if (pfh->wVersion <= 0x201)
				{
					uint32_t dwEnvPos = dwMemPos + sizeof(MT2INSTRUMENT) - 4;
					pehdr[0] = (MT2ENVELOPE *)(lpStream+dwEnvPos);
					pehdr[1] = (MT2ENVELOPE *)(lpStream+dwEnvPos+8);
					pehdr[2] = pehdr[3] = NULL;
					pedata[0] = (uint16_t *)(lpStream+dwEnvPos+16);
					pedata[1] = (uint16_t *)(lpStream+dwEnvPos+16+64);
					pedata[2] = pedata[3] = NULL;
				} else
				{
					uint32_t dwEnvPos = dwMemPos + sizeof(MT2INSTRUMENT);
					for (uint32_t i=0; i<4; i++)
					{
						if (pmi->wEnvFlags1 & (1<<i))
						{
							pehdr[i] = (MT2ENVELOPE *)(lpStream+dwEnvPos);
							pedata[i] = (uint16_t *)pehdr[i]->EnvData;
							dwEnvPos += sizeof(MT2ENVELOPE);
						} else
						{
							pehdr[i] = NULL;
							pedata[i] = NULL;
						}
					}
				}
				// Load envelopes
				for (uint32_t iEnv=0; iEnv<4; iEnv++) if (pehdr[iEnv])
				{
					MT2ENVELOPE *pme = pehdr[iEnv];
					int *pEnvPoints = NULL;
					uint8_t *pEnvData = NULL;
				#ifdef MT2DEBUG
					Log("  Env %d.%d @%04X: %d points\n", iIns, iEnv, (uint32_t)(((uint8_t *)pme)-lpStream), pme->nPoints);
				#endif
					switch(iEnv)
					{
					// Volume Envelope
					case 0:
						if (pme->nFlags & 1) penv->dwFlags |= ENV_VOLUME;
						if (pme->nFlags & 2) penv->dwFlags |= ENV_VOLSUSTAIN;
						if (pme->nFlags & 4) penv->dwFlags |= ENV_VOLLOOP;
						penv->VolEnv.nNodes = (pme->nPoints > 16) ? 16 : pme->nPoints;
						penv->VolEnv.nSustainStart = penv->VolEnv.nSustainEnd = pme->nSustainPos;
						penv->VolEnv.nLoopStart = pme->nLoopStart;
						penv->VolEnv.nLoopEnd = pme->nLoopEnd;
						pEnvPoints = penv->VolEnv.Ticks;
						pEnvData = penv->VolEnv.Values;
						break;

					// Panning Envelope
					case 1:
						if (pme->nFlags & 1) penv->dwFlags |= ENV_PANNING;
						if (pme->nFlags & 2) penv->dwFlags |= ENV_PANSUSTAIN;
						if (pme->nFlags & 4) penv->dwFlags |= ENV_PANLOOP;
						penv->PanEnv.nNodes = (pme->nPoints > 16) ? 16 : pme->nPoints;
						penv->PanEnv.nSustainStart = penv->PanEnv.nSustainEnd = pme->nSustainPos;
						penv->PanEnv.nLoopStart = pme->nLoopStart;
						penv->PanEnv.nLoopEnd = pme->nLoopEnd;
						pEnvPoints = penv->PanEnv.Ticks;
						pEnvData = penv->PanEnv.Values;
						break;

					// Pitch/Filter envelope
					default:
						if (pme->nFlags & 1) penv->dwFlags |= (iEnv==3) ? (ENV_PITCH|ENV_FILTER) : ENV_PITCH;
						if (pme->nFlags & 2) penv->dwFlags |= ENV_PITCHSUSTAIN;
						if (pme->nFlags & 4) penv->dwFlags |= ENV_PITCHLOOP;
						penv->PitchEnv.nNodes = (pme->nPoints > 16) ? 16 : pme->nPoints;
						penv->PitchEnv.nSustainStart = penv->PitchEnv.nSustainEnd = pme->nSustainPos;
						penv->PitchEnv.nLoopStart = pme->nLoopStart;
						penv->PitchEnv.nLoopEnd = pme->nLoopEnd;
						pEnvPoints = penv->PitchEnv.Ticks;
						pEnvData = penv->PitchEnv.Values;
					}
					// Envelope data
					if ((pEnvPoints) && (pEnvData) && (pedata[iEnv]))
					{
						uint16_t *psrc = pedata[iEnv];
						for (uint32_t i=0; i<16; i++)
						{
							pEnvPoints[i] = psrc[i*2];
							pEnvData[i] = (uint8_t)psrc[i*2+1];
						}
					}
				}
				// blah
				if (!(penv->dwFlags & ENV_VOLUME) && penv->nFadeOut < 256) {
					penv->nFadeOut = 8192;
				}
			}
			dwMemPos += pmi->dwDataLen + 36;
			if (pfh->wVersion > 0x201) dwMemPos += 4; // ?
		} else
		{
			dwMemPos += 36;
		}
	}
#ifdef MT2DEBUG
	Log("Loading samples at offset 0x%08X\n", dwMemPos);
#endif
	memset(SampleMap, 0, sizeof(SampleMap));
	m_nSamples = (pfh->wSamples < MAX_SAMPLES) ? pfh->wSamples : MAX_SAMPLES-1;
	for (uint32_t iSmp=1; iSmp<=256; iSmp++)
	{
		if (dwMemPos+36 > dwMemLength) return true;
		MT2SAMPLE *pms = (MT2SAMPLE *)(lpStream+dwMemPos);
	#ifdef MT2DEBUG
		if (iSmp <= m_nSamples) Log("  Sample #%d at offset %04X: %d bytes\n", iSmp, dwMemPos, pms->dwDataLen);
	#endif
		if (iSmp < MAX_SAMPLES)
		{
			memcpy(m_szNames[iSmp], pms->szName, 32);
		}
		if (pms->dwDataLen > 0)
		{
			SampleMap[iSmp-1] = pms;
			if (iSmp < MAX_SAMPLES)
			{
				MODINSTRUMENT *psmp = &Ins[iSmp];
				psmp->nGlobalVol = 64;
				psmp->nVolume = (pms->wVolume >> 7);
				psmp->nPan = (pms->nPan == 0x80) ? 128 : (pms->nPan^0x80);
				psmp->nLength = pms->dwLength;
				psmp->nLoopStart = pms->dwLoopStart;
				psmp->nLoopEnd = pms->dwLoopEnd;

				// this code is stupid
				int f2t = frequency_to_transpose(pms->dwFrequency);
				int transp = f2t >> 7;
				int ftune = f2t & 0x7F;
				if (ftune > 80)
				{
					transp++;
					ftune -= 128;
				}
				if (transp > 127) transp = 127;
				if (transp < -127) transp = -127;
				transp -= pms->nBaseNote - 49;
				psmp->nC5Speed = transpose_to_frequency(transp, ftune);

				if (pms->nQuality == 2) { psmp->uFlags |= CHN_16BIT; psmp->nLength >>= 1; }
				if (pms->nChannels == 2) { psmp->nLength >>= 1; }
				if (pms->nLoop == 1) psmp->uFlags |= CHN_LOOP;
				if (pms->nLoop == 2) psmp->uFlags |= CHN_LOOP|CHN_PINGPONGLOOP;
			}
			dwMemPos += pms->dwDataLen + 36;
		} else
		{
			dwMemPos += 36;
		}
	}
#ifdef MT2DEBUG
	Log("Loading groups at offset 0x%08X\n", dwMemPos);
#endif
	for (uint32_t iMap=0; iMap<255; iMap++) if (InstrMap[iMap])
	{
		if (dwMemPos+8 > dwMemLength) return true;
		MT2INSTRUMENT *pmi = InstrMap[iMap];
		INSTRUMENTHEADER *penv = NULL;
		if (iMap<m_nInstruments) penv = Headers[iMap+1];
		for (uint32_t iGrp=0; iGrp<pmi->wSamples; iGrp++)
		{
			if (penv)
			{
				MT2GROUP *pmg = (MT2GROUP *)(lpStream+dwMemPos);
				for (uint32_t i=0; i<96; i++)
				{
					if (pmi->GroupsMapping[i] == iGrp)
					{
						uint32_t nSmp = pmg->nSmpNo+1;
						penv->Keyboard[i+12] = (uint8_t)nSmp;
						if (nSmp <= m_nSamples)
						{
							Ins[nSmp].nVibType = pmi->bVibType;
							Ins[nSmp].nVibSweep = pmi->bVibSweep;
							Ins[nSmp].nVibDepth = pmi->bVibDepth;
							Ins[nSmp].nVibRate = pmi->bVibRate/4;
						}
					}
				}
			}
			dwMemPos += 8;
		}
	}
#ifdef MT2DEBUG
	Log("Loading sample data at offset 0x%08X\n", dwMemPos);
#endif
	for (uint32_t iData=0; iData<256; iData++) if ((iData < m_nSamples) && (SampleMap[iData]))
	{
		MT2SAMPLE *pms = SampleMap[iData];
		MODINSTRUMENT *psmp = &Ins[iData+1];
		if (!(pms->nFlags & 5))
		{
			if (psmp->nLength > 0) 
			{
			#ifdef MT2DEBUG
				Log("  Reading sample #%d at offset 0x%04X (len=%d)\n", iData+1, dwMemPos, psmp->nLength);
			#endif
				uint32_t rsflags;
				
				if (pms->nChannels == 2)
					rsflags = (psmp->uFlags & CHN_16BIT) ? RS_STPCM16D : RS_STPCM8D;
				else
					rsflags = (psmp->uFlags & CHN_16BIT) ? RS_PCM16D : RS_PCM8D;

				dwMemPos += ReadSample(psmp, rsflags, (const char *)(lpStream+dwMemPos), dwMemLength-dwMemPos);
			}
		} else
		if (dwMemPos+4 < dwMemLength)
		{
			uint32_t nNameLen = *(uint32_t *)(lpStream+dwMemPos);
			dwMemPos += nNameLen + 16;
		}
		if (dwMemPos+4 >= dwMemLength) break;
	}
	return true;
}
