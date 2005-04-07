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

static struct item items_preferences[32];

static const char *time_displays[] = {
        "Off", "Play / Elapsed", "Play / Clock",
        "Play / Off", "Elapsed", "Clock", NULL
};
static const char *bit_rates[] = { "8 Bit", "16 Bit", NULL };
static const char *interpolation_modes[] = {
        "Off (Nearest)", "Linear", "Cubic Spline",
        "8-Tap FIR Filter", NULL
};

static int sample_rate_cursor = 0;

/* --------------------------------------------------------------------- */

static void preferences_draw_const(void)
{
        int n;

        /* left box */
        draw_fill_chars(18, 14, 34, 27, 0);

        /* right box */
        draw_fill_chars(64, 17, 72, 27, 0);

        SDL_LockSurface(screen);

        /* left side */
        draw_box_unlocked(17, 13, 35, 28, BOX_THIN | BOX_INNER | BOX_INSET);
#if SCHISM_MIXER_CONTROL == SOUND_MIXER_VOLUME
	draw_text_unlocked("Master Volume L", 2, 14, 0, 2);
	draw_text_unlocked("Master Volume R", 2, 15, 0, 2);
#else
	draw_text_unlocked("   PCM Volume L", 2, 14, 0, 2);
	draw_text_unlocked("   PCM Volume R", 2, 15, 0, 2);
#endif
        draw_text_unlocked("  Channel Limit", 2, 17, 0, 2);
        draw_text_unlocked("    Sample Rate", 2, 18, 0, 2);
        draw_text_unlocked("        Quality", 2, 19, 0, 2);
        draw_text_unlocked("  Interpolation", 2, 20, 0, 2);
        draw_text_unlocked("   Oversampling", 2, 21, 0, 2);
        draw_text_unlocked("  HQ Resampling", 2, 22, 0, 2);
        draw_text_unlocked("Noise Reduction", 2, 23, 0, 2);
        draw_text_unlocked("Surround Effect", 2, 24, 0, 2);
        draw_text_unlocked("   Time Display", 2, 26, 0, 2);
        draw_text_unlocked("   Classic Mode", 2, 27, 0, 2);
        for (n = 18; n < 35; n++) {
                draw_char_unlocked(154, n, 16, 3, 0);
                draw_char_unlocked(154, n, 25, 3, 0);
        }

        /* right side */
        draw_box_unlocked(53, 13, 78, 29, BOX_THIN | BOX_INNER | BOX_INSET);
        draw_box_unlocked(63, 16, 73, 28, BOX_THIN | BOX_INNER | BOX_INSET);
        draw_text_unlocked("Modplug DSP Settings", 56, 15, 0, 2);
        draw_text_unlocked("   XBass", 55, 17, 0, 2);
        draw_text_unlocked("  Amount", 55, 18, 0, 2);
        draw_text_unlocked("   Range", 55, 19, 0, 2);
        draw_text_unlocked("Surround", 55, 21, 0, 2);
        draw_text_unlocked("   Depth", 55, 22, 0, 2);
        draw_text_unlocked("   Delay", 55, 23, 0, 2);
        draw_text_unlocked("  Reverb", 55, 25, 0, 2);
        draw_text_unlocked("   Depth", 55, 26, 0, 2);
        draw_text_unlocked("   Delay", 55, 27, 0, 2);
        for (n = 64; n < 73; n++) {
                draw_char_unlocked(154, n, 20, 3, 0);
                draw_char_unlocked(154, n, 24, 3, 0);
        }

	{
		const char *text = "http://rigelseven.com/schism/";
		draw_text_unlocked(text, 78 - strlen(text), 48, 1, 2);
	}
	
        SDL_UnlockSurface(screen);
}

/* --------------------------------------------------------------------- */

static void preferences_set_page(void)
{
        mixer_read_volume(&(items_preferences[0].thumbbar.value),
                                 &(items_preferences[1].thumbbar.value));

        items_preferences[2].thumbbar.value = audio_settings.channel_limit;
        items_preferences[3].numentry.value = audio_settings.sample_rate;
        items_preferences[4].menutoggle.state = !!(audio_settings.bits == 16);

        items_preferences[5].menutoggle.state = audio_settings.interpolation_mode;
        items_preferences[6].toggle.state = audio_settings.oversampling;
        items_preferences[7].toggle.state = audio_settings.hq_resampling;
        items_preferences[8].toggle.state = audio_settings.noise_reduction;
        items_preferences[9].toggle.state = song_get_surround();
        
        items_preferences[10].menutoggle.state = status.time_display;
        items_preferences[11].toggle.state = !!(status.flags & CLASSIC_MODE);
        
        items_preferences[12].toggle.state = audio_settings.xbass;
        items_preferences[13].thumbbar.value = audio_settings.xbass_amount;
        items_preferences[14].thumbbar.value = audio_settings.xbass_range;
        items_preferences[15].toggle.state = audio_settings.surround; /* not s91, the other kind */
        items_preferences[16].thumbbar.value = audio_settings.surround_depth;
        items_preferences[17].thumbbar.value = audio_settings.surround_delay;
        items_preferences[18].toggle.state = audio_settings.reverb;
        items_preferences[19].thumbbar.value = audio_settings.reverb_depth;
        items_preferences[20].thumbbar.value = audio_settings.reverb_delay;
}

