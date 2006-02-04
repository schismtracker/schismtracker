/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
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
/* statics */

static struct widget widgets_preferences[32];

static const char *interpolation_modes[] = {
        "Non-Interpolated", "Linear",
	"Cubic Spline", "8-Tap FIR Filter", NULL
};

static int interp_group[] = {
	2,3,4,5,-1,
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
	
        draw_box(25, 22+i*3, 47, 27+i*3, BOX_THIN | BOX_INNER | BOX_INSET);
        draw_box(52, 22+i*3, 74, 27+i*3, BOX_THIN | BOX_INNER | BOX_INSET);

        draw_text("Oversampling", 57, 25, 0, 2);
        draw_text("Noise Reduction", 54, 26, 0, 2);

	draw_fill_chars(73, 24, 77, 26, 0);
	draw_box(69, 24, 78, 27,  BOX_THIN | BOX_INNER | BOX_INSET);
	draw_fill_chars(73, 29, 77, 31, 0);
	draw_box(69, 28, 78, 32,  BOX_THIN | BOX_INNER | BOX_INSET);

        draw_text("XBass", 64, 29, 0, 2);
        draw_text("Reverb", 63, 30, 0, 2);
        draw_text("Surround", 61, 31, 0, 2);
#if 0
        draw_fill_chars(18, 14, 34, 28, 0); /* left box */
        draw_fill_chars(64, 17, 72, 27, 0); /* right box */
	
        /* left side */
        draw_box(17, 13, 35, 29, BOX_THIN | BOX_INNER | BOX_INSET);
#if SCHISM_MIXER_CONTROL == SOUND_MIXER_VOLUME
#else
	draw_text("   PCM Volume L", 2, 14, 0, 2);
	draw_text("   PCM Volume R", 2, 15, 0, 2);
#endif
        draw_text("  Channel Limit", 2, 17, 0, 2);
        draw_text("    Sample Rate", 2, 18, 0, 2);
        draw_text("        Quality", 2, 19, 0, 2);
        draw_text("  Interpolation", 2, 20, 0, 2);
        draw_text("   Oversampling", 2, 21, 0, 2);
        draw_text("  HQ Resampling", 2, 22, 0, 2);
        draw_text("Noise Reduction", 2, 23, 0, 2);
        draw_text("Surround Effect", 2, 24, 0, 2);
        draw_text("   Time Display", 2, 26, 0, 2);
        draw_text("   Classic Mode", 2, 27, 0, 2);
        draw_text("  Visualization", 2, 28, 0, 2);
        for (n = 18; n < 35; n++) {
                draw_char(154, n, 16, 3, 0);
                draw_char(154, n, 25, 3, 0);
        }

        /* right side */
        draw_box(53, 13, 78, 29, BOX_THIN | BOX_INNER | BOX_INSET);
        draw_box(63, 16, 73, 28, BOX_THIN | BOX_INNER | BOX_INSET);
        draw_text("Modplug DSP Settings", 56, 15, 0, 2);
        draw_text("   XBass", 55, 17, 0, 2);
        draw_text("  Amount", 55, 18, 0, 2);
        draw_text("   Range", 55, 19, 0, 2);
        draw_text("Surround", 55, 21, 0, 2);
        draw_text("   Depth", 55, 22, 0, 2);
        draw_text("   Delay", 55, 23, 0, 2);
        draw_text("  Reverb", 55, 25, 0, 2);
        draw_text("   Depth", 55, 26, 0, 2);
        draw_text("   Delay", 55, 27, 0, 2);
        for (n = 64; n < 73; n++) {
                draw_char(154, n, 20, 3, 0);
                draw_char(154, n, 24, 3, 0);
        }

	for (n = 1; n < 79; n++) draw_char(154,n, 30, 1,2);
        draw_text(" Disk Writer Output Settings ", 26, 30, 0, 2);

        draw_text("    Sample Rate", 2, 32, 0, 2);
        draw_text("        Quality", 2, 33, 0, 2);
        draw_text("Output Channels", 2, 34, 0, 2);

        draw_fill_chars(18, 32, 34, 34, 0);
        draw_box(17, 31, 35, 35, BOX_THIN | BOX_INNER | BOX_INSET);
#endif


#define CORNER_BOTTOM "http://rigelseven.com/schism/"
#ifndef RELEASE_VERSION
#define CORNER_BOTTOM2 "http://sourceforge.net/projects/schismtracker/"
	draw_text(CORNER_BOTTOM2, 78 - strlen(CORNER_BOTTOM2), 48, 1, 2);
	draw_text(CORNER_BOTTOM, 78 - strlen(CORNER_BOTTOM), 47, 1, 2);
#else
	draw_text(CORNER_BOTTOM, 78 - strlen(CORNER_BOTTOM), 48, 1, 2);
#endif
}

