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

#ifndef SCHISM_SONG_H_
#define SCHISM_SONG_H_

#include <stdint.h>

#include "player/sndfile.h"
#include "util.h"
#include "disko.h"
#include "fmt.h"

/* --------------------------------------------------------------------- */
/* things that used to be in mplink */

extern song_t *current_song;

extern char song_filename[]; /* the full path (as given to song_load) */
extern char song_basename[]; /* everything after the last slash */

/* milliseconds = (samples * 1000) / frequency */
extern unsigned int samples_played;

extern unsigned int max_channels_used;

/* --------------------------------------------------------------------- */
/* non-song-related structures */

/* defined in audio_playback.cc; also used by page_settings.c */

struct audio_settings {
	int sample_rate, bits, channels, buffer_size;
	int channel_limit, interpolation_mode;

	struct {
		int left;
		int right;
	} master;
	
	int surround_effect;

	unsigned int eq_freq[4];
	unsigned int eq_gain[4];
	int no_ramping;
};

extern struct audio_settings audio_settings;

struct audio_device {
	int id;
	char* name; /* UTF-8; must be free'd */
};

extern struct audio_device* audio_device_list;
extern int audio_device_list_size;

/* --------------------------------------------------------------------- */

typedef struct {
	int freq; // sample rate
	uint8_t bits; // 8 or 16, always system byte order
	uint8_t channels; // channels
	uint16_t samples; // buffer size
	void (*callback)(uint8_t *stream, int len);
} schism_audio_spec_t;

/* An opaque structure that each backend uses for its own data */
typedef struct schism_audio_device schism_audio_device_t;

/* --------------------------------------------------------------------- */
/* some enums */

/* for song_get_mode */
enum song_mode {
	MODE_STOPPED = 0,
	MODE_PLAYING = 1,
	MODE_PATTERN_LOOP = 2,
	MODE_SINGLE_STEP = 4,
};

enum song_new_flags {
	KEEP_PATTERNS = 1,
	KEEP_SAMPLES = 2,
	KEEP_INSTRUMENTS = 4,
	KEEP_ORDERLIST = 8,
};

/* --------------------------------------------------------------------- */

/*
song_load:
	prompt ok/cancel if the existing song hasn't been saved.
	after loading, the current page is changed accordingly.
	this loads into the global song.
song_load_unchecked:
	*NO* dialog, just goes right into loading the song
	doesn't set the page after loading.
	this also loads into the global song.
	return value is nonzero if the load was successful.
	generally speaking, don't use this function directly;
	use song_load instead.
song_create_load:
	internal back-end function that loads and returns a song.
	the above functions both use this.
*/
void song_new(int flags);
void song_load(const char *file);
int song_load_unchecked(const char *file);
song_t *song_create_load(const char *file);

// song_create_load returns NULL on error and sets errno to what might not be a standard value
// use this to divine the meaning of these cryptic numbers
const char *fmt_strerror(int n);

int song_save(const char *file, const char *type); // IT, S3M
int song_export(const char *file, const char *type); // WAV

/* 'num' is only for status text feedback -- all of the sample's data is taken from 'smp'.
this provides an eventual mechanism for saving samples modified from disk (not yet implemented) */
int song_save_sample(const char *file, const char *type, song_sample_t *smp, int num);

void song_clear_sample(int n);
void song_copy_sample(int n, song_sample_t *src);
int song_load_sample(int n, const char *file);

void song_create_host_instrument(int smp);

int song_load_instrument(int n, const char *file);
int song_load_instrument_with_prompt(int n, const char *file);
int song_load_instrument_ex(int n, const char *file, const char *libf, int nx);
int song_save_instrument(const char *filename, const char *type, song_instrument_t *ins, int num);

int song_sample_is_empty(int n);

/* search the orderlist for a pattern, starting at the current order.
return value of -1 means the pattern isn't on the list */
int song_next_order_for_pattern(int pat);

const char *song_get_filename(void);
const char *song_get_basename(void);
const char *song_get_tracker_id(void);
char *song_get_title(void);     // editable
char *song_get_message(void);   // editable

// returned value = seconds
unsigned int song_get_length_to(int order, int row);
void song_get_at_time(unsigned int seconds, int *order, int *row);

// gee. can't just use malloc/free... no, that would be too simple.
signed char *song_sample_allocate(int bytes);
void song_sample_free(signed char *data);

