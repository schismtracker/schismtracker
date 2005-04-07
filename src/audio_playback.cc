// Schism Tracker - a cross-platform Impulse Tracker clone
// copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
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

#include <string>

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cmath>

// ------------------------------------------------------------------------

unsigned long samples_played = 0;
unsigned long max_channels_used = 0;

/* this points to the currently mixed block of audio data (for visuals, i.e. the oscilloscope view)
 *
 * FIXME: get rid of this audio_buffer_ hack -- closing schism while something is playing will cause a
 * segfault in free(); after pointing the buffer to statically allocated memory, it worked. */
static signed short audio_buffer_[16726];
signed short *audio_buffer;
int audio_buffer_size = 0;

struct audio_settings audio_settings;

// ------------------------------------------------------------------------
// playback

// this gets called from modplug
static void mp_mix_hook(int *buffer, unsigned long samples, unsigned long channels)
{
	// Four times the volume may seem like a lot, but Impulse Tracker is still about 150% as loud. This
	// is just a rough calculation based on the output of a test mod written to disk with ITWAV.DRV
	// versus the same mod played with Schism here with the output captured by SDL's wave writer driver,
	// and should be taken with a grain of salt; however, IT certainly *seems* louder in general, at
	// least from Dosemu using the SB driver.
	for (unsigned long n = 0; n < samples * channels; n++) {
		buffer[n] <<= 2;	// n << 2 == 4 * n
	}
}

// this gets called from sdl
static void audio_callback(void *, Uint8 * stream, int len)
{
        if (mp->m_dwSongFlags & SONG_ENDREACHED)
                return;

        int n = mp->Read(stream, len);
#ifndef NDEBUG
        if (!n) {
		song_stop();
		log_appendf(4, "mp->Read returned zero. why?");
		set_page(PAGE_LOG);
		return;
        }
#endif
        samples_played += n;

	if (n < len) {
		memmove(audio_buffer, audio_buffer + len - n,
			(len - n) * sizeof(signed short));
	}
	memcpy(audio_buffer, stream, n * sizeof(signed short));

        if (mp->m_nMixChannels > max_channels_used)
		max_channels_used = MIN(mp->m_nMixChannels, mp->m_nMaxMixChannels);
}

// ------------------------------------------------------------------------------------------------------------
// note playing

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

// useins: play the current instrument if nonzero, else play the current sample with a "blank" instrument,
// i.e. no envelopes, vol/pan swing, nna setting, or note translations. (irrelevant if not instrument mode?)
void song_play_note(int ins, int note, int chan, int useins)
{
	if (!chan) {
		chan = current_play_channel;
		if (multichannel_mode)
			song_change_current_play_channel(1, 1);
	}
	chan--;
	
	SDL_LockAudio(); // I should probably be doing this in a lot more places...
	
	// Stolen from Modplug ;)
	
	MODCHANNEL *c = mp->Chn + chan;
	
	c->nPos = c->nPosLo = c->nLength = 0;
	c->dwFlags &= 0xff;
	c->dwFlags &= ~(CHN_MUTE);
	c->nGlobalVol = 64;
	c->nInsVol = 64;
	c->nPan = 128;
	c->nNewNote = note;
	c->nRightVol = c->nLeftVol = 0;
	c->nROfs = c->nLOfs = 0;
	c->nCutOff = 0x7f;
	c->nResonance = 0;
	if (useins /* && song_is_instrument_mode() */ && mp->Headers[ins]) {
		c->nVolEnvPosition = 0;
		c->nPanEnvPosition = 0;
		c->nPitchEnvPosition = 0;
		mp->InstrumentChange(c, ins);
	} else {
		MODINSTRUMENT *i = mp->Ins + ins;
		c->pCurrentSample = i->pSample;
		c->pHeader = NULL;
		c->pInstrument = i;
		c->pSample = i->pSample;
		c->nFineTune = i->nFineTune;
		c->nC4Speed = i->nC4Speed;
		c->nLoopStart = i->nLoopStart;
		c->nLoopEnd = i->nLoopEnd;
		c->dwFlags = i->uFlags & (0xFF & ~CHN_MUTE); // redundant?
		c->nPan = 128; // redundant?
		//if (i->uFlags & CHN_PANNING) c->nPan = i->nPan; annoying
		c->nInsVol = i->nGlobalVol;
		c->nFadeOutVol = 0x10000;
		
		// Must make sure this stays the same as the code I stuck in Modplug. Wish there was some
		// standard trigger_sample function that did all this.
		i->played = 1;
	}
	c->nVolume = 256;
	mp->NoteChange(chan, note, false, true, true);
	c->nMasterChn = 0;
	
	if (mp->m_dwSongFlags & SONG_ENDREACHED) {
		mp->m_dwSongFlags &= ~SONG_ENDREACHED;
		mp->m_dwSongFlags |= SONG_PAUSED;
	}

	SDL_UnlockAudio();
}

// ------------------------------------------------------------------------------------------------------------

// this should be called with the audio LOCKED
static void song_reset_play_state()
{
        // this is lousy and wrong, but it sort of works
        mp->SetCurrentOrder(0);

        mp->m_dwSongFlags &= ~(SONG_PAUSED | SONG_STEP);
	mp->ResetTimestamps();
        samples_played = 0;
}

void song_start()
{
        SDL_LockAudio();

        song_reset_play_state();
        max_channels_used = 0;

        SDL_UnlockAudio();
}

