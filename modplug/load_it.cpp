/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

#include "sndfile.h"
#include "it_defs.h"

/* blah, -mrsb.
this is a schism header */
#include "midi.h"

uint8_t autovibit2xm[8] = { 0, 3, 1, 4, 2, 0, 0, 0 };
uint8_t autovibxm2it[8] = { 0, 2, 4, 1, 3, 0, 0, 0 };

//////////////////////////////////////////////////////////
// Impulse Tracker IT file support (import only)



bool CSoundFile::ITInstrToMPT(const void *p, SONGINSTRUMENT *penv, uint32_t trkvers)
//--------------------------------------------------------------------------------
{
	if (trkvers < 0x0200)
	{
		const ITOLDINSTRUMENT *pis = (const ITOLDINSTRUMENT *)p;
		memcpy(penv->name, pis->name, 26);
		memcpy(penv->filename, pis->filename, 12);
		penv->nFadeOut = bswapLE16(pis->fadeout) << 6;
		penv->nGlobalVol = 128;
		for (uint32_t j=0; j<120; j++)
		{
			uint32_t note = pis->keyboard[j*2];
			uint32_t ins = pis->keyboard[j*2+1];
			if (ins < MAX_SAMPLES) penv->Keyboard[j] = ins;
			if (note < 128) penv->NoteMap[j] = note+1;
			else if (note >= 0xFE) penv->NoteMap[j] = note;
		}
		if (pis->flags & 0x01) penv->dwFlags |= ENV_VOLUME;
		if (pis->flags & 0x02) penv->dwFlags |= ENV_VOLLOOP;
		if (pis->flags & 0x04) penv->dwFlags |= ENV_VOLSUSTAIN;
		penv->VolEnv.nLoopStart = pis->vls;
		penv->VolEnv.nLoopEnd = pis->vle;
		penv->VolEnv.nSustainStart = pis->sls;
		penv->VolEnv.nSustainEnd = pis->sle;
		penv->VolEnv.nNodes = 25;
		for (uint32_t ev=0; ev<25; ev++)
		{
			if ((penv->VolEnv.Ticks[ev] = pis->nodes[ev*2]) == 0xFF)
			{
				penv->VolEnv.nNodes = ev;
				break;
			}
			penv->VolEnv.Values[ev] = pis->nodes[ev*2+1];
		}
		penv->nNNA = pis->nna;
		penv->nDCT = pis->dnc;
		penv->nPan = 0x80;
	} else
	{
		const ITINSTRUMENT *pis = (const ITINSTRUMENT *)p;
		memcpy(penv->name, pis->name, 26);
		memcpy(penv->filename, pis->filename, 12);
		penv->nMidiProgram = pis->mpr;
		penv->nMidiChannelMask = pis->mch > 16 ? (0x10000 + pis->mch)
		                       : pis->mch == 0 ? (0)
		                       :                 (1 << (pis->mch-1));
		penv->wMidiBank = bswapLE16(pis->mbank);
		penv->nFadeOut = bswapLE16(pis->fadeout) << 5;
		penv->nGlobalVol = pis->gbv;
		if (penv->nGlobalVol > 128) penv->nGlobalVol = 128;
		for (uint32_t j=0; j<120; j++)
		{
			uint32_t note = pis->keyboard[j*2];
			uint32_t ins = pis->keyboard[j*2+1];
			if (ins < MAX_SAMPLES) penv->Keyboard[j] = ins;
			if (note < 128) penv->NoteMap[j] = note+1;
			else if (note >= 0xFE) penv->NoteMap[j] = note;
		}
		// Volume Envelope
		if (pis->volenv.flags & 1) penv->dwFlags |= ENV_VOLUME;
		if (pis->volenv.flags & 2) penv->dwFlags |= ENV_VOLLOOP;
		if (pis->volenv.flags & 4) penv->dwFlags |= ENV_VOLSUSTAIN;
		if (pis->volenv.flags & 8) penv->dwFlags |= ENV_VOLCARRY;
		penv->VolEnv.nNodes = pis->volenv.num;
		if (penv->VolEnv.nNodes > 25) penv->VolEnv.nNodes = 25;

		penv->VolEnv.nLoopStart = pis->volenv.lpb;
		penv->VolEnv.nLoopEnd = pis->volenv.lpe;
		penv->VolEnv.nSustainStart = pis->volenv.slb;
		penv->VolEnv.nSustainEnd = pis->volenv.sle;
		// Panning Envelope
		if (pis->panenv.flags & 1) penv->dwFlags |= ENV_PANNING;
		if (pis->panenv.flags & 2) penv->dwFlags |= ENV_PANLOOP;
		if (pis->panenv.flags & 4) penv->dwFlags |= ENV_PANSUSTAIN;
		if (pis->panenv.flags & 8) penv->dwFlags |= ENV_PANCARRY;
		penv->PanEnv.nNodes = pis->panenv.num;
		if (penv->PanEnv.nNodes > 25) penv->PanEnv.nNodes = 25;
		penv->PanEnv.nLoopStart = pis->panenv.lpb;
		penv->PanEnv.nLoopEnd = pis->panenv.lpe;
		penv->PanEnv.nSustainStart = pis->panenv.slb;
		penv->PanEnv.nSustainEnd = pis->panenv.sle;
		// Pitch Envelope
		if (pis->pitchenv.flags & 1) penv->dwFlags |= ENV_PITCH;
		if (pis->pitchenv.flags & 2) penv->dwFlags |= ENV_PITCHLOOP;
		if (pis->pitchenv.flags & 4) penv->dwFlags |= ENV_PITCHSUSTAIN;
		if (pis->pitchenv.flags & 8) penv->dwFlags |= ENV_PITCHCARRY;
		if (pis->pitchenv.flags & 0x80) penv->dwFlags |= ENV_FILTER;
		penv->PitchEnv.nNodes = pis->pitchenv.num;
		if (penv->PitchEnv.nNodes > 25) penv->PitchEnv.nNodes = 25;
		penv->PitchEnv.nLoopStart = pis->pitchenv.lpb;
		penv->PitchEnv.nLoopEnd = pis->pitchenv.lpe;
		penv->PitchEnv.nSustainStart = pis->pitchenv.slb;
		penv->PitchEnv.nSustainEnd = pis->pitchenv.sle;
		// Envelopes Data
		for (uint32_t ev=0; ev<25; ev++)
		{
			penv->VolEnv.Values[ev] = pis->volenv.data[ev*3];
			penv->VolEnv.Ticks[ev] = (pis->volenv.data[ev*3+2] << 8) | (pis->volenv.data[ev*3+1]);
			penv->PanEnv.Values[ev] = pis->panenv.data[ev*3] + 32;
			penv->PanEnv.Ticks[ev] = (pis->panenv.data[ev*3+2] << 8) | (pis->panenv.data[ev*3+1]);
			penv->PitchEnv.Values[ev] = pis->pitchenv.data[ev*3] + 32;
			penv->PitchEnv.Ticks[ev] = (pis->pitchenv.data[ev*3+2] << 8) | (pis->pitchenv.data[ev*3+1]);
		}
		penv->nNNA = pis->nna % 4;
		penv->nDCT = pis->dct % 4;
		penv->nDNA = pis->dca % 3;
		penv->nPPS = pis->pps;
		penv->nPPC = pis->ppc;
		penv->nIFC = pis->ifc;
		penv->nIFR = pis->ifr;
		penv->nVolSwing = pis->rv;
		penv->nPanSwing = pis->rp;
		penv->nPan = (pis->dfp & 0x7F) << 2;
		if (penv->nPan > 256) penv->nPan = 128;
		if (pis->dfp < 0x80) penv->dwFlags |= ENV_SETPANNING;
	}
	if ((penv->VolEnv.nLoopStart >= 25) || (penv->VolEnv.nLoopEnd >= 25)) penv->dwFlags &= ~ENV_VOLLOOP;
	if ((penv->VolEnv.nSustainStart >= 25) || (penv->VolEnv.nSustainEnd >= 25)) penv->dwFlags &= ~ENV_VOLSUSTAIN;
	return true;
}


