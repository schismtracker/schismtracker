/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
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

#define NEED_TIME
#include "headers.h"

#include <SDL.h>

#include "it.h"
#include "song.h"
#include "page.h"

/* --------------------------------------------------------------------- */
/* globals */

struct tracker_status status = {
        .current_page = PAGE_BLANK,
        .previous_page = PAGE_BLANK,
        .current_help_index = HELP_GLOBAL,
        .dialog_type = DIALOG_NONE,
        .flags = IS_FOCUSED | IS_VISIBLE,
        .time_display = TIME_PLAY_ELAPSED,
        .last_keysym = 0,
};

struct page pages[32];

struct item *items = NULL;
int *selected_item = NULL;
int *total_items = NULL;

/* --------------------------------------------------------------------- */

/* *INDENT-OFF* */
static struct {
        int h, m, s;
} current_time = {0, 0, 0};
/* *INDENT-ON* */

/* return 1 -> the time changed; need to redraw */
static int check_time(void)
{
        time_t timep = 0;
        struct tm local;
        int h, m, s;
        enum tracker_time_display td = status.time_display;
	int is_playing = song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP);

        switch (td) {
        case TIME_PLAY_ELAPSED:
                td = (is_playing ? TIME_PLAYBACK : TIME_ELAPSED);
                break;
        case TIME_PLAY_CLOCK:
                td = (is_playing ? TIME_PLAYBACK : TIME_CLOCK);
                break;
        case TIME_PLAY_OFF:
                td = (is_playing ? TIME_PLAYBACK : TIME_OFF);
                break;
        default:
                break;
        }

        switch (td) {
        case TIME_OFF:
                h = m = s = 0;
                break;
        case TIME_PLAYBACK:
                h = (m = (s = song_get_current_time()) / 60) / 24;
                break;
        case TIME_ELAPSED:
                h = (m = (s = SDL_GetTicks() / 1000) / 60) / 24;
                break;
        default:
                /* this will never happen */
        case TIME_CLOCK:
                /* Impulse Tracker doesn't have this, but I always wanted
                 * it, so here 'tis. */
                time(&timep);
                localtime_r(&timep, &local);
                h = local.tm_hour;
                m = local.tm_min;
                s = local.tm_sec;
                break;
        }

        if (h == current_time.h && m == current_time.m && s == current_time.s) {
                return 0;
        }

        current_time.h = h;
        current_time.m = m;
        current_time.s = s;
        return 1;
}

static inline void draw_time(void)
{
        char buf[16];
	int is_playing = song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP);
	
        if (status.time_display == TIME_OFF || (status.time_display == TIME_PLAY_OFF && !is_playing))
                return;
        
        /* this allows for 999 hours... that's like... 41 days...
         * who on earth leaves a tracker running for 41 days? */
        sprintf(buf, "%3d:%02d:%02d", current_time.h % 1000,
                current_time.m % 60, current_time.s % 60);
        draw_text(buf, 69, 9, 0, 2);
}

/* --------------------------------------------------------------------- */

static void draw_page_title(void)
{
        int x, tpos, tlen = strlen(ACTIVE_PAGE.title);

        SDL_LockSurface(screen);

        if (tlen > 0) {
                tpos = 41 - ((tlen + 1) / 2);

                for (x = 1; x < tpos - 1; x++)
                        draw_char_unlocked(154, x, 11, 1, 2);
                draw_char_unlocked(0, tpos - 1, 11, 1, 2);
                draw_text_unlocked(ACTIVE_PAGE.title, tpos, 11, 0, 2);
                draw_char_unlocked(0, tpos + tlen, 11, 1, 2);
                for (x = tpos + tlen + 1; x < 79; x++)
                        draw_char_unlocked(154, x, 11, 1, 2);
        } else {
                for (x = 1; x < 79; x++)
                        draw_char_unlocked(154, x, 11, 1, 2);
        }

        SDL_UnlockSurface(screen);
}

/* --------------------------------------------------------------------- */
/* Not that happy with the way this function developed, but well, it still
 * works. Maybe someday I'll make it suck less. */

