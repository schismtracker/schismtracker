
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
    } global;
} keybind_list;

char* keybinds_get_help_text(enum page_numbers page);
void keybinds_handle_event(struct key_event* event);
void init_keybinds(void);
extern keybind_list global_keybinds_list;

#define key_pressed(SECTION, NAME) global_keybinds_list.SECTION.NAME.pressed

#endif /* KEYBINDS_H */
