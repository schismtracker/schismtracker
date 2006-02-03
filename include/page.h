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

/* This header has all the page definitions, the kinds of interactive
 * widgets on each page, etc. Since this information isn't useful outside
 * page*.c, it's not in the main header. */

#ifndef PAGE_H
#define PAGE_H

/* there's no good place for this */
#define MOUSE_BUTTON_LEFT	1
#define MOUSE_BUTTON_MIDDLE	2
#define MOUSE_BUTTON_RIGHT	3
#define MOUSE_CLICK		1
#define MOUSE_SCROLL_UP		2
#define MOUSE_SCROLL_DOWN	3
#define MOUSE_DBLCLICK		4
struct key_event {
	SDLKey sym, orig_sym;
	SDLMod mod;
	Uint16 unicode;

	int state; /* 0 for down, 1 for up/release */
	int mouse; /* 0 for none, 1 for click, 2 for scrollup, 3 for down */
	int mouse_button; /* 1 left, 2 middle, 3 right (itf only) */
	int midi_note;
	int midi_channel;
	int midi_volume; /* -1 for not a midi key otherwise 0...128 */
	int midi_bend;	/* normally 0; -8192 to +8192  */
	unsigned int sx, sy; /* start x and y position (character) */
	unsigned int x, hx, fx; /* x position of mouse (character, halfcharacter, fine) */
	unsigned int y, fy; /* y position of mouse (character, fine) */

	unsigned int rx, ry; /* x/y resolution */

	int is_repeat;
	int on_target;
	int is_synthetic; /* 1 came from paste */
};

/* --------------------------------------------------------------------- */
/* there's a value in this enum for each kind of widget... */

enum widget_type {
        WIDGET_TOGGLE, WIDGET_MENUTOGGLE,
        WIDGET_BUTTON, WIDGET_TOGGLEBUTTON,
        WIDGET_TEXTENTRY,
        WIDGET_NUMENTRY, WIDGET_THUMBBAR, WIDGET_PANBAR,
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
        const char **choices;
        int num_choices;
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
        int *group;
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
        int muted:1;
        int surround:1;
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
};
struct widget {
        enum widget_type type;

	union _widget_data_union	d;

        /* for redrawing */
        int x, y, width, height, depressed;
	int clip_start, clip_end;

        /* these next 5 fields specify what widget gets selected next */
        struct {
                int up, down, left, right, tab;
        } next;

        /* called whenever the value is changed... duh ;) */
        void (*changed) (void);

	/* called when the enter key is pressed */
        void (*activate) (void);
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
        /* called when the page is set. this is for reloading the
         * directory in the file browsers. */
        void (*set_page) (void);

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
        PAGE_BLANK = (0),
        PAGE_HELP = (1),
        PAGE_PATTERN_EDITOR = (2),
        PAGE_SAMPLE_LIST = (3),
        /* PAGE_INSTRUMENT_LIST = (4),  * doesn't exist */
        PAGE_INFO = (5),

        PAGE_MIDI = (6),
        PAGE_PREFERENCES = (7),

        PAGE_LOAD_MODULE = (9),
        PAGE_SAVE_MODULE = (10),
        PAGE_ORDERLIST_PANNING = (11),
        PAGE_SONG_VARIABLES = (12),
        PAGE_PALETTE_EDITOR = (13),
        PAGE_ORDERLIST_VOLUMES = (14),
        PAGE_MESSAGE = (15),
        PAGE_LOG = (16),

        /* don't use these directly with set_page */
        PAGE_INSTRUMENT_LIST_GENERAL = (17),
        PAGE_INSTRUMENT_LIST_VOLUME = (18),
        PAGE_INSTRUMENT_LIST_PANNING = (19),
        PAGE_INSTRUMENT_LIST_PITCH = (20),
        
        PAGE_LOAD_SAMPLE = (21),
        PAGE_SAMPLE_BROWSER = (22),
        PAGE_LOAD_INSTRUMENT = (23),
        PAGE_INSTRUMENT_BROWSER = (24),

	PAGE_MIDI_OUTPUT = (25),

	PAGE_FONT_EDIT = (26),

	PAGE_LIBRARY_SAMPLE = (27),
	PAGE_LIBRARY_INSTRUMENT = (28),

	PAGE_ABOUT = (29),

