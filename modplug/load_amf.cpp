/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 */
#define NEED_BYTESWAP

///////////////////////////////////////////////////
//
// AMF module loader
//
// There is 2 types of AMF files:
// - ASYLUM Music Format
// - Advanced Music Format(DSM)
//
///////////////////////////////////////////////////
#include "sndfile.h"

//#define AMFLOG

//#pragma warning(disable:4244)

#pragma pack(1)

typedef struct _AMFFILEHEADER
{
        uint8_t szAMF[3];
        uint8_t version;
        int8_t title[32];
        uint8_t numsamples;
        uint8_t numorders;
        uint16_t numtracks;
        uint8_t numchannels;
} AMFFILEHEADER;

typedef struct _AMFSAMPLE
{
        uint8_t type;
        int8_t  samplename[32];
        int8_t  filename[13];
        uint32_t offset;
        uint32_t length;
        uint16_t c2spd;
        uint8_t volume;
} AMFSAMPLE;


#pragma pack()


#ifdef AMFLOG
extern void Log(const char *, ...);
#endif

void AMF_Unpack(MODCOMMAND *pPat, const uint8_t *pTrack, uint32_t nRows, uint32_t nChannels)
//-------------------------------------------------------------------------------
{
        uint32_t lastinstr = 0;
        uint32_t nTrkSize = bswapLE16(*(uint16_t *)pTrack);
        nTrkSize += (uint32_t)pTrack[2] <<16;
        pTrack += 3;
        while (nTrkSize--)
        {
                uint32_t row = pTrack[0];
                uint32_t cmd = pTrack[1];
                uint32_t arg = pTrack[2];
                if (row >= nRows) break;
                MODCOMMAND *m = pPat + row * nChannels;
                if (cmd < 0x7F) // note+vol
                {
                        m->note = cmd+1;
                        if (!m->instr) m->instr = lastinstr;
                        m->volcmd = VOLCMD_VOLUME;
                        m->vol = arg;
                } else
                if (cmd == 0x7F) // duplicate row
                {
                        signed char rdelta = (signed char)arg;
                        int rowsrc = (int)row + (int)rdelta;
                        if ((rowsrc >= 0) && (rowsrc < (int)nRows)) memcpy(m, &pPat[rowsrc*nChannels],sizeof(pPat[rowsrc*nChannels]));
                } else
                if (cmd == 0x80) // instrument
                {
                        m->instr = arg+1;
                        lastinstr = m->instr;
                } else
                if (cmd == 0x83) // volume
                {
                        m->volcmd = VOLCMD_VOLUME;
                        m->vol = arg;
                } else
                // effect
                {
                        uint32_t command = cmd & 0x7F;
                        uint32_t param = arg;
                        switch(command)
                        {
                        // 0x01: Set Speed
                        case 0x01:      command = CMD_SPEED; break;
                        // 0x02: Volume Slide
                        // 0x0A: Tone Porta + Vol Slide
                        // 0x0B: Vibrato + Vol Slide
                        case 0x02:      command = CMD_VOLUMESLIDE;
                        case 0x0A:      if (command == 0x0A) command = CMD_TONEPORTAVOL;
                        case 0x0B:      if (command == 0x0B) command = CMD_VIBRATOVOL;
                                                if (param & 0x80) param = (-(signed char)param)&0x0F;
                                                else param = (param&0x0F)<<4;
                                                break;
                        // 0x04: Porta Up/Down
                        case 0x04:      if (param & 0x80) { command = CMD_PORTAMENTOUP; param = -(signed char)param; }
                                                else { command = CMD_PORTAMENTODOWN; } break;
                        // 0x06: Tone Portamento
                        case 0x06:      command = CMD_TONEPORTAMENTO; break;
                        // 0x07: Tremor
                        case 0x07:      command = CMD_TREMOR; break;
                        // 0x08: Arpeggio
                        case 0x08:      command = CMD_ARPEGGIO; break;
                        // 0x09: Vibrato
                        case 0x09:      command = CMD_VIBRATO; break;
                        // 0x0C: Pattern Break
                        case 0x0C:      command = CMD_PATTERNBREAK; break;
                        // 0x0D: Position Jump
                        case 0x0D:      command = CMD_POSITIONJUMP; break;
                        // 0x0F: Retrig
                        case 0x0F:      command = CMD_RETRIG; break;
                        // 0x10: Offset
                        case 0x10:      command = CMD_OFFSET; break;
                        // 0x11: Fine Volume Slide
                        case 0x11:      if (param) { command = CMD_VOLUMESLIDE;
                                                        if (param & 0x80) param = 0xF0|((-(signed char)param)&0x0F);
                                                        else param = 0x0F|((param&0x0F)<<4);
                                                } else command = 0; break;
                        // 0x12: Fine Portamento
                        // 0x16: Extra Fine Portamento
                        case 0x12:
                        case 0x16:      if (param) { int mask = (command == 0x16) ? 0xE0 : 0xF0;
                                                        command = (param & 0x80) ? CMD_PORTAMENTOUP : CMD_PORTAMENTODOWN;
                                                        if (param & 0x80) param = mask|((-(signed char)param)&0x0F);
                                                        else param |= mask;
                                                } else command = 0; break;
                        // 0x13: Note Delay
                        case 0x13:      command = CMD_S3MCMDEX; param = 0xD0|(param & 0x0F); break;
                        // 0x14: Note Cut
                        case 0x14:      command = CMD_S3MCMDEX; param = 0xC0|(param & 0x0F); break;
                        // 0x15: Set Tempo
                        case 0x15:      command = CMD_TEMPO; break;
                        // 0x17: Panning
                        case 0x17:      param = (param+64)&0x7F;
                                                if (m->command) { if (!m->volcmd) { m->volcmd = VOLCMD_PANNING;  m->vol = param/2; } command = 0; }
                                                else { command = CMD_PANNING; }
                        // Unknown effects
                        default:        command = param = 0;
                        }
                        if (command)
                        {
                                m->command = command;
                                m->param = param;
                        }
                }
                pTrack += 3;
        }
}



