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

#ifndef SCHISM_KEYBINDS_H_
#define SCHISM_KEYBINDS_H_

#include "sdlmain.h"
#include "page.h"
#include "config.h"

/* *** TYPES *** */

#define KEYBINDS_MAX_SHORTCUTS 3

enum keybind_modifier {
	/* these are treated as different modifiers */
	KEYBIND_MOD_NONE = (0),
	KEYBIND_MOD_LALT = (1 << 0),
	KEYBIND_MOD_RALT = (1 << 1),
	KEYBIND_MOD_ALT = (1 << 2),
	KEYBIND_MOD_LSHIFT = (1 << 3),
	KEYBIND_MOD_RSHIFT = (1 << 4),
	KEYBIND_MOD_SHIFT = (1 << 5),
	KEYBIND_MOD_RCTRL = (1 << 6),
	KEYBIND_MOD_LCTRL = (1 << 7),
	KEYBIND_MOD_CTRL = (1 << 8),
};

typedef struct keybind_shortcut
{
	SDL_Scancode scancode;
	SDL_Keycode keycode;
	enum keybind_modifier modifier;
	int pressed;
	int released;
	int repeated;
	int is_press_repeat;
	int press_repeats;
} keybind_shortcut_t;

typedef struct keybind_section_info
{
	const char* name;
	const char* title;
	int is_active;
	enum page_numbers page;
	int (*page_matcher)(enum page_numbers);
} keybind_section_info_t;

typedef struct keybind_bind
{
	keybind_section_info_t* section_info;
	keybind_shortcut_t shortcuts[KEYBINDS_MAX_SHORTCUTS]; // This array size is KEYBINDS_MAX_SHORTCUTS
	int shortcuts_count; // The amount of shortcuts that are actually initialized
	const char* name; // This is the variable name, for easier debugging
	const char* description; // Text that shows up on help pages
	const char* shortcut_text; // Contains all shortcuts with commas in-between, for example "F10, Ctrl-W"
	const char* first_shortcut_text; // Contains only the first shortcut, for example "F10"
	const char* shortcut_text_parens; // shortcut_text but with parenthesis around it. No parenthesis if there is no shortcut
	const char* first_shortcut_text_parens; // Only first shortcut, with parenthesis around
	const char* help_text; // Text formatted for showing on help page
	int pressed; // 1 on the event when this key was pressed down
	int released; // 1 on the event when this key was released
	int repeated; // 1 when the key was repeated (without releasing it)
	int press_repeats; // Number of times this key has been pressed in a row
} keybind_bind_t;

/* *** KEYBIND LIST *** */

