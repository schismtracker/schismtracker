#include "headers.h"

#include <SDL.h>

#include "it.h"
#include "song.h"
#include "page.h"

/* --------------------------------------------------------------------- */

/* ENSURE_MENU(optional return value)
 * will emit a warning and cause the function to return
 * if a menu is not active. */
#ifndef NDEBUG
# define ENSURE_MENU(...) G_STMT_START {\
        if ((status.dialog_type & DIALOG_MENU) == 0) {\
                fprintf(stderr, "%s called with no menu\n", __FUNCTION__);\
                return __VA_ARGS__;\
        }\
} G_STMT_END
#else
# define ENSURE_MENU(...)
#endif

/* --------------------------------------------------------------------- */
/* protomatypes */

static void main_menu_selected_cb(void);
static void file_menu_selected_cb(void);
static void playback_menu_selected_cb(void);
static void sample_menu_selected_cb(void);
static void instrument_menu_selected_cb(void);

/* --------------------------------------------------------------------- */

struct menu {
        int x, y, w;
        const char *title;
        int num_items;  /* meh... */
        const char *items[14];  /* writing **items doesn't work here :( */
        int selected_item;      /* the highlighted item */
        int active_item;        /* "pressed" menu item, for submenus */
        void (*selected_cb) (void);     /* triggered by return key */
};

/* *INDENT-OFF* */
static struct menu main_menu = {
        x: 6, y: 14, w: 25, title: " Main Menu",
        num_items: 9, {
                "File Menu...",
                "Playback Menu...",
                "View Patterns        (F2)",
                "Sample Menu...",
                "Instrument Menu...",
                "View Orders/Panning (F11)",
                "View Variables      (F12)",
                "Message Editor (Shift-F9)",
                "Help!                (F1)",
        }, selected_item: 0, active_item: -1,
        main_menu_selected_cb
};

static struct menu file_menu = {
        x: 25, y: 16, w: 22, title: "File Menu",
        num_items: 6, {
                "Load...           (F9)",
                "New...        (Ctrl-N)",
                "Save Current  (Ctrl-S)",
                "Save As...       (F10)",
                "Shell to DOS  (Ctrl-D)",
                "Quit          (Ctrl-Q)",
        }, selected_item: 0, active_item: -1,
        file_menu_selected_cb
};

static struct menu playback_menu = {
        x: 25, y: 16, w: 27, title: " Playback Menu",
        num_items: 9, {
                "Show Infopage          (F5)",
                "Play Song         (Ctrl-F5)",
                "Play Pattern           (F6)",
                "Play from Order  (Shift-F6)",
                "Play from Mark/Cursor  (F7)",
                "Stop                   (F8)",
                "Reinit Soundcard   (Ctrl-I)",
                "Driver Screen    (Shift-F5)",
                "Calculate Length   (Ctrl-P)",
        }, selected_item: 0, active_item: -1,
        playback_menu_selected_cb
};

static struct menu sample_menu = {
        x: 25, y: 23, w: 25, title: "Sample Menu",
        num_items: 3, {
                "Sample List          (F3)",
                "Sample Library  (Ctrl-F3)",
                "Reload Soundcard (Ctrl-G)",
        }, selected_item: 0, active_item: -1,
        sample_menu_selected_cb
};

static struct menu instrument_menu = {
        x: 20, y: 23, w: 29, title: "Instrument Menu",
        num_items: 2, {
                "Instrument List          (F4)",
                "Instrument Library  (Ctrl-F4)",
        }, selected_item: 0, active_item: -1,
        instrument_menu_selected_cb
};
/* *INDENT-ON* */

/* updated to whatever menu is currently active.
 * this generalizes the key handling.
 * if status.dialog_type == DIALOG_SUBMENU, use current_menu[1]
 * else, use current_menu[0] */
static struct menu *current_menu[2] = { NULL, NULL };

/* --------------------------------------------------------------------- */

static void _draw_menu(struct menu *menu)
{
        int h = 6, n = menu->num_items;

        while (n--) {
                draw_box(2 + menu->x, 4 + menu->y + 3 * n,
                         5 + menu->x + menu->w, 6 + menu->y + 3 * n,
                         BOX_THIN | BOX_CORNER | (n ==
                                                  menu->
                                                  active_item ? BOX_INSET :
                                                  BOX_OUTSET));

                draw_text_len(menu->items[n], menu->w, 4 + menu->x,
                              5 + menu->y + 3 * n,
                              (n == menu->selected_item ? 3 : 0), 2);

                draw_char(0, 3 + menu->x, 5 + menu->y + 3 * n, 0, 2);
                draw_char(0, 4 + menu->x + menu->w, 5 + menu->y + 3 * n, 0,
                          2);

                h += 3;
        }

        draw_box(menu->x, menu->y, menu->x + menu->w + 7, menu->y + h - 1,
                 BOX_THICK | BOX_OUTER | BOX_FLAT_LIGHT);
        draw_box(menu->x + 1, menu->y + 1, menu->x + menu->w + 6,
                 menu->y + h - 2, BOX_THIN | BOX_OUTER | BOX_FLAT_DARK);
        draw_fill_chars(menu->x + 2, menu->y + 2, menu->x + menu->w + 5,
                        menu->y + 3, 2);
        draw_text(menu->title, menu->x + 6, menu->y + 2, 3, 2);
}

