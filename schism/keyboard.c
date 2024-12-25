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
#include "timer.h"
#include "mem.h"
#include "str.h"

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
#define QZA(n) case SCHISM_KEYSYM_ ## n : return get_effect_number(#n [0])
QZA(a);QZA(b);QZA(c);QZA(d);QZA(e);QZA(f);QZA(g);QZA(h);QZA(i);QZA(j);QZA(k);
QZA(l);QZA(m);QZA(n);QZA(o);QZA(p);QZA(q);QZA(r);QZA(s);QZA(t);QZA(u);QZA(v);
QZA(w);QZA(x);QZA(y);QZA(z);
#undef QZA
	case SCHISM_KEYSYM_PERIOD: case SCHISM_KEYSYM_KP_PERIOD:
		return get_effect_number('.');
	case SCHISM_KEYSYM_1:
		if (!(k->mod & SCHISM_KEYMOD_SHIFT)) return -1;
	case SCHISM_KEYSYM_EXCLAIM:
		return get_effect_number('!');
	case SCHISM_KEYSYM_4:
		if (!(k->mod & SCHISM_KEYMOD_SHIFT)) return -1;
	case SCHISM_KEYSYM_DOLLAR:
		return get_effect_number('$');
	case SCHISM_KEYSYM_7:
		if (!(k->mod & SCHISM_KEYMOD_SHIFT)) return -1;
	case SCHISM_KEYSYM_AMPERSAND:
		return get_effect_number('&');

	default:
		return -1;
	};
}

/* --------------------------------------------------------------------- */

void kbd_key_translate(struct key_event *k)
{
	/* This assumes a US keyboard layout. There's no real "easy" way
	 * to solve this besides possibly using system-specific translate
	 * functions, but that's clunky and a lot of systems don't even
	 * provide that IIRC. */

	k->orig_sym = k->sym;
	if (k->mod & SCHISM_KEYMOD_SHIFT) {
		switch (k->sym) {
		case SCHISM_KEYSYM_COMMA: k->sym = SCHISM_KEYSYM_LESS; break;
		case SCHISM_KEYSYM_PERIOD: k->sym = SCHISM_KEYSYM_GREATER; break;
		case SCHISM_KEYSYM_4: k->sym = SCHISM_KEYSYM_DOLLAR; break;

		case SCHISM_KEYSYM_EQUALS: k->sym = SCHISM_KEYSYM_PLUS; break;
		case SCHISM_KEYSYM_SEMICOLON: k->sym = SCHISM_KEYSYM_COLON; break;

		case SCHISM_KEYSYM_8: k->sym = SCHISM_KEYSYM_ASTERISK; break;
		default:
			break;
		};
	}
	if (k->mod & SCHISM_KEYMOD_GUI) {
		k->mod = ((k->mod & ~SCHISM_KEYMOD_GUI)
			  | ((status.flags & META_IS_CTRL)
				 ? SCHISM_KEYMOD_CTRL : SCHISM_KEYMOD_ALT));
	}
	if ((k->mod & SCHISM_KEYMOD_MODE) && (status.flags & ALTGR_IS_ALT)) {
		/* Treat AltGr as Alt (delt) */
		k->mod = ((k->mod & ~SCHISM_KEYMOD_MODE) | SCHISM_KEYMOD_ALT);
	}
	if (k->mod & SCHISM_KEYMOD_NUM) {
		switch (k->sym) {
		case SCHISM_KEYSYM_KP_0: k->sym = SCHISM_KEYSYM_0; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_1: k->sym = SCHISM_KEYSYM_1; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_2: k->sym = SCHISM_KEYSYM_2; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_3: k->sym = SCHISM_KEYSYM_3; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_4: k->sym = SCHISM_KEYSYM_4; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_5: k->sym = SCHISM_KEYSYM_5; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_6: k->sym = SCHISM_KEYSYM_6; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_7: k->sym = SCHISM_KEYSYM_7; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_8: k->sym = SCHISM_KEYSYM_8; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_9: k->sym = SCHISM_KEYSYM_9; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_PERIOD: k->sym = SCHISM_KEYSYM_PERIOD; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_DIVIDE: k->sym = SCHISM_KEYSYM_SLASH; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_MULTIPLY: k->sym = SCHISM_KEYSYM_ASTERISK; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_MINUS: k->sym = SCHISM_KEYSYM_MINUS; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_PLUS: k->sym = SCHISM_KEYSYM_PLUS; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_ENTER: k->sym = SCHISM_KEYSYM_RETURN; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		case SCHISM_KEYSYM_KP_EQUALS: k->sym = SCHISM_KEYSYM_EQUALS; k->mod &= ~SCHISM_KEYMOD_NUM; break;
		default:
			break;
		};
	} else {
		switch (k->sym) {
		case SCHISM_KEYSYM_KP_0: k->sym = SCHISM_KEYSYM_INSERT; break;
		case SCHISM_KEYSYM_KP_4: k->sym = SCHISM_KEYSYM_LEFT; break;
		case SCHISM_KEYSYM_KP_6: k->sym = SCHISM_KEYSYM_RIGHT; break;
		case SCHISM_KEYSYM_KP_2: k->sym = SCHISM_KEYSYM_DOWN; break;
		case SCHISM_KEYSYM_KP_8: k->sym = SCHISM_KEYSYM_UP; break;

		case SCHISM_KEYSYM_KP_9: k->sym = SCHISM_KEYSYM_PAGEUP; break;
		case SCHISM_KEYSYM_KP_3: k->sym = SCHISM_KEYSYM_PAGEDOWN; break;

		case SCHISM_KEYSYM_KP_7: k->sym = SCHISM_KEYSYM_HOME; break;
		case SCHISM_KEYSYM_KP_1: k->sym = SCHISM_KEYSYM_END; break;

		case SCHISM_KEYSYM_KP_PERIOD: k->sym = SCHISM_KEYSYM_DELETE; break;

		case SCHISM_KEYSYM_KP_DIVIDE: k->sym = SCHISM_KEYSYM_SLASH; break;
		case SCHISM_KEYSYM_KP_MULTIPLY: k->sym = SCHISM_KEYSYM_ASTERISK; break;
		case SCHISM_KEYSYM_KP_MINUS: k->sym = SCHISM_KEYSYM_MINUS; break;
		case SCHISM_KEYSYM_KP_PLUS: k->sym = SCHISM_KEYSYM_PLUS; break;
		case SCHISM_KEYSYM_KP_ENTER: k->sym = SCHISM_KEYSYM_RETURN; break;
		case SCHISM_KEYSYM_KP_EQUALS: k->sym = SCHISM_KEYSYM_EQUALS; break;

		default:
			break;
		};
	}
	// do nothing
}

