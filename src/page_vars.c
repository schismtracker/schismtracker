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

#include "headers.h"

#include "it.h"
#include "song.h"
#include "page.h"

#include <SDL.h>

/* --------------------------------------------------------------------- */
/* static variables */

static struct item items_vars[18];
static int group_control[] = { 8, 9, -1 };
static int group_playback[] = { 10, 11, -1 };
static int group_slides[] = { 12, 13, -1 };

/* --------------------------------------------------------------------- */

static inline void update_song_title(void)
{
        draw_text_len(song_get_title(), 25, 12, 3, 5, 0);
        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static void song_vars_draw_const(void)
{
        int n;

        draw_box(16, 15, 43, 17, BOX_THIN | BOX_INNER | BOX_INSET);
        draw_box(16, 18, 50, 21, BOX_THIN | BOX_INNER | BOX_INSET);
        draw_box(16, 22, 34, 28, BOX_THIN | BOX_INNER | BOX_INSET);
        draw_box(12, 41, 78, 45, BOX_THICK | BOX_INNER | BOX_INSET);

        draw_fill_chars(20, 26, 33, 27, 0);

        SDL_LockSurface(screen);

        draw_text_unlocked("Song Variables", 33, 13, 3, 2);
        draw_text_unlocked("Song Name", 7, 16, 0, 2);
        draw_text_unlocked("Initial Tempo", 3, 19, 0, 2);
        draw_text_unlocked("Initial Speed", 3, 20, 0, 2);
        draw_text_unlocked("Global Volume", 3, 23, 0, 2);
        draw_text_unlocked("Mixing Volume", 3, 24, 0, 2);
        draw_text_unlocked("Separation", 6, 25, 0, 2);
        draw_text_unlocked("Old Effects", 5, 26, 0, 2);
        draw_text_unlocked("Compatible Gxx", 2, 27, 0, 2);
        draw_text_unlocked("Control", 9, 30, 0, 2);
        draw_text_unlocked("Playback", 8, 33, 0, 2);
        draw_text_unlocked("Pitch Slides", 4, 36, 0, 2);
        draw_text_unlocked("Directories", 34, 40, 3, 2);
        draw_text_unlocked("Module", 6, 42, 0, 2);
        draw_text_unlocked("Sample", 6, 43, 0, 2);
        draw_text_unlocked("Instrument", 2, 44, 0, 2);

        for (n = 1; n < 79; n++)
                draw_char_unlocked(129, n, 39, 1, 2);

        SDL_UnlockSurface(screen);
}

/* --------------------------------------------------------------------- */

static void update_values_in_song(void)
{
        song_set_initial_tempo(items_vars[1].thumbbar.value);
        song_set_initial_speed(items_vars[2].thumbbar.value);
        song_set_initial_global_volume(items_vars[3].thumbbar.value);
        song_set_mixing_volume(items_vars[4].thumbbar.value);
	song_set_separation(items_vars[5].thumbbar.value);
        song_set_old_effects(items_vars[6].toggle.state);
        song_set_compatible_gxx(items_vars[7].toggle.state);
        /* TODO: instrument/sample mode. how does this work? */
        /* TODO: stereo/mono (would involve resetting the device) */
        song_set_linear_pitch_slides(items_vars[12].togglebutton.state);
}

static void song_changed_cb(void)
{
        items_vars[0].textentry.text = song_get_title();
        items_vars[0].textentry.cursor_pos =
                strlen(items_vars[0].textentry.text);

        items_vars[1].thumbbar.value = song_get_initial_tempo();
        items_vars[2].thumbbar.value = song_get_initial_speed();
        items_vars[3].thumbbar.value = song_get_initial_global_volume();
        items_vars[4].thumbbar.value = song_get_mixing_volume();
	items_vars[5].thumbbar.value = song_get_separation();
        items_vars[6].toggle.state = song_has_old_effects();
        items_vars[7].toggle.state = song_has_compatible_gxx();

        if (song_is_instrument_mode())
		togglebutton_set(items_vars, 8, 0);
        else
		togglebutton_set(items_vars, 9, 0);
	
        if (song_is_stereo())
		togglebutton_set(items_vars, 10, 0);
        else
		togglebutton_set(items_vars, 11, 0);
        
	if (song_has_linear_pitch_slides())
		togglebutton_set(items_vars, 12, 0);
        else
		togglebutton_set(items_vars, 13, 0);

        update_song_title();
}

/* --------------------------------------------------------------------- */
/* bleh */

static void dir_modules_changed(void)
{
        status.flags |= DIR_MODULES_CHANGED;
}

static void dir_samples_changed(void)
{
        status.flags |= DIR_SAMPLES_CHANGED;
}

static void dir_instruments_changed(void)
{
        status.flags |= DIR_INSTRUMENTS_CHANGED;
}

/* --------------------------------------------------------------------- */

void song_vars_load_page(struct page *page)
{
        page->title = "Song Variables & Directory Configuration (F12)";
        page->draw_const = song_vars_draw_const;
        page->song_changed_cb = song_changed_cb;
        page->total_items = 18;
        page->items = items_vars;
        page->help_index = HELP_GLOBAL;

        /* 0 = song name */
        create_textentry(items_vars, 17, 16, 26, 0, 1, 1,
                         update_song_title, song_get_title(), 25);
        /* 1 = tempo */
        create_thumbbar(items_vars + 1, 17, 19, 33, 0, 2, 2,
                        update_values_in_song, 31, 255);
        /* 2 = speed */
        create_thumbbar(items_vars + 2, 17, 20, 33, 1, 3, 3,
                        update_values_in_song, 1, 255);
        /* 3 = global volume */
        create_thumbbar(items_vars + 3, 17, 23, 17, 2, 4, 4,
                        update_values_in_song, 0, 128);
        /* 4 = mixing volume */
        create_thumbbar(items_vars + 4, 17, 24, 17, 3, 5, 5,
                        update_values_in_song, 0, 128);
        /* 5 = separation */
        create_thumbbar(items_vars + 5, 17, 25, 17, 4, 6, 6,
			update_values_in_song, 0, 128);
        /* 6 = old effects */
        create_toggle(items_vars + 6, 17, 26, 5, 7, 5, 7, 7,
                      update_values_in_song);
        /* 7 = compatible gxx */
        create_toggle(items_vars + 7, 17, 27, 6, 8, 6, 8, 8,
                      update_values_in_song);
        /* 8-13 = switches */
        create_togglebutton(items_vars + 8, 17, 30, 11, 7, 10, 9, 9, 9,
                            update_values_in_song, "Instruments", 1,
                            group_control);
        create_togglebutton(items_vars + 9, 32, 30, 11, 7, 11, 8, 8, 8,
                            update_values_in_song, "Samples", 1,
                            group_control);
        create_togglebutton(items_vars + 10, 17, 33, 11, 8, 12, 11, 11, 11,
                            update_values_in_song, "Stereo", 1,
                            group_playback);
        create_togglebutton(items_vars + 11, 32, 33, 11, 9, 13, 10, 10, 10,
                            update_values_in_song, "Mono", 1,
                            group_playback);
        create_togglebutton(items_vars + 12, 17, 36, 11, 10, 14, 13, 13,
                            13, update_values_in_song, "Linear", 1,
                            group_slides);
        create_togglebutton(items_vars + 13, 32, 36, 11, 11, 14, 12, 12,
                            12, update_values_in_song, "Amiga", 1,
                            group_slides);
        /* 14-16 = directories */
        create_textentry(items_vars + 14, 13, 42, 65, 12, 15, 15,
                         dir_modules_changed, cfg_dir_modules, PATH_MAX);
        create_textentry(items_vars + 15, 13, 43, 65, 14, 16, 16,
                         dir_samples_changed, cfg_dir_samples, PATH_MAX);
        create_textentry(items_vars + 16, 13, 44, 65, 15, 17, 17,
                         dir_instruments_changed, cfg_dir_instruments,
			 PATH_MAX);
        /* 17 = save all preferences */
        create_button(items_vars + 17, 28, 47, 22, 16, 17, 17, 17, 17,
                      cfg_save, "Save all Preferences", 2);
}
