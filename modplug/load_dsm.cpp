/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/

//////////////////////////////////////////////
// DSIK Internal Format (DSM) module loader //
//////////////////////////////////////////////
#include "sndfile.h"

#pragma pack(1)

#define DSMID_RIFF      0x46464952      // "RIFF"
#define DSMID_DSMF      0x464d5344      // "DSMF"
#define DSMID_SONG      0x474e4f53      // "SONG"
#define DSMID_INST      0x54534e49      // "INST"
#define DSMID_PATT      0x54544150      // "PATT"


typedef struct DSMNOTE
{
        uint8_t note,ins,vol,cmd,inf;
} DSMNOTE;


typedef struct DSMINST
{
        uint32_t id_INST;
        uint32_t inst_len;
        int8_t filename[13];
        uint8_t flags;
        uint8_t flags2;
        uint8_t volume;
        uint32_t length;
        uint32_t loopstart;
        uint32_t loopend;
        uint32_t reserved1;
        uint16_t c2spd;
        uint16_t reserved2;
        int8_t samplename[28];
} DSMINST;


typedef struct DSMFILEHEADER
{
        uint32_t id_RIFF;       // "RIFF"
        uint32_t riff_len;
        uint32_t id_DSMF;       // "DSMF"
        uint32_t id_SONG;       // "SONG"
        uint32_t song_len;
} DSMFILEHEADER;


typedef struct DSMSONG
{
        int8_t songname[28];
        uint16_t reserved1;
        uint16_t flags;
        uint32_t reserved2;
        uint16_t numord;
        uint16_t numsmp;
        uint16_t numpat;
        uint16_t numtrk;
        uint8_t globalvol;
        uint8_t mastervol;
        uint8_t speed;
        uint8_t bpm;
        uint8_t panpos[16];
        uint8_t orders[128];
} DSMSONG;

typedef struct DSMPATT
{
        uint32_t id_PATT;
        uint32_t patt_len;
        uint8_t dummy1;
        uint8_t dummy2;
} DSMPATT;

#pragma pack()


