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

#ifndef SCHISM_DIALOG_H_
#define SCHISM_DIALOG_H_

struct key_event;
struct widget;

#include "widget_context.h"

struct dialog;

typedef void (*action_cb)(void *data);
typedef void (*dialog_cb)(struct dialog *this);
typedef int (*dialog_key_event_cb)(struct dialog *this, struct key_event *k);

struct dialog {
	/************************************/
	/* must match struct widget_context */
	enum widget_context_type context_type;
	struct widget *widgets;     /* malloc'ed (type != DIALOG_CUSTOM) */
	int selected_widget;
	int total_widgets;
	/************************************/

	int type;
	int x, y, w, h;

	/* next two are for "simple" dialogs (type != DIALOG_CUSTOM) */
	char *text;     /* malloc'ed */
	int text_x;

	void *data; /* extra data pointer */

	/* maybe these should get the data pointer as well? */
	dialog_cb draw_const;
	dialog_key_event_cb handle_key;

	/* there's no action_ok, as yes and ok are fundamentally the same */
	action_cb action_yes;
	action_cb action_no; /* only useful for y/n dialogs? */
	/* currently, this is only settable for custom dialogs.
	 * it's only used in a couple of places (mostly on the pattern editor) */
	action_cb action_cancel;
};

/* dialog handlers
 * these are set by default for normal dialogs, and can be used with the custom dialogs.
 * they call the {yes, no, cancel} callback, destroy the dialog, and schedule a screen
 * update. (note: connect these to the BUTTONS, not the action_* callbacks!) */
void dialog_yes(struct widget_context *this);
void dialog_no(struct widget_context *this);
void dialog_cancel(struct widget_context *this);

/* dialog handler that simply calls free() on this->data */
void dialog_free_data(struct dialog *this);

int dialog_handle_key(struct key_event * k);
void dialog_draw(void);

struct dialog *dialog_create(int type, const char *text, action_cb action_yes,
		   action_cb action_no, int default_widget, void *data);

void dialog_destroy(void);
void dialog_destroy_all(void);

/* this builds and displays a dialog with an unspecified widget structure.
 * the caller can set other properties of the dialog (i.e. the yes/no/cancel callbacks) after
 * the dialog has been displayed. */
struct dialog *dialog_create_custom(int x, int y, int w, int h, struct widget *dialog_widgets,
				    int dialog_total_widgets, int dialog_selected_widget,
				    dialog_cb draw_const, void *data);

/* dynamic cast to struct dialog * */
struct dialog *widget_context_as_dialog(struct widget_context *this);

#endif /* SCHISM_DIALOG_H_ */
