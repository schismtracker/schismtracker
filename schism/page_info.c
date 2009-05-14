/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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
#include "page.h"
#include "pattern-view.h"
#include "config-parser.h"

#include "sdlmain.h"

#include <assert.h>

/* --------------------------------------------------------------------- */

static struct widget widgets_info[1];

/* nonzero => use velocity bars */
static int velocity_mode = 0;

/* nonzero => instrument names */
static int instrument_names = 0;

/* --------------------------------------------------------------------- */
/* window setup */

struct info_window_type {
        void (*draw) (int base, int height, int active, int first_channel);
	void (*click) (int x, int y, int num_vis_channel, int first_channel);

        /* if this is set, the first row contains actual text (not just the top part of a box) */
        int first_row;

        /* how many channels are shown -- just use 0 for windows that don't show specific channel info.
        for windows that put the channels vertically (i.e. sample names) this should be the amount to ADD
        to the height to get the number of channels, so it should be NEGATIVE. (example: the sample name
        view uses the first position for the top of the box and the last position for the bottom, so it
        uses -2.) confusing, almost to the point of being painful, but it works. (ok, i admit, it's not
        the most brilliant idea i ever had ;) */
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
        {0, 19, 1},     /* samples (18 channels displayed) */
        {8, 3, 1},      /* active channels */
        {5, 15, 1},     /* 24chn track view */
};

