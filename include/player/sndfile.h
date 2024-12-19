/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#ifndef SCHISM_PLAYER_SNDFILE_H_
#define SCHISM_PLAYER_SNDFILE_H_

#include "headers.h"

#include "disko.h"
#include "slurp.h"

#include "tables.h"

#include <time.h> // struct tm
#include "timer.h" // schism_ticks_t


#define MOD_AMIGAC2             0x1AB
#define MAX_SAMPLE_LENGTH       16000000
#define MAX_SAMPLE_RATE         192000
#define MAX_ORDERS              256
#define MAX_PATTERNS            240
#define MAX_SAMPLES             236
#define MAX_INSTRUMENTS         MAX_SAMPLES
#define MAX_CHANNELS            64
#define MAX_ENVPOINTS           32
#define MAX_INFONAME            80
#define MAX_EQ_BANDS            6
#define MAX_MESSAGE             8000
#define MAX_INTERPOLATION_LOOKAHEAD 4
#define MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE 16 // Borrowed from OpenMPT
#define MAX_SAMPLING_POINT_SIZE 4


#define MAX_VOICES              256

#define MIX_MAX_CHANNELS		2 /* used for filters and stuff */
#define MIXBUFFERSIZE           512


#define CHN_16BIT               0x01 // 16-bit sample
#define CHN_LOOP                0x02 // looped sample
#define CHN_PINGPONGLOOP        0x04 // bi-directional (useless unless CHN_LOOP is also set)
#define CHN_SUSTAINLOOP         0x08 // sample with sustain loop
#define CHN_PINGPONGSUSTAIN     0x10 // bi-directional (useless unless CHN_SUSTAINLOOP is also set)
#define CHN_PANNING             0x20 // sample with default panning set
#define CHN_STEREO              0x40 // stereo sample
#define CHN_PINGPONGFLAG        0x80 // when flag is on, sample is processed backwards
#define CHN_MUTE                0x100 // muted channel
#define CHN_KEYOFF              0x200 // exit sustain (note-off encountered)
#define CHN_NOTEFADE            0x400 // fade note (~~~ or end of instrument envelope)
#define CHN_SURROUND            0x800 // use surround channel (S91)
#define CHN_NOIDO               0x1000 // near enough to an exact multiple of c5speed that interpolation
				       // won't be noticeable (or interpolation is disabled completely)
#define CHN_HQSRC               0x2000 // ???
#define CHN_FILTER              0x4000 // filtered output (i.e., Zxx)
#define CHN_VOLUMERAMP          0x8000 // ramp volume
#define CHN_VIBRATO             0x10000 // apply vibrato
#define CHN_TREMOLO             0x20000 // apply tremolo
//#define CHN_PANBRELLO         0x40000 // apply panbrello (handled elsewhere now)
#define CHN_PORTAMENTO          0x80000 // apply portamento
#define CHN_GLISSANDO           0x100000 // glissando mode ("stepped" pitch slides)
#define CHN_VOLENV              0x200000 // volume envelope is active
#define CHN_PANENV              0x400000 // pan envelope is active
#define CHN_PITCHENV            0x800000 // pitch/filter envelope is active
#define CHN_FASTVOLRAMP         0x1000000 // ramp volume very fast (XXX this is a dumb flag)
#define CHN_NEWNOTE             0x2000000 // note was triggered, reset filter
//#define CHN_REVERB            0x4000000
//#define CHN_NOREVERB          0x8000000
#define CHN_NNAMUTE             0x10000000 // turn off mute, but have it reset later
#define CHN_ADLIB               0x20000000 // OPL mode
#define CHN_LOOP_WRAPPED        0x40000000 // loop has just wrapped to the beginning

#define CHN_SAMPLE_FLAGS (CHN_16BIT | CHN_LOOP | CHN_PINGPONGLOOP | CHN_SUSTAINLOOP \
	| CHN_PINGPONGSUSTAIN | CHN_PANNING | CHN_STEREO | CHN_PINGPONGFLAG | CHN_ADLIB)


