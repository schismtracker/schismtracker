/*
 * This program is  free software; you can redistribute it  and modify it
 * under the terms of the GNU  General Public License as published by the
 * Free Software Foundation; either version 2  of the license or (at your
 * option) any later version.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/
#define NEED_BYTESWAP

//////////////////////////////////////////////
// MIDI loader                              //
//////////////////////////////////////////////
#include "stdafx.h"
#include "sndfile.h"

#define MIDI_DRUMCHANNEL	10
#define MIDI_MAXTRACKS		64

UINT gnMidiImportSpeed = 3;
UINT gnMidiPatternLen = 128;

#pragma pack(1)

typedef struct MIDIFILEHEADER
{
	CHAR id[4];		// "MThd" = 0x6468544D
	DWORD len;		// 6
	WORD w1;		// 1?
	WORD wTrks;		// 2?
	WORD wDivision;	// F0
} MIDIFILEHEADER;


typedef struct MIDITRACKHEADER
{
	CHAR id[4];	// "MTrk" = 0x6B72544D
	DWORD len;
} MIDITRACKHEADER;

static LONG midivolumetolinear(UINT nMidiVolume)
{
	return (nMidiVolume * nMidiVolume << 16) / (127*127);
}

//////////////////////////////////////////////////////////////////////
// Midi Loader Internal Structures

#define CHNSTATE_NOTEOFFPENDING		0x0001

// MOD Channel State description (current volume, panning, etc...)
typedef struct MODCHANNELSTATE
{
	DWORD flags;	// Channel Flags
	WORD idlecount;
	WORD pitchsrc, pitchdest;	// Pitch Bend (current position/new position)
	BYTE parent;	// Midi Channel parent
	BYTE pan;		// Channel Panning			0-255
	BYTE note;		// Note On # (0=available)
} MODCHANNELSTATE;

// MIDI Channel State (Midi Channels 0-15)
typedef struct MIDICHANNELSTATE
{
	DWORD flags;		// Channel Flags
	WORD pitchbend;		// Pitch Bend Amount (14-bits unsigned)
	BYTE note_on[128];	// If note=on -> MOD channel # + 1 (0 if note=off)
	BYTE program;		// Channel Midi Program
	WORD bank;			// 0-16383
	// -- Controllers --------- function ---------- CC# --- range  --- init (midi) ---
	BYTE pan;			// Channel Panning			CC10	[0-255]		128 (64)
	BYTE expression;	// Channel Expression		CC11	0-128		128	(127)
	BYTE volume;		// Channel Volume			CC7		0-128		80	(100)
	BYTE modulation;	// Modulation				CC1		0-127		0
	BYTE pitchbendrange;// Pitch Bend Range								64
} MIDICHANNELSTATE;

typedef struct MIDITRACK
{
	LPCBYTE ptracks, ptrmax;
	DWORD status;
	LONG nexteventtime;
} MIDITRACK;

#pragma pack()



LPCSTR szMidiGroupNames[17] =
{
	"Piano",
	"Chromatic Percussion",
	"Organ",
	"Guitar",
	"Bass",
	"Strings",
	"Ensemble",
	"Brass",
	"Reed",
	"Pipe",
	"Synth Lead",
	"Synth Pad",
	"Synth Effects",
	"Ethnic",
	"Percussive",
	"Sound Effects",
	"Percussions"
};


LPCSTR szMidiProgramNames[128] =
{
	// 1-8: Piano
	"Acoustic Grand Piano",
	"Bright Acoustic Piano",
	"Electric Grand Piano",
	"Honky-tonk Piano",
	"Electric Piano 1",
	"Electric Piano 2",
	"Harpsichord",
	"Clavi",
	// 9-16: Chromatic Percussion
	"Celesta",
	"Glockenspiel",
	"Music Box",
	"Vibraphone",
	"Marimba",
	"Xylophone",
	"Tubular Bells",
	"Dulcimer",
	// 17-24: Organ
	"Drawbar Organ",
	"Percussive Organ",
	"Rock Organ",
	"Church Organ",
	"Reed Organ",
	"Accordion",
	"Harmonica",
	"Tango Accordion",
	// 25-32: Guitar
	"Acoustic Guitar (nylon)",
	"Acoustic Guitar (steel)",
	"Electric Guitar (jazz)",
	"Electric Guitar (clean)",
	"Electric Guitar (muted)",
	"Overdriven Guitar",
	"Distortion Guitar",
	"Guitar harmonics",
	// 33-40   Bass
	"Acoustic Bass",
	"Electric Bass (finger)",
	"Electric Bass (pick)",
	"Fretless Bass",
	"Slap Bass 1",
	"Slap Bass 2",
	"Synth Bass 1",
	"Synth Bass 2",
	// 41-48   Strings
	"Violin",
	"Viola",
	"Cello",
	"Contrabass",
	"Tremolo Strings",
	"Pizzicato Strings",
	"Orchestral Harp",
	"Timpani",
	// 49-56   Ensemble
	"String Ensemble 1",
	"String Ensemble 2",
	"SynthStrings 1",
	"SynthStrings 2",
	"Choir Aahs",
	"Voice Oohs",
	"Synth Voice",
	"Orchestra Hit",
	// 57-64   Brass
	"Trumpet",
	"Trombone",
	"Tuba",
	"Muted Trumpet",
	"French Horn",
	"Brass Section",
	"SynthBrass 1",
	"SynthBrass 2",
	// 65-72   Reed
	"Soprano Sax",
	"Alto Sax",
	"Tenor Sax",
	"Baritone Sax",
	"Oboe",
	"English Horn",
	"Bassoon",
	"Clarinet",
	// 73-80   Pipe
	"Piccolo",
	"Flute",
	"Recorder",
	"Pan Flute",
	"Blown Bottle",
	"Shakuhachi",
	"Whistle",
	"Ocarina",
	// 81-88   Synth Lead
	"Lead 1 (square)",
	"Lead 2 (sawtooth)",
	"Lead 3 (calliope)",
	"Lead 4 (chiff)",
	"Lead 5 (charang)",
	"Lead 6 (voice)",
	"Lead 7 (fifths)",
	"Lead 8 (bass + lead)",
	// 89-96   Synth Pad
	"Pad 1 (new age)",
	"Pad 2 (warm)",
	"Pad 3 (polysynth)",
	"Pad 4 (choir)",
	"Pad 5 (bowed)",
	"Pad 6 (metallic)",
	"Pad 7 (halo)",
	"Pad 8 (sweep)",
	// 97-104  Synth Effects
	"FX 1 (rain)",
	"FX 2 (soundtrack)",
	"FX 3 (crystal)",
	"FX 4 (atmosphere)",
	"FX 5 (brightness)",
	"FX 6 (goblins)",
	"FX 7 (echoes)",
	"FX 8 (sci-fi)",
	// 105-112 Ethnic
	"Sitar",
	"Banjo",
	"Shamisen",
	"Koto",
	"Kalimba",
	"Bag pipe",
	"Fiddle",
	"Shanai",
	// 113-120 Percussive
	"Tinkle Bell",
	"Agogo",
	"Steel Drums",
	"Woodblock",
	"Taiko Drum",
	"Melodic Tom",
	"Synth Drum",
	"Reverse Cymbal",
	// 121-128 Sound Effects
	"Guitar Fret Noise",
	"Breath Noise",
	"Seashore",
	"Bird Tweet",
	"Telephone Ring",
	"Helicopter",
	"Applause",
	"Gunshot"
};


// Notes 25-85
LPCSTR szMidiPercussionNames[61] =
{
	"Seq Click",
	"Brush Tap",
	"Brush Swirl",
	"Brush Slap",
	"Brush Swirl W/Attack",
	"Snare Roll",
	"Castanet",
	"Snare Lo",
	"Sticks",
	"Bass Drum Lo",
	"Open Rim Shot",
	"Acoustic Bass Drum",
	"Bass Drum 1",
	"Side Stick",
	"Acoustic Snare",
	"Hand Clap",
	"Electric Snare",
	"Low Floor Tom",
	"Closed Hi Hat",
	"High Floor Tom",
	"Pedal Hi-Hat",
	"Low Tom",
	"Open Hi-Hat",
	"Low-Mid Tom",
	"Hi Mid Tom",
	"Crash Cymbal 1",
	"High Tom",
	"Ride Cymbal 1",
	"Chinese Cymbal",
	"Ride Bell",
	"Tambourine",
	"Splash Cymbal",
	"Cowbell",
	"Crash Cymbal 2",
	"Vibraslap",
	"Ride Cymbal 2",
	"Hi Bongo",
	"Low Bongo",
	"Mute Hi Conga",
	"Open Hi Conga",
	"Low Conga",
	"High Timbale",
	"Low Timbale",
	"High Agogo",
	"Low Agogo",
	"Cabasa",
	"Maracas",
	"Short Whistle",
	"Long Whistle",
	"Short Guiro",
	"Long Guiro",
	"Claves",
	"Hi Wood Block",
	"Low Wood Block",
	"Mute Cuica",
	"Open Cuica",
	"Mute Triangle",
	"Open Triangle",
	"Shaker",
	"Jingle Bell",
	"Bell Tree",
};


const WORD kMidiChannelPriority[16] =
{
	0xFFFE, 0xFFFC, 0xFFF8, 0xFFF0,	0xFFE0, 0xFFC0, 0xFF80, 0xFF00,
	0xFE00, 0xFDFF, 0xF800, 0xF000,	0xE000, 0xC000, 0x8000, 0x0000,
};


///////////////////////////////////////////////////////////////////////////
// Helper functions

static LONG getmidilong(LPCBYTE &p, LPCBYTE pmax)
//----------------------------------------------------------
{
	DWORD n;
	UINT a;

	a = (p < pmax) ? *(p++) : 0;
	n = 0;
	while (a&0x80)
	{
		n = (n<<7)|(a&0x7F);
		a = (p < pmax) ? *(p++) : 0;
	}
	return (n<<7)|(LONG)a;
}


// Returns MOD tempo and tick multiplier
static int ConvertMidiTempo(int tempo_us, int *pTickMultiplier)
//-------------------------------------------------------------
{
	int nBestModTempo = 120;
	int nBestError = 1000000; // 1s
	int nBestMultiplier = 1;
	int nSpeed = gnMidiImportSpeed;
	for (int nModTempo=110; nModTempo<=240; nModTempo++)
	{
		int tick_us = (2500000) / nModTempo;
		int nFactor = (tick_us+tempo_us/2) / tempo_us;
		if (!nFactor) nFactor = 1;
		int nError = tick_us - tempo_us * nFactor;
		if (nError < 0) nError = -nError;
		if (nError < nBestError)
		{
			nBestError = nError;
			nBestModTempo = nModTempo;
			nBestMultiplier = nFactor;
		}
		if ((!nError) || ((nError<=1) && (nFactor==64))) break;
	}
	*pTickMultiplier = nBestMultiplier * nSpeed;
	return nBestModTempo;
}


////////////////////////////////////////////////////////////////////////////////
// Maps a midi instrument - returns the instrument number in the file
UINT CSoundFile::MapMidiInstrument(DWORD dwBankProgram, UINT nChannel, UINT nNote)
//--------------------------------------------------------------------------------
{
	INSTRUMENTHEADER *penv;
	UINT nProgram = dwBankProgram & 0x7F;
	UINT nBank = dwBankProgram >> 7;

	nNote &= 0x7F;
	if (nNote >= 120) return 0;
	for (UINT i=1; i<=m_nInstruments; i++) if (Headers[i])
	{
		INSTRUMENTHEADER *p = Headers[i];
		// Drum Kit ?
		if (nChannel == MIDI_DRUMCHANNEL)
		{
			if (nNote == p->nMidiDrumKey) return i;
		} else
		// Melodic Instrument
		{
			if (nProgram == p->nMidiProgram) return i;
		}
	}
	if ((m_nInstruments + 1 >= MAX_INSTRUMENTS) || (m_nSamples + 1 >= MAX_SAMPLES)) return 0;
	penv = new INSTRUMENTHEADER;
	if (!penv) return 0;
	memset(penv, 0, sizeof(INSTRUMENTHEADER));
	m_nSamples++;
	m_nInstruments++;
	Headers[m_nInstruments] = penv;
	penv->wMidiBank = nBank;
	penv->nMidiProgram = nProgram;
	penv->nMidiChannelMask = 1 << (nChannel-1);
	if (nChannel == MIDI_DRUMCHANNEL)
	{
	    penv->nMidiDrumKey = nNote;
	    penv->nMidiProgram = nNote + 128;
	}
	penv->nGlobalVol = 128;
	penv->nFadeOut = 1024;
	penv->nPan = 128;
	penv->nPPC = 5*12;
	penv->nNNA = NNA_NOTEOFF;
	penv->nDCT = (nChannel == MIDI_DRUMCHANNEL) ? DCT_SAMPLE : DCT_NOTE;
	penv->nDNA = DNA_NOTEFADE;
	for (UINT j=0; j<120; j++)
	{
		int mapnote = j+1;
		if (nChannel == MIDI_DRUMCHANNEL)
		{
			mapnote = 61;
			/*mapnote = 61 + j - nNote;
			if (mapnote < 1) mapnote = 1;
			if (mapnote > 120) mapnote = 120;*/
		}
		penv->Keyboard[j] = m_nSamples;
		penv->NoteMap[j] = (BYTE)mapnote;
	}
	penv->dwFlags |= ENV_VOLUME;
	if (nChannel != MIDI_DRUMCHANNEL) penv->dwFlags |= ENV_VOLSUSTAIN;
	penv->VolEnv.nNodes=4;
	penv->VolEnv.Ticks[0]=0;
	penv->VolEnv.Values[0] = 64;
	penv->VolEnv.Ticks[1] = 10;
	penv->VolEnv.Values[1] = 64;
	penv->VolEnv.Ticks[2] = 15;
	penv->VolEnv.Values[2] = 48;
	penv->VolEnv.Ticks[3] = 20;
	penv->VolEnv.Values[3] = 0;
	penv->VolEnv.nSustainStart=1;
	penv->VolEnv.nSustainEnd=1;
	// Sample
	Ins[m_nSamples].nPan = 128;
	Ins[m_nSamples].nVolume = 256;
	Ins[m_nSamples].nGlobalVol = 64;
	Ins[m_nSamples].pSample = AllocateSample(1);
	Ins[m_nSamples].uFlags &= ~(CHN_LOOP | CHN_16BIT);
	Ins[m_nSamples].nLength = 1;
	if (nChannel != MIDI_DRUMCHANNEL)
	{
		// GM Midi Name
		strcpy((char*)penv->name, (char*)szMidiProgramNames[nProgram]);
		strcpy((char*)m_szNames[m_nSamples], (char*)szMidiProgramNames[nProgram]);
	} else
	{
		strcpy((char*)penv->name, "Percussions");
		if ((nNote >= 24) && (nNote <= 84))
			strcpy((char*)m_szNames[m_nSamples], (char*)szMidiPercussionNames[nNote-24]);
		else
			strcpy((char*)m_szNames[m_nSamples], "Percussions");
	}
	return m_nInstruments;
}


