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
#include "charset.h"
#include "config.h"
#include "keyboard.h"
#include "song.h"
#include "page.h"
#include "osdefs.h"
#include "palettes.h"
#include "fonts.h"
#include "dialog.h"
#include "widget.h"
#include "vgamem.h"

#include "player/snd_gm.h"

#include "disko.h"

/* --------------------------------------------------------------------- */

#define SAVED_AT_EXIT "System configuration will be saved at exit"

static void config_set_page(void);

static struct widget widgets_config[32];

static const char *const time_displays[] = {
	"Off", "Play / Elapsed", "Play / Clock", "Play / Off", "Elapsed", "Clock", "Absolute", NULL
};
static const char *const vis_styles[] = {
	"Off", "Memory Stats", "Oscilloscope", "VU Meter", "Monoscope", "Spectrum", NULL
};

static const char *const sharp_flat[] = {
	"Sharps (#)", "Flats (b)", NULL
};

static const char *const output_channels[] = {
	"Mono", "Stereo", NULL
};

static int sample_rate_cursor = 0;

static const char *const bit_rates[] = { "8 Bit", "16 Bit", /*"24 Bit",*/ "32 Bit",
			NULL };

static const char *const midi_modes[] = {
	"IT semantics", "Tracker semantics", NULL
};

static const int video_fs_group[] = { 9, 10, -1 };
static const int video_menu_bar_group[] = { 16, 17, -1 };
static int video_group[] = { 11, 12, 13, -1 };
static int video_renderer_group[] = { 14, 15, -1 };

static void change_mixer_limits(struct widget_context *this)
{
	audio_settings.channel_limit = this->widgets[0].d.thumbbar.value;

	audio_settings.sample_rate = this->widgets[1].d.numentry.value;
	switch (this->widgets[2].d.menutoggle.state) {
	case 0: audio_settings.bits = 8; break;
	default:
	case 1: audio_settings.bits = 16; break;
	case 2: audio_settings.bits = 32; break;
	}
	audio_settings.channels = this->widgets[3].d.menutoggle.state+1;

	song_init_modplug();
	status_text_flash(SAVED_AT_EXIT);
}
static void change_ui_settings(struct widget_context *this)
{
	status.vis_style = this->widgets[4].d.menutoggle.state;
	status.time_display = this->widgets[7].d.menutoggle.state;
	if (this->widgets[5].d.toggle.state) {
		status.flags |= CLASSIC_MODE;
	} else {
		status.flags &= ~CLASSIC_MODE;
	}
	kbd_sharp_flat_toggle(this->widgets[6].d.menutoggle.state
		? KBD_SHARP_FLAT_FLATS
		: KBD_SHARP_FLAT_SHARPS);

	GM_Reset(current_song, 0);
	if (this->widgets[8].d.toggle.state) {
		status.flags |= MIDI_LIKE_TRACKER;
	} else {
		status.flags &= ~MIDI_LIKE_TRACKER;
	}

	status.flags |= NEED_UPDATE;
	status_text_flash(SAVED_AT_EXIT);
}
static int countdown = 10;
static time_t started = 0;
static int video_revert_interpolation = 0;
static int video_revert_fs = 0;
static int video_revert_hw = 0;

static void video_mode_keep(SCHISM_UNUSED void*ign, SCHISM_UNUSED void *final_data)
{
	status_text_flash(SAVED_AT_EXIT);
	config_set_page();
	status.flags |= NEED_UPDATE;
}
static void video_mode_cancel(SCHISM_UNUSED void*ign, SCHISM_UNUSED void *final_data)
{
	if (cfg_video_interpolation != video_revert_interpolation)
		video_setup(video_revert_interpolation);
	if (video_is_fullscreen() != video_revert_fs)
		video_fullscreen(-1);
	if (video_is_hardware() != video_revert_hw)
		video_set_hardware(video_revert_hw);
	palette_apply();
	font_init();
	config_set_page();
	status.flags |= NEED_UPDATE;
}

static void video_dialog_draw_const(SCHISM_UNUSED struct dialog *ign)
{
	char buf[80];
	time_t now;

	time(&now);
	if (now != started) {
		countdown--;
		time(&started); /* err... */
		status.flags |= NEED_UPDATE;
		if (countdown == 0) {
			dialog_destroy();
			video_mode_cancel(NULL, NULL);
			return;
		}
	}

	draw_text("Your video settings have been changed.", 21,19,0,2);
	sprintf(buf, "In %2d seconds, your changes will be", countdown);
	draw_text(buf, 23, 21, 0, 2);
	draw_text("reverted to the last known-good", 21, 22, 0, 2);
	draw_text("settings.", 21, 23, 0, 2);
	draw_text("To use the new video mode, and make", 21, 24, 0, 2);
	draw_text("it default, select OK.", 21, 25, 0, 2);
}

