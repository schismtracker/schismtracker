# Playlist / Auto-Advance

Design reference for a jukebox-style capability: load and play multiple modules in sequence, automatically advancing to the next module when the current one ends.

**Status:** Proposed (design spec). Nothing here is implemented. Today schismtracker loads exactly one module from the CLI (the trailing-arg loop in [`schism/main.c`](../schism/main.c) keeps only the **last** file) and loops it forever; there is no playlist, queue, or auto-advance.

---

## Confirmed design decisions

These are the intended decisions for the first usable version (MVP). They are proposals, not implemented behavior.

| Topic | Decision |
|-------|----------|
| **End-of-track detection** | Poll `song_get_mode() == MODE_STOPPED` in `event_loop()`; detect the playing→stopped transition. No new DSP. |
| **Where the playlist lives (MVP)** | CLI multi-file args (option A); the shell drives order. Collect **all** trailing file args into a list instead of overwriting `initial_song`. |
| **Advance action** | `song_stop_unlocked()` (clean MIDI/OPL/GM reset) → `song_load_unchecked(next)` → `song_start_once()`. |
| **Loop semantics for playlist** | Use `song_start_once()` (play through once, no loop) so a finite end is observable, instead of `song_start()` (loops forever). |
| **Infinite-module guard** | A per-track time cap (`--max-time`) is **mandatory** for real-world `.mod`/`.it` files; many loop forever via `Bxx`/`Cxx` jumps and never set `SONG_ENDREACHED`. |
| **Handoff gapless-ness (MVP)** | Simple stop→load→start; a short audible gap is accepted. True gapless (double-buffered `song_t`) is out of scope for v1. |
| **Failed loads** | Skip and advance to the next entry; log a warning; never abort the whole playlist. |
| **Repeat / shuffle / next / prev** | UX layer; deferred past MVP (see §6). MVP plays the list once in order then stops. |
| **Headless playback** | Out of scope for MVP. Would require relaxing the current `--headless` ⇒ `--diskwrite` requirement (see §1, §5). |

---

## 1. Problem statement

schismtracker is a single-module program from the command line:

1. **One file only.** `parse_options()` ([`schism/main.c`](../schism/main.c), ~line 180) parses with `getopt_long`. After option parsing, the trailing-arg loop (~line 317) iterates `optind..argc`, and for each **file** argument does `free(initial_song); initial_song = norm;` — so only the **last** file survives. Passing three modules plays only the third.
2. **Infinite loop.** Normal playback uses `song_start()` ([`schism/audio_playback.c`](../schism/audio_playback.c), ~line 852), which loops the song forever. There is no natural "stop when done."
3. **No queue, no auto-advance.** There is nothing that loads a *next* module when the current ends.

Historically this has been pushed onto external tools. Upstream issue **#347** (headless batch rendering) was deflected to `libopenmpt` / `openmpt123`. Upstream issue **#194** (play once / no loop) is still open. Neither delivers an in-app jukebox.

**Key insight: schismtracker already detects the natural end of a song.** Auto-advance is *wiring*, not new audio code:

- `SONG_ENDREACHED` is defined at [`include/player/sndfile.h:213`](../include/player/sndfile.h).
- It is set in [`player/sndmix.c`](../player/sndmix.c) when `csf_read_note()` returns 0 (~line 762), and in `increment_order` when the orderlist reaches `ORDER_LAST` and `repeat_count` says "don't loop" (~lines 883 / 903).
- `song_get_mode()` ([`schism/audio_playback.c:1007`](../schism/audio_playback.c)) returns `MODE_STOPPED` when **both** `SONG_ENDREACHED` and `SONG_PAUSED` are set, and `MODE_PLAYING` / `MODE_PATTERN_LOOP` otherwise (enum in [`include/song.h:97`](../include/song.h)).
- The diskwrite render path ([`schism/disko.c`](../schism/disko.c), ~line 665) already loops `do { ... } while (!(dwsong.flags & SONG_ENDREACHED));`, proving that a finite end-of-song is observable in practice.

So the feature reduces to: observe the transition into `MODE_STOPPED`, and load + start the next entry.

---

## 2. Goals and non-goals

### Goals

