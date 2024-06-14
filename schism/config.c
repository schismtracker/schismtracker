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
#include "video.h" /* shouldn't need this */
#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "config-parser.h"
#include "dmoz.h"
#include "osdefs.h"

/* --------------------------------------------------------------------- */
/* config settings */

char cfg_dir_modules[PATH_MAX + 1], cfg_dir_samples[PATH_MAX + 1], cfg_dir_instruments[PATH_MAX + 1],
	cfg_dir_dotschism[PATH_MAX + 1], cfg_font[NAME_MAX + 1];
char cfg_video_interpolation[8];
int cfg_video_fullscreen = 0;
int cfg_video_want_fixed = 0;
int cfg_video_want_fixed_width = 0;
int cfg_video_want_fixed_height = 0;
int cfg_video_mousecursor = MOUSE_EMULATED;
int cfg_video_width, cfg_video_height;
#ifdef SCHISM_WIN32
int cfg_video_want_menu_bar = 1;
#endif

/* --------------------------------------------------------------------- */

#if defined(SCHISM_WIN32)
# define DOT_SCHISM "Schism Tracker"
#elif defined(SCHISM_MACOSX)
# define DOT_SCHISM "Library/Application Support/Schism Tracker"
#elif defined(SCHISM_WII)
# define DOT_SCHISM "."
#else
# define DOT_SCHISM ".schism"
#endif

void cfg_init_dir(void)
{
#if defined(__amigaos4__)
	strcpy(cfg_dir_dotschism, "PROGDIR:");
#else
	char *cur_dir, *portable_file;

	cur_dir = get_current_directory();
	portable_file = dmoz_path_concat(cur_dir, "portable.txt");

	if(is_file(portable_file)) {
		printf("In portable mode.\n");

		strncpy(cfg_dir_dotschism, cur_dir, PATH_MAX);
		cfg_dir_dotschism[PATH_MAX] = 0;
	} else {
		char *dot_dir, *ptr;

		dot_dir = get_dot_directory();
		ptr = dmoz_path_concat(dot_dir, DOT_SCHISM);
		strncpy(cfg_dir_dotschism, ptr, PATH_MAX);
		cfg_dir_dotschism[PATH_MAX] = 0;
		free(dot_dir);
		free(ptr);

		if (!is_directory(cfg_dir_dotschism)) {
			printf("Creating directory %s\n", cfg_dir_dotschism);
			printf("Schism Tracker uses this directory to store your settings.\n");
			if (os_mkdir(cfg_dir_dotschism, 0777) != 0) {
				perror("Error creating directory");
				fprintf(stderr, "Everything will still work, but preferences will not be saved.\n");
			}
		}
	}

	free(cur_dir);
	free(portable_file);
#endif
}

/* --------------------------------------------------------------------- */

static const char palette_trans[64] = ".0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";
static void cfg_load_palette(cfg_file_t *cfg)
{
	char palette_text[49] = "";
	cfg_get_string(cfg, "General", "palette_cur", palette_text, 48, "");

	if(palette_text[0]) {
		set_palette_from_string(palette_text);
	}

	palette_load_preset(cfg_get_number(cfg, "General", "palette", 2));
}

static void cfg_save_palette(cfg_file_t *cfg)
{
	cfg_set_number(cfg, "General", "palette", current_palette_index);

	if(current_palette_index == 0) {
		char palette_text[49] = "";
		palette_to_string(current_palette_index, palette_text);
		cfg_set_string(cfg, "General", "palette_cur", palette_text);
	}
}

/* --------------------------------------------------------------------------------------------------------- */

