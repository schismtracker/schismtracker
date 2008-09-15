/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
 * URL: http://rigelseven.com/schism/
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

#include "util.h"
#include "diskwriter.h"

/* --------------------------------------------------------------------- */
/* oodles o' structs */

/* midi config */
typedef struct _midiconfig {
	char midi_global_data[9*32];
	char midi_sfx[16*32];
	char midi_zxx[128*32];
} midi_config;
#define MIDI_GCF_START		(0*32)
#define MIDI_GCF_STOP		(1*32)
#define MIDI_GCF_TICK		(2*32)
#define MIDI_GCF_NOTEON		(3*32)
#define MIDI_GCF_NOTEOFF	(4*32)
#define MIDI_GCF_VOLUME		(5*32)
#define MIDI_GCF_PAN		(6*32)
#define MIDI_GCF_BANKCHANGE	(7*32)
#define MIDI_GCF_PROGRAMCHANGE	(8*32)

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
        int relative_tone;      /* mod-ish tuning */
        int finetune;   /* mod-ish tuning */
        unsigned int vib_type;
        unsigned int vib_rate;
        unsigned int vib_depth;
        unsigned int vib_speed;
        char filename[22];
	int played;
        unsigned char AdlibBytes[12];
} song_sample;

/* modchannelsettings */
typedef struct _song_channel {
        unsigned int panning;
        unsigned int volume;
        unsigned int flags;
        unsigned int mix_plugin;
        char name[20];
} song_channel;

/* instrumentenvelope */
typedef struct _song_envelope {
	int ticks[32];
	byte values[32];
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
        unsigned int midi_channel;
        unsigned int midi_drum_key;
        int pitch_pan_separation;
        unsigned int pitch_pan_center;
        char name[32];
        char filename[12];
	
	int played;
} song_instrument;

/* modcommand */
typedef struct _song_note {
        byte note;
        byte instrument;
        byte volume_effect;
        byte effect;
        byte volume;
        byte parameter;
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
        int nPeriod, nC4Speed, sample_freq, nPortamentoDest;
        song_instrument *instrument;    /* NULL if sample mode (?) */
        song_sample *sample;
        int nVolEnvPosition, nPanEnvPosition, nPitchEnvPosition;
        unsigned int master_channel;   // the channel this note was played in
        unsigned int vu_meter;
        int nGlobalVol;        // the channel volume (Mxx)? - range 0-64
        int nInsVol;   /* instrument volume? sample volume? dunno, one of
                         * those two... (range 0-64) */
        int nFineTune, nTranspose;
        int nPortamentoSlide, nAutoVibDepth;
        unsigned int nAutoVibPos, nVibratoPos, nTremoloPos, nPanbrelloPos;

        int nVolSwing, nPanSwing;

        unsigned int note;      // the note that's playing
        unsigned int nNNA;
        unsigned int nNewNote;  // nfi... seems the same as nNote
        unsigned int nNewIns;   // nfi, always seems to be zero
        unsigned int nCommand, nArpeggio;
        unsigned int nOldVolumeSlide, nOldFineVolUpDown;
        unsigned int nOldPortaUpDown, nOldFinePortaUpDown;
        unsigned int nOldPanSlide, nOldChnVolSlide;
        unsigned int nVibratoType, nVibratoSpeed, nVibratoDepth;
        unsigned int nTremoloType, nTremoloSpeed, nTremoloDepth;
        unsigned int nPanbrelloType, nPanbrelloSpeed, nPanbrelloDepth;
        unsigned int nOldCmdEx, nOldVolParam, nOldTempo;
        unsigned int nOldOffset, nOldHiOffset;
        unsigned int nCutOff, nResonance;
        unsigned int nRetrigCount, nRetrigParam;
        unsigned int nTremorCount, nTremorParam;
        unsigned int nPatternLoop, nPatternLoopCount;
        unsigned int nRowNote, nRowInstr;
        unsigned int nRowVolCmd, nRowVolume;
        unsigned int nRowCommand, nRowParam;
        unsigned int left_vu, right_vu;
        unsigned int nActiveMacro, nPadding;
	unsigned int nTickStart;
	unsigned int nRealtime;
	byte stupid_gcc_workaround;

} song_mix_channel;

/* --------------------------------------------------------------------- */
/* non-song-related structures */

/* defined in audio_playback.cc; also used by page_settings.c */

struct audio_settings {
	int sample_rate, bits, channels, buffer_size;
	int channel_limit, interpolation_mode;
	int oversampling, hq_resampling;
	int noise_reduction, surround_effect;
	int xbass, xbass_amount, xbass_range;
	int surround, surround_depth, surround_delay;
	int reverb, reverb_depth, reverb_delay;

	unsigned int eq_freq[4];
	unsigned int eq_gain[4];
	int no_ramping;
};

extern struct audio_settings audio_settings;


/* for saving samples; see also enum sample_format_ids below */

//typedef int (*fmt_save_sample_func) (FILE *fp, song_sample *smp, char *title);
struct sample_save_format {
	const char *name;
	const char *ext;
	//fmt_save_sample_func *save_func;
	int (*save_func) (diskwriter_driver_t *fp,
				song_sample *smp, char *title);
};

extern struct sample_save_format sample_save_formats[];

/* --------------------------------------------------------------------- */
/* some enums */

// sample flags
enum {
        SAMP_16_BIT = (0x01),
        SAMP_LOOP = (0x02),
        SAMP_LOOP_PINGPONG = (0x04),
        SAMP_SUSLOOP = (0x08),
        SAMP_SUSLOOP_PINGPONG = (0x10),
        SAMP_PANNING = (0x20),
        SAMP_STEREO         = (0x40),
        //SAMP_PINGPONGFLAG   = (0x80), -- what is this?
        SAMP_ADLIB   = (0x20000000) // indicates an Adlib sample
};

