#include "headers.h"

#include <SDL.h>

#include "it.h"
#include "song.h"
#include "page.h"

#include "pattern-view.h"

/* --------------------------------------------------------------------- */

static struct item items_info[1];

// nonzero => use velocity bars
static int velocity_mode = 0;

// nonzero => instrument names
static int instrument_names = 0;

/* --------------------------------------------------------------------- */
/* window setup */

struct info_window_type {
        void (*draw) (int base, int height, int active, int first_channel);

        /* if this is set, the first row contains actual text
         * (not just the top part of a box) */
        int first_row;

        /* how many channels are shown -- just use 0 for windows that
         * don't show specific channel info. for windows that put the
         * channels vertically (i.e. sample names) this should be the
         * amount to ADD to the height to get the number of channels, so
         * it should be NEGATIVE. (example: the sample name view uses
         * the first position for the top of the box and the last
         * position for the bottom, so it uses -2.) confusing, almost to
         * the point of being painful, but it works. (ok, i admit, it's
         * not the most brilliant idea i ever had ;) */
        int channels;
};

struct info_window {
        int type;
        int height;
        int first_channel;
};

static int selected_window = 0;
static int num_windows = 3;
static int selected_channel = 1;

/* five, because that's Impulse Tracker's maximum */
#define MAX_WINDOWS 5
static struct info_window windows[MAX_WINDOWS] = {
        {0, 19, 1},     // samples (18 channels displayed)
        {7, 3, 1},      // active channels
        {5, 15, 1},     // 24chn track view
};

/* --------------------------------------------------------------------- */
/* the various stuff that can be drawn... */

