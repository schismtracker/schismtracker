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
#include "lua-engine.h"

#include "sdlmain.h"

#include <assert.h>
#include <time.h>

#define LINES 35
#define LINE_WRAP 76
#define SCROLLBACK_SIZE 8000
#define INPUT_SIZE 2000

static char scrollback [SCROLLBACK_SIZE+1];
static char *scrollback_cursor = scrollback;
#define scrollback_len() (scrollback_cursor - scrollback)

static char input [INPUT_SIZE+1];
static char *input_cursor = input;
#define input_len() (input_cursor - input)

static char input_pretty [(INPUT_SIZE*2)+1]; //rough estimation

static int top_line = 0;
static int cursor_line = 0;
static int cursor_char = 0;
/* this is the absolute cursor position from top of message.
 * (should be updated whenever cursor_line/cursor_char change) */
static int cursor_pos = 0;

static int tail_console = 1;

static struct widget widgets_lua[1];

static void scrollback_roll(int size)
{
	assert(size < SCROLLBACK_SIZE);
	assert(size > 0);

	// TODO: reclaim entire line when cutting mid-line
	memmove(scrollback, scrollback+SCROLLBACK_SIZE-size, size);

	scrollback_cursor -= size;
	*scrollback_cursor = 0;

	status.flags |= NEED_UPDATE;
}

static void scrollback_write(const char* p, int l)
{
	if (l > SCROLLBACK_SIZE) {
		p += l - SCROLLBACK_SIZE;
		l = SCROLLBACK_SIZE;
		scrollback_cursor = scrollback;
	} else if (scrollback_len() + l > SCROLLBACK_SIZE) {
		scrollback_roll(scrollback_len() + l - SCROLLBACK_SIZE);
	}

	memcpy(scrollback_cursor, p, l);
	scrollback_cursor += l;
	*scrollback_cursor = 0;

	status.flags |= NEED_UPDATE;
}

static inline void scrollback_putc(const char c)
{
	scrollback_write(&c, 1);
}

static int tail_scrollback(int print_lines)
{
	char *pprev, *p = scrollback_cursor-1;
	int n = 0;
	int c = 0;

	if (scrollback >= scrollback_cursor)
		return 0;

	for(p = pprev = scrollback_cursor-1; pprev > scrollback && n < print_lines; pprev--) {
		if (*pprev == '\n') {
			p = pprev+1;
			c = 0;
			n++;
		} else if (*pprev == '\t') {
			c += 4;
		} else {
			c++;
		}

		if (c > LINE_WRAP) {
			c = 0;
			n++;
		}
	}

	if (pprev == scrollback) {
		p = scrollback;
	}

	for (n = 0; n < print_lines && p < scrollback_cursor; n++) {
		for (c = 0; c < LINE_WRAP && p < scrollback_cursor; c++, p++) {
			if (*p == '\n') {
				// increment p cuz we break from the increment loop
				p++;
				break;
			} else if (*p == '\t') {
				c += 4;
				continue;
			}
			draw_char(*p, 2+c, 13+n, 6,0);
		}
	}

	return n;
}

static int scrollback_from_line(int n, int print_lines)
{
	return 0;
}

static void input_clear()
{
	input_cursor = input;
	*input_cursor = 0;

	status.flags |= NEED_UPDATE;
}

static void input_write(const char* p, int l)
{
	assert(l >= 0);

	if (l > INPUT_SIZE - input_len()) {
		l = INPUT_SIZE - input_len();
	}

	memcpy(input_cursor, p, l);
	input_cursor += l;
	*input_cursor = 0;

	status.flags |= NEED_UPDATE;
}

static inline void input_putc(const char c)
{
	input_write(&c, 1);
}

static int estimate_input()
{
	char *p;
	int n = 1;
	int c = 2;

	if (input >= input_cursor)
		return n;

	for (p = input; p < input_cursor && n < LINES; p++) {
		if (*p == '\n') {
			n++;
		}

		if (*p == '\t') {
			c += 4;
		} else {
			c++;
		}

		if (c > LINE_WRAP) {
			c = 3;
			n++;
		}
	}

	return n;
}

// dump input with pretty effects to buffer
// useful both when dumping input history to scorllback and when printing
// live input (at the expense of another ~INPUT_SIZE buf).
static int dump_input(char *p, int l)
{
	char *ip = input;
	int i;
	if (l < 2) {
		return 0;
	}

	*p++ = '>';
	*p++ = ' ';
	l -= 2;

	int newline = 0;
	for (i = 0; i < l - 1 && ip < input_cursor; i++, ip++) {
		p[i] = *ip;

		if (*ip == '\n') {
			if (i + 3 >= l - 1) {
				break;
			}
			p[i++] = '>';
			p[i++] = '>';
			p[i++] = ' ';
		}
	}

	if (i < l-1) {
		p[i] = 0;
	} else {
		p[i-1] = 0;
	}

	return i+2;
}

static void print_input(int start_line)
{
	assert(start_line < LINES);

	dump_input(input_pretty, sizeof(input_pretty));

	int n = start_line;

	for (char* p = input_pretty; n < LINES; n++) {
		for (int c = 0; c < LINE_WRAP && *p; c++, p++) {
			if (*p == '\n') {
				// increment p cuz we break from the increment loop
				p++;
				break;
			} else if (*p == '\t') {
				c += 4;
				continue;
			}
			draw_char(*p, 2+c, 13+n, 6,0);
		}
	}
}

static void lua_page_redraw(void)
{
	int input_lines = estimate_input();
	int input_start = 0;
	draw_fill_chars(2, 13, 77, 47, 0);

	if (input_lines < LINES) {
		if (tail_console) {
			input_start = tail_scrollback(LINES - input_lines);
		} else {
			input_start = scrollback_from_line(top_line, LINES - input_lines);
		}
	}

	if (input_start < LINES) {
		print_input(input_start);
	}
}

static void message_draw_const(void)
{
	draw_box(1, 12, 78, 48, BOX_THICK | BOX_INNER | BOX_INSET);
}

static int lua_page_handle_key(UNUSED struct key_event * k)
{
	switch (k->sym.sym) {
	case SDLK_BACKSPACE:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;

		if (input_cursor > input) {
			input_cursor--;
			*input_cursor = 0;
			status.flags |= NEED_UPDATE;
		}
		return 1;
	default:
		if (k->unicode == '\r') {
			if (k->state == KEY_RELEASE)
				return 1;
			scrollback_cursor += dump_input(scrollback_cursor, SCROLLBACK_SIZE - scrollback_len());
			scrollback_putc('\n');
			eval_lua_input(input);
			input_clear();
			return 1;
		} else if (k->unicode >= 32) {
			if (k->state == KEY_RELEASE)
				return 1;
			if (k->mod & (KMOD_SHIFT|KMOD_CAPS)) {
				input_putc((char)toupper((unsigned int)k->unicode));
			} else {
				input_putc((char)k->unicode);
			}
			return 1;
		}
		return 0;
	}

	return 0;
}

void lua_load_page(struct page *page)
{
	set_lua_print(&scrollback_write);
	eval_lua_input("print(\"hello from lua!\", 1+1)");

	page->title = "Lua Console";
	page->draw_const = message_draw_const;
	page->total_widgets = 1;
	page->widgets = widgets_lua;
	page->help_index = HELP_GLOBAL;

	create_other(widgets_lua + 0, 0, lua_page_handle_key, lua_page_redraw);
	widgets_lua[0].accept_text = 1;
}
