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
#include "widget.h"

/* --------------------------------------------------------------------- */

static struct widget widgets_blank[1];

/* --------------------------------------------------------------------- */

static int blank_page_handle_key(UNUSED struct key_event * k)
{
	return 0;
}

static void blank_page_redraw(void)
{
}

/* --------------------------------------------------------------------- */

void blank_load_page(struct page *page)
{
	page->title = "";
	page->total_widgets = 1;
	page->widgets = widgets_blank;
	page->help_index = HELP_GLOBAL;

	widget_create_other(widgets_blank + 0, 0, blank_page_handle_key, NULL, blank_page_redraw);
}
