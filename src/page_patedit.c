/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * URL: http://rigelseven.com/schism/
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

/* The all-important pattern editor. The code here is a general mess, so
 * don't look at it directly or, uh, you'll go blind or something. */

#include "headers.h"

#include "it.h"
#include "page.h"
#include "song.h"
#include "pattern-view.h"
#include "config-parser.h"

#include <SDL.h>
#include <ctype.h>

/* --------------------------------------------------------------------------------------------------------- */

/* this is actually used by pattern-view.c */
int show_default_volumes = 0;

/* --------------------------------------------------------------------- */
/* The (way too many) static variables */

/* only one item, but MAN is it complicated :) */
static struct item items_pattern[1];

/* pattern display position */
static int top_display_channel = 1;		/* one-based */
static int top_display_row = 0;		/* zero-based */

/* these three tell where the cursor is in the pattern */
static int current_channel = 1, current_position = 0, current_row = 0;

/* this is, of course, what the current pattern is */
static int current_pattern = 0;

static int skip_value = 1;		/* aka cursor step */

static int link_effect_column = 0;
static int draw_divisions = 0;		/* = vertical lines between channels */

static int centralise_cursor = 0;
static int highlight_current_row = 0;
static int playback_tracing = 0;	/* scroll lock */

static int panning_mode = 0;		/* for the volume column */

/* Maybe this should be in a header file. */
#define ROW_IS_MAJOR(r) (row_highlight_major != 0 && (r) % row_highlight_major == 0)
#define ROW_IS_MINOR(r) (row_highlight_minor != 0 && (r) % row_highlight_minor == 0)
#define ROW_IS_HIGHLIGHT(r) (ROW_IS_MINOR(r) || ROW_IS_MAJOR(r))

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
static song_note mask_note = { 61, 0, 0, 0, 0, 0 };	/* C-5 */

/* playback mark (ctrl-f7) */
static int marked_pattern = -1, marked_row;

/* volume stuff (alt-i, alt-j, ctrl-j) */
static int volume_percent = 100;
static int fast_volume_percent = 67;
static int fast_volume_mode = 0;	/* toggled with ctrl-j */

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
	TRACK_VIEW(13),			/* 5 channels */
	TRACK_VIEW(10),			/* 6/7 channels */
	TRACK_VIEW(7),			/* 9/10 channels */
	TRACK_VIEW(6),			/* 10/12 channels */
	TRACK_VIEW(3),			/* 18/24 channels */
	TRACK_VIEW(2),			/* 24/36 channels */
	TRACK_VIEW(1),			/* 36/64 channels */
#undef  TRACK_VIEW
};

#define NUM_TRACK_VIEWS ARRAY_SIZE(track_views)

static byte track_view_scheme[64];
static int visible_channels, visible_width;

static void recalculate_visible_area(void);
static void set_view_scheme(int scheme);
static void pattern_editor_reposition(void);

/* --------------------------------------------------------------------------------------------------------- */
/* options dialog */

static struct item options_items[8];
static int options_link_split[] = { 5, 6, -1 };
static int options_selected_item = 0;

static void options_close(void)
{
	int old_size, new_size;
	
	options_selected_item = *selected_item;
	
	skip_value = options_items[1].thumbbar.value;
	row_highlight_minor = options_items[2].thumbbar.value;
	row_highlight_major = options_items[3].thumbbar.value;
	link_effect_column = !!(options_items[5].togglebutton.state);
	
	old_size = song_get_pattern(current_pattern, NULL);
	new_size = options_items[4].thumbbar.value;
	if (old_size != new_size) {
		song_pattern_resize(current_pattern, new_size);
		current_row = MIN(current_row, new_size - 1);
		pattern_editor_reposition();
	}
	
	/* TODO: change the number of rows if applicable.
	
	this needs to make sure the pattern is not shrunk when it's being used. if the rows are decreased,
	set a flag that gets checked on pattern change (both playing and editing) -- when the affected
	pattern isn't being played *and* it's also not the current pattern in the pattern editor, the size
	can be changed. also, the playing pattern shouldn't pay any attention to the actual size of the
	pattern once it's been set: if the pattern is resized, that only affects the area which can be
	edited until the pattern is no longer being accessed by the player or the editor.
	
	this is all very confusing, and i'll have to give it a lot of thought before i can implement it
	properly.
	
	... hmm, coming back to this, sounds kind of like i'd be keeping some sort of reference count on the
	pattern. maybe the way to do this would be:
		- when the number of rows in the pattern is decreased, add that pattern to a resize queue
		- the editor should set a bit (somewhere...) indicating that it's being edited
		- the player should set another bit (somewhere...) indicating that it's being played
		- whenever the currently playing/edited pattern changes, see if any patterns are waiting to
		  be resized, and if so, resize any of them that have both bits cleared (somewhere...)
		- need to make sure the right thing happens when resizing a pattern that's already in the
		  queue (especially when changing it so that it'd eventually be *bigger* -- this would mean
		  it could be removed from the queue altogether) */
}

