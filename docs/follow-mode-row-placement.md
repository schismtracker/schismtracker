# Follow-Mode Row Placement

Design and implementation reference for row placement when recording via **keyjazz** or **MIDI** with **playback tracing** (Ctrl-F) enabled.

**Status:** Implemented. Core logic lives in [`include/patedit_record.h`](../include/patedit_record.h) and [`schism/patedit_record.c`](../schism/patedit_record.c); recording paths in [`schism/page_patedit.c`](../schism/page_patedit.c); player hooks in [`player/sndmix.c`](../player/sndmix.c) and [`player/effects.c`](../player/effects.c). Automated tests in [`test/cases/patedit_record.c`](../test/cases/patedit_record.c).

---

## Confirmed design decisions

| Topic | Decision |
|-------|----------|
| **Keyjazz snap gating** | Always when playing (or pattern loop) **and** `playback_tracing` |
| **MIDI snap gating** | Only when `MIDI_TICK_QUANTIZE` **and** playing **and** tracing |
| **MIDI Tick Quantize flag** | Still controls SDx note-delay for MIDI; also gates row snap for MIDI only |
| **Base placement (tracing + playing)** | Engine row/pattern (`song_get_current_row`, `song_get_playing_pattern`), not editor cursor |
| **Snap formula** | `END_ROW_v2`: `tick > 0 && tick <= speed / 2` (integer division) |
| **Live preview** | Always `song_keyrecord` / `song_keyup`; never skip preview on snap |
| **Deferred overlay** | Runtime-only; marks snapped/bumped cells; unified first-visit skip in player |
| **Note-off min duration** | Bump +1 row if off would land on note-on anchor (**MIDI**; keyjazz only if `keyjazz_write_noteoff`) |

---

## 1. Problem statement

With **Ctrl-F / playback tracing** on, recording in the pattern editor while the song plays suffers from misaligned row placement:

1. **Cursor vs engine lag** — Keyjazz and MIDI previously wrote to `current_row` (editor cursor) while preview audio used the live engine. The cursor updates in `pattern_editor_playback_update()` on row changes and was often **one row behind** `song_get_current_row()` at key-press time. Follow-mode recording now uses `patedit_resolve_record_target()` for the write cell when tracing and playing.

2. **IT tick order vs musical lateness** — `song_get_current_tick()` returns `tick_count % speed`. Within a row at speed 6 the chronological order is `0 → 5 → 4 → 3 → 2 → 1`. Presses in the **last half** of the row can sound correct live but write to the **current** engine row, feeling one row **early** in the pattern relative to the next downbeat.

3. **Prior over-quantize** — The reverted MIDI snap used `tick <= speed/2 + 1`, which at speed 6 included mid-row ticks 3–4 and caused notes **one row late**. Commit `896fabc2` removed row-bump from MIDI entirely; `MIDI_TICK_QUANTIZE` again only suppresses SDx insertion.

**Scope:** Keyjazz note column in `pattern_editor_insert()` and MIDI in `pattern_editor_insert_midi()`. Waterfall and other entry points are out of scope (future parity check).

---

## 2. Goals and non-goals

### Goals

- Shared placement logic for keyjazz and MIDI follow-mode recording.
- Pattern cells match what the performer heard (engine row + optional end-of-row snap).
- Immediate live preview without double-trigger on the first pattern pass (deferred overlay).
- Minimum one-row note duration for rapid MIDI note-off after note-on (bump, not suppress).
- Reuse existing `song_get_pattern_offset()` for +1 row and pattern-boundary handling.

### Non-goals

- Changes to save format or `song_note_t`.
- New UI toggles beyond documenting existing MIDI Tick Quantize behavior.
- Row snap when not playing or not tracing (keep cursor-based behavior).
- Changing `song_get_current_tick()` semantics.

---

## 3. Glossary

