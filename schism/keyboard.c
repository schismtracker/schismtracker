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

#include "it.h"
#include "keyboard.h"
#include "charset.h"
#include "song.h"
#include "page.h"
#include "osdefs.h"

#include "sdlmain.h"
#include <ctype.h>

/* --------------------------------------------------------------------- */

static const char *note_names_up[12] = {
	"C-", "C#", "D-", "D#", "E-", "F-",
	"F#", "G-", "G#", "A-", "A#", "B-"
};

static const char note_names_short_up[12] = "cCdDefFgGaAb";

static const char *note_names_down[12] = {
	"C-", "Db", "D-", "Eb", "E-", "F-",
	"Gb", "G-", "Ab", "A-", "Bb", "B-"
};

static const char note_names_short_down[12] = "CdDeEFgGaAbB";

static const char **note_names = note_names_up;
static const char *note_names_short = note_names_short_up;

/* --------------------------------------------------------------------- */

static int current_octave = 4;

/* --------------------------------------------------------------------- */

/* this is used in a couple of other places... maybe it should be in some
 * general-stuff file? */
const char hexdigits[16] = "0123456789ABCDEF";

/* extra non-IT effects:
 *         '!' = volume '$' = keyoff
 *         '&' = setenvposition
 *         '('/')' = noteslide up/down (IMF) */
static const char effects[] =     ".JFEGHLKRXODB!CQATI?SMNVW$UY?P&Z()?";
static const char ptm_effects[] = ".0123456789ABCDRFFT????GHK?YXPLZ()?";

/* --------------------------------------------------------------------- */

kbd_sharp_flat_t kbd_sharp_flat_state(void)
{
	return (note_names == note_names_up) ? KBD_SHARP_FLAT_SHARPS : KBD_SHARP_FLAT_FLATS;
}

void kbd_sharp_flat_toggle(kbd_sharp_flat_t e)
{
	switch (e) {
	case KBD_SHARP_FLAT_TOGGLE:
		kbd_sharp_flat_toggle((note_names == note_names_up) ? KBD_SHARP_FLAT_FLATS : KBD_SHARP_FLAT_SHARPS);
		return;
	default:
	case KBD_SHARP_FLAT_SHARPS:
		status_text_flash("Displaying accidentals as sharps (#)");
		note_names = note_names_up;
		note_names_short = note_names_short_up;
		break;
	case KBD_SHARP_FLAT_FLATS:
		status_text_flash("Displaying accidentals as flats (b)");
		note_names = note_names_down;
		note_names_short = note_names_short_down;
		break;
	}
}

char get_effect_char(int effect)
{
	if (effect < 0 || effect > 34) {
		log_appendf(4, "get_effect_char: effect %d out of range",
				effect);
		return '?';
	}
	return effects[effect];
}

int get_ptm_effect_number(char effect)
{
	const char *ptr;
	if (effect >= 'a' && effect <= 'z') effect -= 32;

	ptr = strchr(ptm_effects, effect);
	return ptr ? (ptr - effects) : -1;
}

