// Schism Tracker - a cross-platform Impulse Tracker clone
// copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
// copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
// URL: http://nimh.org/schism/
// URL: http://rigelseven.com/schism/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include "headers.h"

#include "it.h"
#include "page.h"
#include "mplink.h"
#include "slurp.h"
#include "config-parser.h"

#include "diskwriter.h"
#include "event.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cmath>

#include "sdlmain.h"

#include "midi.h"

static int midi_playing;
// ------------------------------------------------------------------------

unsigned int samples_played = 0;
unsigned int max_channels_used = 0;

static signed short audio_buffer_[16726];

signed short *audio_buffer = audio_buffer_;
unsigned int audio_buffer_size = 0;

unsigned int audio_output_channels = 2;
unsigned int audio_output_bits = 16;

static unsigned int audio_sample_size;

struct audio_settings audio_settings;

static void _schism_midi_out_note(int chan, const MODCOMMAND *m);
static void _schism_midi_out_raw(unsigned char *data, unsigned int len, unsigned int delay);

unsigned int song_buffer_msec(void)
{
	unsigned int nm;
	nm = CSoundFile::gdwMixingFreq / audio_buffer_size;
	return nm;
}

// ------------------------------------------------------------------------
// playback

extern "C" {
	extern int midi_bend_hit[64], midi_last_bend_hit[64];
};
// this gets called from sdl
static void audio_callback(UNUSED void *qq, Uint8 * stream, int len)
{
	int i, n;

	if (!stream || !len || !mp) {
		song_stop_unlocked();
		return;
	}

        if (mp->m_dwSongFlags & SONG_ENDREACHED) {
		n = 0;
	} else {
        	n = mp->Read(stream, len);
	        if (!n) {
			song_stop_unlocked();
			return;
		}
		samples_played += n;
	}
	
	if (n < len) {
		memmove(audio_buffer, audio_buffer + (len-n),
				(len-(len - n)) * audio_sample_size);
	}
	memcpy(audio_buffer, stream, n * audio_sample_size);

	if (audio_output_bits == 8) {
		/* libmodplug emits unsigned 8bit output...
		*/
		stream = (Uint8*)audio_buffer;
		n *= audio_output_channels;
		for (i = 0; i < n; i++) {
			stream[i] ^= 128;
		}
	}
	
	if (mp->m_nMixChannels > max_channels_used)
		max_channels_used = MIN(mp->m_nMixChannels, mp->m_nMaxMixChannels);

	/* send at end */
	SDL_Event e;
	e.user.type = SCHISM_EVENT_PLAYBACK;
	e.user.code = 0;
	e.user.data1 = 0;
	e.user.data2 = 0;
	SDL_PushEvent(&e);
}

// ------------------------------------------------------------------------------------------------------------
// note playing

static void main_song_mode_changed_cb(void)
{
	int n;
	for (n = 0; n < PAGE_LAST_PAGE; n++) {
		if (pages[n].song_mode_changed_cb)
			pages[n].song_mode_changed_cb();
	}
}


static int current_play_channel = 1;
static int multichannel_mode = 0;

void song_change_current_play_channel(int relative, int wraparound)
{
	current_play_channel += relative;
	if (wraparound) {
		if (current_play_channel < 1)
			current_play_channel = 64;
		else if (current_play_channel > 64)
			current_play_channel = 1;
	} else {
		current_play_channel = CLAMP(current_play_channel, 1, 64);
	}
	status_text_flash("Using channel %d for playback", current_play_channel);
}

void song_toggle_multichannel_mode(void)
{
	multichannel_mode = !multichannel_mode;
	status_text_flash("Multichannel playback %s", (multichannel_mode ? "enabled" : "disabled"));
}
int song_is_multichannel_mode(void)
{
	return multichannel_mode;
}

static int big_song_channels[64];

