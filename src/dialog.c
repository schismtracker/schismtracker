#include "headers.h"

#include <SDL.h>

#include "it.h"
#include "song.h"
#include "page.h"

/* --------------------------------------------------------------------- */

/* ENSURE_DIALOG(optional return value)
 * will emit a warning and cause the function to return
 * if a dialog is not active. */
#ifndef NDEBUG
# define ENSURE_DIALOG(...) G_STMT_START {\
        if ((status.dialog_type & DIALOG_BOX) == 0) {\
                fprintf(stderr, "%s called with no dialog\n", __FUNCTION__);\
                return __VA_ARGS__;\
        }\
} G_STMT_END
#else
# define ENSURE_DIALOG(...)
#endif

/* --------------------------------------------------------------------- */
/* I'm only supporting four dialogs open at a time. This is an absurdly
 * large amount anyway, since the most that should ever be displayed is
 * two (in the case of a custom dialog with a thumbbar, the value prompt
 * dialog will be open on top of the other dialog). */

static struct dialog dialogs[4];
static int num_dialogs = 0;

/* --------------------------------------------------------------------- */

void dialog_draw(void)
{
        int n, d;

        for (d = 0; d < num_dialogs; d++) {
                n = dialogs[d].total_items;

                // draw the border and background
                draw_box(dialogs[d].x, dialogs[d].y,
                         dialogs[d].x + dialogs[d].w - 1,
                         dialogs[d].y + dialogs[d].h - 1,
                         BOX_THICK | BOX_OUTER | BOX_FLAT_LIGHT);
                draw_fill_chars(dialogs[d].x + 1, dialogs[d].y + 1,
                                dialogs[d].x + dialogs[d].w - 2,
                                dialogs[d].y + dialogs[d].h - 2, 2);

                // then the rest of the stuff
                RUN_IF(dialogs[d].draw_const);

                if (dialogs[d].text)
                        draw_text(dialogs[d].text, dialogs[d].text_x, 27,
                                  0, 2);

                n = dialogs[d].total_items;
                while (n) {
                        n--;
                        draw_item(dialogs[d].items + n,
                                  n == dialogs[d].selected_item);
                }
        }
}

/* --------------------------------------------------------------------- */

void dialog_destroy(void)
{
        int d;

        if (num_dialogs == 0)
                return;

        d = num_dialogs - 1;

        if (dialogs[d].type != DIALOG_CUSTOM) {
                free(dialogs[d].text);
                free(dialogs[d].items);
        }

        num_dialogs--;
        if (num_dialogs) {
                d--;
                items = dialogs[d].items;
                selected_item = &(dialogs[d].selected_item);
                total_items = &(dialogs[d].total_items);
                status.dialog_type = dialogs[d].type;
        } else {
                items = ACTIVE_PAGE.items;
                selected_item = &(ACTIVE_PAGE.selected_item);
                total_items = &(ACTIVE_PAGE.total_items);
                status.dialog_type = DIALOG_NONE;
        }

        /* it's up to the calling function to redraw the page */
}

void dialog_destroy_all(void)
{
        while (num_dialogs)
                dialog_destroy();
}

/* --------------------------------------------------------------------- */

static void dialog_yes(void)
{
        ENSURE_DIALOG();

        RUN_IF(dialogs[num_dialogs - 1].action_yes);
        dialog_destroy();
        status.flags |= NEED_UPDATE;
}

static void dialog_no(void)
{
        ENSURE_DIALOG();

        RUN_IF(dialogs[num_dialogs - 1].action_no);
        dialog_destroy();
        status.flags |= NEED_UPDATE;
}

