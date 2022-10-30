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

/* The all-important pattern editor. The code here is a general mess, so
 * don't look at it directly or, uh, you'll go blind or something. */

#include "headers.h"

#include <ctype.h>

#include "it.h"
#include "page.h"
#include "song.h"
#include "pattern-view.h"
#include "config-parser.h"
#include "midi.h"
#include "osdefs.h"

#include "sdlmain.h"
#include "clippy.h"
#include "disko.h"

/* --------------------------------------------------------------------------------------------------------- */

#define ROW_IS_MAJOR(r) (current_song->row_highlight_major != 0 && (r) % current_song->row_highlight_major == 0)
#define ROW_IS_MINOR(r) (current_song->row_highlight_minor != 0 && (r) % current_song->row_highlight_minor == 0)
#define ROW_IS_HIGHLIGHT(r) (ROW_IS_MINOR(r) || ROW_IS_MAJOR(r))

/* this is actually used by pattern-view.c */
int show_default_volumes = 0;

/* --------------------------------------------------------------------- */
/* The (way too many) static variables */

static int midi_start_record = 0;

enum {
    TEMPLATE_OFF = 0,
    TEMPLATE_OVERWRITE,
    TEMPLATE_MIX_PATTERN_PRECEDENCE,
    TEMPLATE_MIX_CLIPBOARD_PRECEDENCE,
    TEMPLATE_NOTES_ONLY,
    TEMPLATE_MODE_MAX,
};
static int template_mode = TEMPLATE_OFF;

static const char *template_mode_names[] = {
	"",
	"Template, Overwrite",
	"Template, Mix - Pattern data precedence",
	"Template, Mix - Clipboard data precedence",
	"Template, Notes only",
};

/* only one widget, but MAN is it complicated :) */
static struct widget widgets_pattern[1];

/* pattern display position */
static int top_display_channel = 1;             /* one-based */
static int top_display_row = 0;         /* zero-based */

/* these three tell where the cursor is in the pattern */
static int current_channel = 1, current_position = 0;
static int current_row = 0;

static int keyjazz_noteoff = 0;       /* issue noteoffs when releasing note */
static int keyjazz_write_noteoff = 0; /* write noteoffs when releasing note */
static int keyjazz_repeat = 1;        /* insert multiple notes on key repeat */

/* this is, of course, what the current pattern is */
static int current_pattern = 0;

static int skip_value = 1;              /* aka cursor step */

static int link_effect_column = 0;
static int draw_divisions = 0;          /* = vertical lines between channels */

static int shift_chord_channels = 0; /* incremented for each shift-note played */

static int centralise_cursor = 0;
static int highlight_current_row = 0;
int playback_tracing = 0;       /* scroll lock */
int midi_playback_tracing = 0;

static int panning_mode = 0;            /* for the volume column */
int midi_bend_hit[64];
int midi_last_bend_hit[64];

/* blah; other forwards */
static void pated_save(const char *descr);
static void pated_history_add2(int groupedf, const char *descr, int x, int y, int width, int height);
static void pated_history_add(const char *descr, int x, int y, int width, int height);
static void pated_history_add_grouped(const char *descr, int x, int y, int width, int height);
static void pated_history_restore(int n);

/* these should fix the playback tracing position discrepancy */
static int playing_row = -1;
static int playing_pattern = -1;

/* the current editing mask (what stuff is copied) */
static int edit_copy_mask = MASK_NOTE | MASK_INSTRUMENT | MASK_VOLUME;

/* and the mask note. note that the instrument field actually isn't used */
static song_note_t mask_note = { 61, 0, 0, 0, 0, 0 };     /* C-5 */

/* playback mark (ctrl-f7) */
static int marked_pattern = -1, marked_row;

/* volume stuff (alt-i, alt-j, ctrl-j) */
static int volume_percent = 100;
static int vary_depth = 10;
static int fast_volume_percent = 67;
static int fast_volume_mode = 0;        /* toggled with ctrl-j */

enum {
    COPY_INST_OFF = 0, /* no search (IT style) */
    COPY_INST_UP = 1, /* search above the cursor for an instrument number */
    COPY_INST_UP_THEN_DOWN = 2, /* search both ways, up to row 0 first, then down */
    COPY_INST_SENTINEL = 3, /* non-value */
};
static int mask_copy_search_mode = COPY_INST_OFF;

/* If nonzero, home/end will move to the first/last row in the current channel
prior to moving to the first/last channel, i.e. operating in a 'z' pattern.
This is closer to FT2's behavior for the keys. */
static int invert_home_end = 0;

/* --------------------------------------------------------------------- */
/* undo and clipboard handling */
struct pattern_snap {
	song_note_t *data;
	int channels;
	int rows;

	/* used by undo/history only */
	const char *snap_op;
	int snap_op_allocated;
	int x, y;
	int patternno;
};
static struct pattern_snap fast_save = {
	NULL, 0, 0,
	"Fast Pattern Save",
	0, 0, 0, -1
};
/* static int fast_save_validity = -1; */

static void snap_paste(struct pattern_snap *s, int x, int y, int xlate);

static struct pattern_snap clipboard = {
	NULL, 0, 0,
	"Clipboard",
	0, 0, 0, -1
};
static struct pattern_snap undo_history[10];
static int undo_history_top = 0;

/* this function is stupid, it doesn't belong here */
void memused_get_pattern_saved(unsigned int *a, unsigned int *b)
{
	int i;
	if (b) {
		for (i = 0; i < 10; i++) {
			if (undo_history[i].data)
				*b = (*b) + undo_history[i].rows;
		}
	}
	if (a) {
		if (clipboard.data) (*a) = (*a) + clipboard.rows;
		if (fast_save.data) (*a) = (*a) + fast_save.rows;
	}
}



/* --------------------------------------------------------------------- */
/* block selection handling */

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


/* this is set to 1 on the first alt-d selection,
 * and shifted left on each successive press. */
static int block_double_size;

/* if first_channel is zero, there's no selection, as the channel
 * numbers start with one. (same deal for last_channel, but i'm only
 * caring about one of them to be efficient.) */
#define SELECTION_EXISTS (selection.first_channel)

/* CHECK_FOR_SELECTION(optional return value)
will display an error dialog and cause the function to return if there is no block marked.
(The spaces around the text are to make it line up the same as Impulse Tracker) */
#define CHECK_FOR_SELECTION(q) do {\
	if (!SELECTION_EXISTS) {\
		dialog_create(DIALOG_OK, "    No block is marked    ", NULL, NULL, 0, NULL);\
		q;\
	}\
} while(0)

/* --------------------------------------------------------------------- */
/* this is for the multiple track views stuff. */

struct track_view {
	int width;
	draw_channel_header_func draw_channel_header;
	draw_note_func draw_note;
	draw_mask_func draw_mask;
};

static const struct track_view track_views[] = {
#define TRACK_VIEW(n) {n, draw_channel_header_##n, draw_note_##n, draw_mask_##n}
	TRACK_VIEW(13),                 /* 5 channels */
	TRACK_VIEW(10),                 /* 6/7 channels */
	TRACK_VIEW(7),                  /* 9/10 channels */
	TRACK_VIEW(6),                  /* 10/12 channels */
	TRACK_VIEW(3),                  /* 18/24 channels */
	TRACK_VIEW(2),                  /* 24/36 channels */
	TRACK_VIEW(1),                  /* 36/64 channels */
#undef  TRACK_VIEW
};

#define NUM_TRACK_VIEWS ARRAY_SIZE(track_views)

static uint8_t track_view_scheme[64];
static int channel_multi_enabled = 0;
static int channel_multi[64];
static int visible_channels, visible_width;

static void recalculate_visible_area(void);
static void set_view_scheme(int scheme);
static void pattern_editor_reposition(void);

/* --------------------------------------------------------------------------------------------------------- */
/* options dialog */

static struct widget options_widgets[8];
static const int options_link_split[] = { 5, 6, -1 };
static int options_selected_widget = 0;
static int options_last_octave = 0;

static void options_close_cancel(UNUSED void *data)
{
	kbd_set_current_octave(options_last_octave);
}
static void options_close(void *data)
{
	int old_size, new_size;

	options_selected_widget = ((struct dialog *) data)->selected_widget;

	skip_value = options_widgets[1].d.thumbbar.value;
	current_song->row_highlight_minor = options_widgets[2].d.thumbbar.value;
	current_song->row_highlight_major = options_widgets[3].d.thumbbar.value;
	link_effect_column = !!(options_widgets[5].d.togglebutton.state);
	status.flags |= SONG_NEEDS_SAVE;

	old_size = song_get_pattern(current_pattern, NULL);
	new_size = options_widgets[4].d.thumbbar.value;
	if (old_size != new_size) {
		song_pattern_resize(current_pattern, new_size);
		current_row = MIN(current_row, new_size - 1);
		pattern_editor_reposition();
	}
}

