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

#include "charset.h"
#include "it.h"
#include "page.h"
#include "widget.h"
#include "vgamem.h"
#include "str.h"
#include "bits.h"

/* --------------------------------------------------------------------- */
/* create_* functions (the constructors, if you will) */

void widget_create_toggle(struct widget *w, int x, int y, int next_up, int next_down,
		   int next_left, int next_right, int next_tab, void (*changed) (void))
{
	w->type = WIDGET_TOGGLE;
	w->accept_text = 0;
	w->x = x;
	w->y = y;
	w->width = 3;   /* "Off" */
	w->next.up = next_up;
	w->next.left = next_left;
	w->next.down = next_down;
	w->next.right = next_right;
	w->next.tab = next_tab;
	w->next.backtab = -1;
	w->changed = changed;
	w->activate = NULL;
	w->depressed = 0;
	w->height = 1;
}

void widget_create_menutoggle(struct widget *w, int x, int y, int next_up, int next_down, int next_left,
		       int next_right, int next_tab, void (*changed) (void), const char *const *choices)
{
	int n, width = 0, len;

	for (n = 0; choices[n]; n++) {
		len = strlen(choices[n]);
		if (width < len)
			width = len;
	}

	w->type = WIDGET_MENUTOGGLE;
	w->accept_text = 0;
	w->x = x;
	w->y = y;
	w->width = width;
	w->depressed = 0;
	w->height = 1;
	w->next.up = next_up;
	w->next.left = next_left;
	w->next.down = next_down;
	w->next.right = next_right;
	w->next.tab = next_tab;
	w->next.backtab = -1;
	w->changed = changed;
	w->d.menutoggle.choices = choices;
	w->d.menutoggle.num_choices = n;
	w->activate = NULL;
	w->d.menutoggle.activation_keys = NULL;
}

void widget_create_button(struct widget *w, int x, int y, int width, int next_up, int next_down, int next_left,
		   int next_right, int next_tab, void (*changed) (void), const char *text, int padding)
{
	w->type = WIDGET_BUTTON;
	w->accept_text = 0;
	w->x = x;
	w->y = y;
	w->width = width;
	w->depressed = 0;
	w->height = 1;
	w->next.up = next_up;
	w->next.left = next_left;
	w->next.down = next_down;
	w->next.right = next_right;
	w->next.tab = next_tab;
	w->next.backtab = -1;
	w->changed = changed;
	w->d.button.text = text;
	w->d.button.padding = padding;
	w->activate = NULL;
}

void widget_create_togglebutton(struct widget *w, int x, int y, int width, int next_up, int next_down,
			 int next_left, int next_right, int next_tab, void (*changed) (void),
			 const char *text, int padding, const int *group)
{
	w->type = WIDGET_TOGGLEBUTTON;
	w->accept_text = 0;
	w->x = x;
	w->y = y;
	w->width = width;
	w->depressed = 0;
	w->height = 1;
	w->next.up = next_up;
	w->next.left = next_left;
	w->next.down = next_down;
	w->next.right = next_right;
	w->next.tab = next_tab;
	w->next.backtab = -1;
	w->changed = changed;
	w->d.togglebutton.text = text;
	w->d.togglebutton.padding = padding;
	w->d.togglebutton.group = group;
	w->activate = NULL;
}

void widget_create_textentry(struct widget *w, int x, int y, int width, int next_up, int next_down,
		      int next_tab, void (*changed) (void), char *text, int max_length)
{
	w->type = WIDGET_TEXTENTRY;
	w->accept_text = 1;
	w->x = x;
	w->y = y;
	w->width = width;
	w->depressed = 0;
	w->height = 1;
	w->next.up = next_up;
	w->next.down = next_down;
	w->next.right = -1;
	w->next.tab = next_tab;
	w->next.backtab = -1;
	w->changed = changed;
	w->d.textentry.text = text;
	w->d.textentry.max_length = max_length;
	w->d.textentry.firstchar = 0;
	w->d.textentry.cursor_pos = 0;
	w->activate = NULL;
}

