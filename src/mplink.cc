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
// variables

CSoundFile *mp = NULL;
void (*song_changed_cb) (void) = NULL;

// ------------------------------------------------------------------------
// song information

char *song_get_title()
{
        return mp->m_szNames[0];
}

char *song_get_message()
{
        return mp->m_lpszSongComments;
}

// returned value is in seconds
unsigned long song_get_length()
{
        return mp->GetSongTime();
}

// ------------------------------------------------------------------------

song_sample *song_get_sample(int n, char **name_ptr)
{
        if (n >= MAX_SAMPLES)
                return NULL;
        if (name_ptr)
                *name_ptr = mp->m_szNames[n];
        return (song_sample *) mp->Ins + n;
}

song_instrument *song_get_instrument(int n, char **name_ptr)
{
        if (n >= MAX_INSTRUMENTS)
                return NULL;
        
        // Make a new instrument if it doesn't exist.
        // TODO | what about saving? sample mode? (how modplug stores
        // TODO | and handles instrument data is really unclear to me.)
        if (!mp->Headers[n]) {
                mp->Headers[n] = (INSTRUMENTHEADER *)
                        calloc(1, sizeof(INSTRUMENTHEADER));
        }

        if (name_ptr)
                *name_ptr = (char *) mp->Headers[n]->name;
        return (song_instrument *) mp->Headers[n];
}

song_channel *song_get_channel(int n)
{
        if (n >= MAX_BASECHANNELS)
                return NULL;
        return (song_channel *) mp->ChnSettings + n;
}

song_mix_channel *song_get_mix_channel(int n)
{
        if (n >= MAX_CHANNELS)
                return NULL;
        return (song_mix_channel *) mp->Chn + n;
}

int song_get_mix_state(unsigned long **channel_list)
{
        if (channel_list)
                *channel_list = mp->ChnMix;
        return mp->m_nMixChannels;
}

// ------------------------------------------------------------------------
// For all of these, channel is ZERO BASED.
// (whereas in the pattern editor etc. it's one based)

static int solo_channel = -1;
static int channel_states[64];  /* nonzero => muted */

void song_set_channel_mute(int channel, int muted)
{
        if (muted) {
                mp->ChnSettings[channel].dwFlags |= CHN_MUTE;
                mp->Chn[channel].dwFlags |= CHN_MUTE;
        } else {
                mp->ChnSettings[channel].dwFlags &= ~CHN_MUTE;
                mp->Chn[channel].dwFlags &= ~CHN_MUTE;
        }
}

void song_toggle_channel_mute(int channel)
{
        // i'm just going by the playing channel's state...
        // if the actual channel is muted but not the playing one,
        // tough luck :)
        song_set_channel_mute(channel,
                              (mp->Chn[channel].dwFlags & CHN_MUTE) == 0);
}

void song_handle_channel_solo(int channel)
{
        int n = 64;

        if (solo_channel >= 0) {
                if (channel == solo_channel) {
                        // undo the solo
                        while (n) {
                                n--;
                                song_set_channel_mute(n,
                                                      channel_states[n]);
                        }
                        solo_channel = -1;
                } else {
                        // change the solo channel
                        // mute all channels...
                        while (n) {
                                n--;
                                song_set_channel_mute(n, 1);
                        }
                        // then unmute the current channel
                        song_set_channel_mute(channel, 0);
                        solo_channel = channel;
                }
        } else {
                // set the solo channel:
                // save each channel's state, then mute it...
                while (n) {
                        n--;
                        channel_states[n] =
                                (song_get_channel(n)->flags & CHN_MUTE);
                        song_set_channel_mute(n, 1);
                }
                // ... and then, unmute the current channel
                song_set_channel_mute(channel, 0);
                solo_channel = channel;
        }
}

void song_clear_solo_channel()
{
        solo_channel = -1;
}

// returned channel number is ONE-based
// (to make it easier to work with in the pattern editor and info page)
int song_find_last_channel()
{
        int n = 64;

        if (solo_channel > 0) {
                while (channel_states[--n])
                        if (n == 0)
                                return 64;
        } else {
                while (song_get_channel(--n)->flags & CHN_MUTE)
                        if (n == 0)
                                return 64;
        }
        return n + 1;
}

// ------------------------------------------------------------------------

