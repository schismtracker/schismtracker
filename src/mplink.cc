// Schism Tracker - a cross-platform Impulse Tracker clone
// copyright (c) 2003-2005 chisel <someguy@here.is> <http://here.is/someguy/>
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "mplink.h"
#include "slurp.h"

#include <string>

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cmath>

// ------------------------------------------------------------------------
// variables

CSoundFile *mp = NULL;

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
// Memory allocation wrappers. Stupid 'new' operator.

signed char *song_sample_allocate(int bytes)
{
	return CSoundFile::AllocateSample(bytes);
}

void song_sample_free(signed char *data)
{
	CSoundFile::FreeSample(data);
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
                mp->Headers[n] = new INSTRUMENTHEADER;
		memset(mp->Headers[n], 0, sizeof(INSTRUMENTHEADER));
        }
	
        if (name_ptr)
                *name_ptr = (char *) mp->Headers[n]->name;
        return (song_instrument *) mp->Headers[n];
}

// this is a fairly gross way to do what should be such a simple thing
int song_get_instrument_number(song_instrument *inst)
{
	if (inst)
		for (int n = 1; n < MAX_INSTRUMENTS; n++)
			if (inst == ((song_instrument *) mp->Headers[n]))
				return n;
	return 0;
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
        return MIN(mp->m_nMixChannels, mp->m_nMaxMixChannels);
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
                                song_set_channel_mute(n, channel_states[n]);
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
			mp->Patterns[n] = CSoundFile::AllocatePattern
				(mp->PatternSize[n], 64);
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

static song_note blank_pattern[64 * 64];
int song_pattern_is_empty(int n)
{
        if (!mp->Patterns[n])
                return true;
        if (mp->PatternSize[n] != 64)
                return false;
        return !memcmp(mp->Patterns[n], blank_pattern, sizeof(blank_pattern));
}

int song_get_num_patterns()
{
        int n;
        for (n = 199; n && song_pattern_is_empty(n); n--)
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

/*
mp->PatternSize
	The size of the pattern, of course.
mp->PatternAllocSize
	Not used anywhere (yet). I'm planning on keeping track of space off the end of a pattern when it's
	shrunk, so that making it longer again will restore it. (i.e., handle resizing the same way IT does)
	I'll add this stuff in later; I have three handwritten pages detailing how to implement it. ;)
*/
void song_pattern_resize(int n, int rows)
{
	//printf("pattern resize requested: %3d, to %d rows\n", n, rows);
	//printf("loaded in pattern editor: %3d\n", get_current_pattern());
	//printf("current playback pattern: %3d\n", song_get_playing_pattern());

	SDL_LockAudio();
	MODCOMMAND *oldpat = mp->Patterns[n];
	MODCOMMAND *newpat = CSoundFile::AllocatePattern(rows, 64); // this occasionally segfaults. wtfbbq?!!
	if (oldpat) {
		memcpy(newpat, oldpat, 64 * mp->PatternSize[n]);
		CSoundFile::FreePattern(oldpat);
	}
	mp->Patterns[n] = newpat;
	mp->PatternSize[n] = rows;
	SDL_UnlockAudio();
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
        return mp->m_nStereoSeparation;
}

void song_set_separation(int new_sep)
{
	mp->m_nStereoSeparation = CLAMP(new_sep, 0, 128);
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
