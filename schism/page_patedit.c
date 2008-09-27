/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
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

#include <ctype.h>

#include "it.h"
#include "page.h"
#include "song.h"
#include "pattern-view.h"
#include "config-parser.h"
#include "midi.h"

#include "sdlmain.h"
#include "clippy.h"

extern void (*shift_release)(void);

/* --------------------------------------------------------------------------------------------------------- */

#define ROW_IS_MAJOR(r) (row_highlight_major != 0 && (r) % row_highlight_major == 0)
#define ROW_IS_MINOR(r) (row_highlight_minor != 0 && (r) % row_highlight_minor == 0)
#define ROW_IS_HIGHLIGHT(r) (ROW_IS_MINOR(r) || ROW_IS_MAJOR(r))

/* this is actually used by pattern-view.c */
int show_default_volumes = 0;

/* --------------------------------------------------------------------- */
/* The (way too many) static variables */

int midi_start_record = 0;

enum TemplateMode {
    NoTemplateMode=0,
    TemplateOverwrite,
    TemplateMixPatternPrecedence,
    TemplateMixClipboardPrecedence,
    TemplateNotesOnly
};

static enum TemplateMode template_mode = NoTemplateMode;

/* only one widget, but MAN is it complicated :) */
static struct widget widgets_pattern[1];

/* pattern display position */
static int top_display_channel = 1;		/* one-based */
static int top_display_row = 0;		/* zero-based */

/* these three tell where the cursor is in the pattern */
static int current_channel = 1, current_position = 0;
static int current_row = 0;

/* when holding shift, this "remembers" the original channel and allows
us to jump back to it when letting go
*/
static int channel_snap_back = -1;

/* if pressing shift WHILE TRACING the song is paused until we release it */
static int tracing_was_playing = 0;

/* this is, of course, what the current pattern is */
static int current_pattern = 0;

static int skip_value = 1;		/* aka cursor step */

static int link_effect_column = 0;
static int draw_divisions = 0;		/* = vertical lines between channels */

static int centralise_cursor = 0;
static int highlight_current_row = 0;
int playback_tracing = 0;	/* scroll lock */
int midi_playback_tracing = 0;

static int panning_mode = 0;		/* for the volume column */
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
#define MASK_NOTE	1 /* immutable */
#define MASK_INSTRUMENT	2
#define MASK_VOLUME	4
#define MASK_EFFECT	8
#define MASK_EFFECTVALUE	16
static int edit_copy_mask = MASK_NOTE | MASK_INSTRUMENT | MASK_VOLUME;
static const int edit_pos_to_copy_mask[9] = { 0,0,1,1,2,2,3,4,4 };


/* and the mask note. note that the instrument field actually isn't used */
static song_note mask_note = { 61, 0, 0, 0, 0, 0 };	/* C-5 */

/* playback mark (ctrl-f7) */
static int marked_pattern = -1, marked_row;

/* volume stuff (alt-i, alt-j, ctrl-j) */
static int volume_percent = 100;
static int vary_depth = 10;
static int fast_volume_percent = 67;
static int fast_volume_mode = 0;	/* toggled with ctrl-j */

/* --------------------------------------------------------------------- */
/* undo and clipboard handling */
struct pattern_snap {
        song_note *data;
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
static int *channel_multi_base = 0; /* <--- uh, wtf is this for? */
static int channel_multi[64];
static int channel_quick[64];
static int channel_keyhack[64];
static int visible_channels, visible_width;

static void recalculate_visible_area(void);
static void set_view_scheme(int scheme);
static void pattern_editor_reposition(void);

/* --------------------------------------------------------------------------------------------------------- */
/* options dialog */

static struct widget options_widgets[8];
static const int options_link_split[] = { 5, 6, -1 };
static int options_selected_widget = 0;

static void options_close(void *data)
{
	int old_size, new_size;
	
	options_selected_widget = ((struct dialog *) data)->selected_widget;
	
	skip_value = options_widgets[1].d.thumbbar.value;
	row_highlight_minor = options_widgets[2].d.thumbbar.value;
	row_highlight_major = options_widgets[3].d.thumbbar.value;
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
		create_thumbbar(options_widgets + 4, 40, 35, 22, 3, 5, 5, NULL, 32, 200);
		create_togglebutton(options_widgets + 5, 40, 38, 8, 4, 7, 6, 6, 6,
				    NULL, "Link", 3, options_link_split);
		create_togglebutton(options_widgets + 6, 52, 38, 9, 4, 7, 5, 5, 5,
				    NULL, "Split", 3, options_link_split);
		create_button(options_widgets + 7, 35, 41, 8, 5, 0, 7, 7, 7, dialog_yes_NULL, "Done", 3);
	}

	options_widgets[0].d.thumbbar.value = kbd_get_current_octave();
	options_widgets[1].d.thumbbar.value = skip_value;
	options_widgets[2].d.thumbbar.value = row_highlight_minor;
	options_widgets[3].d.thumbbar.value = row_highlight_major;
	options_widgets[4].d.thumbbar.value = song_get_pattern(current_pattern, NULL);
	togglebutton_set(options_widgets, link_effect_column ? 5 : 6, 0);
	
