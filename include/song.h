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

#ifndef SONG_H
#define SONG_H

#include <stdint.h>

#include "sndfile.h"
#include "util.h"
#include "disko.h"

/* --------------------------------------------------------------------- */
/* oodles o' structs */

/* midi config */
typedef struct _midiconfig {
        char midi_global_data[9*32];
        char midi_sfx[16*32];
        char midi_zxx[128*32];
} midi_config;
#define MIDI_GCF_START          (0*32)
#define MIDI_GCF_STOP           (1*32)
#define MIDI_GCF_TICK           (2*32)
#define MIDI_GCF_NOTEON         (3*32)
#define MIDI_GCF_NOTEOFF        (4*32)
#define MIDI_GCF_VOLUME         (5*32)
#define MIDI_GCF_PAN            (6*32)
#define MIDI_GCF_BANKCHANGE     (7*32)
#define MIDI_GCF_PROGRAMCHANGE  (8*32)

/* aka modinstrument */
typedef struct _song_sample {
        unsigned int length, loop_start, loop_end;
        unsigned int sustain_start, sustain_end;
        signed char *data;
        unsigned int speed;
        unsigned int panning;
        unsigned int volume;
        unsigned int global_volume;
        unsigned int flags;
        unsigned int vib_type;
        unsigned int vib_rate;
        unsigned int vib_depth;
        unsigned int vib_speed;
        char name[32];
        char filename[22];
        int played;
        uint32_t globalvol_saved;
        unsigned char AdlibBytes[12];
} song_sample;

/* modchannelsettings */
typedef struct _song_channel {
        unsigned int panning;
        unsigned int volume;
        unsigned int flags;
} song_channel;

/* instrumentenvelope */
typedef struct _song_envelope {
        int ticks[32];
        uint8_t values[32];
        int nodes;
        int loop_start, loop_end;
        int sustain_start, sustain_end;
} song_envelope;

/* instrumentheader */
typedef struct _song_instrument {
        unsigned int fadeout;
        unsigned int flags;    // any of the ENV_* flags below
        unsigned int global_volume;
        unsigned int panning;
        unsigned int sample_map[128], note_map[128];
        song_envelope vol_env, pan_env, pitch_env;
        unsigned int nna, dct, dca;
        unsigned int pan_swing, volume_swing;
        unsigned int filter_cutoff;
        unsigned int filter_resonance;
        unsigned int midi_bank;
        unsigned int midi_program;
        unsigned int midi_channel_mask;
        unsigned int midi_drum_key;
        int pitch_pan_separation;
        unsigned int pitch_pan_center;
        char name[32];
        char filename[16];

        int played;
} song_instrument;

/* modcommand */
typedef struct _song_note {
        uint8_t note;
        uint8_t instrument;
        uint8_t volume_effect;
        uint8_t effect;
        uint8_t volume;
        uint8_t parameter;
} song_note;

