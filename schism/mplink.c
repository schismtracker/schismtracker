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
#include "song.h"
#include "slurp.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

// ------------------------------------------------------------------------
// variables

song_t *current_song = NULL;

// ------------------------------------------------------------------------
// song information

unsigned int song_get_length_to(int order, int row)
{
	unsigned int t;

	song_lock_audio();
	current_song->stop_at_order = order;
	current_song->stop_at_row = row;
	t = csf_get_length(current_song);
	current_song->stop_at_order = current_song->stop_at_row = -1;
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
		current_song->stop_at_order = MAX_ORDERS;
		current_song->stop_at_row = 255; /* unpossible */
		current_song->stop_at_time = seconds;
		csf_get_length(current_song);
		if (order) *order = current_song->stop_at_order;
		if (row) *row = current_song->stop_at_row;
		current_song->stop_at_order = current_song->stop_at_row = -1;
		current_song->stop_at_time = 0;
		song_unlock_audio();
	}
}

song_sample_t *song_get_sample(int n)
{
	if (n >= MAX_SAMPLES)
		return NULL;
	return current_song->samples + n;
}

song_instrument_t *song_get_instrument(int n)
{
	if (n >= MAX_INSTRUMENTS)
		return NULL;

	// Make a new instrument if it doesn't exist.
	if (!current_song->instruments[n]) {
		current_song->instruments[n] = csf_allocate_instrument();
	}

	return (song_instrument_t *) current_song->instruments[n];
}

// this is a fairly gross way to do what should be such a simple thing
int song_get_instrument_number(song_instrument_t *inst)
{
	if (inst)
		for (int n = 1; n < MAX_INSTRUMENTS; n++)
			if (inst == ((song_instrument_t *) current_song->instruments[n]))
				return n;
	return 0;
}

song_channel_t *song_get_channel(int n)
{
	if (n >= MAX_CHANNELS)
		return NULL;
	return (song_channel_t *) current_song->channels + n;
}

song_voice_t *song_get_mix_channel(int n)
{
	if (n >= MAX_VOICES)
		return NULL;
	return (song_voice_t *) current_song->voices + n;
}

int song_get_mix_state(uint32_t **channel_list)
{
	if (channel_list)
		*channel_list = current_song->voice_mix;
	return MIN(current_song->num_voices, max_voices);
}

// ------------------------------------------------------------------------
// For all of these, channel is ZERO BASED.
// (whereas in the pattern editor etc. it's one based)

static int channel_states[64];  // saved ("real") mute settings; nonzero = muted

static inline void _save_state(int channel)
{
	channel_states[channel] = current_song->voices[channel].flags & CHN_MUTE;
}

void song_save_channel_states(void)
{
	int n = 64;

	while (n-- > 0)
		_save_state(n);
}
static inline void _fix_mutes_like(int chan)
{
	int i;
	for (i = 0; i < MAX_VOICES; i++) {
		if (i == chan) continue;
		if (((int)current_song->voices[i].master_channel) != (chan+1)) continue;
		current_song->voices[i].flags = (current_song->voices[i].flags & (~(CHN_MUTE)))
				| (current_song->voices[chan].flags &   (CHN_MUTE));
	}
}

void song_set_channel_mute(int channel, int muted)
{
	if (muted) {
		current_song->channels[channel].flags |= CHN_MUTE;
		current_song->voices[channel].flags |= CHN_MUTE;
	} else {
		current_song->channels[channel].flags &= ~CHN_MUTE;
		current_song->voices[channel].flags &= ~CHN_MUTE;
		_save_state(channel);
	}
	_fix_mutes_like(channel);
}

// I don't think this is useful besides undoing a channel solo (a few lines
// below), but I'm making it extern anyway for symmetry.
void song_restore_channel_states(void)
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
	song_set_channel_mute(channel, (current_song->voices[channel].flags & CHN_MUTE) == 0);
}

