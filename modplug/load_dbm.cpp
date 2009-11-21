/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

///////////////////////////////////////////////////////////////
//
// DigiBooster Pro Module Loader (*.dbm)
//
// Note: this loader doesn't handle multiple songs
//
///////////////////////////////////////////////////////////////

#include "sndfile.h"
#include "snd_fx.h"

//#pragma warning(disable:4244)

#define DBM_FILE_MAGIC  0x304d4244
#define DBM_ID_NAME             0x454d414e
#define DBM_NAMELEN             0x2c000000
#define DBM_ID_INFO             0x4f464e49
#define DBM_INFOLEN             0x0a000000
#define DBM_ID_SONG             0x474e4f53
#define DBM_ID_INST             0x54534e49
#define DBM_ID_VENV             0x564e4556
#define DBM_ID_PATT             0x54544150
#define DBM_ID_SMPL             0x4c504d53

#pragma pack(1)

typedef struct DBMFILEHEADER
{
        uint32_t dbm_id;                // "DBM0" = 0x304d4244
        uint16_t trkver;                // Tracker version: 02.15
        uint16_t reserved;
        uint32_t name_id;               // "NAME" = 0x454d414e
        uint32_t name_len;              // name length: always 44
        int8_t songname[44];
        uint32_t info_id;               // "INFO" = 0x4f464e49
        uint32_t info_len;              // 0x0a000000
        uint16_t instruments;
        uint16_t samples;
        uint16_t songs;
        uint16_t patterns;
        uint16_t channels;
        uint32_t song_id;               // "SONG" = 0x474e4f53
        uint32_t song_len;
        int8_t songname2[44];
        uint16_t orders;
//      uint16_t orderlist[0];  // orderlist[orders] in words
} DBMFILEHEADER;

typedef struct DBMINSTRUMENT
{
        int8_t name[30];
        uint16_t sampleno;
        uint16_t volume;
        uint32_t finetune;
        uint32_t loopstart;
        uint32_t looplen;
        uint16_t panning;
        uint16_t flags;
} DBMINSTRUMENT;

typedef struct DBMENVELOPE
{
        uint16_t instrument;
        uint8_t flags;
        uint8_t numpoints;
        uint8_t sustain1;
        uint8_t loopbegin;
        uint8_t loopend;
        uint8_t sustain2;
        uint16_t volenv[2*32];
} DBMENVELOPE;

typedef struct DBMPATTERN
{
        uint16_t rows;
        uint32_t packedsize;
        uint8_t patterndata[2]; // [packedsize]
} DBMPATTERN;

typedef struct DBMSAMPLE
{
        uint32_t flags;
        uint32_t samplesize;
        uint8_t sampledata[2];          // [samplesize]
} DBMSAMPLE;

#pragma pack()


