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
#include "dialog.h"
#include "vgamem.h"
#include "widget.h"
#include "song.h"
#include "page.h"
#include "keyboard.h"
#include "mem.h"

#include <ctype.h>

/* --------------------------------------------------------------------- */

/* ENSURE_DIALOG(optional return value)
 * will emit a warning and cause the function to return
 * if a dialog is not active. */
#ifndef NDEBUG
# define ENSURE_DIALOG(q) do { if (!(status.dialog_type & DIALOG_BOX)) { \
		fprintf(stderr, "%s called with no dialog\n", __func__);\
		q; \
	} \
} while(0)
#else
# define ENSURE_DIALOG(q)
#endif

/* --------------------------------------------------------------------- */
/* I'm only supporting four dialogs open at a time. This is an absurdly
 * large amount anyway, since the most that should ever be displayed is
 * two (in the case of a custom dialog with a thumbbar, the value prompt
 * dialog will be open on top of the other dialog). */

static struct dialog dialogs[4];
static int num_dialogs = 0;

/* --------------------------------------------------------------------- */

void dialog_draw(void)
{
	int n, d;

	for (d = 0; d < num_dialogs; d++) {
		n = dialogs[d].total_widgets;

		/* draw the border and background */
		draw_box(dialogs[d].x, dialogs[d].y,
			 dialogs[d].x + dialogs[d].w - 1,
			 dialogs[d].y + dialogs[d].h - 1, BOX_THICK | BOX_OUTER | BOX_FLAT_LIGHT);
		draw_fill_chars(dialogs[d].x + 1, dialogs[d].y + 1,
				dialogs[d].x + dialogs[d].w - 2, dialogs[d].y + dialogs[d].h - 2,
				DEFAULT_FG, 2);

		/* then the rest of the stuff */
		if (dialogs[d].draw_const) dialogs[d].draw_const(dialogs + d);

		if (dialogs[d].text)
			draw_text(dialogs[d].text, dialogs[d].text_x, 27, 0, 2);

		n = dialogs[d].total_widgets;
		while (n) {
			n--;
			widget_draw_widget(dialogs[d].widgets + n, n == dialogs[d].selected_widget);
		}
	}
}

/* --------------------------------------------------------------------- */

static void dialog_destroy_keep_final_data(void)
{
	int d;

	if (num_dialogs == 0)
		return;

	d = num_dialogs - 1;

	if (dialogs[d].type != DIALOG_CUSTOM) {
		free(dialogs[d].text);
		free(dialogs[d].widgets);

		dialogs[d].text = NULL;
		dialogs[d].widgets = NULL;
	}

	num_dialogs--;
	if (num_dialogs) {
		d--;
		widget_context = (struct widget_context *)&dialogs[d];
		status.dialog_type = dialogs[d].type;
	} else {
		widget_context = (struct widget_context *)&ACTIVE_PAGE;
		status.dialog_type = DIALOG_NONE;
	}

	/* it's up to the calling function to redraw the page */
}

void dialog_destroy(void)
{
	int d;

	if (num_dialogs == 0)
		return;

	d = num_dialogs - 1;

	if (dialogs[d].final_data)
		free(dialogs[d].final_data);

	dialog_destroy_keep_final_data();

	/* it's up to the calling function to redraw the page */
}

void dialog_destroy_all(void)
{
	while (num_dialogs)
		dialog_destroy();
}

/* --------------------------------------------------------------------- */
/* default callbacks */

void dialog_yes(struct widget_context *this)
{
	struct dialog *dialog;
	action_cb action;
	void *data, *final_data;

	ENSURE_DIALOG(return);

	dialog = widget_context_as_dialog(this);

	if (!dialog)
		dialog = &dialogs[num_dialogs - 1];

	if (dialog->finalize)
		dialog->finalize(dialog, DIALOG_BUTTON_YES);

	action = dialog->action_yes;
	data = dialog->data;
	final_data = dialog->final_data;

	/* must precede action, because action may create a new dialog */
	dialog_destroy_keep_final_data();

	if (action)
		action(data, final_data);

	if (final_data)
		free(final_data);

	status.flags |= NEED_UPDATE;
}

