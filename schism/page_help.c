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

/* Well, this page is just a big hack factory, but it's at least an
 * improvement over the message editor :P */

#include "headers.h"

#include "it.h"
#include "page.h"
#include "widget.h"
#include "vgamem.h"
#include "keyboard.h"
#include "mem.h"
#include "str.h"

/* --------------------------------------------------------------------- */

/* Line type characters (the marker at the start of each line) */
enum {
	LTYPE_NORMAL = '|',
	LTYPE_BIOS = '+',
	LTYPE_SCHISM = ':',
	LTYPE_SCHISM_BIOS = ';',
	LTYPE_CLASSIC = '!',
	LTYPE_SEPARATOR = '%',
	LTYPE_DISABLED = '#',
	LTYPE_GRAPHIC = '=',
};

/* Types that should be hidden from view in classic/non-classic mode */
#define LINE_SCHISM_HIDDEN(p) (0[p] == LTYPE_CLASSIC)
#define LINE_CLASSIC_HIDDEN(p) (0[p] == LTYPE_SCHISM || 0[p] == LTYPE_SCHISM_BIOS)

/* Types that should be rendered with the standard font */
#define LINE_BIOS(p) (0[p] == LTYPE_BIOS || 0[p] == LTYPE_SCHISM_BIOS)

static struct widget widgets_help[2];

/*
Pointers to the start of each line, and total line counts,
for each help text, for both classic and "normal" mode.

For example,
	help_cache[HELP_PATTERN_EDITOR][0].lines[3][5]
is the fifth character of the third line of the non-classic-mode help for the
pattern editor.

Each line is terminated by some combination of \r and \n, or \0.
*/
static struct {
	const char **lines;
	int num_lines;
} help_cache[HELP_NUM_ITEMS][2] = {{{NULL, 0}}};

/* Shortcuts for sanity -- this will point to the currently applicable help. */
#define CURRENT_HELP_LINECACHE (help_cache[status.current_help_index][!!(status.flags & CLASSIC_MODE)].lines)
#define CURRENT_HELP_LINECOUNT (help_cache[status.current_help_index][!!(status.flags & CLASSIC_MODE)].num_lines)

/* should always point to the currently applicable help text -- cached
to prevent repetitively checking things that aren't going to change */
static const char **lines = NULL;

static int num_lines = 0;
static int top_line = 0;

static const char blank_line[] = {LTYPE_NORMAL, '\0'};
static const char separator_line[] = {LTYPE_SEPARATOR, '\0'};

static int help_text_lastpos[HELP_NUM_ITEMS] = {0};

/* Incremental search ('/', 'n', 'N'/'?'), modelled on less(1). '/' drops into
search-entry mode: help_searching is set, keystrokes are routed into help_search
(see help_handle_key), and the query is drawn on the bottom line of the viewport
(see help_redraw) -- no dialog, no separate widget. Enter runs the search, Esc
cancels. The query survives n/N repeats and is cleared on page switch. A match
scrolls to the top row, so top_line is itself the search cursor: n/N advance from
top_line +/- 1. */
static char help_search[64] = "";
static int help_searching = 0;   /* in search-entry mode (typing the query) */

/* the viewport's bottom row, where the search prompt is drawn while typing */
#define HELP_PROMPT_ROW 44

/* This isn't defined in an .h file since it's only used here. */
extern const char *help_text[];

/* --------------------------------------------------------------------- */

static void help_draw_const(void)
{
	draw_box(1, 12, 78, 45, BOX_THICK | BOX_INNER | BOX_INSET);

	if (status.dialog_type == DIALOG_NONE) widget_change_focus_to(1);
}