static int song_keydown_ex(int samp, int ins, int note, int vol,
				int chan, int *mm, int at,
				int effect, int param)
{
	int i;
	MODCHANNEL *c;
	MODCOMMAND mc;
	BOOL porta;
	int eff;

	if (chan > -1 && !mm) {
		if (ins < 0 && samp < 0) return chan;

		song_lock_audio();

		c = mp->Chn + chan;
		if (at) {
			c->nVolume = (vol << 2);
			song_unlock_audio();
			return chan;
		}

		c->nTickStart = mp->m_nTickCount;

		c->nInc = 1;
		if (chan > 64) {
			c->nPos = c->nPosLo = c->nLength = 0;
			c->dwFlags &= 0xff;
			c->dwFlags &= ~(CHN_MUTE|CHN_PINGPONGFLAG);
			c->nGlobalVol = 64;
			c->nInsVol = 64;
			c->nPan = 128;
			c->nRightVol = c->nLeftVol = 0;
			c->nROfs = c->nLOfs = 0;
			c->nCutOff = 0x7f;
			c->nResonance = 0;
		}

		c->nNewNote = note;

		porta = FALSE;
		if (effect) {
			/* libmodplug really should do this for us */
			if (effect == CMD_TONEPORTAMENTO
			|| effect == CMD_TONEPORTAVOL)  porta = TRUE;
		}


		if (ins > -1 && song_is_instrument_mode()) {
			if ((note) && (note <= 128) && (!porta)) {
				mp->CheckNNA(chan, ins, note, FALSE);
			}
			if (mp->Headers[ins]) {
				c->nVolEnvPosition = 0;
				c->nPanEnvPosition = 0;
				c->nPitchEnvPosition = 0;
				mp->InstrumentChange(c, ins, FALSE, TRUE, FALSE);
				c->nNewIns = 0;
			}
		} else if (samp > -1) {
			MODINSTRUMENT *i = mp->Ins + samp;
			c->pCurrentSample = i->pSample;
			c->pHeader = NULL;
			c->pInstrument = i;
			c->pSample = i->pSample;
			c->nFineTune = i->nFineTune;
			c->nC4Speed = i->nC4Speed;
			c->nLoopStart = i->nLoopStart;
			c->nLoopEnd = i->nLoopEnd;
			c->dwFlags = i->uFlags & (0xFF & ~CHN_MUTE);
			c->nPan = 128; // redundant?
			c->nInsVol = i->nGlobalVol;
			c->nFadeOutVol = 0x10000;
			i->played = 1;
		}
		c->nVolume = (vol << 2);
		mp->NoteChange(chan, note, porta, true, true);
		c->nMasterChn = 0;
		if (porta && ins > -1 && song_is_instrument_mode()) {
			c->dwFlags |= CHN_FASTVOLRAMP;
			c->nVolEnvPosition = 0;
			c->nPanEnvPosition = 0;
			c->nPitchEnvPosition = 0;
			c->nAutoVibDepth = 0;
			c->nAutoVibPos = 0;
		}

		if (effect) {
			c->nCommand = 0;
			switch (effect) {
			case CMD_VOLUME:
				c->nVolume = (param < 64) ? param*4 : 256;
				c->dwFlags |= CHN_FASTVOLRAMP;
				break;
			case CMD_PORTAMENTOUP:
				mp->PortamentoUp(c, param);
				break;
			case CMD_PORTAMENTODOWN:
				mp->PortamentoDown(c, param);
				break;
			case CMD_VOLUMESLIDE:
				mp->VolumeSlide(c, param);
				break;
			case CMD_TONEPORTAMENTO:
				mp->TonePortamento(c, param);
				break;
			case CMD_TONEPORTAVOL:
				mp->VolumeSlide(c, param);
				mp->TonePortamento(c, 0);
				break;
			case CMD_VIBRATO:
				mp->Vibrato(c, param);
				break;
			case CMD_VIBRATOVOL:
				mp->VolumeSlide(c, param);
				mp->Vibrato(c, 0);
				break;
			case CMD_OFFSET:
				if (param) c->nOldOffset = param;
				else param = c->nOldOffset;
				param <<= 8;
				param |= (unsigned int)(c->nOldHiOffset)<<16;
				if (porta) c->nPos = param;
				else c->nPos += param;
				if (c->nPos >= c->nLength) {
					c->nPos = c->nLoopStart;
					if (mp->m_dwSongFlags & SONG_ITOLDEFFECTS && (c->nLength > 4))
						c->nPos = c->nLength-2;
				}
				break;
			case CMD_ARPEGGIO:
				c->nCommand = CMD_ARPEGGIO;
				if (param) c->nArpeggio = param;
				break;
			case CMD_RETRIG:
				if (param) c->nRetrigParam = param & 255;
				else param = c->nRetrigParam;
				c->nCommand = CMD_RETRIG;
				mp->RetrigNote(chan, param);
				break;
			case CMD_TREMOR:
				c->nCommand = CMD_TREMOR;
				if (param) c->nTremorParam = param;
				break;
			case CMD_GLOBALVOLUME:
				if (mp->m_nType != MOD_TYPE_IT) param <<= 1;
				if (param > 128) param = 128;
				mp->m_nGlobalVolume = param << 1;
				break;
			case CMD_GLOBALVOLSLIDE:
				mp->GlobalVolSlide(param);
				break;
			case CMD_PANNING8:
				c->dwFlags &= ~CHN_SURROUND;
				c->nPan = param;
				c->dwFlags |= CHN_FASTVOLRAMP;
				break;
			case CMD_PANNINGSLIDE:
				mp->PanningSlide(c, param);
				break;
			case CMD_TREMOLO:
				mp->Tremolo(c, param);
				break;
			case CMD_FINEVIBRATO:
				mp->FineVibrato(c, param);
				break;
			case CMD_MODCMDEX:
				mp->ExtendedMODCommands(chan, param);
				break;
			case CMD_S3MCMDEX:
				mp->ExtendedS3MCommands(chan, param);
				break;
			case CMD_KEYOFF:
				mp->KeyOff(chan);
				break;
			case CMD_XFINEPORTAUPDOWN:
				switch(param & 0xF0) {
				case 0x10: mp->ExtraFinePortamentoUp(c, param & 0x0F); break;
				case 0x20: mp->ExtraFinePortamentoDown(c, param & 0x0F); break;
				// Modplug XM Extensions
				case 0x50:
				case 0x60:
				case 0x70:
				case 0x90:
				case 0xA0: mp->ExtendedS3MCommands(chan, param); break;
				};
				break;
			case CMD_CHANNELVOLUME:
				if (param <= 64) {
					c->nGlobalVol = param;
					c->dwFlags |= CHN_FASTVOLRAMP;
				}
				break;
			case CMD_CHANNELVOLSLIDE:
				mp->ChannelVolSlide(c, param);
				break;
			case CMD_PANBRELLO:
				mp->Panbrello(c, param);
				break;
			case CMD_SETENVPOSITION:
                                c->nVolEnvPosition = param;
                                c->nPanEnvPosition = param;
                                c->nPitchEnvPosition = param;
                                if ((mp->m_dwSongFlags & SONG_INSTRUMENTMODE) && c->pHeader) {
                                        INSTRUMENTHEADER *penv = c->pHeader;
                                        if ((c->dwFlags & CHN_PANENV) && (penv->PanEnv.nNodes)
					&& (param > penv->PanEnv.Ticks[penv->PanEnv.nNodes-1])) {
						c->dwFlags &= ~CHN_PANENV;
					}
				}
				break;

			case CMD_MIDI:
				if (param < 0x80) {
					mp->ProcessMidiMacro(chan,
		&mp->m_MidiCfg.szMidiSFXExt[ c->nActiveMacro << 5 ], param, ins > 0 ? ins : 0);
				} else {
					mp->ProcessMidiMacro(chan,
		&mp->m_MidiCfg.szMidiSFXExt[ (param & 0x7f) << 5 ], 0, ins > 0 ? ins : 0);
				}
				break;
			};
		}
	
		if (mp->m_dwSongFlags & SONG_ENDREACHED) {
			mp->m_dwSongFlags &= ~SONG_ENDREACHED;
			mp->m_dwSongFlags |= SONG_PAUSED;
		}
	
		song_unlock_audio();
		/* put it back into range as necessary */
		while (chan > 64) chan -= 64;

		if (ins > -1) {
			mc.note = note;
			mc.instr = ins;
			mc.volcmd = VOLCMD_VOLUME;
			mc.vol = vol;
			mc.command = effect;
			mc.param = param;
			song_lock_audio();
			_schism_midi_out_note(chan, &mc);
			song_unlock_audio();
		}

		return chan;
	}

	if (mm) {
		if (chan < 0) chan = 0;
		for (i = chan; i < 64; i++) {
			if (mm[i] == ((note << 1)|1)) {
				return song_keydown_ex(samp, ins, note,
						vol, 64+i,  0, at,
						effect, param);
			}
			if (mm[i] != 1) continue;
			mm[i] = 1 | (note << 1);
			return song_keydown_ex(samp, ins,
						note, vol, 64+i,  0, at,
						effect, param);
		}
		for (i = 0; i < chan; i++) {
			if (mm[i] == ((note << 1)|1)) {
				return song_keydown_ex(samp, ins, note,
						vol, 64+i,  0, at,
						effect, param);
			}
			if (mm[i] != 1) continue;
			mm[i] = 1 | (note << 1);
			return song_keydown_ex(samp, ins, note,
						vol, 64+i,  0, at,
						effect, param);
		}
		/* put it back into range as necessary */
		while (chan > 64) chan -= 64;
		return chan; /* err... */
	} else {
		if (multichannel_mode) song_change_current_play_channel(1,1);

		for (i = 0; i < 64; i++)
			big_song_channels[i] |= 1;

		return song_keydown_ex(samp, ins, note, vol,
					current_play_channel-1,
					big_song_channels, at,
					effect, param);
	}
}

