#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string>

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cmath>

#include "mplink.h"
#include "slurp.h"

// ------------------------------------------------------------------------

unsigned long samples_played = 0;
unsigned long max_channels_used = 0;

// ------------------------------------------------------------------------
// playback

// this gets called from sdl
static void audio_callback(void *userdata, Uint8 * stream, int len)
{
        if (mp->m_dwSongFlags & SONG_ENDREACHED)
                return;

        unsigned int n = mp->Read(stream, len);
#ifndef NDEBUG
        if (!n) {
                printf("mp->Read returned zero. why?\n");
                samples_played = 0;
                return;
        }
#endif
        samples_played += n;

        if (mp->m_nMixChannels > max_channels_used)
                max_channels_used = mp->m_nMixChannels;

        // appease gcc. the unused attribute doesn't work with c++
        // or something...
        (void) &userdata;
}

// this should be called with the audio LOCKED
static void song_reset_play_state()
{
        // this is lousy and wrong, but it sort of works
        mp->SetCurrentOrder(0);

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
        } else {
                for (n = 0; n < 255; n++) {
                        if (mp->Order[n] == pattern) {
                                song_start_at_order(n, row);
                                return;
                        }
                }
        }

        song_loop_pattern(pattern, row);
}

// ------------------------------------------------------------------------
// info on what's playing

enum song_mode song_get_mode()
{
        if (mp->m_dwSongFlags & SONG_ENDREACHED)
                return MODE_STOPPED;
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

int song_get_current_pattern()
{
        return mp->GetCurrentPattern();
}

int song_get_current_row()
{
        return mp->m_nRow;
}

int song_get_playing_channels()
{
        return mp->m_nMixChannels;
}

int song_get_max_channels()
{
        return max_channels_used;
}

unsigned long song_get_vu_meter(void)
{
        return mp->gnVUMeter;
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

void song_flip_stereo()
{
        mp->gdwSoundSetup ^= SNDMIX_REVERSESTEREO;
}

// ------------------------------------------------------------------------
// TODO: all the audio stuff should be configurable (shift-f5!)

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

void song_initialize(void (*song_changed) (void))
{
        song_changed_cb = song_changed;

        if (mp)
                return;

        // set up the sdl audio
        SDL_AudioSpec desired, obtained;
        desired.freq = 44100;
        desired.format = AUDIO_S16SYS;
        desired.channels = 2;
        desired.samples = 2048;
        desired.callback = audio_callback;
        if (SDL_OpenAudio(&desired, &obtained) < 0) {
                fprintf(stderr, "Couldn't initialize audio: %s\n",
                        SDL_GetError());
                exit(1);
        }
        song_print_info(obtained);

        // setup modplug

        mp = new CSoundFile;

        // obtained.format & 0xff just happens to be the bit rate ;)
        CSoundFile::SetWaveConfig(obtained.freq, obtained.format & 0xff,
                                  obtained.channels);
        CSoundFile::SetResamplingMode(SRCMODE_SPLINE);
        CSoundFile::SetReverbParameters(30, 100);
        CSoundFile::SetXBassParameters(35, 50);
        CSoundFile::SetSurroundParameters(20, 20);
        CSoundFile::m_nMaxMixChannels = MAX_CHANNELS;
        // surround_sound, no_oversampling, reverb, hq_resampling,
        // megabass, noise_reduction, equalizer
        CSoundFile::SetWaveConfigEx
                (false, false, false, true, false, true, false);

        // if this is set, mods are loaded with full L/R/R/L panning
        // instead of 16/48/48/16. (really, it should be L/R/R/L with the
        // pan separation set to 64, but modplug's handling of the pan
        // separation is dysfunctional.)
        //CSoundFile::gdwSoundSetup |= SNDMIX_MAXDEFAULTPAN;
        
        samples_played = 0;
        song_new();
}
