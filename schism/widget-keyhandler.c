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
#include "page.h"
#include "song.h"
#include "keyboard.h"
#include "widget.h"

/* --------------------------------------------------------------------- */

/* n => the delta-value */
static void numentry_move_cursor(struct widget *widget, int n)
{
	if (widget->d.numentry.reverse) return;
	n += *(widget->d.numentry.cursor_pos);
	n = CLAMP(n, 0, widget->width - 1);
	if (*(widget->d.numentry.cursor_pos) == n)
		return;
	*(widget->d.numentry.cursor_pos) = n;
	status.flags |= NEED_UPDATE;
}

static void textentry_move_cursor(struct widget *widget, int n)
{
	n += widget->d.textentry.cursor_pos;
	n = CLAMP(n, 0, widget->d.textentry.max_length);
	if (widget->d.textentry.cursor_pos == n)
		return;
	widget->d.textentry.cursor_pos = n;
	status.flags |= NEED_UPDATE;
}

static void bitset_move_cursor(struct widget *widget, int n)
{
	n += *widget->d.bitset.cursor_pos;
	n = CLAMP(n, 0, widget->d.bitset.nbits-1);
	if (*widget->d.bitset.cursor_pos == n)
		return;
	*widget->d.bitset.cursor_pos = n;
	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* thumbbar value prompt */

static void thumbbar_prompt_finish(struct widget *thumbbar, int n)
{
	if (n >= thumbbar->d.thumbbar.min && n <= thumbbar->d.thumbbar.max) {
		thumbbar->d.thumbbar.value = n;
		if (thumbbar->changed) thumbbar->changed(widget_get_context(thumbbar));
	}

	status.flags |= NEED_UPDATE;
}

static int thumbbar_prompt_value(struct widget *widget, struct key_event *k)
{
	int c;

	if (!NO_MODIFIER(k->mod)) {
		/* annoying */
		return 0;
	}
	if (k->sym == SCHISM_KEYSYM_MINUS) {
		if (widget->d.thumbbar.min >= 0)
			return 0;
		c = '-';
	} else {
		c = numeric_key_event(k, 0);
		if (c < 0)
			return 0;
		c += '0';
	}

	numprompt_create_for_thumbbar("Enter Value", widget, thumbbar_prompt_finish, c);

	return 1;
}

/* --------------------------------------------------------------------- */
/* Find backtabs. */

static inline int find_tab_to(int target)
{
	for (int i = 0; i < widget_context->total_widgets; i++) {
		if (widget_context->widgets[i].next.tab == target && i != target) {
			return i;
		}
	}

	return -1;
}

static inline int find_down_to(int target)
{
	for (int i = 0; i < widget_context->total_widgets; i++) {
		if (widget_context->widgets[i].next.down == target && i != target) {
			return i;
		}
	}

	return -1;
}

static inline int find_right_to(int target)
{
	for (int i = 0; i < widget_context->total_widgets; i++) {
		if (widget_context->widgets[i].next.right == target && i != target) {
			return i;
		}
	}

	return -1;
}

static inline int find_right_or_down_to(int target, int checkNotEqual)
{
	if (status.flags & CLASSIC_MODE) {
		int right_to = find_right_to(target);

		if(right_to > -1 && right_to != checkNotEqual)
			return right_to;

		int down_to = find_down_to(target);

		if(down_to > -1 && down_to != checkNotEqual)
			return down_to;
	} else {
		int down_to = find_down_to(target);

		if(down_to > -1 && down_to != checkNotEqual)
			return down_to;

		int right_to = find_right_to(target);

		if(right_to > -1 && right_to != checkNotEqual)
			return right_to;
	}

	return -1;
}

static inline int find_tab_to_recursive(int target)
{
	int current = target;

	for(int i = 0; i < widget_context->total_widgets; i++) {
		int widget_backtab = widget_context->widgets[current].next.backtab;
		if(widget_backtab > -1) return widget_backtab;

		int tab_to = find_tab_to(current);
		if(tab_to > -1) return tab_to;

		int right_or_down_to = find_right_or_down_to(current, target);

		if(right_or_down_to > -1) {
			current = right_or_down_to;
			continue;
		}

		return -1;
	}

	return -1;
}

static void _backtab(void)
{
	/* hunt for a widget that leads back to this one */
	if (!widget_context) return;

	int selected = widget_context->selected_widget;
	int backtab = find_tab_to_recursive(selected);

	if(backtab > -1) {
		widget_change_focus_to(backtab);
		return;
	}

	int right_or_down_to = find_right_or_down_to(selected, selected);
	if(right_or_down_to > -1) widget_change_focus_to(right_or_down_to);
}

/* return: 1 = handled text, 0 = didn't */
int widget_handle_text_input(const char *text_input)
{
	struct widget *widget = &ACTIVE_WIDGET;
	if (!widget)
		return 0;

	struct widget_context *this = widget_get_context(widget);

	switch (widget->type) {
		case WIDGET_OTHER:
			if (widget->accept_text && widget->d.other.handle_text_input
				&& widget->d.other.handle_text_input(this, text_input))
				return 1;
			break;
		case WIDGET_NUMENTRY:
			if (widget_numentry_handle_text(widget, text_input))
				return 1;
			break;
		case WIDGET_TEXTENTRY:
			if (widget_textentry_add_text(widget, text_input))
				return 1;
			break;
		default:
			break;
	}
	return 0;
}

static int widget_menutoggle_handle_key(struct widget *w, struct key_event *k)
{
	if( ((k->mod & (SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT | SCHISM_KEYMOD_GUI)) == 0)
	   && w->d.menutoggle.activation_keys) {
		const char* m = w->d.menutoggle.activation_keys;
		const char* p = strchr(m, (char)k->sym);
		if (p && *p) {
			w->d.menutoggle.state = p - m;
			if(w->changed) w->changed(widget_get_context(w));
			status.flags |= NEED_UPDATE;
			return 1;
		}
	}
	return 0;
}

static int widget_bitset_handle_key(struct widget *w, struct key_event *k)
{
	if( ((k->mod & (SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT | SCHISM_KEYMOD_GUI)) == 0)
	   && w->d.bitset.activation_keys) {
		const char* m = w->d.bitset.activation_keys;
		const char* p = strchr(m, (char)k->sym);
		if (p && *p) {
			int bit_index = p-m;
			w->d.bitset.value ^= (1 << bit_index);
			if(w->changed) w->changed(widget_get_context(w));
			status.flags |= NEED_UPDATE;
			return 1;
		}
	}
	return 0;
}

static int widget_listbox_handle_key(struct widget_context *this, struct widget *w, struct key_event *k)
{
	int32_t new_device = w->d.listbox.focus;
	uint32_t size = w->d.listbox.size(this);
	int load_selected_device = 0;

	switch (k->mouse) {
	case MOUSE_DBLCLICK:
	case MOUSE_CLICK:
		if (k->state != KEY_PRESS)
			return 0;
		if (k->x < w->x || k->y < w->y || k->y > (w->y + w->height - 1) || k->x > (w->x + w->width - 1)) return 0;
		new_device = (int32_t)w->d.listbox.top + k->y - w->y;
		if (k->mouse == MOUSE_DBLCLICK || new_device == w->d.listbox.focus)
			load_selected_device = 1;
		break;
	case MOUSE_SCROLL_UP:
		new_device -= MOUSE_SCROLL_LINES;
		break;
	case MOUSE_SCROLL_DOWN:
		new_device += MOUSE_SCROLL_LINES;
		break;
	default:
		if (k->state == KEY_RELEASE)
			return 0;
	}

	switch (k->sym) {
	case SCHISM_KEYSYM_UP:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (--new_device < 0)
			return 0;
		break;
	case SCHISM_KEYSYM_DOWN:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (++new_device >= (int32_t)size)
			return 0;
		break;
	case SCHISM_KEYSYM_HOME:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_device = 0;
		break;
	case SCHISM_KEYSYM_PAGEUP:
		if (!NO_MODIFIER(k->mod))
			return 0;

		if (new_device == 0)
			return 1;

		new_device -= 16;
		break;
	case SCHISM_KEYSYM_END:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_device = (int32_t)size;
		break;
	case SCHISM_KEYSYM_PAGEDOWN:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_device += 16;
		break;
	case SCHISM_KEYSYM_RETURN:
		if (!NO_MODIFIER(k->mod))
			return 0;
		load_selected_device = 1;
		break;
	case SCHISM_KEYSYM_TAB: {
		const int *f;

		if (k->mod & SCHISM_KEYMOD_SHIFT) {
			f = w->d.listbox.focus_offsets.left;
		} else if (NO_MODIFIER(k->mod)) {
			f = w->d.listbox.focus_offsets.right;
		} else {
			return 0;
		}

		widget_context_change_focus_to(this, f[w->d.listbox.focus]);
		return 1;
	}
	case SCHISM_KEYSYM_LEFT:
		if (!NO_MODIFIER(k->mod))
			return 0;

		widget_context_change_focus_to(this, w->d.listbox.focus_offsets.left[w->d.listbox.focus]);
		return 1;
	case SCHISM_KEYSYM_RIGHT:
		if (!NO_MODIFIER(k->mod))
			return 0;

		widget_context_change_focus_to(this, w->d.listbox.focus_offsets.right[w->d.listbox.focus]);
		return 1;
	default:
		if (w->d.listbox.handle_key && w->d.listbox.handle_key(this, k))
			return 1;

		if (k->mouse == MOUSE_NONE)
			return 0;
	}

	new_device = CLAMP(new_device, 0, (int32_t)size - 1);

	if (new_device != w->d.listbox.focus) {
		int32_t top = w->d.listbox.top;

		w->d.listbox.focus = new_device;
		status.flags |= NEED_UPDATE;

		/* these HAVE to be done separately (and not as a CLAMP) because they aren't
		 * really guaranteed to be ranges */
		top = MIN(top, w->d.listbox.focus);
		top = MAX(top, (int32_t)w->d.listbox.focus - w->height + 1);

		top = MIN(top, (int32_t)size - w->height + 1);
		top = MAX(top, 0);

		w->d.listbox.top = top;

		if (w->changed)
			w->changed(this);
	}

	if (load_selected_device && w->activate)
		w->activate(this);

	return 1;
}

/* return: 1 = handled key, 0 = didn't */
int widget_handle_key(struct key_event * k)
{
	struct widget *widget = &ACTIVE_WIDGET;
	if (!widget)
		return 0;

	struct widget_context *this = widget_get_context(widget);

	int n, onw, wx, fmin, fmax, pad;
	void (*changed)(struct widget_context *this);
	enum widget_type current_type = widget->type;

	if (!(status.flags & DISKWRITER_ACTIVE)
	    && ((current_type == WIDGET_OTHER && widget->d.other.handle_key(this, k))
			|| (current_type == WIDGET_LISTBOX && widget_listbox_handle_key(this, widget, k))))
		return 1;

	if (!(status.flags & DISKWRITER_ACTIVE) && k->mouse
	    && (status.flags & CLASSIC_MODE)) {
		switch(current_type) {
		case WIDGET_NUMENTRY:
			if (k->mouse_button == MOUSE_BUTTON_LEFT) {
				k->sym = SCHISM_KEYSYM_MINUS;
				k->mouse = MOUSE_NONE;
			} else if (k->mouse_button == MOUSE_BUTTON_RIGHT) {
				k->sym = SCHISM_KEYSYM_PLUS;
				k->mouse = MOUSE_NONE;
			}
			break;
		default:
			break;
		};
	}

	if (k->mouse == MOUSE_NONE)
		onw = 0;
	else {
		switch (widget->type) {
		case WIDGET_BUTTON:
		case WIDGET_TOGGLEBUTTON:
			pad = 2;
			break;
		default:
			pad = 0;
		};
		onw = ((signed) k->x < widget->x
		       || (signed) k->x >= widget->x + widget->width + pad
		       || (signed) k->y != widget->y) ? 0 : 1;
	}

	if (k->mouse == MOUSE_CLICK) {
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (onw) {
			switch (current_type) {
			case WIDGET_TOGGLE:
				if (!NO_MODIFIER(k->mod))
					return 0;
				if (k->state != KEY_PRESS)
					return 1;
				widget->d.toggle.state = !widget->d.toggle.state;
				if (widget->changed) widget->changed(this);
				status.flags |= NEED_UPDATE;
				return 1;
			case WIDGET_MENUTOGGLE:
				if (!NO_MODIFIER(k->mod))
					return 0;
				if (k->state != KEY_PRESS)
					return 1;
				widget->d.menutoggle.state = (widget->d.menutoggle.state + 1)
					% widget->d.menutoggle.num_choices;
				if (widget->changed) widget->changed(this);
				status.flags |= NEED_UPDATE;
				return 1;
			default:
				break;
			}
		}
	} else if (k->mouse == MOUSE_DBLCLICK) {
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (onw) {
			if (current_type == WIDGET_PANBAR) {
				if (!NO_MODIFIER(k->mod))
					return 0;
				widget->d.panbar.muted = !widget->d.panbar.muted;
				changed = widget->changed;
				if (changed) changed(this);
				return 1;
			}
		}
	}

	if (k->mouse == MOUSE_CLICK
	    || (k->mouse == MOUSE_NONE && k->sym == SCHISM_KEYSYM_RETURN)) {
#if 0
		if (k->mouse && k->mouse_button == MOUSE_BUTTON_MIDDLE) {
			if (status.flags & DISKWRITER_ACTIVE) return 0;
			if (k->state == KEY_PRESS)
				return 1;
			status.flags |= CLIPPY_PASTE_SELECTION;
			return 1;
		}
#endif
		if (k->mouse && (current_type == WIDGET_THUMBBAR
		|| current_type == WIDGET_PANBAR)) {
			if (status.flags & DISKWRITER_ACTIVE) return 0;

			/* swallow it */
			if (!k->on_target && (k->state != KEY_DRAG)) return 0;

			fmin = widget->d.thumbbar.min;
			fmax = widget->d.thumbbar.max;
			if (current_type == WIDGET_PANBAR) {
				n = k->fx - ((widget->x + 11) * k->rx);
				wx = (widget->width - 16) * k->rx;
			} else {
				n = k->fx - (widget->x * k->rx);
				wx = (widget->width-1) * k->rx;
			}

			n = CLAMP(n, 0, wx);
			n = fmin + ((n * (fmax - fmin)) / wx);
			n = CLAMP(n, fmin, fmax);

			if (current_type == WIDGET_PANBAR) {
				widget->d.panbar.muted = 0;
				widget->d.panbar.surround = 0;
				if (k->x - widget->x < 11) return 1;
				if (k->x - widget->x > 19) return 1;
			}
			widget_numentry_change_value(widget, n);
			return 1;
		}
		if (k->mouse) {
			n = (k->state == KEY_PRESS && onw) ? 1 : 0;
			if (widget->depressed != n)
				status.flags |= NEED_UPDATE;
			else if (k->state == KEY_RELEASE)
				return 1; // swallor
			widget->depressed = n;
			if (current_type != WIDGET_TEXTENTRY && current_type != WIDGET_NUMENTRY) {
				if (k->state == KEY_PRESS || !onw)
					return 1;
			} else if (!onw) {
				return 1;
			}
		} else {
			n = (k->state == KEY_PRESS) ? 1 : 0;
			if (widget->depressed != n)
				status.flags |= NEED_UPDATE;
			else if (k->state == KEY_RELEASE)
				return 1; // swallor
			widget->depressed = n;
			if (k->state == KEY_PRESS)
				return 1;
		}

		if (k->mouse) {
			switch(current_type) {
			case WIDGET_MENUTOGGLE:
			case WIDGET_BUTTON:
			case WIDGET_TOGGLEBUTTON:
				if (k->on_target && widget->activate) widget->activate(this);
			default:
				break;
			};
		} else if (current_type != WIDGET_OTHER) {
			if (widget->activate) widget->activate(this);
		}

		switch (current_type) {
		case WIDGET_OTHER:
			break;
		case WIDGET_TEXTENTRY:
			if (status.flags & DISKWRITER_ACTIVE) return 0;
			/* LOL WOW THIS SUCKS */
			if (/*k->state == KEY_PRESS && */k->mouse == MOUSE_CLICK && k->on_target) {
				size_t len;

				/* position cursor */
				n = k->x - widget->x;
				n = CLAMP(n, 0, widget->width - 1);
				wx = k->sx - widget->x;
				wx = CLAMP(wx, 0, widget->width - 1);
				widget->d.textentry.cursor_pos = n+widget->d.textentry.firstchar;
				wx  = wx+widget->d.textentry.firstchar;

				len = strlen(widget->d.textentry.text);

				if (widget->d.textentry.cursor_pos >= (signed)len)
					widget->d.textentry.cursor_pos = len;
				if (wx >= len)
					wx = len;
				status.flags |= NEED_UPDATE;
			}

			/* for a text entry, the only thing enter does is run the activate callback.
			thus, if no activate callback is defined, the key wasn't handled */
			return (widget->activate != NULL);

		case WIDGET_NUMENTRY:
			if (status.flags & DISKWRITER_ACTIVE) return 0;
			if (/*k->state == KEY_PRESS && */k->mouse == MOUSE_CLICK && k->on_target) {
				/* position cursor */
				n = k->x - widget->x;
				n = CLAMP(n, 0, widget->width - 1);
				wx = k->sx - widget->x;
				wx = CLAMP(wx, 0, widget->width - 1);
				if (n >= widget->width)
					n = widget->width - 1;
				*widget->d.numentry.cursor_pos = n;
				status.flags |= NEED_UPDATE;
			}

			break;

		case WIDGET_TOGGLEBUTTON:
			if (status.flags & DISKWRITER_ACTIVE) return 0;
			if (widget->d.togglebutton.group) {
				/* this also runs the changed callback and redraws the button(s) */
				if (k->state == KEY_DRAG) {
					/* k-on target */
					widget->depressed = k->on_target;
				} else {
					widget_togglebutton_set(widget_context->widgets, widget_context->selected_widget, 1);
				}
				return 1;
			}
			/* else... */
			widget->d.togglebutton.state = !widget->d.togglebutton.state;
			SCHISM_FALLTHROUGH;
		case WIDGET_BUTTON:
			if (k->state == KEY_DRAG) {
				widget->depressed = k->on_target;
			} else {
				/* maybe buttons should ignore the changed callback, and use activate instead...
				(but still call the changed callback for togglebuttons if they *actually* changed) */
				if (widget->changed) widget->changed(this);
				status.flags |= NEED_UPDATE;
				return 1;
			}
		default:
			break;
		}
		return 0;
	}

	/* a WIDGET_OTHER that *didn't* handle the key itself needs to get run through the switch
	statement to account for stuff like the tab key */
	if (k->state == KEY_RELEASE)
		return 0;

	if (k->mouse == MOUSE_SCROLL_UP && current_type == WIDGET_NUMENTRY) {
		k->sym = SCHISM_KEYSYM_MINUS;
	} else if (k->mouse == MOUSE_SCROLL_DOWN && current_type == WIDGET_NUMENTRY) {
		k->sym = SCHISM_KEYSYM_PLUS;
	}

	switch (k->sym) {
	case SCHISM_KEYSYM_ESCAPE:
		/* this is to keep the text entries from taking the key hostage and inserting '<-'
		characters instead of showing the menu */
		return 0;
	case SCHISM_KEYSYM_UP:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		widget_context_change_focus_to(this, widget->next.up);
		return 1;
	case SCHISM_KEYSYM_DOWN:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		widget_context_change_focus_to(this, widget->next.down);
		return 1;
	case SCHISM_KEYSYM_TAB:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (k->mod & SCHISM_KEYMOD_SHIFT) {
			_backtab();
			return 1;
		}
		if (!NO_MODIFIER(k->mod))
			return 0;
		widget_context_change_focus_to(this, widget->next.tab);
		return 1;
	case SCHISM_KEYSYM_LEFT:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		switch (current_type) {
		case WIDGET_BITSET:
		    if (NO_MODIFIER(k->mod))
			bitset_move_cursor(widget, -1);
		    break;
		case WIDGET_NUMENTRY:
			if (!NO_MODIFIER(k->mod)) {
				return 0;
			}
			numentry_move_cursor(widget, -1);
			return 1;
		case WIDGET_TEXTENTRY:
			if (!NO_MODIFIER(k->mod)) {
				return 0;
			}
			textentry_move_cursor(widget, -1);
			return 1;
		case WIDGET_PANBAR:
			widget->d.panbar.muted = 0;
			widget->d.panbar.surround = 0;
			/* fall through */
		case WIDGET_THUMBBAR:
			/* I'm handling the key modifiers differently than Impulse Tracker, but only
			because I think this is much more useful. :) */
			n = 1;
			if (k->mod & (SCHISM_KEYMOD_ALT | SCHISM_KEYMOD_GUI))
				n *= 8;
			if (k->mod & SCHISM_KEYMOD_SHIFT)
				n *= 4;
			if (k->mod & SCHISM_KEYMOD_CTRL)
				n *= 2;
			n = widget->d.numentry.value - n;
			widget_numentry_change_value(widget, n);
			return 1;
		default:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget_context_change_focus_to(this, widget->next.left);
			return 1;
		}
		break;
	case SCHISM_KEYSYM_RIGHT:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		/* pretty much the same as left, but with a few small
		 * changes here and there... */
		switch (current_type) {
		case WIDGET_BITSET:
		    if (NO_MODIFIER(k->mod))
			bitset_move_cursor(widget, 1);
		    break;
		case WIDGET_NUMENTRY:
			if (!NO_MODIFIER(k->mod)) {
				return 0;
			}
			numentry_move_cursor(widget, 1);
			return 1;
		case WIDGET_TEXTENTRY:
			if (!NO_MODIFIER(k->mod)) {
				return 0;
			}
			textentry_move_cursor(widget, 1);
			return 1;
		case WIDGET_PANBAR:
			widget->d.panbar.muted = 0;
			widget->d.panbar.surround = 0;
			/* fall through */
		case WIDGET_THUMBBAR:
			n = 1;
			if (k->mod & (SCHISM_KEYMOD_ALT | SCHISM_KEYMOD_GUI))
				n *= 8;
			if (k->mod & SCHISM_KEYMOD_SHIFT)
				n *= 4;
			if (k->mod & SCHISM_KEYMOD_CTRL)
				n *= 2;
			n = widget->d.numentry.value + n;
			widget_numentry_change_value(widget, n);
			return 1;
		default:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget_context_change_focus_to(this, widget->next.right);
			return 1;
		}
		break;
	case SCHISM_KEYSYM_HOME:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		/* Impulse Tracker only does home/end for the thumbbars.
		 * This stuff is all extra. */
		switch (current_type) {
		case WIDGET_NUMENTRY:
			if (!NO_MODIFIER(k->mod))
				return 0;
			*(widget->d.numentry.cursor_pos) = 0;
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_TEXTENTRY:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->d.textentry.cursor_pos = 0;
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_PANBAR:
			widget->d.panbar.muted = 0;
			widget->d.panbar.surround = 0;
			/* fall through */
		case WIDGET_THUMBBAR:
			n = widget->d.thumbbar.min;
			widget_numentry_change_value(widget, n);
			return 1;
		default:
			break;
		}
		break;
	case SCHISM_KEYSYM_END:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		switch (current_type) {
		case WIDGET_NUMENTRY:
			if (!NO_MODIFIER(k->mod))
				return 0;
			*(widget->d.numentry.cursor_pos) = widget->width - 1;
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_TEXTENTRY:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->d.textentry.cursor_pos = strlen(widget->d.textentry.text);
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_PANBAR:
			widget->d.panbar.muted = 0;
			widget->d.panbar.surround = 0;
			/* fall through */
		case WIDGET_THUMBBAR:
			n = widget->d.thumbbar.max;
			widget_numentry_change_value(widget, n);
			return 1;
		default:
			break;
		}
		break;
	case SCHISM_KEYSYM_SPACE:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		switch (current_type) {
		case WIDGET_BITSET:
		    if (!NO_MODIFIER(k->mod))
			return 0;
		    widget->d.bitset.value ^= (1 << *widget->d.bitset.cursor_pos);
			if (widget->changed) widget->changed(this);
		    status.flags |= NEED_UPDATE;
		    return 1;
		case WIDGET_TOGGLE:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->d.toggle.state = !widget->d.toggle.state;
			if (widget->changed) widget->changed(this);
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_MENUTOGGLE:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->d.menutoggle.state = (widget->d.menutoggle.state + 1)
				% widget->d.menutoggle.num_choices;
			if (widget->changed) widget->changed(this);
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_PANBAR:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->d.panbar.muted = !widget->d.panbar.muted;
			changed = widget->changed;
			widget_context_change_focus_to(this, widget->next.down);
			if (changed) changed(this);
			return 1;
		default:
			break;
		}
		break;
	case SCHISM_KEYSYM_BACKSPACE:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_NUMENTRY) {
			if (widget->d.numentry.reverse) {
				/* woot! */
				widget->d.numentry.value /= 10;
				if (widget->changed) widget->changed(this);
				status.flags |= NEED_UPDATE;
				return 1;
			}
		}

		/* this ought to be in a separate function. */
		if (current_type != WIDGET_TEXTENTRY)
			break;
		if (!widget->d.textentry.text[0]) {
			/* nothing to do */
			return 1;
		}
		if (k->mod & SCHISM_KEYMOD_CTRL) {
			/* clear the whole field */
			widget->d.textentry.text[0] = 0;
			widget->d.textentry.cursor_pos = 0;
		} else {
			if (widget->d.textentry.cursor_pos == 0) {
				/* act like ST3 */
				text_delete_next_char(widget->d.textentry.text,
						      &(widget->d.textentry.cursor_pos),
						      widget->d.textentry.max_length);
			} else {
				text_delete_char(widget->d.textentry.text,
						 &(widget->d.textentry.cursor_pos),
						 widget->d.textentry.max_length);
			}
		}
		if (widget->changed) widget->changed(this);
		status.flags |= NEED_UPDATE;
		return 1;
	case SCHISM_KEYSYM_DELETE:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type != WIDGET_TEXTENTRY)
			break;
		if (!widget->d.textentry.text[0]) {
			/* nothing to do */
			return 1;
		}
		text_delete_next_char(widget->d.textentry.text,
				      &(widget->d.textentry.cursor_pos), widget->d.textentry.max_length);
		if (widget->changed) widget->changed(this);
		status.flags |= NEED_UPDATE;
		return 1;
	case SCHISM_KEYSYM_PLUS:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_NUMENTRY) {
			widget_numentry_change_value(widget, widget->d.numentry.value + 1);
			return 1;
		}
		break;
	case SCHISM_KEYSYM_MINUS:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_NUMENTRY && NO_MODIFIER(k->mod)) {
			widget_numentry_change_value(widget, widget->d.numentry.value - 1);
			return 1;
		}
		break;
	case SCHISM_KEYSYM_l:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_PANBAR) {
			if (k->mod & SCHISM_KEYMOD_ALT) {
				song_set_pan_scheme(PANS_LEFT);
				return 1;
			} else if (NO_MODIFIER(k->mod)) {
				widget->d.panbar.muted = 0;
				widget->d.panbar.surround = 0;
				widget_numentry_change_value(widget, 0);
				return 1;
			}
		}
		break;
	case SCHISM_KEYSYM_m:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_PANBAR) {
			if (k->mod & SCHISM_KEYMOD_ALT) {
				song_set_pan_scheme(PANS_MONO);
				return 1;
			} else if (NO_MODIFIER(k->mod)) {
				widget->d.panbar.muted = 0;
				widget->d.panbar.surround = 0;
				widget_numentry_change_value(widget, 32);
				return 1;
			}
		}
		break;
	case SCHISM_KEYSYM_r:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_PANBAR) {
			if (k->mod & SCHISM_KEYMOD_ALT) {
				song_set_pan_scheme(PANS_RIGHT);
				return 1;
			} else if (NO_MODIFIER(k->mod)) {
				widget->d.panbar.muted = 0;
				widget->d.panbar.surround = 0;
				widget_numentry_change_value(widget, 64);
				return 1;
			}
		}
		break;
	case SCHISM_KEYSYM_s:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_PANBAR) {
			if (k->mod & SCHISM_KEYMOD_ALT) {
				song_set_pan_scheme(PANS_STEREO);
				return 1;
			} else if(NO_MODIFIER(k->mod)) {
				widget->d.panbar.muted = 0;
				widget->d.panbar.surround = 1;
				if (widget->changed) widget->changed(this);
				status.flags |= NEED_UPDATE;
				return 1;
			}
		}
		break;
	case SCHISM_KEYSYM_a:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_PANBAR && (k->mod & SCHISM_KEYMOD_ALT)) {
			song_set_pan_scheme(PANS_AMIGA);
			return 1;
		}
		break;