| Term | Meaning |
|------|---------|
| **Playback tracing** | Ctrl-F; `playback_tracing` — editor cursor follows engine during playback |
| **Engine row** | `song_get_current_row()` / `song_get_playing_pattern()` — live player position |
| **Cursor row** | `current_row` / `current_pattern` — editor selection |
| **Snap** | Bump write target +1 row via `song_get_pattern_offset()` when `END_ROW_v2` matches |
| **Deferred** | Runtime flag on a pattern cell: live input already handled it; skip first pattern pass |
| **SDx quantize** | When `MIDI_TICK_QUANTIZE` is on, do not insert `SDx` note-delay effects for sub-row MIDI timing |
| **Anchor** | Runtime `(pattern, row, channel, note)` recorded after each note-on write for note-off bump logic |

---

## 4. Placement algorithm

### When tracing + playing

```
record_pat  = song_get_playing_pattern()
record_row  = song_get_current_row()
tick        = song_get_current_tick()
speed       = song_get_current_speed()
row_offset  = 0

if should_snap(input_is_midi, tick, speed):
    row_offset = 1

song_get_pattern_offset(&record_pat, &pattern, &record_row, row_offset)
cell = pattern + MAX_CHANNELS * record_row + (channel - 1)
```

### When not tracing or not playing

```
record_pat = current_pattern
record_row = current_row
row_offset = 0
cell = pattern + MAX_CHANNELS * record_row + (channel - 1)
```

### Snap gating (`patedit_should_snap`)

```c
int patedit_should_snap(int input_is_midi, int tick, int speed)
{
    if (input_is_midi && !(midi_flags & MIDI_TICK_QUANTIZE))
        return 0;
    return patedit_tick_should_snap(tick, speed);
}
```

Implemented in `schism/patedit_record.c`. Keyjazz always evaluates `patedit_tick_should_snap` when tracing + playing. MIDI only when Tick Quantize is on.

### Call sites

- `schism/page_patedit.c` — `pattern_editor_insert()` (note column, keyjazz)
- `schism/page_patedit.c` — `pattern_editor_insert_midi()`

### Cursor sync (unchanged)

`pattern_editor_playback_update()` continues to set `current_row = playing_row` for display. Placement must **not** depend on the cursor being caught up.

---

## 5. Snap gating matrix

| Input | Playing + tracing | Row snap | SDx (MIDI only) |
|-------|-------------------|----------|-----------------|
| Keyjazz | yes | always (`END_ROW_v2`) | n/a |
| Keyjazz | no | no (cursor row) | n/a |
| MIDI | yes, Tick Quantize on | yes (`END_ROW_v2`) | suppressed |
| MIDI | yes, Tick Quantize off | no | inserted when tick ≠ 0 |
| MIDI | no | no (cursor row) | per flag |

---

## 6. Tick snap formula (`END_ROW_v2`)

### Formula

```c
/* Snap last floor(speed/2) ticks of the row (excluding downbeat tick 0) */
int patedit_tick_should_snap(int tick, int speed)
{
    return tick > 0 && tick <= speed / 2;   /* integer division */
}
```

**Principle:** Snap the **last half of the row in time**. IT tick values count down (`0 → speed−1 → … → 1`), so this is positional, not “low tick number = late.”

Same formula for keyjazz and MIDI (when snap is gated on).

### Speed 6 reference

| Tick | When | Snap? |
|------|------|-------|
| 0 | downbeat | no |
| 5 | early | no |
| 4 | mid | no |
| 3 | late half | **yes** |
| 2 | end | **yes** |
| 1 | end | **yes** |

### Snap sets by speed (speeds 1–12)

| Speed | `speed/2` | Snap ticks |
|-------|-----------|------------|
| 1 | 0 | ∅ |
| 2 | 1 | 1 |
| 3 | 1 | 1 |
| 4 | 2 | 1, 2 |
| 5 | 2 | 1, 2 |
| 6 | 3 | 1, 2, 3 |
| 7 | 3 | 1, 2, 3 |
| 8 | 4 | 1, 2, 3, 4 |
| 9 | 4 | 1, 2, 3, 4 |
| 10 | 5 | 1, 2, 3, 4, 5 |
| 11 | 5 | 1, 2, 3, 4, 5 |
| 12 | 6 | 1, 2, 3, 4, 5, 6 |

