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

/* --------------------------------------------------------------------- */
/* create_* functions (the constructors, if you will) */

void create_toggle(struct item *i, int x, int y,
                   int next_up, int next_down, int next_left,
                   int next_right, int next_tab, void (*changed) (void))
{
        i->type = ITEM_TOGGLE;
        i->x = x;
        i->y = y;
        i->width = 3;   /* "Off" */
        i->next.up = next_up;
        i->next.left = next_left;
        i->next.down = next_down;
        i->next.right = next_right;
        i->next.tab = next_tab;
        i->changed = changed;
        i->activate = NULL;
}

void create_menutoggle(struct item *i, int x, int y,
                       int next_up, int next_down, int next_left,
                       int next_right, int next_tab,
                       void (*changed) (void), const char **choices)
{
        int n, width = 0, len;

        for (n = 0; choices[n]; n++) {
                len = strlen(choices[n]);
                if (width < len)
                        width = len;
        }

        i->type = ITEM_MENUTOGGLE;
        i->x = x;
        i->y = y;
        i->width = width;
        i->next.up = next_up;
        i->next.left = next_left;
        i->next.down = next_down;
        i->next.right = next_right;
        i->next.tab = next_tab;
        i->changed = changed;
        i->menutoggle.choices = choices;
        i->menutoggle.num_choices = n;
        i->activate = NULL;
}

void create_button(struct item *i, int x, int y, int width,
                   int next_up, int next_down, int next_left,
                   int next_right, int next_tab, void (*changed) (void),
                   const char *text, int padding)
{
        i->type = ITEM_BUTTON;
        i->x = x;
        i->y = y;
        i->width = width;
        i->next.up = next_up;
        i->next.left = next_left;
        i->next.down = next_down;
        i->next.right = next_right;
        i->next.tab = next_tab;
        i->changed = changed;
        i->button.text = text;
        i->button.padding = padding;
        i->activate = NULL;
}

void create_togglebutton(struct item *i, int x, int y, int width,
                         int next_up, int next_down, int next_left,
                         int next_right, int next_tab,
                         void (*changed) (void), const char *text,
                         int padding, int *group)
{
        i->type = ITEM_TOGGLEBUTTON;
        i->x = x;
        i->y = y;
        i->width = width;
        i->next.up = next_up;
        i->next.left = next_left;
        i->next.down = next_down;
        i->next.right = next_right;
        i->next.tab = next_tab;
        i->changed = changed;
        i->togglebutton.text = text;
        i->togglebutton.padding = padding;
        i->togglebutton.group = group;
        i->activate = NULL;
}

void create_textentry(struct item *i, int x, int y, int width,
                      int next_up, int next_down, int next_tab,
                      void (*changed) (void), char *text,
		      int max_length)
{
        i->type = ITEM_TEXTENTRY;
        i->x = x;
        i->y = y;
        i->width = width;
        i->next.up = next_up;
        i->next.down = next_down;
        i->next.tab = next_tab;
        i->changed = changed;
        i->textentry.text = text;
        i->textentry.max_length = max_length;
        i->textentry.firstchar = 0;
        i->textentry.cursor_pos = 0;
        i->activate = NULL;
}

void create_numentry(struct item *i, int x, int y, int width,
                     int next_up, int next_down, int next_tab,
                     void (*changed) (void), int min, int max,
                     int *cursor_pos)
{
        i->type = ITEM_NUMENTRY;
        i->x = x;
        i->y = y;
        i->width = width;
        i->next.up = next_up;
        i->next.down = next_down;
        i->next.tab = next_tab;
        i->changed = changed;
        i->numentry.min = min;
        i->numentry.max = max;
        i->numentry.cursor_pos = cursor_pos;
        i->activate = NULL;
}

void create_thumbbar(struct item *i, int x, int y, int width,
                     int next_up, int next_down, int next_tab,
                     void (*changed) (void), int min, int max)
{
        i->type = ITEM_THUMBBAR;
        i->x = x;
        i->y = y;
        i->width = width;
        i->next.up = next_up;
        i->next.down = next_down;
        i->next.tab = next_tab;
        i->changed = changed;
        i->thumbbar.min = min;
        i->thumbbar.max = max;
	i->thumbbar.text_at_min = NULL;
	i->thumbbar.text_at_max = NULL;
        i->activate = NULL;
}

void create_panbar(struct item *i, int x, int y,
                   int next_up, int next_down, int next_tab,
                   void (*changed) (void), int channel)
{
        i->type = ITEM_PANBAR;
        i->x = x;
        i->y = y;
        i->width = 0;   /* never gets used */
        i->next.up = next_up;
        i->next.down = next_down;
        i->next.tab = next_tab;
        i->changed = changed;
        i->panbar.min = 0;
        i->panbar.max = 64;
        i->panbar.channel = channel;
        i->activate = NULL;
}

