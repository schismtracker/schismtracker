/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#ifndef __SNDFILE_H
#define __SNDFILE_H

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#define NEED_BYTESWAP
#include "headers.h"

#ifdef __cplusplus
# include "diskwriter.h"
#endif

#include "tables.h"



#define MOD_AMIGAC2             0x1AB
#define MAX_SAMPLE_LENGTH       16000000
#define MAX_SAMPLE_RATE         192000
#define MAX_ORDERS              256
#define MAX_PATTERNS            240
#define MAX_SAMPLES             240
#define MAX_INSTRUMENTS         MAX_SAMPLES
#define MAX_VOICES              256
#define MAX_CHANNELS            64
#define MAX_ENVPOINTS           32
#define MAX_INFONAME            80
#define MAX_EQ_BANDS            6


#define MOD_TYPE_NONE           0x00
#define MOD_TYPE_MOD            0x01
#define MOD_TYPE_S3M            0x02
#define MOD_TYPE_XM             0x04
#define MOD_TYPE_MED            0x08
#define MOD_TYPE_MTM            0x10
#define MOD_TYPE_IT             0x20
#define MOD_TYPE_669            0x40
#define MOD_TYPE_ULT            0x80
#define MOD_TYPE_STM            0x100
#define MOD_TYPE_FAR            0x200
#define MOD_TYPE_WAV            0x400
#define MOD_TYPE_AMF            0x800
#define MOD_TYPE_AMS            0x1000
#define MOD_TYPE_DSM            0x2000
#define MOD_TYPE_MDL            0x4000
#define MOD_TYPE_OKT            0x8000
#define MOD_TYPE_MID            0x10000
#define MOD_TYPE_DMF            0x20000
#define MOD_TYPE_PTM            0x40000
#define MOD_TYPE_DBM            0x80000
#define MOD_TYPE_MT2            0x100000
#define MOD_TYPE_AMF0           0x200000
#define MOD_TYPE_PSM            0x400000
#define MOD_TYPE_UMX            0x80000000 // Fake type
#define MAX_MODTYPE             23



// Channel flags:
// Bits 0-7:    Sample Flags
#define CHN_16BIT               0x01
#define CHN_LOOP                0x02
#define CHN_PINGPONGLOOP        0x04
#define CHN_SUSTAINLOOP         0x08
#define CHN_PINGPONGSUSTAIN     0x10
#define CHN_PANNING             0x20
#define CHN_STEREO              0x40
#define CHN_PINGPONGFLAG        0x80
// Bits 8-31:   Channel Flags
#define CHN_MUTE                0x100
#define CHN_KEYOFF              0x200
#define CHN_NOTEFADE            0x400
#define CHN_SURROUND            0x800
#define CHN_NOIDO               0x1000
#define CHN_HQSRC               0x2000
#define CHN_FILTER              0x4000
#define CHN_VOLUMERAMP          0x8000
#define CHN_VIBRATO             0x10000
#define CHN_TREMOLO             0x20000
#define CHN_PANBRELLO           0x40000
#define CHN_PORTAMENTO          0x80000
#define CHN_GLISSANDO           0x100000
#define CHN_VOLENV              0x200000
#define CHN_PANENV              0x400000
#define CHN_PITCHENV            0x800000
#define CHN_FASTVOLRAMP         0x1000000
//#define CHN_EXTRALOUD         0x2000000
//#define CHN_REVERB            0x4000000
//#define CHN_NOREVERB          0x8000000
// used to turn off mute but have it reset later
#define CHN_NNAMUTE             0x10000000
// Another sample flag...
#define CHN_ADLIB               0x20000000 /* OPL mode */

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

#define CMD_NONE                0
#define CMD_ARPEGGIO            1
#define CMD_PORTAMENTOUP        2
#define CMD_PORTAMENTODOWN      3
#define CMD_TONEPORTAMENTO      4
#define CMD_VIBRATO             5
#define CMD_TONEPORTAVOL        6
#define CMD_VIBRATOVOL          7
#define CMD_TREMOLO             8
#define CMD_PANNING8            9
#define CMD_OFFSET              10
#define CMD_VOLUMESLIDE         11
#define CMD_POSITIONJUMP        12
#define CMD_VOLUME              13
#define CMD_PATTERNBREAK        14
#define CMD_RETRIG              15
#define CMD_SPEED               16
#define CMD_TEMPO               17
#define CMD_TREMOR              18
#define CMD_S3MCMDEX            20
#define CMD_CHANNELVOLUME       21
#define CMD_CHANNELVOLSLIDE     22
#define CMD_GLOBALVOLUME        23
#define CMD_GLOBALVOLSLIDE      24
#define CMD_KEYOFF              25
#define CMD_FINEVIBRATO         26
#define CMD_PANBRELLO           27
#define CMD_XFINEPORTAUPDOWN    28
#define CMD_PANNINGSLIDE        29
#define CMD_SETENVPOSITION      30
#define CMD_MIDI                31


