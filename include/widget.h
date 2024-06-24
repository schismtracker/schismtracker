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

#ifndef SCHISM_WIDGET_H_
#define SCHISM_WIDGET_H_

/* --------------------------------------------------------------------- */
/* there's a value in this enum for each kind of widget... */

enum widget_type {
	WIDGET_TOGGLE, WIDGET_MENUTOGGLE,
	WIDGET_BUTTON, WIDGET_TOGGLEBUTTON,
	WIDGET_TEXTENTRY, WIDGET_DYNTEXTENTRY,
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
	int firstchar;  /* first visible character (generally 0) */
	int cursor_pos; /* 0 = first character */
	int max_length; /* dynamic if dyntextentry */
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
	int (*handle_text_input) (const uint8_t* text_input);

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

/* --------------------------------------------------- */

void widget_create_toggle(struct widget *w, int x, int y, int next_up,
		   int next_down, int next_left, int next_right,
		   int next_tab, void (*changed) (void));
void widget_create_menutoggle(struct widget *w, int x, int y, int next_up,
		       int next_down, int next_left, int next_right,
		       int next_tab, void (*changed) (void),
		       const char *const *choices);
void widget_create_button(struct widget *w, int x, int y, int width, int next_up,
		   int next_down, int next_left, int next_right,
		   int next_tab, void (*changed) (void), const char *text,
		   int padding);
void widget_create_togglebutton(struct widget *w, int x, int y, int width,
			 int next_up, int next_down, int next_left,
			 int next_right, int next_tab,
			 void (*changed) (void), const char *text,
			 int padding, const int *group);
void widget_create_textentry(struct widget *w, int x, int y, int width, int next_up,
		      int next_down, int next_tab, void (*changed) (void),
		      char *text, int max_length);
void widget_create_dyntextentry(struct widget *w, int x, int y, int width, int next_up,
			  int next_down, int next_tab, void (*changed) (void),
			  char *text, int size); /* size must be the initial size of the buffer */
void widget_create_numentry(struct widget *w, int x, int y, int width, int next_up,
		     int next_down, int next_tab, void (*changed) (void),
		     int min, int max, int *cursor_pos);
void widget_create_thumbbar(struct widget *w, int x, int y, int width, int next_up,
		     int next_down, int next_tab, void (*changed) (void),
		     int min, int max);
void widget_create_bitset(struct widget *w, int x, int y, int width, int next_up,
		   int next_down, int next_tab, void (*changed) (void),
		   int nbits, const char* bits_on, const char* bits_off,
		   int *cursor_pos);
void widget_create_panbar(struct widget *w, int x, int y, int next_up,
		   int next_down, int next_tab, void (*changed) (void),
		   int channel);
void widget_create_other(struct widget *w, int next_tab,
		  int (*w_handle_key) (struct key_event * k),
		  int (*w_handle_text_input) (const uint8_t* text),
		  void (*w_redraw) (void));

/* --------------------------------------------------- */
/* widget.c */

int widget_textentry_add_char(struct widget *widget, uint8_t c);
int widget_textentry_add_text(struct widget *widget, const uint8_t* text);
void widget_numentry_change_value(struct widget *widget, int new_value);
int widget_numentry_handle_text(struct widget *w, const uint8_t* text_input);

int widget_change_focus_to_xy(int x, int y);
void widget_change_focus_to(int new_widget_index);
/* p_widgets should point to the group of widgets (not the actual widget that is
 * being set!) and widget should be the index of the widget within the group. */
void widget_togglebutton_set(struct widget *p_widgets, int widget, int do_callback);
void widget_draw_widget(struct widget *w, int selected);

/* --------------------------------------------------- */
/* widget-keyhandler.c
 * [note: these always uses the current widget] */

int widget_handle_text_input(const uint8_t* text_input);
int widget_handle_key(struct key_event * k);

#endif /* SCHISM_WIDGET_H_ */
