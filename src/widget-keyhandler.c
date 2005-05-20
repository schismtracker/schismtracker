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

#include "headers.h"

#include "it.h"
#include "page.h"

#include <SDL.h>

/* --------------------------------------------------------------------- */

/* n => the delta-value */
static void numentry_move_cursor(struct widget *widget, int n)
{
	n += *(widget->numentry.cursor_pos);
	n = CLAMP(n, 0, widget->width - 1);
	if (*(widget->numentry.cursor_pos) == n)
		return;
	*(widget->numentry.cursor_pos) = n;
	status.flags |= NEED_UPDATE;
}

static void textentry_move_cursor(struct widget *widget, int n)
{
	n += widget->textentry.cursor_pos;
	n = CLAMP(n, 0, widget->textentry.max_length);
	if (widget->textentry.cursor_pos == n)
		return;
	widget->textentry.cursor_pos = n;
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
	int n = atoi(thumbbar_prompt_buf);

	dialog_destroy();

	if (n >= ACTIVE_WIDGET.thumbbar.min && n <= ACTIVE_WIDGET.thumbbar.max) {
		ACTIVE_WIDGET.thumbbar.value = n;
		RUN_IF(ACTIVE_WIDGET.changed);
	}

	status.flags |= NEED_UPDATE;
}

