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

/* ENSURE_MENU(optional return value)
 * will emit a warning and cause the function to return
 * if a menu is not active. */
#ifndef NDEBUG
# define ENSURE_MENU(q) do {\
	if ((status.dialog_type & DIALOG_MENU) == 0) {\
		fprintf(stderr, "%s called with no menu\n", __func__);\
		q;\
	}\
} while(0)
#else
# define ENSURE_MENU(q)
#endif

/* --------------------------------------------------------------------- */
/* protomatypes */

static void main_menu_selected_cb(void);
static void file_menu_selected_cb(void);
static void playback_menu_selected_cb(void);
static void sample_menu_selected_cb(void);
static void instrument_menu_selected_cb(void);
static void settings_menu_selected_cb(void);

/* --------------------------------------------------------------------- */

struct menu {
	unsigned int x, y, w;
	const char *title;
	int num_items;  /* meh... */
	const char *items[14];  /* writing **items doesn't work here :( */
	int selected_item;      /* the highlighted item */
	int active_item;        /* "pressed" menu item, for submenus */
	void (*selected_cb) (void);     /* triggered by return key */
};

static struct menu main_menu = {
	.x = 6,
	.y = 11,
	.w = 25,
	.title = " Main Menu",
	.num_items = 10,
	.items = {
		"File Menu...",
		"Playback Menu...",
		"View Patterns        (F2)",
		"Sample Menu...",
		"Instrument Menu...",
		"View Orders/Panning (F11)",
		"View Variables      (F12)",
		"Message Editor (Shift-F9)",
		"Settings Menu...",
		"Help!                (F1)",
	},
	.selected_item = 0,
	.active_item = -1,
	.selected_cb = main_menu_selected_cb,
};

static struct menu file_menu = {
	.x = 25,
	.y = 13,
	.w = 22,
	.title = "File Menu",
	.num_items = 7,
	.items = {
		"Load...           (F9)",
		"New...        (Ctrl-N)",
		"Save Current  (Ctrl-S)",
		"Save As...       (F10)",
		"Export...  (Shift-F10)",
		"Message Log (Ctrl-F11)",
		"Quit          (Ctrl-Q)",
	},
	.selected_item = 0,
	.active_item = -1,
	.selected_cb = file_menu_selected_cb,
};

static struct menu playback_menu = {
	.x = 25,
	.y = 13,
	.w = 27,
	.title = " Playback Menu",
	.num_items = 9,
	.items = {
		"Show Infopage          (F5)",
		"Play Song         (Ctrl-F5)",
		"Play Pattern           (F6)",
		"Play from Order  (Shift-F6)",
		"Play from Mark/Cursor  (F7)",
		"Stop                   (F8)",
		"Reinit Soundcard   (Ctrl-I)",
		"Driver Screen    (Shift-F5)",
		"Calculate Length   (Ctrl-P)",
	},
	.selected_item = 0,
	.active_item = -1,
	.selected_cb = playback_menu_selected_cb,
};

static struct menu sample_menu = {
	.x = 25,
	.y = 20,
	.w = 25,
	.title = "Sample Menu",
	.num_items = 2,
	.items = {
		"Sample List          (F3)",
		"Sample Library  (Ctrl-F3)",
	},
	.selected_item = 0,
	.active_item = -1,
	.selected_cb = sample_menu_selected_cb,
};

static struct menu instrument_menu = {
	.x = 20,
	.y = 23,
	.w = 29,
	.title = "Instrument Menu",
	.num_items = 2,
	.items = {
		"Instrument List          (F4)",
		"Instrument Library  (Ctrl-F4)",
	},
	.selected_item = 0,
	.active_item = -1,
	.selected_cb = instrument_menu_selected_cb,
};

static struct menu settings_menu = {
	.x = 22,
	.y = 25,
	.w = 34,
	.title = "Settings Menu",
	/* num_items is fiddled with when the menu is loaded (if there's no window manager,
	the toggle fullscreen item doesn't appear) */
	.num_items = 6,
	.items = {
		"Preferences             (Shift-F5)",
		"MIDI Configuration      (Shift-F1)",
		"System Configuration     (Ctrl-F1)",
		"Palette Editor          (Ctrl-F12)",
		"Font Editor            (Shift-F12)",
		"Toggle Fullscreen (Ctrl-Alt-Enter)",
	},
	.selected_item = 0,
	.active_item = -1,
	.selected_cb = settings_menu_selected_cb,
};

/* *INDENT-ON* */

/* updated to whatever menu is currently active.
 * this generalises the key handling.
 * if status.dialog_type == DIALOG_SUBMENU, use current_menu[1]
 * else, use current_menu[0] */
static struct menu *current_menu[2] = { NULL, NULL };

/* --------------------------------------------------------------------- */