/* --------------------------------------------------------------------- */

static void preferences_set_page(void)
{
	int i, j;
        mixer_read_volume(&(widgets_preferences[0].d.thumbbar.value), &(widgets_preferences[1].d.thumbbar.value));

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

        widgets_preferences[i+11].d.toggle.state = audio_settings.oversampling;
        widgets_preferences[i+12].d.toggle.state = audio_settings.noise_reduction;

	widgets_preferences[i+13].d.toggle.state = audio_settings.xbass;
	widgets_preferences[i+14].d.toggle.state = audio_settings.reverb;
	widgets_preferences[i+15].d.toggle.state = audio_settings.surround;

	for (j = 0; j < 4; j++) {
		widgets_preferences[i+2+(j*2)].d.thumbbar.value
						= audio_settings.eq_freq[j];
		widgets_preferences[i+3+(j*2)].d.thumbbar.value
						= audio_settings.eq_gain[j];
	}

#if 0
        widgets_preferences[2].d.thumbbar.value = audio_settings.channel_limit;
        widgets_preferences[3].d.numentry.value = audio_settings.sample_rate;
        widgets_preferences[4].d.menutoggle.state = !!(audio_settings.bits == 16);

        widgets_preferences[5].d.menutoggle.state = audio_settings.interpolation_mode;
        widgets_preferences[6].d.toggle.state = audio_settings.oversampling;
        widgets_preferences[7].d.toggle.state = audio_settings.hq_resampling;
        widgets_preferences[8].d.toggle.state = audio_settings.noise_reduction;
        widgets_preferences[9].d.toggle.state = song_get_surround();

        widgets_preferences[10].d.menutoggle.state = status.time_display;
        widgets_preferences[11].d.toggle.state = !!(status.flags & CLASSIC_MODE);
        widgets_preferences[12].d.menutoggle.state = status.vis_style;

        widgets_preferences[13].d.toggle.state = audio_settings.xbass;
        widgets_preferences[14].d.thumbbar.value = audio_settings.xbass_amount;
        widgets_preferences[15].d.thumbbar.value = audio_settings.xbass_range;
        widgets_preferences[16].d.toggle.state = audio_settings.surround; /* not s91, the other kind */
        widgets_preferences[17].d.thumbbar.value = audio_settings.surround_depth;
        widgets_preferences[18].d.thumbbar.value = audio_settings.surround_delay;
        widgets_preferences[19].d.toggle.state = audio_settings.reverb;
        widgets_preferences[20].d.thumbbar.value = audio_settings.reverb_depth;
        widgets_preferences[21].d.thumbbar.value = audio_settings.reverb_delay;

	widgets_preferences[22].d.numentry.value = diskwriter_output_rate;
	widgets_preferences[23].d.menutoggle.state = (diskwriter_output_bits > 8) ? 1 : 0;
	widgets_preferences[24].d.menutoggle.state = diskwriter_output_channels;
#endif
}

/* --------------------------------------------------------------------- */

static void change_diskwriter(void)
{
#if 0
	diskwriter_output_rate = widgets_preferences[22].d.numentry.value;
	diskwriter_output_bits = widgets_preferences[23].d.menutoggle.state ? 16 : 8;
	diskwriter_output_channels = widgets_preferences[24].d.menutoggle.state;
#endif
}
static void change_volume(void)
{
        mixer_write_volume(widgets_preferences[0].d.thumbbar.value, widgets_preferences[1].d.thumbbar.value);
}

#define SAVED_AT_EXIT "Audio configuration will be saved at exit"

static void change_audio(void)
{
#if 0
	audio_settings.sample_rate = widgets_preferences[3].d.numentry.value;
	audio_settings.bits = widgets_preferences[4].d.menutoggle.state ? 16 : 8;
	status_text_flash(SAVED_AT_EXIT);
#endif
}