#define ENV_VOLUME              0x0001
#define ENV_VOLSUSTAIN          0x0002
#define ENV_VOLLOOP             0x0004
#define ENV_PANNING             0x0008
#define ENV_PANSUSTAIN          0x0010
#define ENV_PANLOOP             0x0020
#define ENV_PITCH               0x0040
#define ENV_PITCHSUSTAIN        0x0080
#define ENV_PITCHLOOP           0x0100
#define ENV_SETPANNING          0x0200
#define ENV_FILTER              0x0400
#define ENV_VOLCARRY            0x0800
#define ENV_PANCARRY            0x1000
#define ENV_PITCHCARRY          0x2000
#define ENV_MUTE                0x4000

#define FX_NONE                0 // .
#define FX_ARPEGGIO            1 // J
#define FX_PORTAMENTOUP        2 // F
#define FX_PORTAMENTODOWN      3 // E
#define FX_TONEPORTAMENTO      4 // G
#define FX_VIBRATO             5 // H
#define FX_TONEPORTAVOL        6 // L
#define FX_VIBRATOVOL          7 // K
#define FX_TREMOLO             8 // R
#define FX_PANNING             9 // X
#define FX_OFFSET              10 // O
#define FX_VOLUMESLIDE         11 // D
#define FX_POSITIONJUMP        12 // B
#define FX_VOLUME              13 // ! (FT2/IMF Cxx)
#define FX_PATTERNBREAK        14 // C
#define FX_RETRIG              15 // Q
#define FX_SPEED               16 // A
#define FX_TEMPO               17 // T
#define FX_TREMOR              18 // I
#define FX_SPECIAL             20 // S
#define FX_CHANNELVOLUME       21 // M
#define FX_CHANNELVOLSLIDE     22 // N
#define FX_GLOBALVOLUME        23 // V
#define FX_GLOBALVOLSLIDE      24 // W
#define FX_KEYOFF              25 // $ (FT2 Kxx)
#define FX_FINEVIBRATO         26 // U
#define FX_PANBRELLO           27 // Y
#define FX_PANNINGSLIDE        29 // P
#define FX_SETENVPOSITION      30 // & (FT2 Lxx)
#define FX_MIDI                31 // Z
#define FX_NOTESLIDEUP         32 // ( (IMF Gxy)
#define FX_NOTESLIDEDOWN       33 // ) (IMF Hxy)
#define FX_MAX                 34
#define FX_UNIMPLEMENTED       FX_MAX // no-op, displayed as "?"

#define FX_IS_EFFECT(v) ((v) > 0 && (v) < FX_MAX)

// Volume Column commands
#define VOLFX_NONE             0
#define VOLFX_VOLUME           1
#define VOLFX_PANNING          2
#define VOLFX_VOLSLIDEUP       3 // C
#define VOLFX_VOLSLIDEDOWN     4 // D
#define VOLFX_FINEVOLUP        5 // A
#define VOLFX_FINEVOLDOWN      6 // B
#define VOLFX_VIBRATOSPEED     7 // $ (FT2 Ax)
#define VOLFX_VIBRATODEPTH     8 // H
#define VOLFX_PANSLIDELEFT     9 // < (FT2 Dx)
#define VOLFX_PANSLIDERIGHT    10 // > (FT2 Ex)
#define VOLFX_TONEPORTAMENTO   11 // G
#define VOLFX_PORTAUP          12 // F
#define VOLFX_PORTADOWN        13 // E

// orderlist
#define ORDER_SKIP              254 // +++
#define ORDER_LAST              255 // ---

// 'Special' notes
// Note fade IS actually supported in Impulse Tracker, but there's no way to handle it in the editor
// (Actually, any non-valid note is handled internally as a note fade, but it's good to have a single
// value for internal representation)
// update 20090805: ok just discovered that IT internally uses 253 for its "no note" value.
// guess we'll use a different value for fade!
// note: 246 is rather arbitrary, but IT conveniently displays this value as "F#D" ("FD" with 2-char notes)
#define NOTE_NONE               0   // ...
#define NOTE_FIRST              1   // C-0
#define NOTE_MIDC               61  // C-5
#define NOTE_LAST               120 // B-9
#define NOTE_FADE               246 // ~~~
#define NOTE_CUT                254 // ^^^
#define NOTE_OFF                255 // ===
#define NOTE_IS_NOTE(n)         ((n) > NOTE_NONE && (n) <= NOTE_LAST) // anything playable - C-0 to B-9
#define NOTE_IS_CONTROL(n)      ((n) > NOTE_LAST)                     // not a note, but non-empty
#define NOTE_IS_INVALID(n)      ((n) > NOTE_LAST && (n) < NOTE_CUT && (n) != NOTE_FADE) // ???

