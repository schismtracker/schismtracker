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

#include "mplink.h"
#include "slurp.h"

#include <cstdio>
#include <cstring>
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

// song midi config
midi_config *song_get_midi_config(void) {
	return (midi_config *)&mp->m_MidiCfg;
}

// returned value is in seconds
unsigned int song_get_length()
{
        return mp->GetSongTime();
}
unsigned int song_get_length_to(int order, int row)
{
	unsigned int t;

	song_lock_audio();
	mp->stop_at_order = order;
	mp->stop_at_row = row;
	t = mp->GetSongTime();
	mp->stop_at_order = mp->stop_at_row = -1;
	song_unlock_audio();
	return t;
}
void song_get_at_time(unsigned int seconds, int *order, int *row)
{
	if (!seconds) {
		if (order) *order = 0;
		if (row) *row = 0;
	} else {
		song_lock_audio();
		mp->stop_at_order = MAX_ORDERS;
		mp->stop_at_row = 255; /* unpossible */
		mp->stop_at_time = seconds;
		(void)mp->GetSongTime();
		if (order) *order = mp->stop_at_order;
		if (row) *row = mp->stop_at_row;
		mp->stop_at_order = mp->stop_at_row = -1;
		mp->stop_at_time = 0;
		song_unlock_audio();
	}
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

static void _init_envelope(INSTRUMENTENVELOPE *e, int n)
{
	e->Ticks[0] = 0;
	e->Values[0] = n;
	e->Ticks[1] = 100;
	e->Values[1] = n;
	e->nNodes = 2;
}

song_instrument *song_get_instrument(int n, char **name_ptr)
{
        if (n >= MAX_INSTRUMENTS)
                return NULL;

        // Make a new instrument if it doesn't exist.
        if (!mp->Headers[n]) {
                mp->Headers[n] = new INSTRUMENTHEADER;
		memset(mp->Headers[n], 0, sizeof(INSTRUMENTHEADER));
		mp->Headers[n]->nGlobalVol = 64;
		mp->Headers[n]->nPan = 128;

		_init_envelope(&mp->Headers[n]->VolEnv, 64);
		_init_envelope(&mp->Headers[n]->PanEnv, 32);
		_init_envelope(&mp->Headers[n]->PitchEnv, 32);
        }
	
        if (name_ptr)
                *name_ptr = (char *) mp->Headers[n]->name;
        return (song_instrument *) mp->Headers[n];
}

int song_get_instrument_default_volume(UNUSED int ins, UNUSED int sam)
{
	if (song_is_instrument_mode()) return 64;
	if (sam == -1) return 64;
// todo (but not really all that soon)
	return 64;
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

int song_get_mix_state(unsigned int **channel_list)
{
        if (channel_list)
                *channel_list = mp->ChnMix;
        return MIN(mp->m_nMixChannels, mp->m_nMaxMixChannels);
}

// ------------------------------------------------------------------------
// For all of these, channel is ZERO BASED.
// (whereas in the pattern editor etc. it's one based)

static int channel_states[64];  // saved ("real") mute settings; nonzero = muted

static inline void _save_state(int channel)
{
	channel_states[channel] = mp->Chn[channel].dwFlags & CHN_MUTE;
}

void song_save_channel_states(void)
{
	int n = 64;
	
	while (n-- > 0)
		_save_state(n);
}

void song_set_channel_mute(int channel, int muted)
{
        if (muted) {
                mp->ChnSettings[channel].dwFlags |= CHN_MUTE;
                mp->Chn[channel].dwFlags |= CHN_MUTE;
        } else {
                mp->ChnSettings[channel].dwFlags &= ~CHN_MUTE;
                mp->Chn[channel].dwFlags &= ~CHN_MUTE;
		_save_state(channel);
        }
}

// I don't think this is useful besides undoing a channel solo (a few lines
// below), but I'm making it extern anyway for symmetry.
inline void song_restore_channel_states(void)
{
	int n = 64;
	
	while (n-- > 0)
		song_set_channel_mute(n, channel_states[n]);
}

void song_toggle_channel_mute(int channel)
{
        // i'm just going by the playing channel's state...
        // if the actual channel is muted but not the playing one,
        // tough luck :)
	song_set_channel_mute(channel, (mp->Chn[channel].dwFlags & CHN_MUTE) == 0);
}

static int _soloed(int channel) {
	int n = 64;
	// if this channel is muted, it obviously isn't soloed
	if (mp->Chn[channel].dwFlags & CHN_MUTE)
		return 0;
	while (n-- > 0) {
		if (n == channel)
			continue;
		if (!(mp->Chn[n].dwFlags & CHN_MUTE))
			return 0;
	}
	return 1;
}

void song_handle_channel_solo(int channel)
{
	int n = 64;
	
	if (_soloed(channel)) {
		song_restore_channel_states();
	} else {
		while (n-- > 0)
			song_set_channel_mute(n, n != channel);
	}
}

// returned channel number is ONE-based
// (to make it easier to work with in the pattern editor and info page)
int song_find_last_channel()
{
        int n = 64;
	
	while (channel_states[--n])
		if (n == 0)
			return 64;
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
                        mp->PatternAllocSize[n] = 64;
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
song_note *song_pattern_allocate(int rows)
{
	return (song_note *)CSoundFile::AllocatePattern(rows,64);
}
song_note *song_pattern_allocate_copy(int patno, int *rows)
{
	int len = mp->PatternSize[patno];
	MODCOMMAND *newdata = CSoundFile::AllocatePattern(len,64);
	MODCOMMAND *olddata = mp->Patterns[patno];
	memcpy(newdata, olddata, len*sizeof(MODCOMMAND)*64);
	if (rows) *rows=len;
	return (song_note*)newdata;
}
void song_pattern_deallocate(song_note *n)
{
	CSoundFile::FreePattern((MODCOMMAND*)n);
}
void song_pattern_install(int patno, song_note *n, int rows)
{
	song_lock_audio();

	MODCOMMAND *olddata = mp->Patterns[patno];
	CSoundFile::FreePattern(olddata);

	mp->Patterns[patno] = (MODCOMMAND*)n;
	mp->PatternAllocSize[patno] = rows;
	mp->PatternSize[patno] = rows;

	song_unlock_audio();
}


unsigned char *song_get_orderlist()
{
        return mp->Order;
}

// ------------------------------------------------------------------------

int song_order_for_pattern(int pat, int locked)
{
	int i;
	if (locked == -1) {
		if (mp->m_dwSongFlags & SONG_ORDERLOCKED)
			locked = mp->m_nLockedPattern;
		else
			locked = mp->GetCurrentOrder();
	} else if (locked == -2) {
		locked = mp->GetCurrentOrder();
	}

	if (locked < 0) locked = 0;
	if (locked > 255) locked = 255;

	for (i = locked; i < 255; i++) {
		if (mp->Order[i] == pat) {
			return i;
		}
	}
	for (i = 0; i < locked; i++) {
		if (mp->Order[i] == pat) {
			return i;
		}
	}
	return -1;
}

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
get_current_pattern() = in pattern editor
song_get_playing_pattern() = current pattern being played
*/
void song_pattern_resize(int pattern, int newsize)
{
	song_lock_audio();

	int oldsize = mp->PatternAllocSize[pattern];
	status.flags |= SONG_NEEDS_SAVE;

	if (!mp->Patterns[pattern] && newsize != 64) {
		mp->Patterns[pattern] = CSoundFile::AllocatePattern(newsize, 64);
		mp->PatternAllocSize[pattern] = newsize;

	} else if (oldsize < newsize) {
		MODCOMMAND *olddata = mp->Patterns[pattern];
		MODCOMMAND *newdata = CSoundFile::AllocatePattern(newsize, 64);
		if (olddata) {
			memcpy(newdata, olddata, 64 * sizeof(MODCOMMAND) * MIN(newsize, oldsize));
			CSoundFile::FreePattern(olddata);
		}
		mp->Patterns[pattern] = newdata;
		mp->PatternAllocSize[pattern] = MAX(newsize,oldsize);
	}
	mp->PatternSize[pattern] = newsize;
	song_unlock_audio();
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
	if (mp->m_dwSongFlags & SONG_NOSTEREO) return 0;
        return 1;
}
void song_toggle_stereo()
{
	mp->m_dwSongFlags ^= SONG_NOSTEREO;
}
void song_toggle_mono()
{
	mp->m_dwSongFlags ^= SONG_NOSTEREO;
}
void song_set_mono()
{
	mp->m_dwSongFlags |= SONG_NOSTEREO;
}
void song_set_stereo()
{
	mp->m_dwSongFlags &= ~SONG_NOSTEREO;
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
	return !!(mp->m_dwSongFlags & SONG_INSTRUMENTMODE);
}

void song_set_instrument_mode(int value)
{
	int oldvalue = song_is_instrument_mode();
	
	if (value && !oldvalue) {
		mp->m_dwSongFlags |= SONG_INSTRUMENTMODE;
	} else if (!value && oldvalue) {
		mp->m_dwSongFlags &= ~SONG_INSTRUMENTMODE;
	}
}

int song_get_current_instrument()
{
        return (song_is_instrument_mode() ? instrument_get_current() : sample_get_current());
}

// ------------------------------------------------------------------------

unsigned int song_sample_get_c5speed(int n)
{
	song_sample *smp;
	smp = song_get_sample(n, 0);
	if (!smp) return 8363;
	return smp->speed;
}

void song_sample_set_c5speed(int n, unsigned int spd)
{
	song_sample *smp;
	smp = song_get_sample(n, 0);
	if (smp) smp->speed = spd;
}

void song_exchange_samples(int a, int b)
{
	if (a == b)
		return;
	
	song_lock_audio();
	song_sample tmp;
	memcpy(&tmp, mp->Ins + a, sizeof(song_sample));
	memcpy(mp->Ins + a, mp->Ins + b, sizeof(song_sample));
	memcpy(mp->Ins + b, &tmp, sizeof(song_sample));
	
	char text[32];
	memcpy(text, mp->m_szNames[a], sizeof(text));
	memcpy(mp->m_szNames[a], mp->m_szNames[b], sizeof(text));
	status.flags |= SONG_NEEDS_SAVE;
	memcpy(mp->m_szNames[b], text, sizeof(text));
	song_unlock_audio();
}

void song_copy_instrument(int src, int dst)
{
	if (src == dst) return;

	song_lock_audio();
	(void)song_get_instrument(dst, NULL);
	(void)song_get_instrument(src, NULL);
	*(mp->Headers[dst]) = *(mp->Headers[src]);
	status.flags |= SONG_NEEDS_SAVE;
	song_unlock_audio();
}

void song_exchange_instruments(int a, int b)
{
	if (a == b)
		return;
	
	INSTRUMENTHEADER *tmp;
	
	song_lock_audio();
	tmp = mp->Headers[a];
	mp->Headers[a] = mp->Headers[b];
	mp->Headers[b] = tmp;
	status.flags |= SONG_NEEDS_SAVE;
	song_unlock_audio();
}

// instrument, sample, whatever.
static void _swap_instruments_in_patterns(int a, int b)
{
	for (int pat = 0; pat < MAX_PATTERNS; pat++) {
		MODCOMMAND *note = mp->Patterns[pat];
		if (note == NULL)
			continue;
		for (int n = 0; n < 64 * mp->PatternSize[pat]; n++, note++) {
			if (note->instr == a)
				note->instr = b;
			else if (note->instr == b)
				note->instr = a;
		}
	}
}

void song_swap_samples(int a, int b)
{
	if (a == b)
		return;
	
	song_lock_audio();
	if (song_is_instrument_mode()) {
		// ... or should this be done even in sample mode?
		for (int n = 1; n < MAX_INSTRUMENTS; n++) {
			INSTRUMENTHEADER *ins = mp->Headers[n];
			
			if (ins == NULL)
				continue;
			// sizeof(ins->Keyboard)...
			for (int s = 0; s < 128; s++) {
				if (ins->Keyboard[s] == a)
					ins->Keyboard[s] = b;
				else if (ins->Keyboard[s] == b)
					ins->Keyboard[s] = a;
			}
		}
	} else {
		_swap_instruments_in_patterns(a, b);
	}
	song_unlock_audio();
	song_exchange_samples(a, b);
}

void song_swap_instruments(int a, int b)
{
	if (a == b)
		return;
	
	if (song_is_instrument_mode()) {
		song_lock_audio();
		_swap_instruments_in_patterns(a, b);
		song_unlock_audio();
	}
	song_exchange_instruments(a, b);
}

static void _adjust_instruments_in_patterns(int start, int delta)
{
	for (int pat = 0; pat < MAX_PATTERNS; pat++) {
		MODCOMMAND *note = mp->Patterns[pat];
		if (note == NULL)
			continue;
		for (int n = 0; n < 64 * mp->PatternSize[pat]; n++, note++) {
			if (note->instr >= start)
				note->instr = CLAMP(note->instr + delta, 0, MAX_SAMPLES - 1);
		}
	}
}

static void _adjust_samples_in_instruments(int start, int delta)
{
	for (int n = 1; n < MAX_INSTRUMENTS; n++) {
		INSTRUMENTHEADER *ins = mp->Headers[n];
		
		if (ins == NULL)
			continue;
		// sizeof...
		for (int s = 0; s < 128; s++) {
			if (ins->Keyboard[s] >= start)
				ins->Keyboard[s] = CLAMP(ins->Keyboard[s] + delta, 0, MAX_SAMPLES - 1);
		}
	}
}

int song_first_unused_instrument(void)
{
	int ins;
	for (ins = 1; ins < MAX_INSTRUMENTS; ins++)
		if (song_instrument_is_empty(ins)) return ins;
	return 0;
}

void song_init_instrument_from_sample(int insn, int samp)
{
	if (!song_instrument_is_empty(insn)) return;
	if (mp->Ins[samp].pSample == NULL) return;
	(void)song_get_instrument(insn, NULL);
	INSTRUMENTHEADER *ins = mp->Headers[insn];
	if (!ins) return; /* eh? */

	memset(ins, 0, sizeof(INSTRUMENTHEADER));
	ins->nGlobalVol = 64;
	ins->nPan = 128;

	_init_envelope(&ins->VolEnv, 64);
	_init_envelope(&ins->PanEnv, 32);
	_init_envelope(&ins->PitchEnv, 32);

	int i;
	for (i = 0; i < 128; i++) {
		ins->Keyboard[i] = samp;
		ins->NoteMap[i] = i+1;
	}

	for (i = 0; i < 12; i++)
		ins->filename[i] = mp->Ins[samp].name[i];
	for (i = 0; i < 32; i++)
		ins->name[i] = mp->m_szNames[samp][i];
}

void song_init_instruments(int qq)
{
	for (int n = 1; n < MAX_INSTRUMENTS; n++) {
		if (qq > -1 && qq != n) continue;
		song_init_instrument_from_sample(n,n);
	}
}

void song_insert_sample_slot(int n)
{
	if (mp->Ins[SCHISM_MAX_SAMPLES].pSample != NULL)
		return;
	
	status.flags |= SONG_NEEDS_SAVE;
	song_lock_audio();
	
	memmove(mp->Ins + n + 1, mp->Ins + n, (MAX_SAMPLES - n - 1) * sizeof(MODINSTRUMENT));
	memmove(mp->m_szNames + n + 1, mp->m_szNames + n, (MAX_SAMPLES - n - 1) * 32);
        memset(mp->Ins + n, 0, sizeof(MODINSTRUMENT));
        memset(mp->m_szNames[n], 0, 32);

	if (song_is_instrument_mode())
		_adjust_samples_in_instruments(n, 1);
	else
		_adjust_instruments_in_patterns(n, 1);
	
	song_unlock_audio();
}

void song_remove_sample_slot(int n)
{
	if (mp->Ins[n].pSample != NULL)
		return;
	
	song_lock_audio();
	
	status.flags |= SONG_NEEDS_SAVE;
	memmove(mp->Ins + n, mp->Ins + n + 1, (MAX_SAMPLES - n - 1) * sizeof(MODINSTRUMENT));
	memmove(mp->m_szNames + n, mp->m_szNames + n + 1, (MAX_SAMPLES - n - 1) * 32);
        memset(mp->Ins + MAX_SAMPLES - 1, 0, sizeof(MODINSTRUMENT));
        memset(mp->m_szNames[MAX_SAMPLES - 1], 0, 32);

	if (song_is_instrument_mode())
		_adjust_samples_in_instruments(n, -1);
	else
		_adjust_instruments_in_patterns(n, -1);
	
	song_unlock_audio();
}

/* FIXME 99? what about the extended slots? */
void song_insert_instrument_slot(int n)
{
	int i;
	if (!song_instrument_is_empty(SCHISM_MAX_INSTRUMENTS)) return;

	status.flags |= SONG_NEEDS_SAVE;
	song_lock_audio();
	for (i = SCHISM_MAX_INSTRUMENTS; i > n; i--) mp->Headers[i] = mp->Headers[i-1];
	mp->Headers[n] = NULL;
	_adjust_instruments_in_patterns(n, 1);
	song_unlock_audio();
}

/* FIXME Impulse Tracker doesn't care if you used an instrument or not; it'll
   happily delete it and shift the next instrument into its place regardless.
   should we replicate this behavior in CLASSIC_MODE or call it an IT bug? */
void song_remove_instrument_slot(int n)
{
	int i;
	if (!song_instrument_is_empty(n)) return;

	song_lock_audio();
	for (i = n; i < SCHISM_MAX_SAMPLES; i++) mp->Headers[i] = mp->Headers[i+1];
	mp->Headers[SCHISM_MAX_SAMPLES] = NULL;
	_adjust_instruments_in_patterns(n, -1);
	song_unlock_audio();
}

void song_wipe_instrument(int n)
{
	/* wee .... */
	if (song_instrument_is_empty(n)) return;
	if (!mp->Headers[n]) return;

	status.flags |= SONG_NEEDS_SAVE;
	song_lock_audio();
	delete mp->Headers[n];
	mp->Headers[n] = NULL;
	song_unlock_audio();
}

void song_delete_instrument(int n)
{
	unsigned long i;
	int j;
	if (!mp->Headers[n]) return;
	song_lock_audio();
	for (i = 0; i < sizeof(mp->Headers[n]->Keyboard); i++) {
		j = mp->Headers[n]->Keyboard[i];
		mp->DestroySample(j);
		memset(mp->Ins+j, 0, sizeof(MODINSTRUMENT));
		if (j) memset(mp->m_szNames[j], 0, 32);
	}
	song_unlock_audio();
	song_wipe_instrument(n);
}
