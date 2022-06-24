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

#include "sdlmain.h"

/* --------------------------------------------------------------------- */

static struct widget widgets_palette[49];

static int selected_palette, max_palette = 0;

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

	draw_text("Predefined Palettes", 57, 25, 0, 2);

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
	draw_box(55, 26, 77, 47, BOX_THICK | BOX_INNER | BOX_INSET);
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

	draw_fill_chars(56, 27, 76, 46, 0);

	fg = 6;
	bg = 0;
	if (focused && -1 == selected_palette) {
		fg = 0;
		bg = 3;
	} else if (-1 == selected_palette) {
		bg = 14;
	}

	draw_text_len("User Defined", 21, 56, 27, fg, bg);
	for (n = 0; n < 19 && palettes[n].name[0]; n++) {
		fg = 6;
		bg = 0;
		if (focused && n == selected_palette) {
			fg = 0;
			bg = 3;
		} else if (n == selected_palette) {
			bg = 14;
		}
		draw_text_len(palettes[n].name, 21, 56, 28 + n, fg, bg);
	}
	max_palette = n;
}

static int palette_list_handle_key_on_list(struct key_event * k)
{
	int new_palette = selected_palette;
	const int focus_offsets[] = { 0, 1, 1, 2, 3, 3, 4, 4, 5, 6, 6, 7, 7, 8, 9, 9, 10, 10, 11, 12 };

	if (k->mouse == MOUSE_CLICK) {
		if (k->state == KEY_PRESS)
			return 0;
		if (k->x < 56 || k->y < 27 || k->y > 46 || k->x > 76) return 0;
		new_palette = (k->y - 28);
		if (new_palette == selected_palette) {
			// alright
			if (selected_palette == -1) return 1;
			palette_load_preset(selected_palette);
			palette_apply();
			update_thumbbars();
			status.flags |= NEED_UPDATE;
			return 1;
		}
	} else {
		if (k->state == KEY_RELEASE)
			return 0;
		if (k->mouse == MOUSE_SCROLL_UP)
			new_palette -= MOUSE_SCROLL_LINES;
		else if (k->mouse == MOUSE_SCROLL_DOWN)
			new_palette += MOUSE_SCROLL_LINES;
	}

	switch (k->sym.sym) {
	case SDLK_UP:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (--new_palette < -1) {
			change_focus_to(47);
			return 1;
		}
		break;
	case SDLK_DOWN:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_palette++;
		break;
	case SDLK_HOME:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_palette = 0;
		break;
	case SDLK_PAGEUP:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (new_palette == -1) {
			change_focus_to(45);
			return 1;
		}
		new_palette -= 16;
		break;
	case SDLK_END:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_palette = max_palette - 1;
		break;
	case SDLK_PAGEDOWN:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_palette += 16;
		break;
	case SDLK_RETURN:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (selected_palette == -1) return 1;
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

	if (new_palette < -1) new_palette = -1;
	else if (new_palette >= (max_palette-1)) new_palette = (max_palette-1);
	if (new_palette != selected_palette) {
		selected_palette = new_palette;
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

	switch (k->sym.sym) {
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
	selected_palette = current_palette_index = -1;
	palette_apply();
	cfg_save();
	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

void palette_load_page(struct page *page)
{
	int n;

	page->title = "Palette Configuration (Ctrl-F12)";
	page->draw_const = palette_draw_const;
	page->handle_key = palette_list_handle_key;
	page->total_widgets = 49;
	page->widgets = widgets_palette;
	page->help_index = HELP_GLOBAL;

	selected_palette = current_palette_index;

	for (n = 0; n < 16; n++) {
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

	create_other(widgets_palette + 48, 0, palette_list_handle_key_on_list, palette_list_draw);
	widgets_palette[48].x = 56;
	widgets_palette[48].y = 27;
	widgets_palette[48].width = 20;
	widgets_palette[48].height = 19;
}

