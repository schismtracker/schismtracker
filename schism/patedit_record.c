/*
 * Schism Tracker - follow-mode row placement (playback tracing)
 * See docs/follow-mode-row-placement.md
 */

#include "headers.h"

#include "it.h"
#include "midi.h"
#include "patedit_record.h"
#include "song.h"

#define PATEDIT_DEFERRED_MAX_ROW 256

static uint64_t deferred[MAX_PATTERNS][PATEDIT_DEFERRED_MAX_ROW];

typedef struct {
	int valid;
	int pattern;
	int row;
	int channel;
	int note;
} patedit_anchor_t;

static patedit_anchor_t note_on_anchor[MAX_CHANNELS];

static int deferred_bit(int chan)
{
	if (chan < 1 || chan > MAX_CHANNELS)
		return -1;
	return chan - 1;
}

int patedit_tick_should_snap(int tick, int speed)
{
	if (speed < 1)
		return 0;
	return tick > 0 && tick <= speed / 2;
}

int patedit_should_snap(int input_is_midi, int tick, int speed)
{
	if (input_is_midi && !(midi_flags & MIDI_TICK_QUANTIZE))
		return 0;
	return patedit_tick_should_snap(tick, speed);
}

void patedit_deferred_clear_all(void)
{
	memset(deferred, 0, sizeof(deferred));
	memset(note_on_anchor, 0, sizeof(note_on_anchor));
}

void patedit_deferred_mark(int pat, int row, int chan)
{
	int bit;

	if (pat < 0 || pat >= MAX_PATTERNS || row < 0 || row >= PATEDIT_DEFERRED_MAX_ROW)
		return;
	bit = deferred_bit(chan);
	if (bit < 0)
		return;
	song_lock_audio();
	deferred[pat][row] |= (1ULL << bit);
	song_unlock_audio();
}

void patedit_deferred_clear(int pat, int row, int chan)
{
	int bit;

	if (pat < 0 || pat >= MAX_PATTERNS || row < 0 || row >= PATEDIT_DEFERRED_MAX_ROW)
		return;
	bit = deferred_bit(chan);
	if (bit < 0)
		return;
	deferred[pat][row] &= ~(1ULL << bit);
}

int patedit_deferred_test(int pat, int row, int chan)
{
	int bit;

	if (pat < 0 || pat >= MAX_PATTERNS || row < 0 || row >= PATEDIT_DEFERRED_MAX_ROW)
		return 0;
	bit = deferred_bit(chan);
	if (bit < 0)
		return 0;
	return !!(deferred[pat][row] & (1ULL << bit));
}

void patedit_note_on_anchor(int pat, int row, int chan, int note)
{
	int idx = chan - 1;

	if (idx < 0 || idx >= MAX_CHANNELS)
		return;
	note_on_anchor[idx].valid = 1;
	note_on_anchor[idx].pattern = pat;
	note_on_anchor[idx].row = row;
	note_on_anchor[idx].channel = chan;
	note_on_anchor[idx].note = note;
}

int patedit_note_off_needs_bump(int pat, int row, int chan, int note)
{
	int idx = chan - 1;
	const patedit_anchor_t *a;

	if (idx < 0 || idx >= MAX_CHANNELS)
		return 0;
	a = &note_on_anchor[idx];
	if (!a->valid)
		return 0;
	return a->pattern == pat && a->row == row
		&& a->channel == chan && a->note == note;
}

void patedit_anchor_clear_channel(int chan)
{
	int idx = chan - 1;

	if (idx < 0 || idx >= MAX_CHANNELS)
		return;
	note_on_anchor[idx].valid = 0;
}

int patedit_resolve_record_target(patedit_record_target_t *out, int channel,
	int input_is_midi)
{
	song_note_t *pattern;
	int row_offset;

	if (!out || channel < 1 || channel > MAX_CHANNELS)
		return 0;

	memset(out, 0, sizeof(*out));

	if ((song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP)) && playback_tracing) {
		out->pattern = song_get_playing_pattern();
		out->row = song_get_current_row();
		out->tick = song_get_current_tick();
		out->speed = song_get_current_speed();
		row_offset = 0;
		if (patedit_should_snap(input_is_midi, out->tick, out->speed))
			row_offset = 1;
		out->row_offset = row_offset;
		out->snap_applied = (row_offset > 0);
		if (!song_get_pattern_offset(&out->pattern, &pattern, &out->row, row_offset))
			return 0;
		out->cell = pattern + MAX_CHANNELS * out->row + (channel - 1);
		return 1;
	}

	out->pattern = get_current_pattern();
	out->row = get_current_row();
	out->tick = 0;
	out->speed = song_get_current_speed();
	out->row_offset = 0;
	out->snap_applied = 0;
	if (!song_get_pattern(out->pattern, &pattern))
		return 0;
	out->cell = pattern + MAX_CHANNELS * out->row + (channel - 1);
	return 1;
}