/* modchannel (good grief...) */
typedef struct _song_mix_channel {
        signed char *sample_data;
        unsigned int sample_pos;
        unsigned int nPosLo;
        unsigned int topnote_offset;
        int nInc;
        int nRightVol; // these two are the current left/right volumes
        int nLeftVol;  /* (duh...) - i'm not sure if that's 100% right,
                         * though. for one, the max seems to be 680, and
                         * another, they seem to be backward (i.e. left
                         * is right and right is left...) */
        int nRightRamp;        // maybe these two have something to do
        int nLeftRamp; // with fadeout or nnas or something? dunno.
        unsigned int sample_length;    /* not counting beyond the loopend */
        unsigned int flags;    /* the channel's flags (surround, mute,
                                 * etc.) and the sample's (16-bit, etc.)
                                 * combined together */
        unsigned int nLoopStart;
        unsigned int nLoopEnd;
        int nRampRightVol;
        int nRampLeftVol;
        int strike;
        double nFilter_Y1, nFilter_Y2, nFilter_Y3, nFilter_Y4;
        double nFilter_A0, nFilter_B0, nFilter_B1;
        int nROfs, nLOfs;
        int nRampLength;

        /* information not used in the mixer */
        signed char *pSample;   /* same as sample_data, except this isn't
                                 * set to NULL at the end of the sample */
        int nNewRightVol, nNewLeftVol; // ???
        /* final_volume is what's actually used for mixing (after the
         * global volume, envelopes, etc. are accounted for). same deal
         * for final_panning. */
        int final_volume;      /* range 0-16384 (?) */
        int final_panning;     /* range 0-256. */
        /* these are the volumes set by the channel. */
        int volume, panning;   /* range 0-256 (?) */
        int nFadeOutVol;       /* ??? */
        int nPeriod, nC5Speed, sample_freq, nPortamentoDest;
        song_instrument *instrument;    /* NULL if sample mode (?) */
        song_sample *sample;
        int nVolEnvPosition, nPanEnvPosition, nPitchEnvPosition;
        unsigned int master_channel;   // the channel this note was played in
        unsigned int vu_meter;
        int nGlobalVol;        // the channel volume (Mxx)? - range 0-64
        int nInsVol;   /* instrument volume? sample volume? dunno, one of
                         * those two... (range 0-64) */
        int nPortamentoSlide, nAutoVibDepth;
        unsigned int nAutoVibPos, nVibratoPos, nTremoloPos, nPanbrelloPos;

        int nVolSwing, nPanSwing;

        unsigned int note;      // the note that's playing
        unsigned int nNNA;
        unsigned int nNewNote;  // nfi... seems the same as nNote
        unsigned int nNewIns;   // nfi, always seems to be zero
        unsigned int nCommand, nArpeggio;
        unsigned int nOldVolumeSlide, nOldFineVolUpDown;
        unsigned int nOldGlbVolSlide;
        unsigned int nOldPortaUpDown, nOldFinePortaUpDown;
        unsigned int nOldPanSlide, nOldChnVolSlide;
        unsigned int nNoteSlideCounter, nNoteSlideSpeed, nNoteSlideStep;
        unsigned int nVibratoType, nVibratoSpeed, nVibratoDepth;
        unsigned int nTremoloType, nTremoloSpeed, nTremoloDepth;
        unsigned int nPanbrelloType, nPanbrelloSpeed, nPanbrelloDepth;
        unsigned int nOldCmdEx, nOldVolParam, nOldTempo;
        unsigned int nOldOffset, nOldHiOffset;
        unsigned int nCutOff, nResonance;
        unsigned int nRetrigCount, nRetrigParam;
        int nNoteDelay, nNoteCut;
        unsigned int nTremorParam, nTremorCount;
        unsigned int nPatternLoop, nPatternLoopCount;
        unsigned int nRowNote, nRowInstr;
        unsigned int nRowVolCmd, nRowVolume;
        unsigned int nRowCommand, nRowParam;
        unsigned int nActiveMacro, nPadding;
} song_mix_channel;

/* --------------------------------------------------------------------- */
/* non-song-related structures */

/* defined in audio_playback.cc; also used by page_settings.c */

struct audio_settings {
        int sample_rate, bits, channels, buffer_size;
        int channel_limit, interpolation_mode;
        int oversampling, hq_resampling;
        int noise_reduction, surround_effect;

        unsigned int eq_freq[4];
        unsigned int eq_gain[4];
        int no_ramping;
};

extern struct audio_settings audio_settings;


/* for saving samples; see also enum sample_format_ids below */

struct sample_save_format {
        const char *name;
        const char *ext;
        int (*save_func) (disko_t *fp, song_sample *smp);
};

extern struct sample_save_format sample_save_formats[];

/* --------------------------------------------------------------------- */
/* some enums */

