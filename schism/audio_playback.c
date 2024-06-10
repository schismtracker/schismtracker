/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "headers.h"

#include "it.h"
#include "page.h"
#include "cmixer.h"
#include "sndfile.h"
#include "song.h"
#include "slurp.h"
#include "config-parser.h"

#include "disko.h"
#include "event.h"

#include <assert.h>

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <math.h>

#include "sdlmain.h"

#include "midi.h"

#include "snd_fm.h"
#include "snd_gm.h"

// Default audio configuration
// (XXX: Can DEF_SAMPLE_RATE be defined to 48000 everywhere?
// Does any sound card NOT support 48khz decently nowadays?)
#ifdef SCHISM_WII
# define DEF_SAMPLE_RATE 48000
#else
# define DEF_SAMPLE_RATE 44100
#endif
#ifdef SCHISM_WIN32
# define DEF_BUFFER_SIZE 2048
#else
# define DEF_BUFFER_SIZE 1024
#endif
#define DEF_CHANNEL_LIMIT 128

static int midi_playing;
// ------------------------------------------------------------------------

#define SMP_INIT (UINT_MAX - 1) /* for a click noise on init */

unsigned int samples_played = 0;
unsigned int max_channels_used = 0;

signed short *audio_buffer = NULL;
unsigned int audio_buffer_samples = 0; /* multiply by audio_sample_size to get bytes */

unsigned int audio_output_channels = 2;
unsigned int audio_output_bits = 16;

static unsigned int audio_sample_size;
static int audio_buffers_per_second = 0;
static int audio_writeout_count = 0;

struct audio_settings audio_settings;

static void _schism_midi_out_note(int chan, const song_note_t *m);
static void _schism_midi_out_raw(const unsigned char *data, unsigned int len, unsigned int delay);

/* Audio driver related stuff */
/* XXX how much of this is really needed now? */

/* The (short) name of the SDL driver in use, e.g. "alsa" */
static char *driver_name = NULL;
static char *device_name = NULL;

/* Whatever was in the config file. This is used if no driver is given to audio_setup. */
static char cfg_audio_driver[256] = { 0 };
static char cfg_audio_device[256] = { 0 };

// ------------------------------------------------------------------------

struct audio_device* audio_device_list = NULL;
int audio_device_list_size = 0;

static SDL_AudioDeviceID current_audio_device = 0;

// ------------------------------------------------------------------------
// playback

extern int midi_bend_hit[64], midi_last_bend_hit[64];
extern void vis_work_16s(short *in, int inlen);
extern void vis_work_16m(short *in, int inlen);
extern void vis_work_8s(char *in, int inlen);
extern void vis_work_8m(char *in, int inlen);