// Volume Column commands
#define VOLCMD_VOLUME           1
#define VOLCMD_PANNING          2
#define VOLCMD_VOLSLIDEUP       3
#define VOLCMD_VOLSLIDEDOWN     4
#define VOLCMD_FINEVOLUP        5
#define VOLCMD_FINEVOLDOWN      6
#define VOLCMD_VIBRATOSPEED     7
#define VOLCMD_VIBRATO          8
#define VOLCMD_PANSLIDELEFT     9
#define VOLCMD_PANSLIDERIGHT    10
#define VOLCMD_TONEPORTAMENTO   11
#define VOLCMD_PORTAUP          12
#define VOLCMD_PORTADOWN        13

#define RSF_16BIT               0x04
#define RSF_STEREO              0x08

#define RS_PCM8S                0       // 8-bit signed
#define RS_PCM8U                1       // 8-bit unsigned
#define RS_PCM8D                2       // 8-bit delta values
#define RS_PCM16D               4       // 16-bit delta values
#define RS_PCM16S               5       // 16-bit signed
#define RS_PCM16U               6       // 16-bit unsigned
#define RS_PCM16M               7       // 16-bit motorola order
#define RS_STPCM8S              (RS_PCM8S|RSF_STEREO)  // stereo 8-bit signed
#define RS_STPCM8U              (RS_PCM8U|RSF_STEREO)  // stereo 8-bit unsigned
#define RS_STPCM8D              (RS_PCM8D|RSF_STEREO)  // stereo 8-bit delta values
#define RS_STPCM16S             (RS_PCM16S|RSF_STEREO) // stereo 16-bit signed
#define RS_STPCM16U             (RS_PCM16U|RSF_STEREO) // stereo 16-bit unsigned
#define RS_STPCM16D             (RS_PCM16D|RSF_STEREO) // stereo 16-bit delta values
#define RS_STPCM16M             (RS_PCM16M|RSF_STEREO) // stereo 16-bit signed big endian
// IT 2.14 compressed samples
#define RS_IT2148               0x10
#define RS_IT21416              0x14
#define RS_IT2158               0x12
#define RS_IT21516              0x16
// AMS Packed Samples
#define RS_AMS8                 0x11
#define RS_AMS16                0x15
// DMF Huffman compression
#define RS_DMF8                 0x13
#define RS_DMF16                0x17
// MDL Huffman compression
#define RS_MDL8                 0x20
#define RS_MDL16                0x24
#define RS_PTM8DTO16    0x25
// Stereo Interleaved Samples
#define RS_STIPCM8S             (RS_PCM8S|0x40|RSF_STEREO)      // stereo 8-bit signed
#define RS_STIPCM8U             (RS_PCM8U|0x40|RSF_STEREO)      // stereo 8-bit unsigned
#define RS_STIPCM16S            (RS_PCM16S|0x40|RSF_STEREO)     // stereo 16-bit signed
#define RS_STIPCM16U            (RS_PCM16U|0x40|RSF_STEREO)     // stereo 16-bit unsigned
#define RS_STIPCM16M            (RS_PCM16M|0x40|RSF_STEREO)     // stereo 16-bit signed big endian
// 24-bit signed
#define RS_PCM24S               (RS_PCM16S|0x80)                // mono 24-bit signed
#define RS_STIPCM24S            (RS_PCM16S|0x80|RSF_STEREO)     // stereo 24-bit signed
#define RS_PCM32S               (RS_PCM16S|0xC0)                // mono 24-bit signed
#define RS_STIPCM32S            (RS_PCM16S|0xC0|RSF_STEREO)     // stereo 24-bit signed

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

// DNA types
#define DNA_NOTECUT             0
#define DNA_NOTEOFF             1
#define DNA_NOTEFADE            2

