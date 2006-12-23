/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
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

#include "headers.h"

#include "clippy.h"

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
static void textentry_select(struct widget *w)
{
	if (w->clip_end < w->clip_start) {
		clippy_select(w, w->d.textentry.text + w->clip_end, w->clip_start-
			w->clip_end);
	} else if (w->clip_end > w->clip_start) {
		clippy_select(w, w->d.textentry.text + w->clip_start, w->clip_end -
			w->clip_start);
	}
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

/* --------------------------------------------------------------------- */
/* thumbbar value prompt */

static char thumbbar_prompt_buf[4];
static struct widget thumbbar_prompt_widgets[1];

/* this is bound to the textentry's activate callback.
since this dialog might be called from another dialog as well as from a page, it can't use the
normal dialog_yes handler -- it needs to destroy the prompt dialog first so that ACTIVE_WIDGET
points to whatever thumbbar actually triggered the dialog box. */
static void thumbbar_prompt_update(void)
{
	/* FIXME: what does IT do with invalid data?
	   entering "ack" is currently interpreted as zero, which is probably wrong */
	int n = atoi(thumbbar_prompt_buf);

	dialog_destroy();

	if (n >= ACTIVE_WIDGET.d.thumbbar.min && n <= ACTIVE_WIDGET.d.thumbbar.max) {
		ACTIVE_WIDGET.d.thumbbar.value = n;
		if (ACTIVE_WIDGET.changed) ACTIVE_WIDGET.changed();
	}

	status.flags |= NEED_UPDATE;
}

static void thumbbar_prompt_draw_const(void)
{
	draw_text((const unsigned char *)"Enter Value", 32, 26, 3, 2);
	draw_box(43, 25, 48, 27, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(44, 26, 47, 26, 0);
}

static int thumbbar_prompt_value(struct widget *widget, struct key_event *k)
{
	int c;
	const char *asciidigits = "0123456789";
	struct dialog *dialog;
	
	if (k->sym == SDLK_MINUS) {
		if (widget->d.thumbbar.min >= 0)
			return 0;
		c = '-';
	} else {
		c = numeric_key_event(k, 0);
		if (c == -1) return 0;
		c = asciidigits[c];
	}

	thumbbar_prompt_buf[0] = c;
	thumbbar_prompt_buf[1] = 0;

	create_textentry(thumbbar_prompt_widgets + 0, 44, 26, 4, 0, 0, 0, NULL, thumbbar_prompt_buf, 3);
	thumbbar_prompt_widgets[0].activate = thumbbar_prompt_update;
	thumbbar_prompt_widgets[0].d.textentry.cursor_pos = 1;

	dialog = dialog_create_custom(29, 24, 22, 5, thumbbar_prompt_widgets, 1, 0,
	                              thumbbar_prompt_draw_const, NULL);

	return 1;
}

/* --------------------------------------------------------------------- */
/* This function is completely disgustipated. */


static void _backtab(void)
{
	struct widget *w;
	int i;

	/* hunt for a widget that leads back to this one */
	if (!total_widgets || !selected_widget) return;

	for (i = 0; i < *total_widgets; i++) {
		w = &widgets[i];
		if (w->next.tab == *selected_widget) {
			/* found backtab */
			change_focus_to(i);
			return;
		}
		
	}
	if (status.flags & CLASSIC_MODE) {
		for (i = 0; i < *total_widgets; i++) {
			w = &widgets[i];
			if (w->next.right == *selected_widget) {
				/* simulate backtab */
				change_focus_to(i);
				return;
			}
		}
		for (i = 0; i < *total_widgets; i++) {
			w = &widgets[i];
			if (w->next.down == *selected_widget) {
				/* simulate backtab */
				change_focus_to(i);
				return;
			}
		}
	} else {
		for (i = 0; i < *total_widgets; i++) {
			w = &widgets[i];
			if (w->next.down == *selected_widget) {
				/* simulate backtab */
				change_focus_to(i);
				return;
			}
		}
		for (i = 0; i < *total_widgets; i++) {
			w = &widgets[i];
			if (w->next.right == *selected_widget) {
				/* simulate backtab */
				change_focus_to(i);
				return;
			}
		}
	}
	change_focus_to(0); /* err... */
}


/* return: 1 = handled key, 0 = didn't */
int widget_handle_key(struct key_event * k)
{
	struct widget *widget = &ACTIVE_WIDGET;
	int n, onw, wx, fmin, fmax, pad;
	enum widget_type current_type = widget->type;
	void (*changed)(void);

	if (!(status.flags & DISKWRITER_ACTIVE) 
	    && (current_type == WIDGET_OTHER)
	    && widget->d.other.handle_key(k))
		return 1;

	if (!(status.flags & DISKWRITER_ACTIVE) && k->mouse
            && (status.flags & CLASSIC_MODE)) {
		switch(current_type) {
		case WIDGET_NUMENTRY:
			if (k->mouse_button == MOUSE_BUTTON_LEFT) {
				k->sym = SDLK_MINUS;
				k->mouse = 0;
			} else if (k->mouse_button == MOUSE_BUTTON_RIGHT) {
				k->sym = SDLK_PLUS;
				k->mouse = 0;
			}
			break;
		default:
			break;
		};
	}

	if (k->mouse == MOUSE_CLICK
	    || (k->mouse == 0 && k->sym == SDLK_RETURN)) {
		if (k->mouse && k->mouse_button == MOUSE_BUTTON_MIDDLE) {
			if (status.flags & DISKWRITER_ACTIVE) return 0;
			if (!k->state) return 1;
			status.flags |= CLIPPY_PASTE_SELECTION;
			return 1;
		}
		if (k->mouse && (current_type == WIDGET_THUMBBAR
		|| current_type == WIDGET_PANBAR)) {
			if (status.flags & DISKWRITER_ACTIVE) return 0;

			/* swallow it */
			if (!k->on_target) return 0;

			fmin = widget->d.thumbbar.min;
			fmax = widget->d.thumbbar.max;
			if (current_type == WIDGET_PANBAR) {
				n = k->fx - ((widget->x + 11) * k->rx);
				wx = (widget->width - 16) * k->rx;
			} else {
				n = k->fx - (widget->x * k->rx);
				wx = (widget->width-1) * k->rx;
			}
			if (n < 0) n = 0;
			else if (n >= wx) n = wx;
			n = fmin + ((n * (fmax - fmin)) / wx);
				
			if (n < fmin) 
				n = fmin;
			else if (n > fmax)
				n = fmax;
			if (current_type == WIDGET_PANBAR) {
				widget->d.panbar.muted = 0;
				widget->d.panbar.surround = 0;
				if (k->x - widget->x < 11) return 1;
				if (k->x - widget->x > 19) return 1;
			}
			numentry_change_value(widget, n);
			return 1;
		}
		if (k->mouse) {
			switch (widget->type) {
			case WIDGET_BUTTON:
				pad = widget->d.button.padding+1;
				break;
			case WIDGET_TOGGLEBUTTON:
				pad = widget->d.togglebutton.padding+1;
				break;
			default:
				pad = 0;
			};
			onw = ((signed) k->x < widget->x
			       || (signed) k->x >= widget->x + widget->width + pad
			       || (signed) k->y != widget->y) ? 0 : 1;
			n = ((!k->state) && onw) ? 1 : 0;
			if (widget->depressed != n) status.flags |= NEED_UPDATE;
			widget->depressed = n;
			if (current_type != WIDGET_TEXTENTRY) {
				if (!k->state || !onw) return 1;
			} else if (!onw) return 1;
		} else {
			n = (!k->state) ? 1 : 0;
			if (widget->depressed != n)
				status.flags |= NEED_UPDATE;
			else if (k->state) return 1; // swallor
			widget->depressed = n;
			if (!k->state) return 1;
		}

		if (k->mouse) {
			switch(current_type) {
			case WIDGET_MENUTOGGLE:
			case WIDGET_BUTTON:
			case WIDGET_TOGGLEBUTTON:
				if (k->on_target && widget->activate) widget->activate();
			default:
				break;
			};
		} else if (current_type != WIDGET_OTHER) {
			if (widget->activate) widget->activate();
		}

		switch (current_type) {
		case WIDGET_OTHER:
			break;
		case WIDGET_TEXTENTRY:
			if (status.flags & DISKWRITER_ACTIVE) return 0;
			/* LOL WOW THIS SUCKS */
			if ((k->mouse == MOUSE_CLICK)
			    && (k->on_target
				|| (k->state
				    && widget == clippy_owner(CLIPPY_SELECT)))) {
				/* position cursor */
				n = k->x - widget->x;
				n = CLAMP(n, 0, widget->width - 1);
				wx = k->sx - widget->x;
				wx = CLAMP(wx, 0, widget->width - 1);
				widget->d.textentry.cursor_pos = n+widget->d.textentry.firstchar;
				wx  = wx+widget->d.textentry.firstchar;
				if (widget->d.textentry.cursor_pos >= (signed) strlen(widget->d.textentry.text))
					widget->d.textentry.cursor_pos = strlen(widget->d.textentry.text);
				if (wx >= (signed) strlen(widget->d.textentry.text))
					wx = strlen(widget->d.textentry.text);
				if (k->sx != k->x || k->sy != k->y) {
					widget->clip_start = wx;
					widget->clip_end = widget->d.textentry.cursor_pos;
					textentry_select(widget);
					status.flags |= NEED_UPDATE;
				}
			}

			/* for a text entry, the only thing enter does is run the activate callback.
			thus, if no activate callback is defined, the key wasn't handled */
			return (widget->activate != NULL);
		case WIDGET_TOGGLEBUTTON:
			if (status.flags & DISKWRITER_ACTIVE) return 0;
			if (widget->d.togglebutton.group) {
				/* this also runs the changed callback and redraws the button(s) */
				togglebutton_set(widgets, *selected_widget, 1);
				return 1;
			}
			/* else... */
			widget->d.togglebutton.state = !widget->d.togglebutton.state;
			/* and fall through */
		case WIDGET_BUTTON:
			/* maybe buttons should ignore the changed callback, and use activate instead...
			(but still call the changed callback for togglebuttons if they *actually* changed) */
			if (widget->changed) widget->changed();
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_MENUTOGGLE:
			if (status.flags & DISKWRITER_ACTIVE) return 0;
			if (status.flags & CLASSIC_MODE) {
				widget->d.menutoggle.state = (widget->d.menutoggle.state + 1)
						% widget->d.menutoggle.num_choices;
				if (widget->changed) widget->changed();
				status.flags |= NEED_UPDATE;
				return 1;
			}
			/* ... */
		default:
			break;
		}
		return 0;
	}

	if (k->mouse == MOUSE_DBLCLICK) {
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		switch (current_type) {
		case WIDGET_TOGGLE:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->d.toggle.state = !widget->d.toggle.state;
			if (widget->changed) widget->changed();
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_MENUTOGGLE:
			if (!NO_MODIFIER(k->mod))
				return 0;
			if (status.flags & CLASSIC_MODE) return 0;
			widget->d.menutoggle.state = (widget->d.menutoggle.state + 1) % widget->d.menutoggle.num_choices;
			if (widget->changed) widget->changed();
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_PANBAR:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->d.panbar.muted = !widget->d.panbar.muted;
			changed = widget->changed;
			change_focus_to(widget->next.down);
			if (changed) changed();
			return 1;
		default:
			break;
		}
	}

	/* a WIDGET_OTHER that *didn't* handle the key itself needs to get run through the switch
	statement to account for stuff like the tab key */
	if (k->state) return 0;

	if (k->mouse == MOUSE_SCROLL_UP && current_type == WIDGET_NUMENTRY) {
		k->sym = SDLK_MINUS;
	} else if (k->mouse == MOUSE_SCROLL_DOWN && current_type == WIDGET_NUMENTRY) {
		k->sym = SDLK_PLUS;
	}

	switch (k->sym) {
	case SDLK_ESCAPE:
		/* this is to keep the text entries from taking the key hostage and inserting '<-'
		characters instead of showing the menu */
		return 0;
	case SDLK_UP:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		change_focus_to(widget->next.up);
		return 1;
	case SDLK_DOWN:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		change_focus_to(widget->next.down);
		return 1;
	case SDLK_TAB:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (k->mod & KMOD_SHIFT) {
			_backtab();
			return 1;
		}
		if (!NO_MODIFIER(k->mod))
			return 0;
		change_focus_to(widget->next.tab);
		return 1;
	case SDLK_LEFT:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		switch (current_type) {
		case WIDGET_NUMENTRY:
			if (!NO_MODIFIER(k->mod))
				return 0;
			numentry_move_cursor(widget, -1);
			return 1;
		case WIDGET_TEXTENTRY:
			if (k->mod & KMOD_SHIFT) {
				if (clippy_owner(CLIPPY_SELECT) != widget) {
					widget->clip_start = widget->d.textentry.cursor_pos;
					widget->clip_end = widget->clip_start-1;
				}
				widget->clip_end--;
				if (widget->clip_end < 0) widget->clip_end = 0;
				textentry_select(widget);
			} else if (!NO_MODIFIER(k->mod)) {
				return 0;
			} else {
				clippy_select(0,0,0);
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
			if (k->mod & (KMOD_ALT | KMOD_META))
				n *= 8;
			if (k->mod & KMOD_SHIFT)
				n *= 4;
			if (k->mod & KMOD_CTRL)
				n *= 2;
			n = widget->d.numentry.value - n;
			numentry_change_value(widget, n);
			return 1;
		default:
			if (!NO_MODIFIER(k->mod))
				return 0;
			change_focus_to(widget->next.left);
			return 1;
		}
		break;
	case SDLK_RIGHT:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		/* pretty much the same as left, but with a few small
		 * changes here and there... */
		switch (current_type) {
		case WIDGET_NUMENTRY:
			if (!NO_MODIFIER(k->mod))
				return 0;
			numentry_move_cursor(widget, 1);
			return 1;
		case WIDGET_TEXTENTRY:
			if (k->mod & KMOD_SHIFT) {
				if (clippy_owner(CLIPPY_SELECT) != widget) {
					widget->clip_start = widget->d.textentry.cursor_pos;
					widget->clip_end = widget->clip_start + 1;
				}
				widget->clip_end++;
				if (widget->clip_end > (signed) strlen(widget->d.textentry.text))
					widget->clip_end = strlen(widget->d.textentry.text);
				textentry_select(widget);
			} else if (!NO_MODIFIER(k->mod)) {
				return 0;
			} else {
				clippy_select(0,0,0);
			}
			textentry_move_cursor(widget, 1);
			return 1;
		case WIDGET_PANBAR:
			widget->d.panbar.muted = 0;
			widget->d.panbar.surround = 0;
			/* fall through */
		case WIDGET_THUMBBAR:
			n = 1;
			if (k->mod & (KMOD_ALT | KMOD_META))
				n *= 8;
			if (k->mod & KMOD_SHIFT)
				n *= 4;
			if (k->mod & KMOD_CTRL)
				n *= 2;
			n = widget->d.numentry.value + n;
			numentry_change_value(widget, n);
			return 1;
		default:
			if (!NO_MODIFIER(k->mod))
				return 0;
			change_focus_to(widget->next.right);
			return 1;
		}
		break;
	case SDLK_HOME:
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
			numentry_change_value(widget, n);
			return 1;
		default:
			break;
		}
		break;
	case SDLK_END:
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
			numentry_change_value(widget, n);
			return 1;
		default:
			break;
		}
		break;
	case SDLK_SPACE:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		switch (current_type) {
		case WIDGET_TOGGLE:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->d.toggle.state = !widget->d.toggle.state;
			if (widget->changed) widget->changed();
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_MENUTOGGLE:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->d.menutoggle.state = (widget->d.menutoggle.state + 1) % widget->d.menutoggle.num_choices;
			if (widget->changed) widget->changed();
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_PANBAR:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->d.panbar.muted = !widget->d.panbar.muted;
			changed = widget->changed;
			change_focus_to(widget->next.down);
			if (changed) changed();
			return 1;
		default:
			break;
		}
		break;
	case SDLK_BACKSPACE:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_NUMENTRY) {
			if (widget->d.numentry.reverse) {
				/* woot! */
				widget->d.numentry.value /= 10;
				if (widget->changed) widget->changed();
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
		if (k->mod & KMOD_CTRL) {
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
						 &(widget->d.textentry.cursor_pos), widget->d.textentry.max_length);
			}
		}
		if (widget->changed) widget->changed();
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_DELETE:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type != WIDGET_TEXTENTRY)
			break;
		if (!widget->d.textentry.text[0]) {
			/* nothing to do */
			return 1;
		}
		text_delete_next_char(widget->d.textentry.text,
				      &(widget->d.textentry.cursor_pos), widget->d.textentry.max_length);
		if (widget->changed) widget->changed();
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_PLUS:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_NUMENTRY && NO_MODIFIER(k->mod)) {
			numentry_change_value(widget, widget->d.numentry.value + 1);
			return 1;
		}
		break;
	case SDLK_MINUS:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_NUMENTRY && NO_MODIFIER(k->mod)) {
			numentry_change_value(widget, widget->d.numentry.value - 1);
			return 1;
		}
		break;
	case SDLK_l:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_PANBAR && NO_MODIFIER(k->mod)) {
			widget->d.panbar.muted = 0;
			widget->d.panbar.surround = 0;
			numentry_change_value(widget, 0);
			return 1;
		}
		break;
	case SDLK_m:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_PANBAR && NO_MODIFIER(k->mod)) {
			widget->d.panbar.muted = 0;
			widget->d.panbar.surround = 0;
			numentry_change_value(widget, 32);
			return 1;
		}
		break;
	case SDLK_r:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_PANBAR && NO_MODIFIER(k->mod)) {
			widget->d.panbar.muted = 0;
			widget->d.panbar.surround = 0;
			numentry_change_value(widget, 64);
			return 1;
		}
		break;
	case SDLK_s:
		if (status.flags & DISKWRITER_ACTIVE) return 0;
		if (current_type == WIDGET_PANBAR && NO_MODIFIER(k->mod)) {
			widget->d.panbar.muted = 0;
			widget->d.panbar.surround = 1;
			if (widget->changed) widget->changed();
			status.flags |= NEED_UPDATE;
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
	case WIDGET_NUMENTRY:
		if (numentry_handle_digit(widget, k))
			return 1;
		break;
	case WIDGET_THUMBBAR:
	case WIDGET_PANBAR:
		if (thumbbar_prompt_value(widget, k))
			return 1;
		break;
	case WIDGET_TEXTENTRY:
		if ((k->mod & (KMOD_CTRL | KMOD_ALT | KMOD_META)) == 0
				&& textentry_add_char(widget, k->unicode))
			return 1;
		break;
	default:
		break;
	}

	/* if we got down here the key wasn't handled */
	return 0;
}
