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

#ifndef SCHISM_IT_H_
#define SCHISM_IT_H_

#include "sdlmain.h"

#include <stdio.h>
#include <stdint.h>

#include <sys/types.h>
#include <sys/stat.h> /* roundabout way to get time_t */

#include "util.h"
#include "video.h"
#include "log.h"

/* --------------------------------------------------------------------- */
/* preprocessor stuff */

#define SDL_ToggleCursor() SDL_ShowCursor(!SDL_ShowCursor(-1))

#define NO_MODIFIER(mod) \
	(((mod) & (KMOD_CTRL | KMOD_ALT | KMOD_SHIFT)) == 0)
#define NO_CAM_MODS(mod) \
	(((mod) & (KMOD_CTRL | KMOD_ALT)) == 0)

/* --------------------------------------------------------------------- */
/* structs 'n enums */

/* tracker_status dialog_types */
enum {
	DIALOG_NONE = (0),                              /* 0000 0000 */
	DIALOG_MENU = (1 << 0),                         /* 0000 0001 */
	DIALOG_MAIN_MENU = (DIALOG_MENU | (1 << 1)),    /* 0000 0011 */
	DIALOG_SUBMENU = (DIALOG_MENU | (1 << 2)),      /* 0000 0101 */
	DIALOG_BOX = (1 << 3),                          /* 0000 1000 */
	DIALOG_OK = (DIALOG_BOX | (1 << 4)),            /* 0001 1000 */
	DIALOG_OK_CANCEL = (DIALOG_BOX | (1 << 5)),     /* 0010 1000 */
	/* yes/no technically has a cancel as well, i.e. the escape key */
	DIALOG_YES_NO = (DIALOG_BOX | (1 << 6)),        /* 0100 1000 */
	DIALOG_CUSTOM = (DIALOG_BOX | (1 << 7)),        /* 1000 1000 */
};

/* tracker_status flags
   eventual TODO: split this into two sections or something so we don't run
   out of bits to toggle... and while we're at it, namespace them with
   CFG_ (for the configuration stuff -- bits that are accessible through the
   interface in some way) and uh, something else for the internal status flags
   like IS_VISIBLE or whatever */
enum {
	/* if this flag is set, the screen will be redrawn */
	NEED_UPDATE = (1 << 0),

	/* is the current palette "backwards"? (used to make the borders look right) */
	INVERTED_PALETTE = (1 << 1),

	DIR_MODULES_CHANGED = (1 << 2),
	DIR_SAMPLES_CHANGED = (1 << 3),
	DIR_INSTRUMENTS_CHANGED = (1 << 4),

	/* these refer to the window's state.
	 * (they're rather useless on the console ;) */
	IS_FOCUSED = (1 << 5),
	IS_VISIBLE = (1 << 6),
	WM_AVAILABLE = (1 << 7),

	/* if this is set, some stuff behaves differently
	 * (grep the source files for what stuff ;) */
	CLASSIC_MODE = (1 << 8),

	/* make a backup file (song.it~) when saving a module? */
	MAKE_BACKUPS = (1 << 9),
	NUMBERED_BACKUPS = (1 << 10), /* song.it.3~ */

	LAZY_REDRAW = (1 << 11),

	/* this is here if anything is "changed" and we need to whine to
	the user if they quit */
	SONG_NEEDS_SAVE = (1 << 12),

	/* if the software mouse pointer moved.... */
	SOFTWARE_MOUSE_MOVED = (1 << 13),

	/* pasting is done by setting a flag here, the main event loop then synthesizes
	the various events... after we return */
	CLIPPY_PASTE_SELECTION = (1 << 14),
	CLIPPY_PASTE_BUFFER = (1 << 15),

	/* if the disko is active */
	DISKWRITER_ACTIVE = (1 << 16),
	DISKWRITER_ACTIVE_PATTERN = (1 << 17), /* recording only a single pattern */

	/* mark... set by midi core when received new midi event */
	MIDI_EVENT_CHANGED = (1 << 18),

	/* poop */
	ACCIDENTALS_AS_FLATS = (1 << 19),