static void draw_page(void)
{
        int n = ACTIVE_PAGE.total_items;

        draw_page_title();
        RUN_IF(ACTIVE_PAGE.draw_const);
        RUN_IF(ACTIVE_PAGE.predraw_hook);

        /* this doesn't use items[] because it needs to draw the page's
         * items whether or not a dialog is active */
        while (n) {
                n--;
                draw_item(ACTIVE_PAGE.items + n,
                          n == ACTIVE_PAGE.selected_item);
        }

        /* redraw the area over the menu if there is one */
        if (status.dialog_type & DIALOG_MENU)
                menu_draw();
        else if (status.dialog_type & DIALOG_BOX)
                dialog_draw();
}

/* --------------------------------------------------------------------- */

inline int page_is_instrument_list(int page)
{
        switch (page) {
        case PAGE_INSTRUMENT_LIST_GENERAL:
        case PAGE_INSTRUMENT_LIST_VOLUME:
        case PAGE_INSTRUMENT_LIST_PANNING:
        case PAGE_INSTRUMENT_LIST_PITCH:
                return 1;
        default:
                return 0;
        }
}

/* --------------------------------------------------------------------------------------------------------- */

static struct item new_song_items[10] = {};
static int new_song_groups[4][3] = { {0, 1, -1}, {2, 3, -1}, {4, 5, -1}, {6, 7, -1} };

static void new_song_ok(void)
{
	int flags = 0;
	if (new_song_items[0].togglebutton.state)
		flags |= KEEP_PATTERNS;
	if (new_song_items[2].togglebutton.state)
		flags |= KEEP_SAMPLES;
	if (new_song_items[4].togglebutton.state)
		flags |= KEEP_INSTRUMENTS;
	if (new_song_items[6].togglebutton.state)
		flags |= KEEP_ORDERLIST;
	song_new(flags);
}

static void new_song_cancel(void)
{
	printf("new song cancel\n");
}

static void new_song_draw_const(void)
{
	SDL_LockSurface(screen);
	draw_text("New Song", 36, 21, 3, 2);
	draw_text("Patterns", 26, 24, 0, 2);
	draw_text("Samples", 27, 27, 0, 2);
	draw_text("Instruments", 23, 30, 0, 2);
	draw_text("Order List", 24, 33, 0, 2);
	SDL_UnlockSurface(screen);
}

static void new_song_dialog(void)
{
	struct dialog *dialog;

	if (new_song_items[0].width == 0) {
		create_togglebutton(new_song_items + 0, 35, 24, 6, 0, 2, 1, 1, 1, NULL, "Keep", 2,
				    new_song_groups[0]);
		create_togglebutton(new_song_items + 1, 45, 24, 7, 1, 3, 0, 0, 0, NULL, "Clear", 2,
				    new_song_groups[0]);
		create_togglebutton(new_song_items + 2, 35, 27, 6, 0, 4, 3, 3, 3, NULL, "Keep", 2,
				    new_song_groups[1]);
		create_togglebutton(new_song_items + 3, 45, 27, 7, 1, 5, 2, 2, 2, NULL, "Clear", 2,
				    new_song_groups[1]);
		create_togglebutton(new_song_items + 4, 35, 30, 6, 2, 6, 5, 5, 5, NULL, "Keep", 2,
				    new_song_groups[2]);
		create_togglebutton(new_song_items + 5, 45, 30, 7, 3, 7, 4, 4, 4, NULL, "Clear", 2,
				    new_song_groups[2]);
		create_togglebutton(new_song_items + 6, 35, 33, 6, 4, 8, 7, 7, 7, NULL, "Keep", 2,
				    new_song_groups[3]);
		create_togglebutton(new_song_items + 7, 45, 33, 7, 5, 9, 6, 6, 6, NULL, "Clear", 2,
				    new_song_groups[3]);
		create_button(new_song_items + 8, 28, 36, 8, 6, 8, 9, 9, 9, dialog_yes, "OK", 4);
		create_button(new_song_items + 9, 41, 36, 8, 6, 9, 8, 8, 8, dialog_cancel, "Cancel", 2);
		togglebutton_set(new_song_items, 1, 0);
		togglebutton_set(new_song_items, 3, 0);
		togglebutton_set(new_song_items, 5, 0);
		togglebutton_set(new_song_items, 7, 0);
	}
	
	dialog = dialog_create_custom(21, 20, 38, 19, new_song_items, 10, 8, new_song_draw_const);
	dialog->action_yes = new_song_ok;
	dialog->action_cancel = new_song_cancel;
}