void cfg_load(void)
{
	char *tmp;
	const char *ptr;
	int i;
	cfg_file_t cfg;

	tmp = dmoz_path_concat(cfg_dir_dotschism, "config");
	cfg_init(&cfg, tmp);
	free(tmp);

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	cfg_get_string(&cfg, "Video", "interpolation", cfg_video_interpolation, 7, "nearest");
	cfg_video_width = cfg_get_number(&cfg, "Video", "width", 640);
	cfg_video_height = cfg_get_number(&cfg, "Video", "height", 400);
	cfg_video_fullscreen = !!cfg_get_number(&cfg, "Video", "fullscreen", 0);
	cfg_video_want_fixed = cfg_get_number(&cfg, "Video", "want_fixed", 0);
	cfg_video_want_fixed_width = cfg_get_number(&cfg, "Video", "want_fixed_width", 640 * 5);
	cfg_video_want_fixed_height = cfg_get_number(&cfg, "Video", "want_fixed_height", 400 * 6);
	cfg_video_mousecursor = cfg_get_number(&cfg, "Video", "mouse_cursor", MOUSE_EMULATED);
	cfg_video_mousecursor = CLAMP(cfg_video_mousecursor, 0, MOUSE_MAX_STATE);
#ifdef SCHISM_WIN32
	cfg_video_want_menu_bar = !!cfg_get_number(&cfg, "Video", "want_menu_bar", 1);
#endif

	tmp = get_home_directory();
	cfg_get_string(&cfg, "Directories", "modules", cfg_dir_modules, PATH_MAX, tmp);
	cfg_get_string(&cfg, "Directories", "samples", cfg_dir_samples, PATH_MAX, tmp);
	cfg_get_string(&cfg, "Directories", "instruments", cfg_dir_instruments, PATH_MAX, tmp);
	free(tmp);

	ptr = cfg_get_string(&cfg, "Directories", "module_pattern", NULL, 0, NULL);
	if (ptr) {
		strncpy(cfg_module_pattern, ptr, PATH_MAX);
		cfg_module_pattern[PATH_MAX] = 0;
	}

	ptr = cfg_get_string(&cfg, "Directories", "export_pattern", NULL, 0, NULL);
	if (ptr) {
		strncpy(cfg_export_pattern, ptr, PATH_MAX);
		cfg_export_pattern[PATH_MAX] = 0;
	}

	ptr = cfg_get_string(&cfg, "General", "numlock_setting", NULL, 0, NULL);
	if (!ptr)
		status.fix_numlock_setting = NUMLOCK_GUESS;
	else if (strcasecmp(ptr, "on") == 0)
		status.fix_numlock_setting = NUMLOCK_ALWAYS_ON;
	else if (strcasecmp(ptr, "off") == 0)
		status.fix_numlock_setting = NUMLOCK_ALWAYS_OFF;
	else
		status.fix_numlock_setting = NUMLOCK_HONOR;

	set_key_repeat(cfg_get_number(&cfg, "General", "key_repeat_delay", 0),
		       cfg_get_number(&cfg, "General", "key_repeat_rate", 0));

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	cfg_load_info(&cfg);
	cfg_load_patedit(&cfg);
	cfg_load_audio(&cfg);
	cfg_load_midi(&cfg);
	cfg_load_disko(&cfg);
	cfg_load_dmoz(&cfg);

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	if (cfg_get_number(&cfg, "General", "classic_mode", 0))
		status.flags |= CLASSIC_MODE;
	else
		status.flags &= ~CLASSIC_MODE;
	if (cfg_get_number(&cfg, "General", "make_backups", 1))
		status.flags |= MAKE_BACKUPS;
	else
		status.flags &= ~MAKE_BACKUPS;
	if (cfg_get_number(&cfg, "General", "numbered_backups", 0))
		status.flags |= NUMBERED_BACKUPS;
	else
		status.flags &= ~NUMBERED_BACKUPS;

	i = cfg_get_number(&cfg, "General", "time_display", TIME_PLAY_ELAPSED);
	/* default to play/elapsed for invalid values */
	if (i < 0 || i >= TIME_PLAYBACK)
		i = TIME_PLAY_ELAPSED;
	status.time_display = i;

	i = cfg_get_number(&cfg, "General", "vis_style", VIS_OSCILLOSCOPE);
	/* default to oscilloscope for invalid values */
	if (i < 0 || i >= VIS_SENTINEL)
		i = VIS_OSCILLOSCOPE;
	status.vis_style = i;

	kbd_sharp_flat_toggle(cfg_get_number(&cfg, "General", "accidentals_as_flats", 0) == 1);

#ifdef SCHISM_MACOSX
# define DEFAULT_META 1
#else
# define DEFAULT_META 0
#endif
	if (cfg_get_number(&cfg, "General", "meta_is_ctrl", DEFAULT_META))
		status.flags |= META_IS_CTRL;
	else
		status.flags &= ~META_IS_CTRL;
	if (cfg_get_number(&cfg, "General", "altgr_is_alt", 1))
		status.flags |= ALTGR_IS_ALT;
	else
		status.flags &= ~ALTGR_IS_ALT;
	if (cfg_get_number(&cfg, "Video", "lazy_redraw", 0))
		status.flags |= LAZY_REDRAW;
	else
		status.flags &= ~LAZY_REDRAW;

	if (cfg_get_number(&cfg, "General", "midi_like_tracker", 0))
		status.flags |= MIDI_LIKE_TRACKER;
	else
		status.flags &= ~MIDI_LIKE_TRACKER;

	cfg_get_string(&cfg, "General", "font", cfg_font, NAME_MAX, "font.cfg");

	cfg_load_palette(&cfg);

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	cfg_free(&cfg);
}

void cfg_midipage_save(void)
{
	char *ptr;
	cfg_file_t cfg;

	ptr = dmoz_path_concat(cfg_dir_dotschism, "config");
	cfg_init(&cfg, ptr);
	free(ptr);

	cfg_save_midi(&cfg);

	cfg_write(&cfg);
	cfg_free(&cfg);
}

