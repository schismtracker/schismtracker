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

#include "headers.h"

#include "it.h"
#include "page.h"

/* --------------------------------------------------------------------- */
/* create_* functions (the constructors, if you will) */

void create_toggle(struct widget *w, int x, int y, int next_up, int next_down,
                   int next_left, int next_right, int next_tab, void (*changed) (void))
{
        w->type = WIDGET_TOGGLE;
        w->x = x;
        w->y = y;
        w->width = 3;   /* "Off" */
        w->next.up = next_up;
        w->next.left = next_left;
        w->next.down = next_down;
        w->next.right = next_right;
        w->next.tab = next_tab;
        w->changed = changed;
        w->activate = NULL;
}

void create_menutoggle(struct widget *w, int x, int y, int next_up, int next_down, int next_left,
                       int next_right, int next_tab, void (*changed) (void), const char **choices)
{
        int n, width = 0, len;

        for (n = 0; choices[n]; n++) {
                len = strlen(choices[n]);
                if (width < len)
                        width = len;
        }

        w->type = WIDGET_MENUTOGGLE;
        w->x = x;
        w->y = y;
        w->width = width;
        w->next.up = next_up;
        w->next.left = next_left;
        w->next.down = next_down;
        w->next.right = next_right;
        w->next.tab = next_tab;
        w->changed = changed;
        w->menutoggle.choices = choices;
        w->menutoggle.num_choices = n;
        w->activate = NULL;
}

void create_button(struct widget *w, int x, int y, int width, int next_up, int next_down, int next_left,
                   int next_right, int next_tab, void (*changed) (void), const char *text, int padding)
{
        w->type = WIDGET_BUTTON;
        w->x = x;
        w->y = y;
        w->width = width;
        w->next.up = next_up;
        w->next.left = next_left;
        w->next.down = next_down;
        w->next.right = next_right;
        w->next.tab = next_tab;
        w->changed = changed;
        w->button.text = text;
        w->button.padding = padding;
        w->activate = NULL;
}

void create_togglebutton(struct widget *w, int x, int y, int width, int next_up, int next_down,
                         int next_left, int next_right, int next_tab, void (*changed) (void),
                         const char *text, int padding, int *group)
{
        w->type = WIDGET_TOGGLEBUTTON;
        w->x = x;
        w->y = y;
        w->width = width;
        w->next.up = next_up;
        w->next.left = next_left;
        w->next.down = next_down;
        w->next.right = next_right;
        w->next.tab = next_tab;
        w->changed = changed;
        w->togglebutton.text = text;
        w->togglebutton.padding = padding;
        w->togglebutton.group = group;
        w->activate = NULL;
}

void create_textentry(struct widget *w, int x, int y, int width, int next_up, int next_down,
                      int next_tab, void (*changed) (void), char *text, int max_length)
{
        w->type = WIDGET_TEXTENTRY;
        w->x = x;
        w->y = y;
        w->width = width;
        w->next.up = next_up;
        w->next.down = next_down;
        w->next.tab = next_tab;
        w->changed = changed;
        w->textentry.text = text;
        w->textentry.max_length = max_length;
        w->textentry.firstchar = 0;
        w->textentry.cursor_pos = 0;
        w->activate = NULL;
}

void create_numentry(struct widget *w, int x, int y, int width, int next_up, int next_down,
                     int next_tab, void (*changed) (void), int min, int max, int *cursor_pos)
{
        w->type = WIDGET_NUMENTRY;
        w->x = x;
        w->y = y;
        w->width = width;
        w->next.up = next_up;
        w->next.down = next_down;
        w->next.tab = next_tab;
        w->changed = changed;
        w->numentry.min = min;
        w->numentry.max = max;
        w->numentry.cursor_pos = cursor_pos;
        w->activate = NULL;
}

void create_thumbbar(struct widget *w, int x, int y, int width, int next_up, int next_down,
                     int next_tab, void (*changed) (void), int min, int max)
{
        w->type = WIDGET_THUMBBAR;
        w->x = x;
        w->y = y;
        w->width = width;
        w->next.up = next_up;
        w->next.down = next_down;
        w->next.tab = next_tab;
        w->changed = changed;
        w->thumbbar.min = min;
        w->thumbbar.max = max;
	w->thumbbar.text_at_min = NULL;
	w->thumbbar.text_at_max = NULL;
        w->activate = NULL;
}

void create_panbar(struct widget *w, int x, int y, int next_up, int next_down, int next_tab,
                   void (*changed) (void), int channel)
{
        w->type = WIDGET_PANBAR;
        w->x = x;
        w->y = y;
        w->width = 0;   /* never gets used */
        w->next.up = next_up;
        w->next.down = next_down;
        w->next.tab = next_tab;
        w->changed = changed;
        w->panbar.min = 0;
        w->panbar.max = 64;
        w->panbar.channel = channel;
        w->activate = NULL;
}