int song_keydown(int samp, int ins, int note, int vol, int chan, int *mm)
{
	return song_keydown_ex(samp,ins,note,vol,chan,mm,0, 0, 0);
}
int song_keyrecord(int samp, int ins, int note, int vol, int chan, int *mm,
				int effect, int param)
{
	return song_keydown_ex(samp,ins,note,vol,chan,mm, 0, effect, param);
}

int song_keyup(int samp, int ins, int note, int chan, int *mm)
{
	int i, j;
	MODCHANNEL *c;
	MODCOMMAND mc;

	if (chan > -1 && !mm) {
		song_lock_audio();
		c = mp->Chn + chan;
		if (samp > -1) {
			mp->NoteChange(chan, NOTE_CUT, false, true, true);
		} else {
			mp->NoteChange(chan, NOTE_OFF, false, true, true);
		}
		song_unlock_audio();

		if (ins > -1) {
			mc.note = NOTE_OFF;
			mc.instr = ins;
			mc.volcmd = 0;
			mc.vol = 0;
			mc.command = 0;
			mc.param = 0;
			song_lock_audio();
			_schism_midi_out_note(chan, &mc);
			song_unlock_audio();
		}
	} else {
		if (!mm) {
			if (multichannel_mode) return -1;
			mm = big_song_channels;
		}
		j = -1;
		for (i = 0; i < 64; i++) {
			if (mm[i] == ((note << 1)|1)) {
				mm[i] = 1;
				j = song_keyup(samp,ins,note,64+i,0);
			}
		}
		if (j > -1) return j;
	}
	/* put it back into range as necessary */
	while (chan > 64) chan -= 64;
	return chan;
}

// useins: play the current instrument if nonzero, else play the current sample with a "blank" instrument,
// i.e. no envelopes, vol/pan swing, nna setting, or note translations. (irrelevant if not instrument mode?)

// ------------------------------------------------------------------------------------------------------------

// this should be called with the audio LOCKED
static void song_reset_play_state()
{
	int n;
	MODCHANNEL *c;
	
	memset(midi_bend_hit, 0, sizeof(midi_bend_hit));
	memset(midi_last_bend_hit, 0, sizeof(midi_last_bend_hit));
	memset(big_song_channels, 0, sizeof(big_song_channels));
	for (n = 0, c = mp->Chn; n < MAX_CHANNELS; n++, c++) {
		c->nLeftVol = c->nNewLeftVol = c->nLeftRamp = c->nLOfs = 0;
		c->nRightVol = c->nNewRightVol = c->nRightRamp = c->nROfs = 0;
		c->nFadeOutVol = c->nLength = c->nLoopStart = c->nLoopEnd = 0;
		c->nNote = c->nNewNote = c->nNewIns = c->nCommand = c->nPeriod = c->nPos = 0;
		c->nPatternLoop = c->nPatternLoopCount = c->nPortamentoDest = c->nTremorCount = 0;
		c->pInstrument = NULL;
		c->pSample = NULL;
		c->pHeader = NULL;
		c->nResonance = 0;
		c->nCutOff = 0x7F;
		c->nVolume = 256;
		if (n < MAX_BASECHANNELS) {
			c->dwFlags = mp->ChnSettings[n].dwFlags;
			c->nPan = mp->ChnSettings[n].nPan;
			c->nGlobalVol = mp->ChnSettings[n].nVolume;
		} else {
			c->dwFlags = 0;
			c->nPan = 128;
			c->nGlobalVol = 64;
		}
	}
	mp->m_nGlobalVolume = mp->m_nDefaultGlobalVolume;
	mp->m_nMusicTempo = mp->m_nDefaultTempo;
	mp->m_nTickCount = mp->m_nMusicSpeed = mp->m_nDefaultSpeed;
	mp->m_nPatternDelay = mp->m_nFrameDelay = 0;

	// turn this crap off
	CSoundFile::gdwSoundSetup &= ~(SNDMIX_NOBACKWARDJUMPS
				| SNDMIX_NOMIXING
				| SNDMIX_DIRECTTODISK);

	// set master volume to be closer to IT's volume
	mp->InitializeDSP(TRUE);
	mp->SetMasterVolume(0x200,1);

	mp->m_nCurrentPattern = 255; // hack...
	mp->m_nNextPattern = 0;
	mp->m_nRow = mp->m_nNextRow = 0;
	mp->m_nRepeatCount = -1;
	mp->m_nBufferCount = 0;
	mp->m_dwSongFlags &= ~(SONG_PAUSED | SONG_STEP | SONG_PATTERNLOOP | SONG_ENDREACHED);

	mp->stop_at_order = -1;
	mp->stop_at_row = -1;
	mp->ResetTimestamps();
	samples_played = 0;
}

void song_start_once()
{
        song_lock_audio();

        song_reset_play_state();
	CSoundFile::gdwSoundSetup |= SNDMIX_NOBACKWARDJUMPS;
        max_channels_used = 0;
	mp->m_nRepeatCount = 1;

        song_unlock_audio();
	main_song_mode_changed_cb();
}

void song_start()
{
        song_lock_audio();

        song_reset_play_state();
        max_channels_used = 0;

        song_unlock_audio();
	main_song_mode_changed_cb();
}
void song_stop()
{
	song_lock_audio();
	song_stop_unlocked();
	song_unlock_audio();
	main_song_mode_changed_cb();
}

/* for midi translation */
static int note_tracker[64];
static int vol_tracker[64];
static int ins_tracker[64];
static int was_program[16];
static int was_banklo[16];
static int was_bankhi[16];

static const MODCOMMAND *last_row[64];
static int last_row_number = -1;

void song_stop_unlocked()
{
	if (!mp) return;

	if (midi_playing) {
		// send all notes off
#define _MIDI_PANIC	"\xb0\x78\0\xb0\x79\0\xb0\x7b\0"
		mp->MidiSend((unsigned char *)_MIDI_PANIC,
			sizeof(_MIDI_PANIC)-1);
		mp->ProcessMidiMacro(0,
			&mp->m_MidiCfg.szMidiGlb[MIDIOUT_STOP*32], // STOP!
			0, 0, 0);
		midi_send_flush(); /* NOW! */

		midi_playing = 0;
	}

	memset(last_row,0,sizeof(last_row));
	last_row_number = -1;

	memset(note_tracker,0,sizeof(note_tracker));
	memset(vol_tracker,0,sizeof(vol_tracker));
	memset(ins_tracker,0,sizeof(ins_tracker));
	memset(was_program,0,sizeof(was_program));
	memset(was_banklo,0,sizeof(was_banklo));
	memset(was_bankhi,0,sizeof(was_bankhi));

	playback_tracing = midi_playback_tracing;

        song_reset_play_state();
        // Modplug doesn't actually have a "stop" mode, but if this is set, mp->Read just returns.
        mp->m_dwSongFlags |= SONG_ENDREACHED;
	
	mp->gnVUMeter = 0;
	mp->gnVULeft = 0;
	mp->gnVURight = 0;
	memset(audio_buffer, 0, audio_buffer_size * audio_sample_size);
}

