
#include "it.h"
#include "config.h"
#include "page.h"
#include "dmoz.h"
#include "charset.h"
#include "keybinds_codes.c"
#include "keybinds_init.c"

#define MAX_BINDS 450
#define MAX_SHORTCUTS 3
static int current_binds_count = 0;
static keybind_bind_t* current_binds[MAX_BINDS];
keybind_list_t global_keybinds_list;
static int has_init_problem = 0;

/* --------------------------------------------------------------------- */
/* Updating bind state (handling input events) */

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

static void update_bind(keybind_bind_t* bind, SDL_Scancode scode, SDL_Keycode kcode, SDL_Keymod mods, const char* text, int is_down)
{
    keybind_shortcut_t* sc;

    // Bind is always updated on key event
    bind->pressed = 0;
    bind->released = 0;
    bind->repeated = 0;
    bind->press_repeats = 0;

    int page_matching = (
        bind->section_info->page_matcher ?
        bind->section_info->page_matcher(status.current_page) :
        (bind->section_info->page == PAGE_GLOBAL || bind->section_info->page == status.current_page)
    );

    if (!page_matching) {
        // We need to reset everything so the state isn't messed up next time we enter the page
        for (int i = 0; i < bind->shortcuts_count; i++) {
            sc = &bind->shortcuts[i];
            sc->pressed = 0;
            sc->released = 0;
            sc->repeated = 0;
            sc->press_repeats = 0;
        }
        return;
    }

    for (int i = 0; i < bind->shortcuts_count; i++) {
        sc = &bind->shortcuts[i];

        int pressed = 0;
        int released = 0;
        int repeated = 0;

        if (
            (sc->keycode != SDLK_UNKNOWN && sc->keycode != kcode) ||
            (sc->scancode != SDL_SCANCODE_UNKNOWN && sc->scancode != scode)
        ) {
            if (sc->keycode != kcode) {
                sc->is_press_repeat = 0;
                sc->press_repeats = 0;
                continue;
            }
        }

        int mods_correct = check_mods(sc->modifier, mods, 0);
        int is_repeat = is_down && (sc->pressed || sc->repeated);

        if (is_down) {
            if (mods_correct) {
                pressed = !is_repeat;
                repeated = is_repeat;

                if (sc->is_press_repeat && !is_repeat) {
                    sc->press_repeats++;
                }
            }
        } else {
            if (sc->pressed || sc->repeated)
                released = 1;
        }

        sc->is_press_repeat = 1;

        // Shortcut only updated when it changes. This allows us to correctly handle repeated/released.
        sc->pressed = pressed;
        sc->released = released;
        sc->repeated = repeated;

        if (pressed || released || repeated) {
            bind->pressed = pressed;
            bind->released = released;
            bind->repeated = repeated;
            bind->press_repeats = sc->press_repeats;
            break;
        }

        return;
    }
}

static void reset_bind(keybind_bind_t* bind)
{
    bind->pressed = 0;
    bind->released = 0;
    bind->repeated = 0;
    bind->press_repeats = 0;
}

void keybinds_handle_event(struct key_event* event)
{
	if (event->mouse != MOUSE_NONE) {
		return;
	}

    // This is so that mod keys don't mess with press_repeats
    switch(event->scancode) {
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL:
        case SDL_SCANCODE_LALT:
        case SDL_SCANCODE_RALT:
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT:
            for (int i = 0; i < current_binds_count; i++) {
                reset_bind(current_binds[i]);
            }
            return;
    }

    int is_down = event->state == KEY_PRESS;
    SDL_Keymod mods = SDL_GetModState();

    for (int i = 0; i < current_binds_count; i++) {
        update_bind(current_binds[i], event->scancode, event->orig_sym, mods, event->orig_text, is_down);
    }
}

/* --------------------------------------------------------------------- */
/* Parsing keybind strings */

static int string_to_mod_length = ARRAY_SIZE(string_to_mod);

static int parse_shortcut_mods(const char* shortcut, SDL_Keymod* mods)
{
    for (int i = 0; i < string_to_mod_length; i++) {
        if (charset_strcasecmp(shortcut, CHARSET_UTF8, string_to_mod[i].name, CHARSET_UTF8) == 0) {
            *mods |= string_to_mod[i].mod;
            return 1;
        }
    }

    return 0;
}

static int string_to_scancode_length = ARRAY_SIZE(string_to_scancode);