// channel flags
enum {
        CHN_MUTE = (0x100),     /* this one's important :) */
        CHN_KEYOFF = (0x200),
        CHN_NOTEFADE = (0x400),
        CHN_SURROUND = (0x800), /* important :) */
        CHN_NOIDO = (0x1000),
        CHN_HQSRC = (0x2000),
        CHN_FILTER = (0x4000),
        CHN_VOLUMERAMP = (0x8000),
        CHN_VIBRATO = (0x10000),
        CHN_TREMOLO = (0x20000),
        CHN_PANBRELLO = (0x40000),
        CHN_PORTAMENTO = (0x80000),
        CHN_GLISSANDO = (0x100000),
        CHN_VOLENV = (0x200000),
        CHN_PANENV = (0x400000),
        CHN_PITCHENV = (0x800000),
        CHN_FASTVOLRAMP = (0x1000000),
        CHN_EXTRALOUD = (0x2000000),
        CHN_REVERB = (0x4000000),
        CHN_NOREVERB = (0x8000000),
};

// instrument envelope flags
enum {
        ENV_VOLUME = (0x0001),
        ENV_VOLSUSTAIN = (0x0002),
        ENV_VOLLOOP = (0x0004),
        ENV_PANNING = (0x0008),
        ENV_PANSUSTAIN = (0x0010),
        ENV_PANLOOP = (0x0020),
        ENV_PITCH = (0x0040),
        ENV_PITCHSUSTAIN = (0x0080),
        ENV_PITCHLOOP = (0x0100),
        ENV_SETPANNING = (0x0200),
        ENV_FILTER = (0x0400),
        ENV_VOLCARRY = (0x0800),
        ENV_PANCARRY = (0x1000),
        ENV_PITCHCARRY = (0x2000),
};

enum {
        ORDER_SKIP = (254),     // the '+++' order
        ORDER_LAST = (255),     // the '---' order
};

// note fade IS actually supported in Impulse Tracker,
// but there's no way to handle it in the editor :)
enum {
        NOTE_FADE = (253),      // '~~~'
        NOTE_CUT = (254),       // '^^^' 
        NOTE_OFF = (255),       // '==='
};

enum {
        VIB_SINE = (0),
        VIB_SQUARE = (1),
        VIB_RAMP_UP = (2),
        VIB_RAMP_DOWN = (3),    /* modplug extension -- not supported */
        VIB_RANDOM = (4),
};

/* volume column effects */
enum {
        VOL_EFFECT_NONE,
        VOL_EFFECT_VOLUME,
        VOL_EFFECT_PANNING,
        VOL_EFFECT_VOLSLIDEUP,
        VOL_EFFECT_VOLSLIDEDOWN,
        VOL_EFFECT_FINEVOLUP,
        VOL_EFFECT_FINEVOLDOWN,
        VOL_EFFECT_VIBRATOSPEED,
        VOL_EFFECT_VIBRATO,
        VOL_EFFECT_PANSLIDELEFT,
        VOL_EFFECT_PANSLIDERIGHT,
	VOL_EFFECT_TONEPORTAMENTO,
        VOL_EFFECT_PORTAUP,
        VOL_EFFECT_PORTADOWN
};

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

void song_new(int flags);
int song_load(const char *file);
int song_load_unchecked(const char *file);
int song_save(const char *file, const char *type);

void song_clear_sample(int n);
void song_copy_sample(int n, song_sample *src, char *srcname);
int song_load_sample(int n, const char *file);
int song_preload_sample(void *f);
int song_save_sample(int n, const char *file, int format_id);

int song_load_instrument(int n, const char *file);
int song_load_instrument_ex(int n, const char *file, const char *libf, int nx);
int song_save_instrument(int n, const char *file);

midi_config *song_get_midi_config(void);
midi_config *song_get_default_midi_config(void);

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
song_sample *song_get_sample(int n, char **name_ptr);
song_instrument *song_get_instrument(int n, char **name_ptr);
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
byte *song_get_orderlist(void);

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

/* these are called later at startup, and also when the relevant settings are changed */
void song_init_audio(const char *driver);
void song_init_modplug(void);

/* eq */
void song_init_eq(int do_reset);

/* --------------------------------------------------------------------- */
/* playback */
void song_lock_audio(void);
void song_unlock_audio(void);
void song_stop_audio(void);
void song_start_audio(void);
const char *song_audio_driver(void);
#define song_audio_driver_name() song_audio_driver()

void song_toggle_multichannel_mode(void);
int song_is_multichannel_mode(void);
void song_change_current_play_channel(int relative, int wraparound);
int song_get_current_play_channel(void);

/* these return the selected channel */
#define KEYDOWN_CHAN_JAM	-1
#define KEYDOWN_CHAN_CURRENT	-2
int song_keydown(int s,int ins, int n, int v, int c, int *mm);
int song_keyrecord(int s,int ins, int n, int v, int c, int *mm,
						int effect, int param);
int song_keyup(int s,int ins, int n, int c, int *mm);

void song_start(void);
void song_start_once(void);
void song_stop(void);
void song_stop_unlocked(void);
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
void song_copy_instrument(int src, int dst);

void song_insert_sample_slot(int n);
void song_remove_sample_slot(int n);
void song_insert_instrument_slot(int n);
void song_remove_instrument_slot(int n);

void song_delete_instrument(int n);
void song_wipe_instrument(int n);

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

/* actually from sndfile.h */
#define SCHISM_MAX_SAMPLES	200
#define SCHISM_MAX_INSTRUMENTS	SCHISM_MAX_SAMPLES

/* --------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* ! SONG_H */
