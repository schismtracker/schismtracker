#include "headers.h"

#include <SDL.h>

#include "it.h"
#include "page.h"

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

        items_blank[0].type = ITEM_OTHER;
        items_blank[0].other.handle_key = blank_page_handle_key;
        items_blank[0].other.redraw = blank_page_redraw;
}
