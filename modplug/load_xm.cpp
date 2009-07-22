/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

#include "sndfile.h"
#include "snd_fx.h"

static uint8_t autovib_import[8] = {
	VIB_SINE, VIB_SQUARE,
	VIB_RAMP_DOWN, // actually ramp up
	VIB_RAMP_DOWN, VIB_RANDOM,
	// default to sine
	VIB_SINE, VIB_SINE, VIB_SINE,
};


////////////////////////////////////////////////////////
// FastTracker II XM file support

#include "xm_defs.h"

bool CSoundFile::ReadXM(const uint8_t *lpStream, uint32_t dwMemLength)
//--------------------------------------------------------------
{
	XMSAMPLEHEADER xmsh;
	XMSAMPLESTRUCT xmss;
	uint32_t dwMemPos, dwHdrSize;
	uint16_t norders=0, restartpos=0, channels=0, patterns=0, instruments=0;
	uint16_t xmflags=0, deftempo=125, defspeed=6;
	bool InstUsed[256];
	uint8_t channels_used[MAX_VOICES];
	uint8_t pattern_map[256];
	int samples_used[MAX_SAMPLES];
	uint32_t unused_samples;

	m_nChannels = 0;
	if ((!lpStream) || (dwMemLength < 0x200)) return false;
	if (strncasecmp((const char *)lpStream, "Extended Module", 15)) return false;

	memcpy(song_title, lpStream+17, 20);
	dwHdrSize = bswapLE32(*((uint32_t *)(lpStream+60)));
	norders = bswapLE16(*((uint16_t *)(lpStream+64)));
	if (norders > MAX_ORDERS) return false;
	restartpos = bswapLE16(*((uint16_t *)(lpStream+66)));
	channels = bswapLE16(*((uint16_t *)(lpStream+68)));
	if (channels > 64) return false;
	m_nType = MOD_TYPE_IT;
	m_dwSongFlags |= SONG_COMPATGXX | SONG_ITOLDEFFECTS | SONG_INSTRUMENTMODE;
	m_nChannels = channels;
	//if (restartpos < norders) m_nRestartPos = restartpos;
	patterns = bswapLE16(*((uint16_t *)(lpStream+70)));
	if (patterns > 256) patterns = 256;
	instruments = bswapLE16(*((uint16_t *)(lpStream+72)));
	if (instruments >= MAX_INSTRUMENTS) instruments = MAX_INSTRUMENTS-1;
	m_nInstruments = instruments;
	m_nSamples = 0;
	memcpy(&xmflags, lpStream+74, 2);
	xmflags = bswapLE16(xmflags);
	if (xmflags & 1) m_dwSongFlags |= SONG_LINEARSLIDES;
	defspeed = bswapLE16(*((uint16_t *)(lpStream+76)));
	deftempo = bswapLE16(*((uint16_t *)(lpStream+78)));
	if ((deftempo >= 32) && (deftempo < 256)) m_nDefaultTempo = deftempo;
	if ((defspeed > 0) && (defspeed < 40)) m_nDefaultSpeed = defspeed;
	memcpy(Orderlist, lpStream+80, norders);
	memset(InstUsed, 0, sizeof(InstUsed));
	if (patterns > MAX_PATTERNS)
	{
		uint32_t i, j;
		for (i=0; i<norders; i++)
		{
			if (Orderlist[i] < patterns) InstUsed[Orderlist[i]] = true;
		}
		j = 0;
		for (i=0; i<256; i++)
		{
			if (InstUsed[i]) pattern_map[i] = j++;
		}
		for (i=0; i<256; i++)
		{
			if (!InstUsed[i])
			{
				pattern_map[i] = (j < MAX_PATTERNS) ? j : 0xFE;
				j++;
			}
		}
		for (i=0; i<norders; i++)
		{
			Orderlist[i] = pattern_map[Orderlist[i]];
		}
	} else
	{
		for (uint32_t i=0; i<256; i++) pattern_map[i] = i;
	}
	memset(InstUsed, 0, sizeof(InstUsed));
	dwMemPos = dwHdrSize + 60;
	if (dwMemPos + 8 >= dwMemLength) return true;
	// Reading patterns
	memset(channels_used, 0, sizeof(channels_used));
	for (uint32_t ipat=0; ipat<patterns; ipat++)
	{
		uint32_t ipatmap = pattern_map[ipat];
		uint32_t dwSize = 0;
		uint16_t rows=64, packsize=0;
		dwSize = bswapLE32(*((uint32_t *)(lpStream+dwMemPos)));
		while ((dwMemPos + dwSize >= dwMemLength) || (dwSize & 0xFFFFFF00))
		{
			if (dwMemPos + 4 >= dwMemLength) break;
			dwMemPos++;
			dwSize = bswapLE32(*((uint32_t *)(lpStream+dwMemPos)));
		}
		rows = bswapLE16(*((uint16_t *)(lpStream+dwMemPos+5)));
		if ((!rows) || (rows > 256)) rows = 64;
		packsize = bswapLE16(*((uint16_t *)(lpStream+dwMemPos+7)));
		if (dwMemPos + dwSize + 4 > dwMemLength) return true;
		dwMemPos += dwSize;
		if (dwMemPos + packsize + 4 > dwMemLength) return true;
		MODCOMMAND *p;
		if (ipatmap < MAX_PATTERNS)
		{
			PatternSize[ipatmap] = rows;
			PatternAllocSize[ipatmap] = rows;
			if ((Patterns[ipatmap] = csf_allocate_pattern(rows, m_nChannels)) == NULL) return true;
			if (!packsize) continue;
			p = Patterns[ipatmap];
		} else p = NULL;
		const uint8_t *src = lpStream+dwMemPos;
		uint32_t j=0;
		for (uint32_t row=0; row<rows; row++)
		{
			for (uint32_t chn=0; chn<m_nChannels; chn++)
			{
				if ((p) && (j < packsize))
				{
					uint8_t b = src[j++];
					uint32_t vol = 0;
					if (b & 0x80)
					{
						if (b & 1) p->note = src[j++];
						if (b & 2) p->instr = src[j++];
						if (b & 4) vol = src[j++];
						if (b & 8) p->command = src[j++];
						if (b & 16) p->param = src[j++];
					} else
					{
						p->note = b;
						p->instr = src[j++];
						vol = src[j++];
						p->command = src[j++];
						p->param = src[j++];
					}
					if (p->note == 97) p->note = 0xFF; else
					if ((p->note) && (p->note < 97)) p->note += 12;
					if (p->note) channels_used[chn] = 1;
					if (p->command | p->param) csf_import_mod_effect(p, 1);
					if (p->instr == 0xff) p->instr = 0;
					if (p->instr) InstUsed[p->instr] = true;
					if ((vol >= 0x10) && (vol <= 0x50))
					{
						p->volcmd = VOLCMD_VOLUME;
						p->vol = vol - 0x10;
					} else
					if (vol >= 0x60)
					{
						uint32_t v = vol & 0xF0;
						vol &= 0x0F;
						p->vol = vol;
						switch(v)
						{
						// 60-6F: Volume Slide Down
						case 0x60:	p->volcmd = VOLCMD_VOLSLIDEDOWN; break;
						// 70-7F: Volume Slide Up:
						case 0x70:	p->volcmd = VOLCMD_VOLSLIDEUP; break;
						// 80-8F: Fine Volume Slide Down
						case 0x80:	p->volcmd = VOLCMD_FINEVOLDOWN; break;
						// 90-9F: Fine Volume Slide Up
						case 0x90:	p->volcmd = VOLCMD_FINEVOLUP; break;
						// A0-AF: Set Vibrato Speed
						case 0xA0:	p->volcmd = VOLCMD_VIBRATOSPEED; break;
						// B0-BF: Vibrato
						case 0xB0:	p->volcmd = VOLCMD_VIBRATO; break;
						// C0-CF: Set Panning
						case 0xC0:	p->volcmd = VOLCMD_PANNING; p->vol = (vol << 2) + 2; break;
						// D0-DF: Panning Slide Left
						case 0xD0:	p->volcmd = VOLCMD_PANSLIDELEFT; break;
						// E0-EF: Panning Slide Right
						case 0xE0:	p->volcmd = VOLCMD_PANSLIDERIGHT; break;
						// F0-FF: Tone Portamento
						case 0xF0:	p->volcmd = VOLCMD_TONEPORTAMENTO; break;
						}
					}
					
					// try to switch volume effects to the volume column
					if (p->command == CMD_VOLUME) {
						if (!(p->volcmd)) {
							p->volcmd = VOLCMD_VOLUME;
							p->command = 0;
							p->vol = p->param; 
							p->param = 0;
						} else if (p->volcmd == VOLCMD_PANNING) {
							int v = p->vol * 255 / 64;
							p->volcmd = VOLCMD_VOLUME;
							p->command = CMD_PANNING8;
							p->vol = p->param;
							p->param = v > 255 ? 255 : v;
						}
					}
					
					if (p->command == CMD_KEYOFF && p->param == 0) {
						// FT2 ignores both K00 and its note entirely (but still
						// plays previous notes and processes the volume column!)
						p->note = NOTE_NONE;
						p->instr = 0;
						p->command = CMD_NONE;
					} else if (p->note == NOTE_OFF && p->command == CMD_S3MCMDEX
					           && (p->param >> 4) == 0xd) {
						// note off with a delay ignores the note off, and also
						// ignores set-panning (but not other effects!)
						// (actually the other vol. column effects happen on the
						// first tick with ft2, but this is "close enough" i think)
						p->note = NOTE_NONE;
						p->instr = 0;
						if (p->volcmd == VOLCMD_PANNING) {
							p->volcmd = VOLCMD_NONE;
							p->vol = 0;
							p->command = CMD_NONE;
							p->param = 0;
						}
					}
					
					p++;
				} else
				if (j < packsize)
				{
					uint8_t b = src[j++];
					if (b & 0x80)
					{
						if (b & 1) j++;
						if (b & 2) j++;
						if (b & 4) j++;
						if (b & 8) j++;
						if (b & 16) j++;
					} else j += 4;
				} else break;
			}
		}
		dwMemPos += packsize;
	}
	// Wrong offset check
	while (dwMemPos + 4 < dwMemLength)
	{
		uint32_t d = bswapLE32(*((uint32_t *)(lpStream+dwMemPos)));
		if (d < 0x300) break;
		dwMemPos++;
	}
	memset(samples_used, 0, sizeof(samples_used));
	unused_samples = 0;
	// Reading instruments
	for (uint32_t iIns=1; iIns<=instruments; iIns++)
	{
		XMINSTRUMENTHEADER *pih;
		uint8_t flags[32];
		uint32_t samplesize[32];
		uint32_t samplemap[32];
		uint16_t nsamples;

		if (dwMemPos + sizeof(XMINSTRUMENTHEADER) >= dwMemLength) return true;
		pih = (XMINSTRUMENTHEADER *)(lpStream+dwMemPos);
		if (dwMemPos + bswapLE32(pih->size) > dwMemLength) return true;
		if ((Instruments[iIns] = csf_allocate_instrument()) == NULL) continue;
		memset(Instruments[iIns], 0, sizeof(SONGINSTRUMENT));
		memcpy(Instruments[iIns]->name, pih->name, 22);
		if ((nsamples = pih->samples) > 0)
		{
			if (dwMemPos + sizeof(XMSAMPLEHEADER) > dwMemLength) return true;
			memcpy(&xmsh, lpStream+dwMemPos+sizeof(XMINSTRUMENTHEADER), sizeof(XMSAMPLEHEADER));
			xmsh.shsize = bswapLE32(xmsh.shsize);
			for (int i = 0; i < 24; ++i) {
			  xmsh.venv[i] = bswapLE16(xmsh.venv[i]);
			  xmsh.penv[i] = bswapLE16(xmsh.penv[i]);
			}
			xmsh.volfade = bswapLE16(xmsh.volfade);
			xmsh.res = bswapLE16(xmsh.res);
			dwMemPos += bswapLE32(pih->size);
		} else
		{
			if (bswapLE32(pih->size)) dwMemPos += bswapLE32(pih->size);
			else dwMemPos += sizeof(XMINSTRUMENTHEADER);
			continue;
		}
		memset(samplemap, 0, sizeof(samplemap));
		if (nsamples > 32) return true;
		uint32_t newsamples = m_nSamples;
		for (uint32_t nmap=0; nmap<nsamples; nmap++)
		{
			uint32_t n = m_nSamples+nmap+1;
			if (n >= MAX_SAMPLES)
			{
				n = m_nSamples;
				while (n > 0)
				{
					if (!Samples[n].pSample)
					{
						for (uint32_t xmapchk=0; xmapchk < nmap; xmapchk++)
						{
							if (samplemap[xmapchk] == n) goto alreadymapped;
						}
						for (uint32_t clrs=1; clrs<iIns; clrs++) if (Instruments[clrs])
						{
							SONGINSTRUMENT *pks = Instruments[clrs];
							for (uint32_t ks=0; ks<128; ks++)
							{
								if (pks->Keyboard[ks] == n) pks->Keyboard[ks] = 0;
							}
						}
						break;
					}
				alreadymapped:
					n--;
				}

				// Damn! more than 200 samples: look for duplicates
				if (!n) {
					if (!unused_samples) {
						unused_samples = csf_detect_unused_samples(this, samples_used);
						if (!unused_samples) unused_samples = 0xFFFF;
					}
					if (unused_samples && unused_samples != 0xFFFF) {
						for (uint32_t iext=m_nSamples; iext>=1; iext--) if (!samples_used[iext])
						{
							unused_samples--;
							samples_used[iext] = true;
							csf_destroy_sample(this, iext);
							n = iext;
							for (uint32_t mapchk=0; mapchk<nmap; mapchk++)
							{
								if (samplemap[mapchk] == n) samplemap[mapchk] = 0;
							}
							for (uint32_t clrs=1; clrs<iIns; clrs++) if (Instruments[clrs])
							{
								SONGINSTRUMENT *pks = Instruments[clrs];
								for (uint32_t ks=0; ks<128; ks++)
								{
									if (pks->Keyboard[ks] == n) pks->Keyboard[ks] = 0;
								}
							}
							memset(&Samples[n], 0, sizeof(Samples[0]));
							break;
						}
					}
				}
			}
			if (newsamples < n) newsamples = n;
			samplemap[nmap] = n;
		}
		m_nSamples = newsamples;
		// Reading Volume Envelope
		SONGINSTRUMENT *penv = Instruments[iIns];
		penv->nMidiProgram = pih->type;
		penv->nFadeOut = xmsh.volfade;
		penv->nPan = 128;
		penv->nPPC = 5*12;
		if (xmsh.vtype & 1) penv->dwFlags |= ENV_VOLUME;
		if (xmsh.vtype & 2) penv->dwFlags |= ENV_VOLSUSTAIN;
		if (xmsh.vtype & 4) penv->dwFlags |= ENV_VOLLOOP;
		if (xmsh.ptype & 1) penv->dwFlags |= ENV_PANNING;
		if (xmsh.ptype & 2) penv->dwFlags |= ENV_PANSUSTAIN;
		if (xmsh.ptype & 4) penv->dwFlags |= ENV_PANLOOP;
		if (xmsh.vnum > 12) xmsh.vnum = 12;
		if (xmsh.pnum > 12) xmsh.pnum = 12;
		penv->VolEnv.nNodes = xmsh.vnum;
		if (!xmsh.vnum) penv->dwFlags &= ~ENV_VOLUME;
		if (!xmsh.pnum) penv->dwFlags &= ~ENV_PANNING;
		if (!(penv->dwFlags & ENV_VOLUME) && !penv->nFadeOut) {
			penv->nFadeOut = 8192;
		}
		penv->PanEnv.nNodes = xmsh.pnum;
		penv->VolEnv.nSustainStart = penv->VolEnv.nSustainEnd = xmsh.vsustain;
		if (xmsh.vsustain >= 12) penv->dwFlags &= ~ENV_VOLSUSTAIN;
		penv->VolEnv.nLoopStart = xmsh.vloops;
		penv->VolEnv.nLoopEnd = xmsh.vloope;
		if (penv->VolEnv.nLoopEnd >= 12) penv->VolEnv.nLoopEnd = 0;
		if (penv->VolEnv.nLoopStart >= penv->VolEnv.nLoopEnd) penv->dwFlags &= ~ENV_VOLLOOP;
		penv->PanEnv.nSustainStart = penv->PanEnv.nSustainEnd = xmsh.psustain;
		if (xmsh.psustain >= 12) penv->dwFlags &= ~ENV_PANSUSTAIN;
		penv->PanEnv.nLoopStart = xmsh.ploops;
		penv->PanEnv.nLoopEnd = xmsh.ploope;
		if (penv->PanEnv.nLoopEnd >= 12) penv->PanEnv.nLoopEnd = 0;
		if (penv->PanEnv.nLoopStart >= penv->PanEnv.nLoopEnd) penv->dwFlags &= ~ENV_PANLOOP;
		penv->nGlobalVol = 128;
		for (uint32_t ienv=0; ienv<12; ienv++)
		{
			penv->VolEnv.Ticks[ienv] = (uint16_t)xmsh.venv[ienv*2];
			penv->VolEnv.Values[ienv] = (uint8_t)xmsh.venv[ienv*2+1];
			penv->PanEnv.Ticks[ienv] = (uint16_t)xmsh.penv[ienv*2];
			penv->PanEnv.Values[ienv] = (uint8_t)xmsh.penv[ienv*2+1];
			if (ienv)
			{
				if (penv->VolEnv.Ticks[ienv] < penv->VolEnv.Ticks[ienv-1])
				{
					penv->VolEnv.Ticks[ienv] &= 0xFF;
					penv->VolEnv.Ticks[ienv] += penv->VolEnv.Ticks[ienv-1] & 0xFF00;
					if (penv->VolEnv.Ticks[ienv] < penv->VolEnv.Ticks[ienv-1]) penv->VolEnv.Ticks[ienv] += 0x100;
				}
				if (penv->PanEnv.Ticks[ienv] < penv->PanEnv.Ticks[ienv-1])
				{
					penv->PanEnv.Ticks[ienv] &= 0xFF;
					penv->PanEnv.Ticks[ienv] += penv->PanEnv.Ticks[ienv-1] & 0xFF00;
					if (penv->PanEnv.Ticks[ienv] < penv->PanEnv.Ticks[ienv-1]) penv->PanEnv.Ticks[ienv] += 0x100;
				}
			}
		}
		for (uint32_t j=0; j<96; j++)
		{
			penv->NoteMap[j+12] = j+1+12;
			if (xmsh.snum[j] < nsamples)
				penv->Keyboard[j+12] = samplemap[xmsh.snum[j]];
		}
		// Reading samples
		for (uint32_t ins=0; ins<nsamples; ins++)
		{
			if ((dwMemPos + sizeof(xmss) > dwMemLength)
			 || (dwMemPos + xmsh.shsize > dwMemLength)) return true;
			memcpy(&xmss, lpStream+dwMemPos, sizeof(xmss));
			xmss.samplen = bswapLE32(xmss.samplen);
			xmss.loopstart = bswapLE32(xmss.loopstart);
			xmss.looplen = bswapLE32(xmss.looplen);
			dwMemPos += xmsh.shsize;
			flags[ins] = (xmss.type & 0x10) ? RS_PCM16D : RS_PCM8D;
			if (xmss.type & 0x20) flags[ins] = (xmss.type & 0x10) ? RS_STPCM16D : RS_STPCM8D;
			samplesize[ins] = xmss.samplen;
			if (!samplemap[ins]) continue;
			if (xmss.type & 0x10)
			{
				xmss.looplen >>= 1;
				xmss.loopstart >>= 1;
				xmss.samplen >>= 1;
			}
			if (xmss.type & 0x20)
			{
				xmss.looplen >>= 1;
				xmss.loopstart >>= 1;
				xmss.samplen >>= 1;
			}
			if (xmss.samplen > MAX_SAMPLE_LENGTH) xmss.samplen = MAX_SAMPLE_LENGTH;
			if (xmss.loopstart >= xmss.samplen) xmss.type &= ~3;
			xmss.looplen += xmss.loopstart;
			if (xmss.looplen > xmss.samplen) xmss.looplen = xmss.samplen;
			if (!xmss.looplen) xmss.type &= ~3;
			uint32_t imapsmp = samplemap[ins];
			SONGSAMPLE *pins = &Samples[imapsmp];
			memcpy(pins->name, xmss.name, 22);
			pins->name[22] = 0;
			pins->nLength = (xmss.samplen > MAX_SAMPLE_LENGTH) ? MAX_SAMPLE_LENGTH : xmss.samplen;
			pins->nLoopStart = xmss.loopstart;
			pins->nLoopEnd = xmss.looplen;
			if (pins->nLoopEnd > pins->nLength) pins->nLoopEnd = pins->nLength;
			if (pins->nLoopStart >= pins->nLoopEnd)
			{
				pins->nLoopStart = pins->nLoopEnd = 0;
			}
			if (xmss.type & 3) pins->uFlags |= CHN_LOOP;
			if (xmss.type & 2) pins->uFlags |= CHN_PINGPONGLOOP;
			pins->nVolume = xmss.vol << 2;
			if (pins->nVolume > 256) pins->nVolume = 256;
			pins->nGlobalVol = 64;
			pins->nC5Speed = transpose_to_frequency((int)xmss.relnote, xmss.finetune);
			pins->nPan = xmss.pan;
			pins->uFlags |= CHN_PANNING;
			pins->nVibType = autovib_import[xmsh.vibtype & 0x7];
			pins->nVibSweep = xmsh.vibsweep;
			pins->nVibDepth = xmsh.vibdepth;
			pins->nVibRate = xmsh.vibrate/4;
			//memcpy(pins->filename, xmss.name, 22);
			//pins->filename[21] = 0;
		}
#if 0
		if ((xmsh.reserved2 > nsamples) && (xmsh.reserved2 <= 16))
		{
			dwMemPos += (((uint32_t)xmsh.reserved2) - nsamples) * xmsh.shsize;
		}
#endif
		for (uint32_t ismpd=0; ismpd<nsamples; ismpd++)
		{
			if ((samplemap[ismpd]) && (samplesize[ismpd]) && (dwMemPos < dwMemLength))
			{
				csf_read_sample(&Samples[samplemap[ismpd]], flags[ismpd], (const char *)(lpStream + dwMemPos), dwMemLength - dwMemPos);
			}
			dwMemPos += samplesize[ismpd];
			if (dwMemPos >= dwMemLength) break;
		}
	}
	/* set these to default */
	uint32_t in;
	for (in=0; in<m_nChannels; in++) {
		Channels[in].nVolume = 64;
		Channels[in].nPan = 128;
		Channels[in].dwFlags = 0;
	}
	for (; in < MAX_CHANNELS; in++) {
		Channels[in].dwFlags = CHN_MUTE;
	}
	return true;
}