static void _draw_menu(struct menu *menu)
{
	int h = 6, n = menu->num_items;

	while (n--) {
		draw_box(2 + menu->x, 4 + menu->y + 3 * n,
			 5 + menu->x + menu->w, 6 + menu->y + 3 * n,
			 BOX_THIN | BOX_CORNER | (n == menu->active_item ? BOX_INSET : BOX_OUTSET));

		draw_text_len(menu->items[n], menu->w, 4 + menu->x, 5 + menu->y + 3 * n,
			      (n == menu->selected_item ? 3 : 0), 2);

		draw_char(0, 3 + menu->x, 5 + menu->y + 3 * n, 0, 2);
		draw_char(0, 4 + menu->x + menu->w, 5 + menu->y + 3 * n, 0, 2);

		h += 3;
	}

	draw_box(menu->x, menu->y, menu->x + menu->w + 7, menu->y + h - 1,
		 BOX_THICK | BOX_OUTER | BOX_FLAT_LIGHT);
	draw_box(menu->x + 1, menu->y + 1, menu->x + menu->w + 6,
		 menu->y + h - 2, BOX_THIN | BOX_OUTER | BOX_FLAT_DARK);
	draw_fill_chars(menu->x + 2, menu->y + 2, menu->x + menu->w + 5, menu->y + 3, 2);
	draw_text(menu->title, menu->x + 6, menu->y + 2, 3, 2);
}

void menu_draw(void)
{
	ENSURE_MENU(return);

	_draw_menu(current_menu[0]);
	if (current_menu[1])
		_draw_menu(current_menu[1]);
}

/* --------------------------------------------------------------------- */

void menu_show(void)
{
	dialog_destroy_all();
	status.dialog_type = DIALOG_MAIN_MENU;
	current_menu[0] = &main_menu;

	status.flags |= NEED_UPDATE;
}

void menu_hide(void)
{
	ENSURE_MENU(return);

	status.dialog_type = DIALOG_NONE;

	/* "unpress" the menu items */
	current_menu[0]->active_item = -1;
	if (current_menu[1])
		current_menu[1]->active_item = -1;

	current_menu[0] = current_menu[1] = NULL;

	/* note! this does NOT redraw the screen; that's up to the caller.
	 * the reason for this is that so many of the menu items cause a
	 * page switch, and redrawing the current page followed by
	 * redrawing a new page is redundant. */
}

/* --------------------------------------------------------------------- */