inline void menu_draw(void)
{
        ENSURE_MENU();

        _draw_menu(current_menu[0]);
        if (current_menu[1])
                _draw_menu(current_menu[1]);
}

/* --------------------------------------------------------------------- */

inline void menu_show(void)
{
        status.dialog_type = DIALOG_MAIN_MENU;
        current_menu[0] = &main_menu;

        status.flags |= NEED_UPDATE;
}

inline void menu_hide(void)
{
        ENSURE_MENU();

        status.dialog_type = DIALOG_NONE;

        /* "unpress" the menu items */
        current_menu[0]->active_item = -1;
        if (current_menu[1])
                current_menu[1]->active_item = -1;

        current_menu[0] = current_menu[1] = NULL;

        /* note! this does NOT redraw the screen; that's up to the caller.
         * the reason for this is that so many of the menu items cause a
         * page switch, and redrawing the current page followed by
         * redrawing a new page is redundant. */
}

/* --------------------------------------------------------------------- */

static inline void set_submenu(struct menu *menu)
{
        ENSURE_MENU();

        status.dialog_type = DIALOG_SUBMENU;
        main_menu.active_item = main_menu.selected_item;
        current_menu[1] = menu;

        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* callbacks */

static void main_menu_selected_cb(void)
{
        switch (main_menu.selected_item) {
        case 0:
                set_submenu(&file_menu);
                break;
        case 1:
                set_submenu(&playback_menu);
                break;
        case 2:
                set_page(PAGE_PATTERN_EDITOR);
                break;
        case 3:
                set_submenu(&sample_menu);
                break;
        case 4:
                set_submenu(&instrument_menu);
                break;
        case 5:
                set_page(PAGE_ORDERLIST_PANNING);
                break;
        case 6:
                set_page(PAGE_SONG_VARIABLES);
                break;
        case 7:
                set_page(PAGE_MESSAGE);
                break;
        case 8:
                set_page(PAGE_HELP);
                break;
        }
}

static void file_menu_selected_cb(void)
{
        switch (file_menu.selected_item) {
        case 0:
                set_page(PAGE_LOAD_MODULE);
                return;
        case 4:
                /* shell to dos: well, can't really shell with no dos :) */
                if (status.flags & WM_AVAILABLE) {
                        /* tell the window manager to minimize? dunno. */
                } else {
                        /* open a new console? */
                }
                break;
        case 5:
                show_exit_prompt();
                return;
        default:
                printf("selected: %d\n", file_menu.selected_item);
                break;
        }

        menu_hide();
        status.flags |= NEED_UPDATE;
}

static void playback_menu_selected_cb(void)
{
        switch (playback_menu.selected_item) {
        case 0:
                if (song_get_mode() == MODE_STOPPED)
                        song_start();
                set_page(PAGE_INFO);
                return;
        case 1:
                song_start();
                break;
        case 2:
                song_loop_pattern(get_current_pattern(), 0);
                break;
        case 3:
                song_start_at_order(get_current_order(), 0);
                break;
        case 4:
                play_song_from_mark();
                break;
        case 5:
                song_stop();
                break;
        case 8:
                show_song_length();
                return;
        default:
                printf("selected: %d\n", playback_menu.selected_item);
                break;
        }

        menu_hide();
        status.flags |= NEED_UPDATE;
}

static void sample_menu_selected_cb(void)
{
        switch (sample_menu.selected_item) {
        case 0:
                set_page(PAGE_SAMPLE_LIST);
                return;
        default:
                printf("selected: %d\n", sample_menu.selected_item);
                break;
        }

        menu_hide();
        status.flags |= NEED_UPDATE;
}

static void instrument_menu_selected_cb(void)
{
        switch (instrument_menu.selected_item) {
        case 0:
                set_page(PAGE_INSTRUMENT_LIST);
                return;
        default:
                printf("selected: %d\n", instrument_menu.selected_item);
                break;
        }

        menu_hide();
        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

void menu_handle_key(SDL_keysym * k)
{
        struct menu *menu =
                (status.dialog_type ==
                 DIALOG_SUBMENU ? current_menu[1] : current_menu[0]);

        ENSURE_MENU();

        switch (k->sym) {
        case SDLK_ESCAPE:
                current_menu[1] = NULL;
                if (status.dialog_type == DIALOG_SUBMENU) {
                        status.dialog_type = DIALOG_MAIN_MENU;
                        main_menu.active_item = -1;
                } else {
                        menu_hide();
                }
                break;
        case SDLK_UP:
                if (menu->selected_item > 0) {
                        menu->selected_item--;
                        break;
                }
                return;
        case SDLK_DOWN:
                if (menu->selected_item < menu->num_items - 1) {
                        menu->selected_item++;
                        break;
                }
                return;
                /* home/end are new here :) */
        case SDLK_HOME:
                menu->selected_item = 0;
                break;
        case SDLK_END:
                menu->selected_item = menu->num_items - 1;
                break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
                menu->selected_cb();
                return;
        default:
                return;
        }

        status.flags |= NEED_UPDATE;
}
