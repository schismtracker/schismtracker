/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/

#include "sndfile.h"

#include "snd_fm.h"
#include "snd_gm.h"
#include "snd_flt.h"

#define CLAMP(a,y,z) ((a) < (y) ? (y) : ((a) > (z) ? (z) : (a)))


////////////////////////////////////////////////////////////
// Channels effects

void fx_note_cut(CSoundFile *csf, uint32_t nChn, uint32_t nTick)
{
	if ((csf->m_nMusicSpeed - csf->m_nTickCount) == nTick) {
		SONGVOICE *pChn = &csf->Voices[nChn];
		// if (m_dwSongFlags & SONG_INSTRUMENTMODE) KeyOff(pChn); ?
		pChn->nVolume = 0;
		pChn->dwFlags |= CHN_FASTVOLRAMP;
		pChn->nLength = 0;

		OPL_NoteOff(nChn);
		OPL_Touch(nChn, 0);
		GM_KeyOff(nChn);
		GM_Touch(nChn, 0);
	}
}

void fx_key_off(CSoundFile *csf, uint32_t nChn)
{
	SONGVOICE *pChn = &csf->Voices[nChn];
	bool bKeyOn = (pChn->dwFlags & CHN_KEYOFF) ? false : true;

	/*fprintf(stderr, "KeyOff[%d] [ch%u]: flags=0x%X\n",
		m_nTickCount, (unsigned)nChn, pChn->dwFlags);*/
	OPL_NoteOff(nChn);
	GM_KeyOff(nChn);

	SONGINSTRUMENT *penv = (csf->m_dwSongFlags & SONG_INSTRUMENTMODE) ? pChn->pHeader : NULL;
	
	/*if ((pChn->dwFlags & CHN_ADLIB)
	||  (penv && penv->nMidiChannelMask))
	{
		// When in AdLib / MIDI mode, end the sample
		pChn->dwFlags |= CHN_FASTVOLRAMP;
		pChn->nLength = 0;
		pChn->nPos    = 0;
		return;
	}*/

	pChn->dwFlags |= CHN_KEYOFF;
	//if ((!pChn->pHeader) || (!(pChn->dwFlags & CHN_VOLENV)))
	if ((csf->m_dwSongFlags & SONG_INSTRUMENTMODE) && pChn->pHeader && !(pChn->dwFlags & CHN_VOLENV)) {
		pChn->dwFlags |= CHN_NOTEFADE;
	}
	if (!pChn->nLength)
		return;
	if ((pChn->dwFlags & CHN_SUSTAINLOOP) && pChn->pInstrument && bKeyOn) {
		SONGSAMPLE *psmp = pChn->pInstrument;
		if (psmp->uFlags & CHN_LOOP) {
			if (psmp->uFlags & CHN_PINGPONGLOOP)
				pChn->dwFlags |= CHN_PINGPONGLOOP;
			else
				pChn->dwFlags &= ~(CHN_PINGPONGLOOP|CHN_PINGPONGFLAG);
			pChn->dwFlags |= CHN_LOOP;
			pChn->nLength = psmp->nLength;
			pChn->nLoopStart = psmp->nLoopStart;
			pChn->nLoopEnd = psmp->nLoopEnd;
			if (pChn->nLength > pChn->nLoopEnd) pChn->nLength = pChn->nLoopEnd;
		} else {
			pChn->dwFlags &= ~(CHN_LOOP|CHN_PINGPONGLOOP|CHN_PINGPONGFLAG);
			pChn->nLength = psmp->nLength;
		}
	}
	if (penv && penv->nFadeOut && (penv->dwFlags & ENV_VOLLOOP))
		pChn->dwFlags |= CHN_NOTEFADE;
}


static void fx_do_freq_slide(uint32_t flags, SONGVOICE *pChn, int32_t nFreqSlide)
{
	// IT Linear slides
	if (!pChn->nPeriod) return;
	if (flags & SONG_LINEARSLIDES) {
		if (nFreqSlide < 0) {
			uint32_t n = (-nFreqSlide) >> 2;
			if (n > 255)
				n = 255;
			pChn->nPeriod = _muldivr(pChn->nPeriod, LinearSlideDownTable[n], 65536);
		} else {
			uint32_t n = (nFreqSlide) >> 2;
			if (n > 255)
				n = 255;
			pChn->nPeriod = _muldivr(pChn->nPeriod, LinearSlideUpTable[n], 65536);
		}
	} else {
		pChn->nPeriod += nFreqSlide;
	}
}

static void fx_fine_portamento_up(uint32_t flags, SONGVOICE *pChn, uint32_t param)
{
	if ((flags & SONG_FIRSTTICK) && pChn->nPeriod && param) {
		if (flags & SONG_LINEARSLIDES) {
			pChn->nPeriod = _muldivr(pChn->nPeriod, LinearSlideDownTable[param & 0x0F], 65536);
		} else {
			pChn->nPeriod -= (int)(param * 4);
		}
	}
}

static void fx_fine_portamento_down(uint32_t flags, SONGVOICE *pChn, uint32_t param)
{
	if ((flags & SONG_FIRSTTICK) && pChn->nPeriod && param) {
		if (flags & SONG_LINEARSLIDES) {
			pChn->nPeriod = _muldivr(pChn->nPeriod, LinearSlideUpTable[param & 0x0F], 65536);
		} else {
			pChn->nPeriod += (int)(param * 4);
		}
	}
}

static void fx_extra_fine_portamento_up(uint32_t flags, SONGVOICE *pChn, uint32_t param)
{
	if ((flags & SONG_FIRSTTICK) && pChn->nPeriod && param) {
		if (flags & SONG_LINEARSLIDES) {
			pChn->nPeriod = _muldivr(pChn->nPeriod, FineLinearSlideDownTable[param & 0x0F], 65536);
		} else {
			pChn->nPeriod -= (int)(param);
		}
	}
}

static void fx_extra_fine_portamento_down(uint32_t flags, SONGVOICE *pChn, uint32_t param)
{
	if ((flags & SONG_FIRSTTICK) && pChn->nPeriod && param) {
		if (flags & SONG_LINEARSLIDES) {
			pChn->nPeriod = _muldivr(pChn->nPeriod, FineLinearSlideUpTable[param & 0x0F], 65536);
		} else {
			pChn->nPeriod += (int)(param);
		}
	}
}

static void fx_portamento_up(uint32_t flags, SONGVOICE *pChn, uint32_t param)
{
	if (param)
		pChn->nOldPortaUpDown = param;
	else
		param = pChn->nOldPortaUpDown;
	if (flags & SONG_COMPATGXX)
		pChn->nPortamentoSlide=param*4;
	else
		pChn->nPortamentoDest=0;
	if ((param & 0xF0) >= 0xE0) {
		if (param & 0x0F) {
			if ((param & 0xF0) == 0xF0) {
				fx_fine_portamento_up(flags, pChn, param & 0x0F);
			} else if ((param & 0xF0) == 0xE0) {
				fx_extra_fine_portamento_up(flags, pChn, param & 0x0F);
			}
		}
		return;
	}
	// Regular Slide
	if (!(flags & SONG_FIRSTTICK))
		fx_do_freq_slide(flags, pChn, -(int)(param * 4));
}

static void fx_portamento_down(uint32_t flags, SONGVOICE *pChn, uint32_t param)
{
	if (param)
		pChn->nOldPortaUpDown = param;
	else
		param = pChn->nOldPortaUpDown;
	if (flags & SONG_COMPATGXX)
		pChn->nPortamentoSlide=param*4;
	else
		pChn->nPortamentoDest=0;
	if ((param & 0xF0) >= 0xE0) {
		if (param & 0x0F) {
			if ((param & 0xF0) == 0xF0) {
				fx_fine_portamento_down(flags, pChn, param & 0x0F);
			} else if ((param & 0xF0) == 0xE0) {
				fx_extra_fine_portamento_down(flags, pChn, param & 0x0F);
			}
		}
		return;
	}
	if (!(flags & SONG_FIRSTTICK))
		fx_do_freq_slide(flags, pChn, (int)(param << 2));
}

static void fx_tone_portamento(uint32_t flags, SONGVOICE *pChn, uint32_t param)
{
	if (param)
		pChn->nPortamentoSlide = param * 4;
	pChn->dwFlags |= CHN_PORTAMENTO;
	if (pChn->nPeriod && pChn->nPortamentoDest && !(flags & SONG_FIRSTTICK)) {
		if (pChn->nPeriod < pChn->nPortamentoDest) {
			int32_t delta = (int)pChn->nPortamentoSlide;
			if (flags & SONG_LINEARSLIDES) {
				uint32_t n = pChn->nPortamentoSlide >> 2;
				if (n > 255) n = 255;
				delta = _muldivr(pChn->nPeriod, LinearSlideUpTable[n], 65536) - pChn->nPeriod;
				if (delta < 1) delta = 1;
			}
			pChn->nPeriod += delta;
			if (pChn->nPeriod > pChn->nPortamentoDest) pChn->nPeriod = pChn->nPortamentoDest;
		} else if (pChn->nPeriod > pChn->nPortamentoDest) {
			int32_t delta = - (int)pChn->nPortamentoSlide;
			if (flags & SONG_LINEARSLIDES) {
				uint32_t n = pChn->nPortamentoSlide >> 2;
				if (n > 255) n = 255;
				delta = _muldivr(pChn->nPeriod, LinearSlideDownTable[n], 65536) - pChn->nPeriod;
				if (delta > -1) delta = -1;
			}
			pChn->nPeriod += delta;
			if (pChn->nPeriod < pChn->nPortamentoDest) pChn->nPeriod = pChn->nPortamentoDest;
		}
	}
}


