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

#include "headers.h"

#include "keybinds.h"

#include "log.h"

/* This is a private implementation detail. :) */
enum {
	/* Low 8 bytes: LCTRL, RCTRL, etc */
	KEYBIND_MOD_LCTRL = (0x01),
	KEYBIND_MOD_RCTRL = (0x02),
	KEYBIND_MOD_LSHIFT = (0x04),
	KEYBIND_MOD_RSHIFT = (0x08),
	KEYBIND_MOD_LALT = (0x10),
	KEYBIND_MOD_RALT = (0x20),
	/*
	KEYBIND_MOD_LGUI = (0x40),
	KEYBIND_MOD_RGUI = (0x80),
	*/

	/* Next 8 bytes: CTRL, SHIFT, ALT. Defined in a way to where mutual exclusion
	 * is easily calculated:
	 *
	 *     uint16_t x = (keybind_mods & 0xFFFF);
	 *
	 *     SCHISM_RUNTIME_ASSERT(((x >> 8) & x) != 0,
	 *       "left/right and full modifiers are mutually exclusive"); */
	KEYBIND_MOD_CTRL = ((KEYBIND_MOD_LCTRL | KEYBIND_MOD_RCTRL) << 8),
	KEYBIND_MOD_SHIFT = ((KEYBIND_MOD_LSHIFT | KEYBIND_MOD_RSHIFT) << 8),
	KEYBIND_MOD_ALT = ((KEYBIND_MOD_LALT | KEYBIND_MOD_RALT) << 8),
	/*
	KEYBIND_MOD_GUI = ((KEYBIND_MOD_LGUI | KEYBIND_MOD_RGUI) << 8),
	*/
};

enum {
	KEYBIND_TYPE_SCANCODE,
	KEYBIND_TYPE_KEYCODE,
};

enum {
	KEYBIND_STATE_PRESSED = (0x01),
	KEYBIND_STATE_RELEASED = (0x02),
	KEYBIND_STATE_REPEATED = (0x04),
};

/* Try to keep this structure reasonably small!
 *
 * I've cobbled this together so that it only takes
 * up 12 bytes for now.
 *
 * If more stuff gets added, try to fit it so where
 * the structure will take up the least space after
 * alignment.
 *
 * NOTE: this not only holds the (somewhat read-only)
 * keybind info, but also the current state. */
struct keybind {
	/* how many times the current key has been repeated */
	uint16_t repeats;

	/* bitwise OR of the state values defined above */
	uint8_t state;

	/* scancode & keycode are mutually exclusive */
	uint8_t type;
	union {
		/* these are actually the same size, so there's no difference
		 * besides some code hints to show WHICH one we're using. */
		schism_scancode_t scan;
		schism_keysym_t key;
	} code;

	/* bitwise OR of the modifier values defined above */
	uint16_t modifiers;

	/* how many repeats we need to count this bind as pressed.
	 * this is an ugly thing derived from old hardcoded keybinds. */
	uint16_t repeats_needed;
};

struct keybind_section {
	/* FIXME find a better way to get the upper limit of binds?
	 * maybe we should put a static assertion under each definition, and just make
	 * it all public? idk  --paper */
	struct keybind binds[MAX(KEYBIND_BIND_GLOBAL_MAX_, KEYBIND_BIND_PATTERN_EDITOR_MAX_)];

	size_t num_binds;
};

static struct keybind_section keybind_sections[KEYBIND_SECTION_MAX_] = {0};

/* ------------------------------------------------------------------------ */
/* initialization code */

static inline void keybinds_add_test_bind(schism_keysym_t code, uint16_t mods)
{
	struct keybind *b = &keybind_sections[0].binds[keybind_sections[0].num_binds];

	b->code.key = code;
	b->type = KEYBIND_TYPE_KEYCODE;
	b->modifiers = mods;

	keybind_sections[0].num_binds++;
}

int keybinds_init(void)
{
	/* TODO... should this use the same format as outlined in the previous
	 * configurable keybinds PR, or something else entirely? */

	/* no modifier */
	keybinds_add_test_bind(SCHISM_KEYSYM_a, 0);
	/* both ctrl keys */
	keybinds_add_test_bind(SCHISM_KEYSYM_a, KEYBIND_MOD_CTRL);
	/* left shift (right shift should not trigger this) */
	keybinds_add_test_bind(SCHISM_KEYSYM_a, KEYBIND_MOD_LSHIFT);
	/* ctrl+shift */
	keybinds_add_test_bind(SCHISM_KEYSYM_a, KEYBIND_MOD_CTRL | KEYBIND_MOD_SHIFT);

	log_appendf(1, "keybinds size: %" PRIuSZ, sizeof(struct keybind));

	return 0;
}