int numeric_key_event(struct key_event *k, int kponly)
{
	if (kponly) {
		switch (k->sym) {
		case SCHISM_KEYSYM_KP_0: return 0;
		case SCHISM_KEYSYM_KP_1: return 1;
		case SCHISM_KEYSYM_KP_2: return 2;
		case SCHISM_KEYSYM_KP_3: return 3;
		case SCHISM_KEYSYM_KP_4: return 4;
		case SCHISM_KEYSYM_KP_5: return 5;
		case SCHISM_KEYSYM_KP_6: return 6;
		case SCHISM_KEYSYM_KP_7: return 7;
		case SCHISM_KEYSYM_KP_8: return 8;
		case SCHISM_KEYSYM_KP_9: return 9;
		default:
			break;
		};
		return -1;
	}

	switch (k->sym) {
	case SCHISM_KEYSYM_0: case SCHISM_KEYSYM_KP_0: return 0;
	case SCHISM_KEYSYM_1: case SCHISM_KEYSYM_KP_1: return 1;
	case SCHISM_KEYSYM_2: case SCHISM_KEYSYM_KP_2: return 2;
	case SCHISM_KEYSYM_3: case SCHISM_KEYSYM_KP_3: return 3;
	case SCHISM_KEYSYM_4: case SCHISM_KEYSYM_KP_4: return 4;
	case SCHISM_KEYSYM_5: case SCHISM_KEYSYM_KP_5: return 5;
	case SCHISM_KEYSYM_6: case SCHISM_KEYSYM_KP_6: return 6;
	case SCHISM_KEYSYM_7: case SCHISM_KEYSYM_KP_7: return 7;
	case SCHISM_KEYSYM_8: case SCHISM_KEYSYM_KP_8: return 8;
	case SCHISM_KEYSYM_9: case SCHISM_KEYSYM_KP_9: return 9;
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
		buf[0] = buf[1] = '\xAD';
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
		buf[0] = buf[1] = buf[2] = '\xAD';
		break;
	case NOTE_CUT:
		buf[0] = buf[1] = buf[2] = '\x5E';
		break;
	case NOTE_OFF:
		buf[0] = buf[1] = buf[2] = '\xCD';
		break;
	case NOTE_FADE:
		/* this is sure to look "out of place" to anyone
		   ... yeah, no kidding. /storlek */
#if 0
		buf[0] = 185;
		buf[1] = 186;
		buf[2] = 185;
#else
		buf[0] = buf[1] = buf[2] = '\x7E';
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
		buf[0] = buf[1] = '\xAD';
		break;
	case NOTE_CUT:
		buf[0] = buf[1] = '\x5E';
		break;
	case NOTE_OFF:
		buf[0] = buf[1] = '\xCD';
		break;
	case NOTE_FADE:
		buf[0] = buf[1] = '\x7E';
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

int kbd_char_to_99(struct key_event *k)
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

	switch (k->orig_sym) {
	case SCHISM_KEYSYM_KP_0: if (!(k->mod & SCHISM_KEYMOD_NUM)) return -1;
	case SCHISM_KEYSYM_0: return 0;
	case SCHISM_KEYSYM_KP_1: if (!(k->mod & SCHISM_KEYMOD_NUM)) return -1;
	case SCHISM_KEYSYM_1: return 1;
	case SCHISM_KEYSYM_KP_2: if (!(k->mod & SCHISM_KEYMOD_NUM)) return -1;
	case SCHISM_KEYSYM_2: return 2;
	case SCHISM_KEYSYM_KP_3: if (!(k->mod & SCHISM_KEYMOD_NUM)) return -1;
	case SCHISM_KEYSYM_3: return 3;
	case SCHISM_KEYSYM_KP_4: if (!(k->mod & SCHISM_KEYMOD_NUM)) return -1;
	case SCHISM_KEYSYM_4: return 4;
	case SCHISM_KEYSYM_KP_5: if (!(k->mod & SCHISM_KEYMOD_NUM)) return -1;
	case SCHISM_KEYSYM_5: return 5;
	case SCHISM_KEYSYM_KP_6: if (!(k->mod & SCHISM_KEYMOD_NUM)) return -1;
	case SCHISM_KEYSYM_6: return 6;
	case SCHISM_KEYSYM_KP_7: if (!(k->mod & SCHISM_KEYMOD_NUM)) return -1;
	case SCHISM_KEYSYM_7: return 7;
	case SCHISM_KEYSYM_KP_8: if (!(k->mod & SCHISM_KEYMOD_NUM)) return -1;
	case SCHISM_KEYSYM_8: return 8;
	case SCHISM_KEYSYM_KP_9: if (!(k->mod & SCHISM_KEYMOD_NUM)) return -1;
	case SCHISM_KEYSYM_9: return 9;
	case SCHISM_KEYSYM_a: return 10;
	case SCHISM_KEYSYM_b: return 11;
	case SCHISM_KEYSYM_c: return 12;
	case SCHISM_KEYSYM_d: return 13;
	case SCHISM_KEYSYM_e: return 14;
	case SCHISM_KEYSYM_f: return 15;
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
	int note;

	if (!NO_CAM_MODS(k->mod)) return -1;

	if (k->sym == SCHISM_KEYSYM_KP_1 || k->sym == SCHISM_KEYSYM_KP_PERIOD)
		if (!(k->mod & SCHISM_KEYMOD_NUM)) return -1;

	switch (k->scancode) {
	case SCHISM_SCANCODE_GRAVE:
		if (k->mod & SCHISM_KEYMOD_SHIFT) return NOTE_FADE;
	case SCHISM_SCANCODE_NONUSHASH: /* for delt */
	case SCHISM_SCANCODE_KP_HASH:
		return NOTE_OFF;
	case SCHISM_SCANCODE_1:
		return NOTE_CUT;
	case SCHISM_SCANCODE_PERIOD:
		return 0; /* clear */
	case SCHISM_SCANCODE_Z: note = 1; break;
	case SCHISM_SCANCODE_S: note = 2; break;
	case SCHISM_SCANCODE_X: note = 3; break;
	case SCHISM_SCANCODE_D: note = 4; break;
	case SCHISM_SCANCODE_C: note = 5; break;
	case SCHISM_SCANCODE_V: note = 6; break;
	case SCHISM_SCANCODE_G: note = 7; break;
	case SCHISM_SCANCODE_B: note = 8; break;
	case SCHISM_SCANCODE_H: note = 9; break;
	case SCHISM_SCANCODE_N: note = 10; break;
	case SCHISM_SCANCODE_J: note = 11; break;
	case SCHISM_SCANCODE_M: note = 12; break;

	case SCHISM_SCANCODE_Q: note = 13; break;
	case SCHISM_SCANCODE_2: note = 14; break;
	case SCHISM_SCANCODE_W: note = 15; break;
	case SCHISM_SCANCODE_3: note = 16; break;
	case SCHISM_SCANCODE_E: note = 17; break;
	case SCHISM_SCANCODE_R: note = 18; break;
	case SCHISM_SCANCODE_5: note = 19; break;
	case SCHISM_SCANCODE_T: note = 20; break;
	case SCHISM_SCANCODE_6: note = 21; break;
	case SCHISM_SCANCODE_Y: note = 22; break;
	case SCHISM_SCANCODE_7: note = 23; break;
	case SCHISM_SCANCODE_U: note = 24; break;
	case SCHISM_SCANCODE_I: note = 25; break;
	case SCHISM_SCANCODE_9: note = 26; break;
	case SCHISM_SCANCODE_O: note = 27; break;
	case SCHISM_SCANCODE_0: note = 28; break;
	case SCHISM_SCANCODE_P: note = 29; break;

	default: return -1;
	};
	note += (12 * current_octave);
	return CLAMP(note, 1, 120);
}

int kbd_get_alnum(struct key_event *k)
{
	if (k->sym >= 127)
		return 0;
	if (k->mod & SCHISM_KEYMOD_SHIFT) {
		const char shifted_digits[] = ")!@#$%^&*("; // comical profanity
		switch (k->orig_sym) {
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
			// maybe key_translate handled it
			default: return k->sym; // shift + some weird key = ???
		}
	}

	// hm
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

	const schism_ticks_t now = timer_ticks();
	if (timer_ticks_passed(now, key_repeat_next_tick)) {
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

	key_repeat_next_tick = timer_ticks() + key_repeat_delay + key_repeat_rate;
}

void kbd_empty_key_repeat(void)
{
	if (!key_repeat_enabled)
		return;

	if (cached_key_event.text) {
		free((void *)cached_key_event.text);
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