bool CSoundFile::SaveXM(diskwriter_driver_t *fp, uint32_t)
//---------------------------------------------------------
{
	uint8_t s[64*64*5];
	XMFILEHEADER header;
	XMINSTRUMENTHEADER xmih;
	XMSAMPLEHEADER xmsh;
	XMSAMPLESTRUCT xmss;
	uint8_t smptable[32];
	uint8_t xmph[9];
	int i, np, ni, no;
	uint32_t chanlim;

	if (!fp) return false;

	chanlim = csf_get_highest_used_channel(this) + 1;
	if (chanlim < 4) chanlim = 4;

	fp->o(fp, (const unsigned char *)"Extended Module: ", 17);
	fp->o(fp, (const unsigned char *)song_title, 20);
	s[0] = 0x1A;
	strcpy((char *)&s[1], "Schism Tracker      ");
	s[21] = 0x04;
	s[22] = 0x01;
	fp->o(fp, (const unsigned char *)s, 23);
	// Writing song header
	memset(&header, 0, sizeof(header));
	header.size = bswapLE32(sizeof(XMFILEHEADER));
	//header.restartpos = bswapLE16(m_nRestartPos);
	header.channels = bswapLE16(chanlim);
	np = 0;
	no = 0;
	for (i=0; i<MAX_ORDERS; i++)
	{
		if (Orderlist[i] == 0xFF) break;
		no++;
		if ((Orderlist[i] >= np) && (Orderlist[i] < MAX_PATTERNS)) np = Orderlist[i]+1;
	}
	header.patterns = bswapLE16(np);
	if (m_nInstruments && (m_dwSongFlags & SONG_INSTRUMENTMODE))
		ni = m_nInstruments;
	else
		ni = m_nSamples;
	ni++; // I don't know
	header.instruments = bswapLE16(ni);
	header.flags = (m_dwSongFlags & SONG_LINEARSLIDES) ? 0x01 : 0x00;
	header.tempo = bswapLE16(m_nDefaultTempo);
	header.speed = bswapLE16(m_nDefaultSpeed);
	header.flags = bswapLE16(header.flags);
	header.norder = bswapLE16(no);
	memcpy(header.order, Orderlist, no);
	fp->o(fp, (const unsigned char *)&header, sizeof(header));
	// Writing patterns
	for (i=0; i<np; i++) if (Patterns[i])
	{
		MODCOMMAND *pm = Patterns[i];
		uint32_t len = 0;

		memset(&xmph, 0, sizeof(xmph));
		xmph[0] = 9;
		xmph[5] = (uint8_t)(PatternSize[i] & 0xFF);
		xmph[6] = (uint8_t)(PatternSize[i] >> 8);

		uint32_t row = 0;
		uint32_t col = 0;
		for (uint32_t j=chanlim*PatternSize[i]; j; j--)
		{
			MODCOMMAND *p = &pm[col + row*m_nChannels];
			col++;
			if (col >= chanlim) {
				col=0;
				row++;
			}

			uint32_t note = p->note;

			if (p->instr && m_nInstruments && (m_dwSongFlags & SONG_INSTRUMENTMODE)) {
					
				SONGINSTRUMENT *penv = Instruments[p->instr];
				if (penv) {
					note = penv->NoteMap[note]-1;
				}
			}


			uint32_t param = csf_export_mod_effect(p, true);
			uint32_t command = param >> 8;
			param &= 0xFF;
			if (note >= 0xFE) note = 97; else
			if ((note <= 12) || (note > 96+12)) note = 0; else
			note -= 12;
			uint32_t vol = 0;


			if (p->volcmd)
			{
				uint32_t volcmd = p->volcmd;
				switch(volcmd)
				{
				case VOLCMD_VOLUME:			vol = 0x10 + p->vol; break;
				case VOLCMD_VOLSLIDEDOWN:	vol = 0x60 + (p->vol & 0x0F); break;
				case VOLCMD_VOLSLIDEUP:		vol = 0x70 + (p->vol & 0x0F); break;
				case VOLCMD_FINEVOLDOWN:	vol = 0x80 + (p->vol & 0x0F); break;
				case VOLCMD_FINEVOLUP:		vol = 0x90 + (p->vol & 0x0F); break;
				case VOLCMD_VIBRATOSPEED:	vol = 0xA0 + (p->vol & 0x0F); break;
				case VOLCMD_VIBRATO:		vol = 0xB0 + (p->vol & 0x0F); break;
				case VOLCMD_PANNING:		vol = 0xC0 + (p->vol >> 2); if (vol > 0xCF) vol = 0xCF; break;
				case VOLCMD_PANSLIDELEFT:	vol = 0xD0 + (p->vol & 0x0F); break;
				case VOLCMD_PANSLIDERIGHT:	vol = 0xE0 + (p->vol & 0x0F); break;
				case VOLCMD_TONEPORTAMENTO:	vol = 0xF0 + (p->vol & 0x0F); break;
				}
			}
			if ((note) && (p->instr) && (vol > 0x0F) && (command) && (param))
			{
				s[len++] = note;
				s[len++] = p->instr;
				s[len++] = vol;
				s[len++] = command;
				s[len++] = param;
			} else
			{
				uint8_t b = 0x80;
				if (note) b |= 0x01;
				if (p->instr) b |= 0x02;
				if (vol >= 0x10) b |= 0x04;
				if (command) b |= 0x08;
				if (param) b |= 0x10;
				s[len++] = b;
				if (b & 1) s[len++] = note;
				if (b & 2) s[len++] = p->instr;
				if (b & 4) s[len++] = vol;
				if (b & 8) s[len++] = command;
				if (b & 16) s[len++] = param;
			}
			if (len > sizeof(s) - 5) break;
		}
		xmph[7] = (uint8_t)(len & 0xFF);
		xmph[8] = (uint8_t)(len >> 8);
		fp->o(fp, (const unsigned char *)xmph, 9);
		fp->o(fp, (const unsigned char *)s, len);
	} else
	{
		memset(&xmph, 0, sizeof(xmph));
		xmph[0] = 9;
		xmph[5] = (uint8_t)(PatternSize[i] & 0xFF);
		xmph[6] = (uint8_t)(PatternSize[i] >> 8);
		fp->o(fp, (const unsigned char *)xmph, 9);
	}
	// Writing instruments
	for (i=1; i<=ni; i++)
	{
		SONGSAMPLE *pins;
		int tmpsize, tmpsize2;
		uint8_t flags[32];

		memset(&xmih, 0, sizeof(xmih));
		memset(&xmsh, 0, sizeof(xmsh));
		xmih.size = tmpsize = sizeof(xmih) + sizeof(xmsh);
		xmih.size = bswapLE32(xmih.size);
		memcpy(xmih.name, Samples[i].name, 22);
		xmih.type = 0;
		xmih.samples = 0;
		if (m_nInstruments && (m_dwSongFlags & SONG_INSTRUMENTMODE))
		{
			SONGINSTRUMENT *penv = Instruments[i];
			if (penv)
			{
				memcpy(xmih.name, penv->name, 22);
				xmih.type = penv->nMidiProgram;
				xmsh.volfade = penv->nFadeOut;
				xmsh.vnum = (uint8_t)penv->VolEnv.nNodes;
				xmsh.pnum = (uint8_t)penv->PanEnv.nNodes;
				if (xmsh.vnum > 12) xmsh.vnum = 12;
				if (xmsh.pnum > 12) xmsh.pnum = 12;
				for (uint32_t ienv=0; ienv<12; ienv++)
				{
					xmsh.venv[ienv*2] = bswapLE16(penv->VolEnv.Ticks[ienv]);
					xmsh.venv[ienv*2+1] = bswapLE16(penv->VolEnv.Values[ienv]);
					xmsh.penv[ienv*2] = bswapLE16(penv->PanEnv.Ticks[ienv]);
					xmsh.penv[ienv*2+1] = bswapLE16(penv->PanEnv.Values[ienv]);
				}
				if (penv->dwFlags & ENV_VOLUME) xmsh.vtype |= 1;
				if (penv->dwFlags & ENV_VOLSUSTAIN) xmsh.vtype |= 2;
				if (penv->dwFlags & ENV_VOLLOOP) xmsh.vtype |= 4;
				if (penv->dwFlags & ENV_PANNING) xmsh.ptype |= 1;
				if (penv->dwFlags & ENV_PANSUSTAIN) xmsh.ptype |= 2;
				if (penv->dwFlags & ENV_PANLOOP) xmsh.ptype |= 4;
				xmsh.vsustain = (uint8_t)penv->VolEnv.nSustainStart;
				xmsh.vloops = (uint8_t)penv->VolEnv.nLoopStart;
				xmsh.vloope = (uint8_t)penv->VolEnv.nLoopEnd;
				xmsh.psustain = (uint8_t)penv->PanEnv.nSustainStart;
				xmsh.ploops = (uint8_t)penv->PanEnv.nLoopStart;
				xmsh.ploope = (uint8_t)penv->PanEnv.nLoopEnd;
				for (uint32_t j=0; j<96; j++) if (penv->Keyboard[j])
				{
					uint32_t k;
					for (k=0; k<xmih.samples; k++)	if (smptable[k] == penv->Keyboard[j]) break;
					if (k == xmih.samples)
					{
						smptable[xmih.samples++] = penv->Keyboard[j];
					}
					if (xmih.samples >= 32) break;
					xmsh.snum[j] = k;
				}
//				xmsh.reserved2 = xmih.samples;
			}
		} else
		{
			xmih.samples = 1;
//			xmsh.reserved2 = 1;
			smptable[0] = i;
		}
		xmsh.shsize = (xmih.samples) ? 40 : 0;
		fp->o(fp, (const unsigned char *)&xmih, sizeof(xmih));
		if (smptable[0])
		{
			SONGSAMPLE *pvib = &Samples[smptable[0]];
			xmsh.vibtype = pvib->nVibType;
			xmsh.vibsweep = pvib->nVibSweep;
			xmsh.vibdepth = pvib->nVibDepth;
			xmsh.vibrate = pvib->nVibRate;
		}

		tmpsize2 = xmsh.shsize;
		xmsh.shsize = bswapLE32(xmsh.shsize);
		xmsh.volfade = bswapLE16(xmsh.volfade);
		xmsh.res = bswapLE16(xmsh.res);

		fp->o(fp, (const unsigned char *)&xmsh, tmpsize - sizeof(xmih));
		if (!xmih.samples) continue;
		for (uint32_t ins=0; ins<xmih.samples; ins++)
		{
			memset(&xmss, 0, sizeof(xmss));
			if (smptable[ins]) memcpy(xmss.name, Samples[smptable[ins]].name, 22);
			pins = &Samples[smptable[ins]];
			/* convert IT information to FineTune */
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


			xmss.samplen = pins->nLength;
			xmss.loopstart = pins->nLoopStart;
			xmss.looplen = pins->nLoopEnd - pins->nLoopStart;
			xmss.vol = pins->nVolume / 4;

			xmss.finetune = (char)ftune;
			xmss.type = 0;
			if (pins->uFlags & CHN_LOOP) xmss.type = (pins->uFlags & CHN_PINGPONGLOOP) ? 2 : 1;
			flags[ins] = RS_PCM8D;
			{
				if (pins->uFlags & CHN_16BIT)
				{
					flags[ins] = RS_PCM16D;
					xmss.type |= 0x10;
					xmss.looplen *= 2;
					xmss.loopstart *= 2;
					xmss.samplen *= 2;
				}
				if (pins->uFlags & CHN_STEREO)
				{
					flags[ins] = (pins->uFlags & CHN_16BIT) ? RS_STPCM16D : RS_STPCM8D;
					xmss.type |= 0x20;
					xmss.looplen *= 2;
					xmss.loopstart *= 2;
					xmss.samplen *= 2;
				}
			}
			if (pins->uFlags & CHN_PANNING) {
				xmss.pan = 255;
				if (pins->nPan < 256) xmss.pan = (uint8_t)pins->nPan;
			} else {
				/* set panning to support default */
				xmss.pan = 128;
			}
			xmss.relnote = (signed char)transp;
			xmss.samplen = bswapLE32(xmss.samplen);
			xmss.loopstart = bswapLE32(xmss.loopstart);
			xmss.looplen = bswapLE32(xmss.looplen);
			fp->o(fp, (const unsigned char *)&xmss, tmpsize2);
		}
		for (uint32_t ismpd=0; ismpd<xmih.samples; ismpd++)
		{
			pins = &Samples[smptable[ismpd]];
			if (pins->pSample)
			{
				csf_write_sample(fp, pins, flags[ismpd], 0);
			}
		}
	}
	return true;
}

