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

#include "headers.h" /* always include this one first, kthx */
#include "it.h"
#include "page.h"
#include "song.h"
#include "vgamem.h"
#include "widget.h"
#include "charset.h"
#include "accessibility.h"

#include <stdarg.h>
#include <stdbool.h>
#include <SRAL.h>


static char* message = NULL;
static int current_line = 0;
static int current_char = 0;

const char* a11y_get_widget_info(struct widget *w, enum a11y_info info, char *buf)
{
	size_t len = 0;
	buf[0] = '\0';
	if (info & INFO_LABEL) {
		a11y_get_widget_label(w, buf);
		if (strlen(buf)) strcat(buf, " ");
	}
	if (info & INFO_TYPE) {
		strcat(buf, a11y_get_widget_type(w));
		if (strlen(buf)) strcat(buf, " ");
	}
	if (info & INFO_STATE) {
		strcat(buf, a11y_get_widget_state(w));
		if (strlen(buf)) strcat(buf, " ");
	}
	if (info & INFO_VALUE) {
		a11y_get_widget_value(w, &buf[strlen(buf)]);
	}
	return buf;
}

const char* a11y_get_widget_label(struct widget *w, char *buf)
{
	buf[0] = '\0';
	switch (w-> type) {
	case WIDGET_BUTTON:
		CHARSET_EASY_MODE_CONST(w->d.button.text, CHARSET_CP437, CHARSET_CHAR, {
			strcpy(buf, out);
		});
		break;
	case WIDGET_TOGGLEBUTTON:
		if (*selected_widget == w->d.togglebutton.group[0]) {
			a11y_try_find_widget_label(w, buf);
			if (strlen(buf)) strcat(buf, ": ");
		}
		CHARSET_EASY_MODE_CONST(w->d.togglebutton.text, CHARSET_CP437, CHARSET_CHAR, {
			strcat(buf, out);
		});
		break;
	case WIDGET_PANBAR:
		sprintf(buf, "Channel %d", w->d.panbar.channel);
		break;
	default:
		a11y_try_find_widget_label(w, buf);
		break;
	}
	return buf;
}

const char* a11y_get_widget_state(struct widget *w)
{
	switch (w->type) {
	case WIDGET_TOGGLE:
		return w->d.toggle.state ? "On" : "Off";
	case WIDGET_MENUTOGGLE:
		return w->d.menutoggle.choices[w->d.menutoggle.state];
	case WIDGET_TOGGLEBUTTON:
		return w->d.togglebutton.state ? "On" : "Off";
	default:
		return "";
	}
}

const char* a11y_get_widget_type(struct widget *w)
{
	switch (w->type) {
	case WIDGET_TOGGLE:
	case WIDGET_MENUTOGGLE:
		return "Toggle";
	case WIDGET_BUTTON:
		return "Button";
	case WIDGET_TOGGLEBUTTON:
		return "Toggle button";
	case WIDGET_TEXTENTRY:
		return "Edit";
	case WIDGET_NUMENTRY:
		return "Number edit";
	case WIDGET_BITSET:
		return "Bit set";
	case WIDGET_THUMBBAR:
		return "Thumb bar";
	case WIDGET_PANBAR:
		return "Pan bar";
	case WIDGET_OTHER:
		if (w->d.other.a11y_type)
			return w->d.other.a11y_type;
		// Fall through
	default:
		return "Unknown";
	}
}

