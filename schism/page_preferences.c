/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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

#include "mixer.h"

#include "sdlmain.h"

#include "diskwriter.h"

#define VOLUME_SCALE	31

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

        draw_text("Oversampling", 57, 25, 0, 2);
        draw_text("Noise Reduction", 54, 26, 0, 2);

	draw_fill_chars(73, 24, 77, 26, 0);
	draw_box(69, 24, 78, 27,  BOX_THIN | BOX_INNER | BOX_INSET);
	draw_fill_chars(73, 29, 77, 31, 0);
	draw_box(69, 28, 78, 32,  BOX_THIN | BOX_INNER | BOX_INSET);

        draw_text("XBass", 64, 29, 0, 2);
        draw_text("Reverb", 63, 30, 0, 2);
        draw_text("Surround", 61, 31, 0, 2);

#define CORNER_BOTTOM "http://schismtracker.org/"
	draw_text(CORNER_BOTTOM, 78 - strlen(CORNER_BOTTOM), 48, 1, 2);
}

/* --------------------------------------------------------------------- */

static void preferences_set_page(void)
{
	int i, j;
	int lim = mixer_get_max_volume();
        mixer_read_volume(&i, &j);
	widgets_preferences[0].d.thumbbar.value = i * VOLUME_SCALE / lim;
	widgets_preferences[1].d.thumbbar.value = j * VOLUME_SCALE / lim;

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
        widgets_preferences[i+13].d.toggle.state = audio_settings.oversampling;
        widgets_preferences[i+14].d.toggle.state = audio_settings.noise_reduction;
	widgets_preferences[i+15].d.toggle.state = audio_settings.xbass;
	widgets_preferences[i+16].d.toggle.state = audio_settings.reverb;
	widgets_preferences[i+17].d.toggle.state = audio_settings.surround;

}

/* --------------------------------------------------------------------- */

static void change_volume(void)
{
	int lim = mixer_get_max_volume();
        mixer_write_volume(
		widgets_preferences[0].d.thumbbar.value * lim / VOLUME_SCALE,
		widgets_preferences[1].d.thumbbar.value * lim / VOLUME_SCALE);
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
	status.flags |= NEED_UPDATE;
}
static void dsp_dialog_cancel(UNUSED void*ign)
{
	int i;

	*dsp_value0 = 0;

	for (i = 0; interpolation_modes[i]; i++);
	widgets_preferences[i+15].d.toggle.state = audio_settings.xbass;
	widgets_preferences[i+16].d.toggle.state = audio_settings.reverb;
	widgets_preferences[i+17].d.toggle.state = audio_settings.surround;

	*dsp_value1 = dsp_value_save1;
	*dsp_value2 = dsp_value_save2;
	dialog_destroy();
	dsp_dialog_update(0);
	status.flags |= NEED_UPDATE;
}

/* do we need these unused parameters? */
static void show_dsp_dialog(const char *text, int *ov,
		const char *label1, int low1, int high1, int *v1,
		const char *label2, UNUSED int low2, UNUSED int high2, int *v2)
{
	struct dialog *d;

	dsp_dialog_title = text;
	dsp_dialog_label1 = label1;
	dsp_dialog_label2 = label2;
	create_thumbbar(dsp_dialog_widgets+0, 30, 27, 25, 0, 1, 1,
					(void *) dsp_dialog_update, low1, high1);
	create_thumbbar(dsp_dialog_widgets+1, 30, 28, 25, 0, 2, 2,
					(void *) dsp_dialog_update, low1, high1);
	dsp_value0 = ov;
	dsp_value1 = v1; dsp_dialog_widgets[0].d.thumbbar.value = *v1;
	dsp_value2 = v2; dsp_dialog_widgets[1].d.thumbbar.value = *v2;
	create_button(dsp_dialog_widgets+2, 28,31,8, 1, 2, 2, 3, 3,
					(void *) dsp_dialog_ok, "OK", 4);
	create_button(dsp_dialog_widgets+3, 42,31,8, 1, 2, 2, 3, 3,
					(void *) dsp_dialog_cancel, "Cancel", 2);
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

	audio_settings.xbass = widgets_preferences[i+15].d.toggle.state;
	song_init_modplug();

	if (!audio_settings.xbass) {
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

	audio_settings.reverb = widgets_preferences[i+16].d.toggle.state;
	song_init_modplug();

	if (!audio_settings.reverb) {
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

	audio_settings.surround = widgets_preferences[i+17].d.toggle.state;
	song_init_modplug();

	if (!audio_settings.surround) {
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
	audio_settings.oversampling = widgets_preferences[i+13].d.toggle.state;
	audio_settings.noise_reduction = widgets_preferences[i+14].d.toggle.state;
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
	int i, j, n;

        page->title = "Preferences (Shift-F5)";
        page->draw_const = preferences_draw_const;
        page->set_page = preferences_set_page;
        page->total_widgets = 18;
        page->widgets = widgets_preferences;
        page->help_index = HELP_GLOBAL;


        create_thumbbar(widgets_preferences + 0, 22, 14, 5, 0, 1, 1, change_volume, 0, VOLUME_SCALE);
        create_thumbbar(widgets_preferences + 1, 22, 15, 5, 0, 2, 2, change_volume, 0, VOLUME_SCALE);

	for (n = 0; interpolation_modes[n]; n++);
	for (i = 0; interpolation_modes[i]; i++) {

		sprintf(buf, "%d Bit, %s", audio_settings.bits, interpolation_modes[i]);
		ptr = str_dup(buf);
		create_togglebutton(widgets_preferences+i+2,
					6, 20 + (i * 3), 26,
					i+1, i+3, i+2, n+11, i+3,
					change_mixer,
					ptr,
					2,
					interp_group);
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

	create_toggle(widgets_preferences+i+13, /* Oversampling */
			70, 25,
			i+13,i+14,1+i, i+13,i+14,
			change_mixer);
	create_toggle(widgets_preferences+i+14,	/* Noise Reduction */
			70, 26,
			i+13,i+15,1+i, i+14,i+15,
			change_mixer);
	create_toggle(widgets_preferences+i+15,	/* XBass */
			70, 29,
			i+14,i+16,1+i, i+15,i+16,
			change_mixer_xbass);
	create_toggle(widgets_preferences+i+16,	/* Reverb */
			70, 30,
			i+15,i+17,1+i, i+16,i+17,
			change_mixer_reverb);
	create_toggle(widgets_preferences+i+17,	/* Surround */
			70, 31,
			i+16,i+17,1+i, i+17,0,
			change_mixer_surround);
}
