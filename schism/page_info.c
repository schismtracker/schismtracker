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
#include "vgamem.h"
#include "song.h"
#include "page.h"
#include "widget.h"
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
	const char *id;

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

static void info_draw_technical(int base, int height, int active, int first_channel)
{
	int smp, pos, fg, c = first_channel;
	char buf[16];
	const char *ptr;

	/*
	FVl - 0-128, final calculated volume, taking everything into account:
		(sample volume, sample global volume, instrument volume, inst. global volume,
		volume envelope, volume swing, fadeout, channel volume, song global volume, effects (I/Q/R)
	Vl - 0-64, sample volume / volume column (also affected by I/Q/R)
	CV - 0-64, channel volume (M/N)
	SV - 0-64, sample global volume + inst global volume
	Fde - 0-512, HALF the fade
		(initially 1024, and subtracted by instrument fade value each tick when fading out)
	Pn - 0-64 (or "Su"), final channel panning
		+ pan swing + pitch/pan + current pan envelope value! + Yxx
		(note: suggests that Xxx panning is reduced to 64 values when it's applied?)
	PE - 0-64, pan envelope
		note: this value is not changed if pan env is turned off (e.g. with S79) -- so it's copied
	all of the above are still set to valid values in sample mode
	*/

	draw_fill_chars(5, base + 1, 29, base + height - 2, DEFAULT_FG, 0);
	draw_box(4, base, 30, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_text("Frequency", 6, base, 2, 1);
	draw_text("Position", 17, base, 2, 1);
	draw_text("Smp", 27, base, 2, 1);

	draw_fill_chars(32, base + 1, 56, base + height - 2, DEFAULT_FG, 0);
	draw_box(31, base, 57, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_text("FVl", 32, base, 2, 1);
	draw_text("Vl",  36, base, 2, 1);
	draw_text("CV",  39, base, 2, 1);
	draw_text("SV",  42, base, 2, 1);
	draw_text("VE",  45, base, 2, 1);
	draw_text("Fde", 48, base, 2, 1);
	draw_text("Pn",  52, base, 2, 1);
	draw_text("PE",  55, base, 2, 1);

	if (song_is_instrument_mode()) {
		draw_fill_chars(59, base + 1, 65, base + height - 2, DEFAULT_FG, 0);
		draw_box(58, base, 66, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
		draw_text("NNA", 59, base, 2, 1);
		draw_text("Tot", 63, base, 2, 1);
	}

	for (pos = base + 1; pos < base + height - 1; pos++, c++) {
		song_channel_t *channel = current_song->channels + c - 1;
		song_voice_t *voice = current_song->voices + c - 1;

		if (c == selected_channel) {
			fg = (channel->flags & CHN_MUTE) ? 6 : 3;
		} else {
			if (channel->flags & CHN_MUTE)
				fg = 2;
			else
				fg = active ? 1 : 0;
		}
		draw_text(num99tostr(c, buf), 2, pos, fg, 2); /* channel number */

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
			draw_text("---\xa8", 59, pos, 2, 0); /* will be overwritten if something's playing */

			/* count how many voices claim this channel */
			int nv, tot;
			for (nv = tot = 0; nv < MAX_VOICES; nv++) {
				song_voice_t *v = current_song->voices + nv;
				if (v->master_channel == (unsigned int) c && v->current_sample_data && v->length)
					tot++;
			}
			if (voice->current_sample_data && voice->length)
				tot++;
			draw_text(numtostr(3, tot, buf), 63, pos, 2, 0);
		}

		if (voice->current_sample_data && voice->length && voice->ptr_sample) {
			// again with the hacks...
			smp = voice->ptr_sample - current_song->samples;
			if (smp <= 0 || smp >= MAX_SAMPLES)
				continue;
		} else {
			continue;
		}

		// Frequency
		sprintf(buf, "%10" PRIu32, voice->sample_freq);
		draw_text(buf, 5, pos, 2, 0);
		// Position
		sprintf(buf, "%10" PRIu32, voice->position);
		draw_text(buf, 16, pos, 2, 0);

		draw_text(numtostr(3, smp, buf), 27, pos, 2, 0); // Smp
		draw_text(numtostr(3, voice->final_volume / 128, buf), 32, pos, 2, 0); // FVl
		draw_text(numtostr(2, voice->volume >> 2, buf), 36, pos, 2, 0); // Vl
		draw_text(numtostr(2, voice->global_volume, buf), 39, pos, 2, 0); // CV
		draw_text(numtostr(2, voice->ptr_sample->global_volume, buf), 42, pos, 2, 0); // SV
        // FIXME: VE means volume envelope. Also, voice->instrument_volume is actually sample global volume
		draw_text(numtostr(2, voice->instrument_volume, buf), 45, pos, 2, 0); // VE
		draw_text(numtostr(3, voice->fadeout_volume / 128, buf), 48, pos, 2, 0); // Fde

		// Pn
		if (voice->flags & CHN_SURROUND)
			draw_text("Su", 52, pos, 2, 0);
		else
			draw_text(numtostr(2, voice->panning >> 2, buf), 52, pos, 2, 0);

		draw_text(numtostr(2, voice->final_panning >> 2, buf), 55, pos, 2, 0); // PE

		if (song_is_instrument_mode()) {
			switch (voice->nna) {
				case NNA_NOTECUT: ptr = "Cut"; break;
				case NNA_CONTINUE: ptr = "Con"; break;
				case NNA_NOTEOFF: ptr = "Off"; break;
				case NNA_NOTEFADE: ptr = "Fde"; break;
				default: ptr = "???"; break;
			};
			draw_text(ptr, 59, pos, 2, 0);
		}
	}
}

static void info_draw_samples(int base, int height, int active, int first_channel)
{
	int vu, smp, ins, n, pos, fg, fg2, c = first_channel;
	char buf[8];
	char *ptr;

	draw_fill_chars(5, base + 1, 28, base + height - 2, DEFAULT_FG, 0);
	draw_fill_chars(31, base + 1, 61, base + height - 2, DEFAULT_FG, 0);

	draw_box(4, base, 29, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(30, base, 62, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	if (song_is_stereo()) {
		draw_fill_chars(64, base + 1, 72, base + height - 2, DEFAULT_FG, 0);
		draw_box(63, base, 73, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	} else {
		draw_fill_chars(63, base, 73, base + height, DEFAULT_FG, 2);
	}

	if (song_get_mode() == MODE_STOPPED) {
		for (pos = base + 1; pos < base + height - 1; pos++, c++) {
			song_channel_t *channel = song_get_channel(c - 1);

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
		song_voice_t *voice = current_song->voices + c - 1;
		/* always draw the channel number */

		if (c == selected_channel) {
			fg = (voice->flags & CHN_MUTE) ? 6 : 3;
			draw_text(numtostr(2, c, buf), 2, pos, fg, 2);
		} else if (!(voice->flags & CHN_MUTE)) {
			fg = active ? 1 : 0;
			draw_text(numtostr(2, c, buf), 2, pos, fg, 2);
		}

		if (!(voice->current_sample_data && voice->length))
			continue;

		/* first box: vu meter */
		if (velocity_mode)
			vu = voice->final_volume >> 8;
		else
			vu = voice->vu_meter >> 2;
		if (voice->flags & CHN_MUTE) {
			fg = 1; fg2 = 2;
		} else {
			fg = 5; fg2 = 4;
		}
		draw_vu_meter(5, pos, 24, vu, fg, fg2);

		/* second box: sample number/name */
		ins = song_get_instrument_number(voice->ptr_instrument);
		/* figuring out the sample number is an ugly hack... considering all the crap that's
		copied to the channel, i'm surprised that the sample and instrument numbers aren't
		in there somewhere... */
		if (voice->ptr_sample)
			smp = voice->ptr_sample - current_song->samples;
		else
			smp = ins = 0;
		if(smp < 0 || smp >= MAX_SAMPLES)
			smp = ins = 0; /* This sample is not in the sample array */

		if (smp) {
			draw_text(num99tostr(smp, buf), 31, pos, 6, 0);
			if (ins) {
				draw_char('/', 33, pos, 6, 0);
				draw_text(num99tostr(ins, buf), 34, pos, 6, 0);
				n = 36;
			} else {
				n = 33;
			}
			if (voice->volume == 0)
				fg = 4;
			else if (voice->flags & (CHN_KEYOFF | CHN_NOTEFADE))
				fg = 7;
			else
				fg = 6;
			draw_char(':', n++, pos, fg, 0);
			if (instrument_names && voice->ptr_instrument) {
				ptr = voice->ptr_instrument->name;
			} else {
				ptr = current_song->samples[smp].name;
			}
			draw_text_len(ptr, 25, n, pos, 6, 0);
		} else if (ins && voice->ptr_instrument && voice->ptr_instrument->midi_channel_mask) {
			// XXX why? what?
			if (voice->ptr_instrument->midi_channel_mask >= 0x10000) {
				draw_text(numtostr(2, ((c-1) % 16)+1, buf), 31, pos, 6, 0);
			} else {
				int ch = 0;
				while(!(voice->ptr_instrument->midi_channel_mask & (1 << ch))) ++ch;
				draw_text(numtostr(2, ch, buf), 31, pos, 6, 0);
			}
			draw_char('/', 33, pos, 6, 0);
			draw_text(num99tostr(ins, buf), 34, pos, 6, 0);
			n = 36;
			if (voice->volume == 0)
				fg = 4;
			else if (voice->flags & (CHN_KEYOFF | CHN_NOTEFADE))
				fg = 7;
			else
				fg = 6;
			draw_char(':', n++, pos, fg, 0);
			ptr = voice->ptr_instrument->name;
			draw_text_len( ptr, 25, n, pos, 6, 0);
		} else {
			continue;
		}

		/* last box: panning. this one's much easier than the
		 * other two, thankfully :) */
		if (song_is_stereo()) {
			if (!voice->ptr_sample) {
				/* nothing... */
			} else if (voice->flags & CHN_SURROUND) {
				draw_text("Surround", 64, pos, 2, 0);
			} else if (voice->final_panning >> 2 == 0) {
				draw_text("Left", 64, pos, 2, 0);
			} else if ((voice->final_panning + 3) >> 2 == 64) {
				draw_text("Right", 68, pos, 2, 0);
			} else {
				draw_thumb_bar(64, pos, 9, 0, 256, voice->final_panning, 0);
			}
		}
	}
}

static void _draw_fill_notes(int col, int first_row, int height, int num_channels,
			     int channel_width, int separator, draw_note_func draw_note, int bg)
{
	int row_pos, chan_pos;

	for (row_pos = first_row; row_pos < first_row + height; row_pos++) {
		for (chan_pos = 0; chan_pos < num_channels - 1; chan_pos++) {
			draw_note(col + channel_width * chan_pos, row_pos, blank_note, -1, 6, bg);
			if (separator)
				draw_char(168, (col - 1 + channel_width * (chan_pos + 1)), row_pos, 2, bg);
		}
		draw_note(col + channel_width * chan_pos, row_pos, blank_note, -1, 6, bg);
	}
}

static void _draw_track_view(int base, int height, int first_channel, int num_channels,
			     int channel_width, int separator, draw_note_func draw_note)
{
	/* way too many variables */
	int current_row = song_get_current_row();
	int current_order = song_get_current_order();
	const song_note_t *note;
	// These can't be const because of song_get_pattern, but song_get_pattern is stupid and smells funny.
	song_note_t *cur_pattern, *prev_pattern, *next_pattern;
	const song_note_t *pattern; /* points to either {cur,prev,next}_pattern */
	int cur_pattern_rows = 0, prev_pattern_rows = 0, next_pattern_rows = 0;
	int total_rows; /* same as {cur,prev_next}_pattern_rows */
	int chan_pos, row, row_pos, rows_before;
	char buf[4];

	if (separator)
		channel_width++;

#if 0
	/* can't do this here -- each view does channel numbers differently, don't draw on top of them */
	draw_box(4, base, 5 + num_channels * channel_width - !!separator, base + height - 1,
		 BOX_THICK | BOX_INNER | BOX_INSET);
#endif

	switch (song_get_mode()) {
	case MODE_PATTERN_LOOP:
		prev_pattern_rows = next_pattern_rows = cur_pattern_rows
			= song_get_pattern(song_get_playing_pattern(), &cur_pattern);
		prev_pattern = next_pattern = cur_pattern;
		break;
	case MODE_PLAYING:
		if (current_song->orderlist[current_order] >= 200) {
			/* this does, in fact, happen. just pretend that
			 * it's stopped :P */
	default:
			/* stopped */
			draw_fill_chars(5, base + 1, 4 + num_channels * channel_width - !!separator,
					base + height - 2, DEFAULT_FG, 0);
			return;
		}
		cur_pattern_rows = song_get_pattern(current_song->orderlist[current_order], &cur_pattern);
		if (current_order > 0 && current_song->orderlist[current_order - 1] < 200)
			prev_pattern_rows = song_get_pattern(current_song->orderlist[current_order - 1],
								&prev_pattern);
		else
			prev_pattern = NULL;
		if (current_order < 255 && current_song->orderlist[current_order + 1] < 200)
			next_pattern_rows = song_get_pattern(current_song->orderlist[current_order + 1],
								&next_pattern);
		else
			next_pattern = NULL;
		break;
	}

	/* -2 for the top and bottom border, -1 because if there are an even number
	 * of rows visible, the current row is drawn above center. */
	rows_before = (height - 3) / 2;

	/* "fake" channels (hack for 64-channel view) */
	if (num_channels > 64) {
		_draw_fill_notes(5 + 64, base + 1, height - 2,
				 num_channels - 64, channel_width, separator, draw_note, 0);
		_draw_fill_notes(5 + 64, base + 1 + rows_before, 1,
				 num_channels - 64, channel_width, separator, draw_note, 14);
		num_channels = 64;
	}

	/* draw the area above the current row */
	pattern = cur_pattern;
	total_rows = cur_pattern_rows;
	row = current_row - 1;
	row_pos = base + rows_before;
	while (row_pos > base) {
		if (row < 0) {
			if (prev_pattern == NULL) {
				_draw_fill_notes(5, base + 1, row_pos - base,
						 num_channels, channel_width, separator, draw_note, 0);
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
				_draw_fill_notes(5, row_pos, base + height - row_pos - 1,
						 num_channels, channel_width, separator, draw_note, 0);
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

	draw_box(4, base, 74, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	for (chan = first_channel, chan_pos = 0; chan_pos < 5; chan++, chan_pos++) {
		if (current_song->channels[chan - 1].flags & CHN_MUTE)
			fg = (chan == selected_channel ? 6 : 1);
		else
			fg = (chan == selected_channel ? 3 : (active ? 2 : 0));
		draw_channel_header_13(chan, 5 + 14 * chan_pos, base, fg);
	}
	_draw_track_view(base, height, first_channel, 5, 13, 1, draw_note_13);
}

static void info_draw_track_8(int base, int height, int active, int first_channel)
{
	int chan, chan_pos, fg;
	char buf[4];

	draw_box(4, base, 76, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	for (chan = first_channel, chan_pos = 0; chan_pos < 8; chan++, chan_pos++) {
		if (current_song->channels[chan - 1].flags & CHN_MUTE)
			fg = (chan == selected_channel ? 6 : 1);
		else
			fg = (chan == selected_channel ? 3 : (active ? 2 : 0));
		draw_char(0, 6 + 9 * chan_pos, base, 1, 1);
		draw_char(0, 6 + 9 * chan_pos + 1, base, 1, 1);
		draw_text(numtostr(2, chan, buf), 6 + 9 * chan_pos + 2, base, fg, 1);
		draw_char(0, 6 + 9 * chan_pos + 4, base, 1, 1);
		draw_char(0, 6 + 9 * chan_pos + 5, base, 1, 1);
	}
	_draw_track_view(base, height, first_channel, 8, 8, 1, draw_note_8);
}

static void info_draw_track_10(int base, int height, int active, int first_channel)
{
	int chan, chan_pos, fg;
	char buf[4];

	draw_box(4, base, 75, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	for (chan = first_channel, chan_pos = 0; chan_pos < 10; chan++, chan_pos++) {
		if (current_song->channels[chan - 1].flags & CHN_MUTE)
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

	draw_box(4, base, 77, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	for (chan = first_channel, chan_pos = 0; chan_pos < 12; chan++, chan_pos++) {
		if (current_song->channels[chan - 1].flags & CHN_MUTE)
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

	draw_box(4, base, 76, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	for (chan = first_channel, chan_pos = 0; chan_pos < 18; chan++, chan_pos++) {
		if (current_song->channels[chan - 1].flags & CHN_MUTE)
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

	draw_box(4, base, 77, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	for (chan = first_channel, chan_pos = 0; chan_pos < 24; chan++, chan_pos++) {
		if (current_song->channels[chan - 1].flags & CHN_MUTE)
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

	draw_box(4, base, 77, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	for (chan = first_channel, chan_pos = 0; chan_pos < 36; chan++, chan_pos++) {
		if (current_song->channels[chan - 1].flags & CHN_MUTE)
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

	draw_box(4, base, nchan + 5, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);
	for (chan = first_channel, chan_pos = 0; chan_pos < 64; chan++, chan_pos++) {
		if (current_song->channels[chan - 1].flags & CHN_MUTE)
			fg = (chan == selected_channel ? 14 : 9);
		else
			fg = (chan == selected_channel ? 3 : (active ? 10 : 8));
		draw_half_width_chars(chan / 10 + '0', chan % 10 + '0', 5 + chan_pos, base, fg, 1, fg, 1);
	}
	for (; chan_pos < nchan; chan_pos++)
		draw_char(0, 5 + chan_pos, base, 1, 1);

	_draw_track_view(base, height, first_channel, nchan, 1, 0, draw_note_1);
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
	int fg, v;
	int c, pos;
	int n;
	song_voice_t *voice;
	char buf[4];
	uint8_t d, dn;
	uint8_t dot_field[73][36] = { {0} }; // f#2 -> f#8 = 73 columns

	draw_fill_chars(5, base + 1, 77, base + height - 2, DEFAULT_FG, 0);
	draw_box(4, base, 78, base + height - 1, BOX_THICK | BOX_INNER | BOX_INSET);

	n = current_song->num_voices;
	while (n--) {
		voice = current_song->voices + current_song->voice_mix[n];

		/* 31 = f#2, 103 = f#8. (i hope ;) */
		if (!(voice->ptr_sample && voice->note >= 31 && voice->note <= 103))
			continue;
		pos = voice->master_channel ? voice->master_channel : (1 + current_song->voice_mix[n]);
		if (pos < first_channel)
			continue;
		pos -= first_channel;
		if (pos > height - 1)
			continue;

		fg = (voice->flags & CHN_MUTE) ? 1 : ((voice->ptr_sample - current_song->samples) % 4 + 2);

		if (velocity_mode || (status.flags & CLASSIC_MODE))
			v = (voice->final_volume + 2047) >> 11;
		else
			v = (voice->vu_meter + 31) >> 5;
		d = dot_field[voice->note - 31][pos];
		dn = (v << 4) | fg;
		if (dn > d)
			dot_field[voice->note - 31][pos] = dn;
	}

	for (c = first_channel, pos = 0; pos < height - 2; pos++, c++) {
		for (n = 0; n < 73; n++) {
			d = dot_field[n][pos] ? dot_field[n][pos] : 0x06;

			fg = d & 0xf;
			v = d >> 4;
			draw_char(v + 193, n + 5, pos + base + 1, fg, 0);
		}

		if (c == selected_channel) {
			fg = (current_song->channels[c - 1].flags & CHN_MUTE) ? 6 : 3;
		} else {
			if (current_song->channels[c - 1].flags & CHN_MUTE)
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

static void click_chn_nil(UNUSED int x, UNUSED int y, UNUSED int nc, UNUSED int fc)
{
	/* do nothing */
}

/* --------------------------------------------------------------------- */
/* declarations of the window types */

#define TRACK_VIEW(n) {"track" # n, info_draw_track_##n, click_chn_is_x, 1, n}
static const struct info_window_type window_types[] = {
	{"samples", info_draw_samples, click_chn_is_y_nohead, 0, -2},
	TRACK_VIEW(5),
	TRACK_VIEW(8),
	TRACK_VIEW(10),
	TRACK_VIEW(12),
	TRACK_VIEW(18),
	TRACK_VIEW(24),
	TRACK_VIEW(36),
	TRACK_VIEW(64),
	{"global", info_draw_channels, click_chn_nil, 1, 0},
	{"dots", info_draw_note_dots, click_chn_is_y_nohead, 0, -2},
	{"tech", info_draw_technical, click_chn_is_y, 1, -2},
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
			/* crappy hack (to squeeze in an extra row on the top window) */
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
/* settings */

void cfg_save_info(cfg_file_t *cfg)
{
	// for 5 windows, roughly 12 chars per window, this is way more than enough
	char buf[256] = "";
	char *s = buf;
	int rem = sizeof(buf) - 1;
	int len;
	int i;

	for (i = 0; i < num_windows; i++) {
		len = snprintf(s, rem, " %s %d", window_types[windows[i].type].id, windows[i].height);
		if (!len) {
			// this should not ever happen
			break;
		}
		rem -= len;
		s += len;
	}
	buf[255] = '\0';

	// (don't write the first space to the config)
	cfg_set_string(cfg, "Info Page", "layout", buf + 1);
}

static void cfg_load_info_old(cfg_file_t *cfg)
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
		if (windows[i].type >= 2) {
			// compensate for added 8-channel view
			windows[i].type++;
		}
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
		windows[0].type = 0;    /* samples */
		windows[0].height = 19;
		windows[1].type = 9;    /* active channels */
		windows[1].height = 3;
		windows[2].type = 6;    /* 24chn track view */
		windows[2].height = 15;
	}

	for (i = 0; i < num_windows; i++) {
		windows[i].first_channel = 1;
	}

	recalculate_windows();
	if (status.current_page == PAGE_INFO)
		status.flags |= NEED_UPDATE;
}

void cfg_load_info(cfg_file_t *cfg)
{
	int n;
	char buf[256];
	char *left, *right;
	size_t len;

	if (!cfg_get_string(cfg, "Info Page", "layout", buf, 255, NULL)) {
		cfg_load_info_old(cfg);
		return;
	}

	left = buf;
	num_windows = 0;
	do {
		left += strspn(left, " \t");
		len = strcspn(left, " \t");
		if (!len) {
			break;
		}
		left[len] = '\0'; // chop it into pieces
		windows[num_windows].first_channel = 1;
		windows[num_windows].type = -1;
		for (n = 0; n < NUM_WINDOW_TYPES; n++) {
			if (strcasecmp(window_types[n].id, left) == 0) {
				windows[num_windows].type = n;
				break;
			}
		}
		// (a pythonic for...else would be lovely right about here)
		if (windows[num_windows].type == -1) {
			break;
		}
		right = left + len + 1;
		left[len] = '\0';
		n = strtol(right, &left, 10);
		if (!left || left == right || n < 3) {
			// failed to parse any digits, or number is too small
			break;
		}
		windows[num_windows++].height = n;
	} while (num_windows < MAX_WINDOWS - 1);

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
					n = current_song->orderlist[order];
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
	if (k->orig_sym == SDLK_KP_9) {
		k->sym = SDLK_F9;
	} else if (k->orig_sym == SDLK_KP_0) {
		k->sym = SDLK_F10;
	}

	switch (k->sym) {
	case SDLK_g:
		if (k->state == KEY_PRESS)
			return 1;

		set_current_channel(selected_channel);
		order = song_get_current_order();

		if (song_get_mode() == MODE_PLAYING) {
			n = current_song->orderlist[order];
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
		if (k->state == KEY_RELEASE)
			return 1;

		velocity_mode = !velocity_mode;
		status_text_flash("Using %s bars", (velocity_mode ? "velocity" : "volume"));
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_i:
		if (k->state == KEY_RELEASE)
			return 1;

		instrument_names = !instrument_names;
		status_text_flash("Using %s names", (instrument_names ? "instrument" : "sample"));
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_r:
		if (k->mod & KMOD_ALT) {
			if (k->state == KEY_RELEASE)
				return 1;

			song_flip_stereo();
			status_text_flash("Left/right outputs reversed");
			return 1;
		}
		return 0;
	case SDLK_PLUS:
		if (k->state == KEY_RELEASE)
			return 1;
		if (song_get_mode() == MODE_PLAYING) {
			song_set_current_order(song_get_current_order() + 1);
		}
		return 1;
	case SDLK_MINUS:
		if (k->state == KEY_RELEASE)
			return 1;
		if (song_get_mode() == MODE_PLAYING) {
			song_set_current_order(song_get_current_order() - 1);
		}
		return 1;
	case SDLK_q:
		if (k->state == KEY_RELEASE)
			return 1;
		song_toggle_channel_mute(selected_channel - 1);
		orderpan_recheck_muted_channels();
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_s:
		if (k->state == KEY_RELEASE)
			return 1;

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

		if (k->state == KEY_RELEASE)
			return 1;
		song_toggle_channel_mute(selected_channel - 1);
		if (selected_channel < 64)
			selected_channel++;
		orderpan_recheck_muted_channels();
		break;
	case SDLK_UP:
		if (k->state == KEY_RELEASE)
			return 1;
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
		if (!NO_MODIFIER(k->mod) && !(k->mod & KMOD_ALT))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		if (selected_channel > 1)
			selected_channel--;
		break;
	case SDLK_DOWN:
		if (k->state == KEY_RELEASE)
			return 1;
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
		if (!NO_MODIFIER(k->mod) && !(k->mod & KMOD_ALT))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		if (selected_channel < 64)
			selected_channel++;
		break;
	case SDLK_HOME:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		selected_channel = 1;
		break;
	case SDLK_END:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		selected_channel = song_find_last_channel();
		break;
	case SDLK_INSERT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
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
		if (k->state == KEY_RELEASE)
			return 1;
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
		if (k->state == KEY_RELEASE)
			return 1;
		n = windows[selected_window].type;
		if (n == 0)
			n = NUM_WINDOW_TYPES;
		n--;
		windows[selected_window].type = n;
		break;
	case SDLK_PAGEDOWN:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		windows[selected_window].type = (windows[selected_window].type + 1) % NUM_WINDOW_TYPES;
		break;
	case SDLK_TAB:
		if (k->state == KEY_RELEASE)
			return 1;
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
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_ALT) {
			song_toggle_channel_mute(selected_channel - 1);
			orderpan_recheck_muted_channels();
			return 1;
		}
		return 0;
	case SDLK_F10:
		if (k->mod & KMOD_ALT) {
			if (k->state == KEY_RELEASE)
				return 1;
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
	if (song_get_mode() != MODE_STOPPED)
		status.flags |= NEED_UPDATE;
}

void info_load_page(struct page *page)
{
	page->title = "Info Page (F5)";
	page->playback_update = info_page_playback_update;
	page->total_widgets = 1;
	page->widgets = widgets_info;
	page->help_index = HELP_INFO_PAGE;

	widget_create_other(widgets_info + 0, 0, info_page_handle_key, NULL, info_page_redraw);
}