const char* a11y_get_widget_value(struct widget *w, char *buf)
{
	const char *value;
	buf[0] = '\0';
	switch (w->type) {
	case WIDGET_TEXTENTRY:
		value = w->d.textentry.text;
		CHARSET_EASY_MODE_CONST(value, CHARSET_CP437, CHARSET_CHAR, {
			strcpy(buf, out);
		});
		break;
	case WIDGET_NUMENTRY:
		if (w->d.numentry.reverse) {
			char *str = str_from_num(w->width, w->d.numentry.value, buf);
			while (*str == '0') str++;
			if (*str) strcpy(buf, str);
			break;
		} else if (w->d.numentry.min < 0 || w->d.numentry.max < 0) {
			str_from_num_signed(w->width, w->d.numentry.value,
						buf);
		} else {
			str_from_num(w->width, w->d.numentry.value,
						buf);
		}
		break;
	case WIDGET_BITSET:
		int n = *w->d.bitset.cursor_pos;
		int set = !!(w->d.bitset.value & (1 << n));
		unsigned char label_c1   = set ? w->d.bitset.bits_on[n*2+0]
					  : w->d.bitset.bits_off[n*2+0];
		unsigned char label_c2   = set ? w->d.bitset.bits_on[n*2+1]
					  : w->d.bitset.bits_off[n*2+1];
		buf[0] = label_c1;
		buf[1] = label_c2;
		if (label_c2 != '\0')
			buf[2] = '\0';
		break;
	case WIDGET_THUMBBAR:
		if (w->d.thumbbar.text_at_min && w->d.thumbbar.min == w->d.thumbbar.value) {
			int len = strlen(w->d.thumbbar.text_at_min);
			snprintf(buf, len, "%s", w->d.thumbbar.text_at_min);
		} else if (w->d.thumbbar.text_at_max && w->d.thumbbar.max == w->d.thumbbar.value) {
			int len = strlen(w->d.thumbbar.text_at_max);
			snprintf(buf, len, "%s", w->d.thumbbar.text_at_max);
		} else if (w->d.thumbbar.min < 0 || w->d.thumbbar.max < 0) {
			str_from_num_signed(3, w->d.thumbbar.value, buf);
		} else {
			str_from_num(3, w->d.thumbbar.value, buf);
		}
		break;
	case WIDGET_PANBAR:
		if (w->d.panbar.muted) {
			sprintf(buf, "%s", "Muted");
		} else if (w->d.panbar.surround) {
			sprintf(buf, "%s", "Surround");
		} else {
			str_from_num(3, w->d.thumbbar.value, buf);
		};
		break;
	case WIDGET_OTHER:
		if (w->d.other.a11y_get_value)
			w->d.other.a11y_get_value(buf);
	default:
		break;
	}
	return buf;
}

static inline int find_first_non_null_index(const uint32_t *arr, size_t len)
{
	for (int i = 0; i < len; i++) {
		if (arr[i] != 0)
			return i;
	}
	return -1;
}

static inline uint32_t* replace_nulls_and_terminate(uint32_t *str, size_t len)
{
	for (uint32_t i = 0; i < len; i++) {
		if (str[i] == 0)
			str[i] = 32;
	}
	str[len] = 0;
	return str;
}

static inline int get_label_length(uint32_t *label, size_t len)
{
	for (int i = 0; i < len; i++) {
		if (!isprint(label[i]))
			return i;
	}
	return len;
}

// Behold the truly terrible heuristic mess!
const char* a11y_try_find_widget_label(struct widget *w, char* buf)
{
	uint32_t *label = NULL;
	uint32_t str[81] = { 0 };
	int found = 0;
	int alternative_method = 0;
	int len = 0;
	int i, j;
	buf[0] = '\0';
	if (w->x <= 0 || w->x >= 80 || w->y <= 0 || w->y >= 50)
		return buf;
	alternative: if (w->type == WIDGET_OTHER || alternative_method) {
		for (i = 1; i <= 2; i++) {
			for (j = 0; j < 6 && j < 80 - w->x; j++) {
				label = acbuf_get_ptr_to(w->x + j, w->y - i);
				if (label && isprint(*label)) {
					found = 1;
					break;
				}
			}
			if (found) break;
		}
		if (!found) return buf;
		len = get_label_length(label, 80);
		memcpy(str, label, len * sizeof(uint32_t));
		str[len] = 0;
		CHARSET_EASY_MODE_CONST((uint8_t*)str, CHARSET_UCS4, CHARSET_CHAR, {
			strcpy(buf, out);
		});
		return buf;
	}
	uint32_t *start = acbuf_get_ptr_to(0, w->y);
	uint32_t *wx = &start[w->x];
	label = wx - 2;
	if ((label <= start || !isprint(*label)) && w->type != WIDGET_TOGGLEBUTTON) {
		alternative_method = 1;
		goto alternative;
	}
	for ( ; label >= start; label--) {
		if (label && isprint(*label))
			continue;
		label++;
	found = 1;
		break;
	}
	if (found) {
		len = get_label_length(label, 80);
		memcpy(str, label, len * sizeof(uint32_t));
		str[len] = 0;
		CHARSET_EASY_MODE_CONST((uint8_t*)str, CHARSET_UCS4, CHARSET_CHAR, {
			strcpy(buf, out);
		});
	} else if (w->type != WIDGET_TOGGLEBUTTON) {
		alternative_method = 1;
		goto alternative;
	}
	return buf;
}

