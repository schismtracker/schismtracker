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

#include "backend/audio.h"
#include "it.h"
#include "config.h"
#include "charset.h"
#include "song.h"
#include "page.h"
#include "osdefs.h"
#include "widget.h"
#include "vgamem.h"
#include "mem.h"

#include "disko.h"

#define VOLUME_SCALE    31

/* this page will be the first against the wall when the revolution comes */

/* --------------------------------------------------------------------- */
/* statics */

static struct page *page_ptr; /* ugh */

static struct widget widgets_preferences[49];

static const char *interpolation_modes[] = {
	"Non-Interpolated", "Linear",
	"Cubic Spline", "8-Tap FIR Filter", NULL
};
#define interp_modes ((int)ARRAY_SIZE(interpolation_modes) - 1)

static const int interp_group[] = {
	2,3,4,5,-1,
};

static int ramp_group[] = { /* not const because it is modified */
	-1,-1,-1,
};

static int selected_audio_device = 0;
static int top_audio_device = 0;

static int selected_audio_driver = 0;
static int top_audio_driver = 0;

#define AUDIO_DEVICE_BOX_X 37
#define AUDIO_DEVICE_BOX_Y 16
#define AUDIO_DEVICE_BOX_WIDTH 41
#define AUDIO_DEVICE_BOX_HEIGHT 6
#define AUDIO_DEVICE_BOX_END_X (AUDIO_DEVICE_BOX_X+AUDIO_DEVICE_BOX_WIDTH-1)
#define AUDIO_DEVICE_BOX_END_Y (AUDIO_DEVICE_BOX_Y+AUDIO_DEVICE_BOX_HEIGHT-1)

#define AUDIO_DRIVER_BOX_X 37
#define AUDIO_DRIVER_BOX_Y 25
#define AUDIO_DRIVER_BOX_WIDTH 41
#define AUDIO_DRIVER_BOX_HEIGHT 6
#define AUDIO_DRIVER_BOX_END_X (AUDIO_DRIVER_BOX_X+AUDIO_DRIVER_BOX_WIDTH-1)
#define AUDIO_DRIVER_BOX_END_Y (AUDIO_DRIVER_BOX_Y+AUDIO_DRIVER_BOX_HEIGHT-1)

/* --------------------------------------------------------------------- */

