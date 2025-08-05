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

#ifndef SCHISM_WIDGET_CONTEXT_H_
#define SCHISM_WIDGET_CONTEXT_H_

#include "headers.h"

struct widget;
struct key_event;

enum widget_context_type {
	WIDGET_CONTEXT_INVALID,

	WIDGET_CONTEXT_PAGE,
	WIDGET_CONTEXT_DIALOG,
};

struct widget_context {
	/* base class for struct page and struct dialog */
	enum widget_context_type context_type;
	struct widget *widgets;
	int selected_widget;
	int total_widgets;
};

typedef uint32_t (*widget_cb_uint32)(struct widget_context *this);
typedef int (*widget_uint32_cb_int)(struct widget_context *this, uint32_t);
typedef const char * (*widget_uint32_cb_str)(struct widget_context *this, uint32_t);
typedef void (*widget_cb)(struct widget_context *this);
typedef int (*widget_key_cb)(struct widget_context *this, struct key_event *kk);
typedef int (*widget_text_cb)(struct widget_context *this, const char *text);

void widget_set_context(struct widget_context *context);
void widget_set_context_use_active_page(struct widget_context *context);
struct widget_context *widget_get_context(struct widget *widget);

#endif /* SCHISM_WIDGET_CONTEXT_H_ */
