/* The all-important pattern editor. The code here is a general mess, so
 * don't look at it directly or, uh, you'll go blind or something. */

#include "headers.h"

#include <SDL.h>
#include <ctype.h>

#include "it.h"
#include "page.h"
#include "song.h"
#include "pattern-view.h"

#define DEFAULT_VIEW_SCHEME 2

/* --------------------------------------------------------------------- */
/* The (way too many) static variables */

/* only one item, but MAN is it complicated :) */
static struct item items_pattern[1];

static int first_channel = 1;   /* one-based */
static int first_row = 0;       /* zero-based */

/* these three tell where the cursor is in the pattern */
static int current_channel = 1, current_position = 0, current_row = 0;

/* this is, of course, what the current pattern is */
static int current_pattern = 0;

static int skip_value = 1;      /* aka cursor step */

static int link_effect_column = 0;
static int draw_divisions = 0;  /* = vertical lines between channels */

static int centralise_cursor = 0;
static int highlight_current_row = 0;
static int playback_tracing = 0;        /* scroll lock */

static int panning_mode = 0;    /* for the volume column */

/* Argh!!!! Modplug doesn't even READ the stored values for these in the
 * file header!@*$^% */
static int row_highlight[2] = { 16, 4 };        /* 0 = major, 1 = minor */

/* these should fix the playback tracing position discrepancy */
static int playing_row = -1;
static int playing_pattern = -1;

/* the current editing mask (what stuff is copied) */
enum {
        MASK_INSTRUMENT = (1),
        MASK_VOLUME = (2),
        MASK_EFFECT = (4),
};
static int mask_fields = MASK_INSTRUMENT | MASK_VOLUME;

/* and the mask note. note that the instrument field actually isn't used */
static song_note mask_note = { 61, 0, 0, 0, 0, 0 };     /* C-5 */

/* playback mark (ctrl-f7) */
static int marked_pattern = -1, marked_row;

/* --------------------------------------------------------------------- */
/* block selection and clipboard handling */

/* *INDENT-OFF* */
static struct {
        int first_channel;
        int last_channel;
        int first_row;
        int last_row;
} selection = { 0, 0, 0, 0 };

static struct {
        int in_progress;
        int first_channel;
        int first_row;
} shift_selection = { 0, 0, 0 };

static struct {
        song_note *data;
        int channels;
        int rows;
} clipboard = { NULL, 0, 0 };
/* *INDENT-ON* */

/* set to 1 if the last movement key was shifted */
int previous_shift = 0;

/* this is set to 1 on the first alt-d selection,
 * and shifted left on each successive press. */
static int block_double_size;

/* if first_channel is zero, there's no selection, as the channel
 * numbers start with one. (same deal for last_channel, but i'm only
 * caring about one of them to be efficient.) */
#define SELECTION_EXISTS (selection.first_channel)

/* --------------------------------------------------------------------- */
/* this is for the multiple track views stuff. */

struct track_view {
        int width;
        draw_channel_header_func draw_channel_header;
        draw_note_func draw_note;
};

static const struct track_view track_views[] = {
#define TRACK_VIEW(n) {n, draw_channel_header_##n, draw_note_##n}
        TRACK_VIEW(13), /* 5 channels */
        TRACK_VIEW(10), /* 6/7 channels */
        TRACK_VIEW(7),  /* 9/10 channels */
        TRACK_VIEW(6),  /* 10/12 channels */
        TRACK_VIEW(3),  /* 18/24 channels */
        TRACK_VIEW(2),  /* 24/36 channels */
        TRACK_VIEW(1),  /* 36/64 channels */
#undef  TRACK_VIEW
};

#define NUM_TRACK_VIEWS ARRAY_SIZE(track_views)

static byte track_view_scheme[64];
static int visible_channels, visible_width;

/* --------------------------------------------------------------------- */
/* selection handling functions */

static inline int is_in_selection(int chan, int row)
{
        return (SELECTION_EXISTS
                && chan >= selection.first_channel
                && chan <= selection.last_channel
                && row >= selection.first_row
                && row <= selection.last_row);
}

static void normalize_block_selection(void)
{
        int n;

        if (!SELECTION_EXISTS)
                return;

        if (selection.first_channel > selection.last_channel) {
                n = selection.first_channel;
                selection.first_channel = selection.last_channel;
                selection.last_channel = n;
        }

        if (selection.first_row > selection.last_row) {
                n = selection.first_row;
                selection.first_row = selection.last_row;
                selection.last_row = n;
        }
}

static void shift_selection_begin(void)
{
        shift_selection.in_progress = 1;
        shift_selection.first_channel = current_channel;
        shift_selection.first_row = current_row;
}

static void shift_selection_update(void)
{
        if (shift_selection.in_progress) {
                selection.first_channel = shift_selection.first_channel;
                selection.last_channel = current_channel;
                selection.first_row = shift_selection.first_row;
                selection.last_row = current_row;
                normalize_block_selection();
        }
}

