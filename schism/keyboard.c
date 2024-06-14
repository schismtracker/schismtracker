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

// XXX stupid magic numbers...
void kbd_sharp_flat_toggle(int e)
{
	switch (e) {
	case -1:
		kbd_sharp_flat_toggle(!!(note_names == note_names_up));
		break;
	case 1:
		status.flags |= ACCIDENTALS_AS_FLATS;
		status_text_flash("Displaying accidentals as flats (b)");
		note_names = note_names_down;
		note_names_short = note_names_short_down;
		break;
	default: /* case 0... */
		status.flags &= ~ACCIDENTALS_AS_FLATS;
		status_text_flash("Displaying accidentals as sharps (#)");
		note_names = note_names_up;
		note_names_short = note_names_short_up;
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

void key_translate(struct key_event *k)
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
		numtostr(2, volume, buf);
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
	int note;

	if (!NO_CAM_MODS(k->mod)) return -1;

	if (k->orig_sym == SDLK_KP_PERIOD && k->sym == SDLK_PERIOD) {
		/* lots of systems map an outside scancode for these;
		 * we may need to simply ignore scancodes > 256
		 * but i want a narrow change for this for now
		 * until it is certain we need more...
		 */
		return 0;
	}

	if (k->sym == SDLK_KP_1 || k->sym == SDLK_KP_PERIOD)
		if (!(k->mod & KMOD_NUM)) return -1;

	switch (k->scancode) {
	case SDL_SCANCODE_GRAVE:
		if (k->mod & KMOD_SHIFT) return NOTE_FADE;
	case SDL_SCANCODE_NONUSHASH: /* for delt */
	case SDL_SCANCODE_KP_HASH:
		return NOTE_OFF;
	case SDL_SCANCODE_1:
		return NOTE_CUT;
	case SDL_SCANCODE_PERIOD:
		return 0; /* clear */
	case SDL_SCANCODE_Z: note = 1; break;
	case SDL_SCANCODE_S: note = 2; break;
	case SDL_SCANCODE_X: note = 3; break;
	case SDL_SCANCODE_D: note = 4; break;
	case SDL_SCANCODE_C: note = 5; break;
	case SDL_SCANCODE_V: note = 6; break;
	case SDL_SCANCODE_G: note = 7; break;
	case SDL_SCANCODE_B: note = 8; break;
	case SDL_SCANCODE_H: note = 9; break;
	case SDL_SCANCODE_N: note = 10; break;
	case SDL_SCANCODE_J: note = 11; break;
	case SDL_SCANCODE_M: note = 12; break;

	case SDL_SCANCODE_Q: note = 13; break;
	case SDL_SCANCODE_2: note = 14; break;
	case SDL_SCANCODE_W: note = 15; break;
	case SDL_SCANCODE_3: note = 16; break;
	case SDL_SCANCODE_E: note = 17; break;
	case SDL_SCANCODE_R: note = 18; break;
	case SDL_SCANCODE_5: note = 19; break;
	case SDL_SCANCODE_T: note = 20; break;
	case SDL_SCANCODE_6: note = 21; break;
	case SDL_SCANCODE_Y: note = 22; break;
	case SDL_SCANCODE_7: note = 23; break;
	case SDL_SCANCODE_U: note = 24; break;
	case SDL_SCANCODE_I: note = 25; break;
	case SDL_SCANCODE_9: note = 26; break;
	case SDL_SCANCODE_O: note = 27; break;
	case SDL_SCANCODE_0: note = 28; break;
	case SDL_SCANCODE_P: note = 29; break;

	default: return -1;
	};
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

#define DEFAULT_KEY_REPEAT_DELAY 500
#define DEFAULT_KEY_REPEAT_RATE  30

/* emulate SDL 1.2 style key repeat */
static int key_repeat_delay = DEFAULT_KEY_REPEAT_DELAY, key_repeat_rate = DEFAULT_KEY_REPEAT_RATE;
static uint64_t key_repeat_next_tick = 0;

static struct key_event cached_key_event = {0};

/* use 64-bit functions only if they're available! */
#if SDL_VERSION_ATLEAST(2, 0, 18)
#define SCHISM_TICKS_PASSED(a, b) ((a) >= (b))
#define SCHISM_GET_TICKS SDL_GetTicks64
#else
#define SCHISM_TICKS_PASSED(a, b) (SDL_TICKS_PASSED(a, b))
#define SCHISM_GET_TICKS SDL_GetTicks
#endif

void handle_key_repeat(void) {
	if (!key_repeat_next_tick)
		return;

	const uint64_t now = SCHISM_GET_TICKS();
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

void cache_key_repeat(struct key_event* kk) {
	if (cached_key_event.text)
		free((uint8_t*)cached_key_event.text);

	cached_key_event = *kk;
	cached_key_event.is_repeat = 1;

	/* need to duplicate this as well */
	if (cached_key_event.text)
		cached_key_event.text = str_dup(cached_key_event.text);

	key_repeat_next_tick = SCHISM_GET_TICKS() + key_repeat_delay + key_repeat_rate;
}

void empty_key_repeat(void) {
	if (cached_key_event.text) {
		free((uint8_t*)cached_key_event.text);
		cached_key_event.text = NULL;
	}

	key_repeat_next_tick = 0;
}

void set_key_repeat(int delay, int rate) {
	if (delay) {
		key_repeat_delay = delay;
		key_repeat_rate = rate;
	} else {
		key_repeat_delay = DEFAULT_KEY_REPEAT_DELAY;
		key_repeat_rate = DEFAULT_KEY_REPEAT_RATE;
	}
}