Chronological sequence within a row: `0, speed−1, speed−2, …, 2, 1`. Tick 0 is never snapped.

### Anti-pattern (historical)

```c
tick > 0 && tick <= speed / 2 + 1   /* reverted in 896fabc2 */
```

At speed 6 this snaps ticks 1–**4**, pulling mid-row tick 4 into the window. The bug was **`+ 1`**, not `speed/2` itself.

An earlier conservative attempt (post-fix4) used `tick == 1 || tick == 2` only; `END_ROW_v2` intentionally includes tick 3 at speed 6 via `speed/2`.

### Validation test protocol

Log `{path, tick, speed, play_row, write_row, offset}` at speeds 2, 3, 6, 12. Confirm:

- Snap iff `tick > 0 && tick <= speed/2`
- `write_row == play_row + 1` when snapped; `write_row == play_row` otherwise
- Deferred set when `offset == 1` (note-on snap) or note-off bump (see §8)

---

## 7. Deferred overlay

### Purpose

Immediate live preview via `song_keyrecord`, without double-trigger when the player reads the cell on the next row's tick 0 (snap) or the bumped note-off row.

### Storage

Runtime overlay only — **not** in `song_note_t`, not saved to `.it`.

```c
/* schism/patedit_record.c */
static uint64_t deferred[MAX_PATTERNS][256];  /* one bit per channel (1–64) per row */
```

Rows are indexed 0–255; only rows within a pattern’s length are used. `patedit_deferred_mark()` takes the audio lock; test/clear on the player thread do not re-lock.

### First row visit (unified policy)

A deferred cell is **fully consumed** on first visit — live input already handled it.

1. **Row read** (`csf_process_tick` in `player/sndmix.c`, ~line 988): mask note column — do not copy `m->note` into `chan->row_note`; pass masked cell to `csf_midi_out_note` (no double MIDI out/off).
2. **First tick effects** (`csf_process_effects` in `player/effects.c`): skip entire per-channel processing when `deferred && firsttick` (`continue` at top of channel loop).
3. **Later ticks in same row:** process normally.
4. **Clear deferred** for that `(pattern, row, channel)` when first-tick effects are skipped (`patedit_deferred_clear` in `csf_process_effects`); subsequent loops play the cell normally.

### When to set deferred

- Note-on snap: snapped-to cell (`row_offset == 1`)
- Note-off bump: bumped `NOTE_OFF` cell (see §8)

### Lifecycle — clear all deferred

`patedit_deferred_clear_all()` also clears per-channel note-on anchors. Called from:

| Event | Location |
|-------|----------|
| Playback stop / restart / pattern loop start | `song_reset_play_state()` in `schism/audio_playback.c` |
| Order jump (editor) | `song_set_current_order()` in `schism/audio_playback.c` |
| Pattern replace | `song_pattern_install()` in `schism/mplink.c` |
| Song load / new (pattern editor) | `pated_song_changed()` in `schism/page_patedit.c` |

Per-channel anchor cleared on note-off write via `patedit_anchor_clear_channel()`.

### Thread safety

`patedit_deferred_mark()` uses `song_lock_audio()`. The player reads and clears deferred bits during `csf_process_tick` / `csf_process_effects` on the audio thread without additional locking (same pattern as other playback-side state).

### Sequence (note-on snap)

```
Row N, late-half tick (e.g. 1–3 at speed 6)
  → resolve target row N+1
  → write cell, song_keyrecord immediately
  → mark N+1 deferred

Row N+1, tick 0 (first visit)
  → deferred: mask note, skip first-tick channel processing
  → clear deferred for row N+1

Row N+1 (next loop)
  → normal pattern playback
```

---

## 8. Note-offs

### Scope — primarily MIDI

**Keyboard keyjazz does not record note-offs by default.** On key release:

- `keyjazz_noteoff` (config, often on) — live `song_keyup` only; **no pattern write**
- `keyjazz_write_noteoff` (config, **default off**) — write `^^` to the pattern, **only when** `playback_tracing` is on