// sample flags
#define SAMP_16_BIT            CHN_16BIT
#define SAMP_LOOP              CHN_LOOP
#define SAMP_LOOP_PINGPONG     CHN_PINGPONGLOOP
#define SAMP_SUSLOOP           CHN_SUSTAINLOOP
#define SAMP_SUSLOOP_PINGPONG  CHN_PINGPONGSUSTAIN
#define SAMP_PANNING           CHN_PANNING
#define SAMP_STEREO            CHN_STEREO
#define SAMP_ADLIB             CHN_ADLIB // indicates an adlib sample

#define SAMP_GLOBALVOL 0x10000 /* used for feature-check, completely not related to the player */


/* volume column effects */
#define VOL_EFFECT_NONE            VOLCMD_NONE
#define VOL_EFFECT_VOLUME          VOLCMD_VOLUME
#define VOL_EFFECT_PANNING         VOLCMD_PANNING
#define VOL_EFFECT_VOLSLIDEUP      VOLCMD_VOLSLIDEUP
#define VOL_EFFECT_VOLSLIDEDOWN    VOLCMD_VOLSLIDEDOWN
#define VOL_EFFECT_FINEVOLUP       VOLCMD_FINEVOLUP
#define VOL_EFFECT_FINEVOLDOWN     VOLCMD_FINEVOLDOWN
#define VOL_EFFECT_VIBRATOSPEED    VOLCMD_VIBRATOSPEED
#define VOL_EFFECT_VIBRATODEPTH    VOLCMD_VIBRATODEPTH
#define VOL_EFFECT_PANSLIDELEFT    VOLCMD_PANSLIDELEFT
#define VOL_EFFECT_PANSLIDERIGHT   VOLCMD_PANSLIDERIGHT
#define VOL_EFFECT_TONEPORTAMENTO  VOLCMD_TONEPORTAMENTO
#define VOL_EFFECT_PORTAUP         VOLCMD_PORTAUP
#define VOL_EFFECT_PORTADOWN       VOLCMD_PORTADOWN


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

/* used as indices to sample_save_formats[] */
enum sample_save_format_ids {
        SSMP_ITS = 0,
        SSMP_AIFF = 1,
        SSMP_AU = 2,
        SSMP_WAV = 3,
        SSMP_RAW = 4,
        SSMP_SENTINEL = 5,
};

/* --------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------- */

/*
song_load:
        prompt ok/cancel if the existing song hasn't been saved.
        after loading, the current page is changed accordingly.
song_load_unchecked:
        *NO* dialog, just goes right into loading the song
        doesn't set the page after loading
        return value is nonzero if the load was successful.
        generally speaking, don't use this function directly;
        use song_load instead.
*/
void song_new(int flags);
void song_load(const char *file);
int song_load_unchecked(const char *file);
int song_save(const char *file, const char *type);

void song_clear_sample(int n);
void song_copy_sample(int n, song_sample *src);
int song_load_sample(int n, const char *file);
int song_save_sample(int n, const char *file, int format_id);
void song_stop_sample(song_sample *ssmp);

void song_create_host_instrument(int smp);

int song_load_instrument(int n, const char *file);
int song_load_instrument_ex(int n, const char *file, const char *libf, int nx);
int song_save_instrument(int n, const char *file);

midi_config *song_get_midi_config(void);
midi_config *song_get_default_midi_config(void);

/* returns bytes */
unsigned song_copy_sample_raw(int n, unsigned int rs,
                const void *data, unsigned int samples);



void song_sample_set_c5speed(int n, unsigned int c5);
int song_sample_is_empty(int n);
unsigned int song_sample_get_c5speed(int n);

/* get the order for a particular pattern; locked can be:
        * a starting order number (0-255)
        * "-1" meaning start at the locked order
        * "-2" meaning start at the current order
*/
int song_order_for_pattern(int pat, int locked);

const char *song_get_filename(void);
const char *song_get_basename(void);
const char *song_get_tracker_id(void);
char *song_get_title(void);     // editable
char *song_get_message(void);   // editable

// returned value = seconds
unsigned int song_get_length(void);
unsigned int song_get_length_to(int order, int row);
void song_get_at_time(unsigned int seconds, int *order, int *row);