static void set_submenu(struct menu *menu)
{
	ENSURE_MENU(return);

	status.dialog_type = DIALOG_SUBMENU;
	main_menu.active_item = main_menu.selected_item;
	current_menu[1] = menu;

	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* callbacks */

static void main_menu_selected_cb(void)
{
	switch (main_menu.selected_item) {
	case 0: /* file menu... */
		set_submenu(&file_menu);
		break;
	case 1: /* playback menu... */
		set_submenu(&playback_menu);
		break;
	case 2: /* view patterns */
		set_page(PAGE_PATTERN_EDITOR);
		break;
	case 3: /* sample menu... */
		set_submenu(&sample_menu);
		break;
	case 4: /* instrument menu... */
		set_submenu(&instrument_menu);
		break;
	case 5: /* view orders/panning */
		set_page(PAGE_ORDERLIST_PANNING);
		break;
	case 6: /* view variables */
		set_page(PAGE_SONG_VARIABLES);
		break;
	case 7: /* message editor */
		set_page(PAGE_MESSAGE);
		break;
	case 8: /* settings menu */
		/* fudge the menu to show/hide the fullscreen toggle as appropriate */
		if (status.flags & WM_AVAILABLE)
			settings_menu.num_items = 6;
		else
			settings_menu.num_items = 5;
		set_submenu(&settings_menu);
		break;
	case 9: /* help! */
		set_page(PAGE_HELP);
		break;
	}
}

static void file_menu_selected_cb(void)
{
	switch (file_menu.selected_item) {
	case 0: /* load... */
		set_page(PAGE_LOAD_MODULE);
		break;
	case 1: /* new... */
		new_song_dialog();
		break;
	case 2: /* save current */
		save_song_or_save_as();
		break;
	case 3: /* save as... */
		set_page(PAGE_SAVE_MODULE);
		break;
	case 4:
		/* export ... */
		set_page(PAGE_EXPORT_MODULE);
		break;
	case 5: /* message log */
		set_page(PAGE_LOG);
		break;
	case 6: /* quit */
		show_exit_prompt();
		break;
	}
}

static void playback_menu_selected_cb(void)
{
	switch (playback_menu.selected_item) {
	case 0: /* show infopage */
		if (song_get_mode() == MODE_STOPPED
		    || (song_get_mode() == MODE_SINGLE_STEP && status.current_page == PAGE_INFO))
			song_start();
		set_page(PAGE_INFO);
		return;
	case 1: /* play song */
		song_start();
		break;
	case 2: /* play pattern */
		song_loop_pattern(get_current_pattern(), 0);
		break;
	case 3: /* play from order */
		song_start_at_order(get_current_order(), 0);
		break;
	case 4: /* play from mark/cursor */
		play_song_from_mark();
		break;
	case 5: /* stop */
		song_stop();
		break;
	case 6: /* reinit soundcard */
		audio_reinit(NULL);
		break;
	case 7: /* driver screen */
		set_page(PAGE_PREFERENCES);
		return;
	case 8: /* calculate length */
		show_song_length();
		return;
	}

	menu_hide();
	status.flags |= NEED_UPDATE;
}

static void sample_menu_selected_cb(void)
{
	switch (sample_menu.selected_item) {
	case 0: /* sample list */
		set_page(PAGE_SAMPLE_LIST);
		break;
	case 1: /* sample library */
		set_page(PAGE_LIBRARY_SAMPLE);
		break;
	}
}

static void instrument_menu_selected_cb(void)
{
	switch (instrument_menu.selected_item) {
	case 0: /* instrument list */
		set_page(PAGE_INSTRUMENT_LIST);
		break;
	case 1: /* instrument library */
		set_page(PAGE_LIBRARY_INSTRUMENT);
		break;
	}
}

static void settings_menu_selected_cb(void)
{
	switch (settings_menu.selected_item) {
	case 0: /* preferences page */
		set_page(PAGE_PREFERENCES);
		return;
	case 1: /* midi configuration */
		set_page(PAGE_MIDI);
		return;
	case 2: /* config */
		set_page(PAGE_CONFIG);
		return;
	case 3: /* palette configuration */
		set_page(PAGE_PALETTE_EDITOR);
		return;
	case 4: /* font editor */
		set_page(PAGE_FONT_EDIT);
		return;
	case 5: /* toggle fullscreen */
		toggle_display_fullscreen();
		break;
	}

	menu_hide();
	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

/* As long as there's a menu active, this function will return true. */
int menu_handle_key(struct key_event *k)
{
	struct menu *menu;
	int n, h;

	if ((status.dialog_type & DIALOG_MENU) == 0)
		return 0;

	menu = (status.dialog_type == DIALOG_SUBMENU
		? current_menu[1] : current_menu[0]);

	if (k->mouse) {
		if (k->mouse == MOUSE_CLICK || k->mouse == MOUSE_DBLCLICK) {
			h = menu->num_items * 3;
			if (k->x >= menu->x + 2 && k->x <= menu->x + menu->w + 5
			    && k->y >= menu->y + 4 && k->y <= menu->y + h + 4) {
				n = ((k->y - 4) - menu->y) / 3;
				if (n >= 0 && n < menu->num_items) {
					menu->selected_item = n;
					if (k->state == KEY_RELEASE) {
						menu->active_item = -1;
						menu->selected_cb();
					} else {
						status.flags |= NEED_UPDATE;
						menu->active_item = n;
					}
				}
			} else if (k->state == KEY_RELEASE && (k->x < menu->x || k->x > 7+menu->x+menu->w
			|| k->y < menu->y || k->y >= 5+menu->y+h)) {
				/* get rid of the menu */
				current_menu[1] = NULL;
				if (status.dialog_type == DIALOG_SUBMENU) {
					status.dialog_type = DIALOG_MAIN_MENU;
					main_menu.active_item = -1;
				} else {
					menu_hide();
				}
				status.flags |= NEED_UPDATE;
			}
		}
		return 1;
	}

	switch (k->sym) {
	case SDLK_ESCAPE:
		if (k->state == KEY_RELEASE)
			return 1;
		current_menu[1] = NULL;
		if (status.dialog_type == DIALOG_SUBMENU) {
			status.dialog_type = DIALOG_MAIN_MENU;
			main_menu.active_item = -1;
		} else {
			menu_hide();
		}
		break;
	case SDLK_UP:
		if (k->state == KEY_RELEASE)
			return 1;
		if (menu->selected_item > 0) {
			menu->selected_item--;
			break;
		}
		return 1;
	case SDLK_DOWN:
		if (k->state == KEY_RELEASE)
			return 1;
		if (menu->selected_item < menu->num_items - 1) {
			menu->selected_item++;
			break;
		}
		return 1;
		/* home/end are new here :) */
	case SDLK_HOME:
		if (k->state == KEY_RELEASE)
			return 1;
		menu->selected_item = 0;
		break;
	case SDLK_END:
		if (k->state == KEY_RELEASE)
			return 1;
		menu->selected_item = menu->num_items - 1;
		break;
	case SDLK_RETURN:
		if (k->state == KEY_PRESS) {
			menu->active_item = menu->selected_item;
			status.flags |= NEED_UPDATE;
			return 1;
		}
		menu->selected_cb();
		return 1;
	default:
		return 1;
	}

	status.flags |= NEED_UPDATE;

	return 1;
}