void dialog_no(struct widget_context *this)
{
	struct dialog *dialog;
	action_cb action;
	void *data, *final_data;

	ENSURE_DIALOG(return);

	dialog = widget_context_as_dialog(this);

	if (!dialog)
		dialog = &dialogs[num_dialogs - 1];

	if (dialog->finalize)
		dialog->finalize(dialog, DIALOG_BUTTON_NO);

	action = dialog->action_no;
	data = dialog->data;
	final_data = dialog->final_data;

	/* must precede action, because action may create a new dialog */
	dialog_destroy_keep_final_data();

	if (action)
		action(data, final_data);

	if (final_data)
		free(final_data);

	status.flags |= NEED_UPDATE;
}

void dialog_cancel(struct widget_context *this)
{
	struct dialog *dialog;
	action_cb action;
	void *data, *final_data;

	ENSURE_DIALOG(return);

	dialog = widget_context_as_dialog(this);

	if (!dialog)
		dialog = &dialogs[num_dialogs - 1];

	if (dialog->finalize)
		dialog->finalize(dialog, DIALOG_BUTTON_CANCEL);

	action = dialog->action_cancel;
	data = dialog->data;
	final_data = dialog->final_data;

	/* must precede action, because action may create a new dialog */
	dialog_destroy_keep_final_data();

	if (action)
		action(data, final_data);

	if (final_data)
		free(final_data);

	status.flags |= NEED_UPDATE;
}

void dialog_free_data(void *data, void *final_data)
{
	free(data);
}

/* --------------------------------------------------------------------- */

int dialog_handle_key(struct key_event * k)
{
	struct dialog *d = dialogs + num_dialogs - 1;
	struct widget_context *wc = (struct widget_context *)d;

	ENSURE_DIALOG(return 0);

	if (d->handle_key && d->handle_key(d, k))
		return 1;

	/* this SHOULD be handling on k->state press but the widget key handler is stealing that key. */
	if (k->state == KEY_RELEASE && NO_MODIFIER(k->mod)) {
		switch (k->sym) {
		case SCHISM_KEYSYM_y:
			switch (status.dialog_type) {
			case DIALOG_YES_NO:
			case DIALOG_OK_CANCEL:
				dialog_yes(wc);
				return 1;
			default:
				break;
			}
			break;
		case SCHISM_KEYSYM_n:
			switch (status.dialog_type) {
			case DIALOG_YES_NO:
				/* in Impulse Tracker, 'n' means cancel, not "no"!
				(results in different behavior on sample quality convert dialog) */
				if (!(status.flags & CLASSIC_MODE)) {
					dialog_no(wc);
					return 1;
				}
				SCHISM_FALLTHROUGH;
			case DIALOG_OK_CANCEL:
				dialog_cancel(wc);
				return 1;
			default:
				break;
			}
			break;
		case SCHISM_KEYSYM_c:
			switch (status.dialog_type) {
			case DIALOG_YES_NO:
			case DIALOG_OK_CANCEL:
				break;
			default:
				return 0;
			}
			SCHISM_FALLTHROUGH;
		case SCHISM_KEYSYM_ESCAPE:
			dialog_cancel(wc);
			return 1;
		case SCHISM_KEYSYM_o:
			switch (status.dialog_type) {
			case DIALOG_YES_NO:
			case DIALOG_OK_CANCEL:
				break;
			default:
				return 0;
			}
			SCHISM_FALLTHROUGH;
		case SCHISM_KEYSYM_RETURN:
			dialog_yes(wc);
			return 1;
		default:
			break;
		}
	}

	return 0;
}

/* --------------------------------------------------------------------- */
/* these get called from dialog_create below */