typedef struct keybind_list
{
	/* *** MIDI *** */

	keybind_section_info_t midi_info;
	struct keybinds_midi {
		keybind_bind_t toggle_port;
	} midi;

	/* *** LOAD SAMPLE *** */

	keybind_section_info_t load_sample_info;
	struct keybinds_load_sample {
		keybind_bind_t toggle_multichannel;
	} load_sample;

	/* *** LOAD STEREO SAMPLE DIALOG *** */

	keybind_section_info_t load_stereo_sample_dialog_info;
	struct keybinds_load_stereo_sample_dialog {
		keybind_bind_t load_left;
		keybind_bind_t load_right;
		keybind_bind_t load_both;
	} load_stereo_sample_dialog;

	/* *** MESSAGE EDIT *** */

	keybind_section_info_t message_edit_info;
	struct keybinds_message_edit {
		keybind_bind_t edit_message;
		keybind_bind_t finished_editing;
		keybind_bind_t toggle_extended_font;
		keybind_bind_t delete_line;
		keybind_bind_t clear_message;

		keybind_bind_t goto_first_line;
		keybind_bind_t goto_last_line;
	} message_edit;

	/* *** WATERFALL *** */

	keybind_section_info_t waterfall_info;
	struct keybinds_waterfall {
		keybind_bind_t song_toggle_stereo;
		keybind_bind_t song_flip_stereo;
		keybind_bind_t view_toggle_mono;
		keybind_bind_t decrease_sensitivity;
		keybind_bind_t increase_sensitivity;

		keybind_bind_t goto_next_order;
		keybind_bind_t goto_previous_order;

		keybind_bind_t goto_next_channel;
		keybind_bind_t goto_previous_channel;

		keybind_bind_t goto_next_instrument;
		keybind_bind_t goto_previous_instrument;

		keybind_bind_t goto_pattern_edit;
	} waterfall;

	/* *** MIDI *** */

	keybind_section_info_t time_information_info;
	struct keybinds_time_information {
		keybind_bind_t toggle_session;
	} time_information;

	/* *** LOAD MODULE *** */

	keybind_section_info_t load_module_info;
	struct keybinds_load_module {
		keybind_bind_t show_song_length;
		keybind_bind_t clear_search_text;
	} load_module;

	/* *** PALETTE EDIT *** */

	keybind_section_info_t palette_edit_info;
	struct keybinds_palette_edit {
		keybind_bind_t copy;
		keybind_bind_t paste;
	} palette_edit;

	/* *** ORDER LIST *** */

	keybind_section_info_t order_list_info;
	struct keybinds_order_list {
		keybind_bind_t goto_selected_pattern;
		keybind_bind_t select_order_for_playback;
		keybind_bind_t play_from_order;
		keybind_bind_t insert_next_pattern;
		keybind_bind_t duplicate_pattern;
		keybind_bind_t mark_end_of_song;
		keybind_bind_t skip_to_next_order_mark;
		keybind_bind_t insert_pattern;
		keybind_bind_t delete_pattern;

		// Is duplicate
		// keybind_bind_t toggle_order_list_locked;
		keybind_bind_t sort_order_list;
		keybind_bind_t find_unused_patterns;

		keybind_bind_t link_pattern_to_sample;
		keybind_bind_t copy_pattern_to_sample;
		keybind_bind_t copy_pattern_to_sample_with_split;

		keybind_bind_t continue_next_position_of_pattern;

		keybind_bind_t save_order_list;
		keybind_bind_t restore_order_list;

		keybind_bind_t decrease_instrument;
		keybind_bind_t increase_instrument;
	} order_list;

	keybind_section_info_t order_list_panning_info;
	struct keybinds_order_list_panning {
		keybind_bind_t toggle_channel_mute;
		keybind_bind_t set_panning_left;
		keybind_bind_t set_panning_middle;
		keybind_bind_t set_panning_right;
		keybind_bind_t set_panning_surround;
		keybind_bind_t pan_unmuted_left;
		keybind_bind_t pan_unmuted_middle;
		keybind_bind_t pan_unmuted_right;
		keybind_bind_t pan_unmuted_stereo;
		keybind_bind_t pan_unmuted_amiga_stereo;
		keybind_bind_t linear_panning_left_to_right;
		keybind_bind_t linear_panning_right_to_left;
	} order_list_panning;

	/* *** INFO PAGE *** */

	keybind_section_info_t info_page_info;

	struct keybinds_info_page {
		keybind_bind_t add_window;
		keybind_bind_t delete_window;
		keybind_bind_t nav_next_window;
		keybind_bind_t nav_previous_window;
		keybind_bind_t change_window_type_up;
		keybind_bind_t change_window_type_down;
		keybind_bind_t move_window_base_up;
		keybind_bind_t move_window_base_down;

		keybind_bind_t toggle_volume_velocity_bars;
		keybind_bind_t toggle_sample_instrument_names;

		keybind_bind_t toggle_channel_mute;
		keybind_bind_t toggle_channel_mute_and_go_next;
		keybind_bind_t solo_channel;

		keybind_bind_t goto_next_pattern;
		keybind_bind_t goto_previous_pattern;

		keybind_bind_t toggle_stereo_playback;
		keybind_bind_t reverse_output_channels;

		keybind_bind_t goto_playing_pattern;
	} info_page;

	/* *** INSTRUMENT LIST *** */

	keybind_section_info_t instrument_list_info;
	struct keybinds_instrument_list {
		keybind_bind_t next_page;
		keybind_bind_t previous_page;

		keybind_bind_t move_instrument_up;
		keybind_bind_t move_instrument_down;

		keybind_bind_t goto_first_instrument;
		keybind_bind_t goto_last_instrument;

		// keybind_bind_t load_instrument;
		keybind_bind_t focus_list;
		keybind_bind_t goto_instrument_up;
		keybind_bind_t goto_instrument_down;
		keybind_bind_t clear_name_and_filename;
		keybind_bind_t wipe_data;
		keybind_bind_t edit_name;

		keybind_bind_t delete_instrument_and_samples;
		keybind_bind_t delete_instrument_and_unused_samples;
		keybind_bind_t post_loop_cut;
		keybind_bind_t toggle_multichannel;
		keybind_bind_t save_to_disk;
		keybind_bind_t save_to_disk_exp;
		keybind_bind_t copy;
		keybind_bind_t replace_in_song;
		keybind_bind_t swap;

		// Can't find this and can't figure out in previous version
		// keybind_bind_t update_pattern_data;
		keybind_bind_t exchange;

		keybind_bind_t insert_slot;
		keybind_bind_t remove_slot;

		keybind_bind_t increase_playback_channel;
		keybind_bind_t decrease_playback_channel;
	} instrument_list;

	keybind_section_info_t instrument_note_translation_info;
	struct keybinds_instrument_note_translation {
		keybind_bind_t pickup_sample_number_and_default_play_note;
		keybind_bind_t increase_sample_number;
		keybind_bind_t decrease_sample_number;

		keybind_bind_t change_all_samples;
		keybind_bind_t change_all_samples_with_name;
		keybind_bind_t enter_next_note;
		keybind_bind_t enter_previous_note;
		keybind_bind_t transpose_all_notes_semitone_up;
		keybind_bind_t transpose_all_notes_semitone_down;
		keybind_bind_t insert_row_from_table;
		keybind_bind_t delete_row_from_table;
		keybind_bind_t toggle_edit_mask;
	} instrument_note_translation;

	keybind_section_info_t instrument_envelope_info;
	struct keybinds_instrument_envelope {
		keybind_bind_t pick_up_or_drop_current_node;
		keybind_bind_t add_node;
		keybind_bind_t delete_node;
		keybind_bind_t nav_node_left;
		keybind_bind_t nav_node_right;

		keybind_bind_t move_node_left;
		keybind_bind_t move_node_right;
		keybind_bind_t move_node_left_fast;
		keybind_bind_t move_node_right_fast;
		keybind_bind_t move_node_left_max;
		keybind_bind_t move_node_right_max;
		keybind_bind_t move_node_up;
		keybind_bind_t move_node_down;
		keybind_bind_t move_node_up_fast;
		keybind_bind_t move_node_down_fast;

		keybind_bind_t pre_loop_cut_envelope;
		keybind_bind_t double_envelope_length;
		keybind_bind_t halve_envelope_length;
		keybind_bind_t resize_envelope;
		keybind_bind_t generate_envelope_from_ADSR_values;

		keybind_bind_t play_default_note;
		// keybind_bind_t note_off;
	} instrument_envelope;

	/* *** SAMPLE LIST *** */

	keybind_section_info_t sample_list_info;
	struct keybinds_sample_list {
		// keybind_bind_t load_new_sample;
		// keybind_bind_t move_between_options;
		keybind_bind_t move_up;
		keybind_bind_t move_down;
		keybind_bind_t focus_sample_list;

		keybind_bind_t goto_first_sample;
		keybind_bind_t goto_last_sample;

		keybind_bind_t convert_signed_unsigned;
		keybind_bind_t pre_loop_cut;
		keybind_bind_t clear_name_and_filename;
		keybind_bind_t delete_sample;
		keybind_bind_t downmix_to_mono;
		keybind_bind_t resize_sample_with_interpolation;
		keybind_bind_t resize_sample_without_interpolation;
		keybind_bind_t reverse_sample;
		keybind_bind_t centralise_sample;
		keybind_bind_t invert_sample;
		keybind_bind_t post_loop_cut;
		keybind_bind_t sample_amplifier;
		keybind_bind_t toggle_multichannel_playback;
		keybind_bind_t save_sample_to_disk_it;
		keybind_bind_t copy_sample;
		keybind_bind_t toggle_sample_quality;
		keybind_bind_t replace_current_sample;
		keybind_bind_t swap_sample;
		keybind_bind_t save_sample_to_disk_format_select;
		keybind_bind_t save_sample_to_disk_raw;
		keybind_bind_t exchange_sample;
		keybind_bind_t text_to_sample;
		keybind_bind_t edit_create_adlib_sample;
		keybind_bind_t load_adlib_sample_by_midi_patch_number;

		keybind_bind_t insert_sample_slot;
		keybind_bind_t remove_sample_slot;
		keybind_bind_t swap_sample_with_previous;
		keybind_bind_t swap_sample_with_next;

		keybind_bind_t toggle_current_sample;
		keybind_bind_t solo_current_sample;

		keybind_bind_t decrease_playback_channel;
		keybind_bind_t increase_playback_channel;

		keybind_bind_t increase_c5_frequency_1_octave;
		keybind_bind_t decrease_c5_frequency_1_octave;

		keybind_bind_t increase_c5_frequency_1_semitone;
		keybind_bind_t decrease_c5_frequency_1_semitone;

		keybind_bind_t insert_arrow_up;
	} sample_list;

	/* *** PATTERN EDIT *** */

	keybind_section_info_t pattern_edit_info;
	struct keybinds_pattern_edit {
		keybind_bind_t next_pattern;
		keybind_bind_t previous_pattern;
		keybind_bind_t next_4_pattern;
		keybind_bind_t previous_4_pattern;
		keybind_bind_t next_order_pattern;
		keybind_bind_t previous_order_pattern;
		keybind_bind_t clear_field;
		keybind_bind_t note_cut;
		keybind_bind_t note_off;
		keybind_bind_t toggle_volume_panning;
		keybind_bind_t note_fade;
		keybind_bind_t use_last_value;
		// keybind_bind_t preview_note;

		keybind_bind_t get_default_value;
		keybind_bind_t decrease_instrument;
		keybind_bind_t increase_instrument;
		// These are listed on pattern edit help but are actually global
		// keybind_bind_t decrease_octave;
		// keybind_bind_t increase_octave;
		keybind_bind_t toggle_edit_mask;

		keybind_bind_t insert_row;
		keybind_bind_t delete_row;
		keybind_bind_t insert_pattern_row;
		keybind_bind_t delete_pattern_row;

		keybind_bind_t up_by_skip;
		keybind_bind_t down_by_skip;
		keybind_bind_t set_skip_1;
		keybind_bind_t set_skip_2;
		keybind_bind_t set_skip_3;
		keybind_bind_t set_skip_4;
		keybind_bind_t set_skip_5;
		keybind_bind_t set_skip_6;
		keybind_bind_t set_skip_7;
		keybind_bind_t set_skip_8;
		keybind_bind_t set_skip_9;

		keybind_bind_t up_one_row;
		keybind_bind_t down_one_row;
		keybind_bind_t slide_pattern_up;
		keybind_bind_t slide_pattern_down;
		keybind_bind_t move_cursor_left;
		keybind_bind_t move_cursor_right;
		keybind_bind_t move_forwards_channel;
		keybind_bind_t move_backwards_channel;
		keybind_bind_t move_forwards_note_column;
		keybind_bind_t move_backwards_note_column;
		keybind_bind_t move_up_n_lines;
		keybind_bind_t move_down_n_lines;
		keybind_bind_t move_pattern_top;
		keybind_bind_t move_pattern_bottom;
		keybind_bind_t move_start;
		keybind_bind_t move_end;
		keybind_bind_t move_previous_position;
		keybind_bind_t move_previous;
		keybind_bind_t move_next;

		keybind_bind_t toggle_multichannel;

		keybind_bind_t store_pattern_data;
		keybind_bind_t revert_pattern_data;
		keybind_bind_t undo;

		keybind_bind_t toggle_centralise_cursor;
		keybind_bind_t toggle_highlight_row;
		keybind_bind_t toggle_volume_display;

		keybind_bind_t set_pattern_length;
		keybind_bind_t toggle_midi_trigger;
	} pattern_edit;

	keybind_section_info_t track_view_info;
	struct keybinds_track_view {
		keybind_bind_t cycle_view;
		keybind_bind_t clear_track_views;
		keybind_bind_t toggle_track_view_divisions;
		// I can't find this in the code and can't get it to work in previous version
		// keybind_bind_t deselect_track;
		keybind_bind_t track_scheme_default;
		keybind_bind_t track_scheme_1;
		keybind_bind_t track_scheme_2;
		keybind_bind_t track_scheme_3;
		keybind_bind_t track_scheme_4;
		keybind_bind_t track_scheme_5;
		keybind_bind_t track_scheme_6;

		keybind_bind_t quick_view_scheme_default;
		keybind_bind_t quick_view_scheme_1;
		keybind_bind_t quick_view_scheme_2;
		keybind_bind_t quick_view_scheme_3;
		keybind_bind_t quick_view_scheme_4;
		keybind_bind_t quick_view_scheme_5;
		keybind_bind_t quick_view_scheme_6;

		// Can't find this and can't figure out in previous version
		// keybind_bind_t toggle_cursor_tracking;
	} track_view;

	keybind_section_info_t block_functions_info;
	struct keybinds_block_functions {
		keybind_bind_t mark_beginning_block;
		keybind_bind_t mark_end_block;
		keybind_bind_t quick_mark_lines;
		keybind_bind_t mark_column_or_pattern;
		keybind_bind_t mark_block_left;
		keybind_bind_t mark_block_right;
		keybind_bind_t mark_block_up;
		keybind_bind_t mark_block_down;
		keybind_bind_t mark_block_start_row;
		keybind_bind_t mark_block_end_row;
		keybind_bind_t mark_block_page_up;
		keybind_bind_t mark_block_page_down;

		keybind_bind_t unmark;

		keybind_bind_t raise_notes_semitone;
		keybind_bind_t raise_notes_octave;
		keybind_bind_t lower_notes_semitone;
		keybind_bind_t lower_notes_octave;
		keybind_bind_t set_instrument;
		keybind_bind_t set_volume_or_panning;
		keybind_bind_t wipe_volume_or_panning;
		keybind_bind_t slide_volume_or_panning;
		keybind_bind_t volume_amplifier;
		keybind_bind_t cut_block;
		keybind_bind_t swap_block;
		keybind_bind_t slide_effect_value;

		keybind_bind_t roll_block_down;
		keybind_bind_t roll_block_up;

		keybind_bind_t copy_block;
		keybind_bind_t copy_block_with_mute;
		keybind_bind_t paste_data;
		keybind_bind_t paste_and_overwrite;
		keybind_bind_t paste_and_mix;

		keybind_bind_t double_block_length;
		keybind_bind_t halve_block_length;

		keybind_bind_t select_template_mode;
		keybind_bind_t disable_template_mode;
		keybind_bind_t toggle_fast_volume;
		keybind_bind_t selection_volume_vary;
		keybind_bind_t selection_panning_vary;
		keybind_bind_t selection_effect_vary;
	} block_functions;

	keybind_section_info_t playback_functions_info;
	struct keybinds_playback_functions {
		keybind_bind_t play_note_cursor;
		keybind_bind_t play_row;

		keybind_bind_t play_from_row;
		keybind_bind_t toggle_playback_mark;

		keybind_bind_t toggle_current_channel;
		keybind_bind_t solo_current_channel;
	} playback_functions;

	/* *** FILE LIST *** */

	keybind_section_info_t file_list_info;
	struct keybinds_file_list {
		keybind_bind_t delete;
	} file_list;

	/* *** GLOBAL *** */

	keybind_section_info_t global_info;
	struct keybinds_global {
		keybind_bind_t help;
		keybind_bind_t midi;
		keybind_bind_t system_configure;
		keybind_bind_t pattern_edit;
		keybind_bind_t sample_list;
		keybind_bind_t sample_library;
		keybind_bind_t instrument_list;
		keybind_bind_t instrument_library;
		keybind_bind_t play_information_or_play_song;
		keybind_bind_t play_song;
		keybind_bind_t preferences;
		keybind_bind_t play_current_pattern;
		keybind_bind_t play_song_from_order;
		keybind_bind_t play_song_from_mark;
		keybind_bind_t toggle_playback;
		keybind_bind_t stop_playback;

		keybind_bind_t toggle_playback_tracing;
		keybind_bind_t toggle_midi_input;

		keybind_bind_t load_module;
		keybind_bind_t message_editor;
		keybind_bind_t save_module;
		keybind_bind_t export_module;
		keybind_bind_t order_list;
		keybind_bind_t order_list_lock;
		keybind_bind_t schism_logging;
		keybind_bind_t song_variables;
		keybind_bind_t palette_config;
		keybind_bind_t font_editor;
		keybind_bind_t waterfall;
		keybind_bind_t time_information;

		keybind_bind_t octave_decrease;
		keybind_bind_t octave_increase;
		keybind_bind_t decrease_playback_speed;
		keybind_bind_t increase_playback_speed;
		keybind_bind_t decrease_playback_tempo;
		keybind_bind_t increase_playback_tempo;
		keybind_bind_t decrease_global_volume;
		keybind_bind_t increase_global_volume;

		keybind_bind_t toggle_channel_1;
		keybind_bind_t toggle_channel_2;
		keybind_bind_t toggle_channel_3;
		keybind_bind_t toggle_channel_4;
		keybind_bind_t toggle_channel_5;
		keybind_bind_t toggle_channel_6;
		keybind_bind_t toggle_channel_7;
		keybind_bind_t toggle_channel_8;

		keybind_bind_t mouse_grab;
		keybind_bind_t display_reset;
		keybind_bind_t go_to_time;
		keybind_bind_t audio_reset;
		keybind_bind_t mouse;
		keybind_bind_t new_song;
		keybind_bind_t calculate_song_length;
		keybind_bind_t quit;
		keybind_bind_t quit_no_confirm;
		keybind_bind_t save;

		keybind_bind_t previous_order;
		keybind_bind_t next_order;

		keybind_bind_t fullscreen;

		keybind_bind_t open_menu;

		keybind_bind_t nav_left;
		keybind_bind_t nav_right;
		keybind_bind_t nav_up;
		keybind_bind_t nav_down;
		keybind_bind_t nav_page_up;
		keybind_bind_t nav_page_down;
		keybind_bind_t nav_accept;
		keybind_bind_t nav_cancel;
		keybind_bind_t nav_home;
		keybind_bind_t nav_end;
		keybind_bind_t nav_tab;
		keybind_bind_t nav_backtab;

		keybind_bind_t numentry_increase_value;
		keybind_bind_t numentry_decrease_value;

		keybind_bind_t thumbbar_min_value;
		keybind_bind_t thumbbar_max_value;
		keybind_bind_t thumbbar_increase_value;
		keybind_bind_t thumbbar_increase_value_2x;
		keybind_bind_t thumbbar_increase_value_4x;
		keybind_bind_t thumbbar_increase_value_8x;
		keybind_bind_t thumbbar_decrease_value;
		keybind_bind_t thumbbar_decrease_value_2x;
		keybind_bind_t thumbbar_decrease_value_4x;
		keybind_bind_t thumbbar_decrease_value_8x;
	} global;

	keybind_section_info_t dialog_info;
	struct keybinds_dialog {
		keybind_bind_t yes;
		keybind_bind_t no;
		keybind_bind_t answer_ok;
		keybind_bind_t answer_cancel;
		keybind_bind_t cancel;
		keybind_bind_t accept;
	} dialog;

	keybind_section_info_t notes_info;
	struct keybinds_notes {
		keybind_bind_t note_row1_c;
		keybind_bind_t note_row1_c_sharp;
		keybind_bind_t note_row1_d;
		keybind_bind_t note_row1_d_sharp;
		keybind_bind_t note_row1_e;
		keybind_bind_t note_row1_f;
		keybind_bind_t note_row1_f_sharp;
		keybind_bind_t note_row1_g;
		keybind_bind_t note_row1_g_sharp;
		keybind_bind_t note_row1_a;
		keybind_bind_t note_row1_a_sharp;
		keybind_bind_t note_row1_b;

		keybind_bind_t note_row2_c;
		keybind_bind_t note_row2_c_sharp;
		keybind_bind_t note_row2_d;
		keybind_bind_t note_row2_d_sharp;
		keybind_bind_t note_row2_e;
		keybind_bind_t note_row2_f;
		keybind_bind_t note_row2_f_sharp;
		keybind_bind_t note_row2_g;
		keybind_bind_t note_row2_g_sharp;
		keybind_bind_t note_row2_a;
		keybind_bind_t note_row2_a_sharp;
		keybind_bind_t note_row2_b;

		keybind_bind_t note_row3_c;
		keybind_bind_t note_row3_c_sharp;
		keybind_bind_t note_row3_d;
		keybind_bind_t note_row3_d_sharp;
		keybind_bind_t note_row3_e;
	} notes;
} keybind_list_t;

