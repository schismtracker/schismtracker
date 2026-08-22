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

static struct widget widgets_preferences[49];

static const char *interpolation_modes[] = {
	"Non-Interpolated", "Linear",
	"Cubic Spline", "8-Tap FIR Filter", "256-Tap Sinc", NULL
};
#define interp_modes ((int)ARRAY_SIZE(interpolation_modes) - 1)

static int ramp_group[] = { /* not const because it is modified */
	-1,-1,-1,
};

#define INTERP_BOX_X 2
#define INTERP_BOX_Y 20
#define INTERP_BOX_WIDTH 33
#define INTERP_BOX_HEIGHT 6
#define INTERP_BOX_END_X (INTERP_BOX_X+INTERP_BOX_WIDTH-1)
#define INTERP_BOX_END_Y (INTERP_BOX_Y+INTERP_BOX_HEIGHT-1)

/* "Output Equalizer" row; other EQ widgets are offset from this. */
#define EQ_Y (AUDIO_DRIVER_BOX_END_Y + 3)

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

	draw_text("Master Volume Left", 2, 14, 0, 2);
	draw_text("Master Volume Right", 2, 15, 0, 2);
	draw_box(21, 13, 27, 16, BOX_THIN | BOX_INNER | BOX_INSET);

	draw_text("Mixing Mode", 2, 18, 0, 2);
	draw_box(INTERP_BOX_X - 1, INTERP_BOX_Y - 1, INTERP_BOX_END_X + 1,
		INTERP_BOX_END_Y + 1, BOX_THICK | BOX_INNER | BOX_INSET);

	draw_text("Available Audio Devices", AUDIO_DEVICE_BOX_X, AUDIO_DEVICE_BOX_Y - 2, 0, 2);
	draw_box(AUDIO_DEVICE_BOX_X - 1, AUDIO_DEVICE_BOX_Y - 1, AUDIO_DEVICE_BOX_END_X + 1,
		AUDIO_DEVICE_BOX_END_Y + 1, BOX_THICK | BOX_INNER | BOX_INSET);

	draw_text("Available Audio Drivers", AUDIO_DRIVER_BOX_X, AUDIO_DRIVER_BOX_Y - 2, 0, 2);
	draw_box(AUDIO_DRIVER_BOX_X - 1, AUDIO_DRIVER_BOX_Y - 1, AUDIO_DRIVER_BOX_END_X + 1,
		AUDIO_DRIVER_BOX_END_Y + 1, BOX_THICK | BOX_INNER | BOX_INSET);

	draw_text("Output Equalizer", 2, EQ_Y, 0, 2);
	draw_text(     "Low Frequency Band", 7, EQ_Y+2, 0, 2);
	draw_text( "Med Low Frequency Band", 3, EQ_Y+3, 0, 2);
	draw_text("Med High Frequency Band", 2, EQ_Y+4, 0, 2);
	draw_text(    "High Frequency Band", 6, EQ_Y+5, 0, 2);

	draw_text("Ramp volume at start of sample",2,EQ_Y+8,0,2);

	draw_box(25, EQ_Y+1, 47, EQ_Y+6, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(52, EQ_Y+1, 74, EQ_Y+6, BOX_THIN | BOX_INNER | BOX_INSET);

	snprintf(buf, 80, "Playback Frequency: %dHz", audio_settings.sample_rate);
	draw_text(buf, 2, 48, 0, 2);

#define CORNER_BOTTOM "https://schismtracker.org/"
	draw_text(CORNER_BOTTOM, 78 - sizeof(CORNER_BOTTOM) + 1, 48, 1, 2);
}

/* --------------------------------------------------------------------- */

static void preferences_set_page(void)
{
	int j;
	widgets_preferences[0].d.thumbbar.value = audio_settings.master.left;
	widgets_preferences[1].d.thumbbar.value = audio_settings.master.right;

	if (audio_settings.interpolation_mode < 0 || audio_settings.interpolation_mode >= interp_modes)
		audio_settings.interpolation_mode = 0;
	widgets_preferences[2].d.listbox.focus = audio_settings.interpolation_mode;

	for (j = 0; j < 4; j++) {
		widgets_preferences[3+(j*2)].d.thumbbar.value
						= audio_settings.eq_freq[j];
		widgets_preferences[4+(j*2)].d.thumbbar.value
						= audio_settings.eq_gain[j];
	}

	widgets_preferences[11].d.togglebutton.state
				= audio_settings.no_ramping?0:1;
	widgets_preferences[12].d.togglebutton.state
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
	int j;
	for (j = 0; j < 4; j++) {
		audio_settings.eq_freq[j] = widgets_preferences[3+(j*2)].d.thumbbar.value;
		audio_settings.eq_gain[j] = widgets_preferences[4+(j*2)].d.thumbbar.value;
	}
	song_init_eq(1, current_song->mix_frequency);
}


static void change_mixer(void)
{
	audio_settings.no_ramping = widgets_preferences[12].d.togglebutton.state;

	song_init_modplug();
	status_text_flash(SAVED_AT_EXIT);
}

/* --------------------------------------------------------------------- */

/* interpolation mode listbox (replaces the old stack of toggle buttons) */

