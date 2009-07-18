/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

#include "sndfile.h"
#include "util.h"

//#define MED_LOG

#ifdef MED_LOG
extern void Log(const char * s, ...);
#endif

//////////////////////////////////////////////////////////
// OctaMed MED file support (import only)

// flags
#define	MMD_FLAG_FILTERON	0x1
#define	MMD_FLAG_JUMPINGON	0x2
#define	MMD_FLAG_JUMP8TH	0x4
#define	MMD_FLAG_INSTRSATT	0x8 // instruments are attached (this is a module)
#define	MMD_FLAG_VOLHEX		0x10
#define MMD_FLAG_STSLIDE	0x20 // SoundTracker mode for slides
#define MMD_FLAG_8CHANNEL	0x40 // OctaMED 8 channel song
#define	MMD_FLAG_SLOWHQ		0x80 // HQ slows playing speed (V2-V4 compatibility)
// flags2
#define MMD_FLAG2_BMASK		0x1F
#define MMD_FLAG2_BPM		0x20
#define	MMD_FLAG2_MIX		0x80 // uses Mixing (V7+)
// flags3:
#define	MMD_FLAG3_STEREO	0x1	// mixing in Stereo mode
#define	MMD_FLAG3_FREEPAN	0x2	// free panning
#define MMD_FLAG3_GM		0x4 // module designed for GM/XG compatibility


// generic MMD tags
#define	MMDTAG_END		0
#define	MMDTAG_PTR		0x80000000	// data needs relocation
#define	MMDTAG_MUSTKNOW	0x40000000	// loader must fail if this isn't recognized
#define	MMDTAG_MUSTWARN	0x20000000	// loader must warn if this isn't recognized

// ExpData tags
// # of effect groups, including the global group (will
// override settings in MMDSong struct), default = 1
#define	MMDTAG_EXP_NUMFXGROUPS	1
#define	MMDTAG_TRK_NAME		(MMDTAG_PTR|1)	// trackinfo tags
#define	MMDTAG_TRK_NAMELEN	2				// namelen includes zero term.
#define	MMDTAG_TRK_FXGROUP	3
// effectinfo tags
#define	MMDTAG_FX_ECHOTYPE	1
#define MMDTAG_FX_ECHOLEN	2
#define	MMDTAG_FX_ECHODEPTH	3
#define	MMDTAG_FX_STEREOSEP	4
#define	MMDTAG_FX_GROUPNAME	(MMDTAG_PTR|5)	// the Global Effects group shouldn't have name saved!
#define	MMDTAG_FX_GRPNAMELEN 6	// namelen includes zero term.

#pragma pack(1)

typedef struct tagMEDMODULEHEADER
{
	uint32_t id;		// MMD1-MMD3
	uint32_t modlen;	// Size of file
	uint32_t song;		// Position in file for this song
	uint16_t psecnum;
	uint16_t pseq;
	uint32_t blockarr;	// Position in file for blocks
	uint32_t mmdflags;
	uint32_t smplarr;	// Position in file for samples
	uint32_t reserved;
	uint32_t expdata;	// Absolute offset in file for ExpData (0 if not present)
	uint32_t reserved2;
	uint16_t pstate;
	uint16_t pblock;
	uint16_t pline;
	uint16_t pseqnum;
	uint16_t actplayline;
	uint8_t counter;
	uint8_t extra_songs;	// # of songs - 1
} MEDMODULEHEADER;


typedef struct tagMMD0SAMPLE
{
	uint16_t rep, replen;
	uint8_t midich;
	uint8_t midipreset;
	uint8_t svol;
	signed char strans;
} MMD0SAMPLE;


// Sample header is immediately followed by sample data...
typedef struct tagMMDSAMPLEHEADER
{
	uint32_t length;     // length of *one* *unpacked* channel in *bytes*
	uint16_t type;   
				// if non-negative
					// bits 0-3 reserved for multi-octave instruments, not supported on the PC
					// 0x10: 16 bit (otherwise 8 bit)
					// 0x20: Stereo (otherwise mono)
					// 0x40: Uses DeltaCode
					// 0x80: Packed data
				// -1: Synth
				// -2: Hybrid
	// if type indicates packed data, these fields follow, otherwise we go right to the data
	uint16_t packtype;	// Only 1 = ADPCM is supported
	uint16_t subtype;	// Packing subtype
		// ADPCM subtype
		// 1: g723_40
		// 2: g721
		// 3: g723_24
	uint8_t commonflags;	// flags common to all packtypes (none defined so far)
	uint8_t packerflags;	// flags for the specific packtype
	uint32_t leftchlen;	// packed length of left channel in bytes
	uint32_t rightchlen;	// packed length of right channel in bytes (ONLY PRESENT IN STEREO SAMPLES)
	uint8_t SampleData[1];	// Sample Data
} MMDSAMPLEHEADER;