	PAGE_CONFIG = (30),

/* limit =32 */
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
void save_module_load_page(struct page *page);
void orderpan_load_page(struct page *page);
void ordervol_load_page(struct page *page);
void song_vars_load_page(struct page *page);
void message_load_page(struct page *page);
void palette_load_page(struct page *page);
void log_load_page(struct page *page);
void load_sample_load_page(struct page *page);
void load_instrument_load_page(struct page *page);
void about_load_page(struct page *page);

/* --------------------------------------------------------------------- */

void create_toggle(struct widget *w, int x, int y, int next_up,
                   int next_down, int next_left, int next_right,
                   int next_tab, void (*changed) (void));
void create_menutoggle(struct widget *w, int x, int y, int next_up,
                       int next_down, int next_left, int next_right,
                       int next_tab, void (*changed) (void),
                       const char **choices);
void create_button(struct widget *w, int x, int y, int width, int next_up,
                   int next_down, int next_left, int next_right,
                   int next_tab, void (*changed) (void), const char *text,
                   int padding);
void create_togglebutton(struct widget *w, int x, int y, int width,
                         int next_up, int next_down, int next_left,
                         int next_right, int next_tab,
                         void (*changed) (void), const char *text,
                         int padding, int *group);
void create_textentry(struct widget *w, int x, int y, int width, int next_up,
                      int next_down, int next_tab, void (*changed) (void),
		      char *text, int max_length);
void create_numentry(struct widget *w, int x, int y, int width, int next_up,
                     int next_down, int next_tab, void (*changed) (void),
                     int min, int max, int *cursor_pos);
void create_thumbbar(struct widget *w, int x, int y, int width, int next_up,
                     int next_down, int next_tab, void (*changed) (void),
                     int min, int max);
void create_panbar(struct widget *w, int x, int y, int next_up,
                   int next_down, int next_tab, void (*changed) (void),
                   int channel);
void create_other(struct widget *w, int next_tab, int (*w_handle_key) (struct key_event * k), void (*w_redraw) (void));

/* --------------------------------------------------------------------- */

/* widget.c */
int textentry_add_char(struct widget *widget, Uint16 unicode);
void numentry_change_value(struct widget *widget, int new_value);
int numentry_handle_digit(struct widget *widget, struct key_event *k);
struct widget *find_widget_xy(int x, int y);
struct widget *find_widget_xy_ex(int x, int y, int *num);
int change_focus_to_xy(int x, int y);
void change_focus_to(int new_widget_index);
/* p_widgets should point to the group of widgets (not the actual widget that is
 * being set!) and widget should be the index of the widget within the group. */
void togglebutton_set(struct widget *p_widgets, int widget, int do_callback);
void draw_widget(struct widget *w, int selected);

/* widget-keyhandler.c
 * [note: this always uses the current widget] */
int widget_handle_key(struct key_event * k);

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

/* page_patedit.c */
void update_current_row(void);
void update_current_pattern(void);
void pattern_editor_display_options(void);
void pattern_editor_length_edit(void);

/* page_orderpan.c */
void update_current_order(void);

/* menu.c */
void menu_show(void);
void menu_hide(void);
void menu_draw(void);
int menu_handle_key(struct key_event * k);

/* status.c */
void status_text_redraw(void);

/* --------------------------------------------------------------------- */
/* dialog crap */

struct dialog {
        int type;
        int x, y, w, h;

        /* next two are for "simple" dialogs (type != DIALOG_CUSTOM) */
        char *text;     /* malloc'ed */
        int text_x;

        struct widget *widgets;     /* malloc'ed */
        int selected_widget;
        int total_widgets;

	void *data; /* extra data pointer */

	/* maybe these should get the data pointer as well? */
        void (*draw_const) (void);
        int (*handle_key) (struct key_event * k);

        /* there's no action_ok, as yes and ok are fundamentally the same */
        void (*action_yes) (void *data);
        void (*action_no) (void *data); /* only useful for y/n dialogs? */
	/* currently, this is only settable for custom dialogs.
	 * it's only used in a couple of places (mostly on the pattern editor) */
        void (*action_cancel) (void *data);
};

/* dialog handlers
 * these are set by default for normal dialogs, and can be used with the custom dialogs.
 * they call the {yes, no, cancel} callback, destroy the dialog, and schedule a screen
 * update. (note: connect these to the BUTTONS, not the action_* callbacks!) */
void dialog_yes(void *data);
void dialog_no(void *data);
void dialog_cancel(void *data);
/* these are the same as dialog_yes(NULL) etc., and are used in button callbacks */
void dialog_yes_NULL(void);
void dialog_no_NULL(void);
void dialog_cancel_NULL(void);

int dialog_handle_key(struct key_event * k);
void dialog_draw(void);

struct dialog *dialog_create(int type, const char *text, void (*action_yes) (void *data),
                   void (*action_no) (void *data), int default_widget, void *data);

void dialog_destroy(void);
void dialog_destroy_all(void);

/* this builds and displays a dialog with an unspecified widget structure.
 * the caller can set other properties of the dialog (i.e. the yes/no/cancel callbacks) after
 * the dialog has been displayed. */
struct dialog *dialog_create_custom(int x, int y, int w, int h, struct widget *dialog_widgets,
				    int dialog_total_widgets, int dialog_selected_widget,
				    void (*draw_const) (void), void *data);

#endif /* ! PAGE_H */
