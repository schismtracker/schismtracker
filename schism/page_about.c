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
#include "page.h"
#include "video.h"
#include "song.h"

/* Eventual TODO: draw the pattern data in the Schism logo in a different color than the words */
#include "auto/logoit.h"
#include "auto/logoschism.h"

#define LOGO_WIDTH 292
#define LOGO_PITCH 292
#define LOGO_HEIGHT 50

static int fake_driver = 0;
static SDL_Surface *it_logo = 0;
static SDL_Surface *schism_logo = 0;

static struct widget widgets_about[1];

static struct vgamem_overlay logo_image = {
	23, 17,
	58, 24,
	0, 0, 0, 0, 0, 0,
};


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
	case SDLK_F1...SDLK_F5:
	case SDLK_F9...SDLK_F12:
		dialog_destroy();
		return 0;
	default:
		break;
	}
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
	create_other(widgets_about + 0, 0, about_page_handle_key, about_page_redraw);
}

static void about_close(UNUSED void *data)
{
	dialog_destroy();
	if (status.current_page == PAGE_ABOUT) set_page(PAGE_LOAD_MODULE);
	status.flags |= NEED_UPDATE;
}
static void about_draw_const(void)
{
	char buf[81];

	if (status.current_page == PAGE_ABOUT) {
		/* redraw outer part */
		draw_box(11,16, 68, 34, BOX_THIN | BOX_OUTER | BOX_FLAT_DARK);
	}

	if (status.flags & CLASSIC_MODE) {
		draw_box(25,25, 56, 30, BOX_THIN | BOX_OUTER | BOX_FLAT_DARK);

		draw_text((unsigned char *) "Sound Card Setup", 32, 26, 0, 2);

		if (strcasecmp((char *) song_audio_driver(), "nosound") == 0) {
			draw_text((unsigned char *) "No sound card detected", 29, 28, 0, 2);
		} else {
			switch (fake_driver) {
			case 0:
				draw_text((unsigned char *) "Sound Blaster 16 detected", 26, 28, 0, 2);
				draw_text((unsigned char *) "Port 220h, IRQ 7, DMA 5", 26, 29, 0, 2);
				break;
			case 1:
				draw_text((unsigned char *) "Gravis UltraSound detected", 26, 28, 0, 2);
				draw_text((unsigned char *) "Port 240h, IRQ 5, 1024k RAM", 26, 29, 0, 2);
				break;
			};
		}
	} else {
		snprintf(buf, 80, "Using %s on %s", song_audio_driver(), video_driver_name());
		buf[80] = 0;
		draw_text((unsigned char *) buf, (80 - strlen(buf)) / 2, 25, 0, 2);
		/* build date? */
		draw_text((unsigned char *) "Copyright (C) 2003-2007 Storlek and Mrs. Brisby", 15, 27, 1, 2);
		draw_text((unsigned char *) "Based on Impulse Tracker by Jeffrey Lim aka Pulse", 15, 28, 1, 2);
		/* XXX if we allow key remapping, need to reflect the *real* log viewer key here */
		draw_text((unsigned char *) "Press Ctrl-F11 for copyright and full credits", 15, 29, 1, 2);
	}
	vgamem_fill_reserve(&logo_image, 
		(status.flags & CLASSIC_MODE) ? 11 : 0,
		2);
}

void show_about(void)
{
	static int didit = 0;
	struct dialog *d;
	unsigned char *p;
	int x, y;
	
	fake_driver = (rand() & 3) ? 0 : 1;

	if (!didit) {
		vgamem_font_reserve(&logo_image);
		it_logo = xpmdata(_logo_it_xpm);
		schism_logo = xpmdata(_logo_schism_xpm);
		didit=1;
	}

	if (status.flags & CLASSIC_MODE) {
		p = it_logo ? it_logo->pixels : 0;
	} else {
		p = schism_logo ? schism_logo->pixels : 0;
	}

	/* this is currently pretty gross */
	vgamem_clear_reserve(&logo_image);
	if (p) {
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

	create_button(widgets_about + 0,
			33,32,
			12,
			0,0,0,0,0,
			(void *) about_close, "Continue", 3);
	d = dialog_create_custom(11,16,
			58, 19,
			widgets_about, 1, 0,
			about_draw_const, NULL);
	d->action_yes = about_close;
	d->action_no = about_close;
	d->action_cancel = about_close;
}


