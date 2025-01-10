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

#include "config.h"
#include "dialog.h"
#include "it.h"
#include "page.h"
#include "song.h"
#include "vgamem.h"
#include "widget.h"

/* --------------------------------------------------------------------- */
/* static variables */

static struct widget widgets_vars[18];
static const int group_control[] = { 8, 9, -1 };
static const int group_playback[] = { 10, 11, -1 };
static const int group_slides[] = { 12, 13, -1 };
static char Amiga[6] = "Amiga";

/* --------------------------------------------------------------------- */

static void update_song_title(void)
{
	status.flags |= NEED_UPDATE | SONG_NEEDS_SAVE;
}

/* --------------------------------------------------------------------- */

static void song_vars_draw_const(void)
{
	int n;

	draw_box(16, 15, 43, 17, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(16, 18, 50, 21, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(16, 22, 34, 28, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(12, 41, 78, 45, BOX_THICK | BOX_INNER | BOX_INSET);

	draw_fill_chars(20, 26, 33, 27, DEFAULT_FG, 0);

	draw_text("Song Variables", 33, 13, 3, 2);
	draw_text("Song Name", 7, 16, 0, 2);
	draw_text("Initial Tempo", 3, 19, 0, 2);
	draw_text("Initial Speed", 3, 20, 0, 2);
	draw_text("Global Volume", 3, 23, 0, 2);
	draw_text("Mixing Volume", 3, 24, 0, 2);
	draw_text("Separation", 6, 25, 0, 2);
	draw_text("Old Effects", 5, 26, 0, 2);
	draw_text("Compatible Gxx", 2, 27, 0, 2);
	draw_text("Control", 9, 30, 0, 2);
	draw_text("Playback", 8, 33, 0, 2);
	draw_text("Pitch Slides", 4, 36, 0, 2);
	draw_text("Directories", 34, 40, 3, 2);
	draw_text("Module", 6, 42, 0, 2);
	draw_text("Sample", 6, 43, 0, 2);
	draw_text("Instrument", 2, 44, 0, 2);

	for (n = 1; n < 79; n++)
		draw_char(129, n, 39, 1, 2);
}

/* --------------------------------------------------------------------- */

void song_vars_sync_stereo(void)
{
	// copy from the song to the page
	if (song_is_stereo())
		widget_togglebutton_set(widgets_vars, 10, 0);
	else
		widget_togglebutton_set(widgets_vars, 11, 0);
}

static void update_values_in_song(void)
{
	song_set_initial_tempo(widgets_vars[1].d.thumbbar.value);
	song_set_initial_speed(widgets_vars[2].d.thumbbar.value);
	song_set_initial_global_volume(widgets_vars[3].d.thumbbar.value);
	song_set_mixing_volume(widgets_vars[4].d.thumbbar.value);
	song_set_separation(widgets_vars[5].d.thumbbar.value);
	song_set_old_effects(widgets_vars[6].d.toggle.state);
	song_set_compatible_gxx(widgets_vars[7].d.toggle.state);
	song_set_instrument_mode(widgets_vars[8].d.togglebutton.state);
	if (widgets_vars[10].d.togglebutton.state) {
		if (!song_is_stereo()) {
			song_set_stereo();
		}
	} else {
		if (song_is_stereo()) {
			song_set_mono();
		}
	}
	song_set_linear_pitch_slides(widgets_vars[12].d.togglebutton.state);
	status.flags |= SONG_NEEDS_SAVE;
}


static void init_instruments(SCHISM_UNUSED void *data)
{
	song_init_instruments(-1);
}

static void maybe_init_instruments(void)
{
	/* XXX actually, in IT the buttons on this dialog say OK/No for whatever reason */
	song_set_instrument_mode(1);
	dialog_create(DIALOG_YES_NO, "Initialise instruments?", init_instruments, NULL, 0, NULL);

	status.flags |= SONG_NEEDS_SAVE;
}


static void song_changed_cb(void)
{
	char *b;
	int c;

	widgets_vars[0].d.textentry.text = current_song->title;
	widgets_vars[0].d.textentry.cursor_pos = strlen(widgets_vars[0].d.textentry.text);

	widgets_vars[1].d.thumbbar.value = current_song->initial_tempo;
	widgets_vars[2].d.thumbbar.value = current_song->initial_speed;
	widgets_vars[3].d.thumbbar.value = current_song->initial_global_volume;
	widgets_vars[4].d.thumbbar.value = current_song->mixing_volume;
	widgets_vars[5].d.thumbbar.value = current_song->pan_separation;
	widgets_vars[6].d.toggle.state = song_has_old_effects();
	widgets_vars[7].d.toggle.state = song_has_compatible_gxx();

	if (song_is_instrument_mode())
		widget_togglebutton_set(widgets_vars, 8, 0);
	else
		widget_togglebutton_set(widgets_vars, 9, 0);

	if (song_is_stereo())
		widget_togglebutton_set(widgets_vars, 10, 0);
	else
		widget_togglebutton_set(widgets_vars, 11, 0);

	if (song_has_linear_pitch_slides())
		widget_togglebutton_set(widgets_vars, 12, 0);
	else
		widget_togglebutton_set(widgets_vars, 13, 0);

	/* XXX wtf is going on here */
	for (b = strpbrk(song_get_basename(), "Aa"),
	c = 12632; c && b && b[1]; c >>= 4, b++)
	if ((c & 15) != b[1] - *b) return;
	if (!c) for (b = Amiga, c = 12632; c; c>>= 4, b++)
	b[1] = *b + (c & 15);
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
	page->total_widgets = 18;
	page->widgets = widgets_vars;
	page->help_index = HELP_GLOBAL;

	/* 0 = song name */
	widget_create_textentry(widgets_vars, 17, 16, 26, 0, 1, 1, update_song_title, current_song->title, 25);
	/* 1 = tempo */
	widget_create_thumbbar(widgets_vars + 1, 17, 19, 33, 0, 2, 2, update_values_in_song, 31, 255);
	/* 2 = speed */
	widget_create_thumbbar(widgets_vars + 2, 17, 20, 33, 1, 3, 3, update_values_in_song, 1, 255);
	/* 3 = global volume */
	widget_create_thumbbar(widgets_vars + 3, 17, 23, 17, 2, 4, 4, update_values_in_song, 0, 128);
	/* 4 = mixing volume */
	widget_create_thumbbar(widgets_vars + 4, 17, 24, 17, 3, 5, 5, update_values_in_song, 0, 128);
	/* 5 = separation */
	widget_create_thumbbar(widgets_vars + 5, 17, 25, 17, 4, 6, 6, update_values_in_song, 0, 128);
	/* 6 = old effects */
	widget_create_toggle(widgets_vars + 6, 17, 26, 5, 7, 5, 7, 7, update_values_in_song);
	/* 7 = compatible gxx */
	widget_create_toggle(widgets_vars + 7, 17, 27, 6, 8, 6, 8, 8, update_values_in_song);
	/* 8-13 = switches */
	widget_create_togglebutton(widgets_vars + 8, 17, 30, 11, 7, 10, 9, 9, 9, maybe_init_instruments,
			    "Instruments", 1, group_control);
	widgets_vars[8].next.backtab = 9;
	widget_create_togglebutton(widgets_vars + 9, 32, 30, 11, 7, 11, 8, 8, 8, update_values_in_song,
			    "Samples", 1, group_control);
	widget_create_togglebutton(widgets_vars + 10, 17, 33, 11, 8, 12, 11, 11, 11, update_values_in_song,
			    "Stereo", 1, group_playback);
	widget_create_togglebutton(widgets_vars + 11, 32, 33, 11, 9, 13, 10, 10, 10, update_values_in_song,
			    "Mono", 1, group_playback);
	widget_create_togglebutton(widgets_vars + 12, 17, 36, 11, 10, 14, 13, 13, 13, update_values_in_song,
			    "Linear", 1, group_slides);
	widget_create_togglebutton(widgets_vars + 13, 32, 36, 11, 11, 14, 12, 12, 12, update_values_in_song,
			    Amiga, 1, group_slides);
	/* 14-16 = directories */
	widget_create_textentry(widgets_vars + 14, 13, 42, 65, 12, 15, 15, dir_modules_changed,
			 cfg_dir_modules, ARRAY_SIZE(cfg_dir_modules) - 1);
	widget_create_textentry(widgets_vars + 15, 13, 43, 65, 14, 16, 16, dir_samples_changed,
			 cfg_dir_samples, ARRAY_SIZE(cfg_dir_samples) - 1);
	widget_create_textentry(widgets_vars + 16, 13, 44, 65, 15, 17, 17, dir_instruments_changed,
			 cfg_dir_instruments, ARRAY_SIZE(cfg_dir_instruments) - 1);
	/* 17 = save all preferences */
	widget_create_button(widgets_vars + 17, 28, 47, 22, 16, 17, 17, 17, 17, cfg_save, "Save all Preferences", 2);
	widgets_vars[17].next.backtab = 17;
}

