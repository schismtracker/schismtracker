#include "headers.h"

#include <SDL.h>

#include "it.h"
#include "page.h"

/* TODO: write this */

/* --------------------------------------------------------------------- */

static struct item items_midi[1];

/* --------------------------------------------------------------------- */

static int midi_page_handle_key(UNUSED SDL_keysym * k)
{
        return 0;
}

static void midi_page_redraw(void)
{
}

/* --------------------------------------------------------------------- */

void midi_load_page(struct page *page)
{
        page->title = "Midi Screen (Shift-F1)";
        page->draw_const = NULL;
        page->song_changed_cb = NULL;
        page->predraw_hook = NULL;
        page->playback_update = NULL;
        page->handle_key = NULL;
        page->set_page = NULL;
        page->total_items = 1;
        page->items = items_midi;
        page->help_index = HELP_GLOBAL;

        items_midi[0].type = ITEM_OTHER;
        items_midi[0].other.handle_key = midi_page_handle_key;
        items_midi[0].other.redraw = midi_page_redraw;
}
