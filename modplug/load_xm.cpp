/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

#include "sndfile.h"
#include "snd_fx.h"

////////////////////////////////////////////////////////
// FastTracker II XM file support

#include "xm_defs.h"


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
				case VOLCMD_VIBRATODEPTH:	vol = 0xB0 + (p->vol & 0x0F); break;
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
		uint32_t flags[32];

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
			flags[ins] = SF_LE | SF_PCMD;
			{
				if (pins->uFlags & CHN_16BIT)
				{
					flags[ins] |= SF_16;
					xmss.type |= 0x10;
					xmss.looplen *= 2;
					xmss.loopstart *= 2;
					xmss.samplen *= 2;
				} else {
					flags[ins] |= SF_8;
				}
				if (pins->uFlags & CHN_STEREO)
				{
					flags[ins] |= SF_SS;
					xmss.type |= 0x20;
					xmss.looplen *= 2;
					xmss.loopstart *= 2;
					xmss.samplen *= 2;
				} else {
					flags[ins] |= SF_M;
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