// MMD0/MMD1 song header
typedef struct tagMMD0SONGHEADER
{
	MMD0SAMPLE sample[63];
	uint16_t numblocks;		// # of blocks
	uint16_t songlen;		// # of entries used in playseq
	uint8_t playseq[256];	// Play sequence
	uint16_t deftempo;		// BPM tempo
	signed char playtransp;	// Play transpose
	uint8_t flags;			// 0x10: Hex Volumes | 0x20: ST/NT/PT Slides | 0x40: 8 Channels song
	uint8_t flags2;		// [b4-b0]+1: Tempo LPB, 0x20: tempo mode, 0x80: mix_conv=on
	uint8_t tempo2;		// tempo TPL
	uint8_t trkvol[16];	// track volumes
	uint8_t mastervol;		// master volume
	uint8_t numsamples;	// # of samples (max=63)
} MMD0SONGHEADER;


// MMD2/MMD3 song header
typedef struct tagMMD2SONGHEADER
{
	MMD0SAMPLE sample[63];
	uint16_t numblocks;		// # of blocks
	uint16_t numsections;	// # of sections
	uint32_t playseqtable;	// filepos of play sequence
	uint32_t sectiontable;	// filepos of sections table (uint16_t array)
	uint32_t trackvols;	// filepos of tracks volume (uint8_t array)
	uint16_t numtracks;		// # of tracks (max 64)
	uint16_t numpseqs;		// # of play sequences
	uint32_t trackpans;	// filepos of tracks pan values (uint8_t array)
	int32_t flags3;		// 0x1:stereo_mix, 0x2:free_panning, 0x4:GM/XG compatibility
	uint16_t voladj;		// vol_adjust (set to 100 if 0)
	uint16_t channels;		// # of channels (4 if =0)
	uint8_t mix_echotype;	// 1:normal,2:xecho
	uint8_t mix_echodepth;	// 1..6
	uint16_t mix_echolen;	// > 0
	signed char mix_stereosep;	// -4..4
	uint8_t pad0[223];
	uint16_t deftempo;		// BPM tempo
	signed char playtransp;	// play transpose
	uint8_t flags;			// 0x1:filteron, 0x2:jumpingon, 0x4:jump8th, 0x8:instr_attached, 0x10:hex_vol, 0x20:PT_slides, 0x40:8ch_conv,0x80:hq slows playing speed
	uint8_t flags2;		// 0x80:mix_conv=on, [b4-b0]+1:tempo LPB, 0x20:tempo_mode
	uint8_t tempo2;		// tempo TPL
	uint8_t pad1[16];
	uint8_t mastervol;		// master volume
	uint8_t numsamples;	// # of samples (max 63)
} MMD2SONGHEADER;

// For MMD0 the note information is held in 3 bytes, byte0, byte1, byte2.  For reference we 
// number the bits in each byte 0..7, where 0 is the low bit.
// The note is held as bits 5..0 of byte0
// The instrument is encoded in 6 bits,  bits 7 and 6 of byte0 and bits 7,6,5,4 of byte1
// The command number is bits 3,2,1,0 of byte1, command data is in byte2:
// For command 0, byte2 represents the second data byte, otherwise byte2
// represents the first data byte.
typedef struct tagMMD0BLOCK
{
	uint8_t numtracks;
	uint8_t lines;		// File value is 1 less than actual, so 0 -> 1 line
} MMD0BLOCK;		// uint8_t data[lines+1][tracks][3];


// For MMD1,MMD2,MMD3 the note information is carried in 4 bytes, byte0, byte1,
// byte2 and byte3
// The note is held as byte0 (values above 0x84 are ignored)
// The instrument is held as byte1
// The command number is held as byte2, command data is in byte3
// For commands 0 and 0x19 byte3 represents the second data byte,
// otherwise byte2 represents the first data byte.
typedef struct tagMMD1BLOCK
{
	uint16_t numtracks;	// Number of tracks, may be > 64, but then that data is skipped.
	uint16_t lines;		// Stored value is 1 less than actual, so 0 -> 1 line
	uint32_t info;		// Offset of BlockInfo (if 0, no block_info is present)
} MMD1BLOCK;


typedef struct tagMMD1BLOCKINFO
{
	uint32_t hlmask;		// Unimplemented - ignore
	uint32_t blockname;	// file offset of block name
	uint32_t blocknamelen;	// length of block name (including term. 0)
	uint32_t pagetable;	// file offset of command page table
	uint32_t cmdexttable;	// file offset of command extension table
	uint32_t reserved[4];	// future expansion
} MMD1BLOCKINFO;


// A set of play sequences is stored as an array of uint32_t files offsets
// Each offset points to the play sequence itself.
typedef struct tagMMD2PLAYSEQ
{
	int8_t name[32];
	uint32_t command_offs;	// filepos of command table
	uint32_t reserved;
	uint16_t length;
	uint16_t seq[512];	// skip if > 0x8000
} MMD2PLAYSEQ;


// A command table contains commands that effect a particular play sequence
// entry.  The only commands read in are STOP or POSJUMP, all others are ignored
// POSJUMP is presumed to have extra bytes containing a uint16_t for the position
typedef struct tagMMDCOMMAND
{
	uint16_t offset;		// Offset within current sequence entry
	uint8_t cmdnumber;		// STOP (537) or POSJUMP (538) (others skipped)
	uint8_t extra_count;
	uint8_t extra_bytes[4];// [extra_count];
} MMDCOMMAND;  // Last entry has offset == 0xFFFF, cmd_number == 0 and 0 extrabytes