bool CSoundFile::ReadAMF(const uint8_t * lpStream, uint32_t dwMemLength)
//-----------------------------------------------------------
{
        AMFFILEHEADER *pfh = (AMFFILEHEADER *)lpStream;
        uint32_t dwMemPos;

        if ((!lpStream) || (dwMemLength < 2048)) return false;
        if ((!strncmp((const char *)lpStream, "ASYLUM Music Format V1.0", 25)) && (dwMemLength > 4096))
        {
                uint32_t numorders, numpats, numsamples;

                dwMemPos = 32;
                numpats = lpStream[dwMemPos+3];
                numorders = lpStream[dwMemPos+4];
                numsamples = 64;
                dwMemPos += 6;
                if ((!numpats) || (numpats > MAX_PATTERNS) || (!numorders)
                 || (numpats*64*32 + 294 + 37*64 >= dwMemLength)) return false;
                m_nType = MOD_TYPE_AMF0;
                m_nChannels = 8;
                m_nInstruments = 0;
                m_nSamples = 31;
                m_nDefaultTempo = 125;
                m_nDefaultSpeed = 6;
                for (uint32_t iOrd=0; iOrd<MAX_ORDERS; iOrd++)
                {
                        Orderlist[iOrd] = (iOrd < numorders) ? lpStream[dwMemPos+iOrd] : 0xFF;
                }
                dwMemPos = 294; // ???
                for (uint32_t iSmp=0; iSmp<numsamples; iSmp++)
                {
                        SONGSAMPLE *psmp = &Samples[iSmp+1];
                        memcpy(psmp->name, lpStream+dwMemPos, 22);
                        psmp->nC5Speed = S3MFineTuneTable[(lpStream[dwMemPos+22] & 0x0F) ^ 8];
                        psmp->nVolume = lpStream[dwMemPos+23];
                        psmp->nGlobalVol = 64;
                        if (psmp->nVolume > 0x40) psmp->nVolume = 0x40;
                        psmp->nVolume <<= 2;
                        psmp->nLength = bswapLE32(*((uint32_t *)(lpStream+dwMemPos+25)));
                        psmp->nLoopStart = bswapLE32(*((uint32_t *)(lpStream+dwMemPos+29)));
                        psmp->nLoopEnd = psmp->nLoopStart + bswapLE32(*((uint32_t *)(lpStream+dwMemPos+33)));
                        if ((psmp->nLoopEnd > psmp->nLoopStart) && (psmp->nLoopEnd <= psmp->nLength))
                        {
                                psmp->uFlags = CHN_LOOP;
                        } else
                        {
                                psmp->nLoopStart = psmp->nLoopEnd = 0;
                        }
                        if ((psmp->nLength) && (iSmp>31)) m_nSamples = iSmp+1;
                        dwMemPos += 37;
                }
                for (uint32_t iPat=0; iPat<numpats; iPat++)
                {
                        MODCOMMAND *p = csf_allocate_pattern(64, m_nChannels);
                        if (!p) break;
                        Patterns[iPat] = p;
                        PatternSize[iPat] = 64;
                        PatternAllocSize[iPat] = 64;
                        const uint8_t *pin = lpStream + dwMemPos;
                        for (uint32_t i=0; i<8*64; i++)
                        {
                                p->note = 0;

                                if (pin[0])
                                {
                                        p->note = pin[0] + 13;
                                }
                                p->instr = pin[1];
                                p->command = pin[2];
                                p->param = pin[3];
                                if (p->command > 0x0F)
                                {
                                #ifdef AMFLOG
                                        Log("0x%02X.0x%02X ?", p->command, p->param);
                                #endif
                                        p->command = 0;
                                }
                                csf_import_mod_effect(p, 0);
                                pin += 4;
                                p++;
                        }
                        dwMemPos += 64*32;
                }
                // Read samples
                for (uint32_t iData=0; iData<m_nSamples; iData++)
                {
                        SONGSAMPLE *psmp = &Samples[iData+1];
                        if (psmp->nLength)
                        {
                                dwMemPos += csf_read_sample(psmp, RS_PCM8S, (const char *)(lpStream+dwMemPos), dwMemLength);
                        }
                }
                return true;
        }
        ////////////////////////////
        // DSM/AMF
        uint16_t *ptracks[MAX_PATTERNS];
        uint32_t sampleseekpos[MAX_SAMPLES];

        if ((pfh->szAMF[0] != 'A') || (pfh->szAMF[1] != 'M') || (pfh->szAMF[2] != 'F')
         || (pfh->version < 10) || (pfh->version > 14) || (!bswapLE16(pfh->numtracks))
         || (!pfh->numorders) || (pfh->numorders > MAX_PATTERNS)
         || (!pfh->numsamples) || (pfh->numsamples > MAX_SAMPLES)
         || (pfh->numchannels < 4) || (pfh->numchannels > 32))
                return false;
        memcpy(song_title, pfh->title, 32);
        dwMemPos = sizeof(AMFFILEHEADER);
        m_nType = MOD_TYPE_AMF;
        m_nChannels = pfh->numchannels;
        m_nSamples = pfh->numsamples;
        m_nInstruments = 0;
        // Setup Channel Pan Positions
        if (pfh->version >= 11)
        {
                signed char *panpos = (signed char *)(lpStream + dwMemPos);
                uint32_t nchannels = (pfh->version >= 13) ? 32 : 16;
                for (uint32_t i=0; i<nchannels; i++)
                {
                        int pan = (panpos[i] + 64) * 2;
                        if (pan < 0) pan = 0;
                        if (pan > 256) { pan = 128; Channels[i].dwFlags |= CHN_SURROUND; }
                        Channels[i].nPan = pan;
                }
                dwMemPos += nchannels;
        } else
        {
                for (uint32_t i=0; i<16; i++)
                {
                        Channels[i].nPan = (lpStream[dwMemPos+i] & 1) ? 0x30 : 0xD0;
                }
                dwMemPos += 16;
        }
        // Get Tempo/Speed
        m_nDefaultTempo = 125;
        m_nDefaultSpeed = 6;
        if (pfh->version >= 13)
        {
                if (lpStream[dwMemPos] >= 32) m_nDefaultTempo = lpStream[dwMemPos];
                if (lpStream[dwMemPos+1] <= 32) m_nDefaultSpeed = lpStream[dwMemPos+1];
                dwMemPos += 2;
        }
        // Setup sequence list
        for (uint32_t iOrd=0; iOrd<MAX_ORDERS; iOrd++)
        {
                Orderlist[iOrd] = 0xFF;
                if (iOrd < pfh->numorders)
                {
                        Orderlist[iOrd] = iOrd;
                        PatternSize[iOrd] = 64;
                        PatternAllocSize[iOrd] = 64;
                        if (pfh->version >= 14)
                        {
                                PatternSize[iOrd] = bswapLE16(*(uint16_t *)(lpStream+dwMemPos));
                                PatternAllocSize[iOrd] = bswapLE16(*(uint16_t *)(lpStream+dwMemPos));
                                dwMemPos += 2;
                        }
                        ptracks[iOrd] = (uint16_t *)(lpStream+dwMemPos);
                        dwMemPos += m_nChannels * sizeof(uint16_t);
                }
        }
        if (dwMemPos + m_nSamples * (sizeof(AMFSAMPLE)+8) > dwMemLength) return true;
        // Read Samples
        uint32_t maxsampleseekpos = 0;
        for (uint32_t iIns=0; iIns<m_nSamples; iIns++)
        {
                SONGSAMPLE *pins = &Samples[iIns+1];
                AMFSAMPLE *psh = (AMFSAMPLE *)(lpStream + dwMemPos);

                dwMemPos += sizeof(AMFSAMPLE);
                memcpy(pins->name, psh->samplename, 32);
                memcpy(pins->filename, psh->filename, 13);
                pins->nLength = bswapLE32(psh->length);
                pins->nC5Speed = bswapLE16(psh->c2spd);
                pins->nGlobalVol = 64;
                pins->nVolume = psh->volume * 4;
                if (pfh->version >= 11)
                {
                        pins->nLoopStart = bswapLE32(*(uint32_t *)(lpStream+dwMemPos));
                        pins->nLoopEnd = bswapLE32(*(uint32_t *)(lpStream+dwMemPos+4));
                        dwMemPos += 8;
                } else
                {
                        pins->nLoopStart = bswapLE16(*(uint16_t *)(lpStream+dwMemPos));
                        pins->nLoopEnd = pins->nLength;
                        dwMemPos += 2;
                }
                sampleseekpos[iIns] = 0;
                if ((psh->type) && (bswapLE32(psh->offset) < dwMemLength-1))
                {
                        sampleseekpos[iIns] = bswapLE32(psh->offset);
                        if (bswapLE32(psh->offset) > maxsampleseekpos) maxsampleseekpos = bswapLE32(psh->offset);
                        if ((pins->nLoopEnd > pins->nLoopStart + 2)
                         && (pins->nLoopEnd <= pins->nLength)) pins->uFlags |= CHN_LOOP;
                }
        }
        // Read Track Mapping Table
        uint16_t *pTrackMap = (uint16_t *)(lpStream+dwMemPos);
        uint32_t realtrackcnt = 0;
        dwMemPos += pfh->numtracks * sizeof(uint16_t);
        for (uint32_t iTrkMap=0; iTrkMap<pfh->numtracks; iTrkMap++)
        {
                if (realtrackcnt < pTrackMap[iTrkMap]) realtrackcnt = pTrackMap[iTrkMap];
        }
        // Store tracks positions
        uint8_t **pTrackData = new uint8_t *[realtrackcnt];
        memset(pTrackData, 0, sizeof(pTrackData));
        for (uint32_t iTrack=0; iTrack<realtrackcnt; iTrack++) if (dwMemPos + 3 <= dwMemLength)
        {
                uint32_t nTrkSize = bswapLE16(*(uint16_t *)(lpStream+dwMemPos));
                nTrkSize += (uint32_t)lpStream[dwMemPos+2] << 16;

                if (dwMemPos + nTrkSize * 3 + 3 <= dwMemLength)
                {
                        pTrackData[iTrack] = (uint8_t *)(lpStream + dwMemPos);
                }
                dwMemPos += nTrkSize * 3 + 3;
        }
        // Create the patterns from the list of tracks
        for (uint32_t iPat=0; iPat<pfh->numorders; iPat++)
        {
                MODCOMMAND *p = csf_allocate_pattern(PatternSize[iPat], m_nChannels);
                if (!p) break;
                Patterns[iPat] = p;
                for (uint32_t iChn=0; iChn<m_nChannels; iChn++)
                {
                        uint32_t nTrack = bswapLE16(ptracks[iPat][iChn]);
                        if ((nTrack) && (nTrack <= pfh->numtracks))
                        {
                                uint32_t realtrk = bswapLE16(pTrackMap[nTrack-1]);
                                if (realtrk)
                                {
                                        realtrk--;
                                        if ((realtrk < realtrackcnt) && (pTrackData[realtrk]))
                                        {
                                                AMF_Unpack(p+iChn, pTrackData[realtrk], PatternSize[iPat], m_nChannels);
                                        }
                                }
                        }
                }
        }
        delete pTrackData;
        // Read Sample Data
        for (uint32_t iSeek=1; iSeek<=maxsampleseekpos; iSeek++)
        {
                if (dwMemPos >= dwMemLength) break;
                for (uint32_t iSmp=0; iSmp<m_nSamples; iSmp++) if (iSeek == sampleseekpos[iSmp])
                {
                        SONGSAMPLE *pins = &Samples[iSmp+1];
                        dwMemPos += csf_read_sample(pins, RS_PCM8U, (const char *)(lpStream+dwMemPos), dwMemLength-dwMemPos);
                        break;
                }
        }
        return true;
}