static void change_misc(void)
{
#if 0
	song_set_surround(widgets_preferences[9].d.toggle.state);
        status.time_display = widgets_preferences[10].d.menutoggle.state;
        if (widgets_preferences[11].d.toggle.state)
                status.flags |= CLASSIC_MODE;
        else
                status.flags &= ~CLASSIC_MODE;
        status.vis_style = widgets_preferences[12].d.menutoggle.state;
#endif
}

static void change_eq(void)
{
	int i,j;
	for (i = 0; interpolation_modes[i]; i++);
	for (j = 0; j < 4; j++) {
		audio_settings.eq_freq[j] = widgets_preferences[i+2+(j*2)].d.thumbbar.value;
		audio_settings.eq_gain[j] = widgets_preferences[i+3+(j*2)].d.thumbbar.value;
	}
	song_init_eq(1);
}

static struct widget dsp_dialog_widgets[4];
static const char *dsp_dialog_title = "(null)";
static const char *dsp_dialog_label1 = "(null)";
static const char *dsp_dialog_label2 = "(null)";
static int *dsp_value0;
static int *dsp_value1, dsp_value_save1;
static int *dsp_value2, dsp_value_save2;

static void draw_dsp_dialog_const(void)
{
	int len;
	len = strlen(dsp_dialog_title);
	draw_text(dsp_dialog_title, 40 - (len/2), 24, 0, 2);

	len = strlen(dsp_dialog_label1);
	draw_text(dsp_dialog_label1, 29 - len, 27, 0, 2);

	len = strlen(dsp_dialog_label2);
	draw_text(dsp_dialog_label2, 29 - len, 28, 0, 2);

        draw_box(29, 26, 55, 29, BOX_THIN | BOX_INNER | BOX_INSET);
}
static void dsp_dialog_update(UNUSED void*ign)
{
	*dsp_value1 = dsp_dialog_widgets[0].d.thumbbar.value;
	*dsp_value2 = dsp_dialog_widgets[1].d.thumbbar.value;
	song_init_modplug();
}
static void dsp_dialog_ok(UNUSED void *ign)
{
	dialog_destroy();
	status_text_flash(SAVED_AT_EXIT);
	status.flags |= NEED_UPDATE;
}
static void dsp_dialog_cancel(UNUSED void*ign)
{
	int i;

	*dsp_value0 = 0;

	for (i = 0; interpolation_modes[i]; i++);
	widgets_preferences[i+13].d.toggle.state = audio_settings.xbass;
	widgets_preferences[i+14].d.toggle.state = audio_settings.reverb;
	widgets_preferences[i+15].d.toggle.state = audio_settings.surround;

	*dsp_value1 = dsp_value_save1;
	*dsp_value2 = dsp_value_save2;
	dialog_destroy();
	dsp_dialog_update(0);
	status.flags |= NEED_UPDATE;
}
static void show_dsp_dialog(const char *text, int *ov,
		const char *label1, int low1, int high1, int *v1,
		const char *label2, int low2, int high2, int *v2)
{
	struct dialog *d;

	dsp_dialog_title = text;
	dsp_dialog_label1 = label1;
	dsp_dialog_label2 = label2;
	create_thumbbar(dsp_dialog_widgets+0, 30, 27, 25, 0, 1, 1,
					dsp_dialog_update, low1, high1);
	create_thumbbar(dsp_dialog_widgets+1, 30, 28, 25, 0, 2, 2,
					dsp_dialog_update, low1, high1);
	dsp_value0 = ov;
	dsp_value1 = v1; dsp_dialog_widgets[0].d.thumbbar.value = *v1;
	dsp_value2 = v2; dsp_dialog_widgets[1].d.thumbbar.value = *v2;
	create_button(dsp_dialog_widgets+2, 28,31,8, 1, 2, 2, 3, 3,
					dsp_dialog_ok, "OK", 4);
	create_button(dsp_dialog_widgets+3, 42,31,8, 1, 2, 2, 3, 3,
					dsp_dialog_cancel, "Cancel", 2);
	d = dialog_create_custom(20, 22, 40, 12,
			dsp_dialog_widgets,
			4, 2,
			draw_dsp_dialog_const,0);
	d->action_yes = dsp_dialog_ok;
	d->action_no = dsp_dialog_cancel;
	d->action_cancel = dsp_dialog_cancel;
}