/////////////////////////////////////////////////////////////////
// Loader Status
#define MIDIGLOBAL_SONGENDED		0x0001
#define MIDIGLOBAL_FROZEN			0x0002
#define MIDIGLOBAL_UPDATETEMPO		0x0004
#define MIDIGLOBAL_UPDATEMASTERVOL	0x0008
// Midi Globals
#define MIDIGLOBAL_GMSYSTEMON		0x0100
#define MIDIGLOBAL_XGSYSTEMON		0x0200


BOOL CSoundFile::ReadMID(const BYTE *lpStream, DWORD dwMemLength)
//---------------------------------------------------------------
{
	const MIDIFILEHEADER *pmfh = (const MIDIFILEHEADER *)lpStream;
	const MIDITRACKHEADER *pmth;
	MODCHANNELSTATE chnstate[MAX_BASECHANNELS];
	MIDICHANNELSTATE midichstate[16];
	MIDITRACK miditracks[MIDI_MAXTRACKS];
	DWORD dwMemPos, dwGlobalFlags, tracks, tempo;
	UINT row, pat, midimastervol;
	short int division;
	int midi_clock, nTempoUsec, nPPQN, nTickMultiplier;

	// Fix import parameters
	if (gnMidiImportSpeed < 2) gnMidiImportSpeed = 2;
	if (gnMidiImportSpeed > 6) gnMidiImportSpeed = 6;
	if (gnMidiPatternLen < 64) gnMidiPatternLen = 64;
	if (gnMidiPatternLen > 256) gnMidiPatternLen = 256;
	// Detect RMI files
	if ((dwMemLength > 12)
	 && (memcmp(lpStream, "RIFF",4) == 0)
	 && (memcmp(lpStream, "RMID",4) == 0))
	{
		lpStream += 12;
		dwMemLength -= 12;
		while (dwMemLength > 8)
		{
			char *id = (char*)lpStream;
			DWORD len = *(DWORD *)(lpStream+4);
			lpStream += 8;
			dwMemLength -= 8;
			if ((memcmp(id, "data",4) == 0) && (len < dwMemLength))
			{
				dwMemLength = len;
				pmfh = (const MIDIFILEHEADER *)lpStream;
				break;
			}
			if (len >= dwMemLength) return FALSE;
			lpStream += len;
			dwMemLength -= len;
		}
	}
	// MIDI File Header
	if ((dwMemLength < sizeof(MIDIFILEHEADER)+8) || (memcmp(pmfh->id, "MThd",4) != 0)) return FALSE;
	dwMemPos = 8 + bswapBE32(pmfh->len);
	if (dwMemPos >= dwMemLength - 8) return FALSE;
	pmth = (MIDITRACKHEADER *)(lpStream+dwMemPos);
	tracks = bswapBE16(pmfh->wTrks);
	if ((!tracks) || (memcmp(pmth->id, "MTrk", 4) != 0)) return FALSE;
	if (tracks > MIDI_MAXTRACKS) tracks = MIDI_MAXTRACKS;
	// Reading File...
	m_nType = MOD_TYPE_MID;
	m_nChannels = 32;
	m_nSamples = 0;
	m_nInstruments = 0;
	m_dwSongFlags |= (SONG_LINEARSLIDES | SONG_INSTRUMENTMODE);
	m_szNames[0][0] = 0;
	// MIDI->MOD Tempo Conversion
	division = bswapBE16(pmfh->wDivision);
	if (division < 0)
	{
		int nFrames = -(division>>8);
		int nSubFrames = (division & 0xff);
		nPPQN = nFrames * nSubFrames / 2;
		if (!nPPQN) nPPQN = 1;
	} else
	{
		nPPQN = (division) ? division : 96;
	}
	nTempoUsec = 500000 / nPPQN;
	tempo = ConvertMidiTempo(nTempoUsec, &nTickMultiplier);
	m_nDefaultTempo = tempo;
	m_nDefaultSpeed = gnMidiImportSpeed;
	m_nDefaultGlobalVolume = 256;
	midimastervol = m_nDefaultGlobalVolume;
	
	// Initializing 
	memset(Order, 0xFF, sizeof(Order));
	memset(chnstate, 0, sizeof(chnstate));
	memset(miditracks, 0, sizeof(miditracks));
	memset(midichstate, 0, sizeof(midichstate));
	// Initializing Patterns
	Order[0] = 0;
	for (UINT ipat=0; ipat<MAX_PATTERNS; ipat++) PatternSize[ipat] = gnMidiPatternLen;
	// Initializing Channels
	for (UINT ics=0; ics<MAX_BASECHANNELS; ics++)
	{
		// Channel settings
		ChnSettings[ics].nPan = 128;
		ChnSettings[ics].nVolume = 64;
		ChnSettings[ics].dwFlags = 0;
		// Channels state
		chnstate[ics].pan = 128;
		chnstate[ics].pitchsrc = 0x2000;
		chnstate[ics].pitchdest = 0x2000;
	}
	// Initializing Track Pointers
	for (UINT itrk=0; itrk<tracks; itrk++)
	{
		miditracks[itrk].nexteventtime = -1;
		miditracks[itrk].status = 0x2F;
		pmth = (MIDITRACKHEADER *)(lpStream+dwMemPos);
		if (dwMemPos + 8 >= dwMemLength) break;
		DWORD len = bswapBE32(pmth->len);
		if ((memcmp(pmth->id, "MTrk", 4) == 0) && (dwMemPos + 8 + len <= dwMemLength))
		{
			// Initializing midi tracks
			miditracks[itrk].ptracks = lpStream+dwMemPos+8;
			miditracks[itrk].ptrmax = miditracks[itrk].ptracks + len;
			miditracks[itrk].nexteventtime = getmidilong(miditracks[itrk].ptracks, miditracks[itrk].ptrmax);
		}
		dwMemPos += 8 + len;
	}
	// Initializing midi channels state
	for (UINT imidi=0; imidi<16; imidi++)
	{
		midichstate[imidi].pan = 128;			// middle
		midichstate[imidi].expression = 128;	// no attenuation
		midichstate[imidi].volume = 80;			// GM specs defaults to 100
		midichstate[imidi].pitchbend = 0x2000;	// Pitch Bend Amount
		midichstate[imidi].pitchbendrange = 64;	// Pitch Bend Range: +/- 2 semitones
	}
	////////////////////////////////////////////////////////////////////////////
	// Main Midi Sequencer Loop
	pat = 0;
	row = 0;
	midi_clock = 0;
	dwGlobalFlags = MIDIGLOBAL_UPDATETEMPO | MIDIGLOBAL_FROZEN;
	do
	{
		// Allocate current pattern if not allocated yet
		if (!Patterns[pat])
		{
			Patterns[pat] = AllocatePattern(PatternSize[pat], m_nChannels);
			if (!Patterns[pat]) break;
		}
		dwGlobalFlags |= MIDIGLOBAL_SONGENDED;
		MODCOMMAND *m = Patterns[pat] + row * m_nChannels;
		// Parse Tracks
		for (UINT trk=0; trk<tracks; trk++) if (miditracks[trk].ptracks)
		{
			MIDITRACK *ptrk = &miditracks[trk];
			dwGlobalFlags &= ~MIDIGLOBAL_SONGENDED;
			while ((ptrk->ptracks) && (ptrk->nexteventtime >= 0) && (midi_clock+(nTickMultiplier>>2) >= ptrk->nexteventtime))
			{
				if (ptrk->ptracks[0] & 0x80) ptrk->status = *(ptrk->ptracks++);
				switch(ptrk->status)
				{
				/////////////////////////////////////////////////////////////////////
				// End Of Track
				case 0x2F:
				// End Of Song
				case 0xFC:
					ptrk->ptracks = NULL;
					break;

				/////////////////////////////////////////////////////////////////////
				// SYSEX messages
				case 0xF0:
				case 0xF7:
					{
						LONG len = getmidilong(ptrk->ptracks, ptrk->ptrmax);
						if ((len > 1) && (ptrk->ptracks + len <ptrk->ptrmax) && (ptrk->ptracks[len-1] == 0xF7))
						{
							DWORD dwSysEx1 = 0, dwSysEx2 = 0;
							if (len >= 4) dwSysEx1 = (*((DWORD *)(ptrk->ptracks))) & 0x7F7F7F7F;
							if (len >= 8) dwSysEx2 = (*((DWORD *)(ptrk->ptracks+4))) & 0x7F7F7F7F;
							// GM System On
							if ((len == 5) && (dwSysEx1 == 0x01097F7E))
							{
								dwGlobalFlags |= MIDIGLOBAL_GMSYSTEMON;
							} else
							// XG System On
							if ((len == 8) && ((dwSysEx1 & 0xFFFFF0FF) == 0x004c1043) && (dwSysEx2 == 0x77007e00))
							{
								dwGlobalFlags |= MIDIGLOBAL_XGSYSTEMON;
							} else
							// Midi Master Volume
							if ((len == 7) && (dwSysEx1 == 0x01047F7F))
							{
								midimastervol = midivolumetolinear(ptrk->ptracks[5] & 0x7F) >> 8;
								if (midimastervol < 16) midimastervol = 16;
								dwGlobalFlags |= MIDIGLOBAL_UPDATEMASTERVOL;
							}
						}
						ptrk->ptracks += len;
					}
					break;
				
				//////////////////////////////////////////////////////////////////////
				// META-events: FF.code.len.data[len]
				case 0xFF:
					{
						UINT i = *(ptrk->ptracks++);
						LONG len = getmidilong(ptrk->ptracks, ptrk->ptrmax);
						if (ptrk->ptracks+len > ptrk->ptrmax)
						{
							// EOF
							ptrk->ptracks = NULL;
						} else
						switch(i)
						{
						// FF.01 [text]: Song Information
						case 0x01:
							if (!len) break;
							if ((len < 32) && (!m_szNames[0][0]))
							{
								memcpy(m_szNames[0], ptrk->ptracks, len);
								m_szNames[0][len] = 0;
							} else
							if ((!m_lpszSongComments) && (ptrk->ptracks[0]) && (ptrk->ptracks[0] < 0x7F))
							{
								m_lpszSongComments = new char [len+1];
								if (m_lpszSongComments)
								{
									memcpy(m_lpszSongComments, ptrk->ptracks, len);
									m_lpszSongComments[len] = 0;
								}
							}
							break;
						// FF.02 [text]: Song Copyright
						case 0x02:
							if (!len) break;
							if ((!m_lpszSongComments) && (ptrk->ptracks[0]) && (ptrk->ptracks[0] < 0x7F) && (len > 7))
							{
								m_lpszSongComments = new char [len+1];
								if (m_lpszSongComments)
								{
									memcpy(m_lpszSongComments, ptrk->ptracks, len);
									m_lpszSongComments[len] = 0;
								}
							}
							break;
						// FF.03: Sequence Name
						case 0x03:
						// FF.06: Sequence Text (->Pattern names)
						case 0x06:
							if ((len > 1) && (!trk))
							{
								UINT k = (len < 32) ? len : 31;
								CHAR s[32];
								memcpy(s, ptrk->ptracks, k);
								s[k] = 0;
								if ((!strnicmp((char*)s, "Copyri", 6)) || (!s[0])) break;
								if (i == 0x03)
								{
									if (!m_szNames[0][0]) strcpy((char*)m_szNames[0], (char*)s);
								}
							}
							break;
						// FF.07: Cue Point (marker)
						// FF.20: Channel Prefix
						// FF.2F: End of Track
						case 0x2F:
							ptrk->status = 0x2F;
							ptrk->ptracks = NULL;
							break;
						// FF.51 [tttttt]: Set Tempo
						case 0x51:
							{
								LONG l = ptrk->ptracks[0];
								l = (l << 8) | ptrk->ptracks[1];
								l = (l << 8) | ptrk->ptracks[2];
								if (l <= 0) break;
								nTempoUsec = l / nPPQN;
								if (nTempoUsec < 100) nTempoUsec = 100;
								tempo = ConvertMidiTempo(nTempoUsec, &nTickMultiplier);
								dwGlobalFlags |= MIDIGLOBAL_UPDATETEMPO;
							}
							break;
						// FF.58: Time Signature
						// FF.7F: Sequencer-Specific
						}
						if (ptrk->ptracks) ptrk->ptracks += len;
					}
					break;

				//////////////////////////////////////////////////////////////////////////
				// Regular Voice Events
				default:
				{
					UINT midich = (ptrk->status & 0x0F)+1;
					UINT midist = ptrk->status & 0xF0;
					MIDICHANNELSTATE *pmidich = &midichstate[midich-1];
					UINT note, velocity;

					switch(midist)
					{
					//////////////////////////////////
					// Note Off:	80.note.velocity
					case 0x80:
					// Note On:		90.note.velocity
					case 0x90:
						note = ptrk->ptracks[0] & 0x7F;
						velocity = (midist == 0x90) ? (ptrk->ptracks[1] & 0x7F) : 0;
						ptrk->ptracks += 2;
						// Note On: 90.note.velocity
						if (velocity)
						{
							// Start counting rows
							dwGlobalFlags &= ~MIDIGLOBAL_FROZEN;
							// if the note is already playing, we reuse this channel
							UINT nchn = pmidich->note_on[note];
							if ((nchn) && (chnstate[nchn-1].parent != midich)) nchn = 0;
							// or else, we look for an available child channel
							if (!nchn)
							{
								for (UINT i=0; i<m_nChannels; i++) if (chnstate[i].parent == midich)
								{
									if ((!chnstate[i].note) && ((!m[i].note) || (m[i].note & 0x80)))
									{
										// found an available channel
										nchn = i+1;
										break;
									}
								}
							}
							// still nothing? in this case, we try to allocate a new mod channel
							if (!nchn)
							{
								for (UINT i=0; i<m_nChannels; i++) if (!chnstate[i].parent)
								{
									nchn = i+1;
									chnstate[i].parent = midich;
									break;
								}
							}
							// still not? we have to steal a voice from another channel
							// We found our channel: let's do the note on
							if (nchn)
							{
								pmidich->note_on[note] = nchn;
								nchn--;
								chnstate[nchn].pitchsrc = pmidich->pitchbend;
								chnstate[nchn].pitchdest = pmidich->pitchbend;
								chnstate[nchn].flags &= ~CHNSTATE_NOTEOFFPENDING;
								chnstate[nchn].idlecount = 0;
								chnstate[nchn].note = note+1;
								int realnote = note;
								if (midich != 10)
								{
									realnote += (((int)pmidich->pitchbend - 0x2000) * pmidich->pitchbendrange) / (0x2000*32);
									if (realnote < 0) realnote = 0;
									if (realnote > 119) realnote = 119;
								}
								m[nchn].note = realnote+1;
								m[nchn].instr = MapMidiInstrument(pmidich->program + ((UINT)pmidich->bank << 7), midich, note);
								m[nchn].volcmd = VOLCMD_VOLUME;
								LONG vol = midivolumetolinear(velocity) >> 8;
								vol = (vol * (LONG)pmidich->volume * (LONG)pmidich->expression) >> 13;
								if (vol > 256) vol = 256;
								if (vol < 4) vol = 4;
								m[nchn].vol = (BYTE)(vol>>2);
								// Channel Panning
								if ((!m[nchn].command) && (pmidich->pan != chnstate[nchn].pan))
								{
									chnstate[nchn].pan = pmidich->pan;
									m[nchn].param = pmidich->pan;
									m[nchn].command = CMD_PANNING8;
								}
							}
						} else
						// Note Off; 90.note.00
						if (!(dwGlobalFlags & MIDIGLOBAL_FROZEN))
						{
							UINT nchn = pmidich->note_on[note];
							if (nchn)
							{
								nchn--;
								chnstate[nchn].flags |= CHNSTATE_NOTEOFFPENDING;
								chnstate[nchn].note = 0;
								pmidich->note_on[note] = 0;
							} else
							{
								for (UINT i=0; i<m_nChannels; i++)
								{
									if ((chnstate[i].parent == midich) && (chnstate[i].note == note+1))
									{
										chnstate[i].note = 0;
										chnstate[i].flags |= CHNSTATE_NOTEOFFPENDING;
									}
								}
							}
						}
						break;

					///////////////////////////////////
					// A0.xx.yy: Aftertouch
					case 0xA0:
						{
							ptrk->ptracks += 2;
						}
						break;

					///////////////////////////////////
					// B0: Control Change
					case 0xB0:
						{
							UINT controller = ptrk->ptracks[0];
							UINT value = ptrk->ptracks[1] & 0x7F;
							ptrk->ptracks += 2;
							switch(controller)
							{
							// Bn.00.xx: Bank Select MSB (GS)
							case 0x00:
								pmidich->bank &= 0x7F;
								pmidich->bank |= (value << 7);
								break;
							// Bn.01.xx: Modulation Depth
							case 0x01:
								pmidich->pitchbendrange = value;
								break;
							// Bn.07.xx: Volume
							case 0x07:
								pmidich->volume = (BYTE)(midivolumetolinear(value) >> 9);
								break;
							// Bn.0B.xx: Expression
							case 0x0B:
								pmidich->expression = (BYTE)(midivolumetolinear(value) >> 9);
								break;
							// Bn.0A.xx: Pan
							case 0x0A:
								pmidich->pan = value * 2;
								break;
							// Bn.20.xx: Bank Select LSB (GS)
							case 0x20:
								pmidich->bank &= (0x7F << 7);
								pmidich->bank |= value;
								break;
							// Bn.79.00: Reset All Controllers (GM)
							case 0x79:
								pmidich->modulation = 0;
								pmidich->expression = 128;
								pmidich->pitchbend = 0x2000;
								pmidich->pitchbendrange = 64;
								// Should also reset pedals (40h-43h), NRP, RPN, aftertouch
								break;
							// Bn.78.00: All Sound Off (GS)
							// Bn.7B.00: All Notes Off (GM)
							case 0x78:
							case 0x7B:
								if (value == 0x00)
								{
									// All Notes Off
									for (UINT k=0; k<m_nChannels; k++)
									{
										if (chnstate[k].note)
										{
											chnstate[k].flags |= CHNSTATE_NOTEOFFPENDING;
											chnstate[k].note = 0;
										}
									}
								}
								break;
							////////////////////////////////////
							// Controller List
							//
							// Bn.02.xx: Breath Control
							// Bn.04.xx: Foot Pedal
							// Bn.05.xx: Portamento Time (Glissando Time)
							// Bn.06.xx: Data Entry MSB
							// Bn.08.xx: Balance
							// Bn.10-13.xx: GP Control #1-#4
							// Bn.20-3F.xx: Data LSB for controllers 0-31
							// Bn.26.xx: Data Entry LSB
							// Bn.40.xx: Hold Pedal #1
							// Bn.41.xx: Portamento (GS)
							// Bn.42.xx: Sostenuto (GS)
							// Bn.43.xx: Soft Pedal (GS)
							// Bn.44.xx: Legato Pedal
							// Bn.45.xx: Hold Pedal #2
							// Bn.46.xx: Sound Variation
							// Bn.47.xx: Sound Timbre
							// Bn.48.xx: Sound Release Time
							// Bn.49.xx: Sound Attack Time
							// Bn.4A.xx: Sound Brightness
							// Bn.4B-4F.xx: Sound Control #6-#10
							// Bn.50-53.xx: GP Control #5-#8
							// Bn.54.xx: Portamento Control (GS)
							// Bn.5B.xx: Reverb Level (GS)
							// Bn.5C.xx: Tremolo Depth
							// Bn.5D.xx: Chorus Level (GS)
							// Bn.5E.xx: Celeste Depth
							// Bn.5F.xx: Phaser Depth
							// Bn.60.xx: Data Increment
							// Bn.61.xx: Data Decrement
							// Bn.62.xx: Non-RPN Parameter LSB (GS)
							// Bn.63.xx: Non-RPN Parameter MSB (GS)
							// Bn.64.xx: RPN Parameter LSB (GM)
							// Bn.65.xx: RPN Parameter MSB (GM)
							// Bn.7A.00: Local On/Off
							// Bn.7C.00: Omni Mode Off
							// Bn.7D.00: Omni Mode On
							// Bn.7E.mm: Mono Mode On
							// Bn.7F.00: Poly Mode On
							}
						}
						break;

					////////////////////////////////
					// C0.pp: Program Change
					case 0xC0:
						{
							pmidich->program = ptrk->ptracks[0] & 0x7F;
							ptrk->ptracks++;
						}
						break;

					////////////////////////////////
					// D0: Channel Aftertouch (Polyphonic Key Pressure)
					case 0xD0:
						{
							ptrk->ptracks++;
						}
						break;
					
					////////////////////////////////
					// E0: Pitch Bend
					case 0xE0:
						{
							pmidich->pitchbend = (WORD)(((UINT)ptrk->ptracks[1] << 7) + (ptrk->ptracks[0] & 0x7F));
							for (UINT i=0; i<128; i++) if (pmidich->note_on[i])
							{
								UINT nchn = pmidich->note_on[i]-1;
								if (chnstate[nchn].parent == midich)
								{
									chnstate[nchn].pitchdest = pmidich->pitchbend;
								}
							}
							ptrk->ptracks+=2;
						}
						break;

					//////////////////////////////////////
					// F0 & Unsupported commands: skip it
					default:
						ptrk->ptracks++;
					}
				}} // switch+default
				// Process to next event
				if (ptrk->ptracks)
				{
					ptrk->nexteventtime += getmidilong(ptrk->ptracks, ptrk->ptrmax);
				}
				if (ptrk->ptracks >= ptrk->ptrmax) ptrk->ptracks = NULL;
			}
			// End reached?
			if (ptrk->ptracks >= ptrk->ptrmax) ptrk->ptracks = NULL;
		}

		////////////////////////////////////////////////////////////////////
		// Move to next row
		if (!(dwGlobalFlags & MIDIGLOBAL_FROZEN))
		{
			// Check MOD channels status
			for (UINT ichn=0; ichn<m_nChannels; ichn++)
			{
				// Pending Global Effects ?
				if (!m[ichn].command)
				{
					if ((chnstate[ichn].pitchsrc != chnstate[ichn].pitchdest) && (chnstate[ichn].parent))
					{
						int newpitch = chnstate[ichn].pitchdest;
						int pitchbendrange = midichstate[chnstate[ichn].parent-1].pitchbendrange;
						// +/- 256 for +/- pitch bend range
						int slideamount = (newpitch - (int)chnstate[ichn].pitchsrc) / (int)32;
						if (slideamount)
						{
							const int ppdiv = (16 * 128 * (gnMidiImportSpeed-1));
							newpitch = (int)chnstate[ichn].pitchsrc + slideamount;
							if (slideamount < 0)
							{
								int param = (-slideamount * pitchbendrange + ppdiv/2) / ppdiv;
								if (param >= 0x80) param = 0x80;
								if (param > 0)
								{
									m[ichn].param = (BYTE)param;
									m[ichn].command = CMD_PORTAMENTODOWN;
								}
							} else
							{
								int param = (slideamount * pitchbendrange + ppdiv/2) / ppdiv;
								if (param >= 0x80) param = 0x80;
								if (param > 0)
								{
									m[ichn].param = (BYTE)param;
									m[ichn].command = CMD_PORTAMENTOUP;
								}
							}
						}
						chnstate[ichn].pitchsrc = (WORD)newpitch;

					} else
					if (dwGlobalFlags & MIDIGLOBAL_UPDATETEMPO)
					{
						m[ichn].command = CMD_TEMPO;
						m[ichn].param = (BYTE)tempo;
						dwGlobalFlags &= ~MIDIGLOBAL_UPDATETEMPO;
					} else
					if (dwGlobalFlags & MIDIGLOBAL_UPDATEMASTERVOL)
					{
						m[ichn].command = CMD_GLOBALVOLUME;
						m[ichn].param = midimastervol >> 1; // 0-128
						dwGlobalFlags &= ~MIDIGLOBAL_UPDATEMASTERVOL;
					}
				}
				// Check pending noteoff events for m[ichn]
				if (!m[ichn].note)
				{
					if (chnstate[ichn].flags & CHNSTATE_NOTEOFFPENDING)
					{
						chnstate[ichn].flags &= ~CHNSTATE_NOTEOFFPENDING;
						m[ichn].note = 0xFF;
					}
					// Check State of channel
					chnstate[ichn].idlecount++;
					if ((chnstate[ichn].note) && (chnstate[ichn].idlecount >= 50))
					{
						chnstate[ichn].note = 0;
						m[ichn].note = 0xFF;	// only if not drum channel ?
					} else
					if (chnstate[ichn].idlecount >= 500) // 20secs of inactivity
					{
						chnstate[ichn].idlecount = 0;
						chnstate[ichn].parent = 0;
					}
				}
			}

			if ((++row) >= PatternSize[pat])
			{
				pat++;
				if (pat >= MAX_PATTERNS-1) break;
				Order[pat] = pat;
				Order[pat+1] = 0xFF;
				row = 0;
			}
		}

		// Increase midi clock
		midi_clock += nTickMultiplier;
	} while (!(dwGlobalFlags & MIDIGLOBAL_SONGENDED));
	return TRUE;
}


