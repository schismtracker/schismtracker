/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#ifdef __cplusplus
#include "diskwriter.h"
#endif

#ifndef __SNDFILE_H
#define __SNDFILE_H


#define MOD_AMIGAC2             0x1AB
#define MAX_SAMPLE_LENGTH       16000000
#define MAX_SAMPLE_RATE         192000
#define MAX_ORDERS              256
#define MAX_PATTERNS            240
#define MAX_SAMPLES             240
#define MAX_INSTRUMENTS         MAX_SAMPLES
#define MAX_CHANNELS            256
#define MAX_BASECHANNELS        64
#define MAX_ENVPOINTS           32
#define MAX_INFONAME            80
#define MAX_EQ_BANDS            6
#define MAX_MIXPLUGINS          8


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
#define RS_ADPCM4               3       // 4-bit ADPCM-packed
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
typedef struct _MODINSTRUMENT
{
	uint32_t nLength,nLoopStart,nLoopEnd;
	uint32_t nSustainStart, nSustainEnd;
	signed char *pSample;
	uint32_t nC5Speed;
	uint32_t nPan;
	uint32_t nVolume;
	uint32_t nGlobalVol;
	uint32_t uFlags;
	uint32_t nVibType;
	uint32_t nVibSweep;
	uint32_t nVibDepth;
	uint32_t nVibRate;
	int8_t name[22];
	int played; // for note playback dots

	// This must be 12-bytes to work around a bug in some gcc4.2s
	unsigned char AdlibBytes[12];
} MODINSTRUMENT;

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
typedef struct _INSTRUMENTHEADER
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
	int8_t name[32];
	int8_t filename[12];
	int played; // for note playback dots
} INSTRUMENTHEADER;


// Channel Struct
typedef struct _MODCHANNEL
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
	INSTRUMENTHEADER *pHeader;
	MODINSTRUMENT *pInstrument;
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

} MODCHANNEL;


typedef struct _MODCHANNELSETTINGS
{
	uint32_t nPan;
	uint32_t nVolume;
	uint32_t dwFlags;
	uint32_t nMixPlugin;
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
// Mix Plugins
#define MIXPLUG_MIXREADY                        0x01    // Set when cleared

#ifdef __cplusplus
class IMixPlugin
{
public:
	virtual ~IMixPlugin() = 0;
	virtual int AddRef() = 0;
	virtual int Release() = 0;
	virtual void SaveAllParameters() = 0;
	virtual void RestoreAllParameters() = 0;
	virtual void Process(float *pOutL, float *pOutR, unsigned int nSamples) = 0;
	virtual void Init(unsigned int nFreq, int bReset) = 0;
	virtual void MidiSend(uint32_t dwMidiCode) = 0;
	virtual void MidiCommand(uint32_t nMidiCh, uint32_t nMidiProg, uint32_t note, uint32_t vol) = 0;
};
#endif

#define MIXPLUG_INPUTF_MASTEREFFECT             0x01    // Apply to master mix
#define MIXPLUG_INPUTF_BYPASS                   0x02    // Bypass effect
#define MIXPLUG_INPUTF_WETMIX                   0x04    // Wet Mix (dry added)

typedef struct _SNDMIXPLUGINSTATE
{
	uint32_t dwFlags;                                  // MIXPLUG_XXXX
	int32_t nVolDecayL, nVolDecayR;    // Buffer click removal
	int *pMixBuffer;                                // Stereo effect send buffer
	float *pOutBufferL;                             // Temp storage for int -> float conversion
	float *pOutBufferR;
} SNDMIXPLUGINSTATE, *PSNDMIXPLUGINSTATE;

typedef struct _SNDMIXPLUGININFO
{
	uint32_t dwPluginId1;
	uint32_t dwPluginId2;
	uint32_t dwInputRouting;   // MIXPLUG_INPUTF_XXXX
	uint32_t dwOutputRouting;  // 0=mix 0x80+=fx
	uint32_t dwReserved[4];    // Reserved for routing info
	int8_t szName[32];
	int8_t szLibraryName[64]; // original DLL name
} SNDMIXPLUGININFO, *PSNDMIXPLUGININFO; // Size should be 128

#ifdef __cplusplus
typedef struct _SNDMIXPLUGIN
{
	IMixPlugin *pMixPlugin;
	PSNDMIXPLUGINSTATE pMixState;
	uint32_t nPluginDataSize;
	PVOID pPluginData;
	SNDMIXPLUGININFO Info;
} SNDMIXPLUGIN, *PSNDMIXPLUGIN;

typedef bool (*PMIXPLUGINCREATEPROC)(PSNDMIXPLUGIN);
#endif

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
	char szMidiGlb[9*32];      // changed from int8_t
	char szMidiSFXExt[16*32];  // changed from int8_t
	char szMidiZXXExt[128*32]; // changed from int8_t
} MODMIDICFG, *LPMODMIDICFG;