static void options_draw_const(void)
{
	draw_text("Pattern Editor Options", 28, 19, 0, 2);
	draw_text("Base octave", 28, 23, 0, 2);
	draw_text("Cursor step", 28, 26, 0, 2);
	draw_text("Row hilight minor", 22, 29, 0, 2);
	draw_text("Row hilight major", 22, 32, 0, 2);
	draw_text("Number of rows in pattern", 14, 35, 0, 2);
	draw_text("Command/Value columns", 18, 38, 0, 2);

	draw_box(39, 22, 42, 24, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(39, 25, 43, 27, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(39, 28, 45, 30, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(39, 31, 57, 33, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(39, 34, 62, 36, BOX_THIN | BOX_INNER | BOX_INSET);
}

static void options_change_base_octave(void)
{
	kbd_set_current_octave(options_widgets[0].d.thumbbar.value);
}

/* the base octave is changed directly when the thumbbar is changed.
 * anything else can wait until the dialog is closed. */
void pattern_editor_display_options(void)
{
	struct dialog *dialog;

	if (options_widgets[0].width == 0) {
		/* haven't built it yet */
		create_thumbbar(options_widgets + 0, 40, 23, 2, 7, 1, 1, options_change_base_octave, 0, 8);
		create_thumbbar(options_widgets + 1, 40, 26, 3, 0, 2, 2, NULL, 0, 16);
		create_thumbbar(options_widgets + 2, 40, 29, 5, 1, 3, 3, NULL, 0, 32);
		create_thumbbar(options_widgets + 3, 40, 32, 17, 2, 4, 4, NULL, 0, 128);
		/* Although patterns as small as 1 row can be edited properly (as of c759f7a0166c), I have
		discovered it's a bit annoying to hit 'home' here expecting to get 32 rows but end up with
		just one row instead. so I'll allow editing these patterns, but not really provide a way to
		set the size, at least until I decide how to present the option nonintrusively. */
		create_thumbbar(options_widgets + 4, 40, 35, 22, 3, 5, 5, NULL, 32, 200);
		create_togglebutton(options_widgets + 5, 40, 38, 8, 4, 7, 6, 6, 6,
				    NULL, "Link", 3, options_link_split);
		create_togglebutton(options_widgets + 6, 52, 38, 9, 4, 7, 5, 5, 5,
				    NULL, "Split", 3, options_link_split);
		create_button(options_widgets + 7, 35, 41, 8, 5, 0, 7, 7, 7, dialog_yes_NULL, "Done", 3);
	}

	options_last_octave = kbd_get_current_octave();
	options_widgets[0].d.thumbbar.value = options_last_octave;
	options_widgets[1].d.thumbbar.value = skip_value;
	options_widgets[2].d.thumbbar.value = current_song->row_highlight_minor;
	options_widgets[3].d.thumbbar.value = current_song->row_highlight_major;
	options_widgets[4].d.thumbbar.value = song_get_pattern(current_pattern, NULL);
	togglebutton_set(options_widgets, link_effect_column ? 5 : 6, 0);

	dialog = dialog_create_custom(10, 18, 60, 26, options_widgets, 8, options_selected_widget,
				      options_draw_const, NULL);
	dialog->action_yes = options_close;
	if (status.flags & CLASSIC_MODE) {
		dialog->action_cancel = options_close;
	} else {
		dialog->action_cancel = options_close_cancel;
	}
	dialog->data = dialog;
}


static struct widget template_error_widgets[1];
static void template_error_draw(void)
{
	draw_text("Template Error", 33, 25, 0, 2);
	draw_text("No note in the top left position", 23, 27, 0, 2);
	draw_text("of the clipboard on which to", 25, 28, 0, 2);
	draw_text("base translations.", 31, 29, 0, 2);
}

/* --------------------------------------------------------------------------------------------------------- */
/* pattern length dialog */
static struct widget length_edit_widgets[4];
static void length_edit_draw_const(void)
{
	draw_box(33,23,56,25, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(33,26,60,29, BOX_THIN | BOX_INNER | BOX_INSET);

	draw_text("Set Pattern Length", 31, 21, 0, 2);
	draw_text("Pattern Length", 19, 24, 0, 2);
	draw_text("Start Pattern", 20, 27, 0, 2);
	draw_text("End Pattern", 22, 28, 0, 2);
}
static void length_edit_close(UNUSED void *data)
{
	int i, nl;
	nl = length_edit_widgets[0].d.thumbbar.value;
	status.flags |= SONG_NEEDS_SAVE;
	for (i = length_edit_widgets[1].d.thumbbar.value;
	i <= length_edit_widgets[2].d.thumbbar.value; i++) {
		if (song_get_pattern(i, NULL) != nl) {
			song_pattern_resize(i, nl);
			if (i == current_pattern) {
				status.flags |= NEED_UPDATE;
				current_row = MIN(current_row, nl - 1);
				pattern_editor_reposition();
			}
		}
	}
}
static void length_edit_cancel(UNUSED void *data)
{
	/* do nothing */
}
void pattern_editor_length_edit(void)
{
	struct dialog *dialog;

	create_thumbbar(length_edit_widgets + 0, 34, 24, 22, 0, 1, 1, NULL, 32, 200);
	length_edit_widgets[0].d.thumbbar.value = song_get_pattern(current_pattern, NULL );
	create_thumbbar(length_edit_widgets + 1, 34, 27, 26, 0, 2, 2, NULL, 0, 199);
	create_thumbbar(length_edit_widgets + 2, 34, 28, 26, 1, 3, 3, NULL, 0, 199);
	length_edit_widgets[1].d.thumbbar.value
		= length_edit_widgets[2].d.thumbbar.value
		= current_pattern;

	create_button(length_edit_widgets + 3, 35, 31, 8, 2, 3, 3, 3, 0, dialog_yes_NULL, "OK", 4);

	dialog = dialog_create_custom(15, 19, 51, 15, length_edit_widgets, 4, 0,
				      length_edit_draw_const, NULL);
	dialog->action_yes = length_edit_close;
	dialog->action_cancel = length_edit_cancel;
}

/* --------------------------------------------------------------------------------------------------------- */
/* multichannel dialog */
static struct widget multichannel_widgets[65];
static void multichannel_close(UNUSED void *data)
{
	int i, m = 0;

	for (i = 0; i < 64; i++) {
		channel_multi[i] = !!multichannel_widgets[i].d.toggle.state;
		if (channel_multi[i])
			m = 1;
	}
	if (m)
		channel_multi_enabled = 1;
}
static int multichannel_handle_key(struct key_event *k)
{
	if (k->sym.sym == SDLK_n) {
		if ((k->mod & KMOD_ALT) && k->state == KEY_PRESS)
			dialog_yes(NULL);
		else if (NO_MODIFIER(k->mod) && k->state == KEY_RELEASE)
			dialog_cancel(NULL);
		return 1;
	}
	return 0;
}
static void multichannel_draw_const(void)
{
	char sbuf[16];
	int i;

	for (i = 0; i < 64; i++) {
		sprintf(sbuf, "Channel %02d", i+1);
		draw_text(sbuf,
			9 + ((i / 16) * 16), /* X */
			22 + (i % 16),  /* Y */
			0, 2);
	}
	for (i = 0; i < 64; i += 16) {
		draw_box(
			19 + ((i / 16) * 16), /* X */
			21,
			23 + ((i / 16) * 16), /* X */
			38,
			BOX_THIN|BOX_INNER|BOX_INSET);
	}
	draw_text("Multichannel Selection", 29, 19, 3, 2);
}
static void mp_advance_channel(void)
{
	change_focus_to(*selected_widget + 1);
}

static void pattern_editor_display_multichannel(void)
{
	struct dialog *dialog;
	int i;

	for (i = 0; i < 64; i++) {
		create_toggle(multichannel_widgets+i,
			20 + ((i / 16) * 16), /* X */
			22 + (i % 16),  /* Y */

			((i % 16) == 0) ? 64 : (i-1),
			((i % 16) == 15) ? 64 : (i+1),
			(i < 16) ? (i+48) : (i-16),
			((i + 16) % 64),
			((i + 16) % 64),

			mp_advance_channel);
		multichannel_widgets[i].d.toggle.state = !!channel_multi[i];
	}
	create_button(multichannel_widgets + 64, 36, 40, 6, 15, 0, 64, 64, 64, dialog_yes_NULL, "OK", 3);

	dialog = dialog_create_custom(7, 18, 66, 25, multichannel_widgets, 65, 0,
				      multichannel_draw_const, NULL);
	dialog->action_yes = multichannel_close;
	dialog->action_cancel = multichannel_close;
	dialog->handle_key = multichannel_handle_key;
}



/* This probably doesn't belong here, but whatever */

static int multichannel_get_next(int cur_channel)
{
	int i;

	cur_channel--; /* make it zero-based. oh look, it's a hammer. */
	i = cur_channel;

	if (channel_multi[cur_channel]) {
		/* we're in a multichan-enabled channel, so look for the next one */
		do {
			i = (i + 1) & 63; /* no? next channel, and loop back to zero if we hit 64 */
			if (channel_multi[i]) /* is this a multi-channel? */
				break; /* it is! */
		} while (i != cur_channel);

		/* at this point we've either broken the loop because the channel i is multichan,
		   or the condition failed because we're back where we started */
	}
	/* status_text_flash ("Newly selected channel is %d", (int) i + 1); */
	return i + 1; /* make it one-based again */
}

static int multichannel_get_previous(int cur_channel)
{
	int i;

	cur_channel--; /* once again, .... */
	i = cur_channel;

	if (channel_multi[cur_channel]) {
		do {
			i = i ? (i - 1) : 63; /* loop backwards this time */
			if (channel_multi[i])
				break;
		} while (i != cur_channel);
	}
	return i + 1;
}


/* --------------------------------------------------------------------------------------------------------- */
static void copyin_addnote(song_note_t *note, int *copyin_x, int *copyin_y)
{
	song_note_t *pattern, *p_note;
	int num_rows;

	status.flags |= (SONG_NEEDS_SAVE|NEED_UPDATE);
	num_rows = song_get_pattern(current_pattern, &pattern);
	if ((*copyin_x + (current_channel-1)) >= 64) return;
	if ((*copyin_y + current_row) >= num_rows) return;
	p_note = pattern + 64 * (*copyin_y + current_row) + (*copyin_x + (current_channel-1));
	*p_note = *note;
	(*copyin_x) = (*copyin_x) + 1;
}

static void copyin_addrow(int *copyin_x, int *copyin_y)
{
	*copyin_x=0;
	(*copyin_y) = (*copyin_y) + 1;
}

static int pattern_selection_system_paste(UNUSED int cb, const void *data)
{
	int copyin_x, copyin_y;
	int (*fx_map)(char f);
	const char *str;
	int x, scantmp;

	if (!data) return 0;
	str = (const char *)data;

	for (x = 0; str[x] && str[x] != '\n'; x++);
	if (x <= 11) return 0;
	if (!str[x] || str[x+1] != '|') return 0;
	if (str[x-1] == '\r') x--;
	if ((str[x-3] == ' ' && str[x-2] == 'I' && str[x-1] == 'T')
	|| (str[x-3] == 'S' && str[x-2] == '3' && str[x-1] == 'M')) {
		/* s3m effects */
		fx_map = get_effect_number;
	} else if ((str[x-3] == ' ' && str[x-2] == 'X' && str[x-1] == 'M')
	|| (str[x-3] == 'M' && str[x-2] == 'O' && str[x-1] == 'D')) {
		/* ptm effects */
		fx_map = get_ptm_effect_number;
	} else {
		return 0;
	}
	if (str[x] == '\r') x++;
	str += x+2;
	copyin_x = copyin_y = 0;
	/* okay, let's start parsing */
	while (*str) {
		song_note_t n = {};

		if (!str[0] || !str[1] || !str[2]) break;
		switch (*str) {
		case 'C': case 'c': n.note = 1; break;
		case 'D': case 'd': n.note = 3; break;
		case 'E': case 'e': n.note = 5; break;
		case 'F': case 'f': n.note = 6; break;
		case 'G': case 'g': n.note = 8; break;
		case 'A': case 'a': n.note = 10;break;
		case 'B': case 'b': n.note = 12;break;
		default: n.note = 0;
		};
		/* XXX shouldn't this be note-- for flat? */
		if (str[1] == '#' || str[1] == 'b') n.note++;
		switch (*str) {
		case '=': n.note = NOTE_OFF; break;
		case '^': n.note = NOTE_CUT; break;
		case '~': n.note = NOTE_FADE; break;
		case ' ': case '.': n.note = 0; break;
		default:
			n.note += ((str[2] - '0') * 12);
			break;
		};
		str += 3;
		/* instrument number */
		if (sscanf(str, "%02d", &scantmp) == 1)
			n.instrument = scantmp;
		else
			n.instrument = 0;
		str += 2;
		while (*str) {
			if (*str == '|' || *str == '\r' || *str == '\n') break;
			if (!str[0] || !str[1] || !str[2]) break;
			if (*str >= 'a' && *str <= 'z') {
				if (sscanf(str+1, "%02d", &scantmp) == 1)
					n.volparam = scantmp;
				else
					n.volparam = 0;
				switch (*str) {
					case 'v': n.voleffect = VOLFX_VOLUME; break;
					case 'p': n.voleffect = VOLFX_PANNING; break;
					case 'c': n.voleffect = VOLFX_VOLSLIDEUP; break;
					case 'd': n.voleffect = VOLFX_VOLSLIDEDOWN; break;
					case 'a': n.voleffect = VOLFX_FINEVOLUP; break;
					case 'b': n.voleffect = VOLFX_FINEVOLDOWN; break;
					case 'u': n.voleffect = VOLFX_VIBRATOSPEED; break;
					case 'h': n.voleffect = VOLFX_VIBRATODEPTH; break;
					case 'l': n.voleffect = VOLFX_PANSLIDELEFT; break;
					case 'r': n.voleffect = VOLFX_PANSLIDERIGHT; break;
					case 'g': n.voleffect = VOLFX_TONEPORTAMENTO; break;
					case 'f': n.voleffect = VOLFX_PORTAUP; break;
					case 'e': n.voleffect = VOLFX_PORTADOWN; break;
					default:  n.voleffect = VOLFX_NONE; n.volparam = 0; break;
				};
			} else {
				n.effect = fx_map(*str);
				if (sscanf(str+1, "%02X", &scantmp) == 1)
					n.param = scantmp;
				else
					n.param = 0;
			}
			str += 3;
		}
		copyin_addnote(&n, &copyin_x, &copyin_y);
		if (str[0] == '\r' || str[0] == '\n') {
			while (str[0] == '\r' || str[0] == '\n') str++;
			copyin_addrow(&copyin_x, &copyin_y);
		}
		if (str[0] != '|') break;
		str++;
	}
	return 1;
}
static void pattern_selection_system_copyout(void)
{
	char *str;
	int x, y, len;
	int total_rows;
	song_note_t *pattern, *cur_note;


	if (!(SELECTION_EXISTS)) {
		if (clippy_owner(CLIPPY_SELECT) == widgets_pattern) {
			/* unselect if we don't have a selection */
			clippy_select(NULL, NULL, 0);
		}
		return;
	}

	len = 21;
	total_rows = song_get_pattern(current_pattern, &pattern);
	for (y = selection.first_row; y <= selection.last_row && y < total_rows; y++) {
		for (x = selection.first_channel; x <= selection.last_channel; x++) {
			/* must match template below */
			len += 12;
		}
		len += 2;
	}
	str = mem_alloc(len+1);
	/* the OpenMPT/ModPlug header says:
		ModPlug Tracker S3M\x0d\x0a

	but really we can get away with:
		Pasted Pattern - IT\x0d\x0a

	because that's just how it's parser works. Add your own- just
	remember the file "type" is right-aligned. Please don't invent
	any new formats- even if you add more effects, try to base most of
	them on protracker (XM/MOD) or S3M/IT.

	*/
	strcpy(str, "Pasted Pattern - IT\x0d\x0a");
	len = 21;
	for (y = selection.first_row; y <= selection.last_row && y < total_rows; y++) {
		cur_note = pattern + 64 * y
					+ selection.first_channel - 1;
		for (x = selection.first_channel; x <= selection.last_channel; x++) {
			str[len] = '|'; len++;
			if (cur_note->note == 0) {
				str[len] = str[len+1] = str[len+2] = '.'; /* ... */
			} else if (cur_note->note == NOTE_CUT) {
				str[len] = str[len+1] = str[len+2] = '^'; /* ^^^ */
			} else if (cur_note->note == NOTE_OFF) {
				str[len] = str[len+1] = str[len+2] = '='; /* === */
			} else if (cur_note->note == NOTE_FADE) {
				/* ModPlug won't handle this one, but it'll
				just drop it...
				*/
				str[len] = str[len+1] = str[len+2] = '~'; /* ~~~ */
			} else {
				get_note_string(cur_note->note, str+len);
			}
			len += 3;
			if (cur_note->instrument)
				sprintf(str+len, "%02d", cur_note->instrument);
			else
				str[len] = str[len+1] = '.';
			sprintf(str+len+3, "%02d", cur_note->volparam);
			switch (cur_note->voleffect) {
			case VOLFX_VOLUME:         str[len+2] = 'v';break;
			case VOLFX_PANNING:        str[len+2] = 'p';break;
			case VOLFX_VOLSLIDEUP:     str[len+2] = 'c';break;
			case VOLFX_VOLSLIDEDOWN:   str[len+2] = 'd';break;
			case VOLFX_FINEVOLUP:      str[len+2] = 'a';break;
			case VOLFX_FINEVOLDOWN:    str[len+2] = 'b';break;
			case VOLFX_VIBRATOSPEED:   str[len+2] = 'u';break;
			case VOLFX_VIBRATODEPTH:   str[len+2] = 'h';break;
			case VOLFX_PANSLIDELEFT:   str[len+2] = 'l';break;
			case VOLFX_PANSLIDERIGHT:  str[len+2] = 'r';break;
			case VOLFX_TONEPORTAMENTO: str[len+2] = 'g';break;
			case VOLFX_PORTAUP:        str[len+2] = 'f';break;
			case VOLFX_PORTADOWN:      str[len+2] = 'e';break;
			default:                        str[len+2] = '.';
						/* override above */
							str[len+3] = '.';
							str[len+4] = '.';
			};
			len += 5;
			sprintf(str+len, "%c%02X",
					get_effect_char(cur_note->effect),
					cur_note->param);
			if (str[len] == '.' || str[len] == '?') {
				str[len] = '.';
				if (!cur_note->param)
					str[len+1] = str[len+2] = '.';
			}
			len += 3;
			/* Hints to implementors:

			If you had more columns in your channel, you should
			mark it here with a letter representing the channel
			semantic, followed by your decimal value.

			Add as many as you like- the next channel begins with
			a pipe-character (|) and the next row begins with a
			0D 0A sequence.

			*/
			cur_note++;
		}
		str[len] = '\x0d';
		str[len+1] = '\x0a';
		len += 2;
	}
	str[len] = 0;
	clippy_select(widgets_pattern, str, len);
}

/* --------------------------------------------------------------------------------------------------------- */
/* undo dialog */

static struct widget undo_widgets[1];
static int undo_selection = 0;

static void history_draw_const(void)
{
	int i, j;
	int fg, bg;
	draw_text("Undo", 38, 22, 3, 2);
	draw_box(19,23,60,34, BOX_THIN | BOX_INNER | BOX_INSET);
	j = undo_history_top;
	for (i = 0; i < 10; i++) {
		if (i == undo_selection) {
			fg = 0; bg = 3;
		} else {
			fg = 2; bg = 0;
		}

		draw_char(32, 20, 24+i, fg, bg);
		draw_text_len(undo_history[j].snap_op, 39, 21, 24+i, fg, bg);
		j--;
		if (j < 0) j += 10;
	}
}

static void history_close(UNUSED void *data)
{
	/* nothing! */
}

static int history_handle_key(struct key_event *k)
{
	int i,j;
	if (! NO_MODIFIER(k->mod)) return 0;
	switch (k->sym.sym) {
	case SDLK_ESCAPE:
		if (k->state == KEY_PRESS)
			return 0;
		dialog_cancel(NULL);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_UP:
		if (k->state == KEY_RELEASE)
			return 0;
		undo_selection--;
		if (undo_selection < 0) undo_selection = 0;
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_DOWN:
		if (k->state == KEY_RELEASE)
			return 0;
		undo_selection++;
		if (undo_selection > 9) undo_selection = 9;
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_RETURN:
		if (k->state == KEY_RELEASE)
			return 0;
		j = undo_history_top;
		for (i = 0; i < 10; i++) {
			if (i == undo_selection) {
				pated_history_restore(j);
				break;
			}
			j--;
			if (j < 0) j += 10;
		}
		dialog_cancel(NULL);
		status.flags |= NEED_UPDATE;
		return 1;
	default:
		break;
	};

	return 0;
}

static void pattern_editor_display_history(void)
{
	struct dialog *dialog;

	create_other(undo_widgets + 0, 0, history_handle_key, NULL);
	dialog = dialog_create_custom(17, 21, 47, 16, undo_widgets, 1, 0,
				      history_draw_const, NULL);
	dialog->action_yes = history_close;
	dialog->action_cancel = history_close;
	dialog->handle_key = history_handle_key;
}

/* --------------------------------------------------------------------------------------------------------- */
/* volume fiddling */

static void selection_amplify(int percentage);
static void selection_vary(int fast, int depth, int part);

/* --------------------------------------------------------------------------------------------------------- */
/* volume amplify/attenuate and fast volume setup handlers */

/* this is shared by the fast and normal volume dialogs */
static struct widget volume_setup_widgets[3];

static void fast_volume_setup_ok(UNUSED void *data)
{
	fast_volume_percent = volume_setup_widgets[0].d.thumbbar.value;
	fast_volume_mode = 1;
	status_text_flash("Alt-I / Alt-J fast volume changes enabled");
}

static void fast_volume_setup_cancel(UNUSED void *data)
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
		create_thumbbar(volume_setup_widgets + 0, 33, 30, 11, 0, 1, 1, NULL, 10, 90);

		volume_setup_widgets[0].d.thumbbar.value = fast_volume_percent;
		create_button(volume_setup_widgets + 1, 31, 33, 6, 0, 1, 2, 2, 2,
			      dialog_yes_NULL, "OK", 3);
		create_button(volume_setup_widgets + 2, 41, 33, 6, 0, 2, 1, 1, 1,
			      dialog_cancel_NULL, "Cancel", 1);

		dialog = dialog_create_custom(22, 25, 36, 11, volume_setup_widgets,
					      3, 0, fast_volume_setup_draw_const, NULL);
		dialog->action_yes = fast_volume_setup_ok;
		dialog->action_cancel = fast_volume_setup_cancel;
	}
}

static void fast_volume_amplify(void)
{
	selection_amplify((100/fast_volume_percent)*100);
}

static void fast_volume_attenuate(void)
{
	selection_amplify(fast_volume_percent);
}

/* --------------------------------------------------------------------------------------------------------- */
/* normal (not fast volume) amplify */

static void volume_setup_draw_const(void)
{
	draw_text("Volume Amplification %", 29, 27, 0, 2);
	draw_box(25, 29, 52, 31, BOX_THIN | BOX_INNER | BOX_INSET);
}

static void volume_amplify_ok(UNUSED void *data)
{
	volume_percent = volume_setup_widgets[0].d.thumbbar.value;
	selection_amplify(volume_percent);
}

static int volume_amplify_jj(struct key_event *k)
{
	if (k->state == KEY_PRESS && (k->mod & KMOD_ALT) && (k->sym.sym == SDLK_j)) {
		dialog_yes(NULL);
		return 1;
	}
	return 0;
}

static void volume_amplify(void)
{
	struct dialog *dialog;

	CHECK_FOR_SELECTION(return);
	create_thumbbar(volume_setup_widgets + 0, 26, 30, 26, 0, 1, 1, NULL, 0, 200);
	volume_setup_widgets[0].d.thumbbar.value = volume_percent;
	create_button(volume_setup_widgets + 1, 31, 33, 6, 0, 1, 2, 2, 2, dialog_yes_NULL, "OK", 3);
	create_button(volume_setup_widgets + 2, 41, 33, 6, 0, 2, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);
	dialog = dialog_create_custom(22, 25, 36, 11, volume_setup_widgets,
				      3, 0, volume_setup_draw_const, NULL);
	dialog->handle_key = volume_amplify_jj;
	dialog->action_yes = volume_amplify_ok;
}

/* --------------------------------------------------------------------------------------------------------- */
/* vary depth */
static int current_vary = -1;

static void vary_setup_draw_const(void)
{
	draw_text("Vary depth limit %", 31, 27, 0, 2);
	draw_box(25, 29, 52, 31, BOX_THIN | BOX_INNER | BOX_INSET);
}

static void vary_amplify_ok(UNUSED void *data)
{
	vary_depth = volume_setup_widgets[0].d.thumbbar.value;
	selection_vary(0, vary_depth, current_vary);
}

static void vary_command(int how)
{
	struct dialog *dialog;

	create_thumbbar(volume_setup_widgets + 0, 26, 30, 26, 0, 1, 1, NULL, 0, 50);
	volume_setup_widgets[0].d.thumbbar.value = vary_depth;
	create_button(volume_setup_widgets + 1, 31, 33, 6, 0, 1, 2, 2, 2, dialog_yes_NULL, "OK", 3);
	create_button(volume_setup_widgets + 2, 41, 33, 6, 0, 2, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);
	dialog = dialog_create_custom(22, 25, 36, 11, volume_setup_widgets,
				      3, 0, vary_setup_draw_const, NULL);
	dialog->action_yes = vary_amplify_ok;
	current_vary = how;
}

static int current_effect(void)
{
	song_note_t *pattern, *cur_note;

	song_get_pattern(current_pattern, &pattern);
	cur_note = pattern + 64 * current_row + current_channel - 1;

	return cur_note->effect;
}

/* --------------------------------------------------------------------------------------------------------- */
/* settings */

#define CFG_SET_PE(v) cfg_set_number(cfg, "Pattern Editor", #v, v)
void cfg_save_patedit(cfg_file_t *cfg)
{
	int n;
	char s[65];

	CFG_SET_PE(link_effect_column);
	CFG_SET_PE(draw_divisions);
	CFG_SET_PE(centralise_cursor);
	CFG_SET_PE(highlight_current_row);
	CFG_SET_PE(edit_copy_mask);
	CFG_SET_PE(volume_percent);
	CFG_SET_PE(fast_volume_percent);
	CFG_SET_PE(fast_volume_mode);
	CFG_SET_PE(keyjazz_noteoff);
	CFG_SET_PE(keyjazz_write_noteoff);
	CFG_SET_PE(keyjazz_repeat);
	CFG_SET_PE(mask_copy_search_mode);
	CFG_SET_PE(invert_home_end);

	cfg_set_number(cfg, "Pattern Editor", "crayola_mode", !!(status.flags & CRAYOLA_MODE));
	for (n = 0; n < 64; n++)
		s[n] = track_view_scheme[n] + 'a';
	s[64] = 0;

	cfg_set_string(cfg, "Pattern Editor", "track_view_scheme", s);
	for (n = 0; n < 64; n++)
		s[n] = (channel_multi[n]) ? 'M' : '-';
	s[64] = 0;
	cfg_set_string(cfg, "Pattern Editor", "channel_multi", s);
}

#define CFG_GET_PE(v,d) v = cfg_get_number(cfg, "Pattern Editor", #v, d)
void cfg_load_patedit(cfg_file_t *cfg)
{
	int n, r = 0;
	char s[65];

	CFG_GET_PE(link_effect_column, 0);
	CFG_GET_PE(draw_divisions, 1);
	CFG_GET_PE(centralise_cursor, 0);
	CFG_GET_PE(highlight_current_row, 0);
	CFG_GET_PE(edit_copy_mask, MASK_NOTE | MASK_INSTRUMENT | MASK_VOLUME);
	CFG_GET_PE(volume_percent, 100);
	CFG_GET_PE(fast_volume_percent, 67);
	CFG_GET_PE(fast_volume_mode, 0);
	CFG_GET_PE(keyjazz_noteoff, 0);
	CFG_GET_PE(keyjazz_write_noteoff, 0);
	CFG_GET_PE(keyjazz_repeat, 1);
	CFG_GET_PE(mask_copy_search_mode, 0);
	CFG_GET_PE(invert_home_end, 0);

	if (cfg_get_number(cfg, "Pattern Editor", "crayola_mode", 0))
		status.flags |= CRAYOLA_MODE;
	else
		status.flags &= ~CRAYOLA_MODE;

	cfg_get_string(cfg, "Pattern Editor", "track_view_scheme", s, 64, "a");

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

	cfg_get_string(cfg, "Pattern Editor", "channel_multi", s, 64, "");
	memset(channel_multi, 0, sizeof(channel_multi));
	channel_multi_enabled = 0;
	for (n = 0; n < 64; n++) {
		if (!s[n])
			break;
		channel_multi[n] = ((s[n] >= 'A' && s[n] <= 'Z') || (s[n] >= 'a' && s[n] <= 'z')) ? 1 : 0;
		if (channel_multi[n])
			channel_multi_enabled = 1;
	}

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

	if (selection.first_row < 0) selection.first_row = 0;
	if (selection.last_row < 0) selection.last_row = 0;
	if (selection.first_channel < 1) selection.first_channel = 1;
	if (selection.last_channel < 1) selection.last_channel = 1;

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
	pattern_selection_system_copyout();
}

static void selection_clear(void)
{
	selection.first_channel = 0;
	pattern_selection_system_copyout();
}


// FIXME | this misbehaves if height is an odd number -- e.g. if an odd number
// FIXME | of rows is selected and 2 * sel_rows overlaps the end of the pattern
static void block_length_double(void)
{
	song_note_t *pattern, *src, *dest;
	int sel_rows, total_rows;
	int src_end, dest_end; // = first row that is NOT affected
	int width, height, offset;

	if (!SELECTION_EXISTS)
		return;

	status.flags |= SONG_NEEDS_SAVE;
	total_rows = song_get_pattern(current_pattern, &pattern);

	if (selection.last_row >= total_rows)
		selection.last_row = total_rows - 1;
	if (selection.first_row > selection.last_row)
		selection.first_row = selection.last_row;

	sel_rows = selection.last_row - selection.first_row + 1;
	offset = selection.first_channel - 1;
	width = selection.last_channel - offset;
	dest_end = MIN(selection.first_row + 2 * sel_rows, total_rows);
	height = dest_end - selection.first_row;
	src_end = selection.first_row + height / 2;

	src = pattern + 64 * (src_end - 1);
	dest = pattern + 64 * (dest_end - 1);

	pated_history_add("Undo block length double       (Alt-F)",
		offset, selection.first_row, width, height);

	while (dest > src) {
		memset(dest + offset, 0, width * sizeof(song_note_t));
		dest -= 64;
		memcpy(dest + offset, src + offset, width * sizeof(song_note_t));
		dest -= 64;
		src -= 64;
	}

	pattern_selection_system_copyout();
}

// FIXME: this should erase the end of the selection if 2 * sel_rows > total_rows
static void block_length_halve(void)
{
	song_note_t *pattern, *src, *dest;
	int sel_rows, src_end, total_rows, row;
	int width, height, offset;

	if (!SELECTION_EXISTS)
		return;

	status.flags |= SONG_NEEDS_SAVE;
	total_rows = song_get_pattern(current_pattern, &pattern);

	if (selection.last_row >= total_rows)
		selection.last_row = total_rows - 1;
	if (selection.first_row > selection.last_row)
		selection.first_row = selection.last_row;

	sel_rows = selection.last_row - selection.first_row + 1;
	offset = selection.first_channel - 1;
	width = selection.last_channel - offset;
	src_end = MIN(selection.first_row + 2 * sel_rows, total_rows);
	height = src_end - selection.first_row;
	src = dest = pattern + 64 * selection.first_row;

	pated_history_add("Undo block length halve        (Alt-G)",
		offset, selection.first_row, width, height);

	for (row = 0; row < height / 2; row++) {
		memcpy(dest + offset, src + offset, width * sizeof(song_note_t));
		src += 64 * 2;
		dest += 64;
	}

	pattern_selection_system_copyout();
}


static void selection_erase(void)
{
	song_note_t *pattern, *note;
	int row;
	int chan_width;
	int total_rows;

	if (!SELECTION_EXISTS)
		return;

	status.flags |= SONG_NEEDS_SAVE;
	total_rows = song_get_pattern(current_pattern, &pattern);
	if (selection.last_row >= total_rows)selection.last_row = total_rows-1;
	if (selection.first_row > selection.last_row) selection.first_row = selection.last_row;

	pated_history_add("Undo block cut                 (Alt-Z)",
		selection.first_channel - 1,
		selection.first_row,
		(selection.last_channel - selection.first_channel) + 1,
		(selection.last_row - selection.first_row) + 1);

	if (selection.first_channel == 1 && selection.last_channel == 64) {
		memset(pattern + 64 * selection.first_row, 0, (selection.last_row - selection.first_row + 1)
		       * 64 * sizeof(song_note_t));
	} else {
		chan_width = selection.last_channel - selection.first_channel + 1;
		for (row = selection.first_row; row <= selection.last_row; row++) {
			note = pattern + 64 * row + selection.first_channel - 1;
			memset(note, 0, chan_width * sizeof(song_note_t));
		}
	}
	pattern_selection_system_copyout();
}

static void selection_set_sample(void)
{
	int row, chan;
	song_note_t *pattern, *note;
	int total_rows;

	total_rows = song_get_pattern(current_pattern, &pattern);
	if (selection.last_row >= total_rows)selection.last_row = total_rows-1;
	if (selection.first_row > selection.last_row) selection.first_row = selection.last_row;

	status.flags |= SONG_NEEDS_SAVE;
	pated_history_add("Undo set sample/instrument     (Alt-S)",
		selection.first_channel - 1,
		selection.first_row,
		(selection.last_channel - selection.first_channel) + 1,
		(selection.last_row - selection.first_row) + 1);
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
	pattern_selection_system_copyout();
}


static void selection_swap(void)
{
	/* s_note = selection; p_note = position */
	song_note_t *pattern, *s_note, *p_note, tmp;
	int row, chan, sel_rows, sel_chans, total_rows;
	int affected_width, affected_height;

	CHECK_FOR_SELECTION(return);

	status.flags |= SONG_NEEDS_SAVE;
	total_rows = song_get_pattern(current_pattern, &pattern);
	if (selection.last_row >= total_rows)selection.last_row = total_rows-1;
	if (selection.first_row > selection.last_row) selection.first_row = selection.last_row;
	sel_rows = selection.last_row - selection.first_row + 1;
	sel_chans = selection.last_channel - selection.first_channel + 1;

	affected_width = MAX(selection.last_channel, current_channel + sel_chans - 1)
			- MIN(selection.first_channel, current_channel) + 1;
	affected_height = MAX(selection.last_row, current_row + sel_rows - 1)
			- MIN(selection.first_row, current_row) + 1;

	/* The minimum combined size for the two blocks is double the number of rows in the selection by
	 * double the number of channels. So, if the width and height don't add up, they must overlap. It's
	 * of course possible to have the blocks adjacent but not overlapping -- there is only overlap if
	 * *both* the width and height are less than double the size. */
	if (affected_width < 2 * sel_chans && affected_height < 2 * sel_rows) {
		dialog_create(DIALOG_OK, "   Swap blocks overlap    ", NULL, NULL, 0, NULL);
		return;
	}

	if (current_row + sel_rows > total_rows || current_channel + sel_chans - 1 > 64) {
		dialog_create(DIALOG_OK, "   Out of pattern range   ", NULL, NULL, 0, NULL);
		return;
	}

	pated_history_add("Undo swap block                (Alt-Y)",
		MIN(selection.first_channel, current_channel) - 1,
		MIN(selection.first_row, current_row),
		affected_width, affected_height);

	for (row = 0; row < sel_rows; row++) {
		s_note = pattern + 64 * (selection.first_row + row) + selection.first_channel - 1;
		p_note = pattern + 64 * (current_row + row) + current_channel - 1;
		for (chan = 0; chan < sel_chans; chan++, s_note++, p_note++) {
			tmp = *s_note;
			*s_note = *p_note;
			*p_note = tmp;
		}
	}
	pattern_selection_system_copyout();
}

static void selection_set_volume(void)
{
	int row, chan, total_rows;
	song_note_t *pattern, *note;

	CHECK_FOR_SELECTION(return);

	status.flags |= SONG_NEEDS_SAVE;
	total_rows = song_get_pattern(current_pattern, &pattern);
	if (selection.last_row >= total_rows)selection.last_row = total_rows-1;
	if (selection.first_row > selection.last_row) selection.first_row = selection.last_row;

	pated_history_add("Undo set volume/panning        (Alt-V)",
		selection.first_channel - 1,
		selection.first_row,
		(selection.last_channel - selection.first_channel) + 1,
		(selection.last_row - selection.first_row) + 1);

	for (row = selection.first_row; row <= selection.last_row; row++) {
		note = pattern + 64 * row + selection.first_channel - 1;
		for (chan = selection.first_channel; chan <= selection.last_channel; chan++, note++) {
			note->volparam = mask_note.volparam;
			note->voleffect = mask_note.voleffect;
		}
	}
	pattern_selection_system_copyout();
}

/* The logic for this one makes my head hurt. */
static void selection_slide_volume(void)
{
	int row, chan, total_rows;
	song_note_t *pattern, *note, *last_note;
	int first, last;                /* the volumes */
	int ve, lve;                    /* volume effect */

	/* FIXME: if there's no selection, should this display a dialog, or bail silently? */
	/* Impulse Tracker displays a box "No block is marked" */
	CHECK_FOR_SELECTION(return);
	total_rows = song_get_pattern(current_pattern, &pattern);
	if (selection.last_row >= total_rows)selection.last_row = total_rows-1;
	if (selection.first_row > selection.last_row) selection.first_row = selection.last_row;

	/* can't slide one row */
	if (selection.first_row == selection.last_row)
		return;

	status.flags |= SONG_NEEDS_SAVE;

	pated_history_add("Undo volume or panning slide   (Alt-K)",
		selection.first_channel - 1,
		selection.first_row,
		(selection.last_channel - selection.first_channel) + 1,
		(selection.last_row - selection.first_row) + 1);

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

		ve = note->voleffect;
		lve = last_note->voleffect;

		first = note->volparam;
		last = last_note->volparam;

		/* Note: IT only uses the sample's default volume if there is an instrument number *AND* a
		note. I'm just checking the instrument number, as it's the minimal information needed to
		get the default volume for the instrument.

		Would be nice but way hard to do: if there's a note but no sample number, look back in the
		pattern and use the last sample number in that channel (if there is one). */
		if (ve == VOLFX_NONE) {
			if (note->instrument == 0)
				continue;
			ve = VOLFX_VOLUME;
			/* Modplug hack: volume bit shift */
			first = song_get_sample(note->instrument)->volume >> 2;
		}

		if (lve == VOLFX_NONE) {
			if (last_note->instrument == 0)
				continue;
			lve = VOLFX_VOLUME;
			last = song_get_sample(last_note->instrument)->volume >> 2;
		}

		if (!(ve == lve && (ve == VOLFX_VOLUME || ve == VOLFX_PANNING))) {
			continue;
		}

		for (row = selection.first_row; row <= selection.last_row; row++, note += 64) {
			note->voleffect = ve;
			note->volparam = (((last - first)
					 * (row - selection.first_row)
					 / (selection.last_row - selection.first_row)
					 ) + first);
		}
	}
	pattern_selection_system_copyout();
}

static void selection_wipe_volume(int reckless)
{
	int row, chan, total_rows;
	song_note_t *pattern, *note;

	CHECK_FOR_SELECTION(return);
	total_rows = song_get_pattern(current_pattern, &pattern);
	if (selection.last_row >= total_rows)selection.last_row = total_rows-1;
	if (selection.first_row > selection.last_row) selection.first_row = selection.last_row;

	status.flags |= SONG_NEEDS_SAVE;

	pated_history_add((reckless
				? "Recover volumes/pannings     (2*Alt-K)"
				: "Replace extra volumes/pannings (Alt-W)"),
		selection.first_channel - 1,
		selection.first_row,
		(selection.last_channel - selection.first_channel) + 1,
		(selection.last_row - selection.first_row) + 1);


	for (row = selection.first_row; row <= selection.last_row; row++) {
		note = pattern + 64 * row + selection.first_channel - 1;
		for (chan = selection.first_channel; chan <= selection.last_channel; chan++, note++) {
			if (reckless || (note->instrument == 0 && !NOTE_IS_NOTE(note->note))) {
				note->volparam = 0;
				note->voleffect = VOLFX_NONE;
			}
		}
	}
	pattern_selection_system_copyout();
}
static int vary_value(int ov, int limit, int depth)
{
	int j;
	j = (int)((((float)limit)*rand()) / (RAND_MAX+1.0));
	j = ((limit >> 1) - j);
	j = ov+((j * depth) / 100);
	if (j < 0) j = 0;
	if (j > limit) j = limit;
	return j;
}

static int common_variable_group(int ch)
{
	switch (ch) {
	case FX_PORTAMENTODOWN:
	case FX_PORTAMENTOUP:
	case FX_TONEPORTAMENTO:
		return FX_TONEPORTAMENTO;
	case FX_VOLUMESLIDE:
	case FX_TONEPORTAVOL:
	case FX_VIBRATOVOL:
		return FX_VOLUMESLIDE;
	case FX_PANNING:
	case FX_PANNINGSLIDE:
	case FX_PANBRELLO:
		return FX_PANNING;
	default:
		return ch; /* err... */
	};
}

static void selection_vary(int fast, int depth, int how)
{
	int row, chan, total_rows;
	song_note_t *pattern, *note;
	static char last_vary[39];
	const char *vary_how;
	char ch;

	/* don't ever vary these things */
	switch (how) {
	default:
		if (!FX_IS_EFFECT(how))
			return;
		break;

	case FX_NONE:
	case FX_SPECIAL:
	case FX_SPEED:
	case FX_POSITIONJUMP:
	case FX_PATTERNBREAK:

	case FX_KEYOFF:
	case FX_SETENVPOSITION:
	case FX_VOLUME:
	case FX_NOTESLIDEUP:
	case FX_NOTESLIDEDOWN:
			return;
	}

	CHECK_FOR_SELECTION(return);

	status.flags |= SONG_NEEDS_SAVE;
	switch (how) {
	case FX_CHANNELVOLUME:
	case FX_CHANNELVOLSLIDE:
		vary_how = "Undo volume-channel vary      (Ctrl-U)";
		if (fast) status_text_flash("Fast volume vary");
		break;
	case FX_PANNING:
	case FX_PANNINGSLIDE:
	case FX_PANBRELLO:
		vary_how = "Undo panning vary             (Ctrl-Y)";
		if (fast) status_text_flash("Fast panning vary");
		break;
	default:
		sprintf(last_vary, "%-28s  (Ctrl-K)",
			"Undo Xxx effect-value vary");
		last_vary[5] = common_variable_group(how);
		if (fast) status_text_flash("Fast %-21s", last_vary+5);
		vary_how = last_vary;
		break;
	};

	total_rows = song_get_pattern(current_pattern, &pattern);
	if (selection.last_row >= total_rows)selection.last_row = total_rows-1;
	if (selection.first_row > selection.last_row) selection.first_row = selection.last_row;

	pated_history_add(vary_how,
		selection.first_channel - 1,
		selection.first_row,
		(selection.last_channel - selection.first_channel) + 1,
		(selection.last_row - selection.first_row) + 1);

	for (row = selection.first_row; row <= selection.last_row; row++) {
		note = pattern + 64 * row + selection.first_channel - 1;
		for (chan = selection.first_channel; chan <= selection.last_channel; chan++, note++) {
			if (how == FX_CHANNELVOLUME || how == FX_CHANNELVOLSLIDE) {
				if (note->voleffect == VOLFX_VOLUME) {
					note->volparam = vary_value(note->volparam, 64, depth);
				}
			}
			if (how == FX_PANNINGSLIDE || how == FX_PANNING || how == FX_PANBRELLO) {
				if (note->voleffect == VOLFX_PANNING) {
					note->volparam = vary_value(note->volparam, 64, depth);
				}
			}

			ch = note->effect;
			if (!FX_IS_EFFECT(ch)) continue;
			if (common_variable_group(ch) != common_variable_group(how)) continue;
			switch (ch) {
			/* these are .0 0. and .f f. values */
			case FX_VOLUMESLIDE:
			case FX_CHANNELVOLSLIDE:
			case FX_PANNINGSLIDE:
			case FX_GLOBALVOLSLIDE:
			case FX_VIBRATOVOL:
			case FX_TONEPORTAVOL:
				if ((note->param & 15) == 15) continue;
				if ((note->param & 0xF0) == (0xF0))continue;
				if ((note->param & 15) == 0) {
					note->param = (1+(vary_value(note->param>>4, 15, depth))) << 4;
				} else {
					note->param = 1+(vary_value(note->param & 15, 15, depth));
				}
				break;
			/* tempo has a slide */
			case FX_TEMPO:
				if ((note->param & 15) == 15) continue;
				if ((note->param & 0xF0) == (0xF0))continue;
				/* but otherwise it's absolute */
				note->param = 1 + (vary_value(note->param, 255, depth));
				break;
			/* don't vary .E. and .F. values */
			case FX_PORTAMENTODOWN:
			case FX_PORTAMENTOUP:
				if ((note->param & 15) == 15) continue;
				if ((note->param & 15) == 14) continue;
				if ((note->param & 0xF0) == (0xF0))continue;
				if ((note->param & 0xF0) == (0xE0))continue;
				note->param = 16 + (vary_value(note->param-16, 224, depth));
				break;
			/* these are all "xx" commands */
			// FIXME global/channel volume should be limited to 0-128 and 0-64, respectively
			case FX_TONEPORTAMENTO:
			case FX_CHANNELVOLUME:
			case FX_OFFSET:
			case FX_GLOBALVOLUME:
			case FX_PANNING:
				note->param = 1 + (vary_value(note->param, 255, depth));
				break;
			/* these are all "xy" commands */
			case FX_VIBRATO:
			case FX_TREMOR:
			case FX_ARPEGGIO:
			case FX_RETRIG:
			case FX_TREMOLO:
			case FX_PANBRELLO:
			case FX_FINEVIBRATO:
				note->param = (1 + (vary_value(note->param & 15, 15, depth)))
					| ((1 + (vary_value((note->param >> 4) & 15, 15, depth))) << 4);
				break;
			};
		}
	}
	pattern_selection_system_copyout();
}
static void selection_amplify(int percentage)
{
	int row, chan, volume, total_rows;
	song_note_t *pattern, *note;

	if (!SELECTION_EXISTS)
		return;

	status.flags |= SONG_NEEDS_SAVE;
	total_rows = song_get_pattern(current_pattern, &pattern);
	if (selection.last_row >= total_rows)selection.last_row = total_rows-1;
	if (selection.first_row > selection.last_row) selection.first_row = selection.last_row;

	/* it says Alt-J even when Alt-I was used */
	pated_history_add("Undo volume amplification      (Alt-J)",
		selection.first_channel - 1,
		selection.first_row,
		(selection.last_channel - selection.first_channel) + 1,
		(selection.last_row - selection.first_row) + 1);

	for (row = selection.first_row; row <= selection.last_row; row++) {
		note = pattern + 64 * row + selection.first_channel - 1;
		for (chan = selection.first_channel; chan <= selection.last_channel; chan++, note++) {
			if (note->voleffect == VOLFX_NONE && note->instrument != 0) {
				/* Modplug hack: volume bit shift */
				if (song_is_instrument_mode())
					volume = 64; /* XXX */
				else
					volume = song_get_sample(note->instrument)->volume >> 2;
			} else if (note->voleffect == VOLFX_VOLUME) {
				volume = note->volparam;
			} else {
				continue;
			}
			volume *= percentage;
			volume /= 100;
			if (volume > 64) volume = 64;
			else if (volume < 0) volume = 0;
			note->volparam = volume;
			note->voleffect = VOLFX_VOLUME;
		}
	}
	pattern_selection_system_copyout();
}

static void selection_slide_effect(void)
{
	int row, chan, total_rows;
	song_note_t *pattern, *note;
	int first, last;                /* the effect values */

	/* FIXME: if there's no selection, should this display a dialog, or bail silently? */
	CHECK_FOR_SELECTION(return);
	total_rows = song_get_pattern(current_pattern, &pattern);
	if (selection.last_row >= total_rows)selection.last_row = total_rows-1;
	if (selection.first_row > selection.last_row) selection.first_row = selection.last_row;

	if (selection.first_row == selection.last_row)
		return;

	status.flags |= SONG_NEEDS_SAVE;

	pated_history_add("Undo effect data slide         (Alt-X)",
		selection.first_channel - 1,
		selection.first_row,
		(selection.last_channel - selection.first_channel) + 1,
		(selection.last_row - selection.first_row) + 1);

	/* the channel loop has to go on the outside for this one */
	for (chan = selection.first_channel; chan <= selection.last_channel; chan++) {
		note = pattern + chan - 1;
		first = note[64 * selection.first_row].param;
		last = note[64 * selection.last_row].param;
		note += 64 * selection.first_row;
		for (row = selection.first_row; row <= selection.last_row; row++, note += 64) {
			note->param = (((last - first)
					    * (row - selection.first_row)
					    / (selection.last_row - selection.first_row)
					    ) + first);
		}
	}
	pattern_selection_system_copyout();
}

static void selection_wipe_effect(void)
{
	int row, chan, total_rows;
	song_note_t *pattern, *note;

	CHECK_FOR_SELECTION(return);
	total_rows = song_get_pattern(current_pattern, &pattern);
	if (selection.last_row >= total_rows)selection.last_row = total_rows-1;
	if (selection.first_row > selection.last_row) selection.first_row = selection.last_row;

	status.flags |= SONG_NEEDS_SAVE;

	pated_history_add("Recover effects/effect data  (2*Alt-X)",
		selection.first_channel - 1,
		selection.first_row,
		(selection.last_channel - selection.first_channel) + 1,
		(selection.last_row - selection.first_row) + 1);

	for (row = selection.first_row; row <= selection.last_row; row++) {
		note = pattern + 64 * row + selection.first_channel - 1;
		for (chan = selection.first_channel; chan <= selection.last_channel; chan++, note++) {
			note->effect = 0;
			note->param = 0;
		}
	}
	pattern_selection_system_copyout();
}


enum roll_dir { ROLL_DOWN = -1, ROLL_UP = +1 };
static void selection_roll(enum roll_dir direction)
{
	song_note_t *pattern, *seldata;
	int row, sel_rows, sel_chans, total_rows, copy_bytes, n;

	if (!SELECTION_EXISTS) { return; }
	total_rows = song_get_pattern(current_pattern, &pattern);
	if (selection.last_row >= total_rows) { selection.last_row = total_rows - 1; }
	if (selection.first_row > selection.last_row) { selection.first_row = selection.last_row; }
	sel_rows = selection.last_row - selection.first_row + 1;
	sel_chans = selection.last_channel - selection.first_channel + 1;
	if (sel_rows < 2) { return; }
	seldata = pattern + 64 * selection.first_row + selection.first_channel - 1;

	song_note_t temp[sel_chans];
	copy_bytes = sizeof(temp);
	row = (direction == ROLL_DOWN ? sel_rows - 1 : 0);
	memcpy(temp, seldata + 64 * row, copy_bytes);
	for (n = 1; n < sel_rows; n++, row += direction) {
		memcpy(seldata + 64 * row, seldata + 64 * (row + direction), copy_bytes);
	}
	memcpy(seldata + 64 * row, temp, copy_bytes);

	status.flags |= SONG_NEEDS_SAVE;
}

/* --------------------------------------------------------------------------------------------------------- */
/* Row shifting operations */

/* A couple of the param names here might seem a bit confusing, so:
 *     what_row = what row to start the insert (generally this would be current_row)
 *     num_rows = the number of rows to insert */
static void pattern_insert_rows(int what_row, int num_rows, int first_channel, int chan_width)
{
	song_note_t *pattern;
	int row, total_rows = song_get_pattern(current_pattern, &pattern);

	status.flags |= SONG_NEEDS_SAVE;
	if (first_channel < 1)
		first_channel = 1;
	if (chan_width + first_channel - 1 > 64)
		chan_width = 64 - first_channel + 1;

	if (num_rows + what_row > total_rows)
		num_rows = total_rows - what_row;

	if (first_channel == 1 && chan_width == 64) {
		memmove(pattern + 64 * (what_row + num_rows), pattern + 64 * what_row,
			64 * sizeof(song_note_t) * (total_rows - what_row - num_rows));
		memset(pattern + 64 * what_row, 0, num_rows * 64 * sizeof(song_note_t));
	} else {
		/* shift the area down */
		for (row = total_rows - num_rows - 1; row >= what_row; row--) {
			memmove(pattern + 64 * (row + num_rows) + first_channel - 1,
				pattern + 64 * row + first_channel - 1, chan_width * sizeof(song_note_t));
		}
		/* clear the inserted rows */
		for (row = what_row; row < what_row + num_rows; row++) {
			memset(pattern + 64 * row + first_channel - 1, 0, chan_width * sizeof(song_note_t));
		}
	}
	pattern_selection_system_copyout();
}

/* Same as above, but with a couple subtle differences. */
static void pattern_delete_rows(int what_row, int num_rows, int first_channel, int chan_width)
{
	song_note_t *pattern;
	int row, total_rows = song_get_pattern(current_pattern, &pattern);

	status.flags |= SONG_NEEDS_SAVE;
	if (first_channel < 1)
		first_channel = 1;
	if (chan_width + first_channel - 1 > 64)
		chan_width = 64 - first_channel + 1;

	if (num_rows + what_row > total_rows)
		num_rows = total_rows - what_row;

	if (first_channel == 1 && chan_width == 64) {
		memmove(pattern + 64 * what_row, pattern + 64 * (what_row + num_rows),
			64 * sizeof(song_note_t) * (total_rows - what_row - num_rows));
		memset(pattern + 64 * (total_rows - num_rows), 0, num_rows * 64 * sizeof(song_note_t));
	} else {
		/* shift the area up */
		for (row = what_row; row <= total_rows - num_rows - 1; row++) {
			memmove(pattern + 64 * row + first_channel - 1,
				pattern + 64 * (row + num_rows) + first_channel - 1,
				chan_width * sizeof(song_note_t));
		}
		/* clear the last rows */
		for (row = total_rows - num_rows; row < total_rows; row++) {
			memset(pattern + 64 * row + first_channel - 1, 0, chan_width * sizeof(song_note_t));
		}
	}
	pattern_selection_system_copyout();
}

/* --------------------------------------------------------------------------------------------------------- */
/* history/undo */

static void pated_history_clear(void)
{
	// clear undo history
	int i;
	for (i = 0; i < 10; i++) {
		if (undo_history[i].snap_op_allocated)
			free((void *) undo_history[i].snap_op);
		free(undo_history[i].data);

		memset(&undo_history[i],0,sizeof(struct pattern_snap));
		undo_history[i].snap_op = "Empty";
		undo_history[i].snap_op_allocated = 0;
	}

}

static void set_note_note(song_note_t *n, int a, int b)
{
	if (a > 0 && a < 250) {
		a += b;
		if (a <= 0 || a >= 250) a = 0;
	}
	n->note = a;
}

static void snap_paste(struct pattern_snap *s, int x, int y, int xlate)
{
	song_note_t *pattern, *p_note;
	int row, num_rows, chan_width;
	int chan;


	status.flags |= SONG_NEEDS_SAVE;
	if (x < 0) x = s->x;
	if (y < 0) y = s->y;

	num_rows = song_get_pattern(current_pattern, &pattern);
	num_rows -= y;
	if (s->rows < num_rows)
		num_rows = s->rows;
	if (num_rows <= 0) return;

	chan_width = s->channels;
	if (chan_width + x >= 64)
		chan_width = 64 - x;

	for (row = 0; row < num_rows; row++) {
		p_note = pattern + 64 * (y + row) + x;
		memcpy(pattern + 64 * (y + row) + x,
		       s->data + s->channels * row, chan_width * sizeof(song_note_t));
		if (!xlate) continue;
		for (chan = 0; chan < chan_width; chan++) {
			if (chan + x > 64) break; /* defensive */
			set_note_note(p_note+chan,
					p_note[chan].note,
					xlate);
		}
	}
	pattern_selection_system_copyout();
}

static void snap_copy(struct pattern_snap *s, int x, int y, int width, int height)
{
	song_note_t *pattern;
	int row, total_rows, len;

	memused_songchanged();
	s->channels = width;
	s->rows = height;

	total_rows = song_get_pattern(current_pattern, &pattern);
	s->data = mem_alloc(len = (sizeof(song_note_t) * s->channels * s->rows));

	if (s->rows > total_rows) {
		memset(s->data, 0,  len);
	}

	s->x = x; s->y = y;
	if (x == 0 && width == 64) {
		if (height >total_rows) height = total_rows;
		memcpy(s->data, pattern + 64 * y, (width*height*sizeof(song_note_t)));
	} else {
		for (row = 0; row < s->rows && row < total_rows; row++) {
			memcpy(s->data + s->channels * row,
			       pattern + 64 * (row + s->y) + s->x,
			       s->channels * sizeof(song_note_t));
		}
	}
}

static int snap_honor_mute(struct pattern_snap *s, int base_channel)
{
	int i,j;
	song_note_t *n;
	int mute[64];
	int did_any;

	for (i = 0; i < s->channels; i++) {
		mute[i] = (song_get_channel(i+base_channel)->flags & CHN_MUTE);
	}

	n = s->data;
	did_any = 0;
	for (j = 0; j < s->rows; j++) {
		for (i = 0; i < s->channels; i++) {
			if (mute[i]) {
				memset(n, 0, sizeof(song_note_t));
				did_any = 1;
			}
			n++;
		}
	}

	return did_any;
}

static void pated_history_restore(int n)
{
	if (n < 0 || n > 9) return;
	snap_paste(&undo_history[n], -1, -1, 0);

}

static void pated_save(const char *descr)
{
	int total_rows;

	total_rows = song_get_pattern(current_pattern, NULL);
	pated_history_add(descr,0,0,64,total_rows);
}
static void pated_history_add(const char *descr, int x, int y, int width, int height)
{
	pated_history_add2(0, descr, x, y, width, height);
}
static void pated_history_add_grouped(const char *descr, int x, int y, int width, int height)
{
	pated_history_add2(1, descr, x, y, width, height);
}
static void pated_history_add2(int groupedf, const char *descr, int x, int y, int width, int height)
{
	int j;

	j = undo_history_top;
	if (groupedf
	&& undo_history[j].patternno == current_pattern
	&& undo_history[j].x == x && undo_history[j].y == y
	&& undo_history[j].channels == width
	&& undo_history[j].rows == height
	&& undo_history[j].snap_op
	&& strcmp(undo_history[j].snap_op, descr) == 0) {

		/* do nothing; use the previous bit of history */

	} else {
		j = (undo_history_top + 1) % 10;
		free(undo_history[j].data);
		snap_copy(&undo_history[j], x, y, width, height);
		undo_history[j].snap_op = str_dup(descr);
		undo_history[j].snap_op_allocated = 1;
		undo_history[j].patternno = current_pattern;
		undo_history_top = j;
	}
}
static void fast_save_update(void)
{
	int total_rows;

	free(fast_save.data);
	fast_save.data = NULL;

	total_rows = song_get_pattern(current_pattern, NULL);

	snap_copy(&fast_save, 0, 0, 64, total_rows);
}

/* clipboard */
static void clipboard_free(void)
{
	free(clipboard.data);
	clipboard.data = NULL;
}

/* clipboard_copy is fundementally the same as selection_erase
 * except it uses memcpy instead of memset :) */
static void clipboard_copy(int honor_mute)
{
	int flag;

	CHECK_FOR_SELECTION(return);

	clipboard_free();

	snap_copy(&clipboard,
		selection.first_channel - 1,
		selection.first_row,
		(selection.last_channel - selection.first_channel) + 1,
		(selection.last_row - selection.first_row) + 1);

	flag = 0;
	if (honor_mute) {
		flag = snap_honor_mute(&clipboard, selection.first_channel-1);
	}

	/* transfer to system where appropriate */
	clippy_yank();

	if (flag) {
		status_text_flash("Selection honors current mute settings");
	}
}

static void clipboard_paste_overwrite(int suppress, int grow)
{
	song_note_t *pattern;
	int num_rows, chan_width;

	if (clipboard.data == NULL) {
		dialog_create(DIALOG_OK, "No data in clipboard", NULL, NULL, 0, NULL);
		return;
	}

	num_rows = song_get_pattern(current_pattern, &pattern);
	num_rows -= current_row;
	if (clipboard.rows < num_rows)
		num_rows = clipboard.rows;

	if (clipboard.rows > num_rows && grow) {
		if (current_row+clipboard.rows > 200) {
			status_text_flash("Resized pattern %d, but clipped to 200 rows", current_pattern);
			song_pattern_resize(current_pattern, 200);
		} else {
			status_text_flash("Resized pattern %d to %d rows", current_pattern,
					  current_row + clipboard.rows);
			song_pattern_resize(current_pattern, current_row+clipboard.rows);
		}
	}

	chan_width = clipboard.channels;
	if (chan_width + current_channel > 64)
		chan_width = 64 - current_channel + 1;

	if (!suppress) {
		pated_history_add_grouped("Replace overwritten data       (Alt-O)",
					current_channel-1, current_row,
					chan_width, num_rows);
	}
	snap_paste(&clipboard, current_channel-1, current_row, 0);
}
static void clipboard_paste_insert(void)
{
	int num_rows, total_rows, chan_width;
	song_note_t *pattern;

	if (clipboard.data == NULL) {
		dialog_create(DIALOG_OK, "No data in clipboard", NULL, NULL, 0, NULL);
		return;
	}

	total_rows = song_get_pattern(current_pattern, &pattern);

	pated_save("Undo paste data                (Alt-P)");

	num_rows = total_rows - current_row;
	if (clipboard.rows < num_rows)
		num_rows = clipboard.rows;

	chan_width = clipboard.channels;
	if (chan_width + current_channel > 64)
		chan_width = 64 - current_channel + 1;

	pattern_insert_rows(current_row, clipboard.rows, current_channel, chan_width);
	clipboard_paste_overwrite(1, 0);
	pattern_selection_system_copyout();
}

static void clipboard_paste_mix_notes(int clip, int xlate)
{
	int row, chan, num_rows, chan_width;
	song_note_t *pattern, *p_note, *c_note;

	if (clipboard.data == NULL) {
		dialog_create(DIALOG_OK, "No data in clipboard", NULL, NULL, 0, NULL);
		return;
	}

	status.flags |= SONG_NEEDS_SAVE;
	num_rows = song_get_pattern(current_pattern, &pattern);
	num_rows -= current_row;
	if (clipboard.rows < num_rows)
		num_rows = clipboard.rows;

	chan_width = clipboard.channels;
	if (chan_width + current_channel > 64)
		chan_width = 64 - current_channel + 1;


/* note that IT doesn't do this for "fields" either... */
	pated_history_add_grouped("Replace mixed data             (Alt-M)",
				current_channel-1, current_row,
				chan_width, num_rows);

	p_note = pattern + 64 * current_row + current_channel - 1;
	c_note = clipboard.data;
	for (row = 0; row < num_rows; row++) {
		for (chan = 0; chan < chan_width; chan++) {
			if (memcmp(p_note + chan, blank_note, sizeof(song_note_t)) == 0) {

				p_note[chan] = c_note[chan];
				set_note_note(p_note+chan,
						c_note[chan].note,
						xlate);
				if (clip) {
					p_note[chan].instrument = song_get_current_instrument();
					if (edit_copy_mask & MASK_VOLUME) {
						p_note[chan].voleffect = mask_note.voleffect;
						p_note[chan].volparam = mask_note.volparam;
					} else {
						p_note[chan].voleffect = 0;
						p_note[chan].volparam = 0;
					}
					if (edit_copy_mask & MASK_EFFECT) {
						p_note[chan].effect = mask_note.effect;
						p_note[chan].param = mask_note.param;
					}
				}
			}
		}
		p_note += 64;
		c_note += clipboard.channels;
	}
}

/* Same code as above. Maybe I should generalize it. */
static void clipboard_paste_mix_fields(int prec, int xlate)
{
	int row, chan, num_rows, chan_width;
	song_note_t *pattern, *p_note, *c_note;

	if (clipboard.data == NULL) {
		dialog_create(DIALOG_OK, "No data in clipboard", NULL, NULL, 0, NULL);
		return;
	}

	status.flags |= SONG_NEEDS_SAVE;
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
			if (prec) {
				/* clipboard precedence */
				if (c_note[chan].note != 0) {
					set_note_note(p_note+chan,
							c_note[chan].note,
							xlate);
				}
				if (c_note[chan].instrument != 0)
					p_note[chan].instrument = c_note[chan].instrument;
				if (c_note[chan].voleffect != VOLFX_NONE) {
					p_note[chan].voleffect = c_note[chan].voleffect;
					p_note[chan].volparam = c_note[chan].volparam;
				}
				if (c_note[chan].effect != 0) {
					p_note[chan].effect = c_note[chan].effect;
				}
				if (c_note[chan].param != 0)
					p_note[chan].param = c_note[chan].param;
			} else {
				if (p_note[chan].note == 0) {
					set_note_note(p_note+chan,
							c_note[chan].note,
							xlate);
				}
				if (p_note[chan].instrument == 0)
					p_note[chan].instrument = c_note[chan].instrument;
				if (p_note[chan].voleffect == VOLFX_NONE) {
					p_note[chan].voleffect = c_note[chan].voleffect;
					p_note[chan].volparam = c_note[chan].volparam;
				}
				if (p_note[chan].effect == 0) {
					p_note[chan].effect = c_note[chan].effect;
				}
				if (p_note[chan].param == 0)
					p_note[chan].param = c_note[chan].param;
			}
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
	if (top_display_row < 0)
		top_display_row = 0;
}

static void advance_cursor(int next_row, int multichannel)
{
	int total_rows;

	if (next_row && !((song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP)) && playback_tracing)) {
		total_rows = song_get_rows_in_pattern(current_pattern);

		if (skip_value) {
			if (current_row + skip_value <= total_rows) {
				current_row += skip_value;
				pattern_editor_reposition();
			}
		} else {
			if (current_channel < 64) {
				current_channel++;
			} else {
				current_channel = 1;
				if (current_row < total_rows)
					current_row++;
			}
			pattern_editor_reposition();
		}
	}
	if (multichannel) {
		current_channel = multichannel_get_next(current_channel);
	}
}

/* --------------------------------------------------------------------- */

void update_current_row(void)
{
	char buf[4];

	draw_text(numtostr(3, current_row, buf), 12, 7, 5, 0);
	draw_text(numtostr(3, song_get_rows_in_pattern(current_pattern), buf), 16, 7, 5, 0);
}

int get_current_channel(void)
{
	return current_channel;
}

void set_current_channel(int channel)
{
	current_channel = CLAMP(channel, 0, 64);
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
	char buf[4];

	draw_text(numtostr(3, current_pattern, buf), 12, 6, 5, 0);
	draw_text(numtostr(3, csf_get_num_patterns(current_song) - 1, buf), 16, 6, 5, 0);
}

int get_current_pattern(void)
{
	return current_pattern;
}

static void _pattern_update_magic(void)
{
	song_sample_t *s;
	int i;

	for (i = 1; i <= 99; i++) {
		s = song_get_sample(i);
		if (!s) continue;
		if (((unsigned char)s->name[23]) != 0xFF) continue;
		if (((unsigned char)s->name[24]) != current_pattern) continue;
		disko_writeout_sample(i,current_pattern,1);
		break;
	}
}

void set_current_pattern(int n)
{
	int total_rows;
	char undostr[64];

	if (!playback_tracing || !(song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP))) {
		_pattern_update_magic();
	}

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

	/* save pattern */
	sprintf(undostr, "Pattern %d", current_pattern);
	pated_save(undostr);
	fast_save_update();

	pattern_editor_reposition();
	pattern_selection_system_copyout();

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

void play_song_from_mark_orderpan(void)
{
	if (marked_pattern == -1) {
		song_start_at_order(get_current_order(), current_row);
	} else {
		song_start_at_pattern(marked_pattern, marked_row);
	}
}
void play_song_from_mark(void)
{
	int new_order;

	if (marked_pattern != -1) {
		song_start_at_pattern(marked_pattern, marked_row);
		return;
	}

	new_order = get_current_order();
	while (new_order < 255) {
		if (current_song->orderlist[new_order] == current_pattern) {
			set_current_order(new_order);
			song_start_at_order(new_order, current_row);
			return;
		}
		new_order++;
	}
	new_order = 0;
	while (new_order < 255) {
		if (current_song->orderlist[new_order] == current_pattern) {
			set_current_order(new_order);
			song_start_at_order(new_order, current_row);
			return;
		}
		new_order++;
	}
	song_start_at_pattern(current_pattern, current_row);
}

/* --------------------------------------------------------------------- */

static void recalculate_visible_area(void)
{
	int n, last = 0, new_width;

	visible_width = 0;
	for (n = 0; n < 64; n++) {
		if (track_view_scheme[n] >= NUM_TRACK_VIEWS) {
			/* shouldn't happen, but might (e.g. if someone was messing with the config file) */
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
		/* a division after the last channel would look pretty dopey :) */
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
	char buf[4];
	song_note_t *pattern, *note;
	const struct track_view *track_view;
	int total_rows;
	int fg, bg;
	int mc = (status.flags & INVERTED_PALETTE) ? 1 : 3; /* mask color */
	int pattern_is_playing = ((song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP)) != 0
				  && current_pattern == playing_pattern);

	if (template_mode) {
		draw_text_len(template_mode_names[template_mode], 60, 2, 12, 3, 2);
	}

	/* draw the outer box around the whole thing */
	draw_box(4, 14, 5 + visible_width, 47, BOX_THICK | BOX_INNER | BOX_INSET);

	/* how many rows are there? */
	total_rows = song_get_pattern(current_pattern, &pattern);

	for (chan = top_display_channel, chan_pos = 0; chan_pos < visible_channels; chan++, chan_pos++) {
		track_view = track_views + track_view_scheme[chan_pos];
		/* maybe i'm retarded but the pattern editor should be dealing
		   with the same concept of "channel" as the rest of the
		   interface. the mixing channels really could be any arbitrary
		   number -- modplug just happens to reserve the first 64 for
		   "real" channels. i'd rather pm not replicate this cruft and
		   more or less hide the mixer from the interface... */
		track_view->draw_channel_header(chan, chan_drawpos, 14,
						((song_get_channel(chan - 1)->flags & CHN_MUTE) ? 0 : 3));

		note = pattern + 64 * top_display_row + chan - 1;
		for (row = top_display_row, row_pos = 0; row_pos < 32 && row < total_rows; row++, row_pos++) {
			if (chan_pos == 0) {
				fg = pattern_is_playing && row == playing_row ? 3 : 0;
				bg = (current_pattern == marked_pattern && row == marked_row) ? 11 : 2;
				draw_text(numtostr(3, row, buf), 1, 15 + row_pos, fg, bg);
			}

			if (is_in_selection(chan, row)) {
				fg = 3;
				bg = (ROW_IS_HIGHLIGHT(row) ? 9 : 8);
			} else {
				fg = ((status.flags & (CRAYOLA_MODE | CLASSIC_MODE)) == CRAYOLA_MODE)
					? ((note->instrument + 3) % 4 + 3)
					: 6;

				if (highlight_current_row && row == current_row)
					bg = 1;
				else if (ROW_IS_MAJOR(row))
					bg = 14;
				else if (ROW_IS_MINOR(row))
					bg = 15;
				else
					bg = 0;
			}

			/* draw the cursor if on the current row, and:
			drawing the current channel, regardless of position
			OR: when the template is enabled,
			  and the channel fits within the template size,
			  AND shift is not being held down.
			(oh god it's lisp) */
			int cpos;
			if ((row == current_row)
			    && ((current_position > 0 || template_mode == TEMPLATE_OFF
				 || (status.flags & SHIFT_KEY_DOWN))
				? (chan == current_channel)
				: (chan >= current_channel
				   && chan < (current_channel
					      + (clipboard.data ? clipboard.channels : 1))))) {
				// yes! do write the cursor
				cpos = current_position;
				if (cpos == 6 && link_effect_column && !(status.flags & CLASSIC_MODE))
					cpos = 9; // highlight full effect and value
			} else {
				cpos = -1;
			}
			track_view->draw_note(chan_drawpos, 15 + row_pos, note, cpos, fg, bg);

			if (draw_divisions && chan_pos < visible_channels - 1) {
				if (is_in_selection(chan, row))
					bg = 0;
				draw_char(168, chan_drawpos + track_view->width, 15 + row_pos, 2, bg);
			}

			/* next row, same channel */
			note += 64;
		}
		// hmm...?
		for (; row_pos < 32; row++, row_pos++) {
			if (ROW_IS_MAJOR(row))
				bg = 14;
			else if (ROW_IS_MINOR(row))
				bg = 15;
			else
				bg = 0;
			track_view->draw_note(chan_drawpos, 15 + row_pos, blank_note, -1, 6, bg);
			if (draw_divisions && chan_pos < visible_channels - 1) {
				draw_char(168, chan_drawpos + track_view->width, 15 + row_pos, 2, bg);
			}
		}
		if (chan == current_channel) {
			track_view->draw_mask(chan_drawpos, 47, edit_copy_mask, current_position, mc, 2);
		}
		/* blah */
		if (channel_multi[chan - 1]) {
			if (track_view_scheme[chan_pos] == 0) {
				draw_char(172, chan_drawpos + 3, 47, mc, 2);
			} else if (track_view_scheme[chan_pos] < 3) {
				draw_char(172, chan_drawpos + 2, 47, mc, 2);
			} else if (track_view_scheme[chan_pos] == 3) {
				draw_char(172, chan_drawpos + 1, 47, mc, 2);
			} else if (current_position < 2) {
				draw_char(172, chan_drawpos, 47, mc, 2);
			}
		}

		chan_drawpos += track_view->width + !!draw_divisions;
	}

	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* kill all humans */

static void transpose_notes(int amount)
{
	int row, chan;
	song_note_t *pattern, *note;

	status.flags |= SONG_NEEDS_SAVE;
	song_get_pattern(current_pattern, &pattern);

	pated_history_add_grouped(((amount > 0)
				? "Undo transposition up          (Alt-Q)"
				: "Undo transposition down        (Alt-A)"
			),
		selection.first_channel - 1,
		selection.first_row,
		(selection.last_channel - selection.first_channel) + 1,
		(selection.last_row - selection.first_row) + 1);

	if (SELECTION_EXISTS) {
		for (row = selection.first_row; row <= selection.last_row; row++) {
			note = pattern + 64 * row + selection.first_channel - 1;
			for (chan = selection.first_channel; chan <= selection.last_channel; chan++) {
				if (note->note > 0 && note->note < 121)
					note->note = CLAMP(note->note + amount, 1, 120);
				note++;
			}
		}
	} else {
		note = pattern + 64 * current_row + current_channel - 1;
		if (note->note > 0 && note->note < 121)
			note->note = CLAMP(note->note + amount, 1, 120);
	}
	pattern_selection_system_copyout();
}

/* --------------------------------------------------------------------- */

static void copy_note_to_mask(void)
{
	int row = current_row, num_rows;
	song_note_t *pattern, *note;

	num_rows = song_get_pattern(current_pattern, &pattern);
	note = pattern + 64 * current_row + current_channel - 1;

	mask_note = *note;

	if (mask_copy_search_mode != COPY_INST_OFF) {
		while (!note->instrument && row > 0) {
			note -= 64;
			row--;
		}
		if (mask_copy_search_mode == COPY_INST_UP_THEN_DOWN && !note->instrument) {
			note = pattern + 64 * current_row + current_channel - 1; // Reset
			while (!note->instrument && row < num_rows) {
				note += 64;
				row++;
			}
		}
	}
	if (note->instrument) {
		if (song_is_instrument_mode())
			instrument_set(note->instrument);
		else
			sample_set(note->instrument);
	}
}

/* --------------------------------------------------------------------- */

/* pos is either 0 or 1 (0 being the left digit, 1 being the right)
 * return: 1 (move cursor) or 0 (don't)
 * this is highly modplug specific :P */
static int handle_volume(song_note_t * note, struct key_event *k, int pos)
{
	int vol = note->volparam;
	int fx = note->voleffect;
	int vp = panning_mode ? VOLFX_PANNING : VOLFX_VOLUME;
	int q;

	if (k->is_synthetic)
		return 1;
	if (pos == 0) {
		q = kbd_char_to_hex(k);
		if (q >= 0 && q <= 9) {
			vol = q * 10 + vol % 10;
			fx = vp;
		} else if (k->sym.sym == SDLK_a) {
			fx = VOLFX_FINEVOLUP;
			vol %= 10;
		} else if (k->sym.sym == SDLK_b) {
			fx = VOLFX_FINEVOLDOWN;
			vol %= 10;
		} else if (k->sym.sym == SDLK_c) {
			fx = VOLFX_VOLSLIDEUP;
			vol %= 10;
		} else if (k->sym.sym == SDLK_d) {
			fx = VOLFX_VOLSLIDEDOWN;
			vol %= 10;
		} else if (k->sym.sym == SDLK_e) {
			fx = VOLFX_PORTADOWN;
			vol %= 10;
		} else if (k->sym.sym == SDLK_f) {
			fx = VOLFX_PORTAUP;
			vol %= 10;
		} else if (k->sym.sym == SDLK_g) {
			fx = VOLFX_TONEPORTAMENTO;
			vol %= 10;
		} else if (k->sym.sym == SDLK_h) {
			fx = VOLFX_VIBRATODEPTH;
			vol %= 10;
		} else if (status.flags & CLASSIC_MODE) {
			return 0;
		} else if (k->sym.sym == SDLK_DOLLAR) {
			fx = VOLFX_VIBRATOSPEED;
			vol %= 10;
		} else if (k->sym.sym == SDLK_LESS) {
			fx = VOLFX_PANSLIDELEFT;
			vol %= 10;
		} else if (k->sym.sym == SDLK_GREATER) {
			fx = VOLFX_PANSLIDERIGHT;
			vol %= 10;
		} else {
			return 0;
		}
	} else {
		q = kbd_char_to_hex(k);
		if (q >= 0 && q <= 9) {
			vol = (vol / 10) * 10 + q;
			switch (fx) {
			case VOLFX_NONE:
			case VOLFX_VOLUME:
			case VOLFX_PANNING:
				fx = vp;
			}
		} else {
			return 0;
		}
	}

	note->voleffect = fx;
	if (fx == VOLFX_VOLUME || fx == VOLFX_PANNING)
		note->volparam = CLAMP(vol, 0, 64);
	else
		note->volparam = CLAMP(vol, 0, 9);
	return 1;
}

// return zero iff there is no value in the current cell at the current column
static int seek_done(void)
{
	song_note_t *pattern, *note;

	song_get_pattern(current_pattern, &pattern);
	note = pattern + 64 * current_row + current_channel - 1;

	switch (current_position) {
	case 0:
	case 1:
		return note->note != 0;
	case 2:
	case 3:
		return note->instrument != 0;
	case 4:
	case 5:
		return note->voleffect || note->volparam;
	case 6:
	case 7:
	case 8:
		// effect param columns intentionally check effect column instead
		return note->effect != 0;
	}
	return 1; // please stop seeking because something is probably wrong
}

#if 0
static int note_is_empty(song_note_t *p)
{
	if (!p->note && p->voleffect == VOLFX_NONE && !p->effect && !p->param)
		return 1;
	return 0;
}
#endif

// FIXME: why the 'row' param? should it be removed, or should the references to current_row be replaced?
// fwiw, every call to this uses current_row.
// return: zero if there was a template error, nonzero otherwise
static int patedit_record_note(song_note_t *cur_note, int channel, UNUSED int row, int note, int force)
{
	song_note_t *q;
	int i, r = 1, channels;

	status.flags |= SONG_NEEDS_SAVE;
	if (NOTE_IS_NOTE(note)) {
		if (template_mode) {
			q = clipboard.data;
			if (clipboard.channels < 1 || clipboard.rows < 1 || !clipboard.data) {
				dialog_create(DIALOG_OK, "No data in clipboard", NULL, NULL, 0, NULL);
				r = 0;
			} else if (!q->note) {
				create_button(template_error_widgets+0,36,32,6,0,0,0,0,0,
						dialog_yes_NULL,"OK",3);
				dialog_create_custom(20, 23, 40, 12, template_error_widgets, 1,
						0, template_error_draw, NULL);
				r = 0;
			} else {
				i = note - q->note;

				switch (template_mode) {
				case TEMPLATE_OVERWRITE:
					snap_paste(&clipboard, current_channel-1, current_row, i);
					break;
				case TEMPLATE_MIX_PATTERN_PRECEDENCE:
					clipboard_paste_mix_fields(0, i);
					break;
				case TEMPLATE_MIX_CLIPBOARD_PRECEDENCE:
					clipboard_paste_mix_fields(1, i);
					break;
				case TEMPLATE_NOTES_ONLY:
					clipboard_paste_mix_notes(1, i);
					break;
				};
			}
		} else {
			cur_note->note = note;
		}
	} else {
		/* Note cut, etc. -- need to clear all masked fields. This will never cause a template error.
		Also, for one-row templates, replicate control notes across the width of the template. */
		channels = (template_mode && clipboard.data != NULL && clipboard.rows == 1)
			? clipboard.channels
			: 1;

		for (i = 0; i < channels && i + channel <= 64; i++) {
			/* I don't know what this whole 'force' thing is about, but okay */
			if (!force && cur_note->note)
				continue;

			cur_note->note = note;
			if (edit_copy_mask & MASK_INSTRUMENT) {
				cur_note->instrument = 0;
			}
			if (edit_copy_mask & MASK_VOLUME) {
				cur_note->voleffect = 0;
				cur_note->volparam = 0;
			}
			if (edit_copy_mask & MASK_EFFECT) {
				cur_note->effect = 0;
				cur_note->param = 0;
			}
			cur_note++;
		}
	}
	pattern_selection_system_copyout();
	return r;
}

static int pattern_editor_insert_midi(struct key_event *k)
{
	song_note_t *pattern, *cur_note = NULL;
	int n, v = 0, c = 0, pd, speed, tick;

	status.flags |= SONG_NEEDS_SAVE;
	song_get_pattern(current_pattern, &pattern);

	if (midi_start_record && !(song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP))) {
		switch (midi_start_record) {
		case 1: /* pattern loop */
			song_loop_pattern(current_pattern, current_row);
			midi_playback_tracing = playback_tracing;
			playback_tracing = 1;
			break;
		case 2: /* song play */
			song_start_at_pattern(current_pattern, current_row);
			midi_playback_tracing = playback_tracing;
			playback_tracing = 1;
			break;
		};
	}

	speed = song_get_current_speed();
	tick = song_get_current_tick();
	if (k->midi_note == -1) {
		/* nada */
	} else if (k->state == KEY_RELEASE) {
		c = song_keyup(KEYJAZZ_NOINST, KEYJAZZ_NOINST, k->midi_note);

		/* don't record noteoffs for no good reason... */
		if (!((midi_flags & MIDI_RECORD_NOTEOFF)
				&& (song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP))
				&& playback_tracing)) {
			return 0;
		}

		cur_note = pattern + 64 * current_row + (c-1);
		/* never "overwrite" a note off */
		patedit_record_note(cur_note, c, current_row, NOTE_OFF, 0);


	} else {
		if (k->midi_volume > -1) {
			v = k->midi_volume / 2;
		} else {
			v = 0;
		}
		if (!((song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP)) && playback_tracing)) {
			tick = 0;
		}
		n = k->midi_note;
		c = song_keydown(KEYJAZZ_NOINST, KEYJAZZ_NOINST, n, v, current_channel);
		cur_note = pattern + 64 * current_row + (c-1);
		patedit_record_note(cur_note, c, current_row, n, 0);

		if (!template_mode) {
			cur_note->instrument = song_get_current_instrument();

			if (midi_flags & MIDI_RECORD_VELOCITY) {
				cur_note->voleffect = VOLFX_VOLUME;
				cur_note->volparam = v;
			}
			tick %= speed;
			if (!(midi_flags & MIDI_TICK_QUANTIZE) && !cur_note->effect && tick != 0) {
				cur_note->effect = FX_SPECIAL;
				cur_note->param = 0xD0 | MIN(tick, 15);
			}
		}
	}

	if (!(midi_flags & MIDI_PITCHBEND) || midi_pitch_depth == 0 || k->midi_bend == 0) {
		if (k->state == KEY_RELEASE && k->midi_note > -1 && cur_note->instrument > 0) {
			song_keyrecord(cur_note->instrument, cur_note->instrument, cur_note->note, v, c+1,
				cur_note->effect, cur_note->param);
			pattern_selection_system_copyout();
		}
		return -1;
	}

	/* pitch bend */
	for (c = 0; c < 64; c++) {
		if ((channel_multi[c] & 1) && (channel_multi[c] & (~1))) {
			cur_note = pattern + 64 * current_row + c;

			if (cur_note->effect) {
				if (cur_note->effect != FX_PORTAMENTOUP
				    && cur_note->effect != FX_PORTAMENTODOWN) {
					/* don't overwrite old effects */
					continue;
				}
				pd = midi_last_bend_hit[c];
			} else {
				pd = midi_last_bend_hit[c];
				midi_last_bend_hit[c] = k->midi_bend;
			}


			pd = (((k->midi_bend - pd) * midi_pitch_depth
					/ 8192) * speed) / 2;
			if (pd < -0x7F) pd = -0x7F;
			else if (pd > 0x7F) pd = 0x7F;
			if (pd < 0) {
				cur_note->effect = FX_PORTAMENTODOWN; /* Exx */
				cur_note->param = -pd;
			} else if (pd > 0) {
				cur_note->effect = FX_PORTAMENTOUP; /* Fxx */
				cur_note->param = pd;
			}
			if (k->midi_note == -1 || k->state == KEY_RELEASE)
				continue;
			if (cur_note->instrument < 1)
				continue;
			if (cur_note->voleffect == VOLFX_VOLUME)
				v = cur_note->volparam;
			else
				v = -1;
			song_keyrecord(cur_note->instrument, cur_note->instrument, cur_note->note,
				v, c+1, cur_note->effect, cur_note->param);
		}
	}
	pattern_selection_system_copyout();

	return -1;
}


