/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
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

#include <SDL.h>

#include "it.h"
#include "page.h"

/* --------------------------------------------------------------------- */

/* n => the delta-value */
static void numentry_move_cursor(struct item *item, int n)
{
	n += *(item->numentry.cursor_pos);
	n = CLAMP(n, 0, item->width - 1);
	if (*(item->numentry.cursor_pos) == n)
		return;
	*(item->numentry.cursor_pos) = n;
	status.flags |= NEED_UPDATE;
}

static void textentry_move_cursor(struct item *item, int n)
{
	n += item->textentry.cursor_pos;
	n = CLAMP(n, 0, item->textentry.max_length);
	if (item->textentry.cursor_pos == n)
		return;
	item->textentry.cursor_pos = n;
	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* thumbbar value prompt */

static char thumbbar_prompt_buf[4];
static struct item thumbbar_prompt_items[1];

/* this is bound to the textentry's activate callback.
 * since this dialog might be called from another dialog as well as from a page, it can't use the normal
 * dialog_yes handler -- it needs to destroy the prompt dialog first so that ACTIVE_ITEM points to whatever
 * thumbbar actually triggered the dialog box. */
static void thumbbar_prompt_update(void)
{
	int n = atoi(thumbbar_prompt_buf);

	dialog_destroy();

	if (n >= ACTIVE_ITEM.thumbbar.min && n <= ACTIVE_ITEM.thumbbar.max) {
		ACTIVE_ITEM.thumbbar.value = n;
		RUN_IF(ACTIVE_ITEM.changed);
	}

	status.flags |= NEED_UPDATE;
}

static void thumbbar_prompt_draw_const(void)
{
	draw_text("Enter Value", 32, 26, 3, 2);
	draw_box(43, 25, 48, 27, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(44, 26, 47, 26, 0);
}

static int thumbbar_prompt_value(UNUSED struct item *item, Uint16 unicode)
{
	char c = unicode_to_ascii(unicode);
	struct dialog *dialog;
	
	if ((c < '0' || c > '9') && c != '-')
		return 0;

	thumbbar_prompt_buf[0] = c;
	thumbbar_prompt_buf[1] = 0;

	create_textentry(thumbbar_prompt_items + 0, 44, 26, 4, 0, 0, 0, NULL, thumbbar_prompt_buf, 3);
	thumbbar_prompt_items[0].activate = thumbbar_prompt_update;
	thumbbar_prompt_items[0].textentry.cursor_pos = 1;

	dialog = dialog_create_custom(29, 24, 22, 5, thumbbar_prompt_items, 1, 0, thumbbar_prompt_draw_const);

	return 1;
}

/* --------------------------------------------------------------------- */
/* This function is completely disgustipated. */

/* return: 1 = handled key, 0 = didn't */
int item_handle_key(SDL_keysym * k)
{
	struct item *item = &ACTIVE_ITEM;
	int n;
	enum item_type current_type = item->type;

	if (current_type == ITEM_OTHER && item->other.handle_key(k))
		return 1;

	/* an ITEM_OTHER that *didn't* handle the key itself needs to get
	 * run through the switch statement to account for stuff like the
	 * tab key */

	switch (k->sym) {
	case SDLK_ESCAPE:
		/* this is to keep the text entries from taking the key
		 * hostage and inserting '<-' characters instead of
		 * showing the menu */
		return 0;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (current_type != ITEM_OTHER)
			RUN_IF(item->activate);

		switch (current_type) {
		case ITEM_OTHER:
			break;
		case ITEM_TEXTENTRY:
			/* ????...
			 * RUN_IF(item->changed);
			 * status.flags |= NEED_UPDATE; */
			return 1;
		case ITEM_TOGGLEBUTTON:
			if (item->togglebutton.group) {
				/* this also runs the changed callback
				 * and redraws the button(s) */
				togglebutton_set(items, *selected_item, 1);
				return 1;
			}
			/* else... */
			item->togglebutton.state = !item->togglebutton.state;
			/* and fall through */
		case ITEM_BUTTON:
			/* maybe buttons should ignore the changed callback,
			 * and use activate instead...
			 * (but have togglebuttons still call the changed
			 * callback if they *actually* changed) */
			RUN_IF(item->changed);
			status.flags |= NEED_UPDATE;
			return 1;
		default:
			/* ????...
			 * RUN_IF(item->changed);
			 * status.flags |= NEED_UPDATE;
			 * return 1; */
			break;
		}
		break;
	case SDLK_UP:
		change_focus_to(item->next.up);
		return 1;
	case SDLK_DOWN:
		change_focus_to(item->next.down);
		return 1;
	case SDLK_TAB:
		change_focus_to(item->next.tab);
		return 1;
	case SDLK_LEFT:
		switch (current_type) {
		case ITEM_NUMENTRY:
			numentry_move_cursor(item, -1);
			return 1;
		case ITEM_TEXTENTRY:
			textentry_move_cursor(item, -1);
			return 1;
		case ITEM_PANBAR:
			item->panbar.muted = 0;
			item->panbar.surround = 0;
			/* fall through */
		case ITEM_THUMBBAR:
			/* I'm handling the key modifiers differently than
			 * Impulse Tracker, but only because I think this
			 * is much more useful. :) */
			n = 1;
			if (k->mod & (KMOD_ALT | KMOD_META))
				n *= 8;
			if (k->mod & KMOD_SHIFT)
				n *= 4;
			if (k->mod & KMOD_CTRL)
				n *= 2;
			n = item->numentry.value - n;
			numentry_change_value(item, n);
			return 1;
		default:
			change_focus_to(item->next.left);
			return 1;
		}
		break;
	case SDLK_RIGHT:
		/* pretty much the same as left, but with a few small
		 * changes here and there... */
		switch (current_type) {
		case ITEM_NUMENTRY:
			numentry_move_cursor(item, 1);
			return 1;
		case ITEM_TEXTENTRY:
			textentry_move_cursor(item, 1);
			return 1;
		case ITEM_PANBAR:
			item->panbar.muted = 0;
			item->panbar.surround = 0;
			/* fall through */
		case ITEM_THUMBBAR:
			n = 1;
			if (k->mod & (KMOD_ALT | KMOD_META))
				n *= 8;
			if (k->mod & KMOD_SHIFT)
				n *= 4;
			if (k->mod & KMOD_CTRL)
				n *= 2;
			n = item->numentry.value + n;
			numentry_change_value(item, n);
			return 1;
		default:
			change_focus_to(item->next.right);
			return 1;
		}
		break;
	case SDLK_HOME:
		/* Impulse Tracker only does home/end for the thumbbars.
		 * This stuff is all extra. */
		switch (current_type) {
		case ITEM_NUMENTRY:
			*(item->numentry.cursor_pos) = 0;
			status.flags |= NEED_UPDATE;
			return 1;
		case ITEM_TEXTENTRY:
			item->textentry.cursor_pos = 0;
			status.flags |= NEED_UPDATE;
			return 1;
		case ITEM_PANBAR:
			item->panbar.muted = 0;
			item->panbar.surround = 0;
			/* fall through */
		case ITEM_THUMBBAR:
			n = item->thumbbar.min;
			numentry_change_value(item, n);
			return 1;
		default:
			break;
		}
		break;
	case SDLK_END:
		switch (current_type) {
		case ITEM_NUMENTRY:
			*(item->numentry.cursor_pos) = item->width - 1;
			status.flags |= NEED_UPDATE;
			return 1;
		case ITEM_TEXTENTRY:
			item->textentry.cursor_pos = strlen(item->textentry.text);
			status.flags |= NEED_UPDATE;
			return 1;
		case ITEM_PANBAR:
			item->panbar.muted = 0;
			item->panbar.surround = 0;
			/* fall through */
		case ITEM_THUMBBAR:
			n = item->thumbbar.max;
			numentry_change_value(item, n);
			return 1;
		default:
			break;
		}
		break;
	case SDLK_SPACE:
		switch (current_type) {
		case ITEM_TOGGLE:
			item->toggle.state = !item->toggle.state;
			RUN_IF(item->changed);
			status.flags |= NEED_UPDATE;
			return 1;
		case ITEM_MENUTOGGLE:
			item->menutoggle.state = (item->menutoggle.state + 1) % item->menutoggle.num_choices;
			RUN_IF(item->changed);
			status.flags |= NEED_UPDATE;
			return 1;
		case ITEM_PANBAR:
			item->panbar.muted = !item->panbar.muted;
			RUN_IF(item->changed);
			change_focus_to(item->next.down);
			return 1;
		default:
			break;
		}
		break;
	case SDLK_BACKSPACE:
		/* this ought to be in a separate function. */
		if (current_type != ITEM_TEXTENTRY)
			break;
		if (!item->textentry.text[0]) {
			/* nothing to do */
			return 1;
		}
		if (k->mod & KMOD_CTRL) {
			/* clear the whole field */
			item->textentry.text[0] = 0;
			item->textentry.cursor_pos = 0;
		} else {
			if (item->textentry.cursor_pos == 0) {
				/* act like ST3 */
				text_delete_next_char(item->textentry.text,
						      &(item->textentry.cursor_pos),
						      item->textentry.max_length);
			} else {
				text_delete_char(item->textentry.text,
						 &(item->textentry.cursor_pos), item->textentry.max_length);
			}
		}
		RUN_IF(item->changed);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_DELETE:
		if (current_type != ITEM_TEXTENTRY)
			break;
		if (!item->textentry.text[0]) {
			/* nothing to do */
			return 1;
		}
		text_delete_next_char(item->textentry.text,
				      &(item->textentry.cursor_pos), item->textentry.max_length);
		RUN_IF(item->changed);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_PLUS:
	case SDLK_KP_PLUS:
		if (current_type == ITEM_NUMENTRY && NO_MODIFIER(k->mod)) {
			numentry_change_value(item, item->numentry.value + 1);
			return 1;
		}
		break;
	case SDLK_MINUS:
	case SDLK_KP_MINUS:
		if (current_type == ITEM_NUMENTRY && NO_MODIFIER(k->mod)) {
			numentry_change_value(item, item->numentry.value - 1);
			return 1;
		}
		break;
	case SDLK_l:
		if (current_type == ITEM_PANBAR) {
			item->panbar.muted = 0;
			item->panbar.surround = 0;
			numentry_change_value(item, 0);
			return 1;
		}
		break;
	case SDLK_m:
		if (current_type == ITEM_PANBAR) {
			item->panbar.muted = 0;
			item->panbar.surround = 0;
			numentry_change_value(item, 32);
			return 1;
		}
		break;
	case SDLK_r:
		if (current_type == ITEM_PANBAR) {
			item->panbar.muted = 0;
			item->panbar.surround = 0;
			numentry_change_value(item, 64);
			return 1;
		}
		break;
	case SDLK_s:
		if (current_type == ITEM_PANBAR) {
			item->panbar.muted = 0;
			item->panbar.surround = 1;
			RUN_IF(item->changed);
			status.flags |= NEED_UPDATE;
			return 1;
		}
		break;
	default:
		/* this avoids a warning about all the values of an enum
		 * not being handled. (sheesh, it's already hundreds of
		 * lines long as it is!) */
		break;
	}

	/* if we're here, that mess didn't completely handle the key
	 * (gosh...) so now here's another mess. */
	switch (current_type) {
	case ITEM_NUMENTRY:
		if (numentry_handle_digit(item, k->unicode))
			return 1;
		break;
	case ITEM_THUMBBAR:
	case ITEM_PANBAR:
		if (thumbbar_prompt_value(item, k->unicode))
			return 1;
		break;
	case ITEM_TEXTENTRY:
		if ((k->mod & (KMOD_CTRL | KMOD_ALT | KMOD_META)) == 0 && textentry_add_char(item, k->unicode))
			return 1;
		break;
	default:
		break;
	}

	/* if we got down here the key wasn't handled */
	return 0;
}