// Auto-vibrato types
#define VIB_SINE                0
#define VIB_RAMP_DOWN           1
#define VIB_SQUARE              2
#define VIB_RANDOM              3

// NNA types
#define NNA_NOTECUT             0
#define NNA_CONTINUE            1
#define NNA_NOTEOFF             2
#define NNA_NOTEFADE            3

// DCT types
#define DCT_NONE                0
#define DCT_NOTE                1
#define DCT_SAMPLE              2
#define DCT_INSTRUMENT          3

// DCA types
#define DCA_NOTECUT             0
#define DCA_NOTEOFF             1
#define DCA_NOTEFADE            2

// Nothing innately special about this -- just needs to be above the max pattern length.
// process row is set to this in order to get the player to jump to the end of the pattern.
// (See ITTECH.TXT)
#define PROCESS_NEXT_ORDER      0xFFFE

// Module flags
#define SONG_EMBEDMIDICFG       0x0001 // Embed MIDI macros (Shift-F1) in file
//#define SONG_FASTVOLSLIDES    0x0002
#define SONG_ITOLDEFFECTS       0x0004 // Old Impulse Tracker effect implementations
#define SONG_COMPATGXX          0x0008 // "Compatible Gxx" (handle portamento more like other trackers)
#define SONG_LINEARSLIDES       0x0010 // Linear slides vs. Amiga slides
#define SONG_PATTERNPLAYBACK    0x0020 // Only playing current pattern
//#define SONG_STEP             0x0040
#define SONG_PAUSED             0x0080 // Playback paused (Shift-F8)
//#define SONG_FADINGSONG       0x0100
#define SONG_ENDREACHED         0x0200 // Song is finished (standalone keyjazz mode)
//#define SONG_GLOBALFADE       0x0400
//#define SONG_CPUVERYHIGH      0x0800
#define SONG_FIRSTTICK          0x1000 // Current tick is the first tick of the row (dopey flow-control flag)
//#define SONG_MPTFILTERMODE    0x2000
//#define SONG_SURROUNDPAN      0x4000
//#define SONG_EXFILTERRANGE    0x8000
//#define SONG_AMIGALIMITS      0x10000
#define SONG_INSTRUMENTMODE     0x20000 // Process instruments
#define SONG_ORDERLOCKED        0x40000 // Don't advance orderlist *(Alt-F11)
#define SONG_NOSTEREO           0x80000 // secret code for "mono"
#define SONG_PATTERNLOOP        (SONG_PATTERNPLAYBACK | SONG_ORDERLOCKED) // Loop current pattern (F6)

// Global Options (Renderer)
#define SNDMIX_REVERSESTEREO    0x0001 // swap L/R audio channels
//#define SNDMIX_NOISEREDUCTION 0x0002 // reduce hiss (do not use, it's just a simple low-pass filter)
//#define SNDMIX_AGC            0x0004 // automatic gain control
#define SNDMIX_NORESAMPLING     0x0008 // force no resampling (uninterpolated)
#define SNDMIX_HQRESAMPLER      0x0010 // cubic resampling
//#define SNDMIX_MEGABASS       0x0020
//#define SNDMIX_SURROUND       0x0040
//#define SNDMIX_REVERB         0x0080
//#define SNDMIX_EQ             0x0100 // apply EQ (always on)
//#define SNDMIX_SOFTPANNING    0x0200
#define SNDMIX_ULTRAHQSRCMODE   0x0400 // polyphase resampling (or FIR? I don't know)
// Misc Flags (can safely be turned on or off)
#define SNDMIX_DIRECTTODISK     0x10000 // disk writer mode
#define SNDMIX_NOBACKWARDJUMPS  0x40000 // disallow Bxx jumps from going backward in the orderlist
//#define SNDMIX_MAXDEFAULTPAN  0x80000 // (no longer) Used by the MOD loader
#define SNDMIX_MUTECHNMODE      0x100000 // Notes are not played on muted channels
#define SNDMIX_NOSURROUND       0x200000 // ignore S91
//#define SNDMIX_NOMIXING       0x400000
#define SNDMIX_NORAMPING        0x800000 // don't apply ramping on volume change (causes clicks)