/* keybinds_codes.c */
int keybinds_parse_keycode(const char *name, SDL_Keycode *ret);
int keybinds_parse_modkey(const char *name, enum keybind_modifier *ret);
int keybinds_parse_scancode(const char *name, SDL_Scancode *ret);

/* keybinds_init.c */
void keybinds_init_shortcut(keybind_shortcut_t *shortcut);
int keybinds_init_bind(keybind_bind_t* bind, keybind_section_info_t* section_info, const char* name, const char* description, const char* shortcut);
void keybinds_init_section(keybind_section_info_t* section_info, const char* name, const char* title, enum page_numbers page);
int keybinds_init(void);

/* keybinds.c */
int keybinds_append_bind_to_list(keybind_bind_t *bind);
int keybinds_append_shortcut_to_bind(keybind_bind_t* bind, SDL_Keycode keycode, SDL_Scancode scancode, enum keybind_modifier modifier);
char* keybinds_get_help_text(enum page_numbers page);
void keybinds_handle_event(struct key_event* event);
int keybinds_init_failed(void);
extern keybind_list_t global_keybinds_list;

/* ------------------------------------------------------------------------------ */
/* generate menus */

enum keybinds_menu_item_type {
	KEYBINDS_MENU_ITEM_NULL,
	KEYBINDS_MENU_ITEM_REGULAR,
	KEYBINDS_MENU_ITEM_SEPARATOR,
};