static int mp_chaseback(int order, int row)
{
	static unsigned char big_buffer[65536];
	if (status.flags & CLASSIC_MODE) return 0; /* no chaseback in classic mode */
	return 0;

/* warning (XXX) this could be really dangerous if diskwriter is running... */

	/* disable mp midi send hooks */
	CSoundFile::_midi_out_note = 0;
	CSoundFile::_midi_out_raw = 0;

	unsigned int lim = 6;

	/* calculate how many rows (distance) */
	int j, k;
	if (mp->Order[order] < MAX_PATTERNS) {
		int size = mp->PatternSize[ mp->Order[order] ];
		if (row < size) size = row;
		for (k = size; lim != 0 && k >= 0; k--) {
			lim--;
		}
	}
	k = 0;
	for (j = order-1; j >= 0 && lim != 0; j--) {
		if (mp->Order[j] >= MAX_PATTERNS) continue;
		int size = mp->PatternSize[ mp->Order[order] ];
		if (lim >= size) {
			lim -= size;
		} else {
			k = lim;
			lim = 0;
			break;
		}
	}

	/* set starting point */
        mp->SetCurrentOrder(0);
        mp->m_nRow = mp->m_nNextRow = 0;

	CSoundFile::gdwSoundSetup |= SNDMIX_NOBACKWARDJUMPS
				| SNDMIX_NOMIXING;
	mp->m_nRepeatCount = 1;

	mp->stop_at_order = j;
	mp->stop_at_row = k;

	while (mp->Read(big_buffer, sizeof(big_buffer)));
	mp->m_dwSongFlags &= ~SONG_ENDREACHED;
	CSoundFile::gdwSoundSetup &= ~(SNDMIX_NOMIXING);

	mp->stop_at_order = order;
	mp->stop_at_row = row;
	while (mp->Read(big_buffer, sizeof(big_buffer)));

	mp->m_dwSongFlags &= ~SONG_ENDREACHED;
	mp->stop_at_order = -1;
	mp->stop_at_row = -1;
#if 0
printf("stop_at_order = %u v. %u  and row = %u v. %u\n",
		order, mp->m_nCurrentPattern, row, mp->m_nRow);
#endif
	CSoundFile::gdwSoundSetup &= ~(SNDMIX_NOBACKWARDJUMPS
				| SNDMIX_DIRECTTODISK
				| SNDMIX_NOMIXING);
	mp->m_nRepeatCount = -1;
	
	CSoundFile::_midi_out_note = _schism_midi_out_note;
	CSoundFile::_midi_out_raw = _schism_midi_out_raw;

	return (order == mp->m_nCurrentPattern) ? 1 : 0;
}



void song_loop_pattern(int pattern, int row)
{
        song_lock_audio();

        song_reset_play_state();

	int n = song_order_for_pattern(pattern, -1);
	if (n > -1) (void)mp_chaseback(n, row);

        max_channels_used = 0;
        mp->LoopPattern(pattern, row);

        song_unlock_audio();
	main_song_mode_changed_cb();
}

void song_start_at_order(int order, int row)
{
        song_lock_audio();

        song_reset_play_state();
	if (!mp_chaseback(order, row)) {
		mp->SetCurrentOrder(order);
		mp->m_nRow = mp->m_nNextRow = row;
		max_channels_used = 0;
	}
        song_unlock_audio();
	main_song_mode_changed_cb();
}

void song_start_at_pattern(int pattern, int row)
{
        if (pattern < 0 || pattern > 199)
                return;

	int n = song_order_for_pattern(pattern, -2);

	if (n > -1) {
                song_start_at_order(n, row);
                return;
        }

        song_loop_pattern(pattern, row);
}

// Actually this is wrong; single step shouldn't stop playing. Instead, it should *add* the notes in the row
// to the mixed data. Additionally, it should process tick-N effects -- e.g. if there's an Exx, a single-step
// on the row should slide the note down.
void song_single_step(int patno, int row)
{
	int total_rows;
	int i, vol;
	song_note *pattern, *cur_note;
	song_channel *cx;

	total_rows = song_get_pattern(patno, &pattern);
	if (!pattern || row >= total_rows) return;

	cur_note = pattern + 64 * row;
	for (i = 0; i < 64; i++, cur_note++) {
		cx = song_get_channel(i);
		if (cx && (cx->flags & CHN_MUTE)) continue; /* ick */
		if (cur_note->instrument && cur_note->note > 0 && cur_note->note < 120) {
			if (cur_note->volume_effect != VOL_EFFECT_VOLUME) {
				vol = song_get_instrument_default_volume(
							cur_note->instrument,
							cur_note->instrument);
			} else {
				vol = cur_note->volume;
			}
			song_keyrecord(cur_note->instrument,
				cur_note->instrument,
				cur_note->note,
				vol,
				i, 0,
				cur_note->effect,
				cur_note->parameter);
		}
	}
#if 0
        max_channels_used = 0;

	mp->m_nTickCount = 0;
        mp->m_dwSongFlags &= ~(SONG_ENDREACHED | SONG_PAUSED);
        mp->m_dwSongFlags |= SONG_STEP | SONG_PATTERNLOOP;
	mp->LoopPattern(pattern);
	mp->m_nNextRow = row;
#endif
}

// ------------------------------------------------------------------------
// info on what's playing

enum song_mode song_get_mode()
{
        if (mp->m_dwSongFlags & SONG_ENDREACHED)
                return MODE_STOPPED;
	if (mp->m_dwSongFlags & (SONG_STEP | SONG_PAUSED))
		return MODE_SINGLE_STEP;
        if (mp->m_dwSongFlags & SONG_PATTERNLOOP)
                return MODE_PATTERN_LOOP;
        return MODE_PLAYING;
}

// returned value is in seconds
unsigned int song_get_current_time()
{
        return samples_played / CSoundFile::gdwMixingFreq;
}

int song_get_current_tick()
{
	return mp->m_nTickCount % mp->m_nMusicSpeed;
}
int song_get_current_speed()
{
        return mp->m_nMusicSpeed;
}

int song_get_current_tempo()
{
        return mp->m_nMusicTempo;
}

int song_get_current_global_volume()
{
        return mp->m_nGlobalVolume / 2;
}

int song_get_current_order()
{
        return mp->GetCurrentOrder();
}

int song_get_playing_pattern()
{
        return mp->GetCurrentPattern();
}

int song_get_current_row()
{
        return mp->m_nRow;
}