// these are used to directly manipulate the pattern list
song_note_t *song_pattern_allocate(int rows);
song_note_t *song_pattern_allocate_copy(int patno, int *rows);
void song_pattern_deallocate(song_note_t *n);
void song_pattern_install(int patno, song_note_t *n, int rows);


// these return NULL on failure.
song_sample_t *song_get_sample(int n);
song_instrument_t *song_get_instrument(int n);
int song_get_instrument_number(song_instrument_t *ins); // 0 => no instrument; ignore above comment =)
song_channel_t *song_get_channel(int n);

// this one should probably be organized somewhere else..... meh
void song_set_channel_mute(int channel, int muted);
void song_toggle_channel_mute(int channel);
// if channel is the current soloed channel, undo the solo (reset the
// channel state); otherwise, save the state and solo the channel.
void song_handle_channel_solo(int channel);
void song_save_channel_states(void);
void song_restore_channel_states(void);

// find the last channel that's not muted. (if a channel is soloed, this
// deals with the saved channel state instead.)
int song_find_last_channel(void);

int song_get_pattern(int n, song_note_t ** buf);  // return 0 -> error
int song_get_pattern_offset(int * n, song_note_t ** buf, int * row, int offset);
uint8_t *song_get_orderlist(void);

int song_pattern_is_empty(int p);

int song_get_rows_in_pattern(int pattern);

void song_pattern_resize(int pattern, int rows);

int song_get_initial_speed(void);
void song_set_initial_speed(int new_speed);
int song_get_initial_tempo(void);
void song_set_initial_tempo(int new_tempo);

int song_get_initial_global_volume(void);
void song_set_initial_global_volume(int new_vol);
int song_get_mixing_volume(void);
void song_set_mixing_volume(int new_vol);
int song_get_separation(void);
void song_set_separation(int new_sep);

/* these next few are booleans... */
int song_is_stereo(void);
void song_set_stereo(void);
void song_set_mono(void);
void song_toggle_stereo(void);
void song_toggle_mono(void);

/* called from song_set_stereo et al - this updates the value on F12 to match the song */
void song_vars_sync_stereo(void);

/* void song_set_stereo(int value); ??? */
int song_has_old_effects(void);
void song_set_old_effects(int value);
int song_has_compatible_gxx(void);
void song_set_compatible_gxx(int value);
int song_has_linear_pitch_slides(void);
void song_set_linear_pitch_slides(int value);
int song_is_instrument_mode(void);
void song_set_instrument_mode(int value);


/* this is called way early */
void song_initialise(void);

/* called later at startup, and also when the relevant settings are changed */
void song_init_modplug(void);

/* parses strings in the old "driver spec" format Schism used in the config
 * and still uses in the command line */
void audio_parse_driver_spec(const char* spec, char** driver, char** device);

void audio_flash_reinitialized_text(int success);

/* Called at startup.
 *
 * 'nosound' and 'none' are aliases for 'dummy' for compatibility with previous
 * schism versions, and 'oss' is an alias for 'dsp', because 'dsp' is a dumb name
 * for an audio driver. */
int audio_init(const char *driver, const char *device);

/* Reconfigure the same device that was opened before. */
int audio_reinit(const char *device);

void audio_quit(void);

/* eq */
void song_init_eq(int do_reset, uint32_t mix_freq);

/* --------------------------------------------------------------------- */
/* playback */
void song_lock_audio(void);
void song_unlock_audio(void);
void song_stop_audio(void);
void song_start_audio(void);

const char *song_audio_driver(void);
const char *song_audio_device(void);

void free_audio_device_list(void);
int refresh_audio_device_list(void);

int audio_driver_count(void);
const char *audio_driver_name(int x);

void song_toggle_multichannel_mode(void);
int song_is_multichannel_mode(void);
void song_change_current_play_channel(int relative, int wraparound);
int song_get_current_play_channel(void);

