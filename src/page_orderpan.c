#include "headers.h"

#include <SDL.h>

#include "it.h"
#include "song.h"
#include "page.h"

/* --------------------------------------------------------------------- */

static struct item items_orderpan[65], items_ordervol[65];

static int top_order = 0;
static int current_order = 0;
static int orderlist_cursor_pos = 0;

/* --------------------------------------------------------------------- */

static void orderlist_reposition(void)
{
        if (current_order < top_order) {
                top_order = current_order;
        } else if (current_order > top_order + 31) {
                top_order = current_order - 31;
        }
}

/* --------------------------------------------------------------------- */

void update_current_order(void)
{
        char buf[4];

        draw_text(numtostr_3(current_order, buf), 12, 5, 5, 0);
        draw_text(numtostr_3(song_get_num_orders(), buf), 16, 5, 5, 0);
}


void set_current_order(int order)
{
        current_order = CLAMP(order, 0, 255);
        orderlist_reposition();

        status.flags |= NEED_UPDATE;
}

int get_current_order(void)
{
        return current_order;
}

/* --------------------------------------------------------------------- */
/* called from the pattern editor on ctrl-plus/minus */

void prev_order_pattern(void)
{
        int new_order = current_order - 1;
        int pattern;

        if (new_order < 0)
                new_order = 0;

        pattern = song_get_orderlist()[new_order];
        if (pattern < 200) {
                current_order = new_order;
                orderlist_reposition();
                set_current_pattern(pattern);
                //update_current_order();
        }
}

void next_order_pattern(void)
{
        int new_order = current_order + 1;
        int pattern;

        if (new_order > 255)
                new_order = 255;

        pattern = song_get_orderlist()[new_order];
        if (pattern < 200) {
                current_order = new_order;
                orderlist_reposition();
                set_current_pattern(pattern);
                //update_current_order();
        }
}

/* --------------------------------------------------------------------- */

static void get_pattern_string(unsigned char pattern, char *buf)
{
        switch (pattern) {
        case ORDER_SKIP:
                buf[0] = buf[1] = buf[2] = '+';
                buf[3] = 0;
                break;
        case ORDER_LAST:
                buf[0] = buf[1] = buf[2] = '-';
                buf[3] = 0;
                break;
        default:
                numtostr_3(pattern, buf);
                break;
        }
}