int get_effect_number(char effect)
{
	const char *ptr;

	if (effect >= 'a' && effect <= 'z') {
		effect -= 32;
	} else if (!((effect >= '0' && effect <= '9')
			 || (effect >= 'A' && effect <= 'Z')
			 || (effect == '.'))) {
		/* don't accept pseudo-effects */
		if (status.flags & CLASSIC_MODE) return -1;
	}

	ptr = strchr(effects, effect);
	return ptr ? ptr - effects : -1;
}
int kbd_get_effect_number(struct key_event *k)
{
	if (!NO_CAM_MODS(k->mod)) return -1;
	switch (k->sym) {
#define QZA(n) case SDLK_ ## n : return get_effect_number(#n [0])
QZA(a);QZA(b);QZA(c);QZA(d);QZA(e);QZA(f);QZA(g);QZA(h);QZA(i);QZA(j);QZA(k);
QZA(l);QZA(m);QZA(n);QZA(o);QZA(p);QZA(q);QZA(r);QZA(s);QZA(t);QZA(u);QZA(v);
QZA(w);QZA(x);QZA(y);QZA(z);
#undef QZA
	case SDLK_PERIOD: case SDLK_KP_PERIOD:
		return get_effect_number('.');
	case SDLK_1:
		if (!(k->mod & KMOD_SHIFT)) return -1;
	case SDLK_EXCLAIM:
		return get_effect_number('!');
	case SDLK_4:
		if (!(k->mod & KMOD_SHIFT)) return -1;
	case SDLK_DOLLAR:
		return get_effect_number('$');
	case SDLK_7:
		if (!(k->mod & KMOD_SHIFT)) return -1;
	case SDLK_AMPERSAND:
		return get_effect_number('&');

	default:
		return -1;
	};
}

/* --------------------------------------------------------------------- */

void kbd_key_translate(struct key_event *k)
{
	/* FIXME: this assumes a US keyboard layout */
	k->orig_sym = k->sym;
	if (k->mod & KMOD_SHIFT) {
		switch (k->sym) {
		case SDLK_COMMA: k->sym = SDLK_LESS; break;
		case SDLK_PERIOD: k->sym = SDLK_GREATER; break;
		case SDLK_4: k->sym = SDLK_DOLLAR; break;

		case SDLK_EQUALS: k->sym = SDLK_PLUS; break;
		case SDLK_SEMICOLON: k->sym = SDLK_COLON; break;

		case SDLK_8: k->sym = SDLK_ASTERISK; break;
		default:
			break;
		};
	}
	if (k->mod & KMOD_GUI) {
		k->mod = ((k->mod & ~KMOD_GUI)
			  | ((status.flags & META_IS_CTRL)
				 ? KMOD_CTRL : KMOD_ALT));
	}
	if ((k->mod & KMOD_MODE) && (status.flags & ALTGR_IS_ALT)) {
		/* Treat AltGr as Alt (delt) */
		k->mod = ((k->mod & ~KMOD_MODE) | KMOD_ALT);
	}
	if (k->mod & KMOD_NUM) {
		switch (k->sym) {
		case SDLK_KP_0: k->sym = SDLK_0; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_1: k->sym = SDLK_1; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_2: k->sym = SDLK_2; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_3: k->sym = SDLK_3; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_4: k->sym = SDLK_4; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_5: k->sym = SDLK_5; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_6: k->sym = SDLK_6; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_7: k->sym = SDLK_7; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_8: k->sym = SDLK_8; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_9: k->sym = SDLK_9; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_PERIOD: k->sym = SDLK_PERIOD; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_DIVIDE: k->sym = SDLK_SLASH; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_MULTIPLY: k->sym = SDLK_ASTERISK; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_MINUS: k->sym = SDLK_MINUS; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_PLUS: k->sym = SDLK_PLUS; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_ENTER: k->sym = SDLK_RETURN; k->mod &= ~KMOD_NUM; break;
		case SDLK_KP_EQUALS: k->sym = SDLK_EQUALS; k->mod &= ~KMOD_NUM; break;
		default:
			break;
		};
	} else {
		switch (k->sym) {
		case SDLK_KP_0: k->sym = SDLK_INSERT; break;
		case SDLK_KP_4: k->sym = SDLK_LEFT; break;
		case SDLK_KP_6: k->sym = SDLK_RIGHT; break;
		case SDLK_KP_2: k->sym = SDLK_DOWN; break;
		case SDLK_KP_8: k->sym = SDLK_UP; break;

		case SDLK_KP_9: k->sym = SDLK_PAGEUP; break;
		case SDLK_KP_3: k->sym = SDLK_PAGEDOWN; break;

		case SDLK_KP_7: k->sym = SDLK_HOME; break;
		case SDLK_KP_1: k->sym = SDLK_END; break;

		case SDLK_KP_PERIOD: k->sym = SDLK_DELETE; break;

		case SDLK_KP_DIVIDE: k->sym = SDLK_SLASH; break;
		case SDLK_KP_MULTIPLY: k->sym = SDLK_ASTERISK; break;
		case SDLK_KP_MINUS: k->sym = SDLK_MINUS; break;
		case SDLK_KP_PLUS: k->sym = SDLK_PLUS; break;
		case SDLK_KP_ENTER: k->sym = SDLK_RETURN; break;
		case SDLK_KP_EQUALS: k->sym = SDLK_EQUALS; break;

		default:
			break;
		};
	}
}

