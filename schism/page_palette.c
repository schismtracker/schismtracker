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
#include "config.h"
#include "page.h"
#include "clippy.h"
#include "palettes.h"
#include "widget.h"
#include "vgamem.h"
#include "keyboard.h"

/* --------------------------------------------------------------------- */

static struct widget widgets_palette[51];

/* deprecated name */
#define selected_palette widgets_palette[48].d.listbox.focus

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
		draw_fill_chars(3, 14 + (5 * n), 7, 16 + (5 * n), DEFAULT_FG, n);
		draw_fill_chars(30, 14 + (5 * n), 34, 16 + (5 * n), DEFAULT_FG, n + 7);
	}

	draw_box(56, 13, 62, 17, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(63, 13, 73, 17, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(56, 18, 62, 22, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(63, 18, 73, 22, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(54, 25, 77, 41, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(57, 14, 61, 16, DEFAULT_FG, 14);
	draw_fill_chars(57, 19, 61, 21, DEFAULT_FG, 15);
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

static void palette_copy_palette_to_clipboard(int which) {
	char palette_text[49];
	palette_to_string(which, palette_text);
	palette_text[48] = 0;

	clippy_select(widgets_palette + 49, palette_text, 49);
	clippy_yank();
}

static void palette_copy_current_to_clipboard(void) {
	palette_copy_palette_to_clipboard(current_palette_index);
}

static void palette_paste_from_clipboard(void) {
	clippy_paste(CLIPPY_BUFFER);
}

static int palette_paste_callback(SCHISM_UNUSED int cb, const void *data)
{
	if (!data) return 0;

	int result = set_palette_from_string((const char*)data);

	if (!result) {
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

static const int palette_list_focus_offsets_left_[] = {
	29, 30, 30, 31, 32, 32, 33, 33, 34, 35,
	35, 36, 36, 37, 38, 39, 40, 40, 41, 42,
};

static const int palette_list_focus_offsets_right_[] = {
	8,  9,  9,  10, 11, 11, 12, 12, 13, 14,
	14, 15, 15, 16, 17, 18, 19, 19, 20, 21,
};

static uint32_t palette_list_size_(void)
{
	/* ok */
	return palettes_size();
}

static const char *palette_list_name_(uint32_t i)
{
	return palettes[i].name;
}

static int palette_list_toggled_(uint32_t i)
{
	return ((int)i == current_palette_index);
}

static void palette_list_activate_(void)
{
	palette_load_preset(selected_palette);
	palette_apply();
	update_thumbbars();

	status.flags |= NEED_UPDATE;
}

static int palette_list_handle_key_(struct key_event *kk)
{
	switch (kk->sym) {
	case SCHISM_KEYSYM_c:
		/* pasting is handled by the page */
		if (kk->mod & SCHISM_KEYMOD_CTRL) {
			palette_copy_palette_to_clipboard(selected_palette);
			return 1;
		}
		break;
	default:
		break;
	}

	return 0;
}

/* --------------------------------------------------------------------- */

static void palette_list_handle_key(struct key_event * k)
{
	int n = *selected_widget;

	if (k->state == KEY_RELEASE)
		return;

	switch (k->sym) {
	case SCHISM_KEYSYM_PAGEUP:
		if (!NO_MODIFIER(k->mod))
			n -= 3;
		break;
	case SCHISM_KEYSYM_PAGEDOWN:
		if (!NO_MODIFIER(k->mod))
			n += 3;
		break;
	case SCHISM_KEYSYM_c:
		if (k->mod & SCHISM_KEYMOD_CTRL) {
			palette_copy_current_to_clipboard();
			return;
		}
		break;
	case SCHISM_KEYSYM_v:
		if (k->mod & SCHISM_KEYMOD_CTRL) {
			palette_paste_from_clipboard();
			return;
		}
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
		widget_change_focus_to(n);
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
	memcpy(palettes[0].colors, current_palette, sizeof(current_palette));
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
	page->total_widgets = 51;
	page->selected_widget = 48;
	page->widgets = widgets_palette;
	page->help_index = HELP_PALETTES;
	page->clipboard_paste = palette_paste_callback;

	selected_palette = current_palette_index;

	for (n = 0; n < 16; n++) {
		int tabs[3];
		for (int x = 0; x < 3; x++)
			tabs[x] = 3 * n + (21 + x);
		if (n >= 9 && n <= 13) {
			tabs[0] = tabs[1] = tabs[2] = 48;
		} else if (n > 13) {
			tabs[0] = 3 * n - 42;
			tabs[1] = 3 * n - 41;
			tabs[2] = 3 * n - 40;
		}
		widget_create_thumbbar(widgets_palette + (3 * n), 10 + 27 * (n / 7), 5 * (n % 7) + 14, 9,
				n ? (3 * n - 1) : 0, 3 * n + 1, tabs[0], update_palette, 0, 63);
		widget_create_thumbbar(widgets_palette + (3 * n + 1), 10 + 27 * (n / 7), 5 * (n % 7) + 15, 9,
				3 * n, 3 * n + 2, tabs[1], update_palette, 0, 63);
		widget_create_thumbbar(widgets_palette + (3 * n + 2), 10 + 27 * (n / 7), 5 * (n % 7) + 16, 9,
				3 * n + 1, 3 * n + 3, tabs[2], update_palette, 0, 63);
	}
	update_thumbbars();

	widget_create_listbox(widgets_palette+48, palette_list_size_,
		palette_list_toggled_, palette_list_name_, NULL,
		palette_list_activate_, palette_list_handle_key_,
		palette_list_focus_offsets_left_,
		palette_list_focus_offsets_right_,
		47, 49);
	widgets_palette[48].x = 55;
	widgets_palette[48].y = 26;
	widgets_palette[48].width = 22;
	widgets_palette[48].height = 15;

	for (n = 6; n < 18; n++)
		widgets_palette[n].next.backtab = 48;

	widget_create_button(widgets_palette + 49, 55, 43, 20, 48, 50, 39, 18, 18, palette_copy_current_to_clipboard, "Copy To Clipboard", 3);
	widget_create_button(widgets_palette + 50, 55, 46, 20, 49, 0, 39, 18, 18, palette_paste_from_clipboard, "Paste From Clipboard", 1);

	widgets_palette[0].next.up = 50;
	widgets_palette[39].next.tab = 49;
	widgets_palette[40].next.tab = 49;
	widgets_palette[41].next.tab = 49;
}