/* --------------------------------------------------------------------------------------------------------- */
/* This is an ugly monster. */

/* returns 1 if the key was handled */
static inline int handle_key_global(SDL_keysym * k)
{
        /* first, check the truly global keys (the ones that still work if
         * a dialog's open) */
        switch (k->sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
                /* FIXME | Impulse Tracker uses this key combo for "store
                 * FIXME | pattern data". I'm not exactly sure what that
                 * FIXME | does... */
                if (k->mod & (KMOD_ALT | KMOD_META)) {
                        SDL_WM_ToggleFullScreen(screen);
                        return 1;
                }
                break;
        case SDLK_m:
                if (k->mod & KMOD_CTRL) {
                        SDL_ToggleCursor();
                        return 1;
                }
                break;
        case SDLK_d:
                if (k->mod & KMOD_CTRL) {
                        /* should do something...
                         * minimize? open console? dunno. */
                        return 1;
                }
                break;
        case SDLK_e:
                /* This should reset everything display-related. */
                if (k->mod & KMOD_CTRL) {
                        /* FIXME | screen reset stuff should probably be
                         * FIXME | in a separate function. (which should
                         * FIXME | be called on startup as well) */
                        font_init();
                        clear_all_cached_waveforms();

                        status.flags |= NEED_UPDATE;
                        return 1;
                }
                break;
        default:
                break;
        }

        /* next, if there's no dialog, check the rest of the keys */

        if (status.dialog_type != DIALOG_NONE)
                return 0;

        switch (k->sym) {
        case SDLK_q:
                if (k->mod & KMOD_CTRL) {
                        show_exit_prompt();
                        return 1;
                }
                break;
	case SDLK_n:
		if (k->mod & KMOD_CTRL) {
			new_song_dialog();
			return 1;
		}
		break;
        case SDLK_p:
                if (k->mod & KMOD_CTRL) {
                        show_song_length();
                        return 1;
                }
                break;
        case SDLK_F1:
                if (k->mod & KMOD_CTRL) {
                        set_page(PAGE_LOG);
                } else if (k->mod & KMOD_SHIFT) {
                        set_page(PAGE_MIDI);
                } else if (NO_MODIFIER(k->mod)) {
                        set_page(PAGE_HELP);
                } else {
                        break;
                }
                return 1;
        case SDLK_F2:
		if (k->mod & KMOD_CTRL) {
			if (status.current_page == PAGE_PATTERN_EDITOR) {
				/* TODO: set pattern length dialog */
				return 1;
			}
		} else if (NO_MODIFIER(k->mod)) {
			if (status.current_page == PAGE_PATTERN_EDITOR) {
				pattern_editor_display_options();
			} else {
				set_page(PAGE_PATTERN_EDITOR);
			}
                        return 1;
                }
		break;
        case SDLK_F3:
                if (NO_MODIFIER(k->mod)) {
                        set_page(PAGE_SAMPLE_LIST);
                } else {
                        break;
                }
                return 1;
        case SDLK_F4:
                if (NO_MODIFIER(k->mod)) {
                        set_page(PAGE_INSTRUMENT_LIST);
                } else {
                        break;
                }
                return 1;
        case SDLK_F5:
                if (k->mod & KMOD_CTRL) {
                        song_start();
                } else if (k->mod & KMOD_SHIFT) {
                        set_page(PAGE_SETTINGS);
                } else if (NO_MODIFIER(k->mod)) {
                        if (song_get_mode() == MODE_STOPPED
			    || (song_get_mode() == MODE_SINGLE_STEP && status.current_page == PAGE_INFO))
                                song_start();
                        set_page(PAGE_INFO);
                } else {
                        break;
                }
                return 1;
        case SDLK_F6:
                if (k->mod & KMOD_SHIFT) {
                        song_start_at_order(get_current_order(), 0);
                } else if (NO_MODIFIER(k->mod)) {
                        song_loop_pattern(get_current_pattern(), 0);
                } else {
                        break;
                }
                return 1;
        case SDLK_F7:
                if (NO_MODIFIER(k->mod)) {
                        play_song_from_mark();
                } else {
                        break;
                }
                return 1;
        case SDLK_F8:
                if (NO_MODIFIER(k->mod)) {
                        song_stop();
                        status.flags |= NEED_UPDATE;
                } else {
                        break;
                }
                return 1;
        case SDLK_F9:
                if (k->mod & KMOD_SHIFT) {
                        set_page(PAGE_MESSAGE);
                } else if (NO_MODIFIER(k->mod)) {
                        set_page(PAGE_LOAD_MODULE);
                } else {
                        break;
                }
                return 1;
        case SDLK_r:
                if (k->mod & KMOD_CTRL) {
                        set_page(PAGE_LOAD_MODULE);
                } else {
                        break;
                }
                return 1;
        case SDLK_F10:
                if (NO_MODIFIER(k->mod)) {
                        /* TODO: actually have a save screen ;) */
                        set_page(PAGE_LOG);
                        song_save("test.it");
                } else {
                        break;
                }
                return 1;
        case SDLK_F11:
                if (NO_MODIFIER(k->mod)) {
                        if (status.current_page == PAGE_ORDERLIST_PANNING) {
                                set_page(PAGE_ORDERLIST_VOLUMES);
                        } else {
                                set_page(PAGE_ORDERLIST_PANNING);
                        }
                } else if (k->mod & (KMOD_ALT | KMOD_META)) {
                        printf("TODO: lock/unlock order list\n");
                } else {
                        break;
                }
                return 1;
        case SDLK_F12:
                if (k->mod & KMOD_CTRL) {
                        set_page(PAGE_PALETTE_EDITOR);
                } else if (NO_MODIFIER(k->mod)) {
                        set_page(PAGE_SONG_VARIABLES);
                } else {
                        break;
                }
                return 1;
        default:
                break;
        }
	
	/* got a bit ugly here, sorry */
	do {
		int i = k->sym;
		
		if (k->mod & (KMOD_ALT | KMOD_META)) {
			if (i >= SDLK_F1 && i <= SDLK_F8)
				i -= SDLK_F1;
			else if (i >= SDLK_KP1 && i <= SDLK_KP8)
				i -= SDLK_KP1;
			else
				break;
			
			song_toggle_channel_mute(i);
			if (status.current_page == PAGE_PATTERN_EDITOR) {
				status.flags |= NEED_UPDATE;
			}
			orderpan_recheck_muted_channels();
			return 1;
		}
        } while (0);

        /* oh well */
        return 0;
}

