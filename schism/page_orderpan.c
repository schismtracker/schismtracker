/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
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
#include "song.h"
#include "page.h"
#include "widget.h"
#include "vgamem.h"

#include "sdlmain.h"

/* --------------------------------------------------------------------- */

static struct widget widgets_orderpan[65], widgets_ordervol[65];

static int top_order = 0;
static int current_order = 0;
static int orderlist_cursor_pos = 0;

static unsigned char saved_orderlist[256];
static int _did_save_orderlist = 0;

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

	draw_text(str_from_num(3, current_order, buf), 12, 5, 5, 0);
	draw_text(str_from_num(3, csf_last_order(current_song), buf), 16, 5, 5, 0);
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
	int new_order = current_order;
	int last_pattern = current_song->orderlist[new_order];

	do {
		if (--new_order < 0) {
			new_order = 0;
			break;
		}
	} while (!(status.flags & CLASSIC_MODE)
			&& last_pattern == current_song->orderlist[new_order]
			&& current_song->orderlist[new_order] == ORDER_SKIP);

	if (current_song->orderlist[new_order] < 200) {
		current_order = new_order;
		orderlist_reposition();
		set_current_pattern(current_song->orderlist[new_order]);
	}
}

void next_order_pattern(void)
{
	int new_order = current_order;
	int last_pattern = current_song->orderlist[new_order];

	do {
		if (++new_order > 255) {
			new_order = 255;
			break;
		}
	} while (!(status.flags & CLASSIC_MODE)
			&& last_pattern == current_song->orderlist[new_order]
			&&  current_song->orderlist[new_order] == ORDER_SKIP);

	if (current_song->orderlist[new_order] < 200) {
		current_order = new_order;
		orderlist_reposition();
		set_current_pattern(current_song->orderlist[new_order]);
	}
}

static void orderlist_cheater(void)
{
	song_note_t *data;
	int cp, i, best, first;
	int rows;

	if (current_song->orderlist[current_order] != ORDER_SKIP
	    && current_song->orderlist[current_order] != ORDER_LAST) {
		return;
	}
	cp = get_current_pattern();
	best = first = -1;
	for (i = 0; i < 199; i++) {
		if (csf_pattern_is_empty(current_song, i)) {
			if (first == -1) first = i;
			if (best == -1) best = i;
		} else {
			best = -1;
		}
	}
	if (best == -1) best = first;
	if (best == -1) return;

	status_text_flash("Pattern %d copied to pattern %d, order %d", cp, best, current_order);

	data = song_pattern_allocate_copy(cp, &rows);
	song_pattern_resize(best, rows);
	song_pattern_install(best, data, rows);
	current_song->orderlist[current_order] = best;
	current_order++;
	status.flags |= SONG_NEEDS_SAVE;
	status.flags |= NEED_UPDATE;
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
		str_from_num(3, pattern, buf);
		break;
	}
}

