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

/* This header has all the page definitions, the kinds of interactive
 * widgets on each page, etc. Since this information isn't useful outside
 * page*.c, it's not in the main header. */

#ifndef SCHISM_PAGE_H_
#define SCHISM_PAGE_H_

/* How much to scroll. */
#define MOUSE_SCROLL_LINES       3

/* --------------------------------------------------------------------- */
/* help text */

/* NOTE: this enum should be in the same order as helptexts in Makefile.am */
enum {
	HELP_GLOBAL, /* needs to be first! */
	HELP_COPYRIGHT,
	HELP_INFO_PAGE,
	HELP_INSTRUMENT_LIST,
	HELP_MESSAGE_EDITOR,
	HELP_MIDI_OUTPUT,
	HELP_ORDERLIST_PANNING,
	HELP_ORDERLIST_VOLUME,
	HELP_PATTERN_EDITOR,
	HELP_ADLIB_SAMPLE,
	HELP_SAMPLE_LIST,
	HELP_PALETTES,
	HELP_TIME_INFORMATION,

	HELP_NUM_ITEMS /* needs to be last! */
};

extern const char *help_text[HELP_NUM_ITEMS];

/* --------------------------------------------------------------------- */
/* there's a value in this enum for each kind of widget... */

enum widget_type {
	WIDGET_TOGGLE, WIDGET_MENUTOGGLE,
	WIDGET_BUTTON, WIDGET_TOGGLEBUTTON,
	WIDGET_TEXTENTRY,
	WIDGET_NUMENTRY, WIDGET_THUMBBAR, WIDGET_BITSET, WIDGET_PANBAR,
	/* this last one is for anything that doesn't fit some standard
	type, like the sample list, envelope editor, etc.; a widget of
	this type is just a placeholder so page.c knows there's
	something going on. */
	WIDGET_OTHER      /* sample list, envelopes, etc. */
};

/* --------------------------------------------------------------------- */
/* every widget in the enum has a corresponding struct here. the notes
 * before each widget indicate what keypresses are trapped in page.c for
 * it, and what happens.
 * note that all widget types (except WIDGET_OTHER) trap the enter key for the
 * activate callback. */

/* space -> state changed; cb triggered */
struct widget_toggle {
	int state;      /* 0 = off, 1 = on */
};

/* space -> state changed; cb triggered */
struct widget_menutoggle {
	int state;      /* 0, 1, ..., num_choices - 1, num_choices */
	const char *const *choices;
	int num_choices;
	const char *activation_keys;
};

/* enter -> cb triggered */
struct widget_button {
	const char *text;
	int padding;
};

/* enter -> state changed; cb triggered */
struct widget_togglebutton {
	const char *text;
	int padding;
	int state;      /* 0 = off, 1 = on */
	const int *group;
};

/* backspace -> truncated; changed cb triggered
 * ctrl-bs -> cleared; changed cb triggered
 * <any ascii char> -> appended; changed cb triggered
 * (the callback isn't triggered unless something really changed)
 * left/right -> cursor_pos changed; no cb triggered
 *
 * - if (max_length > (width - 1)) the text scrolls
 * - cursor_pos is set to the end of the text when the widget is focused */
struct widget_textentry {
	char *text;
	int max_length;
	int firstchar;  /* first visible character (generally 0) */
	int cursor_pos; /* 0 = first character */
};

/* <0-9> -> digit @ cursor_pos changed; cursor_pos increased; cb triggered
 * left/right -> cursor_pos changed; cb NOT triggered.
 * +/- -> value increased/decreased; cb triggered
 * cursor_pos for this widget is a pointer so that multiple numbers that
 * are all lined up can share the same position. */
struct widget_numentry {
	int min;
	int max;
	int value;
	int *cursor_pos;
	int (*handle_unknown_key)(struct key_event *k);
	int reverse;
};

/* left/right -> value changed; cb triggered
 * ctrl-left/right -> value changed 4x; cb triggered
 * shift-left/right -> value changed 2x; cb triggered
 * home/end -> value set to min/max; cb triggered
 * <0-9> -> prompt for new number; value changed; cb triggered */
struct widget_thumbbar {
	/* pretty much the same as the numentry, just without the cursor
	 * position field... (NOTE - don't rearrange the order of the
	 * fields in either of these; some code depends on them being
	 * the same) */
	int min;
	int max;
	int value;
	/* this is currently only used with the midi thumbbars on the ins. list + pitch page. if
	 * this is non-NULL, and value == {min,max}, the text is drawn instead of the thumbbar. */
	const char *text_at_min, *text_at_max;
};

struct widget_bitset {
	/* A widget for controlling individual bits */
	int nbits;
	int value;
	int *cursor_pos;
	const char* bits_on;
	const char* bits_off;
	const char* activation_keys;
};


