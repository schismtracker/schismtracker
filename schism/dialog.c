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
#include "song.h"
#include "page.h"

#include "sdlmain.h"

/* --------------------------------------------------------------------- */

/* ENSURE_DIALOG(optional return value)
 * will emit a warning and cause the function to return
 * if a dialog is not active. */
#ifndef NDEBUG
# define ENSURE_DIALOG(q) do { if (!(status.dialog_type & DIALOG_BOX)) { \
		fprintf(stderr, "%s called with no dialog\n", __FUNCTION__);\
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
				dialogs[d].x + dialogs[d].w - 2, dialogs[d].y + dialogs[d].h - 2, 2);

		/* then the rest of the stuff */
		if (dialogs[d].draw_const) dialogs[d].draw_const();

		if (dialogs[d].text)
			draw_text(dialogs[d].text, dialogs[d].text_x, 27, 0, 2);

		n = dialogs[d].total_widgets;
		while (n) {
			n--;
			draw_widget(dialogs[d].widgets + n, n == dialogs[d].selected_widget);
		}
	}
}

/* --------------------------------------------------------------------- */

void dialog_destroy(void)
{
	int d;

	if (num_dialogs == 0)
		return;

	d = num_dialogs - 1;

	if (dialogs[d].type != DIALOG_CUSTOM) {
		free(dialogs[d].text);
		free(dialogs[d].widgets);
	}

	num_dialogs--;
	if (num_dialogs) {
		d--;
		widgets = dialogs[d].widgets;
		selected_widget = &(dialogs[d].selected_widget);
		total_widgets = &(dialogs[d].total_widgets);
		status.dialog_type = dialogs[d].type;
	} else {
		widgets = ACTIVE_PAGE.widgets;
		selected_widget = &(ACTIVE_PAGE.selected_widget);
		total_widgets = &(ACTIVE_PAGE.total_widgets);
		status.dialog_type = DIALOG_NONE;
	}

	/* it's up to the calling function to redraw the page */
}

void dialog_destroy_all(void)
{
	while (num_dialogs)
		dialog_destroy();
}

/* --------------------------------------------------------------------- */
/* default callbacks */

void dialog_yes(void *data)
{
	void (*action) (void *);

	ENSURE_DIALOG(return);

	action = dialogs[num_dialogs - 1].action_yes;
	if (!data) data = dialogs[num_dialogs - 1].data;
	dialog_destroy();
	if (action) action(data);
	status.flags |= NEED_UPDATE;
}

void dialog_no(void *data)
{
	void (*action) (void *);

	ENSURE_DIALOG(return);

	action = dialogs[num_dialogs - 1].action_no;
	if (!data) data = dialogs[num_dialogs - 1].data;
	dialog_destroy();
	if (action) action(data);
	status.flags |= NEED_UPDATE;
}

void dialog_cancel(void *data)
{
	void (*action) (void *);

	ENSURE_DIALOG(return);

	action = dialogs[num_dialogs - 1].action_cancel;
	if (!data) data = dialogs[num_dialogs - 1].data;
	dialog_destroy();
	if (action) action(data);
	status.flags |= NEED_UPDATE;
}

void dialog_yes_NULL(void)
{
	dialog_yes(NULL);
}
void dialog_no_NULL(void)
{
	dialog_no(NULL);
}
void dialog_cancel_NULL(void)
{
	dialog_cancel(NULL);
}

/* --------------------------------------------------------------------- */