void create_other(struct item *i, int next_tab, int (*i_handle_key) (SDL_keysym *k), void (*i_redraw) (void))
{
	i->type = ITEM_OTHER;
	i->next.up = i->next.down = i->next.left = i->next.right = 0;
	i->next.tab = next_tab;
	/* i->changed = NULL; ??? */
	i->activate = NULL;
	
	i->other.handle_key = i_handle_key;
	i->other.redraw = i_redraw;
}

/* --------------------------------------------------------------------- */
/* generic text stuff */

void text_add_char(char *text, char c, int *cursor_pos, int max_length)
{
        int len;

        text[max_length] = 0;
        len = strlen(text);
        if (*cursor_pos >= max_length)
                *cursor_pos = max_length - 1;
        /* FIXME | this causes some weirdness with the end key.
         * FIXME | maybe hitting end should trim spaces? */
        while (len < *cursor_pos) {
                text[len++] = ' ';
        }
        memmove(text + *cursor_pos + 1, text + *cursor_pos,
                max_length - *cursor_pos - 1);
        text[*cursor_pos] = c;
        (*cursor_pos)++;
}

void text_delete_char(char *text, int *cursor_pos, int max_length)
{
        if (*cursor_pos == 0)
                return;
        (*cursor_pos)--;
        memmove(text + *cursor_pos, text + *cursor_pos + 1,
                max_length - *cursor_pos);
}

void text_delete_next_char(char *text, int *cursor_pos, int max_length)
{
        memmove(text + *cursor_pos, text + *cursor_pos + 1,
                max_length - *cursor_pos);
}

/* --------------------------------------------------------------------- */
/* text entries */

static void textentry_reposition(struct item *i)
{
	int len;
	
        i->textentry.text[i->textentry.max_length] = 0;
	
	len = strlen(i->textentry.text);
        if (i->textentry.cursor_pos > len)
                i->textentry.cursor_pos = len;
	
        if (i->textentry.cursor_pos > (i->textentry.firstchar + i->width - 1)) {
                i->textentry.firstchar = i->textentry.cursor_pos - i->width + 1;
                if (i->textentry.firstchar < 0)
                        i->textentry.firstchar = 0;
        }
}

int textentry_add_char(struct item *item, Uint16 unicode)
{
        char c = unicode_to_ascii(unicode);

        if (c == 0)
                return 0;
        text_add_char(item->textentry.text, c,
                      &(item->textentry.cursor_pos),
                      item->textentry.max_length);

        RUN_IF(item->changed);
        status.flags |= NEED_UPDATE;

        return 1;
}

#if 0
void textentry_set(struct item *item, const char *text)
{
	strncpy(item->textentry.text, text, item->textentry.max_length);
	item->textentry.text[item->textentry.max_length] = 0;
	item->textentry.cursor_pos = strlen(item->textentry.text);
}
#endif

/* --------------------------------------------------------------------- */
/* numeric entries */

void numentry_change_value(struct item *item, int new_value)
{
        new_value = CLAMP(new_value, item->numentry.min, item->numentry.max);
        item->numentry.value = new_value;
        RUN_IF(item->changed);
        status.flags |= NEED_UPDATE;
}

/* I'm sure there must be a simpler way to do this. */
int numentry_handle_digit(struct item *item, Uint16 unicode)
{
        char c = unicode_to_ascii(unicode);
        int width, value, n;
        static const int tens[7] =
                { 1, 10, 100, 1000, 10000, 100000, 1000000 };
        int digits[7] = { 0 };

        if (c < '0' || c > '9')
                return 0;

        width = item->width;
        value = item->numentry.value;
        for (n = width - 1; n >= 0; n--)
                digits[n] = value / tens[n] % 10;
        digits[width - *(item->numentry.cursor_pos) - 1] = c - '0';
        value = 0;
        for (n = width - 1; n >= 0; n--)
                value += digits[n] * tens[n];
        value = CLAMP(value, item->numentry.min, item->numentry.max);
        item->numentry.value = value;
        if (*(item->numentry.cursor_pos) < item->width - 1)
                (*(item->numentry.cursor_pos))++;

        RUN_IF(item->changed);
        status.flags |= NEED_UPDATE;

        return 1;
}

/* --------------------------------------------------------------------- */
/* toggle buttons */