static void preferences_draw_const(void)
{
	char buf[80];
	int i;

	draw_text("Master Volume Left", 2, 14, 0, 2);
	draw_text("Master Volume Right", 2, 15, 0, 2);
	draw_box(21, 13, 27, 16, BOX_THIN | BOX_INNER | BOX_INSET);

	draw_text("Mixing Mode", 2, 18, 0, 2);

	draw_text("Available Audio Devices", AUDIO_DEVICE_BOX_X, AUDIO_DEVICE_BOX_Y - 2, 0, 2);
	draw_box(AUDIO_DEVICE_BOX_X - 1, AUDIO_DEVICE_BOX_Y - 1, AUDIO_DEVICE_BOX_END_X + 1,
		AUDIO_DEVICE_BOX_END_Y + 1, BOX_THICK | BOX_INNER | BOX_INSET);

	draw_text("Available Audio Drivers", AUDIO_DRIVER_BOX_X, AUDIO_DRIVER_BOX_Y - 2, 0, 2);
	draw_box(AUDIO_DRIVER_BOX_X - 1, AUDIO_DRIVER_BOX_Y - 1, AUDIO_DRIVER_BOX_END_X + 1,
		AUDIO_DRIVER_BOX_END_Y + 1, BOX_THICK | BOX_INNER | BOX_INSET);

	for (i = 0; interpolation_modes[i]; i++);

	draw_text("Output Equalizer", 2, 21+i*3, 0, 2);
	draw_text(     "Low Frequency Band", 7, 23+i*3, 0, 2);
	draw_text( "Med Low Frequency Band", 3, 24+i*3, 0, 2);
	draw_text("Med High Frequency Band", 2, 25+i*3, 0, 2);
	draw_text(    "High Frequency Band", 6, 26+i*3, 0, 2);

	draw_text("Ramp volume at start of sample",2,29+i*3,0,2);

	draw_box(25, 22+i*3, 47, 27+i*3, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(52, 22+i*3, 74, 27+i*3, BOX_THIN | BOX_INNER | BOX_INSET);

	snprintf(buf, 80, "Playback Frequency: %dHz", audio_settings.sample_rate);
	draw_text(buf, 2, 48, 0, 2);

#define CORNER_BOTTOM "https://schismtracker.org/"
	draw_text(CORNER_BOTTOM, 78 - sizeof(CORNER_BOTTOM) + 1, 48, 1, 2);
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

static const int audio_device_focus_offsets_[] = {1, 1, 2, 2, 2, 3};

static uint32_t audio_device_list_size_(void)
{
	/* include default device... */
	return audio_device_list_size + 1;
}

static const char *audio_device_list_name_(uint32_t i) {
	if (i > 0) {
		i--;

		SCHISM_RUNTIME_ASSERT(i < audio_device_list_size, "overflow");

		return audio_device_list[i].name;
	}

	return "default";
}

static int audio_device_list_toggled_(uint32_t i)
{
	uint32_t ts = song_audio_device_id();

	if (i > 0)
		return (ts == (i - 1));

	/* :) */
	return (ts == AUDIO_BACKEND_DEFAULT);
}

static void audio_device_list_activate_(void)
{
	struct widget *w = &ACTIVE_WIDGET;

	uint32_t id = !w->d.listbox.focus
		? AUDIO_BACKEND_DEFAULT
		: audio_device_list[w->d.listbox.focus - 1].id;

	audio_reinit(&id);
}

/* --------------------------------------------------------------------- */

static const int audio_driver_focus_offsets_[] = {4, 4, 4, 5, 5, 5};

static uint32_t audio_driver_list_size_(void)
{
	return audio_driver_count();
}

static const char *audio_driver_list_name_(uint32_t i) {
	return audio_driver_name(i);
}

static int audio_driver_list_toggled_(uint32_t i)
{
	return !strcmp(song_audio_driver(), audio_driver_name(i));
}

static void audio_driver_list_activate_(void)
{
	const char *n = audio_driver_name(ACTIVE_WIDGET.d.listbox.focus);

	audio_flash_reinitialized_text(audio_init(n, NULL));

	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

/* this is a dirty hack */
static int *page_total_widgets = NULL;

static void save_config_now(void)
{
	/* TODO */ /* uhhh, todo what? */
	cfg_midipage_save(); /* what is this doing here? */
	cfg_atexit_save();
	cfg_save_output();
	status_text_flash("Configuration saved");
}

void preferences_audio_driver_changed(const char *name)
{
	if (!page_total_widgets)
		return;

	*page_total_widgets = 15 + interp_modes + audio_has_control_panel();

	status.flags |= NEED_UPDATE;
}

static void open_control_panel(void)
{
	audio_open_control_panel();
}

void preferences_load_page(struct page *page)
{
	char buf[64];
	char *ptr;
	int i, j;

	/* initialize total num of widgets */
	page_total_widgets = &page->total_widgets;
	preferences_audio_driver_changed(song_audio_driver());

	page->title = "Preferences (Shift-F5)";
	page->draw_const = preferences_draw_const;
	page->set_page = preferences_set_page;
	page->widgets = widgets_preferences;
	page->help_index = HELP_GLOBAL;

	widget_create_thumbbar(widgets_preferences + 0, 22, 14, 5, 0, 1, 1, change_volume, 0, VOLUME_SCALE);
	widget_create_thumbbar(widgets_preferences + 1, 22, 15, 5, 0, 2, 2, change_volume, 0, VOLUME_SCALE);
	widgets_preferences[0].next.left = widgets_preferences[0].next.right =
		widgets_preferences[0].next.tab = widgets_preferences[0].next.backtab =
		widgets_preferences[1].next.left = widgets_preferences[1].next.right =
		widgets_preferences[1].next.tab = widgets_preferences[1].next.backtab =
			interp_modes + 13;


	for (i = 0; interpolation_modes[i]; i++) {
		sprintf(buf, "%d Bit, %s", audio_settings.bits, interpolation_modes[i]);
		ptr = str_dup(buf);
		widget_create_togglebutton(widgets_preferences+i+2,
					6, 20 + (i * 3), 26,
					i+1, i+3, i+2, interp_modes+11, i+3,
					change_mixer,
					ptr,
					2,
					interp_group);
		if (i < 1)
			widgets_preferences[i+2].next.left = widgets_preferences[i+2].next.right =
				interp_modes + 13;
		else if (i < 4)
			widgets_preferences[i+2].next.left = widgets_preferences[i+2].next.right =
				interp_modes + 14;
	}

	for (j = 0; j < 4; j++) {
		int n = i+(j*2);
		if (j == 0) n = i+1;
		widget_create_thumbbar(widgets_preferences+i+2+(j*2),
						26, 23+(i*3)+j,
						21,
						n, i+(j*2)+4, i+(j*2)+3,
						change_eq,
						0, 127);
		n = i+(j*2)+5;
		if (j == 3) n--;
		widget_create_thumbbar(widgets_preferences+i+3+(j*2),
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
	widget_create_togglebutton(widgets_preferences+i+10,
			33,29+i*3,9,
			i+9,i+12,i+10,i+11,i+11,
			change_mixer,
			"Enabled",2,
			ramp_group);

	widget_create_togglebutton(widgets_preferences+i+11,
			46,29+i*3,9,
			i+9,i+12,i+10,i+13,i+13,
			change_mixer,
			"Disabled",1,
			ramp_group);

	widget_create_button(widgets_preferences+i+12,
			2, 44, 27,
			i+10, i+12, i+12, i+13, i+13,
			save_config_now,
			"Save Output Configuration", 2);

	widget_create_listbox(widgets_preferences+i+13, audio_device_list_size_,
		audio_device_list_toggled_, audio_device_list_name_, NULL,
		audio_device_list_activate_, NULL, audio_device_focus_offsets_,
		audio_device_focus_offsets_, 0, i+14);
	widgets_preferences[i+13].x = AUDIO_DEVICE_BOX_X;
	widgets_preferences[i+13].y = AUDIO_DEVICE_BOX_Y;
	widgets_preferences[i+13].width = AUDIO_DEVICE_BOX_WIDTH;
	widgets_preferences[i+13].height = AUDIO_DEVICE_BOX_HEIGHT;

	widget_create_listbox(widgets_preferences+i+14, audio_driver_list_size_,
		audio_driver_list_toggled_, audio_driver_list_name_, NULL,
		audio_driver_list_activate_, NULL, audio_driver_focus_offsets_,
		audio_driver_focus_offsets_, i+13, i+3);
	widgets_preferences[i+14].x = AUDIO_DRIVER_BOX_X;
	widgets_preferences[i+14].y = AUDIO_DRIVER_BOX_Y;
	widgets_preferences[i+14].width = AUDIO_DRIVER_BOX_WIDTH;
	widgets_preferences[i+14].height = AUDIO_DRIVER_BOX_HEIGHT;

	widget_create_button(widgets_preferences+i+15,
		56, 44, 20,
		i+10, i+12, i+12, i+13, i+13,
		open_control_panel,
		"Open Control Panel", 2);
}
