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

#define MAX_BINDS 450
static int current_binds_size = 0;
static keybind_bind_t* current_binds[MAX_BINDS];
keybind_list_t global_keybinds_list = {0};

/* --------------------------------------------------------------------- */
/* Updating bind state (handling input events) */

static int check_mods(enum keybind_modifier needed, SDL_Keymod current)
{
#ifdef SCHISM_WIN32
	/* On windows there is a strange issue with SDL. It means that RALT will always be sent
	 * together with LCTRL and there's no way to tell if LCTRL is actually held or not.
	 * That's why LCTRL will be removed while pressing RALT.
	 *
	 * Issue: https://github.com/libsdl-org/SDL/issues/5685
	*/
	if (current & KMOD_RALT) { current &= ~KMOD_LCTRL; }
#endif

	// If either L or R key needed, fixup the flags so both are active.

	if ((needed & KEYBIND_MOD_LCTRL) && !(current & KMOD_LCTRL))
		return 0;

	if ((needed & KEYBIND_MOD_RCTRL) && !(current & KMOD_RCTRL))
		return 0;

	if ((needed & KEYBIND_MOD_CTRL) && !(current & KMOD_CTRL))
		return 0;

	if ((needed & KEYBIND_MOD_LSHIFT) && !(current & KMOD_LSHIFT))
		return 0;

	if ((needed & KEYBIND_MOD_RSHIFT) && !(current & KMOD_RSHIFT))
		return 0;

	if ((needed & KEYBIND_MOD_SHIFT) && !(current & KMOD_SHIFT))
		return 0;

	if ((needed & KEYBIND_MOD_LALT) && !(current & KMOD_LALT))
		return 0;

	if ((needed & KEYBIND_MOD_RALT) && !(current & KMOD_RALT))
		return 0;

	if ((needed & KEYBIND_MOD_ALT) && !(current & KMOD_ALT))
		return 0;

	// looks like it fits
	return 1;
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
		for (int i = 0; i < current_binds_size; i++)
			reset_bind(current_binds[i]);

		return;
	}

	int is_down = event->state == KEY_PRESS;
	SDL_Keymod mods = SDL_GetModState();

	for (int i = 0; i < current_binds_size; i++)
		update_bind(current_binds[i], event->scancode, event->orig_sym, mods, event->orig_text, is_down);
}

/* --------------------------------------------------------------------- */
/* Initiating keybinds */

int keybinds_append_bind_to_list(keybind_bind_t *bind)
{
	if (current_binds_size >= MAX_BINDS) {
		log_appendf(5, " Keybinds exceeding max bind count. Max is %i. Current is %i.", MAX_BINDS, current_binds_size);
		printf("Keybinds exceeding max bind count. Max is %i. Current is %i.\n", MAX_BINDS, current_binds_size);
		fflush(stdout);
		current_binds_size++;
		return 0;
	}

	current_binds[current_binds_size] = bind;
	current_binds_size++;

	return 1;
}

int keybinds_append_shortcut_to_bind(keybind_bind_t* bind, SDL_Keycode keycode, SDL_Scancode scancode, SDL_Keymod modifier)
{
	if (bind->shortcuts_count >= KEYBINDS_MAX_SHORTCUTS) {
		log_appendf(5, " %s/%s: Trying to bind too many shortcuts. Max is %i.", bind->section_info->name, bind->name, KEYBINDS_MAX_SHORTCUTS);
		printf("%s/%s: Trying to bind too many shortcuts. Max is %i.\n", bind->section_info->name, bind->name, KEYBINDS_MAX_SHORTCUTS);
		fflush(stdout);
		return 0;
	}

	if (keycode == SDLK_UNKNOWN && scancode == SDL_SCANCODE_UNKNOWN) {
		printf("Attempting to bind shortcut with no key. Skipping.\n");
		return 0;
	}

	int i = bind->shortcuts_count;
	bind->shortcuts[i].keycode = keycode;
	bind->shortcuts[i].scancode = scancode;
	bind->shortcuts[i].modifier = modifier;
	bind->shortcuts_count++;
	return 1;
}

/* --------------------------------------------------------------------- */
/* Getting help text */

char *keybinds_get_help_text(enum page_numbers page)
{
	const char *current_title = NULL;
	char *out = strdup("");

	for (int i = 0; i < current_binds_size; i++) {
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

		if (current_title != bind_title) {
			if (current_title != NULL && bind->section_info == &global_keybinds_list.global_info) {
				char *new = STR_CONCAT(2, out, "\n \n%\n");
				free(out);
				out = new;
			}

			char *new = STR_CONCAT(4, out, (current_title) ? "\n \n  " : "\n  ", (char *)bind_title, "\n");
			free(out);
			out = new;

			current_title = bind_title;
		}

		char *prev_out = out;
		out = STR_CONCAT(3, prev_out, (char*)bind->help_text, "\n");
		free(prev_out);
	}

	return out;
}