bool CSoundFile::ReadDSM(const uint8_t * lpStream, uint32_t dwMemLength)
//-----------------------------------------------------------
{
        DSMFILEHEADER *pfh = (DSMFILEHEADER *)lpStream;
        DSMSONG *psong;
        uint32_t dwMemPos;
        uint32_t nPat, nSmp;

        if ((!lpStream) || (dwMemLength < 1024) || (pfh->id_RIFF != DSMID_RIFF)
         || (pfh->riff_len + 8 > dwMemLength) || (pfh->riff_len < 1024)
         || (pfh->id_DSMF != DSMID_DSMF) || (pfh->id_SONG != DSMID_SONG)
         || (pfh->song_len > dwMemLength)) return false;
        psong = (DSMSONG *)(lpStream + sizeof(DSMFILEHEADER));
        dwMemPos = sizeof(DSMFILEHEADER) + pfh->song_len;
        m_nType = MOD_TYPE_DSM;
        m_nChannels = psong->numtrk;
        if (m_nChannels < 4) m_nChannels = 4;
        if (m_nChannels > 16) m_nChannels = 16;
        m_nSamples = psong->numsmp;
        if (m_nSamples > MAX_SAMPLES) m_nSamples = MAX_SAMPLES;
        m_nDefaultSpeed = psong->speed;
        m_nDefaultTempo = psong->bpm;
        m_nDefaultGlobalVolume = psong->globalvol << 1;
        if (!m_nDefaultGlobalVolume || m_nDefaultGlobalVolume > 128) m_nDefaultGlobalVolume = 128;
        m_nSongPreAmp = psong->mastervol & 0x7F;
        for (uint32_t iOrd=0; iOrd<MAX_ORDERS; iOrd++)
        {
                Orderlist[iOrd] = (uint8_t)((iOrd < psong->numord) ? psong->orders[iOrd] : 0xFF);
        }
        for (uint32_t iPan=0; iPan<16; iPan++)
        {
                Channels[iPan].nPan = 0x80;
                if (psong->panpos[iPan] <= 0x80)
                {
                        Channels[iPan].nPan = psong->panpos[iPan] << 1;
                }
        }
        memcpy(song_title, psong->songname, 28);
        nPat = 0;
        nSmp = 1;
        while (dwMemPos < dwMemLength - 8)
        {
                DSMPATT *ppatt = (DSMPATT *)(lpStream + dwMemPos);
                DSMINST *pins = (DSMINST *)(lpStream+dwMemPos);
                // Reading Patterns
                if (ppatt->id_PATT == DSMID_PATT)
                {
                        dwMemPos += 8;
                        if (dwMemPos + ppatt->patt_len >= dwMemLength) break;
                        uint32_t dwPos = dwMemPos;
                        dwMemPos += ppatt->patt_len;
                        MODCOMMAND *m = csf_allocate_pattern(64, m_nChannels);
                        if (!m) break;
                        PatternSize[nPat] = 64;
                        PatternAllocSize[nPat] = 64;
                        Patterns[nPat] = m;
                        uint32_t row = 0;
                        while ((row < 64) && (dwPos + 2 <= dwMemPos))
                        {
                                uint32_t flag = lpStream[dwPos++];
                                if (flag)
                                {
                                        uint32_t ch = (flag & 0x0F) % m_nChannels;
                                        if (flag & 0x80)
                                        {
                                                uint32_t note = lpStream[dwPos++];
                                                if (note)
                                                {
                                                        if (note <= 12*9) note += 12;
                                                        m[ch].note = (uint8_t)note;
                                                }
                                        }
                                        if (flag & 0x40)
                                        {
                                                m[ch].instr = lpStream[dwPos++];
                                        }
                                        if (flag & 0x20)
                                        {
                                                m[ch].volcmd = VOLCMD_VOLUME;
                                                m[ch].vol = lpStream[dwPos++];
                                        }
                                        if (flag & 0x10)
                                        {
                                                uint32_t command = lpStream[dwPos++];
                                                uint32_t param = lpStream[dwPos++];
                                                switch(command)
                                                {
                                                // 4-bit Panning
                                                case 0x08:
                                                        switch(param & 0xF0)
                                                        {
                                                        case 0x00: param <<= 4; break;
                                                        case 0x10: command = 0x0A; param = (param & 0x0F) << 4; break;
                                                        case 0x20: command = 0x0E; param = (param & 0x0F) | 0xA0; break;
                                                        case 0x30: command = 0x0E; param = (param & 0x0F) | 0x10; break;
                                                        case 0x40: command = 0x0E; param = (param & 0x0F) | 0x20; break;
                                                        default: command = 0;
                                                        }
                                                        break;
                                                // Portamentos
                                                case 0x11:
                                                case 0x12:
                                                        command &= 0x0F;
                                                        break;
                                                // 3D Sound (?)
                                                case 0x13:
                                                        command = 'X' - 55;
                                                        param = 0x91;
                                                        break;
                                                default:
                                                        // Volume + Offset (?)
                                                        command = ((command & 0xF0) == 0x20) ? 0x09 : 0;
                                                }
                                                m[ch].command = (uint8_t)command;
                                                m[ch].param = (uint8_t)param;
                                                if (command) csf_import_mod_effect(&m[ch], 0);
                                        }
                                } else
                                {
                                        m += m_nChannels;
                                        row++;
                                }
                        }
                        nPat++;
                } else
                // Reading Samples
                if ((nSmp <= m_nSamples) && (pins->id_INST == DSMID_INST))
                {
                        if (dwMemPos + pins->inst_len >= dwMemLength - 8) break;
                        uint32_t dwPos = dwMemPos + sizeof(DSMINST);
                        dwMemPos += 8 + pins->inst_len;
                        SONGSAMPLE *psmp = &Samples[nSmp];
                        memcpy(psmp->name, pins->samplename, 28);
                        memcpy(psmp->filename, pins->filename, 13);
                        psmp->nGlobalVol = 64;
                        psmp->nC5Speed = pins->c2spd;
                        psmp->uFlags = (uint16_t)((pins->flags & 1) ? CHN_LOOP : 0);
                        psmp->nLength = pins->length;
                        psmp->nLoopStart = pins->loopstart;
                        psmp->nLoopEnd = pins->loopend;
                        psmp->nVolume = (uint16_t)(pins->volume << 2);
                        if (psmp->nVolume > 256) psmp->nVolume = 256;
                        uint32_t smptype = (pins->flags & 2) ? RS_PCM8S : RS_PCM8U;
                        csf_read_sample(psmp, smptype, (const char *)(lpStream+dwPos), dwMemLength - dwPos);
                        nSmp++;
                } else
                {
                        break;
                }
        }
        return true;
}