/* this is the important one */
void handle_key(SDL_keysym * k)
{
	// short circuit booleans rock
	if (handle_key_global(k) || menu_handle_key(k) || item_handle_key(k))
                return;
	
	/* Maybe this should be after the page's key handler
	 * (which should return true/false depending on whether
	 * it handled the key or not, like the item handlers...) */
        if (k->sym == SDLK_ESCAPE) {
                if (status.dialog_type != DIALOG_NONE) {
                        dialog_handle_key(k);
                        return;
                } else if (NO_MODIFIER(k->mod)) {
                        menu_show();
                        return;
                }
        }

        /* now check a couple other keys. */
        switch (k->sym) {
        case SDLK_KP_DIVIDE:
                kbd_set_current_octave(kbd_get_current_octave() - 1);
                return;
        case SDLK_KP_MULTIPLY:
                kbd_set_current_octave(kbd_get_current_octave() + 1);
                return;
        default:
                switch (k->unicode) {
                case '{':
                        song_set_current_speed(song_get_current_speed()
                                               - 1);
                        status_text_flash("Speed set to %d frames per row",
                                          song_get_current_speed());
                        return;
                case '}':
                        song_set_current_speed(song_get_current_speed()
                                               + 1);
                        status_text_flash("Speed set to %d frames per row",
                                          song_get_current_speed());
                        return;
                case '[':
                        song_set_current_global_volume
                                (song_get_current_global_volume() - 1);
                        status_text_flash("Global volume set to %d",
                                          song_get_current_global_volume
                                          ());
                        return;
                case ']':
                        song_set_current_global_volume
                                (song_get_current_global_volume() + 1);
                        status_text_flash("Global volume set to %d",
                                          song_get_current_global_volume
                                          ());
                        return;
                default:
                        break;
                }
                break;
        }

        /* and if we STILL didn't handle the key, pass it to the page.
         * (or dialog, if one's active) */
        if (status.dialog_type & DIALOG_BOX)
                dialog_handle_key(k);
        else
                RUN_IF(ACTIVE_PAGE.handle_key, k);
}