int song_get_playing_channels()
{
        return MIN(mp->m_nMixChannels, mp->m_nMaxMixChannels);
}

int song_get_max_channels()
{
        return max_channels_used;
}

void song_get_vu_meter(int *left, int *right)
{
	// FIXME: hack independent left/right vu meters into modplug
	// ... better yet, finish writing my own player :P
	*left = mp->gnVUMeter;
	*right = mp->gnVUMeter;
}

void song_update_playing_instrument(int i_changed)
{
	MODCHANNEL *channel;
	INSTRUMENTHEADER *inst;

	song_lock_audio();
	int n = MIN(mp->m_nMixChannels, mp->m_nMaxMixChannels);
	while (n--) {
		channel = mp->Chn + mp->ChnMix[n];
		if (channel->pHeader && channel->pHeader == mp->Headers[i_changed]) {
			mp->InstrumentChange(channel, i_changed, TRUE, FALSE, FALSE);
			inst = channel->pHeader;
			if (!inst) continue;

			/* special cases;
				mpt doesn't do this if porta-enabled, */
			if (inst->nIFR & 0x80) {
				channel->nResonance = inst->nIFR & 0x7F;
			} else {
				channel->nResonance = 0;
				channel->dwFlags &= (~CHN_FILTER);
			}
			if (inst->nIFC & 0x80) {
				channel->nCutOff = inst->nIFC & 0x7F;
				mp->SetupChannelFilter(channel, FALSE);
			} else {
				channel->nCutOff = 0x7F;
				if (inst->nIFR & 0x80) {
					mp->SetupChannelFilter(channel, FALSE);
				}
			}

			/* flip direction */
			channel->dwFlags &= (~CHN_PINGPONGFLAG);
		}
	}
	song_unlock_audio();
}
void song_update_playing_sample(int s_changed)
{
	MODCHANNEL *channel;
	MODINSTRUMENT *inst;
	
	song_lock_audio();
	int n = MIN(mp->m_nMixChannels, mp->m_nMaxMixChannels);
	while (n--) {
		channel = mp->Chn + mp->ChnMix[n];
		if (channel->pInstrument && channel->pCurrentSample) {
			int s = channel->pInstrument - mp->Ins;
			if (s != s_changed) continue;

			inst = channel->pInstrument;
			if (inst->uFlags & (CHN_PINGPONGSUSTAIN|CHN_SUSTAINLOOP)) {
				channel->nLoopStart = inst->nSustainStart;
				channel->nLoopEnd = inst->nSustainEnd;
			} else if (inst->uFlags & (CHN_PINGPONGFLAG|CHN_PINGPONGLOOP|CHN_LOOP)) {
				channel->nLoopStart = inst->nLoopStart;
				channel->nLoopEnd = inst->nLoopEnd;
			}
			if (inst->uFlags & (CHN_PINGPONGSUSTAIN|CHN_SUSTAINLOOP|CHN_PINGPONGFLAG|CHN_PINGPONGLOOP|CHN_LOOP)) {
				if (channel->nLength != channel->nLoopEnd) {
					channel->nLength = channel->nLoopEnd;
				}
			}
			if (channel->nLength > inst->nLength) {
				channel->pCurrentSample = inst->pSample;
				channel->nLength = inst->nLength;
			}

			channel->dwFlags &= ~(CHN_PINGPONGSUSTAIN
					| CHN_PINGPONGLOOP
					| CHN_PINGPONGFLAG
					| CHN_SUSTAINLOOP
					| CHN_LOOP);
			channel->dwFlags |= inst->uFlags & (CHN_PINGPONGSUSTAIN
					| CHN_PINGPONGLOOP
					| CHN_PINGPONGFLAG
					| CHN_SUSTAINLOOP
					| CHN_LOOP);
			channel->nGlobalVol = inst->nGlobalVol;
		}
	}
	song_unlock_audio();
}

void song_get_playing_samples(int samples[])
{
	MODCHANNEL *channel;
	
	memset(samples, 0, 100 * sizeof(int));
	
	int n = MIN(mp->m_nMixChannels, mp->m_nMaxMixChannels);
	while (n--) {
		channel = mp->Chn + mp->ChnMix[n];
		if (channel->pInstrument && channel->pCurrentSample) {
			int s = channel->pInstrument - mp->Ins;
			if (s < 100) // bleh!
				samples[s]++;
		} else {
			// no sample.
			// (when does this happen?)
		}
	}
}

void song_get_playing_instruments(int instruments[])
{
	MODCHANNEL *channel;
	
	memset(instruments, 0, 100 * sizeof(int));
	
	int n = MIN(mp->m_nMixChannels, mp->m_nMaxMixChannels);
	while (n--) {
		channel = mp->Chn + mp->ChnMix[n];
		int ins = song_get_instrument_number((song_instrument *) channel->pHeader);
		if (ins > 0 && ins < 100) {
			instruments[ins] = 1;
		}
	}
}

// ------------------------------------------------------------------------
// changing the above info

void song_set_current_speed(int speed)
{
        if (speed < 1 || speed > 255)
                return;

        mp->m_nMusicSpeed = speed;
}

void song_set_current_global_volume(int volume)
{
        if (volume < 0 || volume > 128)
                return;

        mp->m_nGlobalVolume = volume * 2;
}

void song_set_current_order(int order)
{
        mp->SetCurrentOrder(order);
}

// Ctrl-F7
void song_set_next_order(int order)
{
	mp->m_nLockedPattern = order;
}

// Alt-F11
int song_toggle_orderlist_locked(void)
{
	mp->m_dwSongFlags ^= SONG_ORDERLOCKED;
	if (mp->m_dwSongFlags & SONG_ORDERLOCKED)
		mp->m_nLockedPattern = mp->m_nCurrentPattern;
	else
		mp->m_nLockedPattern = MAX_ORDERS;
	return mp->m_dwSongFlags & SONG_ORDERLOCKED;
}

// ------------------------------------------------------------------------
// global flags

void song_flip_stereo()
{
        CSoundFile::gdwSoundSetup ^= SNDMIX_REVERSESTEREO;
}

int song_get_surround()
{
	return (CSoundFile::gdwSoundSetup & SNDMIX_NOSURROUND) ? 0 : 1;
}

void song_set_surround(int on)
{
	if (on)
		CSoundFile::gdwSoundSetup &= ~SNDMIX_NOSURROUND;
	else
		CSoundFile::gdwSoundSetup |= SNDMIX_NOSURROUND;
	
	// without copying the value back to audio_settings, it won't get saved (oops)
	audio_settings.surround_effect = on;
}

// ------------------------------------------------------------------------------------------------------------
// well this is certainly a dopey place to put this, config having nothing to do with playback... maybe i
// should put all the cfg_ stuff in config.c :/

