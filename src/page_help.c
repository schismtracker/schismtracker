/* Well, this page is just a big hack factory, but it's at least an
 * improvement over the message editor :P */

#include "headers.h"

#include <SDL.h>

#include "it.h"
#include "page.h"

/* --------------------------------------------------------------------- */

static struct item items_help[2];

/* newline_pointers[0] = top of text
 * newline_pointers[1] = second line
 * etc.
 * 
 * Each line is terminated by chr 13, chr 10, or chr 0... yeah, maybe I
 * could've done this a smarter way and have every line end with chr 0
 * or something, but it'd be harder to deal with in other places, like
 * editing the text (especially considering the dopey crlf newlines
 * imposed by that ugly OS on the other side of the tracks) */
static const char **newline_pointers = NULL;

static int num_lines = 0;
static int top_line = 0;

static const char *blank_line = "|";
static const char *separator_line = "%";

/* --------------------------------------------------------------------- */

static void help_draw_const(void)
{
        draw_box(1, 12, 78, 45, BOX_THICK | BOX_INNER | BOX_INSET);

        /* meh. */
        draw_box(34, 46, 45, 48, BOX_THIN | BOX_INNER | BOX_OUTSET);
        draw_text("Done", 38, 47, 3, 2);
}

static void help_redraw(void)
{
        int n, pos, x;
        const char **ptr;

        draw_fill_chars(2, 13, 77, 44, 0);

        ptr = newline_pointers + top_line;
        for (pos = 13, n = top_line; pos < 45; pos++, n++) {
                switch (**ptr) {
                case '|':      // normal line
                        draw_text_len(*ptr + 1,
                                      strcspn(*ptr + 1, "\015\012"), 2,
                                      pos, 6, 0);
                        break;
                case '#':      // hidden line
                        draw_text_len(*ptr + 1,
                                      strcspn(*ptr + 1, "\015\012"), 2,
                                      pos, 7, 0);
                        break;
                case '%':      // separator line
                        SDL_LockSurface(screen);
                        for (x = 2; x < 78; x++)
                                draw_char_unlocked(154, x, pos, 6, 0);
                        SDL_UnlockSurface(screen);
                        break;
                default:       // ack!
                        fprintf(stderr, "unknown help line format %c\n",
                                **ptr);
                        break;
                }
                ptr++;
        }
}

/* --------------------------------------------------------------------- */

static int help_handle_key(SDL_keysym * k)
{
        int new_line = top_line;

        switch (k->sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
        case SDLK_ESCAPE:
                set_page(status.previous_page);
                return 1;
        case SDLK_UP:
                new_line--;
                break;
        case SDLK_DOWN:
                new_line++;
                break;
        case SDLK_PAGEUP:
                new_line -= 32;
                break;
        case SDLK_PAGEDOWN:
                new_line += 32;
                break;
        case SDLK_HOME:
                new_line = 0;
                break;
        case SDLK_END:
                new_line = num_lines - 32;
                break;
        default:
                return 0;
        }

        new_line = CLAMP(new_line, 0, num_lines - 32);
        if (new_line != top_line) {
                top_line = new_line;
                status.flags |= NEED_UPDATE;
        }

        return 1;
}

/* --------------------------------------------------------------------- */
/* TODO | move all this crap to helptext.c
 * TODO | (so it gets done for all the pages, all at once) */

static void help_set_page(void)
{
        char *ptr;
        int local_lines = 0, global_lines = 0, cur_line = 0;
        int have_local_help = (status.current_help_index != HELP_GLOBAL);

        top_line = 0;

        /* how many lines? */
        global_lines = get_num_lines(help_text_pointers[HELP_GLOBAL]);
        if (have_local_help) {
                local_lines =
                        get_num_lines(help_text_pointers
                                      [status.current_help_index]);
                num_lines = local_lines + global_lines + 5;
        } else {
                num_lines = global_lines + 2;
        }

        /* allocate the array */
        if (newline_pointers)
                free(newline_pointers);
        newline_pointers = calloc(num_lines + 1, sizeof(char *));

        /* page help text */
        if (have_local_help) {
                ptr = help_text_pointers[status.current_help_index];
                while (local_lines--) {
                        newline_pointers[cur_line++] = ptr;
                        ptr = strpbrk(ptr, "\015\012");
                        if (ptr[0] == 13 && ptr[1] == 10)
                                ptr += 2;
                        else
                                ptr++;
                }
                /* separator line */
                newline_pointers[cur_line++] = blank_line;
                newline_pointers[cur_line++] = separator_line;
                newline_pointers[cur_line++] = blank_line;
        } else {
                /* some padding at the top */
                newline_pointers[cur_line++] = blank_line;
        }

        /* global help text */
        ptr = help_text_pointers[HELP_GLOBAL];
        while (global_lines--) {
                newline_pointers[cur_line++] = ptr;
                ptr = strpbrk(ptr, "\015\012");
                if (ptr[0] == 13 && ptr[1] == 10)
                        ptr += 2;
                else
                        ptr++;
        }

        newline_pointers[cur_line++] = blank_line;
        if (have_local_help) {
                newline_pointers[cur_line++] = separator_line;
        }

        newline_pointers[cur_line] = NULL;
}

/* --------------------------------------------------------------------- */

void help_load_page(struct page *page)
{
        page->title = "Help";
        page->draw_const = help_draw_const;
        page->set_page = help_set_page;
        page->total_items = 1;
        page->items = items_help;

        items_help[0].type = ITEM_OTHER;
        items_help[0].other.redraw = help_redraw;
        items_help[0].other.handle_key = help_handle_key;
}