enum {
	SRCMODE_NEAREST,
	SRCMODE_LINEAR,
	SRCMODE_SPLINE,
	SRCMODE_POLYPHASE,
	NUM_SRC_MODES
};

// ------------------------------------------------------------------------------------------------------------
// Flags for csf_read_sample

// Sample data characteristics
// Note:
// - None of these constants are zero
// - The format specifier must have a value set for each "section"
// - csf_read_sample DOES check the values for validity

// Bit width (8 bits for simplicity)
#define _SDV_BIT(n)            ((n) << 0)
#define SF_BIT_MASK            0xff
#define SF_7                   _SDV_BIT(7)  // 7-bit (weird!)
#define SF_8                   _SDV_BIT(8)  // 8-bit
#define SF_16                  _SDV_BIT(16) // 16-bit
#define SF_24                  _SDV_BIT(24) // 24-bit
#define SF_32                  _SDV_BIT(32) // 32-bit
#define SF_64                  _SDV_BIT(64) // 64-bit (for IEEE floating point)

// Channels (4 bits)
#define _SDV_CHN(n)            ((n) << 8)
#define SF_CHN_MASK            0xf00
#define SF_M                   _SDV_CHN(1) // mono
#define SF_SI                  _SDV_CHN(2) // stereo, interleaved
#define SF_SS                  _SDV_CHN(3) // stereo, split

// Endianness (4 bits)
#define _SDV_END(n)            ((n) << 12)
#define SF_END_MASK            0xf000
#define SF_LE                  _SDV_END(1) // little-endian
#define SF_BE                  _SDV_END(2) // big-endian

// Encoding (8 bits)
#define _SDV_ENC(n)            ((n) << 16)
#define SF_ENC_MASK            0xff0000
#define SF_PCMS                _SDV_ENC(1) // PCM, signed
#define SF_PCMU                _SDV_ENC(2) // PCM, unsigned
#define SF_PCMD                _SDV_ENC(3) // PCM, delta-encoded
#define SF_IT214               _SDV_ENC(4) // Impulse Tracker 2.14 compressed
#define SF_IT215               _SDV_ENC(5) // Impulse Tracker 2.15 compressed
#define SF_AMS                 _SDV_ENC(6) // AMS / Velvet Studio packed
#define SF_DMF                 _SDV_ENC(7) // DMF Huffman compression
#define SF_MDL                 _SDV_ENC(8) // MDL Huffman compression
#define SF_PTM                 _SDV_ENC(9) // PTM 8-bit delta value -> 16-bit sample
#define SF_PCMD16              _SDV_ENC(10) // PCM, 16-byte table delta-encoded
#define SF_IEEE                _SDV_ENC(11) // IEEE floating point

// Sample format shortcut
#define SF(a,b,c,d) (SF_ ## a | SF_ ## b| SF_ ## c | SF_ ## d)

// ------------------------------------------------------------------------------------------------------------

typedef struct song_sample {
	uint32_t length;
	uint32_t loop_start;
	uint32_t loop_end;
	uint32_t sustain_start;
	uint32_t sustain_end;
	signed char *data;
	uint32_t c5speed;
	uint32_t panning;
	uint32_t volume;
	uint32_t global_volume;
	uint32_t flags;
	uint32_t vib_type;
	uint32_t vib_rate;
	uint32_t vib_depth;
	uint32_t vib_speed;
	char name[32];
	char filename[22];
	int played; // for note playback dots
	uint32_t globalvol_saved; // for muting individual samples

	// This must be 12-bytes to work around a bug in some gcc4.2s (XXX why? what bug?)
	unsigned char adlib_bytes[12];
} song_sample_t;