- Play a list of modules in sequence, advancing automatically when each ends.
- Reuse existing end-of-song detection (`SONG_ENDREACHED` / `song_get_mode()`); add no DSP.
- Reuse the existing two-call load+start API and the clean-stop path.
- Provide a guard for modules that never end naturally (time cap), since this is the common case.
- Keep the MVP small (~50 lines, no new UI) and additive — single-file invocation must behave exactly as today.

### Non-goals (v1)

- True gapless / crossfade playback (double-buffered `song_t`).
- A full playlist editor UI (proposed as a later phase; see §6).
- Headless *playback* batch (relaxing `--headless` ⇒ `--diskwrite`); flagged as a follow-up.
- `.m3u` / playlist-file parsing in MVP (option C; later phase).
- Changing save formats or `song_note_t`.
- Solving "infinite module" perfectly — a time cap is a pragmatic bound, not exact song-length detection.

---

## 3. Glossary

| Term | Meaning |
|------|---------|
| **Playlist** | Ordered list of module file paths to play in sequence. |
| **Entry / track** | One module in the playlist. |
| **Auto-advance** | Loading + starting the next entry when the current one ends. |
| **Natural end** | `SONG_ENDREACHED` set by the player (orderlist `ORDER_LAST` reached, or `csf_read_note` returns 0). |
| **`MODE_STOPPED`** | `song_get_mode()` value when `(SONG_ENDREACHED | SONG_PAUSED)` are both set ([`audio_playback.c:1007`](../schism/audio_playback.c)). |
| **Play-once** | `song_start_once()` — `repeat_count = -1`, `mix_flags |= SNDMIX_NOBACKWARDJUMPS`; plays through once and stops (no loop-to-start). |
| **Time cap** | Per-track wall-clock / song-time limit that forces advance even if the module never sets `SONG_ENDREACHED`. |
| **Clean handoff** | Stopping the current song via `song_stop_unlocked()` (MIDI all-notes-off, `OPL_Reset`, `GM_Reset`) before loading the next. |
| **Now-playing** | The currently active playlist index + its file path/title, surfaced in status. |

---

## 4. Core auto-advance mechanism

### Polling site

`event_loop()` ([`schism/main.c`](../schism/main.c), ~line 424) already calls `song_get_mode()` every idle pass (~line 845) for screensaver logic. That is a ready-made polling site: it runs whenever the program is idle and already branches on `MODE_PLAYING` / `MODE_PATTERN_LOOP` / default. We add playlist handling on the transition **into** `MODE_STOPPED`.

### State

```c
/* schism/main.c (or a small new playlist.c/.h) */
static char  **playlist;        /* file paths, collected from CLI args */
static int     playlist_count;
static int     playlist_index;  /* currently-playing entry, -1 = none */
static int     playlist_active; /* nonzero when auto-advance is armed */

/* one-shot edge detector for playing -> stopped */
static enum song_mode prev_mode = MODE_STOPPED;
```

The arg loop in `parse_options()` (~line 317) is changed to **append** each file arg to `playlist` rather than overwriting `initial_song`. For backward compatibility, `initial_song` can remain the last entry (or `playlist[0]`); single-file behavior is unchanged.

### Advance algorithm (pseudocode)

```
/* called once per idle pass in event_loop(), near the existing
   song_get_mode() switch at ~line 845 */
playlist_poll():
    if not playlist_active or playlist_count == 0:
        return

    mode = song_get_mode()

    /* edge: was playing/looping, now stopped == natural end (or time cap) */
    ended = (prev_mode == MODE_PLAYING or prev_mode == MODE_PATTERN_LOOP)
            and mode == MODE_STOPPED
    prev_mode = mode

    if time_cap_enabled and song_get_current_time() >= max_time_secs:
        ended = 1                       /* force advance; see §4.1 */

    if not ended:
        return

    playlist_advance(+1)

playlist_advance(step):
    song_stop_unlocked(...)             /* clean handoff: MIDI/OPL/GM reset */

    next = playlist_index + step
    while next in [0, playlist_count):
        if song_load_unchecked(playlist[next]):   /* load OK */
            playlist_index = next
            song_start_once()                      /* play through once */
            prev_mode = MODE_PLAYING               /* re-arm edge detector */
            announce_now_playing(next)
            return
        log_warn("playlist: skipping unloadable %s", playlist[next])
        next += step                               /* skip failed load */

    /* fell off the end: list finished */
    playlist_active = 0                  /* MVP: stop; with repeat-all, wrap */
```