void create_other(struct widget *w, int next_tab, int (*i_handle_key) (SDL_keysym *k), void (*i_redraw) (void))
{
	w->type = WIDGET_OTHER;
	w->next.up = w->next.down = w->next.left = w->next.right = 0;
	w->next.tab = next_tab;
	/* w->changed = NULL; ??? */
	w->activate = NULL;
	
	w->other.handle_key = i_handle_key;
	w->other.redraw = i_redraw;
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
        /* FIXME: this causes some weirdness with the end key. maybe hitting end should trim spaces? */
        while (len < *cursor_pos)
                text[len++] = ' ';
        memmove(text + *cursor_pos + 1, text + *cursor_pos, max_length - *cursor_pos - 1);
        text[*cursor_pos] = c;
        (*cursor_pos)++;
}

void text_delete_char(char *text, int *cursor_pos, int max_length)
{
        if (*cursor_pos == 0)
                return;
        (*cursor_pos)--;
        memmove(text + *cursor_pos, text + *cursor_pos + 1, max_length - *cursor_pos);
}

void text_delete_next_char(char *text, int *cursor_pos, int max_length)
{
        memmove(text + *cursor_pos, text + *cursor_pos + 1, max_length - *cursor_pos);
}

/* --------------------------------------------------------------------- */
/* text entries */

static void textentry_reposition(struct widget *w)
{
	int len;
	
        w->textentry.text[w->textentry.max_length] = 0;
	
	len = strlen(w->textentry.text);
        if (w->textentry.cursor_pos > len)
                w->textentry.cursor_pos = len;
	
        if (w->textentry.cursor_pos > (w->textentry.firstchar + w->width - 1)) {
                w->textentry.firstchar = w->textentry.cursor_pos - w->width + 1;
                if (w->textentry.firstchar < 0)
                        w->textentry.firstchar = 0;
        }
}

int textentry_add_char(struct widget *w, Uint16 unicode)
{
        char c = unicode_to_ascii(unicode);

        if (c == 0)
                return 0;
        text_add_char(w->textentry.text, c, &(w->textentry.cursor_pos), w->textentry.max_length);

        RUN_IF(w->changed);
        status.flags |= NEED_UPDATE;

        return 1;
}

#if 0
void textentry_set(struct widget *w, const char *text)
{
	strncpy(w->textentry.text, text, w->textentry.max_length);
	w->textentry.text[w->textentry.max_length] = 0;
	w->textentry.cursor_pos = strlen(w->textentry.text);
}
#endif

/* --------------------------------------------------------------------- */
/* numeric entries */

void numentry_change_value(struct widget *w, int new_value)
{
        new_value = CLAMP(new_value, w->numentry.min, w->numentry.max);
        w->numentry.value = new_value;
        RUN_IF(w->changed);
        status.flags |= NEED_UPDATE;
}

/* I'm sure there must be a simpler way to do this. */
int numentry_handle_digit(struct widget *w, Uint16 unicode)
{
        char c = unicode_to_ascii(unicode);
        int width, value, n;
        static const int tens[7] = { 1, 10, 100, 1000, 10000, 100000, 1000000 };
        int digits[7] = { 0 };

        if (c < '0' || c > '9')
                return 0;

        width = w->width;
        value = w->numentry.value;
        for (n = width - 1; n >= 0; n--)
                digits[n] = value / tens[n] % 10;
        digits[width - *(w->numentry.cursor_pos) - 1] = c - '0';
        value = 0;
        for (n = width - 1; n >= 0; n--)
                value += digits[n] * tens[n];
        value = CLAMP(value, w->numentry.min, w->numentry.max);
        w->numentry.value = value;
        if (*(w->numentry.cursor_pos) < w->width - 1)
                (*(w->numentry.cursor_pos))++;

        RUN_IF(w->changed);
        status.flags |= NEED_UPDATE;

        return 1;
}

/* --------------------------------------------------------------------- */
/* toggle buttons */