static void thumbbar_prompt_draw_const(void)
{
	draw_text("Enter Value", 32, 26, 3, 2);
	draw_box(43, 25, 48, 27, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(44, 26, 47, 26, 0);
}

static int thumbbar_prompt_value(UNUSED struct widget *widget, Uint16 unicode)
{
	char c = unicode_to_ascii(unicode);
	struct dialog *dialog;
	
	if ((c < '0' || c > '9') && c != '-')
		return 0;

	thumbbar_prompt_buf[0] = c;
	thumbbar_prompt_buf[1] = 0;

	create_textentry(thumbbar_prompt_widgets + 0, 44, 26, 4, 0, 0, 0, NULL, thumbbar_prompt_buf, 3);
	thumbbar_prompt_widgets[0].activate = thumbbar_prompt_update;
	thumbbar_prompt_widgets[0].textentry.cursor_pos = 1;

	dialog = dialog_create_custom(29, 24, 22, 5, thumbbar_prompt_widgets, 1, 0,
	                              thumbbar_prompt_draw_const, NULL);

	return 1;
}

/* --------------------------------------------------------------------- */
/* This function is completely disgustipated. */

/* return: 1 = handled key, 0 = didn't */
int widget_handle_key(SDL_keysym * k)
{
	struct widget *widget = &ACTIVE_WIDGET;
	int n;
	enum widget_type current_type = widget->type;

	if (current_type == WIDGET_OTHER && widget->other.handle_key(k))
		return 1;

	/* a WIDGET_OTHER that *didn't* handle the key itself needs to get run through the switch
	statement to account for stuff like the tab key */

	switch (k->sym) {
	case SDLK_ESCAPE:
		/* this is to keep the text entries from taking the key hostage and inserting '<-'
		characters instead of showing the menu */
		return 0;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (current_type != WIDGET_OTHER)
			RUN_IF(widget->activate);

		switch (current_type) {
		case WIDGET_OTHER:
			break;
		case WIDGET_TEXTENTRY:
			/* for a text entry, the only thing enter does is run the activate callback.
			thus, if no activate callback is defined, the key wasn't handled */
			return (widget->activate != NULL);
		case WIDGET_TOGGLEBUTTON:
			if (widget->togglebutton.group) {
				/* this also runs the changed callback and redraws the button(s) */
				togglebutton_set(widgets, *selected_widget, 1);
				return 1;
			}
			/* else... */
			widget->togglebutton.state = !widget->togglebutton.state;
			/* and fall through */
		case WIDGET_BUTTON:
			/* maybe buttons should ignore the changed callback, and use activate instead...
			(but still call the changed callback for togglebuttons if they *actually* changed) */
			RUN_IF(widget->changed);
			status.flags |= NEED_UPDATE;
			return 1;
		default:
			break;
		}
		break;
	case SDLK_UP:
		if (!NO_MODIFIER(k->mod))
			return 0;
		change_focus_to(widget->next.up);
		return 1;
	case SDLK_DOWN:
		if (!NO_MODIFIER(k->mod))
			return 0;
		change_focus_to(widget->next.down);
		return 1;
	case SDLK_TAB:
		if (!NO_MODIFIER(k->mod))
			return 0;
		change_focus_to(widget->next.tab);
		return 1;
	case SDLK_LEFT:
		switch (current_type) {
		case WIDGET_NUMENTRY:
			if (!NO_MODIFIER(k->mod))
				return 0;
			numentry_move_cursor(widget, -1);
			return 1;
		case WIDGET_TEXTENTRY:
			if (!NO_MODIFIER(k->mod))
				return 0;
			textentry_move_cursor(widget, -1);
			return 1;
		case WIDGET_PANBAR:
			widget->panbar.muted = 0;
			widget->panbar.surround = 0;
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
			n = widget->numentry.value - n;
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
		/* pretty much the same as left, but with a few small
		 * changes here and there... */
		switch (current_type) {
		case WIDGET_NUMENTRY:
			if (!NO_MODIFIER(k->mod))
				return 0;
			numentry_move_cursor(widget, 1);
			return 1;
		case WIDGET_TEXTENTRY:
			if (!NO_MODIFIER(k->mod))
				return 0;
			textentry_move_cursor(widget, 1);
			return 1;
		case WIDGET_PANBAR:
			widget->panbar.muted = 0;
			widget->panbar.surround = 0;
			/* fall through */
		case WIDGET_THUMBBAR:
			n = 1;
			if (k->mod & (KMOD_ALT | KMOD_META))
				n *= 8;
			if (k->mod & KMOD_SHIFT)
				n *= 4;
			if (k->mod & KMOD_CTRL)
				n *= 2;
			n = widget->numentry.value + n;
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
		/* Impulse Tracker only does home/end for the thumbbars.
		 * This stuff is all extra. */
		switch (current_type) {
		case WIDGET_NUMENTRY:
			if (!NO_MODIFIER(k->mod))
				return 0;
			*(widget->numentry.cursor_pos) = 0;
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_TEXTENTRY:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->textentry.cursor_pos = 0;
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_PANBAR:
			widget->panbar.muted = 0;
			widget->panbar.surround = 0;
			/* fall through */
		case WIDGET_THUMBBAR:
			n = widget->thumbbar.min;
			numentry_change_value(widget, n);
			return 1;
		default:
			break;
		}
		break;
	case SDLK_END:
		switch (current_type) {
		case WIDGET_NUMENTRY:
			if (!NO_MODIFIER(k->mod))
				return 0;
			*(widget->numentry.cursor_pos) = widget->width - 1;
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_TEXTENTRY:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->textentry.cursor_pos = strlen(widget->textentry.text);
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_PANBAR:
			widget->panbar.muted = 0;
			widget->panbar.surround = 0;
			/* fall through */
		case WIDGET_THUMBBAR:
			n = widget->thumbbar.max;
			numentry_change_value(widget, n);
			return 1;
		default:
			break;
		}
		break;
	case SDLK_SPACE:
		switch (current_type) {
		case WIDGET_TOGGLE:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->toggle.state = !widget->toggle.state;
			RUN_IF(widget->changed);
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_MENUTOGGLE:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->menutoggle.state = (widget->menutoggle.state + 1) % widget->menutoggle.num_choices;
			RUN_IF(widget->changed);
			status.flags |= NEED_UPDATE;
			return 1;
		case WIDGET_PANBAR:
			if (!NO_MODIFIER(k->mod))
				return 0;
			widget->panbar.muted = !widget->panbar.muted;
			RUN_IF(widget->changed);
			change_focus_to(widget->next.down);
			return 1;
		default:
			break;
		}
		break;
	case SDLK_BACKSPACE:
		/* this ought to be in a separate function. */
		if (current_type != WIDGET_TEXTENTRY)
			break;
		if (!widget->textentry.text[0]) {
			/* nothing to do */
			return 1;
		}
		if (k->mod & KMOD_CTRL) {
			/* clear the whole field */
			widget->textentry.text[0] = 0;
			widget->textentry.cursor_pos = 0;
		} else {
			if (widget->textentry.cursor_pos == 0) {
				/* act like ST3 */
				text_delete_next_char(widget->textentry.text,
						      &(widget->textentry.cursor_pos),
						      widget->textentry.max_length);
			} else {
				text_delete_char(widget->textentry.text,
						 &(widget->textentry.cursor_pos), widget->textentry.max_length);
			}
		}
		RUN_IF(widget->changed);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_DELETE:
		if (current_type != WIDGET_TEXTENTRY)
			break;
		if (!widget->textentry.text[0]) {
			/* nothing to do */
			return 1;
		}
		text_delete_next_char(widget->textentry.text,
				      &(widget->textentry.cursor_pos), widget->textentry.max_length);
		RUN_IF(widget->changed);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_PLUS:
	case SDLK_KP_PLUS:
		if (current_type == WIDGET_NUMENTRY && NO_MODIFIER(k->mod)) {
			numentry_change_value(widget, widget->numentry.value + 1);
			return 1;
		}
		break;
	case SDLK_MINUS:
	case SDLK_KP_MINUS:
		if (current_type == WIDGET_NUMENTRY && NO_MODIFIER(k->mod)) {
			numentry_change_value(widget, widget->numentry.value - 1);
			return 1;
		}
		break;
	case SDLK_l:
		if (current_type == WIDGET_PANBAR && NO_MODIFIER(k->mod)) {
			widget->panbar.muted = 0;
			widget->panbar.surround = 0;
			numentry_change_value(widget, 0);
			return 1;
		}
		break;
	case SDLK_m:
		if (current_type == WIDGET_PANBAR && NO_MODIFIER(k->mod)) {
			widget->panbar.muted = 0;
			widget->panbar.surround = 0;
			numentry_change_value(widget, 32);
			return 1;
		}
		break;
	case SDLK_r:
		if (current_type == WIDGET_PANBAR && NO_MODIFIER(k->mod)) {
			widget->panbar.muted = 0;
			widget->panbar.surround = 0;
			numentry_change_value(widget, 64);
			return 1;
		}
		break;
	case SDLK_s:
		if (current_type == WIDGET_PANBAR && NO_MODIFIER(k->mod)) {
			widget->panbar.muted = 0;
			widget->panbar.surround = 1;
			RUN_IF(widget->changed);
			status.flags |= NEED_UPDATE;
			return 1;
		}
		break;
	default:
		/* this avoids a warning about all the values of an enum not being handled.
		(sheesh, it's already hundreds of lines long as it is!) */
		break;
	}

	/* if we're here, that mess didn't completely handle the key (gosh...) so now here's another mess. */
	switch (current_type) {
	case WIDGET_NUMENTRY:
		if (numentry_handle_digit(widget, k->unicode))
			return 1;
		break;
	case WIDGET_THUMBBAR:
	case WIDGET_PANBAR:
		if (thumbbar_prompt_value(widget, k->unicode))
			return 1;
		break;
	case WIDGET_TEXTENTRY:
		if ((k->mod & (KMOD_CTRL | KMOD_ALT | KMOD_META)) == 0 && textentry_add_char(widget, k->unicode))
			return 1;
		break;
	default:
		break;
	}

	/* if we got down here the key wasn't handled */
	return 0;
}