#if 0
	case SCHISM_KEYSYM_x:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_PANBAR && (k->mod & SCHISM_KEYMOD_ALT)) {
			song_set_pan_scheme(PANS_CROSS);
			return 1;
		}
		break;
#endif
	case SCHISM_KEYSYM_SLASH:
	case SCHISM_KEYSYM_KP_DIVIDE:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_PANBAR && (k->mod & SCHISM_KEYMOD_ALT)) {
			song_set_pan_scheme(PANS_SLASH);
			return 1;
		}
		break;
	case SCHISM_KEYSYM_BACKSLASH:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_PANBAR && (k->mod & SCHISM_KEYMOD_ALT)) {
			song_set_pan_scheme(PANS_BACKSLASH);
			return 1;
		}
		break;
	default:
		/* this avoids a warning about all the values of an enum not being handled.
		(sheesh, it's already hundreds of lines long as it is!) */
		break;
	}
	if (status.flags & DISKWRITER_ACTIVE) return 0;

	/* if we're here, that mess didn't completely handle the key (gosh...) so now here's another mess. */
	switch (current_type) {
	case WIDGET_MENUTOGGLE:
		if (widget_menutoggle_handle_key(widget, k))
			return 1;
		break;
	case WIDGET_BITSET:
		if (widget_bitset_handle_key(widget, k))
			return 1;
		break;
	case WIDGET_THUMBBAR:
	case WIDGET_PANBAR:
		if (thumbbar_prompt_value(widget, k))
			return 1;
		break;
	case WIDGET_TEXTENTRY:
		if ((k->mod & (SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT | SCHISM_KEYMOD_GUI)) == 0 &&
			k->text && widget_textentry_add_text(widget, k->text))
			return 1;
		break;
	case WIDGET_NUMENTRY:
		if ((k->mod & (SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT | SCHISM_KEYMOD_GUI)) == 0 &&
			k->text && widget_numentry_handle_text(widget, k->text))
			return 1;

		/* weird hack ? */
		if (widget->d.numentry.handle_unknown_key)
			return widget->d.numentry.handle_unknown_key(this, k);
		break;
	default:
		break;
	}

	/* if we got down here the key wasn't handled */
	return 0;
}