### Startup wiring

On startup, if `playlist_count > 0` and `SF_PLAY` is set (or always, for a `--playlist`/auto-advance flag), start at index 0 with `song_start_once()` and set `playlist_active = 1`, `prev_mode = MODE_PLAYING`. Single file with no auto-advance flag keeps the existing `song_start()` (loop forever) behavior.

### Why `song_start_once()` and not `song_start()`

`song_start()` loops forever, so `SONG_ENDREACHED` is reset on every loop and the song never stays `MODE_STOPPED` — the edge never fires. `song_start_once()` ([`schism/audio_playback.c`](../schism/audio_playback.c), ~line 836) sets `repeat_count = -1` and `mix_flags |= SNDMIX_NOBACKWARDJUMPS`, so the player runs to the orderlist `ORDER_LAST` and then sets `SONG_ENDREACHED` (the increment_order path at ~line 903). This is exactly the "play once, no loop" behavior also requested in upstream #194.

### 4.1 The time-cap path

See §5 for why a cap is required. The cap reuses `song_get_current_time()` ([`schism/audio_playback.c:1019`](../schism/audio_playback.c) — `samples_played / mix_frequency`, in seconds). When the playing track's current time reaches `max_time_secs`, the poll forces `ended = 1` and advances exactly as for a natural end. `samples_played` is reset by `song_reset_play_state()` on each start, so the cap is per-track.

---

## 5. Design axis — where the playlist lives