static void orderlist_draw(void)
{
        unsigned char *list = song_get_orderlist();
        char buf[4];
        int pos, n;
        int playing_order =
                (song_get_mode() ==
                 MODE_PLAYING ? song_get_current_order() : -1);

        SDL_LockSurface(screen);

        /* draw the list */
        for (pos = 0, n = top_order; pos < 32; pos++, n++) {
                draw_text_unlocked(numtostr_3(n, buf), 2, 15 + pos,
                                   (n == playing_order ? 3 : 0), 2);
                get_pattern_string(list[n], buf);
                draw_text_unlocked(buf, 6, 15 + pos, 2, 0);
        }

        /* draw the cursor */
        if (ACTIVE_PAGE.selected_item == 0) {
                get_pattern_string(list[current_order], buf);
                pos = current_order - top_order;
                draw_char_unlocked(buf[orderlist_cursor_pos],
                                   orderlist_cursor_pos + 6, 15 + pos, 0,
                                   3);
        }

        SDL_UnlockSurface(screen);

        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static void orderlist_insert_pos(void)
{
        unsigned char *list = song_get_orderlist();

        memmove(list + current_order + 1, list + current_order,
                255 - current_order);
        list[current_order] = ORDER_LAST;

        status.flags |= NEED_UPDATE;
}

static void orderlist_delete_pos(void)
{
        unsigned char *list = song_get_orderlist();

        memmove(list + current_order, list + current_order + 1,
                255 - current_order);
        list[255] = ORDER_LAST;

        status.flags |= NEED_UPDATE;
}

static void orderlist_insert_next(void)
{
        unsigned char *list = song_get_orderlist();
        int next_pattern;

        if (current_order == 0 || list[current_order - 1] > 199)
                return;
        next_pattern = list[current_order - 1] + 1;
        if (next_pattern > 199)
                next_pattern = 199;
        list[current_order] = next_pattern;
        if (current_order < 255)
                current_order++;
        orderlist_reposition();

        status.flags |= NEED_UPDATE;
}

static int orderlist_handle_char(Uint16 unicode)
{
        char c = unicode_to_ascii(unicode);
        int cur_pattern;
        int n[3] = { 0 };

        switch (c) {
        case '0'...'9':        /* GCC owns. :) */
                c -= '0';

                cur_pattern = song_get_orderlist()[current_order];
                if (cur_pattern < 200) {
                        n[0] = cur_pattern / 100;
                        n[1] = cur_pattern / 10 % 10;
                        n[2] = cur_pattern % 10;
                }

                n[orderlist_cursor_pos] = c;
                cur_pattern = n[0] * 100 + n[1] * 10 + n[2];
                cur_pattern = CLAMP(cur_pattern, 0, 199);
                song_get_orderlist()[current_order] = cur_pattern;
                break;
        case '+':
                song_get_orderlist()[current_order] = ORDER_SKIP;
                orderlist_cursor_pos = 2;
                break;
        case '-':
                song_get_orderlist()[current_order] = ORDER_LAST;
                orderlist_cursor_pos = 2;
                break;
        default:
                return 0;
        }

        if (orderlist_cursor_pos == 2) {
                if (current_order < 255)
                        current_order++;
                orderlist_cursor_pos = 0;
                orderlist_reposition();
        } else {
                orderlist_cursor_pos++;
        }

        status.flags |= NEED_UPDATE;

        return 1;
}

static int orderlist_handle_key_on_list(SDL_keysym * k)
{
        int prev_order = current_order;
        int new_order = prev_order;
        int new_cursor_pos = orderlist_cursor_pos;
        int n;

        switch (k->sym) {
        case SDLK_TAB:
                change_focus_to(1);
                return 1;
        case SDLK_LEFT:
                new_cursor_pos--;
                break;
        case SDLK_RIGHT:
                new_cursor_pos++;
                break;
        case SDLK_HOME:
                new_order = 0;
                break;
        case SDLK_END:
                new_order = song_get_num_orders();
                if (song_get_orderlist()[new_order] != ORDER_LAST)
                        new_order++;
                break;
        case SDLK_UP:
                new_order--;
                break;
        case SDLK_DOWN:
                new_order++;
                break;
        case SDLK_PAGEUP:
                new_order -= 16;
                break;
        case SDLK_PAGEDOWN:
                new_order += 16;
                break;
        case SDLK_INSERT:
                orderlist_insert_pos();
                return 1;
        case SDLK_DELETE:
                orderlist_delete_pos();
                return 1;
        case SDLK_n:
                orderlist_insert_next();
                return 1;
        case SDLK_g:
                n = song_get_orderlist()[new_order];
                if (n < 200) {
                        set_current_pattern(n);
                        set_page(PAGE_PATTERN_EDITOR);
                }
                return 1;
        default:
                if ((k->mod & (KMOD_CTRL | KMOD_ALT | KMOD_META)) == 0) {
                        return orderlist_handle_char(k->unicode);
                }
                return 0;
        }

        if (new_cursor_pos < 0)
                new_cursor_pos = 2;
        else if (new_cursor_pos > 2)
                new_cursor_pos = 0;

        if (new_order != prev_order) {
                current_order = CLAMP(new_order, 0, 255);
                orderlist_reposition();
        } else if (new_cursor_pos != orderlist_cursor_pos) {
                orderlist_cursor_pos = new_cursor_pos;
        } else {
                return 0;
        }

        status.flags |= NEED_UPDATE;
        return 1;
}

/* --------------------------------------------------------------------- */

static void order_pan_vol_draw_const(void)
{
        draw_box(5, 14, 9, 47, BOX_THICK | BOX_INNER | BOX_INSET);

        draw_box(30, 14, 40, 47, BOX_THICK | BOX_INNER | BOX_FLAT_LIGHT);
        draw_box(64, 14, 74, 47, BOX_THICK | BOX_INNER | BOX_FLAT_LIGHT);

        draw_char(146, 30, 14, 3, 2);
        draw_char(145, 40, 14, 3, 2);

        draw_char(146, 64, 14, 3, 2);
        draw_char(145, 74, 14, 3, 2);
}

static void orderpan_draw_const(void)
{
        order_pan_vol_draw_const();
        draw_text("L   M   R", 31, 14, 0, 3);
        draw_text("L   M   R", 65, 14, 0, 3);
}

static void ordervol_draw_const(void)
{
        int n;
        char buf[16] = "Channel 42";

        order_pan_vol_draw_const();

        SDL_LockSurface(screen);

        draw_text(" Volumes ", 31, 14, 0, 3);
        draw_text(" Volumes ", 65, 14, 0, 3);

        for (n = 1; n <= 32; n++) {
                numtostr_2(n, buf + 8);
                draw_text(buf, 20, 14 + n, 0, 2);

                numtostr_2(n + 32, buf + 8);
                draw_text(buf, 54, 14 + n, 0, 2);
        }

        SDL_UnlockSurface(screen);
}

/* --------------------------------------------------------------------- */

static void order_pan_vol_playback_update(void)
{
        static int last_order = -1;
        int order =
                ((song_get_mode() ==
                  MODE_STOPPED) ? -1 : song_get_current_order());

        if (order != last_order) {
                last_order = order;
                status.flags |= NEED_UPDATE;
        }
}

/* --------------------------------------------------------------------- */

static void orderpan_update_values_in_song(void)
{
        song_channel *chn;
        int n;

        for (n = 0; n < 64; n++) {
                chn = song_get_channel(n);

                /* yet another modplug hack here! */
                chn->panning = items_orderpan[n + 1].panbar.value * 4;

                if (items_orderpan[n + 1].panbar.surround) {
                        chn->flags |= CHN_SURROUND;
                } else {
                        chn->flags &= ~CHN_SURROUND;
                }

                song_set_channel_mute(n,
                                      items_orderpan[n + 1].panbar.muted);
        }
}

static void ordervol_update_values_in_song(void)
{
        int n;

        for (n = 0; n < 64; n++)
                song_get_channel(n)->volume =
                        items_ordervol[n + 1].thumbbar.value;
}

/* called when a channel is muted/unmuted by means other than the panning
 * page (alt-f10 in the pattern editor, space on the info page...) */
void orderpan_recheck_muted_channels(void)
{
        int n;
        for (n = 0; n < 64; n++)
                items_orderpan[n + 1].panbar.muted =
                        !!(song_get_channel(n)->flags & CHN_MUTE);

        if (status.current_page == PAGE_ORDERLIST_PANNING)
                status.flags |= NEED_UPDATE;
}

static void order_pan_vol_song_changed_cb(void)
{
        int n;
        song_channel *chn;

        for (n = 0; n < 64; n++) {
                chn = song_get_channel(n);
                items_orderpan[n + 1].panbar.value = chn->panning / 4;
                items_orderpan[n + 1].panbar.surround =
                        !!(chn->flags & CHN_SURROUND);
                items_orderpan[n + 1].panbar.muted =
                        !!(chn->flags & CHN_MUTE);

                items_ordervol[n + 1].thumbbar.value = chn->volume;
        }
}

/* --------------------------------------------------------------------- */

static void order_pan_vol_handle_key(SDL_keysym * k)
{
        int n = *selected_item;

        switch (k->sym) {
        case SDLK_PAGEDOWN:
                n += 8;
                break;
        case SDLK_PAGEUP:
                n -= 8;
                break;
        default:
                return;
        }

        n = CLAMP(n, 1, 64);
        if (*selected_item != n)
                change_focus_to(n);
}

/* --------------------------------------------------------------------- */

void orderpan_load_page(struct page *page)
{
        int n;

        page->title = "Order List and Panning (F11)";
        page->draw_const = orderpan_draw_const;
        /* this does the work for both pages */
        page->song_changed_cb = order_pan_vol_song_changed_cb;
        page->playback_update = order_pan_vol_playback_update;
        page->handle_key = order_pan_vol_handle_key;
        page->total_items = 65;
        page->items = items_orderpan;
        page->help_index = HELP_ORDERLIST_PANNING;

        /* 0 = order list */
        items_orderpan[0].type = ITEM_OTHER;
        items_orderpan[0].next.tab = 1;
        items_orderpan[0].other.handle_key = orderlist_handle_key_on_list;
        items_orderpan[0].other.redraw = orderlist_draw;

        /* 1-64 = panbars */
        create_panbar(items_orderpan + 1, 20, 15, 1, 2, 33,
                      orderpan_update_values_in_song, 1);
        for (n = 2; n <= 32; n++) {
                create_panbar(items_orderpan + n, 20, 14 + n, n - 1, n + 1,
                              n + 32, orderpan_update_values_in_song, n);
                create_panbar(items_orderpan + n + 31, 54, 13 + n, n + 30,
                              n + 32, 0, orderpan_update_values_in_song,
                              n + 31);
        }
        create_panbar(items_orderpan + 64, 54, 46, 63, 64, 0,
                      orderpan_update_values_in_song, 64);
}

void ordervol_load_page(struct page *page)
{
        int n;

        page->title = "Order List and Channel Volume (F11)";
        page->draw_const = ordervol_draw_const;
        page->playback_update = order_pan_vol_playback_update;
        page->handle_key = order_pan_vol_handle_key;
        page->total_items = 65;
        page->items = items_ordervol;
        page->help_index = HELP_ORDERLIST_VOLUME;

        /* 0 = order list */
        items_ordervol[0].type = ITEM_OTHER;
        items_ordervol[0].next.tab = 1;
        items_ordervol[0].other.handle_key = orderlist_handle_key_on_list;
        items_ordervol[0].other.redraw = orderlist_draw;

        /* 1-64 = thumbbars */
        create_thumbbar(items_ordervol + 1, 31, 15, 9, 1, 2, 33,
                        ordervol_update_values_in_song, 0, 64);
        for (n = 2; n <= 32; n++) {
                create_thumbbar(items_ordervol + n, 31, 14 + n, 9, n - 1,
                                n + 1, n + 32,
                                ordervol_update_values_in_song, 0, 64);
                create_thumbbar(items_ordervol + n + 31, 65, 13 + n, 9,
                                n + 30, n + 32, 0,
                                ordervol_update_values_in_song, 0, 64);
        }
        create_thumbbar(items_ordervol + 64, 65, 46, 9, 63, 64, 0,
                        ordervol_update_values_in_song, 0, 64);
}