/* return 1 => handled key, 0 => no way */
static int pattern_editor_insert(struct key_event *k)
{
	int ins, smp, j, n, vol;
	song_note_t *pattern, *cur_note;

	song_get_pattern(current_pattern, &pattern);
	/* keydown events are handled here for multichannel */
	if (k->state == KEY_RELEASE && current_position)
		return 0;

	cur_note = pattern + 64 * current_row + current_channel - 1;

	switch (current_position) {
	case 0:                 /* note */
		// FIXME: this is actually quite wrong; instrument numbers should be independent for each
		// channel and take effect when the instrument is played (e.g. with 4/8 or keyjazz input)
		// also, this is fully idiotic
		smp = ins = cur_note->instrument;
		if (song_is_instrument_mode()) {
			smp = KEYJAZZ_NOINST;
		} else {
			ins = KEYJAZZ_NOINST;
		}

		if (k->sym.sym == SDLK_4) {
			if (k->state == KEY_RELEASE)
				return 0;

			if (cur_note->voleffect == VOLFX_VOLUME) {
				vol = cur_note->volparam;
			} else {
				vol = KEYJAZZ_DEFAULTVOL;
			}
			song_keyrecord(smp, ins, cur_note->note,
				vol, current_channel, cur_note->effect, cur_note->param);
			advance_cursor(!(k->mod & KMOD_SHIFT), 1);
			return 1;
		} else if (k->sym.sym == SDLK_8) {
			/* note: Impulse Tracker doesn't skip multichannels when pressing "8"  -delt. */
			if (k->state == KEY_RELEASE)
				return 0;
			song_single_step(current_pattern, current_row);
			advance_cursor(!(k->mod & KMOD_SHIFT), 0);
			return 1;
		}

		if (song_is_instrument_mode()) {
			if (edit_copy_mask & MASK_INSTRUMENT)
				ins = instrument_get_current();
		} else {
			if (edit_copy_mask & MASK_INSTRUMENT)
				smp = sample_get_current();
		}


		if (k->sym.sym == SDLK_SPACE) {
			/* copy mask to note */
			n = mask_note.note;

			vol = ((edit_copy_mask & MASK_VOLUME) && cur_note->voleffect == VOLFX_VOLUME)
				? mask_note.volparam
				: KEYJAZZ_DEFAULTVOL;
		} else {
			n = kbd_get_note(k);
			if (n < 0)
				return 0;

			if ((edit_copy_mask & MASK_VOLUME) && mask_note.voleffect == VOLFX_VOLUME) {
				vol = mask_note.volparam;
			} else if (cur_note->voleffect == VOLFX_VOLUME) {
				vol = cur_note->volparam;
			} else {
				vol = KEYJAZZ_DEFAULTVOL;
			}
		}

		if (k->state == KEY_RELEASE) {
			if (keyjazz_noteoff && NOTE_IS_NOTE(n)) {
				/* coda mode */
				song_keyup(smp, ins, n);
			}
			/* it would be weird to have this enabled and keyjazz_noteoff
			 * disabled, but it's possible, so handle it separately. */
			if (keyjazz_write_noteoff && playback_tracing && NOTE_IS_NOTE(n)) {
				/* go to the next row if a note off would overwrite a note
				 * you (likely) just entered */
				if (cur_note->note) {
					if (++current_row >
						song_get_rows_in_pattern(current_pattern)) {
						return 1;
					}
					cur_note += 64;
					/* give up if the next row has a note too */
					if (cur_note->note) {
						return 1;
					}
				}
				n = NOTE_OFF;
			} else {
				return 1;
			}
		}
		if (k->is_repeat && !keyjazz_repeat)
			return 1;


		int writenote = !(status.flags & CAPS_PRESSED);
		if (writenote && !patedit_record_note(cur_note, current_channel, current_row, n, 1)) {
			// there was a template error, don't advance the cursor and so on
			writenote = 0;
			n = NOTE_NONE;
		}
		/* Be quiet when pasting templates.
		It'd be nice to "play" a template when pasting it (maybe only for ones that are one row high)
		so as to hear the chords being inserted etc., but that's a little complicated to do. */
		if (NOTE_IS_NOTE(n) && !(template_mode && writenote))
			song_keydown(smp, ins, n, vol, current_channel);
		if (!writenote)
			break;

		/* Never copy the instrument etc. from the mask when inserting control notes or when
		erasing a note -- but DO write it when inserting a blank note with the space key. */
		if (!(NOTE_IS_CONTROL(n) || (k->sym.sym != SDLK_SPACE && n == NOTE_NONE)) && !template_mode) {
			if (edit_copy_mask & MASK_INSTRUMENT) {
				if (song_is_instrument_mode())
					cur_note->instrument = instrument_get_current();
				else
					cur_note->instrument = sample_get_current();
			}
			if (edit_copy_mask & MASK_VOLUME) {
				cur_note->voleffect = mask_note.voleffect;
				cur_note->volparam = mask_note.volparam;
			}
			if (edit_copy_mask & MASK_EFFECT) {
				cur_note->effect = mask_note.effect;
				cur_note->param = mask_note.param;
			}
		}

		/* try again, now that we have the effect (this is a dumb way to do this...) */
		if (NOTE_IS_NOTE(n) && !template_mode)
			song_keyrecord(smp, ins, n, vol, current_channel, cur_note->effect, cur_note->param);

		/* copy the note back to the mask */
		mask_note.note = n;
		pattern_selection_system_copyout();

		n = cur_note->note;
		if (NOTE_IS_NOTE(n) && cur_note->voleffect == VOLFX_VOLUME)
			vol = cur_note->volparam;
		if (k->mod & KMOD_SHIFT) {
			// advance horizontally, stopping at channel 64
			// (I have no idea how IT does this, it might wrap)
			if (current_channel < 64) {
				shift_chord_channels++;
				current_channel++;
				pattern_editor_reposition();
			}
		} else {
			advance_cursor(1, 1);
		}
		break;
	case 1:                 /* octave */
		j = kbd_char_to_hex(k);
		if (j < 0 || j > 9) return 0;
		n = cur_note->note;
		if (n > 0 && n <= 120) {
			/* Hehe... this was originally 7 lines :) */
			n = ((n - 1) % 12) + (12 * j) + 1;
			cur_note->note = n;
		}
		advance_cursor(1, 0);
		status.flags |= SONG_NEEDS_SAVE;
		pattern_selection_system_copyout();
		break;
	case 2:                 /* instrument, first digit */
	case 3:                 /* instrument, second digit */
		if (k->sym.sym == SDLK_SPACE) {
			if (song_is_instrument_mode())
				n = instrument_get_current();
			else
				n = sample_get_current();
			if (n && !(status.flags & CLASSIC_MODE))
				current_song->voices[current_channel - 1].last_instrument = n;
			cur_note->instrument = n;
			advance_cursor(1, 0);
			status.flags |= SONG_NEEDS_SAVE;
			break;
		}
		if (kbd_get_note(k) == 0) {
			cur_note->instrument = 0;
			if (song_is_instrument_mode())
				instrument_set(0);
			else
				sample_set(0);
			advance_cursor(1, 0);
			status.flags |= SONG_NEEDS_SAVE;
			break;
		}

		if (current_position == 2) {
			j = kbd_char_to_99(k);
			if (j < 0) return 0;
			n = (j * 10) + (cur_note->instrument % 10);
			current_position++;
		} else {
			j = kbd_char_to_hex(k);
			if (j < 0 || j > 9) return 0;

			n = ((cur_note->instrument / 10) * 10) + j;
			current_position--;
			advance_cursor(1, 0);
		}

		/* this is kind of ugly... */
		if (song_is_instrument_mode()) {
			j = instrument_get_current();
			instrument_set(n);
			if (n != instrument_get_current()) {
				n = j;
			}
			instrument_set(j);
		} else {
			j = sample_get_current();
			sample_set(n);
			if (n != sample_get_current()) {
				n = j;
			}
			sample_set(j);
		}

		if (n && !(status.flags & CLASSIC_MODE))
			current_song->voices[current_channel - 1].last_instrument = n;
		cur_note->instrument = n;
		if (song_is_instrument_mode())
			instrument_set(n);
		else
			sample_set(n);
		status.flags |= SONG_NEEDS_SAVE;
		pattern_selection_system_copyout();
		break;
	case 4:
	case 5:                 /* volume */
		if (k->sym.sym == SDLK_SPACE) {
			cur_note->volparam = mask_note.volparam;
			cur_note->voleffect = mask_note.voleffect;
			advance_cursor(1, 0);
			status.flags |= SONG_NEEDS_SAVE;
			break;
		}
		if (kbd_get_note(k) == 0) {
			cur_note->volparam = mask_note.volparam = 0;
			cur_note->voleffect = mask_note.voleffect = VOLFX_NONE;
			advance_cursor(1, 0);
			status.flags |= SONG_NEEDS_SAVE;
			break;
		}
		if (k->scancode == SDL_SCANCODE_GRAVE) {
			panning_mode = !panning_mode;
			status_text_flash("%s control set", (panning_mode ? "Panning" : "Volume"));
			return 0;
		}
		if (!handle_volume(cur_note, k, current_position - 4))
			return 0;
		mask_note.volparam = cur_note->volparam;
		mask_note.voleffect = cur_note->voleffect;
		if (current_position == 4) {
			current_position++;
		} else {
			current_position = 4;
			advance_cursor(1, 0);
		}
		status.flags |= SONG_NEEDS_SAVE;
		pattern_selection_system_copyout();
		break;
	case 6:                 /* effect */
		if (k->sym.sym == SDLK_SPACE) {
			cur_note->effect = mask_note.effect;
		} else {
			n = kbd_get_effect_number(k);
			if (n < 0)
				return 0;
			cur_note->effect = mask_note.effect = n;
		}
		status.flags |= SONG_NEEDS_SAVE;
		if (link_effect_column)
			current_position++;
		else
			advance_cursor(1, 0);
		pattern_selection_system_copyout();
		break;
	case 7:                 /* param, high nibble */
	case 8:                 /* param, low nibble */
		if (k->sym.sym == SDLK_SPACE) {
			cur_note->param = mask_note.param;
			current_position = link_effect_column ? 6 : 7;
			advance_cursor(1, 0);
			status.flags |= SONG_NEEDS_SAVE;
			pattern_selection_system_copyout();
			break;
		} else if (kbd_get_note(k) == 0) {
			cur_note->param = mask_note.param = 0;
			current_position = link_effect_column ? 6 : 7;
			advance_cursor(1, 0);
			status.flags |= SONG_NEEDS_SAVE;
			pattern_selection_system_copyout();
			break;
		}

		/* FIXME: honey roasted peanuts */

		n = kbd_char_to_hex(k);
		if (n < 0)
			return 0;
		if (current_position == 7) {
			cur_note->param = (n << 4) | (cur_note->param & 0xf);
			current_position++;
		} else /* current_position == 8 */ {
			cur_note->param = (cur_note->param & 0xf0) | n;
			current_position = link_effect_column ? 6 : 7;
			advance_cursor(1, 0);
		}
		status.flags |= SONG_NEEDS_SAVE;
		mask_note.param = cur_note->param;
		pattern_selection_system_copyout();
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

static int pattern_editor_handle_alt_key(struct key_event * k)
{
	int n;
	int total_rows = song_get_rows_in_pattern(current_pattern);

	/* hack to render this useful :) */
	if (k->orig_sym.sym == SDLK_KP_9) {
		k->sym.sym = SDLK_F9;
	} else if (k->orig_sym.sym == SDLK_KP_0) {
		k->sym.sym = SDLK_F10;
	}

	n = numeric_key_event(k, 0);
	if (n > -1 && n <= 9) {
		if (k->state == KEY_RELEASE)
			return 1;
		skip_value = (n == 9) ? 16 : n;
		status_text_flash("Cursor step set to %d", skip_value);
		return 1;
	}

	switch (k->sym.sym) {
	case SDLK_RETURN:
		if (k->state == KEY_PRESS)
			return 1;
		fast_save_update();
		return 1;

	case SDLK_BACKSPACE:
		if (k->state == KEY_PRESS)
			return 1;
		snap_paste(&fast_save, 0, 0, 0);
		return 1;

	case SDLK_b:
		if (k->state == KEY_RELEASE)
			return 1;
		if (!SELECTION_EXISTS) {
			selection.last_channel = current_channel;
			selection.last_row = current_row;
		}
		selection.first_channel = current_channel;
		selection.first_row = current_row;
		normalise_block_selection();
		break;
	case SDLK_e:
		if (k->state == KEY_RELEASE)
			return 1;
		if (!SELECTION_EXISTS) {
			selection.first_channel = current_channel;
			selection.first_row = current_row;
		}
		selection.last_channel = current_channel;
		selection.last_row = current_row;
		normalise_block_selection();
		break;
	case SDLK_d:
		if (k->state == KEY_RELEASE)
			return 1;
		if (status.last_keysym.sym == SDLK_d) {
			if (total_rows - (current_row - 1) > block_double_size)
				block_double_size <<= 1;
		} else {
			// emulate some weird impulse tracker behavior here:
			// with row highlight set to zero, alt-d selects the whole channel
			// if the cursor is at the top, and clears the selection otherwise
			block_double_size = current_song->row_highlight_major ?: (current_row ? 0 : 65536);
			selection.first_channel = selection.last_channel = current_channel;
			selection.first_row = current_row;
		}
		n = block_double_size + current_row - 1;
		selection.last_row = MIN(n, total_rows);
		break;
	case SDLK_l:
		if (k->state == KEY_RELEASE)
			return 1;
		if (status.last_keysym.sym == SDLK_l) {
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
		pattern_selection_system_copyout();
		break;
	case SDLK_r:
		if (k->state == KEY_RELEASE)
			return 1;
		draw_divisions = 1;
		set_view_scheme(0);
		break;
	case SDLK_s:
		if (k->state == KEY_RELEASE)
			return 1;
		selection_set_sample();
		break;
	case SDLK_u:
		if (k->state == KEY_RELEASE)
			return 1;
		if (SELECTION_EXISTS) {
			selection_clear();
		} else if (clipboard.data) {
			clipboard_free();

			clippy_select(NULL, NULL, 0);
			clippy_yank();
		} else {
			dialog_create(DIALOG_OK, "No data in clipboard", NULL, NULL, 0, NULL);
		}
		break;
	case SDLK_c:
		if (k->state == KEY_RELEASE)
			return 1;
		clipboard_copy(0);
		break;
	case SDLK_o:
		if (k->state == KEY_RELEASE)
			return 1;
		if (status.last_keysym.sym == SDLK_o) {
			clipboard_paste_overwrite(0, 1);
		} else {
			clipboard_paste_overwrite(0, 0);
		}
		break;
	case SDLK_p:
		if (k->state == KEY_RELEASE)
			return 1;
		clipboard_paste_insert();
		break;
	case SDLK_m:
		if (k->state == KEY_RELEASE)
			return 1;
		if (status.last_keysym.sym == SDLK_m) {
			clipboard_paste_mix_fields(0, 0);
		} else {
			clipboard_paste_mix_notes(0, 0);
		}
		break;
	case SDLK_f:
		if (k->state == KEY_RELEASE)
			return 1;
		block_length_double();
		break;
	case SDLK_g:
		if (k->state == KEY_RELEASE)
			return 1;
		block_length_halve();
		break;
	case SDLK_n:
		if (k->state == KEY_RELEASE)
			return 1;
		channel_multi[current_channel - 1] ^= 1;
		if (channel_multi[current_channel - 1]) {
			channel_multi_enabled = 1;
		} else {
			channel_multi_enabled = 0;
			for (n = 0; n < 64; n++) {
				if (channel_multi[n]) {
					channel_multi_enabled = 1;
					break;
				}
			}
		}

		if (status.last_keysym.sym == SDLK_n) {
			pattern_editor_display_multichannel();
		}
		break;
	case SDLK_z:
		if (k->state == KEY_RELEASE)
			return 1;
		clipboard_copy(0);
		selection_erase();
		break;
	case SDLK_y:
		if (k->state == KEY_RELEASE)
			return 1;
		selection_swap();
		break;
	case SDLK_v:
		if (k->state == KEY_RELEASE)
			return 1;
		selection_set_volume();
		break;
	case SDLK_w:
		if (k->state == KEY_RELEASE)
			return 1;
		selection_wipe_volume(0);
		break;
	case SDLK_k:
		if (k->state == KEY_RELEASE)
			return 1;
		if (status.last_keysym.sym == SDLK_k) {
			selection_wipe_volume(1);
		} else {
			selection_slide_volume();
		}
		break;
	case SDLK_x:
		if (k->state == KEY_RELEASE)
			return 1;
		if (status.last_keysym.sym == SDLK_x) {
			selection_wipe_effect();
		} else {
			selection_slide_effect();
		}
		break;
	case SDLK_h:
		if (k->state == KEY_RELEASE)
			return 1;
		draw_divisions = !draw_divisions;
		recalculate_visible_area();
		pattern_editor_reposition();
		break;
	case SDLK_q:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_SHIFT)
			transpose_notes(12);
		else
			transpose_notes(1);
		break;
	case SDLK_a:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_SHIFT)
			transpose_notes(-12);
		else
			transpose_notes(-1);
		break;
	case SDLK_i:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_SHIFT)
			template_mode = TEMPLATE_OFF;
		else if (fast_volume_mode)
			fast_volume_amplify();
		else
			template_mode = (template_mode + 1) % TEMPLATE_MODE_MAX; /* cycle */
		break;
	case SDLK_j:
		if (k->state == KEY_RELEASE)
			return 1;
		if (fast_volume_mode)
			fast_volume_attenuate();
		else
			volume_amplify();
		break;
	case SDLK_t:
		if (k->state == KEY_RELEASE)
			return 1;
		n = current_channel - top_display_channel;
		track_view_scheme[n] = ((track_view_scheme[n] + 1) % NUM_TRACK_VIEWS);
		recalculate_visible_area();
		pattern_editor_reposition();
		break;
	case SDLK_UP:
		if (k->state == KEY_RELEASE)
			return 1;
		if (top_display_row > 0) {
			top_display_row--;
			if (current_row > top_display_row + 31)
				current_row = top_display_row + 31;
			return -1;
		}
		return 1;
	case SDLK_DOWN:
		if (k->state == KEY_RELEASE)
			return 1;
		if (top_display_row + 31 < total_rows) {
			top_display_row++;
			if (current_row < top_display_row)
				current_row = top_display_row;
			return -1;
		}
		return 1;
	case SDLK_LEFT:
		if (k->state == KEY_RELEASE)
			return 1;
		current_channel--;
		return -1;
	case SDLK_RIGHT:
		if (k->state == KEY_RELEASE)
			return 1;
		current_channel++;
		return -1;
	case SDLK_INSERT:
		if (k->state == KEY_RELEASE)
			return 1;
		pated_save("Remove inserted row(s)    (Alt-Insert)");
		pattern_insert_rows(current_row, 1, 1, 64);
		break;
	case SDLK_DELETE:
		if (k->state == KEY_RELEASE)
			return 1;
		pated_save("Replace deleted row(s)    (Alt-Delete)");
		pattern_delete_rows(current_row, 1, 1, 64);
		break;
	case SDLK_F9:
		if (k->state == KEY_RELEASE)
			return 1;
		song_toggle_channel_mute(current_channel - 1);
		break;
	case SDLK_F10:
		if (k->state == KEY_RELEASE)
			return 1;
		song_handle_channel_solo(current_channel - 1);
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
static int pattern_editor_handle_ctrl_key(struct key_event * k)
{
	int n;
	int total_rows = song_get_rows_in_pattern(current_pattern);

	n = numeric_key_event(k, 0);
	if (n > -1) {
		if (n < 0 || n >= NUM_TRACK_VIEWS)
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_SHIFT) {
			set_view_scheme(n);
		} else {
			track_view_scheme[current_channel - top_display_channel] = n;
			recalculate_visible_area();
		}
		pattern_editor_reposition();
		status.flags |= NEED_UPDATE;
		return 1;
	}


	switch (k->sym.sym) {
	case SDLK_LEFT:
		if (k->state == KEY_RELEASE)
			return 1;
		if (current_channel > top_display_channel)
			current_channel--;
		return -1;
	case SDLK_RIGHT:
		if (k->state == KEY_RELEASE)
			return 1;
		if (current_channel < top_display_channel + visible_channels - 1)
			current_channel++;
		return -1;
	case SDLK_F6:
		if (k->state == KEY_RELEASE)
			return 1;
		song_loop_pattern(current_pattern, current_row);
		return 1;
	case SDLK_F7:
		if (k->state == KEY_RELEASE)
			return 1;
		set_playback_mark();
		return -1;
	case SDLK_UP:
		if (k->state == KEY_RELEASE)
			return 1;
		set_previous_instrument();
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_DOWN:
		if (k->state == KEY_RELEASE)
			return 1;
		set_next_instrument();
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_PAGEUP:
		if (k->state == KEY_RELEASE)
			return 1;
		current_row = 0;
		return -1;
	case SDLK_PAGEDOWN:
		if (k->state == KEY_RELEASE)
			return 1;
		current_row = total_rows;
		return -1;
	case SDLK_HOME:
		if (k->state == KEY_RELEASE)
			return 1;
		current_row--;
		return -1;
	case SDLK_END:
		if (k->state == KEY_RELEASE)
			return 1;
		current_row++;
		return -1;
	case SDLK_INSERT:
		if (k->state == KEY_RELEASE)
			return 1;
		selection_roll(ROLL_DOWN);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_DELETE:
		if (k->state == KEY_RELEASE)
			return 1;
		selection_roll(ROLL_UP);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_MINUS:
		if (k->state == KEY_RELEASE)
			return 1;
		if (song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP) && playback_tracing)
			return 1;
		prev_order_pattern();
		return 1;
	case SDLK_PLUS:
		if (k->state == KEY_RELEASE)
			return 1;
		if (song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP) && playback_tracing)
			return 1;
		next_order_pattern();
		return 1;
	case SDLK_c:
		if (k->state == KEY_RELEASE)
			return 1;
		centralise_cursor = !centralise_cursor;
		status_text_flash("Centralise cursor %s", (centralise_cursor ? "enabled" : "disabled"));
		return -1;
	case SDLK_h:
		if (k->state == KEY_RELEASE)
			return 1;
		highlight_current_row = !highlight_current_row;
		status_text_flash("Row hilight %s", (highlight_current_row ? "enabled" : "disabled"));
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_j:
		if (k->state == KEY_RELEASE)
			return 1;
		fast_volume_toggle();
		return 1;
	case SDLK_u:
		if (k->state == KEY_RELEASE)
			return 1;
		if (fast_volume_mode)
			selection_vary(1, 100-fast_volume_percent, FX_CHANNELVOLUME);
		else
			vary_command(FX_CHANNELVOLUME);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_y:
		if (k->state == KEY_RELEASE)
			return 1;
		if (fast_volume_mode)
			selection_vary(1, 100-fast_volume_percent, FX_PANBRELLO);
		else
			vary_command(FX_PANBRELLO);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_k:
		if (k->state == KEY_RELEASE)
			return 1;
		if (fast_volume_mode)
			selection_vary(1, 100-fast_volume_percent, current_effect());
		else
			vary_command(current_effect());
		status.flags |= NEED_UPDATE;
		return 1;

	case SDLK_b:
		if (k->mod & KMOD_SHIFT)
			return 0;
		/* fall through */
	case SDLK_o:
		if (k->state == KEY_RELEASE)
			return 1;
		song_pattern_to_sample(current_pattern, !!(k->mod & KMOD_SHIFT), !!(k->sym.sym == SDLK_b));
		return 1;

	case SDLK_v:
		if (k->state == KEY_RELEASE)
			return 1;
		show_default_volumes = !show_default_volumes;
		status_text_flash("Default volumes %s", (show_default_volumes ? "enabled" : "disabled"));
		return 1;
	case SDLK_x:
	case SDLK_z:
		if (k->state == KEY_RELEASE)
			return 1;
		midi_start_record++;
		if (midi_start_record > 2) midi_start_record = 0;
		switch (midi_start_record) {
		case 0:
			status_text_flash("No MIDI Trigger");
			break;
		case 1:
			status_text_flash("Pattern MIDI Trigger");
			break;
		case 2:
			status_text_flash("Song MIDI Trigger");
			break;
		};
		return 1;
	case SDLK_BACKSPACE:
		if (k->state == KEY_RELEASE)
			return 1;
		pattern_editor_display_history();
		return 1;
	default:
		return 0;
	}

	return 0;
}

