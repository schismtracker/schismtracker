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

#define NEED_TIME
#include "headers.h"

#include <stdarg.h>

#include "it.h"
#include "song.h"
#include "page.h"

#include "sndfile.h"

#include "sdlmain.h"

/* --------------------------------------------------------------------- */

static int status_bios = 0;
static char *status_text = NULL;
static uint32_t text_timeout;

/* --------------------------------------------------------------------- */

void status_text_flash(const char *format, ...)
{
	va_list ap;

	text_timeout = SDL_GetTicks() + 1000;

	if (status_text)
		free(status_text);

	status_bios = 0;
	va_start(ap, format);
	if (vasprintf(&status_text, format, ap) == -1) abort();
	va_end(ap);

	status.flags |= NEED_UPDATE;
}

void status_text_flash_bios(const char *format, ...)
{
	va_list ap;

	text_timeout = SDL_GetTicks() + 1000;

	if (status_text)
		free(status_text);

	status_bios = 1;
	va_start(ap, format);
	if (vasprintf(&status_text, format, ap) == -1) abort();
	va_end(ap);

	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static inline int _loop_count(char *buf, int pos)
{
	if (current_song->repeat_count < 1 || (status.flags & CLASSIC_MODE)) {
		pos += draw_text("Playing", pos, 9, 0, 2);
	} else {
		pos += draw_text("Loop: ", pos, 9, 0, 2);
		pos += draw_text(numtostr(0, current_song->repeat_count, buf), pos, 9, 3, 2);
	}
	return pos;
}

static inline void draw_song_playing_status(void)
{
	int pos = 2;
	char buf[16];
	int pattern = song_get_playing_pattern();

	pos = _loop_count(buf, pos);
	pos += draw_text(", Order: ", pos, 9, 0, 2);
	pos += draw_text(numtostr(0, song_get_current_order(), buf), pos, 9, 3, 2);
	draw_char('/', pos, 9, 0, 2);
	pos++;
	pos += draw_text(numtostr(0, csf_last_order(current_song), buf), pos, 9, 3, 2);
	pos += draw_text(", Pattern: ", pos, 9, 0, 2);
	pos += draw_text(numtostr(0, pattern, buf), pos, 9, 3, 2);
	pos += draw_text(", Row: ", pos, 9, 0, 2);
	pos += draw_text(numtostr(0, song_get_current_row(), buf), pos, 9, 3, 2);
	draw_char('/', pos, 9, 0, 2);
	pos++;
	pos += draw_text(numtostr(0, song_get_pattern(pattern, NULL), buf), pos, 9, 3, 2);
	draw_char(',', pos, 9, 0, 2);
	pos++;
	draw_char(0, pos, 9, 0, 2);
	pos++;
	pos += draw_text(numtostr(0, song_get_playing_channels(), buf), pos, 9, 3, 2);

	if (draw_text_len(" Channels", 62 - pos, pos, 9, 0, 2) < 9)
		draw_char(16, 61, 9, 1, 2);
}

static inline void draw_pattern_playing_status(void)
{
	int pos = 2;
	char buf[16];
	int pattern = song_get_playing_pattern();

	pos = _loop_count(buf, pos);
	pos += draw_text(", Pattern: ", pos, 9, 0, 2);
	pos += draw_text(numtostr(0, pattern, buf), pos, 9, 3, 2);
	pos += draw_text(", Row: ", pos, 9, 0, 2);
	pos += draw_text(numtostr(0, song_get_current_row(), buf), pos, 9, 3, 2);
	draw_char('/', pos, 9, 0, 2);
	pos++;
	pos += draw_text(numtostr(0, song_get_pattern(pattern, NULL), buf), pos, 9, 3, 2);
	draw_char(',', pos, 9, 0, 2);
	pos++;
	draw_char(0, pos, 9, 0, 2);
	pos++;
	pos += draw_text(numtostr(0, song_get_playing_channels(), buf), pos, 9, 3, 2);

	if (draw_text_len(" Channels", 62 - pos, pos, 9, 0, 2) < 9)
		draw_char(16, 61, 9, 1, 2);
}

static inline void draw_playing_channels(void)
{
	int pos = 2;
	char buf[16];

	pos += draw_text("Playing, ", 2, 9, 0, 2);
	pos += draw_text(numtostr(0, song_get_playing_channels(), buf), pos, 9, 3, 2);
	draw_text(" Channels", pos, 9, 0, 2);
}

void status_text_redraw(void)
{
	uint32_t now = SDL_GetTicks();

	/* if there's a message set, and it's expired, clear it */
	if (status_text && now > text_timeout) {
		free(status_text);
		status_text = NULL;
	}

	if (status_text) {
		if (status_bios) {
			draw_text_bios_len(status_text, 60, 2, 9, 0, 2);
		} else {
			draw_text_len(status_text, 60, 2, 9, 0, 2);
		}
	} else {
		switch (song_get_mode()) {
		case MODE_PLAYING:
			draw_song_playing_status();
			break;
		case MODE_PATTERN_LOOP:
			draw_pattern_playing_status();
			break;
		case MODE_SINGLE_STEP:
			if (song_get_playing_channels() > 1) {
				draw_playing_channels();
				break;
			}
		default:
			break;
		}
	}
}