static struct widget video_dialog_widgets[2];
static void video_change_dialog(void)
{
	struct dialog *d;

	video_revert_interpolation = cfg_video_interpolation;
	video_revert_fs = video_is_fullscreen();
	video_revert_hw = video_is_hardware();

	countdown = 10;
	time(&started);

	widget_create_button(video_dialog_widgets+0, 28,28,8, 0, 0, 0, 1, 1,
					dialog_yes, "OK", 4);
	widget_create_button(video_dialog_widgets+1, 42,28,8, 1, 1, 0, 1, 0,
					dialog_cancel, "Cancel", 2);
	d = dialog_create_custom(20, 17, 40, 14,
			video_dialog_widgets,
			2, 1,
			video_dialog_draw_const, NULL, NULL);
	d->action_yes = video_mode_keep;
	d->action_no = video_mode_cancel;
	d->action_cancel = video_mode_cancel;
}

static void change_video_settings(struct widget_context *this)
{
	int new_video_interpolation;
	int new_fs_flag;
	int hw;
	int interp_changed;
	int fs_changed;
	int hw_changed;

	new_video_interpolation = this->widgets[11].d.togglebutton.state ? VIDEO_INTERPOLATION_NEAREST :
							  this->widgets[12].d.togglebutton.state ? VIDEO_INTERPOLATION_LINEAR  :
							  this->widgets[13].d.togglebutton.state ? VIDEO_INTERPOLATION_BEST    :
							  VIDEO_INTERPOLATION_NEAREST;

	new_fs_flag = this->widgets[9].d.togglebutton.state;
	hw = this->widgets[14].d.togglebutton.state;

	interp_changed = (new_video_interpolation != cfg_video_interpolation);
	fs_changed = (new_fs_flag != video_is_fullscreen());
	hw_changed = (hw != video_is_hardware());

	if (!interp_changed && !fs_changed && !hw_changed)
		return;

	video_change_dialog();
	if (interp_changed)
		video_setup(new_video_interpolation);
	if (new_fs_flag != video_is_fullscreen())
		toggle_display_fullscreen();

	if (hw != video_is_hardware())
		video_set_hardware(hw);

	palette_apply();
	font_init();
}

static void change_menu_bar_settings(struct widget_context *this) {
	cfg_video_want_menu_bar = !!this->widgets[16].d.togglebutton.state;

	video_toggle_menu(!video_is_fullscreen());
}

/* --------------------------------------------------------------------- */

static void config_draw_const(void)
{
	int n;

	draw_text("Channel Limit",4,15, 0, 2);
	draw_text("Mixing Rate",6,16, 0, 2);
	draw_text("Sample Size",6,17, 0, 2);
	draw_text("Output Channels",2,18, 0, 2);

	draw_text("Visualization",4,20, 0, 2);
	draw_text("Classic Mode",5,21, 0, 2);
	draw_text("Accidentals",6,22, 0, 2);
	draw_text("Time Display",5,23, 0, 2);

	draw_text("MIDI mode", 8,25, 0, 2);

	draw_text("Video Scaling:", 2, 28, 0, 2);
	draw_text("Video Rendering:", 2, 40, 0, 2);
	draw_text("Full Screen:", 38, 28, 0, 2);
	if (video_have_menu())
		draw_text("Menu Bar:", 38, 32, 0, 2);

	draw_fill_chars(18, 15, 34, 25, DEFAULT_FG, 0);
	draw_box(17,14,35,26, BOX_THIN | BOX_INNER | BOX_INSET);

	for (n = 18; n < 35; n++) {
		draw_char(154, n, 19, 3, 0);
		draw_char(154, n, 24, 3, 0);
	}

}
static void config_set_page(void)
{
	widgets_config[0].d.thumbbar.value = audio_settings.channel_limit;
	widgets_config[1].d.numentry.value = audio_settings.sample_rate;
	switch (audio_settings.bits) {
	case 8: widgets_config[2].d.menutoggle.state = 0; break;
	default:
	case 16: widgets_config[2].d.menutoggle.state = 1; break;
	case 32: widgets_config[2].d.menutoggle.state = 2; break;
	}
	widgets_config[3].d.menutoggle.state = audio_settings.channels-1;

	widgets_config[4].d.menutoggle.state = status.vis_style;
	widgets_config[5].d.toggle.state = !!(status.flags & CLASSIC_MODE);
	widgets_config[6].d.menutoggle.state = (kbd_sharp_flat_state() == KBD_SHARP_FLAT_FLATS);
	widgets_config[7].d.menutoggle.state = status.time_display;

	widgets_config[8].d.toggle.state = !!(status.flags & MIDI_LIKE_TRACKER);

	widgets_config[9].d.togglebutton.state = video_is_fullscreen();
	widgets_config[10].d.togglebutton.state = !video_is_fullscreen();

	widgets_config[11].d.togglebutton.state = (cfg_video_interpolation == VIDEO_INTERPOLATION_NEAREST);
	widgets_config[12].d.togglebutton.state = (cfg_video_interpolation == VIDEO_INTERPOLATION_LINEAR);
	widgets_config[13].d.togglebutton.state = (cfg_video_interpolation == VIDEO_INTERPOLATION_BEST);

	widgets_config[14].d.togglebutton.state = video_is_hardware();
	widgets_config[15].d.togglebutton.state = !video_is_hardware();

	if (video_have_menu()) {
		widgets_config[16].d.togglebutton.state = !!cfg_video_want_menu_bar;
		widgets_config[17].d.togglebutton.state = !cfg_video_want_menu_bar;
	}
}

