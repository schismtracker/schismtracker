
#include "it.h"
#include "page.h"
#include "dmoz.h"
#include "keybinds_codes.c"

#define MAX_BINDS 200
#define MAX_SHORTCUTS 3
static int current_binds_count = 0;
static keybind_bind* current_binds[MAX_BINDS];
keybind_list global_keybinds_list;
static int has_init_problem = 0;

static int check_mods(SDL_Keymod needed, SDL_Keymod current, int lenient)
{
    int ralt_needed = needed & KMOD_RALT;
    int ctrl_needed = (needed & KMOD_LCTRL) || (needed & KMOD_RCTRL);
    int shift_needed = (needed & KMOD_LSHIFT) || (needed & KMOD_RSHIFT);
    int alt_needed = needed & KMOD_LALT;

    int has_ralt = current & KMOD_RALT;
    int has_ctrl = !has_ralt && ((current & KMOD_LCTRL) || (current & KMOD_RCTRL));
    int has_shift = (current & KMOD_LSHIFT) || (current & KMOD_RSHIFT);
    int has_alt = current & KMOD_LALT;

    if(lenient) {
        if (ctrl_needed && !has_ctrl) return 0;
        if (shift_needed && !has_shift) return 0;
        if (alt_needed && !has_alt) return 0;
        if (ralt_needed && !has_ralt) return 0;

        return 1;
    } else
        return (ctrl_needed == has_ctrl) && (shift_needed == has_shift) && (alt_needed == has_alt) && (ralt_needed == has_ralt);
}

static void update_bind(keybind_bind* bind, SDL_KeyCode kcode, SDL_Scancode scode, SDL_Keymod mods, const char* text, int is_down, int is_repeat)
{
    keybind_shortcut* sc;

    for(int i = 0; i < bind->shortcuts_count; i++) {
        sc = &bind->shortcuts[i];
        int was_pressed = bind->pressed;
        int mods_correct = 0;
        bind->pressed = 0;
        bind->released = 0;
        bind->repeated = 0;

        if(sc->character[0] && text) {
            if (strcmp(text, sc->character) != 0) {
                continue;
            }
            mods_correct = check_mods(sc->modifier, mods, 1);
        } else if(sc->keycode != SDLK_UNKNOWN) {
            if(sc->keycode != kcode)
                continue;
            mods_correct = 1;
        } else if(sc->scancode != SDL_SCANCODE_UNKNOWN) {
            if(sc->scancode != scode)
                continue;
            mods_correct = check_mods(sc->modifier, mods, 0);
        }

        if(is_down) {
            if(mods_correct) {
                bind->pressed = !is_repeat;
                bind->repeated = is_repeat;
            }
        } else {
            bind->released = was_pressed;
        }

        return;
    }
}

void keybinds_add_bind_shortcut(keybind_bind* bind, SDL_Keycode keycode, SDL_Scancode scancode, const char* character, SDL_Keymod modifier)
{
    if(bind->shortcuts_count == MAX_SHORTCUTS) {
        log_appendf(5, " %s/%s: Trying to bind too many shortcuts. Max is %i.", bind->section_info->name, bind->name, MAX_SHORTCUTS);
        has_init_problem = 1;
        return;
    }

    if(keycode == SDLK_UNKNOWN && scancode == SDL_SCANCODE_UNKNOWN && (!character || !character[0])) {
        printf("Attempting to bind shortcut with no key. Skipping.\n");
        return;
    }

    int i = bind->shortcuts_count;
    bind->shortcuts[i].keycode = keycode;
    bind->shortcuts[i].scancode = scancode;
    bind->shortcuts[i].modifier = modifier;
    bind->shortcuts[i].character = character;
    bind->shortcuts_count++;
}

static int string_to_mod_length = ARRAY_SIZE(string_to_mod);