void togglebutton_set(struct widget *p_widgets, int widget, int do_callback)
{
        int i;
        int *group = p_widgets[widget].togglebutton.group;

        for (i = 0; group[i] >= 0; i++)
                p_widgets[group[i]].togglebutton.state = 0;
        p_widgets[widget].togglebutton.state = 1;
	
	if (do_callback)
		RUN_IF(p_widgets[widget].changed);
	
        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* /me takes a deep breath */

void draw_widget(struct widget *w, int selected)
{
        char buf[16] = "Channel 42";
        const char *ptr, *endptr;       /* for the menutoggle */
        int n;
        int tfg = selected ? 0 : 2;
        int tbg = selected ? 3 : 0;

        switch (w->type) {
        case WIDGET_TOGGLE:
                draw_fill_chars(w->x, w->y, w->x + w->width - 1, w->y, 0);
                draw_text(w->toggle.state ? "On" : "Off", w->x, w->y, tfg, tbg);
                break;
        case WIDGET_MENUTOGGLE:
                draw_fill_chars(w->x, w->y, w->x + w->width - 1, w->y, 0);
                ptr = w->menutoggle.choices[w->menutoggle.state];
                endptr = strchr(ptr, ' ');
                if (endptr) {
                        n = endptr - ptr;
                        draw_text_len(ptr, n, w->x, w->y, tfg, tbg);
                        draw_text(endptr + 1, w->x + n + 1, w->y, 2, 0);
                } else {
                        draw_text(ptr, w->x, w->y, tfg, tbg);
                }
                break;
        case WIDGET_BUTTON:
                draw_box(w->x - 1, w->y - 1, w->x + w->width + 2, w->y + 1,
                         BOX_THIN | BOX_INNER | BOX_OUTSET);
                draw_text(w->button.text, w->x + w->button.padding, w->y, selected ? 3 : 0, 2);
                break;
        case WIDGET_TOGGLEBUTTON:
                draw_box(w->x - 1, w->y - 1, w->x + w->width + 2, w->y + 1,
                         BOX_THIN | BOX_INNER | (w->togglebutton.state ? BOX_INSET : BOX_OUTSET));
                draw_text(w->togglebutton.text, w->x + w->togglebutton.padding, w->y, selected ? 3 : 0, 2);
                break;
        case WIDGET_TEXTENTRY:
                textentry_reposition(w);
                draw_text_len(w->textentry.text + w->textentry.firstchar, w->width, w->x, w->y, 2, 0);
                if (selected) {
                        n = w->textentry.cursor_pos - w->textentry.firstchar;
                        draw_char(((n < (signed) strlen(w->textentry.text))
                                   ? (w->textentry.text[w->textentry.cursor_pos]) : ' '),
                                  w->x + n, w->y, 0, 3);
                }
                break;
        case WIDGET_NUMENTRY:
                /* Impulse Tracker's numeric entries are all either three or seven digits long. */
                switch (w->width) {
                case 3:
                        draw_text_len(numtostr(3, w->numentry.value, buf), 3, w->x, w->y, 2, 0);
                        break;
                case 7:
                        draw_text_len(numtostr(7, w->numentry.value, buf), 7, w->x, w->y, 2, 0);
                        break;
                }
                if (selected) {
                        n = *(w->numentry.cursor_pos);
                        draw_char(buf[n], w->x + n, w->y, 0, 3);
                }
                break;
        case WIDGET_THUMBBAR:
		if (w->thumbbar.text_at_min && w->thumbbar.min == w->thumbbar.value) {
			draw_text_len(w->thumbbar.text_at_min, w->width, w->x, w->y, selected ? 3 : 2, 0);
		} else if (w->thumbbar.text_at_max && w->thumbbar.max == w->thumbbar.value) {
			/* this will probably do Bad Things if the text is too long */
			int len = strlen(w->thumbbar.text_at_max);
			int pos = w->x + w->width - len;
			
			draw_fill_chars(w->x, w->y, pos - 1, w->y, 0);
			draw_text_len(w->thumbbar.text_at_max, len, pos, w->y, selected ? 3 : 2, 0);
		} else {
			draw_thumb_bar(w->x, w->y, w->width, w->thumbbar.min,
				       w->thumbbar.max, w->thumbbar.value, selected);
		}
                draw_text(numtostr(3, w->thumbbar.value, buf), w->x + w->width + 1, w->y, 1, 2);
                break;
        case WIDGET_PANBAR:
                numtostr(2, w->panbar.channel, buf + 8);
                draw_text(buf, w->x, w->y, selected ? 3 : 0, 2);
                if (w->panbar.muted) {
                        draw_text("  Muted  ", w->x + 11, w->y, selected ? 3 : 5, 0);
                        /* draw_fill_chars(w->x + 21, w->y, w->x + 23, w->y, 2); */
                } else if (w->panbar.surround) {
                        draw_text("Surround ", w->x + 11, w->y, selected ? 3 : 5, 0);
                        /* draw_fill_chars(w->x + 21, w->y, w->x + 23, w->y, 2); */
                } else {
                        draw_thumb_bar(w->x + 11, w->y, 9, 0, 64, w->panbar.value, selected);
                        draw_text(numtostr(3, w->thumbbar.value, buf), w->x + 21, w->y, 1, 2);
                }
                break;
        case WIDGET_OTHER:
                w->other.redraw();
                break;
        default:
                /* shouldn't ever happen... */
                break;
        }
}

/* --------------------------------------------------------------------- */
/* more crap */

void change_focus_to(int new_widget_index)
{
        *selected_widget = new_widget_index;

        if (ACTIVE_WIDGET.type == WIDGET_TEXTENTRY)
                ACTIVE_WIDGET.textentry.cursor_pos = strlen(ACTIVE_WIDGET.textentry.text);

        status.flags |= NEED_UPDATE;
}