/* --------------------------------------------------------------------- */
void config_load_page(struct page *page)
{
	page->title = "System Configuration (Ctrl-F1)";
	page->draw_const = config_draw_const;
	page->set_page = config_set_page;
	page->total_widgets = 16;
	page->widgets = widgets_config;
	page->help_index = HELP_GLOBAL;

	widget_create_thumbbar(widgets_config+0,
			18, 15, 17,
			0,1,1,
			change_mixer_limits, 4, 256);
	widget_create_numentry(widgets_config+1,
			18, 16, 7,
			0,2,2,
			change_mixer_limits,
			4000, 192000,
			&sample_rate_cursor);
	widget_create_menutoggle(widgets_config+2,
			18, 17,
			1,3,2,2,3,
			change_mixer_limits,
			bit_rates);
	widget_create_menutoggle(widgets_config+3,
			18, 18,
			2,4,3,3,4,
			change_mixer_limits,
			output_channels);
	////
	widget_create_menutoggle(widgets_config+4,
			18, 20,
			3,5,4,4,5,
			change_ui_settings,
			vis_styles);
	widget_create_toggle(widgets_config+5,
			18, 21,
			4,6,5,5,6,
			change_ui_settings);
	widget_create_menutoggle(widgets_config+6,
			18, 22,
			5,7,6,6,7,
			change_ui_settings,
			sharp_flat);
	widget_create_menutoggle(widgets_config+7,
			18, 23,
			6,8,7,7,8,
			change_ui_settings,
			time_displays);
	////
	widget_create_menutoggle(widgets_config+8,
			18, 25,
			7,11,8,8,11,
			change_ui_settings,
			midi_modes);
	////
	widget_create_togglebutton(widgets_config+9,
			44, 30, 5,
			8,9,11,10,10,
			change_video_settings,
			"Yes",
			2, video_fs_group);
	widget_create_togglebutton(widgets_config+10,
			54, 30, 5,
			10,10,9,10,0,
			change_video_settings,
			"No",
			2, video_fs_group);
	////
	widget_create_togglebutton(widgets_config+11,
			6, 30, 26,
			8,12,11,9,12,
			change_video_settings,
			"Nearest",
			2, video_group);
	widget_create_togglebutton(widgets_config+12,
			6, 33, 26,
			11,13,12,9,13,
			change_video_settings,
			"Linear",
			2, video_group);
	widget_create_togglebutton(widgets_config+13,
			6, 36, 26,
			12,14,13,9,14,
			change_video_settings,
			"Best",
			2, video_group);
	////
	widget_create_togglebutton(widgets_config+14,
			6, 42, 26,
			13,15,14,9,15,
			change_video_settings,
			"Hardware",
			2, video_renderer_group);
	widget_create_togglebutton(widgets_config+15,
			6, 45, 26,
			14,15,15,9,16,
			change_video_settings,
			"Software",
			2, video_renderer_group);
	////
	if (video_have_menu()) {
		widget_create_togglebutton(widgets_config+16,
				44, 34, 5,
				8,9,11,10,10,
				change_menu_bar_settings,
				"Yes",
				2, video_menu_bar_group);
		widget_create_togglebutton(widgets_config+17,
				54, 34, 5,
				10,10,9,10,0,
				change_menu_bar_settings,
				"No",
				2, video_menu_bar_group);
		page->total_widgets += 2;
	}
}
