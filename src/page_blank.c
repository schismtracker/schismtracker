/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <someguy@here.is> <http://here.is/someguy/>
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

#include <SDL.h>

/* --------------------------------------------------------------------- */

static struct item items_blank[1];

/* --------------------------------------------------------------------- */

static int blank_page_handle_key(UNUSED SDL_keysym * k)
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
        page->total_items = 1;
        page->items = items_blank;
        page->help_index = HELP_GLOBAL;

	create_other(items_blank + 0, 0, blank_page_handle_key, blank_page_redraw);
}
