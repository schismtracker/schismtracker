/*
 * Schism Tracker - follow-mode row placement (playback tracing)
 * See docs/follow-mode-row-placement.md
 */

#ifndef SCHISM_PATEDIT_RECORD_H_
#define SCHISM_PATEDIT_RECORD_H_

#include "player/sndfile.h"

typedef struct {
	int pattern;
	int row;
	int row_offset;
	int tick;
	int speed;
	song_note_t *cell;
	int snap_applied;
} patedit_record_target_t;

int patedit_tick_should_snap(int tick, int speed);
int patedit_should_snap(int input_is_midi, int tick, int speed);

int patedit_resolve_record_target(patedit_record_target_t *out, int channel,
	int input_is_midi);

void patedit_deferred_clear_all(void);
void patedit_deferred_mark(int pat, int row, int chan);
void patedit_deferred_clear(int pat, int row, int chan);
int patedit_deferred_test(int pat, int row, int chan);

void patedit_note_on_anchor(int pat, int row, int chan, int note);
int patedit_note_off_needs_bump(int pat, int row, int chan, int note);
void patedit_anchor_clear_channel(int chan);

#endif /* SCHISM_PATEDIT_RECORD_H_ */