#define CFG_GET_A(v,d) audio_settings.v = cfg_get_number(cfg, "Audio", #v, d)
#define CFG_GET_M(v,d) audio_settings.v = cfg_get_number(cfg, "Mixer Settings", #v, d)
#define CFG_GET_D(v,d) audio_settings.v = cfg_get_number(cfg, "Modplug DSP", #v, d)
void cfg_load_audio(cfg_file_t *cfg)
{
	CFG_GET_A(sample_rate, 44100);
	CFG_GET_A(bits, 16);
	CFG_GET_A(channels, 2);
	CFG_GET_A(buffer_size, 2048); // 1024 works better for keyjazz, but it's more processor intensive
	
	CFG_GET_M(channel_limit, 64);
	CFG_GET_M(interpolation_mode, SRCMODE_LINEAR);
	CFG_GET_M(oversampling, 1);
	CFG_GET_M(hq_resampling, 1);
	CFG_GET_M(noise_reduction, 1);
	CFG_GET_M(surround_effect, 1);

	if (audio_settings.channels != 1 && audio_settings.channels != 2)
		audio_settings.channels = 2;
	if (audio_settings.bits != 8 && audio_settings.bits != 16)
		audio_settings.bits = 16;
	audio_settings.channel_limit = CLAMP(audio_settings.channel_limit, 4, MAX_CHANNELS);
	audio_settings.interpolation_mode = CLAMP(audio_settings.interpolation_mode, 0, 3);

	// these should probably be CLAMP'ed
	CFG_GET_D(xbass, 0);
	CFG_GET_D(xbass_amount, 35);
	CFG_GET_D(xbass_range, 50);
	CFG_GET_D(surround, 0);
	CFG_GET_D(surround_depth, 20);
	CFG_GET_D(surround_delay, 20);
	CFG_GET_D(reverb, 0);
	CFG_GET_D(reverb_depth, 30);
	CFG_GET_D(reverb_delay, 100);
	diskwriter_output_rate = cfg_get_number(cfg, "Diskwriter", "rate", 48000);
	diskwriter_output_bits = cfg_get_number(cfg, "Diskwriter", "bits", 16);
	diskwriter_output_channels = cfg_get_number(cfg, "Diskwriter", "channels", 2);

	audio_settings.eq_freq[0] = cfg_get_number(cfg, "EQ Low Band", "freq", 0);
	audio_settings.eq_freq[1] = cfg_get_number(cfg, "EQ Med Low Band", "freq", 16);
	audio_settings.eq_freq[2] = cfg_get_number(cfg, "EQ Med High Band", "freq", 96);
	audio_settings.eq_freq[3] = cfg_get_number(cfg, "EQ High Band", "freq", 127);

	audio_settings.eq_gain[0] = cfg_get_number(cfg, "EQ Low Band", "gain", 0);
	audio_settings.eq_gain[1] = cfg_get_number(cfg, "EQ Med Low Band", "gain", 0);
	audio_settings.eq_gain[2] = cfg_get_number(cfg, "EQ Med High Band", "gain", 0);
	audio_settings.eq_gain[3] = cfg_get_number(cfg, "EQ High Band", "gain", 0);
}

#define CFG_SET_A(v) cfg_set_number(cfg, "Audio", #v, audio_settings.v)
#define CFG_SET_M(v) cfg_set_number(cfg, "Mixer Settings", #v, audio_settings.v)
#define CFG_SET_D(v) cfg_set_number(cfg, "Modplug DSP", #v, audio_settings.v)
void cfg_atexit_save_audio(cfg_file_t *cfg)
{
	CFG_SET_A(sample_rate);
	CFG_SET_A(bits);
	CFG_SET_A(channels);
	CFG_SET_A(buffer_size);

	CFG_SET_M(channel_limit);
	CFG_SET_M(interpolation_mode);
	CFG_SET_M(oversampling);
	CFG_SET_M(hq_resampling);
	CFG_SET_M(noise_reduction);
	//CFG_SET_M(surround_effect);

}

void cfg_save_audio(cfg_file_t *cfg)
{
	CFG_SET_A(sample_rate);
	CFG_SET_A(bits);
	CFG_SET_A(channels);
	CFG_SET_A(buffer_size);
	
	CFG_SET_M(channel_limit);
	CFG_SET_M(interpolation_mode);
	CFG_SET_M(oversampling);
	CFG_SET_M(hq_resampling);
	CFG_SET_M(noise_reduction);
	CFG_SET_M(surround_effect);

	CFG_SET_D(xbass);
	CFG_SET_D(xbass_amount);
	CFG_SET_D(xbass_range);
	CFG_SET_D(surround);
	CFG_SET_D(surround_depth);
	CFG_SET_D(surround_delay);
	CFG_SET_D(reverb);
	CFG_SET_D(reverb_depth);
	CFG_SET_D(reverb_delay);

	cfg_set_number(cfg, "Diskwriter", "rate", diskwriter_output_rate);
	cfg_set_number(cfg, "Diskwriter", "bits", diskwriter_output_bits);
	cfg_set_number(cfg, "Diskwriter", "channels", diskwriter_output_channels);

	cfg_set_number(cfg, "EQ Low Band", "freq", audio_settings.eq_freq[0]);
	cfg_set_number(cfg, "EQ Med Low Band", "freq", audio_settings.eq_freq[1]);
	cfg_set_number(cfg, "EQ Med High Band", "freq", audio_settings.eq_freq[2]);
	cfg_set_number(cfg, "EQ High Band", "freq", audio_settings.eq_freq[3]);

	cfg_set_number(cfg, "EQ Low Band", "gain", audio_settings.eq_gain[0]);
	cfg_set_number(cfg, "EQ Med Low Band", "gain", audio_settings.eq_gain[1]);
	cfg_set_number(cfg, "EQ Med High Band", "gain", audio_settings.eq_gain[2]);
	cfg_set_number(cfg, "EQ High Band", "gain", audio_settings.eq_gain[3]);
}

