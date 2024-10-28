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

#include "it.h"
#include "keybinds.h"
#include "page.h"
#include "config.h"
#include "config-parser.h"
#include "dmoz.h"

/* lost puppies */
static int file_list_load_keybinds(cfg_file_t *cfg);
static int global_load_keybinds(cfg_file_t *cfg);

/* --------------------------------------------------------------------- */
/* Parsing keybind strings */

static int parse_shortcut_scancode(keybind_bind_t* bind, const char* shortcut, SDL_Scancode* code)
{
	if (keybinds_parse_scancode(shortcut, code))
		return 1;

	if (!strncmp(shortcut, "US_", 3)) {
		/* unknown scancode ? */
		log_appendf(5, " %s/%s: Unknown scancode '%s'", bind->section_info->name, bind->name, shortcut);
		printf("%s/%s: Unknown scancode '%s'\n", bind->section_info->name, bind->name, shortcut);
		fflush(stdout);
		return -1;
	}

	return 0;
}

static int parse_shortcut_keycode(keybind_bind_t* bind, const char* shortcut, SDL_Keycode* kcode)
{
	if (keybinds_parse_keycode(shortcut, kcode))
		return 1;

	*kcode = SDL_GetKeyFromName(shortcut);
	if (*kcode != SDLK_UNKNOWN)
		return 1;

	log_appendf(5, " %s/%s: Unknown code '%s'", bind->section_info->name, bind->name, shortcut);
	printf("%s/%s: Unknown code '%s'\n", bind->section_info->name, bind->name, shortcut);
	fflush(stdout);
	return -1;
}

static int keybinds_parse_shortcut(keybind_bind_t* bind, const char* shortcut)
{
	int has_problem = 0;

	enum keybind_modifier mods = KEYBIND_MOD_NONE;
	SDL_Scancode scan_code = SDL_SCANCODE_UNKNOWN;
	SDL_Keycode key_code = SDLK_UNKNOWN;

	for (const char *last_pch = shortcut, *pch = shortcut; pch; last_pch = pch + 1) {
		pch = strchr(last_pch, '+');

		char *trimmed = (pch) ? strn_dup(last_pch, pch - last_pch) : str_dup(last_pch);
		str_trim(trimmed);

		enum keybind_modifier mod;
		if (keybinds_parse_modkey(trimmed, &mod)) {
			mods |= mod;
			free(trimmed);
			continue;
		}

		int result;

		result = parse_shortcut_scancode(bind, trimmed, &scan_code);
		if (result == 1) { free(trimmed); break; }
		if (result == -1) { free(trimmed); has_problem = 1; break; }

		result = parse_shortcut_keycode(bind, trimmed, &key_code);
		if (result == 1) { free(trimmed); break; }
		if (result == -1) { free(trimmed); has_problem = 1; break; }

		free(trimmed);
	}

	if (has_problem)
		return 0;

	keybinds_append_shortcut_to_bind(bind, key_code, scan_code, mods);
	return 1;
}

static int keybinds_parse_shortcuts(keybind_bind_t* bind, const char* shortcut)
{
	for (const char *last_pch = shortcut, *pch; ; last_pch = pch + 1) {
		pch = strchr(last_pch, ',');

		char *shortcut_dup = (pch) ? strn_dup(last_pch, pch - last_pch) : str_dup(last_pch);

		if (!keybinds_parse_shortcut(bind, shortcut_dup)) {
			free(shortcut_dup);
			return 0;
		}

		free(shortcut_dup);

		if (!pch)
			break;
	}

	return 1;
}

