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
#include "vgamem.h"
#include "widget.h"
#include "charset.h"
#include "accessibility.h"

#include <stdarg.h>

static char* a11y_message = NULL;

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
		CHARSET_EASY_MODE_CONST(w->d.togglebutton.text, CHARSET_CP437, CHARSET_CHAR, {
			strcpy(buf, out);
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
		// IS this the only case where something non-ASCII is possible? It seems so.
		CHARSET_EASY_MODE_CONST(value, CHARSET_CP437, CHARSET_CHAR, {
			strcpy(buf, out);
		});
		break;
	case WIDGET_NUMENTRY:
		if (w->d.numentry.reverse) {
			unsigned char *str = str_from_num(w->width, w->d.numentry.value, buf);
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

// Behold the truly terrible heuristic mess!
const char* a11y_try_find_widget_label(struct widget *w, char* buf)
{
	uint32_t *label = NULL;
	int found = 0;
	int alternative_method = 0;
	int i, j;
	buf[0] = '\0';
	if (w->x <= 0 || w->x >= 80 || w->y <= 0 || w->y >= 50)
		return buf;
	alternative: if (w->type == WIDGET_OTHER || alternative_method) {
		for (i = 1; i <= 2; i++) {
			for (j = 0; j < 6 && j < 80 - w->x; j++) {
				label = acbuf_get_ptr_to(w->x + j, w->y - i);
				if (label && *label) {
					found = 1;
					break;
				}
			}
			if (found) break;
		}
		if (!found)
			return buf;
		CHARSET_EASY_MODE_CONST((uint8_t*)label, CHARSET_UCS4, CHARSET_CHAR, {
			strcpy(buf, out);
		});
		return buf;
	}
	uint32_t *start = acbuf_get_ptr_to(0, w->y);
	uint32_t *wx = &start[w->x];
	label = wx - 2;
	if (label <= start || !*label) {
		alternative_method = 1;
		goto alternative;
	}
	for ( ; label >= start; label--) {
		if (label && *label)
			continue;
		label++;
	found = 1;
		break;
	}
	if (found) {
		CHARSET_EASY_MODE_CONST((uint8_t*)label, CHARSET_UCS4, CHARSET_CHAR, {
			strcpy(buf, out);
		});
	} else {
		alternative_method = 1;
		goto alternative;
	}
	return buf;
}

static int _find_first_non_null_index(const uint32_t *arr, size_t len)
{
	for (int i = 0; i < len; i++) {
		if (arr[i] != 0)
			return i;
	}
	return -1;
}

static uint32_t* _replace_nulls_and_terminate(uint32_t *str, size_t len)
{
	for (uint32_t i = 0; i < len; i++) {
		if (str[i] == 0)
			str[i] = 32;
	}
	str[len] = 0;
	return str;
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
		start = _find_first_non_null_index(str, w);
		if (start == -1)
			continue;
		_replace_nulls_and_terminate(&str[start], w - start);
		CHARSET_EASY_MODE_CONST((uint8_t*)&str[start], CHARSET_UCS4, CHARSET_CHAR, {
			strcat(buf, out);
		});
		if (strlen(buf)) strcat(buf, " ");
	}
	return buf;
}

int a11y_init(void)
{
	return SRAL_Initialize(ENGINE_NONE);
}

int a11y_output(const char* text, int interrupt)
{
	if(!strlen(text)) return 0;
	return SRAL_Output(text, interrupt);
}

int a11y_output_cp437(const char* text, int interrupt)
{
	int result = 0;
	if(!strlen(text)) return 0;
	CHARSET_EASY_MODE_CONST(text, CHARSET_CP437, CHARSET_CHAR, {
		result = SRAL_Output(out, interrupt);
	});
	return result;
}

int a11y_output_char(char chr, int interrupt)
{
	unsigned char text[2] = { chr, '\0' };
	if (!*text) return 0;
	return a11y_output_cp437(text, interrupt);
}

int a11y_outputf(const char *format, int interrupt, ...)
{
	int result;

	va_list ap;

	va_start(ap, interrupt);

	if (vasprintf(&a11y_message, format, ap) == -1) abort();

	result = a11y_output(a11y_message, interrupt);

	va_end(ap);
	if (a11y_message)
		free(a11y_message);
	return result;
}

void a11y_interrupt(void)
{
	SRAL_StopSpeech();
}

void a11y_uninit(void)
{
	SRAL_Uninitialize();
}

void a11y_delay(int delay_ms)
{
	SRAL_Delay(delay_ms);
}