static int _soloed(int channel) {
	int n = 64;
	// if this channel is muted, it obviously isn't soloed
	if (current_song->voices[channel].flags & CHN_MUTE)
		return 0;
	while (n-- > 0) {
		if (n == channel)
			continue;
		if (!(current_song->voices[n].flags & CHN_MUTE))
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
int song_find_last_channel(void)
{
	int n = 64;

	while (channel_states[--n])
		if (n == 0)
			return 64;
	return n + 1;
}

// ------------------------------------------------------------------------

// calculates row of offset from passed row.
// sets actual pattern number, row and optional pattern buffer.
// returns length of selected patter, or 0 on error.
// if song mode is pattern loop (MODE_PATTERN_LOOP), offset is mod calculated
// in current pattern.
int song_get_pattern_offset(int * n, song_note_t ** buf, int * row, int offset)
{
	int tot;
	if (song_get_mode() & MODE_PATTERN_LOOP) {
		// just wrap around current rows
		*row = (*row + offset) % song_get_rows_in_pattern(*n);
		return song_get_pattern(*n, buf);
	}

	tot = song_get_rows_in_pattern(*n);
	while (offset + *row > tot) {
		offset -= tot;
		(*n)++;
		tot = song_get_rows_in_pattern(*n);
		if (!tot) {
			return 0;
		}
	}

	*row += offset;
	return song_get_pattern(*n, buf);
}

// returns length of the pattern, or 0 on error. (this can be used to
// get a pattern's length by passing NULL for buf.)
int song_get_pattern(int n, song_note_t ** buf)
{
	if (n >= MAX_PATTERNS)
		return 0;

	if (buf) {
		if (!current_song->patterns[n]) {
			current_song->pattern_size[n] = 64;
			current_song->pattern_alloc_size[n] = 64;
			current_song->patterns[n] = csf_allocate_pattern(current_song->pattern_size[n]);
		}
		*buf = current_song->patterns[n];
	} else {
		if (!current_song->patterns[n])
			return 64;
	}
	return current_song->pattern_size[n];
}
song_note_t *song_pattern_allocate_copy(int patno, int *rows)
{
	int len = current_song->pattern_size[patno];
	song_note_t *olddata = current_song->patterns[patno];
	song_note_t *newdata = NULL;
	if (olddata) {
		newdata = csf_allocate_pattern(len);
		memcpy(newdata, olddata, len * sizeof(song_note_t) * 64);
	}
	if (rows)
		*rows = len;
	return newdata;
}
void song_pattern_install(int patno, song_note_t *n, int rows)
{
	song_lock_audio();

	song_note_t *olddata = current_song->patterns[patno];
	csf_free_pattern(olddata);

	current_song->patterns[patno] = n;
	current_song->pattern_alloc_size[patno] = rows;
	current_song->pattern_size[patno] = rows;

	song_unlock_audio();
}

// ------------------------------------------------------------------------

int song_next_order_for_pattern(int pat)
{
	int i, ord = current_song->current_order;

	ord = CLAMP(ord, 0, 255);

	for (i = ord; i < 255; i++) {
		if (current_song->orderlist[i] == pat) {
			return i;
		}
	}
	for (i = 0; i < ord; i++) {
		if (current_song->orderlist[i] == pat) {
			return i;
		}
	}
	return -1;
}


int song_get_rows_in_pattern(int pattern)
{
	if (pattern > MAX_PATTERNS)
		return 0;
	return (current_song->pattern_size[pattern] ? current_song->pattern_size[pattern] : 64) - 1;
}

// ------------------------------------------------------------------------

void song_pattern_resize(int pattern, int newsize)
{
	song_lock_audio();

	int oldsize = current_song->pattern_alloc_size[pattern];
	status.flags |= SONG_NEEDS_SAVE;

	if (!current_song->patterns[pattern] && newsize != 64) {
		current_song->patterns[pattern] = csf_allocate_pattern(newsize);
		current_song->pattern_alloc_size[pattern] = newsize;

	} else if (oldsize < newsize) {
		song_note_t *olddata = current_song->patterns[pattern];
		song_note_t *newdata = csf_allocate_pattern(newsize);
		if (olddata) {
			memcpy(newdata, olddata, 64 * sizeof(song_note_t) * MIN(newsize, oldsize));
			csf_free_pattern(olddata);
		}
		current_song->patterns[pattern] = newdata;
		current_song->pattern_alloc_size[pattern] = MAX(newsize,oldsize);
	}
	current_song->pattern_size[pattern] = newsize;
	song_unlock_audio();
}

// ------------------------------------------------------------------------

void song_set_initial_speed(int new_speed)
{
	current_song->initial_speed = CLAMP(new_speed, 1, 255);
}

void song_set_initial_tempo(int new_tempo)
{
	current_song->initial_tempo = CLAMP(new_tempo, 31, 255);
}

void song_set_initial_global_volume(int new_vol)
{
	current_song->initial_global_volume = CLAMP(new_vol, 0, 128);
}

void song_set_mixing_volume(int new_vol)
{
	current_song->mixing_volume = CLAMP(new_vol, 0, 128);
}

void song_set_separation(int new_sep)
{
	current_song->pan_separation = CLAMP(new_sep, 0, 128);
}

int song_is_stereo(void)
{
	if (current_song->flags & SONG_NOSTEREO) return 0;
	return 1;
}
void song_toggle_stereo(void)
{
	current_song->flags ^= SONG_NOSTEREO;
	song_vars_sync_stereo();
}
void song_toggle_mono(void)
{
	current_song->flags ^= SONG_NOSTEREO;
	song_vars_sync_stereo();
}
void song_set_mono(void)
{
	current_song->flags |= SONG_NOSTEREO;
	song_vars_sync_stereo();
}
void song_set_stereo(void)
{
	current_song->flags &= ~SONG_NOSTEREO;
	song_vars_sync_stereo();
}

int song_has_old_effects(void)
{
	return !!(current_song->flags & SONG_ITOLDEFFECTS);
}

void song_set_old_effects(int value)
{
	if (value)
		current_song->flags |= SONG_ITOLDEFFECTS;
	else
		current_song->flags &= ~SONG_ITOLDEFFECTS;
}

int song_has_compatible_gxx(void)
{
	return !!(current_song->flags & SONG_COMPATGXX);
}

void song_set_compatible_gxx(int value)
{
	if (value)
		current_song->flags |= SONG_COMPATGXX;
	else
		current_song->flags &= ~SONG_COMPATGXX;
}

int song_has_linear_pitch_slides(void)
{
	return !!(current_song->flags & SONG_LINEARSLIDES);
}

void song_set_linear_pitch_slides(int value)
{
	if (value)
		current_song->flags |= SONG_LINEARSLIDES;
	else
		current_song->flags &= ~SONG_LINEARSLIDES;
}

int song_is_instrument_mode(void)
{
	return !!(current_song->flags & SONG_INSTRUMENTMODE);
}

void song_set_instrument_mode(int value)
{
	int oldvalue = song_is_instrument_mode();
	int i, j;

	if (value && !oldvalue) {
		current_song->flags |= SONG_INSTRUMENTMODE;
		for (i = 0; i < MAX_INSTRUMENTS; i++) {
			if (!current_song->instruments[i]) continue;
			/* fix wiped notes */
			for (j = 0; j < 128; j++) {
				if (current_song->instruments[i]->note_map[j] < 1
				|| current_song->instruments[i]->note_map[j] > 120)
					current_song->instruments[i]->note_map[j] = j+1;
			}
		}
	} else if (!value && oldvalue) {
		current_song->flags &= ~SONG_INSTRUMENTMODE;
	}
}

int song_get_current_instrument(void)
{
	return (song_is_instrument_mode() ? instrument_get_current() : sample_get_current());
}

// ------------------------------------------------------------------------

void song_exchange_samples(int a, int b)
{
	if (a == b)
		return;

	song_lock_audio();
	song_sample_t tmp;
	memcpy(&tmp, current_song->samples + a, sizeof(song_sample_t));
	memcpy(current_song->samples + a, current_song->samples + b, sizeof(song_sample_t));
	memcpy(current_song->samples + b, &tmp, sizeof(song_sample_t));
	status.flags |= SONG_NEEDS_SAVE;
	song_unlock_audio();
}

void song_copy_instrument(int dst, int src)
{
	if (src == dst) return;

	song_lock_audio();
	song_get_instrument(dst);
	song_get_instrument(src);
	*(current_song->instruments[dst]) = *(current_song->instruments[src]);
	status.flags |= SONG_NEEDS_SAVE;
	song_unlock_audio();
}

void song_exchange_instruments(int a, int b)
{
	if (a == b)
		return;

	song_instrument_t *tmp;

	song_lock_audio();
	tmp = current_song->instruments[a];
	current_song->instruments[a] = current_song->instruments[b];
	current_song->instruments[b] = tmp;
	status.flags |= SONG_NEEDS_SAVE;
	song_unlock_audio();
}

// instrument, sample, whatever.
static void _swap_instruments_in_patterns(int a, int b)
{
	for (int pat = 0; pat < MAX_PATTERNS; pat++) {
		song_note_t *note = current_song->patterns[pat];
		if (note == NULL)
			continue;
		for (int n = 0; n < 64 * current_song->pattern_size[pat]; n++, note++) {
			if (note->instrument == a)
				note->instrument = b;
			else if (note->instrument == b)
				note->instrument = a;
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
			song_instrument_t *ins = current_song->instruments[n];

			if (ins == NULL)
				continue;
			// sizeof(ins->sample_map)...
			for (int s = 0; s < 128; s++) {
				if (ins->sample_map[s] == (unsigned int)a)
					ins->sample_map[s] = (unsigned int)b;
				else if (ins->sample_map[s] == (unsigned int)b)
					ins->sample_map[s] = (unsigned int)a;
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
	int pat, n;

	for (pat = 0; pat < MAX_PATTERNS; pat++) {
		song_note_t *note = current_song->patterns[pat];
		if (note == NULL)
			continue;
		for (n = 0; n < 64 * current_song->pattern_size[pat]; n++, note++) {
			if (note->instrument >= start)
				note->instrument = CLAMP(note->instrument + delta, 0, MAX_SAMPLES - 1);
		}
	}
}

static void _adjust_samples_in_instruments(int start, int delta)
{
	int n, s;

	for (n = 1; n < MAX_INSTRUMENTS; n++) {
		song_instrument_t *ins = current_song->instruments[n];

		if (ins == NULL)
			continue;
		// sizeof...
		for (s = 0; s < 128; s++) {
			if (ins->sample_map[s] >= (unsigned int) start) {
				ins->sample_map[s] = (unsigned int) CLAMP(
					((int) ins->sample_map[s]) + delta,
					0, MAX_SAMPLES - 1);
			}
		}
	}
}

void song_init_instrument_from_sample(int insn, int samp)
{
	if (!csf_instrument_is_empty(current_song->instruments[insn])) return;
	if (current_song->samples[samp].data == NULL) return;
	song_get_instrument(insn);
	song_instrument_t *ins = current_song->instruments[insn];
	if (!ins) return; /* eh? */

	csf_init_instrument(ins, samp);

	// IT doesn't set instrument filenames unless loading an instrument from disk
	//memcpy(ins->filename, current_song->samples[samp].filename, 12);
	memcpy(ins->name, current_song->samples[samp].name, 32);
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
	if (current_song->samples[MAX_SAMPLES - 1].data != NULL)
		return;

	status.flags |= SONG_NEEDS_SAVE;
	song_lock_audio();

	memmove(current_song->samples + n + 1, current_song->samples + n, (MAX_SAMPLES - n - 1) * sizeof(song_sample_t));
	memset(current_song->samples + n, 0, sizeof(song_sample_t));
	current_song->samples[n].c5speed = 8363;
	current_song->samples[n].volume = 64 * 4;
	current_song->samples[n].global_volume = 64;
	if (song_is_instrument_mode())
		_adjust_samples_in_instruments(n, 1);
	else
		_adjust_instruments_in_patterns(n, 1);

	song_unlock_audio();
}

void song_remove_sample_slot(int n)
{
	if (current_song->samples[n].data != NULL)
		return;

	song_lock_audio();

	status.flags |= SONG_NEEDS_SAVE;
	memmove(current_song->samples + n, current_song->samples + n + 1, (MAX_SAMPLES - n - 1) * sizeof(song_sample_t));
	memset(current_song->samples + MAX_SAMPLES - 1, 0, sizeof(song_sample_t));
	current_song->samples[MAX_SAMPLES - 1].c5speed = 8363;
	current_song->samples[MAX_SAMPLES - 1].volume = 64 * 4;
	current_song->samples[MAX_SAMPLES - 1].global_volume = 64;

	if (song_is_instrument_mode())
		_adjust_samples_in_instruments(n, -1);
	else
		_adjust_instruments_in_patterns(n, -1);

	song_unlock_audio();
}

void song_insert_instrument_slot(int n)
{
	int i;

	if (!csf_instrument_is_empty(current_song->instruments[MAX_INSTRUMENTS - 1]))
		return;

	status.flags |= SONG_NEEDS_SAVE;
	song_lock_audio();
	for (i = MAX_INSTRUMENTS - 1; i > n; i--)
		current_song->instruments[i] = current_song->instruments[i-1];
	current_song->instruments[n] = NULL;
	_adjust_instruments_in_patterns(n, 1);
	song_unlock_audio();
}

void song_remove_instrument_slot(int n)
{
	int i;

	if (!csf_instrument_is_empty(current_song->instruments[n]))
		return;

	song_lock_audio();
	for (i = n; i < MAX_INSTRUMENTS; i++)
		current_song->instruments[i] = current_song->instruments[i+1];
	current_song->instruments[MAX_INSTRUMENTS - 1] = NULL;
	_adjust_instruments_in_patterns(n, -1);
	song_unlock_audio();
}

void song_wipe_instrument(int n)
{
	/* wee .... */
	if (csf_instrument_is_empty(current_song->instruments[n]))
		return;
	if (!current_song->instruments[n])
		return;

	status.flags |= SONG_NEEDS_SAVE;
	song_lock_audio();
	csf_free_instrument(current_song->instruments[n]);
	current_song->instruments[n] = NULL;
	song_unlock_audio();
}

// Returns 1 if sample `n` is used by the specified instrument; 0 otherwise.
static int _song_sample_used_by_instrument(int n, const song_instrument_t *instrument)
{
	int i;

	for (i = 0; i < sizeof instrument->sample_map; i++) {
		if (instrument->sample_map[i] == n) {
			return 1;
		}
	}

	return 0;
}

// Returns 1 if sample `n` is used by at least two instruments; 0 otherwise.
static int _song_sample_used_by_many_instruments(int n)
{
	const song_instrument_t *instrument;
	int found;
	int i;

	found = 0;

	for (i = 1; i < MAX_INSTRUMENTS+1; i++) {
		instrument = current_song->instruments[i];
		if (instrument != NULL) {
			if (_song_sample_used_by_instrument(n, instrument)) {
				if (found) {
					return 1;
				}
				found = 1;
			}
		}
	}

	return 0;
}

// n: The index of the instrument to delete (base-1).
// preserve_samples: If 0, delete all samples used by instrument.
//                   If 1, only delete samples that no other instruments use.
void song_delete_instrument(int n, int preserve_samples)
{
	unsigned long i;
	int j;

	if (!current_song->instruments[n])
		return;
	// 128?  really?
	for (i = 0; i < 128; i++) {
		j = current_song->instruments[n]->sample_map[i];
		if (j) {
			if (!preserve_samples || !_song_sample_used_by_many_instruments(j)) {
				song_clear_sample(j);
			}
		}
	}
	song_wipe_instrument(n);
}

void song_replace_sample(int num, int with)
{
	int i, j;
	song_instrument_t *ins;
	song_note_t *note;

	if (num < 1 || num > MAX_SAMPLES
	    || with < 1 || with > MAX_SAMPLES)
		return;

	if (song_is_instrument_mode()) {
		// for each instrument, for each note in the keyboard table, replace 'smp' with 'with'

		for (i = 1; i < MAX_INSTRUMENTS; i++) {
			ins = current_song->instruments[i];
			if (!ins)
				continue;
			for (j = 0; j < 128; j++) {
				if ((int) ins->sample_map[j] == num)
					ins->sample_map[j] = with;
			}
		}
	} else {
		// for each pattern, for each note, replace 'smp' with 'with'
		for (i = 0; i < MAX_PATTERNS; i++) {
			note = current_song->patterns[i];
			if (!note)
				continue;
			for (j = 0; j < 64 * current_song->pattern_size[i]; j++, note++) {
				if (note->instrument == num)
					note->instrument = with;
			}
		}
	}
}

void song_replace_instrument(int num, int with)
{
	int i, j;
	song_note_t *note;

	if (num < 1 || num > MAX_INSTRUMENTS
	    || with < 1 || with > MAX_INSTRUMENTS
	    || !song_is_instrument_mode())
		return;

	// for each pattern, for each note, replace 'ins' with 'with'
	for (i = 0; i < MAX_PATTERNS; i++) {
		note = current_song->patterns[i];
		if (!note)
			continue;
		for (j = 0; j < 64 * current_song->pattern_size[i]; j++, note++) {
			if (note->instrument == num)
				note->instrument = with;
		}
	}
}

