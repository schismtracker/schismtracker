/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
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

/* I don't really like libcoman; maybe some day I'll write a better one.
 * In particular, there's no provision for default settings, so all the number settings are a big hack. */
#include <libcoman.h>

/* --------------------------------------------------------------------- */
/* config settings */

char cfg_dir_modules[PATH_MAX + 1], cfg_dir_samples[PATH_MAX + 1], cfg_dir_instruments[PATH_MAX + 1];
char cfg_font[NAME_MAX + 1];

/* --------------------------------------------------------------------------------------------------------- */

static int config_handle;

/* --------------------------------------------------------------------------------------------------------- */

void cfg_get_string(const char *section, const char *key, char value[], int max_length, const char *def)
{
	const char *s;
	
	SetGroup(section, config_handle);
	s = GetString(key, config_handle);
	if (!s)
		s = def;
	strncpy(value, s, max_length);
	value[max_length] = 0;
}

int cfg_get_number(const char *section, const char *key, int def)
{
	long n;
	const char *s;
	char *e;
	
	SetGroup(section, config_handle);
	s = GetString(key, config_handle);
	if (!s)
		return def;
	
	n = strtol(s, &e, 10);
	if (e == s) {
		/* Not a number */
		return def;
	} else if (*e) {
		/* Junk at the end of the string. I'm accepting the number here, but it would also be
		acceptable to treat it as junk and return the default value. */
		/* return def; */
	}
	return n;
}

void cfg_set_string(const char *section, const char *key, const char *value)
{
	SetGroup(section, config_handle);
	WriteString(key, value, config_handle);
}

void cfg_set_number(const char *section, const char *key, int value)
{
	SetGroup(section, config_handle);
	WriteNumber(key, value, config_handle);
}

/* --------------------------------------------------------------------------------------------------------- */

static const char palette_trans[64] = ".0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";
static void cfg_load_palette(void)
{
	byte colors[48];
	int n;
	char palette_text[49] = "";
	const char *ptr;
	
	palette_load_preset(cfg_get_number("General", "palette", 2));
	
	cfg_get_string("General", "palette_cur", palette_text, 50, "");
	for (n = 0; n < 48; n++) {
		if (palette_text[n] == '\0' || (ptr = strchr(palette_trans, palette_text[n])) == NULL)
			return;
		colors[n] = ptr - palette_trans;
	}
	memcpy(current_palette, colors, sizeof(current_palette));
}
static void cfg_save_palette(void)
{
	int n;
	char palette_text[49] = "";
	
	cfg_set_number("General", "palette", current_palette_index);

	for (n = 0; n < 48; n++) {
		/* tricky little hack: this is *massively* overstepping the array boundary */
		palette_text[n] = palette_trans[current_palette[0][n]];
	}
	palette_text[48] = '\0';
	cfg_set_string("General", "palette_cur", palette_text);
}

void cfg_load(void)
{
	char filename[PATH_MAX + 1];
	const char *home_dir = getenv("HOME") ? : "/";
	int i;
	
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
	config_handle = InitConfig(filename);

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	cfg_get_string("Directories", "modules", cfg_dir_modules, PATH_MAX, home_dir);
	cfg_get_string("Directories", "samples", cfg_dir_samples, PATH_MAX, home_dir);
	cfg_get_string("Directories", "instruments", cfg_dir_instruments, PATH_MAX, home_dir);
	
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	
	cfg_load_info();
	cfg_load_patedit();
	cfg_load_audio();
	
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	
	i = cfg_get_number("General", "time_display", TIME_PLAY_ELAPSED);
	/* default to play/elapsed for invalid values */
	if (i < 0 || i >= TIME_PLAYBACK)
		i = TIME_PLAY_ELAPSED;
	status.time_display = i;
	
	if (cfg_get_number("General", "classic_mode", 0))
		status.flags |= CLASSIC_MODE;
	else
		status.flags &= ~CLASSIC_MODE;
	
	cfg_get_string("General", "font", cfg_font, NAME_MAX, "font.cfg");
	
	cfg_load_palette();
	
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */
	
	/* argh! why does it insist on rewriting the file even when
	 * nothing at all has changed? */
	CloseConfig("/dev/null", config_handle);
}

void cfg_save(void)
{
	char filename[PATH_MAX + 1];

	strncpy(filename, getenv("HOME") ? : "/", PATH_MAX);
	strncat(filename, "/.schism/config", PATH_MAX);
	filename[PATH_MAX] = 0;
	config_handle = InitConfig(filename);

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	cfg_set_string("Directories", "modules", cfg_dir_modules);
	cfg_set_string("Directories", "samples", cfg_dir_samples);
	cfg_set_string("Directories", "instruments", cfg_dir_instruments);

	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	cfg_save_info();
	cfg_save_patedit();
	cfg_save_audio();
	
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	cfg_set_number("General", "time_display", status.time_display);
	cfg_set_number("General", "classic_mode", !!(status.flags & CLASSIC_MODE));
	cfg_save_palette();
	
	/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

	CloseConfig(filename, config_handle);
}
