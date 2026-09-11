# Help Page Search (`/`, `n`, `N`)

Design reference for incremental text search on the in-app help screens, modeled on the `less` / `vim` convention: press `/` to type a query, jump to the next matching line (case-insensitive), and repeat forward/backward with `n` / `N`.

**Status:** ✅ **Implemented** (all phases) in [`schism/page_help.c`](../schism/page_help.c). No changes to the `helptext/` files or the `help_text[]` format, as planned.

> **Implementation diverged from this spec in two ways** — see the [retrospective](#10-implementation-retrospective) for detail:
> 1. The search prompt is **not** a text-entry dialog. It is drawn inline on the bottom row of the help viewport (`less(1)` style); `/` enters a search-entry mode and keystrokes are routed straight into the query buffer.
> 2. §4.3's "the simple version is sufficient" note about the trailing `\r\n` was **wrong and caused a real bug**. Bounding the search to the visible line length is **required**, not optional.

---

## Confirmed design decisions

| Topic | Decision |
|-------|----------|
| **Search entry key** | `/` enters an inline search-entry mode (`help_searching`). The prompt `/<query>` is drawn on the bottom row of the viewport and keystrokes route into `help_search[]` — no dialog, no widget. (Originally specced as a text-entry dialog; changed during implementation, see §10.) |
| **Repeat keys** | `n` = next match forward, `N` or `?` = previous match backward |
| **Case sensitivity** | Always case-insensitive (`strcasestr` on `line + 1`) |
| **Match comparison offset** | Compare from `line + 1` to skip the line-type marker (see Glossary) |
| **Skipped line types** | `LTYPE_SEPARATOR` (`%`) and `LTYPE_GRAPHIC` (`=`) lines are never matched |
| **Jump target** | `less(1)` style: scroll the matching line to the **top** row of the window: `top_line = CLAMP(match, 0, num_lines - 1)`. A match in the last 32 lines still rises to the top; `help_redraw()` blanks the rows past the end. (Note: this is a looser clamp than the arrow-key tail, which keeps `num_lines - 32`.) |
| **Position persistence** | Mirror the existing arrow-key tail: write `help_text_lastpos[status.current_help_index]` and set `NEED_UPDATE` |
| **Wrap-around** | `help_find()` wraps past the end/start of `lines[]` once, then reports "not found" |
| **Query lifetime** | Reset `help_search[]` on page switch (cleared in `help_set_page()`) |
| **Highlight** | Optional / nice-to-have; rendered in `help_redraw()`. Search works without it |
| **Input routing** | While `help_searching`, `help_handle_key()` (the page's `pre_handle_key`) consumes every key: printable `k->text` is appended, Backspace deletes, Enter runs the search, Esc cancels. Modal, like `less`. Relies on `k->text` being populated on keydowns (schism keeps SDL text input always on and pairs each keydown with its `TEXTINPUT`). |

---

## 1. Problem statement

The help page (`schism/page_help.c`) concatenates the page-local help text with the global-keys help: see `help_set_page()` at [`schism/page_help.c:221`](../schism/page_help.c#L221), which sums `local_lines + global_lines + 5` ([line 240](../schism/page_help.c#L240)) and appends `help_text[HELP_GLOBAL]` after the page-specific block ([lines 270-285](../schism/page_help.c#L270)). The combined text is frequently far longer than the 32-line visible window.

Today the only navigation is line and page scrolling. `help_handle_key()` ([line 150](../schism/page_help.c#L150)) handles `UP`, `DOWN`, `PAGEUP`, `PAGEDOWN`, `HOME`, `END`, and the mouse scroll wheel — all of which adjust a local `new_line` by a fixed delta. There is **no way to find a keybinding or term by name**; a user looking for, say, "transpose" or "Alt-F9" must eyeball-scroll the whole page.

This proposal adds find-as-you-type search reusing the existing dialog and scroll infrastructure, with no change to the help text data.

---

## 2. Goals and non-goals

### Goals

- Add `/` to open a search prompt on any help screen.
- Jump `top_line` to the first line at or after the current position whose visible text contains the query (case-insensitive).
- Repeat the search with `n` (forward) and `N` / `?` (backward), with wrap-around.
- Reuse the existing text-entry dialog (`widget_create_textentry` + `dialog_create_custom`) and the existing scroll/persist tail in `help_handle_key()`.
- Keep the feature self-contained in `schism/page_help.c`.

### Non-goals

- **No** changes to the `helptext/` files or the `extern const char *help_text[]` format ([line 93](../schism/page_help.c#L93)).
- **No** fuzzy / regex / multi-term search — plain case-insensitive substring only.
- **No** persistence of the query across page switches (intentionally reset per page).
- Match highlighting is **optional** (Phase C); the core feature must work without it.
- No new config options or save-format changes.

---

## 3. Glossary

| Term | Meaning |
|------|---------|
| **Line-type marker** | The first character of every line, an `LTYPE_*` enum value at [`schism/page_help.c:40`](../schism/page_help.c#L40) (`LTYPE_NORMAL '|'`, `LTYPE_BIOS '+'`, `LTYPE_SCHISM ':'`, `LTYPE_SEPARATOR '%'`, `LTYPE_DISABLED '#'`, `LTYPE_GRAPHIC '='`, etc.). The visible text starts at `line + 1`. |
| **`lines`** | `static const char **lines` ([line 82](../schism/page_help.c#L82)) — array of pointers to the start of each displayed line for the current page/mode. |
| **`num_lines`** | `static int num_lines` ([line 84](../schism/page_help.c#L84)) — count of entries in `lines[]`. |
| **`top_line`** | `static int top_line` ([line 85](../schism/page_help.c#L85)) — index into `lines[]` of the first visible row; the scroll position. |
| **32-line window** | The visible viewport: `help_redraw()` draws rows `pos = 13 .. 44` (`for (pos = 13, n = top_line; pos < 45; ...)` at [line 115](../schism/page_help.c#L115)), i.e. 32 lines. The scroll clamp is `num_lines - 32` ([line 209](../schism/page_help.c#L209)). |
| **`help_text_lastpos`** | `static int help_text_lastpos[HELP_NUM_ITEMS]` ([line 90](../schism/page_help.c#L90)) — remembered `top_line` per help page; restored in `help_set_page()` at [line 228](../schism/page_help.c#L228). |
| **Drawable length** | `strcspn(*ptr + 1, "\015\012")` ([line 118](../schism/page_help.c#L118)) — number of bytes of a line before the trailing `\r`/`\n`. Lines are *not* NUL-terminated at the visible boundary; each runs until `\r`, `\n`, or `\0`. |

---

## 4. Mechanism

### 4.1 New module state

Add alongside the existing statics (near [line 90](../schism/page_help.c#L90)):

```c
static char help_search[64] = "";   /* current query; reset on page switch */
```

The query is stored in a fixed buffer so it survives across `n` / `N` presses within a page. It is cleared in `help_set_page()` (see §5.5).

### 4.2 Capturing the query (`/`) — ⚠️ SUPERSEDED

> **This dialog-based approach was not shipped.** The implementation draws an inline
> `less`-style prompt on the viewport's bottom row and routes keys via the page's
> `pre_handle_key`/`k->text` — no dialog, no widget. See the [status banner](#) at the top,
> the **Input routing** row in §1, and the retrospective (§10). The text below is kept for
> historical context only.

On `/`, open a one-line text-entry dialog over the help page. Because `help_handle_key()` early-returns `0` while a dialog is open ([line 154](../schism/page_help.c#L154): `if (status.dialog_type != DIALOG_NONE) return 0;`), the dialog owns all keystrokes until it closes, so no extra gating is needed.

```c
/* widget storage for the search dialog */
static struct widget help_search_widgets[1];

static void help_search_accept(void *data)
{
    /* called when the dialog's "yes" action fires (Enter) */
    dialog_destroy();
    if (help_search[0])
        help_do_search(top_line, +1);   /* search forward from current top */
}

static void help_open_search(void)
{
    help_search[0] = '\0';
    /* widget_create_textentry(w, x, y, width, next_up, next_down,
     *                         next_tab, changed_cb, text_buf, max_length) */
    widget_create_textentry(help_search_widgets + 0,
        /* x */ 17, /* y */ 26, /* width */ 47,
        /* next_up/down/tab */ 0, 0, 0,
        /* changed */ NULL,
        help_search, sizeof(help_search) - 1);

    /* dialog_create_custom(x, y, w, h, widgets, total, selected, draw_const, data) */
    struct dialog *d = dialog_create_custom(
        9, 25, 61, 5,
        help_search_widgets, 1, 0,
        help_search_draw_const, NULL);
    d->action_yes    = help_search_accept;   /* Enter  */
    d->action_cancel = NULL;                  /* ESC just closes the dialog */
    d->handle_key    = NULL;
}
```

`widget_create_textentry` is declared at [`include/widget.h:45`](../include/widget.h#L45); `dialog_create_custom` at [`include/dialog.h:80`](../include/dialog.h#L80). The simpler `dialog_create` ([`include/dialog.h:71`](../include/dialog.h#L71)) is for fixed yes/no/cancel boxes and does not host a text-entry widget, so the custom variant is required. `help_search_draw_const` just draws a label such as `"Search:"` next to the entry box; the entry widget itself is rendered by the dialog system.

ESC inside the dialog closes it (standard dialog behavior) and leaves `top_line` unchanged.

### 4.3 Finding a match (`help_find`)

A pure scan helper over `lines[]` with single wrap-around. Comparison uses `strcasestr` (used elsewhere at [`schism/main.c:1312`](../schism/main.c#L1312)) on the **visible span** of `line + 1` (skipping the type marker). `LTYPE_SEPARATOR` and `LTYPE_GRAPHIC` lines carry no searchable prose and are skipped.

> ⚠️ **Critical (cost us a bug):** `lines[]` entries are **not NUL-terminated at the line boundary** — each points into the big `help_text[]` blob and runs until the next `\r`/`\n`/`\0`. So `strcasestr(line + 1, query)` does **not** search one line; it searches *from there to the end of the entire page*. That makes every line before a downstream occurrence report a false match (`match == from` every time → the view creeps one line at a time and never jumps). The search **must** be bounded to `strcspn(line + 1, "\015\012")` bytes. The as-shipped code copies the visible span into a small stack buffer before calling `strcasestr`.

```c
/* Return index of the next line containing `query` (case-insensitive),
 * scanning from `start` in `dir` (+1 forward, -1 backward), wrapping once.
 * Returns -1 if no line matches. */
static int help_find(const char *query, int start, int dir)
{
    int i, n;

    if (!query || !query[0] || num_lines <= 0)
        return -1;

    for (i = 0, n = start; i < num_lines; i++, n += dir) {
        const char *line;
        char visible[128];
        int len;

        /* wrap into [0, num_lines) */
        if (n < 0)            n += num_lines;
        else if (n >= num_lines) n -= num_lines;

        line = lines[n];
        if (!line)
            continue;
        if (line[0] == LTYPE_SEPARATOR || line[0] == LTYPE_GRAPHIC)
            continue;

        /* Copy out ONLY the visible span: lines aren't NUL-terminated at the
         * line boundary, so an unbounded strcasestr would scan into the rest
         * of the page and match downstream occurrences. */
        len = (int)strcspn(line + 1, "\015\012");
        if (len > (int)sizeof(visible) - 1)
            len = sizeof(visible) - 1;
        memcpy(visible, line + 1, len);
        visible[len] = '\0';

        if (strcasestr(visible, query))
            return n;
    }
    return -1;
}
```

If a charset-aware comparison is later wanted, `charset_strcasestr` ([`schism/charset_stdlib.c:283`](../schism/charset_stdlib.c#L283), declared [`include/charset.h:127`](../include/charset.h#L127)) is a drop-in with explicit charsets; ASCII help text makes plain `strcasestr` adequate for Phase A.

### 4.4 Jump + repeat

`help_do_search` runs `help_find`, and on success applies the **exact same tail** as the arrow-key handler ([lines 209-214](../schism/page_help.c#L209)): clamp into the scroll range, store, persist, request a redraw.

```c
/* dir: +1 forward, -1 backward. `from` is the line to start scanning from. */
static void help_do_search(int from, int dir)
{
    int match = help_find(help_search, from, dir);

    if (match < 0) {
        status_text_flash("Search string not found: %s", help_search);
        return;   /* leave top_line where it is */
    }

    int new_line = CLAMP(match, 0, num_lines - 1);   /* match -> top row (less style) */
    if (new_line != top_line) {
        top_line = new_line;
        help_text_lastpos[status.current_help_index] = top_line;
        status.flags |= NEED_UPDATE;
    }
}
```

`status_text_flash()` is declared in [`include/log.h:58`](../include/log.h#L58).

Repeat search starting points (so that `n` advances past the current match instead of re-finding it):

- `/` accept: `help_do_search(top_line, +1)` — search from the current top.
- `n`: `help_do_search(top_line + 1, +1)` — forward, starting one line below the top.
- `N` / `?`: `help_do_search(top_line - 1, -1)` — backward, starting one line above the top.

`help_find`'s wrap-around handles `top_line + 1 == num_lines` and `top_line - 1 < 0`.

### 4.5 Wiring into `help_handle_key`

Add new `case`s to the existing `switch (k->sym)` ([line 164](../schism/page_help.c#L164)). These call the helpers above directly and `return 1` (they manage `top_line` themselves, so they must not fall through to the shared `new_line` tail).

```c
case SCHISM_KEYSYM_SLASH:
    if (k->state == KEY_RELEASE)
        return 1;
    help_open_search();
    return 1;
case SCHISM_KEYSYM_n:
    if (k->state == KEY_RELEASE)
        return 1;
    help_do_search(top_line + 1, +1);
    return 1;
case SCHISM_KEYSYM_N:           /* if 'N' and '?' arrive as distinct syms */
case SCHISM_KEYSYM_QUESTION:
    if (k->state == KEY_RELEASE)
        return 1;
    help_do_search(top_line - 1, -1);
    return 1;
```

(The exact `SCHISM_KEYSYM_*` spellings must be confirmed against `include/keyboard.h`; `n` vs shift-`N` is distinguished via `k->mod` if the keysym does not already encode case. `/` and `?` are the same physical key with/without shift.)

### 4.6 Optional highlight (Phase C)

`help_redraw()` ([line 104](../schism/page_help.c#L104)) currently draws each normal line with a single `draw_text_len` / `draw_text_bios_len` call at fg color 6 ([lines 120-122](../schism/page_help.c#L120)); signatures at [`include/vgamem.h:84-85`](../include/vgamem.h#L84). To highlight, when `help_search[0]` is set and the line contains a match, split the line into three draws: text before the match (color 6), the matched substring (a highlight color, e.g. inverted or color 3), and text after (color 6). The match offset comes from `strcasestr(line + 1, help_search) - (line + 1)`, clamped to the drawable length `strcspn(line + 1, "\015\012")`. Graphic and separator branches are unaffected. This is purely cosmetic and gated behind a non-empty `help_search`.

---

## 5. Edge cases

| Case | Behavior |
|------|----------|
| **Empty query** | `help_find` returns `-1` immediately; `/` accept with empty buffer is a no-op (no flash). |
| **Query longer than any line** | No match; `strcasestr` simply never succeeds → "not found". |
| **No match found** | `status_text_flash("Search string not found: ...")`; `top_line` unchanged. |
| **Wrap-around** | `help_find` scans exactly `num_lines` steps with index wrap, so a match earlier than the start point is found after wrapping; if none exists in the whole page it reports not found. |
| **Match near end** | The match always scrolls to the top row; rows below the end of `lines[]` render blank (`help_redraw()` skips `n >= num_lines`). `help_redraw()` must index by line number with a bounds guard — it must **not** walk a `const char **ptr` past `lines[num_lines]` (the `NULL` terminator), or it reads out of bounds and crashes. |
| **Type-marker offset** | All comparison and highlight offsets use `line + 1`; never compare from `line[0]`. |
| **Separator / graphic lines** | Skipped in `help_find` (`LTYPE_SEPARATOR`, `LTYPE_GRAPHIC`). The injected `blank_line` / `separator_line` sentinels ([lines 87-88](../schism/page_help.c#L87)) are likewise non-matching. |
| **Trailing `\r\n`** | Lines are not NUL-terminated at the visible boundary; matches before the terminator are valid (see §4.3). The query never contains a newline, so no cross-line false match. |
| **Classic vs normal mode** | `lines[]` differs between modes because hidden line types are filtered in `help_set_page()` ([lines 252-258, 273-279](../schism/page_help.c#L252)). Search always operates on the currently active `lines[]`/`num_lines` (`CURRENT_HELP_LINECACHE`), so results follow the active mode automatically. |
| **Page switch** | `help_set_page()` resets `help_search[0] = '\0'` so a stale query never highlights or repeats on a different page. `top_line` is independently restored from `help_text_lastpos`. |
| **Dialog open** | `help_handle_key()` returns `0` while a dialog is up ([line 154](../schism/page_help.c#L154)); `n`/`N`/`/` are inert until the prompt closes — correct, the entry box gets the keys. |
| **`num_lines < 32`** | `num_lines - 32` is negative; `CLAMP(x, 0, negative)` already occurs for arrow keys today, so behavior is unchanged (clamps to 0). |

---

## 6. UX / keybindings

| Key | Action |
|-----|--------|
| `/` | Open the search prompt (text-entry dialog). Type query, Enter to search. |
| `Enter` (in dialog) | Accept query, jump to first match forward from current position. |
| `ESC` (in dialog) | Cancel; close prompt, leave scroll position unchanged. |
| `n` | Repeat search forward (next match below current top). |
| `N` or `?` | Repeat search backward (previous match above current top). |

This matches the `less` / `vim` convention (`/` to search, `n`/`N` to repeat, `?` as the reverse-direction relative). Existing navigation (`UP`/`DOWN`/`PAGEUP`/`PAGEDOWN`/`HOME`/`END`/scroll wheel/`ESC`-to-close-page) is unchanged. A "not found" result surfaces via `status_text_flash`.

---

## 7. Test matrix (manual)

| Scenario | Setup | Expect |
|----------|-------|--------|
| **Search hit** | Open Pattern Editor help, `/`, type a term known to appear below the fold | Window scrolls so the matching line is at/near the top |
| **Search miss** | `/`, type a string not present | "Search string not found"; no scroll |
| **Wrap-around** | Search a term that appears only above the current `top_line` | Found after wrapping; window jumps up |
| **Repeat forward** | After a hit with multiple occurrences, press `n` repeatedly | Advances to each subsequent match, then wraps to the first |
| **Repeat backward** | Press `N` / `?` after a hit | Moves to the previous match, wraps to the last |
| **Classic mode** | Toggle classic mode, search a Schism-only line | That line is absent from `lines[]`; reports not found (or finds a different occurrence) |
| **Separator lines skipped** | Search a string that would only match a separator/graphic row | Not found; never lands on a `%`/`=` row |
| **Page switch resets** | Search on one page, switch help pages, press `n` | `n` does nothing (query cleared); old query not highlighted |
| **Empty query** | `/` then Enter with nothing typed | No-op, no flash, no scroll |
| **Short page** | A help page shorter than 32 lines | Search clamps to top 0; no crash |
| **Highlight (Phase C)** | After a hit | Matched substring drawn in highlight color; rest of line normal |

---

## 8. Implementation status

| Phase | Scope | Status |
|-------|-------|--------|
| **A — Query capture + forward jump** | `help_search[]` state; inline search-entry mode (`help_searching`) + bottom-row prompt; `help_find()` (bounded); `help_do_search()`; `/` case; reset in `help_set_page` | ✅ Done |
| **B — Repeat + wrap** | `n` forward and shift-`n`/`?` backward; wrap-around; "not found" flash | ✅ Done |
| **C — Highlight** | Split-draw the matched substring in `help_redraw()`, gated on non-empty `help_search` | ✅ Done |

All three phases shipped, entirely within `schism/page_help.c` (net ≈ +199 lines).

---

## 9. Key code references

All in [`schism/page_help.c`](../schism/page_help.c) unless noted. **Line numbers are from the pre-implementation spec and have since drifted — grep the symbol.**

| Anchor | Relevance |
|--------|-----------|
| `LTYPE_*` enum | Type markers; `LTYPE_SEPARATOR`/`LTYPE_GRAPHIC` are skipped in search |
| `lines`, `num_lines`, `top_line` | Search target array + scroll state |
| `help_search[]`, `help_searching` | Query buffer + inline search-entry mode flag |
| `help_text_lastpos` | Per-page scroll persistence mirrored by `help_do_search` |
| `help_redraw()` | Bounds-guarded draw loop (indexes by line number, blanks `n >= num_lines`); highlight split-draw; bottom-row search prompt |
| `help_find()` | Bounded substring scan (copies the visible span — see §4.3 warning) |
| `help_do_search()` | Runs `help_find`, scrolls match to top (`CLAMP(match, 0, num_lines - 1)`), persists, `NEED_UPDATE` |
| `help_search_key()` | Routes keystrokes while `help_searching` (append `k->text` / Backspace / Enter / Esc) |
| `help_handle_key()` | Page `pre_handle_key`; `switch (k->sym)` with `/`, `n`, `?` cases; delegates to `help_search_key` when searching |
| `help_set_page()` | Builds `lines[]`; restores `top_line`; resets `help_search`/`help_searching` |
| `extern const char *help_text[]` | Help data format — unchanged |
| `strcasestr` | Case-insensitive substring matcher ([`schism/main.c:1312`](../schism/main.c#L1312)); resolves via `headers.h` |
| `status_text_flash` | "Not found" feedback ([`include/log.h`](../include/log.h)) |
| `draw_text_len` / `draw_text_bios_len` | Highlight + prompt draws ([`include/vgamem.h`](../include/vgamem.h)) |
| `pending_keydown` / `StartTextInput` | Why `k->text` is reliable in `handle_key` ([`sys/sdl2/events.c`](../sys/sdl2/events.c), [`sys/sdl2/video.c`](../sys/sdl2/video.c)) |

---

## 10. Implementation retrospective

Footprint: **≈ +199 net lines, all in `schism/page_help.c`** (0 other source files), matching the "self-contained" goal. What actually happened vs. this spec:

### Where the spec held
The keysym wiring, `n`/`N`/`?` repeat + wrap, reset-on-page-switch, and the highlight all landed as written and worked on the first build. One pre-flight correction was needed and caught before coding: **there is no `SCHISM_KEYSYM_N`** — letter case lives in `k->mod`, so backward search is `?` or shift-`n` (`k->mod & SCHISM_KEYMOD_SHIFT`), not a separate uppercase keysym.

### Where it diverged (and why)

1. **The find bug — the spec's own "good enough" was wrong.** §4.3 originally said bounding the search to the visible line was an optional "stricter variant." In reality `lines[]` runs into the rest of the `help_text[]` blob, so the unbounded `strcasestr` matched any line sitting above a downstream occurrence — every `n` advanced exactly one line and never reached the real hit. Root-caused with a hypothesis-logging pass (the empty line "matching" the query was the tell). **Lesson: the line buffers are not line-terminated; never treat `line + 1` as a single line's worth of text.** Fixed by copying the visible span first.

2. **Scroll model vs. renderer.** Moving to true `less` "match → top row" lets `top_line` exceed `num_lines - 32`. The original spec's `help_redraw()` walked a `const char **ptr` across `lines[]` and would have run off the `NULL` terminator and crashed. The loop was rewritten to index by line number with an `n >= num_lines` bounds guard, blanking rows past the end. The arrow-key clamp stays at `num_lines - 32`; only search uses the looser `num_lines - 1`.

3. **The prompt UI was the long pole, not the search logic.** It went through four shapes before landing: centered text-entry dialog → bottom-left dialog → full-width bottom bar → **inline `less`-style prompt drawn on the viewport's bottom row** (`HELP_PROMPT_ROW`). The dialog system always draws a box (no borderless option), which is why the dialog approach was abandoned entirely. The final version is modeled on how `less` actually works (a mode flag + a command buffer rendered on the bottom line, keystrokes routed by a small state machine), so it needs **no dialog and no widget**. This discarded the spec's entire `dialog_create_custom` + `widget_create_textentry` input path. It works because schism keeps SDL text input on permanently and pairs each keydown with its `TEXTINPUT`, so `k->text` is populated in `handle_key` and the `/` that opened search (consumed by its own case) never lands in the buffer.

### If reused elsewhere
- Any search/scan over `help_text[]`-style data must bound to `strcspn(p, "\015\012")` — assume buffers run past the visible line.
- For a bottom-line prompt that looks like a terminal pager, draw it yourself and route keys via the page's `pre_handle_key` / `k->text`; don't reach for `dialog_create_custom` (it always frames a box).
