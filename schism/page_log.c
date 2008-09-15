/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
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

/* It's lo-og, lo-og, it's big, it's heavy, it's wood!
 * It's lo-og, lo-og, it's better than bad, it's good! */

#include "headers.h"

#include "it.h"
#include "page.h"

#include "sdlmain.h"

#include <stdarg.h>

/* --------------------------------------------------------------------- */

static struct widget widgets_log[1];

#define NUM_LINES 1000
static struct log_line lines[NUM_LINES];
static int top_line = 0;
static int last_line = -1;

/* --------------------------------------------------------------------- */

static void log_draw_const(void)
{
        draw_box(1, 12, 78, 48, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_fill_chars(2, 13, 77, 47, 0);
}

static int log_handle_key(struct key_event * k)
{
	switch (k->sym) {
	case SDLK_UP:
		if (!NO_MODIFIER(k->mod)) return 0;
		if (k->state) return 1;
		top_line--;
		break;
	case SDLK_PAGEUP:
		if (!NO_MODIFIER(k->mod)) return 0;
		if (k->state) return 1;
		top_line -= 15;
		break;
	case SDLK_DOWN:
		if (!NO_MODIFIER(k->mod)) return 0;
		if (k->state) return 1;
		top_line++;
		break;
	case SDLK_PAGEDOWN:
		if (!NO_MODIFIER(k->mod)) return 0;
		if (k->state) return 1;
		top_line += 15;
		break;
	default:
		if (!k->state) {
			if (k->mouse == MOUSE_SCROLL_UP) {
				top_line--;
				break;
			} else if (k->mouse == MOUSE_SCROLL_DOWN) {
				top_line++;
				break;
			}
		}
			
		return 0;
	};
	top_line = CLAMP(top_line, 0, (last_line-33));
	if (top_line < 0) top_line = 0;
	status.flags |= NEED_UPDATE;
        return 1;
}

static void log_redraw(void)
{
        int n, i;

	i = top_line;
        for (n = 0; n <= last_line && n < 33; n++, i++) {
		if (!lines[i].text) continue;
		if (lines[i].bios_font) {
			draw_text_bios_len(lines[i].text,
					74, 3, 14 + n,
					lines[i].color, 0);
		} else {
			draw_text_len(lines[i].text,
					74, 3, 14 + n,
					lines[i].color, 0);
		}
	}
}

/* --------------------------------------------------------------------- */

void log_load_page(struct page *page)
{
        page->title = "Message Log Viewer (Ctrl-F11)";
        page->draw_const = log_draw_const;
        page->total_widgets = 1;
        page->widgets = widgets_log;
        page->help_index = HELP_GLOBAL;

	create_other(widgets_log + 0, 1, log_handle_key, log_redraw);
}

/* --------------------------------------------------------------------- */

inline void log_append2(int bios_font, int color, int must_free, const char *text)
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
        lines[last_line].bios_font = bios_font;
	top_line = CLAMP(last_line - 32, 0, NUM_LINES-32);

        if (status.current_page == PAGE_LOG)
                status.flags |= NEED_UPDATE;
}
inline void log_append(int color, int must_free, const char *text)
{
	log_append2(0, color, must_free, text);
}
inline void log_nl(void)
{
	log_append(0,0,"");
}
void log_appendf(int color, const char *format, ...)
{
        char *ptr;
        va_list ap;

        va_start(ap, format);
        if (vasprintf(&ptr, format, ap) == -1) {
		perror("asprintf");
		exit(255);
	}
        va_end(ap);

	if (!ptr) {
		perror("asprintf");
		exit(255);
	}

        log_append(color, 1, ptr);
}
