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
			draw_text((const unsigned char *)dialogs[d].text, dialogs[d].text_x, 27, 0, 2);

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
	int yes = 0;

	if (!k->state) return 0;
	
	ENSURE_DIALOG(return 0);

	switch (k->sym) {
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
			dialog_no(d->data);
			return 1;
		case DIALOG_OK_CANCEL:
			dialog_cancel(d->data);
			return 1;
		default:
			break;
		}
		break;
	case SDLK_ESCAPE:
		dialog_cancel(d->data);
		return 1;
	case SDLK_RETURN:
		yes = 1;
		break;
	default:
		break;
	}
	
	if (d->handle_key && d->handle_key(k)) {
		return 1;
	} else if (yes) {
		dialog_yes(d->data);
		return 1;
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

	dialogs[d].widgets = calloc(2, sizeof(struct widget));
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

	dialogs[d].widgets = calloc(2, sizeof(struct widget));
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
		return 0;
	}
#endif

	/* FIXME | hmm... a menu should probably be hiding itself when a widget gets selected. */
	if (status.dialog_type & DIALOG_MENU)
		menu_hide();

	dialogs[d].text = strdup(text);
	dialogs[d].data = data;
	dialogs[d].action_yes = action_yes;
	dialogs[d].action_no = action_no;
	dialogs[d].action_cancel = NULL;	/* ??? */
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