static void dialog_create_ok(int textlen)
{
	int d = num_dialogs;
	struct dialog *dialog = dialogs + d;

	/* make the dialog as wide as either the ok button or the text,
	 * whichever is more */
	dialog->text_x = 40 - (textlen / 2);
	if (textlen > 21) {
		dialog->x = dialog->text_x - 2;
		dialog->w = textlen + 4;
	} else {
		dialog->x = 26;
		dialog->w = 29;
	}
	dialog->h = 8;
	dialog->y = 25;

	dialog->widgets = (struct widget *)mem_alloc(sizeof(struct widget));
	dialog->total_widgets = 1;

	widget_create_button(dialog->widgets + 0, 36, 30, 6, 0, 0, 0, 0, 0, dialog_yes, "OK", 3);
}

static void dialog_create_ok_cancel(int textlen)
{
	int d = num_dialogs;
	struct dialog *dialog = dialogs + d;

	/* the ok/cancel buttons (with the borders and all) are 21 chars,
	 * so if the text is shorter, it needs a bit of padding. */
	dialog->text_x = 40 - (textlen / 2);
	if (textlen > 21) {
		dialog->x = dialog->text_x - 4;
		dialog->w = textlen + 8;
	} else {
		dialog->x = 26;
		dialog->w = 29;
	}
	dialog->h = 8;
	dialog->y = 25;

	dialog->widgets = mem_calloc(2, sizeof(struct widget));
	dialog->total_widgets = 2;

	widget_create_button(dialog->widgets + 0, 31, 30, 6, 0, 0, 1, 1, 1, dialog_yes, "OK", 3);
	widget_create_button(dialog->widgets + 1, 42, 30, 6, 1, 1, 0, 0, 0, dialog_cancel, "Cancel", 1);
}

static void dialog_create_yes_no(int textlen)
{
	int d = num_dialogs;
	struct dialog *dialog = dialogs + d;

	dialog->text_x = 40 - (textlen / 2);
	if (textlen > 21) {
		dialog->x = dialog->text_x - 4;
		dialog->w = textlen + 8;
	} else {
		dialog->x = 26;
		dialog->w = 29;
	}
	dialog->h = 8;
	dialog->y = 25;

	dialog->widgets = mem_calloc(2, sizeof(struct widget));
	dialog->total_widgets = 2;

	widget_create_button(dialog->widgets + 0, 30, 30, 7, 0, 0, 1, 1, 1, dialog_yes, "Yes", 3);
	widget_create_button(dialog->widgets + 1, 42, 30, 6, 1, 1, 0, 0, 0, dialog_no, "No", 3);
}

/* --------------------------------------------------------------------- */
/* type can be DIALOG_OK, DIALOG_OK_CANCEL, or DIALOG_YES_NO
 * default_widget: 0 = ok/yes, 1 = cancel/no */

struct dialog *dialog_create(int type, const char *text, action_cb action_yes,
		   action_cb action_no, int default_widget, void *data)
{
	int textlen = strlen(text);
	int d = num_dialogs;
	struct dialog *dialog = dialogs + d;

#ifndef NDEBUG
	if ((type & DIALOG_BOX) == 0) {
		fprintf(stderr, "dialog_create called with bogus dialog type %d\n", type);
		return NULL;
	}
#endif

	/* FIXME | hmm... a menu should probably be hiding itself when a widget gets selected. */
	if (status.dialog_type & DIALOG_MENU)
		menu_hide();

	dialog->type = WIDGET_CONTEXT_DIALOG;

	dialog->text = str_dup(text);
	dialog->data = data;
	dialog->final_data = NULL;
	dialog->finalize = NULL;
	dialog->action_yes = action_yes;
	dialog->action_no = action_no;
	dialog->action_cancel = NULL;        /* ??? */
	dialog->selected_widget = default_widget;
	dialog->draw_const = NULL;
	dialog->handle_key = NULL;

	switch (type) {
	case DIALOG_OK:
		dialog_create_ok(textlen);
		break;
	case DIALOG_OK_CANCEL:
		dialog_create_ok_cancel(textlen);
		break;
	case DIALOG_YES_NO:
		dialog_create_yes_no(textlen);
		break;
	default:
#ifndef NDEBUG
		fprintf(stderr, "this man should not be seen\n");
#endif
		type = DIALOG_OK_CANCEL;
		dialog_create_ok_cancel(textlen);
		break;
	}

	widget_set_context((struct widget_context *)dialog);

	dialog->type = type;
	widget_context = (struct widget_context *)dialog;

	num_dialogs++;

	status.dialog_type = type;
	status.flags |= NEED_UPDATE;
	return &dialogs[d];
}