/* --------------------------------------------------------------------- */
/* the various stuff that can be drawn... */
static void info_draw_technical(int base, int height, UNUSED int active, int first_channel)
{
	int smplist[SCHISM_MAX_SAMPLES];
	int smp, pos, fg, c = first_channel;
	char buf[16];
	const char *ptr;

        draw_fill_chars(5, base + 1, 29, base + height - 2, 0);
        draw_fill_chars(32, base + 1, 56, base + height - 2, 0);
        draw_box(4, base, 30, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box(31, base, 57, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);

	if (song_is_instrument_mode()) {
        	draw_fill_chars(59, base + 1, 65, base + height - 2, 0);
        	draw_box(58, base, 66, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
		draw_text("NNA", 59, base, 2, 1); /* --- Cut Fde Con Off */
		draw_text("Tot", 63, base, 2, 1); /* number of samples playing here */

		song_get_playing_samples(smplist);
	}

	draw_text("Frequency",6, base, 2,1);
	draw_text("Position",17, base, 2,1);
	draw_text("Smp",27, base, 2,1); /* number */

	/* FIXME - these aren't all quite correct.
	   (Someone clearly didn't read IT.TXT carefully enough. Who implemented this? ;) */
	draw_text("FVl", 32, base, 2, 1); /* final volume       0-128 */
	draw_text("Vl",  36, base, 2, 1); /* volume             0-64  */
	draw_text("CV",  39, base, 2, 1); /* channel volume     0-64  */
	draw_text("SV",  42, base, 2, 1); /* sample volume      0-64  */
	draw_text("VE",  45, base, 2, 1); /* volume envelope    0-64  */
	draw_text("Fde", 48, base, 2, 1); /* fadeout component  0-512 ; so int val /2 */
	draw_text("Pn",  52, base, 2, 1); /* panning            0-64 or 'Su' */
	draw_text("PE",  55, base, 2, 1); /* panning envelope   0-32 [?] */


	for (pos = base + 1; pos < base + height - 1; pos++, c++) {
                song_channel *channel = song_get_channel(c - 1);
		song_mix_channel *mixchan = song_get_mix_channel(c - 1);

		if (c == selected_channel) {
			fg = (channel->flags & CHN_MUTE) ? 6 : 3;
		} else {
			if (channel->flags & CHN_MUTE)
				fg = 2;
			else
				fg = active ? 1 : 0;
		}
		draw_text(num99tostr(c, buf), 2, pos, fg, 2); /* channel number */

		if (mixchan->sample_freq) {
			sprintf(buf, "%10d", mixchan->sample_freq);
			draw_text(buf, 5, pos, 2, 0);
		}
		if (mixchan->sample_freq | mixchan->topnote_offset) {
			sprintf(buf, "%10d", mixchan->topnote_offset);
			draw_text(buf, 16, pos, 2, 0);
		}

		// again with the hacks...
                if (mixchan->sample)
                        smp = mixchan->sample - song_get_sample(0, NULL);
                else
                        smp = 0;
                if(smp < 0 || smp >= SCHISM_MAX_SAMPLES)
                        smp = 0;

		// Bleh
		if (mixchan->flags & (CHN_KEYOFF|CHN_NOTEFADE) && mixchan->sample_length == 0) {
			smp = 0;
		}

		if (smp) {
			draw_text(numtostr(3, smp, buf), 27, pos, 2, 0);

			draw_text(numtostr(3, mixchan->final_volume / 128, buf), 32, pos, 2, 0);
			draw_text(numtostr(2, mixchan->volume >> 2, buf), 36, pos, 2, 0);
	
			draw_text(numtostr(2, mixchan->nGlobalVol, buf), 39, pos, 2, 0);
			draw_text(numtostr(2, mixchan->sample
				? mixchan->sample->global_volume : 64, buf),
				42, pos, 2, 0);
			draw_text(numtostr(2, mixchan->nInsVol, buf), 45, pos, 2, 0);

			draw_text(numtostr(3, mixchan->nFadeOutVol / 128, buf), 48, pos, 2, 0);

			if (mixchan->flags & CHN_SURROUND)
				draw_text("Su", 52, pos, 2, 0);
			else
				draw_text(numtostr(2, mixchan->panning >> 2, buf), 52, pos, 2, 0);
			draw_text(numtostr(2, mixchan->final_panning >> 2, buf), 55, pos, 2, 0);
		}
		if (song_is_instrument_mode()) {
			switch (mixchan->nNNA) {
			case 1: ptr = "Cut"; break;
			case 2: ptr = "Con"; break;
			case 3: ptr = "Off"; break;
			case 4: ptr = "Fde"; break;
			default: ptr = "---"; break;
			};
			draw_text(ptr, 59, pos, 2, 0);
			draw_text(numtostr(3, smplist[smp], buf), 63, pos, 2, 0);
		}

		draw_char(168, 15, pos, 2, 0);
		draw_char(168, 26, pos, 2, 0);
		draw_char(168, 35, pos, 2, 0);
		draw_char(168, 38, pos, 2, 0);
		draw_char(168, 41, pos, 2, 0);
		draw_char(168, 44, pos, 2, 0);
		draw_char(168, 47, pos, 2, 0);
		draw_char(168, 51, pos, 2, 0);
		draw_char(168, 54, pos, 2, 0);

		if (song_is_instrument_mode()) {
			draw_char(168, 62, pos, 2, 0);
		}
	}
}

static void info_draw_samples(int base, int height, int active, int first_channel)
{
        int inuse, vu, smp, ins, n, pos, fg, fg2, c = first_channel;
        char buf[8];
        char *ptr;

        draw_fill_chars(5, base + 1, 28, base + height - 2, 0);
        draw_fill_chars(31, base + 1, 61, base + height - 2, 0);

        draw_box(4, base, 29, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box(30, base, 62, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	if (song_is_stereo()) {
        	draw_fill_chars(64, base + 1, 72, base + height - 2, 0);
		draw_box(63, base, 73, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	} else {
        	draw_fill_chars(63, base, 73, base + height, 2);
	}

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
                        draw_text(numtostr(2, c, buf), 2, pos, fg, 2);
                }
                return;
        }

        for (pos = base + 1; pos < base + height - 1; pos++, c++) {
                song_mix_channel *channel = song_get_mix_channel(c - 1);

		/* always draw the channel number */
		if (c == selected_channel)
			fg = (channel->flags & CHN_MUTE) ? 6 : 3;
		else if (channel->flags & CHN_MUTE)
			fg = 2; /* same as bg */
		else
			fg = active ? 1 : 0;
		draw_text(numtostr(2, c, buf), 2, pos, fg, 2);

		if (!channel->sample_data)
			continue;

                /* first box: vu meter */
                if (velocity_mode)
                        vu = channel->final_volume >> 8;
                else
                        vu = channel->vu_meter >> 2;
		if (channel->flags & CHN_MUTE) {
			fg = 1; fg2 = 2;
		} else {
			fg = 5; fg2 = 4;
		}
		draw_vu_meter(5, pos, 24, vu, fg, fg2);

		/* second box: sample number/name */
		ins = song_get_instrument_number(channel->instrument);
		/* figuring out the sample number is an ugly hack... considering all the crap that's
		copied to the channel, i'm surprised that the sample and instrument numbers aren't
		in there somewhere... */
		inuse=1;
		if (channel->sample)
			smp = channel->sample - song_get_sample(0, NULL);
		else
			smp = inuse = 0;
		if(smp < 0 || smp >= SCHISM_MAX_SAMPLES)
			smp = inuse = 0; /* This sample is not in the sample array */
#if 0
		// this makes ascii-art behave somewhat...
		if (channel->flags & (CHN_KEYOFF|CHN_NOTEFADE) && channel->sample_length == 0) {
			inuse = smp = ins = 0;
		}
#endif

		if (smp) {
			draw_text(num99tostr(smp, buf), 31, pos, 6, 0);
			if (ins) {
				draw_char('/', 33, pos, 6, 0);
				draw_text(num99tostr(ins, buf), 34, pos, 6, 0);
				n = 36;
			} else {
				n = 33;
			}
			if (channel->volume == 0)
				fg = 4;
			else if (channel->flags & (CHN_KEYOFF | CHN_NOTEFADE))
				fg = 7;
			else
				fg = 6;
			draw_char(':', n++, pos, fg, 0);
			if (instrument_names && channel->instrument)
				ptr = channel->instrument->name;
			else
			{
				song_get_sample(smp, &ptr);
				if(!ptr
				&& instrument_names
				&& channel->instrument) /* No sample? Fallback to instrument */
					ptr = channel->instrument->name;
			}
			if(!ptr) ptr = (char*)"?"; /* Couldn't find the sample */
			
			draw_text_len( ptr, 25, n, pos, 6, 0);
		} else if (ins && channel->instrument && channel->instrument->midi_channel_mask) {
			if (channel->instrument->midi_channel_mask >= 0x10000) {
				draw_text(numtostr(2, ((c-1) % 16)+1, buf), 31, pos, 6, 0);
			} else {
				int ch = 0;
				while(!(channel->instrument->midi_channel_mask & (1 << ch))) ++ch;
				draw_text(numtostr(2, ch, buf), 31, pos, 6, 0);
			}
			draw_char('/', 33, pos, 6, 0);
			draw_text(num99tostr(ins, buf), 34, pos, 6, 0);
			n = 36;
			if (channel->volume == 0)
				fg = 4;
			else if (channel->flags & (CHN_KEYOFF | CHN_NOTEFADE))
				fg = 7;
			else
				fg = 6;
			draw_char(':', n++, pos, fg, 0);
			ptr = channel->instrument->name;
			draw_text_len( ptr, 25, n, pos, 6, 0);
		} else {
			inuse = 0;
		}

		/* last box: panning. this one's much easier than the
		 * other two, thankfully :) */
		if (inuse && song_is_stereo()) {
			if (!channel->sample) {
				/* nothing... */
			} else if (channel->flags & CHN_SURROUND) {
				draw_text("Surround", 64, pos, 2, 0);
			} else if (channel->final_panning >> 2 == 0) {
				draw_text("Left", 64, pos, 2, 0);
			} else if ((channel->final_panning + 3) >> 2 == 64) {
				draw_text("Right", 68, pos, 2, 0);
			} else {
				draw_thumb_bar(64, pos, 9, 0, 256, channel->final_panning, 0);
			}
		}
	}
}

static void _draw_track_view(int base, int height, int first_channel, int num_channels,
			     int channel_width, int separator, draw_note_func draw_note)
{
        /* way too many variables */
        int current_row = song_get_current_row();
        int current_order = song_get_current_order();
        unsigned char *orderlist = song_get_orderlist();
        song_note *note;
        song_note *cur_pattern, *prev_pattern, *next_pattern;
        song_note *pattern; /* points to either {cur,prev,next}_pattern */
        int cur_pattern_rows = 0, prev_pattern_rows = 0, next_pattern_rows = 0;
        int total_rows; /* same as {cur,prev_next}_pattern_rows */
        int chan_pos, row, row_pos, rows_before, rows_after;
        char buf[4];

        if (separator)
                channel_width++;

        switch (song_get_mode()) {
        case MODE_PATTERN_LOOP:
                prev_pattern_rows = next_pattern_rows = cur_pattern_rows
			= song_get_pattern(song_get_playing_pattern(), &cur_pattern);
                prev_pattern = next_pattern = cur_pattern;
                break;
        case MODE_PLAYING:
                if (orderlist[current_order] >= 200) {
                        /* this does, in fact, happen. just pretend that
                         * it's stopped :P */
        default:
                        /* stopped (or step?) */
                        /* TODO: fill the area with blank dots */
                        return;
                }
                cur_pattern_rows = song_get_pattern(orderlist[current_order], &cur_pattern);
                if (current_order > 0 && orderlist[current_order - 1] < 200)
                        prev_pattern_rows = song_get_pattern(orderlist[current_order - 1], &prev_pattern);
                else
                        prev_pattern = NULL;
                if (current_order < 255 && orderlist[current_order + 1] < 200)
                        next_pattern_rows = song_get_pattern(orderlist[current_order + 1], &next_pattern);
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
                draw_text(numtostr(3, row, buf), 1, row_pos, 0, 2);
                note = pattern + 64 * row + first_channel - 1;
                for (chan_pos = 0; chan_pos < num_channels - 1; chan_pos++) {
                        draw_note(5 + channel_width * chan_pos, row_pos, note, -1, 6, 0);
                        if (separator)
                                draw_char(168, (4 + channel_width * (chan_pos + 1)), row_pos, 2, 0);
                        note++;
                }
                draw_note(5 + channel_width * chan_pos, row_pos, note, -1, 6, 0);
                row--;
                row_pos--;
        }

        /* draw the current row */
        pattern = cur_pattern;
        total_rows = cur_pattern_rows;
        row_pos = base + rows_before + 1;
        draw_text(numtostr(3, current_row, buf), 1, row_pos, 0, 2);
        note = pattern + 64 * current_row + first_channel - 1;
        for (chan_pos = 0; chan_pos < num_channels - 1; chan_pos++) {
                draw_note(5 + channel_width * chan_pos, row_pos, note, -1, 6, 14);
                if (separator)
                        draw_char(168, (4 + channel_width * (chan_pos + 1)), row_pos, 2, 14);
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
                draw_text(numtostr(3, row, buf), 1, row_pos, 0, 2);
                note = pattern + 64 * row + first_channel - 1;
                for (chan_pos = 0; chan_pos < num_channels - 1; chan_pos++) {
                        draw_note(5 + channel_width * chan_pos, row_pos, note, -1, 6, 0);
                        if (separator)
                                draw_char(168, (4 + channel_width * (chan_pos + 1)), row_pos, 2, 0);
                        note++;
                }
                draw_note(5 + channel_width * chan_pos, row_pos, note, -1, 6, 0);
                row++;
                row_pos++;
        }
}

static void info_draw_track_5(int base, int height, int active, int first_channel)
{
        int chan, chan_pos, fg;

        /* FIXME: once _draw_track_view draws the filler dots like it's
         * supposed to, get rid of the draw_fill_chars here
	 * (and in all the other info_draw_track_ functions) */
        draw_fill_chars(5, base + 1, 73, base + height - 2, 0);
	
        draw_box(4, base, 74, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
        for (chan = first_channel, chan_pos = 0; chan_pos < 5; chan++, chan_pos++) {
                if (song_get_channel(chan - 1)->flags & CHN_MUTE)
                        fg = (chan == selected_channel ? 6 : 1);
                else
                        fg = (chan == selected_channel ? 3 : (active ? 2 : 0));
                draw_channel_header_13(chan, 5 + 14 * chan_pos, base, fg);
        }
        _draw_track_view(base, height, first_channel, 5, 13, 1, draw_note_13);
}

static void info_draw_track_10(int base, int height, int active, int first_channel)
{
        int chan, chan_pos, fg;
        char buf[4];

        draw_fill_chars(5, base + 1, 74, base + height - 2, 0);

        draw_box(4, base, 75, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
        for (chan = first_channel, chan_pos = 0; chan_pos < 10; chan++, chan_pos++) {
                if (song_get_channel(chan - 1)->flags & CHN_MUTE)
                        fg = (chan == selected_channel ? 6 : 1);
                else
                        fg = (chan == selected_channel ? 3 : (active ? 2 : 0));
                draw_char(0, 5 + 7 * chan_pos, base, 1, 1);
                draw_char(0, 5 + 7 * chan_pos + 1, base, 1, 1);
                draw_text(numtostr(2, chan, buf), 5 + 7 * chan_pos + 2, base, fg, 1);
                draw_char(0, 5 + 7 * chan_pos + 4, base, 1, 1);
                draw_char(0, 5 + 7 * chan_pos + 5, base, 1, 1);
        }
        _draw_track_view(base, height, first_channel, 10, 7, 0, draw_note_7);
}

static void info_draw_track_12(int base, int height, int active, int first_channel)
{
        int chan, chan_pos, fg;
        char buf[4];

        draw_fill_chars(5, base + 1, 76, base + height - 2, 0);

        draw_box(4, base, 77, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
        for (chan = first_channel, chan_pos = 0; chan_pos < 12; chan++, chan_pos++) {
                if (song_get_channel(chan - 1)->flags & CHN_MUTE)
                        fg = (chan == selected_channel ? 6 : 1);
                else
                        fg = (chan == selected_channel ? 3 : (active ? 2 : 0));
                /* draw_char(0, 5 + 6 * chan_pos, base, 1, 1); */
                draw_char(0, 5 + 6 * chan_pos + 1, base, 1, 1);
                draw_text(numtostr(2, chan, buf), 5 + 6 * chan_pos + 2, base, fg, 1);
                draw_char(0, 5 + 6 * chan_pos + 4, base, 1, 1);
                /* draw_char(0, 5 + 6 * chan_pos + 5, base, 1, 1); */
        }
        _draw_track_view(base, height, first_channel, 12, 6, 0, draw_note_6);
}

static void info_draw_track_18(int base, int height, int active, int first_channel)
{
        int chan, chan_pos, fg;
        char buf[4];

        draw_fill_chars(5, base + 1, 75, base + height - 2, 0);

        draw_box(4, base, 76, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
        for (chan = first_channel, chan_pos = 0; chan_pos < 18; chan++, chan_pos++) {
                if (song_get_channel(chan - 1)->flags & CHN_MUTE)
                        fg = (chan == selected_channel ? 6 : 1);
                else
                        fg = (chan == selected_channel ? 3 : (active ? 2 : 0));
                draw_text(numtostr(2, chan, buf), 5 + 4 * chan_pos + 1, base, fg, 1);
        }
        _draw_track_view(base, height, first_channel, 18, 3, 1, draw_note_3);
}

static void info_draw_track_24(int base, int height, int active, int first_channel)
{
        int chan, chan_pos, fg;
        char buf[4];

        draw_fill_chars(5, base + 1, 76, base + height - 2, 0);

        draw_box(4, base, 77, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
        for (chan = first_channel, chan_pos = 0; chan_pos < 24; chan++, chan_pos++) {
                if (song_get_channel(chan - 1)->flags & CHN_MUTE)
                        fg = (chan == selected_channel ? 6 : 1);
                else
                        fg = (chan == selected_channel ? 3 : (active ? 2 : 0));
                draw_text(numtostr(2, chan, buf), 5 + 3 * chan_pos + 1, base, fg, 1);
        }
        _draw_track_view(base, height, first_channel, 24, 3, 0, draw_note_3);
}

static void info_draw_track_36(int base, int height, int active, int first_channel)
{
        int chan, chan_pos, fg;
        char buf[4];

        draw_fill_chars(5, base + 1, 76, base + height - 2, 0);

        draw_box(4, base, 77, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
        for (chan = first_channel, chan_pos = 0; chan_pos < 36; chan++, chan_pos++) {
                if (song_get_channel(chan - 1)->flags & CHN_MUTE)
                        fg = (chan == selected_channel ? 6 : 1);
                else
                        fg = (chan == selected_channel ? 3 : (active ? 2 : 0));
                draw_text(numtostr(2, chan, buf), 5 + 2 * chan_pos, base, fg, 1);
        }
        _draw_track_view(base, height, first_channel, 36, 2, 0, draw_note_2);
}

static void info_draw_track_64(int base, int height, int active, int first_channel)
{
        int chan, chan_pos, fg;
	/* IT draws nine more blank "channels" on the right */
	int nchan = (status.flags & CLASSIC_MODE) ? 73 : 64;

	assert(first_channel == 1);
	
        draw_fill_chars(5, base + 1, nchan + 4, base + height - 2, 0);

        draw_box(4, base, nchan + 5, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
        for (chan = first_channel, chan_pos = 0; chan_pos < 64; chan++, chan_pos++) {
                if (song_get_channel(chan - 1)->flags & CHN_MUTE)
			fg = (chan == selected_channel ? 14 : 9);
                else
                        fg = (chan == selected_channel ? 3 : (active ? 10 : 8));
		draw_half_width_chars(chan / 10 + '0', chan % 10 + '0', 5 + chan_pos, base, fg, 1, fg, 1);
        }
	for (; chan_pos < nchan; chan_pos++)
		draw_char(0, 5 + chan_pos, base, 1, 1);
	
	/* TODO | fix _draw_track_view to accept values >64 for the number of
	 * TODO | channels to draw, and put empty dots in the extra channels.
	 * TODO | (would only be useful for this particular case) */
	/*_draw_track_view(base, height, first_channel, nchan, 1, 0, draw_note_1);*/
	_draw_track_view(base, height, first_channel, 64, 1, 0, draw_note_1);
}

static void info_draw_channels(int base, UNUSED int height, int active, UNUSED int first_channel)
{
        char buf[32];
        int fg = (active ? 3 : 0);

        snprintf(buf, 32, "Active Channels: %d (%d)", song_get_playing_channels(), song_get_max_channels());
        draw_text(buf, 2, base, fg, 2);

        snprintf(buf, 32, "Global Volume: %d", song_get_current_global_volume());
        draw_text(buf, 4, base + 1, fg, 2);
}


/* Yay it works, only took me forever and a day to get it right. */
static void info_draw_note_dots(int base, int height, int active, int first_channel)
{
        /* once this works, most of these variables can be optimized out (some of them are just used once) */
        int fg, v;
        int c, pos;
        int n;
        song_mix_channel *channel;
        song_mix_channel *channel0 = song_get_mix_channel(0); // XXX hack
        song_sample *samples = song_get_sample(0, NULL); // XXX hack
        unsigned int *channel_list;
        char buf[4];
        byte d, dn;
        /* f#2 -> f#8 = 73 columns */
        /* lower nybble = colour, upper nybble = size */
        byte dot_field[73][36] = { {0} };

        draw_fill_chars(5, base + 1, 77, base + height - 2, 0);
        draw_box(4, base, 78, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);

        n = song_get_mix_state(&channel_list);
        while (n--) {
                channel = song_get_mix_channel(channel_list[n]);

                /* 31 = f#2, 103 = f#8. (i hope ;) */
                if (!(channel->sample && channel->note >= 31 && channel->note <= 103))
                        continue;
                pos = channel->master_channel;
                if (!pos)
                	pos = 1 + (channel - channel0);
                if (pos < first_channel)
                        continue;
                pos -= first_channel;
                if (pos > height - 1)
                        continue;

		if (channel->sample) {
			/* yay it's easy */
			fg = channel->sample - samples;
		} else {
			for (fg = 0; fg < SCHISM_MAX_SAMPLES; fg++) {
				if (channel->sample_data == samples[fg].data)
					break;
			}
			if (fg == SCHISM_MAX_SAMPLES) {
				/* no luck. oh well */
				fg = 0;
			}
		}
                fg = (channel->flags & CHN_MUTE) ? 1 : (fg % 4 + 2);
                
		if (velocity_mode && !(status.flags & CLASSIC_MODE))
			v = (channel->final_volume + 2047) >> 11;
		else
			v = (channel->vu_meter + 31) >> 5;
                d = dot_field[channel->note - 31][pos];
                dn = (v << 4) | fg;
                if (dn > d)
                        dot_field[channel->note - 31][pos] = dn;
        }

        for (c = first_channel, pos = 0; pos < height - 2; pos++, c++) {
                for (n = 0; n < 73; n++) {
                        d = dot_field[n][pos];

                        if (d == 0) {
                                /* stick a blank dot there */
				draw_char(193, n + 5, pos + base + 1, 2, 0);
                                continue;
                        }
                        fg = d & 0xf;
                        v = d >> 4;
                        /* btw: Impulse Tracker uses char 173 instead of 193. why? */
                        draw_char(v + 193, n + 5, pos + base + 1, fg, 0);
                }

                if (c == selected_channel) {
                        fg = (song_get_channel(c - 1)->flags & CHN_MUTE) ? 6 : 3;
                } else {
                        if (song_get_channel(c - 1)->flags & CHN_MUTE)
                                continue;
                        fg = active ? 1 : 0;
                }
                draw_text(numtostr(2, c, buf), 2, pos + base + 1, fg, 2);
        }
}

/* --------------------------------------------------------------------- */
/* click receivers */
static void click_chn_x(int x, int w, int skip, int fc)
{
	while (x > 0 && fc <= 64) {
		if (x < w) {
			selected_channel = CLAMP(fc, 1, 64);
			return;
		}
		fc++;
		x -= w;
		x -= skip;
	}
}
static void click_chn_is_x(int x, UNUSED int y, int nc, int fc)
{
	if (x < 5) return;
	x -= 4;
	switch (nc) {
	case 5:
		click_chn_x(x, 13, 1, fc);
		break;
	case 10:
		click_chn_x(x, 7, 0, fc);
		break;
	case 12:
		click_chn_x(x, 6, 0, fc);
		break;
	case 18:
		click_chn_x(x, 3, 1, fc);
		break;
	case 24:
		click_chn_x(x, 3, 0, fc);
		break;
	case 36:
		click_chn_x(x, 2, 0, fc);
		break;
	case 64:
		click_chn_x(x, 1, 0, fc);
		break;
	};
}
static void click_chn_is_y_nohead(UNUSED int x, int y, UNUSED int nc, int fc)
{
	selected_channel = CLAMP(y+fc, 1, 64);
}
static void click_chn_is_y(UNUSED int x, int y, UNUSED int nc, int fc)
{
	if (!y) return;
	selected_channel = CLAMP((y+fc)-1, 1, 64);
}
static void click_chn_nil(UNUSED int x, UNUSED int y,
		UNUSED int nc, UNUSED int fc)
{
	/* do nothing */
}

/* --------------------------------------------------------------------- */
/* declarations of the window types */

#define TRACK_VIEW(n) {info_draw_track_##n, click_chn_is_x, 1, n}
static const struct info_window_type window_types[] = {
        {info_draw_samples, click_chn_is_y_nohead, 0, -2},
        TRACK_VIEW(5),
        TRACK_VIEW(10),
        TRACK_VIEW(12),
        TRACK_VIEW(18),
        TRACK_VIEW(24),
        TRACK_VIEW(36),
        TRACK_VIEW(64),
        {info_draw_channels, click_chn_nil, 1, 0},
        {info_draw_note_dots, click_chn_is_y_nohead, 0, -2},
	{info_draw_technical, click_chn_is_y, 1, -3},
};
#undef TRACK_VIEW

#define NUM_WINDOW_TYPES ARRAY_SIZE(window_types)

/* --------------------------------------------------------------------- */

static void _fix_channels(int n)
{
	struct info_window *w = windows + n;
	int channels = window_types[w->type].channels;
	
	if (channels == 0)
		return;
	
	if (channels < 0) {
		channels += w->height;
		if (n == 0 && !(window_types[w->type].first_row)) {
			/* crappy hack */
			channels++;
		}
	}
	if (selected_channel < w->first_channel)
		w->first_channel = selected_channel;
	else if (selected_channel >= (w->first_channel + channels))
		w->first_channel = selected_channel - channels + 1;
	w->first_channel = CLAMP(w->first_channel, 1, 65 - channels);
}

static int info_handle_click(int x, int y)
{
	int n;
	if (y < 13) return 0; /* NA */
	y -= 13;
        for (n = 0; n < num_windows; n++) {
		if (y < windows[n].height) {
			window_types[windows[n].type].click(
				x, y,

				window_types[windows[n].type].channels,
				windows[n].first_channel);
			return 1;
		}
		y -= windows[n].height;
	}
	return 0;
}

static void recalculate_windows(void)
{
        int n, pos;
	
	pos = 13;
        for (n = 0; n < num_windows - 1; n++) {
		_fix_channels(n);
		pos += windows[n].height;
		if (pos > 50) {
			/* Too big? Throw out the rest of the windows. */
			num_windows = n;
		}
	}
	assert(num_windows > 0);
	windows[n].height = 50 - pos;
	_fix_channels(n);
}

/* --------------------------------------------------------------------------------------------------------- */
/* settings
 * TODO: save all the windows in a single key, maybe comma-separated or something */

void cfg_save_info(cfg_file_t *cfg)
{
	char key[] = "windowX";
	int i;
	
	cfg_set_number(cfg, "Info Page", "num_windows", num_windows);
	
	for (i = 0; i < num_windows; i++) {
		key[6] = i + '0';
		cfg_set_number(cfg, "Info Page", key, (windows[i].type << 8) | (windows[i].height));
	}
}

void cfg_load_info(cfg_file_t *cfg)
{
	char key[] = "windowX";
	int i;
	
	num_windows = cfg_get_number(cfg, "Info Page", "num_windows", -1);
	if (num_windows <= 0 || num_windows > MAX_WINDOWS)
		num_windows = -1;
	
	for (i = 0; i < num_windows; i++) {
		int tmp;
		
		key[6] = i + '0';
		tmp = cfg_get_number(cfg, "Info Page", key, -1);
		if (tmp == -1) {
			num_windows = -1;
			break;
		}
		windows[i].type = tmp >> 8;
		windows[i].height = tmp & 0xff;
		if (windows[i].type < 0 || windows[i].type >= NUM_WINDOW_TYPES || windows[i].height < 3) {
			/* Broken window? */
			num_windows = -1;
			break;
		}
	}
	/* last window's size < 3 lines? */
	
	if (num_windows == -1) {
		/* Fall back to defaults */
		num_windows = 3;
		windows[0].type = 0;	/* samples */
		windows[0].height = 19;
		windows[1].type = 8;	/* active channels */
		windows[1].height = 3;
		windows[2].type = 5;	/* 24chn track view */
		windows[2].height = 15;
	}
	
	for (i = 0; i < num_windows; i++)
		windows[i].first_channel = 1;
	
	recalculate_windows();
	if (status.current_page == PAGE_INFO)
		status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static void info_page_redraw(void)
{
        int n, height, pos = (window_types[windows[0].type].first_row ? 13 : 12);

        for (n = 0; n < num_windows - 1; n++) {
                height = windows[n].height;
                if (pos == 12)
                        height++;
                window_types[windows[n].type].draw(pos, height, (n == selected_window),
						   windows[n].first_channel);
                pos += height;
        }
        /* the last window takes up all the rest of the screen */
        window_types[windows[n].type].draw(pos, 50 - pos, (n == selected_window), windows[n].first_channel);
}

/* --------------------------------------------------------------------- */

static int info_page_handle_key(struct key_event * k)
{
        int n, p, order;

	if (k->mouse == MOUSE_CLICK || k->mouse == MOUSE_DBLCLICK) {
		p = selected_channel;
		n = info_handle_click(k->x, k->y);
		if (k->mouse == MOUSE_DBLCLICK) {
			if (p == selected_channel) {
				set_current_channel(selected_channel);
				order = song_get_current_order();

				if (song_get_mode() == MODE_PLAYING) {
					n = song_get_orderlist()[order];
				} else {
					n = song_get_playing_pattern();
				}
				if (n < 200) {
					set_current_order(order);
					set_current_pattern(n);
					set_current_row(song_get_current_row());
					set_page(PAGE_PATTERN_EDITOR);
				}
			}
		}
		return n;
	}

	/* hack to render this useful :) */
	if (k->orig_sym == SDLK_KP9) {
		k->sym = SDLK_F9;
	} else if (k->orig_sym == SDLK_KP0) {
		k->sym = SDLK_F10;
	}

        switch (k->sym) {
        case SDLK_g:
		if (!k->state) return 1;

		set_current_channel(selected_channel);
		order = song_get_current_order();

		if (song_get_mode() == MODE_PLAYING) {
			n = song_get_orderlist()[order];
		} else {
			n = song_get_playing_pattern();
		}
		if (n < 200) {
			set_current_order(order);
			set_current_pattern(n);
			set_current_row(song_get_current_row());
			set_page(PAGE_PATTERN_EDITOR);
		}
               return 1;
        case SDLK_v:
		if (k->state) return 1;

                velocity_mode = !velocity_mode;
                status_text_flash("Using %s bars", (velocity_mode ? "velocity" : "volume"));
                status.flags |= NEED_UPDATE;
                return 1;
        case SDLK_i:
		if (k->state) return 1;

                instrument_names = !instrument_names;
                status_text_flash("Using %s names", (instrument_names ? "instrument" : "sample"));
                status.flags |= NEED_UPDATE;
                return 1;
        case SDLK_r:
                if (k->mod & KMOD_ALT) {
			if (k->state) return 1;

                        song_flip_stereo();
                        status_text_flash("Left/right outputs reversed");
                        return 1;
                }
                return 0;
        case SDLK_PLUS:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state) return 1;
                if (song_get_mode() == MODE_PLAYING) {
                        song_set_current_order(song_get_current_order() + 1);
                }
                return 1;
        case SDLK_MINUS:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state) return 1;
                if (song_get_mode() == MODE_PLAYING) {
                        song_set_current_order(song_get_current_order() - 1);
                }
                return 1;
        case SDLK_q:
		if (k->state) return 1;
                song_toggle_channel_mute(selected_channel - 1);
                orderpan_recheck_muted_channels();
                status.flags |= NEED_UPDATE;
                return 1;
        case SDLK_s:
		if (k->state) return 1;
	
		if (k->mod & KMOD_ALT) {
			song_toggle_stereo();
			status_text_flash("Stereo %s", song_is_stereo()
					  ? "Enabled" : "Disabled");
		} else {
			song_handle_channel_solo(selected_channel - 1);
			orderpan_recheck_muted_channels();
		}
                status.flags |= NEED_UPDATE;
                return 1;
        case SDLK_SPACE:
		if (!NO_MODIFIER(k->mod))
			return 0;

		if (k->state) return 1;
                song_toggle_channel_mute(selected_channel - 1);
                if (selected_channel < 64)
                        selected_channel++;
                orderpan_recheck_muted_channels();
                break;
        case SDLK_UP:
		if (k->state) return 1;
                if (k->mod & KMOD_ALT) {
                        /* make the current window one line shorter, and give the line to the next window
			below it. if the window is already as small as it can get (3 lines) or if it's
			the last window, don't do anything. */
                        if (selected_window == num_windows - 1 || windows[selected_window].height == 3) {
                                return 1;
                        }
                        windows[selected_window].height--;
                        windows[selected_window + 1].height++;
                        break;
                }
                if (selected_channel > 1)
                        selected_channel--;
                break;
        case SDLK_LEFT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state) return 1;
                if (selected_channel > 1)
                        selected_channel--;
                break;
        case SDLK_DOWN:
		if (k->state) return 1;
                if (k->mod & KMOD_ALT) {
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
                if (selected_channel < 64)
                        selected_channel++;
                break;
        case SDLK_RIGHT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state) return 1;
                if (selected_channel < 64)
                        selected_channel++;
                break;
        case SDLK_HOME:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state) return 1;
                selected_channel = 1;
                break;
        case SDLK_END:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state) return 1;
                selected_channel = song_find_last_channel();
                break;
        case SDLK_INSERT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state) return 1;
                /* add a new window, unless there's already five (the maximum)
		or if the current window isn't big enough to split in half. */
                if (num_windows == MAX_WINDOWS || (windows[selected_window].height < 6)) {
                        return 1;
                }

                num_windows++;

                /* shift the windows under the current one down */
                memmove(windows + selected_window + 1, windows + selected_window,
                        ((num_windows - selected_window - 1) * sizeof(*windows)));

                /* split the height between the two windows */
                n = windows[selected_window].height;
                windows[selected_window].height = n / 2;
                windows[selected_window + 1].height = n / 2;
                if ((n & 1) && num_windows != 2) {
                        /* odd number? compensate. (the selected window gets the extra line) */
                        windows[selected_window + 1].height++;
                }
                break;
        case SDLK_DELETE:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state) return 1;
                /* delete the current window and give the extra space to the next window down.
		if this is the only window, well then don't delete it ;) */
                if (num_windows == 1)
                        return 1;

                n = windows[selected_window].height + windows[selected_window + 1].height;

                /* shift the windows under the current one up */
                memmove(windows + selected_window, windows + selected_window + 1,
                        ((num_windows - selected_window - 1) * sizeof(*windows)));

                /* fix the current window's height */
                windows[selected_window].height = n;

                num_windows--;
                if (selected_window == num_windows)
                        selected_window--;
                break;
        case SDLK_PAGEUP:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state) return 1;
                n = windows[selected_window].type;
                if (n == 0)
                        n = NUM_WINDOW_TYPES;
                n--;
                windows[selected_window].type = n;
                break;
        case SDLK_PAGEDOWN:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state) return 1;
                windows[selected_window].type = (windows[selected_window].type + 1) % NUM_WINDOW_TYPES;
                break;
        case SDLK_TAB:
		if (k->state) return 1;
                if (k->mod & KMOD_SHIFT) {
                        if (selected_window == 0)
                                selected_window = num_windows;
                        selected_window--;
                } else {
                        selected_window = (selected_window + 1) % num_windows;
                }
                status.flags |= NEED_UPDATE;
                return 1;
	case SDLK_F9:
		if (k->state) return 1;
                if (k->mod & KMOD_ALT) {
			song_toggle_channel_mute(selected_channel - 1);
			orderpan_recheck_muted_channels();
			return 1;
		}
		return 0;
	case SDLK_F10:
                if (k->mod & KMOD_ALT) {
			if (k->state) return 1;
			song_handle_channel_solo(selected_channel - 1);
			orderpan_recheck_muted_channels();
			return 1;
		}
		return 0;
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
static void info_page_set(void)
{
	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

void info_load_page(struct page *page)
{
        page->title = "Info Page (F5)";
        page->playback_update = info_page_playback_update;
        page->total_widgets = 1;
        page->widgets = widgets_info;
        page->help_index = HELP_INFO_PAGE;
	page->set_page = info_page_set;

	create_other(widgets_info + 0, 0, info_page_handle_key, info_page_redraw);
}