static int parse_shortcut_mods(const char* shortcut, SDL_Keymod* mods)
{
    char* shortcut_dup = strdup(shortcut);
    str_to_upper(shortcut_dup);

    for (int i = 0; i < string_to_mod_length; i++) {
        if (strcmp(shortcut_dup, string_to_mod[i].name) == 0) {
            *mods |= string_to_mod[i].mod;
            return 1;
        }
    }

    return 0;
}

static int string_to_scancode_length = ARRAY_SIZE(string_to_scancode);

static int parse_shortcut_scancode(keybind_bind* bind, const char* shortcut, SDL_Scancode* code)
{
    char* shortcut_dup = strdup(shortcut);
    str_to_upper(shortcut_dup);

    for (int i = 0; i < string_to_scancode_length; i++) {
        if (strcmp(shortcut_dup, string_to_scancode[i].name) == 0) {
            *code = string_to_scancode[i].code;
            return 1;
        }
    }

    if(shortcut[0] == 'U' && shortcut[1] == 'S' && shortcut[2] == '_') {
        log_appendf(5, " %s/%s: Unknown code '%s'", bind->section_info->name, bind->name, shortcut);
        has_init_problem = 1;
        return -1;
    }

    return 0;
}

static int parse_shortcut_character(keybind_bind* bind, const char* shortcut, char** character)
{
    if(!shortcut || !shortcut[0]) return 0;

    if(utf8_length(shortcut) > 1) {
        log_appendf(5, " %s/%s: Too many characters in '%s'", bind->section_info->name, bind->name, shortcut);
        has_init_problem = 1;
        return -1;
    }

    *character = strdup(shortcut);
    return 1;
}

static void keybinds_parse_shortcut_splitted(keybind_bind* bind, const char* shortcut)
{
    char* shortcut_dup = strdup(shortcut);
    char *strtok_ptr;
    const char* delim = "+";
    char* trimmed = NULL;
    int has_problem = 0;

    SDL_Keymod mods = KMOD_NONE;
    SDL_Scancode scan_code = SDL_SCANCODE_UNKNOWN;
    SDL_Keycode key_code = SDLK_UNKNOWN;
    char* character = "\0";

    for(int i = 0;; i++) {
        const char* next = strtok_r(i == 0 ? shortcut_dup : NULL, delim, &strtok_ptr);

        if (trimmed) free(trimmed);
        trimmed = strdup(next);
        trim_string(trimmed);

        if (next == NULL) break;

        if (parse_shortcut_mods(trimmed, &mods)) continue;

        int scan_result = parse_shortcut_scancode(bind, trimmed, &scan_code);
        if (scan_result == 1) break;
        if (scan_result == -1) { has_problem = 1; break; }

        int char_result = parse_shortcut_character(bind, trimmed, &character);
        if (char_result == 1) break;
        if (char_result == -1) { has_problem = 1; break; }
    }

    if (trimmed) free(trimmed);
    free(shortcut_dup);
    if(has_problem) return;
    keybinds_add_bind_shortcut(bind, key_code, scan_code, character, mods);
}

void keybinds_parse_shortcut(keybind_bind* bind, const char* shortcut)
{
    char* shortcut_dup = strdup(shortcut);
    char *strtok_ptr;
    const char* delim = ",";

    for(int i = 0; i < 10; i++) {
        const char* next = strtok_r(i == 0 ? shortcut_dup : NULL, delim, &strtok_ptr);
        if (next == NULL) break;
        keybinds_parse_shortcut_splitted(bind, next);
    }

    free(shortcut_dup);
}

static void init_section(keybind_section_info* section_info, const char* name, const char* title, enum page_numbers page)
{
    section_info->is_active = 0;
    section_info->name = name;
    section_info->title = title;
    section_info->page = page;
}