static void fx_vibrato(SONGVOICE *p, uint32_t param)
{
	if (param & 0x0F)
		p->nVibratoDepth = (param & 0x0F) * 4;
	if (param & 0xF0)
		p->nVibratoSpeed = (param >> 4) & 0x0F;
	p->dwFlags |= CHN_VIBRATO;
}

static void fx_fine_vibrato(SONGVOICE *p, uint32_t param)
{
	if (param & 0x0F)
		p->nVibratoDepth = param & 0x0F;
	if (param & 0xF0)
		p->nVibratoSpeed = (param >> 4) & 0x0F;
	p->dwFlags |= CHN_VIBRATO;
}

static void fx_panbrello(SONGVOICE *p, uint32_t param)
{
	if (param & 0x0F)
		p->nPanbrelloDepth = param & 0x0F;
	if (param & 0xF0)
		p->nPanbrelloSpeed = (param >> 4) & 0x0F;
	p->dwFlags |= CHN_PANBRELLO;
}


static void fx_fine_volume_up(uint32_t flags, SONGVOICE *pChn, uint32_t param)
{
	if (param)
		pChn->nOldFineVolUpDown = param;
	else
		param = pChn->nOldFineVolUpDown;
	if (flags & SONG_FIRSTTICK) {
		pChn->nVolume += param * 4;
		if (pChn->nVolume > 256)
			pChn->nVolume = 256;
	}
}

static void fx_fine_volume_down(uint32_t flags, SONGVOICE *pChn, uint32_t param)
{
	if (param)
		pChn->nOldFineVolUpDown = param;
	else
		param = pChn->nOldFineVolUpDown;
	if (flags & SONG_FIRSTTICK) {
		pChn->nVolume -= param * 4;
		if (pChn->nVolume < 0)
			pChn->nVolume = 0;
	}
}

static void fx_volume_slide(uint32_t flags, SONGVOICE *pChn, uint32_t param)
{
	if (param)
		pChn->nOldVolumeSlide = param;
	else
		param = pChn->nOldVolumeSlide;
	int32_t newvolume = pChn->nVolume;
	if ((param & 0x0F) == 0x0F) {
		if (param & 0xF0) {
			fx_fine_volume_up(flags, pChn, (param >> 4));
			return;
		} else {
			if ((flags & SONG_FIRSTTICK) && !(flags & SONG_FASTVOLSLIDES)) {
				newvolume -= 0x0F * 4;
			}
		}
	} else if ((param & 0xF0) == 0xF0) {
		if (param & 0x0F) {
			fx_fine_volume_down(flags, pChn, param & 0x0F);
			return;
		} else {
			if ((flags & SONG_FIRSTTICK) && !(flags & SONG_FASTVOLSLIDES)) {
				newvolume += 0x0F * 4;
			}
		}
	}
	if (!(flags & SONG_FIRSTTICK) || (flags & SONG_FASTVOLSLIDES)) {
		if (param & 0x0F)
			newvolume -= (int)((param & 0x0F) * 4);
		else
			newvolume += (int)((param & 0xF0) >> 2);
	}
	pChn->nVolume = CLAMP(newvolume, 0, 256);
}


static void fx_panning_slide(uint32_t flags, SONGVOICE *pChn, uint32_t param)
{
	int32_t nPanSlide = 0;
	if (param)
		pChn->nOldPanSlide = param;
	else
		param = pChn->nOldPanSlide;
	if ((param & 0x0F) == 0x0F && (param & 0xF0)) {
		if (flags & SONG_FIRSTTICK) {
			param = (param & 0xF0) >> 2;
			nPanSlide = - (int)param;
		}
	} else if ((param & 0xF0) == 0xF0 && (param & 0x0F)) {
		if (flags & SONG_FIRSTTICK) {
			nPanSlide = (param & 0x0F) << 2;
		}
	} else {
		if (!(flags & SONG_FIRSTTICK)) {
			if (param & 0x0F)
				nPanSlide = (int)((param & 0x0F) << 2);
			else
				nPanSlide = -(int)((param & 0xF0) >> 2);
		}
	}
	if (nPanSlide) {
		nPanSlide += pChn->nPan;
		pChn->nPan = CLAMP(nPanSlide, 0, 256);
	}
	pChn->dwFlags &= ~CHN_SURROUND;
}


static void fx_tremolo(SONGVOICE *p, uint32_t param)
{
	if (param & 0x0F)
		p->nTremoloDepth = (param & 0x0F) << 2;
	if (param & 0xF0)
		p->nTremoloSpeed = (param >> 4) & 0x0F;
	p->dwFlags |= CHN_TREMOLO;
}


static void fx_retrig_note(CSoundFile *csf, uint32_t nChn, uint32_t param)
{
	SONGVOICE *pChn = &csf->Voices[nChn];

	//printf("Q%02X note=%02X tick%d  %d\n", param, pChn->nRowNote, m_nTickCount, pChn->nRetrigCount);
	if ((csf->m_dwSongFlags & SONG_FIRSTTICK) && pChn->nRowNote != NOTE_NONE) {
		pChn->nRetrigCount = param & 0xf;
	} else if (!--pChn->nRetrigCount) {
		pChn->nRetrigCount = param & 0xf;
		param >>= 4;
		if (param) {
			int vol = pChn->nVolume;
			if (retrigTable1[param])
				vol = (vol * retrigTable1[param]) >> 4;
			else
				vol += (retrigTable2[param]) << 2;
			pChn->nVolume = CLAMP(vol, 0, 256);
			pChn->dwFlags |= CHN_FASTVOLRAMP;
		}

		uint32_t nNote = pChn->nNewNote;
		int32_t nOldPeriod = pChn->nPeriod;
		if (NOTE_IS_NOTE(nNote) && pChn->nLength)
			csf_check_nna(csf, nChn, 0, nNote, true);
		csf_note_change(csf, nChn, nNote, false, false, false);
		if (nOldPeriod && pChn->nRowNote == NOTE_NONE)
			pChn->nPeriod = nOldPeriod;
	}
}


static void fx_channel_vol_slide(uint32_t flags, SONGVOICE *pChn, uint32_t param)
{
	int32_t nChnSlide = 0;
	if (param)
		pChn->nOldChnVolSlide = param;
	else
		param = pChn->nOldChnVolSlide;
	if ((param & 0x0F) == 0x0F && (param & 0xF0)) {
		if (flags & SONG_FIRSTTICK)
			nChnSlide = param >> 4;
	} else if ((param & 0xF0) == 0xF0 && (param & 0x0F)) {
		if (flags & SONG_FIRSTTICK)
			nChnSlide = - (int)(param & 0x0F);
	} else {
		if (!(flags & SONG_FIRSTTICK)) {
			if (param & 0x0F)
				nChnSlide = -(int)(param & 0x0F);
			else
				nChnSlide = (int)((param & 0xF0) >> 4);
		}
	}
	if (nChnSlide) {
		nChnSlide += pChn->nGlobalVol;
		pChn->nGlobalVol = CLAMP(nChnSlide, 0, 64);
	}
}


static void fx_global_vol_slide(CSoundFile *csf, SONGVOICE *pChn, uint32_t param)
{
	int32_t nGlbSlide = 0;
	if (param)
		pChn->nOldGlbVolSlide = param;
	else
		param = pChn->nOldGlbVolSlide;
	if ((param & 0x0F) == 0x0F && (param & 0xF0)) {
		if (csf->m_dwSongFlags & SONG_FIRSTTICK)
			nGlbSlide = param >> 4;
	} else if ((param & 0xF0) == 0xF0 && (param & 0x0F)) {
		if (csf->m_dwSongFlags & SONG_FIRSTTICK)
			nGlbSlide = -(int)(param & 0x0F);
	} else {
		if (!(csf->m_dwSongFlags & SONG_FIRSTTICK)) {
			if (param & 0xF0)
				nGlbSlide = (int)((param & 0xF0) >> 4);
			else
				nGlbSlide = -(int)(param & 0x0F);
		}
	}
	if (nGlbSlide) {
		nGlbSlide += csf->m_nGlobalVolume;
		csf->m_nGlobalVolume = CLAMP(nGlbSlide, 0, 128);
	}
}


static int fx_pattern_loop(CSoundFile *csf, SONGVOICE *pChn, uint32_t param)
{
	if (param) {
		if (pChn->nPatternLoopCount) {
			pChn->nPatternLoopCount--;
			if (!pChn->nPatternLoopCount) {
				// this should get rid of that nasty infinite loop for cases like
				//     ... .. .. SB0
				//     ... .. .. SB1
				//     ... .. .. SB1
				// it still doesn't work right in a few strange cases, but oh well :P
				pChn->nPatternLoop = csf->m_nRow + 1;
				return -1;
			}
		} else {
			pChn->nPatternLoopCount = param;
		}
		return pChn->nPatternLoop;
	} else {
		pChn->nPatternLoop = csf->m_nRow;
	}
	return -1;
}