static void orderlist_draw(void)
{
	char buf[4];
	int pos, n;
	int playing_order = (song_get_mode() == MODE_PLAYING ? song_get_current_order() : -1);

	/* draw the list */
	for (pos = 0, n = top_order; pos < 32; pos++, n++) {
		draw_text(str_from_num(3, n, buf), 2, 15 + pos, (n == playing_order ? 3 : 0), 2);
		get_pattern_string(current_song->orderlist[n], buf);
		draw_text(buf, 6, 15 + pos, 2, 0);
	}

	/* draw the cursor */
	if (ACTIVE_PAGE.selected_widget == 0) {
		get_pattern_string(current_song->orderlist[current_order], buf);
		pos = current_order - top_order;
		draw_char(buf[orderlist_cursor_pos], orderlist_cursor_pos + 6, 15 + pos, 0, 3);
	}

	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static void orderlist_insert_pos(void)
{
	memmove(current_song->orderlist + current_order + 1,
		current_song->orderlist + current_order,
		255 - current_order);
	current_song->orderlist[current_order] = ORDER_LAST;

	status.flags |= NEED_UPDATE | SONG_NEEDS_SAVE;
}

static void orderlist_save(void)
{
	memcpy(saved_orderlist, current_song->orderlist, 255);
	_did_save_orderlist = 1;
}
static void orderlist_restore(void)
{
	unsigned char oldlist[256];
	if (!_did_save_orderlist) return;
	memcpy(oldlist, current_song->orderlist, 255);
	memcpy(current_song->orderlist, saved_orderlist, 255);
	memcpy(saved_orderlist, oldlist, 255);
}

static void orderlist_delete_pos(void)
{
	memmove(current_song->orderlist + current_order,
		current_song->orderlist + current_order + 1,
		255 - current_order);
	current_song->orderlist[255] = ORDER_LAST;

	status.flags |= NEED_UPDATE | SONG_NEEDS_SAVE;
}

static void orderlist_insert_next(void)
{
	int next_pattern;

	if (current_order == 0 || current_song->orderlist[current_order - 1] > 199)
		return;
	next_pattern = current_song->orderlist[current_order - 1] + 1;
	if (next_pattern > 199)
		next_pattern = 199;
	current_song->orderlist[current_order] = next_pattern;
	if (current_order < 255)
		current_order++;
	orderlist_reposition();

	status.flags |= NEED_UPDATE | SONG_NEEDS_SAVE;
}

static void orderlist_add_unused_patterns(void)
{
	/* n0 = the first free order
	 * n = orderlist position
	 * p = pattern iterator
	 * np = number of patterns */
	int n0, n, p, np = csf_get_num_patterns(current_song);
	uint8_t used[200] = {0};                /* could be a bitset... */

	for (n = 0; n < 255; n++)
		if (current_song->orderlist[n] < 200)
			used[current_song->orderlist[n]] = 1;

	/* after the loop, n == 255 */
	while (n >= 0 && current_song->orderlist[n] == 0xff)
		n--;
	if (n == -1)
		n = 0;
	else
		n += 2;

	n0 = n;
	for (p = 0; p < np; p++) {
		if (used[p] || csf_pattern_is_empty(current_song, p))
			continue;
		if (n > 255) {
			/* status_text_flash("No more room in orderlist"); */
			break;
		}
		current_song->orderlist[n++] = p;
	}
	if (n == n0) {
		status_text_flash("No unused patterns");
	} else {
		set_current_order(n - 1);
		set_current_order(n0);
		if (n - n0 == 1) {
			status_text_flash("1 unused pattern found");
		} else {
			status_text_flash("%d unused patterns found", n - n0);
		}
	}

	status.flags |= NEED_UPDATE | SONG_NEEDS_SAVE;
}

static void orderlist_reorder(void)
{
	/* err, I hope this is going to be done correctly...
	*/
	song_note_t *np[256] = {0};
	int nplen[256];
	unsigned char mapol[256];
	int i, j;

	song_lock_audio();

	orderlist_add_unused_patterns();

	memset(mapol, ORDER_LAST, sizeof(mapol));
	for (i = j = 0; i < 255; i++) {
		if (current_song->orderlist[i] == ORDER_LAST || current_song->orderlist[i] == ORDER_SKIP) {
			continue;
		}
		if (mapol[ current_song->orderlist[i] ] == ORDER_LAST) {
			np[j] = song_pattern_allocate_copy(current_song->orderlist[i], &nplen[j]);
			mapol[ current_song->orderlist[i] ] = j;
			j++;
		}
		/* replace orderlist entry */
		current_song->orderlist[i] = mapol[ current_song->orderlist[i] ];
	}
	for (i = 0; i < 200; i++) {
		if (!np[i]) {
			song_pattern_install(i, NULL, 64);
		} else {
			song_pattern_install(i, np[i], nplen[i]);
		}
	}

	status.flags |= NEED_UPDATE | SONG_NEEDS_SAVE;

	song_stop_unlocked(0);

	song_unlock_audio();
}

static int orderlist_handle_char(char sym)
{
	int c;
	int cur_pattern;
	//unsigned char *list;
	song_note_t *tmp;
	int n[3] = { 0 };

	switch (sym) {
	default:
		if (sym >= '0' && sym <= '9')
			c = sym - '0';
		else
			return 0;

		status.flags |= SONG_NEEDS_SAVE;
		cur_pattern = current_song->orderlist[current_order];
		if (cur_pattern < 200) {
			n[0] = cur_pattern / 100;
			n[1] = cur_pattern / 10 % 10;
			n[2] = cur_pattern % 10;
		}

		n[orderlist_cursor_pos] = c;
		cur_pattern = n[0] * 100 + n[1] * 10 + n[2];
		cur_pattern = CLAMP(cur_pattern, 0, 199);
		song_get_pattern(cur_pattern, &tmp); /* make sure it exists */
		current_song->orderlist[current_order] = cur_pattern;
		break;
	};

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

static int orderlist_handle_text_input_on_list(const uint8_t* text) {
	int success = 0;

	for (; *text; text++)
		if (!orderlist_handle_char(*text))
			success = 1;

	return success;
}

static int orderlist_handle_key_on_list(struct key_event * k)
{
	int prev_order = current_order;
	int new_order = prev_order;
	int new_cursor_pos = orderlist_cursor_pos;
	int n, p;

	if (k->mouse != MOUSE_NONE) {
		if (k->x >= 6 && k->x <= 8 && k->y >= 15 && k->y <= 46) {
			/* FIXME adjust top_order, not the cursor */
			if (k->mouse == MOUSE_SCROLL_UP) {
				new_order -= MOUSE_SCROLL_LINES;
			} else if (k->mouse == MOUSE_SCROLL_DOWN) {
				new_order += MOUSE_SCROLL_LINES;
			} else {
				if (k->state == KEY_PRESS)
					return 0;

				new_order = (k->y - 15) + top_order;
				set_current_order(new_order);
				new_order = current_order;

				if (current_song->orderlist[current_order] != ORDER_LAST
				&& current_song->orderlist[current_order] != ORDER_SKIP) {
					new_cursor_pos = (k->x - 6);
				}
			}
		}
	}

	// This was here, shouldn't be needed
	// case SDLK_F7:
	// 	if (!(k->mod & KMOD_CTRL)) return 0;
	// 	/* fall through */

	// Why was this here???!
	// case SDLK_LESS:
	// case SDLK_SEMICOLON:
	// case SDLK_COLON:
	// 	if (!NO_MODIFIER(k->mod)) return 0;
	// 	if (k->state == KEY_RELEASE)
	// 		return 1;
	// 	sample_set(sample_get_current()-1);
	// 	status.flags |= NEED_UPDATE;
	// 	return 1;
	// case SDLK_GREATER:
	// case SDLK_QUOTE:
	// case SDLK_QUOTEDBL:
	// 	if (!NO_MODIFIER(k->mod)) return 0;
	// 	if (k->state == KEY_RELEASE)
	// 		return 1;
	// 	sample_set(sample_get_current()+1);
	// 	status.flags |= NEED_UPDATE;
	// 	return 1;

	if (KEY_PRESSED(order_list, restore_order_list)) {
		if (status.flags & CLASSIC_MODE) return 0;
		if (!_did_save_orderlist) return 1;
		status_text_flash("Restored orderlist");
		orderlist_restore();
		return 1;
	} else if(KEY_PRESSED(order_list, save_order_list)) {
		if (status.flags & CLASSIC_MODE) return 0;
		status_text_flash("Saved orderlist");
		orderlist_save();
		return 1;
	} else if(KEY_PRESSED(order_list, goto_selected_pattern)) {
		n = current_song->orderlist[new_order];
		while (n >= 200 && new_order > 0)
			n = current_song->orderlist[--new_order];
		if (n < 200) {
			set_current_pattern(n);
			set_page(PAGE_PATTERN_EDITOR);
		}
		return 1;
	} else if(KEY_PRESSED_OR_REPEATED(global, nav_tab)) {
		widget_change_focus_to(1);
	} else if(KEY_PRESSED_OR_REPEATED(global, nav_backtab)) {
		widget_change_focus_to(33);
	} else if(KEY_PRESSED_OR_REPEATED(global, nav_left)) {
		new_cursor_pos--;
	} else if(KEY_PRESSED_OR_REPEATED(global, nav_right)) {
		new_cursor_pos++;
	} else if(KEY_PRESSED_OR_REPEATED(global, nav_up)) {
		new_order--;
	} else if(KEY_PRESSED_OR_REPEATED(global, nav_down)) {
		new_order++;
	} else if(KEY_PRESSED_OR_REPEATED(global, nav_home)) {
		new_order = 0;
	} else if(KEY_PRESSED_OR_REPEATED(global, nav_end)) {
		new_order = csf_last_order(current_song);
		if (current_song->orderlist[new_order] != ORDER_LAST)
			new_order++;
	} else if(KEY_PRESSED_OR_REPEATED(global, nav_page_up)) {
		new_order -= 16;
	} else if(KEY_PRESSED_OR_REPEATED(global, nav_page_down)) {
		new_order += 16;
	} else if(KEY_PRESSED(order_list, decrease_instrument)) {
		if (status.flags & CLASSIC_MODE) return 0;
		sample_set(sample_get_current() - 1);
		status.flags |= NEED_UPDATE;
		return 1;
	} else if(KEY_PRESSED(order_list, increase_instrument)) {
		if (status.flags & CLASSIC_MODE) return 0;
		sample_set(sample_get_current() + 1);
		status.flags |= NEED_UPDATE;
	} else if(KEY_PRESSED(order_list, insert_pattern)) {
		orderlist_insert_pos();
	} else if(KEY_PRESSED(order_list, delete_pattern)) {
		orderlist_delete_pos();
	} else if(KEY_PRESSED(order_list, select_order_for_playback)) {
		song_set_next_order(current_order);
		status_text_flash("Playing order %d next", current_order);
	} else if(KEY_PRESSED(order_list, play_from_order)) {
		song_start_at_order(current_order, 0);
		return 1;
	} else if(KEY_PRESSED(order_list, duplicate_pattern)) {
		orderlist_cheater();
		return 1;
	} else if(KEY_PRESSED(order_list, insert_next_pattern)) {
		orderlist_insert_next();
		return 1;
	} else if(KEY_PRESSED(order_list, continue_next_position_of_pattern)) {
		if (status.flags & CLASSIC_MODE) return 0;
		p = get_current_pattern();
		for (n = current_order+1; n < 256; n++) {
			if (current_song->orderlist[n] == p) {
				new_order = n;
				break;
			}
		}
		if (n == 256) {
			for (n = 0; n < current_order; n++) {
				if (current_song->orderlist[n] == p) {
					new_order = n;
					break;
				}
			}
			if (n == current_order) {
				status_text_flash("Pattern %d not on Order List", p);
				return 1;
			}
		}
	} else if(KEY_PRESSED(order_list, sort_order_list)) {
		orderlist_reorder();
		return 1;
	} else if(KEY_PRESSED(order_list, find_unused_patterns)) {
		orderlist_add_unused_patterns();
		return 1;
	} else if(KEY_PRESSED(order_list, link_pattern_to_sample)) {
		song_pattern_to_sample(current_song->orderlist[current_order], 0, 1);
	} else if(KEY_PRESSED(order_list, copy_pattern_to_sample)) {
		song_pattern_to_sample(current_song->orderlist[current_order], 0, 0);
	} else if(KEY_PRESSED(order_list, copy_pattern_to_sample_with_split)) {
		song_pattern_to_sample(current_song->orderlist[current_order], 1, 0);
	} else if(KEY_PRESSED(order_list, mark_end_of_song)) {
		status.flags |= SONG_NEEDS_SAVE;
		current_song->orderlist[current_order] = ORDER_LAST;
		new_order++;
	} else if(KEY_PRESSED(order_list, skip_to_next_order_mark)) {
		status.flags |= SONG_NEEDS_SAVE;
		current_song->orderlist[current_order] = ORDER_SKIP;
		new_order++;
	} else {
		if (k->mouse == MOUSE_NONE) {
			if (!(k->mod & (KMOD_CTRL | KMOD_ALT)) && k->text)
				return orderlist_handle_text_input_on_list(k->text);

			return 0;
		}
	}

	if (new_cursor_pos < 0)
		new_cursor_pos = 2;
	else if (new_cursor_pos > 2)
		new_cursor_pos = 0;

	if (new_order != prev_order) {
		set_current_order(new_order);
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
	char buf[16];
	int fg;

	strcpy(buf, "Channel 42");

	order_pan_vol_draw_const();

	draw_text(" Volumes ", 31, 14, 0, 3);
	draw_text(" Volumes ", 65, 14, 0, 3);

	for (n = 1; n <= 32; n++) {
		fg = 0;
		if (!(status.flags & CLASSIC_MODE)) {
			if (ACTIVE_PAGE.selected_widget == n) {
				fg = 3;
			}
		}

		str_from_num(2, n, buf + 8);
		draw_text(buf, 20, 14 + n, fg, 2);

		fg = 0;
		if (!(status.flags & CLASSIC_MODE)) {
			if (ACTIVE_PAGE.selected_widget == n+32) {
				fg = 3;
			}
		}

		str_from_num(2, n + 32, buf + 8);
		draw_text(buf, 54, 14 + n, fg, 2);
	}
}

/* --------------------------------------------------------------------- */

static void order_pan_vol_playback_update(void)
{
	static int last_order = -1;
	int order = ((song_get_mode() == MODE_STOPPED) ? -1 : song_get_current_order());

	if (order != last_order) {
		last_order = order;
		status.flags |= NEED_UPDATE;
	}
}

/* --------------------------------------------------------------------- */

static void orderpan_update_values_in_song(void)
{
	song_channel_t *chn;
	int n;

	status.flags |= SONG_NEEDS_SAVE;
	for (n = 0; n < 64; n++) {
		chn = song_get_channel(n);

		/* yet another modplug hack here! */
		chn->panning = widgets_orderpan[n + 1].d.panbar.value * 4;

		if (widgets_orderpan[n + 1].d.panbar.surround)
			chn->flags |= CHN_SURROUND;
		else
			chn->flags &= ~CHN_SURROUND;

		song_set_channel_mute(n, widgets_orderpan[n + 1].d.panbar.muted);
	}
}

static void ordervol_update_values_in_song(void)
{
	int n;

	status.flags |= SONG_NEEDS_SAVE;
	for (n = 0; n < 64; n++)
		song_get_channel(n)->volume = widgets_ordervol[n + 1].d.thumbbar.value;
}

/* called when a channel is muted/unmuted by means other than the panning
 * page (alt-f10 in the pattern editor, space on the info page...) */
void orderpan_recheck_muted_channels(void)
{
	int n;
	for (n = 0; n < 64; n++)
		widgets_orderpan[n + 1].d.panbar.muted = !!(song_get_channel(n)->flags & CHN_MUTE);

	if (status.current_page == PAGE_ORDERLIST_PANNING)
		status.flags |= NEED_UPDATE;
}

static void order_pan_vol_song_changed_cb(void)
{
	int n;
	song_channel_t *chn;

	for (n = 0; n < 64; n++) {
		chn = song_get_channel(n);
		widgets_orderpan[n + 1].d.panbar.value = chn->panning / 4;
		widgets_orderpan[n + 1].d.panbar.surround = !!(chn->flags & CHN_SURROUND);
		widgets_orderpan[n + 1].d.panbar.muted = !!(chn->flags & CHN_MUTE);
		widgets_ordervol[n + 1].d.thumbbar.value = chn->volume;
	}
}

/* --------------------------------------------------------------------- */

static void order_pan_vol_handle_key(struct key_event * k)
{
	int n = ACTIVE_PAGE.selected_widget;

	if (KEY_PRESSED_OR_REPEATED(global, nav_page_down)) {
		n += 8;
	} else if (KEY_PRESSED_OR_REPEATED(global, nav_page_up)) {
		n -= 8;
	} else {
		return;
	}

	n = CLAMP(n, 1, 64);
	if (ACTIVE_PAGE.selected_widget != n)
		widget_change_focus_to(n);
}

static int order_pre_key(struct key_event *k)
{
	// hack to sync the active widget between pan/vol pages
	if (!(status.flags & CLASSIC_MODE)) {
		pages[PAGE_ORDERLIST_PANNING].selected_widget
			= pages[PAGE_ORDERLIST_VOLUMES].selected_widget
			= ACTIVE_PAGE.selected_widget;
	}

	return 0;
}

static void order_pan_set_page(void)
{
	orderpan_recheck_muted_channels();
}

/* --------------------------------------------------------------------- */

void orderpan_load_page(struct page *page)
{
	int n;
	char* shortcut_text = (char*)global_keybinds_list.global.order_list.shortcut_text_parens;
	page->title = STR_CONCAT(2, "Order List and Panning", shortcut_text);
	page->draw_const = orderpan_draw_const;
	/* this does the work for both pages */
	page->song_changed_cb = order_pan_vol_song_changed_cb;
	page->playback_update = order_pan_vol_playback_update;
	page->pre_handle_key = order_pre_key;
	page->handle_key = order_pan_vol_handle_key;
	page->set_page = order_pan_set_page;
	page->total_widgets = 65;
	page->widgets = widgets_orderpan;
	page->help_index = HELP_ORDERLIST_PANNING;

	/* 0 = order list */
	widget_create_other(widgets_orderpan + 0, 1, orderlist_handle_key_on_list,
		orderlist_handle_text_input_on_list, orderlist_draw);
	widgets_orderpan[0].accept_text = 1;
	widgets_orderpan[0].x = 6;
	widgets_orderpan[0].y = 15;
	widgets_orderpan[0].width = 3;
	widgets_orderpan[0].height = 32;

	/* 1-64 = panbars */
	widget_create_panbar(widgets_orderpan + 1, 20, 15, 1, 2, 33, orderpan_update_values_in_song, 1);
	for (n = 2; n <= 32; n++) {
		widget_create_panbar(widgets_orderpan + n, 20, 14 + n, n - 1, n + 1, n + 32,
			      orderpan_update_values_in_song, n);
		widget_create_panbar(widgets_orderpan + n + 31, 54, 13 + n, n + 30, n + 32, 0,
			      orderpan_update_values_in_song, n + 31);
	}
	widget_create_panbar(widgets_orderpan + 64, 54, 46, 63, 64, 0, orderpan_update_values_in_song, 64);
}

void ordervol_load_page(struct page *page)
{
	int n;
	char* shortcut_text = (char*)global_keybinds_list.global.order_list.shortcut_text_parens;
	page->title = STR_CONCAT(2, "Order List and Channel Volume", shortcut_text);

	page->draw_const = ordervol_draw_const;
	page->playback_update = order_pan_vol_playback_update;
	page->pre_handle_key = order_pre_key;
	page->handle_key = order_pan_vol_handle_key;
	page->total_widgets = 65;
	page->widgets = widgets_ordervol;
	page->help_index = HELP_ORDERLIST_VOLUME;

	/* 0 = order list */
	widget_create_other(widgets_ordervol + 0, 1, orderlist_handle_key_on_list,
		orderlist_handle_text_input_on_list, orderlist_draw);
	widgets_ordervol[0].accept_text = 1;
	widgets_ordervol[0].x = 6;
	widgets_ordervol[0].y = 15;
	widgets_ordervol[0].width = 3;
	widgets_ordervol[0].height = 32;

	/* 1-64 = thumbbars */
	widget_create_thumbbar(widgets_ordervol + 1, 31, 15, 9, 1, 2, 33, ordervol_update_values_in_song, 0, 64);
	for (n = 2; n <= 32; n++) {
		widget_create_thumbbar(widgets_ordervol + n, 31, 14 + n, 9, n - 1, n + 1, n + 32,
				ordervol_update_values_in_song, 0, 64);
		widget_create_thumbbar(widgets_ordervol + n + 31, 65, 13 + n, 9, n + 30, n + 32, 0,
				ordervol_update_values_in_song, 0, 64);
	}
	widget_create_thumbbar(widgets_ordervol + 64, 65, 46, 9, 63, 64, 0, ordervol_update_values_in_song, 0, 64);
}

static int order_list_page_matcher(enum page_numbers page)
{
	return page == PAGE_ORDERLIST_PANNING || page == PAGE_ORDERLIST_VOLUMES;
}

int ordervol_load_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(order_list, "Order Keys.", PAGE_ORDERLIST_PANNING);
	global_keybinds_list.order_list_info.page_matcher = order_list_page_matcher;

	INIT_BIND(order_list, goto_selected_pattern, "Goto selected pattern", "ENTER,KP_ENTER,G");
	INIT_BIND(order_list, play_from_order, "Play from order", "Shift+F6");
	INIT_BIND(order_list, select_order_for_playback, "Select pattern for playback", "SPACE,Ctrl+F7");
	INIT_BIND(order_list, insert_next_pattern, "Insert next pattern", "N");
	INIT_BIND(order_list, duplicate_pattern, "Copy current pattern to new pattern, and insert order", "Shift+N");
	INIT_BIND(order_list, mark_end_of_song, "End of song mark", "MINUS,KP_MINUS,PERIOD");
	INIT_BIND(order_list, skip_to_next_order_mark, "Skip to next order mark", "Shift+EQUALS,KP_PLUS");
	INIT_BIND(order_list, insert_pattern, "Insert a pattern", "INSERT");
	INIT_BIND(order_list, delete_pattern, "Delete a pattern", "DELETE");

	// Is duplicate
	// INIT_BIND(order_list, toggle_order_list_locked, "Lock/unlock order list", "Alt+F11");
	INIT_BIND(order_list, sort_order_list, "Sort order list", "Alt+R");
	INIT_BIND(order_list, find_unused_patterns, "Search for unused patterns\n ", "Alt+U");

	INIT_BIND(order_list, link_pattern_to_sample, "Link (diskwriter) this pattern to the current sample", "Ctrl+B");
	INIT_BIND(order_list, copy_pattern_to_sample, "Copy (diskwriter) this pattern to the current sample", "Ctrl+O");
	INIT_BIND(order_list, copy_pattern_to_sample_with_split, "Copy (diskwriter) to the current sample, with split\n ", "Ctrl+Shift+O");

	INIT_BIND(order_list, continue_next_position_of_pattern, "Continue to next position of current pattern\n ", "C");

	INIT_BIND(order_list, save_order_list, "Save order list", "Alt+ENTER");
	INIT_BIND(order_list, restore_order_list, "Swap order list with saved order list", "Alt+BACKSPACE");

	// I'm not sure why this is here, but it is
	INIT_BIND(order_list, decrease_instrument, "Decrease instrument", "Shift+COMMA,Ctrl+UP");
	INIT_BIND(order_list, increase_instrument, "Increase instrument", "Shift+PERIOD,Ctrl+DOWN");

	INIT_SECTION(order_list_panning, "Panning Keys.", PAGE_ORDERLIST_PANNING);
	INIT_BIND(order_list_panning, toggle_channel_mute, "Toggle channel muted", "SPACE");
	INIT_BIND(order_list_panning, set_panning_left, "Set panning left", "L");
	INIT_BIND(order_list_panning, set_panning_middle, "Set panning middle", "M");
	INIT_BIND(order_list_panning, set_panning_right, "Set panning right", "R");
	INIT_BIND(order_list_panning, set_panning_surround, "Set panning surround", "S");
	INIT_BIND(order_list_panning, pan_unmuted_left, "Pan all unmuted channels left", "Alt+L");
	INIT_BIND(order_list_panning, pan_unmuted_middle, "Pan all unmuted channels middle", "Alt+M");
	INIT_BIND(order_list_panning, pan_unmuted_right, "Pan all unmuted channels right", "Alt+R");
	INIT_BIND(order_list_panning, pan_unmuted_stereo, "Pan all unmuted channels stereo", "Alt+S");
	INIT_BIND(order_list_panning, pan_unmuted_amiga_stereo, "Pan all unmuted channels amiga stereo", "Alt+A");
	INIT_BIND(order_list_panning, linear_panning_left_to_right, "Linear panning (left to right)", "Alt+BACKSLASH");
	INIT_BIND(order_list_panning, linear_panning_right_to_left, "Linear panning (right to left)", "Alt+SLASH,Alt+KP_DIVIDE");

	return 1;
}

/* --------------------------------------------------------------------- */
/* this function is a lost little puppy */

#define MAX_CHANNELS 64 // blah
void song_set_pan_scheme(int scheme)
{
	int n, nc;
	//int half;
	int active = 0;
	song_channel_t *chn = song_get_channel(0);

	//mphack alert, all pan values multiplied by 4
	switch (scheme) {
	case PANS_STEREO:
		for (n = 0; n < MAX_CHANNELS; n++) {
			if (!(chn[n].flags & CHN_MUTE))
				chn[n].panning = (n & 1) ? 256 : 0;
		}
		break;
	case PANS_AMIGA:
		for (n = 0; n < MAX_CHANNELS; n++) {
			if (!(chn[n].flags & CHN_MUTE))
				chn[n].panning = ((n + 1) & 2) ? 256 : 0;
		}
		break;
	case PANS_LEFT:
		for (n = 0; n < MAX_CHANNELS; n++) {
			if (!(chn[n].flags & CHN_MUTE))
				chn[n].panning = 0;
		}
		break;
	case PANS_RIGHT:
		for (n = 0; n < MAX_CHANNELS; n++) {
			if (!(chn[n].flags & CHN_MUTE))
				chn[n].panning = 256;
		}
		break;
	case PANS_MONO:
		for (n = 0; n < MAX_CHANNELS; n++) {
			if (!(chn[n].flags & CHN_MUTE))
				chn[n].panning = 128;
		}
		break;
	case PANS_SLASH:
		for (n = 0; n < MAX_CHANNELS; n++) {
			if (!(chn[n].flags & CHN_MUTE))
				active++;
		}
		for (n = 0, nc = 0; nc < active; n++) {
			if (!(chn[n].flags & CHN_MUTE)) {
				chn[n].panning = 256 - (256 * nc / (active - 1));
				nc++;
			}
		}
		break;
	case PANS_BACKSLASH:
		for (n = 0; n < MAX_CHANNELS; n++) {
			if (!(chn[n].flags & CHN_MUTE))
				active++;
		}
		for (n = 0, nc = 0; nc < active; n++) {
			if (!(chn[n].flags & CHN_MUTE)) {
				chn[n].panning = (256 * nc / (active - 1));
				nc++;
			}
		}
		break;
#if 0
	case PANS_CROSS:
		for (n = 0; n < MAX_CHANNELS; n++) {
			if (!(chn[n].flags & CHN_MUTE))
				active++;
		}
		half = active / 2;
		for (n = 0, nc = 0; nc < half; n++) {
			if (!(chn[n].flags & CHN_MUTE)) {
				if (nc & 1) {
					// right bias - go from 64 to 32
					chn[n].panning = (64 - (32 * nc / half)) * 4;
				} else {
					// left bias - go from 0 to 32
					chn[n].panning = (32 * nc / half) * 4;
				}
				nc++;
			}
		}
		for (; nc < active; n++) {
			if (!(chn[n].flags & CHN_MUTE)) {
				if (nc & 1) {
					chn[n].panning = (64 - (32 * (active - nc) / half)) * 4;
				} else {
					chn[n].panning = (32 * (active - nc) / half) * 4;
				}
				nc++;
			}
		}
		break;
#endif
	default:
		printf("oh i am confused\n");
	}
	// get the values on the page to correspond to the song...
	order_pan_vol_song_changed_cb();
}

