/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
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

#include <SDL.h>
#include <stdarg.h>

#include "it.h"
#include "song.h"
#include "page.h"

/* --------------------------------------------------------------------- */

static char *status_text = NULL;
static struct timeval text_timeout;

/* --------------------------------------------------------------------- */

void status_text_flash(const char *format, ...)
{
        va_list ap;

        if (gettimeofday(&text_timeout, NULL) < 0) {
                perror("gettimeofday");
                return;
        }

        /* the message expires in one second */
        text_timeout.tv_sec++;

        if (status_text)
                free(status_text);

        va_start(ap, format);
        vasprintf(&status_text, format, ap);
        va_end(ap);

        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static inline void draw_song_playing_status(void)
{
        int pos = 2;
        char buf[16];
        int pattern = song_get_current_pattern();

        SDL_LockSurface(screen);

        pos += draw_text_unlocked("Playing, Order: ", 2, 9, 0, 2);
        pos += draw_text_unlocked(numtostr(0, song_get_current_order(), buf), pos, 9, 3, 2);
        draw_char_unlocked('/', pos, 9, 0, 2);
        pos++;
        pos += draw_text_unlocked(numtostr(0, song_get_num_orders(), buf), pos, 9, 3, 2);
        pos += draw_text_unlocked(", Pattern: ", pos, 9, 0, 2);
        pos += draw_text_unlocked(numtostr(0, pattern, buf), pos, 9, 3, 2);
        pos += draw_text_unlocked(", Row: ", pos, 9, 0, 2);
        pos += draw_text_unlocked(numtostr(0, song_get_current_row(), buf), pos, 9, 3, 2);
        draw_char_unlocked('/', pos, 9, 0, 2);
        pos++;
        pos += draw_text_unlocked(numtostr(0, song_get_pattern(pattern, NULL), buf), pos, 9, 3, 2);
        draw_char_unlocked(',', pos, 9, 0, 2);
        pos++;
        draw_char_unlocked(0, pos, 9, 0, 2);
        pos++;
        pos += draw_text_unlocked(numtostr(0, song_get_playing_channels(), buf), pos, 9, 3, 2);
        SDL_UnlockSurface(screen);
	
        if (draw_text_len(" Channels", 62 - pos, pos, 9, 0, 2) < 9)
                draw_char(16, 61, 9, 1, 2);
}

static inline void draw_pattern_playing_status(void)
{
        int pos = 2;
        char buf[16];
        int pattern = song_get_current_pattern();

        SDL_LockSurface(screen);

        pos += draw_text_unlocked("Playing, Pattern: ", 2, 9, 0, 2);
        pos += draw_text_unlocked(numtostr(0, pattern, buf), pos, 9, 3, 2);
        pos += draw_text_unlocked(", Row: ", pos, 9, 0, 2);
        pos += draw_text_unlocked(numtostr(0, song_get_current_row(), buf), pos, 9, 3, 2);
        draw_char_unlocked('/', pos, 9, 0, 2);
        pos++;
        pos += draw_text_unlocked(numtostr(0, song_get_pattern(pattern, NULL), buf), pos, 9, 3, 2);
        draw_char_unlocked(',', pos, 9, 0, 2);
        pos++;
        draw_char_unlocked(0, pos, 9, 0, 2);
        pos++;
        pos += draw_text_unlocked(numtostr(0, song_get_playing_channels(), buf), pos, 9, 3, 2);
        SDL_UnlockSurface(screen);

        if (draw_text_len(" Channels", 62 - pos, pos, 9, 0, 2) < 9)
                draw_char(16, 61, 9, 1, 2);
}

static inline void draw_playing_channels(void)
{
	int pos = 2;
	char buf[16];
	
	SDL_LockSurface(screen);
	
	pos += draw_text_unlocked("Playing, ", 2, 9, 0, 2);
	pos += draw_text_unlocked(numtostr(0, song_get_playing_channels(), buf), pos, 9, 3, 2);
	draw_text_unlocked(" Channels", pos, 9, 0, 2);
	
	SDL_UnlockSurface(screen);
}

void status_text_redraw(void)
{
        struct timeval tv;

        gettimeofday(&tv, NULL);

        /* if there's a message set, and it's expired, clear it */
        if (status_text && timercmp(&tv, &text_timeout, >)) {
                free(status_text);
                status_text = NULL;
        }

        if (status_text) {
                draw_text_len(status_text, 60, 2, 9, 0, 2);
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
			/* else... fall through */
		default:
			//draw_fill_chars(2, 9, 62, 9, 2);
                        break;
                }
        }
}
