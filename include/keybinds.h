
#ifndef KEYBINDS_H
#define KEYBINDS_H

#include "SDL.h"
#include "page.h"

typedef struct keybind_shortcut
{
    SDL_Keycode keycode;
    SDL_Scancode scancode;
    const char* character;
    SDL_Keymod modifier;
} keybind_shortcut;

typedef struct keybind_section_info
{
    const char* name;
    const char* title;
    int is_active;
    enum page_numbers page;
    int (*page_matcher)(enum page_numbers);
} keybind_section_info;

typedef struct keybind_bind
{
    keybind_section_info* section_info;
    keybind_shortcut* shortcuts; // This array size is MAX_SHORTCUTS
    int shortcuts_count; // This number is used to skip checking the entire array
    const char* name;
    const char* description;
    const char* shortcut_text; // Contains all shortcuts with commas in-between, for example "F10, Ctrl-W"
    const char* first_shortcut_text; // Contains only the first shortcut, for example "F10"
    const char* shortcut_text_parens; // shortcut_text but with parenthesis around it. No parenthesis if there is no shortcut
    const char* first_shortcut_text_parens;
    int pressed;
    int released;
    int repeated;
} keybind_bind;

typedef struct keybind_list
{
    /* *** PALETTE EDIT *** */

    keybind_section_info palette_edit_info;
    struct keybinds_palette_edit {
        keybind_bind copy;
        keybind_bind paste;
    } palette_edit;

    /* *** ORDER LIST *** */

    keybind_section_info order_list_info;
    struct keybinds_order_list {
        keybind_bind goto_selected_pattern;
        keybind_bind select_order_for_playback;
        keybind_bind play_from_order;
        keybind_bind insert_next_pattern;
        keybind_bind duplicate_pattern;
        keybind_bind mark_end_of_song;
        keybind_bind skip_to_next_order_mark;
        keybind_bind insert_pattern;
        keybind_bind delete_pattern;
        keybind_bind play_this_order_next;

        // Is duplicate
        // keybind_bind toggle_order_list_locked;
        keybind_bind sort_order_list;
        keybind_bind find_unused_patterns;

        keybind_bind link_pattern_to_sample;
        keybind_bind copy_pattern_to_sample;
        keybind_bind copy_pattern_to_sample_with_split;

        keybind_bind continue_next_position_of_pattern;

        keybind_bind save_order_list;
        keybind_bind restore_order_list;
    } order_list;

    keybind_section_info order_list_panning_info;
    struct keybinds_order_list_panning {
        keybind_bind set_panning_left;
        keybind_bind set_panning_middle;
        keybind_bind set_panning_right;
        keybind_bind set_panning_surround;
        keybind_bind pan_unmuted_left;
        keybind_bind pan_unmuted_middle;
        keybind_bind pan_unmuted_right;
        keybind_bind pan_unmuted_stereo;
        keybind_bind pan_unmuted_amiga_stereo;
        keybind_bind linear_panning_left_to_right;
        keybind_bind linear_panning_right_to_left;
    } order_list_panning;

    /* *** INFO PAGE *** */

    keybind_section_info info_page_info;

    struct keybinds_info_page {
        keybind_bind add_window;
        keybind_bind delete_window;
        keybind_bind nav_next_window;
        keybind_bind nav_previous_window;
        keybind_bind change_window_type_up;
        keybind_bind change_window_type_down;
        keybind_bind move_window_base_up;
        keybind_bind move_window_base_down;

        keybind_bind toggle_volume_velocity_bars;
        keybind_bind toggle_sample_instrument_names;

        keybind_bind toggle_channel_mute;
        keybind_bind toggle_channel_mute_and_go_next;
        keybind_bind solo_channel;

        keybind_bind goto_next_pattern;
        keybind_bind goto_previous_pattern;

        keybind_bind toggle_stereo_playback;
        keybind_bind reverse_output_channels;

        keybind_bind goto_playing_pattern;
    } info_page;

    /* *** INSTRUMENT LIST *** */

    keybind_section_info instrument_list_info;
    struct keybinds_instrument_list {
        keybind_bind load_instrument;
        keybind_bind focus_list;
        keybind_bind nav_first;
        keybind_bind nav_last;
        keybind_bind move_instrument_up;
        keybind_bind move_instrument_down;
        keybind_bind clear_name_and_filename;
        keybind_bind wipe_data;
        keybind_bind edit_name;

        keybind_bind delete_instrument_and_samples;
        keybind_bind delete_instrument_and_unused_samples;
        keybind_bind post_loop_cut;
        keybind_bind toggle_multichannel;
        keybind_bind save_to_disk;
        keybind_bind copy;
        keybind_bind replace_in_song;
        keybind_bind swap;
        keybind_bind update_pattern_data;
        keybind_bind exchange;

        keybind_bind insert_slot;
        keybind_bind remove_slot;

        keybind_bind increase_playback_channel;
        keybind_bind decrease_playback_channel;
    } instrument_list;

    keybind_section_info instrument_note_translation_info;
    struct keybinds_instrument_note_translation {
        keybind_bind pickup_sample_number_and_default_play_note;
        keybind_bind increase_sample_number;
        keybind_bind decrease_sample_number;

        keybind_bind change_all_samples;
        keybind_bind change_all_samples_with_name;
        keybind_bind enter_next_note;
        keybind_bind enter_previous_note;
        keybind_bind transpose_all_notes_semitone_up;
        keybind_bind transpose_all_notes_semitone_down;
        keybind_bind insert_row_from_table;
        keybind_bind delete_row_from_table;
        keybind_bind toggle_edit_mask;
    } instrument_note_translation;

    keybind_section_info instrument_envelope_info;
    struct keybinds_instrument_envelope {
        keybind_bind pick_up_or_drop_current_node;
        keybind_bind add_node;
        keybind_bind delete_node;
        keybind_bind nav_node_left;
        keybind_bind nav_node_right;

        keybind_bind move_node_left;
        keybind_bind move_node_right;
        keybind_bind move_node_left_fast;
        keybind_bind move_node_right_fast;
        keybind_bind move_node_left_max;
        keybind_bind move_node_right_max;
        keybind_bind move_node_up;
        keybind_bind move_node_down;
        keybind_bind move_node_up_fast;
        keybind_bind move_node_down_fast;

        keybind_bind pre_loop_cut_envelope;
        keybind_bind double_envelope_length;
        keybind_bind halve_envelope_length;
        keybind_bind resize_envelope;
        keybind_bind generate_envelope_from_ADSR_values;

        keybind_bind play_default_note;
        // keybind_bind note_off;
    } instrument_envelope;

    /* *** SAMPLE LIST *** */

    keybind_section_info sample_list_info;
    struct keybinds_sample_list {
        keybind_bind load_new_sample;
        keybind_bind move_between_options;
        keybind_bind move_up;
        keybind_bind move_down;
        keybind_bind focus_sample_list;

        keybind_bind convert_signed_unsigned;
        keybind_bind pre_loop_cut;
        keybind_bind clear_name_and_filename;
        keybind_bind delete_sample;
        keybind_bind downmix_to_mono;
        keybind_bind resize_sample_with_interpolation;
        keybind_bind resize_sample_without_interpolation;
        keybind_bind reverse_sample;
        keybind_bind centralise_sample;
        keybind_bind invert_sample;
        keybind_bind post_loop_cut;
        keybind_bind sample_amplifier;
        keybind_bind toggle_multichannel_playback;
        keybind_bind save_sample_to_disk_it;
        keybind_bind copy_sample;
        keybind_bind toggle_sample_quality;
        keybind_bind replace_current_sample;
        keybind_bind swap_sample;
        keybind_bind save_sample_to_disk_format_select;
        keybind_bind save_sample_to_disk_raw;
        keybind_bind exchange_sample;
        keybind_bind text_to_sample;
        keybind_bind edit_create_adlib_sample;
        keybind_bind load_adlib_sample_by_midi_patch_number;

        keybind_bind insert_sample_slot;
        keybind_bind remove_sample_slot;
        keybind_bind swap_sample_with_previous;
        keybind_bind swap_sample_with_next;

        keybind_bind toggle_current_sample;
        keybind_bind solo_current_sample;

        keybind_bind decrease_playback_channel;
        keybind_bind increase_playback_channel;

        keybind_bind increase_c5_frequency_1_octave;
        keybind_bind decrease_c5_frequency_1_octave;

        keybind_bind increase_c5_frequency_1_semitone;
        keybind_bind decrease_c5_frequency_1_semitone;
    } sample_list;

    /* *** PATTERN EDIT *** */

    keybind_section_info pattern_edit_info;
    struct keybinds_pattern_edit {
        keybind_bind next_pattern;
        keybind_bind previous_pattern;
        keybind_bind next_4_pattern;
        keybind_bind previous_4_pattern;
        keybind_bind next_order_pattern;
        keybind_bind previous_order_pattern;
        keybind_bind use_last_value;
        // keybind_bind preview_note;

        keybind_bind get_default_value;
        keybind_bind decrease_instrument;
        keybind_bind increase_instrument;
        keybind_bind decrease_octave;
        keybind_bind increase_octave;
        keybind_bind toggle_edit_mask;

        keybind_bind insert_row;
        keybind_bind delete_row;
        keybind_bind insert_pattern_row;
        keybind_bind delete_pattern_row;

        keybind_bind up_by_skip;
        keybind_bind down_by_skip;
        keybind_bind set_skip_1;
        keybind_bind set_skip_2;
        keybind_bind set_skip_3;
        keybind_bind set_skip_4;
        keybind_bind set_skip_5;
        keybind_bind set_skip_6;
        keybind_bind set_skip_7;
        keybind_bind set_skip_8;
        keybind_bind set_skip_9;

        keybind_bind up_one_row;
        keybind_bind down_one_row;
        keybind_bind slide_pattern_up;
        keybind_bind slide_pattern_down;
        keybind_bind move_cursor_left;
        keybind_bind move_cursor_right;
        keybind_bind move_forwards_channel;
        keybind_bind move_backwards_channel;
        keybind_bind move_forwards_note_column;
        keybind_bind move_backwards_note_column;
        keybind_bind move_up_n_lines;
        keybind_bind move_down_n_lines;
        keybind_bind move_pattern_top;
        keybind_bind move_pattern_bottom;
        keybind_bind move_start;
        keybind_bind move_end;
        keybind_bind move_previous_position;
        keybind_bind move_previous;
        keybind_bind move_next;

        keybind_bind toggle_multichannel;

        keybind_bind store_pattern_data;
        keybind_bind revert_pattern_data;
        keybind_bind undo;

        keybind_bind toggle_centralise_cursor;
        keybind_bind toggle_highlight_row;
        keybind_bind toggle_volume_display;

        keybind_bind set_pattern_length;
    } pattern_edit;

    keybind_section_info track_view_info;
    struct keybinds_track_view {
        keybind_bind cycle_view;
        keybind_bind clear_track_views;
        keybind_bind toggle_track_view_divisions;
        keybind_bind deselect_track;
        keybind_bind track_scheme_1;
        keybind_bind track_scheme_2;
        keybind_bind track_scheme_3;
        keybind_bind track_scheme_4;
        keybind_bind track_scheme_5;
        keybind_bind track_scheme_6;
        keybind_bind move_column_left;
        keybind_bind move_column_right;

        keybind_bind quick_view_scheme_1;
        keybind_bind quick_view_scheme_2;
        keybind_bind quick_view_scheme_3;
        keybind_bind quick_view_scheme_4;
        keybind_bind quick_view_scheme_5;
        keybind_bind quick_view_scheme_6;

        keybind_bind toggle_cursor_tracking;
    } track_view;

    keybind_section_info block_functions_info;
    struct keybinds_block_functions {
        keybind_bind mark_beginning_block;
        keybind_bind mark_end_block;
        keybind_bind quick_mark_lines;
        keybind_bind mark_column_or_pattern;
        keybind_bind mark_block_left;
        keybind_bind mark_block_right;
        keybind_bind mark_block_up;
        keybind_bind mark_block_down;
        keybind_bind mark_block_start_row;
        keybind_bind mark_block_end_row;
        keybind_bind mark_block_page_up;
        keybind_bind mark_block_page_down;

        keybind_bind unmark;

        keybind_bind raise_notes_semitone;
        keybind_bind raise_notes_octave;
        keybind_bind lower_notes_semitone;
        keybind_bind lower_notes_octave;
        keybind_bind set_instrument;
        keybind_bind set_volume_or_panning;
        keybind_bind wipe_volume_or_panning;
        keybind_bind slide_volume_or_panning;
        // keybind_bind wipe_all_volume_or_panning;
        keybind_bind volume_amplifier;
        keybind_bind cut_block;
        keybind_bind swap_block;
        keybind_bind slide_effect_value;
        // keybind_bind wipe_all_effect_data;

        keybind_bind roll_block_down;
        keybind_bind roll_block_up;

        keybind_bind copy_block;
        keybind_bind copy_block_with_mute;
        keybind_bind paste_data;
        keybind_bind paste_and_overwrite;
        // keybind_bind grow_pattern_from_clipboard_length;
        keybind_bind paste_and_mix;
        // keybind_bind paste_and_mix_field;

        keybind_bind double_block_length;
        keybind_bind halve_block_length;

        keybind_bind select_template_mode;
        keybind_bind disable_template_mode;
        keybind_bind toggle_fast_volume;
        keybind_bind selection_volume_vary;
        keybind_bind selection_panning_vary;
        keybind_bind selection_effect_vary;
    } block_functions;

    keybind_section_info playback_functions_info;
    struct keybinds_playback_functions {
        keybind_bind play_note_cursor;
        keybind_bind play_row;

        keybind_bind play_from_row;
        keybind_bind toggle_playback_mark;

        keybind_bind toggle_current_channel;
        keybind_bind solo_current_channel;

        keybind_bind toggle_playback_tracing;
        keybind_bind toggle_midi_input;
    } playback_functions;

    /* *** GLOBAL *** */

    keybind_section_info global_info;
    struct keybinds_global {
        keybind_bind help;
        keybind_bind midi;
        keybind_bind system_configure;
        keybind_bind pattern_edit;
        keybind_bind pattern_edit_length;
        keybind_bind sample_list;
        keybind_bind sample_library;
        keybind_bind instrument_list;
        keybind_bind instrument_library;
        keybind_bind play_information_or_play_song;
        keybind_bind play_song;
        keybind_bind preferences;
        keybind_bind play_current_pattern;
        keybind_bind play_song_from_order;
        keybind_bind play_song_from_mark;
        keybind_bind toggle_playback;
        keybind_bind stop_playback;
        keybind_bind load_module;
        keybind_bind message_editor;
        keybind_bind save_module;
        keybind_bind export_module;
        keybind_bind order_list;
        keybind_bind order_list_lock;
        keybind_bind schism_logging;
        keybind_bind song_variables;
        keybind_bind palette_config;
        keybind_bind font_editor;
        keybind_bind waterfall;

        keybind_bind octave_decrease;
        keybind_bind octave_increase;
        keybind_bind decrease_playback_speed;
        keybind_bind increase_playback_speed;
        keybind_bind decrease_playback_tempo;
        keybind_bind increase_playback_tempo;
        keybind_bind decrease_global_volume;
        keybind_bind increase_global_volume;

        keybind_bind toggle_channel_1;
        keybind_bind toggle_channel_2;
        keybind_bind toggle_channel_3;
        keybind_bind toggle_channel_4;
        keybind_bind toggle_channel_5;
        keybind_bind toggle_channel_6;
        keybind_bind toggle_channel_7;
        keybind_bind toggle_channel_8;

        keybind_bind mouse_grab;
        keybind_bind display_reset;
        keybind_bind go_to_time;
        keybind_bind audio_reset;
        keybind_bind mouse;
        keybind_bind new_song;
        keybind_bind calculate_song_length;
        keybind_bind quit;
        keybind_bind quit_no_confirm;
        keybind_bind save;

        keybind_bind fullscreen;

        keybind_bind nav_left;
        keybind_bind nav_right;
        keybind_bind nav_up;
        keybind_bind nav_down;
        keybind_bind nav_page_up;
        keybind_bind nav_page_down;
        keybind_bind nav_accept;
        keybind_bind nav_cancel;
        keybind_bind nav_home;
        keybind_bind nav_end;
        keybind_bind nav_tab;
        keybind_bind nav_backtab;

        keybind_bind thumbbar_increase_value;
        keybind_bind thumbbar_decrease_value;

    } global;

    keybind_section_info dialog_info;
    struct keybinds_dialog {
        keybind_bind yes;
        keybind_bind no;
        keybind_bind answer_ok;
        keybind_bind answer_cancel;
        keybind_bind cancel;
        keybind_bind accept;
    } dialog;
} keybind_list;

char* keybinds_get_help_text(enum page_numbers page);
void keybinds_handle_event(struct key_event* event);
void init_keybinds(void);
extern keybind_list global_keybinds_list;

#define key_pressed(SECTION, NAME) global_keybinds_list.SECTION.NAME.pressed
#define key_released(SECTION, NAME) global_keybinds_list.SECTION.NAME.released
#define key_repeated(SECTION, NAME) global_keybinds_list.SECTION.NAME.repeated
#define key_pressed_or_repeated(SECTION, NAME) \
    global_keybinds_list.SECTION.NAME.pressed || \
    global_keybinds_list.SECTION.NAME.repeated

#endif /* KEYBINDS_H */