// this gets called from sdl
static void audio_callback(UNUSED void *qq, uint8_t * stream, int len)
{
	unsigned int wasrow = current_song->row;
	unsigned int waspat = current_song->current_order;
	int i, n;

	memset(stream, 0, len);

	if (!stream || !len || !current_song) {
		if (status.current_page == PAGE_WATERFALL || status.vis_style == VIS_FFT) {
			vis_work_8m(NULL, 0);
		}
		song_stop_unlocked(0);
		goto POST_EVENT;
	}

	if (samples_played >= SMP_INIT) {
		memset(stream, 0x80, len);
		samples_played++; // will loop back to 0
		return;
	}

	if (current_song->flags & SONG_ENDREACHED) {
		n = 0;
	} else {
		n = csf_read(current_song, stream, len);
		if (!n) {
			if (status.current_page == PAGE_WATERFALL
			|| status.vis_style == VIS_FFT) {
				vis_work_8m(NULL, 0);
			}
			song_stop_unlocked(0);
			goto POST_EVENT;
		}
		samples_played += n;
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

	if (current_song->num_voices > max_channels_used)
		max_channels_used = MIN(current_song->num_voices, max_voices);
POST_EVENT:
	audio_writeout_count++;
	if (audio_writeout_count > audio_buffers_per_second) {
		audio_writeout_count = 0;
	} else if (waspat == current_song->current_order && wasrow == current_song->row
			&& !midi_need_flush()) {
		/* skip it */
		return;
	}

	/* send at end */
	SDL_Event e;
	e.user.type = SCHISM_EVENT_PLAYBACK;
	e.user.code = 0;
	e.user.data1 = NULL;
	e.user.data2 = NULL;
	SDL_PushEvent(&e);
}

// ------------------------------------------------------------------------------------------------------------
// audio device list
// TODO: provide a way to pick between audio drivers as well

static void free_audio_device_list(void) {
	for (int count = 0; count < audio_device_list_size; count++)
		free(audio_device_list[count].name);

	free(audio_device_list);

	audio_device_list = NULL;
	audio_device_list_size = 0;
}

/* called when SDL_AUDIODEVICEADDED/SDL_AUDIODEVICEREMOVED event received */
int refresh_audio_device_list(void) {
	free_audio_device_list();

	const int count = SDL_GetNumAudioDevices(0);
	if (count < 0)
		return 0;

	audio_device_list = malloc(count * sizeof(*audio_device_list));
	if (!audio_device_list)
		return 0;

	for (int i = 0; i < count; i++) {
		struct audio_device* dev = audio_device_list + i;
		dev->id = i;
		dev->name = str_dup(SDL_GetAudioDeviceName(i, 0));
	}

	audio_device_list_size = count;

	return 1;
}

// ------------------------------------------------------------------------------------------------------------
// note playing

/* this should be in page.c; the audio handling code doesn't need to know what
   a page is, much less be talking to them */
static void main_song_mode_changed_cb(void)
{
	int n;
	for (n = 0; n < PAGE_MAX; n++) {
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
That is, keyjazz_note_to_chan[66] will indicate in which channel F-5 was played most recently.
This will break if the same note was keydown'd twice without a keyup, but I think that's a
fairly unlikely scenario that you'd have to TRY to bring about. */
static int keyjazz_note_to_chan[NOTE_LAST + 1];
/* last note played by channel tracking */
static int keyjazz_chan_to_note[MAX_CHANNELS + 1];

/* **** chan ranges from 1 to 64   */
static int song_keydown_ex(int samp, int ins, int note, int vol, int chan, int effect, int param)
{
	int ins_mode;
	int midi_note = note; /* note gets overwritten, possibly NOTE_NONE */
	song_voice_t *c;
	song_note_t mc;
	song_sample_t *s = NULL;
	song_instrument_t *i = NULL;

	if (chan == KEYJAZZ_CHAN_CURRENT) {
		chan = current_play_channel;
		if (multichannel_mode)
			song_change_current_play_channel(1, 1);
	}
    // back to the 0..63 range
    int chan_internal = chan -1;

	song_lock_audio();

	c = current_song->voices + chan_internal;

	ins_mode = song_is_instrument_mode();

	if (NOTE_IS_NOTE(note)) {
		// keep track of what channel this note was played in so we can note-off properly later
		if (keyjazz_chan_to_note[chan]) {
			// reset note-off pending state for last note in channel
			keyjazz_note_to_chan[keyjazz_chan_to_note[chan]] = 0;
		}

		keyjazz_note_to_chan[note] = chan;
		keyjazz_chan_to_note[chan] = note;

		// handle blank instrument values and "fake" sample #0 (used by sample loader)
		if (samp == 0)
			samp = c->last_instrument;
		else if (samp == KEYJAZZ_INST_FAKE)
			samp = 0; // dumb hack
		if (ins == 0)
			ins = c->last_instrument;
		else if (ins == KEYJAZZ_INST_FAKE)
			ins = 0; // dumb hack
		c->last_instrument = ins_mode ? ins : samp;

		// give the channel a sample, and maybe an instrument
		s = (samp == KEYJAZZ_NOINST) ? NULL : current_song->samples + samp;
		i = (ins == KEYJAZZ_NOINST) ? NULL : song_get_instrument(ins); // blah

		if (i && samp == KEYJAZZ_NOINST) {
			// we're playing an instrument and don't know what sample! WHAT WILL WE EVER DO?!
			// well, look it up in the note translation table, silly.
			// the weirdness here the default value here is to mimic IT behavior: we want to use
			// the sample corresponding to the instrument number if in sample mode and no sample
			// is defined for the note in the instrument's note map.
			s = csf_translate_keyboard(current_song, i, note, ins_mode ? NULL : (current_song->samples + ins));
		}
	}

	c->row_effect = effect;
	c->row_param = param;

	// now do a rough equivalent of csf_instrument_change and csf_note_change
	if (i)
		csf_check_nna(current_song, chan_internal, ins, note, 0);
	if (s) {
		if (c->flags & CHN_ADLIB) {
			OPL_NoteOff(chan_internal);
			OPL_Patch(chan_internal, s->adlib_bytes);
		}

		c->flags = (s->flags & CHN_SAMPLE_FLAGS) | (c->flags & CHN_MUTE);
		if (c->flags & CHN_MUTE) {
			c->flags |= CHN_NNAMUTE;
		}

		c->cutoff = 0x7f;
		c->resonance = 0;
		if (i) {
			c->ptr_instrument = i;

			if (!(i->flags & ENV_VOLCARRY)) c->vol_env_position = 0;
			if (!(i->flags & ENV_PANCARRY)) c->pan_env_position = 0;
			if (!(i->flags & ENV_PITCHCARRY)) c->pitch_env_position = 0;
			if (i->flags & ENV_VOLUME) c->flags |= CHN_VOLENV;
			if (i->flags & ENV_PANNING) c->flags |= CHN_PANENV;
			if (i->flags & ENV_PITCH) c->flags |= CHN_PITCHENV;

			i->played = 1;

			if ((status.flags & MIDI_LIKE_TRACKER) && i) {
				if (i->midi_channel_mask) {
					GM_KeyOff(chan_internal);
					GM_DPatch(chan_internal, i->midi_program, i->midi_bank, i->midi_channel_mask);
				}
			}

			if (i->ifc & 0x80)
				c->cutoff = i->ifc & 0x7f;
			if (i->ifr & 0x80)
				c->resonance = i->ifr & 0x7f;
			//?
			c->vol_swing = i->vol_swing;
			c->pan_swing = i->pan_swing;
			c->nna = i->nna;
		} else {
			c->ptr_instrument = NULL;
			c->cutoff = 0x7f;
			c->resonance = 0;
		}

		c->master_channel = 0; // indicates foreground channel.
		//c->flags &= ~(CHN_PINGPONGFLAG);

		// ?
		//c->autovib_depth = 0;
		//c->autovib_position = 0;

		// csf_note_change copies stuff from c->ptr_sample as long as c->length is zero
		// and if period != 0 (ie. sample not playing at a stupid rate)
		c->ptr_sample = s;
		c->length = 0;
		// ... but it doesn't copy the volumes, for somewhat obvious reasons.
		c->volume = (vol == KEYJAZZ_DEFAULTVOL) ? s->volume : (((unsigned) vol) << 2);
		c->instrument_volume = s->global_volume;
		if (i)
			c->instrument_volume = (c->instrument_volume * i->global_volume) >> 7;
		c->global_volume = 64;
		// gotta set these by hand, too
		c->c5speed = s->c5speed;
		c->new_note = note;
		s->played = 1;
	} else if (NOTE_IS_NOTE(note)) {
		// Note given with no sample number. This might happen if on the instrument list and playing
		// an instrument that has no sample mapped for the given note. In this case, ignore the note.
		note = NOTE_NONE;
	}
	if (c->increment < 0)
		c->increment = -c->increment; // lousy hack
	csf_note_change(current_song, chan_internal, note, 0, 0, 1);

	if (!(status.flags & MIDI_LIKE_TRACKER) && i) {
		/* midi keyjazz shouldn't require a sample */
		mc.note = note ? note : midi_note;

		mc.instrument = ins;
		mc.voleffect = VOLFX_VOLUME;
		mc.volparam = vol;
		mc.effect = effect;
		mc.param = param;
		_schism_midi_out_note(chan_internal, &mc);
	}

	/*
	TODO:
	- If this is the ONLY channel playing, and the song is stopped, always reset the tick count
	  (will fix the "random" behavior for most effects)
	- If other channels are playing, don't reset the tick count, but do process first-tick effects
	  for this note *right now* (this will fix keyjamming with effects like Oxx and SCx)
	- Need to handle volume column effects with this function...
	*/
	if (current_song->flags & SONG_ENDREACHED) {
		current_song->flags &= ~SONG_ENDREACHED;
		current_song->flags |= SONG_PAUSED;
	}

	song_unlock_audio();

	return chan;
}

int song_keydown(int samp, int ins, int note, int vol, int chan)
{
	return song_keydown_ex(samp, ins, note, vol, chan, FX_PANNING, 0x80);
}

int song_keyrecord(int samp, int ins, int note, int vol, int chan, int effect, int param)
{
	return song_keydown_ex(samp, ins, note, vol, chan, effect, param);
}

int song_keyup(int samp, int ins, int note)
{
	int chan = keyjazz_note_to_chan[note];
	if (!chan) {
		// could not find channel, drop.
		return -1;
	};
	return song_keyup_channel(samp, ins, note, chan);
}

int song_keyup_channel(int samp, int ins, int note, int chan) {
	if (keyjazz_chan_to_note[chan] != note) {
		return -1;
	}
	keyjazz_chan_to_note[chan] = 0;
	keyjazz_note_to_chan[note] = 0;
	return song_keydown_ex(samp, ins, NOTE_OFF, KEYJAZZ_DEFAULTVOL, chan, 0, 0);
}

void song_single_step(int patno, int row)
{
	int total_rows;
	int i, vol, smp, ins;
	song_note_t *pattern, *cur_note;
	song_voice_t *cx;

	total_rows = song_get_pattern(patno, &pattern);
	if (!pattern || row >= total_rows) return;

	cur_note = pattern + 64 * row;
	cx = song_get_mix_channel(0);
	for (i = 1; i <= 64; i++, cx++, cur_note++) {
		if (cx && (cx->flags & CHN_MUTE)) continue; /* ick */
		if (cur_note->voleffect == VOLFX_VOLUME) {
			vol = cur_note->volparam;
		} else {
			vol = KEYJAZZ_DEFAULTVOL;
		}

		// look familiar? this is modified slightly from pattern_editor_insert
		// (and it is wrong for the same reason as described there)
		smp = ins = cur_note->instrument;
		if (song_is_instrument_mode()) {
			if (ins < 1)
				ins = KEYJAZZ_NOINST;
			smp = -1;
		} else {
			if (smp < 1)
				smp = KEYJAZZ_NOINST;
			ins = -1;
		}

		song_keyrecord(smp, ins, cur_note->note,
			vol, i, cur_note->effect, cur_note->param);
	}
}

// ------------------------------------------------------------------------------------------------------------

// this should be called with the audio LOCKED
static void song_reset_play_state(void)
{
	memset(midi_bend_hit, 0, sizeof(midi_bend_hit));
	memset(midi_last_bend_hit, 0, sizeof(midi_last_bend_hit));
	memset(keyjazz_note_to_chan, 0, sizeof(keyjazz_note_to_chan));
	memset(keyjazz_chan_to_note, 0, sizeof(keyjazz_chan_to_note));

	// turn this crap off
	current_song->mix_flags &= ~(SNDMIX_NOBACKWARDJUMPS | SNDMIX_DIRECTTODISK);

	OPL_Reset(); /* gruh? */

	csf_set_current_order(current_song, 0);

	current_song->repeat_count = 0;
	current_song->buffer_count = 0;
	current_song->flags &= ~(SONG_PAUSED | SONG_PATTERNLOOP | SONG_ENDREACHED);

	current_song->stop_at_order = -1;
	current_song->stop_at_row = -1;
	samples_played = 0;
}

void song_start_once(void)
{
	song_lock_audio();

	song_reset_play_state();
	current_song->mix_flags |= SNDMIX_NOBACKWARDJUMPS;
	max_channels_used = 0;
	current_song->repeat_count = -1; // FIXME do this right

	GM_SendSongStartCode();
	song_unlock_audio();
	main_song_mode_changed_cb();

	csf_reset_playmarks(current_song);
}

void song_start(void)
{
	song_lock_audio();

	song_reset_play_state();
	max_channels_used = 0;

	GM_SendSongStartCode();
	song_unlock_audio();
	main_song_mode_changed_cb();

	csf_reset_playmarks(current_song);
}

void song_pause(void)
{
	song_lock_audio();
	// Highly unintuitive, but SONG_PAUSED has nothing to do with pause.
	if (!(current_song->flags & SONG_PAUSED))
		current_song->flags ^= SONG_ENDREACHED;
	song_unlock_audio();
	main_song_mode_changed_cb();
}

void song_stop(void)
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

static const song_note_t *last_row[64];
static int last_row_number = -1;

void song_stop_unlocked(int quitting)
{
	if (!current_song) return;

	if (midi_playing) {
		unsigned char moff[4];

		/* shut off everything; not IT like, but less annoying */
		for (int chan = 0; chan < 64; chan++) {
			if (note_tracker[chan] != 0) {
				for (int j = 0; j < 16; j++) {
					csf_process_midi_macro(current_song, chan,
						current_song->midi_config.note_off,
						0, note_tracker[chan], 0, j);
				}
				moff[0] = 0x80 + chan;
				moff[1] = note_tracker[chan];
				csf_midi_send(current_song, (unsigned char *) moff, 2, 0, 0);
			}
		}
		for (int j = 0; j < 16; j++) {
			moff[0] = 0xe0 + j;
			moff[1] = 0;
			csf_midi_send(current_song, (unsigned char *) moff, 2, 0, 0);

			moff[0] = 0xb0 + j;	/* channel mode message */
			moff[1] = 0x78;		/* all sound off */
			moff[2] = 0;
			csf_midi_send(current_song, (unsigned char *) moff, 3, 0, 0);

			moff[1] = 0x79;		/* reset all controllers */
			csf_midi_send(current_song, (unsigned char *) moff, 3, 0, 0);

			moff[1] = 0x7b;		/* all notes off */
			csf_midi_send(current_song, (unsigned char *) moff, 3, 0, 0);
		}

		csf_process_midi_macro(current_song, 0, current_song->midi_config.stop, 0, 0, 0, 0); // STOP!
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
	// Modplug doesn't actually have a "stop" mode, but if SONG_ENDREACHED is set, current_song->Read just returns.
	current_song->flags |= SONG_PAUSED | SONG_ENDREACHED;

	global_vu_left = 0;
	global_vu_right = 0;
	memset(audio_buffer, 0, audio_buffer_samples * audio_sample_size);
}




void song_loop_pattern(int pattern, int row)
{
	song_lock_audio();

	song_reset_play_state();

	max_channels_used = 0;
	csf_loop_pattern(current_song, pattern, row);

	GM_SendSongStartCode();

	song_unlock_audio();
	main_song_mode_changed_cb();

	csf_reset_playmarks(current_song);
}

void song_start_at_order(int order, int row)
{
	song_lock_audio();

	song_reset_play_state();

	csf_set_current_order(current_song, order);
	current_song->break_row = row;
	max_channels_used = 0;

	GM_SendSongStartCode();
	/* TODO: GM_SendSongPositionCode(calculate the number of 1/16 notes) */
	song_unlock_audio();
	main_song_mode_changed_cb();

	csf_reset_playmarks(current_song);
}

void song_start_at_pattern(int pattern, int row)
{
	if (pattern < 0 || pattern > 199)
		return;

	int n = song_next_order_for_pattern(pattern);

	if (n > -1) {
		song_start_at_order(n, row);
		return;
	}

	song_loop_pattern(pattern, row);
}

// ------------------------------------------------------------------------
// info on what's playing

enum song_mode song_get_mode(void)
{
	if ((current_song->flags & (SONG_ENDREACHED | SONG_PAUSED)) == (SONG_ENDREACHED | SONG_PAUSED))
		return MODE_STOPPED;
	if (current_song->flags & SONG_PAUSED)
		return MODE_SINGLE_STEP;
	if (current_song->flags & SONG_PATTERNPLAYBACK)
		return MODE_PATTERN_LOOP;
	return MODE_PLAYING;
}

// returned value is in seconds
unsigned int song_get_current_time(void)
{
	return samples_played / current_song->mix_frequency;
}

int song_get_current_tick(void)
{
	return current_song->tick_count % current_song->current_speed;
}
int song_get_current_speed(void)
{
	return current_song->current_speed;
}

void song_set_current_tempo(int new_tempo)
{
	song_lock_audio();
	current_song->current_tempo = CLAMP(new_tempo, 31, 255);
	song_unlock_audio();
}
int song_get_current_tempo(void)
{
	return current_song->current_tempo;
}

int song_get_current_global_volume(void)
{
	return current_song->current_global_volume;
}

int song_get_current_order(void)
{
	return current_song->current_order;
}

int song_get_playing_pattern(void)
{
	return current_song->current_pattern;
}

int song_get_current_row(void)
{
	return current_song->row;
}

int song_get_playing_channels(void)
{
	return MIN(current_song->num_voices, max_voices);
}

int song_get_max_channels(void)
{
	return max_channels_used;
}
// Returns the max value in dBs, scaled as 0 = -40dB and 128 = 0dB.
void song_get_vu_meter(int *left, int *right)
{
	*left = dB_s(40, global_vu_left/256.f, 0.f);
	*right = dB_s(40, global_vu_right/256.f, 0.f);
}

void song_update_playing_instrument(int i_changed)
{
	song_voice_t *channel;
	song_instrument_t *inst;

	song_lock_audio();
	int n = MIN(current_song->num_voices, max_voices);
	while (n--) {
		channel = current_song->voices + current_song->voice_mix[n];
		if (channel->ptr_instrument && channel->ptr_instrument == current_song->instruments[i_changed]) {
			csf_instrument_change(current_song, channel, i_changed, 1, 0);
			inst = channel->ptr_instrument;
			if (!inst) continue;

			/* special cases;
				mpt doesn't do this if porta-enabled, */
			if (inst->ifr & 0x80) {
				channel->resonance = inst->ifr & 0x7F;
			} else {
				channel->resonance = 0;
				channel->flags &= (~CHN_FILTER);
			}
			if (inst->ifc & 0x80) {
				channel->cutoff = inst->ifc & 0x7F;
				setup_channel_filter(channel, 0, 256, current_song->mix_frequency);
			} else {
				channel->cutoff = 0x7F;
				if (inst->ifr & 0x80) {
					setup_channel_filter(channel, 0, 256, current_song->mix_frequency);
				}
			}

			/* flip direction */
			channel->flags &= (~CHN_PINGPONGFLAG);
		}
	}
	song_unlock_audio();
}

void song_update_playing_sample(int s_changed)
{
	song_voice_t *channel;
	song_sample_t *inst;

	song_lock_audio();
	int n = MIN(current_song->num_voices, max_voices);
	while (n--) {
		channel = current_song->voices + current_song->voice_mix[n];
		if (channel->ptr_sample && channel->current_sample_data) {
			int s = channel->ptr_sample - current_song->samples;
			if (s != s_changed) continue;

			inst = channel->ptr_sample;
			if (inst->flags & (CHN_PINGPONGSUSTAIN|CHN_SUSTAINLOOP)) {
				channel->loop_start = inst->sustain_start;
				channel->loop_end = inst->sustain_end;
			} else if (inst->flags & (CHN_PINGPONGFLAG|CHN_PINGPONGLOOP|CHN_LOOP)) {
				channel->loop_start = inst->loop_start;
				channel->loop_end = inst->loop_end;
			}
			if (inst->flags & (CHN_PINGPONGSUSTAIN | CHN_SUSTAINLOOP
					    | CHN_PINGPONGFLAG | CHN_PINGPONGLOOP|CHN_LOOP)) {
				if (channel->length != channel->loop_end) {
					channel->length = channel->loop_end;
				}
			}
			if (channel->length > inst->length) {
				channel->current_sample_data = inst->data;
				channel->length = inst->length;
			}

			channel->flags &= ~(CHN_PINGPONGSUSTAIN
					| CHN_PINGPONGLOOP
					| CHN_PINGPONGFLAG
					| CHN_SUSTAINLOOP
					| CHN_LOOP);
			channel->flags |= inst->flags & (CHN_PINGPONGSUSTAIN
					| CHN_PINGPONGLOOP
					| CHN_PINGPONGFLAG
					| CHN_SUSTAINLOOP
					| CHN_LOOP);
			channel->instrument_volume = inst->global_volume;
		}
	}
	song_unlock_audio();
}

void song_get_playing_samples(int samples[])
{
	song_voice_t *channel;

	memset(samples, 0, MAX_SAMPLES * sizeof(int));

	song_lock_audio();
	int n = MIN(current_song->num_voices, max_voices);
	while (n--) {
		channel = current_song->voices + current_song->voice_mix[n];
		if (channel->ptr_sample && channel->current_sample_data) {
			int s = channel->ptr_sample - current_song->samples;
			if (s >= 0 && s < MAX_SAMPLES) {
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
	song_voice_t *channel;

	memset(instruments, 0, MAX_INSTRUMENTS * sizeof(int));

	song_lock_audio();
	int n = MIN(current_song->num_voices, max_voices);
	while (n--) {
		channel = current_song->voices + current_song->voice_mix[n];
		int ins = song_get_instrument_number((song_instrument_t *) channel->ptr_instrument);
		if (ins > 0 && ins < MAX_INSTRUMENTS) {
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
	current_song->current_speed = speed;
	song_unlock_audio();
}

void song_set_current_global_volume(int volume)
{
	if (volume < 0 || volume > 128)
		return;

	song_lock_audio();
	current_song->current_global_volume = volume;
	song_unlock_audio();
}

void song_set_current_order(int order)
{
	song_lock_audio();
	csf_set_current_order(current_song, order);
	song_unlock_audio();
}

// Ctrl-F7
void song_set_next_order(int order)
{
	song_lock_audio();
	current_song->process_order = order - 1;
	song_unlock_audio();
}

// Alt-F11
int song_toggle_orderlist_locked(void)
{
	current_song->flags ^= SONG_ORDERLOCKED;
	return current_song->flags & SONG_ORDERLOCKED;
}

// ------------------------------------------------------------------------
// global flags

void song_flip_stereo(void)
{
	current_song->mix_flags ^= SNDMIX_REVERSESTEREO;
}

int song_get_surround(void)
{
	return (current_song->mix_flags & SNDMIX_NOSURROUND) ? 0 : 1;
}

void song_set_surround(int on)
{
	if (on)
		current_song->mix_flags &= ~SNDMIX_NOSURROUND;
	else
		current_song->mix_flags |= SNDMIX_NOSURROUND;

	// without copying the value back to audio_settings, it won't get saved (oops)
	audio_settings.surround_effect = on;
}

// ------------------------------------------------------------------------------------------------------------
// well this is certainly a dopey place to put this, config having nothing to do with playback... maybe i
// should put all the cfg_ stuff in config.c :/

void audio_parse_driver_spec(const char* spec, char** driver, char** device) {
	if (!str_break(spec, ':', driver, device)) {
		*driver = str_dup(spec);
		*device = NULL;
	}
}

#define CFG_GET_A(v,d) audio_settings.v = cfg_get_number(cfg, "Audio", #v, d)
#define CFG_GET_M(v,d) audio_settings.v = cfg_get_number(cfg, "Mixer Settings", #v, d)
void cfg_load_audio(cfg_file_t *cfg)
{
	CFG_GET_A(sample_rate, DEF_SAMPLE_RATE);
	CFG_GET_A(bits, 16);
	CFG_GET_A(channels, 2);
	CFG_GET_A(buffer_size, DEF_BUFFER_SIZE);
	CFG_GET_A(master.left, 31);
	CFG_GET_A(master.right, 31);

	cfg_get_string(cfg, "Audio", "driver", cfg_audio_driver, 255, NULL);
	if (!cfg_get_string(cfg, "Audio", "device", cfg_audio_device, 255, NULL)) {
		char *driver, *device;
		audio_parse_driver_spec(cfg_audio_driver, &driver, &device);
		if (device) {
			strncpy(cfg_audio_driver, driver, 255);
			strncpy(cfg_audio_device, device, 255);
			free(device);
		}
		free(driver);
	}

	CFG_GET_M(channel_limit, DEF_CHANNEL_LIMIT);
	CFG_GET_M(interpolation_mode, SRCMODE_LINEAR);
	CFG_GET_M(no_ramping, 0);
	CFG_GET_M(surround_effect, 1);

	if (audio_settings.channels != 1 && audio_settings.channels != 2)
		audio_settings.channels = 2;
	if (audio_settings.bits != 8 && audio_settings.bits != 16)
		audio_settings.bits = 16;
	audio_settings.channel_limit = CLAMP(audio_settings.channel_limit, 4, MAX_VOICES);
	audio_settings.interpolation_mode = CLAMP(audio_settings.interpolation_mode, 0, 3);

	audio_settings.eq_freq[0] = cfg_get_number(cfg, "EQ Low Band", "freq", 0);
	audio_settings.eq_freq[1] = cfg_get_number(cfg, "EQ Med Low Band", "freq", 16);
	audio_settings.eq_freq[2] = cfg_get_number(cfg, "EQ Med High Band", "freq", 96);
	audio_settings.eq_freq[3] = cfg_get_number(cfg, "EQ High Band", "freq", 127);

	audio_settings.eq_gain[0] = cfg_get_number(cfg, "EQ Low Band", "gain", 0);
	audio_settings.eq_gain[1] = cfg_get_number(cfg, "EQ Med Low Band", "gain", 0);
	audio_settings.eq_gain[2] = cfg_get_number(cfg, "EQ Med High Band", "gain", 0);
	audio_settings.eq_gain[3] = cfg_get_number(cfg, "EQ High Band", "gain", 0);

	if (cfg_get_number(cfg, "General", "stop_on_load", 1)) {
		status.flags &= ~PLAY_AFTER_LOAD;
	} else {
		status.flags |= PLAY_AFTER_LOAD;
	}
}

#define CFG_SET_A(v) cfg_set_number(cfg, "Audio", #v, audio_settings.v)
#define CFG_SET_M(v) cfg_set_number(cfg, "Mixer Settings", #v, audio_settings.v)
void cfg_atexit_save_audio(cfg_file_t *cfg)
{
	CFG_SET_A(sample_rate);
	CFG_SET_A(bits);
	CFG_SET_A(channels);
	CFG_SET_A(buffer_size);
	CFG_SET_A(master.left);
	CFG_SET_A(master.right);

	CFG_SET_M(channel_limit);
	CFG_SET_M(interpolation_mode);
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

	cfg_set_number(cfg, "General", "stop_on_load", !(status.flags & PLAY_AFTER_LOAD));
}

// ------------------------------------------------------------------------------------------------------------
static void _schism_midi_out_note(int chan, const song_note_t *starting_note)
{
	const song_note_t *m = starting_note;
	unsigned int tc;
	int m_note;

	unsigned char buf[4];
	int ins, mc, mg, mbl, mbh;
	int need_note, need_velocity;
	song_voice_t *c;

	if (!current_song || !song_is_instrument_mode() || (status.flags & MIDI_LIKE_TRACKER)) return;

    /*if(m)
    fprintf(stderr, "midi_out_note called (ch %d)note(%d)instr(%d)volcmd(%02X)cmd(%02X)vol(%02X)p(%02X)\n",
	chan, m->note, m->instrument, m->voleffect, m->effect, m->volparam, m->param);
    else fprintf(stderr, "midi_out_note called (ch %d) m=%p\n", m);*/

	if (!midi_playing) {
		csf_process_midi_macro(current_song, 0, current_song->midi_config.start, 0, 0, 0, 0); // START!
		midi_playing = 1;
	}

	if (chan < 0) {
		return;
	}

	c = &current_song->voices[chan];

	chan %= 64;

	if (!m) {
		if (last_row_number != (signed) current_song->row) return;
		m = last_row[chan];
		if (!m) return;
	} else {
		last_row[chan] = m;
		last_row_number = current_song->row;
	}

	ins = ins_tracker[chan];
	if (m->instrument > 0) {
		ins = m->instrument;
	}
	if (ins < 0 || ins >= MAX_INSTRUMENTS)
		return; /* err...  almost certainly */
	if (!current_song->instruments[ins]) return;

	if (current_song->instruments[ins]->midi_channel_mask >= 0x10000) {
		mc = chan % 16;
	} else {
		mc = 0;
		if(current_song->instruments[ins]->midi_channel_mask > 0)
			while(!(current_song->instruments[ins]->midi_channel_mask & (1 << mc)))
				++mc;
	}

	m_note = m->note;
	tc = current_song->tick_count % current_song->current_speed;
#if 0
printf("channel = %d note=%d starting_note=%p\n",chan,m_note,starting_note);
#endif
	if (m->effect == FX_SPECIAL) {
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
		if (tc != 0 && !starting_note) return;
	}

	need_note = need_velocity = -1;
	if (m_note > 120) {
		if (note_tracker[chan] != 0) {
			csf_process_midi_macro(current_song, chan, current_song->midi_config.note_off,
				0, note_tracker[chan], 0, ins_tracker[chan]);
		}

		note_tracker[chan] = 0;
		if (m->voleffect != VOLFX_VOLUME) {
			vol_tracker[chan] = 64;
		} else {
			vol_tracker[chan] = m->voleffect;
		}
	} else if (!m->note && m->voleffect == VOLFX_VOLUME) {
		vol_tracker[chan] = m->volparam;
		need_velocity = vol_tracker[chan];

	} else if (m->note) {
		if (note_tracker[chan] != 0) {
			csf_process_midi_macro(current_song, chan, current_song->midi_config.note_off,
				0, note_tracker[chan], 0, ins_tracker[chan]);
		}
		note_tracker[chan] = m_note;
		if (m->voleffect != VOLFX_VOLUME) {
			vol_tracker[chan] = 64;
		} else {
			vol_tracker[chan] = m->volparam;
		}
		need_note = note_tracker[chan];
		need_velocity = vol_tracker[chan];
	}

	if (m->instrument > 0) {
		ins_tracker[chan] = ins;
	}

	mg = (current_song->instruments[ins]->midi_program)
		+ ((midi_flags & MIDI_BASE_PROGRAM1) ? 1 : 0);
	mbl = current_song->instruments[ins]->midi_bank;
	mbh = (current_song->instruments[ins]->midi_bank >> 7) & 127;

	if (mbh > -1 && was_bankhi[mc] != mbh) {
		buf[0] = 0xB0 | (mc & 15); // controller
		buf[1] = 0x00; // corse bank/select
		buf[2] = mbh; // corse bank/select
		csf_midi_send(current_song, buf, 3, 0, 0);
		was_bankhi[mc] = mbh;
	}
	if (mbl > -1 && was_banklo[mc] != mbl) {
		buf[0] = 0xB0 | (mc & 15); // controller
		buf[1] = 0x20; // fine bank/select
		buf[2] = mbl; // fine bank/select
		csf_midi_send(current_song, buf, 3, 0, 0);
		was_banklo[mc] = mbl;
	}
	if (mg > -1 && was_program[mc] != mg) {
		was_program[mc] = mg;
		csf_process_midi_macro(current_song, chan, current_song->midi_config.set_program,
			mg, 0, 0, ins); // program change
	}
	if (c->flags & CHN_MUTE) {
		// don't send noteon events when muted
	} else if (need_note > 0) {
		if (need_velocity == -1) need_velocity = 64; // eh?
		need_velocity = CLAMP(need_velocity*2,0,127);
		csf_process_midi_macro(current_song, chan, current_song->midi_config.note_on,
			0, need_note, need_velocity, ins); // noteon
	} else if (need_velocity > -1 && note_tracker[chan] > 0) {
		need_velocity = CLAMP(need_velocity*2,0,127);
		csf_process_midi_macro(current_song, chan, current_song->midi_config.set_volume,
			need_velocity, note_tracker[chan], need_velocity, ins); // volume-set
	}

}
static void _schism_midi_out_raw(const unsigned char *data, unsigned int len, unsigned int pos)
{
#ifdef SCHISM_MIDI_DEBUG
	/* prints all of the raw midi messages into the terminal; useful for debugging output */
	int i = (8000*(audio_buffer_samples)) / (current_song->mix_frequency);

	for (int i=0; i < len; i++) {
		printf("%02x ",data[i]);
	}
	puts(""); /* newline */
#endif

	if (!_disko_writemidi(data,len,pos))
		midi_send_buffer(data,len,pos);
}



// ------------------------------------------------------------------------------------------------------------

void song_lock_audio(void)
{
	SDL_LockAudioDevice(current_audio_device);
}
void song_unlock_audio(void)
{
	SDL_UnlockAudioDevice(current_audio_device);
}
void song_start_audio(void)
{
	SDL_PauseAudioDevice(current_audio_device, 0);
}
void song_stop_audio(void)
{
	SDL_PauseAudioDevice(current_audio_device, 1);
}


static void song_print_info_top(const char *d)
{
	log_append(2, 0, "Audio initialised");
	log_underline(17);
	log_appendf(5, " Using driver '%s'", d);
}


/* --------------------------------------------------------------------------------------------------------- */
/* This is completely horrible! :) */

static int audio_was_init = 0;

const char *song_audio_driver(void)
{
	return driver_name ? driver_name : "unknown";
}

const char *song_audio_device(void)
{
	return device_name ? device_name : "unknown";
}

static void _cleanup_audio_device(void)
{
	if (current_audio_device) {
		SDL_CloseAudioDevice(current_audio_device);
		current_audio_device = 0;
		free(device_name);
		device_name = NULL;
	}
}

static int _audio_open_driver(const char *driver)
{
	const char *n;

	if (audio_was_init) {
		_cleanup_audio_device();
		free(driver_name);
		driver_name = NULL;
		SDL_AudioQuit();
		audio_was_init = 0;
	}

	const int cnt = SDL_GetNumAudioDrivers();

	if (driver && *driver) {
		/* compatibility! */
		n = !strcmp(driver, "oss") ? "dsp"
			: (!strcmp(driver, "nosound") || !strcmp(driver, "none")) ? "dummy"
			: driver;

		if (!SDL_AudioInit(n))
			goto audio_was_init;
	}

	/* ... */
	n = SDL_getenv("SDL_AUDIODRIVER");
	if (n && *n) {
		if (!SDL_AudioInit(n))
			goto audio_was_init;
	}

	for (int i = 0; i < cnt; i++) {
		n = SDL_GetAudioDriver(i);

		if (!SDL_AudioInit(n))
			goto audio_was_init;
	}

	/* really? give up */
	driver_name = NULL;
	return 0;

audio_was_init:
	driver_name = str_dup(n);
	audio_was_init = 1;
	return 1;
}

static int _audio_open_device(const char *device, int verbose)
{
	_cleanup_audio_device();

	/* if the buffer size isn't a power of two, the dsp driver will punt since it's not nice enough to fix
	 * it for us. (contrast alsa, which is TOO nice and fixes it even when we don't want it to) */
	int size_pow2 = 2;
	while (size_pow2 < audio_settings.buffer_size)
		size_pow2 <<= 1;

	/* round to the nearest (kept for compatibility) */
	if (size_pow2 != audio_settings.buffer_size
	    && (size_pow2 - audio_settings.buffer_size) > (audio_settings.buffer_size - (size_pow2 >> 1)))
		size_pow2 >>= 1;

	/* This is needed in order to coax alsa into actually respecting the buffer size, since it's evidently
	 * ignored entirely for "fake" devices such as "default" -- which SDL happens to use if no device name
	 * is set. (see SDL_alsa_audio.c: http://tinyurl.com/ybf398f)
	 * If hw doesn't exist, so be it -- let this fail, we'll fall back to the dummy device, and the
	 * user can pick a more reasonable device later. */

	/* I can't replicate this issue at all, so I'm just gonna comment this out. If it really *is* still an
	 * issue, it can be uncommented.
	 *  - paper */

//	if (!strcmp(driver_name, "alsa")) {
//		char *dev = SDL_getenv("AUDIODEV");
//		if (!dev || !*dev)
//			put_env_var("AUDIODEV", "hw");
//	}

	SDL_AudioSpec desired = {
		.freq = audio_settings.sample_rate,
		.format = (audio_settings.bits == 8) ? AUDIO_U8 : AUDIO_S16SYS,
		.channels = audio_settings.channels,
		.samples = size_pow2,
		.callback = audio_callback,
		.userdata = NULL,
	};
	SDL_AudioSpec obtained;

#define SCHISM_CHANGE_ALLOWED (SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE)

	if (device && *device) {
		current_audio_device = SDL_OpenAudioDevice(device, 0, &desired, &obtained, SCHISM_CHANGE_ALLOWED);
		if (current_audio_device) {
			device_name = str_dup(device);
			goto success;
		} else fputs("Failed to open requested audio device! Falling back to default...\n", stderr);
	}

	current_audio_device = SDL_OpenAudioDevice(NULL, 0, &desired, &obtained, SCHISM_CHANGE_ALLOWED);
	if (current_audio_device) {
		device_name = str_dup("default"); // ????
		goto success;
	}

	/* oops ! */
	return 0;

success:
	song_lock_audio();

	csf_set_wave_config(current_song, obtained.freq,
		SDL_AUDIO_BITSIZE(obtained.format),
		obtained.channels);
	audio_output_channels = obtained.channels;
	audio_output_bits = SDL_AUDIO_BITSIZE(obtained.format);
	audio_sample_size = audio_output_channels * (audio_output_bits / 8);
	audio_buffer_samples = obtained.samples;

	if (verbose) {
		song_print_info_top(driver_name);

		log_appendf(5, " %d Hz, %d bit, %s", obtained.freq, SDL_AUDIO_BITSIZE(obtained.format),
			obtained.channels == 1 ? "mono" : "stereo");
		log_appendf(5, " Buffer size: %d samples", obtained.samples);
	}

	return 1;
}

// Configure a device. (called at startup)
static void _audio_init_head(const char *driver, const char *device, int verbose)
{
	/* Use the driver from the config if it exists. */
	if (!driver || !*driver)
		driver = cfg_audio_driver;

	if (!device || !*device)
		device = cfg_audio_device;

	if (!_audio_open_driver(driver)) {
		fputs("Failed to open audio driver!\n", stderr);
		goto fail;
	}

	if (!_audio_open_device(device, verbose)) {
		fputs("Failed to open audio device!\n", stderr);
		goto fail;
	}

	return;

fail:
	/* use the dummy device as a fallback */
	if (!_audio_open_driver("dummy") || !_audio_open_device(NULL, verbose))
		goto catastrophy;

	return;

catastrophy:
	/* whoops! */
	fputs("Couldn't initialize audio!\n", stderr);
	const char* err = SDL_GetError();
	if (err) fprintf(stderr, "%s\n", err);
	schism_exit(1);
}

// Set up audio_buffer, reset the sample count, and kick off the mixer
// (note: _audio_open will leave the device LOCKED)
static void _audio_init_tail(void)
{
	free(audio_buffer);
	audio_buffer = mem_calloc(audio_buffer_samples, audio_sample_size);
	samples_played = (status.flags & CLASSIC_MODE) ? SMP_INIT : 0;

	song_unlock_audio();
	song_start_audio();
}

/* driver == NULL || device == NULL is fine here */
void audio_init(const char *driver, const char *device)
{
	_audio_init_head(driver, device, 1);
	_audio_init_tail();
}

void audio_reinit(const char *device)
{
	if (status.flags & (DISKWRITER_ACTIVE|DISKWRITER_ACTIVE_PATTERN)) {
		/* never allowed */
		return;
	}
	song_stop();
	_audio_init_head(driver_name, device, 0);
	_audio_init_tail();

	if (status.flags & CLASSIC_MODE)
		// FIXME: but we spontaneously report a GUS card sometimes...
		status_text_flash("Sound Blaster 16 reinitialised");
	else
		status_text_flash("Audio output reinitialised");
}

/* --------------------------------------------------------------------------------------------------------- */

void song_init_eq(int do_reset, uint32_t mix_freq)
{
	uint32_t pg[4];
	uint32_t pf[4];
	int i;

	for (i = 0; i < 4; i++) {
		pg[i] = audio_settings.eq_gain[i];
		pf[i] = 120 + (((i*128) * audio_settings.eq_freq[i])
			* (mix_freq / 128) / 1024);
	}

	set_eq_gains(pg, 4, pf, do_reset, mix_freq);
}


void song_init_modplug(void)
{
	song_lock_audio();

	max_voices = audio_settings.channel_limit;
	csf_set_resampling_mode(current_song, audio_settings.interpolation_mode);
	if (audio_settings.no_ramping)
		current_song->mix_flags |= SNDMIX_NORAMPING;
	else
		current_song->mix_flags &= ~SNDMIX_NORAMPING;

	// disable the S91 effect? (this doesn't make anything faster, it
	// just sounds better with one woofer.)
	song_set_surround(audio_settings.surround_effect);

	// update midi queue configuration
	midi_queue_alloc(audio_buffer_samples, audio_sample_size, current_song->mix_frequency);

	// timelimit the playback_update() calls when midi isn't actively going on
	audio_buffers_per_second = (current_song->mix_frequency / (audio_buffer_samples * 8 * audio_sample_size));
	if (audio_buffers_per_second > 1) audio_buffers_per_second--;

	song_unlock_audio();
}

void song_initialise(void)
{
	csf_midi_out_note = _schism_midi_out_note;
	csf_midi_out_raw = _schism_midi_out_raw;


	current_song = csf_allocate();

	//song_stop(); <- song_new does this
	song_set_linear_pitch_slides(1);
	song_new(0);

	// hmm.
	current_song->mix_flags |= SNDMIX_MUTECHNMODE;
}

