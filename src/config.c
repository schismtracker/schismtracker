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

#include <sys/types.h>
#include <sys/stat.h>

#include "config-parser.h"

/* --------------------------------------------------------------------- */
/* config settings */

char cfg_dir_modules[PATH_MAX + 1], cfg_dir_samples[PATH_MAX + 1], cfg_dir_instruments[PATH_MAX + 1];
char cfg_font[NAME_MAX + 1];

/* --------------------------------------------------------------------- */

static const char palette_trans[64] = ".0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";
static void cfg_load_palette(cfg_file_t *cfg)
{
	byte colors[48];
	int n;
	char palette_text[49] = "";
	const char *ptr;
	
	palette_load_preset(cfg_get_number(cfg, "General", "palette", 2));
	
	cfg_get_string(cfg, "General", "palette_cur", palette_text, 50, "");
	for (n = 0; n < 48; n++) {
		if (palette_text[n] == '\0' || (ptr = strchr(palette_trans, palette_text[n])) == NULL)
			return;
		colors[n] = ptr - palette_trans;
	}
	memcpy(current_palette, colors, sizeof(current_palette));
}

static void cfg_save_palette(cfg_file_t *cfg)
{
	int n;
	char palette_text[49] = "";
	
	cfg_set_number(cfg, "General", "palette", current_palette_index);

	for (n = 0; n < 48; n++) {
		/* tricky little hack: this is *massively* overstepping the array boundary */
		palette_text[n] = palette_trans[current_palette[0][n]];
	}
	palette_text[48] = '\0';
	cfg_set_string(cfg, "General", "palette_cur", palette_text);
}

/* --------------------------------------------------------------------------------------------------------- */

void cfg_load(void)
{
	char filename[PATH_MAX + 1];
	const char *home_dir = getenv("HOME") ? : "/";
	int i;
	cfg_file_t cfg;
	
	strncpy(filename, home_dir, PATH_MAX);
	strncat(filename, "/.schism", PATH_MAX);
	if (!is_directory(filename)) {
		printf("Creating directory ~/.schism where Schism Tracker will store your settings.\n");
		i = mkdir(filename, 0777);
		if (i != 0) {
			perror("Error creating directory");
			fprintf(stderr, "Everything will still work, but preferences will not be saved.\n");
		}
	}
	
	strncat(filename, "/config", PATH_MAX);
	filename[PATH_MAX] = 0;
	cfg_init(&cfg, filename);

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	cfg_get_string(&cfg, "Directories", "modules", cfg_dir_modules, PATH_MAX, home_dir);
	cfg_get_string(&cfg, "Directories", "samples", cfg_dir_samples, PATH_MAX, home_dir);
	cfg_get_string(&cfg, "Directories", "instruments", cfg_dir_instruments, PATH_MAX, home_dir);
	
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	
	cfg_load_info(&cfg);
	cfg_load_patedit(&cfg);
	cfg_load_audio(&cfg);
	
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	
	i = cfg_get_number(&cfg, "General", "time_display", TIME_PLAY_ELAPSED);
	/* default to play/elapsed for invalid values */
	if (i < 0 || i >= TIME_PLAYBACK)
		i = TIME_PLAY_ELAPSED;
	status.time_display = i;
	
	if (cfg_get_number(&cfg, "General", "classic_mode", 0))
		status.flags |= CLASSIC_MODE;
	else
		status.flags &= ~CLASSIC_MODE;
	if (cfg_get_number(&cfg, "General", "make_backups", 0))
		status.flags |= MAKE_BACKUPS;
	else
		status.flags &= ~MAKE_BACKUPS;
	
	cfg_get_string(&cfg, "General", "font", cfg_font, NAME_MAX, "font.cfg");
	
	cfg_load_palette(&cfg);
	
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	
	cfg_free(&cfg);
}

void cfg_save(void)
{
	char filename[PATH_MAX + 1];
	cfg_file_t cfg;

	strncpy(filename, getenv("HOME") ? : "/", PATH_MAX);
	strncat(filename, "/.schism/config", PATH_MAX);
	filename[PATH_MAX] = 0;
	
	cfg_init(&cfg, filename);

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	cfg_set_string(&cfg, "Directories", "modules", cfg_dir_modules);
	cfg_set_string(&cfg, "Directories", "samples", cfg_dir_samples);
	cfg_set_string(&cfg, "Directories", "instruments", cfg_dir_instruments);

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	cfg_save_info(&cfg);
	cfg_save_patedit(&cfg);
	cfg_save_audio(&cfg);
	
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	cfg_set_number(&cfg, "General", "time_display", status.time_display);
	cfg_set_number(&cfg, "General", "classic_mode", !!(status.flags & CLASSIC_MODE));
	cfg_set_number(&cfg, "General", "make_backups", !!(status.flags & MAKE_BACKUPS));
	cfg_save_palette(&cfg);
	
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	cfg_write(&cfg);
	cfg_free(&cfg);
}

void cfg_atexit_save(void)
{
	char filename[PATH_MAX + 1];
	cfg_file_t cfg;

	strncpy(filename, getenv("HOME") ? : "/", PATH_MAX);
	strncat(filename, "/.schism/config", PATH_MAX);
	filename[PATH_MAX] = 0;
	
	cfg_init(&cfg, filename);
	
	cfg_atexit_save_audio(&cfg);
	
	/* hm... most of the time probably nothing's different, so saving the
	config file here just serves to make the backup useless. maybe add a
	'dirty' flag to the config parser that checks if any settings are
	actually *different* from those in the file? */
	cfg_write(&cfg);
	cfg_free(&cfg);
}