typedef struct song_envelope {
	int32_t ticks[32];
	uint8_t values[32];
	int32_t nodes;
	int32_t loop_start;
	int32_t loop_end;
	int32_t sustain_start;
	int32_t sustain_end;
} song_envelope_t;

typedef struct song_instrument {
	uint32_t fadeout;
	uint32_t flags;
	uint32_t global_volume;
	uint32_t panning;
	uint8_t sample_map[128];
	uint8_t note_map[128];
	song_envelope_t vol_env;
	song_envelope_t pan_env;
	song_envelope_t pitch_env;
	uint32_t nna;
	uint32_t dct;
	uint32_t dca;
	uint32_t pan_swing;
	uint32_t vol_swing;
	uint32_t ifc;
	uint32_t ifr;
	int32_t midi_bank; // TODO split this?
	int32_t midi_program;
	uint32_t midi_channel_mask; // FIXME why is this a mask? why is a mask useful? does 2.15 use a mask?
	int32_t pitch_pan_separation;
	uint32_t pitch_pan_center;
	char name[32];
	char filename[16];
	int32_t played; // for note playback dots
} song_instrument_t;

// (TODO write decent descriptions of what the various volume
// variables are used for - are all of them *really* necessary?)
// (TODO also the majority of this is irrelevant outside of the "main" 64 channels;
// this struct should really only be holding the stuff actually needed for mixing)
typedef struct song_voice {
	// First 32-bytes: Most used mixing information: don't change it
	signed char * current_sample_data;
	uint32_t position; // sample position, fixed-point -- integer part
	uint32_t position_frac; // fractional part
	int32_t increment; // 16.16 fixed point, how much to add to position per sample-frame of output
	int32_t right_volume; // ?
	int32_t left_volume; // ?
	int32_t right_ramp; // ?
	int32_t left_ramp; // ?
	// 2nd cache line
	uint32_t length; // only to the end of the loop
	uint32_t flags;
	uint32_t old_flags;
	uint32_t loop_start; // loop or sustain, whichever is active
	uint32_t loop_end;
	int32_t right_ramp_volume; // ?
	int32_t left_ramp_volume; // ?
	int32_t strike; // decremented to zero. this affects how long the initial hit on the playback marks lasts (bigger dot in instrument and sample list windows)

	//int32_t filter_y1, filter_y2, filter_y3, filter_y4;
	//int32_t filter_a0, filter_b0, filter_b1;
	int32_t filter_y[MIX_MAX_CHANNELS][2];
	int32_t filter_a0, filter_b0, filter_b1;

	int32_t rofs, lofs; // ?
	int32_t ramp_length;
	// Information not used in the mixer
	int32_t right_volume_new, left_volume_new; // ?
	int32_t final_volume; // range 0-16384 (?), accounting for sample+channel+global+etc. volumes
	int32_t final_panning; // range 0-256 (but can temporarily exceed that range during calculations)
	int32_t volume, panning; // range 0-256 (?); these are the current values set for the channel
	int32_t fadeout_volume;
	int32_t frequency;
	int32_t c5speed;
	int32_t sample_freq; // only used on the info page (F5)
	int32_t portamento_target;
	song_instrument_t *ptr_instrument;      // these two suck, and should
	song_sample_t *ptr_sample;              // be replaced with numbers
	int32_t vol_env_position;
	int32_t pan_env_position;
	int32_t pitch_env_position;
	uint32_t master_channel; // nonzero = background/NNA voice, indicates what channel it "came from"
	uint32_t vu_meter;
    // TODO: As noted elsewhere, this means current channel volume.
	int32_t global_volume;
    // FIXME: Here instrument_volume means the value calculated from sample global volume and instrument global volume.
    //  And we miss a value for "running envelope volume" for the page_info
	int32_t instrument_volume;
	int32_t autovib_depth;
	uint32_t autovib_position, vibrato_position, tremolo_position, panbrello_position;
	// 16-bit members

	// these were `int', so I'm keeping them as `int32_t'.
	//   - paper
	int32_t vol_swing, pan_swing;
	uint16_t channel_panning;

	// formally 8-bit members
	uint32_t note; // the note that's playing
	uint32_t nna;
	uint32_t new_note, new_instrument; // ?
	// Effect memory and handling
	uint32_t n_command; // This sucks and needs to go away (dumb "flag" for arpeggio / tremor)
	uint32_t mem_vc_volslide; // Ax Bx Cx Dx (volume column)
	uint32_t mem_arpeggio; // Axx
	uint32_t mem_volslide; // Dxx
	uint32_t mem_pitchslide; // Exx Fxx (and Gxx maybe)
	int32_t mem_portanote; // Gxx (synced with mem_pitchslide if compat gxx is set)
	uint32_t mem_tremor; // Ixx
	uint32_t mem_channel_volslide; // Nxx
	uint32_t mem_offset; // final, combined yxx00h from Oxx and SAy
	uint32_t mem_panslide; // Pxx
	uint32_t mem_retrig; // Qxx
	uint32_t mem_special; // Sxx
	uint32_t mem_tempo; // Txx
	uint32_t mem_global_volslide; // Wxx
	uint32_t note_slide_counter, note_slide_speed, note_slide_step; // IMF effect
	uint32_t vib_type, vibrato_speed, vibrato_depth;
	uint32_t tremolo_type, tremolo_speed, tremolo_depth;
	uint32_t panbrello_type, panbrello_speed, panbrello_depth;
	int32_t tremolo_delta, panbrello_delta;

	uint32_t cutoff;
	uint32_t resonance;
	int32_t cd_note_delay; // countdown: note starts when this hits zero
	int32_t cd_note_cut; // countdown: note stops when this hits zero
	int32_t cd_retrig; // countdown: note retrigs when this hits zero
	uint32_t cd_tremor; // (weird) countdown + flag: see snd_fx.c and sndmix.c
	uint32_t patloop_row; // row number that SB0 was on
	uint32_t cd_patloop; // countdown: pattern loops back when this hits zero

	uint32_t row_note, row_instr;
	uint32_t row_voleffect, row_volparam;
	uint32_t row_effect, row_param;
	uint32_t active_macro, last_instrument;
} song_voice_t;

