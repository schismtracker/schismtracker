/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

#include "headers.h"
#include "version.h"

#include "sndfile.h"
#include "midi.h"
#include "it.h"

//////////////////////////////////////////////////////
// ScreamTracker S3M file support

typedef struct tagS3MSAMPLESTRUCT
{
        uint8_t type;
        int8_t dosname[12];
        uint8_t hmem;
        uint16_t memseg;
        uint32_t length;
        uint32_t loopbegin;
        uint32_t loopend;
        uint8_t vol;
        uint8_t bReserved;
        uint8_t pack;
        uint8_t flags;
        uint32_t finetune;
        uint32_t dwReserved;
        uint16_t intgp;
        uint16_t int512;
        uint32_t lastused;
        int8_t name[28];
        int8_t scrs[4];
} S3MSAMPLESTRUCT;


static uint8_t S3MFiller[16] =
{
        0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80,
        0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
};


bool CSoundFile::SaveS3M(disko_t *fp, uint32_t)
//----------------------------------------------------------
{
        uint8_t header[0x60];
        uint32_t nbo,nbi,nbp,i;
        uint32_t chanlim;
        uint16_t patptr[128];
        uint16_t insptr[128];
        uint8_t buffer[8+20*1024];
        S3MSAMPLESTRUCT insex[128];

        if ((!m_nChannels) || (!fp)) return false;
        // Writing S3M header
        memset(header, 0, sizeof(header));
        memset(insex, 0, sizeof(insex));
        memcpy(header, song_title, 0x1C);
        header[0x1B] = 0;
        header[0x1C] = 0x1A;
        header[0x1D] = 0x10;
        nbo = csf_get_num_orders(this);
        if (nbo < 2)
                nbo = 2;
        else if (nbo & 1)
                nbo++;
        header[0x20] = nbo & 0xFF;
        header[0x21] = nbo >> 8;

        nbi = m_nSamples+1;
        if (nbi > 99) nbi = 99;
        header[0x22] = nbi & 0xFF;
        header[0x23] = nbi >> 8;
        nbp = 0;
        for (i=0; Patterns[i]; i++) { nbp = i+1; if (nbp >= MAX_PATTERNS) break; }
        for (i=0; i<MAX_ORDERS; i++) if ((Orderlist[i] < MAX_PATTERNS) && (Orderlist[i] >= nbp)) nbp = Orderlist[i] + 1;
        header[0x24] = nbp & 0xFF;
        header[0x25] = nbp >> 8;
        //if (m_dwSongFlags & SONG_FASTVOLSLIDES) header[0x26] |= 0x40;
        //if ((m_nMaxPeriod < 20000) || (m_dwSongFlags & SONG_AMIGALIMITS)) header[0x26] |= 0x10;
        /* CWT/V identifiers:
            STx.yy  = 1xyy
            Orpheus = 2xyy
            Impulse = 3xyy
            So we'll use 4
        reference: http://xmp.cvs.sf.net/viewvc/xmp/xmp2/src/loaders/s3m_load.c?view=markup */
        header[0x28] = ver_cwtv & 0xff;
        header[0x29] = 0x40 | (ver_cwtv >> 8);
        header[0x2A] = 0x02; // Version = 1 => Signed samples
        header[0x2B] = 0x00;
        header[0x2C] = 'S';
        header[0x2D] = 'C';
        header[0x2E] = 'R';
        header[0x2F] = 'M';
        header[0x30] = m_nDefaultGlobalVolume >> 1;
        header[0x31] = m_nDefaultSpeed;
        header[0x32] = m_nDefaultTempo;
        header[0x33] = ((m_nSongPreAmp < 0x20) ? 0x20 : m_nSongPreAmp) | 0x80;  // Stereo
        header[0x35] = 0xFC;

        chanlim = csf_get_highest_used_channel(this) + 1;
        if (chanlim < 4) chanlim = 4;
        if (chanlim > 32) chanlim = 32;

        for (i=0; i<32; i++)
        {
                if (i < chanlim)
                {
                        uint32_t tmp = (i & 0x0F) >> 1;
                        header[0x40+i] = (i & 0x10) | ((i & 1) ? 8+tmp : tmp);
                } else header[0x40+i] = 0xFF;
        }
        fp->write(fp, header, 0x60);
        fp->write(fp, Orderlist, nbo);
        memset(patptr, 0, sizeof(patptr));
        memset(insptr, 0, sizeof(insptr));
        uint32_t ofs0 = 0x60 + nbo;
        uint32_t ofs1 = ((0x60 + nbo + nbi*2 + nbp*2 + 15) & 0xFFF0);
        uint32_t ofs = ofs1;
        if (header[0x35] == 0xFC) {
                ofs += 0x20;
                ofs1 += 0x20;
        }

        for (i=0; i<nbi; i++) insptr[i] = bswapLE16((uint16_t)((ofs + i*0x50) / 16));
        for (i=0; i<nbp; i++) patptr[i] = bswapLE16((uint16_t)((ofs + nbi*0x50) / 16));
        fp->write(fp, insptr, nbi*2);
        fp->write(fp, patptr, nbp*2);
        if (header[0x35] == 0xFC)
        {
                uint8_t chnpan[32];
                for (i=0; i<32; i++)
                {
                        uint32_t nPan = ((Channels[i].nPan+7) < 0xF0) ? Channels[i].nPan+7 : 0xF0;
                        chnpan[i] = (i<chanlim) ? 0x20 | (nPan >> 4) : 0x08;
                }
                fp->write(fp, chnpan, 0x20);
        }
        if ((nbi*2+nbp*2) & 0x0F)
        {
                fp->write(fp, S3MFiller, 0x10 - ((nbi*2+nbp*2) & 0x0F));
        }
        fp->seek(fp, ofs1);
        ofs1 = fp->pos;
        fp->write(fp, insex, nbi*0x50);
        // Packing patterns
        ofs += nbi*0x50;
        fp->seek(fp,ofs);
        for (i=0; i<nbp; i++)
        {
                uint16_t len = 64;
                memset(buffer, 0, sizeof(buffer));
                patptr[i] = bswapLE16(ofs / 16);
                if (Patterns[i])
                {
                        len = 2;
                        MODCOMMAND *p = Patterns[i];
                        if(PatternSize[i] < 64)
                                log_appendf(4, "Warning: Pattern %u has %u rows, padding", i, PatternSize[i]);
                        else if (PatternSize[i] > 64)
                                log_appendf(4, "Warning: Pattern %u has %u rows, truncating", i, PatternSize[i]);

                        int row;
                        for (row=0; row<PatternSize[i] && row < 64; row++)
                        {
                                for (uint32_t j=0; j < 32 && j<chanlim; j++)
                                {
                                        uint32_t b = j;
                                        MODCOMMAND *m = &p[row*m_nChannels+j];
                                        uint32_t note = m->note;
                                        uint32_t volcmd = m->volcmd;
                                        uint32_t vol = m->vol;
                                        uint32_t command = m->command;
                                        uint32_t param = m->param;
                                        uint32_t inst = m->instr;

                                        if (m_dwSongFlags & SONG_INSTRUMENTMODE
                                        && note && inst) {
                                                uint32_t nn = Instruments[inst]->Keyboard[note];
                                                uint32_t nm = Instruments[inst]->NoteMap[note];
                                                /* translate on save */
                                                note = nm;
                                                inst = nn;
                                        }


                                        if ((note) || (inst)) b |= 0x20;
                                        switch(note)
                                        {
                                            case 0: // no note
                                                note = 0xFF; break;
                                            case 0xFF: // NOTE_OFF ('===')
                                            case 0xFE: // NOTE_CUT ('^^^')
                                            case 0xFD: // NOTE_FADE ('~~~)
                                            {
                                                note = 0xFE; // Create ^^^
                                                // From S3M official documentation:
                                                // 254=key off (used with adlib, with samples stops smp)
                                                //
                                                // In fact, with AdLib S3M, notecut does not even exist.
                                                // The "SCx" opcode is a complete NOP in adlib.
                                                // There are only two ways to cut a note:
                                                // set volume to 0, or use keyoff.
                                                // With digital S3M, notecut is accomplished by ^^^.
                                                // So is notefade (except it doesn't fade),
                                                // and noteoff (except there are no volume
                                                // envelopes, so it cuts immediately).
                                                break;
                                            }
                                            case 1: case 2: case 3: case 4: case 5: case 6:
                                            case 7: case 8: case 9: case 10:case 11:case 12:
                                                note = 0; break; // too low
                                            default:
                                                note -= 13;
                                                // Convert into S3M format
                                                note = (note % 12) + ((note / 12) << 4);
                                                break;
                                        }
                                        if (command == CMD_VOLUME)
                                        {
                                                command = 0;
                                                if (param > 64) param = 64;
                                                volcmd = VOLCMD_VOLUME;
                                                vol = param;
                                        }
                                        if (volcmd == VOLCMD_VOLUME) b |= 0x40; else
                                        if (volcmd == VOLCMD_PANNING) { vol |= 0x80; b |= 0x40; }
                                        if (command)
                                        {
                                                csf_export_s3m_effect(&command, &param, false);
                                                if (command) b |= 0x80;
                                        }
                                        if (b & 0xE0)
                                        {
                                                buffer[len++] = b;
                                                if (b & 0x20)
                                                {
                                                        buffer[len++] = note;
                                                        buffer[len++] = inst;
                                                }
                                                if (b & 0x40)
                                                {
                                                        buffer[len++] = vol;
                                                }
                                                if (b & 0x80)
                                                {
                                                        buffer[len++] = command;
                                                        buffer[len++] = param;
                                                }
                                                if (len > sizeof(buffer) - 20) break;
                                        }
                                }
                                buffer[len++] = 0;
                                if (len > sizeof(buffer) - 20) break;
                        }
                        /* pad to 64 rows */
                        for (; row<64; row++)
                        {
                                buffer[len++] = 0;
                                if (len > sizeof(buffer) - 20) break;
                        }
                }
                buffer[0] = (len) & 0xFF;
                buffer[1] = (len) >> 8;
                len = (len+15) & (~0x0F);

                fp->write(fp, buffer, len);
                ofs += len;
        }
        // Writing samples
        for (i=1; i<=nbi; i++)
        {
                SONGSAMPLE *pins = &Samples[i];
                memcpy(insex[i-1].dosname, pins->filename, 12);
                memcpy(insex[i-1].name, pins->name, 28);
                memcpy(insex[i-1].scrs, "SCRS", 4);
                insex[i-1].hmem = (uint8_t)((uint32_t)ofs >> 20);
                insex[i-1].memseg = bswapLE16((uint16_t)((uint32_t)ofs >> 4));

                insex[i-1].vol = pins->nVolume / 4;
                insex[i-1].finetune = bswapLE32(pins->nC5Speed);

                if (pins->uFlags & CHN_ADLIB)
                {
                    insex[i-1].type = 2;
                    memcpy(&insex[i-1].length, pins->AdlibBytes, 12);
                    // AdlibBytes occupies length, loopbegin and loopend
                }
                else if (pins->pSample)
                {
                        insex[i-1].type = 1;
                        insex[i-1].length = bswapLE32(pins->nLength);
                        insex[i-1].loopbegin = bswapLE32(pins->nLoopStart);
                        insex[i-1].loopend = bswapLE32(pins->nLoopEnd);
                        insex[i-1].flags = (pins->uFlags & CHN_LOOP) ? 1 : 0;
                        uint32_t flags = SF_LE | SF_PCMU;
                        if (pins->uFlags & CHN_16BIT)
                        {
                                insex[i-1].flags |= 4;
                                flags |= SF_16;
                        } else {
                                flags |= SF_8;
                        }
                        if (pins->uFlags & CHN_STEREO)
                        {
                                insex[i-1].flags |= 2;
                                flags |= SF_SS;
                        } else {
                                flags = SF_M;
                        }
                        uint32_t len = csf_write_sample(fp, pins, flags, 0);
                        if (len & 0x0F)
                        {
                                fp->write(fp, S3MFiller, 0x10 - (len & 0x0F));
                        }
                        ofs += (len + 15) & (~0x0F);
                } else {
                        insex[i-1].length = 0;
                }
        }
        // Updating parapointers
        fp->seek(fp, ofs0);
        fp->write(fp, insptr, nbi*2);
        fp->write(fp, patptr, nbp*2);
        fp->seek(fp, ofs1);
        fp->write(fp, insex, 0x50*nbi);
        return true;
}