/* --------------------------------------------------------------------- */

static void change_volume(void)
{
        mixer_write_volume(items_preferences[0].thumbbar.value, items_preferences[1].thumbbar.value);
}

#define SAVED_AT_EXIT "Audio configuration will be saved at exit"

static void change_audio(void)
{
	audio_settings.sample_rate = items_preferences[3].numentry.value;
	audio_settings.bits = items_preferences[4].menutoggle.state ? 16 : 8;
	song_init_audio();
	status_text_flash(SAVED_AT_EXIT);
}

static void change_misc(void)
{
	song_set_surround(items_preferences[9].toggle.state);
        status.time_display = items_preferences[10].menutoggle.state;
        if (items_preferences[11].toggle.state)
                status.flags |= CLASSIC_MODE;
        else
                status.flags &= ~CLASSIC_MODE;
}

static void change_mixer(void)
{
	audio_settings.channel_limit = items_preferences[2].thumbbar.value;
	audio_settings.interpolation_mode = items_preferences[5].menutoggle.state;
	audio_settings.oversampling = items_preferences[6].toggle.state;
	audio_settings.hq_resampling = items_preferences[7].toggle.state;
	audio_settings.noise_reduction = items_preferences[8].toggle.state;
	song_init_modplug();
	status_text_flash(SAVED_AT_EXIT);
}

static void change_modplug_dsp(void)
{
	audio_settings.xbass = items_preferences[12].toggle.state;
	audio_settings.xbass_amount = items_preferences[13].thumbbar.value;
	audio_settings.xbass_range = items_preferences[14].thumbbar.value;
	audio_settings.surround = items_preferences[15].toggle.state;
	audio_settings.surround_depth = items_preferences[16].thumbbar.value;
	audio_settings.surround_delay = items_preferences[17].thumbbar.value;
	audio_settings.reverb = items_preferences[18].toggle.state;
	audio_settings.reverb_depth = items_preferences[19].thumbbar.value;
	audio_settings.reverb_delay = items_preferences[20].thumbbar.value;
	song_init_modplug();
}

/* --------------------------------------------------------------------- */

void preferences_load_page(struct page *page)
{
	int max = mixer_get_max_volume();
	
        page->title = "Preferences (Shift-F5)";
        page->draw_const = preferences_draw_const;
        page->set_page = preferences_set_page;
        page->total_items = 21;
        page->items = items_preferences;
        page->help_index = HELP_GLOBAL;

        create_thumbbar(items_preferences + 0, 18, 14, 17, 0, 1, 12, change_volume, 0, max);
        create_thumbbar(items_preferences + 1, 18, 15, 17, 0, 2, 12, change_volume, 0, max);
        create_thumbbar(items_preferences + 2, 18, 17, 17, 1, 3, 12, change_mixer, 4, 256);
        create_numentry(items_preferences + 3, 18, 18, 7, 2, 4, 13,
			change_audio, 4000, 50000, &sample_rate_cursor);
        create_menutoggle(items_preferences + 4, 18, 19, 3, 5, 14, 14, 14, change_audio, bit_rates);
        create_menutoggle(items_preferences + 5, 18, 20, 4, 6, 15, 15, 15,
			  change_mixer, interpolation_modes);
        create_toggle(items_preferences + 6, 18, 21, 5, 7, 15, 15, 15, change_mixer);
        create_toggle(items_preferences + 7, 18, 22, 6, 8, 16, 16, 16, change_mixer);
        create_toggle(items_preferences + 8, 18, 23, 7, 9, 17, 17, 17, change_mixer);
        create_toggle(items_preferences + 9, 18, 24, 8, 10, 18, 18, 18, change_misc);
        create_menutoggle(items_preferences + 10, 18, 26, 9, 11, 19, 19, 19, change_misc, time_displays);
        create_toggle(items_preferences + 11, 18, 27, 10, 12, 20, 20, 20, change_misc);
        create_toggle(items_preferences + 12, 64, 17, 11, 13, 2, 2, 2, change_modplug_dsp);
        create_thumbbar(items_preferences + 13, 64, 18, 9, 12, 14, 3, change_modplug_dsp, 0, 100);
        create_thumbbar(items_preferences + 14, 64, 19, 9, 13, 15, 4, change_modplug_dsp, 10, 100);
        create_toggle(items_preferences + 15, 64, 21, 14, 16, 6, 6, 6, change_modplug_dsp);
        create_thumbbar(items_preferences + 16, 64, 22, 9, 15, 17, 7, change_modplug_dsp, 0, 100);
        create_thumbbar(items_preferences + 17, 64, 23, 9, 16, 18, 8, change_modplug_dsp, 5, 50);
        create_toggle(items_preferences + 18, 64, 25, 17, 19, 9, 9, 9, change_modplug_dsp);
        create_thumbbar(items_preferences + 19, 64, 26, 9, 18, 20, 10, change_modplug_dsp, 0, 100);
        create_thumbbar(items_preferences + 20, 64, 27, 9, 19, 20, 11, change_modplug_dsp, 40, 200);
}