static void info_draw_samples(int base, int height, UNUSED int active,
                              int first_channel)
{
        int vu, smp, ins, n, pos, fg, c = first_channel;
        char buf[8];
        char *ptr;

        draw_fill_chars(5, base + 1, 28, base + height - 2, 0);
        draw_fill_chars(31, base + 1, 61, base + height - 2, 0);
        draw_fill_chars(64, base + 1, 72, base + height - 2, 0);

        SDL_LockSurface(screen);
        draw_box_unlocked(4, base, 29, base + height - 1,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(30, base, 62, base + height - 1,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(63, base, 73, base + height - 1,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        SDL_UnlockSurface(screen);

        /* FIXME: what about standalone sample playback? */
        if (song_get_mode() == MODE_STOPPED) {
                for (pos = base + 1; pos < base + height - 1; pos++, c++) {
                        song_channel *channel = song_get_channel(c - 1);

                        if (c == selected_channel) {
                                fg = (channel->flags & CHN_MUTE) ? 6 : 3;
                        } else {
                                if (channel->flags & CHN_MUTE)
                                        continue;
                                fg = active ? 1 : 0;
                        }
                        draw_text(numtostr_2(c, buf), 2, pos, fg, 2);
                }
                return;
        }

        for (pos = base + 1; pos < base + height - 1; pos++, c++) {
                song_mix_channel *channel = song_get_mix_channel(c - 1);

                /* first box: vu meter */
                if (velocity_mode)
                        vu = channel->final_volume >> 8;
                else
                        vu = channel->vu_meter >> 2;
                if (channel->flags & CHN_MUTE)
                        draw_vu_meter(5, pos, vu, 1, 2);
                else
                        draw_vu_meter(5, pos, vu, 5, 4);

                /* second box: sample number/name */
                /* not sure how to get the ins. number :( */
                ins = channel->instrument ? 42 : 0;
                /* figuring out the sample number is an ugly hack...
                 * considering all the crap that's copied to the channel,
                 * i'm surprised that the sample and instrument numbers
                 * aren't in there somewhere... */
                if (channel->sample && channel->sample_data)
                        smp = channel->sample - song_get_sample(0, NULL);
                else
                        smp = 0;
                if (smp) {
                        SDL_LockSurface(screen);
                        draw_text_unlocked(numtostr_2(smp, buf), 31, pos,
                                           6, 0);
                        if (ins) {
                                draw_char_unlocked('/', 33, pos, 6, 0);
                                draw_text_unlocked(numtostr_2(ins, buf),
                                                   34, pos, 6, 0);
                                n = 36;
                        } else {
                                n = 33;
                        }
                        if (channel->volume == 0)
                                fg = 4;
                        else if (channel->
                                 flags & (CHN_KEYOFF | CHN_NOTEFADE))
                                fg = 7;
                        else
                                fg = 6;
                        draw_char_unlocked(':', n++, pos, fg, 0);
                        SDL_UnlockSurface(screen);
                        if (instrument_names && channel->instrument)
                                ptr = channel->instrument->name;
                        else
                                song_get_sample(smp, &ptr);
                        draw_text_len(ptr, 25, n, pos, 6, 0);
                }

                /* last box: panning. this one's much easier than the
                 * other two, thankfully :) */
                if (channel->flags & CHN_MUTE) {
                        /* nothing... (this is wrong, really) */
                } else if (channel->flags & CHN_SURROUND) {
                        draw_text("Surround", 64, pos, 2, 0);
                } else if (channel->final_panning >> 2 == 0) {
                        draw_text("Left", 64, pos, 2, 0);
                } else if ((channel->final_panning + 3) >> 2 == 64) {
                        draw_text("Right", 68, pos, 2, 0);
                } else {
                        draw_thumb_bar(64, pos, 9, 0, 256,
                                       channel->final_panning, 0);
                }

                /* finally, do the channel number */
                if (c == selected_channel) {
                        fg = (channel->flags & CHN_MUTE) ? 6 : 3;
                } else {
                        if (channel->flags & CHN_MUTE)
                                continue;
                        fg = active ? 1 : 0;
                }
                draw_text(numtostr_2(c, buf), 2, pos, fg, 2);
        }
}

/* this function needs to be called with the screen locked! */
static void _draw_track_view(int base, int height, int first_channel,
                             int num_channels, int channel_width,
                             int separator, draw_note_func draw_note)
{
        /* way too many variables */
        int current_row = song_get_current_row();
        int current_order = song_get_current_order();
        unsigned char *orderlist = song_get_orderlist();
        song_note *note;
        song_note *cur_pattern, *prev_pattern, *next_pattern;
        song_note *pattern;     // points to either {cur,prev,next}_pattern
        int cur_pattern_rows, prev_pattern_rows, next_pattern_rows;
        int total_rows; // same as {cur,prev_next}_pattern_rows
        int chan_pos, row, row_pos, rows_before, rows_after;
        char buf[4];

        if (separator)
                channel_width++;

        switch (song_get_mode()) {
        case MODE_PATTERN_LOOP:
                prev_pattern_rows = next_pattern_rows = cur_pattern_rows
                        =
                        song_get_pattern(song_get_current_pattern(),
                                         &cur_pattern);
                prev_pattern = next_pattern = cur_pattern;
                break;
        case MODE_PLAYING:
                if (orderlist[current_order] >= 200) {
                        /* this does, in fact, happen. just pretend that
                         * it's stopped :P */
        default:
                        /* stopped (i hope!) */
                        /* TODO: fill the area with blank dots */
                        return;
                }
                cur_pattern_rows =
                        song_get_pattern(orderlist[current_order],
                                         &cur_pattern);
                if (current_order > 0
                    && orderlist[current_order - 1] < 200)
                        prev_pattern_rows =
                                song_get_pattern(orderlist
                                                 [current_order - 1],
                                                 &prev_pattern);
                else
                        prev_pattern = NULL;
                if (current_order < 255
                    && orderlist[current_order + 1] < 200)
                        next_pattern_rows =
                                song_get_pattern(orderlist
                                                 [current_order + 1],
                                                 &next_pattern);
                else
                        next_pattern = NULL;
                break;
        }

        rows_before = (height - 2) / 2;
        rows_after = rows_before;
        if (height & 1)
                rows_after++;

        /* draw the area above the current row */
        pattern = cur_pattern;
        total_rows = cur_pattern_rows;
        row = current_row - 1;
        row_pos = base + rows_before;
        while (row_pos > base) {
                if (row < 0) {
                        if (prev_pattern == NULL) {
                                /* TODO: fill it with blank dots */
                                break;
                        }
                        pattern = prev_pattern;
                        total_rows = prev_pattern_rows;
                        row = total_rows - 1;
                }
                draw_text_unlocked(numtostr_3(row, buf), 1, row_pos, 0, 2);
                note = pattern + 64 * row + first_channel - 1;
                for (chan_pos = 0; chan_pos < num_channels - 1; chan_pos++) {
                        draw_note(5 + channel_width * chan_pos, row_pos,
                                  note, -1, 6, 0);
                        if (separator)
                                draw_char_unlocked(168,
                                                   (4 +
                                                    channel_width *
                                                    (chan_pos + 1)),
                                                   row_pos, 2, 0);
                        note++;
                }
                draw_note(5 + channel_width * chan_pos, row_pos, note, -1,
                          6, 0);
                row--;
                row_pos--;
        }

        /* draw the current row */
        pattern = cur_pattern;
        total_rows = cur_pattern_rows;
        row_pos = base + rows_before + 1;
        draw_text_unlocked(numtostr_3(current_row, buf), 1, row_pos, 0, 2);
        note = pattern + 64 * current_row + first_channel - 1;
        for (chan_pos = 0; chan_pos < num_channels - 1; chan_pos++) {
                draw_note(5 + channel_width * chan_pos, row_pos, note, -1,
                          6, 14);
                if (separator)
                        draw_char_unlocked(168,
                                           (4 +
                                            channel_width * (chan_pos +
                                                             1)), row_pos,
                                           2, 14);
                note++;
        }
        draw_note(5 + channel_width * chan_pos, row_pos, note, -1, 6, 14);

        /* draw the area under the current row */
        row = current_row + 1;
        row_pos++;
        while (row_pos < base + height - 1) {
                if (row >= total_rows) {
                        if (next_pattern == NULL) {
                                /* TODO: fill it with blank dots */
                                break;
                        }
                        pattern = next_pattern;
                        total_rows = next_pattern_rows;
                        row = 0;
                }
                draw_text_unlocked(numtostr_3(row, buf), 1, row_pos, 0, 2);
                note = pattern + 64 * row + first_channel - 1;
                for (chan_pos = 0; chan_pos < num_channels - 1; chan_pos++) {
                        draw_note(5 + channel_width * chan_pos, row_pos,
                                  note, -1, 6, 0);
                        if (separator)
                                draw_char_unlocked(168,
                                                   (4 +
                                                    channel_width *
                                                    (chan_pos + 1)),
                                                   row_pos, 2, 0);
                        note++;
                }
                draw_note(5 + channel_width * chan_pos, row_pos, note, -1,
                          6, 0);
                row++;
                row_pos++;
        }
}

static void info_draw_track_5(int base, int height, int active,
                              int first_channel)
{
        int chan, chan_pos, fg;

        /* FIXME: once _draw_track_view draws the filler dots like it's
         * supposed to, get rid of the draw_fill_chars here */
        draw_fill_chars(5, base + 1, 73, base + height - 2, 0);
        SDL_LockSurface(screen);
        draw_box_unlocked(4, base, 74, base + height - 1,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        for (chan = first_channel, chan_pos = 0; chan_pos < 5;
             chan++, chan_pos++) {
                if (song_get_channel(chan - 1)->flags & CHN_MUTE)
                        fg = (chan == selected_channel ? 6 : 1);
                else
                        fg = (chan ==
                              selected_channel ? 3 : (active ? 2 : 0));
                draw_channel_header_13(chan, 5 + 14 * chan_pos, base, fg);
        }
        _draw_track_view(base, height, first_channel, 5, 13, 1,
                         draw_note_13);
        SDL_UnlockSurface(screen);
}

static void info_draw_track_10(int base, int height, int active,
                               int first_channel)
{
        int chan, chan_pos, fg;
        char buf[4];

        draw_fill_chars(5, base + 1, 74, base + height - 2, 0);

        SDL_LockSurface(screen);
        draw_box_unlocked(4, base, 75, base + height - 1,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        for (chan = first_channel, chan_pos = 0; chan_pos < 10;
             chan++, chan_pos++) {
                if (song_get_channel(chan - 1)->flags & CHN_MUTE)
                        fg = (chan == selected_channel ? 6 : 1);
                else
                        fg = (chan ==
                              selected_channel ? 3 : (active ? 2 : 0));
                draw_char_unlocked(0, 5 + 7 * chan_pos, base, 1, 1);
                draw_char_unlocked(0, 5 + 7 * chan_pos + 1, base, 1, 1);
                draw_text_unlocked(numtostr_2(chan, buf),
                                   5 + 7 * chan_pos + 2, base, fg, 1);
                draw_char_unlocked(0, 5 + 7 * chan_pos + 4, base, 1, 1);
                draw_char_unlocked(0, 5 + 7 * chan_pos + 5, base, 1, 1);
        }
        _draw_track_view(base, height, first_channel, 10, 7, 0,
                         draw_note_7);
        SDL_UnlockSurface(screen);
}

static void info_draw_track_12(int base, int height, int active,
                               int first_channel)
{
        int chan, chan_pos, fg;
        char buf[4];

        draw_fill_chars(5, base + 1, 76, base + height - 2, 0);

        SDL_LockSurface(screen);
        draw_box_unlocked(4, base, 77, base + height - 1,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        for (chan = first_channel, chan_pos = 0; chan_pos < 12;
             chan++, chan_pos++) {
                if (song_get_channel(chan - 1)->flags & CHN_MUTE)
                        fg = (chan == selected_channel ? 6 : 1);
                else
                        fg = (chan ==
                              selected_channel ? 3 : (active ? 2 : 0));
                //draw_char_unlocked(0, 5 + 6 * chan_pos, base, 1, 1);
                draw_char_unlocked(0, 5 + 6 * chan_pos + 1, base, 1, 1);
                draw_text_unlocked(numtostr_2(chan, buf),
                                   5 + 6 * chan_pos + 2, base, fg, 1);
                draw_char_unlocked(0, 5 + 6 * chan_pos + 4, base, 1, 1);
                //draw_char_unlocked(0, 5 + 6 * chan_pos + 5, base, 1, 1);
        }
        _draw_track_view(base, height, first_channel, 12, 6, 0,
                         draw_note_6);
        SDL_UnlockSurface(screen);
}

static void info_draw_track_18(int base, int height, int active,
                               int first_channel)
{
        int chan, chan_pos, fg;
        char buf[4];

        draw_fill_chars(5, base + 1, 75, base + height - 2, 0);
        SDL_LockSurface(screen);
        draw_box_unlocked(4, base, 76, base + height - 1,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        for (chan = first_channel, chan_pos = 0; chan_pos < 18;
             chan++, chan_pos++) {
                if (song_get_channel(chan - 1)->flags & CHN_MUTE)
                        fg = (chan == selected_channel ? 6 : 1);
                else
                        fg = (chan ==
                              selected_channel ? 3 : (active ? 2 : 0));
                draw_text_unlocked(numtostr_2(chan, buf),
                                   5 + 4 * chan_pos + 1, base, fg, 1);
        }
        _draw_track_view(base, height, first_channel, 18, 3, 1,
                         draw_note_3);
        SDL_UnlockSurface(screen);
}

static void info_draw_track_24(int base, int height, int active,
                               int first_channel)
{
        int chan, chan_pos, fg;
        char buf[4];

        draw_fill_chars(5, base + 1, 76, base + height - 2, 0);
        SDL_LockSurface(screen);
        draw_box_unlocked(4, base, 77, base + height - 1,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        for (chan = first_channel, chan_pos = 0; chan_pos < 24;
             chan++, chan_pos++) {
                if (song_get_channel(chan - 1)->flags & CHN_MUTE)
                        fg = (chan == selected_channel ? 6 : 1);
                else
                        fg = (chan ==
                              selected_channel ? 3 : (active ? 2 : 0));
                draw_text_unlocked(numtostr_2(chan, buf),
                                   5 + 3 * chan_pos + 1, base, fg, 1);
        }
        _draw_track_view(base, height, first_channel, 24, 3, 0,
                         draw_note_3);

        SDL_UnlockSurface(screen);
}

static void info_draw_track_36(int base, int height, int active,
                               int first_channel)
{
        int chan, chan_pos, fg;
        char buf[4];

        draw_fill_chars(5, base + 1, 76, base + height - 2, 0);
        SDL_LockSurface(screen);
        draw_box_unlocked(4, base, 77, base + height - 1,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        for (chan = first_channel, chan_pos = 0; chan_pos < 36;
             chan++, chan_pos++) {
                if (song_get_channel(chan - 1)->flags & CHN_MUTE)
                        fg = (chan == selected_channel ? 6 : 1);
                else
                        fg = (chan ==
                              selected_channel ? 3 : (active ? 2 : 0));
                draw_text_unlocked(numtostr_2(chan, buf), 5 + 2 * chan_pos,
                                   base, fg, 1);
        }
        _draw_track_view(base, height, first_channel, 36, 2, 0,
                         draw_note_2);
        SDL_UnlockSurface(screen);
}

static void info_draw_channels(int base, UNUSED int height, int active,
                               UNUSED int first_channel)
{
        char buf[32];
        int fg = (active ? 3 : 0);

        snprintf(buf, 32, "Active Channels: %d (%d)",
                 song_get_playing_channels(), song_get_max_channels());
        draw_text(buf, 2, base, fg, 2);

        snprintf(buf, 32, "Global Volume: %d",
                 song_get_current_global_volume());
        draw_text(buf, 4, base + 1, fg, 2);
}

#if 0
/* "Screw you guys, I'm going home."
 * I can't figure this out... it sorta kinda works, but not really.
 * If anyone wants to finish it: it's all yours. */

static void info_draw_note_dots(int base, int height, int active,
                                int first_channel)
{
        /* once this works, most of these variables can be optimized out
         * (some of them are just used once) */
        int fg, v;
        int c, pos;
        int n;
        song_mix_channel *channel;
        unsigned long *channel_list;
        char buf[4];
        byte d, dn;
        /* f#2 -> f#8 = 73 columns */
        /* lower nybble = colour, upper nybble = size */
        byte dot_field[73][36] = { {0} };

        draw_fill_chars(5, base + 1, 77, base + height - 2, 0);

        SDL_LockSurface(screen);
        draw_box_unlocked(4, base, 78, base + height - 1,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        SDL_UnlockSurface(screen);

        /* if it's stopped, just draw the channel numbers and a bunch
         * of dots. */

        n = song_get_mix_state(&channel_list);
        while (n--) {
                channel = song_get_mix_channel(channel_list[n]);

                /* 31 = f#2, 103 = f#8. (i hope ;) */
                if (!(channel->sample && channel->note >= 31
                      && channel->note <= 103))
                        continue;
                pos = channel->master_channel;
                if (pos < first_channel)
                        continue;
                pos -= first_channel;
                if (pos > height - 1)
                        continue;

                fg = ((channel->flags & CHN_MUTE)
                      ? 1 : ((channel->sample - song_get_sample(0, NULL))
                             % 4 + 2));
                v = (channel->final_volume + 2047) >> 11;
                d = dot_field[channel->note - 31][pos];
                dn = (v << 4) | fg;
                if (dn > d)
                        dot_field[channel->note - 31][pos] = dn;
        }

        SDL_LockSurface(screen);
        for (c = first_channel, pos = 0; pos < height - 2; pos++, c++) {
                for (n = 0; n < 73; n++) {
                        d = dot_field[n][pos];

                        if (d == 0) {
                                /* stick a blank dot there */
                                continue;
                        }
                        fg = d & 0xf;
                        v = d >> 4;
                        /* btw: Impulse Tracker uses char 173 instead
                         * of 193. why? */
                        draw_char_unlocked(v + 193, n + 5, pos + base + 1,
                                           fg, 0);
                }

                if (c == selected_channel) {
                        fg = (song_get_mix_channel(c - 1)->
                              flags & CHN_MUTE) ? 6 : 3;
                } else {
                        if (song_get_mix_channel(c - 1)->flags & CHN_MUTE)
                                continue;
                        fg = active ? 1 : 0;
                }
                draw_text(numtostr_2(c, buf), 2, pos + base + 1, fg, 2);
        }
        SDL_UnlockSurface(screen);
}
#endif

/* --------------------------------------------------------------------- */
/* declarations of the window types */

#define TRACK_VIEW(n) {info_draw_track_##n, 1, n}
static const struct info_window_type window_types[] = {
        {info_draw_samples, 0, -2},
        TRACK_VIEW(5),
        TRACK_VIEW(10),
        TRACK_VIEW(12),
        TRACK_VIEW(18),
        TRACK_VIEW(24),
        TRACK_VIEW(36),
        {info_draw_channels, 1, 0},
        /* {info_draw_note_dots, 0, -2}, */
};
#undef TRACK_VIEW

#define NUM_WINDOW_TYPES ARRAY_SIZE(window_types)

/* --------------------------------------------------------------------- */

static void recalculate_windows(void)
{
        int n, pos, channels;

        /* go through all the windows, and check the first_channel
         * against the window type's visible channels.
         * if it's out of range, fix it. */
        for (n = 0; n < num_windows; n++) {
                channels = window_types[windows[n].type].channels;

                if (channels == 0 || channels == 64)
                        continue;
                if (channels < 0) {
                        channels += windows[n].height;
                        if (n == 0
                            && !(window_types[windows[n].type].
                                 first_row)) {
                                /* crappy hack */
                                channels++;
                        }
                }
                if (selected_channel < windows[n].first_channel)
                        windows[n].first_channel = selected_channel;
                else if (selected_channel >=
                         (windows[n].first_channel + channels))
                        windows[n].first_channel =
                                selected_channel - channels + 1;
                if (windows[n].first_channel + channels > 65)
                        windows[n].first_channel = 65 - channels;
        }

        /* set the last window's height properly.
         * this should probably be merged with the previous loop, but it
         * would require rewriting stuff because the conditions are
         * different... so for now, i'm not caring :P */
        pos = 13;
        for (n = 0; n < num_windows - 1; n++)
                pos += windows[n].height;
        windows[n].height = 50 - pos;
}

/* --------------------------------------------------------------------- */

static void info_page_redraw(void)
{
        int n, height, pos =
                (window_types[windows[0].type].first_row ? 13 : 12);

        for (n = 0; n < num_windows - 1; n++) {
                height = windows[n].height;
                if (pos == 12)
                        height++;
                window_types[windows[n].type].draw(pos, height,
                                                   (n == selected_window),
                                                   windows[n].
                                                   first_channel);
                pos += height;
        }
        /* the last window takes up all the rest of the screen */
        window_types[windows[n].type].draw(pos, 50 - pos,
                                           (n == selected_window),
                                           windows[n].first_channel);
}

/* --------------------------------------------------------------------- */

static int info_page_handle_key(SDL_keysym * k)
{
        int n, order;

        switch (k->sym) {
        case SDLK_g:
                order = song_get_current_order();
                n = song_get_orderlist()[order];
                if (n < 200) {
                        set_current_order(order);
                        set_current_pattern(n);
                        set_current_row(song_get_current_row());
                        set_page(PAGE_PATTERN_EDITOR);
                }
                return 1;
        case SDLK_v:
                velocity_mode = !velocity_mode;
                status_text_flash("Using %s bars",
                                  (velocity_mode ? "velocity" : "volume"));
                status.flags |= NEED_UPDATE;
                return 1;
        case SDLK_i:
                instrument_names = !instrument_names;
                status_text_flash("Using %s names",
                                  (instrument_names ? "instrument" :
                                   "sample"));
                status.flags |= NEED_UPDATE;
                return 1;
        case SDLK_r:
                if (k->mod & (KMOD_ALT | KMOD_META)) {
                        song_flip_stereo();
                        status_text_flash("Left/right outputs reversed");
                        return 1;
                }
                return 0;
        case SDLK_KP_PLUS:
                if (song_get_mode() == MODE_PLAYING) {
                        song_set_current_order(song_get_current_order() +
                                               1);
                }
                return 1;
        case SDLK_KP_MINUS:
                if (song_get_mode() == MODE_PLAYING) {
                        song_set_current_order(song_get_current_order() -
                                               1);
                }
                return 1;
        case SDLK_q:
                song_toggle_channel_mute(selected_channel - 1);
                orderpan_recheck_muted_channels();
                status.flags |= NEED_UPDATE;
                return 1;
        case SDLK_s:
                song_handle_channel_solo(selected_channel - 1);
                orderpan_recheck_muted_channels();
                status.flags |= NEED_UPDATE;
                return 1;
        case SDLK_SPACE:
                song_toggle_channel_mute(selected_channel - 1);
                if (selected_channel < 64)
                        selected_channel++;
                orderpan_recheck_muted_channels();
                break;
        case SDLK_UP:
                if (k->mod & (KMOD_ALT | KMOD_META)) {
                        /* make the current window one line shorter, and
                         * give the line to the next window below it. if
                         * the window is already as small as it can get
                         * (3 lines) or if it's the last window, don't do
                         * anything. */
                        if (selected_window == num_windows - 1
                            || windows[selected_window].height == 3) {
                                return 1;
                        }
                        windows[selected_window].height--;
                        windows[selected_window + 1].height++;
                        break;
                }
                /* fall through */
        case SDLK_LEFT:
                if (selected_channel > 1)
                        selected_channel--;
                break;
        case SDLK_DOWN:
                if (k->mod & (KMOD_ALT | KMOD_META)) {
                        /* expand the current window, taking a line from
                         * the next window down. BUT: don't do anything if
                         * (a) this is the last window, or (b) the next
                         * window is already as small as it can be (three
                         * lines). */
                        if (selected_window == num_windows - 1
                            || windows[selected_window + 1].height == 3) {
                                return 1;
                        }
                        windows[selected_window].height++;
                        windows[selected_window + 1].height--;
                        break;
                }
                /* fall through */
        case SDLK_RIGHT:
                if (selected_channel < 64)
                        selected_channel++;
                break;
        case SDLK_HOME:
                selected_channel = 1;
                break;
        case SDLK_END:
                selected_channel = song_find_last_channel();
                break;
        case SDLK_INSERT:
                /* add a new window, unless there's already five (the
                 * maximum) or if the current window isn't big enough
                 * to split in half. */
                if (num_windows == MAX_WINDOWS
                    || (windows[selected_window].height < 6)) {
                        return 1;
                }

                num_windows++;

                /* shift the windows under the current one down */
                memmove(windows + selected_window + 1,
                        windows + selected_window,
                        ((num_windows - selected_window -
                          1) * sizeof(*windows)));

                /* split the height between the two windows */
                n = windows[selected_window].height;
                windows[selected_window].height = n / 2;
                windows[selected_window + 1].height = n / 2;
                if ((n & 1) && num_windows != 2) {
                        /* odd number? compensate. (the selected window
                         * gets the extra line) */
                        windows[selected_window + 1].height++;
                }
                break;
        case SDLK_DELETE:
                /* delete the current window and give the extra space to
                 * the next window down. if this is the only window,
                 * well then don't delete it ;) */
                if (num_windows == 1)
                        return 1;

                n = windows[selected_window].height +
                        windows[selected_window + 1].height;

                /* shift the windows under the current one up */
                memmove(windows + selected_window,
                        windows + selected_window + 1,
                        ((num_windows - selected_window -
                          1) * sizeof(*windows)));

                /* fix the current window's height */
                windows[selected_window].height = n;

                num_windows--;
                if (selected_window == num_windows)
                        selected_window--;
                break;
        case SDLK_PAGEUP:
                n = windows[selected_window].type;
                if (n == 0)
                        n = NUM_WINDOW_TYPES;
                n--;
                windows[selected_window].type = n;
                break;
        case SDLK_PAGEDOWN:
                windows[selected_window].type =
                        ((windows[selected_window].type +
                          1) % NUM_WINDOW_TYPES);
                break;
        case SDLK_TAB:
                if (k->mod & KMOD_SHIFT) {
                        if (selected_window == 0)
                                selected_window = num_windows;
                        selected_window--;
                } else {
                        selected_window =
                                ((selected_window + 1) % num_windows);
                }
                status.flags |= NEED_UPDATE;
                return 1;
        default:
                return 0;
        }

        recalculate_windows();
        status.flags |= NEED_UPDATE;
        return 1;
}

/* --------------------------------------------------------------------- */

static void info_page_playback_update(void)
{
        /* this will need changed after sample playback is working... */
        if (song_get_mode() != MODE_STOPPED)
                status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

void info_load_page(struct page *page)
{
        page->title = "Info Page (F5)";
        page->playback_update = info_page_playback_update;
        page->total_items = 1;
        page->items = items_info;
        page->help_index = HELP_INFO_PAGE;

        items_info[0].type = ITEM_OTHER;
        items_info[0].other.handle_key = info_page_handle_key;
        items_info[0].other.redraw = info_page_redraw;
}
