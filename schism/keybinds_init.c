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

/* XXX need to shove this crap into their respective page files */
static int init_load_sample_keybinds(cfg_file_t *cfg);
static int init_load_stereo_sample_dialog_keybinds(cfg_file_t *cfg);
static int init_message_edit_keybinds(cfg_file_t *cfg);
static int init_waterfall_keybinds(cfg_file_t *cfg);
static int init_load_module_keybinds(cfg_file_t *cfg);
static int init_palette_edit_keybinds(cfg_file_t *cfg);
static int init_order_list_keybinds(cfg_file_t *cfg);
static int init_info_page_keybinds(cfg_file_t *cfg);
static int init_sample_list_keybinds(cfg_file_t *cfg);
static int init_instrument_list_keybinds(cfg_file_t *cfg);
static int init_pattern_edit_keybinds(cfg_file_t *cfg);
static int init_file_list_keybinds(cfg_file_t *cfg);
static int init_global_keybinds(cfg_file_t *cfg);

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
	if (!shortcut || !shortcut[0])
		return 0;

	if (keybinds_parse_keycode(shortcut, kcode))
		return 1;

	*kcode = SDL_GetKeyFromName(shortcut);

	if (*kcode == SDLK_UNKNOWN) {
		log_appendf(5, " %s/%s: Unknown code '%s'", bind->section_info->name, bind->name, shortcut);
		printf("%s/%s: Unknown code '%s'\n", bind->section_info->name, bind->name, shortcut);
		fflush(stdout);
		return -1;
	}

	return 1;
}