static void shift_selection_end(void)
{
        shift_selection.in_progress = 0;
}

static void selection_clear(void)
{
        selection.first_channel = 0;
}

static void selection_erase(void)
{
        song_note *pattern, *note;
        int row;
        int chan_width;

        if (!SELECTION_EXISTS)
                return;

        song_get_pattern(current_pattern, &pattern);

        if (selection.first_channel == 1 && selection.last_channel == 64) {
                memset(pattern + 64 * selection.first_row, 0,
                       (selection.last_row - selection.first_row + 1)
                       * 64 * sizeof(song_note));
        } else {
                chan_width = selection.last_channel
                        - selection.first_channel + 1;
                for (row = selection.first_row;
                     row <= selection.last_row; row++) {
                        note = pattern + 64 * row
                                + selection.first_channel - 1;
                        memset(note, 0, chan_width * sizeof(song_note));
                }
        }
}

static void clipboard_free(void)
{
        if (clipboard.data) {
                free(clipboard.data);
                clipboard.data = NULL;
        }
}

/* clipboard_copy is fundementally the same as selection_erase
 * except it uses memcpy instead of memset :) */
static void clipboard_copy(void)
{
        song_note *pattern;
        int row;

        if (!SELECTION_EXISTS)
                return;

        clipboard_free();

        clipboard.channels = selection.last_channel
                - selection.first_channel + 1;
        clipboard.rows = selection.last_row - selection.first_row + 1;

        clipboard.data = calloc(clipboard.channels * clipboard.rows,
                                sizeof(song_note));

        song_get_pattern(current_pattern, &pattern);

        if (selection.first_channel == 1 && selection.last_channel == 64) {
                memcpy(clipboard.data,
                       pattern + 64 * selection.first_row,
                       (selection.last_row - selection.first_row + 1)
                       * 64 * sizeof(song_note));
        } else {
                for (row = 0; row < clipboard.rows; row++) {
                        memcpy(clipboard.data + clipboard.channels * row,
                               pattern + 64 * (row + selection.first_row)
                               + selection.first_channel - 1,
                               clipboard.channels * sizeof(song_note));
                }
        }
}

static void clipboard_paste_overwrite(void)
{
        song_note *pattern;
        int row, num_rows, chan_width;

        if (clipboard.data == NULL) {
                dialog_create(DIALOG_OK, "No data in clipboard",
                              NULL, NULL, 0);
                return;
        }

        num_rows = song_get_pattern(current_pattern, &pattern);
        num_rows -= current_row;
        if (clipboard.rows < num_rows)
                num_rows = clipboard.rows;

        chan_width = clipboard.channels;
        if (chan_width + current_channel > 64)
                chan_width = 64 - current_channel + 1;

        for (row = 0; row < num_rows; row++) {
                memcpy(pattern + 64 * (current_row + row)
                       + current_channel - 1,
                       clipboard.data + clipboard.channels * row,
                       chan_width * sizeof(song_note));
        }
}

/* --------------------------------------------------------------------- */

static void pattern_editor_reposition(void)
{
        int total_rows = song_get_rows_in_pattern(current_pattern);

        if (current_channel < first_channel)
                first_channel = current_channel;
        else if (current_channel >= first_channel + visible_channels)
                first_channel = current_channel - visible_channels + 1;

        if (centralise_cursor) {
                if (current_row <= 16)
                        first_row = 0;
                else if (current_row + 15 > total_rows)
                        first_row = total_rows - 31;
                else
                        first_row = current_row - 16;
        } else {
                /* This could be written better. */
                if (current_row < first_row)
                        first_row = current_row;
                else if (current_row > first_row + 31)
                        first_row = current_row - 31;
                if (first_row + 31 > total_rows)
                        first_row = total_rows - 31;
        }
}

static void advance_cursor(void)
{
        int total_rows = song_get_rows_in_pattern(current_pattern);

        if (skip_value) {
                if (current_row + skip_value > total_rows)
                        return;
                current_row += skip_value;
        } else {
                if (current_channel < 64) {
                        current_channel++;
                } else {
                        current_channel = 1;
                        if (current_row < total_rows)
                                current_row++;
                }
        }
        pattern_editor_reposition();
}

/* --------------------------------------------------------------------- */

void update_current_row(void)
{
        byte buf[4];

        draw_text(numtostr_3(current_row, buf), 12, 7, 5, 0);
        draw_text(numtostr_3(song_get_rows_in_pattern
                             (current_pattern), buf), 16, 7, 5, 0);
}

int get_current_row(void)
{
        return current_row;
}