static int parse_shortcut_scancode(keybind_bind_t* bind, const char* shortcut, SDL_Scancode* code)
{
    for (int i = 0; i < string_to_scancode_length; i++) {
        if (charset_strcasecmp(shortcut, CHARSET_UTF8, string_to_scancode[i].name, CHARSET_UTF8) == 0) {
            *code = string_to_scancode[i].code;
            return 1;
        }
    }

    if(shortcut[0] == 'U' && shortcut[1] == 'S' && shortcut[2] == '_') {
        log_appendf(5, " %s/%s: Unknown code '%s'", bind->section_info->name, bind->name, shortcut);
        printf("%s/%s: Unknown code '%s'\n", bind->section_info->name, bind->name, shortcut);
        fflush(stdout);
        has_init_problem = 1;
        return -1;
    }

    return 0;
}

static int string_to_keycode_length = ARRAY_SIZE(string_to_keycode);

static int parse_shortcut_keycode(keybind_bind_t* bind, const char* shortcut, SDL_Keycode* kcode)
{
    if(!shortcut || !shortcut[0]) return 0;

    for (int i = 0; i < string_to_keycode_length; i++) {
        if (charset_strcasecmp(shortcut, CHARSET_UTF8, string_to_keycode[i].name, CHARSET_UTF8) == 0) {
            *kcode = string_to_keycode[i].code;
            return 1;
        }
    }

    SDL_Keycode found_code = SDL_GetKeyFromName(shortcut);

    if (found_code == SDLK_UNKNOWN) {
        log_appendf(5, " %s/%s: Unknown code '%s'", bind->section_info->name, bind->name, shortcut);
        printf("%s/%s: Unknown code '%s'\n", bind->section_info->name, bind->name, shortcut);
        fflush(stdout);
        has_init_problem = 1;
        return -1;
    }

    *kcode = found_code;

    return 1;
}

void keybinds_add_bind_shortcut(keybind_bind_t* bind, SDL_Keycode keycode, SDL_Scancode scancode, SDL_Keymod modifier);

static void keybinds_parse_shortcut_splitted(keybind_bind_t* bind, const char* shortcut)
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

        int key_result = parse_shortcut_keycode(bind, trimmed, &key_code);
        if (key_result == 1) break;
        if (key_result == -1) { has_problem = 1; break; }
    }

    if (trimmed) free(trimmed);
    free(shortcut_dup);
    if(has_problem) return;
    keybinds_add_bind_shortcut(bind, key_code, scan_code, mods);
}

void keybinds_parse_shortcut(keybind_bind_t* bind, const char* shortcut)
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

/* --------------------------------------------------------------------- */
/* Initiating keybinds */

void keybinds_add_bind_shortcut(keybind_bind_t* bind, SDL_Keycode keycode, SDL_Scancode scancode, SDL_Keymod modifier)
{
    if(bind->shortcuts_count == MAX_SHORTCUTS) {
        log_appendf(5, " %s/%s: Trying to bind too many shortcuts. Max is %i.", bind->section_info->name, bind->name, MAX_SHORTCUTS);
        printf("%s/%s: Trying to bind too many shortcuts. Max is %i.\n", bind->section_info->name, bind->name, MAX_SHORTCUTS);
        fflush(stdout);
        has_init_problem = 1;
        return;
    }

    if(keycode == SDLK_UNKNOWN && scancode == SDL_SCANCODE_UNKNOWN) {
        printf("Attempting to bind shortcut with no key. Skipping.\n");
        return;
    }

    int i = bind->shortcuts_count;
    bind->shortcuts[i].keycode = keycode;
    bind->shortcuts[i].scancode = scancode;
    bind->shortcuts[i].modifier = modifier;
    bind->shortcuts_count++;
}

static void init_section(keybind_section_info_t* section_info, const char* name, const char* title, enum page_numbers page)
{
    section_info->is_active = 0;
    section_info->name = name;
    section_info->title = title;
    section_info->page = page;
    section_info->page_matcher = NULL;
}

