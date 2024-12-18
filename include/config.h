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

#ifndef SCHISM_CONFIG_H_
#define SCHISM_CONFIG_H_

#include "headers.h"

extern char cfg_video_interpolation[];
extern char cfg_video_format[];
/* TODO: consolidate these into cfg_video_flags */
extern int cfg_video_fullscreen;
extern int cfg_video_want_fixed;
extern int cfg_video_want_fixed_width;
extern int cfg_video_want_fixed_height;
extern int cfg_video_mousecursor;
extern int cfg_video_width, cfg_video_height;
extern int cfg_video_hardware;
extern int cfg_video_want_menu_bar;

extern int cfg_kbd_repeat_delay;
extern int cfg_kbd_repeat_rate;

extern char cfg_dir_modules[SCHISM_PATH_MAX + 1], cfg_dir_samples[SCHISM_PATH_MAX + 1], cfg_dir_instruments[SCHISM_PATH_MAX + 1];
extern char *cfg_dir_dotschism; /* the full path to ~/.schism */
extern char *cfg_font;
extern int cfg_palette;

extern char cfg_module_pattern[SCHISM_PATH_MAX + 1];
extern char cfg_export_pattern[SCHISM_PATH_MAX + 1];

void cfg_init_dir(void);
void cfg_load(void);
void cfg_save(void);
void cfg_midipage_save(void);
void cfg_atexit_save(void); /* this only saves a handful of settings, not everything */
void cfg_save_output(void);

/* each page with configurable settings has a function to load/save them... */
#include "config-parser.h"

void cfg_load_midi(cfg_file_t *cfg);
void cfg_save_midi(cfg_file_t *cfg);

void cfg_load_patedit(cfg_file_t *cfg);
void cfg_save_patedit(cfg_file_t *cfg);

void cfg_load_info(cfg_file_t *cfg);
void cfg_save_info(cfg_file_t *cfg);

void cfg_load_audio(cfg_file_t *cfg);
void cfg_save_audio(cfg_file_t *cfg);
void cfg_save_audio_playback(cfg_file_t* cfg);
void cfg_atexit_save_audio(cfg_file_t *cfg);

void cfg_load_disko(cfg_file_t *cfg);
void cfg_save_disko(cfg_file_t *cfg);

void cfg_load_dmoz(cfg_file_t *cfg);
void cfg_save_dmoz(cfg_file_t *cfg);

#endif /* SCHISM_CONFIG_H_ */