typedef struct tagMMD0EXP
{
	uint32_t nextmod;			// File offset of next Hdr
	uint32_t exp_smp;			// Pointer to extra instrument data
	uint16_t s_ext_entries;		// Number of extra instrument entries
	uint16_t s_ext_entrsz;		// Size of extra instrument data
	uint32_t annotxt;
	uint32_t annolen;
	uint32_t iinfo;			// Instrument names
	uint16_t i_ext_entries;	
	uint16_t i_ext_entrsz;
	uint32_t jumpmask;
	uint32_t rgbtable;
	uint8_t channelsplit[4];	// Only used if 8ch_conv (extra channel for every nonzero entry)
	uint32_t n_info;
	uint32_t songname;			// Song name
	uint32_t songnamelen;
	uint32_t dumps;
	uint32_t mmdinfo;
	uint32_t mmdrexx;
	uint32_t mmdcmd3x;
	uint32_t trackinfo_ofs;	// ptr to song->numtracks ptrs to tag lists
	uint32_t effectinfo_ofs;	// ptr to group ptrs
	uint32_t tag_end;
} MMD0EXP;

#pragma pack()



static void MedConvert(MODCOMMAND *p, const MMD0SONGHEADER *pmsh)
//---------------------------------------------------------------
{
	const uint8_t bpmvals[9] = { 179,164,152,141,131,123,116,110,104};

	uint32_t command = p->command;
	uint32_t param = p->param;
	switch(command)
	{
	case 0x00:	if (param) command = CMD_ARPEGGIO; else command = 0; break;
	case 0x01:	command = CMD_PORTAMENTOUP; break;
	case 0x02:	command = CMD_PORTAMENTODOWN; break;
	case 0x03:	command = CMD_TONEPORTAMENTO; break;
	case 0x04:	command = CMD_VIBRATO; break;
	case 0x05:	command = CMD_TONEPORTAVOL; break;
	case 0x06:	command = CMD_VIBRATOVOL; break;
	case 0x07:	command = CMD_TREMOLO; break;
	case 0x0A:	if (param & 0xF0) param &= 0xF0; command = CMD_VOLUMESLIDE; if (!param) command = 0; break;
	case 0x0B:	command = CMD_POSITIONJUMP; break;
	case 0x0C:	command = CMD_VOLUME;
				if (pmsh->flags & MMD_FLAG_VOLHEX)
				{
					if (param < 0x80)
					{
						param = (param+1) / 2;
					} else command = 0;
				} else
				{
					if (param <= 0x99)
					{
						param = (param >> 4)*10+((param & 0x0F) % 10);
						if (param > 64) param = 64;
					} else command = 0;
				}
				break;
	case 0x09:	command = (param < 0x20) ? CMD_SPEED : CMD_TEMPO; break;
	case 0x0D:	if (param & 0xF0) param &= 0xF0; command = CMD_VOLUMESLIDE; if (!param) command = 0; break;
	case 0x0F:	// Set Tempo / Special
		// F.00 = Pattern Break
		if (!param)	command = CMD_PATTERNBREAK;	else
		// F.01 - F.F0: Set tempo/speed
		if (param <= 0xF0)
		{
			if (pmsh->flags & MMD_FLAG_8CHANNEL)
			{
				param = (param > 10) ? 99 : bpmvals[param-1];
			} else
			// F.01 - F.0A: Set Speed
			if (param <= 0x0A)
			{
				command = CMD_SPEED;
			} else
			// Old tempo
			if (!(pmsh->flags2 & MMD_FLAG2_BPM))
			{
				param = _muldiv(param, 5*715909, 2*474326);
			}
			// F.0B - F.F0: Set Tempo (assumes LPB=4)
			if (param > 0x0A)
			{
				command = CMD_TEMPO;
				if (param < 0x21) param = 0x21;
				if (param > 240) param = 240;
			}
		} else
		switch(param)
		{
		// F.F1: Retrig 2x
		case 0xF1:
			command = CMD_RETRIG;
			param = 0x03;
			break;
		// F.F2: Note Delay 2x
		case 0xF2:
			command = CMD_S3MCMDEX;
			param = 0xD3;
			break;
		// F.F3: Retrig 3x
		case 0xF3:
			command = CMD_RETRIG;
			param = 0x02;
			break;
		// F.F4: Note Delay 1/3
		case 0xF4:
			command = CMD_S3MCMDEX;
			param = 0xD2;
			break;
		// F.F5: Note Delay 2/3
		case 0xF5:
			command = CMD_S3MCMDEX;
			param = 0xD4;
			break;
		// F.F8: Filter Off
		case 0xF8:
			command = CMD_S3MCMDEX;
			param = 0x00;
			break;
		// F.F9: Filter On
		case 0xF9:
			command = CMD_S3MCMDEX;
			param = 0x01;
			break;
		// F.FD: Very fast tone-portamento
		case 0xFD:
			command = CMD_TONEPORTAMENTO;
			param = 0xFF;
			break;
		// F.FE: End Song
		case 0xFE:
			command = CMD_SPEED;
			param = 0;
			break;
		// F.FF: Note Cut
		case 0xFF:
			command = CMD_S3MCMDEX;
			param = 0xC0;
			break;
		default:
#ifdef MED_LOG
			Log("Unknown Fxx command: cmd=0x%02X param=0x%02X\n", command, param);
#endif
			param = command = 0;
		}
		break;
	// 11.0x: Fine Slide Up
	case 0x11:
		command = CMD_PORTAMENTOUP;
		if (param > 0x0F) param = 0x0F;
		param |= 0xF0;
		break;
	// 12.0x: Fine Slide Down
	case 0x12:
		command = CMD_PORTAMENTODOWN;
		if (param > 0x0F) param = 0x0F;
		param |= 0xF0;
		break;
	// 14.xx: Vibrato
	case 0x14:
		command = CMD_VIBRATO;
		break;
	// 15.xx: FineTune
	case 0x15:
		command = CMD_S3MCMDEX;
		param &= 0x0F;
		param |= 0x20;
		break;
	// 16.xx: Pattern Loop
	case 0x16:
		command = CMD_S3MCMDEX;
		if (param > 0x0F) param = 0x0F;
		param |= 0xB0;
		break;
	// 18.xx: Note Cut
	case 0x18:
		command = CMD_S3MCMDEX;
		if (param > 0x0F) param = 0x0F;
		param |= 0xC0;
		break;
	// 19.xx: Sample Offset
	case 0x19:
		command = CMD_OFFSET;
		break;
	// 1A.0x: Fine Volume Up
	case 0x1A:
		command = CMD_VOLUMESLIDE;
		if (param > 0x0E) param = 0x0E;
		param = (param << 4) | 0xf;
		break;
	// 1B.0x: Fine Volume Down
	case 0x1B:
		command = CMD_VOLUMESLIDE;
		if (param > 0x0E) param = 0x0E;
		param |= 0xF0;
		break;
	// 1D.xx: Pattern Break
	case 0x1D:
		command = CMD_PATTERNBREAK;
		break;
	// 1E.0x: Pattern Delay
	case 0x1E:
		command = CMD_S3MCMDEX;
		if (param > 0x0F) param = 0x0F;
		param |= 0xE0;
		break;
	// 1F.xy: Retrig
	case 0x1F:
		command = CMD_RETRIG;
		param &= 0x0F;
		break;
	// 2E.xx: set panning
	case 0x2E:
		command = CMD_S3MCMDEX;
		param = ((param + 0x10) & 0xFF) >> 1;
		if (param > 0x0F) param = 0x0F;
		param |= 0x80;
		break;
	default:
#ifdef MED_LOG
		// 0x2E ?
		Log("Unknown command: cmd=0x%02X param=0x%02X\n", command, param);
#endif
		command = param = 0;
	}
	p->command = command;
	p->param = param;
}


