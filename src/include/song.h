/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <someguy@here.is> <http://here.is/someguy/>
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

/* --------------------------------------------------------------------- */
/* oodles o' structs */

/* aka modinstrument */
typedef struct _song_sample {
        unsigned long length, loop_start, loop_end;
        unsigned long sustain_start, sustain_end;
        signed char *data;
        unsigned long speed;
        short panning;
        short volume;
        short global_volume;
        short flags;
        signed char relative_tone;      /* mod-ish tuning */
        signed char finetune;   /* mod-ish tuning */
        byte vib_type;
        byte vib_rate;
        byte vib_depth;
        byte vib_speed;
        char filename[22];
} song_sample;

/* modchannelsettings */
typedef struct _song_channel {
        unsigned long panning;
        unsigned long volume;
        unsigned long flags;
        unsigned long mix_plugin;
        char name[20];
} song_channel;

/* instrumentheader */
typedef struct _song_instrument {
        unsigned long fadeout;
        unsigned long flags;    // any of the ENV_* flags below
        unsigned short global_volume;
        unsigned short panning;
        unsigned short vol_env_ticks[32];
        unsigned short pan_env_ticks[32];
        unsigned short pitch_env_ticks[32];
        byte vol_env_values[32];
        byte pan_env_values[32];
        byte pitch_env_values[32];
        byte sample_map[128], note_map[128];
        byte vol_env_nodes;
        byte pan_env_nodes;
        byte pitch_env_nodes;
        byte vol_loop_start, vol_loop_end;
        byte vol_sustain_start, vol_sustain_end;
        byte pan_loop_start, pan_loop_end;
        byte pan_sustain_start, pan_sustain_end;
        byte pitch_loop_start, pitch_loop_end;
        byte pitch_sustain_start, pitch_sustain_end;
        byte nna, dct, dca;
        byte pan_swing, volume_swing;
        byte filter_cutoff;
        byte filter_resonance;
        unsigned short midi_bank;
        byte midi_program;
        byte midi_channel;
        byte midi_drum_key;
        signed char pitch_pan_separation;
        byte pitch_pan_center;
        char name[32];
        char filename[12];
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
        unsigned long sample_pos;
        unsigned long nPosLo;
        long nInc;
        long nRightVol; // these two are the current left/right volumes
        long nLeftVol;  /* (duh...) - i'm not sure if that's 100% right,
                         * though. for one, the max seems to be 680, and
                         * another, they seem to be backward (i.e. left
                         * is right and right is left...) */
        long nRightRamp;        // maybe these two have something to do
        long nLeftRamp; // with fadeout or nnas or something? dunno.
        unsigned long sample_length;    /* not counting beyond the loopend */
        unsigned long flags;    /* the channel's flags (surround, mute,
                                 * etc.) and the sample's (16-bit, etc.)
                                 * combined together */
        unsigned long nLoopStart;
        unsigned long nLoopEnd;
        long nRampRightVol;
        long nRampLeftVol;
        long nFilter_Y1, nFilter_Y2, nFilter_Y3, nFilter_Y4;
        long nFilter_A0, nFilter_B0, nFilter_B1;
        long nROfs, nLOfs;
        long nRampLength;

        /* information not used in the mixer */
        signed char *pSample;   /* same as sample_data, except this isn't
                                 * set to NULL at the end of the sample */
        long nNewRightVol, nNewLeftVol; // ???
        /* final_volume is what's actually used for mixing (after the
         * global volume, envelopes, etc. are accounted for). same deal
         * for final_panning. */
        long final_volume;      /* range 0-16384 (?) */
        long final_panning;     /* range 0-256. */
        /* these are the volumes set by the channel. */
        long volume, panning;   /* range 0-256 (?) */
        long nFadeOutVol;       /* ??? */
        long nPeriod, nC4Speed, nPortamentoDest;
        song_instrument *instrument;    /* NULL if sample mode (?) */
        song_sample *sample;
        unsigned long nVolEnvPosition, nPanEnvPosition, nPitchEnvPosition;
        unsigned long master_channel;   // the channel this note was played in
        unsigned long vu_meter;
        long nGlobalVol;        // the channel volume (Mxx)? - range 0-64
        long nInsVol;   /* instrument volume? sample volume? dunno, one of
                         * those two... (range 0-64) */
        long nFineTune, nTranspose;
        long nPortamentoSlide, nAutoVibDepth;
        unsigned long nAutoVibPos, nVibratoPos, nTremoloPos, nPanbrelloPos;

        signed short nVolSwing, nPanSwing;

        byte note;      // the note that's playing
        byte nNNA;
        byte nNewNote;  // nfi... seems the same as nNote
        byte nNewIns;   // nfi, always seems to be zero
        byte nCommand, nArpeggio;
        byte nOldVolumeSlide, nOldFineVolUpDown;
        byte nOldPortaUpDown, nOldFinePortaUpDown;
        byte nOldPanSlide, nOldChnVolSlide;
        byte nVibratoType, nVibratoSpeed, nVibratoDepth;
        byte nTremoloType, nTremoloSpeed, nTremoloDepth;
        byte nPanbrelloType, nPanbrelloSpeed, nPanbrelloDepth;
        byte nOldCmdEx, nOldVolParam, nOldTempo;
        byte nOldOffset, nOldHiOffset;
        byte nCutOff, nResonance;
        byte nRetrigCount, nRetrigParam;
        byte nTremorCount, nTremorParam;
        byte nPatternLoop, nPatternLoopCount;
        byte nRowNote, nRowInstr;
        byte nRowVolCmd, nRowVolume;
        byte nRowCommand, nRowParam;
        byte left_vu, right_vu;
        byte nActiveMacro, nPadding;
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
};