static void set_shortcut_text(keybind_bind* bind)
{
    char* out[MAX_SHORTCUTS];
    int is_first_shortcut = 1;

    for(int i = 0; i < MAX_SHORTCUTS; i++) {
        out[i] = NULL;
        if(i >= bind->shortcuts_count) continue;

        keybind_shortcut* sc = &bind->shortcuts[i];

        const char* ctrl_text = (sc->modifier & KMOD_CTRL) ? "Ctrl-" : "";
        const char* alt_text = (sc->modifier & KMOD_LALT) ? "Alt-" : "";
        const char* shift_text = (sc->modifier & KMOD_SHIFT) ? "Shift-" : "";

        char* key_text = NULL;

        if(sc->character[0]) {
            key_text = strdup(sc->character);
        } else if(sc->scancode != SDL_SCANCODE_UNKNOWN) {
            if (sc->scancode == SDL_SCANCODE_RETURN) {
                key_text = strdup("Enter");
            } else {
                SDL_Keycode code = SDL_GetKeyFromScancode(sc->scancode);
                key_text = strdup(SDL_GetKeyName(code));
            }
        } else {
            continue;
        }

        int key_text_length = strlen(key_text);

        if (key_text_length == 0)
            continue;

        int length = strlen(ctrl_text) + strlen(alt_text) + strlen(shift_text) + key_text_length;
        char* strings[4] = { (char*)ctrl_text, (char*)alt_text, (char*)shift_text, (char*)key_text };
        // char* next_out = str_concat(ctrl_text, alt_text, shift_text, key_text, NULL);
        char* next_out = str_concat_array(4, strings, 0);

        out[i] = next_out;

        if(is_first_shortcut) {
            is_first_shortcut = 0;
            bind->first_shortcut_text = strdup(next_out);
            bind->first_shortcut_text_parens = str_concat_three(" (", next_out, ")", 0);
        }

        free(key_text);
    }

    char* shortcut_text = str_concat_with_delim(MAX_SHORTCUTS, out, ", ", 1);
    bind->shortcut_text = shortcut_text;
    if(shortcut_text[0])
        bind->shortcut_text_parens = str_concat_three(" (", shortcut_text, ")", 0);
}

static void init_bind(keybind_bind* bind, keybind_section_info* section_info, const char* name, const char* description, const char* shortcut) //, SDL_Keycode keycode, SDL_Scancode scancode, SDL_Keymod modifier)
{
    if(current_binds_count >= MAX_BINDS) {
        log_appendf(5, " Keybinds exceeding max bind count. Max is %i.", MAX_BINDS);
        has_init_problem = 1;
        return;
    }

    current_binds[current_binds_count] = bind;
    current_binds_count++;

    bind->pressed = 0;
    bind->released = 0;
    bind->shortcuts = malloc(sizeof(keybind_shortcut) * 3);

    for(int i = 0; i < 3; i++) {
        bind->shortcuts[i].keycode = SDLK_UNKNOWN;
        bind->shortcuts[i].scancode = SDL_SCANCODE_UNKNOWN;
        bind->shortcuts[i].modifier = KMOD_NONE;
    }

    bind->shortcuts_count = 0;
    bind->section_info = section_info;
    bind->name = name;
    bind->description = description;
    bind->shortcut_text = "";
    bind->first_shortcut_text = "";
    bind->shortcut_text_parens = "";
    bind->first_shortcut_text_parens = "";

    keybinds_parse_shortcut(bind, shortcut);
    set_shortcut_text(bind);
}

void keybinds_handle_event(struct key_event* event)
{
	if (event->mouse != MOUSE_NONE) {
		return;
	}

    int is_down = event->state == KEY_PRESS;
    int is_repeat = event->is_repeat;
    SDL_Keymod mods = SDL_GetModState();

    fflush(stdout);

    for (int i = 0; i < current_binds_count; i++) {
        update_bind(current_binds[i], event->orig_sym, event->scancode, mods, event->orig_text, is_down, is_repeat);
    }
}