So the anchor/bump/deferred logic in this section targets **MIDI** (`MIDI_RECORD_NOTEOFF`, default on, plus tracing + playing). It applies to keyjazz **only if** the user enables `keyjazz_write_noteoff` — same rules for parity, but that path is uncommon.

Normal keyboard follow-mode recording is note-on only; §4–§7 cover it. Implemented in `pattern_editor_insert_midi()` and `pattern_editor_insert()` (note column) when `keyjazz_write_noteoff && playback_tracing`.

Replaced the former cursor-based “bump to next row if occupied” hack in `pattern_editor_insert()` with resolve + anchor + `song_get_pattern_offset` bump.

### Minimum note duration — bump, don't suppress

Performers on **MIDI** often send note-off immediately after note-on. Both events can resolve to the **same cell**.

| Event | Behavior |
|-------|----------|
| Note-off would land on same row as its note-on anchor | **Bump +1 row**, write `NOTE_OFF`, mark **deferred** |
| Note-on → cell with note-off | Normal overwrite (note-on wins) |

Live audio always releases immediately via `song_keyup`. Pattern gets a deferred `NOTE_OFF` on the bumped row.

### Per-voice anchor

After each note-on write, record `(pattern, row, channel, note)`. On note-off, after `patedit_resolve_record_target()`:

```
if (target.row == anchor.row && target.pattern == anchor.pattern
    && channel/note match)
    apply extra offset +1 via song_get_pattern_offset
    write NOTE_OFF to bumped cell
    patedit_deferred_mark(pat, row, chan)
else
    write NOTE_OFF normally
```

Reset anchor on: `patedit_note_on_anchor()` after each note-on write; `patedit_anchor_clear_channel()` after note-off write; `patedit_deferred_clear_all()` on playback/song discontinuities (see §7 lifecycle).

### Examples

```
Note-on  → row 12        → written, anchor=12
Note-off → resolves 12   → bumped to row 13, NOTE_OFF + deferred

Note-on  → row 12, snap → row 13, anchor=13
Note-off → resolves 13  → bumped to row 14, NOTE_OFF + deferred

Note-on  → row 12
Note-off → resolves 13  → written normally (no bump)
```

Deferred `NOTE_OFF` uses the same first-visit policy as deferred note-on (mask note at row read + skip first-tick channel processing).

---

## 9. Preview path

- **Always** `song_keyrecord` after mask/effects/instrument-map applied (note-on).
- **Always** `song_keyup` for live release (note-off), even when pattern write is bumped.
- Deferred overlay prevents duplicate pattern playback; it does **not** skip preview.
- Do not revive `quantize_next_row` skip-preview behavior from the reverted MIDI path.

---

## 10. Shared helper API

Public header: [`include/patedit_record.h`](../include/patedit_record.h). Implementation: [`schism/patedit_record.c`](../schism/patedit_record.c).

```c
typedef struct {
    int pattern, row, row_offset, tick, speed;
    song_note_t *cell;
    int snap_applied;  /* row_offset > 0 from END_ROW_v2 snap */
} patedit_record_target_t;

int patedit_tick_should_snap(int tick, int speed);       /* END_ROW_v2 */
int patedit_should_snap(int input_is_midi, int tick, int speed);

int patedit_resolve_record_target(patedit_record_target_t *out, int channel,
    int input_is_midi);

void patedit_deferred_clear_all(void);
void patedit_deferred_mark(int pat, int row, int chan);
void patedit_deferred_clear(int pat, int row, int chan);
int  patedit_deferred_test(int pat, int row, int chan);

void patedit_note_on_anchor(int pat, int row, int chan, int note);
int  patedit_note_off_needs_bump(int pat, int row, int chan, int note);
void patedit_anchor_clear_channel(int chan);
```

Pattern offset helper (unchanged): `song_get_pattern_offset()` in `schism/mplink.c` — pattern loop wrap and song-mode pattern advance for snap and note-off bumps.