extern struct audio_settings audio_settings;

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
        //SAMP_STEREO         = (0x40),
        //SAMP_PINGPONGFLAG   = (0x80), -- what is this?
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

/* --------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------- */

void song_new(int flags);
int song_load(const char *file);
int song_save(const char *file);

void song_clear_sample(int n);
int song_load_sample(int n, const char *file);
int song_save_sample_its(int n, const char *file);
int song_save_sample_s3i(int n, const char *file);
int song_save_sample_raw(int n, const char *file);

/*
void song_load_sample_library(const char *file);
void song_free_sample_library(void);
void song_load_sample_from_library(int from, int to);
*/

const char *song_get_filename(void);
const char *song_get_basename(void);
char *song_get_title(void);     // editable
char *song_get_message(void);   // editable

// returned value = seconds
unsigned long song_get_length(void);

// gee. can't just use malloc/free... no, that would be too simple.
signed char *song_sample_allocate(int bytes);
void song_sample_free(signed char *data);

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
void song_clear_solo_channel(void);

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
/* void song_set_stereo(int value); ??? */
int song_has_old_effects(void);
void song_set_old_effects(int value);
int song_has_compatible_gxx(void);
void song_set_compatible_gxx(int value);
int song_has_linear_pitch_slides(void);
void song_set_linear_pitch_slides(int value);
int song_is_instrument_mode(void);
/* void song_set_instrument_mode(int value); ??? */

/* this is called way early */
void song_initialise(void);

/* these are called later at startup, and also when the relevant settings are changed */
void song_init_audio(void);
void song_init_modplug(void);

/* --------------------------------------------------------------------- */
/* playback */

void song_toggle_multichannel_mode(void);
void song_change_current_play_channel(int relative, int wraparound);
void song_play_note(int ins, int note, int chan, int useins);   /* channel 0 = use multichannel */

void song_start(void);
void song_stop(void);
void song_loop_pattern(int pattern, int row);
void song_start_at_order(int order, int row);
void song_start_at_pattern(int pattern, int row);
void song_single_step(int pattern, int row);

/* see the enum above */
enum song_mode song_get_mode(void);

/* the time returned is in seconds */
unsigned long song_get_current_time(void);

int song_get_current_speed(void);
int song_get_current_tempo(void);
int song_get_current_global_volume(void);

int song_get_current_order(void);
int song_get_playing_pattern(void);
int song_get_current_row(void);

void song_set_current_order(int order);

int song_get_playing_channels(void);
int song_get_max_channels(void);

void song_get_vu_meter(int *left, int *right);

void song_set_current_speed(int speed);
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
 *         unsigned long *channel_list;
 *         song_mix_channel *channel;
 *         int n = song_get_mix_state(&channel_list);
 *         while (n--) {
 *                 channel = song_get_mix_channel(channel_list[n]);
 *                 (do something with the channel)
 *         }
 * it's kind of ugly, but it'll do... i hope :) */
int song_get_mix_state(unsigned long **channel_list);

/* --------------------------------------------------------------------- */
/* misc. */

void song_flip_stereo(void);

int song_get_surround(void);
void song_set_surround(int on);

static const song_note empty_note = { 0, 0, 0, 0, 0, 0 };

/* --------------------------------------------------------------------- */

#ifdef __cplusplus
}
#endif

#endif /* ! SONG_H */