bool CSoundFile::ReadIT(const uint8_t *lpStream, uint32_t dwMemLength)
//--------------------------------------------------------------
{
	ITFILEHEADER pifh = *(ITFILEHEADER *)lpStream;
	uint32_t dwMemPos = sizeof(ITFILEHEADER);
	uint32_t inspos[MAX_INSTRUMENTS];
	uint32_t smppos[MAX_SAMPLES];
	uint32_t patpos[MAX_PATTERNS];
	uint8_t chnmask[64], channels_used[64];
	MODCOMMAND lastvalue[64];

	if ((!lpStream) || (dwMemLength < 0xc2)) return false;

	pifh.id = bswapLE32(pifh.id);
	if (pifh.id == 0x49504D49) {
		if (dwMemLength < 554) return false;

		uint16_t tv;
		SONGINSTRUMENT *zenv = new SONGINSTRUMENT;
		if (!zenv) return false;
		memset(zenv, 0, sizeof(SONGINSTRUMENT));
		memcpy(&tv, lpStream+0x1C, 2); /* trkvers */
		tv = bswapLE16(tv);
		if (!ITInstrToMPT(lpStream, zenv, tv)) {
			delete zenv;
			return false;
		}

		/* okay, we need samples now */
		unsigned int q = 554;
		uint8_t expect_samples = lpStream[0x1E];

		m_nType = MOD_TYPE_IT;
		m_nInstruments = 1;
		m_nSamples = expect_samples;
		m_dwSongFlags = SONG_INSTRUMENTMODE | SONG_LINEARSLIDES /* eh? */;

		memcpy(song_title, lpStream + 0x20, 26);
		song_title[26] = 0;

		if (q+(80*expect_samples) >= dwMemLength) {
			delete zenv;
			return false;
		}

		for (uint32_t nsmp = 0; nsmp < expect_samples; nsmp++) {

			ITSAMPLESTRUCT pis = *(ITSAMPLESTRUCT *)(lpStream+q);
			q += 80; /* length of ITS header */

			pis.id = bswapLE32(pis.id);
			pis.length = bswapLE32(pis.length);
			pis.loopbegin = bswapLE32(pis.loopbegin);
			pis.loopend = bswapLE32(pis.loopend);
			pis.C5Speed = bswapLE32(pis.C5Speed);
			pis.susloopbegin = bswapLE32(pis.susloopbegin);
			pis.susloopend = bswapLE32(pis.susloopend);
			pis.samplepointer = bswapLE32(pis.samplepointer);
	
			if (pis.id == 0x53504D49)
			{
				SONGSAMPLE *pins = &Samples[nsmp+1];
				memcpy(pins->filename, pis.filename, 12);
				pins->uFlags = 0;
				pins->nLength = 0;
				pins->nLoopStart = pis.loopbegin;
				pins->nLoopEnd = pis.loopend;
				pins->nSustainStart = pis.susloopbegin;
				pins->nSustainEnd = pis.susloopend;
				pins->nC5Speed = pis.C5Speed;
				if (!pins->nC5Speed) pins->nC5Speed = 8363;
				pins->nVolume = pis.vol << 2;
				if (pins->nVolume > 256) pins->nVolume = 256;
				pins->nGlobalVol = pis.gvl;
				if (pins->nGlobalVol > 64) pins->nGlobalVol = 64;
				if (pis.flags & 0x10) pins->uFlags |= CHN_LOOP;
				if (pis.flags & 0x20) pins->uFlags |= CHN_SUSTAINLOOP;
				if (pis.flags & 0x40) pins->uFlags |= CHN_PINGPONGLOOP;
				if (pis.flags & 0x80) pins->uFlags |= CHN_PINGPONGSUSTAIN;
				pins->nPan = (pis.dfp & 0x7F) << 2;
				if (pins->nPan > 256) pins->nPan = 256;
				if (pis.dfp & 0x80) pins->uFlags |= CHN_PANNING;
				pins->nVibType = autovibit2xm[pis.vit & 7];
				pins->nVibRate = pis.vis;
				pins->nVibDepth = pis.vid & 0x7F;
				pins->nVibSweep = pis.vir;
				if ((pis.samplepointer) && (pis.samplepointer < dwMemLength) && (pis.length))
				{
					pins->nLength = pis.length;
					if (pins->nLength > MAX_SAMPLE_LENGTH) pins->nLength = MAX_SAMPLE_LENGTH;
					uint32_t flags = (pis.cvt & 1) ? RS_PCM8S : RS_PCM8U;
					if (pis.flags & 2)
					{
						flags += 5;
						if (pis.flags & 4) flags |= RSF_STEREO;
						pins->uFlags |= CHN_16BIT;
						// IT 2.14 16-bit packed sample ?
						if (pis.flags & 8) flags = ((pifh.cmwt >= 0x215) && (pis.cvt & 4)) ? RS_IT21516 : RS_IT21416;
					} else
					{
						if (pis.flags & 4) flags |= RSF_STEREO;
						if (pis.cvt == 0xFF) flags = RS_ADPCM4; else
						// IT 2.14 8-bit packed sample ?
						if (pis.flags & 8)	flags =	((pifh.cmwt >= 0x215) && (pis.cvt & 4)) ? RS_IT2158 : RS_IT2148;
					}
					ReadSample(&Samples[nsmp+1], flags, (const char *)(lpStream+pis.samplepointer), dwMemLength - pis.samplepointer);
				}
			}
			memcpy(Samples[nsmp+1].name, pis.name, 26);
			
		}

		Instruments[1] = zenv;
		return true;
	}


	pifh.ordnum = bswapLE16(pifh.ordnum);
	pifh.insnum = bswapLE16(pifh.insnum);
	pifh.smpnum = bswapLE16(pifh.smpnum);
	pifh.patnum = bswapLE16(pifh.patnum);
	pifh.cwtv = bswapLE16(pifh.cwtv);
	pifh.cmwt = bswapLE16(pifh.cmwt);
	pifh.flags = bswapLE16(pifh.flags);
	pifh.special = bswapLE16(pifh.special);
	pifh.msglength = bswapLE16(pifh.msglength);
	pifh.msgoffset = bswapLE32(pifh.msgoffset);
	pifh.reserved2 = bswapLE32(pifh.reserved2);



	if ((pifh.id != 0x4D504D49) || (pifh.insnum >= MAX_INSTRUMENTS)
	 || (pifh.smpnum >= MAX_INSTRUMENTS)) return false;
	if (dwMemPos + pifh.ordnum + pifh.insnum*4
	 + pifh.smpnum*4 + pifh.patnum*4 > dwMemLength) return false;
	m_nType = MOD_TYPE_IT;
	if (!(pifh.flags & 0x01)) m_dwSongFlags |= SONG_NOSTEREO;
	if (pifh.flags & 0x04) m_dwSongFlags |= SONG_INSTRUMENTMODE;
	if (pifh.flags & 0x08) m_dwSongFlags |= SONG_LINEARSLIDES;
	if (pifh.flags & 0x10) m_dwSongFlags |= SONG_ITOLDEFFECTS;
	if (pifh.flags & 0x20) m_dwSongFlags |= SONG_ITCOMPATMODE;
	if (pifh.flags & 0x40) {
		midi_flags |= MIDI_PITCH_BEND;
		midi_pitch_depth = pifh.pwd;
	}
	if (pifh.flags & 0x80) m_dwSongFlags |= SONG_EMBEDMIDICFG;
	if (pifh.flags & 0x1000) m_dwSongFlags |= SONG_EXFILTERRANGE;
	memcpy(song_title, pifh.songname, 26);
	song_title[26] = 0;
	if (pifh.cwtv >= 0x0213) {
		m_rowHighlightMinor = pifh.hilight_minor;
		m_rowHighlightMajor = pifh.hilight_major;
	} else {
		m_rowHighlightMinor = 4;
		m_rowHighlightMajor = 16;
	}
	// Global Volume
        m_nDefaultGlobalVolume = pifh.globalvol << 1;
        if (m_nDefaultGlobalVolume > 256) m_nDefaultGlobalVolume = 256;
	if (pifh.speed) m_nDefaultSpeed = pifh.speed;
	if (pifh.tempo) m_nDefaultTempo = pifh.tempo;
	m_nSongPreAmp = pifh.mv;
        if (m_nSongPreAmp > 128)
                m_nSongPreAmp = 128;
	m_nStereoSeparation = pifh.sep;
	// Reading Channels Pan Positions
	for (int ipan=0; ipan<64; ipan++) if (pifh.chnpan[ipan] != 0xFF)
	{
		Channels[ipan].nVolume = pifh.chnvol[ipan];
		Channels[ipan].nPan = 128;
		if (pifh.chnpan[ipan] & 0x80) Channels[ipan].dwFlags |= CHN_MUTE;
		uint32_t n = pifh.chnpan[ipan] & 0x7F;
		if (n <= 64) Channels[ipan].nPan = n << 2;
		if (n == 100) Channels[ipan].dwFlags |= CHN_SURROUND;
	}
	if (m_nChannels < 4) m_nChannels = 4;
	// Reading Song Message
	if ((pifh.special & 0x01) && (pifh.msglength) && (pifh.msgoffset + pifh.msglength < dwMemLength))
	{
		m_lpszSongComments = new char[pifh.msglength+1];
		if (m_lpszSongComments)
		{
			memcpy(m_lpszSongComments, lpStream+pifh.msgoffset, pifh.msglength);
			m_lpszSongComments[pifh.msglength] = 0;
		}
	}
	// Reading orders
	uint32_t nordsize = pifh.ordnum;
	if (nordsize > MAX_ORDERS) nordsize = MAX_ORDERS;
	memcpy(Orderlist, lpStream+dwMemPos, nordsize);

	dwMemPos += pifh.ordnum;
	// Reading Instrument Offsets
	memset(inspos, 0, sizeof(inspos));
	uint32_t inspossize = pifh.insnum;
	if (inspossize > MAX_INSTRUMENTS) inspossize = MAX_INSTRUMENTS;
	inspossize <<= 2;
	memcpy(inspos, lpStream+dwMemPos, inspossize);
	for (uint32_t j=0; j < (inspossize>>2); j++)
	{
	       inspos[j] = bswapLE32(inspos[j]);
	}
	dwMemPos += pifh.insnum * 4;
	// Reading Samples Offsets
	memset(smppos, 0, sizeof(smppos));
	uint32_t smppossize = pifh.smpnum;
	if (smppossize > MAX_SAMPLES) smppossize = MAX_SAMPLES;
	smppossize <<= 2;
	memcpy(smppos, lpStream+dwMemPos, smppossize);
	for (uint32_t j=0; j < (smppossize>>2); j++)
	{
	       smppos[j] = bswapLE32(smppos[j]);
	}
	dwMemPos += pifh.smpnum * 4;
	// Reading Patterns Offsets
	memset(patpos, 0, sizeof(patpos));
	uint32_t patpossize = pifh.patnum;
	if (patpossize > MAX_PATTERNS) patpossize = MAX_PATTERNS;
	patpossize <<= 2;
	memcpy(patpos, lpStream+dwMemPos, patpossize);
	for (uint32_t j=0; j < (patpossize>>2); j++)
	{
	       patpos[j] = bswapLE32(patpos[j]);
	}
	dwMemPos += pifh.patnum * 4;

	for (uint32_t i = 0; i < pifh.ordnum; i++) {
		if (Orderlist[i] >= pifh.patnum && Orderlist[i] < MAX_PATTERNS) {
			pifh.patnum = Orderlist[i];
			for (uint32_t j = patpossize; j < (unsigned)(pifh.patnum>>2); j++)
				patpos[j] = 0;
			patpossize = pifh.patnum;
		}
	}


	// Reading IT Extra Info
	if (dwMemPos + 2 < dwMemLength)
	{
		uint32_t nflt = bswapLE16(*((uint16_t *)(lpStream + dwMemPos)));
		dwMemPos += 2;
		if (dwMemPos + nflt * 8 < dwMemLength) dwMemPos += nflt * 8;
	}
	// Reading Midi Output & Macros
	if (m_dwSongFlags & SONG_EMBEDMIDICFG)
	{
		if (dwMemPos + sizeof(MODMIDICFG) < dwMemLength)
		{
			memcpy(&m_MidiCfg, lpStream+dwMemPos, sizeof(MODMIDICFG));
			dwMemPos += sizeof(MODMIDICFG);
		} else {
			ResetMidiCfg();
		}
	} else {
		ResetMidiCfg();
	}
	// 4-channels minimum
	m_nChannels = 4;
	// Checking for unused channels
	uint32_t npatterns = pifh.patnum;
	if (npatterns > MAX_PATTERNS) npatterns = MAX_PATTERNS;
	for (uint32_t patchk=0; patchk<npatterns; patchk++)
	{
		memset(chnmask, 0, sizeof(chnmask));
		if ((!patpos[patchk]) || ((uint32_t)patpos[patchk] + 4 >= dwMemLength)) continue;
		uint32_t len = bswapLE16(*((uint16_t *)(lpStream+patpos[patchk])));
		uint32_t rows = bswapLE16(*((uint16_t *)(lpStream+patpos[patchk]+2)));
		if ((rows < 4) || (rows > 256)) continue;
		if (patpos[patchk]+8+len > dwMemLength) continue;
		uint32_t i = 0;
		const uint8_t *p = lpStream+patpos[patchk]+8;
		uint32_t nrow = 0;
		while (nrow<rows)
		{
			if (i >= len) break;
			uint8_t b = p[i++];
			if (!b)
			{
				nrow++;
				continue;
			}
			uint32_t ch = b & 0x7F;
			if (ch) ch = (ch - 1) & 0x3F;
			if (b & 0x80)
			{
				if (i >= len) break;
				chnmask[ch] = p[i++];
			}
			// Channel used
			if (chnmask[ch] & 0x0F)
			{
				if ((ch >= m_nChannels) && (ch < 64)) m_nChannels = ch+1;
			}
			// Note
			if (chnmask[ch] & 1) i++;
			// Instrument
			if (chnmask[ch] & 2) i++;
			// Volume
			if (chnmask[ch] & 4) i++;
			// Effect
			if (chnmask[ch] & 8) i += 2;
			if (i >= len) break;
		}
	}
	// Reading Instruments
	m_nInstruments = pifh.insnum;
	if (m_nInstruments >= MAX_INSTRUMENTS) m_nInstruments = MAX_INSTRUMENTS-1;
	for (uint32_t nins=0; nins<m_nInstruments; nins++)
	{
		if ((inspos[nins] > 0) && (inspos[nins] < dwMemLength - sizeof(ITOLDINSTRUMENT)))
		{
			SONGINSTRUMENT *penv = new SONGINSTRUMENT;
			if (!penv) continue;
			Instruments[nins+1] = penv;
			memset(penv, 0, sizeof(SONGINSTRUMENT));
			ITInstrToMPT(lpStream + inspos[nins], penv, pifh.cmwt);
		}
	}
	// Reading Samples
	m_nSamples = pifh.smpnum;
	if (m_nSamples >= MAX_SAMPLES) m_nSamples = MAX_SAMPLES-1;
	for (uint32_t nsmp=0; nsmp<pifh.smpnum; nsmp++) if ((smppos[nsmp]) && (smppos[nsmp] + sizeof(ITSAMPLESTRUCT) <= dwMemLength))
	{
		ITSAMPLESTRUCT pis = *(ITSAMPLESTRUCT *)(lpStream+smppos[nsmp]);
		pis.id = bswapLE32(pis.id);
		pis.length = bswapLE32(pis.length);
		pis.loopbegin = bswapLE32(pis.loopbegin);
		pis.loopend = bswapLE32(pis.loopend);
		pis.C5Speed = bswapLE32(pis.C5Speed);
		pis.susloopbegin = bswapLE32(pis.susloopbegin);
		pis.susloopend = bswapLE32(pis.susloopend);
		pis.samplepointer = bswapLE32(pis.samplepointer);

		if (pis.id == 0x53504D49)
		{
			SONGSAMPLE *pins = &Samples[nsmp+1];
			memcpy(pins->filename, pis.filename, 12);
			pins->uFlags = 0;
			pins->nLength = 0;
			pins->nLoopStart = pis.loopbegin;
			pins->nLoopEnd = pis.loopend;
			pins->nSustainStart = pis.susloopbegin;
			pins->nSustainEnd = pis.susloopend;
			pins->nC5Speed = pis.C5Speed;
			if (!pins->nC5Speed) pins->nC5Speed = 8363;
			pins->nVolume = pis.vol << 2;
			if (pins->nVolume > 256) pins->nVolume = 256;
			pins->nGlobalVol = pis.gvl;
			if (pins->nGlobalVol > 64) pins->nGlobalVol = 64;
			if (pis.flags & 0x10) pins->uFlags |= CHN_LOOP;
			if (pis.flags & 0x20) pins->uFlags |= CHN_SUSTAINLOOP;
			if (pis.flags & 0x40) pins->uFlags |= CHN_PINGPONGLOOP;
			if (pis.flags & 0x80) pins->uFlags |= CHN_PINGPONGSUSTAIN;
			pins->nPan = (pis.dfp & 0x7F) << 2;
			if (pins->nPan > 256) pins->nPan = 256;
			if (pis.dfp & 0x80) pins->uFlags |= CHN_PANNING;
			pins->nVibType = autovibit2xm[pis.vit & 7];
			pins->nVibRate = pis.vis;
			pins->nVibDepth = pis.vid & 0x7F;
			pins->nVibSweep = pis.vir;
			if ((pis.samplepointer) && (pis.samplepointer < dwMemLength) && (pis.length))
			{
				pins->nLength = pis.length;
				if (pins->nLength > MAX_SAMPLE_LENGTH) pins->nLength = MAX_SAMPLE_LENGTH;
				uint32_t flags = (pis.cvt & 1) ? RS_PCM8S : RS_PCM8U;
				if (pis.flags & 2)
				{
					flags += 5;
					if (pis.flags & 4) flags |= RSF_STEREO;
					pins->uFlags |= CHN_16BIT;
					// IT 2.14 16-bit packed sample ?
					if (pis.flags & 8) flags = ((pifh.cmwt >= 0x215) && (pis.cvt & 4)) ? RS_IT21516 : RS_IT21416;
				} else
				{
					if (pis.flags & 4) flags |= RSF_STEREO;
					if (pis.cvt == 0xFF) flags = RS_ADPCM4; else
					// IT 2.14 8-bit packed sample ?
					if (pis.flags & 8)	flags =	((pifh.cmwt >= 0x215) && (pis.cvt & 4)) ? RS_IT2158 : RS_IT2148;
				}
				ReadSample(&Samples[nsmp+1], flags, (const char *)(lpStream+pis.samplepointer), dwMemLength - pis.samplepointer);
			}
		}
		memcpy(Samples[nsmp+1].name, pis.name, 26);
	}
	// Reading Patterns
	for (uint32_t npat=0; npat<npatterns; npat++)
	{
		if ((!patpos[npat]) || ((uint32_t)patpos[npat] + 4 >= dwMemLength))
		{
			PatternSize[npat] = 64;
			PatternAllocSize[npat] = 64;
			Patterns[npat] = AllocatePattern(64, m_nChannels);
			continue;
		}

		uint32_t len = bswapLE16(*((uint16_t *)(lpStream+patpos[npat])));
		uint32_t rows = bswapLE16(*((uint16_t *)(lpStream+patpos[npat]+2)));
		if ((rows < 4) || (rows > 256)) continue;
		if (patpos[npat]+8+len > dwMemLength) continue;
		PatternSize[npat] = rows;
		PatternAllocSize[npat] = rows;
		if ((Patterns[npat] = AllocatePattern(rows, m_nChannels)) == NULL) continue;
		memset(lastvalue, 0, sizeof(lastvalue));
		memset(chnmask, 0, sizeof(chnmask));
		MODCOMMAND *m = Patterns[npat];
		uint32_t i = 0;
		const uint8_t *p = lpStream+patpos[npat]+8;
		uint32_t nrow = 0;
		while (nrow<rows)
		{
			if (i >= len) break;
			uint8_t b = p[i++];
			if (!b)
			{
				nrow++;
				m+=m_nChannels;
				continue;
			}
			uint32_t ch = b & 0x7F;
			if (ch) ch = (ch - 1) & 0x3F;
			if (b & 0x80)
			{
				if (i >= len) break;
				chnmask[ch] = p[i++];
			}
			if ((chnmask[ch] & 0x10) && (ch < m_nChannels))
			{
				m[ch].note = lastvalue[ch].note;
			}
			if ((chnmask[ch] & 0x20) && (ch < m_nChannels))
			{
				m[ch].instr = lastvalue[ch].instr;
			}
			if ((chnmask[ch] & 0x40) && (ch < m_nChannels))
			{
				m[ch].volcmd = lastvalue[ch].volcmd;
				m[ch].vol = lastvalue[ch].vol;
			}
			if ((chnmask[ch] & 0x80) && (ch < m_nChannels))
			{
				m[ch].command = lastvalue[ch].command;
				m[ch].param = lastvalue[ch].param;
			}
			if (chnmask[ch] & 1)	// Note
			{
				if (i >= len) break;
				uint32_t note = p[i++];
				if (ch < m_nChannels)
				{
					if (note < 0x80) note++;
					m[ch].note = note;
					lastvalue[ch].note = note;
					channels_used[ch] = true;
				}
			}
			if (chnmask[ch] & 2)
			{
				if (i >= len) break;
				uint32_t instr = p[i++];
				if (ch < m_nChannels)
				{
					m[ch].instr = instr;
					lastvalue[ch].instr = instr;
				}
			}
			if (chnmask[ch] & 4)
			{
				if (i >= len) break;
				uint32_t vol = p[i++];
				if (ch < m_nChannels)
				{
					// 0-64: Set Volume
					if (vol <= 64) { m[ch].volcmd = VOLCMD_VOLUME; m[ch].vol = vol; } else
					// 128-192: Set Panning
					if ((vol >= 128) && (vol <= 192)) { m[ch].volcmd = VOLCMD_PANNING; m[ch].vol = vol - 128; } else
					// 65-74: Fine Volume Up
					if (vol < 75) { m[ch].volcmd = VOLCMD_FINEVOLUP; m[ch].vol = vol - 65; } else
					// 75-84: Fine Volume Down
					if (vol < 85) { m[ch].volcmd = VOLCMD_FINEVOLDOWN; m[ch].vol = vol - 75; } else
					// 85-94: Volume Slide Up
					if (vol < 95) { m[ch].volcmd = VOLCMD_VOLSLIDEUP; m[ch].vol = vol - 85; } else
					// 95-104: Volume Slide Down
					if (vol < 105) { m[ch].volcmd = VOLCMD_VOLSLIDEDOWN; m[ch].vol = vol - 95; } else
					// 105-114: Pitch Slide Up
					if (vol < 115) { m[ch].volcmd = VOLCMD_PORTADOWN; m[ch].vol = vol - 105; } else
					// 115-124: Pitch Slide Down
					if (vol < 125) { m[ch].volcmd = VOLCMD_PORTAUP; m[ch].vol = vol - 115; } else
					// 193-202: Portamento To
					if ((vol >= 193) && (vol <= 202)) { m[ch].volcmd = VOLCMD_TONEPORTAMENTO; m[ch].vol = vol - 193; } else
					// 203-212: Vibrato
					if ((vol >= 203) && (vol <= 212)) { m[ch].volcmd = VOLCMD_VIBRATO; m[ch].vol = vol - 203; }
					lastvalue[ch].volcmd = m[ch].volcmd;
					lastvalue[ch].vol = m[ch].vol;
				}
			}
			// Reading command/param
			if (chnmask[ch] & 8)
			{
				if (i > len - 2) break;
				uint32_t cmd = p[i++];
				uint32_t param = p[i++];
				if (ch < m_nChannels)
				{
					if (cmd)
					{
						m[ch].command = cmd;
						m[ch].param = param;
						S3MConvert(&m[ch], true);
						lastvalue[ch].command = m[ch].command;
						lastvalue[ch].param = m[ch].param;
					}
				}
			}
		}
	}
	for (uint32_t ncu=0; ncu<MAX_CHANNELS; ncu++)
	{
		if (ncu>=m_nChannels)
		{
			Channels[ncu].nVolume = 64;
			Channels[ncu].dwFlags &= ~CHN_MUTE;
		}
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////////
// IT 2.14 compression

uint32_t ITReadBits(uint32_t &bitbuf, uint32_t &bitnum, uint8_t * &ibuf, int8_t n)
//-----------------------------------------------------------------
{
	uint32_t retval = 0;
	uint32_t i = n;

	if (n > 0)
	{
		do
		{
			if (!bitnum)
			{
				bitbuf = *ibuf++;
				bitnum = 8;
			}
			retval >>= 1;
			retval |= bitbuf << 31;
			bitbuf >>= 1;
			bitnum--;
			i--;
		} while (i);
		i = n;
	}
	return (retval >> (32-i));
}

void ITUnpack8Bit(signed char *pSample, uint32_t dwLen, uint8_t * lpMemFile, uint32_t dwMemLength, bool b215)
//-------------------------------------------------------------------------------------------
{
	signed char *pDst = pSample;
	uint8_t * pSrc = lpMemFile;
	uint32_t wHdr = 0;
	uint32_t wCount = 0;
	uint32_t bitbuf = 0;
	uint32_t bitnum = 0;
	uint8_t bLeft = 0, bTemp = 0, bTemp2 = 0;

	while (dwLen)
	{
		if (!wCount)
		{
			wCount = 0x8000;
			wHdr = bswapLE16(*((uint32_t *)pSrc));
			pSrc += 2;
			bLeft = 9;
			bTemp = bTemp2 = 0;
			bitbuf = bitnum = 0;
		}
		uint32_t d = wCount;
		if (d > dwLen) d = dwLen;
		// Unpacking
		uint32_t dwPos = 0;
		do
		{
			uint16_t wBits = (uint16_t)ITReadBits(bitbuf, bitnum, pSrc, bLeft);
			if (bLeft < 7)
			{
				uint32_t i = 1 << (bLeft-1);
				uint32_t j = wBits & 0xFFFF;
				if (i != j) goto UnpackByte;
				wBits = (uint16_t)(ITReadBits(bitbuf, bitnum, pSrc, 3) + 1) & 0xFF;
				bLeft = ((uint8_t)wBits < bLeft) ? (uint8_t)wBits : (uint8_t)((wBits+1) & 0xFF);
				goto Next;
			}
			if (bLeft < 9)
			{
				uint16_t i = (0xFF >> (9 - bLeft)) + 4;
				uint16_t j = i - 8;
				if ((wBits <= j) || (wBits > i)) goto UnpackByte;
				wBits -= j;
				bLeft = ((uint8_t)(wBits & 0xFF) < bLeft) ? (uint8_t)(wBits & 0xFF) : (uint8_t)((wBits+1) & 0xFF);
				goto Next;
			}
			if (bLeft >= 10) goto SkipByte;
			if (wBits >= 256)
			{
				bLeft = (uint8_t)(wBits + 1) & 0xFF;
				goto Next;
			}
		UnpackByte:
			if (bLeft < 8)
			{
				uint8_t shift = 8 - bLeft;
				signed char c = (signed char)(wBits << shift);
				c >>= shift;
				wBits = (uint16_t)c;
			}
			wBits += bTemp;
			bTemp = (uint8_t)wBits;
			bTemp2 += bTemp;
			pDst[dwPos] = (b215) ? bTemp2 : bTemp;
		SkipByte:
			dwPos++;
		Next:
			if (pSrc >= lpMemFile+dwMemLength+1) return;
		} while (dwPos < d);
		// Move On
		wCount -= d;
		dwLen -= d;
		pDst += d;
	}
}


void ITUnpack16Bit(signed char *pSample, uint32_t dwLen, uint8_t * lpMemFile, uint32_t dwMemLength, bool b215)
//--------------------------------------------------------------------------------------------
{
	signed short *pDst = (signed short *)pSample;
	uint8_t * pSrc = lpMemFile;
	uint32_t wHdr = 0;
	uint32_t wCount = 0;
	uint32_t bitbuf = 0;
	uint32_t bitnum = 0;
	uint8_t bLeft = 0;
	signed short wTemp = 0, wTemp2 = 0;

	while (dwLen)
	{
		if (!wCount)
		{
			wCount = 0x4000;
			wHdr = bswapLE16(*((uint32_t *)pSrc));
			pSrc += 2;
			bLeft = 17;
			wTemp = wTemp2 = 0;
			bitbuf = bitnum = 0;
		}
		uint32_t d = wCount;
		if (d > dwLen) d = dwLen;
		// Unpacking
		uint32_t dwPos = 0;
		do
		{
			uint32_t dwBits = ITReadBits(bitbuf, bitnum, pSrc, bLeft);
			if (bLeft < 7)
			{
				uint32_t i = 1 << (bLeft-1);
				uint32_t j = dwBits;
				if (i != j) goto UnpackByte;
				dwBits = ITReadBits(bitbuf, bitnum, pSrc, 4) + 1;
				bLeft = ((uint8_t)(dwBits & 0xFF) < bLeft) ? (uint8_t)(dwBits & 0xFF) : (uint8_t)((dwBits+1) & 0xFF);
				goto Next;
			}
			if (bLeft < 17)
			{
				uint32_t i = (0xFFFF >> (17 - bLeft)) + 8;
				uint32_t j = (i - 16) & 0xFFFF;
				if ((dwBits <= j) || (dwBits > (i & 0xFFFF))) goto UnpackByte;
				dwBits -= j;
				bLeft = ((uint8_t)(dwBits & 0xFF) < bLeft) ? (uint8_t)(dwBits & 0xFF) : (uint8_t)((dwBits+1) & 0xFF);
				goto Next;
			}
			if (bLeft >= 18) goto SkipByte;
			if (dwBits >= 0x10000)
			{
				bLeft = (uint8_t)(dwBits + 1) & 0xFF;
				goto Next;
			}
		UnpackByte:
			if (bLeft < 16)
			{
				uint8_t shift = 16 - bLeft;
				signed short c = (signed short)(dwBits << shift);
				c >>= shift;
				dwBits = (uint32_t)c;
			}
			dwBits += wTemp;
			wTemp = (signed short)dwBits;
			wTemp2 += wTemp;
			pDst[dwPos] = (b215) ? wTemp2 : wTemp;
		SkipByte:
			dwPos++;
		Next:
			if (pSrc >= lpMemFile+dwMemLength+1) return;
		} while (dwPos < d);
		// Move On
		wCount -= d;
		dwLen -= d;
		pDst += d;
		if (pSrc >= lpMemFile+dwMemLength) break;
	}
}