typedef struct song_channel {
	uint32_t panning;
	uint32_t volume;
	uint32_t flags;
} song_channel_t;

typedef struct song_note {
	uint8_t note;
	uint8_t instrument;
	uint8_t voleffect;
	uint8_t volparam;
	uint8_t effect;
	uint8_t param;
} song_note_t;

typedef struct song_history {
	int time_valid;

	// what time the file was opened
	struct tm time;

	// the amount of milliseconds the file was opened for
	schism_ticks_t runtime;
} song_history_t;

////////////////////////////////////////////////////////////////////

typedef struct {
	char start[32];
	char stop[32];
	char tick[32];
	char note_on[32];
	char note_off[32];
	char set_volume[32];
	char set_panning[32];
	char set_bank[32];
	char set_program[32];
	char sfx[16][32];
	char zxx[128][32];
} midi_config_t;

extern midi_config_t default_midi_config;


extern uint32_t max_voices;
extern uint32_t global_vu_left, global_vu_right;

extern const song_note_t blank_pattern[64 * 64];
extern const song_note_t *blank_note;


struct multi_write {
	int used;
	void *data;
	/* Conveniently, this has the same prototype as disko_write :) */
	void (*write)(void *data, const uint8_t *buf, size_t bytes);
	/* this is optimization for channels that haven't had any data yet
	(nothing to convert/write, just seek ahead in the data stream) */
	void (*silence)(void *data, long bytes);
	int32_t buffer[MIXBUFFERSIZE * 2];
};

