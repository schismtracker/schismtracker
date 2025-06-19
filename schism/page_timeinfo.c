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
#include "widget.h"
#include "vgamem.h"
#include "keyboard.h"
#include "song.h"
#include "mem.h"
#include "str.h"

/* --------------------------------------------------------------------- */

static struct widget widgets_timeinfo[1];

static int display_session = 0;

// list
static int top_line = 0;

/* --------------------------------------------------------------------- */

static void timeinfo_draw_const(void)
{
	draw_text("Module time:", 6, 13, 0, 2);
	draw_text("Current session:", 2, 14, 0, 2);
	draw_text("Total time:", 7, 16, 0, 2);
}

static int timeinfo_handle_key(struct key_event * k)
{
	switch (k->sym) {
	case SCHISM_KEYSYM_BACKQUOTE:
		if (k->state != KEY_RELEASE)
			return 0;

		if ((k->mod & SCHISM_KEYMOD_RALT) && (k->mod & SCHISM_KEYMOD_RSHIFT)) {
			display_session = !display_session;
			status.flags |= NEED_UPDATE;
			return 1;
		}
		return 0;
	case SCHISM_KEYSYM_s:
		if (k->state != KEY_RELEASE)
			return 0;

		if (!(status.flags & CLASSIC_MODE)) {
			display_session = !display_session;
			status.flags |= NEED_UPDATE;
			return 1;
		}
		return 0;
	default:
		break;
	}

	if (display_session) {
		switch (k->sym) {
		case SCHISM_KEYSYM_UP:
			if (k->state == KEY_RELEASE)
				return 1;
			top_line--;
			break;
		case SCHISM_KEYSYM_PAGEUP:
			if (k->state == KEY_RELEASE)
				return 1;
			top_line -= 15;
			break;
		case SCHISM_KEYSYM_DOWN:
			if (k->state == KEY_RELEASE)
				return 1;
			top_line++;
			break;
		case SCHISM_KEYSYM_PAGEDOWN:
			if (k->state == KEY_RELEASE)
				return 1;
			top_line += 15;
			break;
		case SCHISM_KEYSYM_HOME:
			if (k->state == KEY_RELEASE)
				return 1;
			top_line = 0;
			break;
		case SCHISM_KEYSYM_END:
			if (k->state == KEY_RELEASE)
				return 1;
			top_line = current_song->histlen;
			break;
		default:
			if (k->state == KEY_PRESS) {
				if (k->mouse == MOUSE_SCROLL_UP) {
					top_line -= MOUSE_SCROLL_LINES;
					break;
				} else if (k->mouse == MOUSE_SCROLL_DOWN) {
					top_line += MOUSE_SCROLL_LINES;
					break;
				}
			}

			return 0;
		}
		top_line = MIN(top_line, (ptrdiff_t)current_song->histlen);
		top_line = MAX(top_line, 0);
		status.flags |= NEED_UPDATE;
		return 1;
	} else {
		return 0;
	}
}

static inline void draw_time(uint64_t secs, int x, int y)
{
	char buf[64] = {0};

	secs = MIN(secs, UINT64_C(3599999)); // 999:59:59

	int amt = snprintf(buf, 64, "%4" PRIu64 ":%02" PRIu64 ":%02" PRIu64, secs / 3600, secs / 60 % 60, secs % 60);

	amt = CLAMP(amt, 0, 64);
	amt = MIN(amt, 80 - x);

	draw_text_len(buf, amt, x, y, 0, 2);
}

static void timeinfo_redraw(void)
{
	uint64_t total_secs = 0;

	{
		// Module time

		uint64_t msecs = 0;

		for (size_t i = 0; i < current_song->histlen; i++)
			msecs += current_song->history[i].runtime;

		const uint64_t secs = msecs / 1000;

		draw_time(secs, 18, 13);

		total_secs += secs;
	}

	{
		// Current session
		const time_t now = time(NULL);
		const time_t start = mktime(&current_song->editstart.time);

		double secs_d = difftime(now, start);

		draw_time(secs_d, 18, 14);

		total_secs += secs_d;
	}

	draw_time(total_secs, 18, 16);

	// draw the bar
	for (int x = 1; x < (VGAMEM_COLUMNS - 1); x++)
		draw_char(154, x, 18, 0, 2);

	if (display_session) {
		uint64_t session_secs = 0;

		for (size_t i = 0; i < current_song->histlen; i++) {
			char buf[27];

			const uint64_t runtime_secs = current_song->history[i].runtime / 1000;

			session_secs += runtime_secs;

			if ((int)i >= top_line && (int)i < top_line + 29) {
				if (current_song->history[i].time_valid) {
					str_date_from_tm(&current_song->history[i].time, buf, cfg_str_date_format);
					draw_text_len(buf, 27, 4, 20 + i - top_line, 0, 2);

					str_time_from_tm(&current_song->history[i].time, buf, cfg_str_time_format);
					draw_text_len(buf, 27, 29, 20 + i - top_line, 0, 2);
				} else {
					draw_text("<unknown date>", 4, 20 + i - top_line, 0, 2);
				}

				draw_time(runtime_secs, 44, 20 + i - top_line);
				draw_time(session_secs, 64, 20 + i - top_line);
			}
		}
	}
}

static void timeinfo_set_page(void)
{
	// reset this
	display_session = 0;
}

/* --------------------------------------------------------------------- */

void timeinfo_load_page(struct page *page)
{
	page->title = "Time Information";
	page->draw_const = timeinfo_draw_const;
	page->set_page = timeinfo_set_page;
	page->total_widgets = 1;
	page->widgets = widgets_timeinfo;
	page->help_index = HELP_TIME_INFORMATION;

	widget_create_other(widgets_timeinfo + 0, 0, timeinfo_handle_key, NULL, timeinfo_redraw);
}