struct keybinds_menu_item {
	enum keybinds_menu_item_type type;
	int no_osx; // ugh
	union {
		struct {
			const char *name;
			const char *script;  // what gets passed to the event
			keybind_bind_t *bind;
		} regular;
	} info;
};

enum keybinds_menu_type {
	KEYBINDS_MENU_NULL,
	KEYBINDS_MENU_REGULAR,
	KEYBINDS_MENU_APPLE, // this menu gets displayed under the "Schism Tracker" menu on OS X
};

struct keybinds_menu {
	enum keybinds_menu_type type;
	union {
		struct {
			const char *name;
		} regular;
	} info;
	struct keybinds_menu_item *items;
};

extern struct keybinds_menu keybinds_menus[];

struct keybinds_menu_item *keybinds_menu_find_item_from_id(uint32_t id);
void keybinds_menu_item_pressed(struct keybinds_menu_item *i, SDL_Event *event);

/* ------------------------------------------------------------------------------ */

/* Key was pressed this event. Will not trigger on held down repeats. */
#define KEY_PRESSED(SECTION, NAME) global_keybinds_list.SECTION.NAME.pressed

/* Key was released this event. */
#define KEY_RELEASED(SECTION, NAME) global_keybinds_list.SECTION.NAME.released

/* Key was repeated this event. Will not trigger when key is first pressed. */
#define KEY_REPEATED(SECTION, NAME) global_keybinds_list.SECTION.NAME.repeated