/* These return the channel that was used for the note.
Sample/inst slots 1+ are used "normally"; the sample loader uses slot #0 for preview playback -- but reports
KEYJAZZ_INST_FAKE to keydown/up, since zero conflicts with the standard "use previous sample for this channel"
behavior which is normally internal, but is exposed on the pattern editor where it's possible to explicitly
select sample #0. (note: this is a hack to work around another hack) */
#define KEYJAZZ_CHAN_CURRENT 0
// For automatic channel allocation when playing chords in the instrument editor.
#define KEYJAZZ_CHAN_AUTO -1
#define KEYJAZZ_NOINST -1
#define KEYJAZZ_DEFAULTVOL -1
#define KEYJAZZ_INST_FAKE -2
int song_keydown(int samp, int ins, int note, int vol, int chan);
int song_keyrecord(int samp, int ins, int note, int vol, int chan, int effect, int param);
int song_keyup(int samp, int ins, int note);
int song_keyup_channel(int samp, int ins, int note, int chan);

void song_start(void);
void song_start_once(void);
void song_pause(void);
void song_stop(void);
void song_stop_unlocked(int quitting);
void song_loop_pattern(int pattern, int row);
void song_start_at_order(int order, int row);
void song_start_at_pattern(int pattern, int row);
void song_single_step(int pattern, int row);

/* see the enum above */
enum song_mode song_get_mode(void);

/* the time returned is in seconds */
unsigned int song_get_current_time(void);

int song_get_current_speed(void);
int song_get_current_tick(void);
int song_get_current_tempo(void);
int song_get_current_global_volume(void);

int song_get_current_order(void);
int song_get_playing_pattern(void);
int song_get_current_row(void);

void song_set_current_order(int order);
void song_set_next_order(int order);
int song_toggle_orderlist_locked(void);

int song_get_playing_channels(void);
int song_get_max_channels(void);

void song_get_vu_meter(int *left, int *right);

/* fill the array with flags of each playing sample/instrument, such that iff
 * sample #7 is playing, samples[7] will be nonzero. these are a bit processor
 * intensive since they require a linear traversal through the mix channels. */
void song_get_playing_samples(int samples[]);
void song_get_playing_instruments(int instruments[]);

/* update any currently playing channels with current sample configuration */
void song_update_playing_sample(int s_changed);
void song_update_playing_instrument(int i_changed);

void song_set_current_speed(int speed);
void song_set_current_tempo(int t);
void song_set_current_global_volume(int volume);

/* this is very different from song_get_channel!
 * this deals with the channel that's *playing* and is used mostly
 * (entirely?) for the info page. */
song_voice_t *song_get_mix_channel(int n);

/* get the mix state:
 * if channel_list != NULL, it is set to an array of the channels that
 * are being mixed. the return value is the number of channels to mix
 * (i.e. the length of the channel_list array). so... to go through each
 * channel that's being mixed:
 *
 *         unsigned int *channel_list;
 *         song_voice_t *channel;
 *         int n = song_get_mix_state(&channel_list);
 *         while (n--) {
 *                 channel = song_get_mix_channel(channel_list[n]);
 *                 (do something with the channel)
 *         }
 * it's kind of ugly, but it'll do... i hope :) */
int song_get_mix_state(uint32_t **channel_list);

/* --------------------------------------------------------------------- */
/* rearranging stuff */

/* exchange = only in list; swap = in list and song */
void song_exchange_samples(int a, int b);
void song_exchange_instruments(int a, int b);
void song_swap_samples(int a, int b);
void song_swap_instruments(int a, int b);
void song_copy_instrument(int dst, int src);
void song_replace_sample(int num, int with);
void song_replace_instrument(int num, int with);

void song_insert_sample_slot(int n);
void song_remove_sample_slot(int n);
void song_insert_instrument_slot(int n);
void song_remove_instrument_slot(int n);

void song_delete_instrument(int n, int preserve_samples);
void song_wipe_instrument(int n);

int song_instrument_is_empty(int n);
void song_init_instruments(int n); /* -1 for all */
void song_init_instrument_from_sample(int ins, int samp);

/* --------------------------------------------------------------------- */
/* misc. */

void song_flip_stereo(void);

int song_get_surround(void);
void song_set_surround(int on);

/* for the orderpan page */
enum {
	PANS_STEREO,
	PANS_AMIGA,
	PANS_LEFT,
	PANS_RIGHT,
	PANS_MONO,
	PANS_SLASH,
	PANS_BACKSLASH,
//      PANS_CROSS,
};
void song_set_pan_scheme(int scheme);

/* --------------------------------------------------------------------- */

#endif /* SCHISM_SONG_H_ */

