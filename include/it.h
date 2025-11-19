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

#include "headers.h"

#include "backend/timer.h"

#include "util.h"
#include "video.h"
#include "log.h"
#include "keyboard.h"

/* --------------------------------------------------------------------- */
/* preprocessor stuff */

#define NO_MODIFIER(mod) \
	(((mod) & (SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT | SCHISM_KEYMOD_SHIFT)) == 0)
#define NO_CAM_MODS(mod) \
	(((mod) & (SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT)) == 0)

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

	/* if this is set, some stuff behaves differently
	 * (grep the source files for what stuff ;) */
	CLASSIC_MODE = (1 << 5),

	/* make a backup file (song.it~) when saving a module? */
	MAKE_BACKUPS = (1 << 6),
	NUMBERED_BACKUPS = (1 << 7), /* song.it.3~ */

	LAZY_REDRAW = (1 << 8),

	/* this is here if anything is "changed" and we need to whine to
	the user if they quit */
	SONG_NEEDS_SAVE = (1 << 9),

	/* if the software mouse pointer moved.... */
	SOFTWARE_MOUSE_MOVED = (1 << 10),

	/* pasting is done by setting a flag here, the main event loop then synthesizes
	 * the various events... after we return
	 *
	 * FIXME this sucks, we should just call clippy directly */
	CLIPPY_PASTE_SELECTION = (1 << 11),
	CLIPPY_PASTE_BUFFER = (1 << 12),

	/* if the disko is active */
	DISKWRITER_ACTIVE = (1 << 13),
	DISKWRITER_ACTIVE_PATTERN = (1 << 14), /* recording only a single pattern */

	/* mark... set by midi core when received new midi event */
	MIDI_EVENT_CHANGED = (1 << 15),

	/* fontedit */
	STARTUP_FONTEDIT = (1 << 16),

	/* key hacks -- should go away when keyboard redefinition is possible */
	META_IS_CTRL = (1 << 17),
	ALTGR_IS_ALT = (1 << 18),

	/* Devi Ever's hack */
	CRAYOLA_MODE = (1 << 20),

	NO_NETWORK = (1 << 22),

	/* Play MIDI events using the same semantics as tracker samples */
	MIDI_LIKE_TRACKER = (1 << 23),

	/* if true, don't stop playing on load, and start playing new song afterward
	(but only if the last song was already playing before loading) */
	PLAY_AFTER_LOAD = (1 << 24),

	/* --headless passed (generally this should be default with --diskwrite) */
	STATUS_IS_HEADLESS = (1 << 25),
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
	schism_keysym_t last_keysym;
	schism_keysym_t last_orig_keysym;
	schism_keymod_t last_keymod;

	schism_keymod_t keymod;

	timer_ticks_t last_midi_tick;
	unsigned char last_midi_event[64];
	unsigned int last_midi_len;
	unsigned int last_midi_real_len; /* XXX what is this */
	struct midi_port *last_midi_port;

	/* clock is driven from the main/event thread */
	time_t now;
	struct tm tmnow;

	int fix_numlock_setting;
};

#define LAST_KEY_IS(sym, mod) (((mod == 0) ? !status.last_keymod : (status.last_keymod & mod)) && (status.last_keysym == sym))

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

extern int playback_tracing, midi_playback_tracing;

extern const char hexdigits[16];	/* in keyboard.c at the moment */

/* this used to just translate keys to notes, but it's sort of become the
 * keyboard map... perhaps i should rename it. */
extern const char *note_trans;	/* keyboard.c */


extern int show_default_volumes;	/* pattern-view.c */

/* --------------------------------------------------------------------- */
/* text functions */

/* these are sort of for single-line text entries. */
void text_add_char(char *text, uint8_t c, int *cursor_pos, int max_length);
void text_delete_char(char *text, int *cursor_pos, int max_length);
void text_delete_next_char(char *text, int *cursor_pos, int max_length);

/* --------------------------------------------------------------------- */
/* ... */

/* these are in audio_playback.c */
extern void *audio_buffer;
extern uint32_t audio_buffer_samples;
extern uint32_t audio_output_channels;
extern uint32_t audio_output_bits;

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
void handle_text_input(const char *text_input);

/* this should only be called from main.
 * anywhere else, use status.flags |= NEED_UPDATE instead. */
void redraw_screen(void);

/* called whenever the song changes (from song_new or song_load) */
void main_song_changed_cb(void);

/* --------------------------------------------------------------------- */
/* stuff */

/* main.c */
void toggle_display_fullscreen(void);

/* page_instruments/page_samples.c */
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

/* some platforms have different entrypoints (see: macosx) which set up
 * platform-specific stuff, and eventually call this function to actually
 * run schism */
int schism_main(int argc, char** argv);

/* Shutdown the SDL2 system from anywhere without having to use atexit() */
SCHISM_NORETURN void schism_exit(int status);

void schism_crash(void (*log_cb)(FILE *, void *), void *userdata);

/* --------------------------------------------------------------------- */
/* page_waterfall.c */

void vis_init(void);
void vis_set_size(uint32_t size);
void vis_work_32s(const int32_t *in, size_t inlen);
void vis_work_32m(const int32_t *in, size_t inlen);
void vis_work_16s(const int16_t *in, size_t inlen);
void vis_work_16m(const int16_t *in, size_t inlen);
void vis_work_8s(const int8_t *in, size_t inlen);
void vis_work_8m(const int8_t *in, size_t inlen);

// "unsigned char out[width]" ...
void fft_get_columns(uint32_t width, unsigned char *out, uint32_t chan);

/* --------------------------------------------------------------------- */

#endif /* SCHISM_IT_H_ */

