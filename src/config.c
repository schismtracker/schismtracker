#include "headers.h"

#include <libcoman.h>

#include "it.h"

/* --------------------------------------------------------------------- */
/* config settings */

char cfg_dir_modules[PATH_MAX + 1], cfg_dir_samples[PATH_MAX + 1],
        cfg_dir_instruments[PATH_MAX + 1];
char cfg_font[NAME_MAX + 1];

/* --------------------------------------------------------------------- */

static void get_string_safe(const char *string, const char *def_string,
                            char save[], int max_length, int config)
{
        const char *ptr = GetString(string, config);
        if (!ptr)
                ptr = def_string;
        strncpy(save, ptr, max_length);
        save[max_length] = 0;
}

void cfg_load(void)
{
        int config;
        char filename[PATH_MAX + 1], *home_dir;

        home_dir = getenv("HOME");

        strncpy(filename, home_dir, PATH_MAX);
        strncat(filename, "/.schism/config", PATH_MAX);
        filename[PATH_MAX] = 0;
        config = InitConfig(filename);

        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

        SetGroup("Directories", config);
        get_string_safe("modules", home_dir, cfg_dir_modules,
                        PATH_MAX, config);
        get_string_safe("samples", home_dir, cfg_dir_samples,
                        PATH_MAX, config);
        get_string_safe("instruments", home_dir, cfg_dir_instruments,
                        PATH_MAX, config);

        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

        // [Info Page]
        // num_windows=3
        // window0=0,19,1
        // window1=7,3,1
        // window2=5,15,1

        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

        // [Pattern Editor]
        // link_effect_column=0
        // track_divisions=0
        // centralise_cursor=0
        // highlight_row=0
        // edit_mask=3
        // track_view_scheme=(list of 64 numbers)

        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

        // [Audio]
        // sample_rate=44100
        // bits=16
        // stereo=1
        // channel_limit=256
        // interpolation_mode=3
        // oversampling=1
        // hq_resampling=1
        // noise_reduction=1
        // headphones=0

        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

        // [Modplug DSP]
        // xbass=0
        // xbass_amount=35
        // xbass_range=50
        // surround=0
        // surround_depth=20
        // surround_delay=20
        // reverb=0
        // reverb_depth=30
        // reverb_delay=100

        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

        SetGroup("General", config);

        status.time_display = GetNumber("time_display", config);
        if (status.time_display > 0
            && status.time_display <= TIME_PLAYBACK)
                status.time_display--;
        else
                status.time_display = TIME_PLAY_ELAPSED;
        
        if (GetNumber("classic_mode", config))
                status.flags |= CLASSIC_MODE;
        else
                status.flags &= ~CLASSIC_MODE;

        // fullscreen?

        get_string_safe("font", "font.cfg", cfg_font, NAME_MAX, config);

        current_palette = GetNumber("palette", config);
        if (current_palette)
                current_palette--;
        else
                current_palette = 2;

        /* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

        /* argh! why does it insist on rewriting the file even when
         * nothing at all has changed? */
        CloseConfig("/dev/null", config);
}

void cfg_save(void)
{
        /* TODO: actually save the settings */

        /* int WriteString(const char *SettingName, const char *Value,
         *                 int config);
         * int WriteNumber(const char *SettingName, int Value,
         *                 int config); */
}