static void help_redraw(void)
{
	int n, pos, x;
	int lp;
	const uint8_t graphic_chars[] = {0, 0x89, 0x8f, 0x96, 0x84, 0, 0x91, 0x8b, 0x86, 0x8a};
	char ch;

	draw_fill_chars(2, 13, 77, 44, DEFAULT_FG, 0);

	for (pos = 13, n = top_line; pos < 45; pos++, n++) {
		const char *line;

		/* A search pins the matched line to the top row, so top_line can
		 * sit within the last 32 lines; rows past the end render blank. */
		if (n < 0 || n >= num_lines)
			continue;
		line = lines[n];
		if (!line)
			continue;

		switch (line[0]) {
		default: {
			const char *text = line + 1;
			int bios = LINE_BIOS(line);
			int fg = (line[0] == LTYPE_DISABLED) ? 7 : 6;
			const char *m = help_search[0] ? strcasestr(text, help_search) : NULL;
			int off = m ? (int)(m - text) : -1;

			lp = strcspn(text, "\015\012");
			if (m && off < lp) {
				/* split into before / matched / after; the match is
				 * drawn inverted (black on cyan) as a highlight. */
				int mlen = (int)strlen(help_search);
				if (off + mlen > lp)
					mlen = lp - off;
				if (bios) {
					draw_text_bios_len(text, off, 2, pos, fg, 0);
					draw_text_bios_len(text + off, mlen, 2 + off, pos, 0, 3);
					draw_text_bios_len(text + off + mlen, lp - off - mlen, 2 + off + mlen, pos, fg, 0);
				} else {
					draw_text_len(text, off, 2, pos, fg, 0);
					draw_text_len(text + off, mlen, 2 + off, pos, 0, 3);
					draw_text_len(text + off + mlen, lp - off - mlen, 2 + off + mlen, pos, fg, 0);
				}
			} else if (bios) {
				draw_text_bios_len(text, lp, 2, pos, fg, 0);
			} else {
				draw_text_len(text, lp, 2, pos, fg, 0);
			}
			break;
		}
		case LTYPE_GRAPHIC:
			lp = strcspn(line + 1, "\015\012");
			for (x = 1; x <= lp; x++) {
				ch = line[x];
				if (ch >= '1' && ch <= '9')
					ch = graphic_chars[ch - '0'];
				draw_char(ch, x + 1, pos, 6, 0);
			}
			break;
		case LTYPE_SEPARATOR:
			for (x = 2; x < 78; x++)
				draw_char(154, x, pos, 6, 0);
			break;
		}
	}

	/* less(1)-style search prompt: overlay the bottom row of the viewport
	 * with "/<query>" and a cursor block while typing. */
	if (help_searching) {
		int col;
		draw_fill_chars(2, HELP_PROMPT_ROW, 77, HELP_PROMPT_ROW, DEFAULT_FG, 0);
		draw_char('/', 2, HELP_PROMPT_ROW, 6, 0);   /* color 6 = normal help text */
		col = 3 + draw_text_len(help_search, 73, 3, HELP_PROMPT_ROW, 6, 0);
		draw_char(' ', col, HELP_PROMPT_ROW, 0, 6);   /* cursor (inverted) */
	}
}

/* --------------------------------------------------------------------- */

static void _help_close(void)
{
	set_page(status.previous_page);
}

/* --------------------------------------------------------------------- */
/* incremental search */

/* Return the index of the next line containing `query` (case-insensitive),
 * scanning from `start` in `dir` (+1 forward, -1 backward) and wrapping once.
 * Separator/graphic lines carry no searchable prose and are skipped.
 * Returns -1 if nothing matches. */
static int help_find(const char *query, int start, int dir)
{
	int i, n;

	if (!query || !query[0] || num_lines <= 0)
		return -1;

	for (i = 0, n = start; i < num_lines; i++, n += dir) {
		const char *line;
		char visible[128];
		int len;

		/* wrap into [0, num_lines) */
		if (n < 0)
			n += num_lines;
		else if (n >= num_lines)
			n -= num_lines;

		line = lines[n];
		if (!line)
			continue;
		if (line[0] == LTYPE_SEPARATOR || line[0] == LTYPE_GRAPHIC)
			continue;

		/* line[0] is the type marker; visible text starts at line + 1 and
		 * runs until \r/\n/\0. The lines are NOT NUL-terminated at the line
		 * boundary -- they point into the help_text[] blob -- so we must copy
		 * out just the visible span before searching, otherwise strcasestr
		 * scans on into later lines and matches the whole rest of the page. */
		len = (int)strcspn(line + 1, "\015\012");
		if (len > (int)sizeof(visible) - 1)
			len = sizeof(visible) - 1;
		memcpy(visible, line + 1, len);
		visible[len] = '\0';

		if (strcasestr(visible, query))
			return n;
	}
	return -1;
}