void widget_create_numentry(struct widget *w, int x, int y, int width, int next_up, int next_down,
		     int next_tab, void (*changed) (void), int32_t min, int32_t max, int *cursor_pos)
{
	w->type = WIDGET_NUMENTRY;
	w->accept_text = 1;
	w->x = x;
	w->y = y;
	w->width = width;
	w->depressed = 0;
	w->height = 1;
	w->next.up = next_up;
	w->next.down = next_down;
	w->next.right = -1;
	w->next.tab = next_tab;
	w->next.backtab = -1;
	w->changed = changed;
	w->d.numentry.min = min;
	w->d.numentry.max = max;
	w->d.numentry.cursor_pos = cursor_pos;
	w->d.numentry.handle_unknown_key = NULL;
	w->d.numentry.reverse = 0;
	w->activate = NULL;
}

void widget_create_thumbbar(struct widget *w, int x, int y, int width, int next_up, int next_down,
		     int next_tab, void (*changed) (void), int min, int max)
{
	w->type = WIDGET_THUMBBAR;
	w->accept_text = 0;
	w->x = x;
	w->y = y;
	w->width = width;
	w->depressed = 0;
	w->height = 1;
	w->next.up = next_up;
	w->next.down = next_down;
	w->next.right = -1;
	w->next.tab = next_tab;
	w->next.backtab = -1;
	w->changed = changed;
	w->d.thumbbar.min = min;
	w->d.thumbbar.max = max;
	w->d.thumbbar.text_at_min = NULL;
	w->d.thumbbar.text_at_max = NULL;
	w->activate = NULL;
}

void widget_create_bitset(struct widget *w, int x, int y, int width, int next_up, int next_down,
		   int next_tab, void (*changed) (void),
		   int nbits, const char* bits_on, const char* bits_off,
		   int *cursor_pos)
{
	w->type = WIDGET_BITSET;
	w->accept_text = 0;
	w->x = x;
	w->y = y;
	w->width = width;
	w->depressed = 0;
	w->height = 1;
	w->next.up = next_up;
	w->next.down = next_down;
	w->next.right = -1;
	w->next.tab = next_tab;
	w->next.backtab = -1;
	w->changed = changed;
	w->d.numentry.reverse = 0;
	w->d.bitset.nbits = nbits;
	w->d.bitset.bits_on = bits_on;
	w->d.bitset.bits_off = bits_off;
	w->d.bitset.cursor_pos = cursor_pos;
	w->activate = NULL;
}

void widget_create_panbar(struct widget *w, int x, int y, int next_up, int next_down, int next_tab,
		   void (*changed) (void), int channel)
{
	w->type = WIDGET_PANBAR;
	w->accept_text = 0;
	w->x = x;
	w->y = y;
	w->width = 24;
	w->height = 1;
	w->next.up = next_up;
	w->next.down = next_down;
	w->next.right = -1;
	w->next.tab = next_tab;
	w->next.backtab = -1;
	w->changed = changed;
	w->d.numentry.reverse = 0;
	w->d.panbar.min = 0;
	w->d.panbar.max = 64;
	w->d.panbar.channel = channel;
	w->activate = NULL;
}

void widget_create_listbox(struct widget *w, uint32_t (*i_size) (void),
	int (*i_toggled) (uint32_t), const char * (*i_name) (uint32_t),
	void (*i_changed) (void), void (*i_activate)(void),
	int (*i_handle_key) (struct key_event *kk),
	const int *focus_offsets_left, const int *focus_offsets_right,
	int next_up, int next_down)
{
	w->type = WIDGET_LISTBOX;
	w->accept_text = 0;
	w->changed = i_changed;
	w->activate = i_activate;
	w->depressed = 0;

	w->next.up = next_up;
	w->next.down = next_down;
	/* handled by focus_offsets */
	w->next.left = w->next.right = 0;
	w->next.tab = -1;
	w->next.backtab = -1;

	/* unfocusable unless set */
	w->x = -1;
	w->y = -1;
	w->width = -1;
	w->height = 1;

	w->d.listbox.size = i_size;
	w->d.listbox.toggled = i_toggled;
	w->d.listbox.name = i_name;
	w->d.listbox.handle_key = i_handle_key;
	w->d.listbox.focus_offsets.left = focus_offsets_left;
	w->d.listbox.focus_offsets.right = focus_offsets_right;
}

