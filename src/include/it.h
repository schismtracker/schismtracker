#ifndef _IT_H
#define _IT_H

#include <SDL.h>
#include <stdio.h>

#include "util.h"
#include "_decl.h"

/* --------------------------------------------------------------------- */
/* preprocessor stuff */

#define SDL_ToggleCursor() SDL_ShowCursor(!SDL_ShowCursor(-1))

/* --------------------------------------------------------------------- */
/* structs 'n enums */

/* tracker_status dialog_types */
enum {
        DIALOG_NONE = (0),      /* 0000 0000 */
        DIALOG_MENU = (1 << 0), /* 0000 0001 */
        DIALOG_MAIN_MENU = (DIALOG_MENU | (1 << 1)),    /* 0000 0011 */
        DIALOG_SUBMENU = (DIALOG_MENU | (1 << 2)),      /* 0000 0101 */
        DIALOG_BOX = (1 << 3),  /* 0000 1000 */
        DIALOG_OK = (DIALOG_BOX | (1 << 4)),    /* 0001 1000 */
        DIALOG_OK_CANCEL = (DIALOG_BOX | (1 << 5)),     /* 0010 1000 */
        /* yes/no technically has a cancel as well, i.e. the escape key */
        DIALOG_YES_NO = (DIALOG_BOX | (1 << 6)),        /* 0100 1000 */
        DIALOG_CUSTOM = (DIALOG_BOX | (1 << 7)),        /* 1000 1000 */
};

/* tracker_status flags */
enum {
        SAMPLE_CHANGED = (1 << 0),
        INSTRUMENT_CHANGED = (1 << 1),
        DIR_MODULES_CHANGED = (1 << 2),
        DIR_SAMPLES_CHANGED = (1 << 3),
        DIR_INSTRUMENTS_CHANGED = (1 << 4),

        /* if this flag is set, the screen will be redrawn */
        NEED_UPDATE = (1 << 5),

        /* these refer to the window's state.
         * (they're rather useless on the console ;) */
        IS_FOCUSED = (1 << 6),
        IS_VISIBLE = (1 << 7),
        WM_AVAILABLE = (1 << 8),

        /* if this is set, some stuff behaves differently
         * (grep the source files for what stuff ;) */
        CLASSIC_MODE = (1 << 9),
};

/* note! TIME_PLAYBACK is only for internal calculations
 * -- don't use it directly */
enum tracker_time_display {
        TIME_OFF, TIME_PLAY_ELAPSED, TIME_PLAY_CLOCK, TIME_PLAY_OFF,
                TIME_ELAPSED, TIME_CLOCK, TIME_PLAYBACK
};

struct tracker_status {
        int current_page;
        int previous_page;
        int current_help_index;
        int dialog_type;        /* one of the DIALOG_* constants above */
        int flags;
        enum tracker_time_display time_display;
        SDLKey last_keysym;
};


struct log_line {
        int color;
        const char *text;
        /* Generally, if the buffer for a string was malloc'ed, this
         * flag should be set, so the log knows it's supposed to free it
         * when it scrolls out of view. However if a screen is changing
         * a text pointer after adding it to the log, this should *not*
         * be set so that the "owner" page doesn't end up dereferencing
         * an invalid pointer later on. (In this case, the page NEEDS to
         * set the text pointer to some constant value, for example "",
         * so that the log page doesn't dereference an invalid pointer
         * on redraw. Kind of a hack, but this is not likely to come up,
         * anyway.)
         * 
         * In short, set this if the string was malloc'ed, and don't try
         * doing any weird stuff like modifying the text pointer after
         * appending it to the message log unless you really understood
         * that last paragraph. */
        int must_free;
};

/* hahaaaa.... i just realized this takes 69 bytes */
struct it_palette {
        char name[21];
        byte colors[16][3];
};

/* this is for note_trans */
enum {
        NOTE_TRANS_CLEAR = (30),
        NOTE_TRANS_NOTE_CUT,
        NOTE_TRANS_NOTE_OFF,
        NOTE_TRANS_NOTE_FADE,
        NOTE_TRANS_PREV_INS,
        NOTE_TRANS_NEXT_INS,
        NOTE_TRANS_TOGGLE_MASK,
        NOTE_TRANS_VOL_PAN_SWITCH,
};