/* --------------------------------------------------------------------- */
/* Jeffrey, dude, you made this HARD TO DO :) */

static void draw_top_info_const(void)
{
        int n;

        SDL_LockSurface(screen);
        
        /* gcc optimizes out the strlen's here :) */
        if (status.flags & CLASSIC_MODE) {
#define TOP_BANNER_TEXT "Impulse Tracker v2.14" \
        " Copyright (C) 1995-1998 Jeffrey Lim"
                draw_text_unlocked(TOP_BANNER_TEXT,
                                   (80 - strlen(TOP_BANNER_TEXT)) / 2,
                                   1, 0, 2);
#undef TOP_BANNER_TEXT
        } else {
#define TOP_BANNER_TEXT "Schism Tracker v" VERSION \
        " Copyright (C) 2003-2004 chisel"
                draw_text_unlocked(TOP_BANNER_TEXT,
                                   (80 - strlen(TOP_BANNER_TEXT)) / 2,
                                   1, 0, 2);
#undef TOP_BANNER_TEXT
        }

        draw_text_unlocked("Song Name", 2, 3, 0, 2);
        draw_text_unlocked("File Name", 2, 4, 0, 2);
        draw_text_unlocked("Order", 6, 5, 0, 2);
        draw_text_unlocked("Pattern", 4, 6, 0, 2);
        draw_text_unlocked("Row", 8, 7, 0, 2);

        draw_text_unlocked("Speed/Tempo", 38, 4, 0, 2);
        draw_text_unlocked("Octave", 43, 5, 0, 2);

        draw_text_unlocked("F1...Help       F9.....Load", 21, 6, 0, 2);
        draw_text_unlocked("ESC..Main Menu  F5/F8..Play / Stop", 21, 7, 0,
                           2);

        /* the neat-looking (but incredibly ugly to draw) borders */
        draw_char_unlocked(128, 30, 4, 3, 2);
        draw_char_unlocked(128, 57, 4, 3, 2);
        draw_char_unlocked(128, 19, 5, 3, 2);
        draw_char_unlocked(128, 51, 5, 3, 2);
        draw_char_unlocked(129, 36, 4, 3, 2);
        draw_char_unlocked(129, 50, 6, 3, 2);
        draw_char_unlocked(129, 17, 8, 3, 2);
        draw_char_unlocked(129, 18, 8, 3, 2);
        draw_char_unlocked(131, 37, 3, 3, 2);
        draw_char_unlocked(131, 78, 3, 3, 2);
        draw_char_unlocked(131, 19, 6, 3, 2);
        draw_char_unlocked(131, 19, 7, 3, 2);
        draw_char_unlocked(132, 49, 3, 1, 2);
        draw_char_unlocked(132, 49, 4, 1, 2);
        draw_char_unlocked(132, 49, 5, 1, 2);
        draw_char_unlocked(134, 75, 2, 1, 2);
        draw_char_unlocked(134, 76, 2, 1, 2);
        draw_char_unlocked(134, 77, 2, 1, 2);
        draw_char_unlocked(136, 37, 4, 3, 2);
        draw_char_unlocked(136, 78, 4, 3, 2);
        draw_char_unlocked(136, 30, 5, 3, 2);
        draw_char_unlocked(136, 57, 5, 3, 2);
        draw_char_unlocked(136, 51, 6, 3, 2);
        draw_char_unlocked(136, 19, 8, 3, 2);
        draw_char_unlocked(137, 49, 6, 3, 2);
        draw_char_unlocked(137, 11, 8, 3, 2);
        draw_char_unlocked(138, 37, 2, 1, 2);
        draw_char_unlocked(138, 78, 2, 1, 2);
        draw_char_unlocked(139, 11, 2, 1, 2);
        draw_char_unlocked(139, 49, 2, 1, 2);

        for (n = 0; n < 5; n++) {
                draw_char_unlocked(132, 11, 3 + n, 1, 2);
                draw_char_unlocked(129, 12 + n, 8, 3, 2);
                draw_char_unlocked(134, 12 + n, 2, 1, 2);
                draw_char_unlocked(129, 20 + n, 5, 3, 2);
                draw_char_unlocked(129, 31 + n, 4, 3, 2);
                draw_char_unlocked(134, 32 + n, 2, 1, 2);
                draw_char_unlocked(134, 50 + n, 2, 1, 2);
                draw_char_unlocked(129, 52 + n, 5, 3, 2);
                draw_char_unlocked(129, 58 + n, 4, 3, 2);
                draw_char_unlocked(134, 70 + n, 2, 1, 2);
        }
        for (; n < 10; n++) {
                draw_char_unlocked(134, 12 + n, 2, 1, 2);
                draw_char_unlocked(129, 20 + n, 5, 3, 2);
                draw_char_unlocked(134, 50 + n, 2, 1, 2);
                draw_char_unlocked(129, 58 + n, 4, 3, 2);
        }
        for (; n < 20; n++) {
                draw_char_unlocked(134, 12 + n, 2, 1, 2);
                draw_char_unlocked(134, 50 + n, 2, 1, 2);
                draw_char_unlocked(129, 58 + n, 4, 3, 2);
        }

        draw_text_unlocked("Time", 63, 9, 0, 2);
        draw_char_unlocked('/', 15, 5, 1, 0);
        draw_char_unlocked('/', 15, 6, 1, 0);
        draw_char_unlocked('/', 15, 7, 1, 0);
        draw_char_unlocked('/', 53, 4, 1, 0);
        draw_char_unlocked(':', 52, 3, 7, 0);
	
        SDL_UnlockSurface(screen);
	
	/* This is where the rectangle used to be drawn, but I've moved
	 * it down to where the oscilloscope is drawn in redraw_screen.
	 * Eventually I'm going to have a couple other things there, and
	 * there will be an option on the settings page (like for the time
	 * display) -- for example, it'll be possible to set it up so that
	 * an oscilloscope is drawn when the song is playing, but when it's
	 * stopped it gets replaced with a little "Schism Tracker" graphic
	 * or something. */
}

