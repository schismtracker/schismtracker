/* It's lo-og, lo-og, it's big, it's heavy, it's wood!
 * It's lo-og, lo-og, it's better than bad, it's good! */

#include "headers.h"

#include <SDL.h>
#include <stdarg.h>

#include "it.h"
#include "page.h"

/* --------------------------------------------------------------------- */

static struct item items_log[1];

#define NUM_LINES 33
static struct log_line lines[NUM_LINES];
static int last_line = -1;

/* --------------------------------------------------------------------- */

static void log_draw_const(void)
{
        draw_box(1, 12, 78, 48, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_fill_chars(2, 13, 77, 47, 0);
}

static int log_handle_key(UNUSED SDL_keysym * k)
{
        return 0;
}

static void log_redraw(void)
{
        int n;

        for (n = 0; n <= last_line; n++)
                draw_text_len(lines[n].text, 74, 3, 14 + n, lines[n].color,
                              0);
}

/* --------------------------------------------------------------------- */

void log_load_page(struct page *page)
{
        page->title = "Message Log Viewer (Ctrl-F1)";
        page->draw_const = log_draw_const;
        page->total_items = 1;
        page->items = items_log;
        page->help_index = HELP_GLOBAL;

        items_log[0].type = ITEM_OTHER;
        items_log[0].next.tab = 1;
        items_log[0].other.handle_key = log_handle_key;
        items_log[0].other.redraw = log_redraw;
}

/* --------------------------------------------------------------------- */

inline void log_append(int color, int must_free, const char *text)
{
        int n;

        if (last_line < NUM_LINES - 1) {
                last_line++;
        } else {
                if (lines[0].must_free)
                        free((void *) lines[0].text);
                for (n = 0; n < last_line; n++)
                        lines[n] = lines[n + 1];
        }
        lines[last_line].text = text;
        lines[last_line].color = color;
        lines[last_line].must_free = must_free;

        if (status.current_page == PAGE_LOG) {
                /* FIXME | it'd be nice to blit the area and only draw one
                 * FIXME | line, instead of redrawing the whole thing. */
                status.flags |= NEED_UPDATE;
        }
}

void log_appendf(int color, const char *format, ...)
{
        char *ptr;
        va_list ap;

        va_start(ap, format);
        vasprintf(&ptr, format, ap);
        va_end(ap);

        log_append(color, 1, ptr);
}