static int mute_toggle_hack[64]; /* mrsbrisby: please explain this one, i don't get why it's necessary... */
static int pattern_editor_handle_key_default(struct key_event * k)
{
	if (k->is_synthetic)
		return 1;
	/* bleah */
	if (k->sym.sym == SDLK_LESS || k->sym.sym == SDLK_COLON || k->sym.sym == SDLK_SEMICOLON) {
		if (k->state == KEY_RELEASE)
			return 0;
		if ((status.flags & CLASSIC_MODE) || current_position != 4) {
			set_previous_instrument();
			status.flags |= NEED_UPDATE;
			return 1;
		}
	} else if (k->sym.sym == SDLK_GREATER || k->sym.sym == SDLK_QUOTE || k->sym.sym == SDLK_QUOTEDBL) {
		if (k->state == KEY_RELEASE)
			return 0;
		if ((status.flags & CLASSIC_MODE) || current_position != 4) {
			set_next_instrument();
			status.flags |= NEED_UPDATE;
			return 1;
		}
	} else if (k->sym.sym == SDLK_COMMA) {
		if (k->state == KEY_RELEASE)
			return 0;
		switch (current_position) {
		case 2: case 3:
			edit_copy_mask ^= MASK_INSTRUMENT;
			break;
		case 4: case 5:
			edit_copy_mask ^= MASK_VOLUME;
			break;
		case 6: case 7: case 8:
			edit_copy_mask ^= MASK_EFFECT;
			break;
		}
		status.flags |= NEED_UPDATE;
		return 1;
	}
	if (song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP) && playback_tracing && k->is_repeat)
		return 0;

	if (!pattern_editor_insert(k))
		return 0;
	return -1;
}
static int pattern_editor_handle_key(struct key_event * k)
{
	int n, nx, v;
	int total_rows = song_get_rows_in_pattern(current_pattern);
	const struct track_view *track_view;
	int np, nr, nc;
	unsigned int basex;

	if (k->mouse != MOUSE_NONE) {
		if (k->state == KEY_RELEASE) {
			/* mouseup */
			memset(mute_toggle_hack, 0, sizeof(mute_toggle_hack));
		}

		if ((k->mouse == MOUSE_CLICK || k->mouse == MOUSE_DBLCLICK) && k->state == KEY_RELEASE) {
			shift_selection_end();
		}

		if (k->y < 13 && !shift_selection.in_progress) return 0;

		if (k->y >= 15 && k->mouse != MOUSE_CLICK && k->mouse != MOUSE_DBLCLICK) {
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mouse == MOUSE_SCROLL_UP) {
				if (top_display_row > 0) {
					top_display_row = MAX(top_display_row - MOUSE_SCROLL_LINES, 0);
					if (current_row > top_display_row + 31)
						current_row = top_display_row + 31;
					if (current_row < 0)
						current_row = 0;
					return -1;
				}
			} else if (k->mouse == MOUSE_SCROLL_DOWN) {
				if (top_display_row + 31 < total_rows) {
					top_display_row = MIN(top_display_row + MOUSE_SCROLL_LINES, total_rows);
					if (current_row < top_display_row)
						current_row = top_display_row;
					return -1;
				}
			}
			return 1;
		}

		if (k->mouse != MOUSE_CLICK && k->mouse != MOUSE_DBLCLICK)
			return 1;

		basex = 5;
		if (current_row < 0) current_row = 0;
		if (current_row >= total_rows) current_row = total_rows;
		np = current_position; nc = current_channel; nr = current_row;
		for (n = top_display_channel, nx = 0; nx <= visible_channels; n++, nx++) {
			track_view = track_views+track_view_scheme[nx];
			if (((n == top_display_channel && shift_selection.in_progress)
			     || k->x >= basex)
			    && ((n == visible_channels && shift_selection.in_progress)
				|| k->x < basex + track_view->width)) {
				if (!shift_selection.in_progress && (k->y == 14 || k->y == 13)) {
					if (k->state == KEY_PRESS) {
						if (!mute_toggle_hack[n-1]) {
							song_toggle_channel_mute(n-1);
							status.flags |= NEED_UPDATE;
							mute_toggle_hack[n-1]=1;
						}
					}
					break;
				}

				nc = n;
				nr = (k->y - 15) + top_display_row;

				if (k->y < 15 && top_display_row > 0) {
					top_display_row--;
				}


				if (shift_selection.in_progress) break;

				v = k->x - basex;
				switch (track_view_scheme[nx]) {
				case 0: /* 5 channel view */
					switch (v) {
					case 0: np = 0; break;
					case 2: np = 1; break;
					case 4: np = 2; break;
					case 5: np = 3; break;
					case 7: np = 4; break;
					case 8: np = 5; break;
					case 10: np = 6; break;
					case 11: np = 7; break;
					case 12: np = 8; break;
					};
					break;
				case 1: /* 6/7 channels */
					switch (v) {
					case 0: np = 0; break;
					case 2: np = 1; break;
					case 3: np = 2; break;
					case 4: np = 3; break;
					case 5: np = 4; break;
					case 6: np = 5; break;
					case 7: np = 6; break;
					case 8: np = 7; break;
					case 9: np = 8; break;
					};
					break;
				case 2: /* 9/10 channels */
					switch (v) {
					case 0: np = 0; break;
					case 2: np = 1; break;
					case 3: np = 2 + k->hx; break;
					case 4: np = 4 + k->hx; break;
					case 5: np = 6; break;
					case 6: np = 7 + k->hx; break;
					};
					break;
				case 3: /* 18/24 channels */
					switch (v) {
					case 0: np = 0; break;
					case 1: np = 1; break;
					case 2: np = 2 + k->hx; break;
					case 3: np = 4 + k->hx; break;
					case 4: np = 6; break;
					case 5: np = 7 + k->hx; break;
					};
					break;
				case 4: /* now things get weird: 24/36 channels */
				case 5: /* now things get weird: 36/64 channels */
				case 6: /* no point doing anything here; reset */
					np = 0;
					break;
				};
				break;
			}
			basex += track_view->width;
			if (draw_divisions) basex++;
		}
		if (np == current_position && nc == current_channel && nr == current_row) {
			return 1;
		}

		if (nr >= total_rows) nr = total_rows;
		if (nr < 0) nr = 0;
		current_position = np; current_channel = nc; current_row = nr;

		if (k->state == KEY_PRESS && k->sy > 14) {
			if (!shift_selection.in_progress) {
				shift_selection_begin();
			} else {
				shift_selection_update();
			}
		}

		return -1;
	}


	if (k->midi_note > -1 || k->midi_bend != 0) {
		return pattern_editor_insert_midi(k);
	}

	switch (k->sym.sym) {
	case SDLK_UP:
		if (k->state == KEY_RELEASE)
			return 0;
		if (skip_value) {
			if (current_row - skip_value >= 0)
				current_row -= skip_value;
		} else {
			current_row--;
		}
		return -1;
	case SDLK_DOWN:
		if (k->state == KEY_RELEASE)
			return 0;
		if (skip_value) {
			if (current_row + skip_value <= total_rows)
				current_row += skip_value;
		} else {
			current_row++;
		}
		return -1;
	case SDLK_LEFT:
		if (k->state == KEY_RELEASE)
			return 0;
		if (k->mod & KMOD_SHIFT) {
			current_channel--;
		} else if (link_effect_column && current_position == 0 && current_channel > 1) {
			current_channel--;
			current_position = current_effect() ? 8 : 6;
		} else {
			current_position--;
		}
		return -1;
	case SDLK_RIGHT:
		if (k->state == KEY_RELEASE)
			return 0;
		if (k->mod & KMOD_SHIFT) {
			current_channel++;
		} else if (link_effect_column && current_position == 6 && current_channel < 64) {
			current_position = current_effect() ? 7 : 10;
		} else {
			current_position++;
		}
		return -1;
	case SDLK_TAB:
		if (k->state == KEY_RELEASE)
			return 0;
		if ((k->mod & KMOD_SHIFT) == 0)
			current_channel++;
		else if (current_position == 0)
			current_channel--;
		current_position = 0;

		/* hack to keep shift-tab from changing the selection */
		k->mod &= ~KMOD_SHIFT;
		shift_selection_end();

		return -1;
	case SDLK_PAGEUP:
		if (k->state == KEY_RELEASE)
			return 0;
		{
			int rh = current_song->row_highlight_major ?: 16;
			if (current_row == total_rows)
				current_row -= (current_row % rh) ?: rh;
			else
				current_row -= rh;
		}
		return -1;
	case SDLK_PAGEDOWN:
		if (k->state == KEY_RELEASE)
			return 0;
		current_row += current_song->row_highlight_major ?: 16;
		return -1;
	case SDLK_HOME:
		if (k->state == KEY_RELEASE)
			return 0;
		if (current_position == 0) {
			if (invert_home_end ? (current_row != 0) : (current_channel == 1)) {
				current_row = 0;
			} else {
				current_channel = 1;
			}
		} else {
			current_position = 0;
		}
		return -1;
	case SDLK_END:
		if (k->state == KEY_RELEASE)
			return 0;
		n = song_find_last_channel();
		if (current_position == 8) {
			if (invert_home_end ? (current_row != total_rows) : (current_channel == n)) {
				current_row = total_rows;
			} else {
				current_channel = n;
			}
		} else {
			current_position = 8;
		}
		return -1;
	case SDLK_INSERT:
		if (k->state == KEY_RELEASE)
			return 0;
		if (template_mode && clipboard.rows == 1) {
			n = clipboard.channels;
			if (n + current_channel > 64) {
				n = 64 - current_channel;
			}
			pattern_insert_rows(current_row, 1, current_channel, n);
		} else {
			pattern_insert_rows(current_row, 1, current_channel, 1);
		}
		break;
	case SDLK_DELETE:
		if (k->state == KEY_RELEASE)
			return 0;
		if (template_mode && clipboard.rows == 1) {
			n = clipboard.channels;
			if (n + current_channel > 64) {
				n = 64 - current_channel;
			}
			pattern_delete_rows(current_row, 1, current_channel, n);
		} else {
			pattern_delete_rows(current_row, 1, current_channel, 1);
		}
		break;
	case SDLK_MINUS:
		if (k->state == KEY_RELEASE)
			return 0;

		if (playback_tracing) {
			switch (song_get_mode()) {
			case MODE_PATTERN_LOOP:
				return 1;
			case MODE_PLAYING:
				song_set_current_order(song_get_current_order() - 1);
				return 1;
			default:
				break;
			};
		}

		if (k->mod & KMOD_SHIFT)
			set_current_pattern(current_pattern - 4);
		else
			set_current_pattern(current_pattern - 1);
		return 1;
	case SDLK_PLUS:
		if (k->state == KEY_RELEASE)
			return 0;

		if (playback_tracing) {
			switch (song_get_mode()) {
			case MODE_PATTERN_LOOP:
				return 1;
			case MODE_PLAYING:
				song_set_current_order(song_get_current_order() + 1);
				return 1;
			default:
				break;
			};
		}

		if ((k->mod & KMOD_SHIFT) && k->orig_sym.sym == SDLK_KP_PLUS)
			set_current_pattern(current_pattern + 4);
		else
			set_current_pattern(current_pattern + 1);
		return 1;
	case SDLK_BACKSPACE:
		if (k->state == KEY_RELEASE)
			return 0;
		current_channel = multichannel_get_previous (current_channel);
		if (skip_value)
			current_row -= skip_value;
		else
			current_row--;
		return -1;
	case SDLK_RETURN:
		if (k->state == KEY_RELEASE)
			return 0;
		copy_note_to_mask();
		if (template_mode != TEMPLATE_NOTES_ONLY)
			template_mode = TEMPLATE_OFF;
		return 1;
	case SDLK_l:
		if (k->mod & KMOD_SHIFT) {
			if (status.flags & CLASSIC_MODE) return 0;
			if (k->state == KEY_RELEASE)
				return 1;
			clipboard_copy(1);
			break;
		}
		return pattern_editor_handle_key_default(k);
	case SDLK_a:
		if (k->mod & KMOD_SHIFT && !(status.flags & CLASSIC_MODE)) {
			if (k->state == KEY_RELEASE) {
				return 0;
			}
			if (current_row == 0) {
				return 1;
			}
			do {
				current_row--;
			} while (!seek_done() && current_row != 0);
			return -1;
		}
		return pattern_editor_handle_key_default(k);
	case SDLK_f:
		if (k->mod & KMOD_SHIFT && !(status.flags & CLASSIC_MODE)) {
			if (k->state == KEY_RELEASE) {
				return 0;
			}
			if (current_row == total_rows) {
				return 1;
			}
			do {
				current_row++;
			} while (!seek_done() && current_row != total_rows);
			return -1;
		}
		return pattern_editor_handle_key_default(k);

	case SDLK_LSHIFT:
	case SDLK_RSHIFT:
		if (k->state == KEY_PRESS) {
			if (shift_selection.in_progress)
				shift_selection_end();
		} else if (shift_chord_channels) {
			current_channel -= shift_chord_channels;
			while (current_channel < 1)
				current_channel += 64;
			advance_cursor(1, 1);
			shift_chord_channels = 0;
		}
		return 1;

	default:
		return pattern_editor_handle_key_default(k);
	}

	status.flags |= NEED_UPDATE;
	return 1;
}

