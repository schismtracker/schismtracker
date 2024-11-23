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

/* It's lo-og, lo-og, it's big, it's heavy, it's wood!
 * It's lo-og, lo-og, it's better than bad, it's good! */

#include "headers.h"

#include "it.h"
#include "page.h"
#include "widget.h"
#include "vgamem.h"
#include "accessibility.h"
#include "charset.h"

#include "sdlmain.h"

#include <stdarg.h>
#include <errno.h>

struct log_line {
	int color;
	const char *text;
	int bios_font;
	/* Set this flag if the text should be free'd when it is scrolled offscreen.
	DON'T set it if the text is going to be modified after it is added to the log (e.g. for displaying
	status information for module loaders like IT); in that case, change the text pointer to some
	constant value such as "". Also don't try changing must_free after adding a line to the log, since
	there's a chance that the line scrolled offscreen, and it'd never get free'd. (Also, ignore this
	comment since there's currently no interface for manipulating individual lines in the log after
	adding them.) */
	int must_free;
};

/* --------------------------------------------------------------------- */

static struct widget widgets_log[1];

#define NUM_LINES 1000
static struct log_line lines[NUM_LINES];
static int top_line = 0;
static int current_char = 0; // The same thing for chars
static int last_line = -1;
static int current_line = -1; // Virtual accessibility cursor for lines

/* --------------------------------------------------------------------- */

static void log_draw_const(void)
{
	draw_box(1, 12, 78, 48, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(2, 13, 77, 47, DEFAULT_FG, 0);
}

static inline int _line_is_separator(int line)
{
	if (!lines[line].bios_font && (unsigned char)lines[line].text[0] == 0x81)
		return 1;
	return 0;
}

static const char* log_a11y_get_value(char *buf)
{
	if (!lines[current_line].text)
		buf[0] = '\0';
	else if (_line_is_separator(current_line))
		strcpy(buf, "Separator");
	else {
		strcpy(buf, lines[current_line].text);
		CHARSET_EASY_MODE(buf, CHARSET_CP437, CHARSET_CHAR, {
			strcpy(buf, out);
		});
	}
	return buf;
}

static char log_a11y_get_char_at(int pos)
{
	const char *line = lines[current_line].text;
	if (_line_is_separator(current_line))
		return '\0';
	return line[pos];
}

static int log_handle_key(struct key_event * k)
{
	int new_cur_line = current_line;
	char buf[75];
	char ch;
	switch (k->sym) {
	case SDLK_UP:
		if (k->state == KEY_RELEASE)
			return 1;
		top_line--;
		new_cur_line--;
		break;
	case SDLK_PAGEUP:
		if (k->state == KEY_RELEASE)
			return 1;
		top_line -= 15;
		new_cur_line -= 15;
		break;
	case SDLK_DOWN:
		if (k->state == KEY_RELEASE)
			return 1;
		top_line++;
		new_cur_line++;
		break;
	case SDLK_PAGEDOWN:
		if (k->state == KEY_RELEASE)
			return 1;
		top_line += 15;
		new_cur_line += 15;
		break;
	case SDLK_HOME:
		if (k->state == KEY_RELEASE)
			return 1;
		top_line = 0;
		new_cur_line = 0;
		break;
	case SDLK_END:
		if (k->state == KEY_RELEASE)
			return 1;
		top_line = last_line;
		new_cur_line = last_line;
		break;
	case SDLK_LEFT:
		if (k->state == KEY_RELEASE || !lines[current_line].text)
			return 1;
		current_char--;
		if (current_char < 0) current_char = 0;
		ch = log_a11y_get_char_at(current_char);
		if (ch) a11y_output_char(ch, 0);
		return 1;
	case SDLK_RIGHT:
		if (k->state == KEY_RELEASE || !lines[current_line].text)
			return 1;
		current_char++;
		int last_char = strlen(lines[current_line].text) - 1;
		if (current_char > last_char) current_char = last_char;
		ch = log_a11y_get_char_at(current_char);
		if (ch) a11y_output_char(ch, 0);
		return 1;
	default:
		if (k->state == KEY_PRESS) {
			if (k->mouse == MOUSE_SCROLL_UP) {
				top_line -= MOUSE_SCROLL_LINES;
				new_cur_line -= MOUSE_SCROLL_LINES;
				break;
			} else if (k->mouse == MOUSE_SCROLL_DOWN) {
				top_line += MOUSE_SCROLL_LINES;
				new_cur_line += MOUSE_SCROLL_LINES;
				break;
			}
		}

		return 0;
	};
	top_line = CLAMP(top_line, 0, (last_line-32));
	if (top_line < 0) top_line = 0;
	if (new_cur_line < 0)
		new_cur_line = 0;
	else if (new_cur_line > last_line)
		new_cur_line = last_line;
	if(new_cur_line != current_line) {
		current_line = new_cur_line;
		current_char = 0;
	}
	log_a11y_get_value(buf);
	a11y_output(*buf ? buf : "Blank", 0);
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
	page->help_index = HELP_COPYRIGHT; /* I guess */

	widget_create_other(widgets_log + 0, 0, log_handle_key, NULL, log_redraw);
	widgets_log[0].d.other.a11y_type = "";
	widgets_log[0].d.other.a11y_get_value = log_a11y_get_value;
}

/* --------------------------------------------------------------------- */

void log_append2(int bios_font, int color, int must_free, const char *text)
{
	if (last_line < NUM_LINES - 1) {
		last_line++;
		current_line++;
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
void log_append(int color, int must_free, const char *text)
{
	log_append2(0, color, must_free, text);
}
void log_nl(void)
{
	log_append(DEFAULT_FG,0,"");
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

void log_underline(int chars)
{
	char buf[75];

	chars = CLAMP(chars, 0, (int) sizeof(buf) - 1);
	buf[chars--] = '\0';
	do
		buf[chars] = 0x81;
	while (chars--);
	log_appendf(2, "%s", buf);
}

void log_perror(const char *prefix)
{
	char *e = strerror(errno);
	perror(prefix);
	log_appendf(4, "%s: %s", prefix, e);
}