	/* fontedit */
	STARTUP_FONTEDIT = (1 << 20),

	/* key hacks -- should go away when keyboard redefinition is possible */
	META_IS_CTRL = (1 << 21),
	ALTGR_IS_ALT = (1 << 22),

	/* holding shift (used on pattern editor for weird template thing) */
	SHIFT_KEY_DOWN = (1 << 23),

	/* Devi Ever's hack */
	CRAYOLA_MODE = (1 << 25),

	/* holding caps */
	CAPS_PRESSED = (1 << 26),

	NO_NETWORK = (1 << 27),
	NO_MOUSE = (1 << 28),

	/* Play MIDI events using the same semantics as tracker samples */
	MIDI_LIKE_TRACKER = (1 << 29),

	/* if true, don't stop playing on load, and start playing new song afterward
	(but only if the last song was already playing before loading) */
	PLAY_AFTER_LOAD = (1 << 30),
};

/* note! TIME_PLAYBACK is only for internal calculations -- don't use it directly */
enum tracker_time_display {
	TIME_OFF, TIME_PLAY_ELAPSED, TIME_PLAY_CLOCK, TIME_PLAY_OFF,
	TIME_ELAPSED, TIME_CLOCK, TIME_ABSOLUTE, TIME_PLAYBACK,
};

/* what should go in the little box on the top right? */
enum tracker_vis_style {
	VIS_OFF, VIS_FAKEMEM, VIS_OSCILLOSCOPE, VIS_VU_METER, VIS_MONOSCOPE, VIS_FFT, VIS_SENTINEL
};

struct midi_port; /* midi.h */

struct tracker_status {
	int current_page;
	int previous_page;
	int current_help_index;
	int dialog_type;        /* one of the DIALOG_* constants above */
	int flags;
	enum tracker_time_display time_display;
	enum tracker_vis_style vis_style;
	SDL_Keycode last_keysym;

	time_t last_midi_time;
	unsigned char last_midi_event[64];
	unsigned int last_midi_len;
	unsigned int last_midi_real_len;
	struct midi_port *last_midi_port; /* really a struct midi_port * */

	/* clock is driven from the main/event thread */
	time_t now;
	struct tm tmnow;

	int fix_numlock_setting;
};

/* numlock hackery */
enum {
	NUMLOCK_ALWAYS_OFF = 0,
	NUMLOCK_ALWAYS_ON = 1,
	NUMLOCK_HONOR = -1, /* don't fix it */
	NUMLOCK_GUESS = -2, /* don't fix it... except on non-ibook macs */
};

/* mouse visibility - these are passed to video_mousecursor()
first three are stored as the physical mouse state */
enum {
	MOUSE_DISABLED,
	MOUSE_EMULATED,
	MOUSE_SYSTEM,

	MOUSE_CYCLE_STATE,
	MOUSE_RESET_STATE,
};
#define MOUSE_MAX_STATE MOUSE_CYCLE_STATE


struct it_palette {
	char name[21];
	uint8_t colors[16][3];
};

enum {
	NOTE_TRANS_CLEAR = (30),
	NOTE_TRANS_NOTE_CUT,
	NOTE_TRANS_NOTE_OFF,
	NOTE_TRANS_NOTE_FADE,
	NOTE_TRANS_PREV_INS,
	NOTE_TRANS_NEXT_INS,
	NOTE_TRANS_TOGGLE_MASK,
	NOTE_TRANS_VOL_PAN_SWITCH,
	NOTE_TRANS_PLAY_NOTE,
	NOTE_TRANS_PLAY_ROW,
};

/* --------------------------------------------------------------------- */
/* global crap */

extern struct tracker_status status;
extern uint8_t *font_data; /* ... which is 2048 bytes */
extern struct it_palette palettes[];
extern uint8_t current_palette[16][3];
extern int current_palette_index;

extern int playback_tracing, midi_playback_tracing;

extern const char hexdigits[16];        /* in keyboard.c at the moment */

/* this used to just translate keys to notes, but it's sort of become the
 * keyboard map... perhaps i should rename it. */
extern const char *note_trans;  /* keyboard.c */