/* special case of the thumbbar; range goes from 0 to 64. if mute is
 * set, the bar is replaced with the word "muted"; if surround is set,
 * it's replaced with the word "surround". some keys: L/M/R set the
 * value to 0/32/64 respectively, S switches the surround flag, and
 * space toggles the mute flag (and selects the next.down control!)
 * min/max are just for thumbbar compatibility, and are always set to
 * 0 and 64 respectively.
 * note that, due to some weirdness with IT, these draw the channel text
 * as well as the actual bar. */
struct widget_panbar {
	int min;
	int max;
	int value;
	int channel;
	unsigned int muted:1;
	unsigned int surround:1;
};

struct widget_other {
	/* bah. can't do much of anything with this.
	 *
	 * if an 'other' type widget gets the focus, it soaks up all the
	 * keyboard events that the main handler doesn't catch. thus
	 * it is responsible for changing the focus to something else
	 * (and, of course, if it doesn't ever do that, the cursor is
	 * pretty much stuck)
	 * this MUST be set to a valid function.
	 * return value is 1 if the key was handled, 0 if not. */
	int (*handle_key) (struct key_event * k);
	int (*handle_text_input) (const char* text_input);

	/* also the widget drawing function can't possibly know how to
	 * draw a custom widget, so it calls this instead.
	 * this MUST be set to a valid function. */
	void (*redraw) (void);
};

/* --------------------------------------------------------------------- */
/* and all the widget structs go in the union in this struct... */

union _widget_data_union {
	struct widget_toggle toggle;
	struct widget_menutoggle menutoggle;
	struct widget_button button;
	struct widget_togglebutton togglebutton;
	struct widget_textentry textentry;
	struct widget_numentry numentry;
	struct widget_thumbbar thumbbar;
	struct widget_panbar panbar;
	struct widget_other other;
	struct widget_bitset bitset;
};

struct widget {
	enum widget_type type;

	union _widget_data_union d;

	/* for redrawing */
	int x, y, width, height, depressed;
	int clip_start, clip_end;

	/* these next 5 fields specify what widget gets selected next */
	struct {
		int up, down, left, right, tab, backtab;
	} next;

	/* called whenever the value is changed... duh ;) */
	void (*changed) (void);

	/* called when the enter key is pressed */
	void (*activate) (void);

	/* called by the clipboard manager; really, only "other" widgets
	should "override" this... */
	int (*clipboard_paste)(int cb, const void *cptr);

	/* true if the widget accepts "text"- used for digraphs and unicode
	and alt+kp entry... */
	int accept_text;
};

/* this structure keeps all the information needed to draw a page, and a
 * list of all the different widgets on the page. it's the job of the page
 * to change the necessary information when something changes; that's
 * done in the page's draw and update functions.
 *
 * everything in this struct MUST be set for each page.
 * functions that aren't implemented should be set to NULL. */
struct page {
	/* the title of the page, eg "Sample List (F3)" */
	const char *title;

	/* font editor takes over full screen */
	void (*draw_full)(void);
	/* draw the labels, etc. that don't change */
	void (*draw_const) (void);
	/* called after the song is changed. this is to copy the new
	 * values from the song to the widgets on the page. */
	void (*song_changed_cb) (void);
	/* called before widgets are drawn, mostly to fix the values
	 * (for example, on the sample page this sets everything to
	 * whatever values the current sample has) - this is a lousy
	 * hack. sorry. :P */
	void (*predraw_hook) (void);
	/* draw the parts of the page that change when the song is playing
	 * (this is called *very* frequently) */
	void (*playback_update) (void);
	/* this gets first shot at keys (to do unnatural overrides) */
	int (*pre_handle_key) (struct key_event * k);
	/* this catches any keys that the main handler doesn't deal with */
	void (*handle_key) (struct key_event * k);
	/* handle any text input events from SDL */
	void (*handle_text_input) (const char* text_input);
	/* called when the page is set. this is for reloading the
	 * directory in the file browsers. */
	void (*set_page) (void);

	/* called when the song-mode changes */
	void (*song_mode_changed_cb) (void);

	/* called by the clipboard manager */
	int (*clipboard_paste)(int cb, const void *cptr);

	struct widget *widgets;
	int selected_widget;
	int total_widgets;

	/* 0 if no page-specific help */
	int help_index;
};

/* --------------------------------------------------------------------- */

extern struct page pages[];

/* these are updated to point to the relevant data in the selected page
 * (or the dialog, if one is active) */
extern struct widget *widgets;
extern int *selected_widget;
extern int *total_widgets;

/* to make it easier to deal with either the page's widgets or the
 * current dialog's:
 *
 * ACTIVE_WIDGET deals with whatever widget is *really* active.
 * ACTIVE_PAGE_WIDGET references the *page's* idea of what's active.
 *     (these are different if there's a dialog) */