/* dir: +1 forward, -1 backward. `from` is the line to start scanning from.
 * On a hit, scroll the matched line to the top row (less(1) style). Unlike the
 * arrow-key tail this clamps to num_lines - 1, not num_lines - 32, so a match in
 * the last 32 lines still rises to the top; help_redraw() blanks the rows past
 * the end. */
static void help_do_search(int from, int dir)
{
	int match = help_find(help_search, from, dir);
	int new_line;

	if (match < 0) {
		status_text_flash("Search string not found: %s", help_search);
		return;
	}

	new_line = CLAMP(match, 0, num_lines - 1);
	if (new_line != top_line) {
		top_line = new_line;
		help_text_lastpos[status.current_help_index] = top_line;
		status.flags |= NEED_UPDATE;
	}
}

/* --------------------------------------------------------------------- */

/* While in search-entry mode, keystrokes build up the query (less(1) style):
 * printable text is appended, Backspace deletes, Enter runs the search, Esc
 * cancels. Returns 1 (the key is always consumed in this mode). */
static int help_search_key(struct key_event *k)
{
	int len;

	if (k->state == KEY_RELEASE)
		return 1;

	switch (k->sym) {
	case SCHISM_KEYSYM_ESCAPE:
		help_searching = 0;
		status.flags |= NEED_UPDATE;
		return 1;
	case SCHISM_KEYSYM_RETURN:
		help_searching = 0;
		if (help_search[0])
			help_do_search(top_line, +1);   /* forward from current top */
		status.flags |= NEED_UPDATE;
		return 1;
	case SCHISM_KEYSYM_BACKSPACE:
		len = strlen(help_search);
		if (len)
			help_search[len - 1] = '\0';
		status.flags |= NEED_UPDATE;
		return 1;
	default:
		/* k->text holds the typed character(s) (ASCII for help text); the
		 * '/' that opened search was consumed by its own case, so it never
		 * lands here. */
		if (k->text && k->text[0]) {
			len = strlen(help_search);
			strncat(help_search, k->text, sizeof(help_search) - 1 - len);
			status.flags |= NEED_UPDATE;
		}
		return 1;
	}
}

/* --------------------------------------------------------------------- */

static int help_handle_key(struct key_event * k)
{
	int new_line = top_line;

	if (help_searching)
		return help_search_key(k);

	if (status.dialog_type != DIALOG_NONE) return 0;

	if (k->mouse == MOUSE_SCROLL_UP) {
		new_line -= MOUSE_SCROLL_LINES;
	} else if (k->mouse == MOUSE_SCROLL_DOWN) {
		new_line += MOUSE_SCROLL_LINES;

	} else if (k->mouse != MOUSE_NONE) {
		return 0;
	}
	switch (k->sym) {
	case SCHISM_KEYSYM_ESCAPE:
		if (k->state == KEY_RELEASE)
			return 1;
		if (help_search[0]) {
			/* an active search (query set, highlight shown): clear it
			 * first; only a second Esc leaves the help screen. */
			help_search[0] = '\0';
			status.flags |= NEED_UPDATE;
			return 1;
		}
		set_page(status.previous_page);
		return 1;
	case SCHISM_KEYSYM_UP:
		if (k->state == KEY_RELEASE)
			return 1;
		new_line--;
		break;
	case SCHISM_KEYSYM_DOWN:
		if (k->state == KEY_RELEASE)
			return 1;
		new_line++;
		break;
	case SCHISM_KEYSYM_PAGEUP:
		if (k->state == KEY_RELEASE)
			return 1;
		new_line -= 32;
		break;
	case SCHISM_KEYSYM_PAGEDOWN:
		if (k->state == KEY_RELEASE)
			return 1;
		new_line += 32;
		break;
	case SCHISM_KEYSYM_HOME:
		if (k->state == KEY_RELEASE)
			return 1;
		new_line = 0;
		break;
	case SCHISM_KEYSYM_END:
		if (k->state == KEY_RELEASE)
			return 1;
		new_line = num_lines - 32;
		break;
	case SCHISM_KEYSYM_SLASH:
		if (k->state == KEY_RELEASE)
			return 1;
		help_searching = 1;
		help_search[0] = '\0';
		status.flags |= NEED_UPDATE;
		return 1;
	case SCHISM_KEYSYM_n:
		if (k->state == KEY_RELEASE)
			return 1;
		if (!help_search[0])
			return 1;   /* no query yet: no-op, no flash */
		if (k->mod & SCHISM_KEYMOD_SHIFT)
			help_do_search(top_line - 1, -1);   /* shift+n = backward */
		else
			help_do_search(top_line + 1, +1);   /* n = forward */
		return 1;
	case SCHISM_KEYSYM_QUESTION:
		if (k->state == KEY_RELEASE)
			return 1;
		if (!help_search[0])
			return 1;
		help_do_search(top_line - 1, -1);   /* ? = backward */
		return 1;
	default:
		if (k->mouse != MOUSE_NONE) {
			if (k->state == KEY_RELEASE)
				return 1;
		} else {
			return 0;
		}
	}

	new_line = CLAMP(new_line, 0, num_lines - 32);
	if (new_line != top_line) {
		top_line = new_line;
		help_text_lastpos[status.current_help_index] = top_line;
		status.flags |= NEED_UPDATE;
	}

	return 1;
}