// Module flags
#define SONG_EMBEDMIDICFG       0x0001
#define SONG_FASTVOLSLIDES      0x0002
#define SONG_ITOLDEFFECTS       0x0004
#define SONG_ITCOMPATMODE       0x0008
#define SONG_LINEARSLIDES       0x0010
#define SONG_PATTERNLOOP        0x0020
#define SONG_STEP               0x0040
#define SONG_PAUSED             0x0080
//#define SONG_FADINGSONG       0x0100
#define SONG_ENDREACHED         0x0200
//#define SONG_GLOBALFADE       0x0400
//#define SONG_CPUVERYHIGH      0x0800
#define SONG_FIRSTTICK          0x1000
#define SONG_MPTFILTERMODE      0x2000
#define SONG_SURROUNDPAN        0x4000
#define SONG_EXFILTERRANGE      0x8000
//#define SONG_AMIGALIMITS      0x10000
#define SONG_INSTRUMENTMODE     0x20000
#define SONG_ORDERLOCKED        0x40000
#define SONG_NOSTEREO           0x80000

// Global Options (Renderer)
#define SNDMIX_REVERSESTEREO    0x0001
#define SNDMIX_NOISEREDUCTION   0x0002
//#define SNDMIX_AGC            0x0004
#define SNDMIX_NORESAMPLING     0x0008
#define SNDMIX_HQRESAMPLER      0x0010
//#define SNDMIX_MEGABASS       0x0020
//#define SNDMIX_SURROUND       0x0040
//#define SNDMIX_REVERB         0x0080
#define SNDMIX_EQ               0x0100
//#define SNDMIX_SOFTPANNING    0x0200
#define SNDMIX_ULTRAHQSRCMODE   0x0400
// Misc Flags (can safely be turned on or off)
#define SNDMIX_DIRECTTODISK     0x10000
#define SNDMIX_NOBACKWARDJUMPS  0x40000
//#define SNDMIX_MAXDEFAULTPAN  0x80000 // (no longer) Used by the MOD loader
#define SNDMIX_MUTECHNMODE      0x100000 // Notes are not played on muted channels
#define SNDMIX_NOSURROUND       0x200000 // ignore S91
#define SNDMIX_NOMIXING         0x400000 // don't actually do any mixing (values only)
#define SNDMIX_NORAMPING        0x800000

enum {
	SRCMODE_NEAREST,
	SRCMODE_LINEAR,
	SRCMODE_SPLINE,
	SRCMODE_POLYPHASE,
	NUM_SRC_MODES
};


// Sample Struct
typedef struct _SONGSAMPLE
{
	uint32_t nLength,nLoopStart,nLoopEnd;
	uint32_t nSustainStart, nSustainEnd;
	signed char *pSample;
	uint32_t nC5Speed;
	uint32_t nPan;
	uint32_t nVolume;
	uint32_t nGlobalVol;
	uint32_t uFlags;
	uint32_t nVibType; // = type
	uint32_t nVibSweep; // = rate
	uint32_t nVibDepth; // = depth
	uint32_t nVibRate; // = speed
	char name[32];
	char filename[22];
	int played; // for note playback dots

	// This must be 12-bytes to work around a bug in some gcc4.2s
	unsigned char AdlibBytes[12];
} SONGSAMPLE;

typedef struct _INSTRUMENTENVELOPE {
	int Ticks[32];
	uint8_t Values[32];
	int nNodes;
	int nLoopStart;
	int nLoopEnd;
	int nSustainStart;
	int nSustainEnd;
} INSTRUMENTENVELOPE;


// Instrument Struct
typedef struct _SONGINSTRUMENT
{
	uint32_t nFadeOut;
	uint32_t dwFlags;
	unsigned int nGlobalVol;
	unsigned int nPan;
	unsigned int Keyboard[128];
	unsigned int NoteMap[128];
	INSTRUMENTENVELOPE VolEnv;
	INSTRUMENTENVELOPE PanEnv;
	INSTRUMENTENVELOPE PitchEnv;
	unsigned int nNNA;
	unsigned int nDCT;
	unsigned int nDNA;
	unsigned int nPanSwing;
	unsigned int nVolSwing;
	unsigned int nIFC;
	unsigned int nIFR;
	unsigned int wMidiBank;
	unsigned int nMidiProgram;
	unsigned int nMidiChannelMask;
	unsigned int nMidiDrumKey;
	int nPPS;
	unsigned int nPPC;
	char name[32];
	char filename[16];
	int played; // for note playback dots
} SONGINSTRUMENT;