int numeric_key_event(struct key_event *k, int kponly)
{
	if (kponly) {
		switch (k->orig_sym) {
		case SDLK_KP_0: return 0;
		case SDLK_KP_1: return 1;
		case SDLK_KP_2: return 2;
		case SDLK_KP_3: return 3;
		case SDLK_KP_4: return 4;
		case SDLK_KP_5: return 5;
		case SDLK_KP_6: return 6;
		case SDLK_KP_7: return 7;
		case SDLK_KP_8: return 8;
		case SDLK_KP_9: return 9;
		default:
			break;
		};
		return -1;
	}

	switch (k->orig_sym) {
	case SDLK_0: case SDLK_KP_0: return 0;
	case SDLK_1: case SDLK_KP_1: return 1;
	case SDLK_2: case SDLK_KP_2: return 2;
	case SDLK_3: case SDLK_KP_3: return 3;
	case SDLK_4: case SDLK_KP_4: return 4;
	case SDLK_5: case SDLK_KP_5: return 5;
	case SDLK_6: case SDLK_KP_6: return 6;
	case SDLK_7: case SDLK_KP_7: return 7;
	case SDLK_8: case SDLK_KP_8: return 8;
	case SDLK_9: case SDLK_KP_9: return 9;
	default:
		break;
	};
	return -1;
}


/* --------------------------------------------------------------------- */

char *get_volume_string(int volume, int volume_effect, char *buf)
{
	const char cmd_table[16] = "...CDAB$H<>GFE";

	buf[2] = 0;

	if (volume_effect < 0 || volume_effect > 13) {
		log_appendf(4, "get_volume_string: volume effect %d out"
				" of range", volume_effect);
		buf[0] = buf[1] = '?';
		return buf;
	}

	/* '$'=vibratospeed, '<'=panslideleft, '>'=panslideright */
	switch (volume_effect) {
	case VOLFX_NONE:
		buf[0] = buf[1] = 173;
		break;
	case VOLFX_VOLUME:
	case VOLFX_PANNING:
		/* Yeah, a bit confusing :)
		 * The display stuff makes the distinction here with
		 * a different color for panning. */
		str_from_num(2, volume, buf);
		break;
	default:
		buf[0] = cmd_table[volume_effect];
		buf[1] = hexdigits[volume];
		break;
	}

	return buf;
}

/* --------------------------------------------------------------------- */

char *get_note_string(int note, char *buf)
{
#ifndef NDEBUG
	if ((note < 0 || note > 120)
		&& !(note == NOTE_CUT
		 || note == NOTE_OFF || note == NOTE_FADE)) {
		log_appendf(4, "Note %d out of range", note);
		buf[0] = buf[1] = buf[2] = '?';
		buf[3] = 0;
		return buf;
	}
#endif

	switch (note) {
	case 0:        /* nothing */
		buf[0] = buf[1] = buf[2] = 173;
		break;
	case NOTE_CUT:
		buf[0] = buf[1] = buf[2] = 94;
		break;
	case NOTE_OFF:
		buf[0] = buf[1] = buf[2] = 205;
		break;
	case NOTE_FADE:
		/* this is sure to look "out of place" to anyone
		   ... yeah, no kidding. /storlek */
#if 0
		buf[0] = 185;
		buf[1] = 186;
		buf[2] = 185;
#else
		buf[0] = buf[1] = buf[2] = 126;
#endif
		break;
	default:
		note--;
		buf[0] = note_names[note % 12][0];
		buf[1] = note_names[note % 12][1];
		buf[2] = note / 12 + '0';
	}
	buf[3] = 0;
	return buf;
}