void widget_create_other(struct widget *w, int next_tab, int (*i_handle_key) (struct key_event *k),
		  int (*i_handle_text_input) (const char* text), void (*i_redraw) (void))
{
	w->type = WIDGET_OTHER;
	w->accept_text = 0;
	w->next.up = w->next.down = w->next.left = w->next.right = 0;
	w->next.tab = next_tab;
	w->next.backtab = -1;
	/* w->changed = NULL; ??? */
	w->depressed = 0;
	w->activate = NULL;

	/* unfocusable unless set */
	w->x = -1;
	w->y = -1;
	w->width = -1;
	w->height = 1;

	w->d.other.handle_key = i_handle_key;
	w->d.other.handle_text_input = i_handle_text_input;
	w->d.other.redraw = i_redraw;
}

/* --------------------------------------------------------------------- */
/* generic text stuff */

void text_add_char(char *text, uint8_t c, int *cursor_pos, int max_length)
{
	int len;

	text[max_length] = 0;
	len = strlen(text);
	if (*cursor_pos >= max_length)
		*cursor_pos = max_length - 1;
	/* FIXME: this causes some weirdness with the end key. maybe hitting end should trim spaces? */
	while (len < *cursor_pos)
		text[len++] = ' ';
	memmove(text + *cursor_pos + 1, text + *cursor_pos, max_length - *cursor_pos - 1);
	text[*cursor_pos] = c;
	(*cursor_pos)++;
}

void text_delete_char(char *text, int *cursor_pos, int max_length)
{
	if (*cursor_pos == 0)
		return;
	(*cursor_pos)--;
	memmove(text + *cursor_pos, text + *cursor_pos + 1, max_length - *cursor_pos);
}

void text_delete_next_char(char *text, int *cursor_pos, int max_length)
{
	memmove(text + *cursor_pos, text + *cursor_pos + 1, max_length - *cursor_pos);
}

/* --------------------------------------------------------------------- */
/* text entries */

static void textentry_reposition(struct widget *w)
{
	int len;

	w->d.textentry.text[w->d.textentry.max_length] = 0;

	len = strlen(w->d.textentry.text);
	if (w->d.textentry.cursor_pos < w->d.textentry.firstchar) {
		w->d.textentry.firstchar = w->d.textentry.cursor_pos;
	} else if (w->d.textentry.cursor_pos > len) {
		w->d.textentry.cursor_pos = len;
	} else if (w->d.textentry.cursor_pos > (w->d.textentry.firstchar + w->width - 1)) {
		w->d.textentry.firstchar = w->d.textentry.cursor_pos - w->width + 1;
		if (w->d.textentry.firstchar < 0)
			w->d.textentry.firstchar = 0;
	}
}

int widget_textentry_add_char(struct widget *w, unsigned char c)
{
	text_add_char(w->d.textentry.text, c, &(w->d.textentry.cursor_pos), w->d.textentry.max_length);

	if (w->changed) w->changed();
	status.flags |= NEED_UPDATE;

	return 1;
}

int widget_textentry_add_text(struct widget *w, const char *text)
{
	uint8_t *itf;
	size_t i;

	itf = charset_iconv_easy(text, CHARSET_UTF8, CHARSET_ITF);
	if (!itf)
		return 0;

	for (i = 0; itf[i]; i++)
		if (!widget_textentry_add_char(w, itf[i]))
			return 0;

	return 1;
}

/* --------------------------------------------------------------------- */
/* numeric entries */

void widget_numentry_change_value(struct widget *w, int32_t new_value)
{
	new_value = CLAMP(new_value, w->d.numentry.min, w->d.numentry.max);
	w->d.numentry.value = new_value;
	if (w->changed) w->changed();
	status.flags |= NEED_UPDATE;
}

static inline SCHISM_ALWAYS_INLINE uint32_t fast_pow10(uint32_t n)
{
	static const uint32_t tens[] = {
		1,     10,     100,     1000,
		10000, 100000, 1000000, 10000000
	};

	/* use our cache if we can to avoid buffer overrun */
	return (n < ARRAY_SIZE(tens)) ? tens[n] : bpow32(10, n);
}