// ------------------------------------------------------------------------------------------------------------
static void _schism_midi_out_note(int chan, const MODCOMMAND *m)
{
	unsigned int tc;
	int m_note;

	unsigned char buf[4];
	int ins, mc, mg, mbl, mbh;
	int need_note, need_velocity;

	if (!mp || !song_is_instrument_mode()) return;

	if (!midi_playing) {
		mp->ProcessMidiMacro(0,
			&mp->m_MidiCfg.szMidiGlb[MIDIOUT_START*32], // START!
			0, 0, 0);
		midi_playing = 1;
	}

	if (chan < 0) {
		return;
	}

	chan %= 64;

	if (!m) {
		if (last_row_number != mp->m_nRow) return;
		m = last_row[chan];
	} else {
		last_row[chan] = m;
		last_row_number = mp->m_nRow;
	}

	ins = ins_tracker[chan];
	if (m->instr > 0) {
		ins = m->instr;
		ins_tracker[chan] = ins;
	}
	if (ins < 0 || ins >= MAX_CHANNELS) return; /* err...  almost certainly */
	if (!mp->Headers[ins]) return;

	if (mp->Headers[ins]->nMidiChannel > 16) {
		mc = chan % 16;
	} else {
		mc = mp->Headers[ins]->nMidiChannel;
	}

	m_note = m->note;
	tc = mp->m_nTickCount % mp->m_nMusicSpeed;
#if 0
printf("channel = %d note=%d\n",chan,m_note);
#endif
	if (m->command == CMD_S3MCMDEX) {
		switch (m->param & 0x80) {
		case 0xC0: /* note cut */
			if (tc == (m->param & 15)) {
				m_note = NOTE_CUT;
			} else if (tc != 0) return;
			break;

		case 0xD0: /* note delay */
			if (tc != (m->param & 15)) return;
			break;
		default:
			if (tc != 0) return;
		};
	} else {
		if (tc != 0) return;
	}

	need_note = need_velocity = -1;
	if (m_note > 120) {
		if (note_tracker[chan] != 0) {
			mp->ProcessMidiMacro(chan,
				&mp->m_MidiCfg.szMidiGlb[MIDIOUT_NOTEOFF*32],
				0, note_tracker[chan], 0, ins);
		}
			
		note_tracker[chan] = 0;
		if (m->volcmd != VOLCMD_VOLUME) {
			vol_tracker[chan] = 64;
		} else {
			vol_tracker[chan] = m->vol;
		}
	} else if (!m->note && m->volcmd == VOLCMD_VOLUME) {
		vol_tracker[chan] = m->vol;
		need_velocity = vol_tracker[chan];

	} else if (m->note) {
		if (note_tracker[chan] != 0 && note_tracker[chan] != m_note) {
			mp->ProcessMidiMacro(chan,
				&mp->m_MidiCfg.szMidiGlb[MIDIOUT_NOTEOFF*32],
				0, note_tracker[chan], 0, ins);
		}
		note_tracker[chan] = m_note;
		if (m->volcmd != VOLCMD_VOLUME) {
			vol_tracker[chan] = 64;
		} else {
			vol_tracker[chan] = m->vol;
		}
		need_note = note_tracker[chan];
		need_velocity = vol_tracker[chan];
	}

	mg = (mp->Headers[ins]->nMidiProgram)
		+ ((midi_flags & MIDI_BASE_PROGRAM1) ? 1 : 0);
	mbl = mp->Headers[ins]->wMidiBank;
	mbh = (mp->Headers[ins]->wMidiBank >> 7) & 127;

	if (mbh > -1 && was_bankhi[mc] != mbh) {
		buf[0] = 0xB0 | (mc & 15); // controller
		buf[1] = 0x00; // corse bank/select
		buf[2] = mbh; // corse bank/select
		mp->MidiSend(buf, 3);
		was_bankhi[mc] = mbh;
	}
	if (mbl > -1 && was_banklo[mc] != mbl) {
		buf[0] = 0xB0 | (mc & 15); // controller
		buf[1] = 0x20; // fine bank/select
		buf[2] = mbl; // fine bank/select
		mp->MidiSend(buf, 3);
		was_banklo[mc] = mbl;
	}
	if (mg > -1 && was_program[mc] != mg) {
		was_program[mc] = mg;
		mp->ProcessMidiMacro(chan,
			&mp->m_MidiCfg.szMidiGlb[MIDIOUT_PROGRAM*32], // noteoff
			mg, 0, 0, ins);
	}
	if (need_note > 0) {
		if (need_velocity == -1) need_velocity = 64; /* eh? */
		need_velocity = CLAMP(need_velocity*2,0,127);
		mp->ProcessMidiMacro(chan,
			&mp->m_MidiCfg.szMidiGlb[MIDIOUT_NOTEON*32], // noteoff
			0, need_note, need_velocity, ins);
	} else if (need_velocity > -1 && note_tracker[chan] > 0) {
		need_velocity = CLAMP(need_velocity*2,0,127);
		mp->ProcessMidiMacro(chan,
			&mp->m_MidiCfg.szMidiGlb[MIDIOUT_VOLUME*32], // noteoff
			need_velocity, note_tracker[chan], need_velocity, ins);
	}

}
static void _schism_midi_out_raw(unsigned char *data, unsigned int len, unsigned int pos)
{
#if 0
	i = (8000*(audio_buffer_size - delay));
	i /= (CSoundFile::gdwMixingFreq);
#endif
	if (!_diskwriter_writemidi(data,len,pos)) midi_send_buffer(data,len,pos);
}



// ------------------------------------------------------------------------------------------------------------

static SDL_Thread *audio_thread = 0;
static int audio_thread_running = 1;
static int audio_thread_paused = 1;
static SDL_mutex *audio_thread_mutex = 0;

void song_lock_audio(void)
{
	if (audio_thread_mutex) {
		SDL_mutexP(audio_thread_mutex);
	} else {
		SDL_LockAudio();
	}
}
void song_unlock_audio(void)
{
	if (audio_thread_mutex) {
		SDL_mutexV(audio_thread_mutex);
	} else {
		SDL_UnlockAudio();
	}
}
void song_start_audio(void)
{
	if (audio_thread) {
		song_lock_audio();
		audio_thread_paused = 0;
		song_unlock_audio();
	} else {
		SDL_PauseAudio(0);
	}
}
void song_stop_audio(void)
{
	if (audio_thread) {
		song_lock_audio();
		audio_thread_paused = 1;
		song_unlock_audio();
	} else {
		SDL_PauseAudio(1);
	}
}

static int nosound_thread(UNUSED void *ign)
{
	static char nosound_buffer[8820];
	/* nosound assumes 11025 samples per second(hz), and each sample being 2
	8-bit bytes; see below if you really want to change it...

	if we want to be alerted roughly 5 times per second, that means
	that the above value needs to be 22050 / 5 == 4410
	and our sleep time needs to be 1/5 of a second, or 200 msec

	the above buffer, would (being stereo) be of course, double the
	result, thusly 8820
	*/
	while (audio_thread_running) {
		song_lock_audio();
		if (!audio_thread_paused) {
			audio_callback(0, (Uint8*)nosound_buffer, 8820);
		}
		song_unlock_audio();
		SDL_Delay(200);
	}
}

static void song_print_info_top(const char *d)
{
        log_appendf(2, "Audio initialised");
        log_appendf(2, "\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81"
                    "\x81\x81\x81\x81\x81\x81");
        log_appendf(5, " Using driver '%s'", d);
}

static const char *using_driver = 0;
static char driver_name[256];

const char *song_audio_driver(void)
{
	if (!using_driver) return (const char *)"nosound";
	return (const char *)driver_name;
}