/* --------------------------------------------------------------------- */

static void help_set_page(void)
{
	const char *ptr;
	int local_lines = 0, global_lines = 0, cur_line = 0;
	int have_local_help = (status.current_help_index != HELP_GLOBAL);

	widget_change_focus_to(1);
	top_line = help_text_lastpos[status.current_help_index];
	help_search[0] = '\0';   /* a stale query must not repeat/highlight on a new page */
	help_searching = 0;

	lines = CURRENT_HELP_LINECACHE;
	if (lines) {
		num_lines = CURRENT_HELP_LINECOUNT;
		return;
	}

	/* how many lines? */
	global_lines = str_get_num_lines(help_text[HELP_GLOBAL]);
	if (have_local_help) {
		local_lines = str_get_num_lines(help_text[status.current_help_index]);
		num_lines = local_lines + global_lines + 5;
	} else {
		num_lines = global_lines + 2;
	}

	/* allocate the array */
	lines = CURRENT_HELP_LINECACHE = mem_calloc(num_lines + 1, sizeof(char *));

	/* page help text */
	if (have_local_help) {
		ptr = help_text[status.current_help_index];
		while (local_lines--) {
			if (status.flags & CLASSIC_MODE) {
				if (!LINE_CLASSIC_HIDDEN(ptr))
					lines[cur_line++] = ptr;
			} else {
				if (!LINE_SCHISM_HIDDEN(ptr))
					lines[cur_line++] = ptr;
			}
			ptr = strpbrk(ptr, "\015\012");
			if (*ptr == 13)
				ptr++;
			if (*ptr == 10)
				ptr++;
		}
		lines[cur_line++] = blank_line;
		lines[cur_line++] = separator_line;
	}
	lines[cur_line++] = blank_line;

	/* global help text */
	ptr = help_text[HELP_GLOBAL];
	while (global_lines--) {
		if (status.flags & CLASSIC_MODE) {
			if (!LINE_CLASSIC_HIDDEN(ptr))
				lines[cur_line++] = ptr;
		} else {
			if (!LINE_SCHISM_HIDDEN(ptr))
				lines[cur_line++] = ptr;
		}
		ptr = strpbrk(ptr, "\015\012");
		if (*ptr == 13)
			ptr++;
		if (*ptr == 10)
			ptr++;
	}

	lines[cur_line++] = blank_line;
	if (have_local_help)
		lines[cur_line++] = separator_line;

	lines[cur_line] = NULL;
	CURRENT_HELP_LINECOUNT = num_lines = cur_line;
}

/* --------------------------------------------------------------------- */

void help_load_page(struct page *page)
{
	page->title = "Help";
	page->draw_const = help_draw_const;
	page->set_page = help_set_page;
	page->total_widgets = 2;
	page->widgets = widgets_help;
	page->pre_handle_key = help_handle_key;

	widget_create_other(widgets_help + 0, 0, help_handle_key, NULL, help_redraw);
	widget_create_button(widgets_help + 1, 35,47,8, 0, 1, 1,1, 0,
			_help_close, "Done", 3);
}