int dialog_handle_key(struct key_event * k)
{
	struct dialog *d = dialogs + num_dialogs - 1;

	ENSURE_DIALOG(return 0);

	if (d->handle_key && d->handle_key(k))
		return 1;

	/* this SHOULD be handling on k->state press but the widget key handler is stealing that key. */
	if (k->state == KEY_RELEASE && NO_MODIFIER(k->mod)) {
		switch (k->sym.sym) {
		case SDLK_y:
			switch (status.dialog_type) {
			case DIALOG_YES_NO:
			case DIALOG_OK_CANCEL:
				dialog_yes(d->data);
				return 1;
			default:
				break;
			}
			break;
		case SDLK_n:
			switch (status.dialog_type) {
			case DIALOG_YES_NO:
				/* in Impulse Tracker, 'n' means cancel, not "no"!
				(results in different behavior on sample quality convert dialog) */
				if (!(status.flags & CLASSIC_MODE)) {
					dialog_no(d->data);
					return 1;
				} /* else fall through */
			case DIALOG_OK_CANCEL:
				dialog_cancel(d->data);
				return 1;
			default:
				break;
			}
			break;
		case SDLK_c:
			switch (status.dialog_type) {
			case DIALOG_YES_NO:
			case DIALOG_OK_CANCEL:
				break;
			default:
				return 0;
			} /* and fall through */
		case SDLK_ESCAPE:
			dialog_cancel(d->data);
			return 1;
		case SDLK_o:
			switch (status.dialog_type) {
			case DIALOG_YES_NO:
			case DIALOG_OK_CANCEL:
				break;
			default:
				return 0;
			} /* and fall through */
		case SDLK_RETURN:
			dialog_yes(d->data);
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

	/* make the dialog as wide as either the ok button or the text,
	 * whichever is more */
	dialogs[d].text_x = 40 - (textlen / 2);
	if (textlen > 21) {
		dialogs[d].x = dialogs[d].text_x - 2;
		dialogs[d].w = textlen + 4;
	} else {
		dialogs[d].x = 26;
		dialogs[d].w = 29;
	}
	dialogs[d].h = 8;
	dialogs[d].y = 25;

	dialogs[d].widgets = (struct widget *)mem_alloc(sizeof(struct widget));
	dialogs[d].total_widgets = 1;

	create_button(dialogs[d].widgets + 0, 36, 30, 6, 0, 0, 0, 0, 0, dialog_yes_NULL, "OK", 3);
}

static void dialog_create_ok_cancel(int textlen)
{
	int d = num_dialogs;

	/* the ok/cancel buttons (with the borders and all) are 21 chars,
	 * so if the text is shorter, it needs a bit of padding. */
	dialogs[d].text_x = 40 - (textlen / 2);
	if (textlen > 21) {
		dialogs[d].x = dialogs[d].text_x - 4;
		dialogs[d].w = textlen + 8;
	} else {
		dialogs[d].x = 26;
		dialogs[d].w = 29;
	}
	dialogs[d].h = 8;
	dialogs[d].y = 25;

	dialogs[d].widgets = mem_calloc(2, sizeof(struct widget));
	dialogs[d].total_widgets = 2;

	create_button(dialogs[d].widgets + 0, 31, 30, 6, 0, 0, 1, 1, 1, dialog_yes_NULL, "OK", 3);
	create_button(dialogs[d].widgets + 1, 42, 30, 6, 1, 1, 0, 0, 0, dialog_cancel_NULL, "Cancel", 1);
}

static void dialog_create_yes_no(int textlen)
{
	int d = num_dialogs;

	dialogs[d].text_x = 40 - (textlen / 2);
	if (textlen > 21) {
		dialogs[d].x = dialogs[d].text_x - 4;
		dialogs[d].w = textlen + 8;
	} else {
		dialogs[d].x = 26;
		dialogs[d].w = 29;
	}
	dialogs[d].h = 8;
	dialogs[d].y = 25;

	dialogs[d].widgets = mem_calloc(2, sizeof(struct widget));
	dialogs[d].total_widgets = 2;

	create_button(dialogs[d].widgets + 0, 30, 30, 7, 0, 0, 1, 1, 1, dialog_yes_NULL, "Yes", 3);
	create_button(dialogs[d].widgets + 1, 42, 30, 6, 1, 1, 0, 0, 0, dialog_no_NULL, "No", 3);
}

/* --------------------------------------------------------------------- */
/* type can be DIALOG_OK, DIALOG_OK_CANCEL, or DIALOG_YES_NO
 * default_widget: 0 = ok/yes, 1 = cancel/no */

struct dialog *dialog_create(int type, const char *text, void (*action_yes) (void *data),
		   void (*action_no) (void *data), int default_widget, void *data)
{
	int textlen = strlen(text);
	int d = num_dialogs;

#ifndef NDEBUG
	if ((type & DIALOG_BOX) == 0) {
		fprintf(stderr, "dialog_create called with bogus dialog type %d\n", type);
		return NULL;
	}
#endif

	/* FIXME | hmm... a menu should probably be hiding itself when a widget gets selected. */
	if (status.dialog_type & DIALOG_MENU)
		menu_hide();

	dialogs[d].text = str_dup(text);
	dialogs[d].data = data;
	dialogs[d].action_yes = action_yes;
	dialogs[d].action_no = action_no;
	dialogs[d].action_cancel = NULL;        /* ??? */
	dialogs[d].selected_widget = default_widget;
	dialogs[d].draw_const = NULL;
	dialogs[d].handle_key = NULL;

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

	dialogs[d].type = type;
	widgets = dialogs[d].widgets;
	selected_widget = &(dialogs[d].selected_widget);
	total_widgets = &(dialogs[d].total_widgets);

	num_dialogs++;

	status.dialog_type = type;
	status.flags |= NEED_UPDATE;
	return &dialogs[d];
}

/* --------------------------------------------------------------------- */
/* this will probably die painfully if two threads try to make custom dialogs at the same time */

struct dialog *dialog_create_custom(int x, int y, int w, int h, struct widget *dialog_widgets,
				    int dialog_total_widgets, int dialog_selected_widget,
				    void (*draw_const) (void), void *data)
{
	struct dialog *d = dialogs + num_dialogs;

	/* FIXME | see dialog_create */
	if (status.dialog_type & DIALOG_MENU)
		menu_hide();

	num_dialogs++;

	d->type = DIALOG_CUSTOM;
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

	status.dialog_type = DIALOG_CUSTOM;
	widgets = d->widgets;
	selected_widget = &(d->selected_widget);
	total_widgets = &(d->total_widgets);

	status.flags |= NEED_UPDATE;

	return d;
}

/* --------------------------------------------------------------------- */
/* Other prompt stuff */

static const char *numprompt_title, *numprompt_secondary;
static int numprompt_smp_pos1, numprompt_smp_pos2; /* used by the sample prompt */
static int numprompt_titlelen; /* used by the number prompt */
static char numprompt_buf[4];
static struct widget numprompt_widgets[2];
static void (*numprompt_finish)(int n);

/* this is bound to the textentry's activate callback.
since this dialog might be called from another dialog as well as from a page, it can't use the
normal dialog_yes handler -- it needs to destroy the prompt dialog first so that ACTIVE_WIDGET
points to whatever thumbbar actually triggered the dialog box. */
static void numprompt_value(void)
{
	char *eptr;
	long n = strtol(numprompt_buf, &eptr, 10);

	dialog_destroy();
	if (eptr > numprompt_buf && eptr[0] == '\0')
		numprompt_finish(n);
}

static void numprompt_draw_const(void)
{
	int wx = numprompt_widgets[0].x;
	int wy = numprompt_widgets[0].y;
	int ww = numprompt_widgets[0].width;

	draw_text(numprompt_title, wx - numprompt_titlelen - 1, wy, 3, 2);
	draw_box(wx - 1, wy - 1, wx + ww, wy + 1, BOX_THICK | BOX_INNER | BOX_INSET);
}

void numprompt_create(const char *prompt, void (*finish)(int n), char initvalue)
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

	create_textentry(numprompt_widgets + 0, entryx, y, 4, 0, 0, 0, NULL, numprompt_buf, 3);
	numprompt_widgets[0].activate = numprompt_value;
	numprompt_widgets[0].d.textentry.cursor_pos = initvalue ? 1 : 0;
	numprompt_finish = finish;
	dialog_create_custom(dlgx, y - 2, dlgwidth, 5, numprompt_widgets, 1, 0, numprompt_draw_const, NULL);
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
		switch (c) {
			case '0' ... '9': n = c - '0'; break;
			case 'a' ... 'g': n = c - 'a' + 10; break;
			case 'h' ... 'z': n = c - 'h' + 10; break;
			default: return -1;
		}
		n *= 10;
		s++;
	}
	return *s >= '0' && *s <= '9' ? n + *s - '0' : -1;
}

static void smpprompt_value(UNUSED void *data)
{
	int n = strtonum99(numprompt_buf);
	numprompt_finish(n);
}

static void smpprompt_draw_const(void)
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

	create_textentry(numprompt_widgets + 0, 42, 27, 3, 1, 1, 1, NULL, numprompt_buf, 2);
	create_button(numprompt_widgets + 1, 36, 30, 6, 0, 0, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);
	numprompt_finish = finish;
	dialog = dialog_create_custom(26, 23, 29, 10, numprompt_widgets, 2, 0, smpprompt_draw_const, NULL);
	dialog->action_yes = smpprompt_value;
}

