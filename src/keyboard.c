#include "headers.h"

#include <SDL.h>
#include <ctype.h>

#include "it.h"
#include "song.h"
#include "page.h"

/* --------------------------------------------------------------------- */

/* There are a few problems with alternate layouts that I haven't quite
 * figured out what to do with yet. For instance, with the dvorak
 * keyboard it's not possible to insert the 'w' effect because it gets
 * interpreted as the toggle mask key. :( */

#define QWERTY 1
#define DVORAK 2

#define KEY_LAYOUT QWERTY

#if KEY_LAYOUT == QWERTY
const char *note_trans = "zsxdcvgbhnjmq2w3er5t6y7ui9o0p"
        "\0"    /* to separate the non-keyboard stuff */
        "."     /* clear */
        "1"     /* note cut */
        "`"     /* note off */
        "~"     /* note fade (NEW KEY) */
        ";"     /* prev ins */
        "'"     /* next ins */
        ","     /* toggle mask */
        "`"     /* volume/panning */
        "4"     /* play note */
        "8"     /* play row */
;
#elif KEY_LAYOUT == DVORAK
const char *note_trans = ";oqejkixdbhm'2,3.p5y6f7gc9r0l"
        "\0"    /* to separate the non-keyboard stuff */
        "v"     /* clear */
        "1"     /* note cut */
        "`"     /* note off */
        "~"     /* note fade */
        "s"     /* prev ins */
        "-"     /* next ins */
        "w"     /* toggle mask */
        "`"     /* volume/panning */
        "4"     /* play note */
        "8"     /* play row */
;
#else
#error "Unknown keyboard type."
#endif

/* --------------------------------------------------------------------- */

static const char *note_names[12] = {
        "C-", "C#", "D-", "D#", "E-", "F-",
        "F#", "G-", "G#", "A-", "A#", "B-"
};

static const char note_names_short[12] = "cCdDefFgGaAb";

/* --------------------------------------------------------------------- */

static int current_octave = 4;

/* --------------------------------------------------------------------- */

/* this is used in a couple of other places... maybe it should be in some
 * general-stuff file? */
const char hexdigits[16] = "0123456789ABCDEF";

/* extra non-IT effects:
 *         '!' = volume '#' = modcmdex '$' = keyoff
 *         '%' = xfineportaupdown '&' = setenvposition */
static const char effects[33] = ".JFEGHLKRXODB!CQATI#SMNVW$UY%P&Z";

/* --------------------------------------------------------------------- */

char get_effect_char(int effect)
{
        if (effect < 0 || effect > 31) {
                log_appendf(4, "get_effect_char: effect %d out of range",
                            effect);
                return '?';
        }
        return effects[effect];
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
                return -1;
        }

        ptr = strchr(effects, effect);
        return ptr ? ptr - effects : -1;
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
        case VOL_EFFECT_NONE:
                buf[0] = buf[1] = 173;
                break;
        case VOL_EFFECT_VOLUME:
        case VOL_EFFECT_PANNING:
                /* Yeah, a bit confusing :)
                 * The display stuff makes the distinction here with
                 * a different color for panning. */
                numtostr_2(volume, buf);
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
                buf[0] = buf[1] = buf[2] = 126;
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

int kbd_get_current_octave()
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
inline int kbd_get_note(char c)
{
        int pos;
        int note;
        const char *ptr;

        if (c == 0)
                return -1;
        if (c == note_trans[NOTE_TRANS_CLEAR])
                return 0;
        if (c == note_trans[NOTE_TRANS_NOTE_OFF])
                return NOTE_OFF;
        if (c == note_trans[NOTE_TRANS_NOTE_CUT])
                return NOTE_CUT;
        if (c == note_trans[NOTE_TRANS_NOTE_FADE])
                return NOTE_FADE;

        ptr = strchr(note_trans, c);
        if (ptr == NULL)
                return -1;
        pos = ptr - note_trans + 1;
        note = 12 * current_octave + pos;
        return CLAMP(note, 1, 120);
}

int kbd_play_note(char c)
{
        char buf[4];
        int note = kbd_get_note(c);

        if (note <= 0 || note > 120)
                return 0;

        if (song_is_instrument_mode()
            && status.current_page != PAGE_SAMPLE_LIST) {
                song_instrument *ins =
                        song_get_instrument(instrument_get_current(),
                                            NULL);
                if (!(ins->sample_map[note] && ins->note_map[note]))
                        return 1;
                printf("kbd_play_note: instrument #%d, note %s (0x%02x)\n",
                       instrument_get_current(),
                       get_note_string(note, buf), note);
        } else {
                printf("kbd_play_note: sample #%d, note %s (0x%02x)\n",
                       sample_get_current(), get_note_string(note, buf),
                       note);
        }
        return 1;
}