void song_stop()
{
        SDL_LockAudio();

        song_reset_play_state();
        // modplug doesn't actually have a "stop" mode, but if this is
        // set, mp->Read just returns.
        mp->m_dwSongFlags |= SONG_ENDREACHED;

        mp->m_dwSongFlags &= ~(SONG_PAUSED | SONG_STEP);

        SDL_UnlockAudio();
}

void song_loop_pattern(int pattern, int row)
{
        SDL_LockAudio();

        song_reset_play_state();
        max_channels_used = 0;
        mp->LoopPattern(pattern, row);

        SDL_UnlockAudio();
}

void song_start_at_order(int order, int row)
{
        SDL_LockAudio();

        song_reset_play_state();
        // I would imagine this is *not* the right method here, but it
        // seems to work.
        mp->SetCurrentOrder(order);
        mp->m_nRow = mp->m_nNextRow = row;
        max_channels_used = 0;

        SDL_UnlockAudio();
}

void song_start_at_pattern(int pattern, int row)
{
        if (pattern < 0 || pattern > 199)
                return;

        int n = get_current_order();

        if (mp->Order[n] == pattern) {
                song_start_at_order(n, row);
                return;
        }
	else {
                for (n = 0; n < 255; n++) {
                        if (mp->Order[n] == pattern) {
                                song_start_at_order(n, row);
                                return;
                        }
                }
        }

        song_loop_pattern(pattern, row);
}

// Actually this is wrong; single step shouldn't stop playing. Instead, it should *add* the notes in the row
// to the mixed data. Additionally, it should process tick-N effects -- e.g. if there's an Exx, a single-step
// on the row should slide the note down.
void song_single_step(int pattern, int row)
{
        max_channels_used = 0;

        mp->m_dwSongFlags &= ~(SONG_ENDREACHED | SONG_PAUSED);
        mp->m_dwSongFlags |= SONG_STEP;
	mp->LoopPattern(pattern);
	mp->m_nNextRow = row;
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
unsigned long song_get_current_time()
{
        return samples_played / mp->gdwMixingFreq;
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
				samples[s] = 1;
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
}

// ------------------------------------------------------------------------------------------------------------

static inline void song_print_info(SDL_AudioSpec & obtained)
{
        char buf[256];

        log_appendf(2, "Audio initialised");
        log_appendf(2, "\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81"
                    "\x81\x81\x81\x81\x81\x81");
        log_appendf(5, " Using driver '%s'",
                    SDL_AudioDriverName(buf, 256));
        log_appendf(5, " %d Hz, %d bit, %s", obtained.freq,
                    obtained.format & 0xff,
                    obtained.channels == 1 ? "mono" : "stereo");
        log_appendf(5, " Buffer size: %d samples", obtained.samples);

        // barf out some more info on modplug's settings?

        log_append(0, 0, "");
        log_append(0, 0, "");
}

void song_init_audio(void)
{
	song_stop();
	SDL_PauseAudio(1);
	SDL_CloseAudio();
	
        // set up the sdl audio
        SDL_AudioSpec desired, obtained;
        desired.freq = audio_settings.sample_rate;
	desired.format = (audio_settings.bits == 8) ? AUDIO_U8 : AUDIO_S16SYS;
	desired.channels = audio_settings.channels;
        desired.samples = audio_settings.buffer_size;
        desired.callback = audio_callback;
        if (SDL_OpenAudio(&desired, &obtained) < 0) {
		// this shouldn't die -- suppose the audio settings are changed at runtime by shift-f5
		// to something the soundcard doesn't support.
                fprintf(stderr, "Couldn't initialise audio: %s\n",
                        SDL_GetError());
                exit(1);
        }
        song_print_info(obtained);
	
	audio_buffer_size = obtained.samples;
	//audio_buffer = (signed short *) calloc(audio_buffer_size, sizeof(signed short));
	audio_buffer = audio_buffer_;
	
        // obtained.format & 0xff just happens to be the bit rate ;)
        CSoundFile::SetWaveConfig(obtained.freq, obtained.format & 0xff, obtained.channels);

        samples_played = 0;
	
	SDL_PauseAudio(0);
}

void song_init_modplug(void)
{
	SDL_LockAudio();
	
        CSoundFile::m_nMaxMixChannels = audio_settings.channel_limit;
        CSoundFile::SetResamplingMode(audio_settings.interpolation_mode);
        CSoundFile::SetXBassParameters(audio_settings.xbass_amount, audio_settings.xbass_range);
        CSoundFile::SetSurroundParameters(audio_settings.surround_depth, audio_settings.surround_delay);
        CSoundFile::SetReverbParameters(audio_settings.reverb_depth, audio_settings.reverb_delay);
	// the last param is the equalizer, which apparently isn't functional
        CSoundFile::SetWaveConfigEx(audio_settings.surround, !(audio_settings.oversampling),
				    audio_settings.reverb, audio_settings.hq_resampling,
				    audio_settings.xbass, audio_settings.noise_reduction, false);
	
	// disable the S91 effect? (this doesn't make anything faster, it
	// just sounds better with one woofer.)
	song_set_surround(audio_settings.surround_effect);

	SDL_UnlockAudio();
}

void song_initialise(void)
{
	mp = new CSoundFile;
	mp->Create(NULL, 0);
	song_stop();
	song_set_linear_pitch_slides(1);
	song_new(0);
	
	// add my amplifier
	CSoundFile::gpSndMixHook = mp_mix_hook;
	
	// hmm.
	CSoundFile::gdwSoundSetup |= SNDMIX_MUTECHNMODE;
}