// Channel Struct
typedef struct _SONGVOICE
{
	// First 32-bytes: Most used mixing information: don't change it
	signed char * pCurrentSample;
	uint32_t nPos;
	uint32_t nPosLo;   // actually 16-bit
	unsigned int topnote_offset;
	int32_t nInc;              // 16.16
	int32_t nRightVol;
	int32_t nLeftVol;
	int32_t nRightRamp;
	int32_t nLeftRamp;
	// 2nd cache line
	uint32_t nLength;
	uint32_t dwFlags;
	uint32_t nLoopStart;
	uint32_t nLoopEnd;
	int32_t nRampRightVol;
	int32_t nRampLeftVol;
	int32_t strike; // decremented to zero

	double nFilter_Y1, nFilter_Y2, nFilter_Y3, nFilter_Y4;
	double nFilter_A0, nFilter_B0, nFilter_B1;

	int32_t nROfs, nLOfs;
	int32_t nRampLength;
	// Information not used in the mixer
	signed char * pSample;
	int32_t nNewRightVol, nNewLeftVol;
	int32_t nRealVolume, nRealPan;
	int32_t nVolume, nPan, nFadeOutVol;
	int32_t nPeriod, nC5Speed, sample_freq, nPortamentoDest;
	SONGINSTRUMENT *pHeader;
	SONGSAMPLE *pInstrument;
	int nVolEnvPosition, nPanEnvPosition, nPitchEnvPosition;
	uint32_t nMasterChn, nVUMeter;
	int32_t nGlobalVol, nInsVol;
	int32_t nPortamentoSlide, nAutoVibDepth;
	uint32_t nAutoVibPos, nVibratoPos, nTremoloPos, nPanbrelloPos;
	// 16-bit members
	int nVolSwing, nPanSwing;

	// formally 8-bit members
	unsigned int nNote, nNNA;
	unsigned int nNewNote, nNewIns, nCommand, nArpeggio;
	unsigned int nOldVolumeSlide, nOldFineVolUpDown;
	unsigned int nOldGlbVolSlide;
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
	unsigned int nLeftVU, nRightVU;
	unsigned int nActiveMacro, nLastInstr;
	unsigned int nTickStart;
	unsigned int nRealtime;
	uint8_t stupid_gcc_workaround;

} SONGVOICE;


typedef struct _MODCHANNELSETTINGS
{
	uint32_t nPan;
	uint32_t nVolume;
	uint32_t dwFlags;
} MODCHANNELSETTINGS;


typedef struct _MODCOMMAND
{
	uint8_t note;
	uint8_t instr;
	uint8_t volcmd;
	uint8_t command;
	uint8_t vol;
	uint8_t param;
} MODCOMMAND, *LPMODCOMMAND;

////////////////////////////////////////////////////////////////////

enum {
	MIDIOUT_START=0,
	MIDIOUT_STOP,
	MIDIOUT_TICK,
	MIDIOUT_NOTEON,
	MIDIOUT_NOTEOFF,
	MIDIOUT_VOLUME,
	MIDIOUT_PAN,
	MIDIOUT_BANKSEL,
	MIDIOUT_PROGRAM,
};


typedef struct MODMIDICFG
{
	char szMidiGlb[9*32];
	char szMidiSFXExt[16*32];
	char szMidiZXXExt[128*32];
} MODMIDICFG, *LPMODMIDICFG;


#include "snd_fx.h" // blah

#ifdef __cplusplus
extern "C" {
#endif

MODCOMMAND *csf_allocate_pattern(uint32_t rows, uint32_t channels);
void csf_free_pattern(void *pat);
signed char *csf_allocate_sample(uint32_t nbytes);
void csf_free_sample(void *p);

void csf_adjust_sample_loop(SONGSAMPLE *pIns);

#ifdef __cplusplus
} /* extern "C" */
#endif


#ifdef __cplusplus

class CSoundFile;