/* --------------------------------------------------------------------- */
/* this will probably die painfully if two threads try to make custom dialogs at the same time */

struct dialog *dialog_create_custom(int x, int y, int w, int h, struct widget *dialog_widgets,
				    int dialog_total_widgets, int dialog_selected_widget,
				    dialog_cb draw_const, void *data, dialog_finalize_cb finalize)
{
	struct dialog *d = dialogs + num_dialogs;
	int i;

	/* FIXME | see dialog_create */
	if (status.dialog_type & DIALOG_MENU)
		menu_hide();

	num_dialogs++;

	d->context_type = WIDGET_CONTEXT_DIALOG;

	d->type = DIALOG_CUSTOM;
	d->finalize = finalize;
	d->final_data = NULL;
	d->x = x;
	d->y = y;
	d->w = w;
	d->h = h;
	d->widgets = dialog_widgets;
	d->selected_widget = dialog_selected_widget;
	d->total_widgets = dialog_total_widgets;
	d->draw_const = draw_const;

	d->text = NULL;
	d->data = data;
	d->action_yes = NULL;
	d->action_no = NULL;
	d->action_cancel = NULL;
	d->handle_key = NULL;

	widget_set_context((struct widget_context *)d);

	status.dialog_type = DIALOG_CUSTOM;
	widget_context = (struct widget_context *)d;

	status.flags |= NEED_UPDATE;

	return d;
}

/* dynamic cast to struct dialog * */
struct dialog *widget_context_as_dialog(struct widget_context *this)
{
	if (this && (this->context_type == WIDGET_CONTEXT_DIALOG))
		return (struct dialog *)this;
	else
		return NULL;
}

/* --------------------------------------------------------------------- */
/* Other prompt stuff */

static const char *numprompt_title, *numprompt_secondary;
static int numprompt_smp_pos1, numprompt_smp_pos2; /* used by the sample prompt */
static int numprompt_titlelen; /* used by the number prompt */
static char numprompt_buf[4];
static struct widget numprompt_widgets[2];
static void (*numprompt_finish)(int n);
static void (*numprompt_finish_with_thumbbar)(struct widget *thumbbar, int n);

/* this is bound to the textentry's activate callback.
since this dialog might be called from another dialog as well as from a page, it can't use the
normal dialog_yes handler -- it needs to destroy the prompt dialog first so that ACTIVE_WIDGET
points to whatever thumbbar actually triggered the dialog box. */
static void numprompt_value(SCHISM_UNUSED struct widget_context *dialog)
{
	char *eptr;
	long n = strtol(numprompt_buf, &eptr, 10);
	struct dialog *this = widget_context_as_dialog(dialog);
	struct widget *numprompt_thumbbar = (struct widget *)this->data;

	dialog_destroy();
	if (eptr > numprompt_buf && eptr[0] == '\0')
	{
		if (numprompt_finish_with_thumbbar)
			numprompt_finish_with_thumbbar(numprompt_thumbbar, n);
		else
			numprompt_finish(n);
	}
}

static void numprompt_draw_const(SCHISM_UNUSED struct dialog *this)
{
	int wx = this->widgets[0].x;
	int wy = this->widgets[0].y;
	int ww = this->widgets[0].width;

	draw_text(numprompt_title, wx - numprompt_titlelen - 1, wy, 3, 2);
	draw_box(wx - 1, wy - 1, wx + ww, wy + 1, BOX_THICK | BOX_INNER | BOX_INSET);
}