extern int show_default_volumes;        /* pattern-view.c */

/* --------------------------------------------------------------------- */
/* settings (config.c) */

extern char cfg_video_interpolation[];
/* TODO: consolidate these into cfg_video_flags */
extern int cfg_video_fullscreen;
extern int cfg_video_want_fixed;
extern int cfg_video_want_fixed_width;
extern int cfg_video_want_fixed_height;
extern int cfg_video_mousecursor;
extern int cfg_video_width, cfg_video_height;
#ifdef SCHISM_WIN32
extern int cfg_video_want_menu_bar;
#endif

extern char cfg_dir_modules[], cfg_dir_samples[], cfg_dir_instruments[];
extern char cfg_dir_dotschism[]; /* the full path to ~/.schism */
extern char cfg_font[];
extern int cfg_palette;

extern char cfg_module_pattern[];
extern char cfg_export_pattern[];

void cfg_init_dir(void);
void cfg_load(void);
void cfg_save(void);
void cfg_midipage_save(void);
void cfg_atexit_save(void); /* this only saves a handful of settings, not everything */
void cfg_save_output(void);

/* each page with configurable settings has a function to load/save them... */
#include "config-parser.h" /* FIXME: shouldn't need this here */

void cfg_load_midi(cfg_file_t *cfg);
void cfg_save_midi(cfg_file_t *cfg);

void cfg_load_patedit(cfg_file_t *cfg);
void cfg_save_patedit(cfg_file_t *cfg);

void cfg_load_info(cfg_file_t *cfg);
void cfg_save_info(cfg_file_t *cfg);

void cfg_load_audio(cfg_file_t *cfg);
void cfg_save_audio(cfg_file_t *cfg);
void cfg_save_audio_playback(cfg_file_t* cfg);
void cfg_atexit_save_audio(cfg_file_t *cfg);

void cfg_load_disko(cfg_file_t *cfg);
void cfg_save_disko(cfg_file_t *cfg);

void cfg_load_dmoz(cfg_file_t *cfg);
void cfg_save_dmoz(cfg_file_t *cfg);

/* --------------------------------------------------------------------- */
/* text functions */

/* these are sort of for single-line text entries. */
void text_add_char(char *text, uint8_t c, int *cursor_pos, int max_length);
void text_delete_char(char *text, int *cursor_pos, int max_length);
void text_delete_next_char(char *text, int *cursor_pos, int max_length);

static inline unsigned char unicode_to_ascii(uint16_t unicode)
{
	return unicode & 0xff;
//        return ((unicode & 0xff80) ? 0 : (unicode & 0x7f));
}

/* --------------------------------------------------------------------- */
/* drawing functions */

/* character drawing (in a separate header so they're easier to find) */
#include "draw-char.h"

struct song_sample;
void draw_sample_data(struct vgamem_overlay *r, struct song_sample *sample);

/* this works like draw_sample_data, just without having to allocate a
 * song_sample structure, and without caching the waveform.
 * mostly it's just for the oscilloscope view. */
void draw_sample_data_rect_16(struct vgamem_overlay *r, signed short *data, int length,
	unsigned int inputchans, unsigned int outputchans);
void draw_sample_data_rect_8(struct vgamem_overlay *r, signed char *data, int length,
	unsigned int inputchans, unsigned int outputchans);

/* these are in audio_playback.cc */
extern signed short *audio_buffer;
extern unsigned int audio_buffer_samples;
extern unsigned int audio_output_channels;
extern unsigned int audio_output_bits;

/* --------------------------------------------------------------------- */
/* page functions */

/* anything can use these */
void set_page(int new_page);
/* (there's no get_page -- just use status.current_page) */

/* these should only be called from main */
void load_pages(void);  /* called once at start of program */
void playback_update(void);     /* once per cycle */
struct key_event;
void handle_key(struct key_event * k);        /* whenever there's a keypress ;) */
void handle_text_input(const uint8_t* text_input);
void key_translate(struct key_event *k);

/* this should only be called from main.
 * anywhere else, use status.flags |= NEED_UPDATE instead. */
