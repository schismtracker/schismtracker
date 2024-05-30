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
#include "clippy.h"

#include "sdlmain.h"

#define NUM_PALETTES 15

/* --------------------------------------------------------------------- */

static struct widget widgets_palette[51];

static int selected_palette;

/* --------------------------------------------------------------------- */
/*
This is actually wrong. For some reason the boxes around the little color swatches are drawn with the top
right and bottom left corners in color 3 instead of color 1 like all the other thick boxes have. I'm going
to leave it this way, though -- it's far more likely that someone will comment on, say, my completely
changing the preset switcher than about the corners having different colors :)

(Another discrepancy: seems that Impulse Tracker draws the thumbbars with a "fake" range of 0-64, because
it never gets drawn at the far right. Oh well.) */

static void palette_draw_const(void)
{
	int n;

	draw_text("Predefined Palettes", 56, 24, 0, 2);

	for (n = 0; n < 7; n++) {
		draw_box(2, 13 + (5 * n), 8, 17 + (5 * n), BOX_THICK | BOX_INNER | BOX_INSET);
		draw_box(9, 13 + (5 * n), 19, 17 + (5 * n), BOX_THICK | BOX_INNER | BOX_INSET);
		draw_box(29, 13 + (5 * n), 35, 17 + (5 * n), BOX_THICK | BOX_INNER | BOX_INSET);
		draw_box(36, 13 + (5 * n), 46, 17 + (5 * n), BOX_THICK | BOX_INNER | BOX_INSET);
		draw_fill_chars(3, 14 + (5 * n), 7, 16 + (5 * n), n);
		draw_fill_chars(30, 14 + (5 * n), 34, 16 + (5 * n), n + 7);
	}

	draw_box(56, 13, 62, 17, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(63, 13, 73, 17, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(56, 18, 62, 22, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(63, 18, 73, 22, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(54, 25, 77, 41, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(57, 14, 61, 16, 14);
	draw_fill_chars(57, 19, 61, 21, 15);
}

/* --------------------------------------------------------------------- */

static void update_thumbbars(void)
{
	int n;

	for (n = 0; n < 16; n++) {
		/* palettes[current_palette_index].colors[n] ?
		 * or current_palette[n] ? */
		widgets_palette[3 * n].d.thumbbar.value = current_palette[n][0];
		widgets_palette[3 * n + 1].d.thumbbar.value = current_palette[n][1];
		widgets_palette[3 * n + 2].d.thumbbar.value = current_palette[n][2];
	}
}

/* --------------------------------------------------------------------- */

static void palette_list_draw(void)
{
	int n, focused = (ACTIVE_PAGE.selected_widget == 48);
	int fg, bg;

	draw_fill_chars(55, 26, 76, 40, 0);

	for (n = 0; n < NUM_PALETTES; n++) {
		fg = 6;
		bg = 0;
		if (focused && n == selected_palette) {
			fg = 0;
			bg = 3;
		} else if (n == selected_palette) {
			bg = 14;
		}

		if(n == current_palette_index)
			draw_text_len("*", 1, 55, 26 + n, fg, bg);
		else
			draw_text_len(" ", 1, 55, 26 + n, fg, bg);

		draw_text_len(palettes[n].name, 21, 56, 26 + n, fg, bg);
	}
}

static int palette_list_handle_key_on_list(struct key_event * k)
{
	int new_palette = selected_palette;
	int load_selected_palette = 0;
	const int focus_offsets[] = { 0, 1, 1, 2, 3, 3, 4, 4, 5, 6, 6, 7, 7, 8, 9, 9, 10, 10, 11, 12 };

	if(k->mouse == MOUSE_DBLCLICK) {
		if (k->state == KEY_PRESS)
			return 0;
		if (k->x < 55 || k->y < 26 || k->y > 40 || k->x > 76) return 0;
		new_palette = (k->y - 26);
		load_selected_palette = 1;
	} else if (k->mouse == MOUSE_CLICK) {
		if (k->state == KEY_PRESS)
			return 0;
		if (k->x < 55 || k->y < 26 || k->y > 40 || k->x > 76) return 0;
		new_palette = (k->y - 26);
		if(new_palette == selected_palette)
			load_selected_palette = 1;
	} else {
		if (k->state == KEY_RELEASE)
			return 0;
		if (k->mouse == MOUSE_SCROLL_UP)
			new_palette -= MOUSE_SCROLL_LINES;
		else if (k->mouse == MOUSE_SCROLL_DOWN)
			new_palette += MOUSE_SCROLL_LINES;
	}

	switch (k->sym) {
	case SDLK_UP:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (--new_palette < 0) {
			change_focus_to(47);
			return 1;
		}
		break;
	case SDLK_DOWN:
		if (!NO_MODIFIER(k->mod))
			return 0;
		// new_palette++;
		if (++new_palette >= NUM_PALETTES) {
			change_focus_to(49);
			return 1;
		}
		break;
	case SDLK_HOME:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_palette = 0;
		break;
	case SDLK_PAGEUP:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (new_palette == 0) {
			change_focus_to(45);
			return 1;
		}
		new_palette -= 16;
		break;
	case SDLK_END:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_palette = NUM_PALETTES - 1;
		break;
	case SDLK_PAGEDOWN:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_palette += 16;
		break;
	case SDLK_RETURN:
		if (!NO_MODIFIER(k->mod))
			return 0;
		// if (selected_palette == -1) return 1;
		palette_load_preset(selected_palette);
		palette_apply();
		update_thumbbars();
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_RIGHT:
	case SDLK_TAB:
		if (k->mod & KMOD_SHIFT) {
			change_focus_to(focus_offsets[selected_palette+1] + 29);
			return 1;
		}
		if (!NO_MODIFIER(k->mod))
			return 0;
		change_focus_to(focus_offsets[selected_palette+1] + 8);
		return 1;
	case SDLK_LEFT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		change_focus_to(focus_offsets[selected_palette+1] + 29);
		return 1;
	default:
		if (k->mouse == MOUSE_NONE)
			return 0;
	}

	new_palette = CLAMP(new_palette, 0, NUM_PALETTES - 1);

	if (new_palette != selected_palette || load_selected_palette) {
		selected_palette = new_palette;

		if(load_selected_palette) {
			palette_load_preset(selected_palette);
			palette_apply();
			update_thumbbars();
		}

		status.flags |= NEED_UPDATE;
	}

	return 1;
}

/* --------------------------------------------------------------------- */

static void palette_list_handle_key(struct key_event * k)
{
	int n = *selected_widget;

	if (!NO_MODIFIER(k->mod))
		return;

	if (k->state == KEY_RELEASE)
		return;

	switch (k->sym) {
	case SDLK_PAGEUP:
		n -= 3;
		break;
	case SDLK_PAGEDOWN:
		n += 3;
		break;
	default:
		return;
	}

	if (status.flags & CLASSIC_MODE) {
		if (n < 0)
			return;
		if (n > 48)
			n = 48;
	} else {
		n = CLAMP(n, 0, 48);
	}
	if (n != *selected_widget)
		change_focus_to(n);
}

static void palette_copy_to_clipboard(void) {
	char palette_text[49] = "";
	palette_to_string(palette_text);
	clippy_select(widgets_palette + 49, palette_text, 49);
	clippy_yank();
}

static void palette_paste_from_clipboard(void) {
	clippy_paste(CLIPPY_BUFFER);
}

static int palette_paste_callback(UNUSED int cb, const void *data)
{
	if (!data) return 0;
	const char* str = (const char *)data;

	int result = set_palette_from_string(data);

	if(!result) {
		status_text_flash("Bad character or wrong length");
		return 0;
	}

	selected_palette = 0;
	palette_load_preset(selected_palette);
	palette_apply();
	update_thumbbars();
	status.flags |= NEED_UPDATE;

	status_text_flash("Palette pasted");

	return 1;
}

/* --------------------------------------------------------------------- */

/* TODO | update_palette should only change the palette index for the color that's being changed, not all
   TODO | of them. also, it should call ccache_destroy_color(n) instead of wiping out the whole character
   TODO | cache whenever a color value is changed. */

static void update_palette(void)
{
	int n;

	for (n = 0; n < 16; n++) {
		current_palette[n][0] = widgets_palette[3 * n].d.thumbbar.value;
		current_palette[n][1] = widgets_palette[3 * n + 1].d.thumbbar.value;
		current_palette[n][2] = widgets_palette[3 * n + 2].d.thumbbar.value;
	}
	selected_palette = current_palette_index = 0;
	palette_apply();
	cfg_save();
	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

void palette_load_page(struct page *page)
{
	page->title = "Palette Configuration (Ctrl-F12)";
	page->draw_const = palette_draw_const;
	page->handle_key = palette_list_handle_key;
	page->total_widgets = 51;
	page->selected_widget = 48;
	page->widgets = widgets_palette;
	page->help_index = HELP_GLOBAL;
	page->clipboard_paste = palette_paste_callback;

	selected_palette = current_palette_index;

	for (int n = 0; n < 16; n++) {
		int tabs[3] = { 3 * n + 21, 3 * n + 22, 3 * n + 23 };
		if (n >= 9 && n <= 13) {
			tabs[0] = tabs[1] = tabs[2] = 48;
		} else if (n > 13) {
			tabs[0] = 3 * n - 42;
			tabs[1] = 3 * n - 41;
			tabs[2] = 3 * n - 40;
		}
		create_thumbbar(widgets_palette + (3 * n), 10 + 27 * (n / 7), 5 * (n % 7) + 14, 9,
				n ? (3 * n - 1) : 0, 3 * n + 1, tabs[0], update_palette, 0, 63);
		create_thumbbar(widgets_palette + (3 * n + 1), 10 + 27 * (n / 7), 5 * (n % 7) + 15, 9,
				3 * n, 3 * n + 2, tabs[1], update_palette, 0, 63);
		create_thumbbar(widgets_palette + (3 * n + 2), 10 + 27 * (n / 7), 5 * (n % 7) + 16, 9,
				3 * n + 1, 3 * n + 3, tabs[2], update_palette, 0, 63);
	}
	update_thumbbars();

	create_other(widgets_palette + 48, 0, palette_list_handle_key_on_list, NULL, palette_list_draw);
	widgets_palette[48].x = 56;
	widgets_palette[48].y = 26;
	widgets_palette[48].width = 20;
	widgets_palette[48].height = 15;

	for(int i = 6; i < 18; i++) {
		widgets_palette[i].next.backtab = 48;
	}

	create_button(widgets_palette + 49, 55, 43, 20, 48, 50, 39, 18, 18, palette_copy_to_clipboard, "Copy To Clipboard", 3);
	create_button(widgets_palette + 50, 55, 46, 20, 49, 0, 39, 18, 18, palette_paste_from_clipboard, "Paste From Clipboard", 1);

	widgets_palette[0].next.up = 50;
	widgets_palette[39].next.tab = 49;
	widgets_palette[40].next.tab = 49;
	widgets_palette[41].next.tab = 49;
}