const char* a11y_get_text_from(int x, int y, char* buf)
{
	uint32_t *ptr = NULL;
	int start =	 -1;
	uint32_t str[81] = { 0 };
	buf[0] = '\0';
	if (x < 0 || x >= 80 || y < 0 || y >= 50)
		return buf;
	ptr = acbuf_get_ptr_to(x, y);
	CHARSET_EASY_MODE_CONST((uint8_t*)&ptr, CHARSET_UCS4, CHARSET_CHAR, {
		strcat(buf, out);
	});
	return buf;
}

const char* a11y_get_text_from_rect(int x, int y, int w, int h, char *buf)
{
	uint32_t *ptr = NULL;
	int start =	 -1;
	uint32_t str[81] = { 0 };
	buf[0] = '\0';
	if (x < 0 || x >= 80 || y < 0 || y >= 50 || w < 1 || h < 1 || (x + w) > 80 || (y + h) > 50)
		return buf;
	for (int i = 0; i < h; i++) {
		ptr = acbuf_get_ptr_to(x, y + i);
		memcpy(str, ptr, w * sizeof(uint32_t));
		start = find_first_non_null_index(str, w);
		if (start == -1)
			continue;
		replace_nulls_and_terminate(&str[start], w - start);
		CHARSET_EASY_MODE_CONST((uint8_t*)&str[start], CHARSET_UCS4, CHARSET_CHAR, {
			strcat(buf, out);
		});
	}
	return buf;
}

static void a11y_set_char_mode(int state)
{
	if (!(status.flags & ACCESSIBILITY_MODE)) return;
	int engine = SRAL_GetCurrentEngine();
	switch (engine) {
	case ENGINE_NVDA:
		SRAL_SetEngineParameter(engine, SYMBOL_LEVEL, state ? 1000 : 100); // Character or Some
		break;
	case ENGINE_SPEECH_DISPATCHER:
		SRAL_SetEngineParameter(engine, SYMBOL_LEVEL, state ? 10 : 2); // All or Some
		break;
	default:
		break; // Unsupported
	}
}

int a11y_init(void)
{
	if (!(status.flags & ACCESSIBILITY_MODE)) return 0;
	int result = SRAL_Initialize(ENGINE_NONE);
	if (!result) return 0;
	a11y_set_char_mode(0);
	return result;
}

int a11y_output(const char* text, int interrupt)
{
	if (!(status.flags & ACCESSIBILITY_MODE)) return 1;
	if (!text || !*text) return 0;
	return SRAL_Output(text, interrupt);
}

static int a11y_char_mode_output(const char *text, int interrupt)
{
	int result = 0;
	if (isupper(*text))
		return a11y_outputf("Cap %s", interrupt, text);
	a11y_set_char_mode(1);
	result = a11y_output(*text ? text : "Blank", interrupt);
	a11y_set_char_mode(0);
	return result;
}

int a11y_output_cp437(const char* text, int interrupt)
{
	if (!(status.flags & ACCESSIBILITY_MODE)) return 1;
	int result = 0;
	int len = strlen(text);

	CHARSET_EASY_MODE_CONST(text, CHARSET_CP437, CHARSET_CHAR, {
		if (len <= 1)
			a11y_char_mode_output(out, interrupt);
		else
			result = a11y_output(out, interrupt);
	});
	return result;
}

int a11y_output_char(char chr, int interrupt)
{
	if (!(status.flags & ACCESSIBILITY_MODE)) return 1;
	char text[2] = { chr, '\0' };
	int result = a11y_output_cp437(text, interrupt);
	return result;
}

int a11y_outputf(const char *format, int interrupt, ...)
{
	int result;

	if (!(status.flags & ACCESSIBILITY_MODE)) return 1;
	va_list ap;

	va_start(ap, interrupt);

	if (vasprintf(&message, format, ap) == -1) abort();

	result = a11y_output(message, interrupt);

	va_end(ap);
	if (message)
		free(message);
	return result;
}

void a11y_interrupt(void)
{
	if (!(status.flags & ACCESSIBILITY_MODE)) return;
	SRAL_StopSpeech();
}

void a11y_uninit(void)
{
	if (status.flags & ACCESSIBILITY_MODE)
		current_line = 0;
		current_char = 0;
		SRAL_Uninitialize();
}

void a11y_delay(int delay_ms)
{
	if (!(status.flags & ACCESSIBILITY_MODE)) return;
	SRAL_Delay(delay_ms);
}

/* Higher-level convenience functions */

int a11y_report_widget(struct widget *w)
{
	char buf[512];
	if (!(status.flags & ACCESSIBILITY_MODE)) return 1;
	current_line = w->y;
	current_char = w->x;
	a11y_get_widget_info(w, INFO_LABEL | INFO_TYPE | INFO_STATE, buf);
	a11y_output(buf, 0);
	a11y_get_widget_info(w, INFO_VALUE, buf);
	a11y_output(buf, 0);
	return 1;
}

