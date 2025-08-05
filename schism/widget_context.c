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

#include "widget_context.h"
#include "keyboard.h"
#include "page.h"
#include "it.h"

static struct widget_context use_active_page;

/* --------------------------------------------------------------------- */
/* initialization functions that hold contexts (pages, dialogs) together */

void widget_set_context(struct widget_context *context)
{
	int i;

	for (i = 0; i < context->total_widgets; i++)
		context->widgets[i].this = context;
}

void widget_set_context_use_active_page(struct widget_context *context)
{
	int i;

	for (i = 0; i < context->total_widgets; i++)
		context->widgets[i].this = &use_active_page;
}

/* --------------------------------------------------------------------- */
/* accessor that handles the special flag used for widgets on pages */
/* consumers should generally not read ->this directly */

struct widget_context *widget_get_context(struct widget *widget)
{
	if (widget->this == &use_active_page)
		return (struct widget_context *)&ACTIVE_PAGE;
	else
		return widget->this;
}