typedef struct song {
	int32_t mix_buffer[MIXBUFFERSIZE * 2];

	song_voice_t voices[MAX_VOICES];                // Channels
	uint32_t voice_mix[MAX_VOICES];                 // Channels to be mixed
	song_sample_t samples[MAX_SAMPLES+1];           // Samples (1-based!)
	song_instrument_t *instruments[MAX_INSTRUMENTS+1]; // Instruments (1-based!)
	song_channel_t channels[MAX_CHANNELS];          // Channel settings
	song_note_t *patterns[MAX_PATTERNS];            // Patterns
	uint16_t pattern_size[MAX_PATTERNS];            // Pattern Lengths
	uint16_t pattern_alloc_size[MAX_PATTERNS];      // Allocated lengths (for async. resizing/playback)
	uint8_t orderlist[MAX_ORDERS + 1];              // Pattern Orders
	midi_config_t midi_config;                      // Midi macro config table
	uint32_t initial_speed;
	uint32_t initial_tempo;
	uint32_t initial_global_volume;
	uint32_t flags;                                 // Song flags SONG_XXXX
	uint32_t pan_separation;
	uint32_t num_voices; // how many are currently playing. (POTENTIALLY larger than global max_voices)
	uint32_t mix_stat; // number of channels being mixed (not really used)
	uint32_t buffer_count; // number of samples to mix per tick
	uint32_t tick_count;
	uint32_t frame_delay;
	int32_t row_count; /* IMPORTANT needs to be signed */
	uint32_t current_speed;
	uint32_t current_tempo;
	uint32_t process_row;
	uint32_t row; // no analogue in pm.h? should be either renamed or factored out.
	uint32_t break_row;
	uint32_t current_pattern;
	uint32_t current_order;
	uint32_t process_order;
	uint32_t current_global_volume;
	uint32_t mixing_volume;
	uint32_t freq_factor; // not used -- for tweaking the song speed LP-style (interesting!)
	uint32_t tempo_factor; // ditto
	int32_t repeat_count; // 0 = first playback, etc. (note: set to -1 to stop instead of looping)
	uint8_t row_highlight_major;
	uint8_t row_highlight_minor;
	char message[MAX_MESSAGE + 1];
	char title[32];

	// irrelevant to the song, just used by some loaders (fingerprint)
	// ...expanded this to the size of the log -paper
	char tracker_id[74];

	// These store the existing IT save history from prior editing sessions.
	// Current session data is added at save time, and is NOT a part of histdata.
	size_t histlen; // How many session history data entries exist (each entry is eight bytes)
	song_history_t *history; // Preserved entries from prior sessions, might be NULL if histlen = 0

	song_history_t editstart; // When the song was loaded

	// mixer stuff
	uint32_t mix_flags; // SNDMIX_*
	uint32_t mix_frequency, mix_bits_per_sample, mix_channels;

	int patloop; // effects.c: need this for stupid pattern break compatibility

	// noise reduction filter
	int32_t left_nr, right_nr;

	// chaseback
	int stop_at_order;
	int stop_at_row;
	unsigned int stop_at_time;

	// multi-write stuff -- NULL if no multi-write is in progress, else array of one struct per channel
	struct multi_write *multi_write;
} song_t;

song_note_t *csf_allocate_pattern(uint32_t rows);
void csf_free_pattern(void *pat);
signed char *csf_allocate_sample(uint32_t nbytes);
void csf_free_sample(void *p);
song_instrument_t *csf_allocate_instrument(void);
void csf_init_instrument(song_instrument_t *ins, int samp);
void csf_free_instrument(song_instrument_t *p);

uint32_t csf_read_sample(song_sample_t *sample, uint32_t flags, slurp_t *fp);
uint32_t csf_write_sample(disko_t *fp, song_sample_t *sample, uint32_t flags, uint32_t maxlengthmask);
void csf_adjust_sample_loop(song_sample_t *sample);

extern void (*csf_midi_out_note)(int chan, const song_note_t *m);
extern void (*csf_midi_out_raw)(const unsigned char *, uint32_t, uint32_t);

void csf_import_mod_effect(song_note_t *m, int from_xm);
uint16_t csf_export_mod_effect(const song_note_t *m, int xm);

void csf_import_s3m_effect(song_note_t *m, int it);
void csf_export_s3m_effect(uint8_t *pcmd, uint8_t *pprm, int it);


// counting stuff

int csf_note_is_empty(song_note_t *note);
int csf_pattern_is_empty(song_t *csf, int n);
int csf_sample_is_empty(song_sample_t *smp);
int csf_instrument_is_empty(song_instrument_t *ins);
int csf_last_order(song_t *csf); // last order of "main" song (IT-style, only for display)
int csf_get_num_orders(song_t *csf); // last non-blank order (for saving)
int csf_get_num_patterns(song_t *csf);
int csf_get_num_samples(song_t *csf);
int csf_get_num_instruments(song_t *csf);