static void set_shortcut_text(keybind_bind_t* bind)
{
	char *out[KEYBINDS_MAX_SHORTCUTS] = {0};

	for(int i = 0; i < KEYBINDS_MAX_SHORTCUTS && i < bind->shortcuts_count; i++) {
		keybind_shortcut_t* sc = &bind->shortcuts[i];

		static const struct {
			enum keybind_modifier mod;
			const char *text;
		} translation[] = {
			{KEYBIND_MOD_CTRL, "Ctrl"},
			{KEYBIND_MOD_LCTRL, "LCtrl"},
			{KEYBIND_MOD_RCTRL, "RCtrl"},
			{KEYBIND_MOD_SHIFT, "Shift"},
			{KEYBIND_MOD_LSHIFT, "LShift"},
			{KEYBIND_MOD_RSHIFT, "RShift"},
			{KEYBIND_MOD_ALT, "Alt"},
			{KEYBIND_MOD_LALT, "LAlt"},
			{KEYBIND_MOD_RALT, "RAlt"},
		};

		for (int j = 0; j < ARRAY_SIZE(translation); j++) {
			// mmm
			if (!(sc->modifier & translation[j].mod))
				continue;

			if (out[i]) {
				char *old = out[i];
				out[i] = STR_IMPLODE(2, "-", old, translation[j].text);
				free(old);
			} else {
				out[i] = str_dup(translation[j].text);
			}
		}

		SDL_Keycode kc = (sc->keycode != SDLK_UNKNOWN)
			? sc->keycode
			: (sc->scancode != SDL_SCANCODE_UNKNOWN)
				? SDL_GetKeyFromScancode(sc->scancode)
				: SDLK_UNKNOWN;

		if (kc == SDLK_UNKNOWN)
			continue;

		char *key_text;

		switch (kc) {
		case SDLK_RETURN:
#ifdef SCHISM_MACOSX
			key_text = str_dup("Return");
#else
			key_text = str_dup("Enter");
#endif
			break;
		case SDLK_EQUALS:
			key_text = str_dup("Equals");
			break;
		case SDLK_MINUS:
			key_text = str_dup("Minus");
			break;
		case SDLK_SPACE:
			key_text = str_dup("Spacebar");
			break;
		default:
			key_text = str_dup(SDL_GetKeyName(kc));
			break;
		}

		int key_text_length = strlen(key_text);
		if (!key_text_length)
			continue;

		// mmm
		if (out[i]) {
			char *old = out[i];
			out[i] = STR_IMPLODE(2, "-", old, key_text);
			free(old);
		} else {
			out[i] = str_dup(key_text);
		}

		if(i == 0) {
			bind->first_shortcut_text = str_dup(out[i]);
			bind->first_shortcut_text_parens = STR_CONCAT(3, " (", out[i], ")");
		}

		free(key_text);
	}

	bind->shortcut_text = str_implode(KEYBINDS_MAX_SHORTCUTS, ", ", (const char**)out);
	if (*bind->shortcut_text)
		bind->shortcut_text_parens = STR_CONCAT(3, " (", bind->shortcut_text, ")");

	for (int i = 0; i < KEYBINDS_MAX_SHORTCUTS; i++) {
		if (!out[i])
			continue;

		char *text = out[i];

		if (text && *text) {
			char* padded = str_pad_end(text, ' ', 18);
			out[i] = STR_CONCAT(2, "    ", padded);
			free(padded);
		}

		free(text);
	}

	char* help_shortcuts = str_implode_free(KEYBINDS_MAX_SHORTCUTS, "\n\"", out);
	bind->help_text = STR_CONCAT(4, "\"", help_shortcuts, bind->description, "\n");
	free(help_shortcuts);
}

void keybinds_init_shortcut(keybind_shortcut_t *sc)
{
	sc->scancode = SDL_SCANCODE_UNKNOWN;
	sc->keycode = SDLK_UNKNOWN;
	sc->modifier = KEYBIND_MOD_NONE;
	sc->pressed = 0;
	sc->released = 0;
	sc->repeated = 0;
	sc->is_press_repeat = 0;
	sc->press_repeats = 0;
}

int keybinds_init_bind(keybind_bind_t* bind, keybind_section_info_t* section_info, const char* name, const char* description, const char* shortcut)
{
	bind->pressed = 0;
	bind->released = 0;

	for (int i = 0; i < KEYBINDS_MAX_SHORTCUTS; i++)
		keybinds_init_shortcut(bind->shortcuts + i);

	bind->shortcuts_count = 0;
	bind->section_info = section_info;
	bind->name = name;
	bind->description = description;
	bind->shortcut_text = "";
	bind->first_shortcut_text = "";
	bind->shortcut_text_parens = "";
	bind->first_shortcut_text_parens = "";
	bind->help_text = "";

	bind->pressed = 0;
	bind->released = 0;
	bind->repeated = 0;
	bind->press_repeats = 0;

	if (!keybinds_parse_shortcuts(bind, shortcut))
		return 0;

	set_shortcut_text(bind);

	return 1;
}

void keybinds_init_section(keybind_section_info_t* section_info, const char* name, const char* title, enum page_numbers page)
{
	section_info->is_active = 0;
	section_info->name = name;
	section_info->title = title;
	section_info->page = page;
	section_info->page_matcher = NULL;
}