char *get_note_string_short(int note, char *buf)
{
#ifndef NDEBUG
	if ((note < 0 || note > 120)
		&& !(note == NOTE_CUT
		 || note == NOTE_OFF || note == NOTE_FADE)) {
		log_appendf(4, "Note %d out of range", note);
		buf[0] = buf[1] = '?';
		buf[2] = 0;
		return buf;
	}
#endif

	switch (note) {
	case 0:        /* nothing */
		buf[0] = buf[1] = 173;
		break;
	case NOTE_CUT:
		buf[0] = buf[1] = 94;
		break;
	case NOTE_OFF:
		buf[0] = buf[1] = 205;
		break;
	case NOTE_FADE:
		buf[0] = buf[1] = 126;
		break;
	default:
		note--;
		buf[0] = note_names_short[note % 12];
		buf[1] = note / 12 + '0';
	}
	buf[2] = 0;
	return buf;
}

/* --------------------------------------------------------------------- */

int kbd_get_current_octave(void)
{
	return current_octave;
}

void kbd_set_current_octave(int new_octave)
{
	new_octave = CLAMP(new_octave, 0, 8);
	current_octave = new_octave;

	/* a full screen update for one lousy letter... */
	status.flags |= NEED_UPDATE;
}

inline int kbd_char_to_99(struct key_event *k)
{
	int c;
	if (!NO_CAM_MODS(k->mod)) return -1;

	c = tolower(k->sym);
	if (c >= 'h' && c <= 'z')
		return 10 + c - 'h';

	return kbd_char_to_hex(k);

}
int kbd_char_to_hex(struct key_event *k)
{
	if (!NO_CAM_MODS(k->mod)) return -1;

	switch (k->sym) {
	case SDLK_KP_0: if (!(k->mod & KMOD_NUM)) return -1;
	case SDLK_0: return 0;
	case SDLK_KP_1: if (!(k->mod & KMOD_NUM)) return -1;
	case SDLK_1: return 1;
	case SDLK_KP_2: if (!(k->mod & KMOD_NUM)) return -1;
	case SDLK_2: return 2;
	case SDLK_KP_3: if (!(k->mod & KMOD_NUM)) return -1;
	case SDLK_3: return 3;
	case SDLK_KP_4: if (!(k->mod & KMOD_NUM)) return -1;
	case SDLK_4: return 4;
	case SDLK_KP_5: if (!(k->mod & KMOD_NUM)) return -1;
	case SDLK_5: return 5;
	case SDLK_KP_6: if (!(k->mod & KMOD_NUM)) return -1;
	case SDLK_6: return 6;
	case SDLK_KP_7: if (!(k->mod & KMOD_NUM)) return -1;
	case SDLK_7: return 7;
	case SDLK_KP_8: if (!(k->mod & KMOD_NUM)) return -1;
	case SDLK_8: return 8;
	case SDLK_KP_9: if (!(k->mod & KMOD_NUM)) return -1;
	case SDLK_9: return 9;
	case SDLK_a: return 10;
	case SDLK_b: return 11;
	case SDLK_c: return 12;
	case SDLK_d: return 13;
	case SDLK_e: return 14;
	case SDLK_f: return 15;
	default:
		return -1;
	};
}

/* --------------------------------------------------------------------- */