/* The order of these is important!
 * If this enum gets edited, also change the ordering in the call to
 * create_help.sh in the Makefile. */
enum {
        HELP_GLOBAL,
        HELP_PATTERN_EDITOR,
        HELP_SAMPLE_LIST,
        HELP_INSTRUMENT_LIST,
        HELP_INFO_PAGE,
        HELP_MESSAGE_EDITOR,
        HELP_ORDERLIST_PANNING,
        HELP_ORDERLIST_VOLUME,
        HELP_NUM_ITEMS
};

/* --------------------------------------------------------------------- */

DECL_BEGIN();

/* --------------------------------------------------------------------- */
/* global crap */

extern SDL_Surface *screen;
extern struct tracker_status status;
extern byte *font_data; /* ... which is 2048 bytes */
extern struct it_palette palettes[];
extern int current_palette;

extern const char hexdigits[16];        /* in keyboard.c at the moment */

/* this used to just translate keys to notes, but it's sort of become the
 * keyboard map... perhaps i should rename it. */
extern const char *note_trans;  /* keyboard.c */

/* currently in page_vars.c */
extern char dir_modules[], dir_samples[], dir_instruments[];

extern char *help_text_pointers[HELP_NUM_ITEMS];

/* --------------------------------------------------------------------- */
/* util functions */

/* essentially the same as sprintf(buf, "%02d", n),
 * just without the overhead */

static inline char *numtostr_2(int n, char *buf)
{
        buf[2] = 0;
        buf[1] = '0' + n / 1 % 10;
        buf[0] = '0' + n / 10 % 10;
        return buf;
}

static inline char *numtostr_3(int n, char *buf)
{
        buf[3] = 0;
        buf[2] = '0' + n / 1 % 10;
        buf[1] = '0' + n / 10 % 10;
        buf[0] = '0' + n / 100 % 10;
        return buf;
}

static inline char *numtostr_7(int n, char *buf)
{
        buf[7] = 0;
        buf[6] = '0' + n / 1 % 10;
        buf[5] = '0' + n / 10 % 10;
        buf[4] = '0' + n / 100 % 10;
        buf[3] = '0' + n / 1000 % 10;
        buf[2] = '0' + n / 10000 % 10;
        buf[1] = '0' + n / 100000 % 10;
        buf[0] = '0' + n / 1000000 % 10;
        return buf;
}

static inline char *numtostr_n(int n, char *buf)
{
        /* bah */
        sprintf(buf, "%d", n);
        return buf;
}

/* --------------------------------------------------------------------- */
/* text functions */

/* these are sort of for single-line text entries. */
void text_add_char(char *text, char c, int *cursor_pos, int max_length);
void text_delete_char(char *text, int *cursor_pos, int max_length);
void text_delete_next_char(char *text, int *cursor_pos, int max_length);

static inline char unicode_to_ascii(Uint16 unicode)
{
        return ((unicode & 0xff80) ? 0 : (unicode & 0x7f));
}

/* --------------------------------------------------------------------- */
/* drawing functions */

/* h4rdc0r3 drawing - not really useful except in a few places.
 * note: these need to be called with the surface locked! */
void putpixel(SDL_Surface * surface, int x, int y, Uint32 c);
/* always use putpixel_screen(...) instead of putpixel(screen, ...) */
void putpixel_screen(int x, int y, Uint32 c);
void draw_line(SDL_Surface * surface, int xs, int ys, int xe, int ye,
               Uint32 c);
/* same deal here as with putpixel_screen */
void draw_line_screen(int xs, int ys, int xe, int ye, Uint32 c);
void draw_fill_rect(SDL_Rect * rect, Uint32 c);

/* character drawing (in a separate header so they're easier to find) */
#include "draw-char.h"

