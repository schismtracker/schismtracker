#include "headers.h"

#include <SDL.h>

#include "it.h"
#include "page.h"

/* --------------------------------------------------------------------- */

static struct item items_palette[49];

static int selected_palette, max_palette = 0;

/* --------------------------------------------------------------------- */
/*
 * This is actually wrong. For some reason the boxes around the little
 * color swatches are drawn with the top right and bottom left corners
 * in color 3 instead of color 1 like all the other thick boxes have.
 * I'm going to leave it this way, though -- it's far more likely that
 * someone will comment on, say, my completely changing the preset
 * switcher than about the corners having different colors :)
 * 
 * (Another discrepancy: seems that Impulse Tracker draws the thumbbars
 * with a "fake" range of 0-64, because it never gets drawn at the far
 * right. Oh well.) */

static void palette_draw_const(void)
{
        int n;

        draw_text("Predefined Palettes", 57, 25, 0, 2);

        for (n = 0; n < 7; n++) {
                SDL_LockSurface(screen);
                draw_box_unlocked(2, 13 + (5 * n), 8, 17 + (5 * n),
                                  BOX_THICK | BOX_INNER | BOX_INSET);
                draw_box_unlocked(9, 13 + (5 * n), 19, 17 + (5 * n),
                                  BOX_THICK | BOX_INNER | BOX_INSET);
                draw_box_unlocked(29, 13 + (5 * n), 35, 17 + (5 * n),
                                  BOX_THICK | BOX_INNER | BOX_INSET);
                draw_box_unlocked(36, 13 + (5 * n), 46, 17 + (5 * n),
                                  BOX_THICK | BOX_INNER | BOX_INSET);
                SDL_UnlockSurface(screen);
                draw_fill_chars(3, 14 + (5 * n), 7, 16 + (5 * n), n);
                draw_fill_chars(30, 14 + (5 * n), 34, 16 + (5 * n), n + 7);
        }

        SDL_LockSurface(screen);
        draw_box_unlocked(56, 13, 62, 17,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(63, 13, 73, 17,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(56, 18, 62, 22,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(63, 18, 73, 22,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(55, 26, 77, 47,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        SDL_UnlockSurface(screen);

        draw_fill_chars(57, 14, 61, 16, 14);
        draw_fill_chars(57, 19, 61, 21, 15);
}

/* --------------------------------------------------------------------- */

static void update_thumbbars(void)
{
        int n;

        for (n = 0; n < 16; n++) {
                items_palette[3 * n].thumbbar.value =
                        palettes[current_palette].colors[n][0];
                items_palette[3 * n + 1].thumbbar.value =
                        palettes[current_palette].colors[n][1];
                items_palette[3 * n + 2].thumbbar.value =
                        palettes[current_palette].colors[n][2];
        }
}

/* --------------------------------------------------------------------- */

static void palette_list_draw(void)
{
        int n, focused = (ACTIVE_PAGE.selected_item == 48);

        draw_fill_chars(56, 27, 76, 46, 0);
        for (n = 0; n < 20 && palettes[n].name[0]; n++) {
                int fg = 6, bg = 0;
                if (focused && n == selected_palette) {
                        fg = 0;
                        bg = 3;
                } else if (n == selected_palette) {
                        bg = 14;
                }
                draw_text_len(palettes[n].name, 21, 56, 27 + n, fg, bg);
        }
        max_palette = n;
}

static int palette_list_handle_key_on_list(SDL_keysym * k)
{
        int new_palette = selected_palette;
        const int focus_offsets[] =
                { 0, 1, 1, 2, 3, 3, 4, 4, 5, 6, 6, 7, 7, 8, 9, 9, 10, 10,
    11, 12 };

        switch (k->sym) {
        case SDLK_UP:
                if (--new_palette < 0) {
                        change_focus_to(47);
                        return 1;
                }
                break;
        case SDLK_DOWN:
                new_palette++;
                break;
        case SDLK_HOME:
                new_palette = 0;
                break;
        case SDLK_PAGEUP:
                if (new_palette == 0) {
                        change_focus_to(45);
                        return 1;
                }
                new_palette -= 16;
                break;
        case SDLK_END:
                new_palette = max_palette - 1;
                break;
        case SDLK_PAGEDOWN:
                new_palette += 16;
                break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
                palette_load_preset(selected_palette);
                update_thumbbars();
                status.flags |= NEED_UPDATE;
                return 1;
        case SDLK_RIGHT:
        case SDLK_TAB:
                change_focus_to(focus_offsets[selected_palette] + 8);
                return 1;
        case SDLK_LEFT:
                change_focus_to(focus_offsets[selected_palette] + 29);
                return 1;
        default:
                return 0;
        }

        new_palette = CLAMP(new_palette, 0, max_palette - 1);
        if (new_palette != selected_palette) {
                selected_palette = new_palette;
                status.flags |= NEED_UPDATE;
        }

        return 1;
}

/* --------------------------------------------------------------------- */

static void palette_list_handle_key(SDL_keysym * k)
{
        int n = *selected_item;

        switch (k->sym) {
        case SDLK_PAGEUP:
                n -= 3;
                break;
        case SDLK_PAGEDOWN:
                n += 3;
                break;
        default:
                return;
        }

        if (status.flags & CLASSIC_MODE) {
                if (n < 0)
                        return;
                if (n > 48)
                        n = 48;
        } else {
                n = CLAMP(n, 0, 48);
        }
        if (n != *selected_item)
                change_focus_to(n);
}

/* --------------------------------------------------------------------- */

static void update_palette(void)
{
        int n;
        byte colors[16][3];

        for (n = 0; n < 16; n++) {
                colors[n][0] = items_palette[3 * n].thumbbar.value;
                colors[n][1] = items_palette[3 * n + 1].thumbbar.value;
                colors[n][2] = items_palette[3 * n + 2].thumbbar.value;
        }
        palette_set(colors);
        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

void palette_load_page(struct page *page)
{
        int n;

        page->title = "Palette Configuration (Ctrl-F12)";
        page->draw_const = palette_draw_const;
        page->handle_key = palette_list_handle_key;
        page->total_items = 49;
        page->items = items_palette;
        page->help_index = HELP_GLOBAL;

        selected_palette = current_palette;

        for (n = 0; n < 16; n++) {
                int tabs[3] = { 3 * n + 21, 3 * n + 22, 3 * n + 23 };
                if (n >= 9 && n <= 13) {
                        tabs[0] = tabs[1] = tabs[2] = 48;
                } else if (n > 13) {
                        tabs[0] = 3 * n - 42;
                        tabs[1] = 3 * n - 41;
                        tabs[2] = 3 * n - 40;
                }
                create_thumbbar(items_palette + (3 * n), 10 + 27 * (n / 7),
                                5 * (n % 7) + 14, 9, n ? (3 * n - 1) : 0,
                                3 * n + 1, tabs[0], update_palette, 0, 63);
                create_thumbbar(items_palette + (3 * n + 1),
                                10 + 27 * (n / 7), 5 * (n % 7) + 15, 9,
                                3 * n, 3 * n + 2, tabs[1], update_palette,
                                0, 63);
                create_thumbbar(items_palette + (3 * n + 2),
                                10 + 27 * (n / 7), 5 * (n % 7) + 16, 9,
                                3 * n + 1, 3 * n + 3, tabs[2],
                                update_palette, 0, 63);
                items_palette[3 * n].thumbbar.value =
                        palettes[current_palette].colors[n][0];
                items_palette[3 * n + 1].thumbbar.value =
                        palettes[current_palette].colors[n][1];
                items_palette[3 * n + 2].thumbbar.value =
                        palettes[current_palette].colors[n][2];
        }

        items_palette[48].type = ITEM_OTHER;
        items_palette[48].other.handle_key =
                palette_list_handle_key_on_list;
        items_palette[48].other.redraw = palette_list_draw;
}