static void change_mixer_xbass(void)
{
	int i;
	for (i = 0; interpolation_modes[i]; i++);

	audio_settings.xbass = widgets_preferences[i+13].d.toggle.state;
	song_init_modplug();

	if (!audio_settings.xbass) {
		status_text_flash(SAVED_AT_EXIT);
		return;
	}

	show_dsp_dialog("XBass Expansion", &audio_settings.xbass,
		"Amount", 0, 100, &audio_settings.xbass_amount,
		"Range", 10, 100, &audio_settings.xbass_range);
}
static void change_mixer_reverb(void)
{
	int i;
	for (i = 0; interpolation_modes[i]; i++);

	audio_settings.reverb = widgets_preferences[i+14].d.toggle.state;
	song_init_modplug();

	if (!audio_settings.reverb) {
		status_text_flash(SAVED_AT_EXIT);
		return;
	}

	show_dsp_dialog("Reverb", &audio_settings.reverb,
		"Depth", 0, 100, &audio_settings.reverb_depth,
		"Delay", 40, 200, &audio_settings.reverb_delay);
}
static void change_mixer_surround(void)
{
	int i;
	for (i = 0; interpolation_modes[i]; i++);

	audio_settings.surround = widgets_preferences[i+15].d.toggle.state;
	song_init_modplug();

	if (!audio_settings.surround) {
		status_text_flash(SAVED_AT_EXIT);
		return;
	}

	show_dsp_dialog("Surround Expansion", &audio_settings.surround,
		"Depth", 0, 100, &audio_settings.surround_depth,
		"Delay", 5, 50, &audio_settings.surround_delay);
}
static void change_mixer(void)
{
	int i;
	for (i = 0; interpolation_modes[i]; i++) {
		if (widgets_preferences[2+i].d.togglebutton.state) {
			audio_settings.interpolation_mode = i;
		}
	}
#if 0
	audio_settings.channel_limit = widgets_preferences[2].d.thumbbar.value;
	audio_settings.interpolation_mode = widgets_preferences[5].d.menutoggle.state;
	audio_settings.oversampling = widgets_preferences[6].d.toggle.state;
	audio_settings.hq_resampling = widgets_preferences[7].d.toggle.state;
	audio_settings.noise_reduction = widgets_preferences[8].d.toggle.state;
#endif
	song_init_modplug();
	status_text_flash(SAVED_AT_EXIT);
}

static void change_modplug_dsp(void)
{
#if 0
	audio_settings.xbass = widgets_preferences[13].d.toggle.state;
	audio_settings.xbass_amount = widgets_preferences[14].d.thumbbar.value;
	audio_settings.xbass_range = widgets_preferences[15].d.thumbbar.value;
	audio_settings.surround = widgets_preferences[16].d.toggle.state;
	audio_settings.surround_depth = widgets_preferences[17].d.thumbbar.value;
	audio_settings.surround_delay = widgets_preferences[18].d.thumbbar.value;
	audio_settings.reverb = widgets_preferences[19].d.toggle.state;
	audio_settings.reverb_depth = widgets_preferences[20].d.thumbbar.value;
	audio_settings.reverb_delay = widgets_preferences[21].d.thumbbar.value;
	song_init_modplug();
#endif
}

/* --------------------------------------------------------------------- */
static void save_config_now(UNUSED void *ign)
{
	/* TODO */
	cfg_midipage_save();
	cfg_atexit_save();
	status_text_flash("Configuration saved");
}