void cfg_save(void)
{
	char *ptr;
	cfg_file_t cfg;

	ptr = dmoz_path_concat(cfg_dir_dotschism, "config");
	cfg_init(&cfg, ptr);
	free(ptr);

	// this wart is here because Storlek is retarded
	cfg_delete_key(&cfg, "Directories", "filename_pattern");

	cfg_set_string(&cfg, "Directories", "modules", cfg_dir_modules);
	cfg_set_string(&cfg, "Directories", "samples", cfg_dir_samples);
	cfg_set_string(&cfg, "Directories", "instruments", cfg_dir_instruments);
	/* No, it's not a directory, but whatever. */
	cfg_set_string(&cfg, "Directories", "module_pattern", cfg_module_pattern);
	cfg_set_string(&cfg, "Directories", "export_pattern", cfg_export_pattern);

	cfg_save_info(&cfg);
	cfg_save_patedit(&cfg);
	cfg_save_audio(&cfg);
	cfg_save_palette(&cfg);
	cfg_save_disko(&cfg);
	cfg_save_dmoz(&cfg);

	cfg_write(&cfg);
	cfg_free(&cfg);
}

void cfg_save_output(void)
{
	char *ptr;
	cfg_file_t cfg;

	ptr = dmoz_path_concat(cfg_dir_dotschism, "config");
	cfg_init(&cfg, ptr);
	free(ptr);

	cfg_save_audio_playback(&cfg);

	cfg_write(&cfg);
	cfg_free(&cfg);
}

void cfg_atexit_save(void)
{
	char *ptr;
	cfg_file_t cfg;

	ptr = dmoz_path_concat(cfg_dir_dotschism, "config");
	cfg_init(&cfg, ptr);
	free(ptr);

	cfg_atexit_save_audio(&cfg);

	/* TODO: move these config options to video.c, this is lame :)
	Or put everything here, which is what the note in audio_loadsave.cc
	says. Very well, I contradict myself. */
	cfg_set_string(&cfg, "Video", "interpolation", SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY));
	cfg_set_number(&cfg, "Video", "fullscreen", !!(video_is_fullscreen()));
	cfg_set_number(&cfg, "Video", "mouse_cursor", video_mousecursor_visible());
	cfg_set_number(&cfg, "Video", "lazy_redraw", !!(status.flags & LAZY_REDRAW));
#ifdef SCHISM_WIN32
	cfg_set_number(&cfg, "Video", "want_menu_bar", !!cfg_video_want_menu_bar);
#endif

	cfg_set_number(&cfg, "General", "vis_style", status.vis_style);
	cfg_set_number(&cfg, "General", "time_display", status.time_display);
	cfg_set_number(&cfg, "General", "classic_mode", !!(status.flags & CLASSIC_MODE));
	cfg_set_number(&cfg, "General", "make_backups", !!(status.flags & MAKE_BACKUPS));
	cfg_set_number(&cfg, "General", "numbered_backups", !!(status.flags & NUMBERED_BACKUPS));

	cfg_set_number(&cfg, "General", "accidentals_as_flats", !!(status.flags & ACCIDENTALS_AS_FLATS));
	cfg_set_number(&cfg, "General", "meta_is_ctrl", !!(status.flags & META_IS_CTRL));
	cfg_set_number(&cfg, "General", "altgr_is_alt", !!(status.flags & ALTGR_IS_ALT));

	cfg_set_number(&cfg, "General", "midi_like_tracker", !!(status.flags & MIDI_LIKE_TRACKER));
	/* Say, whose bright idea was it to make this a string setting?
	The config file is human editable but that's mostly for developer convenience and debugging
	purposes. These sorts of things really really need to be options in the GUI so that people
	don't HAVE to touch the settings. Then we can just use an enum (and we *could* in theory
	include comments to the config by default listing what the numbers are, but that shouldn't
	be necessary in most cases. */
	switch (status.fix_numlock_setting) {
	case NUMLOCK_ALWAYS_ON:
		cfg_set_string(&cfg, "General", "numlock_setting", "on");
		break;
	case NUMLOCK_ALWAYS_OFF:
		cfg_set_string(&cfg, "General", "numlock_setting", "off");
		break;
	case NUMLOCK_HONOR:
		cfg_set_string(&cfg, "General", "numlock_setting", "system");
		break;
	case NUMLOCK_GUESS:
		/* leave empty */
		break;
	};

	/* hm... most of the time probably nothing's different, so saving the
	config file here just serves to make the backup useless. maybe add a
	'dirty' flag to the config parser that checks if any settings are
	actually *different* from those in the file? */

	cfg_write(&cfg);
	cfg_free(&cfg);
}

