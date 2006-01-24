/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
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

#include "auto/logoit.h"
#include "auto/logoschism.h"

#include "headers.h"
#include "it.h"
#include "page.h"
#include "video.h"

static SDL_Surface *it_logo = 0;
static SDL_Surface *schism_logo = 0;

static struct widget widgets_about[1];


static int about_page_handle_key(UNUSED struct key_event *k)
{
	return 0;
}
static void about_page_redraw(void)
{
	draw_fill_chars(0,0,79,49,0);
}

static int _fixup_ignore_globals(struct key_event *k)
{
	if (k->mouse && k->y > 20) return 0;
	switch (k->sym) {
	case SDLK_LEFT:
	case SDLK_RIGHT:
	case SDLK_DOWN:
	case SDLK_UP:
	case SDLK_TAB:
	case SDLK_RETURN:
	case SDLK_ESCAPE:
		/* use default handler */
		return 0;
	};
	/* this way, we can't pull up help here */
	return 1;
}
static void _draw_full(void)
{
}

void about_load_page(struct page *page)
{
	page->title = "";
	page->total_widgets = 1;
	page->widgets = widgets_about;
	page->pre_handle_key = _fixup_ignore_globals;
	page->help_index = HELP_GLOBAL;
	page->draw_full = _draw_full;
	create_other(widgets_about+0, 0, about_page_handle_key, about_page_redraw);
}

static struct widget about_widgets[1];
static struct vgamem_overlay logo_image = {
	24,18,
	57,23,
};

static void about_close(UNUSED void *data)
{
	dialog_destroy();
	if (status.current_page == PAGE_ABOUT) set_page(PAGE_LOAD_MODULE);
	status.flags |= NEED_UPDATE;
}
static void about_draw_const(void)
{
	static char buf[80];

	if (status.current_page == PAGE_ABOUT) {
		/* redraw outer part */
		draw_box(11,16, 68, 34, BOX_THIN | BOX_OUTER | BOX_FLAT_DARK);
	}

	if (status.flags & CLASSIC_MODE) {
		draw_box(25,25, 56, 30, BOX_THIN | BOX_OUTER | BOX_FLAT_DARK);

		draw_text("Sound Card Setup", 32, 26, 0, 2);

		if (strcasecmp(song_audio_driver(), "nosound") == 0) {
			draw_text("No sound card detected", 29, 28, 0, 2);
		} else {
			draw_text("Sound Blaster 16 detected", 26, 28, 0, 2);
			draw_text("Port 220h, IRQ 7, DMA 5", 26, 29, 0, 2);
		}
	} else {
		draw_text("Schism Tracker is Copyright (C) 2003-2005",
					21,25, 1, 2);
		draw_text("Written by Storlek, and Mrs. Brisby, and contains code",
					12,27, 1, 2);
		draw_text("written by Olivier Lapicque, Markus Fick, Adam Goode,",
					12,28, 1, 2);
		draw_text("Ville Jokela, Juan Linietsky, Juha Niemimaki, and others",
					12,29, 1, 2);
		draw_text("and is based on Impulse Tracker by Jeffrey Lim.",
					12,30, 1, 2);

		if (status.current_page == PAGE_ABOUT) {
			draw_text("This program is free software; you can redistribute it and/or modify", 1, 1, 6, 0);
			draw_text("it under the terms of the GNU General Public License as published by", 1, 2, 6, 0);
			draw_text("the Free Software Foundation; either version 2 of the License, or", 1, 3, 6, 0);
			draw_text("(at your option) any later version.", 1,4,6,0);

			draw_text("This program is distributed in the hope that it will be useful,", 1,6,6,0);
			draw_text("but WITHOUT ANY WARRANTY; without even the implied warranty of", 1,7,6,0);
			draw_text("MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the", 1,8,6,0);
			draw_text("GNU General Public License for more details.", 1,9,6,0);

			draw_text("You should have received a copy of the GNU General Public License", 1, 11,6,0);
			draw_text("along with this program; if not, write to the Free Software", 1,12,6,0);
			draw_text("Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA", 1,13,6,0);

			sprintf(buf, "Using %s on %s", song_audio_driver(), video_driver_name());
			draw_text(buf, 79 - strlen(buf), 48, 6, 0);
		}
	}
	vgamem_fill_reserve(&logo_image, 0, 2);
}

void show_about(void)
{
	static int didit = 0;
	struct dialog *d;
	unsigned char *p;
	int x, y;
	
	if (!didit) {
		vgamem_font_reserve(&logo_image);
		it_logo = xpmdata(_logo_it_xpm);
		schism_logo = xpmdata(_logo_schism_xpm);
	}

	if (status.flags & CLASSIC_MODE) {
		p = it_logo ? it_logo->pixels : 0;
	} else {
		p = schism_logo ? schism_logo->pixels : 0;
	}

	/* this is currently pretty gross */
	vgamem_clear_reserve(&logo_image);
	if (p) {
#define LOGO_WIDTH 272
#define LOGO_PITCH 272
#define LOGO_HEIGHT 40
		for (y = 0; y < LOGO_HEIGHT; y++) {
			for (x = 0; x < LOGO_WIDTH; x++) {
				if (p[x]) {
					vgamem_font_clearpixel(&logo_image, x+2, y+6);
				} else {
					vgamem_font_putpixel(&logo_image, x+2, y+6);
				}
			}
			vgamem_font_clearpixel(&logo_image, x, y+6);
			vgamem_font_clearpixel(&logo_image, x+1, y+6);
			p += LOGO_PITCH;
		}
	}

	create_button(about_widgets+0,
			33,32,
			12,
			0,0,0,0,0,
			about_close, "Continue", 3);
	d = dialog_create_custom(11,16,
			58, 19,
			about_widgets, 1, 0,
			about_draw_const, NULL);
	d->action_yes = about_close;
	d->action_no = about_close;
	d->action_cancel = about_close;
}