void numprompt_create(const char *prompt, void (*finish)(int n), char initvalue)
{
	numprompt_create_for_thumbbar(prompt, NULL, NULL, initvalue);
	numprompt_finish = finish;
}

void numprompt_create_for_thumbbar(const char *prompt, struct widget *thumbbar, void (*finish)(struct widget *thumbbar, int n), char initvalue)
{
	int y = 26; // an indisputable fact of life
	int dlgwidth, dlgx, entryx;

	numprompt_title = prompt;
	numprompt_titlelen = strlen(prompt);
	numprompt_buf[0] = initvalue;
	numprompt_buf[1] = '\0';

	/* Dialog is made up of border, padding (2 left, 1 right), frame around the text entry, the entry
	itself, and the prompt; the text entry is offset from the left of the dialog by 4 chars (padding +
	borders) plus the length of the prompt. */
	dlgwidth = 2 + 3 + 2 + 4 + numprompt_titlelen;
	dlgx = (80 - dlgwidth) / 2;
	entryx = dlgx + 4 + numprompt_titlelen;

	widget_create_textentry(numprompt_widgets + 0, entryx, y, 4, 0, 0, 0, NULL, numprompt_buf, 3);
	numprompt_widgets[0].activate = numprompt_value;
	numprompt_widgets[0].d.textentry.cursor_pos = initvalue ? 1 : 0;
	numprompt_finish_with_thumbbar = finish;
	dialog_create_custom(dlgx, y - 2, dlgwidth, 5, numprompt_widgets, 1, 0, numprompt_draw_const, thumbbar, NULL);
}


static int strtonum99(const char *s)
{
	// aaarghhhh
	int n = 0;
	if (!s || !*s)
		return -1;
	if (s[1]) {
		// two chars
		int c = tolower(*s);

		if (c >= '0' && c <= '9')
			n = c - '0';
		else if (c >= 'a' && c <= 'g')
			n = c - 'a' + 10;
		else if (c >= 'h' && c <= 'z')
			n = c - 'h' + 10;
		else
			return -1;

		n *= 10;
		s++;
	}
	return *s >= '0' && *s <= '9' ? n + *s - '0' : -1;
}

static void smpprompt_value(SCHISM_UNUSED void *data, SCHISM_UNUSED void *final_data)
{
	int n = strtonum99(numprompt_buf);
	numprompt_finish(n);
}

static void smpprompt_draw_const(SCHISM_UNUSED struct dialog *this)
{
	int wx = numprompt_widgets[0].x;
	int wy = numprompt_widgets[0].y;
	int ww = numprompt_widgets[0].width;

	draw_text(numprompt_title, numprompt_smp_pos1, 25, 0, 2);
	draw_text(numprompt_secondary, numprompt_smp_pos2, 27, 0, 2);
	draw_box(wx - 1, wy - 1, wx + ww, wy + 1, BOX_THICK | BOX_INNER | BOX_INSET);
}

void smpprompt_create(const char *title, const char *prompt, void (*finish)(int n))
{
	struct dialog *dialog;

	numprompt_title = title;
	numprompt_secondary = prompt;
	numprompt_smp_pos1 = (81 - strlen(title)) / 2;
	numprompt_smp_pos2 = 41 - strlen(prompt);
	numprompt_buf[0] = '\0';

	widget_create_textentry(numprompt_widgets + 0, 42, 27, 3, 1, 1, 1, NULL, numprompt_buf, 2);
	widget_create_button(numprompt_widgets + 1, 36, 30, 6, 0, 0, 1, 1, 1, dialog_cancel, "Cancel", 1);
	numprompt_finish = finish;
	dialog = dialog_create_custom(26, 23, 29, 10, numprompt_widgets, 2, 0, smpprompt_draw_const, NULL, NULL);
	dialog->action_yes = smpprompt_value;
}