char* keybinds_get_help_text(enum page_numbers page)
{
    const char* current_title = NULL;
    char* out = strdup("");

    for (int i = 0; i < MAX_BINDS; i++) {
        // lines[i] = NULL;

        if (i >= current_binds_count)
            continue;

        keybind_bind* bind = current_binds[i];
        const char* bind_title = bind->section_info->title;
        enum page_numbers bind_page = bind->section_info->page;

        if (bind_page != PAGE_ANY && bind_page != page)
            continue;

        char* strings[2] = { (char*)bind->description, (char*)bind->shortcut_text };

        if (current_title != bind_title) {
            current_title = bind_title;
            char* prev_out = out;
            out = str_concat_four(out, "\n  ", (char*)bind_title, "\n", 0);
            free(prev_out);
        }

        char* shortcut = str_pad_between((char*)bind->shortcut_text, "", ' ', 18, 1, 0);
        char* prev_out = out;
        out = str_concat_five(out, "    ", shortcut, (char*)bind->description, "\n", 0);
        free(shortcut);
        free(prev_out);
    }

    return out;
}

#define init_section_macro(SECTION, TITLE, PAGE) \
    init_section(&global_keybinds_list.SECTION##_info, #SECTION, TITLE, PAGE); \
    current_section_name = #SECTION; \
    current_section_info = &global_keybinds_list.SECTION##_info;

#define init_bind_macro(SECTION, BIND, DESCRIPTION, DEFAULT_KEYBIND) \
	cfg_get_string(cfg, current_section_name, #BIND, current_shortcut, 255, DEFAULT_KEYBIND); \
	cfg_set_string(cfg, current_section_name, #BIND, current_shortcut); \
    init_bind(&global_keybinds_list.SECTION.BIND, current_section_info, #BIND, DESCRIPTION, current_shortcut);

keybind_section_info* current_section_info = NULL;
static const char* current_section_name = "";
static char current_shortcut[256] = "";

static void init_global_keybinds(cfg_file_t* cfg)
{
    init_section_macro(global, "Global Keys.", PAGE_ANY);
    init_bind_macro(global, help, "Help (Context Sensitive!)", "US_F1 ,US_DDD,sdf,b,c,d");
    init_bind_macro(global, midi, "MIDI Screen", "Shift+US_F1");
    init_bind_macro(global, system_configure, "System Configuration", "Ctrl+US_F1");
    init_bind_macro(global, pattern_edit, "Pattern Editor / Pattern Editor Options", "US_F2");
    init_bind_macro(global, pattern_edit_length, "Edit Pattern Length", "Ctrl+US_F2");
    init_bind_macro(global, sample_list, "Sample List", "US_F3");
    init_bind_macro(global, sample_library, "Sample Library", "Ctrl+US_F3");
    init_bind_macro(global, instrument_list, "Instrument List", "US_F4");
    init_bind_macro(global, instrument_library, "Instrument Library", "Ctrl+US_F4");
    init_bind_macro(global, play_information_or_play_song, "Play Information / Play Song", "US_F5");
    init_bind_macro(global, play_song, "Play Song", "Ctrl+US_F5");
    init_bind_macro(global, preferences, "Preferences", "Shift+US_F5");
    init_bind_macro(global, play_current_pattern, "Play Current Pattern", "US_F6");
    init_bind_macro(global, play_song_from_order, "Play Song From Current Order", "Shift+US_F6");
    init_bind_macro(global, play_song_from_mark, "Play From Mark / Current Row", "US_F7");
    init_bind_macro(global, stop_playback, "Stop Playback", "US_F8");
    init_bind_macro(global, toggle_playback, "Pause / Resume Playback", "Shift+US_F8");
    init_bind_macro(global, load_module, "Load Module", "US_F9,Ctrl+US_L");
    init_bind_macro(global, message_editor, "Message Editor", "Shift+US_F9");
    init_bind_macro(global, save_module, "Save Module", "US_F10,Ctrl+W");
    init_bind_macro(global, export_module, "Export Module (to WAV, AIFF)", "Shift+US_F10");
    init_bind_macro(global, order_list, "Order List and Panning / Channel Volume", "US_F11");
    init_bind_macro(global, schism_logging, "Schism Logging", "Ctrl+US_F11");
    init_bind_macro(global, order_list_lock, "Schism Logging", "Alt+US_F11");
    init_bind_macro(global, song_variables, "Song Variables & Directory Configuration", "US_F12");
    init_bind_macro(global, palette_config, "Palette Configuration", "Ctrl+US_F12");
    init_bind_macro(global, font_editor, "Font Editor", "Shift+US_F12");
    init_bind_macro(global, waterfall, "Waterfall", "Alt+US_F12");

    init_bind_macro(global, octave_decrease, "Decrease Octave", "US_HOME");
    init_bind_macro(global, octave_increase, "Increase Octave", "US_END");
    init_bind_macro(global, decrease_playback_speed, "Decrease Playback Speed", "Shift+US_LEFTBRACKET");
    init_bind_macro(global, decrease_playback_speed, "Increase Playback Speed", "Shift+US_RIGHTBRACKET");
    init_bind_macro(global, decrease_playback_tempo, "Decrease Playback Tempo", "Ctrl+US_LEFTBRACKET");
    init_bind_macro(global, increase_playback_tempo, "Increase Playback Tempo", "Ctrl+US_RIGHTBRACKET");
    init_bind_macro(global, decrease_global_volume, "Decrease Global Volume", "US_LEFTBRACKET");
    init_bind_macro(global, increase_global_volume, "Increase Global Volume", "US_RIGHTBRACKET");

    init_bind_macro(global, toggle_channel_1, "Toggle Channel 1", "Alt+US_F1");
    init_bind_macro(global, toggle_channel_2, "Toggle Channel 2", "Alt+US_F2");
    init_bind_macro(global, toggle_channel_3, "Toggle Channel 3", "Alt+US_F3");
    init_bind_macro(global, toggle_channel_4, "Toggle Channel 4", "Alt+US_F4");
    init_bind_macro(global, toggle_channel_5, "Toggle Channel 5", "Alt+US_F5");
    init_bind_macro(global, toggle_channel_6, "Toggle Channel 6", "Alt+US_F6");
    init_bind_macro(global, toggle_channel_7, "Toggle Channel 7", "Alt+US_F7");
    init_bind_macro(global, toggle_channel_8, "Toggle Channel 8", "Alt+US_F8");

    init_bind_macro(global, mouse_grab, "Toggle Mouse / Keyboard Grab", "Ctrl+US_D");
    init_bind_macro(global, display_reset, "Refresh Screen And Reset Chache Identification", "Ctrl+US_E");
    init_bind_macro(global, go_to_time, "Go To Order / Pattern / Row Given Time", "Ctrl+US_G");
    init_bind_macro(global, audio_reset, "Reinitialize Sound Driver", "Ctrl+US_I");
    init_bind_macro(global, mouse, "Toggle Mouse Cursor", "Ctrl+US_M");
    init_bind_macro(global, new_song, "New Song", "Ctrl+US_N");
    init_bind_macro(global, calculate_song_length, "Calculate Approximate Song Length", "Ctrl+US_P");
    init_bind_macro(global, quit, "Quit Schism Tracker", "Ctrl+US_Q");
    init_bind_macro(global, quit_no_confirm, "Quit Without Confirmation", "Ctrl+Shift+US_Q");
    init_bind_macro(global, save, "Save Current Song", "Ctrl+US_S");

    init_bind_macro(global, fullscreen, "Toggle Fullscreen", "Ctrl+Alt+US_ENTER");
}

void init_keybinds(void)
{
    if (current_binds_count != 0) return;

	log_append(2, 0, "Custom Key Bindings");
	log_underline(19);

	char* path = dmoz_path_concat(cfg_dir_dotschism, "keybinds.ini");
	cfg_file_t cfg;
	cfg_init(&cfg, path);
    init_global_keybinds(&cfg);
    cfg_write(&cfg);
    cfg_free(&cfg);

    if(!has_init_problem)
        log_appendf(5, " No issues");
    log_nl();
}
