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

/* It's lo-og, lo-og, it's big, it's heavy, it's wood!
 * It's lo-og, lo-og, it's better than bad, it's good! */

#include "headers.h"

#include "it.h"
#include "page.h"

#include <SDL.h>
#include <stdarg.h>

/* --------------------------------------------------------------------- */

static struct item items_log[1];

#define NUM_LINES 33
static struct log_line lines[NUM_LINES];
static int last_line = -1;

/* --------------------------------------------------------------------- */

static void log_draw_const(void)
{
        draw_box(1, 12, 78, 48, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_fill_chars(2, 13, 77, 47, 0);
}

static int log_handle_key(UNUSED SDL_keysym * k)
{
        return 0;
}

static void log_redraw(void)
{
        int n;

        for (n = 0; n <= last_line; n++)
                draw_text_len(lines[n].text, 74, 3, 14 + n,
			      lines[n].color, 0);
}

/* --------------------------------------------------------------------- */

void log_load_page(struct page *page)
{
        page->title = "Message Log Viewer (Ctrl-F1)";
        page->draw_const = log_draw_const;
        page->total_items = 1;
        page->items = items_log;
        page->help_index = HELP_GLOBAL;

	create_other(items_log + 0, 1, log_handle_key, log_redraw);
}

/* --------------------------------------------------------------------- */

inline void log_append(int color, int must_free, const char *text)
{
        if (last_line < NUM_LINES - 1) {
                last_line++;
        } else {
                if (lines[0].must_free)
                        free((void *) lines[0].text);
		memmove(lines, lines + 1, last_line * sizeof(struct log_line));
        }
        lines[last_line].text = text;
        lines[last_line].color = color;
        lines[last_line].must_free = must_free;

        if (status.current_page == PAGE_LOG) {
                /* FIXME | it'd be nice to blit the area and only draw one
                 * FIXME | line, instead of redrawing the whole thing. */
                status.flags |= NEED_UPDATE;
        }
}

void log_appendf(int color, const char *format, ...)
{
        char *ptr;
        va_list ap;

        va_start(ap, format);
        vasprintf(&ptr, format, ap);
        va_end(ap);

        log_append(color, 1, ptr);
}