typedef void (* LPSNDMIXHOOKPROC)(int *, unsigned int, unsigned int); // buffer, samples, channels


#ifdef __cplusplus




//==============
class CSoundFile
//==============
{
public: // Static Members
	static uint32_t m_nMaxMixChannels;
	static int32_t m_nStreamVolume;
	static uint32_t gdwSysInfo, gdwSoundSetup, gdwMixingFreq, gnBitsPerSample, gnChannels;
	static uint32_t gnVolumeRampSamples;
	static uint32_t gnVULeft, gnVURight;
	static LPSNDMIXHOOKPROC gpSndMixHook;
	static PMIXPLUGINCREATEPROC gpMixPluginCreateProc;

public: // for Editing
	MODCHANNEL Chn[MAX_CHANNELS];                                   // Channels
	uint32_t ChnMix[MAX_CHANNELS];                                              // Channels to be mixed
	MODINSTRUMENT Ins[MAX_SAMPLES];                                 // Instruments
	INSTRUMENTHEADER *Headers[MAX_INSTRUMENTS];             // Instrument Headers
	MODCHANNELSETTINGS ChnSettings[MAX_BASECHANNELS]; // Channels settings
	MODCOMMAND *Patterns[MAX_PATTERNS];                             // Patterns
	uint16_t PatternSize[MAX_PATTERNS];                                 // Patterns Lengths
	uint16_t PatternAllocSize[MAX_PATTERNS];                            // Allocated pattern lengths (for async. resizing/playback)
	uint8_t Order[MAX_ORDERS];                                                 // Pattern Orders
	MODMIDICFG m_MidiCfg;                                                   // Midi macro config table
	SNDMIXPLUGIN m_MixPlugins[MAX_MIXPLUGINS];              // Mix plugins
	uint32_t m_nDefaultSpeed, m_nDefaultTempo, m_nDefaultGlobalVolume;
	uint32_t m_dwSongFlags;                                                    // Song flags SONG_XXXX
	uint32_t m_nStereoSeparation;
	uint32_t m_nChannels, m_nMixChannels, m_nMixStat, m_nBufferCount;
	uint32_t m_nType, m_nSamples, m_nInstruments;
	uint32_t m_nTickCount, m_nTotalCount, m_nPatternDelay, m_nFrameDelay;
	uint32_t m_nMusicSpeed, m_nMusicTempo;
	uint32_t m_nNextRow, m_nRow;
	uint32_t m_nPattern,m_nCurrentPattern,m_nNextPattern,m_nLockedPattern,m_nRestartPos;
	uint32_t m_nGlobalVolume, m_nSongPreAmp;
	uint32_t m_nFreqFactor, m_nTempoFactor, m_nOldGlbVolSlide;
	int32_t m_nRepeatCount, m_nInitialRepeatCount;
	uint8_t m_rowHighlightMajor, m_rowHighlightMinor;
	LPSTR m_lpszSongComments;
	char m_szNames[MAX_INSTRUMENTS][32];    // changed from int8_t
	int8_t CompressionTable[16];

	// chaseback
	int stop_at_order;
	int stop_at_row;
	unsigned int stop_at_time;

	static MODMIDICFG m_MidiCfgDefault;                                                     // Midi macro config table
public:
	CSoundFile();
	~CSoundFile();

public:
	bool Create(LPCBYTE lpStream, uint32_t dwMemLength=0);
	bool Destroy();
	uint32_t GetHighestUsedChannel();
	uint32_t GetType() const { return m_nType; }
	uint32_t GetLogicalChannels() const { return m_nChannels; }
	uint32_t GetNumPatterns() const;
	uint32_t GetNumInstruments() const;
	uint32_t GetNumSamples() const { return m_nSamples; }
	uint32_t GetCurrentPos() const;
	uint32_t GetCurrentPattern() const { return m_nPattern; }
	uint32_t GetCurrentOrder() const { return m_nCurrentPattern; }
	uint32_t GetMaxPosition() const;
	void SetCurrentPos(uint32_t nPos);
	void SetCurrentOrder(uint32_t nOrder);
	void GetTitle(LPSTR s) const { strncpy(s,m_szNames[0],32); }
	LPCSTR GetTitle() const { return m_szNames[0]; }
	uint32_t GetMusicSpeed() const { return m_nMusicSpeed; }
	uint32_t GetMusicTempo() const { return m_nMusicTempo; }
	unsigned int GetLength(bool bAdjust, bool bTotal=false);
	unsigned int GetSongTime() { return GetLength(false, true); }
	void SetRepeatCount(int n) { m_nRepeatCount = n; m_nInitialRepeatCount = n; }
	int GetRepeatCount() const { return m_nRepeatCount; }
	bool IsPaused() const { return (m_dwSongFlags & SONG_PAUSED) ? true : false; }
	void LoopPattern(int nPat, int nRow=0);
	// Module Loaders
	bool ReadXM(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadS3M(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadMod(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadMed(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadMTM(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadSTM(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadIT(LPCBYTE lpStream, uint32_t dwMemLength);
	bool Read669(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadUlt(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadWav(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadDSM(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadFAR(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadAMS(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadAMS2(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadMDL(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadOKT(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadDMF(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadPTM(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadDBM(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadAMF(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadMT2(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadPSM(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadUMX(LPCBYTE lpStream, uint32_t dwMemLength);
	bool ReadMID(LPCBYTE lpStream, uint32_t dwMemLength);
	// Save Functions
	uint32_t WriteSample(diskwriter_driver_t *f, MODINSTRUMENT *pins, uint32_t nFlags, uint32_t nMaxLen=0);
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
	// Real-time sound functions
	void ResetChannels();

	uint32_t CreateStereoMix(int count);
	uint32_t GetTotalTickCount() const { return m_nTotalCount; }
	void ResetTotalTickCount() { m_nTotalCount = 0; }

public:
	static bool IsStereo() { return (gnChannels > 1) ? true : false; }
	static uint32_t GetSampleRate() { return gdwMixingFreq; }
	static uint32_t GetBitsPerSample() { return gnBitsPerSample; }
	static uint32_t GetSysInfo() { return gdwSysInfo; }
	uint32_t InitSysInfo();

public:
	bool ProcessEffects();
	uint32_t GetNNAChannel(uint32_t nChn);
	void CheckNNA(uint32_t nChn, uint32_t instr, int note, bool bForceCut);
	void NoteChange(uint32_t nChn, int note, bool bPorta=false, bool bResetEnv=true, bool bManual=false);
	void InstrumentChange(MODCHANNEL *pChn, uint32_t instr, bool bPorta=false,bool bUpdVol=true,bool bResetEnv=true);
	void TranslateKeyboard(INSTRUMENTHEADER* penv, uint32_t note, MODINSTRUMENT*& psmp);
	// Channel Effects
	void PortamentoUp(MODCHANNEL *pChn, uint32_t param);
	void PortamentoDown(MODCHANNEL *pChn, uint32_t param);
	void FinePortamentoUp(MODCHANNEL *pChn, uint32_t param);
	void FinePortamentoDown(MODCHANNEL *pChn, uint32_t param);
	void ExtraFinePortamentoUp(MODCHANNEL *pChn, uint32_t param);
	void ExtraFinePortamentoDown(MODCHANNEL *pChn, uint32_t param);
	void TonePortamento(MODCHANNEL *pChn, uint32_t param);
	void Vibrato(MODCHANNEL *pChn, uint32_t param);
	void FineVibrato(MODCHANNEL *pChn, uint32_t param);
	void VolumeSlide(MODCHANNEL *pChn, uint32_t param);
	void PanningSlide(MODCHANNEL *pChn, uint32_t param);
	void ChannelVolSlide(MODCHANNEL *pChn, uint32_t param);
	void FineVolumeUp(MODCHANNEL *pChn, uint32_t param);
	void FineVolumeDown(MODCHANNEL *pChn, uint32_t param);
	void Tremolo(MODCHANNEL *pChn, uint32_t param);
	void Panbrello(MODCHANNEL *pChn, uint32_t param);
	void RetrigNote(uint32_t nChn, uint32_t param);
	void NoteCut(uint32_t nChn, uint32_t nTick);
	void KeyOff(uint32_t nChn);
	int PatternLoop(MODCHANNEL *, uint32_t param);
	void ExtendedS3MCommands(uint32_t nChn, uint32_t param);
	void ExtendedChannelEffect(MODCHANNEL *, uint32_t param);
	void MidiSend(const unsigned char *data, unsigned int len, uint32_t nChn=0, int fake = 0);
	void ProcessMidiMacro(uint32_t nChn, LPCSTR pszMidiMacro, uint32_t param=0,
			uint32_t note=0, uint32_t velocity=0, uint32_t use_instr=0);
	//void SetupChannelFilter(MODCHANNEL *pChn, bool bReset, int flt_modifier=256,int freq=0) const;
	// Low-Level effect processing
	void DoFreqSlide(MODCHANNEL *pChn, int32_t nFreqSlide);
	// Global Effects
	void SetTempo(uint32_t param);
	void SetSpeed(uint32_t param);
	void GlobalVolSlide(uint32_t param);
	uint32_t IsSongFinished(uint32_t nOrder, uint32_t nRow) const;
	bool IsValidBackwardJump(uint32_t nStartOrder, uint32_t nStartRow, uint32_t nJumpOrder, uint32_t nJumpRow) const;
	// Read/Write sample functions
	signed char GetDeltaValue(signed char prev, uint32_t n) const { return (signed char)(prev + CompressionTable[n & 0x0F]); }
	uint32_t ReadSample(MODINSTRUMENT *pIns, uint32_t nFlags, LPCSTR pMemFile, uint32_t dwMemLength);
	bool DestroySample(uint32_t nSample);
	bool DestroyInstrument(uint32_t nInstr);
	bool IsSampleUsed(uint32_t nSample);
	bool IsInstrumentUsed(uint32_t nInstr);
	bool RemoveInstrumentSamples(uint32_t nInstr);
	uint32_t DetectUnusedSamples(bool *);
	bool RemoveSelectedSamples(bool *);
	void AdjustSampleLoop(MODINSTRUMENT *pIns);
	// I/O from another sound file
	bool ReadInstrumentFromSong(uint32_t nInstr, CSoundFile *, uint32_t nSrcInstrument);
	bool ReadSampleFromSong(uint32_t nSample, CSoundFile *, uint32_t nSrcSample);
	// Period/Note functions
	uint32_t GetPeriodFromNote(uint32_t note, int nFineTune, uint32_t nC5Speed) const;
	uint32_t GetFreqFromPeriod(uint32_t period, uint32_t nC5Speed, int nPeriodFrac=0) const;
	// Misc functions
	MODINSTRUMENT *GetSample(uint32_t n) { return Ins+n; }
	void ResetMidiCfg();
	uint32_t MapMidiInstrument(uint32_t dwProgram, uint32_t nChannel, uint32_t nNote);
	bool ITInstrToMPT(const void *p, INSTRUMENTHEADER *penv, uint32_t trkvers);
	uint32_t SaveMixPlugins(FILE *f=NULL, bool bUpdate=true);
	uint32_t LoadMixPlugins(const void *pData, uint32_t nLen);
	void ResetTimestamps(); // for note playback dots

	// System-Dependant functions
public:
	static MODCOMMAND *AllocatePattern(uint32_t rows, uint32_t nchns);
	static signed char* AllocateSample(uint32_t nbytes);
	static void FreePattern(LPVOID pat);
	static void FreeSample(LPVOID p);
	static uint32_t Normalize24BitBuffer(LPBYTE pbuffer, uint32_t cbsizebytes, uint32_t lmax24, uint32_t dwByteInc);

private:
    /* CSoundFile is a sentinel, prevent copying to avoid memory leaks */
    CSoundFile(const CSoundFile&);
    void operator=(const CSoundFile&);
};

int csf_set_wave_config(CSoundFile *csf, uint32_t nRate,uint32_t nBits,uint32_t nChannels);
int csf_set_wave_config_ex(CSoundFile *csf, bool,bool bNoOverSampling,bool,bool hqido,bool,bool bNR,bool bEQ);

// Mixer Config
int csf_init_player(CSoundFile *csf, int reset); // bReset=false
int csf_set_resampling_mode(CSoundFile *csf, uint32_t nMode); // SRCMODE_XXXX


// sndmix
int csf_fade_song(CSoundFile *csf, unsigned int msec);
int csf_global_fade_song(CSoundFile *csf, unsigned int msec);
unsigned int csf_read(CSoundFile *csf, LPVOID lpDestBuffer, unsigned int cbBuffer);
int csf_process_row(CSoundFile *csf);
int csf_read_note(CSoundFile *csf);

// snd_dsp
void csf_initialize_dsp(CSoundFile *csf, int reset);
void csf_process_stereo_dsp(CSoundFile *csf, int count);
void csf_process_mono_dsp(CSoundFile *csf, int count);

#endif

//////////////////////////////////////////////////////////
// WAVE format information

#pragma pack(1)

// Standard IFF chunks IDs
#define IFFID_FORM              0x4d524f46
#define IFFID_RIFF              0x46464952
#define IFFID_WAVE              0x45564157
#define IFFID_LIST              0x5453494C
#define IFFID_INFO              0x4F464E49

// IFF Info fields
#define IFFID_ICOP              0x504F4349
#define IFFID_IART              0x54524149
#define IFFID_IPRD              0x44525049
#define IFFID_INAM              0x4D414E49
#define IFFID_ICMT              0x544D4349
#define IFFID_IENG              0x474E4549
#define IFFID_ISFT              0x54465349
#define IFFID_ISBJ              0x4A425349
#define IFFID_IGNR              0x524E4749
#define IFFID_ICRD              0x44524349

// Wave IFF chunks IDs
#define IFFID_wave              0x65766177
#define IFFID_fmt               0x20746D66
#define IFFID_wsmp              0x706D7377
#define IFFID_pcm               0x206d6370
#define IFFID_data              0x61746164
#define IFFID_smpl              0x6C706D73
#define IFFID_xtra              0x61727478

typedef struct WAVEFILEHEADER
{
	uint32_t id_RIFF;          // "RIFF"
	uint32_t filesize;         // file length-8
	uint32_t id_WAVE;
} WAVEFILEHEADER;


typedef struct WAVEFORMATHEADER
{
	uint32_t id_fmt;           // "fmt "
	uint32_t hdrlen;           // 16
	uint16_t format;            // 1
	uint16_t channels;          // 1:mono, 2:stereo
	uint32_t freqHz;           // sampling freq
	uint32_t bytessec;         // bytes/sec=freqHz*samplesize
	uint16_t samplesize;        // sizeof(sample)
	uint16_t bitspersample;     // bits per sample (8/16)
} WAVEFORMATHEADER;


typedef struct WAVEDATAHEADER
{
	uint32_t id_data;          // "data"
	uint32_t length;           // length of data
} WAVEDATAHEADER;


typedef struct WAVESMPLHEADER
{
	// SMPL
	uint32_t smpl_id;          // "smpl"       -> 0x6C706D73
	uint32_t smpl_len;         // length of smpl: 3Ch  (54h with sustain loop)
	uint32_t dwManufacturer;
	uint32_t dwProduct;
	uint32_t dwSamplePeriod;   // 1000000000/freqHz
	uint32_t dwBaseNote;       // 3Ch = C-4 -> 60 + RelativeTone
	uint32_t dwPitchFraction;
	uint32_t dwSMPTEFormat;
	uint32_t dwSMPTEOffset;
	uint32_t dwSampleLoops;    // number of loops
	uint32_t cbSamplerData;
} WAVESMPLHEADER;


typedef struct SAMPLELOOPSTRUCT
{
	uint32_t dwIdentifier;
	uint32_t dwLoopType;               // 0=normal, 1=bidi
	uint32_t dwLoopStart;
	uint32_t dwLoopEnd;                // Byte offset ?
	uint32_t dwFraction;
	uint32_t dwPlayCount;              // Loop Count, 0=infinite
} SAMPLELOOPSTRUCT;


typedef struct WAVESAMPLERINFO
{
	WAVESMPLHEADER wsiHdr;
	SAMPLELOOPSTRUCT wsiLoops[2];
} WAVESAMPLERINFO;


typedef struct WAVELISTHEADER
{
	uint32_t list_id;  // "LIST" -> 0x5453494C
	uint32_t list_len;
	uint32_t info;             // "INFO"
} WAVELISTHEADER;


typedef struct WAVEEXTRAHEADER
{
	uint32_t xtra_id;  // "xtra"       -> 0x61727478
	uint32_t xtra_len;
	uint32_t dwFlags;
	uint16_t  wPan;
	uint16_t  wVolume;
	uint16_t  wGlobalVol;
	uint16_t  wReserved;
	uint8_t nVibType;
	uint8_t nVibSweep;
	uint8_t nVibDepth;
	uint8_t nVibRate;
} WAVEEXTRAHEADER;

#pragma pack()

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



#include "tables.h"

#define NEED_BYTESWAP
#include "headers.h"

#endif