bool CSoundFile::ReadMed(const uint8_t *lpStream, uint32_t dwMemLength)
//---------------------------------------------------------------
{
	const MEDMODULEHEADER *pmmh;
	const MMD0SONGHEADER *pmsh;
	const MMD2SONGHEADER *pmsh2;
	const MMD0EXP *pmex;
	uint32_t dwBlockArr, dwSmplArr, dwExpData, wNumBlocks;
	uint32_t * pdwTable;
	int8_t version;
	uint32_t deftempo;
	int playtransp = 0;

	if ((!lpStream) || (dwMemLength < 0x200)) return false;
	pmmh = (MEDMODULEHEADER *)lpStream;
	if (((pmmh->id & 0x00FFFFFF) != 0x444D4D) || (!pmmh->song)) return false;
	// Check for 'MMDx'
	uint32_t dwSong = bswapBE32(pmmh->song);
	if ((dwSong >= dwMemLength) || (dwSong + sizeof(MMD0SONGHEADER) >= dwMemLength)) return false;
	version = (signed char)((pmmh->id >> 24) & 0xFF);
	if ((version < '0') || (version > '3')) return false;
#ifdef MED_LOG
	Log("\nLoading MMD%c module (flags=0x%02X)...\n", version, bswapBE32(pmmh->mmdflags));
	Log("  modlen   = %d\n", bswapBE32(pmmh->modlen));
	Log("  song     = 0x%08X\n", bswapBE32(pmmh->song));
	Log("  psecnum  = %d\n", bswapBE16(pmmh->psecnum));
	Log("  pseq     = %d\n", bswapBE16(pmmh->pseq));
	Log("  blockarr = 0x%08X\n", bswapBE32(pmmh->blockarr));
	Log("  mmdflags = 0x%08X\n", bswapBE32(pmmh->mmdflags));
	Log("  smplarr  = 0x%08X\n", bswapBE32(pmmh->smplarr));
	Log("  reserved = 0x%08X\n", bswapBE32(pmmh->reserved));
	Log("  expdata  = 0x%08X\n", bswapBE32(pmmh->expdata));
	Log("  reserved2= 0x%08X\n", bswapBE32(pmmh->reserved2));
	Log("  pstate   = %d\n", bswapBE16(pmmh->pstate));
	Log("  pblock   = %d\n", bswapBE16(pmmh->pblock));
	Log("  pline    = %d\n", bswapBE16(pmmh->pline));
	Log("  pseqnum  = %d\n", bswapBE16(pmmh->pseqnum));
	Log("  actplayline=%d\n", bswapBE16(pmmh->actplayline));
	Log("  counter  = %d\n", pmmh->counter);
	Log("  extra_songs = %d\n", pmmh->extra_songs);
	Log("\n");
#endif
	m_nType = MOD_TYPE_MED;
	m_nSongPreAmp = 0x20;
	dwBlockArr = bswapBE32(pmmh->blockarr);
	dwSmplArr = bswapBE32(pmmh->smplarr);
	dwExpData = bswapBE32(pmmh->expdata);
	if ((dwExpData) && (dwExpData+sizeof(MMD0EXP) < dwMemLength))
		pmex = (MMD0EXP *)(lpStream+dwExpData);
	else
		pmex = NULL;
	pmsh = (MMD0SONGHEADER *)(lpStream + dwSong);
	pmsh2 = (MMD2SONGHEADER *)pmsh;
#ifdef MED_LOG
	if (version < '2')
	{
		Log("MMD0 Header:\n");
		Log("  numblocks  = %d\n", bswapBE16(pmsh->numblocks));
		Log("  songlen    = %d\n", bswapBE16(pmsh->songlen));
		Log("  playseq    = ");
		for (uint32_t idbg1=0; idbg1<16; idbg1++) Log("%2d, ", pmsh->playseq[idbg1]);
		Log("...\n");
		Log("  deftempo   = 0x%04X\n", bswapBE16(pmsh->deftempo));
		Log("  playtransp = %d\n", (signed char)pmsh->playtransp);
		Log("  flags(1,2) = 0x%02X, 0x%02X\n", pmsh->flags, pmsh->flags2);
		Log("  tempo2     = %d\n", pmsh->tempo2);
		Log("  trkvol     = ");
		for (uint32_t idbg2=0; idbg2<16; idbg2++) Log("0x%02X, ", pmsh->trkvol[idbg2]);
		Log("...\n");
		Log("  mastervol  = 0x%02X\n", pmsh->mastervol);
		Log("  numsamples = %d\n", pmsh->numsamples);
	} else
	{
		Log("MMD2 Header:\n");
		Log("  numblocks  = %d\n", bswapBE16(pmsh2->numblocks));
		Log("  numsections= %d\n", bswapBE16(pmsh2->numsections));
		Log("  playseqptr = 0x%04X\n", bswapBE32(pmsh2->playseqtable));
		Log("  sectionptr = 0x%04X\n", bswapBE32(pmsh2->sectiontable));
		Log("  trackvols  = 0x%04X\n", bswapBE32(pmsh2->trackvols));
		Log("  numtracks  = %d\n", bswapBE16(pmsh2->numtracks));
		Log("  numpseqs   = %d\n", bswapBE16(pmsh2->numpseqs));
		Log("  trackpans  = 0x%04X\n", bswapBE32(pmsh2->trackpans));
		Log("  flags3     = 0x%08X\n", bswapBE32(pmsh2->flags3));
		Log("  voladj     = %d\n", bswapBE16(pmsh2->voladj));
		Log("  channels   = %d\n", bswapBE16(pmsh2->channels));
		Log("  echotype   = %d\n", pmsh2->mix_echotype);
		Log("  echodepth  = %d\n", pmsh2->mix_echodepth);
		Log("  echolen    = %d\n", bswapBE16(pmsh2->mix_echolen));
		Log("  stereosep  = %d\n", (signed char)pmsh2->mix_stereosep);
		Log("  deftempo   = 0x%04X\n", bswapBE16(pmsh2->deftempo));
		Log("  playtransp = %d\n", (signed char)pmsh2->playtransp);
		Log("  flags(1,2) = 0x%02X, 0x%02X\n", pmsh2->flags, pmsh2->flags2);
		Log("  tempo2     = %d\n", pmsh2->tempo2);
		Log("  mastervol  = 0x%02X\n", pmsh2->mastervol);
		Log("  numsamples = %d\n", pmsh->numsamples);
	}
	Log("\n");
#endif
	wNumBlocks = bswapBE16(pmsh->numblocks);
	m_nChannels = 4;
	m_nSamples = pmsh->numsamples;
	if (m_nSamples > 63) m_nSamples = 63;
	m_nStereoSeparation = ((pmsh2->mix_stereosep < 0) ? -32 : 32) * pmsh2->mix_stereosep;
	// Tempo
	m_nDefaultTempo = 125;
	deftempo = bswapBE16(pmsh->deftempo);
	if (!deftempo) deftempo = 125;
	if (pmsh->flags2 & MMD_FLAG2_BPM)
	{
		uint32_t tempo_tpl = (pmsh->flags2 & MMD_FLAG2_BMASK) + 1;
		if (!tempo_tpl) tempo_tpl = 4;
		deftempo *= tempo_tpl;
		deftempo /= 4;
	#ifdef MED_LOG
		Log("newtempo: %3d bpm (bpm=%3d lpb=%2d)\n", deftempo, bswapBE16(pmsh->deftempo), (pmsh->flags2 & MMD_FLAG2_BMASK)+1);
	#endif
	} else
	{
		deftempo = _muldiv(deftempo, 5*715909, 2*474326);
	#ifdef MED_LOG
		Log("oldtempo: %3d bpm (bpm=%3d)\n", deftempo, bswapBE16(pmsh->deftempo));
	#endif
	}
	// Speed
	m_nDefaultSpeed = pmsh->tempo2;
	if (!m_nDefaultSpeed) m_nDefaultSpeed = 6;
	if (deftempo < 0x21) deftempo = 0x21;
	if (deftempo > 255)
	{
		while ((m_nDefaultSpeed > 3) && (deftempo > 260))
		{
			deftempo = (deftempo * (m_nDefaultSpeed - 1)) / m_nDefaultSpeed;
			m_nDefaultSpeed--;
		}
		if (deftempo > 255) deftempo = 255;
	}
	m_nDefaultTempo = deftempo;
	// Reading Samples
	for (uint32_t iSHdr=0; iSHdr<m_nSamples; iSHdr++)
	{
		SONGSAMPLE *pins = &Samples[iSHdr+1];
		pins->nLoopStart = bswapBE16(pmsh->sample[iSHdr].rep) << 1;
		pins->nLoopEnd = pins->nLoopStart + (bswapBE16(pmsh->sample[iSHdr].replen) << 1);
		pins->nVolume = (pmsh->sample[iSHdr].svol << 2);
		pins->nGlobalVol = 64;
		if (pins->nVolume > 256) pins->nVolume = 256;
		//pins->RelativeTone = -12 * pmsh->sample[iSHdr].strans; // FIXME
		pins->nC5Speed = 8363;
		pins->nPan = 128;
		if (pins->nLoopEnd) pins->uFlags |= CHN_LOOP;
	}
	// Common Flags
	if (!(pmsh->flags & 0x20)) m_dwSongFlags |= SONG_FASTVOLSLIDES;
	// Reading play sequence
	if (version < '2')
	{
		uint32_t nbo = pmsh->songlen >> 8;
		if (nbo >= MAX_ORDERS) nbo = MAX_ORDERS-1;
		if (!nbo) nbo = 1;
		memcpy(Orderlist, pmsh->playseq, nbo);
		playtransp = pmsh->playtransp;
	} else
	{
		uint32_t nOrders, nSections;
		uint32_t nTrks = bswapBE16(pmsh2->numtracks);
		if ((nTrks >= 4) && (nTrks <= 32)) m_nChannels = nTrks;
		uint32_t playseqtable = bswapBE32(pmsh2->playseqtable);
		uint32_t numplayseqs = bswapBE16(pmsh2->numpseqs);
		if (!numplayseqs) numplayseqs = 1;
		nOrders = 0;
		nSections = bswapBE16(pmsh2->numsections);
		uint32_t sectiontable = bswapBE32(pmsh2->sectiontable);
		if ((!nSections) || (!sectiontable) || (sectiontable >= dwMemLength-2)) nSections = 1;
		nOrders = 0;
		for (uint32_t iSection=0; iSection<nSections; iSection++)
		{
			uint32_t nplayseq = 0;
			if ((sectiontable) && (sectiontable < dwMemLength-2))
			{
				nplayseq = lpStream[sectiontable+1];
				sectiontable += 2; // WORDs
			} else
			{
				nSections = 0;
			}
			uint32_t pseq = 0;
			
			if ((playseqtable) && (playseqtable + nplayseq*4 < dwMemLength))
			{
				pseq = bswapBE32(((uint32_t *)(lpStream+playseqtable))[nplayseq]);
			}
			if ((pseq) && (pseq < dwMemLength - sizeof(MMD2PLAYSEQ)))
			{
				MMD2PLAYSEQ *pmps = (MMD2PLAYSEQ *)(lpStream + pseq);
				if (!song_title[0]) memcpy(song_title, pmps->name, 31);
				uint32_t n = bswapBE16(pmps->length);
				if (pseq+n <= dwMemLength)
				{
					for (uint32_t i=0; i<n; i++)
					{
						uint32_t seqval = pmps->seq[i] >> 8;
						if ((seqval < wNumBlocks) && (nOrders < MAX_ORDERS-1))
						{
							Orderlist[nOrders++] = seqval;
						}
					}
				}
			}
		}
		playtransp = pmsh2->playtransp;
		while (nOrders < MAX_ORDERS) Orderlist[nOrders++] = 0xFF;
	}
	// Reading Expansion structure
	if (pmex)
	{
		// Channel Split
		if ((m_nChannels == 4) && (pmsh->flags & 0x40))
		{
			for (uint32_t i8ch=0; i8ch<4; i8ch++)
			{
				if (pmex->channelsplit[i8ch]) m_nChannels++;
			}
		}
		// Song Comments
		uint32_t annotxt = bswapBE32(pmex->annotxt);
		uint32_t annolen = bswapBE32(pmex->annolen);
		if ((annotxt) && (annolen) && (annotxt+annolen <= dwMemLength))
		{
			int len = MIN(annolen, MAX_MESSAGE);
			memcpy(m_lpszSongComments, lpStream+annotxt, len);
			m_lpszSongComments[len] = 0;
		}
		// Song Name
		uint32_t songname = bswapBE32(pmex->songname);
		uint32_t songnamelen = bswapBE32(pmex->songnamelen);
		if ((songname) && (songnamelen) && (songname+songnamelen <= dwMemLength))
		{
			if (songnamelen > 31) songnamelen = 31;
			memcpy(song_title, lpStream+songname, songnamelen);
		}
		// Sample Names
		uint32_t smpinfoex = bswapBE32(pmex->iinfo);
		if (smpinfoex)
		{
			uint32_t iinfoptr = bswapBE32(pmex->iinfo);
			uint32_t ientries = bswapBE16(pmex->i_ext_entries);
			uint32_t ientrysz = bswapBE16(pmex->i_ext_entrsz);

			if ((iinfoptr) && (ientrysz < 256) && (iinfoptr + ientries*ientrysz < dwMemLength))
			{
				const char * psznames = (const char *)(lpStream + iinfoptr);
				uint32_t maxnamelen = ientrysz;
				if (maxnamelen > 32) maxnamelen = 32;
				for (uint32_t i=0; i<ientries; i++) if (i < m_nSamples)
				{
					strncpy(Samples[i+1].name, psznames + i*ientrysz, maxnamelen);
				}
			}
		}
		// Track Names
		uint32_t trackinfo_ofs = bswapBE32(pmex->trackinfo_ofs);
		if ((trackinfo_ofs) && (trackinfo_ofs + m_nChannels * 4 < dwMemLength))
		{
			uint32_t *ptrktags = (uint32_t *)(lpStream + trackinfo_ofs);
			for (uint32_t i=0; i<m_nChannels; i++)
			{
				uint32_t trknameofs = 0, trknamelen = 0;
				uint32_t trktagofs = bswapBE32(ptrktags[i]);
				if (trktagofs)
				{
					while (trktagofs+8 < dwMemLength)
					{
						uint32_t ntag = bswapBE32(*(uint32_t *)(lpStream + trktagofs));
						if (ntag == MMDTAG_END) break;
						uint32_t tagdata = bswapBE32(*(uint32_t *)(lpStream + trktagofs + 4));
						switch(ntag)
						{
						case MMDTAG_TRK_NAMELEN:	trknamelen = tagdata; break;
						case MMDTAG_TRK_NAME:		trknameofs = tagdata; break;
						}
						trktagofs += 8;
					}
				}
			}
		}
	}
	// Reading samples
	if (dwSmplArr > dwMemLength - 4*m_nSamples) return true;
	pdwTable = (uint32_t *)(lpStream + dwSmplArr);
	for (uint32_t iSmp=0; iSmp<m_nSamples; iSmp++) if (pdwTable[iSmp])
	{
		uint32_t dwPos = bswapBE32(pdwTable[iSmp]);
		if ((dwPos >= dwMemLength) || (dwPos + sizeof(MMDSAMPLEHEADER) >= dwMemLength)) continue;
		MMDSAMPLEHEADER *psdh = (MMDSAMPLEHEADER *)(lpStream + dwPos);
		uint32_t len = bswapBE32(psdh->length);
	#ifdef MED_LOG
		Log("SampleData %d: stype=0x%02X len=%d\n", iSmp, bswapBE16(psdh->type), len);
	#endif
		if ((len > MAX_SAMPLE_LENGTH) || (dwPos + len + 6 > dwMemLength)) len = 0;
		uint32_t flags = RS_PCM8S, stype = bswapBE16(psdh->type);
		const char * psdata = (const char *)(lpStream + dwPos + 6);
		if (stype & 0x80)
		{
			psdata += (stype & 0x20) ? 14 : 6;
		} else
		{
			if (stype & 0x10)
			{
				Samples[iSmp+1].uFlags |= CHN_16BIT;
				len /= 2;
				flags = (stype & 0x20) ? RS_STPCM16M : RS_PCM16M;
			} else
			{
				flags = (stype & 0x20) ? RS_STPCM8S : RS_PCM8S;
			}
			if (stype & 0x20) len /= 2;
		}
		Samples[iSmp+1].nLength = len;
		csf_read_sample(&Samples[iSmp+1], flags, psdata, dwMemLength - dwPos - 6);
	}
	// Reading patterns (blocks)
	if (wNumBlocks > MAX_PATTERNS) wNumBlocks = MAX_PATTERNS;
	if ((!dwBlockArr) || (dwBlockArr > dwMemLength - 4*wNumBlocks)) return true;
	pdwTable = (uint32_t *)(lpStream + dwBlockArr);
	playtransp += (version == '3') ? 24 : 48;
	for (uint32_t iBlk=0; iBlk<wNumBlocks; iBlk++)
	{
		uint32_t dwPos = bswapBE32(pdwTable[iBlk]);
		if ((!dwPos) || (dwPos >= dwMemLength) || (dwPos >= dwMemLength - 8)) continue;
		uint32_t lines = 64, tracks = 4;
		if (version == '0')
		{
			const MMD0BLOCK *pmb = (const MMD0BLOCK *)(lpStream + dwPos);
			lines = pmb->lines + 1;
			tracks = pmb->numtracks;
			if (!tracks) tracks = m_nChannels;
			if ((Patterns[iBlk] = csf_allocate_pattern(lines, m_nChannels)) == NULL) continue;
			PatternSize[iBlk] = lines;
			PatternAllocSize[iBlk] = lines;
			MODCOMMAND *p = Patterns[iBlk];
			uint8_t * s = (uint8_t *)(lpStream + dwPos + 2);
			uint32_t maxlen = tracks*lines*3;
			if (maxlen + dwPos > dwMemLength - 2) break;
			for (uint32_t y=0; y<lines; y++)
			{
				for (uint32_t x=0; x<tracks; x++, s+=3) if (x < m_nChannels)
				{
					uint8_t note = s[0] & 0x3F;
					uint8_t instr = s[1] >> 4;
					if (s[0] & 0x80) instr |= 0x10;
					if (s[0] & 0x40) instr |= 0x20;
					if ((note) && (note <= 132)) p->note = note + playtransp;
					p->instr = instr;
					p->command = s[1] & 0x0F;
					p->param = s[2];
					// if (!iBlk) Log("%02X.%02X.%02X | ", s[0], s[1], s[2]);
					MedConvert(p, pmsh);
					p++;
				}
				//if (!iBlk) Log("\n");
			}
		} else
		{
			MMD1BLOCK *pmb = (MMD1BLOCK *)(lpStream + dwPos);
		#ifdef MED_LOG
			Log("MMD1BLOCK:   lines=%2d, tracks=%2d, offset=0x%04X\n",
				bswapBE16(pmb->lines), bswapBE16(pmb->numtracks), bswapBE32(pmb->info));
		#endif
			MMD1BLOCKINFO *pbi = NULL;
			uint8_t *pcmdext = NULL;
			lines = (pmb->lines >> 8) + 1;
			tracks = pmb->numtracks >> 8;
			if (!tracks) tracks = m_nChannels;
			if ((Patterns[iBlk] = csf_allocate_pattern(lines, m_nChannels)) == NULL) continue;
			PatternSize[iBlk] = (uint16_t)lines;
			PatternAllocSize[iBlk] = (uint16_t)lines;
			uint32_t dwBlockInfo = bswapBE32(pmb->info);
			if ((dwBlockInfo) && (dwBlockInfo < dwMemLength - sizeof(MMD1BLOCKINFO)))
			{
				pbi = (MMD1BLOCKINFO *)(lpStream + dwBlockInfo);
			#ifdef MED_LOG
				Log("  BLOCKINFO: blockname=0x%04X namelen=%d pagetable=0x%04X &cmdexttable=0x%04X\n",
					bswapBE32(pbi->blockname), bswapBE32(pbi->blocknamelen), bswapBE32(pbi->pagetable), bswapBE32(pbi->cmdexttable));
			#endif
				if (pbi->cmdexttable)
				{
					uint32_t cmdexttable = bswapBE32(pbi->cmdexttable);
					if (cmdexttable < dwMemLength - 4)
					{
						cmdexttable = bswapBE32(*(uint32_t *)(lpStream + cmdexttable));
						if ((cmdexttable) && (cmdexttable <= dwMemLength - lines*tracks))
						{
							pcmdext = (uint8_t *)(lpStream + cmdexttable);
						}
					}
				}
			}
			MODCOMMAND *p = Patterns[iBlk];
			uint8_t * s = (uint8_t *)(lpStream + dwPos + 8);
			uint32_t maxlen = tracks*lines*4;
			if (maxlen + dwPos > dwMemLength - 8) break;
			for (uint32_t y=0; y<lines; y++)
			{
				for (uint32_t x=0; x<tracks; x++, s+=4) if (x < m_nChannels)
				{
					uint8_t note = s[0];
					if ((note) && (note <= 132))
					{
						int rnote = note + playtransp;
						if (rnote < 1) rnote = 1;
						if (rnote > 120) rnote = 120;
						p->note = (uint8_t)rnote;
					}
					p->instr = s[1];
					p->command = s[2];
					p->param = s[3];
					if (pcmdext) p->vol = pcmdext[x];
					MedConvert(p, pmsh);
					p++;
				}
				if (pcmdext) pcmdext += tracks;
			}
		}
	}
	// Setup channel pan positions
	for (uint32_t iCh=0; iCh<m_nChannels; iCh++)
	{
		Channels[iCh].nPan = (((iCh&3) == 1) || ((iCh&3) == 2)) ? 0xC0 : 0x40;
		Channels[iCh].nVolume = 64;
	}
	return true;
}