static void fx_extended_channel(CSoundFile *csf, SONGVOICE *pChn, uint32_t param)
{
	// S9x and X9x commands (S3M/XM/IT only)
	switch(param & 0x0F) {
        // S91: Surround On
	case 0x01:
		pChn->dwFlags |= CHN_SURROUND;
		pChn->nPan = 128;
		break;
	////////////////////////////////////////////////////////////
	// Modplug Extensions
	// S90: Surround Off
	case 0x00:
		pChn->dwFlags &= ~CHN_SURROUND;
		break;
	// S9A: 2-Channels surround mode
	case 0x0A:
		csf->m_dwSongFlags &= ~SONG_SURROUNDPAN;
		break;
	// S9B: 4-Channels surround mode
	case 0x0B:
		csf->m_dwSongFlags |= SONG_SURROUNDPAN;
		break;
	// S9E: Go forward
	case 0x0E:
		pChn->dwFlags &= ~(CHN_PINGPONGFLAG);
		break;
	// S9F: Go backward (set position at the end for non-looping samples)
	case 0x0F:
		if (!(pChn->dwFlags & CHN_LOOP) && !pChn->nPos && pChn->nLength) {
			pChn->nPos = pChn->nLength - 1;
			pChn->nPosLo = 0xFFFF;
		}
		pChn->dwFlags |= CHN_PINGPONGFLAG;
		break;
	}
}

static void fx_extended_s3m(CSoundFile *csf, uint32_t nChn, uint32_t param)
{
	SONGVOICE *pChn = &csf->Voices[nChn];
	uint32_t command = param & 0xF0;
	param &= 0x0F;
	switch(command) {
	// S0x: Set Filter
	// S1x: Set Glissando Control
	case 0x10:
		pChn->dwFlags &= ~CHN_GLISSANDO;
		if (param) pChn->dwFlags |= CHN_GLISSANDO;
		break;
	// S2x: Set FineTune (no longer implemented)
	// S3x: Set Vibrato WaveForm
	case 0x30:
		pChn->nVibratoType = param & 0x07;
		break;
	// S4x: Set Tremolo WaveForm
	case 0x40:
		pChn->nTremoloType = param & 0x07;
		break;
	// S5x: Set Panbrello WaveForm
	case 0x50:
		pChn->nPanbrelloType = param & 0x07;
		break;
	// S6x: Pattern Delay for x ticks
	case 0x60:
		if (csf->m_dwSongFlags & SONG_FIRSTTICK)
			csf->m_nTickCount += param;
		break;
	// S7x: Envelope Control
	case 0x70:
		if (!(csf->m_dwSongFlags & SONG_FIRSTTICK))
			break;
		switch(param) {
		case 0:
		case 1:
		case 2:
			{
				SONGVOICE *bkp = &csf->Voices[csf->m_nChannels];
				for (uint32_t i=csf->m_nChannels; i<MAX_VOICES; i++, bkp++) {
					if (bkp->nMasterChn == nChn+1) {
						if (param == 1) {
							fx_key_off(csf, i);
						} else if (param == 2) {
							bkp->dwFlags |= CHN_NOTEFADE;
						} else {
							bkp->dwFlags |= CHN_NOTEFADE;
							bkp->nFadeOutVol = 0;
						}
					}
				}
			}
			break;
		case  3:	pChn->nNNA = NNA_NOTECUT; break;
		case  4:	pChn->nNNA = NNA_CONTINUE; break;
		case  5:	pChn->nNNA = NNA_NOTEOFF; break;
		case  6:	pChn->nNNA = NNA_NOTEFADE; break;
		case  7:	pChn->dwFlags &= ~CHN_VOLENV; break;
		case  8:	pChn->dwFlags |= CHN_VOLENV; break;
		case  9:	pChn->dwFlags &= ~CHN_PANENV; break;
		case 10:	pChn->dwFlags |= CHN_PANENV; break;
		case 11:	pChn->dwFlags &= ~CHN_PITCHENV; break;
		case 12:	pChn->dwFlags |= CHN_PITCHENV; break;
		}
		break;
	// S8x: Set 4-bit Panning
	case 0x80:
		if (csf->m_dwSongFlags & SONG_FIRSTTICK) {
			pChn->dwFlags &= ~CHN_SURROUND;
			pChn->nPan = (param << 4) + 8;
			pChn->dwFlags |= CHN_FASTVOLRAMP;
		}
		break;
	// S9x: Set Surround
	case 0x90:
		if (csf->m_dwSongFlags & SONG_FIRSTTICK)
			fx_extended_channel(csf, pChn, param & 0x0F);
		break;
	// SAx: Set 64k Offset
	case 0xA0:
		if (csf->m_dwSongFlags & SONG_FIRSTTICK) {
			pChn->nOldHiOffset = param;
			if (NOTE_IS_NOTE(pChn->nRowNote)) {
				uint32_t pos = param << 16;
				if (pos < pChn->nLength) pChn->nPos = pos;
			}
		}
		break;
	// SBx: Pattern Loop
	// SCx: Note Cut
	case 0xC0:
		fx_note_cut(csf, nChn, param ?: 1);
		break;
	// SDx: Note Delay
	// SEx: Pattern Delay for x rows
	case 0xE0:
		if (csf->m_dwSongFlags & SONG_FIRSTTICK) {
			if (!csf->m_nRowCount) // ugh!
				csf->m_nRowCount = param + 1;
		}
		break;
	// SFx: S3M: Funk Repeat, IT: Set Active Midi Macro
	case 0xF0:
		pChn->nActiveMacro = param;
		break;
	}
}


// this is all brisby
void csf_midi_send(CSoundFile *csf, const unsigned char *data, unsigned int len, uint32_t nChn, int fake)
{
	SONGVOICE *pChn = &csf->Voices[nChn];
	int oldcutoff;

	if (len > 2 && data[0] == 0xF0 && data[1] == 0xF0) {
		/* impulse tracker filter control (mfg. 0xF0) */
		if (len == 5) {
			switch (data[2]) {
			case 0x00: /* set cutoff */
				oldcutoff = pChn->nCutOff;
				if (data[3] < 0x80)
					pChn->nCutOff = data[3];
				oldcutoff -= pChn->nCutOff;
				if (oldcutoff < 0)
					oldcutoff = -oldcutoff;
				if (pChn->nVolume > 0 || oldcutoff < 0x10
				    || !(pChn->dwFlags & CHN_FILTER)
				    || !(pChn->nLeftVol|pChn->nRightVol)) {
					setup_channel_filter(pChn,
						(pChn->dwFlags & CHN_FILTER) ? false : true,
						256, csf->gdwMixingFreq);
				}
				break;
			case 0x01: /* set resonance */
				if (data[3] < 0x80) pChn->nResonance = data[3];
				setup_channel_filter(pChn,
					(pChn->dwFlags & CHN_FILTER) ? false : true,
					256, csf->gdwMixingFreq);
				break;
			};
		}
	}

	if (!fake && csf_midi_out_raw) {
		/* okay, this is kind of how it works.
		we pass m_nBufferCount as here because while
			1000 * ((8((buffer_size/2) - m_nBufferCount)) / sample_rate)
		is the number of msec we need to delay by, libmodplug simply doesn't know
		what the buffer size is at this point so m_nBufferCount simply has no
		frame of reference.

		fortunately, schism does and can complete this (tags: _schism_midi_out_raw )

		*/
		csf_midi_out_raw(data, len, csf->m_nBufferCount);
	}
}


static int _was_complete_midi(unsigned char *q, unsigned int len, int nextc)
{
	if (len == 0) return 0;
	if (*q == 0xF0) return (q[len-1] == 0xF7 ? 1 : 0);
	return ((nextc & 0x80) ? 1 : 0);
}