/* --------------------------------------------------------------------- */
/* this function name's a bit confusing, but this is just what gets
 * called from the main key handler.
 * pattern_editor_handle_*_key above do the actual work. */

static int pattern_editor_handle_key_cb(struct key_event * k)
{
	int ret;
	int total_rows = song_get_rows_in_pattern(current_pattern);

	if (k->mod & KMOD_SHIFT) {
		switch (k->sym.sym) {
		case SDLK_LEFT:
		case SDLK_RIGHT:
		case SDLK_UP:
		case SDLK_DOWN:
		case SDLK_HOME:
		case SDLK_END:
		case SDLK_PAGEUP:
		case SDLK_PAGEDOWN:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!shift_selection.in_progress)
				shift_selection_begin();
		default:
			break;
		};
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
		} else if (current_pattern == playing_pattern) {
			status.flags |= NEED_UPDATE;
		}
	}
}

static void pated_song_changed(void)
{
	pated_history_clear();

	// reset ctrl-f7
	marked_pattern = -1;
	marked_row = 0;
}

/* --------------------------------------------------------------------- */

static int _fix_f7(struct key_event *k)
{
	if (k->sym.sym == SDLK_F7) {
		if (!NO_MODIFIER(k->mod)) return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		play_song_from_mark();
		return 1;
	}
	return 0;
}

void pattern_editor_load_page(struct page *page)
{
	int i;
	for (i = 0; i < 10; i++) {
		memset(&undo_history[i],0,sizeof(struct pattern_snap));
		undo_history[i].snap_op = "Empty";
		undo_history[i].snap_op_allocated = 0;
	}
	page->title = "Pattern Editor (F2)";
	page->playback_update = pattern_editor_playback_update;
	page->song_changed_cb = pated_song_changed;
	page->pre_handle_key = _fix_f7;
	page->total_widgets = 1;
	page->clipboard_paste = pattern_selection_system_paste;
	page->widgets = widgets_pattern;
	page->help_index = HELP_PATTERN_EDITOR;

	create_other(widgets_pattern + 0, 0, pattern_editor_handle_key_cb, pattern_editor_redraw);
}