// for these, 'start' indicates minimum sample/instrument to check
int csf_first_blank_sample(song_t *csf, int start);
int csf_first_blank_instrument(song_t *csf, int start);

int csf_get_highest_used_channel(song_t *csf);



int csf_set_wave_config(song_t *csf, uint32_t rate, uint32_t bits, uint32_t channels);

// Mixer Config
int32_t csf_init_player(song_t *csf, int reset); // bReset=false
int csf_set_resampling_mode(song_t *csf, uint32_t mode); // SRCMODE_XXXX


// sndmix
uint32_t csf_read(song_t *csf, void *v_buffer, uint32_t bufsize);
int32_t csf_process_tick(song_t *csf);
int32_t csf_read_note(song_t *csf);

// snd_fx
uint32_t csf_get_length(song_t *csf); // (in seconds)
void csf_instrument_change(song_t *csf, song_voice_t *chn, uint32_t instr, int porta, int instr_column);
void csf_note_change(song_t *csf, uint32_t chan, int note, int porta, int retrig, int have_inst);
uint32_t csf_get_nna_channel(song_t *csf, uint32_t chan);
void csf_check_nna(song_t *csf, uint32_t chan, uint32_t instr, int note, int force_cut);
void csf_process_effects(song_t *csf, int firsttick);
int32_t csf_fx_do_freq_slide(uint32_t flags, int32_t frequency, int32_t slide, int is_tone_portamento);

void fx_note_cut(song_t *csf, uint32_t chan, int clear_note);
void fx_key_off(song_t *csf, uint32_t chan);
void csf_midi_send(song_t *csf, const unsigned char *data, uint32_t len, uint32_t chan, int fake);
void csf_process_midi_macro(song_t *csf, uint32_t chan, const char *midi_macro, uint32_t param,
			uint32_t note, uint32_t velocity, uint32_t use_instr);
song_sample_t *csf_translate_keyboard(song_t *csf, song_instrument_t *ins, uint32_t note, song_sample_t *def);

// various utility functions in snd_fx.c
int32_t get_note_from_frequency(int32_t frequency, uint32_t c5speed);
int32_t get_frequency_from_note(int32_t note, uint32_t c5speed);
uint32_t transpose_to_frequency(int32_t transp, int32_t ftune);
int32_t frequency_to_transpose(uint32_t freq);
uint32_t calc_halftone(uint32_t hz, int32_t rel);

// sndfile
song_t *csf_allocate(void);
void csf_free(song_t *csf);

void csf_destroy(song_t *csf); /* erase everything -- equiv. to new song */
int csf_destroy_sample(song_t *csf, uint32_t smpnum);
void csf_precompute_sample_loops(song_sample_t *smp);

void csf_stop_sample(song_t *csf, song_sample_t *smp);

void csf_reset_midi_cfg(song_t *csf);
void csf_copy_midi_cfg(song_t *dest, song_t *src);
void csf_set_current_order(song_t *csf, uint32_t position);
void csf_loop_pattern(song_t *csf, int pattern, int start_row);
void csf_reset_playmarks(song_t *csf);

void csf_insert_restart_pos(song_t *csf, uint32_t restart_order); // hax

void csf_forget_history(song_t *csf); // Send the edit log down the memory hole.

/* apply a preset Adlib patch */
void adlib_patch_apply(song_sample_t *smp, int32_t patchnum);

///////////////////////////////////////////////////////////

// Return (a*b)/c - no divide error
static inline int32_t _muldiv(int32_t a, int32_t b, int32_t c)
{
	return ((int64_t) a * (int64_t) b ) / c;
}


// Return (a*b+c/2)/c - no divide error
static inline int32_t _muldivr(int32_t a, int32_t b, int32_t c)
{
	return ((int64_t) a * (int64_t) b + (c >> 1)) / c;
}

#endif /* SCHISM_PLAYER_SNDFILE_H_ */

