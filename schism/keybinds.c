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
#include "config.h"
#include "page.h"
#include "dmoz.h"
#include "charset.h"
#include "util.h"

/* these two ought to be compiled separately */
#include "keybinds_init.c"

#define MAX_BINDS 450
#define MAX_SHORTCUTS 3
static int current_binds_count = 0;
static keybind_bind_t* current_binds[MAX_BINDS];
keybind_list_t global_keybinds_list = {0};
static int has_init_problem = 0;

/* --------------------------------------------------------------------- */
/* Updating bind state (handling input events) */

static int check_mods(SDL_Keymod needed, SDL_Keymod current)
{
    current &= (KMOD_CTRL | KMOD_SHIFT | KMOD_ALT); // Remove unsupported mods.

    /*
        On windows there is a strange issue with SDL. It means that RALT will always be sent
        together with LCTRL and there's no way to tell if LCTRL is actually held or not.
        That's why LCTRL will be removed while pressing RALT.
        Issue: https://github.com/libsdl-org/SDL/issues/5685
    */
#ifdef SCHISM_WIN32
    if (current & KMOD_RALT) { current &= ~KMOD_LCTRL; }
#endif

    // If either L or R key needed, fixup the flags so both are active.

    if ((needed & KMOD_LCTRL) && (needed & KMOD_RCTRL) && (current & KMOD_CTRL))
        current |= KMOD_CTRL;

    if ((needed & KMOD_LSHIFT) && (needed & KMOD_RSHIFT) && (current & KMOD_SHIFT))
        current |= KMOD_SHIFT;

    if ((needed & KMOD_LALT) && (needed & KMOD_RALT) && (current & KMOD_ALT))
        current |= KMOD_ALT;

    return needed == current;
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

		if ((sc->keycode != SDLK_UNKNOWN && sc->keycode != kcode)
			|| (sc->scancode != SDL_SCANCODE_UNKNOWN && sc->scancode != scode)) {
			if (sc->keycode != kcode) {
				sc->is_press_repeat = 0;
				sc->press_repeats = 0;
				continue;
			}
		}

		int mods_correct = check_mods(sc->modifier, mods);
		int is_repeat = is_down && (sc->pressed || sc->repeated);

		if (is_down) {
			if (mods_correct) {
				pressed = !is_repeat;
				repeated = is_repeat;

				if (sc->is_press_repeat && !is_repeat)
					sc->press_repeats++;
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
	if (event->mouse != MOUSE_NONE)
		return;

	// This is so that mod keys don't mess with press_repeats
	switch(event->scancode) {
	case SDL_SCANCODE_LCTRL:
	case SDL_SCANCODE_RCTRL:
	case SDL_SCANCODE_LALT:
	case SDL_SCANCODE_RALT:
	case SDL_SCANCODE_LSHIFT:
	case SDL_SCANCODE_RSHIFT:
		for (int i = 0; i < current_binds_count; i++)
			reset_bind(current_binds[i]);

		return;
	}

	int is_down = event->state == KEY_PRESS;
	SDL_Keymod mods = SDL_GetModState();

	for (int i = 0; i < current_binds_count; i++)
		update_bind(current_binds[i], event->scancode, event->orig_sym, mods, event->orig_text, is_down);
}

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
		has_init_problem = 1;
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
		has_init_problem = 1;
		return -1;
	}

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

	for (int i = 0; ; i++) {
		const char* next = strtok_r(i == 0 ? shortcut_dup : NULL, delim, &strtok_ptr);

		if (trimmed) free(trimmed);
		trimmed = strdup(next);
		trim_string(trimmed);

		if (next == NULL) break;

		SDL_Keymod mod;
		if (keybinds_parse_modkey(trimmed, &mod)) {
			mods |= mod;
			continue;
		}

		int scan_result = parse_shortcut_scancode(bind, trimmed, &scan_code);
		if (scan_result == 1) break;
		if (scan_result == -1) { has_problem = 1; break; }

		int key_result = parse_shortcut_keycode(bind, trimmed, &key_code);
		if (key_result == 1) break;
		if (key_result == -1) { has_problem = 1; break; }
	}

	if (trimmed)
		free(trimmed);

	free(shortcut_dup);

	if (has_problem)
		return;

	keybinds_add_bind_shortcut(bind, key_code, scan_code, mods);
}