| Option | Description | Pros | Cons | Phase |
|--------|-------------|------|------|-------|
| **A. CLI multi-file args** | `schismtracker a.it b.mod c.xm`; shell drives order/globbing. Extend the trailing-arg loop to collect all files. | Smallest change (~50 lines, no UI). Composable with shell (`*.mod`, `find`, `sort`). Backward compatible. | No in-app reordering. Order fixed at launch. | **MVP** |
| **B. In-app playlist page / manager** | A new page (like the load-module browser) to add/remove/reorder/save a queue, with now-playing highlight. | Interactive; discoverable; matches the TUI. Enables next/prev/shuffle UI naturally. | Significant UI work; new page, key handling, persistence. | Later |
| **C. Directory or `.m3u` argument** | `schismtracker --playlist list.m3u` or pass a directory to enqueue its modules. | Familiar jukebox idiom; persistable lists; easy to script. | Needs `.m3u`/dir parsing + recursion rules; ordering/sort policy. | Later |
| **D. Lua-driven (PR #312)** | Expose the playlist and an `on_song_end` callback to Lua. User scripts decide what plays next. | Maximum flexibility; programmable queues, conditional logic, integration with the existing scripting effort. | Depends on PR #312 (open/stalled, by repo owner `guysv`). API surface to design. | Later / optional |

Options compose: A is the engine; B and C are front-ends that populate the same `playlist[]`; D exposes the same list/hook to scripts. The MVP ships A so that B/C/D are purely additive front-ends over a working advance loop.

---

## 6. UX and modes

All of these are **post-MVP** unless noted. MVP plays the list once in order, then stops (`playlist_active = 0`).

| Mode | Behavior |
|------|----------|
| **Repeat none** (MVP) | Play list once, then stop. |
| **Repeat all** | After the last entry, wrap to index 0. (`playlist_advance` wraps instead of clearing `playlist_active`.) |
| **Repeat one** | Replay the current entry on end (advance step 0). This is also "old behavior" if a single file uses `song_start()`. |
| **Shuffle** | Randomized order; either a shuffled index permutation or random next-pick. |
| **Stop after current** | Latch a flag; on next end, do not advance — clear `playlist_active`. |

Controls and surfacing:

- **Next / Prev keybinds** — call `playlist_advance(+1)` / `playlist_advance(-1)` immediately (interrupting the current track via `song_stop_unlocked()` first). Bindings to be assigned in the key handler; avoid clashing with existing global keys.
- **Now-playing status** — show `playlist_index + 1 / playlist_count` and the current filename/title. The load path already tracks the song filename (`song_get_filename()`, used in [`schism/page_loadmodule.c`](../schism/page_loadmodule.c)).
- **Optional track-change hook** — schismtracker already runs hooks via `os_run_hook(cfg_dir_dotschism, "<name>", ...)` ([`schism/main.c:99-110`](../schism/main.c): startup-hook, exit-hook, diskwriter-hook). A `track-change-hook` (passing the new file path) would let users drive external displays / scrobbling. Gated by `ENABLE_HOOKS` / `SF_HOOKS` like the others.

---

## 7. The "infinite module" problem (the hard part)

Most real-world modules **never end naturally**. They loop forever via internal pattern jumps (`Bxx` position-jump back, `SBx` pattern-loop), so the orderlist never cleanly reaches `ORDER_LAST` with no backward jump. `song_start_once()` only suppresses *backward jumps to before the start* (`SNDMIX_NOBACKWARDJUMPS`) and the loop-back-to-song-start case — it does **not** turn an arbitrary mid-song `Bxx` loop into an end. For such files `SONG_ENDREACHED` may never be set, and naive auto-advance would hang on track 1 forever.

Options for bounding a track:

| Strategy | How | Pros | Cons |
|----------|-----|------|------|
| **Repeat-count cap** | Advance after the song's `repeat_count` exceeds N (the player already increments `repeat_count` at [`player/sndmix.c`](../player/sndmix.c) ~883/903). | Reuses existing counter; "play each module twice through" is meaningful. | Only counts orderlist wraps / pattern-loop repeats, not arbitrary `Bxx`; a tight `Bxx` loop may never bump it. |
| **Per-track time limit** (`--max-time`, like `openmpt123 --end-time`) | Force advance when `song_get_current_time() >= max_time_secs` (§4.1). | Always terminates; predictable; simple; matches a known tool's UX. | Cuts songs that are legitimately longer; arbitrary default. |
| **Pre-scan length** | Render the track headless through the disko path (which already loops to `SONG_ENDREACHED`, [`disko.c`](../schism/disko.c) ~665) to learn its finite length, then play with that bound. Disko caps runaway renders at ~`MAX_SAMPLE_LENGTH` (~3 min @ 44k, ~line 667). | Accurate finite length when one exists; reuses existing render-to-end. | Doubles work (scan + play); for truly infinite modules the disko cap (~3 min) is itself just another time cap. |
| **Hybrid: natural-end OR timeout** (recommended) | Advance on whichever fires first: `MODE_STOPPED` edge **or** time cap. | Finite songs end naturally and snappily; infinite songs are bounded. | Needs both code paths (already both cheap). |

**Decision:** a time cap is effectively **mandatory** for real-world `.mod` files. MVP ships the hybrid (natural-end OR `--max-time`), with a sensible default cap that the user can override or disable. Repeat-count cap and pre-scan are optional refinements.

---

## 8. Clean track handoff

| Concern | Handling |
|---------|----------|
| **Hanging notes / MIDI / OPL** | Before loading the next entry, call the clean-stop path. `song_stop_unlocked()` ([`schism/audio_playback.c:884`](../schism/audio_playback.c)) issues MIDI all-notes-off, `OPL_Reset`, and `GM_Reset`. Skipping this leaves notes hanging across the handoff. |
| **Load + start** | Two-call API: `song_load_unchecked(path)` ([`schism/audio_loadsave.c:282`](../schism/audio_loadsave.c)) then `song_start_once()`. Same sequence already used by drag-drop ([`schism/main.c:731`](../schism/main.c), `song_load`) and the load-module page ([`schism/page_loadmodule.c:160`](../schism/page_loadmodule.c)). Use `song_load_unchecked` (no "discard unsaved changes" prompt) for the unattended jukebox case. |
| **Gap vs gapless** | Simple stop→load→start has an audible gap (load + buffer flush). True gapless requires loading the next `song_t` into a second buffer and switching at the sample boundary — a double-buffered player. **Out of scope for v1**; documented as a future enhancement. |
| **Failed load** | `song_load_unchecked` returns success/failure. On failure, log a warning and continue to the next entry (`playlist_advance` skip loop in §4). Never abort the playlist on one bad file. |
| **Edge detector re-arm** | After a successful start, set `prev_mode = MODE_PLAYING` so the next playing→stopped transition is detected. Without this, a fast-starting track that is already (briefly) stopped could double-advance. |
| **Headless-playback caveat** | `--headless` currently **requires** `--diskwrite` and renders exactly one file then exits ([`schism/main.c`](../schism/main.c) ~line 1297). A headless *playback* jukebox (no UI, audio out, advancing) would mean relaxing that requirement and adding an audio-device path to the headless branch. Out of scope for MVP; called out so the requirement check is not mistaken for playlist support. |

---

## 9. Edge cases

| Case | Expected behavior |
|------|-------------------|
| Single file, no auto-advance flag | Unchanged: `song_start()`, loops forever. |
| Empty playlist | No auto-advance; behaves as today. |
| One file with auto-advance | Plays once (`song_start_once`); on end either stops (repeat-none) or replays (repeat-one). |
| Unloadable file mid-list | Logged, skipped; advance continues. |
| All files unloadable | `playlist_active = 0`; program idles (no crash, no spin). |
| Module that never ends, no time cap | Plays forever — exactly the failure §7 prevents; hence cap is default-on. |
| Module shorter than buffer / instantly stopped | Edge detector still fires once; advance proceeds. Re-arm prevents double-advance. |
| User manually stops playback (F8) | This also produces `MODE_STOPPED`. Need to distinguish *natural end* from *user stop* so a manual stop doesn't auto-advance. Use a flag set only when the player set `SONG_ENDREACHED` itself (or only advance while `playlist_active` and the stop wasn't user-initiated). **Open design point** — must be resolved before MVP ships, e.g. by checking that the stop came from the player, not from a `song_stop()` UI action. |
| User manually loads a new file from the UI | Should it leave the playlist? MVP: a manual load deactivates auto-advance (`playlist_active = 0`) to avoid surprising jumps. |
| Time cap shorter than a short song | Song is cut at the cap; acceptable and documented. Default cap should be generous. |
| `-` (stdin) entry | Single-stream only; not meaningfully enqueued more than once. MVP: allow at most one `-` and otherwise treat as today. |
| Directory passed as arg | Today it sets `initial_dir`. MVP keeps that; enqueueing a directory's contents is option C (later). |