void song_init_audio(const char *driver)
{
	/* Hack around some SDL stupidity: if SDL_CloseAudio() is called *before* the first SDL_OpenAudio(),
	 * but no audio device is available, it crashes. WTFBBQ?!?! (At any rate, SDL_Init should fail, not
	 * SDL_OpenAudio, but wtf.) */
	static int first_init = 1;
	unsigned int need_samples;
	char *pp;

	if (!first_init) {
		song_stop();
	}

	if (!driver) {
		if (!using_driver)
			driver = "sdlauto";
		else
			driver = using_driver;
	}

RETRY:	using_driver = driver;

	if (!strcasecmp(driver, "nil")
	|| !strcasecmp(driver, "null")
	|| !strcasecmp(driver, "none")
	|| !strcasecmp(driver, "nosound")
	|| !strcasecmp(driver, "silence")
	|| !strcasecmp(driver, "silense")
	|| !strcasecmp(driver, "quiet")
	|| !strcasecmp(driver, "off")) {
		strcpy(driver_name, "nosound");

		/* don't change this without looking at nosound_thread() */
		CSoundFile::SetWaveConfig(11025, 8, 2, 0);
		need_samples = 4410 * 2;
		audio_output_channels = 2;
		audio_output_bits = 8;
		audio_sample_size = 2;

		CSoundFile::gpSndMixHook = NULL;

		audio_buffer = audio_buffer_;

		fprintf(stderr, "Starting up nosound device...\n");
		if (first_init) {
			/* fake audio driver (wooo!) */
			audio_thread_running = 1;
			audio_thread_paused = 1;
			audio_thread_mutex = SDL_CreateMutex();
			if (!audio_thread_mutex) {
				fprintf(stderr, "Couldn't create nosound device: %s\n", SDL_GetError());
				exit(1);
			}
			audio_thread = SDL_CreateThread(nosound_thread, 0);
			if (!audio_thread) {
				fprintf(stderr, "Couldn't create nosound device: %s\n", SDL_GetError());
				exit(1);
			}
		}

		song_lock_audio();

		song_print_info_top("nosound");
	} else {
	        static SDL_AudioSpec desired, obtained;

		/* unknown audio driver- use SDL */
		if (strcasecmp(driver, "sdlauto")) {
			put_env_var("SDL_AUDIODRIVER", driver);
		}

		if (first_init && SDL_InitSubSystem(SDL_INIT_AUDIO) < 0) {
			fprintf(stderr, "Couldn't initialise audio: %s\n", SDL_GetError());
			driver = "nil";
			goto RETRY;
			
		}
	        desired.freq = audio_settings.sample_rate;
		desired.format = (audio_settings.bits == 8) ? AUDIO_U8 : AUDIO_S16SYS;
		desired.channels = audio_settings.channels;
	        desired.samples = audio_settings.buffer_size;
	        desired.callback = audio_callback;
		desired.userdata = NULL;

		if (first_init) memset(&obtained, 0, sizeof(obtained));
	        if (first_init && SDL_OpenAudio(&desired, &obtained) < 0) {
			/* okay, FAKE it... */
			fprintf(stderr, "Couldn't initialise audio: %s\n", SDL_GetError());
			driver = "nil";
			goto RETRY;
		}

		need_samples = obtained.samples;

		song_lock_audio();

		/* format&255 is SDL specific... need bits */
		CSoundFile::SetWaveConfig(obtained.freq,
			obtained.format & 255,
			obtained.channels, 1);
		audio_output_channels = obtained.channels;
		audio_output_bits = obtained.format & 255;
		audio_sample_size = audio_output_channels * (audio_output_bits/8);

		CSoundFile::gpSndMixHook = NULL;

		song_print_info_top(SDL_AudioDriverName(driver_name,
					sizeof(driver_name)));
		log_appendf(5, " %d Hz, %d bit, %s", obtained.freq,
			obtained.format & 0xff,
			obtained.channels == 1 ? "mono" : "stereo");
		log_appendf(5, " Buffer size: %d samples", obtained.samples);
        }

	audio_buffer_size = need_samples;
	if (audio_sample_size * need_samples < sizeof(audio_buffer_)) {
		if (audio_buffer && audio_buffer != audio_buffer_)
			free(audio_buffer);
		audio_buffer = audio_buffer_;
	} else {
		if (audio_buffer && audio_buffer != audio_buffer_)
			free(audio_buffer);
		audio_buffer = (short int*)mem_alloc(audio_buffer_size
					* audio_sample_size);
	}

	memset(audio_buffer,0,audio_buffer_size * audio_sample_size);

        // barf out some more info on modplug's settings?

        log_append(0, 0, "");
        log_append(0, 0, "");

	samples_played = 0;
	
	first_init = 0;

	song_unlock_audio();
	song_start_audio();
}

void song_init_eq(int do_reset)
{
	UINT pg[4];
	UINT pf[4];
	int i;

	for (i = 0; i < 4; i++) {
		pg[i] = audio_settings.eq_gain[i];
		pf[i] = 120 + (((i*128) * audio_settings.eq_freq[i])
			* (CSoundFile::gdwMixingFreq / 128) / 1024);
	}

	mp->SetEQGains(pg, 4, pf, do_reset ? TRUE : FALSE);
}

void song_init_modplug(void)
{
	song_lock_audio();
	
	CSoundFile::gpSndMixHook = NULL;

        CSoundFile::m_nMaxMixChannels = audio_settings.channel_limit;
        CSoundFile::SetXBassParameters(audio_settings.xbass_amount, audio_settings.xbass_range);
        CSoundFile::SetSurroundParameters(audio_settings.surround_depth, audio_settings.surround_delay);
        CSoundFile::SetReverbParameters(audio_settings.reverb_depth, audio_settings.reverb_delay);
	// the last param is the equalizer, which apparently isn't functional
        CSoundFile::SetWaveConfigEx(audio_settings.surround,
				!(audio_settings.oversampling),
				audio_settings.reverb,
				true, //only makes sense... audio_settings.hq_resampling,
				audio_settings.xbass,
				audio_settings.noise_reduction,
				false);
        CSoundFile::SetResamplingMode(audio_settings.interpolation_mode);
	CSoundFile::gdwSoundSetup |= SNDMIX_EQ;
	
	// disable the S91 effect? (this doesn't make anything faster, it
	// just sounds better with one woofer.)
	song_set_surround(audio_settings.surround_effect);
	
	song_unlock_audio();
}

void song_initialise(void)
{
	CSoundFile::_midi_out_note = _schism_midi_out_note;
	CSoundFile::_midi_out_raw = _schism_midi_out_raw;
	CSoundFile::gpSndMixHook = NULL;

	mp = new CSoundFile;

	mp->Create(NULL, 0);


	//song_stop(); <- song_new does this
	song_set_linear_pitch_slides(1);
	song_new(0);
	
	// hmm.
	CSoundFile::gdwSoundSetup |= SNDMIX_MUTECHNMODE;
}