// returns length of the pattern, or 0 on error. (this can be used to
// get a pattern's length by passing NULL for buf.)
int song_get_pattern(int n, song_note ** buf)
{
        if (n >= MAX_PATTERNS)
                return 0;

        if (buf) {
                if (!mp->Patterns[n]) {
                        mp->PatternSize[n] = 64;
                        mp->Patterns[n] = (MODCOMMAND *)
                                calloc(64 * mp->PatternSize[n],
                                       sizeof(MODCOMMAND));
                }
                *buf = (song_note *) mp->Patterns[n];
        } else {
                if (!mp->Patterns[n])
                        return 64;
        }
        return mp->PatternSize[n];
}

unsigned char *song_get_orderlist()
{
        return mp->Order;
}

// ------------------------------------------------------------------------

int song_get_num_orders()
{
        // for some reason, modplug calls it patterns, not orders... *shrug*
        int n = mp->GetNumPatterns();
        return n ? n - 1 : n;
}

static inline bool pattern_is_empty(int n)
{
        if (!mp->Patterns[n])
                return true;
        if (mp->PatternSize[n] != 64)
                return false;
        unsigned char blank_pattern[4096] = { 0 };
        return !memcmp(mp->Patterns[n], blank_pattern, 4096);
}

int song_get_num_patterns()
{
        int n;
        for (n = 199; n && pattern_is_empty(n); n--)
                /* do nothing */ ;
        return n;
}

int song_get_rows_in_pattern(int pattern)
{
        if (pattern > MAX_PATTERNS)
                return 0;
        return (mp->PatternSize[pattern] ? : 64) - 1;
}

// ------------------------------------------------------------------------

int song_get_initial_speed()
{
        return mp->m_nDefaultSpeed;
}

void song_set_initial_speed(int new_speed)
{
        mp->m_nDefaultSpeed = CLAMP(new_speed, 1, 255);
}

int song_get_initial_tempo()
{
        return mp->m_nDefaultTempo;
}

void song_set_initial_tempo(int new_tempo)
{
        mp->m_nDefaultTempo = CLAMP(new_tempo, 31, 255);
}

int song_get_initial_global_volume()
{
        return mp->m_nDefaultGlobalVolume / 2;
}

void song_set_initial_global_volume(int new_vol)
{
        mp->m_nDefaultGlobalVolume = CLAMP(new_vol, 0, 128) * 2;
}

int song_get_mixing_volume()
{
        return mp->m_nSongPreAmp;
}

void song_set_mixing_volume(int new_vol)
{
        mp->m_nSongPreAmp = CLAMP(new_vol, 0, 128);
}

int song_get_separation()
{
        // FIXME | d'oh, this is static! also, modplug always writes 128
        // FIXME | for the separation... garh
        return CSoundFile::m_nStereoSeparation;
}

int song_is_stereo()
{
        return CSoundFile::IsStereo();  // FIXME: static == wrong
}

int song_has_old_effects()
{
        return !!(mp->m_dwSongFlags & SONG_ITOLDEFFECTS);
}

void song_set_old_effects(int value)
{
        if (value)
                mp->m_dwSongFlags |= SONG_ITOLDEFFECTS;
        else
                mp->m_dwSongFlags &= ~SONG_ITOLDEFFECTS;
}

int song_has_compatible_gxx()
{
        return !!(mp->m_dwSongFlags & SONG_ITCOMPATMODE);
}

void song_set_compatible_gxx(int value)
{
        if (value)
                mp->m_dwSongFlags |= SONG_ITCOMPATMODE;
        else
                mp->m_dwSongFlags &= ~SONG_ITCOMPATMODE;
}

int song_has_linear_pitch_slides()
{
        return !!(mp->m_dwSongFlags & SONG_LINEARSLIDES);
}

void song_set_linear_pitch_slides(int value)
{
        if (value)
                mp->m_dwSongFlags |= SONG_LINEARSLIDES;
        else
                mp->m_dwSongFlags &= ~SONG_LINEARSLIDES;
}

int song_is_instrument_mode()
{
        return !!mp->m_nInstruments;
}

char *song_get_instrument_name(int n, char **name)
{
        if (song_is_instrument_mode())
                song_get_instrument(n, name);
        else
                song_get_sample(n, name);
        return *name;
}

int song_get_current_instrument()
{
        return (song_is_instrument_mode()
                ? instrument_get_current()
                : sample_get_current()
                );
}