static void dialog_cancel(void)
{
        ENSURE_DIALOG();

        /* RUN_IF(dialogs[num_dialogs - 1].action_cancel); */
        dialog_destroy();
        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

int dialog_handle_key(SDL_keysym * k)
{
        ENSURE_DIALOG(0);

        switch (k->sym) {
        case SDLK_y:
                switch (status.dialog_type) {
                case DIALOG_YES_NO:
                case DIALOG_OK_CANCEL:
                        dialog_yes();
                        return 1;
                default:
                        break;
                }
                break;
        case SDLK_n:
                switch (status.dialog_type) {
                case DIALOG_YES_NO:
                        dialog_no();
                        return 1;
                case DIALOG_OK_CANCEL:
                        dialog_cancel();
                        return 1;
                default:
                        break;
                }
                break;
        case SDLK_ESCAPE:
                dialog_cancel();
                return 1;
        default:
                break;
        }

        return 0;
}

/* --------------------------------------------------------------------- */
/* these get called from dialog_create below */

static void dialog_create_ok(int textlen)
{
        int d = num_dialogs;

        /* make the dialog as wide as either the ok button or the text,
         * whichever is more */
        dialogs[d].text_x = 40 - (textlen / 2);
        if (textlen > 21) {
                dialogs[d].x = dialogs[d].text_x - 2;
                dialogs[d].w = textlen + 4;
        } else {
                dialogs[d].x = 26;
                dialogs[d].w = 29;
        }
        dialogs[d].h = 8;
        dialogs[d].y = 25;

        dialogs[d].items = malloc(sizeof(struct item));
        dialogs[d].total_items = 1;

        create_button(dialogs[d].items + 0, 36, 30, 6, 0, 0, 0, 0, 0,
                      dialog_yes, "OK", 3);
}

static void dialog_create_ok_cancel(int textlen)
{
        int d = num_dialogs;

        /* the ok/cancel buttons (with the borders and all) are 21 chars,
         * so if the text is shorter, it needs a bit of padding. */
        dialogs[d].text_x = 40 - (textlen / 2);
        if (textlen > 21) {
                dialogs[d].x = dialogs[d].text_x - 4;
                dialogs[d].w = textlen + 8;
        } else {
                dialogs[d].x = 26;
                dialogs[d].w = 29;
        }
        dialogs[d].h = 8;
        dialogs[d].y = 25;

        dialogs[d].items = calloc(2, sizeof(struct item));
        dialogs[d].total_items = 2;

        create_button(dialogs[d].items + 0, 31, 30, 6, 0, 0, 1, 1, 1,
                      dialog_yes, "OK", 3);
        create_button(dialogs[d].items + 1, 42, 30, 6, 1, 1, 0, 0, 0,
                      dialog_cancel, "Cancel", 1);
}

static void dialog_create_yes_no(int textlen)
{
        int d = num_dialogs;

        dialogs[d].text_x = 40 - (textlen / 2);
        if (textlen > 21) {
                dialogs[d].x = dialogs[d].text_x - 4;
                dialogs[d].w = textlen + 8;
        } else {
                dialogs[d].x = 26;
                dialogs[d].w = 29;
        }
        dialogs[d].h = 8;
        dialogs[d].y = 25;

        dialogs[d].items = calloc(2, sizeof(struct item));
        dialogs[d].total_items = 2;

        create_button(dialogs[d].items + 0, 30, 30, 7, 0, 0, 1, 1, 1,
                      dialog_yes, "Yes", 3);
        create_button(dialogs[d].items + 1, 42, 30, 6, 1, 1, 0, 0, 0,
                      dialog_no, "No", 3);
}

/* --------------------------------------------------------------------- */
/* type can be DIALOG_OK, DIALOG_OK_CANCEL, or DIALOG_YES_NO
 * default_item: 0 = ok/yes, 1 = cancel/no */

void dialog_create(int type, const char *text, void (*action_yes) (void),
                   void (*action_no) (void), int default_item)
{
        int textlen = strlen(text);
        int d = num_dialogs;

#ifndef NDEBUG
        if ((type & DIALOG_BOX) == 0) {
                fprintf(stderr,
                        "dialog_create called with bogus dialog type %d\n",
                        type);
                return;
        }
#endif

        /* FIXME | hmm... a menu should probably be hiding itself when an
         * FIXME | item gets selected. */
        if (status.dialog_type & DIALOG_MENU)
                menu_hide();

        dialogs[d].text = strdup(text);
        dialogs[d].action_yes = action_yes;
        dialogs[d].action_no = action_no;
        dialogs[d].selected_item = default_item;
        dialogs[d].draw_const = NULL;
        dialogs[d].handle_key = NULL;

        switch (type) {
        case DIALOG_OK:
                dialog_create_ok(textlen);
                break;
        case DIALOG_OK_CANCEL:
                dialog_create_ok_cancel(textlen);
                break;
        case DIALOG_YES_NO:
                dialog_create_yes_no(textlen);
                break;
        default:
#ifndef NDEBUG
                fprintf(stderr, "this man should not be seen\n");
#endif
                type = DIALOG_OK_CANCEL;
                dialog_create_ok_cancel(textlen);
                break;
        }

        dialogs[d].type = type;
        items = dialogs[d].items;
        selected_item = &(dialogs[d].selected_item);
        total_items = &(dialogs[d].total_items);

        num_dialogs++;

        status.dialog_type = type;
        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

struct dialog *dialog_create_custom(void)
{
        int d = num_dialogs;

        dialogs[d].type = DIALOG_CUSTOM;
        dialogs[d].text = NULL;
        dialogs[d].action_yes = NULL;
        dialogs[d].action_no = NULL;

        return dialogs + d;
}

void dialog_display_custom(void)
{
        int d = num_dialogs;

        status.dialog_type = DIALOG_CUSTOM;

        items = dialogs[d].items;
        selected_item = &(dialogs[d].selected_item);
        total_items = &(dialogs[d].total_items);

        num_dialogs++;

        status.flags |= NEED_UPDATE;
}
