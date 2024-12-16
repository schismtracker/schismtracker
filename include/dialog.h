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

struct dialog {
	int type;
	int x, y, w, h;

	/* next two are for "simple" dialogs (type != DIALOG_CUSTOM) */
	char *text;     /* malloc'ed */
	int text_x;

	struct widget *widgets;     /* malloc'ed */
	int selected_widget;
	int total_widgets;

	void *data; /* extra data pointer */

	/* maybe these should get the data pointer as well? */
	void (*draw_const) (void);
	int (*handle_key) (struct key_event * k);

	/* there's no action_ok, as yes and ok are fundamentally the same */
	void (*action_yes) (void *data);
	void (*action_no) (void *data); /* only useful for y/n dialogs? */
	/* currently, this is only settable for custom dialogs.
	 * it's only used in a couple of places (mostly on the pattern editor) */
	void (*action_cancel) (void *data);
};

/* dialog handlers
 * these are set by default for normal dialogs, and can be used with the custom dialogs.
 * they call the {yes, no, cancel} callback, destroy the dialog, and schedule a screen
 * update. (note: connect these to the BUTTONS, not the action_* callbacks!) */
void dialog_yes(void *data);
void dialog_no(void *data);
void dialog_cancel(void *data);
/* these are the same as dialog_yes(NULL) etc., and are used in button callbacks */
void dialog_yes_NULL(void);
void dialog_no_NULL(void);
void dialog_cancel_NULL(void);

int dialog_handle_key(struct key_event * k);
void dialog_draw(void);

struct dialog *dialog_create(int type, const char *text, void (*action_yes) (void *data),
		   void (*action_no) (void *data), int default_widget, void *data);

void dialog_destroy(void);
void dialog_destroy_all(void);

/* this builds and displays a dialog with an unspecified widget structure.
 * the caller can set other properties of the dialog (i.e. the yes/no/cancel callbacks) after
 * the dialog has been displayed. */
struct dialog *dialog_create_custom(int x, int y, int w, int h, struct widget *dialog_widgets,
				    int dialog_total_widgets, int dialog_selected_widget,
				    void (*draw_const) (void), void *data);

#endif /* SCHISM_DIALOG_H_ */