int a11y_report_order(void)
{
	char buf[16];

	if (!(status.flags & ACCESSIBILITY_MODE)) return 1;
	strcpy(buf, "Order ");
	str_from_num(3, get_current_order(), &buf[6]);
	strcat(buf, "/");
	str_from_num(3, csf_last_order(current_song), &buf[10]);
	return a11y_output(buf, 0);
}

int a11y_report_pattern(void)
{
	char buf[16];

	if (!(status.flags & ACCESSIBILITY_MODE)) return 1;
	strcpy(buf, "Pattern ");
	str_from_num(3, get_current_pattern(), &buf[8]);
	strcat(buf, "/");
	str_from_num(3, csf_get_num_patterns(current_song) - 1, &buf[12]);
	return a11y_output(buf, 0);
}

int a11y_report_instrument(void)
{
	int ins_mode, n;
	char *name = NULL;
	char buf[40];

	if (!(status.flags & ACCESSIBILITY_MODE)) return 1;
	if (page_is_instrument_list(status.current_page)
	|| status.current_page == PAGE_SAMPLE_LIST
	|| status.current_page == PAGE_LOAD_SAMPLE
	|| status.current_page == PAGE_LIBRARY_SAMPLE
	|| (!(status.flags & CLASSIC_MODE)
		&& (status.current_page == PAGE_ORDERLIST_PANNING
			|| status.current_page == PAGE_ORDERLIST_VOLUMES)))
		ins_mode = 0;
	else
		ins_mode = song_is_instrument_mode();

	if (ins_mode) {
		n = instrument_get_current();
		if (n > 0) {
			strcpy(buf, "Instrument ");
			name = song_get_instrument(n)->name;
		} else strcpy(buf, "(No instrument)");
	} else {
		n = sample_get_current();
		if (n > 0) {
			strcpy(buf, "Sample ");
			name = song_get_sample(n)->name;
		} else strcpy(buf, "(No sample)");
	}

	if (n > 0) {
		str_from_num99(n, &buf[strlen(buf)]);
		sprintf(&buf[strlen(buf)], ": %s", name);
	}
	return a11y_output_cp437(buf, 0);
}

/* Oh! I really wanted to avoid making my own crappy screen reader . Sorry. */

int a11y_cursor_get_current_line(void)
{
	return current_line;
}

int a11y_cursor_get_current_char(void)
{
	return current_char;
}

int a11y_cursor_report_line(int line)
{
	char buf[256];
	if (!(status.flags & ACCESSIBILITY_MODE)) return 1;
	if (line < 0 || line >= 50)
		return 0;
	a11y_get_text_from_rect(0, line, 80, 1, buf);
	current_line = line;
	current_char = 0;
	return a11y_output(buf, 0);
}

int a11y_cursor_report_previous_line(void)
{
	if (!(status.flags & ACCESSIBILITY_MODE)) return 1;
	if (current_line > 0) current_line--;
	return a11y_cursor_report_line(current_line);
}

int a11y_cursor_report_next_line(void)
{
	if (!(status.flags & ACCESSIBILITY_MODE)) return 1;
	if (current_line < 49) current_line++;
	return a11y_cursor_report_line(current_line);
}

int a11y_cursor_report_char(int ch)
{
	char buf[5];
	if (!(status.flags & ACCESSIBILITY_MODE)) return 1;
	a11y_get_text_from_rect(current_char, current_line, 1, 1, buf);
	return a11y_char_mode_output(*buf ? buf : " ", 0);
}

int a11y_cursor_report_previous_char(void)
{
	if (!(status.flags & ACCESSIBILITY_MODE)) return 1;
	if (current_char > 0) current_char--;
	return a11y_cursor_report_char(current_char);
}

int a11y_cursor_report_next_char(void)
{
	if (!(status.flags & ACCESSIBILITY_MODE)) return 1;
	if (current_char < 79) current_char++;
	return a11y_cursor_report_char(current_char);
}

void a11y_toggle_accessibility_mode(void)
{
	if (status.flags & ACCESSIBILITY_MODE) {
		status_text_flash("Accessibility mode disabled");
		status.flags &= ~ACCESSIBILITY_MODE;
		a11y_uninit();
	} else {
		status.flags |= ACCESSIBILITY_MODE;
		a11y_init();
		status_text_flash("Accessibility mode enabled");
	}
}