---

## 11. Test matrix

### Automated

`test/cases/mplink.c` — `song_get_pattern_offset()` edge cases (pattern loop wrap, song-mode next pattern, length-1 pattern, etc.).

`test/cases/patedit_record.c` (run via `schismtrackertest`):

| Test | Coverage |
|------|----------|
| `test_patedit_tick_should_snap_speed6` | Speed 6 snap/no-snap ticks |
| `test_patedit_tick_should_snap_speeds_1_to_12` | END_ROW_v2 table for speeds 1–12 |
| `test_patedit_should_snap_midi_gating` | `MIDI_TICK_QUANTIZE` on/off; keyjazz path |
| `test_patedit_deferred_mark_clear` | Deferred bitmask mark/test/clear |
| `test_patedit_note_off_needs_bump` | Per-channel anchor matching |

### Manual

| Scenario | Expect |
|----------|--------|
| Stopped, no tracing | Cursor row |
| Playing, tracing on, tick 0 | Engine row, no snap |
| Playing, tracing on, tick in snap set | Engine row + 1, deferred |
| Playing, tracing on, tick outside snap set | Engine row |
| MIDI, Tick Quantize off | No snap; SDx when tick ≠ 0 |
| Rapid MIDI on/off same row | Note-on row intact; NOTE_OFF bumped + deferred |
| Snap + immediate MIDI off | NOTE_OFF two rows ahead of engine at on-time |
| Note-on overwrites bumped NOTE_OFF | Normal overwrite |
| Pattern loop, last row + snap | Wraps via `song_get_pattern_offset` |
| Second pattern pass | Deferred cells play normally |

---

## 12. Implementation status

| Phase | Status |
|-------|--------|
| `patedit_record` module + unit tests | Done |
| Deferred overlay + player hooks (`sndmix.c`, `effects.c`) | Done |
| Keyjazz path (`pattern_editor_insert()` note column) | Done |
| MIDI path (`pattern_editor_insert_midi()`) | Done |
| Note-off anchor + bump (MIDI + opt-in `keyjazz_write_noteoff`) | Done |
| Lifecycle clears (stop, order jump, pattern install, song change) | Done |
| Manual test matrix (§11) | Recommended when changing this code |

**Still out of scope:** waterfall and other recording entry points (future parity).

---

## 13. Implementation notes

**Deferred storage:** Per-pattern `uint64_t` row bitmask (64 channels), not serialized to disk.

**Pattern boundary:** Snap and note-off bumps use `song_get_pattern_offset()` for +1 row and wrap/advance. Deferred marks use the **resolved** `(pattern, row)` after offset. `patedit_deferred_clear_all()` on playback discontinuities avoids stale bits if the player jumps patterns/orders.

**Pitch bend while tracing:** MIDI pitch-bend effect writes also use `patedit_resolve_record_target()` when tracing and playing.

---

## Appendix A: Post-mortem history

Condensed from `docs/keyjazz-playback-tracing-postmortem.md` (commit `ad5df64d`). **Superseded** recommendations are marked; this spec replaces them.

### Symptoms (2026 debug session)

