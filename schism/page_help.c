/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * URL: http://rigelseven.com/schism/
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

/* Well, this page is just a big hack factory, but it's at least an
 * improvement over the message editor :P */

#include "headers.h"

#include "it.h"
#include "page.h"

#include "sdlmain.h"

/* --------------------------------------------------------------------- */

static struct widget widgets_help[2];

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

	if (status.dialog_type == DIALOG_NONE) change_focus_to(1);
}

static void help_redraw(void)
{
        int n, pos, x;
        const char **ptr;

        draw_fill_chars(2, 13, 77, 44, 0);

        ptr = newline_pointers + top_line;
        for (pos = 13, n = top_line; pos < 45; pos++, n++) {
                switch (**ptr) {
		case ':':	/* schism-only (drawn the same) */
                case '|':	/* normal line */
		case '!':	/* classic mode only */
                        draw_text_len((const unsigned char *) *ptr + 1,
					strcspn(*ptr + 1, "\015\012"), 2,
					pos, 6, 0);
                        break;
                case '#':      /* hidden line */
                        draw_text_len((const unsigned char *) *ptr + 1,
					strcspn(*ptr + 1, "\015\012"), 2,
					pos, 7, 0);
                        break;
                case '%':      /* separator line */
                        for (x = 2; x < 78; x++)
                                draw_char(154, x, pos, 6, 0);
                        break;
                default:       /* ack! */
                        fprintf(stderr, "unknown help line format %c\n", **ptr);
                        break;
                }
                ptr++;
        }
}

/* --------------------------------------------------------------------- */
static void _help_close(void)
{
	set_page(status.previous_page);
}
static int help_handle_key(struct key_event * k)
{
        int new_line = top_line;

	if (status.dialog_type != DIALOG_NONE) return 0;

	if (k->mouse == 2) {
		new_line--;
	} else if (k->mouse == 3) {
		new_line++;

	} else if (k->mouse) {
		return 0;
	}
        switch (k->sym) {
        case SDLK_ESCAPE:
		if (!k->state) return 1;
                set_page(status.previous_page);
                return 1;
        case SDLK_UP:
		if (k->state) return 1;
                new_line--;
                break;
        case SDLK_DOWN:
		if (k->state) return 1;
                new_line++;
                break;
        case SDLK_PAGEUP:
		if (k->state) return 1;
                new_line -= 32;
                break;
        case SDLK_PAGEDOWN:
		if (k->state) return 1;
                new_line += 32;
                break;
        case SDLK_HOME:
		if (k->state) return 1;
                new_line = 0;
                break;
        case SDLK_END:
		if (k->state) return 1;
                new_line = num_lines - 32;
                break;
        default:
		if (k->mouse) {
			if (k->state) return 1;
		} else {
			return 0;
		}
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

	change_focus_to(1);
        top_line = 0;

        /* how many lines? */
        global_lines = get_num_lines(help_text_pointers[HELP_GLOBAL]);
        if (have_local_help) {
                local_lines = get_num_lines(help_text_pointers
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
			if (status.flags & CLASSIC_MODE) {
				if (ptr[0] != ':' && ptr[0] != '#')
					newline_pointers[cur_line++] = ptr;
			} else {
				if (ptr[0] != '!')
					newline_pointers[cur_line++] = ptr;
			}
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
		if (status.flags & CLASSIC_MODE) {
			if (ptr[0] != ':' && ptr[0] != '#')
				newline_pointers[cur_line++] = ptr;
		} else {
			if (ptr[0] != '!')
				newline_pointers[cur_line++] = ptr;
		}
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
	num_lines = cur_line;
}

/* --------------------------------------------------------------------- */

void help_load_page(struct page *page)
{
        page->title = "Help";
        page->draw_const = help_draw_const;
        page->set_page = help_set_page;
        page->total_widgets = 2;
        page->widgets = widgets_help;
	page->pre_handle_key = help_handle_key;

	create_other(widgets_help + 0, 0, help_handle_key, help_redraw);
	create_button(widgets_help + 1, 35,47,8, 0, 1, 1,1, 0,
			_help_close, "Done", 3);
}