	dialog = dialog_create_custom(10, 18, 60, 26, options_widgets, 8, options_selected_widget,
				      options_draw_const, NULL);
	dialog->action_yes = options_close;
	dialog->action_cancel = options_close;
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
		if (song_get_pattern(i, 0) != nl) {
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
	length_edit_widgets[0].d.thumbbar.value = song_get_pattern(current_pattern, 0);
	create_thumbbar(length_edit_widgets + 1, 34, 27, 26, 0, 2, 2, NULL, 0, 199);
	create_thumbbar(length_edit_widgets + 2, 34, 28, 26, 1, 3, 3, NULL, 0, 199);
	length_edit_widgets[1].d.thumbbar.value
		= length_edit_widgets[2].d.thumbbar.value
		= current_pattern;

	create_button(length_edit_widgets + 3,
			35,31,8,
			2,
			3,
			3,
			3,
			0,
			dialog_yes_NULL, "OK", 4);

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
	int i, cnt = 0;
	for (i = 0; i < 64; i++) {
		channel_multi[i] = multichannel_widgets[i].d.toggle.state
			? 1 : 0;
		if (channel_multi[i]) cnt++;
	}
	if (cnt) {
		channel_multi_base = channel_multi;
	} else {
		channel_multi_base = NULL;
	}
}
static int multichannel_handle_key(struct key_event *k)
{
	if (!k->state) return 0;
	if (NO_MODIFIER(k->mod) && k->sym == SDLK_n) {
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
			22 + (i % 16),	/* Y */
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
	draw_text("Multichannel Selection", 28, 19, 3, 2);
}
static void mp_advance_channel(void)
{
	change_focus_to(ACTIVE_WIDGET.next.tab);
}

static void pattern_editor_display_multichannel(void)
{
	struct dialog *dialog;
	int i;

	for (i = 0; i < 64; i++) {
		create_toggle(multichannel_widgets+i,
			20 + ((i / 16) * 16), /* X */
			22 + (i % 16),	/* Y */

			((i % 16) == 0) ? 64 : (i-1),
			((i % 16) == 15) ? 64 : (i+1),
			(i < 16) ? (i+48) : (i-16),
			((i + 16) % 64),
			i+1,

			mp_advance_channel);
		multichannel_widgets[i].d.toggle.state = channel_multi[i] & 1;
	}
	create_button(multichannel_widgets+64,
			35,40,8,
			15,
			0,
			63,
			15,
			0,
			dialog_yes_NULL, "OK", 4);

	dialog = dialog_create_custom(7, 18, 66, 25, multichannel_widgets, 65, 0,
				      multichannel_draw_const, NULL);
	dialog->action_yes = multichannel_close;
	dialog->action_cancel = multichannel_close;
	dialog->handle_key = multichannel_handle_key;
}



/* This probably doesn't belong here, but whatever */

static int multichannel_get_next (int cur_channel)
{
	int i;

        /* stub * / return cur_channel; */

	cur_channel--; /* make it zero-based. oh look, it's a hammer. */
        i = cur_channel;
	
	if (channel_multi[cur_channel] & 1) {
		/* we're in a multichan-enabled channel, so look for the next one */
		do {
			i = (i + 1) & 63; /* no? next channel, and loop back to zero if we hit 64 */
			if (channel_multi[i] & 1) /* is this a multi-channel? */
				break; /* it is! */
		} while (i != cur_channel);

		/* at this point we've either broken the loop because the channel i is multichan,
		   or the condition failed because we're back where we started */
	}
        /* status_text_flash ("Newly selected channel is %d", (int) i + 1); */
	return i + 1; /* make it one-based again */
}

static int multichannel_get_previous (int cur_channel)
{
        int i;

        cur_channel--; /* once again, .... */
        i = cur_channel;

        if (channel_multi [cur_channel] & 1)
	{
                do
		{
                        i = i ? (i - 1): 63; /* loop backwards this time */
                        if (channel_multi [i] & 1)
                        	break;
                        } while (i != cur_channel);
	}
        return i + 1;
}


/* --------------------------------------------------------------------------------------------------------- */
static void copyin_addnote(song_note *note, int *copyin_x, int *copyin_y)
{
	song_note *pattern, *p_note;
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
	song_note n;
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
		memset(&n, 0, sizeof(song_note));
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
					n.volume = scantmp;
				else
					n.volume = 0;
				switch (*str) {
				case 'v':n.volume_effect=VOL_EFFECT_VOLUME;break;
				case 'p':n.volume_effect=VOL_EFFECT_PANNING;break;
				case 'c':n.volume_effect=VOL_EFFECT_VOLSLIDEUP;break;
				case 'd':n.volume_effect=VOL_EFFECT_VOLSLIDEDOWN;break;
				case 'a':n.volume_effect=VOL_EFFECT_FINEVOLUP;break;
				case 'b':n.volume_effect=VOL_EFFECT_FINEVOLDOWN;break;
				case 'u':n.volume_effect=VOL_EFFECT_VIBRATOSPEED;break;
				case 'h':n.volume_effect=VOL_EFFECT_VIBRATO;break;
				case 'l':n.volume_effect=VOL_EFFECT_PANSLIDELEFT;break;
				case 'r':n.volume_effect=VOL_EFFECT_PANSLIDERIGHT;break;
				case 'g':n.volume_effect=VOL_EFFECT_TONEPORTAMENTO;break;
				case 'f':n.volume_effect=VOL_EFFECT_PORTAUP;break;
				case 'e':n.volume_effect=VOL_EFFECT_PORTADOWN;break;
				default: n.volume_effect=VOL_EFFECT_NONE;n.volume=0;break;
				};
			} else {
				n.effect = fx_map(*str);
				if (sscanf(str+1, "%02X", &scantmp) == 1)
					n.parameter = scantmp;
				else
					n.parameter = 0;
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
	song_note *pattern, *cur_note;


	if (!(SELECTION_EXISTS)) {
		if (clippy_owner(CLIPPY_SELECT) == widgets_pattern) {
			/* unselect if we don't have a selection */
			clippy_select(0,0,0);
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
				str[len] = str[len+1] = str[len+2] = '.';
			} else if (cur_note->note == NOTE_CUT) {
				str[len] = str[len+1] = str[len+2] = '^';
			} else if (cur_note->note == NOTE_OFF) {
				str[len] = str[len+1] = str[len+2] = '=';
			} else if (cur_note->note == NOTE_FADE) {
				/* ModPlug won't handle this one, but it'll
				just drop it...
				*/
				str[len] = str[len+1] = str[len+2] = '~';
			} else {
				get_note_string(cur_note->note, str+len);
			}
			len += 3;
			if (cur_note->instrument)
				sprintf(str+len, "%02d", cur_note->instrument);
			else
				str[len] = str[len+1] = '.';
			sprintf(str+len+3, "%02d", cur_note->volume);
			switch (cur_note->volume_effect) {
			case VOL_EFFECT_VOLUME:		str[len+2] = 'v';break;
			case VOL_EFFECT_PANNING:	str[len+2] = 'p';break;
			case VOL_EFFECT_VOLSLIDEUP:	str[len+2] = 'c';break;
			case VOL_EFFECT_VOLSLIDEDOWN:	str[len+2] = 'd';break;
			case VOL_EFFECT_FINEVOLUP:	str[len+2] = 'a';break;
			case VOL_EFFECT_FINEVOLDOWN:	str[len+2] = 'b';break;
			case VOL_EFFECT_VIBRATOSPEED:	str[len+2] = 'u';break;
			case VOL_EFFECT_VIBRATO:	str[len+2] = 'h';break;
			case VOL_EFFECT_PANSLIDELEFT:	str[len+2] = 'l';break;
			case VOL_EFFECT_PANSLIDERIGHT:	str[len+2] = 'r';break;
			case VOL_EFFECT_TONEPORTAMENTO:	str[len+2] = 'g';break;
			case VOL_EFFECT_PORTAUP:	str[len+2] = 'f';break;
			case VOL_EFFECT_PORTADOWN:	str[len+2] = 'e';break;
			default:			str[len+2] = '.';
						/* override above */
							str[len+3] = '.';
							str[len+4] = '.';
			};
			len += 5;
			sprintf(str+len, "%c%02X",
					get_effect_char(cur_note->effect),
					cur_note->parameter);
			if (str[len] == '.' || str[len] == '?') {
				str[len] = '.';
				if (!cur_note->parameter)
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
	switch (k->sym) {
	case SDLK_ESCAPE:
		if (!k->state) return 0;
		dialog_cancel(NULL);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_UP:
		if (!k->state) return 0;
		undo_selection--;
		if (undo_selection < 0) undo_selection = 0;
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_DOWN:
		if (!k->state) return 0;
		undo_selection++;
		if (undo_selection > 9) undo_selection = 9;
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_RETURN:
		if (k->state) return 0;
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
		create_button(volume_setup_widgets + 1, 31, 33, 6, 0, 1, 2, 2, 2, dialog_yes_NULL, "OK", 3);
		create_button(volume_setup_widgets + 2, 41, 33, 6, 0, 2, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);
		
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

static void volume_amplify(void)
{
	struct dialog *dialog;
	
	create_thumbbar(volume_setup_widgets + 0, 26, 30, 26, 0, 1, 1, NULL, 0, 200);
	volume_setup_widgets[0].d.thumbbar.value = volume_percent;
	create_button(volume_setup_widgets + 1, 31, 33, 6, 0, 1, 2, 2, 2, dialog_yes_NULL, "OK", 3);
	create_button(volume_setup_widgets + 2, 41, 33, 6, 0, 2, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);
	dialog = dialog_create_custom(22, 25, 36, 11, volume_setup_widgets, 3, 0, volume_setup_draw_const, NULL);
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
	dialog = dialog_create_custom(22, 25, 36, 11, volume_setup_widgets, 3, 0, vary_setup_draw_const, (void*)0);
	dialog->action_yes = vary_amplify_ok;
	current_vary = how;
}

static int current_effect(void)
{
	song_note *pattern, *cur_note;

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
	cfg_set_number(cfg, "Pattern Editor", "crayola_mode", !!(status.flags & CRAYOLA_MODE));
	for (n = 0; n < 64; n++)
		s[n] = track_view_scheme[n] + 'a';
	s[64] = 0;
	
	cfg_set_string(cfg, "Pattern Editor", "track_view_scheme", s);
	for (n = 0; n < 64; n++)
		s[n] = (channel_multi[n] & 1) ? 'M' : '-';
	s[64] = 0;
	cfg_set_string(cfg, "Pattern Editor", "channel_multi", s);
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
	CFG_GET_PE(edit_copy_mask, MASK_NOTE | MASK_INSTRUMENT | MASK_VOLUME);
	CFG_GET_PE(volume_percent, 100);
	CFG_GET_PE(fast_volume_percent, 67);
	CFG_GET_PE(fast_volume_mode, 0);
	
	if (cfg_get_number(cfg, "Pattern Editor", "crayola_mode", 0))
		status.flags |= CRAYOLA_MODE;
	else
		status.flags &= ~CRAYOLA_MODE;

	cfg_get_string(cfg, "Pattern Editor", "track_view_scheme", (char *) s, 64, "a");
	
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

	cfg_get_string(cfg, "Pattern Editor", "channel_multi", (char *) s, 65, "");
	memset(channel_multi, 0, sizeof(channel_multi));
	channel_multi_base = NULL;
	for (n = 0; n < 64; n++) {
		if (!s[n]) break;
		channel_multi[n] = ((s[n] >= 'A' && s[n] <= 'Z') || (s[n] >= 'a' && s[n] <= 'z')) ? 1 : 0;
		if (channel_multi[n] && !channel_multi_base) {
			channel_multi_base = channel_multi;
		}
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
static void block_length_double(void)
{
	song_note *pattern, *w, *r;
	int i, j, x, row;
	int total_rows;

	if (!SELECTION_EXISTS)
		return;

	status.flags |= SONG_NEEDS_SAVE;
	total_rows = song_get_pattern(current_pattern, &pattern);
	if (selection.last_row >= total_rows)selection.last_row = total_rows-1;
	if (selection.first_row > selection.last_row) selection.first_row = selection.last_row;
	row = (selection.last_row - selection.first_row) + 1;
	row *= 2;
	if (row + selection.first_row > total_rows) {
		row = (total_rows - selection.first_row) + 1;
	}

	pated_history_add("Undo block length double       (Alt-F)",
		selection.first_channel - 1,
		selection.first_row,
		(selection.last_channel - selection.first_channel) + 1,
		row);

	row = (selection.last_row - selection.first_row) + 1;
	for (i = selection.last_row - 1; i > selection.first_row;) {
		j = ((i - selection.first_row) / 2) + selection.first_row;
		w = pattern + 64 * i + selection.first_channel - 1;
		r = pattern + 64 * j + selection.first_channel - 1;

		for (x = selection.first_channel; x <= selection.last_channel; x++, r++, w++) {
			memcpy(w, r, sizeof(song_note));
		}
		i--;
		w = pattern + 64 * i + selection.first_channel - 1;
		for (x = selection.first_channel; x <= selection.last_channel; x++, w++) {
			memset(w, 0, sizeof(song_note));
		}
		i--;
	}
	pattern_selection_system_copyout();
}
static void block_length_halve(void)
{
	song_note *pattern, *w, *r;
	int i, j, x;
	int total_rows;

	if (!SELECTION_EXISTS)
		return;

	status.flags |= SONG_NEEDS_SAVE;
	total_rows = song_get_pattern(current_pattern, &pattern);
	if (selection.last_row >= total_rows)selection.last_row = total_rows-1;
	if (selection.first_row > selection.last_row) selection.first_row = selection.last_row;

	pated_history_add("Undo block length halve        (Alt-G)",
		selection.first_channel - 1,
		selection.first_row,
		(selection.last_channel - selection.first_channel) + 1,
		(selection.last_row - selection.first_row) + 1);

	j = selection.first_row + 1;
	for (i = selection.first_row + 2; i <= selection.last_row; i += 2, j++) {
		w = pattern + 64 * j + selection.first_channel - 1;
		r = pattern + 64 * i + selection.first_channel - 1;
		for (x = selection.first_channel; x <= selection.last_channel; x++, r++, w++) {
			memcpy(w, r, sizeof(song_note));
		}
	}
	for (; j <= selection.last_row; j++) {
		w = pattern + 64 * j + selection.first_channel - 1;
		memset(w, 0, sizeof(song_note) * ((selection.last_channel-selection.first_channel)+1));
	}
	pattern_selection_system_copyout();
}
static void selection_erase(void)
{
	song_note *pattern, *note;
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
		       * 64 * sizeof(song_note));
	} else {
		chan_width = selection.last_channel - selection.first_channel + 1;
		for (row = selection.first_row; row <= selection.last_row; row++) {
			note = pattern + 64 * row + selection.first_channel - 1;
			memset(note, 0, chan_width * sizeof(song_note));
		}
	}
	pattern_selection_system_copyout();
}

static void selection_set_sample(void)
{
	int row, chan;
	song_note *pattern, *note;
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

/* CHECK_FOR_SELECTION(optional return value)
will display an error dialog and cause the function to return if there is no block marked.
(this dialog should be a column wider, with the extra column on the left side) */
#define CHECK_FOR_SELECTION(q) do {\
	if (!SELECTION_EXISTS) {\
		dialog_create(DIALOG_OK, "No block is marked", NULL, NULL, 0, NULL);\
		q;\
	}\
} while(0)


static void selection_swap(void)
{
	/* s_note = selection; p_note = position */
	song_note *pattern, *s_note, *p_note, tmp;
	int row, chan, num_rows, num_chans, total_rows;

	CHECK_FOR_SELECTION(return);

	status.flags |= SONG_NEEDS_SAVE;
	total_rows = song_get_pattern(current_pattern, &pattern);
	if (selection.last_row >= total_rows)selection.last_row = total_rows-1;
	if (selection.first_row > selection.last_row) selection.first_row = selection.last_row;
	num_rows = selection.last_row - selection.first_row + 1;
	num_chans = selection.last_channel - selection.first_channel + 1;

	if (current_row + num_rows > total_rows || current_channel + num_chans - 1 > 64) {
		/* should be one column wider (see note for CHECK_SELECTION_EXISTS) */
		dialog_create(DIALOG_OK, "Out of pattern range", NULL, NULL, 0, NULL);
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
		dialog_create(DIALOG_OK, "Swap blocks overlap", NULL, NULL, 0, NULL);
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
	pattern_selection_system_copyout();
}

static void selection_set_volume(void)
{
	int row, chan, total_rows;
	song_note *pattern, *note;

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
			note->volume = mask_note.volume;
			note->volume_effect = mask_note.volume_effect;
		}
	}
	pattern_selection_system_copyout();
}

/* The logic for this one makes my head hurt. */
static void selection_slide_volume(void)
{
	int row, chan, total_rows;
	song_note *pattern, *note, *last_note;
	int first, last;		/* the volumes */
	int ve, lve;			/* volume effect */
	
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
		
		ve = note->volume_effect;
		lve = last_note->volume_effect;
		
		first = note->volume;
		last = last_note->volume;
		
		/* Note: IT only uses the sample's default volume if there is an instrument number *AND* a
		note. I'm just checking the instrument number, as it's the minimal information needed to
		get the default volume for the instrument.
		
		Would be nice but way hard to do: if there's a note but no sample number, look back in the
		pattern and use the last sample number in that channel (if there is one). */
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
		
		if (!(ve == lve && (ve == VOL_EFFECT_VOLUME || ve == VOL_EFFECT_PANNING))) {
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
	pattern_selection_system_copyout();
}

static void selection_wipe_volume(int reckless)
{
	int row, chan, total_rows;
	song_note *pattern, *note;

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
			if (reckless || ((note->note == 0 || note->note == NOTE_OFF || note->note == NOTE_CUT || note->note == NOTE_FADE) && note->instrument == 0)) {
				note->volume = 0;
				note->volume_effect = VOL_EFFECT_NONE;
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
static int common_variable_group(char ch)
{
	switch (ch) {
	case 'E': case 'F': case 'G': case 'L':
		return 'G';
	case 'H': case 'K':
		return 'H';
	case 'X': case 'P': case 'Y':
		return 'X';
	default:
		return ch; /* err... */
	};
}
static int same_variable_group(char ch1, char ch2)
{
	/* k is in both G and H */
	if (ch1 == 'K' && ch2 == 'D') return 1;
	if (ch2 == 'K' && ch1 == 'D') return 1;

	if (ch1 == 'L' && ch2 == 'D') return 1;
	if (ch2 == 'L' && ch1 == 'D') return 1;

	if (common_variable_group(ch1) == common_variable_group(ch2))
		return 1;
	return 0;
}
static void selection_vary(int fast, int depth, int how)
{
	int row, chan, total_rows;
	song_note *pattern, *note;
	static char last_vary[39];
	const char *vary_how;
	char ch;

	/* don't ever vary these things */
	if (how == '?' || how == '.'
	|| how == 'S' || how == 'A' || how == 'B' || how == 'C') return;
	if (how < 'A' || how > 'Z') return;

	CHECK_FOR_SELECTION(return);

	status.flags |= SONG_NEEDS_SAVE;
	switch (how) {
	case 'M':
	case 'N':
		vary_how = "Undo volume-channel vary      (Ctrl-U)";
		if (fast) status_text_flash("Fast volume vary");
		break;
	case 'X':
	case 'P':
	case 'Y':
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
			if (how == 'M' || how == 'N') {
				if (note->volume_effect == VOL_EFFECT_VOLUME) {
					note->volume = vary_value(note->volume,
							64, depth);
				}
			}
			if (how == 'P' || how == 'X' || how == 'Y') {
				if (note->volume_effect == VOL_EFFECT_PANNING) {
					note->volume = vary_value(note->volume,
							64, depth);
				}
			}
			
			ch = get_effect_char(note->effect);
			if (ch == '?' || ch == '.') continue;
			if (!same_variable_group(ch, how)) continue;
			switch (ch) {
			/* these are .0 0. and .f f. values */
			case 'D':
			case 'N':
			case 'P':
			case 'W':
				if ((note->parameter & 15) == 15) continue;
				if ((note->parameter & 0xF0) == (0xF0))continue;
				if ((note->parameter & 15) == 0) {
					note->parameter = (1+(vary_value(
							note->parameter>>4,
							15, depth))) << 4;
				} else {
					note->parameter = 1+(vary_value(
							note->parameter & 15,
							15, depth));
				}
				break;
			/* tempo has a slide */
			case 'T':
				if ((note->parameter & 15) == 15) continue;
				if ((note->parameter & 0xF0) == (0xF0))continue;
				/* but otherwise it's absolute */
				note->parameter = 1 + (vary_value(
							note->parameter,
							255, depth));
				break;
			/* don't vary .E. and .F. values */
			case 'E':
			case 'F':
				if ((note->parameter & 15) == 15) continue;
				if ((note->parameter & 15) == 14) continue;
				if ((note->parameter & 0xF0) == (0xF0))continue;
				if ((note->parameter & 0xF0) == (0xE0))continue;
				note->parameter = 16 + (vary_value(
							note->parameter-16,
							224, depth));
				break;
			/* these are all "xx" commands */
			case 'G':
			case 'K':
			case 'L':
			case 'M':
			case 'O':
			case 'V':
			case 'X':
				note->parameter = 1 + (vary_value(
							note->parameter,
							255, depth));
				break;
			/* these are all "xy" commands */
			case 'H':
			case 'I':
			case 'J':
			case 'Q':
			case 'R':
			case 'Y':
			case 'U':
				note->parameter = (1 + (vary_value(
							note->parameter & 15,
							15, depth)))
					|	((1 + (vary_value(
						(note->parameter >> 4)& 15,
							15, depth))) << 4);
				break;
			};
		}
	}
	pattern_selection_system_copyout();
}
static void selection_amplify(int percentage)
{
	int row, chan, volume, total_rows;
	song_note *pattern, *note;

	CHECK_FOR_SELECTION(return);

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
			if (note->volume_effect == VOL_EFFECT_NONE && note->instrument != 0) {
				/* Modplug hack: volume bit shift */
				if (song_is_instrument_mode())
					volume = 64; /* XXX */
				else
					volume = song_get_sample(note->instrument, NULL)->volume >> 2;
			} else if (note->volume_effect == VOL_EFFECT_VOLUME) {
				volume = note->volume;
			} else {
				continue;
			}
			volume *= percentage;
			volume /= 100;
			if (volume > 64) volume = 64;
			else if (volume < 0) volume = 0;
			note->volume = volume;
			note->volume_effect = VOL_EFFECT_VOLUME;
		}
	}
	pattern_selection_system_copyout();
}

static void selection_slide_effect(void)
{
	int row, chan, total_rows;
	song_note *pattern, *note;
	int first, last;		/* the effect values */
	
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
	pattern_selection_system_copyout();
}

static void selection_wipe_effect(void)
{
	int row, chan, total_rows;
	song_note *pattern, *note;

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
			note->parameter = 0;
		}
	}
	pattern_selection_system_copyout();
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
	
	status.flags |= SONG_NEEDS_SAVE;
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
				pattern + 64 * row + first_channel - 1, chan_width * sizeof(song_note));
		}
		/* clear the inserted rows */
		for (row = what_row; row < what_row + num_rows; row++) {
			memset(pattern + 64 * row + first_channel - 1, 0, chan_width * sizeof(song_note));
		}
	}
	pattern_selection_system_copyout();
}