extern "C" {

int csf_set_wave_config(CSoundFile *csf, uint32_t nRate,uint32_t nBits,uint32_t nChannels);
int csf_set_wave_config_ex(CSoundFile *csf, bool,bool bNoOverSampling,bool,bool hqido,bool,bool bNR,bool bEQ);

// Mixer Config
int csf_init_player(CSoundFile *csf, int reset); // bReset=false
int csf_set_resampling_mode(CSoundFile *csf, uint32_t nMode); // SRCMODE_XXXX


// sndmix
int csf_fade_song(CSoundFile *csf, unsigned int msec);
int csf_global_fade_song(CSoundFile *csf, unsigned int msec);
unsigned int csf_read(CSoundFile *csf, void * lpDestBuffer, unsigned int cbBuffer);
int csf_process_row(CSoundFile *csf);
int csf_read_note(CSoundFile *csf);

// snd_dsp
void csf_initialize_dsp(CSoundFile *csf, int reset);
void csf_process_stereo_dsp(CSoundFile *csf, int count);
void csf_process_mono_dsp(CSoundFile *csf, int count);

// snd_fx
unsigned int csf_get_length(CSoundFile *csf);
void csf_instrument_change(CSoundFile *csf, SONGVOICE *pChn, uint32_t instr,
                           bool bPorta, bool bUpdVol, bool bResetEnv);
void csf_note_change(CSoundFile *csf, uint32_t nChn, int note, bool bPorta, bool bResetEnv, bool bManual);
uint32_t csf_get_nna_channel(CSoundFile *csf, uint32_t nChn);
void csf_check_nna(CSoundFile *csf, uint32_t nChn, uint32_t instr, int note, bool bForceCut);
bool csf_process_effects(CSoundFile *csf);

void fx_note_cut(CSoundFile *csf, uint32_t nChn, uint32_t nTick);
void fx_key_off(CSoundFile *csf, uint32_t nChn);
void csf_midi_send(CSoundFile *csf, const unsigned char *data, unsigned int len, uint32_t nChn, int fake);
void csf_process_midi_macro(CSoundFile *csf, uint32_t nChn, const char * pszMidiMacro, uint32_t param,
			uint32_t note, uint32_t velocity, uint32_t use_instr);

// sndfile
void csf_reset_midi_cfg(CSoundFile *csf);
uint32_t csf_get_num_orders(CSoundFile *csf);
void csf_set_current_order(CSoundFile *csf, uint32_t nPos);
void csf_loop_pattern(CSoundFile *csf, int nPat, int nRow);
void csf_reset_timestamps(CSoundFile *csf);

uint32_t csf_get_highest_used_channel(CSoundFile *csf);
uint32_t csf_detect_unused_samples(CSoundFile *csf, bool *pbIns);
void csf_destroy(CSoundFile *csf);
bool csf_destroy_sample(CSoundFile *csf, uint32_t nSample);

// fastmix
unsigned int csf_create_stereo_mix(CSoundFile *csf, int count);

} // extern "C"