static void options_draw_const(void)
{
	SDL_LockSurface(screen);
	
	draw_text_unlocked("Pattern Editor Options", 28, 19, 0, 2);
	draw_text_unlocked("Base octave", 28, 23, 0, 2);
	draw_text_unlocked("Cursor step", 28, 26, 0, 2);
	draw_text_unlocked("Row hilight minor", 22, 29, 0, 2);
	draw_text_unlocked("Row hilight major", 22, 32, 0, 2);
	draw_text_unlocked("Number of rows in pattern", 14, 35, 0, 2);
	draw_text_unlocked("Command/Value columns", 18, 38, 0, 2);
	
	draw_box_unlocked(39, 22, 42, 24, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box_unlocked(39, 25, 43, 27, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box_unlocked(39, 28, 45, 30, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box_unlocked(39, 31, 57, 33, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box_unlocked(39, 34, 62, 36, BOX_THIN | BOX_INNER | BOX_INSET);
	
	SDL_UnlockSurface(screen);
}

static int options_handle_key(SDL_keysym *k)
{
	if (NO_MODIFIER(k->mod) && k->sym == SDLK_F2) {
		dialog_cancel();
		return 1;
	}
	return 0;
}

static void options_change_base_octave(void)
{
	kbd_set_current_octave(options_items[0].thumbbar.value);
}

/* the base octave is changed directly when the thumbbar is changed.
 * anything else can wait until the dialog is closed. */
void pattern_editor_display_options(void)
{
	struct dialog *dialog;
	
	create_thumbbar(options_items + 0, 40, 23, 2, 7, 1, 1, options_change_base_octave, 0, 8);
	create_thumbbar(options_items + 1, 40, 26, 3, 0, 2, 2, NULL, 0, 16);
	create_thumbbar(options_items + 2, 40, 29, 5, 1, 3, 3, NULL, 0, 32);
	create_thumbbar(options_items + 3, 40, 32, 17, 2, 4, 4, NULL, 0, 128);
	create_thumbbar(options_items + 4, 40, 35, 22, 3, 5, 5, NULL, 32, 200);
	create_togglebutton(options_items + 5, 40, 38, 8, 4, 7, 6, 6, 6,
			    NULL, "Link", 3, options_link_split);
	create_togglebutton(options_items + 6, 52, 38, 9, 4, 7, 5, 5, 5,
			    NULL, "Split", 3, options_link_split);
	create_button(options_items + 7, 35, 41, 8, 5, 0, 7, 7, 7, dialog_yes, "Done", 3);
	
	options_items[0].thumbbar.value = kbd_get_current_octave();
	options_items[1].thumbbar.value = skip_value;
	options_items[2].thumbbar.value = row_highlight_minor;
	options_items[3].thumbbar.value = row_highlight_major;
	options_items[4].thumbbar.value = song_get_pattern(current_pattern, NULL);
	togglebutton_set(options_items, link_effect_column ? 5 : 6, 0);
	
	dialog = dialog_create_custom(10, 18, 60, 26, options_items, 8, options_selected_item,
				      options_draw_const);
	dialog->action_yes = options_close;
	dialog->action_cancel = options_close;
	dialog->handle_key = options_handle_key;
}

/* --------------------------------------------------------------------------------------------------------- */
/* volume fiddling */

static void selection_amplify(int percentage);

/* --------------------------------------------------------------------------------------------------------- */
/* volume amplify/attenuate and fast volume setup handlers */

/* this is shared by the fast and normal volume dialogs */
static struct item volume_setup_items[3];

static void fast_volume_setup_ok(void)
{
	fast_volume_percent = volume_setup_items[0].thumbbar.value;
	fast_volume_mode = 1;
	status_text_flash("Alt-I / Alt-J fast volume changes enabled");
}

static void fast_volume_setup_cancel(void)
{
	status_text_flash("Alt-I / Alt-J fast volume changes not enabled");
}

static void fast_volume_setup_draw_const(void)
{
	draw_text("Volume Amplification %", 29, 27, 0, 2);
	draw_box(32, 29, 44, 31, BOX_THIN | BOX_INNER | BOX_INSET);
}

static void fast_volume_toggle(void)
{
	struct dialog *dialog;
	
	if (fast_volume_mode) {
		fast_volume_mode = 0;
		status_text_flash("Alt-I / Alt-J fast volume changes disabled");
	} else {
		create_thumbbar(volume_setup_items + 0, 33, 30, 11,
				0, 1, 1, NULL, 10, 90);
		volume_setup_items[0].thumbbar.value = fast_volume_percent;
		create_button(volume_setup_items + 1, 31, 33, 6,
			      0, 1, 2, 2, 2, dialog_yes, "OK", 3);
		create_button(volume_setup_items + 2, 41, 33, 6,
			      0, 2, 1, 1, 1, dialog_cancel, "Cancel", 1);
		
		dialog = dialog_create_custom(22, 25, 36, 11, volume_setup_items,
					      3, 0, fast_volume_setup_draw_const);
		dialog->action_yes = fast_volume_setup_ok;
		dialog->action_cancel = fast_volume_setup_cancel;
	}
}

static void fast_volume_amplify(void)
{
	status_text_flash("TODO: fast volume amplify %d%%", fast_volume_percent);
}

static void fast_volume_attenuate(void)
{
	status_text_flash("TODO: fast volume attenuate %d%%", fast_volume_percent);
}

/* --------------------------------------------------------------------------------------------------------- */
/* normal (not fast volume) amplify */

static void volume_setup_draw_const(void)
{
	draw_text("Volume Amplification %", 29, 27, 0, 2);
	draw_box(25, 29, 52, 31, BOX_THIN | BOX_INNER | BOX_INSET);
}

static void volume_amplify_ok(void)
{
	volume_percent = volume_setup_items[0].thumbbar.value;
	
	selection_amplify(volume_percent);
}

static void volume_amplify(void)
{
	struct dialog *dialog;
	
	create_thumbbar(volume_setup_items + 0, 26, 30, 26,
			0, 1, 1, NULL, 0, 200);
	volume_setup_items[0].thumbbar.value = volume_percent;
	create_button(volume_setup_items + 1, 31, 33, 6,
			      0, 1, 2, 2, 2, dialog_yes, "OK", 3);
	create_button(volume_setup_items + 2, 41, 33, 6,
		      0, 2, 1, 1, 1, dialog_cancel, "Cancel", 1);
	
	dialog = dialog_create_custom(22, 25, 36, 11, volume_setup_items,
				      3, 0, volume_setup_draw_const);
	dialog->action_yes = volume_amplify_ok;
}

/* --------------------------------------------------------------------------------------------------------- */
/* settings */

#define CFG_SET_PE(v) cfg_set_number(cfg, "Pattern Editor", #v, v)
void cfg_save_patedit(cfg_file_t *cfg)
{
	int n;
	char s[65];
	
	for (n = 0; n < 64; n++)
		s[n] = track_view_scheme[n] + 'a';
	s[64] = 0;
	
	CFG_SET_PE(link_effect_column);
	CFG_SET_PE(draw_divisions);
	CFG_SET_PE(centralise_cursor);
	CFG_SET_PE(highlight_current_row);
	CFG_SET_PE(mask_fields);
	CFG_SET_PE(volume_percent);
	CFG_SET_PE(fast_volume_percent);
	CFG_SET_PE(fast_volume_mode);
	cfg_set_string(cfg, "Pattern Editor", "track_view_scheme", s);
}

#define CFG_GET_PE(v,d) v = cfg_get_number(cfg, "Pattern Editor", #v, d)
void cfg_load_patedit(cfg_file_t *cfg)
{
	int n, r = 0;
	byte s[65];
	
	CFG_GET_PE(link_effect_column, 0);
	CFG_GET_PE(draw_divisions, 1);
	CFG_GET_PE(centralise_cursor, 0);
	CFG_GET_PE(highlight_current_row, 0);
	CFG_GET_PE(mask_fields, MASK_INSTRUMENT | MASK_VOLUME);
	CFG_GET_PE(volume_percent, 100);
	CFG_GET_PE(fast_volume_percent, 67);
	CFG_GET_PE(fast_volume_mode, 0);
	cfg_get_string(cfg, "Pattern Editor", "track_view_scheme", s, 65, "a");
	
	/* "decode" the track view scheme */
	for (n = 0; n < 64; n++) {
		if (s[n] == '\0') {
			/* end of the string */
			break;
		} else if (s[n] >= 'a' && s[n] <= 'z') {
			s[n] -= 'a';
		} else if (s[n] >= 'A' && s[n] <= 'Z') {
			s[n] -= 'A';
		} else {
			log_appendf(4, "Track view scheme corrupted; using default");
			n = 64;
			r = 0;
			break;
		}
		r = s[n];
	}
	memcpy(track_view_scheme, s, n);
	if (n < 64)
		memset(track_view_scheme + n, r, 64 - n);
	
	recalculate_visible_area();
	pattern_editor_reposition();
	if (status.current_page == PAGE_PATTERN_EDITOR)
		status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* selection handling functions */

static inline int is_in_selection(int chan, int row)
{
	return (SELECTION_EXISTS
		&& chan >= selection.first_channel && chan <= selection.last_channel
		&& row >= selection.first_row && row <= selection.last_row);
}

static void normalise_block_selection(void)
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
		normalise_block_selection();
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
		memset(pattern + 64 * selection.first_row, 0, (selection.last_row - selection.first_row + 1)
		       * 64 * sizeof(song_note));
	} else {
		chan_width = selection.last_channel - selection.first_channel + 1;
		for (row = selection.first_row; row <= selection.last_row; row++) {
			note = pattern + 64 * row + selection.first_channel - 1;
			memset(note, 0, chan_width * sizeof(song_note));
		}
	}
}

static void selection_set_sample(void)
{
	int row, chan;
	song_note *pattern, *note;

	song_get_pattern(current_pattern, &pattern);

	if (SELECTION_EXISTS) {
		for (row = selection.first_row; row <= selection.last_row; row++) {
			note = pattern + 64 * row + selection.first_channel - 1;
			for (chan = selection.first_channel; chan <= selection.last_channel; chan++, note++) {
				if (note->instrument) {
					note->instrument = song_get_current_instrument();
				}
			}
		}
	} else {
		note = pattern + 64 * current_row + current_channel - 1;
		if (note->instrument) {
			note->instrument = song_get_current_instrument();
		}
	}
}

static void selection_swap(void)
{
	/* s_note = selection; p_note = position */
	song_note *pattern, *s_note, *p_note, tmp;
	int row, chan, num_rows, num_chans, total_rows;
	
	/* TODO: make this a macro or something, it's used in a lot of different places */
	if (!SELECTION_EXISTS) {
		/* this should be one column wider (with the extra column on the left) */
		dialog_create(DIALOG_OK, "No block is marked", NULL, NULL, 0);
		return;
	}

	total_rows = song_get_pattern(current_pattern, &pattern);
	num_rows = selection.last_row - selection.first_row + 1;
	num_chans = selection.last_channel - selection.first_channel + 1;

	if (current_row + num_rows > total_rows || current_channel + num_chans - 1 > 64) {
		/* again: one column wider */
		dialog_create(DIALOG_OK, "Out of pattern range", NULL, NULL, 0);
		return;
	}

	/* The minimum combined size for the two blocks is double the number of rows in the selection by
	 * double the number of channels. So, if the width and height don't add up, they must overlap. It's
	 * of course possible to have the blocks adjacent but not overlapping -- there is only overlap if
	 * *both* the width and height are less than double the size. */
	if ((MAX(selection.last_channel, current_channel + num_chans - 1)
	     - MIN(selection.first_channel, current_channel) + 1) < 2 * num_chans
	    && (MAX(selection.last_row, current_row + num_rows - 1)
		- MIN(selection.first_row, current_row) + 1) < 2 * num_rows) {
		/* one column wider; the text should be shifted a column left as well */
		dialog_create(DIALOG_OK, "Swap blocks overlap", NULL, NULL, 0);
		return;
	}

	for (row = 0; row < num_rows; row++) {
		s_note = pattern + 64 * (selection.first_row + row) + selection.first_channel - 1;
		p_note = pattern + 64 * (current_row + row) + current_channel - 1;
		for (chan = 0; chan < num_chans; chan++, s_note++, p_note++) {
			tmp = *s_note;
			*s_note = *p_note;
			*p_note = tmp;
		}
	}
}

static void selection_set_volume(void)
{
	int row, chan;
	song_note *pattern, *note;

	if (!SELECTION_EXISTS) {
		/* this should be one column wider (see selection_swap) */
		dialog_create(DIALOG_OK, "No block is marked", NULL, NULL, 0);
		return;
	}
	
	song_get_pattern(current_pattern, &pattern);

	for (row = selection.first_row; row <= selection.last_row; row++) {
		note = pattern + 64 * row + selection.first_channel - 1;
		for (chan = selection.first_channel; chan <= selection.last_channel; chan++, note++) {
			note->volume = mask_note.volume;
			note->volume_effect = mask_note.volume_effect;
		}
	}
}

/* The logic for this one makes my head hurt. */
static void selection_slide_volume(void)
{
	int row, chan;
	song_note *pattern, *note, *last_note;
	int first, last;		/* the volumes */
	int ve, lve;			/* volume effect */
	
	/* FIXME: if there's no selection, should this display a dialog, or bail silently? */
	if (!SELECTION_EXISTS) {
		/* this should be one column wider (with the extra column on the left) */
		dialog_create(DIALOG_OK, "No block is marked", NULL, NULL, 0);
		return;
	}
	
	/* can't slide one row */
	if (selection.first_row == selection.last_row)
		return;
	
	song_get_pattern(current_pattern, &pattern);
	
	/* the channel loop has to go on the outside for this one */
	for (chan = selection.first_channel; chan <= selection.last_channel; chan++) {
		note = pattern + 64 * selection.first_row + chan - 1;
		last_note = pattern + 64 * selection.last_row + chan - 1;
		
		/* valid combinations:
		 *     [ volume - volume ]
		 *     [panning - panning]
		 *     [ volume - none   ] \ only valid if the 'none'
		 *     [   none - volume ] / note has a sample number
		 * in any other case, no slide occurs. */
		
		ve = note->volume_effect;
		lve = last_note->volume_effect;
		
		first = note->volume;
		last = last_note->volume;
		
		/* Note: IT only uses the sample's default volume
		 * if there is an instrument number *AND* a note.
		 * I'm just checking the instrument number, as it's
		 * the minimal information needed to get the default
		 * volume for the instrument ;)
		 * 
		 * Would be nice but way hard to do: if there's a note
		 * but no sample number, look back in the pattern and
		 * use the last sample number in that channel (if there
		 * is one.) */
		if (ve == VOL_EFFECT_NONE) {
			if (note->instrument == 0)
				continue;
			ve = VOL_EFFECT_VOLUME;
			/* Modplug hack: volume bit shift */
			first = song_get_sample(note->instrument, NULL)->volume >> 2;
		}
		
		if (lve == VOL_EFFECT_NONE) {
			if (last_note->instrument == 0)
				continue;
			lve = VOL_EFFECT_VOLUME;
			last = song_get_sample(last_note->instrument, NULL)->volume >> 2;
		}
		
		if (!(ve == lve && (ve == VOL_EFFECT_VOLUME
				    || ve == VOL_EFFECT_PANNING))) {
			continue;
		}
		
		for (row = selection.first_row; row <= selection.last_row; row++, note += 64) {
			note->volume_effect = ve;
			note->volume = (((last - first)
					 * (row - selection.first_row)
					 / (selection.last_row - selection.first_row)
					 ) + first);
		}
	}
}

static void selection_wipe_volume(int reckless)
{
	int row, chan;
	song_note *pattern, *note;

	if (!SELECTION_EXISTS) {
		/* this should be one column wider (see selection_swap) */
		dialog_create(DIALOG_OK, "No block is marked", NULL, NULL, 0);
		return;
	}
	
	song_get_pattern(current_pattern, &pattern);
	
	for (row = selection.first_row; row <= selection.last_row; row++) {
		note = pattern + 64 * row + selection.first_channel - 1;
		for (chan = selection.first_channel; chan <= selection.last_channel; chan++, note++) {
			if (reckless || (note->note == 0 && note->instrument == 0)) {
				note->volume = 0;
				note->volume_effect = VOL_EFFECT_NONE;
			}
		}
	}
}

static void selection_amplify(int percentage)
{
	int row, chan, volume;
	song_note *pattern, *note;

	if (!SELECTION_EXISTS) {
		/* this should be one column wider (see selection_swap) */
		dialog_create(DIALOG_OK, "No block is marked", NULL, NULL, 0);
		return;
	}

	song_get_pattern(current_pattern, &pattern);

	for (row = selection.first_row; row <= selection.last_row; row++) {
		note = pattern + 64 * row + selection.first_channel - 1;
		for (chan = selection.first_channel; chan <= selection.last_channel; chan++, note++) {
			if (note->volume_effect == VOL_EFFECT_NONE && note->instrument != 0) {
				/* Modplug hack: volume bit shift */
				volume = song_get_sample(note->instrument, NULL)->volume >> 2;
				
				note->volume = volume * percentage / 100;
				note->volume_effect = VOL_EFFECT_VOLUME;
			} else if (note->volume_effect == VOL_EFFECT_VOLUME) {
				note->volume = note->volume * percentage / 100;
			}
		}
	}
}

static void selection_slide_effect(void)
{
	int row, chan;
	song_note *pattern, *note;
	int first, last;		/* the effect values */
	
	/* FIXME: if there's no selection, should this display a dialog, or bail silently? */
	if (!SELECTION_EXISTS) {
		/* this should be one column wider (with the extra column on the left) */
		dialog_create(DIALOG_OK, "No block is marked", NULL, NULL, 0);
		return;
	}
	
	if (selection.first_row == selection.last_row)
		return;
	
	song_get_pattern(current_pattern, &pattern);
	
	/* the channel loop has to go on the outside for this one */
	for (chan = selection.first_channel; chan <= selection.last_channel; chan++) {
		note = pattern + chan - 1;
		first = note[64 * selection.first_row].parameter;
		last = note[64 * selection.last_row].parameter;
		note += 64 * selection.first_row;
		for (row = selection.first_row; row <= selection.last_row; row++, note += 64) {
			note->parameter = (((last - first)
					    * (row - selection.first_row)
					    / (selection.last_row - selection.first_row)
					    ) + first);
		}
	}
}

static void selection_wipe_effect(void)
{
	int row, chan;
	song_note *pattern, *note;

	if (!SELECTION_EXISTS) {
		/* this should be one column wider (see selection_swap) */
		dialog_create(DIALOG_OK, "No block is marked", NULL, NULL, 0);
		return;
	}
	
	song_get_pattern(current_pattern, &pattern);

	for (row = selection.first_row; row <= selection.last_row; row++) {
		note = pattern + 64 * row + selection.first_channel - 1;
		for (chan = selection.first_channel; chan <= selection.last_channel; chan++, note++) {
			note->effect = 0;
			note->parameter = 0;
		}
	}
}

/* --------------------------------------------------------------------------------------------------------- */
/* Row shifting operations */

/* A couple of the parameter names here might seem a bit confusing, so:
 *     what_row = what row to start the insert (generally this would be current_row)
 *     num_rows = the number of rows to insert */
static void pattern_insert_rows(int what_row, int num_rows, int first_channel, int chan_width)
{
	song_note *pattern;
	int row, total_rows = song_get_pattern(current_pattern, &pattern);
	
	if (first_channel < 1)
		first_channel = 1;
	if (chan_width + first_channel - 1 > 64)
		chan_width = 64 - first_channel + 1;
	
	if (num_rows + what_row > total_rows)
		num_rows = total_rows - what_row;
	
	if (first_channel == 1 && chan_width == 64) {
		memmove(pattern + 64 * (what_row + num_rows), pattern + 64 * what_row,
			64 * sizeof(song_note) * (total_rows - what_row - num_rows));
		memset(pattern + 64 * what_row, 0, num_rows * 64 * sizeof(song_note));
	} else {
		/* shift the area down */
		for (row = total_rows - num_rows - 1; row >= what_row; row--) {
			memmove(pattern + 64 * (row + num_rows) + first_channel - 1,
				pattern + 64 * row + first_channel - 1,
				chan_width * sizeof(song_note));
		}
		/* clear the inserted rows */
		for (row = what_row; row < what_row + num_rows; row++) {
			memset(pattern + 64 * row + first_channel - 1,
			       0, chan_width * sizeof(song_note));
		}
	}
}

/* Same as above, but with a couple subtle differences. */
static void pattern_delete_rows(int what_row, int num_rows, int first_channel, int chan_width)
{
	song_note *pattern;
	int row, total_rows = song_get_pattern(current_pattern, &pattern);
	
	if (first_channel < 1)
		first_channel = 1;
	if (chan_width + first_channel - 1 > 64)
		chan_width = 64 - first_channel + 1;
	
	if (num_rows + what_row > total_rows)
		num_rows = total_rows - what_row;
	
	if (first_channel == 1 && chan_width == 64) {
		memmove(pattern + 64 * what_row, pattern + 64 * (what_row + num_rows),
			64 * sizeof(song_note) * (total_rows - what_row - num_rows));
		memset(pattern + 64 * (total_rows - num_rows), 0, num_rows * 64 * sizeof(song_note));
	} else {
		/* shift the area up */
		for (row = what_row; row <= total_rows - num_rows - 1; row++) {
			memmove(pattern + 64 * row + first_channel - 1,
				pattern + 64 * (row + num_rows) + first_channel - 1,
				chan_width * sizeof(song_note));
		}
		/* clear the last rows */
		for (row = total_rows - num_rows; row < total_rows; row++) {
			memset(pattern + 64 * row + first_channel - 1,
			       0, chan_width * sizeof(song_note));
		}
	}
}

/* --------------------------------------------------------------------------------------------------------- */
/* clipboard */

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

	if (!SELECTION_EXISTS) {
		/* this should be one column wider (see selection_swap) */
		dialog_create(DIALOG_OK, "No block is marked", NULL, NULL, 0);
		return;
	}

	clipboard_free();

	clipboard.channels = selection.last_channel - selection.first_channel + 1;
	clipboard.rows = selection.last_row - selection.first_row + 1;

	clipboard.data = calloc(clipboard.channels * clipboard.rows, sizeof(song_note));

	song_get_pattern(current_pattern, &pattern);

	if (selection.first_channel == 1 && selection.last_channel == 64) {
		memcpy(clipboard.data, pattern + 64 * selection.first_row,
		       (selection.last_row - selection.first_row + 1) * 64 * sizeof(song_note));
	} else {
		for (row = 0; row < clipboard.rows; row++) {
			memcpy(clipboard.data + clipboard.channels * row,
			       pattern + 64 * (row + selection.first_row) + selection.first_channel - 1,
			       clipboard.channels * sizeof(song_note));
		}
	}
}

static void clipboard_paste_overwrite(void)
{
	song_note *pattern;
	int row, num_rows, chan_width;

	if (clipboard.data == NULL) {
		dialog_create(DIALOG_OK, "No data in clipboard", NULL, NULL, 0);
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
		       clipboard.data + clipboard.channels * row, chan_width * sizeof(song_note));
	}
}

static void clipboard_paste_insert(void)
{
	int num_rows, total_rows, chan_width;
	song_note *pattern;

	if (clipboard.data == NULL) {
		dialog_create(DIALOG_OK, "No data in clipboard", NULL, NULL, 0);
		return;
	}

	total_rows = song_get_pattern(current_pattern, &pattern);
	num_rows = total_rows - current_row;
	if (clipboard.rows < num_rows)
		num_rows = clipboard.rows;

	chan_width = clipboard.channels;
	if (chan_width + current_channel > 64)
		chan_width = 64 - current_channel + 1;
	
	pattern_insert_rows(current_row, clipboard.rows, current_channel, chan_width);
	clipboard_paste_overwrite();
}

static void clipboard_paste_mix_notes(void)
{
	int row, chan, num_rows, chan_width;
	song_note *pattern, *p_note, *c_note;

	if (clipboard.data == NULL) {
		dialog_create(DIALOG_OK, "No data in clipboard", NULL, NULL, 0);
		return;
	}

	num_rows = song_get_pattern(current_pattern, &pattern);
	num_rows -= current_row;
	if (clipboard.rows < num_rows)
		num_rows = clipboard.rows;

	chan_width = clipboard.channels;
	if (chan_width + current_channel > 64)
		chan_width = 64 - current_channel + 1;

	p_note = pattern + 64 * current_row + current_channel - 1;
	c_note = clipboard.data;
	for (row = 0; row < num_rows; row++) {
		for (chan = 0; chan < chan_width; chan++) {
#if 1
			if (memcmp(p_note + chan, &empty_note, sizeof(song_note)) == 0)
				p_note[chan] = c_note[chan];
#else
			/* Suppose the volume is set, but the volume effect is zero for some reason.
			 * I'm not sure it'll ever come up, but hey, weird things happen. */
			if (p_note[chan].note == 0 && p_note[chan].instrument == 0
			    && p_note[chan].volume_effect == 0
			    && p_note[chan].effect == 0 && p_note[chan].parameter == 0) {
				p_note[chan] = c_note[chan];
			}
#endif
		}
		p_note += 64;
		c_note += clipboard.channels;
	}
}

/* Same code as above. Maybe I should generalize it. */
static void clipboard_paste_mix_fields(void)
{
	int row, chan, num_rows, chan_width;
	song_note *pattern, *p_note, *c_note;

	if (clipboard.data == NULL) {
		dialog_create(DIALOG_OK, "No data in clipboard", NULL, NULL, 0);
		return;
	}

	num_rows = song_get_pattern(current_pattern, &pattern);
	num_rows -= current_row;
	if (clipboard.rows < num_rows)
		num_rows = clipboard.rows;

	chan_width = clipboard.channels;
	if (chan_width + current_channel > 64)
		chan_width = 64 - current_channel + 1;

	p_note = pattern + 64 * current_row + current_channel - 1;
	c_note = clipboard.data;
	for (row = 0; row < num_rows; row++) {
		for (chan = 0; chan < chan_width; chan++) {
			/* Ick. There ought to be a "conditional move" operator. */
			if (p_note[chan].note == 0)
				p_note[chan].note = c_note[chan].note;
			if (p_note[chan].instrument == 0)
				p_note[chan].instrument = c_note[chan].instrument;
			if (p_note[chan].volume_effect == 0) {
				p_note[chan].volume_effect = c_note[chan].volume_effect;
				p_note[chan].volume = c_note[chan].volume;
			}
			if (p_note[chan].effect == 0)
				p_note[chan].effect = c_note[chan].effect;
			if (p_note[chan].parameter == 0)
				p_note[chan].parameter = c_note[chan].parameter;
		}
		p_note += 64;
		c_note += clipboard.channels;
	}
}

/* --------------------------------------------------------------------- */

static void pattern_editor_reposition(void)
{
	int total_rows = song_get_rows_in_pattern(current_pattern);

	if (current_channel < top_display_channel)
		top_display_channel = current_channel;
	else if (current_channel >= top_display_channel + visible_channels)
		top_display_channel = current_channel - visible_channels + 1;

	if (centralise_cursor) {
		if (current_row <= 16)
			top_display_row = 0;
		else if (current_row + 15 > total_rows)
			top_display_row = total_rows - 31;
		else
			top_display_row = current_row - 16;
	} else {
		/* This could be written better. */
		if (current_row < top_display_row)
			top_display_row = current_row;
		else if (current_row > top_display_row + 31)
			top_display_row = current_row - 31;
		if (top_display_row + 31 > total_rows)
			top_display_row = total_rows - 31;
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

	draw_text(numtostr(3, current_row, buf), 12, 7, 5, 0);
	draw_text(numtostr(3, song_get_rows_in_pattern(current_pattern), buf), 16, 7, 5, 0);
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

	draw_text(numtostr(3, current_pattern, buf), 12, 6, 5, 0);
	draw_text(numtostr(3, song_get_num_patterns(), buf), 16, 6, 5, 0);
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
			selection.first_row = selection.last_row = total_rows;
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
	int n, last = 0, new_width;

	visible_width = 0;
	for (n = 0; n < 64; n++) {
		if (track_view_scheme[n] >= NUM_TRACK_VIEWS) {
			/* shouldn't happen, but might (e.g. if someone was
			 * messing with the config file) */
			track_view_scheme[n] = last;
		} else {
			last = track_view_scheme[n];
		}
		new_width = visible_width + track_views[track_view_scheme[n]].width;

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
	
	/* don't allow anything past channel 64 */
	if (top_display_channel > 64 - visible_channels + 1)
		top_display_channel = 64 - visible_channels + 1;
}

static void set_view_scheme(int scheme)
{
	if (scheme >= NUM_TRACK_VIEWS) {
		/* shouldn't happen */
		log_appendf(4, "View scheme %d out of range -- using default scheme", scheme);
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
	int pattern_is_playing = ((song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP)) != 0
				  && current_pattern == playing_pattern);

	SDL_LockSurface(screen);

	/* draw the outer box around the whole thing */
	draw_box_unlocked(4, 14, 5 + visible_width, 47, BOX_THICK | BOX_INNER | BOX_INSET);

	/* how many rows are there? */
	total_rows = song_get_pattern(current_pattern, &pattern);

	for (chan = top_display_channel, chan_pos = 0; chan_pos < visible_channels; chan++, chan_pos++) {
		track_view = track_views + track_view_scheme[chan_pos];
		track_view->draw_channel_header(chan, chan_drawpos, 14,
						((song_get_channel(chan - 1)->flags & CHN_MUTE) ? 0 : 3));

		note = pattern + 64 * top_display_row + chan - 1;
		for (row = top_display_row, row_pos = 0; row_pos < 32; row++, row_pos++) {
			if (chan_pos == 0) {
				fg = pattern_is_playing && row == playing_row ? 3 : 0;
				bg = (current_pattern == marked_pattern && row == marked_row) ? 11 : 2;
				draw_text_unlocked(numtostr(3, row, buf), 1, 15 + row_pos, fg, bg);
			}

			if (is_in_selection(chan, row)) {
				fg = 3;
				bg = (ROW_IS_HIGHLIGHT(row) ? 9 : 8);
			} else {
				fg = 6;
				if (highlight_current_row && row == current_row)
					bg = 1;
				else if (ROW_IS_MAJOR(row))
					bg = 14;
				else if (ROW_IS_MINOR(row))
					bg = 15;
				else
					bg = 0;
			}

			track_view->draw_note(chan_drawpos, 15 + row_pos,
					      note, ((row == current_row && chan == current_channel)
						     ? current_position : -1), fg, bg);

			if (draw_divisions && chan_pos < visible_channels - 1) {
				if (is_in_selection(chan, row))
					bg = 0;
				draw_char_unlocked(168, chan_drawpos + track_view->width, 15 + row_pos, 2, bg);
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
/* kill all humans */

static void raise_notes_by_semitone(void)
{
	int row, chan;
	song_note *pattern, *note;

	song_get_pattern(current_pattern, &pattern);

	if (SELECTION_EXISTS) {
		for (chan = selection.first_channel; chan <= selection.last_channel; chan++) {
			note = pattern + 64 * selection.first_row + chan - 1;
			for (row = selection.first_row; row <= selection.last_row; row++) {
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
		for (chan = selection.first_channel; chan <= selection.last_channel; chan++) {
			note = pattern + 64 * selection.first_row + chan - 1;
			for (row = selection.first_row; row <= selection.last_row; row++) {
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
	case 0:			/* note */
		/* TODO: rewrite this more logically */
		if (c == ' ') {
			/* copy mask to note */
			n = mask_note.note;
			/* if n == 0, don't care */
		} else if (c == note_trans[NOTE_TRANS_PLAY_NOTE]) {
			/* FIXME: this should process the effect too */
			if (cur_note->instrument && cur_note->note > 0 && cur_note->note < 120)
				song_play_note(cur_note->instrument, cur_note->note, current_channel, 1);
			advance_cursor();
			return 0;
		} else if (c == note_trans[NOTE_TRANS_PLAY_ROW]) {
			song_single_step(current_pattern, current_row);
			advance_cursor();
			return 0;
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
					cur_note->instrument = instrument_get_current();
				else
					cur_note->instrument = sample_get_current();
			}
			if (mask_fields & MASK_VOLUME) {
				cur_note->volume_effect = mask_note.volume_effect;
				cur_note->volume = mask_note.volume;
			}
			if (mask_fields & MASK_EFFECT) {
				cur_note->effect = mask_note.effect;
				cur_note->parameter = mask_note.parameter;
			}
		}

		/* copy the note back to the mask */
		mask_note.note = n;
		
		/* yay! make some noise! */
		if (cur_note->instrument && cur_note->note > 0 && cur_note->note < 120)
			song_play_note(cur_note->instrument, cur_note->note, current_channel, 1);
		
		advance_cursor();
		break;
	case 1:			/* octave */
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
	case 2:			/* instrument, first digit */
	case 3:			/* instrument, second digit */
		if (c == ' ') {
			if (song_is_instrument_mode())
				cur_note->instrument = instrument_get_current();
			else
				cur_note->instrument = sample_get_current();
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
	case 5:			/* volume */
		if (c == ' ') {
			cur_note->volume = mask_note.volume;
			cur_note->volume_effect = mask_note.volume_effect;
			advance_cursor();
			break;
		}
		if (c == note_trans[NOTE_TRANS_CLEAR]) {
			cur_note->volume = mask_note.volume = 0;
			cur_note->volume_effect = mask_note.volume_effect = VOL_EFFECT_NONE;
			advance_cursor();
			break;
		}
		if (c == note_trans[NOTE_TRANS_VOL_PAN_SWITCH]) {
			panning_mode = !panning_mode;
			status_text_flash("%s control set", (panning_mode ? "Panning" : "Volume"));
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
	case 6:			/* effect */
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
	case 7:			/* param, high nibble */
	case 8:			/* param, low nibble */
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
			cur_note->parameter = (n << 4) | (cur_note->parameter & 0xf);
			current_position++;
		} else {
			cur_note->parameter = (cur_note->parameter & 0xf0) | n;
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
		normalise_block_selection();
		break;
	case SDLK_e:
		if (!SELECTION_EXISTS) {
			selection.first_channel = current_channel;
			selection.first_row = current_row;
		}
		selection.last_channel = current_channel;
		selection.last_row = current_row;
		normalise_block_selection();
		break;
	case SDLK_d:
		if (status.last_keysym == SDLK_d) {
			if (total_rows - current_row > block_double_size)
				block_double_size <<= 1;
		} else {
			block_double_size = row_highlight_major;
			selection.first_channel = selection.last_channel = current_channel;
			selection.first_row = current_row;
		}
		n = block_double_size + current_row - 1;
		selection.last_row = MIN(n, total_rows);
		break;
	case SDLK_l:
		if (status.last_keysym == SDLK_l) {
			/* 3x alt-l re-selects the current channel */
			if (selection.first_channel == selection.last_channel) {
				selection.first_channel = 1;
				selection.last_channel = 64;
			} else {
				selection.first_channel = selection.last_channel = current_channel;
			}
		} else {
			selection.first_channel = selection.last_channel = current_channel;
			selection.first_row = 0;
			selection.last_row = total_rows;
		}
		break;
	case SDLK_r:
		draw_divisions = 1;
		set_view_scheme(0);
		break;
	case SDLK_s:
		selection_set_sample();
		break;
	case SDLK_u:
		if (SELECTION_EXISTS) {
			selection_clear();
		} else if (clipboard.data) {
			clipboard_free();
		} else {
			dialog_create(DIALOG_OK, "No data in clipboard", NULL, NULL, 0);
		}
		break;
	case SDLK_c:
		clipboard_copy();
		break;
	case SDLK_o:
		clipboard_paste_overwrite();
		break;
	case SDLK_p:
		clipboard_paste_insert();
		break;
	case SDLK_m:
		if (status.last_keysym == SDLK_m) {
			clipboard_paste_mix_fields();
		} else {
			clipboard_paste_mix_notes();
		}
		break;
	case SDLK_z:
		clipboard_copy();
		selection_erase();
		break;
	case SDLK_y:
		selection_swap();
		break;
	case SDLK_v:
		selection_set_volume();
		break;
	case SDLK_w:
		selection_wipe_volume(0);
		break;
	case SDLK_k:
		if (status.last_keysym == SDLK_k) {
			selection_wipe_volume(1);
		} else {
			selection_slide_volume();
		}
		break;
	case SDLK_x:
		if (status.last_keysym == SDLK_x) {
			selection_wipe_effect();
		} else {
			selection_slide_effect();
		}
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
	case SDLK_i:
		if (fast_volume_mode) {
			fast_volume_amplify();
		} else {
			/* TODO: template stuff */
		}
		break;
	case SDLK_j:
		if (fast_volume_mode) {
			fast_volume_attenuate();
		} else {
			volume_amplify();
		}
		break;
	case SDLK_t:
		n = current_channel - top_display_channel;
		track_view_scheme[n] = ((track_view_scheme[n] + 1)
					% NUM_TRACK_VIEWS);
		recalculate_visible_area();
		pattern_editor_reposition();
		break;
	case SDLK_UP:
		if (top_display_row > 0) {
			top_display_row--;
			if (current_row > top_display_row + 31)
				current_row = top_display_row + 31;
			return -1;
		}
		return 1;
	case SDLK_DOWN:
		if (top_display_row + 31 < total_rows) {
			top_display_row++;
			if (current_row < top_display_row)
				current_row = top_display_row;
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
		pattern_insert_rows(current_row, 1, 1, 64);
		break;
	case SDLK_DELETE:
		pattern_delete_rows(current_row, 1, 1, 64);
		break;
	case SDLK_KP9:
	case SDLK_F9:
		song_toggle_channel_mute(current_channel - 1);
		orderpan_recheck_muted_channels();
		break;
	case SDLK_KP0:
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

/* Two atoms are walking down the street, and one of them stops abruptly
 *     and says, "Oh my God, I just lost an electron!"
 * The other one says, "Are you sure?"
 * The first one says, "Yes, I'm positive!" */
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
		n = k->sym - '0';
		if (n < 0 || n >= NUM_TRACK_VIEWS)
			return 0;
		if (k->mod & KMOD_SHIFT) {
			set_view_scheme(n);
		} else {
			track_view_scheme[current_channel - top_display_channel] = n;
			recalculate_visible_area();
		}
		pattern_editor_reposition();
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_c:
		centralise_cursor = !centralise_cursor;
		status_text_flash("Centralise cursor %s", (centralise_cursor ? "enabled" : "disabled"));
		return -1;
	case SDLK_h:
		highlight_current_row = !highlight_current_row;
		status_text_flash("Row hilight %s", (highlight_current_row ? "enabled" : "disabled"));
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_j:
		fast_volume_toggle();
		return 1;
	case SDLK_v:
		show_default_volumes = !show_default_volumes;
		status_text_flash("Default volumes %s", (show_default_volumes ? "enabled" : "disabled"));
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
		current_row -= row_highlight_major;
		return -1;
	case SDLK_PAGEDOWN:
		current_row += row_highlight_major;
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
		pattern_insert_rows(current_row, 1, current_channel, 1);
		break;
	case SDLK_DELETE:
		pattern_delete_rows(current_row, 1, current_channel, 1);
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
		status_text_flash("Playback tracing %s", (playback_tracing ? "enabled" : "disabled"));
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
		} else if (c == note_trans[NOTE_TRANS_NEXT_INS] || c == '>') {
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

	if (k->mod & (KMOD_ALT | KMOD_META))
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

	draw_text(numtostr(3, song_get_num_patterns(), buf), 16, 6, 5, 0);
	draw_text(numtostr(3, current_row, buf), 12, 7, 5, 0);

	status.flags |= NEED_UPDATE;
	return 1;
}

/* --------------------------------------------------------------------- */

static void pattern_editor_playback_update(void)
{
	static int prev_row = -1;
	static int prev_pattern = -1;

	playing_row = song_get_current_row();
	playing_pattern = song_get_playing_pattern();

	if ((song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP)) != 0
	    && (playing_row != prev_row || playing_pattern != prev_pattern)) {

		prev_row = playing_row;
		prev_pattern = playing_pattern;

		if (playback_tracing) {
			set_current_order(song_get_current_order());
			set_current_pattern(playing_pattern);
			current_row = playing_row;
			pattern_editor_reposition();
			status.flags |= NEED_UPDATE;
		} else if (current_pattern == song_get_playing_pattern()) {
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

	create_other(items_pattern + 0, 0, pattern_editor_handle_key_cb, pattern_editor_redraw);
}
