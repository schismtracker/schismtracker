#include "headers.h"

#include <SDL.h>

#include "it.h"
#include "page.h"

/* --------------------------------------------------------------------- */

/* n => the delta-value */
static void numentry_move_cursor(struct item *item, int n)
{
        n += *(item->numentry.cursor_pos);
        n = CLAMP(n, 0, item->width - 1);
        if (*(item->numentry.cursor_pos) == n)
                return;
        *(item->numentry.cursor_pos) = n;
        status.flags |= NEED_UPDATE;
}

static void textentry_move_cursor(struct item *item, int n)
{
        n += item->textentry.cursor_pos;
        n = CLAMP(n, 0, item->textentry.max_length);
        if (item->textentry.cursor_pos == n)
                return;
        item->textentry.cursor_pos = n;
        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* thumbbar value prompt */

static char thumbbar_prompt_buf[4];
static struct item thumbbar_prompt_items[1];

/* this is bound to the textentry's activate callback */
static void thumbbar_prompt_update(void)
{
        int n = atoi(thumbbar_prompt_buf);

        dialog_destroy();

        if (n >= ACTIVE_ITEM.thumbbar.min && n <= ACTIVE_ITEM.thumbbar.max) {
                ACTIVE_ITEM.thumbbar.value = n;
                RUN_IF(ACTIVE_ITEM.changed);
        }

        status.flags |= NEED_UPDATE;
}

static void thumbbar_prompt_draw_const(void)
{
        draw_text("Enter Value", 32, 26, 3, 2);
        draw_box(43, 25, 48, 27, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_fill_chars(44, 26, 47, 26, 0);
}

static int thumbbar_prompt_value(UNUSED struct item *item, Uint16 unicode)
{
        char c = unicode_to_ascii(unicode);
        struct dialog *dialog;

        if (c < '0' || c > '9')
                return 0;

        thumbbar_prompt_buf[0] = c;
        thumbbar_prompt_buf[1] = 0;

        create_textentry(thumbbar_prompt_items + 0, 44, 26, 4, 0, 0, 0,
                         NULL, thumbbar_prompt_update, thumbbar_prompt_buf,
                         3);
        thumbbar_prompt_items[0].textentry.cursor_pos = 1;

        dialog = dialog_create_custom();
        dialog->x = 29;
        dialog->y = 24;
        dialog->w = 22;
        dialog->h = 5;
        dialog->handle_key = NULL;
        dialog->draw_const = thumbbar_prompt_draw_const;
        dialog->items = thumbbar_prompt_items;
        dialog->total_items = 1;
        dialog->selected_item = 0;
        dialog_display_custom();

        return 1;
}

/* --------------------------------------------------------------------- */
/* This function is completely disgustipated. */

/* return: 1 = handled key, 0 = didn't */
int item_handle_key(struct item *item, SDL_keysym * k)
{
        int n;
        enum item_type current_type = item->type;

        if (current_type == ITEM_OTHER && item->other.handle_key(k))
                return 1;

        /* an ITEM_OTHER that *didn't* handle the key itself needs to get
         * run through the switch statement to account for stuff like the
         * tab key */

        switch (k->sym) {
        case SDLK_ESCAPE:
                /* this is to keep the text entries from taking the key
                 * hostage and inserting '<-' characters instead of
                 * showing the menu */
                return 0;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
                /* currently enter triggers the changed callback for
                 * (almost) any kind of item. no particular reason...
                 * i might get rid of this behavior, actually. */
                switch (current_type) {
                case ITEM_OTHER:
                        break;
                case ITEM_TEXTENTRY:
                        RUN_IF(item->changed);
                        RUN_IF(item->textentry.activate);
                        status.flags |= NEED_UPDATE;
                        return 1;
                case ITEM_TOGGLEBUTTON:
                        if (item->togglebutton.group) {
                                /* this also runs the changed callback
                                 * and redraws the button(s) */
                                togglebutton_set(item);
                                return 1;
                        }
                        /* else... */
                        item->togglebutton.state =
                                !item->togglebutton.state;
                        /* fall through */
                default:
                        RUN_IF(item->changed);
                        status.flags |= NEED_UPDATE;
                        return 1;
                }
                break;
        case SDLK_UP:
                change_focus_to(item->next.up);
                return 1;
        case SDLK_DOWN:
                change_focus_to(item->next.down);
                return 1;
        case SDLK_TAB:
                change_focus_to(item->next.tab);
                return 1;
        case SDLK_LEFT:
                switch (current_type) {
                case ITEM_NUMENTRY:
                        numentry_move_cursor(item, -1);
                        return 1;
                case ITEM_TEXTENTRY:
                        textentry_move_cursor(item, -1);
                        return 1;
                case ITEM_PANBAR:
                        item->panbar.muted = 0;
                        item->panbar.surround = 0;
                        /* fall through */
                case ITEM_THUMBBAR:
                        /* I'm handling the key modifiers differently than
                         * Impulse Tracker, but only because I think this
                         * is much more useful. :) */
                        n = 1;
                        if (k->mod & (KMOD_ALT | KMOD_META))
                                n *= 8;
                        if (k->mod & KMOD_SHIFT)
                                n *= 4;
                        if (k->mod & KMOD_CTRL)
                                n *= 2;
                        n = item->numentry.value - n;
                        numentry_change_value(item, n);
                        return 1;
                default:
                        change_focus_to(item->next.left);
                        return 1;
                }
                break;
        case SDLK_RIGHT:
                /* pretty much the same as left, but with a few small
                 * changes here and there... */
                switch (current_type) {
                case ITEM_NUMENTRY:
                        numentry_move_cursor(item, 1);
                        return 1;
                case ITEM_TEXTENTRY:
                        textentry_move_cursor(item, 1);
                        return 1;
                case ITEM_PANBAR:
                        item->panbar.muted = 0;
                        item->panbar.surround = 0;
                        /* fall through */
                case ITEM_THUMBBAR:
                        n = 1;
                        if (k->mod & (KMOD_ALT | KMOD_META))
                                n *= 8;
                        if (k->mod & KMOD_SHIFT)
                                n *= 4;
                        if (k->mod & KMOD_CTRL)
                                n *= 2;
                        n = item->numentry.value + n;
                        numentry_change_value(item, n);
                        return 1;
                default:
                        change_focus_to(item->next.right);
                        return 1;
                }
                break;
        case SDLK_HOME:
                /* Impulse Tracker only does home/end for the thumbbars.
                 * This stuff is all extra. */
                switch (current_type) {
                case ITEM_NUMENTRY:
                        *(item->numentry.cursor_pos) = 0;
                        status.flags |= NEED_UPDATE;
                        return 1;
                case ITEM_TEXTENTRY:
                        item->textentry.cursor_pos = 0;
                        status.flags |= NEED_UPDATE;
                        return 1;
                case ITEM_PANBAR:
                        item->panbar.muted = 0;
                        item->panbar.surround = 0;
                        /* fall through */
                case ITEM_THUMBBAR:
                        n = item->thumbbar.min;
                        numentry_change_value(item, n);
                        return 1;
                default:
                        break;
                }
                break;
        case SDLK_END:
                switch (current_type) {
                case ITEM_NUMENTRY:
                        *(item->numentry.cursor_pos) = item->width - 1;
                        status.flags |= NEED_UPDATE;
                        return 1;
                case ITEM_TEXTENTRY:
                        item->textentry.cursor_pos =
                                strlen(item->textentry.text);
                        status.flags |= NEED_UPDATE;
                        return 1;
                case ITEM_PANBAR:
                        item->panbar.muted = 0;
                        item->panbar.surround = 0;
                        /* fall through */
                case ITEM_THUMBBAR:
                        n = item->thumbbar.max;
                        numentry_change_value(item, n);
                        return 1;
                default:
                        break;
                }
                break;
        case SDLK_SPACE:
                switch (current_type) {
                case ITEM_TOGGLE:
                        item->toggle.state = !item->toggle.state;
                        if (item->changed)
                                item->changed();
                        status.flags |= NEED_UPDATE;
                        return 1;
                case ITEM_MENUTOGGLE:
                        item->menutoggle.state =
                                (item->menutoggle.state +
                                 1) % item->menutoggle.num_choices;
                        if (item->changed)
                                item->changed();
                        status.flags |= NEED_UPDATE;
                        return 1;
                case ITEM_PANBAR:
                        item->panbar.muted = !item->panbar.muted;
                        if (item->changed)
                                item->changed();
                        change_focus_to(item->next.down);
                        return 1;
                default:
                        break;
                }
                break;
        case SDLK_BACKSPACE:
                /* this ought to be in a separate function. */
                if (current_type != ITEM_TEXTENTRY)
                        break;
                if (!item->textentry.text[0]) {
                        /* nothing to do */
                        return 1;
                }
                if (k->mod & KMOD_CTRL) {
                        /* clear the whole field */
                        item->textentry.text[0] = 0;
                        item->textentry.cursor_pos = 0;
                } else {
                        /* TODO: ST3-ish backspace key... if the cursor is
                         * at the beginning, delete the first character */
                        text_delete_char(item->textentry.text,
                                         &(item->textentry.cursor_pos),
                                         item->textentry.max_length);
                }
                if (item->changed)
                        item->changed();
                status.flags |= NEED_UPDATE;
                return 1;
        case SDLK_DELETE:
                if (current_type != ITEM_TEXTENTRY)
                        break;
                if (!item->textentry.text[0]) {
                        /* nothing to do */
                        return 1;
                }
                text_delete_next_char(item->textentry.text,
                                      &(item->textentry.cursor_pos),
                                      item->textentry.max_length);
                if (item->changed)
                        item->changed();
                status.flags |= NEED_UPDATE;
                return 1;
        case SDLK_PLUS:
        case SDLK_KP_PLUS:
                if (current_type == ITEM_NUMENTRY) {
                        numentry_change_value(item,
                                              item->numentry.value + 1);
                        return 1;
                }
                break;
        case SDLK_MINUS:
        case SDLK_KP_MINUS:
                if (current_type == ITEM_NUMENTRY) {
                        numentry_change_value(item,
                                              item->numentry.value - 1);
                        return 1;
                }
                break;
        case SDLK_l:
                if (current_type == ITEM_PANBAR) {
                        item->panbar.muted = 0;
                        item->panbar.surround = 0;
                        numentry_change_value(item, 0);
                        return 1;
                }
                break;
        case SDLK_m:
                if (current_type == ITEM_PANBAR) {
                        item->panbar.muted = 0;
                        item->panbar.surround = 0;
                        numentry_change_value(item, 32);
                        return 1;
                }
                break;
        case SDLK_r:
                if (current_type == ITEM_PANBAR) {
                        item->panbar.muted = 0;
                        item->panbar.surround = 0;
                        numentry_change_value(item, 64);
                        return 1;
                }
                break;
        case SDLK_s:
                if (current_type == ITEM_PANBAR) {
                        item->panbar.muted = 0;
                        item->panbar.surround = 1;
                        if (item->changed)
                                item->changed();
                        status.flags |= NEED_UPDATE;
                        return 1;
                }
                break;
        default:
                /* this avoids a warning about all the values of an enum
                 * not being handled. (sheesh, it's already hundreds of
                 * lines long as it is!) */
                break;
        }

        /* if we're here, that mess didn't completely handle the key
         * (gosh...) so now here's another mess. */
        switch (current_type) {
        case ITEM_NUMENTRY:
                if (numentry_handle_digit(item, k->unicode))
                        return 1;
                break;
        case ITEM_THUMBBAR:
        case ITEM_PANBAR:
                if (thumbbar_prompt_value(item, k->unicode))
                        return 1;
                break;
        case ITEM_TEXTENTRY:
                if ((k->mod & (KMOD_CTRL | KMOD_ALT | KMOD_META)) == 0
                    && textentry_add_char(item, k->unicode))
                        return 1;
                break;
        default:
                break;
        }

        /* if we got down here the key wasn't handled */
        return 0;
}