int keybinds_init(void)
{
	log_append(2, 0, "Custom Key Bindings");
	log_underline(19);

	char* path = dmoz_path_concat(cfg_dir_dotschism, "keybinds.ini");
	if (!path)
		return 0;

	cfg_file_t cfg;
	cfg_init(&cfg, path);

#define KEYBINDS_INIT_ASSERT(x) \
	do { if (!(x)) { cfg_write(&cfg); cfg_free(&cfg); return 0; } } while (0)

	KEYBINDS_INIT_ASSERT(info_load_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(instrument_list_load_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(load_module_load_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(load_sample_load_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(message_load_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(midi_load_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(ordervol_load_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(palette_load_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(pattern_editor_load_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(sample_list_load_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(timeinfo_load_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(waterfall_load_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(file_list_load_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(global_load_keybinds(&cfg));

#undef KEYBINDS_INIT_ASSERT

	cfg_write(&cfg);
	cfg_free(&cfg);

	free(path);

	log_appendf(5, " No issues");

	log_nl();
	return 1;
}

/* -------------------------------------------------------------------------- */

static int file_list_page_matcher(enum page_numbers page)
{
	return page == PAGE_LOAD_INSTRUMENT || page == PAGE_LOAD_MODULE || page == PAGE_LOAD_SAMPLE;
}

static int file_list_load_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(file_list, "File List Keys.", PAGE_GLOBAL);
	global_keybinds_list.file_list_info.page_matcher = file_list_page_matcher;

	INIT_BIND(file_list, delete, "Delete file", "DELETE");

	return 1;
}

static int global_load_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(global, "Global Keys.", PAGE_GLOBAL);
	INIT_BIND(global, help, "Help (context sensitive!)", "F1");
	INIT_BIND(global, midi, "MIDI screen", "Shift+F1");
	INIT_BIND(global, system_configure, "System configuration", "Ctrl+F1");
	INIT_BIND(global, pattern_edit, "Pattern editor / pattern editor options", "F2");
	INIT_BIND(global, sample_list, "Sample list", "F3");
	INIT_BIND(global, sample_library, "Sample library", "Ctrl+F3");
	INIT_BIND(global, instrument_list, "Instrument list", "F4");
	INIT_BIND(global, instrument_library, "Instrument library", "Ctrl+F4");
	INIT_BIND(global, play_information_or_play_song, "Play information / play song", "F5");
	INIT_BIND(global, play_song, "Play song", "Ctrl+F5");
	INIT_BIND(global, preferences, "Preferences", "Shift+F5");
	INIT_BIND(global, play_current_pattern, "Play current pattern", "F6");
	INIT_BIND(global, play_song_from_order, "Play song from current order", "Shift+F6");
	INIT_BIND(global, play_song_from_mark, "Play from mark / current row", "F7");
	INIT_BIND(global, toggle_playback, "Pause / resume playback", "Shift+F8");
	INIT_BIND(global, stop_playback, "Stop playback", "F8");

	/* os x steals plain scroll lock for brightness,
	* so catch ctrl+scroll lock here as well */
	INIT_BIND(global, toggle_playback_tracing, "Toggle playback tracing", "SCROLLLOCK,Ctrl+SCROLLLOCK,Ctrl+F");
	INIT_BIND(global, toggle_midi_input, "Toggle MIDI input\n ", "Alt+SCROLLLOCK");

	INIT_BIND(global, load_module, "Load module", "F9,Ctrl+L");
	INIT_BIND(global, message_editor, "Message editor", "Shift+F9");
	INIT_BIND(global, save_module, "Save module", "F10,Ctrl+W");
	INIT_BIND(global, export_module, "Export module (to WAV, AIFF)", "Shift+F10");
	INIT_BIND(global, order_list,
		"Order list and panning" TEXT_2X
		"Order list and channel volume", "F11");
	INIT_BIND(global, schism_logging, "Schism logging", "Ctrl+F11");
	INIT_BIND(global, order_list_lock, "Lock/unlock order list", "Alt+F11");
	INIT_BIND(global, song_variables, "Song variables & directory configuration", "F12");
	INIT_BIND(global, palette_config, "Palette configuration", "Ctrl+F12");
	INIT_BIND(global, font_editor, "Font editor", "Shift+F12");
	INIT_BIND(global, waterfall, "Waterfall", "Alt+F12");
	INIT_BIND(global, time_information, "Time Information\n ", "LShift+LAlt+RAlt+RCtrl+Pause,Ctrl+Shift+T");

	INIT_BIND(global, octave_decrease, "Decrease octave", "KP_DIVIDE,Alt+HOME");
	INIT_BIND(global, octave_increase, "Increase octave", "KP_MULTIPLY,Alt+END");
	INIT_BIND(global, decrease_playback_speed, "Decrease playback speed", "Shift+LEFTBRACKET");
	INIT_BIND(global, increase_playback_speed, "Increase playback speed", "Shift+RIGHTBRACKET");
	INIT_BIND(global, decrease_playback_tempo, "Decrease playback tempo", "Ctrl+LEFTBRACKET");
	INIT_BIND(global, increase_playback_tempo, "Increase playback tempo", "Ctrl+RIGHTBRACKET");
	INIT_BIND(global, decrease_global_volume, "Decrease global volume", "LEFTBRACKET");
	INIT_BIND(global, increase_global_volume, "Increase global volume\n ", "RIGHTBRACKET");

	INIT_BIND(global, toggle_channel_1, "Toggle channel 1", "Alt+F1");
	INIT_BIND(global, toggle_channel_2, "Toggle channel 2", "Alt+F2");
	INIT_BIND(global, toggle_channel_3, "Toggle channel 3", "Alt+F3");
	INIT_BIND(global, toggle_channel_4, "Toggle channel 4", "Alt+F4");
	INIT_BIND(global, toggle_channel_5, "Toggle channel 5", "Alt+F5");
	INIT_BIND(global, toggle_channel_6, "Toggle channel 6", "Alt+F6");
	INIT_BIND(global, toggle_channel_7, "Toggle channel 7", "Alt+F7");
	INIT_BIND(global, toggle_channel_8, "Toggle channel 8\n ", "Alt+F8");

	INIT_BIND(global, mouse_grab, "Toggle mouse / keyboard grab", "Ctrl+D");
	INIT_BIND(global, display_reset, "Refresh screen and reset cache identification", "Ctrl+E");
	INIT_BIND(global, go_to_time, "Go to order / pattern / row given time", "Ctrl+G");
	INIT_BIND(global, audio_reset, "Reinitialize sound driver", "Ctrl+I");
	INIT_BIND(global, mouse, "Toggle mouse cursor", "Ctrl+M");
	INIT_BIND(global, new_song, "New song", "Ctrl+N");
	INIT_BIND(global, calculate_song_length, "Calculate approximate song length", "Ctrl+P");
	INIT_BIND(global, quit, "Quit schism tracker", "Ctrl+Q");
	INIT_BIND(global, quit_no_confirm, "Quit without confirmation", "Ctrl+Shift+Q");
	INIT_BIND(global, save, "Save current song\n ", "Ctrl+S");

	INIT_BIND(global, previous_order, "Previous order (while playing, not on pattern edit)", "Ctrl+LEFT");
	INIT_BIND(global, next_order, "Next order (while playing, not on pattern edit)\n ", "Ctrl+RIGHT");

	INIT_BIND(global, fullscreen, "Toggle fullscreen\n ", "Ctrl+Alt+ENTER");

	INIT_BIND(global, open_menu, "Open menu\n ", "ESCAPE");

	INIT_BIND(global, nav_left, "Navigate left", "LEFT");
	INIT_BIND(global, nav_right, "Navigate right", "RIGHT");
	INIT_BIND(global, nav_up, "Navigate up", "UP");
	INIT_BIND(global, nav_down, "Navigate down\n ", "DOWN");

	INIT_BIND(global, nav_page_up, "Navigate page up", "PAGEUP");
	INIT_BIND(global, nav_page_down, "Navigate page down\n ", "PAGEDOWN");

	INIT_BIND(global, nav_accept, "Navigate accept", "ENTER");
	INIT_BIND(global, nav_cancel, "Navigate cancel\n ", "ESCAPE");

	INIT_BIND(global, nav_home, "Navigate home (start of line/first in list)", "HOME");
	INIT_BIND(global, nav_end, "Navigate end (end of line/last in list)\n ", "END");

	INIT_BIND(global, nav_tab, "Navigate to next item right", "TAB");
	INIT_BIND(global, nav_backtab, "Navigate to next item left\n ", "Shift+TAB");

	INIT_BIND(global, thumbbar_max_value, "Thumbbar max value", "END");
	INIT_BIND(global, thumbbar_increase_value, "Thumbbar increase value", "RIGHT");
	INIT_BIND(global, thumbbar_increase_value_2x, "Thumbbar increase value 2x", "Ctrl+RIGHT");
	INIT_BIND(global, thumbbar_increase_value_4x, "Thumbbar increase value 4x", "Shift+RIGHT");
	INIT_BIND(global, thumbbar_increase_value_8x, "Thumbbar increase value 8x\n ", "Alt+RIGHT");

	INIT_BIND(global, thumbbar_min_value, "Thumbbar min value", "HOME");
	INIT_BIND(global, thumbbar_decrease_value, "Thumbbar decrease value", "LEFT");
	INIT_BIND(global, thumbbar_decrease_value_2x, "Thumbbar decrease value 2x", "Ctrl+LEFT");
	INIT_BIND(global, thumbbar_decrease_value_4x, "Thumbbar decrease value 4x", "Shift+LEFT");
	INIT_BIND(global, thumbbar_decrease_value_8x, "Thumbbar decrease value 8x\n ", "Alt+LEFT");

	INIT_BIND(global, numentry_increase_value, "Number increase value", "KP_PLUS,Shift+EQUALS");
	INIT_BIND(global, numentry_decrease_value, "Number decrease value 8x\n ", "KP_MINUS,Shift+MINUS");

	INIT_SECTION(dialog, "Dialog Keys.", PAGE_GLOBAL);
	INIT_BIND(dialog, yes, "Answer yes in yes/no dialog.", "Y");
	INIT_BIND(dialog, no, "Answer no in yes/no dialog.\n ", "N");

	INIT_BIND(dialog, answer_ok, "Answer ok in ok/cancel dialog.", "O");
	INIT_BIND(dialog, answer_cancel, "Answer cancel in ok/cancel dialog.\n ", "C");

	INIT_BIND(dialog, cancel, "Dialog cancel.", "ESCAPE");
	INIT_BIND(dialog, accept, "Dialog accept.", "ENTER");

	INIT_SECTION(notes, "Note Keys.", PAGE_GLOBAL);
	INIT_BIND(notes, note_row1_c, "Row 1 C", "US_Z");
	INIT_BIND(notes, note_row1_c_sharp, "Row 1 C#", "US_S");
	INIT_BIND(notes, note_row1_d, "Row 1 D", "US_X");
	INIT_BIND(notes, note_row1_d_sharp, "Row 1 D#", "US_D");
	INIT_BIND(notes, note_row1_e, "Row 1 E\n ", "US_C");

	INIT_BIND(notes, note_row1_f, "Row 1 F", "US_V");
	INIT_BIND(notes, note_row1_f_sharp, "Row 1 F#", "US_G");
	INIT_BIND(notes, note_row1_g, "Row 1 G", "US_B");
	INIT_BIND(notes, note_row1_g_sharp, "Row 1 G#\n ", "US_H");

	INIT_BIND(notes, note_row1_a, "Row 1 A", "US_N");
	INIT_BIND(notes, note_row1_a_sharp, "Row 1 A#", "US_J");
	INIT_BIND(notes, note_row1_b, "Row 1 B\n ", "US_M");

	INIT_BIND(notes, note_row2_c, "Row 2 C", "US_Q");
	INIT_BIND(notes, note_row2_c_sharp, "Row 2 C#", "US_2");
	INIT_BIND(notes, note_row2_d, "Row 2 D", "US_W");
	INIT_BIND(notes, note_row2_d_sharp, "Row 2 D#", "US_3");
	INIT_BIND(notes, note_row2_e, "Row 2 E\n ", "US_E");

	INIT_BIND(notes, note_row2_f, "Row 2 F", "US_R");
	INIT_BIND(notes, note_row2_f_sharp, "Row 2 F#", "US_5");
	INIT_BIND(notes, note_row2_g, "Row 2 G", "US_T");
	INIT_BIND(notes, note_row2_g_sharp, "Row 2 G#\n ", "US_6");

	INIT_BIND(notes, note_row2_a, "Row 2 A", "US_Y");
	INIT_BIND(notes, note_row2_a_sharp, "Row 2 A#", "US_7");
	INIT_BIND(notes, note_row2_b, "Row 2 B\n ", "US_U");

	INIT_BIND(notes, note_row3_c, "Row 2 C", "US_I");
	INIT_BIND(notes, note_row3_c_sharp, "Row 2 C#", "US_9");
	INIT_BIND(notes, note_row3_d, "Row 2 D", "US_O");
	INIT_BIND(notes, note_row3_d_sharp, "Row 2 D#", "US_0");
	INIT_BIND(notes, note_row3_e, "Row 2 E\n ", "US_P");

	return 1;
}
