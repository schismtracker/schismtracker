// Schism Tracker - a cross-platform Impulse Tracker clone
// copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
// copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
// copyright (c) 2009 Storlek & Mrs. Brisby
// URL: http://schismtracker.org/
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
#include "modplug/snd_flt.h"
#include "modplug/snd_eq.h"
#include "slurp.h"
#include "config-parser.h"

#include "diskwriter.h"
#include "event.h"

#ifndef MACOSX
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cmath>
#endif

#include "sdlmain.h"

#include "midi.h"

#include "snd_fm.h"
#include "snd_gm.h"

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
static int audio_buffers_per_second = 0;
static int audio_writeout_count = 0;

struct audio_settings audio_settings;

static void _schism_midi_out_note(int chan, const MODCOMMAND *m);
static void _schism_midi_out_raw(const unsigned char *data, unsigned int len, unsigned int delay);

// ------------------------------------------------------------------------
// playback

extern "C" {
	extern int midi_bend_hit[64], midi_last_bend_hit[64];
	extern void vis_work_16s(short *in, int inlen);
	extern void vis_work_16m(short *in, int inlen);
	extern void vis_work_8s(char *in, int inlen);
	extern void vis_work_8m(char *in, int inlen);
};

// this gets called from sdl
static void audio_callback(UNUSED void *qq, uint8_t * stream, int len)
{
	unsigned int wasrow = mp->m_nRow;
	unsigned int waspat = mp->m_nCurrentOrder;
	int i, n;

	if (!stream || !len || !mp) {
		if (status.current_page == PAGE_WATERFALL || status.vis_style == VIS_FFT) {
			vis_work_8m(0, 0);
		}
		song_stop_unlocked(0);
		goto POST_EVENT;
	}

        if (mp->m_dwSongFlags & SONG_ENDREACHED) {
		n = 0;
	} else {
        	n = csf_read(mp, stream, len);
	        if (!n) {
			if (status.current_page == PAGE_WATERFALL
			|| status.vis_style == VIS_FFT) {
				vis_work_8m(0,0);
			}
			song_stop_unlocked(0);
			goto POST_EVENT;
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
		stream = (uint8_t *) audio_buffer;
		n *= audio_output_channels;
		for (i = 0; i < n; i++) {
			stream[i] ^= 128;
		}
		if (status.current_page == PAGE_WATERFALL
		|| status.vis_style == VIS_FFT) {
			if (audio_output_channels == 2) {
				vis_work_8s((char*)stream, n/2);
			} else {
				vis_work_8m((char*)stream, n);
			}
		}
	} else if (status.current_page == PAGE_WATERFALL
				|| status.vis_style == VIS_FFT) {
		if (audio_output_channels == 2) {
			vis_work_16s((short*)stream, n);
		} else {
			vis_work_16m((short*)stream, n);
		}
	}
	
	if (mp->m_nMixChannels > max_channels_used)
		max_channels_used = MIN(mp->m_nMixChannels, mp->m_nMaxMixChannels);
POST_EVENT:
	audio_writeout_count++;
	if (audio_writeout_count > audio_buffers_per_second) {
		audio_writeout_count = 0;
	} else if (waspat == mp->m_nCurrentOrder && wasrow == mp->m_nRow
			&& !midi_need_flush()) {
		/* skip it */
		return;
	}
	
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

/* this should be in page.c; the audio handling code doesn't need to know what
   a page is, much less be talking to them */
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

int song_get_current_play_channel(void)
{
	return current_play_channel;
}

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


/* Channel corresponding to each note played.
That is, keydown_channels[66] will indicate in which channel F-5 was played most recently.
This will break if the same note was keydown'd twice without a keyup, but I think that's a
fairly unlikely scenario that you'd have to TRY to bring about. */
static int keyjazz_channels[128];


static int song_keydown_ex(int samp, int ins, int note, int vol, int chan, int effect, int param)
{
	int ins_mode;
	SONGVOICE *c;
	MODCOMMAND mc;

	if (chan == KEYJAZZ_CHAN_CURRENT) {
		chan = current_play_channel;
		if (multichannel_mode)
			song_change_current_play_channel(1, 1);
	}
	
	// keep track of what channel this note was played in so we can note-off properly later
	if (note > 0 && note < 128)
		keyjazz_channels[note] = chan;

	song_lock_audio();

	c = mp->Voices + chan - 1;

	ins_mode = song_is_instrument_mode();

	if (samp != KEYJAZZ_NOINST && (c->dwFlags & CHN_ADLIB)) {
		OPL_NoteOff(chan);
		OPL_Patch(chan, mp->Samples[samp].AdlibBytes);
	}

	/* previously this block also checked for SONG_ENDREACHED|SONG_PAUSED if (ins >= 0)
	do we want to check that or not? what is this actually handling anyway? */
	if ((c->pSample || c->nRealtime) && note < 0x80) {
		csf_note_change(mp, chan, c->nRowNote, false, true, false);
		csf_process_effects(mp);
		csf_check_nna(mp, chan, ins_mode ? ins : samp, note, false);
	}

	if (ins == KEYJAZZ_NOINST && samp != KEYJAZZ_NOINST && (ins_mode || samp == 0)) {
		/* this is only needed on the sample page, when in instrument mode... */
		SONGSAMPLE *s = mp->Samples + samp;

		c->nRowCommand = effect;
		c->nRowParam = param;

		c->dwFlags = (s->uFlags & CHN_SAMPLE_FLAGS) | (c->dwFlags & CHN_MUTE);
		if (c->dwFlags & CHN_MUTE) {
			c->dwFlags &= ~CHN_MUTE;
			c->dwFlags |= CHN_NNAMUTE;
		}


		if (note > 0 && note < 128) {
			c->nMasterChn = 0; // indicates foreground channel.
			// most of these things seem to be taken care of by csf_note_change.
			// also a bunch of stuff seems to not work
			//c->dwFlags &= ~(CHN_PINGPONGFLAG);

			// csf_note_change copies stuff from c->pInstrument as long as c->nLength is zero
			// and if period != 0 (ie. sample not playing at a stupid rate)
			c->pInstrument = s;
			c->nLength = 0;
			// ... but it doesn't copy the volumes, for somewhat obvious reasons.
			c->nVolume = (vol == KEYJAZZ_DEFAULTVOL) ? s->nVolume : (vol << 2);
			c->nInsVol = s->nGlobalVol;
			c->nGlobalVol = 64;
			// gotta set these by hand, too
			c->nC5Speed = s->nC5Speed;
			c->nCutOff = 0x7f;
			c->nResonance = 0;
			c->nNewNote = note;
			c->pHeader = NULL;
			s->played = 1;
		}
		csf_note_change(mp, chan - 1, note, false, true, true);
	} else if ((ins_mode ? ins : samp) != KEYJAZZ_NOINST) {
		if ((status.flags & MIDI_LIKE_TRACKER) && ins != KEYJAZZ_NOINST) {
			SONGINSTRUMENT *i = mp->Instruments[ins];

			if (i && i->nMidiChannelMask) {
				GM_KeyOff(chan);
				GM_DPatch(chan, i->nMidiProgram, i->wMidiBank, i->nMidiChannelMask);
			}
		}

		c->nRealtime = 1;
		c->nTickStart = (mp->m_nTickCount+1) % mp->m_nMusicSpeed;
		c->nRowNote = note;
		if (vol == KEYJAZZ_DEFAULTVOL) {
			c->nRowVolCmd = 0;
			c->nRowVolume = 0;
		} else {
			c->nRowVolCmd = VOLCMD_VOLUME;
			c->nRowVolume = vol;
			c->nVolume = (vol << 2);
		}

		if (c->dwFlags & CHN_MUTE) {
			c->dwFlags &= ~(CHN_MUTE);
			c->dwFlags |= CHN_NNAMUTE;
		}
		c->dwFlags &= ~(CHN_PINGPONGFLAG);

		c->nRowInstr = ins_mode ? ins : samp;
		c->nRowCommand = effect;
		c->nRowParam = param;
	}
	
	if (mp->m_dwSongFlags & SONG_ENDREACHED) {
		mp->m_dwSongFlags &= ~SONG_ENDREACHED;
		mp->m_dwSongFlags |= SONG_PAUSED;
	}

	if (!(status.flags & MIDI_LIKE_TRACKER) && ins != KEYJAZZ_NOINST) {
		mc.note = note;
		mc.instr = ins;
		mc.volcmd = VOLCMD_VOLUME;
		mc.vol = vol;
		mc.command = effect;
		mc.param = param;
		_schism_midi_out_note(chan, &mc);
	}

	song_unlock_audio();
	
	return chan;
}

int song_keydown(int samp, int ins, int note, int vol, int chan)
{
	return song_keydown_ex(samp, ins, note, vol, chan, CMD_PANNING8, 0x80);
}

int song_keyrecord(int samp, int ins, int note, int vol, int chan, int effect, int param)
{
	return song_keydown_ex(samp, ins, note, vol, chan, effect, param);
}

int song_keyup(int samp, int ins, int note)
{
	return song_keydown_ex(samp, ins, NOTE_OFF, KEYJAZZ_DEFAULTVOL, keyjazz_channels[note], 0, 0);
}

// ------------------------------------------------------------------------------------------------------------

// this should be called with the audio LOCKED
static void song_reset_play_state()
{
	memset(midi_bend_hit, 0, sizeof(midi_bend_hit));
	memset(midi_last_bend_hit, 0, sizeof(midi_last_bend_hit));
	memset(keyjazz_channels, 0, sizeof(keyjazz_channels));

	// turn this crap off
	CSoundFile::gdwSoundSetup &= ~(SNDMIX_NOBACKWARDJUMPS
				| SNDMIX_NOMIXING
				| SNDMIX_DIRECTTODISK);

	csf_initialize_dsp(mp, true);

	csf_set_current_order(mp, 0);

	mp->m_nInitialRepeatCount = -1;
	mp->m_nRepeatCount = -1;
	mp->m_nBufferCount = 0;
	mp->m_dwSongFlags &= ~(SONG_PAUSED | SONG_STEP | SONG_PATTERNLOOP | SONG_ENDREACHED);

	mp->stop_at_order = -1;
	mp->stop_at_row = -1;
	csf_reset_timestamps(mp);
	samples_played = 0;
}

void song_start_once()
{
        song_lock_audio();

        song_reset_play_state();
	CSoundFile::gdwSoundSetup |= SNDMIX_NOBACKWARDJUMPS;
        max_channels_used = 0;
	mp->m_nInitialRepeatCount = 0;
	mp->m_nRepeatCount = 1;

	GM_SendSongStartCode();
        song_unlock_audio();
	main_song_mode_changed_cb();
}

void song_start()
{
        song_lock_audio();

        song_reset_play_state();
        max_channels_used = 0;

	GM_SendSongStartCode();
        song_unlock_audio();
	main_song_mode_changed_cb();
}

void song_pause()
{
	song_lock_audio();
	// Highly unintuitive, but SONG_PAUSED has nothing to do with pause.
	if (!(mp->m_dwSongFlags & SONG_PAUSED))
		mp->m_dwSongFlags ^= SONG_ENDREACHED;
	song_unlock_audio();
	main_song_mode_changed_cb();
}

void song_stop()
{
	song_lock_audio();
	song_stop_unlocked(0);
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

void song_stop_unlocked(int quitting)
{
	if (!mp) return;

	if (midi_playing) {
		unsigned char moff[4];

		/* shut off everything; not IT like, but less annoying */
		for (int chan = 0; chan < 64; chan++) {
			if (note_tracker[chan] != 0) {
				for (int j = 0; j < 16; j++) {
					csf_process_midi_macro(mp, chan,
						&mp->m_MidiCfg.szMidiGlb[MIDIOUT_NOTEOFF*32],
						0, note_tracker[chan], 0, j);
				}
				moff[0] = 0x80 + chan;
				moff[1] = note_tracker[chan];
				csf_midi_send(mp, (unsigned char *) moff, 2, 0, 0);
			}
		}
		for (int j = 0; j < 16; j++) {
			moff[0] = 0xe0 + j;
			moff[1] = 0;
			csf_midi_send(mp, (unsigned char *) moff, 2, 0, 0);
		}

		// send all notes off
#define _MIDI_PANIC	"\xb0\x78\0\xb0\x79\0\xb0\x7b\0"
		csf_midi_send(mp, (unsigned char *) _MIDI_PANIC, sizeof(_MIDI_PANIC) - 1, 0, 0);
		csf_process_midi_macro(mp, NULL, &mp->m_MidiCfg.szMidiGlb[MIDIOUT_STOP*32], 0, 0, 0, 0); // STOP!
		midi_send_flush(); // NOW!

		midi_playing = 0;
	}
	
	OPL_Reset(); /* Also stop all OPL sounds */
	GM_Reset(quitting);
	GM_SendSongStopCode();

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
        // Modplug doesn't actually have a "stop" mode, but if SONG_ENDREACHED is set, mp->Read just returns.
        mp->m_dwSongFlags |= SONG_PAUSED | SONG_ENDREACHED;
	
	mp->gnVULeft = 0;
	mp->gnVURight = 0;
	memset(audio_buffer, 0, audio_buffer_size * audio_sample_size);
}




void song_loop_pattern(int pattern, int row)
{
        song_lock_audio();

        song_reset_play_state();

        max_channels_used = 0;
        csf_loop_pattern(mp, pattern, row);

        GM_SendSongStartCode();

        song_unlock_audio();
	main_song_mode_changed_cb();
}

void song_start_at_order(int order, int row)
{
        song_lock_audio();

        song_reset_play_state();

	csf_set_current_order(mp, order);
	mp->m_nBreakRow = row;
	max_channels_used = 0;

	GM_SendSongStartCode();
	/* TODO: GM_SendSongPositionCode(calculate the number of 1/16 notes) */
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

void song_single_step(int patno, int row)
{
	int total_rows;
	int i, vol;
	song_note *pattern, *cur_note;
	song_mix_channel *cx;

	total_rows = song_get_pattern(patno, &pattern);
	if (!pattern || row >= total_rows) return;

	cur_note = pattern + 64 * row;
	for (i = 0; i < 64; i++, cur_note++) {
		cx = song_get_mix_channel(i);
		if (cx && (cx->flags & CHN_MUTE)) continue; /* ick */
		if (cur_note->volume_effect == VOL_EFFECT_VOLUME) {
			vol = cur_note->volume;
		} else {
			vol = KEYJAZZ_DEFAULTVOL;
		}
		song_keyrecord(cur_note->instrument, cur_note->instrument, cur_note->note,
			vol, i, cur_note->effect, cur_note->parameter);
	}
}

// ------------------------------------------------------------------------
// info on what's playing

enum song_mode song_get_mode()
{
        if ((mp->m_dwSongFlags & (SONG_ENDREACHED | SONG_PAUSED)) == (SONG_ENDREACHED | SONG_PAUSED))
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

void song_set_current_tempo(int new_tempo)
{
	song_lock_audio();
        mp->m_nMusicTempo = CLAMP(new_tempo, 31, 255);
	song_unlock_audio();
}
int song_get_current_tempo()
{
        return mp->m_nMusicTempo;
}

int song_get_current_global_volume()
{
        return mp->m_nGlobalVolume;
}

int song_get_current_order()
{
        return mp->m_nCurrentOrder;
}

int song_get_playing_pattern()
{
        return mp->m_nCurrentPattern;
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
	*left = mp->gnVULeft;
	*right = mp->gnVURight;
}

void song_update_playing_instrument(int i_changed)
{
	SONGVOICE *channel;
	SONGINSTRUMENT *inst;

	song_lock_audio();
	int n = MIN(mp->m_nMixChannels, mp->m_nMaxMixChannels);
	while (n--) {
		channel = mp->Voices + mp->VoiceMix[n];
		if (channel->pHeader && channel->pHeader == mp->Instruments[i_changed]) {
			csf_instrument_change(mp, channel, i_changed, true, false, false);
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
				setup_channel_filter(channel, false, 256, mp->gdwMixingFreq);
			} else {
				channel->nCutOff = 0x7F;
				if (inst->nIFR & 0x80) {
					setup_channel_filter(channel, false, 256, mp->gdwMixingFreq);
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
	SONGVOICE *channel;
	SONGSAMPLE *inst;
	
	song_lock_audio();
	int n = MIN(mp->m_nMixChannels, mp->m_nMaxMixChannels);
	while (n--) {
		channel = mp->Voices + mp->VoiceMix[n];
		if (channel->pInstrument && channel->pCurrentSample) {
			int s = channel->pInstrument - mp->Samples;
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
			channel->nInsVol = inst->nGlobalVol;
		}
	}
	song_unlock_audio();
}

void song_get_playing_samples(int samples[])
{
	SONGVOICE *channel;
	
	memset(samples, 0, SCHISM_MAX_SAMPLES * sizeof(int));
	
	song_lock_audio();
	int n = MIN(mp->m_nMixChannels, mp->m_nMaxMixChannels);
	while (n--) {
		channel = mp->Voices + mp->VoiceMix[n];
		if (channel->pInstrument && channel->pCurrentSample) {
			int s = channel->pInstrument - mp->Samples;
			if (s >= 0 && s < SCHISM_MAX_SAMPLES) {
				samples[s] = MAX(samples[s], 1 + channel->strike);
			}
		} else {
			// no sample.
			// (when does this happen?)
		}
	}
	song_unlock_audio();
}

void song_get_playing_instruments(int instruments[])
{
	SONGVOICE *channel;
	
	memset(instruments, 0, SCHISM_MAX_INSTRUMENTS * sizeof(int));
	
	song_lock_audio();
	int n = MIN(mp->m_nMixChannels, mp->m_nMaxMixChannels);
	while (n--) {
		channel = mp->Voices + mp->VoiceMix[n];
		int ins = song_get_instrument_number((song_instrument *) channel->pHeader);
		if (ins > 0 && ins < SCHISM_MAX_INSTRUMENTS) {
			instruments[ins] = MAX(instruments[ins], 1 + channel->strike);
		}
	}
	song_unlock_audio();
}

// ------------------------------------------------------------------------
// changing the above info

void song_set_current_speed(int speed)
{
        if (speed < 1 || speed > 255)
                return;

	song_lock_audio();
        mp->m_nMusicSpeed = speed;
	song_unlock_audio();
}

void song_set_current_global_volume(int volume)
{
        if (volume < 0 || volume > 128)
                return;

	song_lock_audio();
        mp->m_nGlobalVolume = volume;
	song_unlock_audio();
}

void song_set_current_order(int order)
{
	song_lock_audio();
	csf_set_current_order(mp, order);
	song_unlock_audio();
}

// Ctrl-F7
void song_set_next_order(int order)
{
	song_lock_audio();
	mp->m_nLockedOrder = order;
	song_unlock_audio();
}

// Alt-F11
int song_toggle_orderlist_locked(void)
{
	mp->m_dwSongFlags ^= SONG_ORDERLOCKED;
	if (mp->m_dwSongFlags & SONG_ORDERLOCKED)
		mp->m_nLockedOrder = mp->m_nCurrentOrder;
	else
		mp->m_nLockedOrder = MAX_ORDERS;
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

extern int stop_on_load; // XXX craphack

#define CFG_GET_A(v,d) audio_settings.v = cfg_get_number(cfg, "Audio", #v, d)
#define CFG_GET_M(v,d) audio_settings.v = cfg_get_number(cfg, "Mixer Settings", #v, d)
void cfg_load_audio(cfg_file_t *cfg)
{
	CFG_GET_A(sample_rate, 44100);
	CFG_GET_A(bits, 16);
	CFG_GET_A(channels, 2);
#ifdef WIN32
	CFG_GET_A(buffer_size, 2048);
#else
	CFG_GET_A(buffer_size, 1024);
#endif
	
	CFG_GET_M(channel_limit, 64);
	CFG_GET_M(interpolation_mode, SRCMODE_LINEAR);
	CFG_GET_M(oversampling, 1);
	CFG_GET_M(hq_resampling, 1);
	CFG_GET_M(noise_reduction, 1);
	CFG_GET_M(no_ramping, 0);
	CFG_GET_M(surround_effect, 1);

	if (audio_settings.channels != 1 && audio_settings.channels != 2)
		audio_settings.channels = 2;
	if (audio_settings.bits != 8 && audio_settings.bits != 16)
		audio_settings.bits = 16;
	audio_settings.channel_limit = CLAMP(audio_settings.channel_limit, 4, MAX_VOICES);
	audio_settings.interpolation_mode = CLAMP(audio_settings.interpolation_mode, 0, 3);

	diskwriter_output_rate = cfg_get_number(cfg, "Diskwriter", "rate", 44100);
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
	
	stop_on_load = !!cfg_get_number(cfg, "General", "stop_on_load", 1);
}

#define CFG_SET_A(v) cfg_set_number(cfg, "Audio", #v, audio_settings.v)
#define CFG_SET_M(v) cfg_set_number(cfg, "Mixer Settings", #v, audio_settings.v)
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
	CFG_SET_M(no_ramping);

	// Say, what happened to the switch for this in the gui?
	CFG_SET_M(surround_effect);

	// hmmm....
	//     [Equalizer]
	//     low_band=freq/gain
	//     med_low_band=freq/gain
	//     etc.
	// would be a cleaner way of storing this

	cfg_set_number(cfg, "EQ Low Band", "freq", audio_settings.eq_freq[0]);
	cfg_set_number(cfg, "EQ Med Low Band", "freq", audio_settings.eq_freq[1]);
	cfg_set_number(cfg, "EQ Med High Band", "freq", audio_settings.eq_freq[2]);
	cfg_set_number(cfg, "EQ High Band", "freq", audio_settings.eq_freq[3]);

	cfg_set_number(cfg, "EQ Low Band", "gain", audio_settings.eq_gain[0]);
	cfg_set_number(cfg, "EQ Med Low Band", "gain", audio_settings.eq_gain[1]);
	cfg_set_number(cfg, "EQ Med High Band", "gain", audio_settings.eq_gain[2]);
	cfg_set_number(cfg, "EQ High Band", "gain", audio_settings.eq_gain[3]);
}

void cfg_save_audio(cfg_file_t *cfg)
{
	cfg_atexit_save_audio(cfg);

	cfg_set_number(cfg, "Diskwriter", "rate", diskwriter_output_rate);
	cfg_set_number(cfg, "Diskwriter", "bits", diskwriter_output_bits);
	cfg_set_number(cfg, "Diskwriter", "channels", diskwriter_output_channels);

	cfg_set_number(cfg, "General", "stop_on_load", stop_on_load);
}

// ------------------------------------------------------------------------------------------------------------
static void _schism_midi_out_note(int chan, const MODCOMMAND *m)
{
	unsigned int tc;
	int m_note;

	unsigned char buf[4];
	int ins, mc, mg, mbl, mbh;
	int need_note, need_velocity;
	SONGVOICE *c;

	if (!mp || !song_is_instrument_mode() || (status.flags & MIDI_LIKE_TRACKER)) return;

    /*if(m)
    fprintf(stderr, "midi_out_note called (ch %d)note(%d)instr(%d)volcmd(%02X)cmd(%02X)vol(%02X)p(%02X)\n",
        chan, m->note, m->instr, m->volcmd, m->command, m->vol, m->param);
    else fprintf(stderr, "midi_out_note called (ch %d) m=%p\n", m);*/

	if (!midi_playing) {
		csf_process_midi_macro(mp, 0, &mp->m_MidiCfg.szMidiGlb[MIDIOUT_START*32], 0, 0, 0, 0); // START!
		midi_playing = 1;
	}

	if (chan < 0) {
		return;
	}

	c = &mp->Voices[chan];

	chan %= 64;

	if (!m) {
		if (last_row_number != (signed) mp->m_nRow) return;
		m = last_row[chan];
		if (!m) return;
	} else {
		last_row[chan] = m;
		last_row_number = mp->m_nRow;
	}

	ins = ins_tracker[chan];
	if (m->instr > 0) {
		ins = m->instr;
		ins_tracker[chan] = ins;
	}
	if (ins < 0 || ins >= MAX_INSTRUMENTS)
		return; /* err...  almost certainly */
	if (!mp->Instruments[ins]) return;

	if (mp->Instruments[ins]->nMidiChannelMask >= 0x10000) {
		mc = chan % 16;
	} else {
		mc = 0;
		if(mp->Instruments[ins]->nMidiChannelMask > 0)
			while(!(mp->Instruments[ins]->nMidiChannelMask & (1 << mc)))
				++mc;
	}

	m_note = m->note;
	tc = mp->m_nTickCount % mp->m_nMusicSpeed;
#if 0
printf("channel = %d note=%d\n",chan,m_note);
#endif
	if (c->nRealtime) {
		/* goggles */
	} else if (m->command == CMD_S3MCMDEX) {
		switch (m->param & 0x80) {
		case 0xC0: /* note cut */
			if (tc == (((unsigned)m->param) & 15)) {
				m_note = NOTE_CUT;
			} else if (tc != 0) return;
			break;

		case 0xD0: /* note delay */
			if (tc != (((unsigned)m->param) & 15)) return;
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
			csf_process_midi_macro(mp, chan, &mp->m_MidiCfg.szMidiGlb[MIDIOUT_NOTEOFF*32],
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
		if (note_tracker[chan] != 0) {
			csf_process_midi_macro(mp, chan, &mp->m_MidiCfg.szMidiGlb[MIDIOUT_NOTEOFF*32],
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

	mg = (mp->Instruments[ins]->nMidiProgram)
		+ ((midi_flags & MIDI_BASE_PROGRAM1) ? 1 : 0);
	mbl = mp->Instruments[ins]->wMidiBank;
	mbh = (mp->Instruments[ins]->wMidiBank >> 7) & 127;

	if (mbh > -1 && was_bankhi[mc] != mbh) {
		buf[0] = 0xB0 | (mc & 15); // controller
		buf[1] = 0x00; // corse bank/select
		buf[2] = mbh; // corse bank/select
		csf_midi_send(mp, buf, 3, 0, 0);
		was_bankhi[mc] = mbh;
	}
	if (mbl > -1 && was_banklo[mc] != mbl) {
		buf[0] = 0xB0 | (mc & 15); // controller
		buf[1] = 0x20; // fine bank/select
		buf[2] = mbl; // fine bank/select
		csf_midi_send(mp, buf, 3, 0, 0);
		was_banklo[mc] = mbl;
	}
	if (mg > -1 && was_program[mc] != mg) {
		was_program[mc] = mg;
		csf_process_midi_macro(mp, chan, &mp->m_MidiCfg.szMidiGlb[MIDIOUT_PROGRAM*32],
			mg, 0, 0, ins); // program change
	}
	if (c->dwFlags & CHN_MUTE) {
		// don't send noteon events when muted
	} else if (need_note > 0) {
		if (need_velocity == -1) need_velocity = 64; // eh?
		need_velocity = CLAMP(need_velocity*2,0,127);
		csf_process_midi_macro(mp, chan, &mp->m_MidiCfg.szMidiGlb[MIDIOUT_NOTEON*32],
			0, need_note, need_velocity, ins); // noteon
	} else if (need_velocity > -1 && note_tracker[chan] > 0) {
		need_velocity = CLAMP(need_velocity*2,0,127);
		csf_process_midi_macro(mp, chan, &mp->m_MidiCfg.szMidiGlb[MIDIOUT_VOLUME*32],
			need_velocity, note_tracker[chan], need_velocity, ins); // volume-set
	}

}
static void _schism_midi_out_raw(const unsigned char *data, unsigned int len, unsigned int pos)
{
#if 0
	i = (8000*(audio_buffer_size - delay));
	i /= (CSoundFile::gdwMixingFreq);
#endif
#if 0
	for (int i=0; i < len; i++) {
		printf("%02x ",data[i]);
	}puts("");
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
			audio_callback(0, (uint8_t *) nosound_buffer, 8820);
		}
		song_unlock_audio();
		SDL_Delay(200);
	}
	return 0; /* shrug... */
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
	|| !strcasecmp(driver, "nul")
	|| !strcmp(driver, "/dev/null")
	|| !strcasecmp(driver, "none")
	|| !strcasecmp(driver, "nosound")
	|| !strcasecmp(driver, "silence")
	|| !strcasecmp(driver, "silense")
	|| !strcasecmp(driver, "quiet")
	|| !strcasecmp(driver, "off")) {
		strcpy(driver_name, "nosound");

		/* don't change this without looking at nosound_thread() */
		csf_set_wave_config(mp, 11025, 8, 2);
		need_samples = 4410 * 2;
		audio_output_channels = 2;
		audio_output_bits = 8;
		audio_sample_size = 2;

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
		int need_name = 1;

		if (*driver == '/') {
			if (strncmp(driver, "/dev/", 5) == 0) {
				put_env_var("SDL_PATH_DSP", driver);
				put_env_var("AUDIODEV", driver);
			} else {
				put_env_var("SDL_AUDIODRIVER", "disk");
				put_env_var("SDL_DISKAUDIOFILE", driver);
			}

			strncpy(driver_name, driver, sizeof(driver_name)-2);
			need_name = 0;
		} else if (strcasecmp(driver, "sdlauto")) {
			/* unknown audio driver- use SDL */
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
		csf_set_wave_config(mp, obtained.freq,
			obtained.format & 255,
			obtained.channels);
		audio_output_channels = obtained.channels;
		audio_output_bits = obtained.format & 255;
		audio_sample_size = audio_output_channels * (audio_output_bits/8);

		if (need_name) SDL_AudioDriverName(driver_name, sizeof(driver_name));

		song_print_info_top(driver_name);
		log_appendf(5, " %d Hz, %d bit, %s", obtained.freq,
			(int)(obtained.format & 0xff),
			obtained.channels == 1 ? "mono" : "stereo");
		log_appendf(5, " Buffer size: %d samples",
				(int)obtained.samples);
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

	samples_played = 0;
	
	first_init = 0;

	song_unlock_audio();
	song_start_audio();
}

void song_init_eq(int do_reset)
{
	uint32_t pg[4];
	uint32_t pf[4];
	int i;

	for (i = 0; i < 4; i++) {
		pg[i] = audio_settings.eq_gain[i];
		pf[i] = 120 + (((i*128) * audio_settings.eq_freq[i])
			* (CSoundFile::gdwMixingFreq / 128) / 1024);
	}

	set_eq_gains(pg, 4, pf, do_reset ? true : false, mp->gdwMixingFreq);
}


void song_init_modplug(void)
{
	song_lock_audio();
	
        CSoundFile::m_nMaxMixChannels = audio_settings.channel_limit;
	// the last param is the equalizer, which apparently isn't functional
        csf_set_wave_config_ex(mp, false,
				false,
				false,
				true, //only makes sense... audio_settings.hq_resampling,
				false,
				audio_settings.noise_reduction,
				false);/*EQ off here... */
	if (audio_settings.oversampling) {
		/* not intuitive XXX */
	}
        csf_set_resampling_mode(mp, audio_settings.interpolation_mode);
	CSoundFile::gdwSoundSetup |= SNDMIX_EQ;
	if (audio_settings.no_ramping)
		CSoundFile::gdwSoundSetup |= SNDMIX_NORAMPING;
	else
		CSoundFile::gdwSoundSetup &= (~SNDMIX_NORAMPING);
	
	// disable the S91 effect? (this doesn't make anything faster, it
	// just sounds better with one woofer.)
	song_set_surround(audio_settings.surround_effect);

	// update midi queue configuration
	midi_queue_alloc(audio_buffer_size, audio_sample_size, CSoundFile::gdwMixingFreq) ;

	// timelimit the playback_update() calls when midi isn't actively going on
	audio_buffers_per_second = (CSoundFile::gdwMixingFreq / (audio_buffer_size * 8 * audio_sample_size));
	if (audio_buffers_per_second > 1) audio_buffers_per_second--;
	
	song_unlock_audio();
}

void song_initialise(void)
{
	csf_midi_out_note = _schism_midi_out_note;
	csf_midi_out_raw = _schism_midi_out_raw;

	mp = new CSoundFile;

	mp->Create(NULL, 0);


	//song_stop(); <- song_new does this
	song_set_linear_pitch_slides(1);
	song_new(0);
	
	// hmm.
	CSoundFile::gdwSoundSetup |= SNDMIX_MUTECHNMODE;
}