void togglebutton_set(struct item *p_items, int item, int do_callback)
{
        int i;
        int *group = p_items[item].togglebutton.group;

        for (i = 0; group[i] >= 0; i++)
                p_items[group[i]].togglebutton.state = 0;
        p_items[item].togglebutton.state = 1;
	
	if (do_callback)
		RUN_IF(p_items[item].changed);
	
        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* /me takes a deep breath */

void draw_item(struct item *i, int selected)
{
        char buf[16] = "Channel 42";
        const char *ptr, *endptr;       /* for the menutoggle */
        int n;
        int tfg = selected ? 0 : 2;
        int tbg = selected ? 3 : 0;

        switch (i->type) {
        case ITEM_TOGGLE:
                draw_fill_chars(i->x, i->y, i->x + i->width - 1, i->y, 0);
                draw_text(i->toggle.state ? "On" : "Off", i->x, i->y, tfg,
                          tbg);
                break;
        case ITEM_MENUTOGGLE:
                draw_fill_chars(i->x, i->y, i->x + i->width - 1, i->y, 0);
                ptr = i->menutoggle.choices[i->menutoggle.state];
                endptr = strchr(ptr, ' ');
                if (endptr) {
                        n = endptr - ptr;
                        draw_text_len(ptr, n, i->x, i->y, tfg, tbg);
                        draw_text(endptr + 1, i->x + n + 1, i->y, 2, 0);
                } else {
                        draw_text(ptr, i->x, i->y, tfg, tbg);
                }
                break;
        case ITEM_BUTTON:
                draw_box(i->x - 1, i->y - 1, i->x + i->width + 2, i->y + 1,
                         BOX_THIN | BOX_INNER | BOX_OUTSET);
                draw_text(i->button.text, i->x + i->button.padding, i->y,
                          selected ? 3 : 0, 2);
                break;
        case ITEM_TOGGLEBUTTON:
                draw_box(i->x - 1, i->y - 1, i->x + i->width + 2, i->y + 1,
                         BOX_THIN | BOX_INNER | (i->togglebutton.
                                                 state ? BOX_INSET :
                                                 BOX_OUTSET));
                draw_text(i->togglebutton.text,
                          i->x + i->togglebutton.padding, i->y,
                          selected ? 3 : 0, 2);
                break;
        case ITEM_TEXTENTRY:
                textentry_reposition(i);
                draw_text_len(i->textentry.text + i->textentry.firstchar,
                              i->width, i->x, i->y, 2, 0);
                if (selected) {
                        n = i->textentry.cursor_pos -
                                i->textentry.firstchar;
                        draw_char(((n < (signed) strlen(i->textentry.text))
                                   ? (i->textentry.
                                      text[i->textentry.
                                           cursor_pos]) : ' '), i->x + n,
                                  i->y, 0, 3);
                }
                break;
        case ITEM_NUMENTRY:
                /* Impulse Tracker's numeric entries are all either
                 * three or seven digits long. */
                switch (i->width) {
                case 3:
                        draw_text_len(numtostr(3, i->numentry.value, buf),
                                      3, i->x, i->y, 2, 0);
                        break;
                case 7:
                        draw_text_len(numtostr(7, i->numentry.value, buf),
                                      7, i->x, i->y, 2, 0);
                        break;
                }
                if (selected) {
                        n = *(i->numentry.cursor_pos);
                        draw_char(buf[n], i->x + n, i->y, 0, 3);
                }
                break;
        case ITEM_THUMBBAR:
		if (i->thumbbar.text_at_min && i->thumbbar.min == i->thumbbar.value) {
			draw_text_len(i->thumbbar.text_at_min, i->width, i->x, i->y,
				      selected ? 3 : 2, 0);
		} else if (i->thumbbar.text_at_max && i->thumbbar.max == i->thumbbar.value) {
			/* this will probably do Bad Things if the text is too long */
			int len = strlen(i->thumbbar.text_at_max);
			int pos = i->x + i->width - len;
			
			draw_fill_chars(i->x, i->y, pos - 1, i->y, 0);
			draw_text_len(i->thumbbar.text_at_max, len, pos, i->y,
				      selected ? 3 : 2, 0);
		} else {
			draw_thumb_bar(i->x, i->y, i->width, i->thumbbar.min,
				       i->thumbbar.max, i->thumbbar.value, selected);
		}
                draw_text(numtostr(3, i->thumbbar.value, buf),
                          i->x + i->width + 1, i->y, 1, 2);
                break;
        case ITEM_PANBAR:
                numtostr(2, i->panbar.channel, buf + 8);
                draw_text(buf, i->x, i->y, selected ? 3 : 0, 2);
                if (i->panbar.muted) {
                        draw_text("  Muted  ", i->x + 11, i->y,
                                  selected ? 3 : 5, 0);
                        /* draw_fill_chars(i->x + 21, i->y,
                         *                 i->x + 23, i->y, 2); */
                } else if (i->panbar.surround) {
                        draw_text("Surround ", i->x + 11, i->y,
                                  selected ? 3 : 5, 0);
                        /* draw_fill_chars(i->x + 21, i->y,
                         *                 i->x + 23, i->y, 2); */
                } else {
                        draw_thumb_bar(i->x + 11, i->y, 9, 0, 64,
                                       i->panbar.value, selected);
                        draw_text(numtostr(3, i->thumbbar.value, buf),
                                  i->x + 21, i->y, 1, 2);
                }
                break;
        case ITEM_OTHER:
                i->other.redraw();
                break;
        default:
                /* shouldn't ever happen... */
                break;
        }
}

/* --------------------------------------------------------------------- */
/* more crap */

void change_focus_to(int new_item_index)
{
        *selected_item = new_item_index;

        if (ACTIVE_ITEM.type == ITEM_TEXTENTRY)
                ACTIVE_ITEM.textentry.cursor_pos =
                        strlen(ACTIVE_ITEM.textentry.text);

        status.flags |= NEED_UPDATE;
}
