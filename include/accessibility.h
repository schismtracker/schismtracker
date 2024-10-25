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

#ifndef SCHISM_ACCESSIBILITY_H_
#define SCHISM_ACCESSIBILITY_H_

#include <stdbool.h>
#include "SRAL.h"

#define MESSAGE_DELAY 3000

enum a11y_info {
	INFO_LABEL = 1,
	INFO_TYPE = 2,
	INFO_STATE = 4,
	INFO_VALUE = 8,
	INFO_ALL = 15
};

const char* a11y_get_widget_info(struct widget *w, enum a11y_info info, char *buf);
const char* a11y_get_widget_label(struct widget *w, char *buf);
const char* a11y_get_widget_type(struct widget *w);
const char* a11y_get_widget_state(struct widget *w);
const char* a11y_get_widget_value(struct widget *w, char *buf);
const char* a11y_try_find_widget_label(struct widget *w, char* buf);
const char* a11y_get_text_from(int x, int y, char* buf);
const char* a11y_get_text_from_rect(int x, int y, int w, int h, char *buf);
int a11y_init(void);
int a11y_output(const char* text, int interrupt);
int a11y_output_cp437(const char* text, int interrupt);
int a11y_output_char(char chr, int interrupt);
int a11y_outputf(const char *format, int interrupt, ...);
void a11y_interrupt(void);
void a11y_uninit(void);
void a11y_delay(int delay_ms);
int a11y_report_widget(struct widget *w);
int a11y_report_order(void);
int a11y_report_pattern(void);
int a11y_report_instrument(void);

/* Dumb screen reader mode stuff */

int a11y_cursor_get_current_line(void);
int a11y_cursor_get_current_char(void);
int a11y_cursor_report_line(int line);
int a11y_cursor_report_previous_line(void);
int a11y_cursor_report_next_line(void);
int a11y_cursor_report_char(int ch);
int a11y_cursor_report_previous_char(void);
int a11y_cursor_report_next_char(void);
void a11y_toggle_accessibility_mode(void);

#endif /* SCHISM_ACCESSIBILITY_H_ */
