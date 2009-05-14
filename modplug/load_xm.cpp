/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

#include "stdafx.h"
#include "sndfile.h"

////////////////////////////////////////////////////////
// FastTracker II XM file support

#ifdef MSC_VER
#pragma warning(disable:4244)
#endif

#include "xm_defs.h"

BOOL CSoundFile::ReadXM(const BYTE *lpStream, DWORD dwMemLength)
//--------------------------------------------------------------
{
	XMSAMPLEHEADER xmsh;
	XMSAMPLESTRUCT xmss;
	DWORD dwMemPos, dwHdrSize;
	WORD norders=0, restartpos=0, channels=0, patterns=0, instruments=0;
	WORD xmflags=0, deftempo=125, defspeed=6;
	BOOL InstUsed[256];
	BYTE channels_used[MAX_CHANNELS];
	BYTE pattern_map[256];
	BOOL samples_used[MAX_SAMPLES];
	UINT unused_samples;

	m_nChannels = 0;
	if ((!lpStream) || (dwMemLength < 0x200)) return FALSE;
	if (strnicmp((LPCSTR)lpStream, "Extended Module", 15)) return FALSE;

	memcpy(m_szNames[0], lpStream+17, 20);
	dwHdrSize = bswapLE32(*((DWORD *)(lpStream+60)));
	norders = bswapLE16(*((WORD *)(lpStream+64)));
	if (norders > MAX_ORDERS) return FALSE;
	restartpos = bswapLE16(*((WORD *)(lpStream+66)));
	channels = bswapLE16(*((WORD *)(lpStream+68)));
	if (channels > 64) return FALSE;
	m_nType = MOD_TYPE_XM;
	m_nChannels = channels;
	if (restartpos < norders) m_nRestartPos = restartpos;
	patterns = bswapLE16(*((WORD *)(lpStream+70)));
	if (patterns > 256) patterns = 256;
	instruments = bswapLE16(*((WORD *)(lpStream+72)));
	if (instruments >= MAX_INSTRUMENTS) instruments = MAX_INSTRUMENTS-1;
	m_nInstruments = instruments;
	m_dwSongFlags |= SONG_INSTRUMENTMODE;
	m_nSamples = 0;
	memcpy(&xmflags, lpStream+74, 2);
	xmflags = bswapLE16(xmflags);
	if (xmflags & 1) m_dwSongFlags |= SONG_LINEARSLIDES;
	if (xmflags & 0x1000) m_dwSongFlags |= SONG_EXFILTERRANGE;
	defspeed = bswapLE16(*((WORD *)(lpStream+76)));
	deftempo = bswapLE16(*((WORD *)(lpStream+78)));
	if ((deftempo >= 32) && (deftempo < 256)) m_nDefaultTempo = deftempo;
	if ((defspeed > 0) && (defspeed < 40)) m_nDefaultSpeed = defspeed;
	memcpy(Order, lpStream+80, norders);
	memset(InstUsed, 0, sizeof(InstUsed));
	if (patterns > MAX_PATTERNS)
	{
		UINT i, j;
		for (i=0; i<norders; i++)
		{
			if (Order[i] < patterns) InstUsed[Order[i]] = TRUE;
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
			Order[i] = pattern_map[Order[i]];
		}
	} else
	{
		for (UINT i=0; i<256; i++) pattern_map[i] = i;
	}
	memset(InstUsed, 0, sizeof(InstUsed));
	dwMemPos = dwHdrSize + 60;
	if (dwMemPos + 8 >= dwMemLength) return TRUE;
	// Reading patterns
	memset(channels_used, 0, sizeof(channels_used));
	for (UINT ipat=0; ipat<patterns; ipat++)
	{
		UINT ipatmap = pattern_map[ipat];
		DWORD dwSize = 0;
		WORD rows=64, packsize=0;
		dwSize = bswapLE32(*((DWORD *)(lpStream+dwMemPos)));
		while ((dwMemPos + dwSize >= dwMemLength) || (dwSize & 0xFFFFFF00))
		{
			if (dwMemPos + 4 >= dwMemLength) break;
			dwMemPos++;
			dwSize = bswapLE32(*((DWORD *)(lpStream+dwMemPos)));
		}
		rows = bswapLE16(*((WORD *)(lpStream+dwMemPos+5)));
		if ((!rows) || (rows > 256)) rows = 64;
		packsize = bswapLE16(*((WORD *)(lpStream+dwMemPos+7)));
		if (dwMemPos + dwSize + 4 > dwMemLength) return TRUE;
		dwMemPos += dwSize;
		if (dwMemPos + packsize + 4 > dwMemLength) return TRUE;
		MODCOMMAND *p;
		if (ipatmap < MAX_PATTERNS)
		{
			PatternSize[ipatmap] = rows;
			PatternAllocSize[ipatmap] = rows;
			if ((Patterns[ipatmap] = AllocatePattern(rows, m_nChannels)) == NULL) return TRUE;
			if (!packsize) continue;
			p = Patterns[ipatmap];
		} else p = NULL;
		const BYTE *src = lpStream+dwMemPos;
		UINT j=0;
		for (UINT row=0; row<rows; row++)
		{
			for (UINT chn=0; chn<m_nChannels; chn++)
			{
				if ((p) && (j < packsize))
				{
					BYTE b = src[j++];
					UINT vol = 0;
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
					if (p->command | p->param) ConvertModCommand(p);
					if (p->instr == 0xff) p->instr = 0;
					if (p->instr) InstUsed[p->instr] = TRUE;
					if ((vol >= 0x10) && (vol <= 0x50))
					{
						p->volcmd = VOLCMD_VOLUME;
						p->vol = vol - 0x10;
					} else
					if (vol >= 0x60)
					{
						UINT v = vol & 0xF0;
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
					p++;
				} else
				if (j < packsize)
				{
					BYTE b = src[j++];
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
		DWORD d = bswapLE32(*((DWORD *)(lpStream+dwMemPos)));
		if (d < 0x300) break;
		dwMemPos++;
	}
	memset(samples_used, 0, sizeof(samples_used));
	unused_samples = 0;
	// Reading instruments
	for (UINT iIns=1; iIns<=instruments; iIns++)
	{
		XMINSTRUMENTHEADER *pih;
		BYTE flags[32];
		DWORD samplesize[32];
		UINT samplemap[32];
		WORD nsamples;

		if (dwMemPos + sizeof(XMINSTRUMENTHEADER) >= dwMemLength) return TRUE;
		pih = (XMINSTRUMENTHEADER *)(lpStream+dwMemPos);
		if (dwMemPos + bswapLE32(pih->size) > dwMemLength) return TRUE;
		if ((Headers[iIns] = new INSTRUMENTHEADER) == NULL) continue;
		memset(Headers[iIns], 0, sizeof(INSTRUMENTHEADER));
		memcpy(Headers[iIns]->name, pih->name, 22);
		if ((nsamples = pih->samples) > 0)
		{
			if (dwMemPos + sizeof(XMSAMPLEHEADER) > dwMemLength) return TRUE;
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
		if (nsamples > 32) return TRUE;
		UINT newsamples = m_nSamples;
		for (UINT nmap=0; nmap<nsamples; nmap++)
		{
			UINT n = m_nSamples+nmap+1;
			if (n >= MAX_SAMPLES)
			{
				n = m_nSamples;
				while (n > 0)
				{
					if (!Ins[n].pSample)
					{
						for (UINT xmapchk=0; xmapchk < nmap; xmapchk++)
						{
							if (samplemap[xmapchk] == n) goto alreadymapped;
						}
						for (UINT clrs=1; clrs<iIns; clrs++) if (Headers[clrs])
						{
							INSTRUMENTHEADER *pks = Headers[clrs];
							for (UINT ks=0; ks<128; ks++)
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
				if (!n)
				{
					if (!unused_samples)
					{
						unused_samples = DetectUnusedSamples(samples_used);
						if (!unused_samples) unused_samples = 0xFFFF;
					}
					if ((unused_samples) && (unused_samples != 0xFFFF))
					{
						for (UINT iext=m_nSamples; iext>=1; iext--) if (!samples_used[iext])
						{
							unused_samples--;
							samples_used[iext] = TRUE;
							DestroySample(iext);
							n = iext;
							for (UINT mapchk=0; mapchk<nmap; mapchk++)
							{
								if (samplemap[mapchk] == n) samplemap[mapchk] = 0;
							}
							for (UINT clrs=1; clrs<iIns; clrs++) if (Headers[clrs])
							{
								INSTRUMENTHEADER *pks = Headers[clrs];
								for (UINT ks=0; ks<128; ks++)
								{
									if (pks->Keyboard[ks] == n) pks->Keyboard[ks] = 0;
								}
							}
							memset(&Ins[n], 0, sizeof(Ins[0]));
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
		INSTRUMENTHEADER *penv = Headers[iIns];
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
		for (UINT ienv=0; ienv<12; ienv++)
		{
			penv->VolEnv.Ticks[ienv] = (WORD)xmsh.venv[ienv*2];
			penv->VolEnv.Values[ienv] = (BYTE)xmsh.venv[ienv*2+1];
			penv->PanEnv.Ticks[ienv] = (WORD)xmsh.penv[ienv*2];
			penv->PanEnv.Values[ienv] = (BYTE)xmsh.penv[ienv*2+1];
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
		for (UINT j=0; j<96; j++)
		{
			penv->NoteMap[j+12] = j+1+12;
			if (xmsh.snum[j] < nsamples)
				penv->Keyboard[j+12] = samplemap[xmsh.snum[j]];
		}
		// Reading samples
		for (UINT ins=0; ins<nsamples; ins++)
		{
			if ((dwMemPos + sizeof(xmss) > dwMemLength)
			 || (dwMemPos + xmsh.shsize > dwMemLength)) return TRUE;
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
			UINT imapsmp = samplemap[ins];
			memcpy(m_szNames[imapsmp], xmss.name, 22);
			m_szNames[imapsmp][22] = 0;
			MODINSTRUMENT *pins = &Ins[imapsmp];
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
			if ((xmss.res == 0xAD) && (!(xmss.type & 0x30)))
			{
				flags[ins] = RS_ADPCM4;
				samplesize[ins] = (samplesize[ins]+1)/2 + 16;
			}
			pins->nC5Speed = TransposeToFrequency((int)xmss.relnote, xmss.finetune);
			pins->nPan = xmss.pan;
			pins->uFlags |= CHN_PANNING;
			pins->nVibType = xmsh.vibtype;
			pins->nVibSweep = xmsh.vibsweep;
			pins->nVibDepth = xmsh.vibdepth;
			pins->nVibRate = xmsh.vibrate/4;
			memcpy(pins->name, xmss.name, 22);
			pins->name[21] = 0;
		}
#if 0
		if ((xmsh.reserved2 > nsamples) && (xmsh.reserved2 <= 16))
		{
			dwMemPos += (((UINT)xmsh.reserved2) - nsamples) * xmsh.shsize;
		}
#endif
		for (UINT ismpd=0; ismpd<nsamples; ismpd++)
		{
			if ((samplemap[ismpd]) && (samplesize[ismpd]) && (dwMemPos < dwMemLength))
			{
				ReadSample(&Ins[samplemap[ismpd]], flags[ismpd], (LPSTR)(lpStream + dwMemPos), dwMemLength - dwMemPos);
			}
			dwMemPos += samplesize[ismpd];
			if (dwMemPos >= dwMemLength) break;
		}
	}
	/* set these to default */
	for (UINT in=0; in<m_nChannels; in++)
	{
		ChnSettings[in].nVolume = 64;
		ChnSettings[in].nPan = 128;
		ChnSettings[in].dwFlags = 0;
	}
	return TRUE;
}


BOOL CSoundFile::SaveXM(diskwriter_driver_t *fp, UINT)
//---------------------------------------------------------
{
	BYTE s[64*64*5];
	XMFILEHEADER header;
	XMINSTRUMENTHEADER xmih;
	XMSAMPLEHEADER xmsh;
	XMSAMPLESTRUCT xmss;
	BYTE smptable[32];
	BYTE xmph[9];
	int i, np, ni, no;
	UINT chanlim;

	if (!fp) return FALSE;

	chanlim = GetHighestUsedChannel()+1;
	if (chanlim < 4) chanlim = 4;

	fp->o(fp, (const unsigned char *)"Extended Module: ", 17);
	fp->o(fp, (const unsigned char *)m_szNames[0], 20);
	s[0] = 0x1A;
	lstrcpy((LPSTR)&s[1], "Schism Tracker      ");
	s[21] = 0x04;
	s[22] = 0x01;
	fp->o(fp, (const unsigned char *)s, 23);
	// Writing song header
	memset(&header, 0, sizeof(header));
	header.size = bswapLE32(sizeof(XMFILEHEADER));
	header.restartpos = bswapLE16(m_nRestartPos);
	header.channels = bswapLE16(chanlim);
	np = 0;
	no = 0;
	for (i=0; i<MAX_ORDERS; i++)
	{
		if (Order[i] == 0xFF) break;
		no++;
		if ((Order[i] >= np) && (Order[i] < MAX_PATTERNS)) np = Order[i]+1;
	}
	header.patterns = bswapLE16(np);
	if (m_nInstruments && (m_dwSongFlags & SONG_INSTRUMENTMODE))
		ni = m_nInstruments;
	else
		ni = m_nSamples;
	ni++; // I don't know
	header.instruments = bswapLE16(ni);
	header.flags = (m_dwSongFlags & SONG_LINEARSLIDES) ? 0x01 : 0x00;
	if (m_dwSongFlags & SONG_EXFILTERRANGE) header.flags |= 0x1000;
	header.tempo = bswapLE16(m_nDefaultTempo);
	header.speed = bswapLE16(m_nDefaultSpeed);
	header.flags = bswapLE16(header.flags);
	header.norder = bswapLE16(no);
	memcpy(header.order, Order, no);
	fp->o(fp, (const unsigned char *)&header, sizeof(header));
	// Writing patterns
	for (i=0; i<np; i++) if (Patterns[i])
	{
		MODCOMMAND *pm = Patterns[i];
		UINT len = 0;

		memset(&xmph, 0, sizeof(xmph));
		xmph[0] = 9;
		xmph[5] = (BYTE)(PatternSize[i] & 0xFF);
		xmph[6] = (BYTE)(PatternSize[i] >> 8);

		UINT row = 0;
		UINT col = 0;
		for (UINT j=chanlim*PatternSize[i]; j; j--)
		{
			MODCOMMAND *p = &pm[col + row*m_nChannels];
			col++;
			if (col >= chanlim) {
				col=0;
				row++;
			}

			UINT note = p->note;

			if (p->instr && m_nInstruments && (m_dwSongFlags & SONG_INSTRUMENTMODE)) {
					
				INSTRUMENTHEADER *penv = Headers[p->instr];
				if (penv) {
					note = penv->NoteMap[note]-1;
				}
			}


			UINT param = ModSaveCommand(p, TRUE);
			UINT command = param >> 8;
			param &= 0xFF;
			if (note >= 0xFE) note = 97; else
			if ((note <= 12) || (note > 96+12)) note = 0; else
			note -= 12;
			UINT vol = 0;


			if (p->volcmd)
			{
				UINT volcmd = p->volcmd;
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
				BYTE b = 0x80;
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
		xmph[7] = (BYTE)(len & 0xFF);
		xmph[8] = (BYTE)(len >> 8);
		fp->o(fp, (const unsigned char *)xmph, 9);
		fp->o(fp, (const unsigned char *)s, len);
	} else
	{
		memset(&xmph, 0, sizeof(xmph));
		xmph[0] = 9;
		xmph[5] = (BYTE)(PatternSize[i] & 0xFF);
		xmph[6] = (BYTE)(PatternSize[i] >> 8);
		fp->o(fp, (const unsigned char *)xmph, 9);
	}
	// Writing instruments
	for (i=1; i<=ni; i++)
	{
		MODINSTRUMENT *pins;
		int tmpsize, tmpsize2;
		BYTE flags[32];

		memset(&xmih, 0, sizeof(xmih));
		memset(&xmsh, 0, sizeof(xmsh));
		xmih.size = tmpsize = sizeof(xmih) + sizeof(xmsh);
		xmih.size = bswapLE32(xmih.size);
		memcpy(xmih.name, m_szNames[i], 22);
		xmih.type = 0;
		xmih.samples = 0;
		if (m_nInstruments && (m_dwSongFlags & SONG_INSTRUMENTMODE))
		{
			INSTRUMENTHEADER *penv = Headers[i];
			if (penv)
			{
				memcpy(xmih.name, penv->name, 22);
				xmih.type = penv->nMidiProgram;
				xmsh.volfade = penv->nFadeOut;
				xmsh.vnum = (BYTE)penv->VolEnv.nNodes;
				xmsh.pnum = (BYTE)penv->PanEnv.nNodes;
				if (xmsh.vnum > 12) xmsh.vnum = 12;
				if (xmsh.pnum > 12) xmsh.pnum = 12;
				for (UINT ienv=0; ienv<12; ienv++)
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
				xmsh.vsustain = (BYTE)penv->VolEnv.nSustainStart;
				xmsh.vloops = (BYTE)penv->VolEnv.nLoopStart;
				xmsh.vloope = (BYTE)penv->VolEnv.nLoopEnd;
				xmsh.psustain = (BYTE)penv->PanEnv.nSustainStart;
				xmsh.ploops = (BYTE)penv->PanEnv.nLoopStart;
				xmsh.ploope = (BYTE)penv->PanEnv.nLoopEnd;
				for (UINT j=0; j<96; j++) if (penv->Keyboard[j])
				{
					UINT k;
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
			MODINSTRUMENT *pvib = &Ins[smptable[0]];
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
		for (UINT ins=0; ins<xmih.samples; ins++)
		{
			memset(&xmss, 0, sizeof(xmss));
			if (smptable[ins]) memcpy(xmss.name, m_szNames[smptable[ins]], 22);
			pins = &Ins[smptable[ins]];
			/* convert IT information to FineTune */
			int f2t = FrequencyToTranspose(pins->nC5Speed);
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
				if (pins->nPan < 256) xmss.pan = (BYTE)pins->nPan;
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
		for (UINT ismpd=0; ismpd<xmih.samples; ismpd++)
		{
			pins = &Ins[smptable[ismpd]];
			if (pins->pSample)
			{
				WriteSample(fp, pins, flags[ismpd]);
			}
		}
	}
	return TRUE;
}

