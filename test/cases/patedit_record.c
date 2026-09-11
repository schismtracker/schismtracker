/*
 * Schism Tracker - follow-mode row placement tests
 */

#include "test.h"
#include "test-assertions.h"

#include "midi.h"
#include "patedit_record.h"

static const int snap_ticks_speed_6[] = { 1, 2, 3 };
static const int no_snap_ticks_speed_6[] = { 0, 4, 5 };

testresult_t test_patedit_tick_should_snap_speed6(void)
{
	int i;

	for (i = 0; i < (int)ARRAY_SIZE(snap_ticks_speed_6); i++) {
		ASSERT(patedit_tick_should_snap(snap_ticks_speed_6[i], 6));
	}
	for (i = 0; i < (int)ARRAY_SIZE(no_snap_ticks_speed_6); i++) {
		ASSERT(!patedit_tick_should_snap(no_snap_ticks_speed_6[i], 6));
	}

	RETURN_PASS;
}

testresult_t test_patedit_tick_should_snap_speeds_1_to_12(void)
{
	static const struct {
		int speed;
		int snap_ticks[6];
		int snap_count;
	} table[] = {
		{ 1, {}, 0 },
		{ 2, { 1 }, 1 },
		{ 3, { 1 }, 1 },
		{ 4, { 1, 2 }, 2 },
		{ 5, { 1, 2 }, 2 },
		{ 6, { 1, 2, 3 }, 3 },
		{ 7, { 1, 2, 3 }, 3 },
		{ 8, { 1, 2, 3, 4 }, 4 },
		{ 9, { 1, 2, 3, 4 }, 4 },
		{ 10, { 1, 2, 3, 4, 5 }, 5 },
		{ 11, { 1, 2, 3, 4, 5 }, 5 },
		{ 12, { 1, 2, 3, 4, 5, 6 }, 6 },
	};
	int s, t, half;

	for (s = 0; s < (int)ARRAY_SIZE(table); s++) {
		int speed = table[s].speed;
		int limit = speed / 2;

		ASSERT(!patedit_tick_should_snap(0, speed));
		for (t = 1; t <= limit; t++) {
			ASSERT(patedit_tick_should_snap(t, speed));
		}
		for (t = limit + 1; t < speed; t++) {
			ASSERT(!patedit_tick_should_snap(t, speed));
		}
		for (t = 0; t < table[s].snap_count; t++) {
			ASSERT(patedit_tick_should_snap(table[s].snap_ticks[t], speed));
		}
		half = speed / 2;
		for (t = half + 1; t < speed; t++) {
			ASSERT(!patedit_tick_should_snap(t, speed));
		}
	}

	RETURN_PASS;
}

testresult_t test_patedit_should_snap_midi_gating(void)
{
	int saved = midi_flags;
	int tick = 3;
	int speed = 6;

	midi_flags = MIDI_TICK_QUANTIZE;
	ASSERT(patedit_should_snap(1, tick, speed));

	midi_flags = 0;
	ASSERT(!patedit_should_snap(1, tick, speed));

	midi_flags = saved;
	ASSERT(patedit_should_snap(0, tick, speed));

	RETURN_PASS;
}

testresult_t test_patedit_deferred_mark_clear(void)
{
	patedit_deferred_clear_all();
	ASSERT(!patedit_deferred_test(0, 10, 1));
	patedit_deferred_mark(0, 10, 1);
	ASSERT(patedit_deferred_test(0, 10, 1));
	ASSERT(!patedit_deferred_test(0, 10, 2));
	patedit_deferred_clear(0, 10, 1);
	ASSERT(!patedit_deferred_test(0, 10, 1));
	patedit_deferred_clear_all();
	RETURN_PASS;
}

testresult_t test_patedit_note_off_needs_bump(void)
{
	patedit_deferred_clear_all();
	patedit_note_on_anchor(2, 12, 4, 60);
	ASSERT(patedit_note_off_needs_bump(2, 12, 4, 60));
	ASSERT(!patedit_note_off_needs_bump(2, 13, 4, 60));
	ASSERT(!patedit_note_off_needs_bump(2, 12, 4, 61));
	patedit_deferred_clear_all();
	ASSERT(!patedit_note_off_needs_bump(2, 12, 4, 60));
	RETURN_PASS;
}
