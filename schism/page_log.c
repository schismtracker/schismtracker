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
#include "keyboard.h"
#include "charset.h"
#include "mem.h"
#include "str.h"
#include "config.h"

#define MAX_LINE_LENGTH 74

struct log_line {
	uint8_t color;
	const char *text;
	charset_t set;
	size_t underline_len; /* this is NOT the actual length of text */
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
static int last_line = -1;

/* --------------------------------------------------------------------- */

static void log_draw_const(void)
{
	draw_box(1, 12, 78, 48, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(2, 13, 77, 47, DEFAULT_FG, 0);
}

static int log_handle_key(struct key_event * k)
{
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
		top_line = last_line;
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
	};
	top_line = CLAMP(top_line, 0, (last_line-32));
	if (top_line < 0)
		top_line = 0;
	status.flags |= NEED_UPDATE;
	return 1;
}

static void log_redraw(void)
{
	int n, i;

	i = top_line;
	for (n = 0; i <= last_line && n < 33; n++, i++) {
		if (!lines[i].text)
			continue;

		draw_text_charset_len(lines[i].text, lines[i].set, MAX_LINE_LENGTH,
			3, 14 + n, lines[i].color, 0);
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
}

/* --------------------------------------------------------------------- */

void log_append3(charset_t set, int color, int must_free, const char *text)
{
	if (status.flags & STATUS_IS_HEADLESS) {
		// XXX: Maybe stdout should always get all of the log messages,
		// regardless of whether we're headless or not? Hm.
		puts(text);

		if (must_free)
			free((void *)text);
	} else {
		if (last_line < NUM_LINES - 1) {
			last_line++;
		} else {
			/* XXX: would probably be faster to just use a circular buffer */
			if (lines[0].must_free)
				free((void *) lines[0].text);

			memmove(lines, lines + 1, last_line * sizeof(struct log_line));
		}

		lines[last_line].text          = text;
		lines[last_line].set           = set;
		lines[last_line].color         = color;
		lines[last_line].must_free     = must_free;
		lines[last_line].underline_len = charset_strlen(text, set);

		top_line = CLAMP(last_line - 32, 0, NUM_LINES - 32);

		if (status.current_page == PAGE_LOG)
			status.flags |= NEED_UPDATE;
	}
}

void log_append2(int bios_font, int color, int must_free, const char *text)
{
	log_append3((bios_font) ? CHARSET_CP437 : CHARSET_ITF, color, must_free, text);
}

void log_append(int color, int must_free, const char *text)
{
	log_append2(0, color, must_free, text);
}

void log_nl(void)
{
	log_append(DEFAULT_FG, 0, "");
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

	log_append3(CHARSET_UTF8, color, 1, ptr);
}

static inline SCHISM_ALWAYS_INLINE
void log_underline_impl(int chars)
{
	char buf[MAX_LINE_LENGTH + 1];

	chars = CLAMP(chars, 0, (int) sizeof(buf) - 1);
	buf[chars--] = '\0';
	do {
		buf[chars] = 0x81;
	} while (chars--);

	log_append(2, 1, str_dup(buf));
}

void log_underline(void)
{
	log_underline_impl(lines[last_line].underline_len);
}

void log_perror(const char *prefix)
{
	char *e = strerror(errno);
	perror(prefix);
	log_appendf(4, "%s: %s", prefix, e);
}

/* ------------------------------------------------------------------------ */
/* timestamps */

/* takes in UTF-8 data always */
void log_append_timestamp(int color, const char *text)
{
	char datestr[27], timestr[27];
	time_t thetime;
	struct tm tm;
	size_t s, ds, ts/*pmo*/, i;
	charset_decode_t dec;
	uint32_t ucs4text[MAX_LINE_LENGTH + 1];

	time(&thetime);
	localtime_r(&thetime, &tm);

	/* output date/time in user preferred format */
	str_date_from_tm(&tm, datestr, cfg_str_date_format);
	str_time_from_tm(&tm, timestr, cfg_str_time_format);

	ds = strlen(datestr);
	ts = strlen(timestr);

	/* decode directly into our buffer, getting the on-screen length of the
	 * text at the same time */
	memset(&dec, 0, sizeof(dec));

	dec.in = text;
	dec.offset = 0;
	dec.size = SIZE_MAX;

	for (s = 0; dec.state == DECODER_STATE_NEED_MORE && !charset_decode_next(&dec, CHARSET_UTF8); s++) {
		/* break out if the codepoint is a NUL terminator */
		if (dec.codepoint == 0)
			break;

		if (ds+ts+4+s >= MAX_LINE_LENGTH) {
			/* can't fit the timestamp; give up */
			log_append3(CHARSET_UTF8, color, 1, str_dup(text));
			return;
		}

		ucs4text[s] = dec.codepoint;
	}

	/* fill the rest of the buffer with spaces */
	for (i = s; i < MAX_LINE_LENGTH; i++)
		ucs4text[i] = ' ';

	/* this is somewhat spaghetti-fied, sorry */
	/* ucs4text[MAX_LINE_LENGTH - 1 - ts - 1 - ds - 2] = ' '; --- already done */
	ucs4text[MAX_LINE_LENGTH - 1 - ts - 1 - ds - 1] = '[';

	/* date output should always be in English ASCII,
	 * so we can just convert it in-place */
	for (i = 0; i < ds; i++)
		ucs4text[MAX_LINE_LENGTH - 1 - ts - 1 - ds + i] = datestr[i];

	/* ucs4text[MAX_LINE_LENGTH - 1 - ts - 1] = ' '; --- already done */

	/* same for time output */
	for (i = 0; i < ts; i++)
		ucs4text[MAX_LINE_LENGTH - 1 - ts + i] = timestr[i];

	ucs4text[MAX_LINE_LENGTH - 1] = ']';

	ucs4text[MAX_LINE_LENGTH] = '\0';

	{
		/* copy buffer to the heap */
		uint32_t *x = mem_alloc((MAX_LINE_LENGTH + 1) * 4);
		memcpy(x, ucs4text, sizeof(ucs4text));
		/* bad hack */
		log_append3(CHARSET_UCS4, color, 1, (const char *)x);
	}

	/* fix the underline length */
	lines[last_line].underline_len = s;
}

void log_appendf_timestamp(int color, const char *format, ...)
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

	log_append_timestamp(color, ptr);

	free(ptr);
}
