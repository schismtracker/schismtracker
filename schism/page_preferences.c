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
#include "osdefs.h"
#include "sdlmain.h"

#include "disko.h"

#define VOLUME_SCALE    31

/* this page will be the first against the wall when the revolution comes */

/* --------------------------------------------------------------------- */
/* statics */

static struct widget widgets_preferences[48];

static const char *interpolation_modes[] = {
	"Non-Interpolated", "Linear",
	"Cubic Spline", "8-Tap FIR Filter", NULL
};

static const int interp_group[] = {
	2,3,4,5,-1,
};
static int ramp_group[] = { /* not const because it is modified */
	-1,-1,-1,
};

/* --------------------------------------------------------------------- */

static void preferences_draw_const(void)
{
	char buf[80];
	int i;

	draw_text("Master Volume Left", 2, 14, 0, 2);
	draw_text("Master Volume Right", 2, 15, 0, 2);
	draw_box(21, 13, 27, 16, BOX_THIN | BOX_INNER | BOX_INSET);

	sprintf(buf, "Mixing Mode, Playback Frequency: %dHz", audio_settings.sample_rate);
	draw_text(buf, 2, 18, 0, 2);

	for (i = 0; interpolation_modes[i]; i++);

	draw_text("Output Equalizer", 2, 21+i*3, 0, 2);
	draw_text(     "Low Frequency Band", 7, 23+i*3, 0, 2);
	draw_text( "Med Low Frequency Band", 3, 24+i*3, 0, 2);
	draw_text("Med High Frequency Band", 2, 25+i*3, 0, 2);
	draw_text(    "High Frequency Band", 6, 26+i*3, 0, 2);

	draw_text("Ramp volume at start of sample",2,29+i*3,0,2);

	draw_box(25, 22+i*3, 47, 27+i*3, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(52, 22+i*3, 74, 27+i*3, BOX_THIN | BOX_INNER | BOX_INSET);

#define CORNER_BOTTOM "http://schismtracker.org/"
	draw_text(CORNER_BOTTOM, 78 - strlen(CORNER_BOTTOM), 48, 1, 2);
}

/* --------------------------------------------------------------------- */

static void preferences_set_page(void)
{
	int i, j;
	widgets_preferences[0].d.thumbbar.value = audio_settings.master.left;
	widgets_preferences[1].d.thumbbar.value = audio_settings.master.right;

	for (i = j = 0; interpolation_modes[i]; i++) {
		if (i == audio_settings.interpolation_mode) {
			widgets_preferences[i + 2].d.togglebutton.state=1;
			j = 1;
		} else {
			widgets_preferences[i + 2].d.togglebutton.state=0;
		}
	}
	if (!j) {
		audio_settings.interpolation_mode = 0;
		widgets_preferences[2].d.togglebutton.state=1;
	}

	for (j = 0; j < 4; j++) {
		widgets_preferences[i+2+(j*2)].d.thumbbar.value
						= audio_settings.eq_freq[j];
		widgets_preferences[i+3+(j*2)].d.thumbbar.value
						= audio_settings.eq_gain[j];
	}

	widgets_preferences[i+10].d.togglebutton.state
				= audio_settings.no_ramping?0:1;
	widgets_preferences[i+11].d.togglebutton.state
				= audio_settings.no_ramping?1:0;
}

/* --------------------------------------------------------------------- */

static void change_volume(void)
{
	audio_settings.master.left = widgets_preferences[0].d.thumbbar.value;
	audio_settings.master.right = widgets_preferences[1].d.thumbbar.value;
}

#define SAVED_AT_EXIT "Audio configuration will be saved at exit"

static void change_eq(void)
{
	int i,j;
	for (i = 0; interpolation_modes[i]; i++);
	for (j = 0; j < 4; j++) {
		audio_settings.eq_freq[j] = widgets_preferences[i+2+(j*2)].d.thumbbar.value;
		audio_settings.eq_gain[j] = widgets_preferences[i+3+(j*2)].d.thumbbar.value;
	}
	song_init_eq(1, current_song->mix_frequency);
}


static void change_mixer(void)
{
	int i;
	for (i = 0; interpolation_modes[i]; i++) {
		if (widgets_preferences[2+i].d.togglebutton.state) {
			audio_settings.interpolation_mode = i;
		}
	}
	audio_settings.no_ramping = widgets_preferences[i+11].d.togglebutton.state;

	song_init_modplug();
	status_text_flash(SAVED_AT_EXIT);
}

/* --------------------------------------------------------------------- */
static void save_config_now(UNUSED void *ign)
{
	/* TODO */
	cfg_midipage_save(); /* what is this doing here? */
	cfg_atexit_save();
	status_text_flash("Configuration saved");
}

void preferences_load_page(struct page *page)
{
	char buf[64];
	char *ptr;
	int i, j;
        int interp_modes;

        for (interp_modes = 0; interpolation_modes[interp_modes]; interp_modes++) {
                /* nothing */
        }

	page->title = "Preferences (Shift-F5)";
	page->draw_const = preferences_draw_const;
	page->set_page = preferences_set_page;
        page->total_widgets = 13 + interp_modes;
	page->widgets = widgets_preferences;
	page->help_index = HELP_GLOBAL;

	create_thumbbar(widgets_preferences + 0, 22, 14, 5, 0, 1, 1, change_volume, 0, VOLUME_SCALE);
	create_thumbbar(widgets_preferences + 1, 22, 15, 5, 0, 2, 2, change_volume, 0, VOLUME_SCALE);

	for (i = 0; interpolation_modes[i]; i++) {
		sprintf(buf, "%d Bit, %s", audio_settings.bits, interpolation_modes[i]);
		ptr = str_dup(buf);
		create_togglebutton(widgets_preferences+i+2,
					6, 20 + (i * 3), 26,
                                        i+1, i+3, i+2, interp_modes+11, i+3,
					change_mixer,
					ptr,
					2,
					interp_group);
	}

	for (j = 0; j < 4; j++) {
                int n = i+(j*2);
		if (j == 0) n = i+1;
		create_thumbbar(widgets_preferences+i+2+(j*2),
						26, 23+(i*3)+j,
						21,
						n, i+(j*2)+4, i+(j*2)+3,
						change_eq,
						0, 127);
		n = i+(j*2)+5;
		if (j == 3) n--;
		create_thumbbar(widgets_preferences+i+3+(j*2),
						53, 23+(i*3)+j,
						21,
						i+(j*2)+1, n, i+(j*2)+4,
						change_eq,
						0, 127);
	}
	/* default EQ setting */
	widgets_preferences[i+2].d.thumbbar.value = 0;
	widgets_preferences[i+4].d.thumbbar.value = 16;
	widgets_preferences[i+6].d.thumbbar.value = 96;
	widgets_preferences[i+8].d.thumbbar.value = 127;

	ramp_group[0] = i+10;
	ramp_group[1] = i+11;
	create_togglebutton(widgets_preferences+i+10,
			33,29+i*3,9,
			i+9,i+12,i+10,i+11,i+11,
			change_mixer,
			"Enabled",2,
			ramp_group);

	create_togglebutton(widgets_preferences+i+11,
			46,29+i*3,9,
			i+9,i+12,i+10,i+13,i+13,
			change_mixer,
			"Disabled",1,
			ramp_group);

	create_button(widgets_preferences+i+12,
			2, 44, 27,
			i+10, i+12, i+12, i+13, i+13,
			(void *) save_config_now,
			"Save Output Configuration", 2);
}