static void set_shortcut_text(keybind_bind_t* bind)
{
    char* out[MAX_SHORTCUTS];

    for(int i = 0; i < MAX_SHORTCUTS; i++) {
        out[i] = NULL;
        if(i >= bind->shortcuts_count) continue;

        keybind_shortcut_t* sc = &bind->shortcuts[i];

        const char* ctrl_text = (sc->modifier & KMOD_CTRL) ? "Ctrl-" : "";
        const char* alt_text = (sc->modifier & KMOD_LALT) ? "Alt-" : "";
        const char* shift_text = (sc->modifier & KMOD_SHIFT) ? "Shift-" : "";

        char* key_text = NULL;

        if (sc->keycode != SDLK_UNKNOWN) {
            switch(sc->keycode) {
                case SDLK_RETURN:
                    key_text = strdup("Enter");
                    break;
                case SDLK_SPACE:
                    key_text = strdup("Spacebar");
                    break;
                default:
                    key_text = strdup(SDL_GetKeyName(sc->keycode));
            }
        } else if(sc->scancode != SDL_SCANCODE_UNKNOWN) {
            switch(sc->scancode) {
                case SDL_SCANCODE_RETURN:
                    key_text = strdup("Enter");
                    break;
                case SDL_SCANCODE_SPACE:
                    key_text = strdup("Spacebar");
                    break;
                default:
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
        char* next_out = str_concat_array(4, strings, 0);

        out[i] = next_out;

        if(i == 0) {
            bind->first_shortcut_text = strdup(next_out);
            bind->first_shortcut_text_parens = str_concat_three(" (", next_out, ")", 0);
        }

        free(key_text);
    }

    char* shortcut_text = str_concat_with_delim(MAX_SHORTCUTS, out, ", ", 0);
    bind->shortcut_text = shortcut_text;
    if(shortcut_text[0])
        bind->shortcut_text_parens = str_concat_three(" (", shortcut_text, ")", 0);

    for (int i = 0; i < MAX_SHORTCUTS; i++) {
        if (!out[i]) continue;

        char* text = out[i];

        if(text && text[0]) {
            char* padded = str_pad_between(text, "", ' ', 18, 1, 0);
            out[i] = str_concat_two("    ", padded, 0);
            free(padded);
        }

        if (text)
            free(text);
    }

    char* help_shortcuts = str_concat_with_delim(MAX_SHORTCUTS, out, "\n", 1);
    bind->help_text = str_concat_three(help_shortcuts, (char*)bind->description, "\n", 0);
    free(help_shortcuts);
}

static void init_bind(keybind_bind_t* bind, keybind_section_info_t* section_info, const char* name, const char* description, const char* shortcut)
{
    if(current_binds_count >= MAX_BINDS) {
        log_appendf(5, " Keybinds exceeding max bind count. Max is %i. Current is %i.", MAX_BINDS, current_binds_count);
        printf("Keybinds exceeding max bind count. Max is %i. Current is %i.\n", MAX_BINDS, current_binds_count);
        fflush(stdout);
        has_init_problem = 1;
        current_binds_count++;
        return;
    }

    current_binds[current_binds_count] = bind;
    current_binds_count++;

    bind->pressed = 0;
    bind->released = 0;
    bind->shortcuts = malloc(sizeof(keybind_shortcut_t) * 3);

    for(int i = 0; i < 3; i++) {
        keybind_shortcut_t* sc = &bind->shortcuts[i];
        sc->scancode = SDL_SCANCODE_UNKNOWN;
        sc->keycode = SDLK_UNKNOWN;
        sc->modifier = KMOD_NONE;
        sc->pressed = 0;
        sc->released = 0;
        sc->repeated = 0;
        sc->is_press_repeat = 0;
        sc->press_repeats = 0;
    }

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

    keybinds_parse_shortcut(bind, shortcut);
    set_shortcut_text(bind);
}

void init_keybinds(void)
{
    if (current_binds_count != 0) return;

	log_append(2, 0, "Custom Key Bindings");
	log_underline(19);

	char* path = dmoz_path_concat(cfg_dir_dotschism, "keybinds.ini");
	cfg_file_t cfg;
	cfg_init(&cfg, path);

    // Defined in keybinds_init.c
    init_all_keybinds(&cfg);

    cfg_write(&cfg);
    cfg_free(&cfg);

    if(!has_init_problem)
        log_appendf(5, " No issues");
    log_nl();
}

/* --------------------------------------------------------------------- */
/* Getting help text */

char* keybinds_get_help_text(enum page_numbers page)
{
    const char* current_title = NULL;
    char* out = strdup("");

    for (int i = 0; i < MAX_BINDS; i++) {
        // lines[i] = NULL;

        if (i >= current_binds_count)
            continue;

        keybind_bind_t* bind = current_binds[i];
        const char* bind_title = bind->section_info->title;
        enum page_numbers bind_page = bind->section_info->page;

        if (bind->section_info->page_matcher) {
            if (!bind->section_info->page_matcher(page))
                continue;
        } else {
            if (bind_page != PAGE_GLOBAL && bind_page != page)
                continue;
        }

        char* strings[2] = { (char*)bind->description, (char*)bind->shortcut_text };

        if (current_title != bind_title) {
            if (current_title != NULL && bind->section_info == &global_keybinds_list.global_info) {
                out = str_concat_two(out, "\n \n%\n", 0);
            }

            char* prev_out = out;
            if(current_title)
                out = str_concat_four(out, "\n \n  ", (char*)bind_title, "\n", 0);
            else
                out = str_concat_four(out, "\n  ", (char*)bind_title, "\n", 0);
            current_title = bind_title;
            free(prev_out);
        }

        char* prev_out = out;
        out = str_concat_three(out, (char*)bind->help_text, "\n", 0);
        free(prev_out);
    }

    return out;
}