// gee. can't just use malloc/free... no, that would be too simple.
signed char *song_sample_allocate(int bytes);
void song_sample_free(signed char *data);

// these are used to directly manipulate the pattern list
song_note *song_pattern_allocate(int rows);
song_note *song_pattern_allocate_copy(int patno, int *rows);
void song_pattern_deallocate(song_note *n);
void song_pattern_install(int patno, song_note *n, int rows);


// these return NULL on failure.
song_sample *song_get_sample(int n);
song_instrument *song_get_instrument(int n);
int song_get_instrument_number(song_instrument *ins); // 0 => no instrument; ignore above comment =)
song_channel *song_get_channel(int n);

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

int song_get_pattern(int n, song_note ** buf);  // return 0 -> error
uint8_t *song_get_orderlist(void);

int song_pattern_is_empty(int p);

int song_get_num_orders(void);
int song_get_num_patterns(void);
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

/* Called at startup.
The 'driver_spec' parameter is formatted as driver[:device].
        'driver' is the name of the SDL driver to use
                example: "alsa", "dsound"
                SDL_AUDIODRIVER is set to this value
        'device' (optional) is the name of the device to use
                example: "hw:2", "/dev/dsp"
                SDL_PATH_DSP and AUDIODEV are set to this

For the SDL driver, 'nosound' and 'none' are aliases for 'dummy', for
compatibility with previous Schism Tracker versions, and 'oss' is an
alias for 'dsp', because 'dsp' is a dumb name for an audio driver. */
void audio_init(const char *driver_spec);

/* Reconfigure the same device that was opened before. */
void audio_reinit(void);

/* eq */
void song_init_eq(int do_reset);

/* --------------------------------------------------------------------- */
/* playback */
void song_lock_audio(void);
void song_unlock_audio(void);
void song_stop_audio(void);
void song_start_audio(void);
const char *song_audio_driver(void);

void song_toggle_multichannel_mode(void);
int song_is_multichannel_mode(void);
void song_change_current_play_channel(int relative, int wraparound);
int song_get_current_play_channel(void);

/* these return the channel that was used for the note.
sample/inst slots 1+ are used "normally"; the sample loader uses slot #0 for preview playback */
#define KEYJAZZ_CHAN_CURRENT 0
#define KEYJAZZ_NOINST -1
#define KEYJAZZ_DEFAULTVOL -1
int song_keydown(int samp, int ins, int note, int vol, int chan);
int song_keyrecord(int samp, int ins, int note, int vol, int chan, int effect, int param);
int song_keyup(int samp, int ins, int note);

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
song_mix_channel *song_get_mix_channel(int n);

/* get the mix state:
 * if channel_list != NULL, it is set to an array of the channels that
 * are being mixed. the return value is the number of channels to mix
 * (i.e. the length of the channel_list array). so... to go through each
 * channel that's being mixed:
 *
 *         unsigned int *channel_list;
 *         song_mix_channel *channel;
 *         int n = song_get_mix_state(&channel_list);
 *         while (n--) {
 *                 channel = song_get_mix_channel(channel_list[n]);
 *                 (do something with the channel)
 *         }
 * it's kind of ugly, but it'll do... i hope :) */
int song_get_mix_state(unsigned int **channel_list);

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

void song_delete_instrument(int n);
void song_wipe_instrument(int n);
void song_delete_sample(int n);

int song_instrument_is_empty(int n);
void song_init_instruments(int n); /* -1 for all */
void song_init_instrument_from_sample(int ins, int samp);

/* --------------------------------------------------------------------- */
/* misc. */

/* called by audio system when buffer stuff change */
void midi_queue_alloc(int buffer_size, int channels, int samples_per_second);

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

/* actually from sndfile.h */
#define SCHISM_MAX_SAMPLES      MAX_SAMPLES
#define SCHISM_MAX_INSTRUMENTS  MAX_INSTRUMENTS
#define SCHISM_MAX_MESSAGE      MAX_MESSAGE

/* --------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* ! SONG_H */

