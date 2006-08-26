/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
 * URL: http://rigelseven.com/schism/
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

#include "mixer.h"

#include "sdlmain.h"

#include "diskwriter.h"

/* --------------------------------------------------------------------- */

#define SAVED_AT_EXIT "System configuration will be saved at exit"

static void config_set_page(void);

static struct widget widgets_config[32];

static const char *time_displays[] = {
        "Off", "Play / Elapsed", "Play / Clock", "Play / Off", "Elapsed", "Clock", "Absolute", NULL
};
static const char *vis_styles[] = {
	"Off", "Memory Stats", "Oscilloscope", "VU Meter", NULL
};

static const char *output_channels[] = {
        "Monaural", "Stereo", "Dolby/Surround", NULL
};

static int sample_rate_cursor = 0;
static int sample_rate_cursor2 = 0;

static const char *bit_rates[] = { "8 Bit", "16 Bit", //"24 Bit", "32 Bit",
			NULL };

static int video_fs_group[] = { 8, 9, -1 };
static int video_group[] = { 10, 11, 12, 13, -1 };

static void change_mixer_limits(void)
{
	audio_settings.channel_limit = widgets_config[0].d.thumbbar.value;

	audio_settings.sample_rate = widgets_config[1].d.numentry.value;
	audio_settings.bits = widgets_config[2].d.menutoggle.state ? 16 : 8;
	audio_settings.channels = widgets_config[3].d.menutoggle.state+1;

	song_init_modplug();
	status_text_flash(SAVED_AT_EXIT);
}
static void change_ui_settings(void)
{
	status.vis_style = widgets_config[4].d.menutoggle.state;
	status.time_display = widgets_config[7].d.menutoggle.state;
	if (widgets_config[5].d.toggle.state) {
		status.flags |= CLASSIC_MODE;
	} else {
		status.flags &= ~CLASSIC_MODE;
	}
	kbd_digitrakker_voodoo(widgets_config[6].d.toggle.state);
	status.flags |= NEED_UPDATE;
	status_text_flash(SAVED_AT_EXIT);
}
static int countdown = 10;
static time_t started = 0;

static const char *video_revert_driver = 0;
static int video_revert_fs = 0;

static void video_mode_keep(UNUSED void*ign)
{
	dialog_destroy();
	status_text_flash(SAVED_AT_EXIT);
	config_set_page();
	status.flags |= NEED_UPDATE;
}
static void video_mode_cancel(UNUSED void*ign)
{
	dialog_destroy();
	if (video_revert_driver) video_init(video_revert_driver);
	video_fullscreen(video_revert_fs);
	palette_apply();
	font_init();
	config_set_page();
	status.flags |= NEED_UPDATE;
}

static void video_dialog_draw_const(void)
{
	char buf[80];
	time_t now;

	time(&now);
	if (now != started) {
		countdown--;
		time(&started); /* err... */
		status.flags |= NEED_UPDATE;
		if (countdown == 0) {
			video_mode_cancel(0);
			return;
		}
	}

	draw_text((unsigned char *) "Your video settings have been changed.", 21,19,0,2);
	sprintf(buf, "In %2d seconds, your changes will be", countdown);
	draw_text((unsigned char *) buf, 23, 21, 0, 2);
	draw_text((unsigned char *) "reverted to the last known-good", 21, 22, 0, 2);
	draw_text((unsigned char *) "settings.", 21, 23, 0, 2);
	draw_text((unsigned char *) "To use the new video mode, and make", 21, 24, 0, 2);
	draw_text((unsigned char *) "it default, press select OK below.", 21, 25, 0, 2);
}

static struct widget video_dialog_widgets[2];
static void video_change_dialog(void)
{
	struct dialog *d;

	video_revert_driver = video_driver_name();
	video_revert_fs = video_is_fullscreen();

	countdown = 10;
	time(&started);
	
	create_button(video_dialog_widgets+0, 28,28,8, 0, 0, 0, 1, 1,
					(void *) video_mode_keep, "OK", 4);
	create_button(video_dialog_widgets+1, 42,28,8, 1, 1, 0, 1, 0,
					(void *) video_mode_cancel, "Cancel", 2);
	d = dialog_create_custom(20, 17, 40, 14,
			video_dialog_widgets,
			2, 1, 
			video_dialog_draw_const, NULL);
	d->action_yes = video_mode_keep;
	d->action_no = video_mode_cancel;
	d->action_cancel = video_mode_cancel;
}

static void change_video_settings(void)
{
	const char *new_video_driver;
	int new_fs_flag;

	if (widgets_config[10].d.togglebutton.state) {
		new_video_driver = "sdl";
	} else if (widgets_config[11].d.togglebutton.state) {
		new_video_driver = "yuv";
	} else if (widgets_config[12].d.togglebutton.state) {
		new_video_driver = "gl";
	} else if (widgets_config[13].d.togglebutton.state) {
		new_video_driver = "directdraw";;
	} else {
		new_video_driver = "sdl";
	}

	if (widgets_config[8].d.togglebutton.state) {
		new_fs_flag = 1;
	} else {
		new_fs_flag = 0;
	}

	if (!strcasecmp(new_video_driver, video_driver_name())
	&& new_fs_flag == video_is_fullscreen()) {
		return;
	}

	video_change_dialog();
	if (strcasecmp(new_video_driver, video_driver_name()))
		video_init(new_video_driver);
	if (new_fs_flag != video_is_fullscreen())
		video_fullscreen(new_fs_flag);
	palette_apply();
	font_init();
}

