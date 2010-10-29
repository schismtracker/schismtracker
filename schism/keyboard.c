/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010 Storlek
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
        if (k->mod & KMOD_META) {
                k->mod = ((k->mod & ~KMOD_META)
                          | ((status.flags & META_IS_CTRL)
                             ? KMOD_CTRL : KMOD_ALT));
        }
        if ((k->mod & KMOD_MODE) && (status.flags & ALTGR_IS_ALT)) {
                /* Treat AltGr as Alt (delt) */
                k->mod = ((k->mod & ~KMOD_MODE) | KMOD_ALT);
        }
        if (k->mod & KMOD_NUM) {
                switch (k->sym) {
                case SDLK_KP0: k->sym = SDLK_0; k->mod &= ~KMOD_NUM; break;
                case SDLK_KP1: k->sym = SDLK_1; k->mod &= ~KMOD_NUM; break;
                case SDLK_KP2: k->sym = SDLK_2; k->mod &= ~KMOD_NUM; break;
                case SDLK_KP3: k->sym = SDLK_3; k->mod &= ~KMOD_NUM; break;
                case SDLK_KP4: k->sym = SDLK_4; k->mod &= ~KMOD_NUM; break;
                case SDLK_KP5: k->sym = SDLK_5; k->mod &= ~KMOD_NUM; break;
                case SDLK_KP6: k->sym = SDLK_6; k->mod &= ~KMOD_NUM; break;
                case SDLK_KP7: k->sym = SDLK_7; k->mod &= ~KMOD_NUM; break;
                case SDLK_KP8: k->sym = SDLK_8; k->mod &= ~KMOD_NUM; break;
                case SDLK_KP9: k->sym = SDLK_9; k->mod &= ~KMOD_NUM; break;
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
                case SDLK_KP0: k->sym = SDLK_INSERT; break;
                case SDLK_KP4: k->sym = SDLK_LEFT; break;
                case SDLK_KP6: k->sym = SDLK_RIGHT; break;
                case SDLK_KP2: k->sym = SDLK_DOWN; break;
                case SDLK_KP8: k->sym = SDLK_UP; break;

                case SDLK_KP9: k->sym = SDLK_PAGEUP; break;
                case SDLK_KP3: k->sym = SDLK_PAGEDOWN; break;

                case SDLK_KP7: k->sym = SDLK_HOME; break;
                case SDLK_KP1: k->sym = SDLK_END; break;

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

        if (k->sym == k->orig_sym) {
                switch (k->sym) {
                case SDLK_RETURN: k->unicode = '\r'; break;
                default:
                        if (k->is_synthetic != 3) {
                                /* "un" unicode it */
                                k->unicode = char_unicode_to_cp437(k->unicode);
                        }
                };
                return;
        }

        switch (k->sym) {
        case SDLK_SLASH: k->unicode = (k->mod & KMOD_SHIFT) ? '?' : '/'; break;
        case SDLK_ASTERISK: k->unicode = '*'; break;
        case SDLK_MINUS: k->unicode = (k->mod & KMOD_SHIFT) ? '_' : '-'; break;
        case SDLK_PLUS: k->unicode = '+'; break;
        case SDLK_RETURN: k->unicode = '\r'; break;
        case SDLK_EQUALS: k->unicode = (k->mod & KMOD_SHIFT) ? '+' : '='; break;
        case SDLK_PERIOD: k->unicode = (k->mod & KMOD_SHIFT) ? '>' : '.'; break;
        case SDLK_0: k->unicode = (k->mod & KMOD_SHIFT) ? ')' : '0'; break;
        case SDLK_1: k->unicode = (k->mod & KMOD_SHIFT) ? '!' : '1'; break;
        case SDLK_2: k->unicode = (k->mod & KMOD_SHIFT) ? '@' : '2'; break;
        case SDLK_3: k->unicode = (k->mod & KMOD_SHIFT) ? '#' : '3'; break;
        case SDLK_4: k->unicode = (k->mod & KMOD_SHIFT) ? '$' : '4'; break;
        case SDLK_5: k->unicode = (k->mod & KMOD_SHIFT) ? '%' : '5'; break;
        case SDLK_6: k->unicode = (k->mod & KMOD_SHIFT) ? '^' : '6'; break;
        case SDLK_7: k->unicode = (k->mod & KMOD_SHIFT) ? '&' : '7'; break;
        case SDLK_8: k->unicode = (k->mod & KMOD_SHIFT) ? '*' : '8'; break;
        case SDLK_9: k->unicode = (k->mod & KMOD_SHIFT) ? '(' : '9'; break;
        default:
                break;
        };
}

int numeric_key_event(struct key_event *k, int kponly)
{
        if (kponly) {
                switch (k->orig_sym) {
                case SDLK_KP0: return 0;
                case SDLK_KP1: return 1;
                case SDLK_KP2: return 2;
                case SDLK_KP3: return 3;
                case SDLK_KP4: return 4;
                case SDLK_KP5: return 5;
                case SDLK_KP6: return 6;
                case SDLK_KP7: return 7;
                case SDLK_KP8: return 8;
                case SDLK_KP9: return 9;
                default:
                        break;
                };
                return -1;
        }

        if (k->unicode >= '0' && k->unicode <= '9')
                return k->unicode - '0';

        switch (k->orig_sym) {
        case SDLK_0: case SDLK_KP0: return 0;
        case SDLK_1: case SDLK_KP1: return 1;
        case SDLK_2: case SDLK_KP2: return 2;
        case SDLK_3: case SDLK_KP3: return 3;
        case SDLK_4: case SDLK_KP4: return 4;
        case SDLK_5: case SDLK_KP5: return 5;
        case SDLK_6: case SDLK_KP6: return 6;
        case SDLK_7: case SDLK_KP7: return 7;
        case SDLK_8: case SDLK_KP8: return 8;
        case SDLK_9: case SDLK_KP9: return 9;
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
                snprintf(buf, 4, "%.2s%.1d", note_names[note % 12],
                         note / 12);
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

        c = tolower(k->unicode ?: k->sym);
        if (c >= 'h' && c <= 'z')
                return 10 + c - 'h';

        return kbd_char_to_hex(k);

}
int kbd_char_to_hex(struct key_event *k)
{
        if (!NO_CAM_MODS(k->mod)) return -1;

        if (k->unicode == '0') return 0;
        if (k->unicode == '1') return 1;
        if (k->unicode == '2') return 2;
        if (k->unicode == '3') return 3;
        if (k->unicode == '4') return 4;
        if (k->unicode == '5') return 5;
        if (k->unicode == '6') return 6;
        if (k->unicode == '7') return 7;
        if (k->unicode == '8') return 8;
        if (k->unicode == '9') return 9;
        if (k->unicode == 'a' || k->unicode == 'A') return 10;
        if (k->unicode == 'b' || k->unicode == 'B') return 11;
        if (k->unicode == 'c' || k->unicode == 'C') return 12;
        if (k->unicode == 'd' || k->unicode == 'D') return 13;
        if (k->unicode == 'e' || k->unicode == 'E') return 14;
        if (k->unicode == 'f' || k->unicode == 'F') return 15;

        switch (k->sym) {
        case SDLK_KP0: if (!(k->mod & KMOD_NUM)) return -1;
        case SDLK_0: return 0;
        case SDLK_KP1: if (!(k->mod & KMOD_NUM)) return -1;
        case SDLK_1: return 1;
        case SDLK_KP2: if (!(k->mod & KMOD_NUM)) return -1;
        case SDLK_2: return 2;
        case SDLK_KP3: if (!(k->mod & KMOD_NUM)) return -1;
        case SDLK_3: return 3;
        case SDLK_KP4: if (!(k->mod & KMOD_NUM)) return -1;
        case SDLK_4: return 4;
        case SDLK_KP5: if (!(k->mod & KMOD_NUM)) return -1;
        case SDLK_5: return 5;
        case SDLK_KP6: if (!(k->mod & KMOD_NUM)) return -1;
        case SDLK_6: return 6;
        case SDLK_KP7: if (!(k->mod & KMOD_NUM)) return -1;
        case SDLK_7: return 7;
        case SDLK_KP8: if (!(k->mod & KMOD_NUM)) return -1;
        case SDLK_8: return 8;
        case SDLK_KP9: if (!(k->mod & KMOD_NUM)) return -1;
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
inline int kbd_get_note(struct key_event *k)
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

        switch (key_scancode_lookup(k->scancode, k->sym)) {
        case SDLK_BACKQUOTE:
                if (k->mod & KMOD_SHIFT) return NOTE_FADE;
        case SDLK_HASH: /* for delt */
                return NOTE_OFF;
        case SDLK_KP1:
                if (!(k->mod & KMOD_NUM)) return -1;
        case SDLK_1:
                return NOTE_CUT;
        case SDLK_KP_PERIOD:
                if (!(k->mod & KMOD_NUM)) return -1;
        case SDLK_PERIOD:
                return 0; /* clear */
        case SDLK_z: note = 1; break;
        case SDLK_s: note = 2; break;
        case SDLK_x: note = 3; break;
        case SDLK_d: note = 4; break;
        case SDLK_c: note = 5; break;
        case SDLK_v: note = 6; break;
        case SDLK_g: note = 7; break;
        case SDLK_b: note = 8; break;
        case SDLK_h: note = 9; break;
        case SDLK_n: note = 10; break;
        case SDLK_j: note = 11; break;
        case SDLK_m: note = 12; break;

        case SDLK_q: note = 13; break;
        case SDLK_2: note = 14; break;
        case SDLK_w: note = 15; break;
        case SDLK_3: note = 16; break;
        case SDLK_e: note = 17; break;
        case SDLK_r: note = 18; break;
        case SDLK_5: note = 19; break;
        case SDLK_t: note = 20; break;
        case SDLK_6: note = 21; break;
        case SDLK_y: note = 22; break;
        case SDLK_7: note = 23; break;
        case SDLK_u: note = 24; break;
        case SDLK_i: note = 25; break;
        case SDLK_9: note = 26; break;
        case SDLK_o: note = 27; break;
        case SDLK_0: note = 28; break;
        case SDLK_p: note = 29; break;

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
                        case 'a'...'z': return toupper(k->sym);
                        case '0'...'9': return shifted_digits[k->sym - '0'];
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

/* --------------------------------------------------------------------- */

static int keydelay = SDL_DEFAULT_REPEAT_DELAY, keyrate = SDL_DEFAULT_REPEAT_INTERVAL;

void set_key_repeat(int delay, int rate)
{
        /* save these for later */
        if (delay) {
                keydelay = delay;
                keyrate = rate;
        }
        SDL_EnableKeyRepeat(keydelay, keyrate);
}