//==============
class CSoundFile
//==============
{
public: // Static Members
	static uint32_t m_nMaxMixChannels;
	static int32_t m_nStreamVolume;
	static uint32_t gdwSoundSetup, gdwMixingFreq, gnBitsPerSample, gnChannels;
	static uint32_t gnVolumeRampSamples;
	static uint32_t gnVULeft, gnVURight;

public: // for Editing
	SONGVOICE Voices[MAX_VOICES];                                   // Channels
	uint32_t VoiceMix[MAX_VOICES];                                              // Channels to be mixed
	SONGSAMPLE Samples[MAX_SAMPLES];                                 // Instruments
	SONGINSTRUMENT *Instruments[MAX_INSTRUMENTS];             // Instrument Instruments
	MODCHANNELSETTINGS Channels[MAX_CHANNELS]; // Channels settings
	MODCOMMAND *Patterns[MAX_PATTERNS];                             // Patterns
	uint16_t PatternSize[MAX_PATTERNS];                                 // Patterns Lengths
	uint16_t PatternAllocSize[MAX_PATTERNS];                            // Allocated pattern lengths (for async. resizing/playback)
	uint8_t Orderlist[MAX_ORDERS];                                                 // Pattern Orders
	MODMIDICFG m_MidiCfg;                                                   // Midi macro config table
	uint32_t m_nDefaultSpeed, m_nDefaultTempo, m_nDefaultGlobalVolume;
	uint32_t m_dwSongFlags;                                                    // Song flags SONG_XXXX
	uint32_t m_nStereoSeparation;
	uint32_t m_nChannels, m_nMixChannels, m_nMixStat, m_nBufferCount;
	uint32_t m_nType, m_nSamples, m_nInstruments;
	uint32_t m_nTickCount, m_nCurrentPatternDelay, m_nFrameDelay;
	uint32_t m_nMusicSpeed, m_nMusicTempo;
	uint32_t m_nNextRow, m_nRow;
	uint32_t m_nCurrentPattern,m_nCurrentOrder,m_nNextOrder,m_nLockedOrder,m_nRestartPos;
	uint32_t m_nGlobalVolume, m_nSongPreAmp;
	uint32_t m_nFreqFactor, m_nTempoFactor;
	int32_t m_nRepeatCount, m_nInitialRepeatCount;
	uint8_t m_rowHighlightMajor, m_rowHighlightMinor;
	char * m_lpszSongComments;
	char song_title[32];

	// chaseback
	int stop_at_order;
	int stop_at_row;
	unsigned int stop_at_time;

	static MODMIDICFG m_MidiCfgDefault;   // Midi macro config table
public:
	CSoundFile();
	~CSoundFile();

public:
	bool Create(const uint8_t * lpStream, uint32_t dwMemLength=0);
	uint32_t GetNumPatterns() const;
	uint32_t GetNumInstruments() const;
	// Module Loaders
	bool ReadXM(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadS3M(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadMod(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadMed(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadMTM(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadSTM(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadIT(const uint8_t * lpStream, uint32_t dwMemLength);
	bool Read669(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadUlt(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadDSM(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadFAR(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadAMS(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadAMS2(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadMDL(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadOKT(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadDMF(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadPTM(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadDBM(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadAMF(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadMT2(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadPSM(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadUMX(const uint8_t * lpStream, uint32_t dwMemLength);
	bool ReadMID(const uint8_t * lpStream, uint32_t dwMemLength);
	// Save Functions
	uint32_t WriteSample(diskwriter_driver_t *f, SONGSAMPLE *pins, uint32_t nFlags, uint32_t nMaxLen=0);
	bool SaveXM(diskwriter_driver_t *f, uint32_t);
	bool SaveS3M(diskwriter_driver_t *f, uint32_t);
	bool SaveMod(diskwriter_driver_t *f, uint32_t);
	// MOD Convert function
	void ConvertModCommand(MODCOMMAND *m, bool from_xm) const;
	void S3MConvert(MODCOMMAND *m, bool bIT) const;
	void S3MSaveConvert(uint32_t *pcmd, uint32_t *pprm, bool bIT) const;
	uint16_t ModSaveCommand(const MODCOMMAND *m, bool bXM) const;
public:
	// backhooks :)
	static void (*_midi_out_note)(int chan, const MODCOMMAND *m);
	static void (*_midi_out_raw)(const unsigned char *,unsigned int, unsigned int);
	static void (*_multi_out_raw)(int chan, int *buf, int len);

public:
	uint32_t ReadSample(SONGSAMPLE *pIns, uint32_t nFlags, const char * pMemFile, uint32_t dwMemLength);

private:
    /* CSoundFile is a sentinel, prevent copying to avoid memory leaks */
    CSoundFile(const CSoundFile&);
    void operator=(const CSoundFile&);
};


#endif


///////////////////////////////////////////////////////////
// Low-level Mixing functions

#define MIXBUFFERSIZE           512
#define MIXING_ATTENUATION      5
#define MIXING_CLIPMIN          (-0x04000000)
#define MIXING_CLIPMAX          (0x03FFFFFF)
#define VOLUMERAMPPRECISION     12
#define FADESONGDELAY           100
#define EQ_BUFFERSIZE           (MIXBUFFERSIZE)

#define MOD2XMFineTune(k)       ((int)( (signed char)((k)<<4) ))
#define XM2MODFineTune(k)       ((int)( (k>>4)&0x0f ))

// Return (a*b)/c - no divide error
static inline int _muldiv(int a, int b, int c)
{
	return ((unsigned long long) a * (unsigned long long) b ) / c;
}


// Return (a*b+c/2)/c - no divide error
static inline int _muldivr(int a, int b, int c)
{
	return ((unsigned long long) a * (unsigned long long) b + (c >> 1)) / c;
}


#endif

