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
 * items on each page, etc. Since this information isn't useful outside
 * page*.c, it's not in the main header. */

#ifndef PAGE_H
#define PAGE_H

/* --------------------------------------------------------------------- */
/* there's a value in this enum for each kind of item... */

enum item_type {
        ITEM_TOGGLE, ITEM_MENUTOGGLE,
        ITEM_BUTTON, ITEM_TOGGLEBUTTON,
        ITEM_TEXTENTRY,
        ITEM_NUMENTRY, ITEM_THUMBBAR, ITEM_PANBAR,
        /* this last one is for anything that doesn't fit some standard
         * type, like the sample list, envelope editor, etc.; an item of
         * this type is just a placeholder so page.c knows there's
         * something going on. */
        ITEM_OTHER      /* sample list, envelopes, etc. */
};

/* --------------------------------------------------------------------- */
/* every item in the enum has a corresponding struct here. the notes
 * before each item indicate what keypresses are trapped in page.c for
 * it, and what happens.
 * note that all item types (except ITEM_OTHER) trap the enter key for the
 * activate callback. */

/* space -> state changed; cb triggered */
struct item_toggle {
        int state;      /* 0 = off, 1 = on */
};

/* space -> state changed; cb triggered */
/* FIXME: think of a better name for this one */
struct item_menutoggle {
        int state;      /* 0, 1, ..., num_choices - 1, num_choices */
        const char **choices;
        int num_choices;
};

/* enter -> cb triggered */
struct item_button {
        const char *text;
        int padding;
};

/* enter -> state changed; cb triggered */
struct item_togglebutton {
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
 * - cursor_pos is set to the end of the text when the item is focused */
struct item_textentry {
        char *text;
        int max_length;
        int firstchar;  /* first visible character (generally 0) */
        int cursor_pos; /* 0 = first character */
};

/* <0-9> -> digit @ cursor_pos changed; cursor_pos increased; cb triggered
 * left/right -> cursor_pos changed; cb NOT triggered.
 * +/- -> value increased/decreased; cb triggered
 * the width for this one MUST be 3 or 7.
 * cursor_pos for this item is a pointer so that multiple numbers that
 * are all lined up can share the same position. */
struct item_numentry {
        int min;
        int max;
        int value;
        int *cursor_pos;
};

/* left/right -> value changed; cb triggered
 * ctrl-left/right -> value changed 4x; cb triggered
 * shift-left/right -> value changed 2x; cb triggered
 * home/end -> value set to min/max; cb triggered
 * <0-9> -> prompt for new number; value changed; cb triggered */
struct item_thumbbar {
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
struct item_panbar {
        int min;
        int max;
        int value;
        int channel;
        int muted:1;
        int surround:1;
};

struct item_other {
        /* bah. can't do much of anything with this.
         * 
         * if an 'other' type item gets the focus, it soaks up all the
         * keyboard events that the main handler doesn't catch. thus
         * it is responsible for changing the focus to something else
         * (and, of course, if it doesn't ever do that, the cursor is
         * pretty much stuck)
         * this MUST be set to a valid function.
         * return value is 1 if the key was handled, 0 if not. */
        int (*handle_key) (SDL_keysym * k);

        /* also the item drawing function can't possibly know how to
         * draw a custom item, so it calls this instead.
         * this MUST be set to a valid function. */
        void (*redraw) (void);
};

/* --------------------------------------------------------------------- */
/* and all the item structs go in the union in this struct... */

struct item {
        enum item_type type;
        union {
                struct item_toggle toggle;
                struct item_menutoggle menutoggle;
                struct item_button button;
                struct item_togglebutton togglebutton;
                struct item_textentry textentry;
                struct item_numentry numentry;
                struct item_thumbbar thumbbar;
                struct item_panbar panbar;
                struct item_other other;
        };

        /* for redrawing */
        int x, y, width;

        /* these next 5 fields specify what item gets selected next */
        struct {
                int up, down, left, right, tab;
        } next;

        /* called whenever the value is changed... duh ;) */
        void (*changed) (void);
	
	/* called when the enter key is pressed */
        void (*activate) (void);
};

/* this structure keeps all the information needed to draw a page, and a
 * list of all the different items on the page. it's the job of the page
 * to change the necessary information when something changes; that's
 * done in the page's draw and update functions.
 *
 * everything in this struct MUST be set for each page.
 * functions that aren't implemented should be set to NULL. */
struct page {
        /* the title of the page, eg "Sample List (F3)" */
        const char *title;
        /* draw the labels, etc. that don't change */
        void (*draw_const) (void);
        /* called after the song is changed. this is to copy the new
         * values from the song to the items on the page. */
        void (*song_changed_cb) (void);
        /* called before items are drawn, mostly to fix the item values
         * (for example, on the sample page this sets everything to
         * whatever values the current sample has) - this is a lousy
         * hack. sorry. :P */
        void (*predraw_hook) (void);
        /* draw the parts of the page that change when the song is playing
         * (this is called *very* frequently) */
        void (*playback_update) (void);
        /* this catches any keys that the main handler doesn't deal with */
        void (*handle_key) (SDL_keysym * k);
        /* called when the page is set. this is for reloading the
         * directory in the file browsers. */
        void (*set_page) (void);