/* --------------------------------------------------------------------- */

void update_current_instrument(void)
{
        int ins_mode, n;
        char *name;
        char buf[4];

        if (page_is_instrument_list(status.current_page)
            || status.current_page == PAGE_SAMPLE_LIST)
                ins_mode = 0;
        else
                ins_mode = song_is_instrument_mode();

        if (ins_mode) {
                draw_text("Instrument", 39, 3, 0, 2);
                n = instrument_get_current();
		song_get_instrument(n, &name);
        } else {
                draw_text("    Sample", 39, 3, 0, 2);
                n = sample_get_current();
		song_get_sample(n, &name);
        }
        
        if (n > 0) {
                draw_text(numtostr(2, n, buf), 50, 3, 5, 0);
                draw_text_len(name, 25, 53, 3, 5, 0);
        } else {
                draw_text("..", 50, 3, 5, 0);
                draw_text(".........................", 53, 3, 5, 0);
        }
}

static void redraw_top_info(void)
{
        char buf[8];

        update_current_instrument();

        draw_text_len(song_get_basename(), 18, 12, 4, 5, 0);
        draw_text_len(song_get_title(), 25, 12, 3, 5, 0);

        update_current_order();
        update_current_pattern();
        update_current_row();

        draw_text(numtostr(3, song_get_current_speed(), buf), 50, 4, 5, 0);
        draw_text(numtostr(3, song_get_current_tempo(), buf), 54, 4, 5, 0);
        draw_char('0' + kbd_get_current_octave(), 50, 5, 5, 0);
}