/* the sample number indicates what position to cache it in, zero being
 * theoretically for the currently active sample in the sample library
 * and 1-99 for the respective samples in the sample list.
 * clear_cached_waveform erases the cached image of the sample data.
 * this should be called (say) after the sample changes. the rect passed
 * to draw_sample_data tells where to draw it (duh ;) */
struct _song_sample;
void draw_sample_data(SDL_Rect * rect, struct _song_sample *sample,
                      int number);
void clear_cached_waveform(int sample);
void clear_all_cached_waveforms(void);

/* --------------------------------------------------------------------- */
/* page functions */

/* anything can use these */
void set_page(int new_page);
/* (there's no get_page -- just use status.current_page) */

/* these should only be called from main */
void load_pages(void);  /* called once at start of program */
void playback_update(void);     /* once per cycle */
void handle_key(SDL_keysym * k);        /* whenever there's a keypress ;) */

/* this should only be called from main.
 * anywhere else, use status.flags |= NEED_UPDATE instead. */
void redraw_screen(void);

/* set up in main.c to be called from song.cc whenever the song changes */
void main_song_changed_cb(void);

/* --------------------------------------------------------------------- */
/* colors and fonts */

int font_load(const char *filename);
void palette_set(byte colors[16][3]);
void palette_load_preset(int palette_index);

/* mostly for the itf editor */
int font_save(const char *filename);

/* if bank = 0, set the normal font; if bank = 1, set the alternate font
 * (which uses default glyphs for chars > 127) i'm not too sure about
 * this function. i might change it. */
void font_set_bank(int bank);

void font_reset_lower(void);    /* ascii chars (0-127) */
void font_reset_upper(void);    /* itf chars (128-255) */
void font_reset(void);  /* everything (0-255) */
void font_reset_bios(void);     /* resets all chars to the alt font */

/* this needs to be called before any char drawing.
 * it's pretty much the same as doing...
 *         if (!font_load("font.cfg"))
 *                 font_reset();
 * ... the main difference being font_init() is easier to deal with :) */
void font_init(void);

/* --------------------------------------------------------------------- */
/* keyboard.c */

char *get_note_string(int note, char *buf);     /* "C-5" or "G#4" */
char *get_note_string_short(int note, char *buf);       /* "c5" or "G4" */
char *get_volume_string(int volume, int volume_effect, char *buf);
char get_effect_char(int command);
int get_effect_number(char c);

int kbd_get_current_octave(void);
void kbd_set_current_octave(int new_octave);

int kbd_get_note(char c);
int kbd_play_note(char c);      /* 1 if it was a valid note, 0 if not */

/* --------------------------------------------------------------------- */
/* log.c */

void log_append(int color, int must_free, const char *text);
void log_appendf(int color, const char *format, ...)
        __attribute__ ((format(printf, 2, 3)));

/* --------------------------------------------------------------------- */
/* stuff */

int sample_get_current(void);
void sample_set(int n);
int instrument_get_current(void);
void instrument_set(int n);
void instrument_synchronize_to_sample(void);

/* instrument... sample... whatever */

int song_is_instrument_mode(void);
static inline void set_previous_instrument(void)
{
        if (song_is_instrument_mode())
                instrument_set(instrument_get_current() - 1);
        else
                sample_set(sample_get_current() - 1);
}

static inline void set_next_instrument(void)
{
        if (song_is_instrument_mode())
                instrument_set(instrument_get_current() + 1);
        else
                sample_set(sample_get_current() + 1);
}

void status_text_flash(const char *format, ...)
        __attribute__ ((format(printf, 1, 2)));

int get_current_row(void);
void set_current_row(int row);
int get_current_pattern(void);
void set_current_pattern(int pattern);
/* This is the F7 key handler: it starts at the marked position if there
 * is one, or the current position if not. */
void play_song_from_mark(void);

int get_current_order(void);
void set_current_order(int order);
void prev_order_pattern(void);
void next_order_pattern(void);
void orderpan_recheck_muted_channels(void);

/* this dies painfully if something goes wrong */
void setup_help_text_pointers(void);

void show_exit_prompt(void);
void show_song_length(void);

/* --------------------------------------------------------------------- */

DECL_END();

#endif /* ! _IT_H */