void set_current_row(int row)
{
        int total_rows = song_get_rows_in_pattern(current_pattern);

        current_row = CLAMP(row, 0, total_rows);
        pattern_editor_reposition();
        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

void update_current_pattern(void)
{
        byte buf[4];

        draw_text(numtostr_3(current_pattern, buf), 12, 6, 5, 0);
        draw_text(numtostr_3(song_get_num_patterns(), buf), 16, 6, 5, 0);
}

int get_current_pattern(void)
{
        return current_pattern;
}

void set_current_pattern(int n)
{
        int total_rows;

        current_pattern = CLAMP(n, 0, 199);
        total_rows = song_get_rows_in_pattern(current_pattern);

        if (current_row > total_rows)
                current_row = total_rows;

        if (SELECTION_EXISTS) {
                if (selection.first_row > total_rows) {
                        selection.first_row
                                = selection.last_row = total_rows;
                } else if (selection.last_row > total_rows) {
                        selection.last_row = total_rows;
                }
        }

        pattern_editor_reposition();

        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static void set_playback_mark(void)
{
        if (marked_pattern == current_pattern && marked_row == current_row) {
                marked_pattern = -1;
        } else {
                marked_pattern = current_pattern;
                marked_row = current_row;
        }
}

void play_song_from_mark(void)
{
        if (marked_pattern == -1)
                song_start_at_pattern(current_pattern, current_row);
        else
                song_start_at_pattern(marked_pattern, marked_row);
}

/* --------------------------------------------------------------------- */

static void recalculate_visible_area(void)
{
        int n, new_width;

        visible_width = 0;
        for (n = 0; n < 64; n++) {
                new_width = visible_width
                        + track_views[track_view_scheme[n]].width;

                if (new_width > 72)
                        break;
                visible_width = new_width;
                if (draw_divisions)
                        visible_width++;
        }

        if (draw_divisions) {
                /* a division after the last channel
                 * would look pretty dopey :) */
                visible_width--;
        }
        visible_channels = n;
}

static void set_view_scheme(int scheme)
{
        if (scheme >= NUM_TRACK_VIEWS) {
                /* shouldn't happen */
                log_appendf(4, "View scheme %d out of range"
                            " -- using default scheme", scheme);
                scheme = 0;
        }
        memset(track_view_scheme, scheme, 64);
        recalculate_visible_area();
}

/* --------------------------------------------------------------------- */

static void pattern_editor_redraw(void)
{
        int chan, chan_pos, chan_drawpos = 5;
        int row, row_pos;
        byte buf[4];
        song_note *pattern, *note;
        const struct track_view *track_view;
        int total_rows;
        int fg, bg;
        int pattern_is_playing = (song_get_mode() != MODE_STOPPED
                                  && current_pattern == playing_pattern);

        SDL_LockSurface(screen);

        /* draw the outer box around the whole thing */
        draw_box_unlocked(4, 14, 5 + visible_width, 47,
                          BOX_THICK | BOX_INNER | BOX_INSET);

        /* how many rows are there? */
        total_rows = song_get_pattern(current_pattern, &pattern);

        for (chan = first_channel, chan_pos = 0;
             chan_pos < visible_channels; chan++, chan_pos++) {
                track_view = track_views + track_view_scheme[chan_pos];
                track_view->draw_channel_header
                        (chan, chan_drawpos, 14,
                         ((song_get_channel(chan - 1)->flags & CHN_MUTE)
                          ? 0 : 3));

                note = pattern + 64 * first_row + chan - 1;
                for (row = first_row, row_pos = 0; row_pos < 32;
                     row++, row_pos++) {
                        if (chan_pos == 0) {
                                fg = pattern_is_playing
                                        && row == playing_row ? 3 : 0;
                                bg = (current_pattern == marked_pattern
                                      && row == marked_row) ? 11 : 2;
                                draw_text_unlocked(numtostr_3(row, buf), 1,
                                                   15 + row_pos, fg, bg);
                        }

                        if (is_in_selection(chan, row)) {
                                fg = 3;
                                bg = (row % row_highlight[0] == 0
                                      || row % row_highlight[1] == 0)
                                        ? 9 : 8;
                        } else {
                                fg = 6;
                                if (highlight_current_row
                                    && row == current_row)
                                        bg = 1;
                                else if (row % row_highlight[0] == 0)
                                        bg = 14;
                                else if (row % row_highlight[1] == 0)
                                        bg = 15;
                                else
                                        bg = 0;
                        }

                        track_view->draw_note(chan_drawpos, 15 + row_pos,
                                              note,
                                              ((row == current_row
                                                && chan == current_channel)
                                               ? current_position : -1),
                                              fg, bg);

                        if (draw_divisions
                            && chan_pos < visible_channels - 1) {
                                if (is_in_selection(chan, row))
                                        bg = 0;
                                draw_char_unlocked(168,
                                                   chan_drawpos +
                                                   track_view->width,
                                                   15 + row_pos, 2, bg);
                        }

                        /* next row, same channel */
                        note += 64;
                }

                chan_drawpos += track_view->width + !!draw_divisions;
        }

        SDL_UnlockSurface(screen);

        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* weird, the code to insert/delete a channel is actually more complicated
 * than the code to insert/delete an entire row. */

static void channel_insert_row(int channel)
{
        song_note *pattern;
        int row, total_rows;

        total_rows = song_get_pattern(current_pattern, &pattern);
        channel--;      /* so it's zero based */
        for (row = total_rows - 2; row >= current_row; row--)
                pattern[64 * (row + 1) + channel] =
                        pattern[64 * row + channel];
        pattern[64 * current_row + channel] = empty_note;
}

static void channel_delete_row(int channel)
{
        song_note *pattern;
        int row, total_rows;

        total_rows = song_get_pattern(current_pattern, &pattern);
        channel--;
        for (row = current_row; row <= total_rows - 2; row++)
                pattern[64 * row + channel] =
                        pattern[64 * (row + 1) + channel];
        pattern[64 * (total_rows - 1) + channel] = empty_note;
}

static void pattern_insert_row(void)
{
        song_note *pattern;
        int total_rows;

        total_rows = song_get_pattern(current_pattern, &pattern);

        /* shift the pattern down */
        memmove(pattern + 64 * (current_row + 1),
                pattern + 64 * current_row,
                64 * sizeof(song_note) * (total_rows - current_row - 1));
        /* and clear the current row */
        memset(pattern + 64 * current_row, 0, 64 * sizeof(song_note));
}

static void pattern_delete_row(void)
{
        song_note *pattern;
        int total_rows;

        total_rows = song_get_pattern(current_pattern, &pattern);

        /* shift the pattern up */
        memmove(pattern + 64 * current_row,
                pattern + 64 * (current_row + 1),
                64 * sizeof(song_note) * (total_rows - current_row - 1));
        /* and clear the bottom row */
        memset(pattern + 64 * (total_rows - 1), 0, 64 * sizeof(song_note));
}

/* --------------------------------------------------------------------- */

static void raise_notes_by_semitone(void)
{
        int row, chan;
        song_note *pattern, *note;

        song_get_pattern(current_pattern, &pattern);

        if (SELECTION_EXISTS) {
                for (chan = selection.first_channel;
                     chan <= selection.last_channel; chan++) {
                        note = pattern + 64 * selection.first_row
                                + chan - 1;
                        for (row = selection.first_row;
                             row <= selection.last_row; row++) {
                                if (note->note > 0 && note->note < 120)
                                        note->note++;
                                note += 64;
                        }
                }
        } else {
                note = pattern + 64 * current_row + current_channel - 1;
                if (note->note > 0 && note->note < 120)
                        note->note++;
        }
}

static void lower_notes_by_semitone(void)
{
        int row, chan;
        song_note *pattern, *note;

        song_get_pattern(current_pattern, &pattern);

        if (SELECTION_EXISTS) {
                for (chan = selection.first_channel;
                     chan <= selection.last_channel; chan++) {
                        note = pattern + 64 * selection.first_row
                                + chan - 1;
                        for (row = selection.first_row;
                             row <= selection.last_row; row++) {
                                if (note->note > 1 && note->note < 121)
                                        note->note--;
                                note += 64;
                        }
                }
        } else {
                note = pattern + 64 * current_row + current_channel - 1;
                if (note->note > 1 && note->note < 121)
                        note->note--;
        }
}

/* --------------------------------------------------------------------- */

static void copy_note_to_mask(void)
{
        int n;
        song_note *pattern, *cur_note;

        song_get_pattern(current_pattern, &pattern);
        cur_note = pattern + 64 * current_row + current_channel - 1;

        mask_note = *cur_note;

        n = cur_note->instrument;
        if (n) {
                if (song_is_instrument_mode())
                        instrument_set(n);
                else
                        sample_set(n);
        }
}

/* --------------------------------------------------------------------- */

/* input = '3', 'a', 'F', etc.
 * output = 3, 10, 15, etc. */
static inline int char_to_hex(char c)
{
        switch (c) {
        case '0'...'9':
                return c - '0';
        case 'a'...'f':
                c ^= 32;
                /* fall through */
        case 'A'...'F':
                return c - 'A' + 10;
        default:
                return -1;
        }
}

/* pos is either 0 or 1 (0 being the left digit, 1 being the right)
 * return: 1 (move cursor) or 0 (don't)
 * this is highly modplug specific :P */
static int handle_volume(song_note * note, char c, int pos)
{
        int vol = note->volume;
        int fx = note->volume_effect;
        int vp = panning_mode ? VOL_EFFECT_PANNING : VOL_EFFECT_VOLUME;
        byte vol_effects[8] = {
                VOL_EFFECT_FINEVOLUP, VOL_EFFECT_FINEVOLDOWN,
                VOL_EFFECT_VOLSLIDEUP, VOL_EFFECT_VOLSLIDEDOWN,
                VOL_EFFECT_PORTADOWN, VOL_EFFECT_PORTAUP,
                VOL_EFFECT_TONEPORTAMENTO, VOL_EFFECT_VIBRATOSPEED
        };

        if (pos == 0) {
                switch (c) {
                case '0'...'9':
                        vol = (c - '0') * 10 + vol % 10;
                        fx = vp;
                        break;
                case 'a'...'h':
                        c ^= 32;
                        /* fall through */
                case 'A'...'H':
                        fx = vol_effects[c - 'A'];
                        vol %= 10;
                        break;
                default:
                        return 0;
                }
        } else {
                switch (c) {
                case '0'...'9':
                        vol = (vol / 10) * 10 + (c - '0');
                        switch (fx) {
                        case VOL_EFFECT_NONE:
                        case VOL_EFFECT_VOLUME:
                        case VOL_EFFECT_PANNING:
                                fx = vp;
                        }
                        break;
                default:
                        return 0;
                }
        }

        note->volume_effect = fx;
        if (fx == VOL_EFFECT_VOLUME || fx == VOL_EFFECT_PANNING)
                note->volume = CLAMP(vol, 0, 64);
        else
                note->volume = CLAMP(vol, 0, 9);
        return 1;
}

/* return 1 => redraw */
static int pattern_editor_insert(char c)
{
        int total_rows;
        int n;
        song_note *pattern, *cur_note;

        total_rows = song_get_pattern(current_pattern, &pattern);
        cur_note = pattern + 64 * current_row + current_channel - 1;

        switch (current_position) {
        case 0:        /* note */
                /* TODO: rewrite this more logically */
                if (c == ' ') {
                        /* copy mask to note */
                        n = mask_note.note;
                        /* if n == 0, don't care */
                } else {
                        n = kbd_get_note(c);
                        if (n < 0)
                                return 0;
                        /* if n == 0, clear masked fields */
                }

                cur_note->note = n;

                /* mask stuff: if it's note cut/off/fade/clear, clear the
                 * masked fields; otherwise, copy from the mask note */
                if (n > 120 || (c != ' ' && n == 0)) {
                        /* note cut/off/fade = clear masked fields */
                        if (mask_fields & MASK_INSTRUMENT) {
                                cur_note->instrument = 0;
                        }
                        if (mask_fields & MASK_VOLUME) {
                                cur_note->volume_effect = 0;
                                cur_note->volume = 0;
                        }
                        if (mask_fields & MASK_EFFECT) {
                                cur_note->effect = 0;
                                cur_note->parameter = 0;
                        }
                } else {
                        /* copy the current sample/instrument
                         * -- UNLESS the note is empty */
                        if (mask_fields & MASK_INSTRUMENT) {
                                if (song_is_instrument_mode())
                                        cur_note->instrument =
                                                instrument_get_current();
                                else
                                        cur_note->instrument =
                                                sample_get_current();
                        }
                        if (mask_fields & MASK_VOLUME) {
                                cur_note->volume_effect =
                                        mask_note.volume_effect;
                                cur_note->volume = mask_note.volume;
                        }
                        if (mask_fields & MASK_EFFECT) {
                                cur_note->effect = mask_note.effect;
                                cur_note->parameter = mask_note.parameter;
                        }
                }

                /* copy the note back to the mask */
                mask_note.note = n;

                advance_cursor();
                break;
        case 1:        /* octave */
                if (c < '0' || c > '9')
                        return 0;
                n = cur_note->note;
                if (n > 0 && n <= 120) {
                        /* Hehe... this was originally 7 lines :) */
                        n = ((n - 1) % 12) + (12 * (c - '0')) + 1;
                        cur_note->note = n;
                }
                advance_cursor();
                break;
        case 2:        /* instrument, first digit */
        case 3:        /* instrument, second digit */
                if (c == ' ') {
                        if (song_is_instrument_mode())
                                cur_note->instrument =
                                        instrument_get_current();
                        else
                                cur_note->instrument =
                                        sample_get_current();
                        advance_cursor();
                        break;
                }
                if (c == note_trans[NOTE_TRANS_CLEAR]) {
                        cur_note->instrument = 0;
                        if (song_is_instrument_mode())
                                instrument_set(0);
                        else
                                sample_set(0);
                        advance_cursor();
                        break;
                }
                if (c < '0' || c > '9')
                        return 0;
                c -= '0';

                if (current_position == 2) {
                        n = (c * 10) + (cur_note->instrument % 10);
                        current_position++;
                } else {
                        n = ((cur_note->instrument / 10) * 10) + c;
                        current_position--;
                        advance_cursor();
                }
                cur_note->instrument = n;
                if (song_is_instrument_mode())
                        instrument_set(n);
                else
                        sample_set(n);
                break;
        case 4:
        case 5:        /* volume */
                if (c == ' ') {
                        cur_note->volume = mask_note.volume;
                        cur_note->volume_effect = mask_note.volume_effect;
                        advance_cursor();
                        break;
                }
                if (c == note_trans[NOTE_TRANS_CLEAR]) {
                        cur_note->volume = mask_note.volume = 0;
                        cur_note->volume_effect = mask_note.volume_effect =
                                VOL_EFFECT_NONE;
                        advance_cursor();
                        break;
                }
                if (c == note_trans[NOTE_TRANS_VOL_PAN_SWITCH]) {
                        panning_mode = !panning_mode;
                        status_text_flash("%s control set",
                                          (panning_mode
                                           ? "Panning" : "Volume"));
                        return 0;
                }
                if (!handle_volume(cur_note, c, current_position - 4))
                        return 0;
                mask_note.volume = cur_note->volume;
                mask_note.volume_effect = cur_note->volume_effect;
                if (current_position == 4) {
                        current_position++;
                } else {
                        current_position = 4;
                        advance_cursor();
                }
                break;
        case 6:        /* effect */
                if (c == ' ') {
                        cur_note->effect = mask_note.effect;
                } else {
                        n = get_effect_number(c);
                        if (n < 0)
                                return 0;
                        cur_note->effect = mask_note.effect = n;
                }
                if (link_effect_column)
                        current_position++;
                else
                        advance_cursor();
                break;
        case 7:        /* param, high nibble */
        case 8:        /* param, low nibble */
                if (c == ' ') {
                        cur_note->parameter = mask_note.parameter;
                        current_position = 6 + !link_effect_column;
                        advance_cursor();
                        break;
                } else if (c == note_trans[NOTE_TRANS_CLEAR]) {
                        cur_note->parameter = mask_note.parameter = 0;
                        current_position = 6 + !link_effect_column;
                        advance_cursor();
                        break;
                }

                /* FIXME: honey roasted peanuts */

                n = char_to_hex(c);
                if (n < 0)
                        return 0;
                if (current_position == 7) {
                        cur_note->parameter =
                                (n << 4) | (cur_note->parameter & 0xf);
                        current_position++;
                } else {
                        cur_note->parameter =
                                (cur_note->parameter & 0xf0) | n;
                        current_position = 6 + !link_effect_column;
                        advance_cursor();
                }
                mask_note.parameter = cur_note->parameter;
                break;
        }

        return 1;
}

/* --------------------------------------------------------------------- */
/* return values:
 * 1 = handled key completely. don't do anything else
 * -1 = partly done, but need to recalculate cursor stuff
 *         (for keys that move the cursor)
 * 0 = didn't handle the key. */

static int pattern_editor_handle_alt_key(SDL_keysym * k)
{
        int n;
        int total_rows = song_get_rows_in_pattern(current_pattern);

        switch (k->sym) {
        case '0'...'9':
                skip_value = k->sym - '0';
                status_text_flash("Cursor step set to %d", skip_value);
                return 1;
        case SDLK_b:
                if (!SELECTION_EXISTS) {
                        selection.last_channel = current_channel;
                        selection.last_row = current_row;
                }
                selection.first_channel = current_channel;
                selection.first_row = current_row;
                normalize_block_selection();
                break;
        case SDLK_e:
                if (!SELECTION_EXISTS) {
                        selection.first_channel = current_channel;
                        selection.first_row = current_row;
                }
                selection.last_channel = current_channel;
                selection.last_row = current_row;
                normalize_block_selection();
                break;
        case SDLK_d:
                if (status.last_keysym == SDLK_d) {
                        if (total_rows - current_row > block_double_size)
                                block_double_size <<= 1;
                } else {
                        block_double_size = row_highlight[0];
                        selection.first_channel = selection.last_channel =
                                current_channel;
                        selection.first_row = current_row;
                }
                n = block_double_size + current_row - 1;
                selection.last_row = MIN(n, total_rows);
                break;
        case SDLK_l:
                if (status.last_keysym == SDLK_l) {
                        /* 3x alt-l re-selects the current channel */
                        if (selection.first_channel ==
                            selection.last_channel) {
                                selection.first_channel = 1;
                                selection.last_channel = 64;
                        } else {
                                selection.first_channel =
                                        selection.last_channel =
                                        current_channel;
                        }
                } else {
                        selection.first_channel = selection.last_channel =
                                current_channel;
                        selection.first_row = 0;
                        selection.last_row = total_rows;
                }
                break;
        case SDLK_u:
                if (SELECTION_EXISTS) {
                        selection_clear();
                } else if (clipboard.data) {
                        clipboard_free();
                } else {
                        dialog_create(DIALOG_OK, "No data in clipboard",
                                      NULL, NULL, 0);
                }
                break;
        case SDLK_c:
                clipboard_copy();
                break;
        case SDLK_o:
                clipboard_paste_overwrite();
                break;
        case SDLK_p:
                printf("TODO: paste\n");
                break;
        case SDLK_m:
                if (status.last_keysym == SDLK_m) {
                        printf("TODO: mix fields\n");
                } else {
                        printf("TODO: mix notes\n");
                }
                break;
        case SDLK_z:
                clipboard_copy();
                selection_erase();
                break;
        case SDLK_h:
                draw_divisions = !draw_divisions;
                recalculate_visible_area();
                pattern_editor_reposition();
                break;
        case SDLK_q:
                raise_notes_by_semitone();
                break;
        case SDLK_a:
                lower_notes_by_semitone();
                break;
        case SDLK_UP:
                if (first_row > 0) {
                        first_row--;
                        if (current_row > first_row + 31)
                                current_row = first_row + 31;
                        return -1;
                }
                return 1;
        case SDLK_DOWN:
                if (first_row + 31 < total_rows) {
                        first_row++;
                        if (current_row < first_row)
                                current_row = first_row;
                        return -1;
                }
                return 1;
        case SDLK_LEFT:
                current_channel--;
                return -1;
        case SDLK_RIGHT:
                current_channel++;
                return -1;
        case SDLK_INSERT:
                pattern_insert_row();
                break;
        case SDLK_DELETE:
                pattern_delete_row();
                break;
        case SDLK_F9:
                song_toggle_channel_mute(current_channel - 1);
                orderpan_recheck_muted_channels();
                break;
        case SDLK_F10:
                song_handle_channel_solo(current_channel - 1);
                orderpan_recheck_muted_channels();
                break;
        default:
                return 0;
        }

        status.flags |= NEED_UPDATE;
        return 1;
}

static int pattern_editor_handle_ctrl_key(SDL_keysym * k)
{
        int n;
        int total_rows = song_get_rows_in_pattern(current_pattern);

        switch (k->sym) {
        case SDLK_F6:
                song_loop_pattern(current_pattern, current_row);
                return 1;
        case SDLK_F7:
                set_playback_mark();
                return -1;
        case SDLK_UP:
                set_previous_instrument();
                return 1;
        case SDLK_DOWN:
                set_next_instrument();
                return 1;
        case SDLK_PAGEUP:
                current_row = 0;
                return -1;
        case SDLK_PAGEDOWN:
                current_row = total_rows;
                return -1;
        case SDLK_HOME:
                current_row--;
                return -1;
        case SDLK_END:
                current_row++;
                return -1;
        case SDLK_KP_MINUS:
                prev_order_pattern();
                return 1;
        case SDLK_KP_PLUS:
                next_order_pattern();
                return 1;
        case '0'...'9':
                /* this is ctrl-shift in IT...
                 * as I haven't implemented multiple track views yet,
                 * I'm just using ctrl. */
                n = k->sym - '0';
                if (n >= 0 && n < NUM_TRACK_VIEWS) {
                        set_view_scheme(n);
                        pattern_editor_reposition();
                        status.flags |= NEED_UPDATE;
                        return 1;
                }
                return 0;
        case SDLK_c:
                centralise_cursor = !centralise_cursor;
                status_text_flash("Centralise cursor %s",
                                  (centralise_cursor ? "enabled" :
                                   "disabled"));
                return -1;
        case SDLK_h:
                highlight_current_row = !highlight_current_row;
                status_text_flash("Row hilight %s",
                                  (highlight_current_row ? "enabled" :
                                   "disabled"));
                status.flags |= NEED_UPDATE;
                return 1;
        default:
                return 0;
        }

        return 0;
}

static int pattern_editor_handle_key(SDL_keysym * k)
{
        int n;
        int total_rows = song_get_rows_in_pattern(current_pattern);
        char c;

        switch (k->sym) {
        case SDLK_UP:
                if (skip_value)
                        current_row -= skip_value;
                else
                        current_row--;
                return -1;
        case SDLK_DOWN:
                if (skip_value)
                        current_row += skip_value;
                else
                        current_row++;
                return -1;
        case SDLK_LEFT:
                if (k->mod & KMOD_SHIFT)
                        current_channel--;
                else
                        current_position--;
                return -1;
        case SDLK_RIGHT:
                if (k->mod & KMOD_SHIFT)
                        current_channel++;
                else
                        current_position++;
                return -1;
        case SDLK_TAB:
                if ((k->mod & KMOD_SHIFT) == 0)
                        current_channel++;
                else if (current_position == 0)
                        current_channel--;
                current_position = 0;

                /* hack to keep shift-tab from changing the selection */
                k->mod &= ~KMOD_SHIFT;
                shift_selection_end();
                previous_shift = 0;

                return -1;
        case SDLK_PAGEUP:
                if (current_row == total_rows)
                        current_row++;
                current_row -= row_highlight[0];
                return -1;
        case SDLK_PAGEDOWN:
                current_row += row_highlight[0];
                return -1;
        case SDLK_HOME:
                if (current_position == 0) {
                        if (current_channel == 1)
                                current_row = 0;
                        else
                                current_channel = 1;
                } else {
                        current_position = 0;
                }
                return -1;
        case SDLK_END:
                n = song_find_last_channel();
                if (current_position == 8) {
                        if (current_channel == n)
                                current_row = total_rows;
                        else
                                current_channel = n;
                } else {
                        current_position = 8;
                }
                return -1;
        case SDLK_INSERT:
                channel_insert_row(current_channel);
                break;
        case SDLK_DELETE:
                channel_delete_row(current_channel);
                break;
        case SDLK_KP_MINUS:
                if (k->mod & KMOD_SHIFT)
                        set_current_pattern(current_pattern - 4);
                else
                        set_current_pattern(current_pattern - 1);
                return 1;
        case SDLK_KP_PLUS:
                if (k->mod & KMOD_SHIFT)
                        set_current_pattern(current_pattern + 4);
                else
                        set_current_pattern(current_pattern + 1);
                return 1;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
                copy_note_to_mask();
                return 1;
        case SDLK_SCROLLOCK:
                playback_tracing = !playback_tracing;
                status_text_flash("Playback tracing %s",
                                  (playback_tracing ? "enabled" :
                                   "disabled"));
                return 1;
        default:
                c = unicode_to_ascii(k->unicode);
                if (c == 0)
                        return 0;

                if (isupper(c))
                        c = tolower(c);

                /* bleah */
                if (k->mod & KMOD_SHIFT) {
                        k->mod &= ~KMOD_SHIFT;
                        shift_selection_end();
                        previous_shift = 0;
                }

                if (c == note_trans[NOTE_TRANS_PREV_INS] || c == '<') {
                        set_previous_instrument();
                        return 1;
                } else if (c == note_trans[NOTE_TRANS_NEXT_INS]
                           || c == '>') {
                        set_next_instrument();
                        return 1;
                } else if (c == note_trans[NOTE_TRANS_TOGGLE_MASK]) {
                        switch (current_position) {
                        case 0:
                        case 1:
                                break;
                        case 2:
                        case 3:
                                mask_fields ^= MASK_INSTRUMENT;
                                break;
                        case 4:
                        case 5:
                                mask_fields ^= MASK_VOLUME;
                                break;
                        default:
                                mask_fields ^= MASK_EFFECT;
                                break;
                        }

                        /* FIXME | redraw the bottom part of the pattern
                         * FIXME | that shows the active mask bits */
                        return 1;
                } else {
                        if (!pattern_editor_insert(c))
                                return 0;
                }
                return -1;
        }

        status.flags |= NEED_UPDATE;
        return 1;
}

/* --------------------------------------------------------------------- */
/* this function name's a bit confusing, but this is just what gets
 * called from the main key handler.
 * pattern_editor_handle_*_key above do the actual work. */

static int pattern_editor_handle_key_cb(SDL_keysym * k)
{
        byte buf[4];
        int ret;
        int total_rows = song_get_rows_in_pattern(current_pattern);

        if (k->mod & KMOD_SHIFT) {
                if (!previous_shift)
                        shift_selection_begin();
                previous_shift = 1;
        } else if (previous_shift) {
                shift_selection_end();
                previous_shift = 0;
        }

        if (k->mod & KMOD_ALT)
                ret = pattern_editor_handle_alt_key(k);
        else if (k->mod & KMOD_CTRL)
                ret = pattern_editor_handle_ctrl_key(k);
        else
                ret = pattern_editor_handle_key(k);

        if (ret != -1)
                return ret;

        current_row = CLAMP(current_row, 0, total_rows);
        if (current_position > 8) {
                if (current_channel < 64) {
                        current_position = 0;
                        current_channel++;
                } else {
                        current_position = 8;
                }
        } else if (current_position < 0) {
                if (current_channel > 1) {
                        current_position = 8;
                        current_channel--;
                } else {
                        current_position = 0;
                }
        }

        current_channel = CLAMP(current_channel, 1, 64);
        pattern_editor_reposition();
        if (k->mod & KMOD_SHIFT)
                shift_selection_update();

        draw_text(numtostr_3(song_get_num_patterns(), buf), 16, 6, 5, 0);
        draw_text(numtostr_3(current_row, buf), 12, 7, 5, 0);

        status.flags |= NEED_UPDATE;
        return 1;
}

/* --------------------------------------------------------------------- */

static void pattern_editor_playback_update(void)
{
        static int prev_row = -1;
        static int prev_pattern = -1;

        playing_row = song_get_current_row();
        playing_pattern = song_get_current_pattern();

        if (song_get_mode() != MODE_STOPPED
            && (playing_row != prev_row
                || playing_pattern != prev_pattern)) {

                prev_row = playing_row;
                prev_pattern = playing_pattern;

                if (playback_tracing) {
                        set_current_order(song_get_current_order());
                        set_current_pattern(playing_pattern);
                        current_row = playing_row;
                        pattern_editor_reposition();
                        status.flags |= NEED_UPDATE;
                } else if (current_pattern == song_get_current_pattern()) {
                        status.flags |= NEED_UPDATE;
                }
        }
}

/* --------------------------------------------------------------------- */

void pattern_editor_load_page(struct page *page)
{
        page->title = "Pattern Editor (F2)";
        page->playback_update = pattern_editor_playback_update;
        page->total_items = 1;
        page->items = items_pattern;
        page->help_index = HELP_PATTERN_EDITOR;

        items_pattern[0].type = ITEM_OTHER;
        items_pattern[0].other.handle_key = pattern_editor_handle_key_cb;
        items_pattern[0].other.redraw = pattern_editor_redraw;

        set_view_scheme(DEFAULT_VIEW_SCHEME);
}