#define ACTIVE_PAGE        (pages[status.current_page])
#define ACTIVE_WIDGET      (widgets[*selected_widget])
#define ACTIVE_PAGE_WIDGET (ACTIVE_PAGE.widgets[ACTIVE_PAGE.selected_widget])

extern int instrument_list_subpage;
#define PAGE_INSTRUMENT_LIST instrument_list_subpage

/* --------------------------------------------------------------------- */

enum page_numbers {
	PAGE_BLANK,
	PAGE_HELP,
	PAGE_ABOUT,
	PAGE_LOG,

	PAGE_PATTERN_EDITOR,
	PAGE_SAMPLE_LIST,
	// PAGE_INSTRUMENT_LIST doesn't exist
	PAGE_INFO,

	PAGE_CONFIG,
	PAGE_PREFERENCES,

	PAGE_MIDI,
	PAGE_MIDI_OUTPUT,

	PAGE_LOAD_MODULE,
	PAGE_SAVE_MODULE,
	PAGE_EXPORT_MODULE,

	PAGE_ORDERLIST_PANNING,
	PAGE_ORDERLIST_VOLUMES,

	PAGE_SONG_VARIABLES,
	PAGE_MESSAGE,

	PAGE_TIME_INFORMATION,

	/* don't use these directly with set_page */
	PAGE_INSTRUMENT_LIST_GENERAL,
	PAGE_INSTRUMENT_LIST_VOLUME,
	PAGE_INSTRUMENT_LIST_PANNING,
	PAGE_INSTRUMENT_LIST_PITCH,

	PAGE_LOAD_SAMPLE,
	PAGE_LIBRARY_SAMPLE,
	PAGE_LOAD_INSTRUMENT,
	PAGE_LIBRARY_INSTRUMENT,

	PAGE_PALETTE_EDITOR,
	PAGE_FONT_EDIT,

	PAGE_WATERFALL,

	PAGE_MAX
};

/* --------------------------------------------------------------------- */
void show_about(void);

void blank_load_page(struct page *page);
void help_load_page(struct page *page);
void pattern_editor_load_page(struct page *page);
void sample_list_load_page(struct page *page);
void instrument_list_general_load_page(struct page *page);
void instrument_list_volume_load_page(struct page *page);
void instrument_list_panning_load_page(struct page *page);
void instrument_list_pitch_load_page(struct page *page);
void info_load_page(struct page *page);
void midi_load_page(struct page *page);
void midiout_load_page(struct page *page);
void fontedit_load_page(struct page *page);
void preferences_load_page(struct page *page);
void load_module_load_page(struct page *page);
void save_module_load_page(struct page *page, int do_export);
void orderpan_load_page(struct page *page);
void ordervol_load_page(struct page *page);
void song_vars_load_page(struct page *page);
void message_load_page(struct page *page);
void palette_load_page(struct page *page);
void log_load_page(struct page *page);
void load_sample_load_page(struct page *page);
void load_instrument_load_page(struct page *page);
void about_load_page(struct page *page);
void library_sample_load_page(struct page *page);
void library_instrument_load_page(struct page *page);
void config_load_page(struct page *page);
void waterfall_load_page(struct page *page);
void timeinfo_load_page(struct page *page);

/* --------------------------------------------------------------------- */

/* draw-misc.c */
void draw_thumb_bar(int x, int y, int width, int min, int max, int val,
		    int selected);
/* vu meter values should range from 0 to 64. the color is generally 5
 * unless the channel is disabled (in which case it's 1). impulse tracker
 * doesn't do peak color; st3 style, use color 4 (unless it's disabled,
 * in which case it should probably be 2, or maybe 3).
 * the width should be a multiple of three. */
void draw_vu_meter(int x, int y, int val, int width, int color, int peak_color);

/* page.c */
int page_is_instrument_list(int page);
void update_current_instrument(void);

void new_song_dialog(void);
void save_song_or_save_as(void);

// support for the song length dialog
void show_length_dialog(const char *label, unsigned int length);

/* page_patedit.c */
void update_current_row(void);
void update_current_pattern(void);
void pattern_editor_display_options(void);
void pattern_editor_length_edit(void);
int pattern_max_channels(int patno, int opt_bits[64]);

/* page_orderpan.c */
void update_current_order(void);

/* menu.c */
void menu_show(void);
void menu_hide(void);
void menu_draw(void);
int menu_handle_key(struct key_event * k);

/* status.c */
void status_text_redraw(void);

// page_message.c
void message_reset_selection(void);

/* --------------------------------------------------------------------- */
/* Other UI prompt stuff. */

/* Ask for a value, like the thumbbars. */
void numprompt_create(const char *prompt, void (*finish)(int n), char initvalue);

/* Ask for a sample / instrument number, like the "swap sample" dialog. */
void smpprompt_create(const char *title, const char *prompt, void (*finish)(int n));

#endif /* SCHISM_PAGE_H_ */