void keybinds_parse_shortcut(keybind_bind_t* bind, const char* shortcut)
{
	char* shortcut_dup = strdup(shortcut);
	char *strtok_ptr;
	const char* delim = ",";

	for (int i = 0; i < 10; i++) {
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
	if (bind->shortcuts_count == MAX_SHORTCUTS) {
		log_appendf(5, " %s/%s: Trying to bind too many shortcuts. Max is %i.", bind->section_info->name, bind->name, MAX_SHORTCUTS);
		printf("%s/%s: Trying to bind too many shortcuts. Max is %i.\n", bind->section_info->name, bind->name, MAX_SHORTCUTS);
		fflush(stdout);
		has_init_problem = 1;
		return;
	}

	if (keycode == SDLK_UNKNOWN && scancode == SDL_SCANCODE_UNKNOWN) {
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

		const char* ctrl_text = "";
		const char* alt_text = "";
		const char* shift_text = "";

        if ((sc->modifier & KMOD_LCTRL) && (sc->modifier & KMOD_RCTRL))
            ctrl_text = "Ctrl-";
        else if (sc->modifier & KMOD_LCTRL)
            ctrl_text = "LCtrl-";
        else if (sc->modifier & KMOD_RCTRL)
            ctrl_text = "RCtrl-";

        if ((sc->modifier & KMOD_LSHIFT) && (sc->modifier & KMOD_RSHIFT))
            ctrl_text = "Shift-";
        else if (sc->modifier & KMOD_LSHIFT)
            ctrl_text = "LShift-";
        else if (sc->modifier & KMOD_RSHIFT)
            ctrl_text = "RShift-";

        if ((sc->modifier & KMOD_LALT) && (sc->modifier & KMOD_RALT))
            ctrl_text = "Alt-";
        else if (sc->modifier & KMOD_LALT)
            ctrl_text = "LAlt-";
        else if (sc->modifier & KMOD_RALT)
            ctrl_text = "RAlt-";

		char* key_text = NULL;

		if (sc->keycode != SDLK_UNKNOWN) {
			switch(sc->keycode) {
			case SDLK_RETURN:
#ifdef SCHISM_MACOSX
				key_text = strdup("Return");
#else
				key_text = strdup("Enter");
#endif
				break;
			case SDLK_SPACE:
				key_text = strdup("Spacebar");
				break;
			default:
				key_text = strdup(SDL_GetKeyName(sc->keycode));
				break;
			}
		} else if (sc->scancode != SDL_SCANCODE_UNKNOWN) {
			switch(sc->scancode) {
			case SDL_SCANCODE_RETURN:
				key_text = strdup("Enter");
				break;
			case SDL_SCANCODE_SPACE:
				key_text = strdup("Spacebar");
				break;
			default: {
				SDL_Keycode code = SDL_GetKeyFromScancode(sc->scancode);
				key_text = strdup(SDL_GetKeyName(code));
				break;
			}
			}
		} else {
			continue;
		}

		int key_text_length = strlen(key_text);

		if (key_text_length == 0)
			continue;

		char* next_out = STR_CONCAT(4, ctrl_text, alt_text, shift_text, key_text);

		out[i] = next_out;

		if(i == 0) {
			bind->first_shortcut_text = strdup(next_out);
			bind->first_shortcut_text_parens = STR_CONCAT(3, " (", next_out, ")");
		}

		free(key_text);
	}

	char* shortcut_text = str_implode(MAX_SHORTCUTS, ", ", (const char**)out);
	bind->shortcut_text = shortcut_text;
	if(shortcut_text[0])
		bind->shortcut_text_parens = STR_CONCAT(3, " (", shortcut_text, ")");

	for (int i = 0; i < MAX_SHORTCUTS; i++) {
		if (!out[i])
			continue;

		char* text = out[i];

		if (text && text[0]) {
			char* padded = str_pad_between(text, "", ' ', 18, 1);
			out[i] = STR_CONCAT(2, "    ", padded);
			free(padded);
		}

		if (text)
			free(text);
	}

	char* help_shortcuts = str_implode_free(MAX_SHORTCUTS, "\n", out);
	bind->help_text = STR_CONCAT(3, help_shortcuts, (char*)bind->description, "\n");
	free(help_shortcuts);
}

static void init_bind(keybind_bind_t* bind, keybind_section_info_t* section_info, const char* name, const char* description, const char* shortcut)
{
	if (current_binds_count >= MAX_BINDS) {
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

	for (int i = 0; i < 3; i++) {
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

void keybinds_init(void)
{
	if (current_binds_count != 0)
		return;

	log_append(2, 0, "Custom Key Bindings");
	log_underline(19);

	char* path = dmoz_path_concat(cfg_dir_dotschism, "keybinds.ini");

	cfg_file_t cfg;
	cfg_init(&cfg, path);

	// Defined in keybinds_init.c
	init_all_keybinds(&cfg);

	cfg_write(&cfg);
	cfg_free(&cfg);

	free(path);

	if(!has_init_problem)
		log_appendf(5, " No issues");
	log_nl();
}

/* --------------------------------------------------------------------- */
/* Getting help text */

char *keybinds_get_help_text(enum page_numbers page)
{
	const char *current_title = NULL;
	char *out = strdup("");

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

		char *strings[2] = { (char *)bind->description, (char *)bind->shortcut_text };

		if (current_title != bind_title) {
			if (current_title != NULL && bind->section_info == &global_keybinds_list.global_info)
				out = STR_CONCAT(2, out, "\n \n%\n");

			char* prev_out = out;

			out = STR_CONCAT(4, out, current_title ? "\n \n  " : "\n  ", (char *)bind_title, "\n");

			current_title = bind_title;
			free(prev_out);
		}

		char* prev_out = out;
		out = STR_CONCAT(3, out, (char*)bind->help_text, "\n");
		free(prev_out);
	}

	return out;
}