/* return values:
 *      < 0 = invalid note
 *        0 = clear field ('.' in qwerty)
 *    1-120 = note
 * NOTE_CUT = cut ("^^^")
 * NOTE_OFF = off ("===")
 * NOTE_FADE = fade ("~~~")
 *         i haven't really decided on how to display this.
 *         and for you people who might say 'hey, IT doesn't do that':
 *         yes it does. read the documentation. it's not in the editor,
 *         but it's in the player. */
int kbd_get_note(struct key_event *k)
{
	if (KEY_ACTIVE(pattern_edit, clear_field)) {
		return 0;
	} else if(KEY_ACTIVE(pattern_edit, note_cut)) {
		return NOTE_CUT;
	} else if(KEY_ACTIVE(pattern_edit, note_off)) {
		return NOTE_OFF;
	} else if(KEY_ACTIVE(pattern_edit, note_fade)) {
		return NOTE_FADE;
	}

	int note;

	if (!NO_CAM_MODS(k->mod)) return -1;

	if (KEY_ACTIVE(notes, note_row1_c)) {
		note = 1;
	} else if (KEY_ACTIVE(notes, note_row1_c_sharp)) {
		note = 2;
	} else if (KEY_ACTIVE(notes, note_row1_d)) {
		note = 3;
	} else if (KEY_ACTIVE(notes, note_row1_d_sharp)) {
		note = 4;
	} else if (KEY_ACTIVE(notes, note_row1_e)) {
		note = 5;
	} else if (KEY_ACTIVE(notes, note_row1_f)) {
		note = 6;
	} else if (KEY_ACTIVE(notes, note_row1_f_sharp)) {
		note = 7;
	} else if (KEY_ACTIVE(notes, note_row1_g)) {
		note = 8;
	} else if (KEY_ACTIVE(notes, note_row1_g_sharp)) {
		note = 9;
	} else if (KEY_ACTIVE(notes, note_row1_a)) {
		note = 10;
	} else if (KEY_ACTIVE(notes, note_row1_a_sharp)) {
		note = 11;
	} else if (KEY_ACTIVE(notes, note_row1_b)) {
		note = 12;
	} else if (KEY_ACTIVE(notes, note_row2_c)) {
		note = 13;
	} else if (KEY_ACTIVE(notes, note_row2_c_sharp)) {
		note = 14;
	} else if (KEY_ACTIVE(notes, note_row2_d)) {
		note = 15;
	} else if (KEY_ACTIVE(notes, note_row2_d_sharp)) {
		note = 16;
	} else if (KEY_ACTIVE(notes, note_row2_e)) {
		note = 17;
	} else if (KEY_ACTIVE(notes, note_row2_f)) {
		note = 18;
	} else if (KEY_ACTIVE(notes, note_row2_f_sharp)) {
		note = 19;
	} else if (KEY_ACTIVE(notes, note_row2_g)) {
		note = 20;
	} else if (KEY_ACTIVE(notes, note_row2_g_sharp)) {
		note = 21;
	} else if (KEY_ACTIVE(notes, note_row2_a)) {
		note = 22;
	} else if (KEY_ACTIVE(notes, note_row2_a_sharp)) {
		note = 23;
	} else if (KEY_ACTIVE(notes, note_row2_b)) {
		note = 24;
	} else if (KEY_ACTIVE(notes, note_row3_c)) {
		note = 25;
	} else if (KEY_ACTIVE(notes, note_row3_c_sharp)) {
		note = 26;
	} else if (KEY_ACTIVE(notes, note_row3_d)) {
		note = 27;
	} else if (KEY_ACTIVE(notes, note_row3_d_sharp)) {
		note = 28;
	} else if (KEY_ACTIVE(notes, note_row3_e)) {
		note = 29;
	} else {
		return -1;
	}

	note += (12 * current_octave);
	return CLAMP(note, 1, 120);
}