        struct item *items;
        int selected_item;
        int total_items;

        /* 0 if no page-specific help */
        int help_index;
};

/* --------------------------------------------------------------------- */

extern struct page pages[];

/* these are updated to point to the relevant data in the selected page
 * (or the dialog, if one is active) */
extern struct item *items;
extern int *selected_item;
extern int *total_items;

/* to make it easier to deal with either the page's items or the
 * current dialog's:
 * 
 * ACTIVE_ITEM deals with whatever item is *really* active.
 * ACTIVE_PAGE_ITEM references the *page's* idea of what's active.
 *     (these are different if there's a dialog) */
#define ACTIVE_PAGE      (pages[status.current_page])
#define ACTIVE_ITEM      (items[*selected_item])
#define ACTIVE_PAGE_ITEM (ACTIVE_PAGE.items[ACTIVE_PAGE.selected_item])

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
        PAGE_SETTINGS = (7),

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
};

/* --------------------------------------------------------------------- */

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
void settings_load_page(struct page *page);
void load_module_load_page(struct page *page);
void save_module_load_page(struct page *page);
void orderpan_load_page(struct page *page);
void ordervol_load_page(struct page *page);
void song_vars_load_page(struct page *page);
void message_load_page(struct page *page);
void palette_load_page(struct page *page);
void log_load_page(struct page *page);
void load_sample_load_page(struct page *page);

/* --------------------------------------------------------------------- */

void create_toggle(struct item *i, int x, int y, int next_up,
                   int next_down, int next_left, int next_right,
                   int next_tab, void (*changed) (void));
void create_menutoggle(struct item *i, int x, int y, int next_up,
                       int next_down, int next_left, int next_right,
                       int next_tab, void (*changed) (void),
                       const char **choices);
void create_button(struct item *i, int x, int y, int width, int next_up,
                   int next_down, int next_left, int next_right,
                   int next_tab, void (*changed) (void), const char *text,
                   int padding);
void create_togglebutton(struct item *i, int x, int y, int width,
                         int next_up, int next_down, int next_left,
                         int next_right, int next_tab,
                         void (*changed) (void), const char *text,
                         int padding, int *group);
void create_textentry(struct item *i, int x, int y, int width, int next_up,
                      int next_down, int next_tab, void (*changed) (void),
		      char *text, int max_length);
void create_numentry(struct item *i, int x, int y, int width, int next_up,
                     int next_down, int next_tab, void (*changed) (void),
                     int min, int max, int *cursor_pos);
void create_thumbbar(struct item *i, int x, int y, int width, int next_up,
                     int next_down, int next_tab, void (*changed) (void),
                     int min, int max);
void create_panbar(struct item *i, int x, int y, int next_up,
                   int next_down, int next_tab, void (*changed) (void),
                   int channel);
void create_other(struct item *i, int next_tab, int (*i_handle_key) (SDL_keysym * k), void (*i_redraw) (void));

/* --------------------------------------------------------------------- */

/* item.c */
int textentry_add_char(struct item *item, Uint16 unicode);
void numentry_change_value(struct item *item, int new_value);
int numentry_handle_digit(struct item *item, Uint16 unicode);
void change_focus_to(int new_item_index);
/* p_items should point to the group of items (not the actual item that is
 * being set!) and item should be the index of the item within the group. */
void togglebutton_set(struct item *p_items, int item, int do_callback);
void draw_item(struct item *i, int selected);

/* item-keyhandler.c
 * [note: this always uses the current item] */
int item_handle_key(SDL_keysym * k);

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

/* page_orderpan.c */
void update_current_order(void);

/* menu.c */
void menu_show(void);
void menu_hide(void);
void menu_draw(void);
int menu_handle_key(SDL_keysym * k);

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

        struct item *items;     /* malloc'ed */
        int selected_item;
        int total_items;

        void (*draw_const) (void);
        int (*handle_key) (SDL_keysym * k);

        /* there's no action_ok, as yes and ok are fundamentally the same */
        void (*action_yes) (void);
        void (*action_no) (void);       /* only useful for y/n dialogs? */
	/* currently, this is only settable for custom dialogs.
	 * it's only used in a couple of places (mostly on the pattern editor) */
        void (*action_cancel) (void);
};

/* dialog handlers
 * these are set by default for normal dialogs, and can be used with the custom dialogs.
 * they call the {yes, no, cancel} callback, destroy the dialog, and schedule a screen
 * update. (note: connect these to the BUTTONS, not the action_* callbacks!) */
void dialog_yes(void);
void dialog_no(void);
void dialog_cancel(void);


int dialog_handle_key(SDL_keysym * k);
void dialog_draw(void);

void dialog_create(int type, const char *text, void (*action_yes) (void),
                   void (*action_no) (void), int default_item);

void dialog_destroy(void);
void dialog_destroy_all(void);

/* this builds and displays a dialog with an unspecified item structure.
 * the caller can set other properties of the dialog (i.e. the yes/no/cancel callbacks) after
 * the dialog has been displayed. */
struct dialog *dialog_create_custom(int x, int y, int w, int h, struct item *dialog_items,
				    int dialog_total_items, int dialog_selected_item,
				    void (*draw_const) (void));

#endif /* ! PAGE_H */