static int keybinds_parse_shortcut(keybind_bind_t* bind, const char* shortcut)
{
	int has_problem = 0;

	SDL_Keymod mods = KMOD_NONE;
	SDL_Scancode scan_code = SDL_SCANCODE_UNKNOWN;
	SDL_Keycode key_code = SDLK_UNKNOWN;

	for (const char *last_pch = shortcut, *pch = shortcut; pch; last_pch = pch + 1) {
		pch = strchr(last_pch, '+');

		char *trimmed = (pch) ? strndup(last_pch, pch - last_pch) : strdup(last_pch);
		trim_string(trimmed);

		SDL_Keymod mod;
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

		char *shortcut_dup = (pch) ? strndup(last_pch, pch - last_pch) : strdup(last_pch);

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
	char* out[KEYBINDS_MAX_SHORTCUTS] = {0};

	for(int i = 0; i < KEYBINDS_MAX_SHORTCUTS && i < bind->shortcuts_count; i++) {
		keybind_shortcut_t* sc = &bind->shortcuts[i];

		const char* ctrl_text = (sc->modifier & KMOD_CTRL)
			? "Ctrl"
			: (sc->modifier & KMOD_LCTRL)
				? "LCtrl"
				: (sc->modifier & KMOD_RCTRL)
					? "RCtrl"
					: "";
		const char* alt_text = (sc->modifier & KMOD_ALT)
			? "Alt"
			: (sc->modifier & KMOD_LALT)
				? "LAlt"
				: (sc->modifier & KMOD_RALT)
					? "RAlt"
					: "";
		const char* shift_text = (sc->modifier & KMOD_SHIFT)
			? "Shift"
			: (sc->modifier & KMOD_LSHIFT)
				? "LShift"
				: (sc->modifier & KMOD_RSHIFT)
					? "RShift"
					: "";

		SDL_Keycode kc = (sc->keycode != SDLK_UNKNOWN)
			? sc->keycode
			: (sc->scancode != SDL_SCANCODE_UNKNOWN)
				? SDL_GetKeyFromScancode(sc->scancode)
				: SDLK_UNKNOWN;

		if (kc == SDLK_UNKNOWN)
			continue;

		char *key_text;

		switch(sc->keycode) {
		case SDLK_RETURN:
#ifdef SCHISM_MACOSX
			key_text = strdup("Return");
#else
			key_text = strdup("Enter");
#endif
			break;
		case SDLK_EQUALS:
			key_text = strdup("Equals");
			break;
		case SDLK_MINUS:
			key_text = strdup("Minus");
			break;
		case SDLK_SPACE:
			key_text = strdup("Spacebar");
			break;
		default:
			key_text = strdup(SDL_GetKeyName(sc->keycode));
			break;
		}

		int key_text_length = strlen(key_text);
		if (!key_text_length)
			continue;

		out[i] = STR_IMPLODE(4, "-", ctrl_text, alt_text, shift_text, key_text);

		if(i == 0) {
			bind->first_shortcut_text = strdup(out[i]);
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

		puts(text);

		if (text && *text) {
			char* padded = str_pad_end(text, ' ', 18);
			out[i] = STR_CONCAT(2, "    ", padded);
			free(padded);
		}

		free(text);
	}

	char* help_shortcuts = str_implode_free(KEYBINDS_MAX_SHORTCUTS, "\n", out);
	bind->help_text = STR_CONCAT(3, help_shortcuts, (char*)bind->description, "\n");
	free(help_shortcuts);
}

void keybinds_init_shortcut(keybind_shortcut_t *sc)
{
	sc->scancode = SDL_SCANCODE_UNKNOWN;
	sc->keycode = SDLK_UNKNOWN;
	sc->modifier = KMOD_NONE;
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

	KEYBINDS_INIT_ASSERT(init_load_sample_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(init_load_stereo_sample_dialog_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(init_message_edit_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(init_waterfall_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(init_load_module_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(init_palette_edit_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(init_order_list_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(init_info_page_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(init_sample_list_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(init_instrument_list_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(init_pattern_edit_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(init_file_list_keybinds(&cfg));
	KEYBINDS_INIT_ASSERT(init_global_keybinds(&cfg));

#undef KEYBINDS_INIT_ASSERT

	cfg_write(&cfg);
	cfg_free(&cfg);

	free(path);

	log_appendf(5, " No issues");

	log_nl();
	return 1;
}

/* -------------------------------------------------------------------------- */

#define DESCRIPTION_SPACER "                      "
#define TEXT_2X "\n                  2x: "

#define INIT_SECTION(SECTION, TITLE, PAGE) \
	keybinds_init_section(&global_keybinds_list.SECTION##_info, #SECTION, TITLE, PAGE);

#define INIT_BIND(SECTION, BIND, DESCRIPTION, DEFAULT_KEYBIND) \
	do { \
		const char *shortcut = str_dup(cfg_get_string(cfg, #SECTION, #BIND, NULL, 0, DEFAULT_KEYBIND)); \
		cfg_set_string(cfg, #SECTION, #BIND, shortcut); \
	\
		if (!keybinds_init_bind(&global_keybinds_list.SECTION.BIND, &global_keybinds_list.SECTION ## _info, #BIND, DESCRIPTION, shortcut)) { \
			free((void *)shortcut); \
			return 0; \
		} \
	\
		free((void *)shortcut); \
	\
		if (!keybinds_append_bind_to_list(&global_keybinds_list.SECTION.BIND)) \
			return 0; \
	} while (0)

static int init_midi_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(midi, "Midi Keys.", PAGE_MIDI);
	INIT_BIND(midi, toggle_port, "Toggle port", "SPACE");
	return 1;
}

static int init_load_sample_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(load_sample, "Load Sample Keys.", PAGE_LOAD_SAMPLE);
	INIT_BIND(load_sample, toggle_multichannel, "Toggle multichannel mode", "Alt+N");
	return 1;
}

static int init_load_stereo_sample_dialog_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(load_stereo_sample_dialog, "Stereo Sample Dialog Keys.", PAGE_LOAD_SAMPLE);
	INIT_BIND(load_stereo_sample_dialog, load_left, "Load left channel", "L");
	INIT_BIND(load_stereo_sample_dialog, load_right, "Load right channel", "R");
	INIT_BIND(load_stereo_sample_dialog, load_both, "Load both channels", "B,S");
	return 1;
}

static int init_message_edit_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(message_edit, "Editing Keys.", PAGE_MESSAGE);
	INIT_BIND(message_edit, edit_message, "Edit message", "ENTER");
	INIT_BIND(message_edit, finished_editing, "Finished editing", "ESCAPE");
	INIT_BIND(message_edit, toggle_extended_font, "Toggle extended ASCII font", "Ctrl+T");
	INIT_BIND(message_edit, delete_line, "Delete line", "Ctrl+Y");
	INIT_BIND(message_edit, clear_message, "Clear message\n ", "Alt+C");

	INIT_BIND(message_edit, goto_first_line, "Go to first line", "Ctrl+HOME");
	INIT_BIND(message_edit, goto_last_line, "Go to last line", "Ctrl+END");
	return 1;
}

static int init_waterfall_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(waterfall, "Waterfall Keys.", PAGE_WATERFALL);
	INIT_BIND(waterfall, song_toggle_stereo, "Toggle song stereo/mono", "Alt+S");
	INIT_BIND(waterfall, song_flip_stereo, "Flip song left/right", "Alt+R");
	INIT_BIND(waterfall, view_toggle_mono, "Toggle mono display", "Alt+M");
	INIT_BIND(waterfall, decrease_sensitivity, "Decrease sensitivity", "LEFT");
	INIT_BIND(waterfall, increase_sensitivity, "Increase sensitivity", "RIGHT");
	INIT_BIND(waterfall, goto_previous_order, "Play previous order (while playing)", "KP_MINUS");
	INIT_BIND(waterfall, goto_next_order, "Play next order (while playing)", "KP_PLUS");
	INIT_BIND(waterfall, goto_pattern_edit, "Go to current position in pattern editor", "Alt+G");
	return 1;
}

static int init_load_module_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(load_module, "Load Module Keys.", PAGE_LOAD_MODULE);
	INIT_BIND(load_module, show_song_length, "Show song length", "Alt+P");
	INIT_BIND(load_module, clear_search_text, "Clear search text", "Ctrl+BACKSPACE");
	return 1;
}

static int init_palette_edit_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(palette_edit, "Palette Keys.", PAGE_PALETTE_EDITOR);
	INIT_BIND(palette_edit, copy, "Copy current palette to the clipboard", "Ctrl+C");
	INIT_BIND(palette_edit, paste, "Paste a palette from the clipboard", "Ctrl+V");
	return 1;
}

static int order_list_page_matcher(enum page_numbers page)
{
	return page == PAGE_ORDERLIST_PANNING || page == PAGE_ORDERLIST_VOLUMES;
}

static int init_order_list_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(order_list, "Order Keys.", PAGE_ORDERLIST_PANNING);
	global_keybinds_list.order_list_info.page_matcher = order_list_page_matcher;

	INIT_BIND(order_list, goto_selected_pattern, "Goto selected pattern", "ENTER,KP_ENTER,G");
	INIT_BIND(order_list, play_from_order, "Play from order", "Shift+F6");
	INIT_BIND(order_list, select_order_for_playback, "Select pattern for playback", "SPACE,Ctrl+F7");
	INIT_BIND(order_list, insert_next_pattern, "Insert next pattern", "N");
	INIT_BIND(order_list, duplicate_pattern, "Copy current pattern to new pattern, and insert order", "Shift+N");
	INIT_BIND(order_list, mark_end_of_song, "End of song mark", "MINUS,KP_MINUS,PERIOD");
	INIT_BIND(order_list, skip_to_next_order_mark, "Skip to next order mark", "Shift+EQUALS,KP_PLUS");
	INIT_BIND(order_list, insert_pattern, "Insert a pattern", "INSERT");
	INIT_BIND(order_list, delete_pattern, "Delete a pattern", "DELETE");

	// Is duplicate
	// INIT_BIND(order_list, toggle_order_list_locked, "Lock/unlock order list", "Alt+F11");
	INIT_BIND(order_list, sort_order_list, "Sort order list", "Alt+R");
	INIT_BIND(order_list, find_unused_patterns, "Search for unused patterns\n ", "Alt+U");

	INIT_BIND(order_list, link_pattern_to_sample, "Link (diskwriter) this pattern to the current sample", "Ctrl+B");
	INIT_BIND(order_list, copy_pattern_to_sample, "Copy (diskwriter) this pattern to the current sample", "Ctrl+O");
	INIT_BIND(order_list, copy_pattern_to_sample_with_split, "Copy (diskwriter) to the current sample, with split\n ", "Ctrl+Shift+O");

	INIT_BIND(order_list, continue_next_position_of_pattern, "Continue to next position of current pattern\n ", "C");

	INIT_BIND(order_list, save_order_list, "Save order list", "Alt+ENTER");
	INIT_BIND(order_list, restore_order_list, "Swap order list with saved order list", "Alt+BACKSPACE");

	// I'm not sure why this is here, but it is
	INIT_BIND(order_list, decrease_instrument, "Decrease instrument", "Shift+COMMA,Ctrl+UP");
	INIT_BIND(order_list, increase_instrument, "Increase instrument", "Shift+PERIOD,Ctrl+DOWN");

	INIT_SECTION(order_list_panning, "Panning Keys.", PAGE_ORDERLIST_PANNING);
	INIT_BIND(order_list_panning, toggle_channel_mute, "Toggle channel muted", "SPACE");
	INIT_BIND(order_list_panning, set_panning_left, "Set panning left", "L");
	INIT_BIND(order_list_panning, set_panning_middle, "Set panning middle", "M");
	INIT_BIND(order_list_panning, set_panning_right, "Set panning right", "R");
	INIT_BIND(order_list_panning, set_panning_surround, "Set panning surround", "S");
	INIT_BIND(order_list_panning, pan_unmuted_left, "Pan all unmuted channels left", "Alt+L");
	INIT_BIND(order_list_panning, pan_unmuted_middle, "Pan all unmuted channels middle", "Alt+M");
	INIT_BIND(order_list_panning, pan_unmuted_right, "Pan all unmuted channels right", "Alt+R");
	INIT_BIND(order_list_panning, pan_unmuted_stereo, "Pan all unmuted channels stereo", "Alt+S");
	INIT_BIND(order_list_panning, pan_unmuted_amiga_stereo, "Pan all unmuted channels amiga stereo", "Alt+A");
	INIT_BIND(order_list_panning, linear_panning_left_to_right, "Linear panning (left to right)", "Alt+BACKSLASH");
	INIT_BIND(order_list_panning, linear_panning_right_to_left, "Linear panning (right to left)", "Alt+SLASH,Alt+KP_DIVIDE");

	return 1;
}

static int init_info_page_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(info_page, "Info Page Keys.", PAGE_INFO);
	INIT_BIND(info_page, add_window, "Add a new window", "INSERT");
	INIT_BIND(info_page, delete_window, "Delete current window", "DELETE");
	INIT_BIND(info_page, nav_next_window, "Move to next window", "TAB");
	INIT_BIND(info_page, nav_previous_window, "Move to previous window", "Shift+TAB");
	INIT_BIND(info_page, change_window_type_up, "Change window type up", "PAGEUP");
	INIT_BIND(info_page, change_window_type_down, "Change window type down", "PAGEDOWN");
	INIT_BIND(info_page, move_window_base_up, "Move window base up", "Alt+UP");
	INIT_BIND(info_page, move_window_base_down, "Move window base down\n ", "Alt+DOWN");

	INIT_BIND(info_page, toggle_volume_velocity_bars, "Toggle between volume/velocity bars", "V");
	INIT_BIND(info_page, toggle_sample_instrument_names, "Toggle between sample/instrument names\n ", "I");

	INIT_BIND(info_page, toggle_channel_mute, "Mute/unmute current channel", "Q");
	INIT_BIND(info_page, toggle_channel_mute_and_go_next, "Mute/unmute current channel and go to next", "SPACE");
	INIT_BIND(info_page, solo_channel, "Solo current channel\n ", "S");

	INIT_BIND(info_page, goto_next_pattern, "Move forwards one pattern in song", "KP_PLUS");
	INIT_BIND(info_page, goto_previous_pattern, "Move backwards one pattern in song\n ", "KP_MINUS");

	INIT_BIND(info_page, toggle_stereo_playback, "Toggle stereo playback", "Alt+S");
	INIT_BIND(info_page, reverse_output_channels, "Reverse output channels\n ", "Alt+R");

	INIT_BIND(info_page, goto_playing_pattern, "Goto pattern currently playing", "G");

	return 1;
}

static int instrument_list_page_matcher(enum page_numbers page)
{
	return page == PAGE_INSTRUMENT_LIST_GENERAL || page == PAGE_INSTRUMENT_LIST_PANNING || page == PAGE_INSTRUMENT_LIST_PITCH || page == PAGE_INSTRUMENT_LIST_VOLUME;
}

static int init_instrument_list_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(instrument_list, "Instrument List Keys.", PAGE_INSTRUMENT_LIST);
	global_keybinds_list.instrument_list_info.page_matcher = instrument_list_page_matcher;

	INIT_BIND(instrument_list, next_page, "Next page", "F4");
	INIT_BIND(instrument_list, previous_page, "Previous page\n ", "Shift+F4");

	INIT_BIND(instrument_list, move_instrument_up, "Move instrument up (when on list)", "Alt+UP");
	INIT_BIND(instrument_list, move_instrument_down, "Move instrument down (when on list)\n ", "Alt+DOWN");

	INIT_BIND(instrument_list, goto_first_instrument, "Select first instrument (when on list)", "Ctrl+PAGEUP");
	INIT_BIND(instrument_list, goto_last_instrument, "Select last instrument (when on list)", "Ctrl+PAGEDOWN");

	// INIT_BIND(instrument_list, load_instrument, "Load new instrument", "ENTER");
	INIT_BIND(instrument_list, focus_list, "Focus on list", "Shift+ESCAPE");
	INIT_BIND(instrument_list, goto_instrument_up, "Goto instrument up (when not on list)", "Ctrl+PAGEUP");
	INIT_BIND(instrument_list, goto_instrument_down, "Goto instrument down (when not on list)", "Ctrl+PAGEDOWN");
	INIT_BIND(instrument_list, clear_name_and_filename, "Clean instrument name & filename", "Alt+C");
	INIT_BIND(instrument_list, wipe_data, "Wipe instrument data", "Alt+W");
	INIT_BIND(instrument_list, edit_name, "Edit instrument name (ESC to exit)\n ", "SPACE");

	INIT_BIND(instrument_list, delete_instrument_and_samples, "Delete instrument & all related samples", "Alt+D");
	INIT_BIND(instrument_list, delete_instrument_and_unused_samples, "Delete instrument & all related unused samples", "Alt+Shift+D");
	INIT_BIND(instrument_list, post_loop_cut, "Post-loop cut envelope", "Alt+L");
	INIT_BIND(instrument_list, toggle_multichannel, "Toggle multichannel playback", "Alt+N");
	INIT_BIND(instrument_list, save_to_disk, "Save current instrument to disk (IT Format)", "Alt+O");
	INIT_BIND(instrument_list, save_to_disk_exp, "Save current instrument to disk (Export Format)", "Alt+T");
	INIT_BIND(instrument_list, copy, "Copy instrument", "Alt+P");
	INIT_BIND(instrument_list, replace_in_song, "Replace current instrument in song", "Alt+R");
	INIT_BIND(instrument_list, swap, "Swap instruments (in song also)", "Alt+S");
	// Can't find this and can't figure out in previous version
	// INIT_BIND(instrument_list, update_pattern_data, "Update pattern data", "Alt+U");
	INIT_BIND(instrument_list, exchange, "Exchange instruments (only in instrument list)\n ", "Alt+X");

	INIT_BIND(instrument_list, insert_slot, "Insert instrument slot (updates pattern data)", "Alt+INSERT");
	INIT_BIND(instrument_list, remove_slot, "Remove instrument slot (updates pattern data)\n ", "Alt+DELETE");

	INIT_BIND(instrument_list, increase_playback_channel, "Increase playback channel", "Shift+PERIOD");
	INIT_BIND(instrument_list, decrease_playback_channel, "Decrease playback channel", "Shift+COMMA");

	INIT_SECTION(instrument_note_translation, "Note Translation.", PAGE_INSTRUMENT_LIST);
	global_keybinds_list.instrument_note_translation_info.page_matcher = instrument_list_page_matcher;

	INIT_BIND(instrument_note_translation, pickup_sample_number_and_default_play_note, "Pickup sample number & default play note", "ENTER");
	INIT_BIND(instrument_note_translation, increase_sample_number, "Increase sample number", "Shift+PERIOD,Ctrl+DOWN");
	INIT_BIND(instrument_note_translation, decrease_sample_number, "Decrease sample number\n ", "Shift+COMMA,Ctrl+UP");

	INIT_BIND(instrument_note_translation, change_all_samples, "Change all samples", "Alt+A");
	INIT_BIND(instrument_note_translation, change_all_samples_with_name, "Change all samples, copy name", "Alt+Shift+A");
	INIT_BIND(instrument_note_translation, enter_next_note, "Enter next note", "Alt+N");
	INIT_BIND(instrument_note_translation, enter_previous_note, "Enter previous note", "Alt+P");
	INIT_BIND(instrument_note_translation, transpose_all_notes_semitone_up, "Transpose all notes a semitone up", "Alt+UP");
	INIT_BIND(instrument_note_translation, transpose_all_notes_semitone_down, "Transpose all notes a semitone down", "Alt+DOWN");
	INIT_BIND(instrument_note_translation, insert_row_from_table, "Insert a row from the table", "Alt+INSERT");
	INIT_BIND(instrument_note_translation, delete_row_from_table, "Delete a row from the table", "Alt+DELETE");
	INIT_BIND(instrument_note_translation, toggle_edit_mask, "Toggle edit mask for current field", "COMMA");

	INIT_SECTION(instrument_envelope, "Envelope Keys.", PAGE_INSTRUMENT_LIST);
	global_keybinds_list.instrument_envelope_info.page_matcher = instrument_list_page_matcher;

	INIT_BIND(instrument_envelope, pick_up_or_drop_current_node, "Pick up/drop current node", "ENTER");
	INIT_BIND(instrument_envelope, add_node, "Add node", "INSERT");
	INIT_BIND(instrument_envelope, delete_node, "Delete node", "DELETE");
	INIT_BIND(instrument_envelope, nav_node_left, "Go to node left", "Ctrl+LEFT");
	INIT_BIND(instrument_envelope, nav_node_right, "Go to node right\n ", "Ctrl+RIGHT");

	INIT_BIND(instrument_envelope, move_node_left, "Move node left", "LEFT");
	INIT_BIND(instrument_envelope, move_node_right, "Move node right", "RIGHT");
	INIT_BIND(instrument_envelope, move_node_left_fast, "Move node left (fast)", "Alt+LEFT,TAB");
	INIT_BIND(instrument_envelope, move_node_right_fast, "Move node right (fast)", "Alt+RIGHT,Shift+TAB");
	INIT_BIND(instrument_envelope, move_node_left_max, "Move node left (max)", "HOME");
	INIT_BIND(instrument_envelope, move_node_right_max, "Move node right (max)", "END");
	INIT_BIND(instrument_envelope, move_node_up, "Move node up", "UP");
	INIT_BIND(instrument_envelope, move_node_down, "Move node down", "DOWN");
	INIT_BIND(instrument_envelope, move_node_up_fast, "Move node up (fast)", "Alt+UP,PAGEUP");
	INIT_BIND(instrument_envelope, move_node_down_fast, "Move node down (fast)\n ", "Alt+DOWN,PAGEDOWN");

	INIT_BIND(instrument_envelope, pre_loop_cut_envelope, "Pre-loop cut envelope", "Alt+B");
	INIT_BIND(instrument_envelope, double_envelope_length, "Double envelope length", "Alt+F");
	INIT_BIND(instrument_envelope, halve_envelope_length, "Halve envelope length", "Alt+G");
	INIT_BIND(instrument_envelope, resize_envelope, "Resize envelope", "Alt+E");
	INIT_BIND(instrument_envelope, generate_envelope_from_ADSR_values, "Generate envelope frome ADSR values\n ", "Alt+Z");

	INIT_BIND(instrument_envelope, play_default_note, "Play default note", "SPACE");
	// INIT_BIND(instrument_envelope, note_off, "Note off command", "");

	return 1;
}

static int init_sample_list_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(sample_list, "Sample List Keys.", PAGE_SAMPLE_LIST);
	// Don't really need these, they are better replaced by global navs
	// INIT_BIND(sample_list, load_new_sample, "Load new sample", "ENTER");
	// INIT_BIND(sample_list, move_between_options, "Move between options", "TAB");
	INIT_BIND(sample_list, move_up, "Move up (when not on list)", "PAGEUP");
	INIT_BIND(sample_list, move_down, "Move down (when not on list)", "PAGEDOWN");
	INIT_BIND(sample_list, focus_sample_list, "Focus on sample list\n ", "Shift+ESCAPE");

	INIT_BIND(sample_list, goto_first_sample, "Select first sample (when on list)", "Ctrl+PAGEUP");
	INIT_BIND(sample_list, goto_last_sample, "Select last sample (when on list)\n ", "Ctrl+PAGEDOWN");

	INIT_BIND(sample_list, convert_signed_unsigned, "Convert signed to/from unsigned samples", "Alt+A");
	INIT_BIND(sample_list, pre_loop_cut, "Pre-loop cut sample", "Alt+B");
	INIT_BIND(sample_list, clear_name_and_filename, "Clear sample name & filename (only in sample list)", "Alt+C");
	INIT_BIND(sample_list, delete_sample, "Delete sample", "Alt+D");
	INIT_BIND(sample_list, downmix_to_mono, "Downmix stero sample to mono", "Alt+Shift+D");
	INIT_BIND(sample_list, resize_sample_with_interpolation, "Resize sample (with interpolation)", "Alt+E");
	INIT_BIND(sample_list, resize_sample_without_interpolation, "Resize sample (without interpolation)", "Alt+F");
	INIT_BIND(sample_list, reverse_sample, "Reverse sample", "Alt+G");
	INIT_BIND(sample_list, centralise_sample, "Centralise sample", "Alt+H");
	INIT_BIND(sample_list, invert_sample, "Invert sample", "Alt+I");
	INIT_BIND(sample_list, post_loop_cut, "Post-loop cut sample", "Alt+L");
	INIT_BIND(sample_list, sample_amplifier, "Sample amplifier", "ALT+M");
	INIT_BIND(sample_list, toggle_multichannel_playback, "Toggle multichannel playback", "Alt+N");
	INIT_BIND(sample_list, save_sample_to_disk_it, "Save current sample to disk (IT format)", "Alt+O");
	INIT_BIND(sample_list, copy_sample, "Copy sample", "Alt+P");
	INIT_BIND(sample_list, toggle_sample_quality, "Toggle sample quality", "Alt+Q");
	INIT_BIND(sample_list, replace_current_sample, "Replace current sample in song", "Alt+R");
	INIT_BIND(sample_list, swap_sample, "Swap sample (in song also)", "Alt+S");
	INIT_BIND(sample_list, save_sample_to_disk_format_select, "Save current sample to disk (choose format)", "Alt+T");
	INIT_BIND(sample_list, save_sample_to_disk_raw, "Save current sample to disk (RAW format)", "Alt+W");
	INIT_BIND(sample_list, exchange_sample, "Exchange sample (only in sample list)", "Alt+X");
	INIT_BIND(sample_list, text_to_sample, "Text to sample data", "Alt+Y");
	INIT_BIND(sample_list, edit_create_adlib_sample, "Edit/create AdLib (FM) sample", "Alt+Z");
	INIT_BIND(sample_list, load_adlib_sample_by_midi_patch_number, "Load predefined AdLib sample by MIDI patch number\n ", "Alt+Shift+Z");

	INIT_BIND(sample_list, insert_sample_slot, "Insert sample slot (updates pattern data)", "Alt+INSERT");
	INIT_BIND(sample_list, remove_sample_slot, "Remove sample slot (updates pattern data)", "Alt+DELETE");
	INIT_BIND(sample_list, swap_sample_with_previous, "Swap sample with previous", "Alt+UP");
	INIT_BIND(sample_list, swap_sample_with_next, "Swap sample with next\n ", "Alt+DOWN");

	INIT_BIND(sample_list, toggle_current_sample, "Toggle current sample", "Alt+F9");
	INIT_BIND(sample_list, solo_current_sample, "Solo current sample\n ", "Alt+F10");

	INIT_BIND(sample_list, decrease_playback_channel, "Decrease playback channel", "Alt+Shift+COMMA");
	INIT_BIND(sample_list, increase_playback_channel, "Increase playback channel\n ", "Alt+Shift+PERIOD");

	INIT_BIND(sample_list, increase_c5_frequency_1_octave, "Increase C-5 frequency by 1 octave", "Alt+KP_PLUS");
	INIT_BIND(sample_list, decrease_c5_frequency_1_octave, "Decrease C-5 frequency by 1 octave", "Alt+KP_MINUS");
	INIT_BIND(sample_list, increase_c5_frequency_1_semitone, "Increase C-5 frequency by 1 semitone", "Ctrl+KP_PLUS");
	INIT_BIND(sample_list, decrease_c5_frequency_1_semitone, "Decrease C-5 frequency by 1 semitone\n ", "Ctrl+KP_MINUS");

	INIT_BIND(sample_list, insert_arrow_up, "Insert arrow up (on list)", "Ctrl+BACKSPACE");

	return 1;
}

static int init_pattern_edit_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(pattern_edit, "Pattern Edit Keys.", PAGE_PATTERN_EDITOR);
	INIT_BIND(pattern_edit, next_pattern, "Next pattern (*)", "KP_PLUS");
	INIT_BIND(pattern_edit, previous_pattern, "Previous pattern (*)", "KP_MINUS");
	INIT_BIND(pattern_edit, next_4_pattern, "Next 4 pattern (*)", "Shift+KP_PLUS");
	INIT_BIND(pattern_edit, previous_4_pattern, "Previous 4 pattern (*)", "Shift+KP_MINUS");
	INIT_BIND(pattern_edit, next_order_pattern, "Next order's pattern (*)", "Ctrl+KP_PLUS");
	INIT_BIND(pattern_edit, previous_order_pattern,
		"Previous order's pattern (*)\n"
		"    0-9               Change octave/volume/instrument\n"
		"    0-9, A-F          Change effect value\n"
		"    A-Z               Change effect",
		"Ctrl+KP_MINUS");
	INIT_BIND(pattern_edit, clear_field, "Clear field(s)", "PERIOD");
	INIT_BIND(pattern_edit, note_cut, "Note cut (^^^)", "1");
	INIT_BIND(pattern_edit, note_off, "Note off (===) / panning toggle", "BACKQUOTE");
	INIT_BIND(pattern_edit, toggle_volume_panning, "Toggle panning / volume (on volume field)", "BACKQUOTE");
	INIT_BIND(pattern_edit, note_fade, "Note fade (~~~)", "Shift+BACKQUOTE");
	INIT_BIND(pattern_edit, use_last_value,
		"Use last note/instrument/volume/effect/effect value\n"
		"    Caps Lock+Key     Preview note\n ",
		"SPACE");

	INIT_BIND(pattern_edit, get_default_value, "Get default note/instrument/volume/effect", "ENTER");
	INIT_BIND(pattern_edit, decrease_instrument, "Decrease instrument", "Shift+COMMA,Ctrl+UP");
	INIT_BIND(pattern_edit, increase_instrument, "Increase instrument", "Shift+PERIOD,Ctrl+DOWN");
	INIT_BIND(pattern_edit, toggle_edit_mask, "Toggle edit mask for current field\n ", "COMMA");

	INIT_BIND(pattern_edit, insert_row, "Insert a row to current channel", "INSERT");
	INIT_BIND(pattern_edit, delete_row, "Delete a row from current channel\n ", "DELETE");

	INIT_BIND(pattern_edit, insert_pattern_row, "Insert an entire row to pattern (*)", "Alt+INSERT");
	INIT_BIND(pattern_edit, delete_pattern_row, "Delete an entire row from pattern (*)\n ", "Alt+DELETE");

	INIT_BIND(pattern_edit, up_by_skip, "Move up by the skip value (set with Alt 1-9)", "UP");
	INIT_BIND(pattern_edit, down_by_skip, "Move down by the skip value", "DOWN");
	INIT_BIND(pattern_edit, set_skip_1, "Set skip value to 1", "Alt+1");
	INIT_BIND(pattern_edit, set_skip_2, "Set skip value to 2", "Alt+2");
	INIT_BIND(pattern_edit, set_skip_3, "Set skip value to 3", "Alt+3");
	INIT_BIND(pattern_edit, set_skip_4, "Set skip value to 4", "Alt+4");
	INIT_BIND(pattern_edit, set_skip_5, "Set skip value to 5", "Alt+5");
	INIT_BIND(pattern_edit, set_skip_6, "Set skip value to 6", "Alt+6");
	INIT_BIND(pattern_edit, set_skip_7, "Set skip value to 7", "Alt+7");
	INIT_BIND(pattern_edit, set_skip_8, "Set skip value to 8", "Alt+8");
	INIT_BIND(pattern_edit, set_skip_9, "Set skip value to 16\n ", "Alt+9");

	INIT_BIND(pattern_edit, up_one_row, "Move up by 1 row", "Ctrl+HOME");
	INIT_BIND(pattern_edit, down_one_row, "Move down by 1 row", "Ctrl+END");
	INIT_BIND(pattern_edit, slide_pattern_up, "Slide pattern up by 1 row", "Alt+UP");
	INIT_BIND(pattern_edit, slide_pattern_down, "Slide pattern down by 1 row", "Alt+DOWN");
	INIT_BIND(pattern_edit, move_cursor_left, "Move cursor left", "LEFT");
	INIT_BIND(pattern_edit, move_cursor_right, "Move cursor right", "RIGHT");
	INIT_BIND(pattern_edit, move_forwards_channel, "Move forwards one channel", "Alt+RIGHT,Ctrl+RIGHT");
	INIT_BIND(pattern_edit, move_backwards_channel, "Move backwards one channel", "Alt+LEFT,Ctrl+LEFT");
	INIT_BIND(pattern_edit, move_forwards_note_column, "Move forwards to note column", "TAB");
	INIT_BIND(pattern_edit, move_backwards_note_column, "Move backwards to note column", "Shift+TAB");
	INIT_BIND(pattern_edit, move_up_n_lines, "Move up n lines (n=row highlight major)", "PAGEUP");
	INIT_BIND(pattern_edit, move_down_n_lines, "Move down n lines", "PAGEDOWN");
	INIT_BIND(pattern_edit, move_pattern_top, "Move to top of pattern", "Ctrl+PAGEUP");
	INIT_BIND(pattern_edit, move_pattern_bottom, "Move to bottom of pattern", "Ctrl+PAGEDOWN");
	INIT_BIND(pattern_edit, move_start, "Move to start of column/start of line/start of pattern", "HOME");
	INIT_BIND(pattern_edit, move_end, "Move to end of column/end of line/end of pattern", "END");
	INIT_BIND(pattern_edit, move_previous_position, "Move to previous position (accounts for multichannel)", "BACKSPACE");
	INIT_BIND(pattern_edit, move_previous, "Move to previous note/instrument/volume/effect", "Shift+A");
	INIT_BIND(pattern_edit, move_next, "Move to next note/instrument/volume/effect\n ", "Shift+F");

	INIT_BIND(pattern_edit, toggle_multichannel,
		"Toggle multichannel mode for current channel" TEXT_2X
		"Multichannel selection menu\n ", "Alt+N");

	INIT_BIND(pattern_edit, store_pattern_data, "Store pattern data", "Alt+ENTER");
	INIT_BIND(pattern_edit, revert_pattern_data, "Revert pattern data (*)", "Alt+BACKSPACE");
	INIT_BIND(pattern_edit, undo, "Undo - any function with (*) can be undone\n ", "Ctrl+BACKSPACE");

	INIT_BIND(pattern_edit, toggle_centralise_cursor, "Toggle centralise cursor", "Ctrl+C");
	INIT_BIND(pattern_edit, toggle_highlight_row, "Toggle current row highlight", "Ctrl+H");
	INIT_BIND(pattern_edit, toggle_volume_display, "Toggle default volume display\n ", "Ctrl+V");

	INIT_BIND(pattern_edit, set_pattern_length, "Set pattern length", "Ctrl+F2");
	INIT_BIND(pattern_edit, toggle_midi_trigger, "Toggle MIDI trigger", "Ctrl+X,Ctrl+Z");

	INIT_SECTION(track_view, " Track View Functions.", PAGE_PATTERN_EDITOR);
	INIT_BIND(track_view, cycle_view, "Cycle current track's view", "Alt+T");
	INIT_BIND(track_view, clear_track_views, "Clear all track views", "Alt+R");
	INIT_BIND(track_view, toggle_track_view_divisions, "Toggle track view divisions", "Alt+H");

	INIT_BIND(track_view, track_scheme_default, "View current track in scheme default", "Ctrl+0,Ctrl+BACKQUOTE");
	INIT_BIND(track_view, track_scheme_1, "View current track in scheme 1", "Ctrl+1");
	INIT_BIND(track_view, track_scheme_2, "View current track in scheme 2", "Ctrl+2");
	INIT_BIND(track_view, track_scheme_3, "View current track in scheme 3", "Ctrl+3");
	INIT_BIND(track_view, track_scheme_4, "View current track in scheme 4", "Ctrl+4");
	INIT_BIND(track_view, track_scheme_5, "View current track in scheme 5", "Ctrl+5");
	INIT_BIND(track_view, track_scheme_6, "View current track in scheme 6\n ", "Ctrl+6");

	// INIT_BIND(track_view, move_column_left, "Go to channel left (keep column position)", "Ctrl+LEFT");
	// INIT_BIND(track_view, move_column_right, "Go to channel right (keep column position)\n ", "Ctrl+RIGHT");

	// I added backquote here because Ctrl+Shift+0 is not possible to use on windows. (took me so long to figure this out XD)
	INIT_BIND(track_view, quick_view_scheme_default, "Quick view scheme setup default", "Ctrl+Shift+0,Ctrl+Shift+BACKQUOTE");
	INIT_BIND(track_view, quick_view_scheme_1, "Quick view scheme setup 1", "Ctrl+Shift+1");
	INIT_BIND(track_view, quick_view_scheme_2, "Quick view scheme setup 2", "Ctrl+Shift+2");
	INIT_BIND(track_view, quick_view_scheme_3, "Quick view scheme setup 3", "Ctrl+Shift+3");
	INIT_BIND(track_view, quick_view_scheme_4, "Quick view scheme setup 4", "Ctrl+Shift+4");
	INIT_BIND(track_view, quick_view_scheme_5, "Quick view scheme setup 5", "Ctrl+Shift+5");
	INIT_BIND(track_view, quick_view_scheme_6, "Quick view scheme setup 6\n ", "Ctrl+Shift+6");

	// Can't find this and can't figure out in previous version
	// INIT_BIND(track_view, toggle_cursor_tracking, "Toggle View-Channel cursor-tracking\n ", "Ctrl+T");

	INIT_SECTION(block_functions, " Block Functions.", PAGE_PATTERN_EDITOR);
	INIT_BIND(block_functions, mark_beginning_block, "Mark beginning of block", "Alt+B");
	INIT_BIND(block_functions, mark_end_block, "Mark end of block", "Alt+E");
	INIT_BIND(block_functions, quick_mark_lines, "Quick mark n/2n/4n/... lines (n=row highlight major)", "Alt+D");
	INIT_BIND(block_functions, mark_column_or_pattern, "Mark entire column/pattern", "Alt+L");
	INIT_BIND(block_functions, mark_block_left, "Mark block left", "Shift+LEFT");
	INIT_BIND(block_functions, mark_block_right, "Mark block right", "Shift+RIGHT");
	INIT_BIND(block_functions, mark_block_up, "Mark block up", "Shift+UP");
	INIT_BIND(block_functions, mark_block_down, "Mark block down\n ", "Shift+DOWN");
	INIT_BIND(block_functions, mark_block_start_row, "Mark block start of row/rows/pattern", "Shift+HOME");
	INIT_BIND(block_functions, mark_block_end_row, "Mark block end of row/rows/pattern", "Shift+END");
	INIT_BIND(block_functions, mark_block_page_up, "Mark block up one page", "Shift+PAGEUP");
	INIT_BIND(block_functions, mark_block_page_down, "Mark block down one page\n ", "Shift+PAGEDOWN");

	INIT_BIND(block_functions, unmark, "Unmark block/release clipboard memory\n ", "Alt+U");

	INIT_BIND(block_functions, raise_notes_semitone, "Raise notes by a semitone (*)", "Alt+Q");
	INIT_BIND(block_functions, raise_notes_octave, "Raise notes by an octave (*)", "Alt+Shift+Q");
	INIT_BIND(block_functions, lower_notes_semitone, "Lower notes by a semitone (*)", "Alt+A");
	INIT_BIND(block_functions, lower_notes_octave, "Lower notes by an octave (*)", "Alt+Shift+A");
	INIT_BIND(block_functions, set_instrument, "Set Instrument (*)", "Alt+S");
	INIT_BIND(block_functions, set_volume_or_panning, "Set volume/panning (*)", "Alt+V");
	INIT_BIND(block_functions, wipe_volume_or_panning, "Wipe vol/pan not associated with a note/instrument (*)", "Alt+W");
	INIT_BIND(block_functions, slide_volume_or_panning,
		"Slide volume/panning column (*)" TEXT_2X
		"Wipe all volume/panning controls (*)", "Alt+K");
	INIT_BIND(block_functions, volume_amplifier, "Volume amplifier (*) / fast volume attenuate (*)", "Alt+J");
	INIT_BIND(block_functions, cut_block, "Cut block (*)", "Alt+Z");
	INIT_BIND(block_functions, swap_block, "Swap block (*)", "Alt+Y");
	INIT_BIND(block_functions, slide_effect_value,
		"Slide effect value (*)" TEXT_2X
		"Wipe all effect data (*)\n ", "Alt+X");

	INIT_BIND(block_functions, roll_block_down, "Roll block down", "Ctrl+INSERT");
	INIT_BIND(block_functions, roll_block_up, "Roll block up", "Ctrl+DELETE");

	INIT_BIND(block_functions, copy_block, "Copy block into clipboard", "Alt+C");
	INIT_BIND(block_functions, copy_block_with_mute, "Copy block to clipboard honoring current mute-settings", "Shift+L");
	INIT_BIND(block_functions, paste_data, "Paste data from clipboard (*)", "Alt+P");
	INIT_BIND(block_functions, paste_and_overwrite,
		"Overwrite with data from clipboard (*)" TEXT_2X
		"Grow pattern to clipboard length", "Alt+O");
	INIT_BIND(block_functions, paste_and_mix,
		"Mix each row from clipboard with pattern data (*)" TEXT_2X
		"Mix each field from clipboard with pattern data\n ", "Alt+M");

	INIT_BIND(block_functions, double_block_length, "Double block length (*)", "Alt+F");
	INIT_BIND(block_functions, halve_block_length, "Halve block length (*)\n ", "Alt+G");

	INIT_BIND(block_functions, select_template_mode, "Select template mode / fast volume amplify (*)", "Alt+I");
	INIT_BIND(block_functions, disable_template_mode, "Disable template mode", "Alt+Shift+I");
	INIT_BIND(block_functions, toggle_fast_volume, "Toggle fast volume mode", "Ctrl+J");
	INIT_BIND(block_functions, selection_volume_vary, "Selection volume vary / fast volume vary (*)", "Ctrl+U");
	INIT_BIND(block_functions, selection_panning_vary, "Selection panning vary / fast panning vary (*)", "Ctrl+Y");
	INIT_BIND(block_functions, selection_effect_vary, "Selection effect vary / fast effect vary (*)", "Ctrl+K");

	INIT_SECTION(playback_functions, " Playback Functions.", PAGE_PATTERN_EDITOR);
	INIT_BIND(playback_functions, play_note_cursor, "Play note under cursor", "4");
	INIT_BIND(playback_functions, play_row, "Play row\n ", "8");

	INIT_BIND(playback_functions, play_from_row, "Play from current row", "Ctrl+F6");
	INIT_BIND(playback_functions, toggle_playback_mark, "Set/clear playback mark (for use with F7)\n ", "Ctrl+F7");

	INIT_BIND(playback_functions, toggle_current_channel, "Toggle current channel", "Alt+F9");
	INIT_BIND(playback_functions, solo_current_channel, "Solo current channel", "Alt+F10");

	return 1;
}

static int file_list_page_matcher(enum page_numbers page)
{
	return page == PAGE_LOAD_INSTRUMENT || page == PAGE_LOAD_MODULE;
	return page == PAGE_LOAD_INSTRUMENT || page == PAGE_LOAD_MODULE || page == PAGE_LOAD_SAMPLE;
}

static int init_file_list_keybinds(cfg_file_t* cfg)
{
	INIT_SECTION(file_list, "File List Keys.", PAGE_GLOBAL);
	global_keybinds_list.file_list_info.page_matcher = file_list_page_matcher;

	INIT_BIND(file_list, delete, "Delete file", "DELETE");

	return 1;
}

static int init_global_keybinds(cfg_file_t* cfg)
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
	INIT_BIND(global, waterfall, "Waterfall\n ", "Alt+F12");

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