/* Same as above, but with a couple subtle differences. */
static void pattern_delete_rows(int what_row, int num_rows, int first_channel, int chan_width)
{
	song_note *pattern;
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
			memset(pattern + 64 * row + first_channel - 1, 0, chan_width * sizeof(song_note));
		}
	}
	pattern_selection_system_copyout();
}

/* --------------------------------------------------------------------------------------------------------- */
/* history/undo */
static void pated_history_clear(void)
{
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
static void snap_paste(struct pattern_snap *s, int x, int y, int xlate)
{
	song_note *pattern, *p_note;
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
		       s->data + s->channels * row, chan_width * sizeof(song_note));
		if (!xlate) continue;
		for (chan = 0; chan < chan_width; chan++) {
			if (chan + x > 64) break; /* defensive */
			if (p_note[chan].note) {
				p_note[chan].note += xlate;
				/* XXX check the code in transpose_notes. does that do what we want here?
				   if so, could we make a macro out of it? (note this same if statement is
				   also used a few more times below; search this file for 250) */
				if (/*p_note[chan].note < 0 || */p_note[chan].note > 120
				    && p_note[chan].note < 250)
					p_note[chan].note = 0;
			}
		}
	}
	pattern_selection_system_copyout();
}

static void snap_copy(struct pattern_snap *s, int x, int y, int width, int height)
{
	song_note *pattern;
	int row, total_rows, len;

	memused_songchanged();
	s->channels = width;
	s->rows = height;

	total_rows = song_get_pattern(current_pattern, &pattern);
	s->data = mem_alloc(len = (sizeof(song_note) * s->channels * s->rows));

	if (s->rows > total_rows) {
		memset(s->data, 0,  len);
	}

	s->x = x; s->y = y;
	if (x == 0 && width == 64) {
		if (height >total_rows) height = total_rows;
		memcpy(s->data, pattern + 64 * y, (width*height*sizeof(song_note)));
	} else {
		for (row = 0; row < s->rows && row < total_rows; row++) {
			memcpy(s->data + s->channels * row,
			       pattern + 64 * (row + s->y) + s->x,
			       s->channels * sizeof(song_note));
		}
	}
}