int kbd_get_alnum(struct key_event *k)
{
	if (k->sym >= 127)
		return 0;
	if (k->mod & KMOD_SHIFT) {
		const char shifted_digits[] = ")!@#$%^&*("; // comical profanity
		switch (k->sym) {
			case 'a': case 'b': case 'c':
			case 'd': case 'e': case 'f':
			case 'g': case 'h': case 'i':
			case 'j': case 'k': case 'l':
			case 'm': case 'n': case 'o':
			case 'p': case 'q': case 'r':
			case 's': case 't': case 'u':
			case 'v': case 'w': case 'x':
			case 'y': case 'z':
				return toupper(k->sym);
			case '0': case '1': case '2':
			case '3': case '4': case '5':
			case '6': case '7': case '8':
			case '9':
				return shifted_digits[k->sym - '0'];
			case '[': return '{';
			case ']': return '}';
			case ';': return ':';
			case '=': return '+';
			case '-': return '_';
			case '`': return '~';
			case ',': return '<';
			case '.': return '>';
			case '/': return '?';
			case '\\': return '|';
			case '\'': return '"';
			default: return k->sym; // shift + some weird key = ???
		}
	}
	return k->sym;
}

/* -------------------------------------------------- */

/* emulate SDL 1.2 style key repeat */
static int key_repeat_delay = 0;
static int key_repeat_rate = 0;
static int key_repeat_enabled = 0;
static schism_ticks_t key_repeat_next_tick = 0;

static struct key_event cached_key_event = {0};

int kbd_key_repeat_enabled(void)
{
	return key_repeat_enabled;
}

void kbd_handle_key_repeat(void)
{
	if (!key_repeat_next_tick || !key_repeat_enabled)
		return;

	const schism_ticks_t now = SCHISM_GET_TICKS();
	if (SCHISM_TICKS_PASSED(now, key_repeat_next_tick)) {
		/* handle key functions have the ability to
		 * change the values of the key_event structure.
		 *
		 * see: issue #465 */
		struct key_event kk = cached_key_event;
		handle_key(&kk);
		key_repeat_next_tick = now + key_repeat_rate;
	}
}

void kbd_cache_key_repeat(struct key_event* kk)
{
	if (!key_repeat_enabled)
		return;

	if (cached_key_event.text)
		free((uint8_t*)cached_key_event.text);

	cached_key_event = *kk;
	cached_key_event.is_repeat = 1;

	/* need to duplicate this as well */
	if (cached_key_event.text)
		cached_key_event.text = str_dup(cached_key_event.text);

	key_repeat_next_tick = SCHISM_GET_TICKS() + key_repeat_delay + key_repeat_rate;
}

void kbd_empty_key_repeat(void)
{
	if (!key_repeat_enabled)
		return;

	if (cached_key_event.text) {
		free((uint8_t*)cached_key_event.text);
		cached_key_event.text = NULL;
	}

	key_repeat_next_tick = 0;
}

void kbd_set_key_repeat(int delay, int rate)
{
	/* I don't know why this check is here but i'm keeping it to
	 * retain compatibility */
	if (delay) {
		key_repeat_delay = delay;
		key_repeat_rate = rate;
		key_repeat_enabled = 1;
	}
}

/* -------------------------------------------------- */

/* These are here for linking text input to keyboard inputs.
 * If no keyboard input can be found, then the text will
 * be sent to the already existing handlers written for the
 * original port to SDL2.
 *
 * - paper */

static struct key_event pending_keydown;
static int have_pending_keydown = 0;

void kbd_push_pending_keydown(struct key_event* kk)
{
	if (!have_pending_keydown) {
		pending_keydown = *kk;
		have_pending_keydown = 1;
	}
}

void kbd_pop_pending_keydown(const uint8_t* text)
{
	/* text is optional, but should be in CP437 if provided */
	if (have_pending_keydown) {
		pending_keydown.text = text;
		kbd_cache_key_repeat(&pending_keydown);
		handle_key(&pending_keydown);
		have_pending_keydown = 0;
	}
}

int kbd_have_pending_keydown(void)
{
	return have_pending_keydown;
}