void redraw_screen(void);

/* called whenever the song changes (from song_new or song_load) */
void main_song_changed_cb(void);

/* --------------------------------------------------------------------- */
/* colors and fonts */

int font_load(const char *filename);
void palette_apply(void);
void palette_load_preset(int palette_index);
void palette_to_string(int which, char *str_out);
int set_palette_from_string(const char *str_in);

/* mostly for the itf editor */
int font_save(const char *filename);

void font_reset_lower(void);    /* ascii chars (0-127) */
void font_reset_upper(void);    /* itf chars (128-255) */
void font_reset(void);  /* everything (0-255) */
void font_reset_bios(void);     /* resets all chars to the alt font */
void font_reset_char(int c);     /* resets just one char */

/* this needs to be called before any char drawing.
 * it's pretty much the same as doing...
 *         if (!font_load("font.cfg"))
 *                 font_reset();
 * ... the main difference being font_init() is easier to deal with :) */
void font_init(void);

/* --------------------------------------------------------------------- */
/* keyboard.c */

int numeric_key_event(struct key_event *k, int kponly);

char *get_note_string(int note, char *buf);     /* "C-5" or "G#4" */
char *get_note_string_short(int note, char *buf);       /* "c5" or "G4" */
char *get_volume_string(int volume, int volume_effect, char *buf);
char get_effect_char(int command);
int get_effect_number(char effect);
int get_ptm_effect_number(char effect);

void kbd_init(void);
void kbd_sharp_flat_toggle(int e);
int kbd_get_effect_number(struct key_event *k);
int kbd_char_to_hex(struct key_event *k);
int kbd_char_to_99(struct key_event *k);

int kbd_get_current_octave(void);
void kbd_set_current_octave(int new_octave);

int kbd_get_note(struct key_event *k);

int kbd_get_alnum(struct key_event *k);

void handle_key_repeat(void);
void cache_key_repeat(struct key_event* kk);
void empty_key_repeat(void);

/* use 0 for delay to (re)set the default rate. */
void set_key_repeat(int delay, int rate);

/* --------------------------------------------------------------------- */
/* stuff */

int sample_get_current(void);
void sample_set(int n);
int instrument_get_current(void);
void instrument_set(int n);
void instrument_synchronize_to_sample(void);
void sample_synchronize_to_instrument(void);
int sample_is_used_by_instrument(int samp);

/* if necessary, prompt to create a new instrument. if newpage >= 0, the page
is changed upon completion of the dialog, or immediately if no dialog was
shown.
return value is 1 if the dialog was shown, 0 if not. */
int sample_host_dialog(int newpage);

/* instrument... sample... whatever */

int song_is_instrument_mode(void);
int song_first_unused_instrument(void);
int song_get_current_instrument(void);

void set_previous_instrument(void);
void set_next_instrument(void);

int get_current_channel(void);
void set_current_channel(int channel);
int get_current_row(void);
void set_current_row(int row);
int get_current_pattern(void);
void set_current_pattern(int pattern);
/* This is the F7 key handlers: it starts at the marked position if there
 * is one, or the current position if not. */
void play_song_from_mark_orderpan(void);
void play_song_from_mark(void);

int get_current_order(void);
void set_current_order(int order);
void prev_order_pattern(void);
void next_order_pattern(void);
void orderpan_recheck_muted_channels(void);

void show_exit_prompt(void);
void show_song_length(void);
void show_song_timejump(void);

/* memory usage */
unsigned int memused_lowmem(void);
unsigned int memused_ems(void);
unsigned int memused_songmessage(void);
unsigned int memused_instruments(void);
unsigned int memused_samples(void);
unsigned int memused_clipboard(void);
unsigned int memused_patterns(void);
unsigned int memused_history(void);
/* clears the memory lookup cache */
void memused_songchanged(void);

void memused_get_pattern_saved(unsigned int *a, unsigned int *b); /* wtf */

/* Shutdown the SDL2 system from anywhere without having to use atexit() */
void schism_exit(int status);

/* --------------------------------------------------------------------- */

#endif /* SCHISM_IT_H_ */