int widget_numentry_handle_text(struct widget *w, const char *text)
{
	static const char *valid_digits = "0123456789";
	size_t len;
	int value;

	if (!text)
		return 0;

	len = strspn(text, valid_digits);
	if (len < 1)
		return 0;

	value = w->d.numentry.value;

	if (w->d.numentry.reverse) {
		size_t i;

		for (i = 0; i < len; i++) {
			value *= 10;
			value += text[0] - '0';
		}
	} else {
		int pos = *(w->d.numentry.cursor_pos);
		size_t n;

		for (n = 0; n < len && pos < w->width; n++, pos++) {
			int32_t pow10_of_pos = fast_pow10(w->width - 1 - pos);

			/* isolate our digit and subtract it */
			value -= value % (pow10_of_pos * 10) / pow10_of_pos * pow10_of_pos;
			/* add our digit in its place */
			value += (text[n] - '0') * pow10_of_pos;
		}

		*(w->d.numentry.cursor_pos) = CLAMP(pos, 0, w->width - 1);
	}

	/* notify that our value changed */
	widget_numentry_change_value(w, value);

	return 1;
}

/* --------------------------------------------------------------------- */
/* toggle buttons */

void widget_togglebutton_set(struct widget *p_widgets, int widget, int do_callback)
{
	const int *group = p_widgets[widget].d.togglebutton.group;
	int i;

	if (!group) return; /* assert */

	for (i = 0; group[i] >= 0; i++)
		p_widgets[group[i]].d.togglebutton.state = 0;
	p_widgets[widget].d.togglebutton.state = 1;

	if (do_callback && p_widgets[widget].changed)
		p_widgets[widget].changed();

	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* /me takes a deep breath */

void widget_draw_widget(struct widget *w, int selected)
{
	char buf[64] = "Channel 42";
	const char *ptr, *endptr;       /* for the menutoggle */
	char *str;
	int n;
	const int tfg = selected ? 0 : 2;
	const int tbg = selected ? 3 : 0;
	int drew_cursor = 0;
	int fg,bg;

	switch (w->type) {
	case WIDGET_TOGGLE:
		draw_fill_chars(w->x, w->y, w->x + w->width - 1, w->y, DEFAULT_FG, 0);
		draw_text((w->d.toggle.state ? "On" : "Off"), w->x, w->y, tfg, tbg);
		break;
	case WIDGET_MENUTOGGLE:
		draw_fill_chars(w->x, w->y, w->x + w->width - 1, w->y, DEFAULT_FG, 0);
		ptr = w->d.menutoggle.choices[w->d.menutoggle.state];
		endptr = strchr(ptr, ' ');
		if (endptr) {
			n = endptr - ptr;
			draw_text_len(ptr, n, w->x, w->y, tfg, tbg);
			draw_text(endptr + 1, w->x + n + 1, w->y, 2, 0);
		} else {
			draw_text(ptr, w->x, w->y, tfg, tbg);
		}
		break;
	case WIDGET_BUTTON:
		draw_box(w->x - 1, w->y - 1, w->x + w->width + 2, w->y + 1,
			 BOX_THIN | BOX_INNER | (
				w->depressed ? BOX_INSET : BOX_OUTSET));
		draw_text(w->d.button.text, w->x + w->d.button.padding, w->y, selected ? 3 : 0, 2);
		break;
	case WIDGET_TOGGLEBUTTON:
		draw_box(w->x - 1, w->y - 1, w->x + w->width + 2, w->y + 1,
			 BOX_THIN | BOX_INNER |(
				(w->d.togglebutton.state || w->depressed) ? BOX_INSET : BOX_OUTSET));
		draw_text(w->d.togglebutton.text, w->x + w->d.togglebutton.padding, w->y, selected ? 3 : 0, 2);
		break;
	case WIDGET_TEXTENTRY:
		textentry_reposition(w);
		draw_text_len(w->d.textentry.text + w->d.textentry.firstchar, w->width, w->x, w->y, 2, 0);
		if (selected && !drew_cursor) {
			n = w->d.textentry.cursor_pos - w->d.textentry.firstchar;
			draw_char(((n < (signed) strlen(w->d.textentry.text))
				   ? (w->d.textentry.text[w->d.textentry.cursor_pos]) : ' '),
				  w->x + n, w->y, 0, 3);
		}
		break;
	case WIDGET_NUMENTRY:
		if (w->d.numentry.reverse) {
			str = str_from_num(w->width, w->d.numentry.value, buf);
			while (*str == '0') str++;
			draw_text_len("", w->width, w->x, w->y, 2, 0);
			if (*str) {
				draw_text(str, (w->x+w->width) - strlen(str),
						w->y, 2, 0);
			}
			if (selected && !drew_cursor) {
				while (str[0] && str[1]) str++;
				if (!str[0]) str[0] = ' ';
				draw_char(str[0], w->x + (w->width-1), w->y, 0, 3);
			}
		} else {
			if (w->d.numentry.min < 0 || w->d.numentry.max < 0) {
				str_from_num_signed(w->width, w->d.numentry.value, buf);
			} else {
				str_from_num(w->width, w->d.numentry.value, buf);
			}
			draw_text_len(buf,
					w->width, w->x, w->y, 2, 0);
			if (selected && !drew_cursor) {
				n = *(w->d.numentry.cursor_pos);
				draw_char(buf[n], w->x + n, w->y, 0, 3);
			}
		}
		break;
	case WIDGET_BITSET:
		for(n = 0; n < w->d.bitset.nbits; ++n)
		{
			int set = !!(w->d.bitset.value & (1 << n));
			char label_c1   = set ? w->d.bitset.bits_on[n*2+0]
					      : w->d.bitset.bits_off[n*2+0];
			char label_c2   = set ? w->d.bitset.bits_on[n*2+1]
					      : w->d.bitset.bits_off[n*2+1];
			int is_focused = selected && n == *w->d.bitset.cursor_pos;
			/* In textentries, cursor=0,3; normal=2,0 */
			static const char fg_selection[4] =
			{
				2, /* not cursor, not set */
				3, /* not cursor, is  set */
				0, /* has cursor, not set */
				0  /* has cursor, is  set */
			};
			static const char bg_selection[4] =
			{
				0, /* not cursor, not set */
				0, /* not cursor, is  set */
				2, /* has cursor, not set */
				3  /* has cursor, is  set */
			};
			fg = fg_selection[set + is_focused*2];
			bg = bg_selection[set + is_focused*2];
			if(label_c2)
			      draw_half_width_chars(label_c1, label_c2, w->x + n, w->y, fg, bg, fg, bg);
			else
			      draw_char(label_c1, w->x + n, w->y, fg, bg);
		}
		break;
	case WIDGET_THUMBBAR:
		if (w->d.thumbbar.text_at_min && w->d.thumbbar.min == w->d.thumbbar.value) {
			draw_text_len(w->d.thumbbar.text_at_min, w->width, w->x, w->y, selected ? 3 : 2, 0);
		} else if (w->d.thumbbar.text_at_max && w->d.thumbbar.max == w->d.thumbbar.value) {
			/* this will probably do Bad Things if the text is too long */
			int len = strlen(w->d.thumbbar.text_at_max);
			int pos = w->x + w->width - len;

			draw_fill_chars(w->x, w->y, pos - 1, w->y, DEFAULT_FG, 0);
			draw_text_len(w->d.thumbbar.text_at_max, len, pos, w->y, selected ? 3 : 2, 0);
		} else {
			draw_thumb_bar(w->x, w->y, w->width, w->d.thumbbar.min,
				       w->d.thumbbar.max, w->d.thumbbar.value, selected);
		}
		if (w->d.thumbbar.min < 0 || w->d.thumbbar.max < 0) {
			str_from_num_signed(3, w->d.thumbbar.value, buf);
		} else {
			str_from_num(3, w->d.thumbbar.value, buf);
		}
		draw_text(buf,
				w->x + w->width + 1, w->y, 1, 2);
		break;
	case WIDGET_PANBAR:
		str_from_num(2, w->d.panbar.channel, buf + 8);
		draw_text(buf, w->x, w->y, selected ? 3 : 0, 2);
		if (w->d.panbar.muted) {
			draw_text("  Muted  ", w->x + 11, w->y, selected ? 3 : 5, 0);
			/* draw_fill_chars(w->x + 21, w->y, w->x + 23, w->y, 2); */
		} else if (w->d.panbar.surround) {
			draw_text("Surround ", w->x + 11, w->y, selected ? 3 : 5, 0);
			/* draw_fill_chars(w->x + 21, w->y, w->x + 23, w->y, 2); */
		} else {
			draw_thumb_bar(w->x + 11, w->y, 9, 0, 64, w->d.panbar.value, selected);
			draw_text(str_from_num(3, w->d.thumbbar.value, buf), w->x + 21, w->y, 1, 2);
		}
		break;
	case WIDGET_LISTBOX: {
		int32_t i;
		uint32_t o;
		uint32_t size = w->d.listbox.size();

		draw_fill_chars(w->x, w->y, w->x + w->width - 1, w->y + w->height - 1, DEFAULT_FG, 0);

		if (w->d.listbox.top >= size)
			break; /* wat */

#define GET_FG_BG(o) \
	do { \
		if ((o) == w->d.listbox.focus) { \
			if (w == &ACTIVE_WIDGET) { \
				fg = 0; \
				bg = 3; \
			} else { \
				fg = 6; \
				bg = 14; \
			} \
		} else { \
			fg = 6; \
			bg = 0; \
		} \
	} while (0)

		for (o = w->d.listbox.top, i = 0; o < size && i < w->height; i++, o++) {
			GET_FG_BG(o);

			draw_text_utf8_len(w->d.listbox.toggled(o) ? "*" : " ", 1, w->x, w->y + i, fg, bg);
			draw_text_utf8_len(w->d.listbox.name(o), w->width - 1, w->x + 1, w->y + i, fg, bg);
		}

		if (w->height >= 2) {
			/* this should always be true; otherwise it's terrible */

			if (w->d.listbox.top > 0) {
				GET_FG_BG(w->d.listbox.top);

				/* Draw up arrow in the top right.
				 * this is similar to what Motif does to notate that
				 * it can be scrolled.
				 *
				 * TODO should allow clicking onto it to scroll. */
				draw_char_bios(30, w->x + w->width - 1, w->y, fg, bg);
			}

			if (w->d.listbox.top + w->height < size) {
				GET_FG_BG(w->d.listbox.top + w->height - 1);

				/* Draw down arrow in the bottom right. */
				draw_char_bios(31, w->x + w->width - 1, w->y + w->height - 1, fg, bg);
			}
		}

#undef GET_FG_BG

		break;
	}
	case WIDGET_OTHER:
		if (w->d.other.redraw) w->d.other.redraw();
		break;
	default:
		/* shouldn't ever happen... */
		break;
	}
}

/* --------------------------------------------------------------------- */
/* more crap */

void widget_change_focus_to(int new_widget_index)
{
	if(new_widget_index == *selected_widget || new_widget_index < 0 || new_widget_index >= *total_widgets) {
		return;
	}

	if (ACTIVE_WIDGET.depressed) ACTIVE_WIDGET.depressed = 0;

	*selected_widget = new_widget_index;

	ACTIVE_WIDGET.depressed = 0;

	if (ACTIVE_WIDGET.type == WIDGET_TEXTENTRY)
		ACTIVE_WIDGET.d.textentry.cursor_pos
				= strlen(ACTIVE_WIDGET.d.textentry.text);

	status.flags |= NEED_UPDATE;
}

int widget_find_xy(int x, int y)
{
	struct widget *w;
	int i, pad;

	if (!total_widgets)
		return -1;
	for (i = 0; i < *total_widgets; i++) {
		w = widgets + i;
		switch (w->type) {
		case WIDGET_BUTTON:
		case WIDGET_TOGGLEBUTTON:
			pad = 2;
			break;
		default:
			pad = 0;
		}
		if (x >= w->x && x < w->x + w->width + pad && y >= w->y && y < w->y + w->height) {
			return i;
		}
	}
	return -1;
}

int widget_change_focus_to_xy(int x, int y)
{
	int n = widget_find_xy(x, y);
	if (n >= 0) {
		widget_change_focus_to(n);
		return 1;
	}
	return 0;
}