---

## 10. Test matrix

### Automatable

The advance state machine can be unit-tested if `playlist_advance` / the edge detector are factored into a small module that takes `song_get_mode()` and load/start as injectable calls.

| Test | Coverage |
|------|----------|
| `test_playlist_arg_collection` | Multiple file args produce an ordered `playlist[]` (not just the last). |
| `test_playlist_edge_detect` | playing→stopped transition fires exactly once; re-arm prevents double-fire. |
| `test_playlist_advance_skip` | Failed load skips to next; all-failed deactivates. |
| `test_playlist_repeat_modes` | none stops at end; all wraps; one replays. |
| `test_playlist_time_cap` | Forced advance when current time ≥ cap. |
| `test_playlist_manual_stop` | User stop does not auto-advance. |

End-of-song detection itself is already exercised by the disko render path; the playlist tests should mock or drive the mode, not real audio.

### Manual

| Scenario | Expect |
|----------|--------|
| `schismtracker -p a.it b.it c.it` | Plays a, then b, then c, then stops (repeat-none). |
| Finite song ends naturally | Advances promptly at end. |
| Infinite `.mod`, cap = 60s | Advances at ~60s. |
| Bad/missing file in middle | Logged warning; skips to next. |
| MIDI/OPL module → next track | No hanging notes across the handoff. |
| Manual F8 stop mid-track | Stays stopped; no auto-advance. |
| Next / Prev keybind (when implemented) | Jumps immediately, clean stop first. |
| Now-playing status (when implemented) | Shows correct index/total + filename. |
| Single file, no flag | Loops forever as before (regression check). |
| Track-change hook (when implemented) | Hook runs with the new path on each advance. |

---

## 11. Suggested MVP path

1. **Multi-file args + auto-advance** (~50 lines, no UI): collect all CLI file args into `playlist[]`; add the edge-detector poll in `event_loop()`; advance via clean-stop + `song_load_unchecked` + `song_start_once`. Resolve the manual-stop-vs-natural-end distinction (§9).
2. **Per-track time cap** (`--max-time`): hybrid natural-end OR timeout, default-on with a generous default, since infinite modules are the common case (§7).
3. **Then decide front-end**: in-app playlist page (B), `.m3u`/directory args (C), or Lua hook (D) — all additive over the working engine.