void preferences_load_page(struct page *page)
{
        int max = mixer_get_max_volume();
	char buf[64];
	char *ptr;
	int i, j, n;

        page->title = "Preferences (Shift-F5)";
        page->draw_const = preferences_draw_const;
        page->set_page = preferences_set_page;
        page->total_widgets = 16;
        page->widgets = widgets_preferences;
        page->help_index = HELP_GLOBAL;


        create_thumbbar(widgets_preferences + 0, 22, 14, 5, 0, 1, 1, change_volume, 0, max);
        create_thumbbar(widgets_preferences + 1, 22, 15, 5, 0, 2, 2, change_volume, 0, max);

	for (n = 0; interpolation_modes[n]; n++);
	for (i = 0; interpolation_modes[i]; i++) {

		sprintf(buf, "%d Bit, %s", audio_settings.bits, interpolation_modes[i]);
		ptr = strdup(buf);
		create_togglebutton(widgets_preferences+i+2,
					6, 20 + (i * 3), 26,
					i+1, i+3, i+2, n+11, i+3,
					change_mixer,
					ptr,
					2,
					&interp_group);
		page->total_widgets++;
	}

	for (j = 0; j < 4; j++) {
		n = i+(j*2);
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

	create_button(widgets_preferences+i+10,
			2, 44, 27,
			i+8, i+10, i+10, i+11, i+11,
			save_config_now,
			"Save Output Configuration", 2);

	create_toggle(widgets_preferences+i+11, /* Oversampling */
			70, 25,
			i+11,i+12,1+i, i+11,i+12,
			change_mixer);
	create_toggle(widgets_preferences+i+12,	/* Noise Reduction */
			70, 26,
			i+11,i+13,1+i, i+12,i+13,
			change_mixer);
	create_toggle(widgets_preferences+i+13,	/* XBass */
			70, 29,
			i+12,i+14,1+i, i+13,i+14,
			change_mixer_xbass);
	create_toggle(widgets_preferences+i+14,	/* Reverb */
			70, 30,
			i+13,i+15,1+i, i+14,i+15,
			change_mixer_reverb);
	create_toggle(widgets_preferences+i+15,	/* Surround */
			70, 31,
			i+14,i+15,1+i, i+15,0,
			change_mixer_surround);

			

#if 0
        create_thumbbar(widgets_preferences + 2, 18, 17, 17, 1, 3, 13, change_mixer, 4, 256);
        create_numentry(widgets_preferences + 3, 18, 18, 7, 2, 4, 14,
                        change_audio, 4000, 50000, &sample_rate_cursor);
        create_menutoggle(widgets_preferences + 4, 18, 19, 3, 5, 15, 15, 15, change_audio, bit_rates);
        create_menutoggle(widgets_preferences + 5, 18, 20, 4, 6, 16, 16, 16,
                          change_mixer, interpolation_modes);
        create_toggle(widgets_preferences + 6, 18, 21, 5, 7, 16, 16, 16, change_mixer);
        create_toggle(widgets_preferences + 7, 18, 22, 6, 8, 17, 17, 17, change_mixer);
        create_toggle(widgets_preferences + 8, 18, 23, 7, 9, 18, 18, 18, change_mixer);
        create_toggle(widgets_preferences + 9, 18, 24, 8, 10, 19, 19, 19, change_misc);
        create_menutoggle(widgets_preferences + 10, 18, 26, 9, 11, 20, 20, 20, change_misc, time_displays);
        create_toggle(widgets_preferences + 11, 18, 27, 10, 12, 21, 21, 21, change_misc);
        create_menutoggle(widgets_preferences + 12, 18, 28, 11, 22, 21, 21, 21, change_misc, vis_styles);

        create_toggle(widgets_preferences + 13, 64, 17, 12, 14, 2, 2, 2, change_modplug_dsp);
        create_thumbbar(widgets_preferences + 14, 64, 18, 9, 13, 15, 3, change_modplug_dsp, 0, 100);
        create_thumbbar(widgets_preferences + 15, 64, 19, 9, 14, 16, 4, change_modplug_dsp, 10, 100);
        create_toggle(widgets_preferences + 16, 64, 21, 15, 17, 6, 6, 6, change_modplug_dsp);
        create_thumbbar(widgets_preferences + 17, 64, 22, 9, 16, 18, 7, change_modplug_dsp, 0, 100);
        create_thumbbar(widgets_preferences + 18, 64, 23, 9, 17, 19, 8, change_modplug_dsp, 5, 50);
        create_toggle(widgets_preferences + 19, 64, 25, 18, 20, 9, 9, 9, change_modplug_dsp);
        create_thumbbar(widgets_preferences + 20, 64, 26, 9, 19, 21, 10, change_modplug_dsp, 0, 100);
        create_thumbbar(widgets_preferences + 21, 64, 27, 9, 20, 22, 11, change_modplug_dsp, 40, 200);

        create_numentry(widgets_preferences + 22, 18, 32, 7, 12, 23, 23,
                        change_diskwriter, 4000, 50000, &sample_rate_cursor2);
        create_menutoggle(widgets_preferences + 23, 18, 33, 22, 24, 23, 23, 24,
			change_diskwriter, bit_rates);
        create_menutoggle(widgets_preferences + 24, 18, 34, 23, 24, 24, 24, 24,
			change_diskwriter, output_channels);
#endif
}