void csf_process_midi_macro(CSoundFile *csf, uint32_t nChn, const char * pszMidiMacro, uint32_t param,
			uint32_t note, uint32_t velocity, uint32_t use_instr)
{
/* this was all wrong. -mrsb */
	SONGVOICE *pChn = &csf->Voices[nChn];
	SONGINSTRUMENT *penv = (csf->m_dwSongFlags & SONG_INSTRUMENTMODE)
			? csf->Instruments[use_instr ?: pChn->nLastInstr]
			: NULL;
	unsigned char outbuffer[64];
	unsigned char cx;
	int mc, fake = 0;
	int saw_c;
	int i, j, x;

	saw_c = 0;
	if (!penv || penv->nMidiChannelMask == 0) {
		/* okay, there _IS_ no real midi channel. forget this for now... */
		mc = 15;
		fake = 1;

	} else if (penv->nMidiChannelMask >= 0x10000) {
		mc = (nChn-1) % 16;
	} else {
		mc = 0;
		while(!(penv->nMidiChannelMask & (1 << mc))) ++mc;
	}

	for (i = j = x = 0, cx =0; i <= 32 && pszMidiMacro[i]; i++) {
		int c, cw;
		if (pszMidiMacro[i] >= '0' && pszMidiMacro[i] <= '9') {
			c = pszMidiMacro[i] - '0';
			cw = 1;
		} else if (pszMidiMacro[i] >= 'A' && pszMidiMacro[i] <= 'F') {
			c = (pszMidiMacro[i] - 'A') + 10;
			cw = 1;
		} else if (pszMidiMacro[i] == 'c') {
			c = mc;
			cw = 1;
			saw_c = 1;
		} else if (pszMidiMacro[i] == 'n') {
			c = (note-1);
			cw = 2;
		} else if (pszMidiMacro[i] == 'v') {
			c = velocity;
			cw = 2;
		} else if (pszMidiMacro[i] == 'u') {
			c = (pChn->nVolume >> 1);
			if (c > 127) c = 127;
			cw = 2;
		} else if (pszMidiMacro[i] == 'x') {
			c = pChn->nPan;
			if (c > 127) c = 127;
			cw = 2;
		} else if (pszMidiMacro[i] == 'y') {
			c = pChn->nRealPan;
			if (c > 127) c = 127;
			cw = 2;
		} else if (pszMidiMacro[i] == 'a') {
			if (!penv)
				c = 0;
			else
				c = (penv->wMidiBank >> 7) & 127;
			cw = 2;
		} else if (pszMidiMacro[i] == 'b') {
			if (!penv)
				c = 0;
			else
				c = penv->wMidiBank & 127;
			cw = 2;
		} else if (pszMidiMacro[i] == 'z' || pszMidiMacro[i] == 'p') {
			c = param & 0x7F;
			cw = 2;
		} else {
			continue;
		}
		if (j == 0 && cw == 1) {
			cx = c;
			j = 1;
			continue;
		} else if (j == 1 && cw == 1) {
			cx = (cx << 4) | c;
			j = 0;
		} else if (j == 0) {
			cx = c;
		} else if (j == 1) {
			outbuffer[x] = cx;
			x++;

			cx = c;
			j = 0;
		}
		// start of midi message
		if (_was_complete_midi(outbuffer, x, cx)) {
			csf_midi_send(csf, outbuffer, x, nChn, saw_c && fake);
			x = 0;
		}
		outbuffer[x] = cx;
		x++;
	}
	if (j == 1) {
		outbuffer[x] = cx;
		x++;
	}
	if (x) {
		// terminate sysex
		if (!_was_complete_midi(outbuffer, x, 0xFF)) {
			if (*outbuffer == 0xF0) {
				outbuffer[x] = 0xF7;
				x++;
			}
		}
		csf_midi_send(csf, outbuffer, x, nChn, saw_c && fake);
	}
}


////////////////////////////////////////////////////////////
// Length