/* --------------------------------------------------------------------- */

static void config_draw_const(void)
{
	int n;

	draw_text((unsigned char *) "Channel Limit",4,15, 0, 2);
	draw_text((unsigned char *) "Mixing Rate",6,16, 0, 2);
	draw_text((unsigned char *) "Sample Size",6,17, 0, 2);
	draw_text((unsigned char *) "Output Channels",2,18, 0, 2);

	draw_text((unsigned char *) "Visualization",4,20, 0, 2);
	draw_text((unsigned char *) "Classic Mode",5,21, 0, 2);
	draw_text((unsigned char *) "b's not #'s",6,22, 0, 2);
	draw_text((unsigned char *) "Time Display",5,23, 0, 2);

	draw_text((unsigned char *) "Video Driver:", 2, 26, 0, 2);
	draw_text((unsigned char *) "Full Screen:", 38, 26, 0, 2);

	draw_fill_chars(18, 15, 34, 23, 0);
	draw_box(17,14,35,24, BOX_THIN | BOX_INNER | BOX_INSET);

	for (n = 18; n < 35; n++) {
		draw_char(154, n, 19, 3, 0);
	}

}
static void config_set_page(void)
{
	const char *nn;

	widgets_config[0].d.thumbbar.value = audio_settings.channel_limit;
	widgets_config[1].d.numentry.value = audio_settings.sample_rate;
	widgets_config[2].d.menutoggle.state = !!(audio_settings.bits == 16);
	widgets_config[3].d.menutoggle.state = audio_settings.channels-1;

	widgets_config[4].d.menutoggle.state = status.vis_style;
	widgets_config[5].d.toggle.state = !!(status.flags & CLASSIC_MODE);
	widgets_config[6].d.toggle.state = !!(status.flags & DIGITRAKKER_VOODOO);
	widgets_config[7].d.menutoggle.state = status.time_display;

	widgets_config[8].d.togglebutton.state = video_is_fullscreen();
	widgets_config[9].d.togglebutton.state = !video_is_fullscreen();

	nn = video_driver_name();
	widgets_config[10].d.togglebutton.state = (strcasecmp(nn,"sdl") == 0);
	widgets_config[11].d.togglebutton.state = (strcasecmp(nn,"yuv") == 0);
	widgets_config[12].d.togglebutton.state = (strcasecmp(nn,"gl") == 0);
	widgets_config[13].d.togglebutton.state = (strcasecmp(nn,"directdraw") == 0);
}

/* --------------------------------------------------------------------- */
void config_load_page(struct page *page)
{
	int i;

	page->title = "System Configuration (Ctrl-F1)";
	page->draw_const = config_draw_const;
	page->set_page = config_set_page;
	page->total_widgets = 14;
	page->widgets = widgets_config;
	page->help_index = HELP_GLOBAL;
	
	create_thumbbar(widgets_config+0, 
			18, 15, 17,
			0,1,1,
			change_mixer_limits, 4, 256);
	create_numentry(widgets_config+1,
			18, 16, 7,
			0,2,2,
			change_mixer_limits,
			4000, 192000,
			&sample_rate_cursor);
	create_menutoggle(widgets_config+2,
			18, 17,
			1,3,2,2,3,
			change_mixer_limits,
			bit_rates);
	create_menutoggle(widgets_config+3,
			18, 18,
			2,4,3,3,4,
			change_mixer_limits,
			output_channels);

	create_menutoggle(widgets_config+4,
			18, 20,
			3,5,4,4,5,
			change_ui_settings,
			vis_styles);
	create_toggle(widgets_config+5,
			18, 21,
			4,6,5,5,6,
			change_ui_settings);
	create_toggle(widgets_config+6,
			18, 22,
			5,7,6,6,7,
			change_ui_settings);
	create_menutoggle(widgets_config+7,
			18, 23,
			6,10,7,7,10,
			change_ui_settings,
			time_displays);

	create_togglebutton(widgets_config+8,
			44, 28, 5,
			7,8,10,9,9,
			change_video_settings,
			"Yes",
			2, video_fs_group);
	create_togglebutton(widgets_config+9,
			54, 28, 5,
			9,9,8,9,0,
			change_video_settings,
			"No",
			2, video_fs_group);

	create_togglebutton(widgets_config+10,
			6, 28, 26,
			7,11,10,8,11,
			change_video_settings,
			"SDL Video Surface",
			2, video_group);

	create_togglebutton(widgets_config+11,
			6, 31, 26,
			10,12,11,8,12,
			change_video_settings,
			"YUV Video Overlay",
			2, video_group);

	create_togglebutton(widgets_config+12,
			6, 34, 26,
			11,13,12,8,13,
			change_video_settings,
			"OpenGL Graphic Context",
			2, video_group);

	create_togglebutton(widgets_config+13,
			6, 37, 26,
			12,13,13,8,8,
			change_video_settings,
			"DirectDraw Surface",
			2, video_group);
#ifndef WIN32
	/* patch ddraw out */
	video_group[3] = -1;
	widgets_config[13].d.togglebutton.state = 0;
	widgets_config[12].next.down = 12;
	widgets_config[12].next.tab = 8;
	page->total_widgets--;
#endif

}
