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

/* --------------------------------------------------- */

void widget_create_toggle(struct widget *w, int x, int y, int next_up,
		   int next_down, int next_left, int next_right,
		   int next_tab, widget_cb changed);
void widget_create_menutoggle(struct widget *w, int x, int y, int next_up,
		       int next_down, int next_left, int next_right,
		       int next_tab, widget_cb changed,
		       const char *const *choices);
void widget_create_button(struct widget *w, int x, int y, int width, int next_up,
		   int next_down, int next_left, int next_right,
		   int next_tab, widget_cb changed, const char *text,
		   int padding);
void widget_create_togglebutton(struct widget *w, int x, int y, int width,
			 int next_up, int next_down, int next_left,
			 int next_right, int next_tab,
			 widget_cb changed, const char *text,
			 int padding, const int *group);
void widget_create_textentry(struct widget *w, int x, int y, int width, int next_up,
		      int next_down, int next_tab, widget_cb changed,
		      char *text, int max_length);
void widget_create_numentry(struct widget *w, int x, int y, int width, int next_up,
		     int next_down, int next_tab, widget_cb changed,
		     int32_t min, int32_t max, int *cursor_pos);
void widget_create_thumbbar(struct widget *w, int x, int y, int width, int next_up,
		     int next_down, int next_tab, widget_cb changed,
		     int min, int max);
void widget_create_bitset(struct widget *w, int x, int y, int width, int next_up,
		   int next_down, int next_tab, widget_cb changed,
		   int nbits, const char* bits_on, const char* bits_off,
		   int *cursor_pos);
void widget_create_panbar(struct widget *w, int x, int y, int next_up,
		   int next_down, int next_tab, widget_cb changed,
		   int channel);
void widget_create_listbox(struct widget *w,
	widget_cb_uint32 i_size, widget_uint32_cb_int i_toggled,
	widget_uint32_cb_str i_name, widget_cb i_changed,
	widget_cb i_activate, widget_key_cb i_handle_key,
	const int *focus_offsets_left, const int *focus_offsets_right,
	int next_up, int next_down);
void widget_create_other(struct widget *w, int next_tab,
		  widget_key_cb w_handle_key,
		  widget_text_cb w_handle_text_input,
		  widget_cb w_redraw);

/* --------------------------------------------------- */
/* widget.c */

int widget_textentry_add_char(struct widget *widget, unsigned char c);
int widget_textentry_add_text(struct widget *widget, const char* text);
void widget_numentry_change_value(struct widget *widget, int32_t new_value);
int widget_numentry_handle_text(struct widget *w, const char* text_input);

int widget_find_xy(int x, int y);

void widget_context_change_focus_to(struct widget_context *this, int new_widget_index);

int widget_change_focus_to_xy(int x, int y);
void widget_change_focus_to(int new_widget_index);
/* p_widgets should point to the group of widgets (not the actual widget that is
 * being set!) and widget should be the index of the widget within the group. */
void widget_togglebutton_set(struct widget *p_widgets, int widget, int do_callback);
void widget_draw_widget(struct widget *w, int selected);

/* --------------------------------------------------- */
/* widget-keyhandler.c
 * [note: these always uses the current widget] */

int widget_handle_text_input(const char *text_input);
int widget_handle_key(struct key_event * k);

#endif /* SCHISM_WIDGET_H_ */