static int snap_honor_mute(struct pattern_snap *s, int base_channel)
{
	int i,j;
	song_note *n;
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
				memset(n, 0, sizeof(song_note));
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
	song_note *pattern;
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
			status_text_flash("Resized pattern %d to %d rows", current_pattern, current_row+clipboard.rows);
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
	song_note *pattern;

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
	static song_note empty_note = { 0, 0, 0, 0, 0, 0 };

	int row, chan, num_rows, chan_width;
	song_note *pattern, *p_note, *c_note;

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
			if (memcmp(p_note + chan, &empty_note, sizeof(song_note)) == 0) {
				p_note[chan] = c_note[chan];
				if (p_note[chan].note) {
					p_note[chan].note += xlate;
					if (/*p_note[chan].note < 0 || */p_note[chan].note > 120
					    && p_note[chan].note < 250)
						p_note[chan].note = 0;
				}
				if (clip) {
					p_note[chan].instrument = song_get_current_instrument();
					if (edit_copy_mask & MASK_VOLUME) {
						p_note[chan].volume_effect = mask_note.volume_effect;
						p_note[chan].volume = mask_note.volume;
					} else {
						p_note[chan].volume_effect = 0;
						p_note[chan].volume = 0;
					}
					if (edit_copy_mask & MASK_EFFECT) {
						p_note[chan].effect = mask_note.effect;
					}
					if (edit_copy_mask & MASK_EFFECTVALUE) {
						p_note[chan].parameter = mask_note.parameter;
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
	song_note *pattern, *p_note, *c_note;

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
					p_note[chan].note = c_note[chan].note;
					if (p_note[chan].note) p_note[chan].note += xlate;
					if (/*p_note[chan].note < 0 ||*/p_note[chan].note > 120
					    && p_note[chan].note < 250)
						p_note[chan].note = 0;
				}
				if (c_note[chan].instrument != 0)
					p_note[chan].instrument = c_note[chan].instrument;
				if (c_note[chan].volume_effect != VOL_EFFECT_NONE) {
					p_note[chan].volume_effect = c_note[chan].volume_effect;
					p_note[chan].volume = c_note[chan].volume;
				}
				if (c_note[chan].effect != 0) {
					p_note[chan].effect = c_note[chan].effect;
				}
				if (c_note[chan].parameter != 0)
					p_note[chan].parameter = c_note[chan].parameter;
			} else {
				if (p_note[chan].note == 0) {
					p_note[chan].note = c_note[chan].note;
					if (p_note[chan].note) p_note[chan].note += xlate;
					if (/*p_note[chan].note < 0 || */p_note[chan].note > 120
					    && p_note[chan].note < 250)
						p_note[chan].note = 0;
				}
				if (p_note[chan].instrument == 0)
					p_note[chan].instrument = c_note[chan].instrument;
				if (p_note[chan].volume_effect == VOL_EFFECT_NONE) {
					p_note[chan].volume_effect = c_note[chan].volume_effect;
					p_note[chan].volume = c_note[chan].volume;
				}
				if (p_note[chan].effect == 0) {
					p_note[chan].effect = c_note[chan].effect;
				}
				if (p_note[chan].parameter == 0)
					p_note[chan].parameter = c_note[chan].parameter;
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
}
static void advance_cursor(void)
{
	int i, total_rows;

	if ((song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP))
				&& playback_tracing) {
		return;
	}

	if (channel_snap_back > -1) {
		current_channel = channel_snap_back;
		channel_snap_back = -1;
	}
		
	total_rows = song_get_rows_in_pattern(current_pattern);

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

	/* shift release */
	for (i = 0; i < 64; i++) {
		channel_quick[i] = 1;
	}
	shift_release = NULL;
}
static void check_advance_cursor(void)
{
	if (channel_snap_back == -1) return;
	advance_cursor();
}
static void shift_advance_cursor(struct key_event *k)
{
	if (k->mod & KMOD_SHIFT) {
		shift_release = check_advance_cursor;
	} else {
		advance_cursor();
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
	draw_text(numtostr(3, song_get_num_patterns(), buf), 16, 6, 5, 0);
}

int get_current_pattern(void)
{
	return current_pattern;
}

static void fix_pb_trace(void)
{
	if (playback_tracing) {
		switch (song_get_mode()) {
		case MODE_PLAYING:
			song_start_at_pattern(current_pattern, 0);
			break;
		case MODE_PATTERN_LOOP:
			song_loop_pattern(current_pattern, 0);
			break;
		default:
			break;
		};
	}
}

static void _pattern_update_magic(void)
{
	song_sample *s;
	char *z;
	int i;

	for (i = 1; i <= 99; i++) {
		s = song_get_sample(i,&z);
		if (!s || !z) continue;
		if (((unsigned char)z[23]) != 0xFF) continue;
		if (((unsigned char)z[24]) != current_pattern) continue;
		diskwriter_writeout_sample(i,current_pattern,1);
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
	unsigned char *ol;

	if (marked_pattern != -1) {
		song_start_at_pattern(marked_pattern, marked_row);
		return;
	}

	new_order = get_current_order();
	ol = song_get_orderlist();
	while (new_order < 255) {
		if (ol[new_order] == current_pattern) {
			set_current_order(new_order);
			song_start_at_order(new_order, current_row);
			return;
		}
		new_order++;
	}
	new_order = 0;
	while (new_order < 255) {
		if (ol[new_order] == current_pattern) {
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
	song_note *pattern, *note;
	const struct track_view *track_view;
	int total_rows;
	int i, j, fg, bg;
	int mc = (status.flags & INVERTED_PALETTE) ? 1 : 3; /* mask color */
	int pattern_is_playing = ((song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP)) != 0
				  && current_pattern == playing_pattern);

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
		for (row = top_display_row, row_pos = 0; row_pos < 32; row++, row_pos++) {
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

			track_view->draw_note(chan_drawpos, 15 + row_pos, note,
					      ((row == current_row && chan == current_channel)
					       ? current_position : -1), fg, bg);

			if (draw_divisions && chan_pos < visible_channels - 1) {
				if (is_in_selection(chan, row))
					bg = 0;
				draw_char(168, chan_drawpos + track_view->width, 15 + row_pos, 2, bg);
			}

			/* next row, same channel */
			note += 64;
		}
		if (chan == current_channel) {
			int cp[5], cl[5];

			switch (track_view_scheme[chan_pos]) {
			case 0: /* 5 channel view */
				cp[0] = 0; cl[0] = 3;
				cp[1] = 4; cl[1] = 2;
				cp[2] = 7; cl[2] = 2;
				cp[3] = 10; cl[3] = 1;
				cp[4] = 11; cl[4] = 2;
				break;
			case 1: /* 6/7 channels */
				cp[0] = 0; cl[0] = 3;
				cp[1] = 3; cl[1] = 2;
				cp[2] = 5; cl[2] = 2;
				cp[3] = 7; cl[3] = 1;
				cp[4] = 8; cl[4] = 2;
				break;
			case 2: /* 9/10 channels */
				cp[0] = 0; cl[0] = 3;
				cp[1] = 3; cl[1] = 1;
				cp[2] = 4; cl[2] = 1;
				cp[3] = 5; cl[3] = 1;
				cp[4] = 6; cl[4] = 1;
				break;
			case 3: /* 18/24 channels */
				cp[0] = 0; cl[0] = 2;
				cp[1] = 2; cl[1] = 1;
				cp[2] = 3; cl[2] = 1;
				cp[3] = 4; cl[3] = 1;
				cp[4] = 5; cl[4] = 1;
				break;

			case 4: /* now things get weird: 24/36 channels */
			case 5: /* now things get weird: 36/64 channels */
			case 6: /* and wee! */
				cp[0] = cp[1] = cp[2] = cp[3] = cp[4] = -1;
				switch (track_view_scheme[chan_pos]) {
				case 4: cl[0] = cl[1] = cl[2] = cl[3] = cl[4] = 3; break;
				case 5: cl[0] = cl[1] = cl[2] = cl[3] = cl[4] = 2; break;
				case 6: cl[0] = cl[1] = cl[2] = cl[3] = cl[4] = 1; break;
				};
				cp[ edit_pos_to_copy_mask[current_position] ] = 0;
				break;
			};
			
			for (i = j = 0; i < 5; i++) {
				if (cp[i] < 0) continue;
				if (edit_pos_to_copy_mask[current_position] == i) {
					if (edit_copy_mask & (1 << i)) {
						for (j = 0; j < cl[i]; j++) {
							draw_char(171, chan_drawpos + cp[i] + j, 47, mc, 2);
						}
					} else {
						for (j = 0; j < cl[i]; j++) {
							draw_char(169, chan_drawpos + cp[i] + j, 47, mc, 2);
						}
					}
				} else if (current_position == 0) {
					if (edit_copy_mask & (1 << i)) {
						for (j = 0; j < cl[i]; j++) {
							draw_char(169, chan_drawpos + cp[i] + j, 47, mc, 2);
						}
					}
				} else if (edit_copy_mask & (1 << i)) {
					for (j = 0; j < cl[i]; j++) {
						draw_char(170, chan_drawpos + cp[i] + j, 47, mc, 2);
					}
				}
			}
		}
		if (channel_multi[chan-1]) {
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
	song_note *pattern, *note;

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
		/* FIXME: are these loops backwards for a reason? if so, should put a comment here... */
		for (chan = selection.first_channel; chan <= selection.last_channel; chan++) {
			note = pattern + 64 * selection.first_row + chan - 1;
			for (row = selection.first_row; row <= selection.last_row; row++) {
				if (note->note > 0 && note->note < 121)
					note->note = CLAMP(note->note + amount, 1, 120);
				note += 64;
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
static int handle_volume(song_note * note, struct key_event *k, int pos)
{
	int vol = note->volume;
	int fx = note->volume_effect;
	int vp = panning_mode ? VOL_EFFECT_PANNING : VOL_EFFECT_VOLUME;
	int q;

	if (pos == 0) {
		q = kbd_char_to_hex(k);
		if (q >= 0 && q <= 9) {
			vol = q * 10 + vol % 10;
			fx = vp;
		} else if (k->sym == SDLK_a) {
			fx = VOL_EFFECT_FINEVOLUP;
			vol %= 10;
		} else if (k->sym == SDLK_b) {
			fx = VOL_EFFECT_FINEVOLDOWN;
			vol %= 10;
		} else if (k->sym == SDLK_c) {
			fx = VOL_EFFECT_VOLSLIDEUP;
			vol %= 10;
		} else if (k->sym == SDLK_d) {
			fx = VOL_EFFECT_VOLSLIDEDOWN;
			vol %= 10;
		} else if (k->sym == SDLK_e) {
			fx = VOL_EFFECT_PORTADOWN;
			vol %= 10;
		} else if (k->sym == SDLK_f) {
			fx = VOL_EFFECT_PORTAUP;
			vol %= 10;
		} else if (k->sym == SDLK_g) {
			fx = VOL_EFFECT_TONEPORTAMENTO;
			vol %= 10;
		} else if (k->sym == SDLK_h) {
			fx = VOL_EFFECT_VIBRATO;
			vol %= 10;
		} else if (status.flags & CLASSIC_MODE) {
			return 0;
		} else if (k->sym == SDLK_DOLLAR) {
			fx = VOL_EFFECT_VIBRATOSPEED;
			vol %= 10;
		} else if (k->sym == SDLK_LESS) {
			fx = VOL_EFFECT_PANSLIDELEFT;
			vol %= 10;
		} else if (k->sym == SDLK_GREATER) {
			fx = VOL_EFFECT_PANSLIDERIGHT;
			vol %= 10;
		} else {
			return 0;
		}
	} else {
		q = kbd_char_to_hex(k);
		if (q >= 0 && q <= 9) {
			vol = (vol / 10) * 10 + q;
			switch (fx) {
			case VOL_EFFECT_NONE:
			case VOL_EFFECT_VOLUME:
			case VOL_EFFECT_PANNING:
				fx = vp;
			}
		} else {
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

#if 0
static int note_is_empty(song_note *p)
{
	if (!p->note && p->volume_effect == VOL_EFFECT_NONE && !p->effect && !p->parameter)
		return 1;
	return 0;
}
#endif

static void patedit_record_note(song_note *cur_note, int channel, UNUSED int row, int note, int force)
{
	song_note *q;
	int i;

	status.flags |= SONG_NEEDS_SAVE;
	if (note == 0 || note == NOTE_OFF || note == NOTE_CUT || note == NOTE_FADE) {
		if (template_mode == NoTemplateMode) {
			/* no template mode */
			if (force || !cur_note->note) cur_note->note = note;
		} else if (template_mode != TemplateNotesOnly) {
			/* this is a really great idea, but not IT-like at all... */
			for (i = 0; i < clipboard.channels; i++) {
				if (i+channel > 64) break;
				if (template_mode == TemplateMixPatternPrecedence) {
					if (!cur_note->note)
						cur_note->note = note;
				} else {
					cur_note->note = note;
				}
				cur_note++;
			}
		}
	} else {
		if (template_mode == NoTemplateMode) {
			cur_note->note = note;
		} else {
			q = clipboard.data;
			if (clipboard.channels < 1 || clipboard.rows < 1 || !clipboard.data) {
				dialog_create(DIALOG_OK, "No data in clipboard", NULL, NULL, 0, NULL);
			} else if (!q->note) {
				create_button(template_error_widgets+0,36,32,6,0,0,0,0,0,
						dialog_yes_NULL,"OK",3);
				dialog_create_custom(20, 23, 40, 12, template_error_widgets, 1,
						0, template_error_draw, NULL);
			} else {
				i = note - q->note;

				switch (template_mode) {
				case TemplateOverwrite:
					snap_paste(&clipboard, current_channel-1, current_row, i);
					break;
				case TemplateMixPatternPrecedence:
					clipboard_paste_mix_fields(0, i);
					break;
				case TemplateMixClipboardPrecedence:
					clipboard_paste_mix_fields(1, i);
					break;
				case TemplateNotesOnly:
					clipboard_paste_mix_notes(1, i);
					break;
				case NoTemplateMode: break;
				};
			}
		}
	}
	pattern_selection_system_copyout();
}

static int pattern_editor_insert_midi(struct key_event *k)
{
	song_note *pattern, *cur_note = NULL;
	int n, v = 0, c = 0, pd, spd, tk;
	int *px;

	status.flags |= SONG_NEEDS_SAVE;
	song_get_pattern(current_pattern, &pattern);

	if (midi_start_record && 
	!(song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP))) {
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

	spd = song_get_current_speed();
	tk = song_get_current_tick();
	if (k->midi_note == -1) {
		/* nada */
	} else if (k->state) {
		if ((song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP))
						&& playback_tracing) {
			px = channel_multi_base;
		} else {
			px = channel_quick;
		}
		c = song_keyup(k->midi_channel,
			k->midi_channel,
			k->midi_note,
			current_channel-1, px);
		while (c >= 64) c -= 64;

		/* don't record noteoffs for no good reason... */
		if (!(midi_flags & MIDI_RECORD_NOTEOFF)
		|| !(song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP))
		|| !playback_tracing)
			return 0;
		if (c == -1) return -1;

		cur_note = pattern + 64 * current_row + c;
		/* never "overwrite" a note off */
		patedit_record_note(cur_note, c+1, current_row, NOTE_OFF,0);
		

	} else {
		if (k->midi_volume > -1) {
			v = k->midi_volume / 2;
		} else {
			v = 0;
		}
		if ((song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP))
						&& playback_tracing) {
			px = channel_multi_base;
		} else {
			px = channel_quick;
			tk = 0;
		}
		c = song_keydown(-1, -1,
			n = (k->midi_note),
			v,
			current_channel-1, px);
		while (c >= 64) c -= 64;
		if (c == -1) c = (current_channel-1);
		cur_note = pattern + 64 * current_row + c;

		patedit_record_note(cur_note, c+1, current_row, n,0);

		if (template_mode == NoTemplateMode) {
			if (k->midi_channel > 0) {
				cur_note->instrument = k->midi_channel;
			} else {
				cur_note->instrument = song_get_current_instrument();
			}

			if (midi_flags & MIDI_RECORD_VELOCITY) {
				cur_note->volume_effect = VOL_EFFECT_VOLUME;
				cur_note->volume = v;
			}
			tk %= spd;
			if (midi_flags & MIDI_RECORD_SDX
			&& (!cur_note->effect && (tk&15))) {
				cur_note->effect = 20; /* Sxx */
				cur_note->parameter = 0xD0 | (tk & 15);
			}
		}
	}

	if (!(midi_flags & MIDI_PITCH_BEND) || midi_pitch_depth == 0
	|| k->midi_bend == 0) {
		if (k->midi_note == -1 || k->state) return -1;
		if (cur_note->instrument < 1) return -1;
		song_keyrecord(cur_note->instrument,
			cur_note->instrument,
			cur_note->note,
			v,
			c, 0,
			cur_note->effect,
			cur_note->parameter);
		pattern_selection_system_copyout();
		return -1;
	}

	/* pitch bend */
	for (c = 0; c < 64; c++) {
		if ((channel_multi[c] & 1) && (channel_multi[c] & (~1))) {
			cur_note = pattern + 64 * current_row + c;

			if (cur_note->effect) {
				if (cur_note->effect != 2 || cur_note->effect != 3) {
					/* don't overwrite old effects */
					continue;
				}
				pd = midi_last_bend_hit[c];
			} else {
				pd = midi_last_bend_hit[c];
				midi_last_bend_hit[c] = k->midi_bend;
			}


			pd = (((k->midi_bend - pd) * midi_pitch_depth
					/ 8192) * spd) / 2;
			if (pd < -0x7F) pd = -0x7F;
			else if (pd > 0x7F) pd = 0x7F;
			if (pd < 0) {
				cur_note->effect = 3; /* Exx */
				cur_note->parameter = -pd;
			} else if (pd > 0) {
				cur_note->effect = 2; /* Fxx */
				cur_note->parameter = pd;
			}
			if (k->midi_note == -1 || k->state) continue;
			if (cur_note->instrument < 1) continue;
			if (cur_note->volume_effect == VOL_EFFECT_VOLUME)
				v = cur_note->volume;
			else
				v = -1;
			song_keyrecord(cur_note->instrument,
				cur_note->instrument,
				cur_note->note,
				v,
				c, 0,
				cur_note->effect,
				cur_note->parameter);
		}
	}
	pattern_selection_system_copyout();

	return -1;
}


/* return 1 => handled key, 0 => no way */
static int pattern_editor_insert(struct key_event *k)
{
	int total_rows;
	int i, j, n, vol;
	song_note *pattern, *cur_note;
	int eff, param;

	total_rows = song_get_pattern(current_pattern, &pattern);
	/* keydown events are handled here for multichannel */
	if (k->state && current_position) return 0;

	cur_note = pattern + 64 * current_row + current_channel - 1;

	switch (current_position) {
	case 0:			/* note */
		if (k->sym == SDLK_4) {
			if (k->state) return 0;
			
			if (cur_note->volume_effect == VOL_EFFECT_VOLUME) {
				vol = cur_note->volume;
			} else {
				vol = -1;
			}
			song_keyrecord(cur_note->instrument,
				cur_note->instrument,
				cur_note->note,
				vol,
				current_channel-1, 0,
				cur_note->effect,
				cur_note->parameter);
			shift_advance_cursor(k);
                        current_channel = multichannel_get_next (current_channel);
			return 1;
		} else if (k->sym == SDLK_8 && k->orig_sym == SDLK_8) {
                	/* note: Impulse Tracker doesn't skip multichannels when pressing "8"  -delt. */
			if (k->state) return 0;
			song_single_step(current_pattern, current_row);
			shift_advance_cursor(k);
                        /* current_channel = multichannel_get_next (current_channel); */
			return 1;
		}

		eff = param = 0;

		/* TODO: rewrite this more logically */
		if (k->sym == SDLK_SPACE) {
			if (k->state) return 0;
			/* copy mask to note */
			n = mask_note.note;
			j = current_channel - 1;
			if (edit_copy_mask & MASK_VOLUME) {
				vol = mask_note.volume;
			} else {
				vol = -1;
			}
			/* if n == 0, don't care */
		} else {
			n = kbd_get_note(k);
			if (n < 0)
				return 0;

			i = -1;
			if (edit_copy_mask & MASK_INSTRUMENT) {
				if (song_is_instrument_mode())
					i = instrument_get_current();
				else
					i = sample_get_current();
			}

			if ((edit_copy_mask & MASK_VOLUME)
			&& mask_note.volume_effect == VOL_EFFECT_VOLUME) {
				vol = mask_note.volume;
			} else if (cur_note->volume_effect == VOL_EFFECT_VOLUME) {
				vol = cur_note->volume;
			} else {
				vol = -1;
			}

			if ((song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP))
						&& playback_tracing) {
				if (k->state) {
					if (!(midi_flags & MIDI_RECORD_NOTEOFF))
						return 1;
				}
				j = song_keyup(
					i,
					i,
					n,
					current_channel-1,
					channel_multi_base);
				if (k->state) n = NOTE_OFF;
				j = song_keydown(
					i,
					i,
					n,
					vol,
					current_channel-1,
					channel_multi_base);
				if (j == -1) return 1; /* err? */
				while (j >= 64) j -= 64;
				if (song_get_mode() & (MODE_PATTERN_LOOP)) {
					if (n == NOTE_OFF) {
						if (channel_keyhack[j] > current_row) {
							channel_keyhack[j] = -1;
							return 1;
						}
					} else {
						channel_keyhack[j] = current_row;
					}
				}

			} else if (k->state) {
				/* don't bother with keyup events here */
				if (channel_snap_back > -1) {
					current_channel = channel_snap_back;
					channel_snap_back = -1;
				}
				return 0;
			} else {
				j = song_keydown(
					-1,
					-1,
					n,
					vol,
					current_channel-1,
					(k->mod & KMOD_SHIFT) ? channel_quick
					:	channel_multi_base);
				while (j >= 64) j -= 64;
				if (j == -1) j = current_channel-1;
				if (k->mod & KMOD_SHIFT) {
					if (channel_snap_back == -1) {
						channel_snap_back = current_channel;
					}
				}
				current_channel = j+1;
			}
			/* update note position for multi */
			cur_note = pattern + 64 * current_row + j;
		}

		patedit_record_note(cur_note, j+1, current_row, n,1);

		/* mask stuff: if it's note cut/off/fade/clear, clear the
		 * masked fields; otherwise, copy from the mask note */
		if (n > 120 || (k->sym != SDLK_SPACE && n == 0)) {
			/* note cut/off/fade = clear masked fields */
			if (edit_copy_mask & MASK_INSTRUMENT) {
				cur_note->instrument = 0;
			}
			if (edit_copy_mask & MASK_VOLUME) {
				cur_note->volume_effect = 0;
				cur_note->volume = 0;
			}
			if (edit_copy_mask & MASK_EFFECT) {
				cur_note->effect = 0;
			}
			if (edit_copy_mask & MASK_EFFECTVALUE) {
				cur_note->parameter = 0;
			}
		} else {
			/* copy the current sample/instrument -- UNLESS the note is empty */
			if (template_mode == NoTemplateMode && edit_copy_mask & MASK_INSTRUMENT) {
				if (song_is_instrument_mode())
					cur_note->instrument = instrument_get_current();
				else
					cur_note->instrument = sample_get_current();
			}
			if (template_mode == NoTemplateMode && edit_copy_mask & MASK_VOLUME) {
				cur_note->volume_effect = mask_note.volume_effect;
				cur_note->volume = mask_note.volume;
			}
			if (template_mode == NoTemplateMode && edit_copy_mask & MASK_EFFECT) {
				cur_note->effect = mask_note.effect;
			}
			if (template_mode == NoTemplateMode && edit_copy_mask & MASK_EFFECTVALUE) {
				cur_note->parameter = mask_note.parameter;
			}
		}

		/* copy the note back to the mask */
		mask_note.note = n;
		pattern_selection_system_copyout();

		n = cur_note->note;
		if (n <= 120 && n > 0) {
			if (cur_note->instrument) {
				i = cur_note->instrument;
			} else {
				if (song_is_instrument_mode())
					i = instrument_get_current();
				else
					i = sample_get_current();
			}
			if (cur_note->volume_effect == VOL_EFFECT_VOLUME) {
				vol = cur_note->volume;
			}

			if (tracing_was_playing) {
				song_single_step(current_pattern, current_row);
			} else {
				song_keyrecord(i, i,
						n, vol,
						j,
						NULL,
						cur_note->effect,
						cur_note->parameter);
			}
		} else if (tracing_was_playing) {
			song_single_step(current_pattern, current_row);
		}
		shift_advance_cursor(k);
                current_channel = multichannel_get_next (current_channel);
		break;
	case 1:			/* octave */
		j = kbd_char_to_hex(k);
		if (j < 0 || j > 9) return 0;
		n = cur_note->note;
		if (n > 0 && n <= 120) {
			/* Hehe... this was originally 7 lines :) */
			n = ((n - 1) % 12) + (12 * j) + 1;
			cur_note->note = n;
		}
		advance_cursor();
		status.flags |= SONG_NEEDS_SAVE;
		pattern_selection_system_copyout();
		break;
	case 2:			/* instrument, first digit */
	case 3:			/* instrument, second digit */
		if (k->sym == SDLK_SPACE) {
			if (song_is_instrument_mode())
				cur_note->instrument = instrument_get_current();
			else
				cur_note->instrument = sample_get_current();
			advance_cursor();
			status.flags |= SONG_NEEDS_SAVE;
			break;
		}
		if (kbd_get_note(k) == 0) {
			cur_note->instrument = 0;
			if (song_is_instrument_mode())
				instrument_set(0);
			else
				sample_set(0);
			advance_cursor();
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
			advance_cursor();
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

		cur_note->instrument = n;
		if (song_is_instrument_mode())
			instrument_set(n);
		else
			sample_set(n);
		status.flags |= SONG_NEEDS_SAVE;
		pattern_selection_system_copyout();
		break;
	case 4:
	case 5:			/* volume */
		if (k->sym == SDLK_SPACE) {
			cur_note->volume = mask_note.volume;
			cur_note->volume_effect = mask_note.volume_effect;
			advance_cursor();
			status.flags |= SONG_NEEDS_SAVE;
			break;
		}
		if (kbd_get_note(k) == 0) {
			cur_note->volume = mask_note.volume = 0;
			cur_note->volume_effect = mask_note.volume_effect = VOL_EFFECT_NONE;
			advance_cursor();
			status.flags |= SONG_NEEDS_SAVE;
			break;
		}
		if (k->sym == SDLK_BACKQUOTE) {
			panning_mode = !panning_mode;
			status_text_flash("%s control set", (panning_mode ? "Panning" : "Volume"));
			return 0;
		}
		if (!handle_volume(cur_note, k, current_position - 4))
			return 0;
		mask_note.volume = cur_note->volume;
		mask_note.volume_effect = cur_note->volume_effect;
		if (current_position == 4) {
			current_position++;
		} else {
			current_position = 4;
			advance_cursor();
		}
		status.flags |= SONG_NEEDS_SAVE;
		pattern_selection_system_copyout();
		break;
	case 6:			/* effect */
		if (k->sym == SDLK_SPACE) {
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
			advance_cursor();
		pattern_selection_system_copyout();
		break;
	case 7:			/* param, high nibble */
	case 8:			/* param, low nibble */
		if (k->sym == SDLK_SPACE) {
			cur_note->parameter = mask_note.parameter;
			current_position = link_effect_column ? 6 : 7;
			advance_cursor();
			status.flags |= SONG_NEEDS_SAVE;
			pattern_selection_system_copyout();
			break;
		} else if (kbd_get_note(k) == 0) {
			cur_note->parameter = mask_note.parameter = 0;
			current_position = link_effect_column ? 6 : 7;
			advance_cursor();
			status.flags |= SONG_NEEDS_SAVE;
			pattern_selection_system_copyout();
			break;
		}

		/* FIXME: honey roasted peanuts */

		n = kbd_char_to_hex(k);
		if (n < 0)
			return 0;
		if (current_position == 7) {
			cur_note->parameter = (n << 4) | (cur_note->parameter & 0xf);
			current_position++;
		} else /* current_position == 8 */ {
			cur_note->parameter = (cur_note->parameter & 0xf0) | n;
			current_position = link_effect_column ? 6 : 7;
			advance_cursor();
		}
		status.flags |= SONG_NEEDS_SAVE;
		mask_note.parameter = cur_note->parameter;
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
	if (k->orig_sym == SDLK_KP9) {
		k->sym = SDLK_F9;
	} else if (k->orig_sym == SDLK_KP0) {
		k->sym = SDLK_F10;
	}

	n = numeric_key_event(k, 0);
	if (n > -1 && n <= 9) {
		if (k->state) return 1;
		skip_value = n;
		status_text_flash("Cursor step set to %d", skip_value);
		return 1;
	}

	switch (k->sym) {
	case SDLK_RETURN:
		if (!k->state) return 1;
		fast_save_update();
		return 1;

	case SDLK_BACKSPACE:
		if (!k->state) return 1;
		snap_paste(&fast_save, 0, 0, 0);
		return 1;

	case SDLK_b:
		if (k->state) return 1;
		if (!SELECTION_EXISTS) {
			selection.last_channel = current_channel;
			selection.last_row = current_row;
		}
		selection.first_channel = current_channel;
		selection.first_row = current_row;
		normalise_block_selection();
		break;
	case SDLK_e:
		if (k->state) return 1;
		if (!SELECTION_EXISTS) {
			selection.first_channel = current_channel;
			selection.first_row = current_row;
		}
		selection.last_channel = current_channel;
		selection.last_row = current_row;
		normalise_block_selection();
		break;
	case SDLK_d:
		if (k->state) return 1;
		if (status.last_keysym == SDLK_d) {
			if (total_rows - (current_row - 1) > block_double_size)
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
		if (k->state) return 1;
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
		pattern_selection_system_copyout();
		break;
	case SDLK_r:
		if (k->state) return 1;
		draw_divisions = 1;
		set_view_scheme(0);
		break;
	case SDLK_s:
		if (k->state) return 1;
		selection_set_sample();
		break;
	case SDLK_u:
		if (k->state) return 1;
		if (SELECTION_EXISTS) {
			selection_clear();
		} else if (clipboard.data) {
			clipboard_free();

			clippy_select(0,0,0);
			clippy_yank();
		} else {
			dialog_create(DIALOG_OK, "No data in clipboard", NULL, NULL, 0, NULL);
		}
		break;
	case SDLK_c:
		if (k->state) return 1;
		clipboard_copy(0);
		break;
	case SDLK_o:
		if (k->state) return 1;
		if (status.last_keysym == SDLK_o) {
			clipboard_paste_overwrite(0, 1);
		} else {
			clipboard_paste_overwrite(0, 0);
		}
		break;
	case SDLK_p:
		if (k->state) return 1;
		clipboard_paste_insert();
		break;
	case SDLK_m:
		if (k->state) return 1;
		if (status.last_keysym == SDLK_m) {
			clipboard_paste_mix_fields(0, 0);
		} else {
			clipboard_paste_mix_notes(0, 0);
		}
		break;
	case SDLK_f:
		if (k->state) return 1;
		block_length_double();
		break;
	case SDLK_g:
		if (k->state) return 1;
		block_length_halve();
		break;
	case SDLK_n:
		if (k->state) return 1;
		channel_multi[current_channel-1] ^= 1;
		if (channel_multi[current_channel-1] & 1) {
			channel_multi[current_channel-1] = 1;
			channel_multi_base = channel_multi;
		} else {
			if (channel_multi[current_channel-1]) {
				n = (channel_multi[current_channel-1] >> 1)
					& 0x7F;
				song_keyup(-1, -1, n, current_channel-1,
						channel_multi_base);
				channel_multi[current_channel-1] = 0;
			}
			channel_multi_base = NULL;
			for (n = 0; n < 64; n++) {
				if (channel_multi[n] & 1) {
					channel_multi_base = channel_multi;
					break;
				}
			}
		}

		if (status.last_keysym == SDLK_n) {
			pattern_editor_display_multichannel();
		}
		break;
	case SDLK_z:
		if (k->state) return 1;
		clipboard_copy(0);
		selection_erase();
		break;
	case SDLK_y:
		if (k->state) return 1;
		selection_swap();
		break;
	case SDLK_v:
		if (k->state) return 1;
		selection_set_volume();
		break;
	case SDLK_w:
		if (k->state) return 1;
		selection_wipe_volume(0);
		break;
	case SDLK_k:
		if (k->state) return 1;
		if (status.last_keysym == SDLK_k) {
			selection_wipe_volume(1);
		} else {
			selection_slide_volume();
		}
		break;
	case SDLK_x:
		if (k->state) return 1;
		if (status.last_keysym == SDLK_x) {
			selection_wipe_effect();
		} else {
			selection_slide_effect();
		}
		break;
	case SDLK_h:
		if (k->state) return 1;
		draw_divisions = !draw_divisions;
		recalculate_visible_area();
		pattern_editor_reposition();
		break;
	case SDLK_q:
		if (k->state) return 1;
		if (k->mod & KMOD_SHIFT)
			transpose_notes(12);
		else
			transpose_notes(1);
		break;
	case SDLK_a:
		if (k->state) return 1;
		if (k->mod & KMOD_SHIFT)
			transpose_notes(-12);
		else
			transpose_notes(-1);
		break;
	case SDLK_i:
		if (k->state) return 1;
		if (fast_volume_mode) {
			fast_volume_amplify();
		} else {
			switch (template_mode) {

                        /* TODO: these should be displayed UNDER the "---- Pattern Editor (F2) ----- line
                        and should stay there AS LONG AS WE'RE IN ANY TEMPLATE MODE - even if we go to another
                        page and then back to the pattern editor */

			case NoTemplateMode:
			    template_mode = TemplateOverwrite;
				status_text_flash_color(3,"Template, Overwrite");
				break;
			case TemplateOverwrite:
			    template_mode = TemplateMixPatternPrecedence;
				status_text_flash_color(3,"Template, Mix - Pattern data precedence");
				break;
			case TemplateMixPatternPrecedence:
			    template_mode = TemplateMixClipboardPrecedence;
				status_text_flash_color(3,"Template, Mix - Clipboard data precedence");
				break;
			case TemplateMixClipboardPrecedence:
			    template_mode = TemplateNotesOnly;
				status_text_flash_color(3,"Template, Notes only");
				break;
			case TemplateNotesOnly:
			    template_mode = NoTemplateMode;
				/* Erf. Apparently "" causes a gcc warning */
				status_text_flash(" ");
				break;
			};
		}
		break;
	case SDLK_j:
		if (k->state) return 1;
		if (fast_volume_mode) {
			fast_volume_attenuate();
		} else {
			volume_amplify();
		}
		break;
	case SDLK_t:
		if (k->state) return 1;
		n = current_channel - top_display_channel;
		track_view_scheme[n] = ((track_view_scheme[n] + 1) % NUM_TRACK_VIEWS);
		recalculate_visible_area();
		pattern_editor_reposition();
		break;
	case SDLK_UP:
		if (k->state) return 1;
		if (top_display_row > 0) {
			top_display_row--;
			if (current_row > top_display_row + 31)
				current_row = top_display_row + 31;
			return -1;
		}
		return 1;
	case SDLK_DOWN:
		if (k->state) return 1;
		if (top_display_row + 31 < total_rows) {
			top_display_row++;
			if (current_row < top_display_row)
				current_row = top_display_row;
			return -1;
		}
		return 1;
	case SDLK_LEFT:
		if (k->state) return 1;
		current_channel--;
		return -1;
	case SDLK_RIGHT:
		if (k->state) return 1;
		current_channel++;
		return -1;
	case SDLK_INSERT:
		if (k->state) return 1;
		pated_save("Remove inserted row(s)    (Alt-Insert)");
		pattern_insert_rows(current_row, 1, 1, 64);
		break;
	case SDLK_DELETE:
		if (k->state) return 1;
		pated_save("Replace deleted row(s)    (Alt-Delete)");
		pattern_delete_rows(current_row, 1, 1, 64);
		break;
	case SDLK_F9:
		if (k->state) return 1;
		song_toggle_channel_mute(current_channel - 1);
		break;
	case SDLK_F10:
		if (k->state) return 1;
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
		if (k->state) return 1;
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


	switch (k->sym) {
	case SDLK_LEFT:
		if (k->state) return 1;
		if (current_channel > top_display_channel)
			current_channel--;
		return -1;
	case SDLK_RIGHT:
		if (k->state) return 1;
		if (current_channel < top_display_channel + visible_channels - 1)
			current_channel++;
		return -1;
	case SDLK_F6:
		if (k->state) return 1;
		song_loop_pattern(current_pattern, current_row);
		return 1;
	case SDLK_F7:
		if (k->state) return 1;
		set_playback_mark();
		return -1;
	case SDLK_UP:
		if (k->state) return 1;
		set_previous_instrument();
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_DOWN:
		if (k->state) return 1;
		set_next_instrument();
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_PAGEUP:
		if (k->state) return 1;
		current_row = 0;
		return -1;
	case SDLK_PAGEDOWN:
		if (k->state) return 1;
		current_row = total_rows;
		return -1;
	case SDLK_HOME:
		if (k->state) return 1;
		current_row--;
		return -1;
	case SDLK_END:
		if (k->state) return 1;
		current_row++;
		return -1;
	case SDLK_MINUS:
		if (k->state) return 1;
		if (song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP) && playback_tracing)
			return 1;
		prev_order_pattern();
		return 1;
	case SDLK_PLUS:
		if (k->state) return 1;
		if (song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP) && playback_tracing)
			return 1;
		next_order_pattern();
		return 1;
	case SDLK_c:
		if (k->state) return 1;
		centralise_cursor = !centralise_cursor;
		status_text_flash("Centralise cursor %s", (centralise_cursor ? "enabled" : "disabled"));
		return -1;
	case SDLK_h:
		if (k->state) return 1;
		highlight_current_row = !highlight_current_row;
		status_text_flash("Row hilight %s", (highlight_current_row ? "enabled" : "disabled"));
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_j:
		if (k->state) return 1;
		fast_volume_toggle();
		return 1;
	case SDLK_u:
		if (k->state) return 1;
		if (fast_volume_mode)
			selection_vary(1, 100-fast_volume_percent, 'M');
		else
			vary_command('M');
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_y:
		if (k->state) return 1;
		if (fast_volume_mode)
			selection_vary(1, 100-fast_volume_percent, 'Y');
		else
			vary_command('Y');
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_k:
		if (k->state) return 1;
		if (fast_volume_mode)
			selection_vary(1, 100-fast_volume_percent,
					get_effect_char(current_effect()));
		else
			vary_command(get_effect_char(current_effect()));
		status.flags |= NEED_UPDATE;
		return 1;

	case SDLK_v:
		if (k->state) return 1;
		show_default_volumes = !show_default_volumes;
		status_text_flash("Default volumes %s", (show_default_volumes ? "enabled" : "disabled"));
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_x:
	case SDLK_z:
		if (k->state) return 1;
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
		if (k->state) return 1;
		pattern_editor_display_history();
		return 1;
	default:
		return 0;
	}

	return 0;
}

static int mute_toggle_hack[64]; /* mrsbrisby: please explain this one, i don't get why it's necessary... */
static int pattern_editor_handle_key(struct key_event * k)
{
	int n, nx, v;
	int total_rows = song_get_rows_in_pattern(current_pattern);
	const struct track_view *track_view;
	int np, nr, nc;
	unsigned int basex;

	if (k->mouse) {
		if (k->state) {
			/* mouseup */
			memset(mute_toggle_hack, 0, sizeof(mute_toggle_hack));
		}

		if ((k->mouse == MOUSE_CLICK || k->mouse == MOUSE_DBLCLICK) && k->state) {
			shift_selection_end();
		}
			
		if (k->y < 13 && !shift_selection.in_progress) return 0;

		if (k->y >= 15 && k->mouse != MOUSE_CLICK && k->mouse != MOUSE_DBLCLICK) {
			if (k->state) return 0;
			if (k->mouse == MOUSE_SCROLL_UP) {
				if (top_display_row > 0) {
					top_display_row--;
					if (current_row > top_display_row + 31)
						current_row = top_display_row + 31;
					if (current_row < 0)
						current_row = 0;
					return -1;
				}
			} else if (k->mouse == MOUSE_SCROLL_DOWN) {
				if (top_display_row + 31 < total_rows) {
					top_display_row++;
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
			if (((n == top_display_channel && shift_selection.in_progress) || k->x >= basex) && ((n == visible_channels && shift_selection.in_progress) || k->x < basex + track_view->width)) {
				if (!shift_selection.in_progress && (k->y == 14 || k->y == 13)) {
					if (!k->state) {
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

		if (!k->state && k->sy > 14) {
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

	switch (k->sym) {
	case SDLK_l:
		if (status.flags & CLASSIC_MODE) return 0;
		if (k->state) return 1;
		clipboard_copy(1);
		break;

	case SDLK_UP:
		if (k->state) return 0;
		channel_snap_back = -1;
		if (skip_value)
			current_row -= skip_value;
		else
			current_row--;
		return -1;
	case SDLK_DOWN:
		if (k->state) return 0;
		channel_snap_back = -1;
		if (skip_value)
			current_row += skip_value;
		else
			current_row++;
		return -1;
	case SDLK_LEFT:
		if (k->state) return 0;
		channel_snap_back = -1;
		if (k->mod & KMOD_SHIFT)
			current_channel--;
		else
			current_position--;
		return -1;
	case SDLK_RIGHT:
		if (k->state) return 0;
		channel_snap_back = -1;
		if (k->mod & KMOD_SHIFT)
			current_channel++;
		else
			current_position++;
		return -1;
	case SDLK_TAB:
		if (k->state) return 0;
		channel_snap_back = -1;
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
		if (k->state) return 0;
		channel_snap_back = -1;
		if (current_row == total_rows)
			current_row++;
		current_row -= row_highlight_major;
		return -1;
	case SDLK_PAGEDOWN:
		if (k->state) return 0;
		channel_snap_back = -1;
		current_row += row_highlight_major;
		return -1;
	case SDLK_HOME:
		if (k->state) return 0;
		channel_snap_back = -1;
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
		if (k->state) return 0;
		channel_snap_back = -1;
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
		if (k->state) return 0;
		channel_snap_back = -1;
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
		if (k->state) return 0;
		channel_snap_back = -1;
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
		if (k->state) return 0;
		channel_snap_back = -1;

		if (playback_tracing) {
			switch (song_get_mode()) {
			case MODE_PATTERN_LOOP:
				return 1;
			case MODE_PLAYING:
				prev_order_pattern();
				fix_pb_trace();
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
		if (k->state) return 0;
		channel_snap_back = -1;

		if (playback_tracing) {
			switch (song_get_mode()) {
			case MODE_PATTERN_LOOP:
				return 1;
			case MODE_PLAYING:
				next_order_pattern();
				fix_pb_trace();
				return 1;
			default:
				break;
			};
		}

		if ((k->mod & KMOD_SHIFT) && k->orig_sym == SDLK_KP_PLUS)
			set_current_pattern(current_pattern + 4);
		else
			set_current_pattern(current_pattern + 1);
		return 1;
	case SDLK_BACKSPACE:
		if (k->state) return 0;
		channel_snap_back = -1;
                current_channel = multichannel_get_previous (current_channel);
		if (skip_value)
			current_row -= skip_value;
		else
			current_row--;
		return -1;
	case SDLK_RETURN:
		if (k->state) return 0;
		copy_note_to_mask();
		return 1;
	default:
		/* bleah */
		if (k->sym == SDLK_LSHIFT || k->sym == SDLK_RSHIFT) {
			if (!k->state && shift_selection.in_progress) {
				shift_selection_end();
			}
		}

		if (k->sym == SDLK_LESS || k->sym == SDLK_COLON || k->sym == SDLK_SEMICOLON) {
			if (k->state) return 0;
			if ((status.flags & CLASSIC_MODE)
			|| current_position != 4) {
				set_previous_instrument();
				status.flags |= NEED_UPDATE;
				return 1;
			}
			/* fall through */
		} else if (k->sym == SDLK_GREATER || k->sym == SDLK_QUOTE || k->sym == SDLK_QUOTEDBL) {
			if (k->state) return 0;
			if ((status.flags & CLASSIC_MODE)
			|| current_position != 4) {
				set_next_instrument();
				status.flags |= NEED_UPDATE;
				return 1;
			}
			/* fall through */
		} else if (k->sym == SDLK_COMMA) {
			if (k->state) return 0;
			if (current_position > 1) {
				edit_copy_mask ^= (1 << edit_pos_to_copy_mask[current_position]);
				status.flags |= NEED_UPDATE;
			}
			return 1;
		}
		if (song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP)
		&& playback_tracing && k->is_repeat)
			return 0;

		if (!pattern_editor_insert(k))
			return 0;
		return -1;
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
	char buf[4];
	int ret;
	int total_rows = song_get_rows_in_pattern(current_pattern);

#if 0
	/* this is fun; if we're playback tracing, hold shift to "pause the current position"
	 * erm... why? you could just turn tracing off with scroll lock and
	   accomplish the same effect. -storlek */
	if (k->sym == SDLK_LSHIFT || k->sym == SDLK_RSHIFT) {
		if (k->state) {
			if (tracing_was_playing) {
				midi_playback_tracing = playback_tracing = 1;
				song_start_at_pattern(current_pattern, tracing_was_playing-1);
				tracing_was_playing = 0;
			}
		} else if ((song_get_mode() & (MODE_PLAYING|MODE_PATTERN_LOOP)) && playback_tracing) {
			tracing_was_playing = current_row + 1;
			midi_playback_tracing = playback_tracing = 0;
			song_single_step(current_pattern, current_row);
		}
	}
#endif

	if (k->mod & KMOD_SHIFT) {
		if (k->state) return 0;
		if (!shift_selection.in_progress)
			shift_selection_begin();
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
int pattern_max_channels(int patno, int opt_bits[64])
{
	song_note *pattern;
	song_instrument *pi;
	song_channel *pc;
	song_sample *ps;
	int total_rows;
	int x, n, inst, samp;
	int count[64];
	int mlim;

	for (n = 0; n < 64; n++) count[n] = 0;

	mlim = -1;
	total_rows = song_get_pattern(patno, &pattern);
	while (total_rows > 0) {
		for (n = 0; n < 64; n++) {
#define MSTEREO_HIT (count[n]=255)
#define MLIM_HIT (count[n]++)

			pc = song_get_channel(n);
			if (!pc || (pc->flags & CHN_MUTE)) {
				pattern++;
				continue;
			}

			if (pc->flags & CHN_SURROUND) MSTEREO_HIT;

			if (pattern->note) {
				if (pc->panning != 128) MLIM_HIT;
			}

			switch (pattern->volume_effect) {
			case VOL_EFFECT_PANNING:
			case VOL_EFFECT_PANSLIDELEFT:
			case VOL_EFFECT_PANSLIDERIGHT:
				if (pattern->volume) MLIM_HIT;
			};
			switch (get_effect_char(pattern->effect)) {
			case 'X':
			case 'Y':
			case 'P':
				MLIM_HIT;
				break;
			case 'S':
				if (pattern->parameter == 0x91)
					MSTEREO_HIT;

				if (pattern->parameter >= 0x80
				&& pattern->parameter < 0x90)
					MLIM_HIT;
			};

			x = pattern->note;
			inst = pattern->instrument;
			pattern++;
			if (inst == 0) continue;

			if (song_is_instrument_mode()) {
				pi = song_get_instrument(inst, NULL);
				if (!pi) continue;

				samp = pi->sample_map[x & 127];
				if (samp == 0) continue;
				if (pi->flags & ENV_PANNING) MSTEREO_HIT;
				if (pi->flags & ENV_SETPANNING) MLIM_HIT;
				if (pi->pan_swing > 0) MSTEREO_HIT;
				if (pi->pitch_pan_separation != 0) MSTEREO_HIT;
			} else {
				samp = inst;
			}

			ps = song_get_sample(samp, NULL);
			if (!ps) continue;
			if (ps->flags & SAMP_STEREO) MSTEREO_HIT;
			if (ps->flags & SAMP_PANNING) MLIM_HIT;
		}
		total_rows--;
	}
#undef MLIM_HIT
#undef MSTEREO_HIT
	x = 0;
	for (n = 0; n < 64; n++) {
		switch (count[n]) {
		case 0: break;
		default:
			if (opt_bits) opt_bits[n] = 0;
		case 1: x++;
		};
	}
	if (x < 2) return 1;
	return 2;
}

/* --------------------------------------------------------------------- */

static void _fix_keyhack(void)
{
	int i;
	for (i = 0; i < 64; i++) {
		channel_keyhack[i] = -1;
	}
}
static int _fix_f7(struct key_event *k)
{
	if (k->sym == SDLK_F7) {
		if (!NO_MODIFIER(k->mod)) return 0;
		if (k->state) return 1;
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
	for (i = 0; i < 64; i++) {
		channel_quick[i] = 1;
		channel_keyhack[i] = -1;
	}
	page->title = "Pattern Editor (F2)";
	page->playback_update = pattern_editor_playback_update;
	page->song_changed_cb = pated_history_clear;
	page->song_mode_changed_cb = _fix_keyhack;
	page->pre_handle_key = _fix_f7;
	page->total_widgets = 1;
	page->clipboard_paste = pattern_selection_system_paste;
	page->widgets = widgets_pattern;
	page->help_index = HELP_PATTERN_EDITOR;

	create_other(widgets_pattern + 0, 0, pattern_editor_handle_key_cb, pattern_editor_redraw);
}