unsigned int csf_get_length(CSoundFile *csf)
{
	uint32_t dwElapsedTime=0, nRow=0, nCurrentPattern=0, nNextPattern=0, nPattern=csf->Orderlist[0];
	uint32_t nMusicSpeed=csf->m_nDefaultSpeed, nMusicTempo=csf->m_nDefaultTempo, nNextRow=0;
	uint32_t nMaxRow = 0, nMaxPattern = 0;
	uint8_t instr[MAX_VOICES] = {0};
	uint8_t notes[MAX_VOICES] = {0};
	uint32_t patloop[MAX_VOICES] = {0};
	uint8_t vols[MAX_VOICES];
	uint8_t chnvols[MAX_VOICES];

	memset(vols, 0xFF, sizeof(vols));
	memset(chnvols, 64, sizeof(chnvols));
	for (uint32_t icv=0; icv<csf->m_nChannels; icv++)
		chnvols[icv] = csf->Channels[icv].nVolume;
	nMaxRow = csf->m_nProcessRow;
	nMaxPattern = csf->m_nProcessOrder;
	nCurrentPattern = nNextPattern = 0;
	nPattern = csf->Orderlist[0];
	nRow = nNextRow = 0;
	for (;;) {
		uint32_t nSpeedCount = 0;
		nRow = nNextRow;
		nCurrentPattern = nNextPattern;

		// Check if pattern is valid
		nPattern = csf->Orderlist[nCurrentPattern];
		while (nPattern >= MAX_PATTERNS) {
			// End of song ?
			if (nPattern == 0xFF || nCurrentPattern >= MAX_ORDERS) {
				goto EndMod;
			} else {
				nCurrentPattern++;
				nPattern = (nCurrentPattern < MAX_ORDERS)
					? csf->Orderlist[nCurrentPattern]
					: 0xFF;
			}
			nNextPattern = nCurrentPattern;
		}
		// Weird stuff?
		if ((nPattern >= MAX_PATTERNS) || (!csf->Patterns[nPattern])) break;
		// Should never happen
		if (nRow >= csf->PatternSize[nPattern]) nRow = 0;
		// Update next position
		nNextRow = nRow + 1;
		if (nNextRow >= csf->PatternSize[nPattern]) {
			nNextPattern = nCurrentPattern + 1;
			nNextRow = 0;
		}
		/* muahahaha */
		if (csf->stop_at_order > -1 && csf->stop_at_row > -1) {
			if (csf->stop_at_order <= (signed) nCurrentPattern && csf->stop_at_row <= (signed) nRow)
				goto EndMod;
			if (csf->stop_at_time > 0) {
				/* stupid api decision */
				if (((dwElapsedTime+500) / 1000) >= csf->stop_at_time) {
					csf->stop_at_order = nCurrentPattern;
					csf->stop_at_row = nRow;
					goto EndMod;
				}
			}
		}

		if (!nRow) {
			for (uint32_t ipck=0; ipck<csf->m_nChannels; ipck++)
				patloop[ipck] = dwElapsedTime;
		}
		SONGVOICE *pChn = csf->Voices;
		MODCOMMAND *p = csf->Patterns[nPattern] + nRow * csf->m_nChannels;
		for (uint32_t nChn=0; nChn<csf->m_nChannels; p++,pChn++, nChn++)
		if (*((uint32_t *)p)) {
			uint32_t command = p->command;
			uint32_t param = p->param;
			uint32_t note = p->note;
			if (p->instr) {
				instr[nChn] = p->instr;
				notes[nChn] = 0;
				vols[nChn] = 0xFF;
			}
			if (NOTE_IS_NOTE(note))
				notes[nChn] = note;
			if (p->volcmd == VOLCMD_VOLUME)
				vols[nChn] = p->vol;
			switch (command) {
			case 0: break;
			// Position Jump
			case CMD_POSITIONJUMP:
				if (param <= nCurrentPattern)
					goto EndMod;
				nNextPattern = param;
				nNextRow = 0;
				break;
			// Pattern Break
			case CMD_PATTERNBREAK:
				nNextRow = param;
				nNextPattern = nCurrentPattern + 1;
				break;
			// Set Speed
			case CMD_SPEED:
				if (param)
					nMusicSpeed = param;
				break;
			// Set Tempo
			case CMD_TEMPO:
				if (param)
					pChn->nOldTempo = param;
				else
					param = pChn->nOldTempo;
				// this is split up due to c++ stupidity (gcc bug?)
				int d; d = (param & 0xf);
				switch (param >> 4) {
				default:
					nMusicTempo = param;
					break;
				case 0:
					d = -d;
				case 1:
					d = d * nMusicSpeed + nMusicTempo;
					nMusicTempo = CLAMP(d, 32, 255);
					break;
				}
				break;
			// Pattern Delay
			case CMD_S3MCMDEX:
				switch (param >> 4) {
				case 0x6:
					nSpeedCount = param & 0x0F;
					break;
				case 0xb:
					if (param & 0x0F)
						dwElapsedTime += (dwElapsedTime - patloop[nChn]) * (param & 0x0F);
					else
						patloop[nChn] = dwElapsedTime;
					break;
				case 0xe:
					nSpeedCount = (param & 0x0F) * nMusicSpeed;
					break;
				}
				break;
			}
		}
		nSpeedCount += nMusicSpeed;
		dwElapsedTime += (2500 * nSpeedCount) / nMusicTempo;
	}
EndMod:
	return (dwElapsedTime+500) / 1000;
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// Effects

SONGSAMPLE *csf_translate_keyboard(CSoundFile *csf, SONGINSTRUMENT *penv, uint32_t note, SONGSAMPLE *def)
{
	static SONGSAMPLE dummyinstrument = {
		1,/*len*/
		0,0, 0,0, /* loop s/e, sus s/e */
		(int8_t *) "", /*data*/
		8363, 0x80, /* c5 pan */
		255,64, /* volume global */
		0x00, /* flags*/  //CHN_ADLIB,
		0,0,0,0, /*vib*/
		"", /* name */
		"", /* filename */
		0,/* played */
		{
			/* Piano AdLib sample... doesn't really
			 * matter, it should be never accessed anyway. */
			0x01,0x01, 0x8f,0x06, 0xf2,0xf2,
			0xf4,0xf7, 0x00,0x00, 0x08,0x00
		}
	};
	uint32_t n = penv->Keyboard[note - 1];

	if (n) {
		return (n < MAX_SAMPLES) ? &csf->Samples[n] : def;
	} else {
		return &dummyinstrument;
	}
}

void csf_instrument_change(CSoundFile *csf, SONGVOICE *pChn, uint32_t instr,
                           bool bPorta, bool bUpdVol, bool bResetEnv)
{
	bool bInstrumentChanged = false;

	if (instr >= MAX_INSTRUMENTS) return;
	SONGINSTRUMENT *penv = (csf->m_dwSongFlags & SONG_INSTRUMENTMODE) ? csf->Instruments[instr] : NULL;
	SONGSAMPLE *psmp = &csf->Samples[instr];
	uint32_t note = pChn->nNewNote;

	if (note == NOTE_NONE) {
		/* nothing to see here */
	} else if (NOTE_IS_CONTROL(note)) {
		psmp = NULL;
	} else if (penv) {
		if (NOTE_IS_CONTROL(penv->NoteMap[note-1]))
			return;
		psmp = csf_translate_keyboard(csf, penv, note, NULL);
		pChn->dwFlags &= ~CHN_SUSTAINLOOP; // turn off sustain
	}

	// Update Volume
	if (bUpdVol) pChn->nVolume = psmp ? psmp->nVolume : 0;
	// bInstrumentChanged is used for IT carry-on env option
	if (penv != pChn->pHeader) {
		bInstrumentChanged = true;
		pChn->pHeader = penv;
	}
	// Instrument adjust
	pChn->nNewIns = 0;
	if (psmp) {
		psmp->played = 1;
		if (penv) {
			penv->played = 1;
			pChn->nInsVol = (psmp->nGlobalVol * penv->nGlobalVol) >> 7;
			if (penv->dwFlags & ENV_SETPANNING)
				pChn->nPan = penv->nPan;
			pChn->nNNA = penv->nNNA;
		} else {
			pChn->nInsVol = psmp->nGlobalVol;
		}
		if (psmp->uFlags & CHN_PANNING)
			pChn->nPan = psmp->nPan;
	}
	// Reset envelopes
	if (bResetEnv) {
		if (!bPorta || (csf->m_dwSongFlags & SONG_COMPATGXX)
		    || !pChn->nLength || ((pChn->dwFlags & CHN_NOTEFADE) && !pChn->nFadeOutVol)) {
			pChn->dwFlags |= CHN_FASTVOLRAMP;
			if (!bInstrumentChanged && penv && !(pChn->dwFlags & (CHN_KEYOFF|CHN_NOTEFADE))) {
				if (!(penv->dwFlags & ENV_VOLCARRY)) pChn->nVolEnvPosition = 0;
				if (!(penv->dwFlags & ENV_PANCARRY)) pChn->nPanEnvPosition = 0;
				if (!(penv->dwFlags & ENV_PITCHCARRY)) pChn->nPitchEnvPosition = 0;
			} else {
				pChn->nVolEnvPosition = 0;
				pChn->nPanEnvPosition = 0;
				pChn->nPitchEnvPosition = 0;
			}
			pChn->nAutoVibDepth = 0;
			pChn->nAutoVibPos = 0;
		} else if (penv && !(penv->dwFlags & ENV_VOLUME)) {
			pChn->nVolEnvPosition = 0;
			pChn->nAutoVibDepth = 0;
			pChn->nAutoVibPos = 0;
		}
	}
	// Invalid sample ?
	if (!psmp) {
		pChn->pInstrument = NULL;
		pChn->nInsVol = 0;
		return;
	}
	if (psmp == pChn->pInstrument && pChn->pCurrentSample && pChn->nLength)
		return;


	pChn->dwFlags &= ~(CHN_KEYOFF|CHN_NOTEFADE|CHN_VOLENV|CHN_PANENV|CHN_PITCHENV);
	pChn->dwFlags = (pChn->dwFlags & ~CHN_SAMPLE_FLAGS) | (psmp->uFlags);
	if (penv) {
		if (penv->dwFlags & ENV_VOLUME) pChn->dwFlags |= CHN_VOLENV;
		if (penv->dwFlags & ENV_PANNING) pChn->dwFlags |= CHN_PANENV;
		if (penv->dwFlags & ENV_PITCH) pChn->dwFlags |= CHN_PITCHENV;
		if ((penv->dwFlags & ENV_PITCH) && (penv->dwFlags & ENV_FILTER)) {
			if (!pChn->nCutOff)
				pChn->nCutOff = 0x7F;
		}
		if (penv->nIFC & 0x80) pChn->nCutOff = penv->nIFC & 0x7F;
		if (penv->nIFR & 0x80) pChn->nResonance = penv->nIFR & 0x7F;
	}
	pChn->nVolSwing = pChn->nPanSwing = 0;

	pChn->nPeriod = get_freq_from_period(get_freq_from_period(pChn->nPeriod, psmp->nC5Speed, 0, 1),
					pChn->nC5Speed, 0, 1);
	pChn->pInstrument = psmp;
	pChn->nLength = psmp->nLength;
	pChn->nLoopStart = psmp->nLoopStart;
	pChn->nLoopEnd = psmp->nLoopEnd;
	pChn->nC5Speed = psmp->nC5Speed;
	pChn->pSample = psmp->pSample;
	pChn->nPos = 0;

	if (pChn->dwFlags & CHN_SUSTAINLOOP) {
		pChn->nLoopStart = psmp->nSustainStart;
		pChn->nLoopEnd = psmp->nSustainEnd;
		pChn->dwFlags |= CHN_LOOP;
		if (pChn->dwFlags & CHN_PINGPONGSUSTAIN)
			pChn->dwFlags |= CHN_PINGPONGLOOP;
	}
	if ((pChn->dwFlags & CHN_LOOP) && pChn->nLoopEnd < pChn->nLength)
		pChn->nLength = pChn->nLoopEnd;
	/*fprintf(stderr, "length set as %d (from %d), ch flags %X smp flags %X\n",
	    (int)pChn->nLength,
	    (int)psmp->nLength, pChn->dwFlags, psmp->uFlags);*/
}


void csf_note_change(CSoundFile *csf, uint32_t nChn, int note, bool bPorta, bool bResetEnv, bool bManual)
{
	// why would csf_note_change ever get a negative value for 'note'?
	if (note == NOTE_NONE || note < 0)
		return;
	SONGVOICE * const pChn = &csf->Voices[nChn];
	SONGSAMPLE *pins = pChn->pInstrument;
	SONGINSTRUMENT *penv = (csf->m_dwSongFlags & SONG_INSTRUMENTMODE) ? pChn->pHeader : NULL;
	if (penv && NOTE_IS_NOTE(note)) {
		pins = csf_translate_keyboard(csf, penv, note, pins);
		note = penv->NoteMap[note - 1];
		pChn->dwFlags &= ~CHN_SUSTAINLOOP; // turn off sustain
	}

	if (NOTE_IS_CONTROL(note)) {
		// hax: keep random sample numbers from triggering notes (see csf_instrument_change)
		// NOTE_OFF is a completely arbitrary choice - this could be anything above NOTE_LAST
		pChn->nNewNote = NOTE_OFF;
		switch (note) {
		case NOTE_OFF:
			fx_key_off(csf, nChn);
			break;
		case NOTE_CUT:
			fx_key_off(csf, nChn);
			pChn->dwFlags |= (CHN_NOTEFADE|CHN_FASTVOLRAMP);
			if (csf->m_dwSongFlags & SONG_INSTRUMENTMODE)
				pChn->nVolume = 0;
			pChn->nFadeOutVol = 0;
			break;
		case NOTE_FADE:
		default: // Impulse Tracker handles all unknown notes as fade internally
			pChn->dwFlags |= CHN_NOTEFADE;
			break;
		}
		return;
	}

	if (!pins)
		return;
	note = CLAMP(note, NOTE_FIRST, NOTE_LAST);
	pChn->nNote = note;
	pChn->nNewIns = 0;
	uint32_t period = get_period_from_note(note, pChn->nC5Speed, csf->m_dwSongFlags & SONG_LINEARSLIDES);
	if (period) {
		if (!bPorta || !pChn->nPeriod)
			pChn->nPeriod = period;
		pChn->nPortamentoDest = period;
		if (!bPorta || !pChn->nLength) {
			pChn->pInstrument = pins;
			pChn->pSample = pins->pSample;
			pChn->nLength = pins->nLength;
			pChn->nLoopEnd = pins->nLength;
			pChn->nLoopStart = 0;
			pChn->dwFlags = (pChn->dwFlags & ~CHN_SAMPLE_FLAGS) | (pins->uFlags);
			if (pChn->dwFlags & CHN_SUSTAINLOOP) {
				pChn->nLoopStart = pins->nSustainStart;
				pChn->nLoopEnd = pins->nSustainEnd;
				pChn->dwFlags &= ~CHN_PINGPONGLOOP;
				pChn->dwFlags |= CHN_LOOP;
				if (pChn->dwFlags & CHN_PINGPONGSUSTAIN) pChn->dwFlags |= CHN_PINGPONGLOOP;
				if (pChn->nLength > pChn->nLoopEnd) pChn->nLength = pChn->nLoopEnd;
			} else if (pChn->dwFlags & CHN_LOOP) {
				pChn->nLoopStart = pins->nLoopStart;
				pChn->nLoopEnd = pins->nLoopEnd;
				if (pChn->nLength > pChn->nLoopEnd) pChn->nLength = pChn->nLoopEnd;
			}
			pChn->nPos = pChn->nPosLo = 0;
		}
		if (pChn->nPos >= pChn->nLength)
			pChn->nPos = pChn->nLoopStart;
	} else {
		bPorta = false;
	}
	if (!bPorta
	    || ((pChn->dwFlags & CHN_NOTEFADE) && !pChn->nFadeOutVol)
	    || ((csf->m_dwSongFlags & SONG_COMPATGXX) && pChn->nRowInstr)) {
		if ((pChn->dwFlags & CHN_NOTEFADE) && !pChn->nFadeOutVol) {
			pChn->nVolEnvPosition = 0;
			pChn->nPanEnvPosition = 0;
			pChn->nPitchEnvPosition = 0;
			pChn->nAutoVibDepth = 0;
			pChn->nAutoVibPos = 0;
			pChn->dwFlags &= ~CHN_NOTEFADE;
			pChn->nFadeOutVol = 65536;
		}
		if (!bPorta || !(csf->m_dwSongFlags & SONG_COMPATGXX) || pChn->nRowInstr) {
			pChn->dwFlags &= ~CHN_NOTEFADE;
			pChn->nFadeOutVol = 65536;
		}
	}
	pChn->dwFlags &= ~CHN_KEYOFF;
	// Enable Ramping
	if (!bPorta) {
		//pChn->nVUMeter = 0x100;
		pChn->strike = 4; /* this affects how long the initial hit on the playback marks lasts */
		pChn->nLeftVU = pChn->nRightVU = 0xFF;
		pChn->dwFlags &= ~CHN_FILTER;
		pChn->dwFlags |= CHN_FASTVOLRAMP;
		if (bResetEnv) {
			pChn->nVolSwing = pChn->nPanSwing = 0;
			if (penv) {
				/* This is done above as well, with the instrument reset, but
				 * I have a feeling that maybe it should only be here. Tests? */
				pChn->dwFlags &= ~(CHN_VOLENV | CHN_PANENV | CHN_PITCHENV);
				if (penv->dwFlags & ENV_VOLUME) pChn->dwFlags |= CHN_VOLENV;
				if (penv->dwFlags & ENV_PANNING) pChn->dwFlags |= CHN_PANENV;
				if (penv->dwFlags & ENV_PITCH) pChn->dwFlags |= CHN_PITCHENV;
				
				if (!(penv->dwFlags & ENV_VOLCARRY)) pChn->nVolEnvPosition = 0;
				if (!(penv->dwFlags & ENV_PANCARRY)) pChn->nPanEnvPosition = 0;
				if (!(penv->dwFlags & ENV_PITCHCARRY)) pChn->nPitchEnvPosition = 0;
				// Volume Swing
				if (penv->nVolSwing) {
					/* this was wrong */
					int d = ((int32_t)penv->nVolSwing*(int32_t)((rand() & 0xFF) - 0x7F)) / 256;
					pChn->nVolSwing = (signed short)((d * pChn->nVolume + 1)/256);
				}
				// Pan Swing
				if (penv->nPanSwing) {
					int d = ((int32_t)penv->nPanSwing*(int32_t)((rand() & 0xFF) - 0x7F)) / 128;
					pChn->nPanSwing = (signed short)d;
				}
			}
			pChn->nAutoVibDepth = 0;
			pChn->nAutoVibPos = 0;
		}
		pChn->nLeftVol = pChn->nRightVol = 0;
		// Setup Initial Filter for this note
		if (penv) {
			bool bFlt = false;
			if (penv->nIFR & 0x80) {
				pChn->nResonance = penv->nIFR & 0x7F;
				bFlt = true;
			}
			if (penv->nIFC & 0x80) {
				pChn->nCutOff = penv->nIFC & 0x7F;
				bFlt = true;
			}

			if (pChn->nCutOff < 0x7F && bFlt)
				setup_channel_filter(pChn, true, 256, csf->gdwMixingFreq);
		} else {
			pChn->nVolSwing = pChn->nPanSwing = 0;
		}
	}
	// Special case for MPT
	if (bManual)
		pChn->dwFlags &= ~CHN_MUTE;
	if (((pChn->dwFlags & CHN_MUTE) && (csf->gdwSoundSetup & SNDMIX_MUTECHNMODE))
	    || (pChn->pInstrument && (pChn->pInstrument->uFlags & CHN_MUTE) && !bManual)
	    || ((csf->m_dwSongFlags & SONG_INSTRUMENTMODE) && pChn->pHeader
	        && (pChn->pHeader->dwFlags & ENV_MUTE) && !bManual)) {
		if (!bManual)
			pChn->nPeriod = 0;
	}

}


uint32_t csf_get_nna_channel(CSoundFile *csf, uint32_t nChn)
{
	SONGVOICE *pChn = &csf->Voices[nChn];
	// Check for empty channel
	SONGVOICE *pi = &csf->Voices[csf->m_nChannels];
	for (uint32_t i=csf->m_nChannels; i<MAX_VOICES; i++, pi++) {
		if (!pi->nLength) {
			if (pi->dwFlags & CHN_MUTE) {
				if (pi->dwFlags & CHN_NNAMUTE) {
					pi->dwFlags &= ~(CHN_NNAMUTE|CHN_MUTE);
				} else {
					/* this channel is muted; skip */
					continue;
				}
			}
			return i;
		}
	}
	if (!pChn->nFadeOutVol) return 0;
	// All channels are used: check for lowest volume
	uint32_t result = 0;
	uint32_t vol = 64*65536;	// 25%
	int envpos = 0xFFFFFF;
	const SONGVOICE *pj = &csf->Voices[csf->m_nChannels];
	for (uint32_t j=csf->m_nChannels; j<MAX_VOICES; j++, pj++) {
		if (!pj->nFadeOutVol) return j;
		uint32_t v = pj->nVolume;
		if (pj->dwFlags & CHN_NOTEFADE)
			v = v * pj->nFadeOutVol;
		else
			v <<= 16;
		if (pj->dwFlags & CHN_LOOP) v >>= 1;
		if (v < vol || (v == vol && pj->nVolEnvPosition > envpos)) {
			envpos = pj->nVolEnvPosition;
			vol = v;
			result = j;
		}
	}
	if (result) {
		/* unmute new nna channel */
		csf->Voices[result].dwFlags &= ~(CHN_MUTE|CHN_NNAMUTE);
	}
	return result;
}


void csf_check_nna(CSoundFile *csf, uint32_t nChn, uint32_t instr, int note, bool bForceCut)
{
        SONGVOICE *p;
	SONGVOICE *pChn = &csf->Voices[nChn];
	SONGINSTRUMENT *penv = (csf->m_dwSongFlags & SONG_INSTRUMENTMODE) ? pChn->pHeader : NULL;
	SONGINSTRUMENT *pHeader;
	signed char *pSample;
	if (!NOTE_IS_NOTE(note))
		return;
	// Always NNA cut - using
	if (bForceCut || !(csf->m_dwSongFlags & SONG_INSTRUMENTMODE)) {
		if (!pChn->nLength || (pChn->dwFlags & CHN_MUTE) || (!pChn->nLeftVol && !pChn->nRightVol))
			return;
		uint32_t n = csf_get_nna_channel(csf, nChn);
		if (!n) return;
		p = &csf->Voices[n];
		// Copy Channel
		*p = *pChn;
		p->dwFlags &= ~(CHN_VIBRATO|CHN_TREMOLO|CHN_PANBRELLO|CHN_PORTAMENTO);
		p->nMasterChn = nChn+1;
		p->nCommand = 0;
		// Cut the note
		p->nFadeOutVol = 0;
		p->dwFlags |= (CHN_NOTEFADE|CHN_FASTVOLRAMP);
		// Stop this channel
		pChn->nLength = pChn->nPos = pChn->nPosLo = 0;
		pChn->nROfs = pChn->nLOfs = 0;
		pChn->nLeftVol = pChn->nRightVol = 0;
		OPL_NoteOff(nChn);
		OPL_Touch(nChn, 0);
		GM_KeyOff(nChn);
		GM_Touch(nChn, 0);
		return;
	}
	if (instr >= MAX_INSTRUMENTS) instr = 0;
	pSample = pChn->pSample;
	pHeader = pChn->pHeader;
	if (instr && note) {
		pHeader = (csf->m_dwSongFlags & SONG_INSTRUMENTMODE) ? csf->Instruments[instr] : NULL;
		if (pHeader) {
			uint32_t n = 0;
			if (!NOTE_IS_CONTROL(note)) {
				n = pHeader->Keyboard[note-1];
				note = pHeader->NoteMap[note-1];
				if (n && n < MAX_SAMPLES)
					pSample = csf->Samples[n].pSample;
			}
		} else {
			pSample = NULL;
		}
	}
	if (!penv) return;
	p = pChn;
	for (uint32_t i=nChn; i<MAX_VOICES; p++, i++) {
		if (!((i >= csf->m_nChannels || p == pChn)
		      && ((p->nMasterChn == nChn+1 || p == pChn)
		          && p->pHeader)))
			continue;
		bool bOk = false;
		// Duplicate Check Type
		switch (p->pHeader->nDCT) {
		case DCT_NOTE:
			if (NOTE_IS_NOTE(note) && (int) p->nNote == note && pHeader == p->pHeader)
				bOk = true;
			break;
		case DCT_SAMPLE:
			if (pSample && pSample == p->pSample)
				bOk = true;
			break;
		case DCT_INSTRUMENT:
			if (pHeader == p->pHeader)
				bOk = true;
			break;
		}
		// Duplicate Note Action
		if (bOk) {
			switch(p->pHeader->nDCA) {
			case DCA_NOTECUT:
				fx_key_off(csf, i);
				p->nVolume = 0;
				break;
			case DCA_NOTEOFF:
				fx_key_off(csf, i);
				break;
			case DCA_NOTEFADE:
				p->dwFlags |= CHN_NOTEFADE;
				break;
			}
			if (!p->nVolume) {
				p->nFadeOutVol = 0;
				p->dwFlags |= (CHN_NOTEFADE|CHN_FASTVOLRAMP);
			}
		}
	}
	if (pChn->dwFlags & CHN_MUTE)
		return;
	// New Note Action
	if (pChn->nVolume && pChn->nLength) {
		uint32_t n = csf_get_nna_channel(csf, nChn);
		if (n) {
			p = &csf->Voices[n];
			// Copy Channel
			*p = *pChn;
			p->dwFlags &= ~(CHN_VIBRATO|CHN_TREMOLO|CHN_PANBRELLO|CHN_PORTAMENTO);
			p->nMasterChn = nChn+1;
			p->nCommand = 0;
			// Key Off the note
			switch(pChn->nNNA) {
			case NNA_NOTEOFF:
				fx_key_off(csf, n);
				break;
			case NNA_NOTECUT:
				p->nFadeOutVol = 0;
			case NNA_NOTEFADE:
				p->dwFlags |= CHN_NOTEFADE;
				break;
			}
			if (!p->nVolume) {
				p->nFadeOutVol = 0;
				p->dwFlags |= (CHN_NOTEFADE|CHN_FASTVOLRAMP);
			}
			// Stop this channel
			pChn->nLength = pChn->nPos = pChn->nPosLo = 0;
			pChn->nROfs = pChn->nLOfs = 0;
		}
	}
}


void csf_process_effects(CSoundFile *csf)
{
	SONGVOICE *pChn = csf->Voices;
	for (uint32_t nChn=0; nChn<csf->m_nChannels; nChn++, pChn++) {
		pChn->nCommand=0;

		uint32_t instr = pChn->nRowInstr;
		uint32_t volcmd = pChn->nRowVolCmd;
		uint32_t vol = pChn->nRowVolume;
		uint32_t cmd = pChn->nRowCommand;
		uint32_t param = pChn->nRowParam;
		bool bPorta = (cmd == CMD_TONEPORTAMENTO
		               || cmd == CMD_TONEPORTAVOL
		               || volcmd == VOLCMD_TONEPORTAMENTO);
		uint32_t nStartTick = (csf->m_dwSongFlags & SONG_FIRSTTICK) ? 0 : -1;

		pChn->dwFlags &= ~CHN_FASTVOLRAMP;
		// Process special effects (note delay, pattern delay, pattern loop)
		// FIXME why are these here and not with the rest of them?
		if (cmd == CMD_S3MCMDEX) {
			int nloop; // g++ is dumb
			if (param)
				pChn->nOldCmdEx = param;
			else
				param = pChn->nOldCmdEx;
			switch (param >> 4) {
			case 0xd:
				// Note Delay
				nStartTick = (param & 0x0F) ?: 1;
				break;
			case 0xb:
				// Pattern loop
				if (!(csf->m_dwSongFlags & SONG_FIRSTTICK))
					break;
				nloop = fx_pattern_loop(csf, pChn, param & 0x0F);
				if (nloop >= 0)
					csf->m_nProcessRow = nloop - 1;
				break;
			}
		}

		// Handles note/instrument/volume changes
		// m_nTickCount decrements from speed, and is always nonzero
		// thus (m_nMusicSpeed - m_nTickCount) indicates how many ticks we are from zero
		// nStartTick is the n'th tick on the row that the note should fire on
		if (instr) pChn->nNewIns = instr;
		if ((csf->m_nMusicSpeed - csf->m_nTickCount) == nStartTick) {
			uint32_t note = pChn->nRowNote;
			if (instr && note == NOTE_NONE) {
				if (csf->m_dwSongFlags & SONG_INSTRUMENTMODE) {
					if (pChn->pInstrument)
						pChn->nVolume = pChn->pInstrument->nVolume;
				} else {
					if (instr < MAX_SAMPLES)
						pChn->nVolume = csf->Samples[instr].nVolume;
				}
			}
			// Invalid Instrument ?
			if (instr >= MAX_INSTRUMENTS) instr = 0;
			// Note Cut/Off => ignore instrument
			if ((NOTE_IS_CONTROL(note)) || (note != NOTE_NONE && !bPorta)) {
				/* This is required when the instrument changes (KeyOff is not called) */
				/* Possibly a better bugfix could be devised. --Bisqwit */
				OPL_NoteOff(nChn);
				OPL_Touch(nChn, 0);
				GM_KeyOff(nChn);
				GM_Touch(nChn, 0);
			}

			if (NOTE_IS_CONTROL(note)) {
				instr = 0;
			} else if (NOTE_IS_NOTE(note)) {
				pChn->nNewNote = note;
				// New Note Action ? (not when paused!!!)
				if (!bPorta)
					csf_check_nna(csf, nChn, instr, note, false);
			}
			// Instrument Change ?
			if (instr) {
				SONGSAMPLE *psmp = pChn->pInstrument;
				csf_instrument_change(csf, pChn, instr, bPorta, true, true);
				OPL_Patch(nChn, csf->Samples[instr].AdlibBytes);
				
				if((csf->m_dwSongFlags & SONG_INSTRUMENTMODE) && csf->Instruments[instr])
					GM_DPatch(nChn, csf->Instruments[instr]->nMidiProgram,
						csf->Instruments[instr]->wMidiBank,
						csf->Instruments[instr]->nMidiChannelMask);
				
				pChn->nNewIns = 0;
				// Special IT case: portamento+note causes sample change -> ignore portamento
				if (psmp != pChn->pInstrument && NOTE_IS_NOTE(note)) {
					bPorta = false;
				}
			}
			// New Note ?
			if (note != NOTE_NONE) {
				if (!instr && pChn->nNewIns && NOTE_IS_NOTE(note)) {
					csf_instrument_change(csf, pChn, pChn->nNewIns, bPorta, false, true);
					if ((csf->m_dwSongFlags & SONG_INSTRUMENTMODE)
					    && csf->Instruments[pChn->nNewIns]) {
						OPL_Patch(nChn, csf->Samples[pChn->nNewIns].AdlibBytes);
						GM_DPatch(nChn, csf->Instruments[pChn->nNewIns]->nMidiProgram,
							csf->Instruments[pChn->nNewIns]->wMidiBank,
							csf->Instruments[pChn->nNewIns]->nMidiChannelMask);
					}
					pChn->nNewIns = 0;
				}
				csf_note_change(csf, nChn, note, bPorta, true, false);
			}
			// Tick-0 only volume commands
			if (volcmd == VOLCMD_VOLUME) {
				if (vol > 64) vol = 64;
				pChn->nVolume = vol << 2;
				pChn->dwFlags |= CHN_FASTVOLRAMP;
			} else if (volcmd == VOLCMD_PANNING) {
				if (vol > 64) vol = 64;
				pChn->nPan = vol << 2;
				pChn->dwFlags |= CHN_FASTVOLRAMP;
				pChn->dwFlags &= ~CHN_SURROUND;
			}
		}

		// Volume Column Effect (except volume & panning)
		if (volcmd > VOLCMD_PANNING) {
			if (volcmd == VOLCMD_TONEPORTAMENTO) {
				fx_tone_portamento(csf->m_dwSongFlags, pChn,
					ImpulseTrackerPortaVolCmd[vol & 0x0F]);
			} else {
				if (vol)
					pChn->nOldVolParam = vol;
				else
					vol = pChn->nOldVolParam;
				switch(volcmd) {
				case VOLCMD_VOLSLIDEUP:
					fx_volume_slide(csf->m_dwSongFlags, pChn, vol << 4);
					break;

				case VOLCMD_VOLSLIDEDOWN:
					fx_volume_slide(csf->m_dwSongFlags, pChn, vol);
					break;

				case VOLCMD_FINEVOLUP:
					if (csf->m_nTickCount == nStartTick)
						fx_volume_slide(csf->m_dwSongFlags, pChn, (vol << 4) | 0x0F);
					break;

				case VOLCMD_FINEVOLDOWN:
					if (csf->m_nTickCount == nStartTick)
						fx_volume_slide(csf->m_dwSongFlags, pChn, 0xF0 | vol);
					break;

				case VOLCMD_VIBRATOSPEED:
					fx_vibrato(pChn, vol << 4);
					break;

				case VOLCMD_VIBRATO:
					fx_vibrato(pChn, vol);
					break;

				case VOLCMD_PANSLIDELEFT:
					fx_panning_slide(csf->m_dwSongFlags, pChn, vol);
					break;

				case VOLCMD_PANSLIDERIGHT:
					fx_panning_slide(csf->m_dwSongFlags, pChn, vol << 4);
					break;

				case VOLCMD_PORTAUP:
					fx_portamento_up(csf->m_dwSongFlags, pChn, vol << 2);
					break;

				case VOLCMD_PORTADOWN:
					fx_portamento_down(csf->m_dwSongFlags, pChn, vol << 2);
					break;
				}
			}
		}

		// Effects
		switch (cmd) {
		case 0:
			break;
		// Set Volume
		case CMD_VOLUME:
			if (!(csf->m_dwSongFlags & SONG_FIRSTTICK))
				break;
			pChn->nVolume = (param < 64) ? param*4 : 256;
			pChn->dwFlags |= CHN_FASTVOLRAMP;
			for (uint32_t i=csf->m_nChannels; i<MAX_VOICES; i++) {
				SONGVOICE *c = &csf->Voices[i];
				if (c->nMasterChn == nChn + 1) {
					c->nVolume = pChn->nVolume;
					c->dwFlags |= CHN_FASTVOLRAMP;
				}
			}
			break;

		// Portamento Up
		case CMD_PORTAMENTOUP:
			fx_portamento_up(csf->m_dwSongFlags, pChn, param);
			break;

		// Portamento Down
		case CMD_PORTAMENTODOWN:
			fx_portamento_down(csf->m_dwSongFlags, pChn, param);
			break;

		// Volume Slide
		case CMD_VOLUMESLIDE:
			fx_volume_slide(csf->m_dwSongFlags, pChn, param);
			break;

		// Tone-Portamento
		case CMD_TONEPORTAMENTO:
			fx_tone_portamento(csf->m_dwSongFlags, pChn, param);
			break;

		// Tone-Portamento + Volume Slide
		case CMD_TONEPORTAVOL:
			fx_volume_slide(csf->m_dwSongFlags, pChn, param);
			fx_tone_portamento(csf->m_dwSongFlags, pChn, 0);
			break;

		// Vibrato
		case CMD_VIBRATO:
			fx_vibrato(pChn, param);
			break;

		// Vibrato + Volume Slide
		case CMD_VIBRATOVOL:
			fx_volume_slide(csf->m_dwSongFlags, pChn, param);
			fx_vibrato(pChn, 0);
			break;

		// Set Speed
		case CMD_SPEED:
			if ((csf->m_dwSongFlags & SONG_FIRSTTICK) && param) {
				csf->m_nTickCount = param;
				csf->m_nMusicSpeed = param;
			}
			break;

		// Set Tempo
		case CMD_TEMPO:
			if (csf->m_dwSongFlags & SONG_FIRSTTICK) {
				if (param)
					pChn->nOldTempo = param;
				else
					param = pChn->nOldTempo;
				if (param >= 0x20)
					csf->m_nMusicTempo = param;
			} else {
                                param = pChn->nOldTempo; // this just got set on tick zero

                                switch (param >> 4) {
                                case 0:
                                        csf->m_nMusicTempo -= param & 0xf;
                                        if (csf->m_nMusicTempo < 32)
                                                csf->m_nMusicTempo = 32;
                                        break;
                                case 1:
                                        csf->m_nMusicTempo += param & 0xf;
                                        if (csf->m_nMusicTempo > 255)
                                                csf->m_nMusicTempo = 255;
                                        break;
                                }
                        }
			break;

		// Set Offset
		case CMD_OFFSET:
			if (!(csf->m_dwSongFlags & SONG_FIRSTTICK))
				break;
			if (param)
				pChn->nOldOffset = param;
			else
				param = pChn->nOldOffset;
			param <<= 8;
			param |= (uint32_t)(pChn->nOldHiOffset) << 16;
			if (NOTE_IS_NOTE(pChn->nRowNote)) {
				if (bPorta)
					pChn->nPos = param;
				else
					pChn->nPos += param;
				if (pChn->nPos >= pChn->nLength) {
					pChn->nPos = pChn->nLoopStart;
					if ((csf->m_dwSongFlags & SONG_ITOLDEFFECTS) && pChn->nLength > 4) {
						pChn->nPos = pChn->nLength - 2;
					}
				}
			}
			break;

		// Arpeggio
		case CMD_ARPEGGIO:
			pChn->nCommand = CMD_ARPEGGIO;
			if (!(csf->m_dwSongFlags & SONG_FIRSTTICK))
				break;
			if (!pChn->nPeriod || pChn->nNote == NOTE_NONE)
				break;
			if (param)
				pChn->nArpeggio = param;
			break;

		// Retrig
		case CMD_RETRIG:
			if (param)
				pChn->nRetrigParam = param & 0xFF;
			fx_retrig_note(csf, nChn, pChn->nRetrigParam);
			break;

		// Tremor
		case CMD_TREMOR:
			pChn->nCommand = CMD_TREMOR;
			if ((csf->m_dwSongFlags & SONG_FIRSTTICK) && param)
				pChn->nTremorParam = param;
			break;

		// Set Global Volume
		case CMD_GLOBALVOLUME:
			if (!(csf->m_dwSongFlags & SONG_FIRSTTICK))
				break;
			if (param <= 128)
				csf->m_nGlobalVolume = param;
			break;

		// Global Volume Slide
		case CMD_GLOBALVOLSLIDE:
			fx_global_vol_slide(csf, pChn, param);
			break;

		// Set 8-bit Panning (Xxx)
		case CMD_PANNING8:
			if (!(csf->m_dwSongFlags & SONG_FIRSTTICK))
				break;
			if (!(csf->m_dwSongFlags & SONG_SURROUNDPAN))
				pChn->dwFlags &= ~CHN_SURROUND;
			pChn->nPan = param;
			pChn->dwFlags |= CHN_FASTVOLRAMP;
			break;

		// Panning Slide
		case CMD_PANNINGSLIDE:
			fx_panning_slide(csf->m_dwSongFlags, pChn, param);
			break;

		// Tremolo
		case CMD_TREMOLO:
			fx_tremolo(pChn, param);
			break;

		// Fine Vibrato
		case CMD_FINEVIBRATO:
			fx_fine_vibrato(pChn, param);
			break;

		// S3M/IT Sxx Extended Commands
		case CMD_S3MCMDEX:
			fx_extended_s3m(csf, nChn, param);
			break;

		// Key Off
		case CMD_KEYOFF:
			if (!(csf->m_dwSongFlags & SONG_FIRSTTICK))
				break;
			fx_key_off(csf, nChn);
			break;

		// Extra-fine porta up/down
		case CMD_XFINEPORTAUPDOWN:
			switch(param & 0xF0) {
			case 0x10:
				fx_extra_fine_portamento_up(csf->m_dwSongFlags, pChn, param & 0x0F);
				break;
			case 0x20:
				fx_extra_fine_portamento_down(csf->m_dwSongFlags, pChn, param & 0x0F);
				break;
			// Modplug XM Extensions
			case 0x50:
			case 0x60:
			case 0x70:
			case 0x90:
			case 0xA0:
				fx_extended_s3m(csf, nChn, param);
				break;
			}
			break;

		// Set Channel Global Volume
		case CMD_CHANNELVOLUME:
			if (!(csf->m_dwSongFlags & SONG_FIRSTTICK))
				break;
			if (param <= 64) {
				pChn->nGlobalVol = param;
				pChn->dwFlags |= CHN_FASTVOLRAMP;
				for (uint32_t i=csf->m_nChannels; i<MAX_VOICES; i++) {
					SONGVOICE *c = &csf->Voices[i];
					if (c->nMasterChn == nChn + 1) {
						c->nGlobalVol = param;
						c->dwFlags |= CHN_FASTVOLRAMP;
					}
				}
			}
			break;

		// Channel volume slide
		case CMD_CHANNELVOLSLIDE:
			{
				int saw_self = 0;

				for (uint32_t i=csf->m_nChannels; i<MAX_VOICES; i++) {
					SONGVOICE *c = &csf->Voices[i];
					if (c->nMasterChn == nChn + 1) {
						if (c == pChn)
							saw_self = 1;
						fx_channel_vol_slide(csf->m_dwSongFlags, c, param);
					}
				}
				if (!saw_self)
					fx_channel_vol_slide(csf->m_dwSongFlags, pChn, param);
			}
			
			break;

		// Panbrello (IT)
		case CMD_PANBRELLO:
			fx_panbrello(pChn, param);
			break;

		// Set Envelope Position
		case CMD_SETENVPOSITION:
			if (!(csf->m_dwSongFlags & SONG_FIRSTTICK))
				break;
			pChn->nVolEnvPosition = param;
			pChn->nPanEnvPosition = param;
			pChn->nPitchEnvPosition = param;
			if ((csf->m_dwSongFlags & SONG_INSTRUMENTMODE) && pChn->pHeader) {
				SONGINSTRUMENT *penv = pChn->pHeader;
				if ((pChn->dwFlags & CHN_PANENV)
				    && (penv->PanEnv.nNodes)
				    && ((int)param > penv->PanEnv.Ticks[penv->PanEnv.nNodes-1])) {
					pChn->dwFlags &= ~CHN_PANENV;
				}
			}
			break;

		// Position Jump
		case CMD_POSITIONJUMP:
			if (csf->m_dwSongFlags & SONG_FIRSTTICK) {
				csf->m_nProcessOrder = param - 1;
				csf->m_nProcessRow = PROCESS_NEXT_ORDER;
			}
			break;

		// Pattern Break
		case CMD_PATTERNBREAK:
			if (csf->m_dwSongFlags & SONG_FIRSTTICK) {
				csf->m_nBreakRow = param;
				csf->m_nProcessRow = PROCESS_NEXT_ORDER;
			}
			break;

		// Midi Controller
		case CMD_MIDI:
			if (!(csf->m_dwSongFlags & SONG_FIRSTTICK))
				break;
			if (param < 0x80) {
				csf_process_midi_macro(csf, nChn,
					&csf->m_MidiCfg.szMidiSFXExt[pChn->nActiveMacro << 5],
					param, 0, 0, 0);
			} else {
				csf_process_midi_macro(csf, nChn,
					&csf->m_MidiCfg.szMidiZXXExt[(param & 0x7F) << 5],
					0, 0, 0, 0);
			}
			break;
		}
	}
}