/* Key was pressed or repeated this event. Will not trigger on key released. */
#define KEY_PRESSED_OR_REPEATED(SECTION, NAME) ( \
	global_keybinds_list.SECTION.NAME.pressed || \
	global_keybinds_list.SECTION.NAME.repeated)

/* Key was pressed, repeated, or released this event */
#define KEY_ACTIVE(SECTION, NAME) ( \
	global_keybinds_list.SECTION.NAME.pressed || \
	global_keybinds_list.SECTION.NAME.repeated || \
	global_keybinds_list.SECTION.NAME.released \
)

/*
	How many times the key was pressed and released in a row.
	Pressing any other key will reset to 0.
	Does not count repeats while holding down the key.
*/
#define KEY_PRESS_REPEATS(SECTION, NAME) ( \
	global_keybinds_list.SECTION.NAME.press_repeats \
)

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

int info_load_keybinds(cfg_file_t* cfg);
int instrument_list_load_keybinds(cfg_file_t* cfg);
int load_module_load_keybinds(cfg_file_t* cfg);
int load_sample_load_keybinds(cfg_file_t* cfg);
int message_load_keybinds(cfg_file_t* cfg);
int midi_load_keybinds(cfg_file_t* cfg);
int ordervol_load_keybinds(cfg_file_t* cfg);
int palette_load_keybinds(cfg_file_t* cfg);
int pattern_editor_load_keybinds(cfg_file_t* cfg);
int sample_list_load_keybinds(cfg_file_t* cfg);
int timeinfo_load_keybinds(cfg_file_t* cfg);
int waterfall_load_keybinds(cfg_file_t* cfg);

#endif /* SCHISM_KEYBINDS_H_ */