static inline void _draw_vis_box(void)
{
	draw_box(62, 5, 78, 8, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_fill_chars(63, 6, 77, 7, 0);
}

static void vis_oscilloscope(void)
{
	SDL_Rect vis_rect = { 504, 48, 120, 16 };
	
	_draw_vis_box();
	draw_sample_data_rect(&vis_rect, audio_buffer, audio_buffer_size);
}

static void vis_vu_meter(void)
{
	int left, right;
	
	song_get_vu_meter(&left, &right);
	left /= 4;
	right /= 4;
	
	_draw_vis_box();
	draw_vu_meter(63, 6, 15, left, 5, 4);
	draw_vu_meter(63, 7, 15, right, 5, 4);
}

static void vis_fakemem(void)
{
	/* Fill it in with something relatively meaningless ;) */
	draw_text("FreeMem 206k", 63, 6, 0, 2);
	draw_text("FreeEMS 65344k", 63, 7, 0, 2);
}

/* this completely redraws everything. */
void redraw_screen(void)
{
        int n;
        char buf[4];
        SDL_Rect r = { 0, 0, 640, 400 };
	
        draw_fill_rect(&r, 2);
	
        SDL_LockSurface(screen);
        draw_char_unlocked(128, 0, 0, 3, 2);
        for (n = 79; n > 49; n--)
                draw_char_unlocked(129, n, 0, 3, 2);
        do {
                draw_char_unlocked(129, n, 0, 3, 2);
                draw_char_unlocked(131, 0, n, 3, 2);
        } while (--n);
        SDL_UnlockSurface(screen);
	
        draw_top_info_const();
        redraw_top_info();
        draw_page();
	
	if ((status.flags & CLASSIC_MODE) == 0 && song_get_mode()) {
		vis_oscilloscope();
		//vis_vu_meter();
	} else {
		vis_fakemem();
	}
	    
        draw_time();

        draw_text(numtostr(3, song_get_current_speed(), buf), 50, 4, 5, 0);
        draw_text(numtostr(3, song_get_current_tempo(), buf), 54, 4, 5, 0);

        status_text_redraw();

        SDL_Flip(screen);
}

/* important :) */
void playback_update(void)
{
        /* the order here is significant -- check_time has side effects */
        if (check_time() || song_get_mode())
                status.flags |= NEED_UPDATE;

        RUN_IF(ACTIVE_PAGE.playback_update);
}

/* --------------------------------------------------------------------- */

void set_page(int new_page)
{
        int prev_page = status.current_page;

        if (new_page != prev_page)
                status.previous_page = prev_page;
        status.current_page = new_page;
	
        if (new_page != PAGE_HELP)
                status.current_help_index = ACTIVE_PAGE.help_index;
	
        /* synchronize the sample/instrument.
         * FIXME | this isn't quite right. for instance, in impulse
         * FIXME | tracker, flipping back and forth between the sample
         * FIXME | list and instrument list will keep changing the
         * FIXME | current sample/instrument. */
        if (status.flags & SAMPLE_CHANGED) {
                if (song_is_instrument_mode())
                        instrument_synchronize_to_sample();
#if 0
		/* This breaks samples. */
                else
			instrument_set(sample_get_current());
#endif
        } else if (status.flags & INSTRUMENT_CHANGED) {
                sample_set(instrument_get_current());
        }
        status.flags &= ~(SAMPLE_CHANGED | INSTRUMENT_CHANGED);

        /* bit of ugliness to keep the sample/instrument numbers sane */
        if (page_is_instrument_list(new_page)
            && instrument_get_current() < 1)
                instrument_set(1);
        else if (new_page == PAGE_SAMPLE_LIST && sample_get_current() < 1)
                sample_set(1);

        RUN_IF(ACTIVE_PAGE.set_page);

        if (status.dialog_type & DIALOG_MENU) {
                menu_hide();
        } else {
#ifndef NDEBUG
                if (status.dialog_type != DIALOG_NONE) {
                        fprintf(stderr,
                                "set_page invoked with a dialog active:"
                                " how did this happen?\n");
                        return;
                }
#endif
		if (new_page == prev_page)
			return;
        }

        /* update the pointers */
        items = ACTIVE_PAGE.items;
        selected_item = &(ACTIVE_PAGE.selected_item);
        total_items = &(ACTIVE_PAGE.total_items);

        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

void load_pages(void)
{
        /* this probably isn't necessary, but why not. */
        memset(pages, 0, sizeof(pages));

        blank_load_page(pages + PAGE_BLANK);
        help_load_page(pages + PAGE_HELP);
        pattern_editor_load_page(pages + PAGE_PATTERN_EDITOR);
        sample_list_load_page(pages + PAGE_SAMPLE_LIST);
        instrument_list_general_load_page(pages + PAGE_INSTRUMENT_LIST_GENERAL);
        instrument_list_volume_load_page(pages + PAGE_INSTRUMENT_LIST_VOLUME);
        instrument_list_panning_load_page(pages + PAGE_INSTRUMENT_LIST_PANNING);
        instrument_list_pitch_load_page(pages + PAGE_INSTRUMENT_LIST_PITCH);
        info_load_page(pages + PAGE_INFO);
        settings_load_page(pages + PAGE_SETTINGS);
        midi_load_page(pages + PAGE_MIDI);
        load_module_load_page(pages + PAGE_LOAD_MODULE);
        orderpan_load_page(pages + PAGE_ORDERLIST_PANNING);
        ordervol_load_page(pages + PAGE_ORDERLIST_VOLUMES);
        song_vars_load_page(pages + PAGE_SONG_VARIABLES);
        palette_load_page(pages + PAGE_PALETTE_EDITOR);
        message_load_page(pages + PAGE_MESSAGE);
        log_load_page(pages + PAGE_LOG);
        load_sample_load_page(pages + PAGE_LOAD_SAMPLE);

        items = pages[PAGE_BLANK].items;
        selected_item = &(pages[PAGE_BLANK].selected_item);
        total_items = &(pages[PAGE_BLANK].total_items);
}

/* --------------------------------------------------------------------- */
/* this function's name sucks, but I don't know what else to call it. */

void main_song_changed_cb(void)
{
        int n;

        clear_all_cached_waveforms();

        /* perhaps this should be in page_patedit.c? */
        set_current_order(0);
        n = song_get_orderlist()[0];
        if (n > 199)
                n = 0;
        set_current_pattern(n);
        set_current_row(0);
        song_clear_solo_channel();

        for (n = ARRAY_SIZE(pages) - 1; n >= 0; n--)
                RUN_IF(pages[n].song_changed_cb);

        /* With Modplug, loading is sort of an atomic operation from the
         * POV of the client, so the other info IT prints wouldn't be
         * very useful. */
        if (song_get_basename()[0]) {
                log_appendf(2, "Loaded song: %s", song_get_basename());
        }
        /* TODO | print some message like "new song created" if there's
         * TODO | no filename, and thus no file. (but DON'T print it the
         * TODO | very first time this is called) */

        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* not sure where else to toss this crap */

static void exit_ok(void)
{
        /* this is be the place to prompt for a save before exiting */
        exit(0);
}

void show_exit_prompt(void)
{
        /* since this can be called with a dialog already active (on sdl
         * quit action, when the window's close button is clicked for
         * instance) it needs to get rid of any other open dialogs.
         * (dialog_create takes care of closing menus.) */
        dialog_destroy_all();
        if (status.flags & CLASSIC_MODE) {
                dialog_create(DIALOG_OK_CANCEL, "Exit Impulse Tracker?",
                              exit_ok, NULL, 0);
        } else {
                dialog_create(DIALOG_OK_CANCEL, "Exit Schism Tracker?",
                              exit_ok, NULL, 0);
        }
}

void show_song_length(void)
{
        char buf[64];   /* this is way enough space ;) */
        unsigned long length = song_get_length();

        snprintf(buf, 64, "Total song time: %3ld:%02ld:%02ld",
                 length / 3600, (length / 60) % 60, length % 60);

        dialog_create(DIALOG_OK, buf, NULL, NULL, 0);
}
