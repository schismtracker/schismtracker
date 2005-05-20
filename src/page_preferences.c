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

#include <SDL.h>

/* --------------------------------------------------------------------- */
/* statics */

static struct widget widgets_preferences[32];

static const char *time_displays[] = {
        "Off", "Play / Elapsed", "Play / Clock", "Play / Off", "Elapsed", "Clock", NULL
};
static const char *vis_styles[] = {
	"Off", "Memory Stats", "Oscilloscope", "VU Meter", NULL
};
static const char *bit_rates[] = { "8 Bit", "16 Bit", NULL };
static const char *interpolation_modes[] = {
        "Off (Nearest)", "Linear", "Cubic Spline", "8-Tap FIR Filter", NULL
};

static int sample_rate_cursor = 0;

/* --------------------------------------------------------------------- */

#define CORNER_TEXT "http://rigelseven.com/schism/"

static void preferences_draw_const(void)
{
        int n;
        
        draw_fill_chars(18, 14, 34, 28, 0); /* left box */
        draw_fill_chars(64, 17, 72, 27, 0); /* right box */
	
        /* left side */
        draw_box(17, 13, 35, 29, BOX_THIN | BOX_INNER | BOX_INSET);
#if SCHISM_MIXER_CONTROL == SOUND_MIXER_VOLUME
	draw_text("Master Volume L", 2, 14, 0, 2);
	draw_text("Master Volume R", 2, 15, 0, 2);
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

	draw_text(CORNER_TEXT, 78 - strlen(CORNER_TEXT), 48, 1, 2);
}

/* --------------------------------------------------------------------- */

static void preferences_set_page(void)
{
        mixer_read_volume(&(widgets_preferences[0].thumbbar.value), &(widgets_preferences[1].thumbbar.value));

        widgets_preferences[2].thumbbar.value = audio_settings.channel_limit;
        widgets_preferences[3].numentry.value = audio_settings.sample_rate;
        widgets_preferences[4].menutoggle.state = !!(audio_settings.bits == 16);

        widgets_preferences[5].menutoggle.state = audio_settings.interpolation_mode;
        widgets_preferences[6].toggle.state = audio_settings.oversampling;
        widgets_preferences[7].toggle.state = audio_settings.hq_resampling;
        widgets_preferences[8].toggle.state = audio_settings.noise_reduction;
        widgets_preferences[9].toggle.state = song_get_surround();

        widgets_preferences[10].menutoggle.state = status.time_display;
        widgets_preferences[11].toggle.state = !!(status.flags & CLASSIC_MODE);
        widgets_preferences[12].menutoggle.state = status.vis_style;

        widgets_preferences[13].toggle.state = audio_settings.xbass;
        widgets_preferences[14].thumbbar.value = audio_settings.xbass_amount;
        widgets_preferences[15].thumbbar.value = audio_settings.xbass_range;
        widgets_preferences[16].toggle.state = audio_settings.surround; /* not s91, the other kind */
        widgets_preferences[17].thumbbar.value = audio_settings.surround_depth;
        widgets_preferences[18].thumbbar.value = audio_settings.surround_delay;
        widgets_preferences[19].toggle.state = audio_settings.reverb;
        widgets_preferences[20].thumbbar.value = audio_settings.reverb_depth;
        widgets_preferences[21].thumbbar.value = audio_settings.reverb_delay;
}

/* --------------------------------------------------------------------- */

static void change_volume(void)
{
        mixer_write_volume(widgets_preferences[0].thumbbar.value, widgets_preferences[1].thumbbar.value);
}

#define SAVED_AT_EXIT "Audio configuration will be saved at exit"

static void change_audio(void)
{
	audio_settings.sample_rate = widgets_preferences[3].numentry.value;
	audio_settings.bits = widgets_preferences[4].menutoggle.state ? 16 : 8;
	song_init_audio();
	status_text_flash(SAVED_AT_EXIT);
}

static void change_misc(void)
{
	song_set_surround(widgets_preferences[9].toggle.state);
        status.time_display = widgets_preferences[10].menutoggle.state;
        if (widgets_preferences[11].toggle.state)
                status.flags |= CLASSIC_MODE;
        else
                status.flags &= ~CLASSIC_MODE;
        status.vis_style = widgets_preferences[12].menutoggle.state;
}

static void change_mixer(void)
{
	audio_settings.channel_limit = widgets_preferences[2].thumbbar.value;
	audio_settings.interpolation_mode = widgets_preferences[5].menutoggle.state;
	audio_settings.oversampling = widgets_preferences[6].toggle.state;
	audio_settings.hq_resampling = widgets_preferences[7].toggle.state;
	audio_settings.noise_reduction = widgets_preferences[8].toggle.state;
	song_init_modplug();
	status_text_flash(SAVED_AT_EXIT);
}

static void change_modplug_dsp(void)
{
	audio_settings.xbass = widgets_preferences[13].toggle.state;
	audio_settings.xbass_amount = widgets_preferences[14].thumbbar.value;
	audio_settings.xbass_range = widgets_preferences[15].thumbbar.value;
	audio_settings.surround = widgets_preferences[16].toggle.state;
	audio_settings.surround_depth = widgets_preferences[17].thumbbar.value;
	audio_settings.surround_delay = widgets_preferences[18].thumbbar.value;
	audio_settings.reverb = widgets_preferences[19].toggle.state;
	audio_settings.reverb_depth = widgets_preferences[20].thumbbar.value;
	audio_settings.reverb_delay = widgets_preferences[21].thumbbar.value;
	song_init_modplug();
}

/* --------------------------------------------------------------------- */

void preferences_load_page(struct page *page)
{
        int max = mixer_get_max_volume();

        page->title = "Preferences (Shift-F5)";
        page->draw_const = preferences_draw_const;
        page->set_page = preferences_set_page;
        page->total_widgets = 22;
        page->widgets = widgets_preferences;
        page->help_index = HELP_GLOBAL;

        create_thumbbar(widgets_preferences + 0, 18, 14, 17, 0, 1, 13, change_volume, 0, max);
        create_thumbbar(widgets_preferences + 1, 18, 15, 17, 0, 2, 13, change_volume, 0, max);
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
        create_menutoggle(widgets_preferences + 12, 18, 28, 11, 13, 21, 21, 21, change_misc, vis_styles);
        create_toggle(widgets_preferences + 13, 64, 17, 12, 14, 2, 2, 2, change_modplug_dsp);
        create_thumbbar(widgets_preferences + 14, 64, 18, 9, 13, 15, 3, change_modplug_dsp, 0, 100);
        create_thumbbar(widgets_preferences + 15, 64, 19, 9, 14, 16, 4, change_modplug_dsp, 10, 100);
        create_toggle(widgets_preferences + 16, 64, 21, 15, 17, 6, 6, 6, change_modplug_dsp);
        create_thumbbar(widgets_preferences + 17, 64, 22, 9, 16, 18, 7, change_modplug_dsp, 0, 100);
        create_thumbbar(widgets_preferences + 18, 64, 23, 9, 17, 19, 8, change_modplug_dsp, 5, 50);
        create_toggle(widgets_preferences + 19, 64, 25, 18, 20, 9, 9, 9, change_modplug_dsp);
        create_thumbbar(widgets_preferences + 20, 64, 26, 9, 19, 21, 10, change_modplug_dsp, 0, 100);
        create_thumbbar(widgets_preferences + 21, 64, 27, 9, 20, 21, 11, change_modplug_dsp, 40, 200);
}