/* one target widget per row; Tab/Left/Right from any row jumps to the device list */
static const int interp_focus_offsets_[] = {14, 14, 14, 14, 14, 14};

static uint32_t interp_list_size_(void)
{
	return interp_modes;
}

static char interp_list_buf_[64];

static const char *interp_list_name_(uint32_t i)
{
	if (i >= (uint32_t)interp_modes)
		return "";
	snprintf(interp_list_buf_, sizeof(interp_list_buf_), "%d Bit, %s",
		audio_settings.bits, interpolation_modes[i]);
	return interp_list_buf_;
}

static int interp_list_toggled_(uint32_t i)
{
	return ((int)i == audio_settings.interpolation_mode);
}

static void interp_list_changed_(void)
{
	audio_settings.interpolation_mode = ACTIVE_WIDGET.d.listbox.focus;
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
	cfg_preferences_save();
	status_text_flash("Configuration saved");
}

void preferences_audio_driver_changed(void)
{
	if (!page_total_widgets)
		return;

	/* 16 fixed widgets (0..15), plus the optional control panel button (16) */
	*page_total_widgets = 16 + audio_has_control_panel();

	status.flags |= NEED_UPDATE;
}

static void open_control_panel(void)
{
	audio_open_control_panel();
}

void preferences_load_page(struct page *page)
{
	int j;

	/* initialize total num of widgets */
	page_total_widgets = &page->total_widgets;
	preferences_audio_driver_changed();

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
			15;


	/* widgets: 2=interp list, 3..10=EQ, 11/12=ramp, 13=save, 14=device, 15=driver, 16=ctrl */
	widget_create_listbox(widgets_preferences + 2, interp_list_size_,
		interp_list_toggled_, interp_list_name_, interp_list_changed_,
		NULL, NULL, interp_focus_offsets_, interp_focus_offsets_,
		1, 3);
	widgets_preferences[2].x = INTERP_BOX_X;
	widgets_preferences[2].y = INTERP_BOX_Y;
	widgets_preferences[2].width = INTERP_BOX_WIDTH;
	widgets_preferences[2].height = INTERP_BOX_HEIGHT;
	widgets_preferences[2].next.left = widgets_preferences[2].next.right = 14;

	for (j = 0; j < 4; j++) {
		int n = (j == 0) ? 2 : 3+(j*2);
		widget_create_thumbbar(widgets_preferences+3+(j*2),
						26, EQ_Y+2+j,
						21,
						n, 3+(j*2)+2, 3+(j*2)+1,
						change_eq,
						0, 127);
		n = 3+(j*2)+3;
		if (j == 3) n--;
		widget_create_thumbbar(widgets_preferences+4+(j*2),
						53, EQ_Y+2+j,
						21,
						3+(j*2), n, 3+(j*2)+2,
						change_eq,
						0, 127);
	}
	/* default EQ setting */
	widgets_preferences[3].d.thumbbar.value = 0;
	widgets_preferences[5].d.thumbbar.value = 16;
	widgets_preferences[7].d.thumbbar.value = 96;
	widgets_preferences[9].d.thumbbar.value = 127;

	ramp_group[0] = 11;
	ramp_group[1] = 12;
	widget_create_togglebutton(widgets_preferences+11,
			33,EQ_Y+8,9,
			10,13,11,12,12,
			change_mixer,
			"Enabled",2,
			ramp_group);

	widget_create_togglebutton(widgets_preferences+12,
			46,EQ_Y+8,9,
			10,13,11,14,14,
			change_mixer,
			"Disabled",1,
			ramp_group);

	widget_create_button(widgets_preferences+13,
			2, 44, 27,
			11, 13, 13, 14, 14,
			save_config_now,
			"Save Output Configuration", 2);

	widget_create_listbox(widgets_preferences+14, audio_device_list_size_,
		audio_device_list_toggled_, audio_device_list_name_, NULL,
		audio_device_list_activate_, NULL, audio_device_focus_offsets_,
		audio_device_focus_offsets_, 0, 15);
	widgets_preferences[14].x = AUDIO_DEVICE_BOX_X;
	widgets_preferences[14].y = AUDIO_DEVICE_BOX_Y;
	widgets_preferences[14].width = AUDIO_DEVICE_BOX_WIDTH;
	widgets_preferences[14].height = AUDIO_DEVICE_BOX_HEIGHT;

	widget_create_listbox(widgets_preferences+15, audio_driver_list_size_,
		audio_driver_list_toggled_, audio_driver_list_name_, NULL,
		audio_driver_list_activate_, NULL, audio_driver_focus_offsets_,
		audio_driver_focus_offsets_, 14, 3);
	widgets_preferences[15].x = AUDIO_DRIVER_BOX_X;
	widgets_preferences[15].y = AUDIO_DRIVER_BOX_Y;
	widgets_preferences[15].width = AUDIO_DRIVER_BOX_WIDTH;
	widgets_preferences[15].height = AUDIO_DRIVER_BOX_HEIGHT;

	widget_create_button(widgets_preferences+16,
		56, 44, 20,
		11, 13, 13, 14, 14,
		open_control_panel,
		"Open Control Panel", 2);
}