/* ------------------------------------------------------------------------ */
/* event handler */

static inline int keybinds_modifier_held(const struct keybind *b, schism_keymod_t mod)
{
	/* only see the modifiers we care about... */
	mod &= (SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT | SCHISM_KEYMOD_SHIFT);

	/* now, remove any bits that we need */
#define CHECK_MODIFIER(x) \
	do { \
		if (b->modifiers & KEYBIND_MOD_##x) { \
			if (!(mod & SCHISM_KEYMOD_##x)) \
				return 0; \
		\
			mod &= ~(SCHISM_KEYMOD_##x); \
		} \
	} while (0)

#define CHECK_LRMODIFIERS(x) \
	CHECK_MODIFIER(x); \
	CHECK_MODIFIER(L##x); \
	CHECK_MODIFIER(R##x)

	CHECK_LRMODIFIERS(CTRL);
	CHECK_LRMODIFIERS(SHIFT);
	CHECK_LRMODIFIERS(ALT);

#undef CHECK_MODIFIER
#undef CHECK_LRMODIFIERS

	/* if we have ANYTHING left, we have extra modifiers being held.
	 * this means there is probably some other keybind that this should
	 * map to, so punt. */
	if (mod)
		return 0;

	return 1;
}

static inline int keybind_check_code(const struct keybind *b, const struct key_event *kk)
{
	switch (b->type) {
	case KEYBIND_TYPE_KEYCODE:  return (kk->sym      == b->code.key);
	case KEYBIND_TYPE_SCANCODE: return (kk->scancode == b->code.scan);
	}

	SCHISM_UNREACHABLE;
	return 0;
}

static int keybind_check(const struct keybind *b, const struct key_event *kk)
{
	return (keybind_check_code(b, kk) && keybinds_modifier_held(b, kk->mod));
}

void keybinds_event(const struct key_event *kk)
{
	size_t i, j;

	for (i = 0; i < ARRAY_SIZE(keybind_sections); i++) {
		for (j = 0; j < keybind_sections[i].num_binds; j++) {
			struct keybind *b = &keybind_sections[i].binds[j];

			if (keybind_check(b, kk)) {
				switch (kk->state) {
				case KEY_PRESS:
					if (b->state & KEYBIND_STATE_RELEASED) {
						b->state |= KEYBIND_STATE_REPEATED;
						/* maybe handle overflow here? */
						b->repeats++;
					}

					b->state |= KEYBIND_STATE_PRESSED;
					b->state &= ~KEYBIND_STATE_RELEASED;
					break;
				case KEY_RELEASE:
					b->state |= KEYBIND_STATE_RELEASED;
					b->state &= ~KEYBIND_STATE_PRESSED;
					break;
				default:
					SCHISM_UNREACHABLE;
					break;
				}
			} else {
				/* keybind isn't pressed or released */
				b->state = 0;
				b->repeats = 0;
			}

			/*log_appendf(1, "KEYBIND/%" PRIuSZ "/%" PRIuSZ ": state: %" PRIx8 ", repeats: %" PRIu16,
				i, j, b->state, b->repeats);*/
		}
	}
}

/* ------------------------------------------------------------------------ */
/* ahem. */

int keybinds_pressed(int section, int bind)
{
	struct keybind *b;

	SCHISM_RUNTIME_ASSERT(section < ARRAY_SIZE(keybind_sections),
		"keybinds section is out of bounds");
	SCHISM_RUNTIME_ASSERT(bind < keybind_sections[section].num_binds,
		"keybinds bind for section is out of bounds");

	b = &keybind_sections[section].binds[bind];

	return ((b->state & KEYBIND_STATE_PRESSED)
		&& b->repeats >= b->repeats_needed);
}

int keybinds_released(int section, int bind)
{
	struct keybind *b;

	SCHISM_RUNTIME_ASSERT(section < ARRAY_SIZE(keybind_sections),
		"keybinds section is out of bounds");
	SCHISM_RUNTIME_ASSERT(bind < keybind_sections[section].num_binds,
		"keybinds bind for section is out of bounds");

	b = &keybind_sections[section].binds[bind];

	return ((b->state & KEYBIND_STATE_RELEASED)
		&& b->repeats >= b->repeats_needed);
}