bool CSoundFile::ReadDBM(const uint8_t *lpStream, uint32_t dwMemLength)
//---------------------------------------------------------------
{
        DBMFILEHEADER *pfh = (DBMFILEHEADER *)lpStream;
        uint32_t dwMemPos;
        uint32_t nOrders, nSamples, nInstruments, nPatterns;

        if ((!lpStream) || (dwMemLength <= sizeof(DBMFILEHEADER)) || (!pfh->channels)
         || (pfh->dbm_id != DBM_FILE_MAGIC) || (!pfh->songs) || (pfh->song_id != DBM_ID_SONG)
         || (pfh->name_id != DBM_ID_NAME) || (pfh->name_len != DBM_NAMELEN)
         || (pfh->info_id != DBM_ID_INFO) || (pfh->info_len != DBM_INFOLEN)) return false;
        dwMemPos = sizeof(DBMFILEHEADER);
        nOrders = bswapBE16(pfh->orders);
        if (dwMemPos + 2 * nOrders + 8*3 >= dwMemLength) return false;
        nInstruments = bswapBE16(pfh->instruments);
        nSamples = bswapBE16(pfh->samples);
        nPatterns = bswapBE16(pfh->patterns);
        m_nType = MOD_TYPE_DBM;
        m_nChannels = bswapBE16(pfh->channels);
        if (m_nChannels < 4) m_nChannels = 4;
        if (m_nChannels > 64) m_nChannels = 64;
        memcpy(song_title, (pfh->songname[0]) ? pfh->songname : pfh->songname2, 32);
        song_title[31] = 0;
        for (uint32_t iOrd=0; iOrd < nOrders; iOrd++)
        {
                Orderlist[iOrd] = lpStream[dwMemPos+iOrd*2+1];
                if (iOrd >= MAX_ORDERS-2) break;
        }
        dwMemPos += 2*nOrders;
        while (dwMemPos + 10 < dwMemLength)
        {
                uint32_t chunk_id = ((uint32_t *)(lpStream+dwMemPos))[0];
                uint32_t chunk_size = bswapBE32(((uint32_t *)(lpStream+dwMemPos))[1]);
                uint32_t chunk_pos;

                dwMemPos += 8;
                chunk_pos = dwMemPos;
                if ((dwMemPos + chunk_size > dwMemLength) || (chunk_size > dwMemLength)) break;
                dwMemPos += chunk_size;
                // Instruments
                if (chunk_id == DBM_ID_INST)
                {
                        if (nInstruments >= MAX_INSTRUMENTS) nInstruments = MAX_INSTRUMENTS-1;
                        for (uint32_t iIns=0; iIns<nInstruments; iIns++)
                        {
                                SONGSAMPLE *psmp;
                                SONGINSTRUMENT *penv;
                                DBMINSTRUMENT *pih;
                                uint32_t nsmp;

                                if (chunk_pos + sizeof(DBMINSTRUMENT) > dwMemPos) break;
                                if ((penv = csf_allocate_instrument()) == NULL) break;
                                pih = (DBMINSTRUMENT *)(lpStream+chunk_pos);
                                nsmp = bswapBE16(pih->sampleno);
                                psmp = ((nsmp) && (nsmp < MAX_SAMPLES)) ? &Samples[nsmp] : NULL;
                                memcpy(penv->name, pih->name, 30);
                                if (psmp)
                                {
                                        memcpy(psmp->name, pih->name, 30);
                                        psmp->name[30] = 0;
                                }
                                Instruments[iIns+1] = penv;
                                penv->nFadeOut = 1024;  // ???
                                penv->nGlobalVol = 128;
                                penv->nPan = bswapBE16(pih->panning);
                                if ((penv->nPan) && (penv->nPan < 256))
                                        penv->dwFlags = ENV_SETPANNING;
                                else
                                        penv->nPan = 128;
                                penv->nPPC = 5*12;
                                for (uint32_t i=0; i<120; i++)
                                {
                                        penv->Keyboard[i] = nsmp;
                                        penv->NoteMap[i] = i+1;
                                }
                                // Sample Info
                                if (psmp)
                                {
                                        uint32_t sflags = bswapBE16(pih->flags);
                                        psmp->nVolume = bswapBE16(pih->volume) * 4;
                                        if ((!psmp->nVolume) || (psmp->nVolume > 256)) psmp->nVolume = 256;
                                        psmp->nGlobalVol = 64;
                                        psmp->nC5Speed = bswapBE32(pih->finetune);
                                        // what?
                                        //int f2t = frequency_to_transpose(psmp->nC5Speed);
                                        //psmp->RelativeTone = f2t >> 7;
                                        //psmp->nFineTune = f2t & 0x7F;
                                        if ((pih->looplen) && (sflags & 3))
                                        {
                                                psmp->nLoopStart = bswapBE32(pih->loopstart);
                                                psmp->nLoopEnd = psmp->nLoopStart + bswapBE32(pih->looplen);
                                                psmp->uFlags |= CHN_LOOP;
                                                psmp->uFlags &= ~CHN_PINGPONGLOOP;
                                                if (sflags & 2) psmp->uFlags |= CHN_PINGPONGLOOP;
                                        }
                                }
                                chunk_pos += sizeof(DBMINSTRUMENT);
                                m_nInstruments = iIns+1;
                        }
                        m_dwSongFlags |= SONG_INSTRUMENTMODE;
                } else
                // Volume Envelopes
                if (chunk_id == DBM_ID_VENV)
                {
                        uint32_t nEnvelopes = lpStream[chunk_pos+1];

                        chunk_pos += 2;
                        for (uint32_t iEnv=0; iEnv<nEnvelopes; iEnv++)
                        {
                                DBMENVELOPE *peh;
                                uint32_t nins;

                                if (chunk_pos + sizeof(DBMENVELOPE) > dwMemPos) break;
                                peh = (DBMENVELOPE *)(lpStream+chunk_pos);
                                nins = bswapBE16(peh->instrument);
                                if ((nins) && (nins < MAX_INSTRUMENTS) && (Instruments[nins]) && (peh->numpoints))
                                {
                                        SONGINSTRUMENT *penv = Instruments[nins];

                                        if (peh->flags & 1) penv->dwFlags |= ENV_VOLUME;
                                        if (peh->flags & 2) penv->dwFlags |= ENV_VOLSUSTAIN;
                                        if (peh->flags & 4) penv->dwFlags |= ENV_VOLLOOP;
                                        penv->VolEnv.nNodes = peh->numpoints + 1;
                                        if (penv->VolEnv.nNodes > MAX_ENVPOINTS) penv->VolEnv.nNodes = MAX_ENVPOINTS;
                                        penv->VolEnv.nLoopStart = peh->loopbegin;
                                        penv->VolEnv.nLoopEnd = peh->loopend;
                                        penv->VolEnv.nSustainStart = penv->VolEnv.nSustainEnd = peh->sustain1;
                                        for (int i=0; i<penv->VolEnv.nNodes; i++)
                                        {
                                                penv->VolEnv.Ticks[i] = bswapBE16(peh->volenv[i*2]);
                                                penv->VolEnv.Values[i] = (uint8_t)bswapBE16(peh->volenv[i*2+1]);
                                        }
                                }
                                chunk_pos += sizeof(DBMENVELOPE);
                        }
                } else
                // Packed Pattern Data
                if (chunk_id == DBM_ID_PATT)
                {
                        if (nPatterns > MAX_PATTERNS) nPatterns = MAX_PATTERNS;
                        for (uint32_t iPat=0; iPat<nPatterns; iPat++)
                        {
                                DBMPATTERN *pph;
                                uint32_t pksize;
                                uint32_t nRows;

                                if (chunk_pos + sizeof(DBMPATTERN) > dwMemPos) break;
                                pph = (DBMPATTERN *)(lpStream+chunk_pos);
                                pksize = bswapBE32(pph->packedsize);
                                if ((chunk_pos + pksize + 6 > dwMemPos) || (pksize > dwMemPos)) break;
                                nRows = bswapBE16(pph->rows);
                                if ((nRows >= 4) && (nRows <= 256))
                                {
                                        MODCOMMAND *m = csf_allocate_pattern(nRows, m_nChannels);
                                        if (m)
                                        {
                                                uint8_t * pkdata = (uint8_t *)&pph->patterndata;
                                                uint32_t row = 0;
                                                uint32_t i = 0;

                                                PatternSize[iPat] = nRows;
                                                PatternAllocSize[iPat] = nRows;
                                                Patterns[iPat] = m;
                                                while ((i+3<pksize) && (row < nRows))
                                                {
                                                        uint32_t ch = pkdata[i++];

                                                        if (ch)
                                                        {
                                                                uint8_t b = pkdata[i++];
                                                                ch--;
                                                                if (ch < m_nChannels)
                                                                {
                                                                        if (b & 0x01)
                                                                        {
                                                                                uint32_t note = pkdata[i++];

                                                                                if (note == 0x1F) note = 0xFF; else
                                                                                if ((note) && (note < 0xFE))
                                                                                {
                                                                                        note = ((note >> 4)*12) + (note & 0x0F) + 13;
                                                                                }
                                                                                m[ch].note = note;
                                                                        }
                                                                        if (b & 0x02) m[ch].instr = pkdata[i++];
                                                                        if (b & 0x3C)
                                                                        {
                                                                                uint32_t cmd1 = 0xFF, param1 = 0, cmd2 = 0xFF, param2 = 0;
                                                                                if (b & 0x04) cmd1 = (uint32_t)pkdata[i++];
                                                                                if (b & 0x08) param1 = pkdata[i++];
                                                                                if (b & 0x10) cmd2 = (uint32_t)pkdata[i++];
                                                                                if (b & 0x20) param2 = pkdata[i++];
                                                                                if (cmd1 == 0x0C)
                                                                                {
                                                                                        m[ch].volcmd = VOLCMD_VOLUME;
                                                                                        m[ch].vol = param1;
                                                                                        cmd1 = 0xFF;
                                                                                } else
                                                                                if (cmd2 == 0x0C)
                                                                                {
                                                                                        m[ch].volcmd = VOLCMD_VOLUME;
                                                                                        m[ch].vol = param2;
                                                                                        cmd2 = 0xFF;
                                                                                }
                                                                                if ((cmd1 > 0x13) || ((cmd1 >= 0x10) && (cmd2 < 0x10)))
                                                                                {
                                                                                        cmd1 = cmd2;
                                                                                        param1 = param2;
                                                                                        cmd2 = 0xFF;
                                                                                }
                                                                                if (cmd1 <= 0x13)
                                                                                {
                                                                                        m[ch].command = cmd1;
                                                                                        m[ch].param = param1;
                                                                                        csf_import_mod_effect(&m[ch], 0);
                                                                                }
                                                                        }
                                                                } else
                                                                {
                                                                        if (b & 0x01) i++;
                                                                        if (b & 0x02) i++;
                                                                        if (b & 0x04) i++;
                                                                        if (b & 0x08) i++;
                                                                        if (b & 0x10) i++;
                                                                        if (b & 0x20) i++;
                                                                }
                                                        } else
                                                        {
                                                                row++;
                                                                m += m_nChannels;
                                                        }
                                                }
                                        }
                                }
                                chunk_pos += 6 + pksize;
                        }
                } else
                // Reading Sample Data
                if (chunk_id == DBM_ID_SMPL)
                {
                        if (nSamples >= MAX_SAMPLES) nSamples = MAX_SAMPLES-1;
                        m_nSamples = nSamples;
                        for (uint32_t iSmp=1; iSmp<=nSamples; iSmp++)
                        {
                                SONGSAMPLE *pins;
                                DBMSAMPLE *psh;
                                uint32_t samplesize;
                                uint32_t sampleflags;

                                if (chunk_pos + sizeof(DBMSAMPLE) >= dwMemPos) break;
                                psh = (DBMSAMPLE *)(lpStream+chunk_pos);
                                chunk_pos += 8;
                                samplesize = bswapBE32(psh->samplesize);
                                sampleflags = bswapBE32(psh->flags);
                                pins = &Samples[iSmp];
                                pins->nLength = samplesize;
                                if (sampleflags & 2)
                                {
                                        pins->uFlags |= CHN_16BIT;
                                        samplesize <<= 1;
                                }
                                if ((chunk_pos+samplesize > dwMemPos) || (samplesize > dwMemLength)) break;
                                if (sampleflags & 3)
                                {
                                        csf_read_sample(pins, (pins->uFlags & CHN_16BIT) ? RS_PCM16M : RS_PCM8S,
                                                                (const char *)(psh->sampledata), samplesize);
                                }
                                chunk_pos += samplesize;
                        }
                }
        }
        return true;
}