---

## 12. Implementation status (phased plan — nothing done)

| Phase | Work | Status |
|-------|------|--------|
| 0 | Factor playlist state + advance logic into a small unit (testable). | Not started |
| 1 | `parse_options()` arg loop collects all file args into `playlist[]` (keep single-file behavior). | Not started |
| 1 | Edge-detector poll in `event_loop()` near the `song_get_mode()` switch (~line 845). | Not started |
| 1 | Clean handoff: `song_stop_unlocked` → `song_load_unchecked` → `song_start_once`; skip failed loads. | Not started |
| 1 | Distinguish natural end from user stop (§9 open point). | Not started |
| 2 | `--max-time` per-track cap (hybrid natural-end OR timeout) + usage/manpage. | Not started |
| 2 | Unit tests for arg collection, edge detect, skip, repeat modes, time cap, manual stop. | Not started |
| 3 | Repeat none/all/one + shuffle + stop-after-current. | Not started |
| 3 | Next / Prev keybinds + now-playing status surfacing. | Not started |
| 4 | Optional `track-change-hook` via `os_run_hook` (gated by `ENABLE_HOOKS`). | Not started |
| 4 | Front-end: in-app playlist page (B) and/or `.m3u`/directory args (C). | Not started |
| 5 | Lua playlist / `on_song_end` callback (depends on PR #312). | Not started |
| — | Headless playback jukebox (relax `--headless` ⇒ `--diskwrite`). | Out of scope / future |
| — | True gapless (double-buffered `song_t`). | Out of scope / future |

---

## 13. Key code references

**Arg collection (to change — currently keeps only the last file):**

```c
/* schism/main.c ~line 317, parse_options() */
} else {
    free(initial_song);
    initial_song = norm;   /* <-- overwrites; replace with append to playlist[] */
}
```

**Polling site (existing idle-pass mode switch — add playlist poll here):**

```c
/* schism/main.c ~line 845, event_loop() */
switch (song_get_mode()) {
case MODE_PLAYING:
case MODE_PATTERN_LOOP:
    ... screensaver off ...
    break;
default:
    ... screensaver on ...   /* MODE_STOPPED arrives here */
    break;
}
```

**End-of-song flag and detection (reuse, no change):**

- `SONG_ENDREACHED` — [`include/player/sndfile.h:213`](../include/player/sndfile.h)
- set in [`player/sndmix.c`](../player/sndmix.c) ~762 (`csf_read_note` == 0) and ~883/903 (`increment_order` at `ORDER_LAST`)
- `song_get_mode()` → `MODE_STOPPED` — [`schism/audio_playback.c:1007`](../schism/audio_playback.c)
- finite-end render proof — [`schism/disko.c`](../schism/disko.c) ~665 (`do { ... } while (!(dwsong.flags & SONG_ENDREACHED));`)

**Play-once vs loop:**

- `song_start_once()` (`repeat_count = -1`, `SNDMIX_NOBACKWARDJUMPS`) — [`schism/audio_playback.c`](../schism/audio_playback.c) ~836
- `song_start()` (loops) — [`schism/audio_playback.c`](../schism/audio_playback.c) ~852

**Clean handoff:**

- `song_stop_unlocked()` (MIDI all-notes-off, `OPL_Reset`, `GM_Reset`) — [`schism/audio_playback.c:884`](../schism/audio_playback.c)
- `song_load_unchecked()` — [`schism/audio_loadsave.c:282`](../schism/audio_loadsave.c)
- existing load+start callers: drag-drop [`schism/main.c:731`](../schism/main.c); load-module page [`schism/page_loadmodule.c:160`](../schism/page_loadmodule.c)

**Time cap source:**

- `song_get_current_time()` (`samples_played / mix_frequency`, seconds) — [`schism/audio_playback.c:1019`](../schism/audio_playback.c)

**Headless single-file render (would need relaxing for headless playback):**

- `--headless` requires `--diskwrite`, renders one file, exits — [`schism/main.c`](../schism/main.c) ~1297

**Hooks (for optional track-change-hook):**

- `run_startup_hook` / `run_exit_hook` / `run_disko_complete_hook` via `os_run_hook` — [`schism/main.c:99-110`](../schism/main.c)