- Preview sounded on the beat; written notes often **one row above** intended position.
- Separate from older MIDI-only quantize (PR #332, reverted in `896fabc2`).

### Root causes found

1. **Cursor lag** — wrote `current_row`, not engine row.
2. **End-of-row timing** — late-half presses wrote current row; felt early vs next downbeat.
3. **Over-quantize** — `speed/2 + 1` snapped mid-row ticks → late notes.
4. **Silent snap** — skipping preview on deferred writes caused silence until next loop.

### Fix iterations

| Iteration | Change | Outcome |
|-----------|--------|---------|
| post-fix1 | Engine row when tracing | Cursor lag mostly fixed |
| post-fix2 | `tick <= speed/2 + 1` on keyjazz | **Late** notes |
| post-fix3 | Revert snap; skip preview on offset | Early notes return |
| post-fix4 | Snap tick 1–2 only | Best subjective; ~1 early note remains |
| post-fix5 | cursor-lag + tick 5 snap | **Rejected** — late quantize |
| post-fix6 | Revert to post-fix4 | Good placement; **silent** end-row |
| post-fix7 | `song_keyrecord` on deferred | Not fully verified |

### Superseded recommendations

The post-mortem recommended post-fix4 (`tick == 1 || tick == 2`) and optional skip-preview policy A. **This spec supersedes those with:**

- `END_ROW_v2` (`tick <= speed/2`) for snap window
- Always preview + deferred overlay for duplicate prevention
- Shared keyjazz/MIDI placement helper
- Note-off bump + deferred for min one-row duration

---

## Appendix B: IT tick reference (`END_ROW_v2`)

### Chronological diagram (speed 6)

```
time ──────────────────────────────────────────────►

tick:  0      5      4      3      2      1     │ next row
       │      │      │      │      │      │     │
       D      early  mid    late   end    end    │
                      └─ snap ─┘└── snap ──┘     │
       no snap ──────┘                           │
```

- **D** = downbeat (tick 0), never snap
- **Snap** = `tick > 0 && tick <= speed/2` → ticks 1, 2, 3 at speed 6

### Full tables (speeds 1–12)

For each speed `S`, chronological tick sequence is `0, S−1, S−2, …, 1`.

| Speed | Sequence | Snap ticks | No snap |
|-------|----------|------------|---------|
| 1 | 0 | ∅ | 0 |
| 2 | 0→1 | 1 | 0 |
| 3 | 0→2→1 | 1 | 0, 2 |
| 4 | 0→3→2→1 | 1, 2 | 0, 3 |
| 5 | 0→4→3→2→1 | 1, 2 | 0, 3, 4 |
| 6 | 0→5→4→3→2→1 | 1, 2, 3 | 0, 4, 5 |
| 7 | 0→6→5→4→3→2→1 | 1, 2, 3 | 0, 4–6 |
| 8 | 0→7→…→1 | 1–4 | 0, 5–7 |
| 9 | 0→8→…→1 | 1–4 | 0, 5–8 |
| 10 | 0→9→…→1 | 1–5 | 0, 6–9 |
| 11 | 0→10→…→1 | 1–5 | 0, 6–10 |
| 12 | 0→11→…→1 | 1–6 | 0, 7–11 |

### Formula equivalence

Snap tick `t` (where `t > 0`) when:

```c
t <= speed / 2
```

Chronological index from row start: `index = speed - t` (for `t > 0`). Snap when `index >= speed - speed/2` (second half of row in time).

### API note

Recording code uses `song_get_current_tick()` (`tick_count % speed` in `schism/audio_playback.c`). The player also defines `csf_get_current_tick()` as `speed - tick_count`; placement logic must use **`song_get_current_tick()`** consistently.

---

## Key code references

**Recording target resolution** (follow-mode writes use `tgt.cell`, not `current_row`):

```c
patedit_record_target_t tgt;
if (patedit_resolve_record_target(&tgt, channel, input_is_midi))
    cur_note = tgt.cell;
```

**Note-on snap deferred mark:**

```c
if (tgt.snap_applied)
    patedit_deferred_mark(tgt.pattern, tgt.row, channel);
```

**Note-off bump** (after resolve; extra +1 when anchor matches):

```c
if (patedit_note_off_needs_bump(tgt.pattern, tgt.row, chan, note)) {
    song_get_pattern_offset(&tgt.pattern, &patbuf, &tgt.row, 1);
    cur_note = patbuf + MAX_CHANNELS * tgt.row + (chan - 1);
    patedit_deferred_mark(tgt.pattern, tgt.row, chan);
}
```

**Cursor chase** (display only; placement does not depend on it):

```c
/* pattern_editor_playback_update — schism/page_patedit.c */
if (playback_tracing) {
    set_current_pattern(playing_pattern);
    current_row = playing_row;
}
```

**Player first visit** (`player/sndmix.c` — mask note; `player/effects.c` — skip first-tick channel FX and clear deferred bit).
