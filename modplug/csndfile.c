/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>,
 *          Adam Goode       <adam@evdebs.org> (endian and char fixes for PPC)
*/
#define NEED_BYTESWAP

#include <math.h>
#include <stdint.h>
#include <assert.h>

#include "sndfile.h"
#include "log.h"
#include "util.h"
#include "fmt.h" // for it_decompress8 / it_decompress16


static void _csf_reset(CSoundFile *csf)
{
        unsigned int i;

        csf->m_dwSongFlags = 0;
        csf->m_nStereoSeparation = 128;
        csf->m_nChannels = 0;
        csf->m_nMixChannels = 0;
        csf->m_nSamples = 0;
        csf->m_nInstruments = 0;
        csf->m_nFreqFactor = csf->m_nTempoFactor = 128;
        csf->m_nDefaultGlobalVolume = 128;
        csf->m_nGlobalVolume = 128;
        csf->m_nDefaultSpeed = 6;
        csf->m_nDefaultTempo = 125;
        csf->m_nProcessRow = 0;
        csf->m_nRow = 0;
        csf->m_nCurrentPattern = 0;
        csf->m_nCurrentOrder = 0;
        csf->m_nProcessOrder = 0;
        csf->m_nSongPreAmp = 0x30;
        memset(csf->m_lpszSongComments, 0, sizeof(csf->m_lpszSongComments));

        csf->m_rowHighlightMajor = 16;
        csf->m_rowHighlightMinor = 4;

        memset(csf->Voices, 0, sizeof(csf->Voices));
        memset(csf->VoiceMix, 0, sizeof(csf->VoiceMix));
        memset(csf->Samples, 0, sizeof(csf->Samples));
        memset(csf->Instruments, 0, sizeof(csf->Instruments));
        memset(csf->Orderlist, 0xFF, sizeof(csf->Orderlist));
        memset(csf->Patterns, 0, sizeof(csf->Patterns));

        csf_reset_midi_cfg(csf);

        for (i = 0; i < MAX_PATTERNS; i++) {
                csf->PatternSize[i] = 64;
                csf->PatternAllocSize[i] = 64;
        }
        for (i = 0; i < MAX_SAMPLES; i++) {
                csf->Samples[i].nC5Speed = 8363;
                csf->Samples[i].nVolume = 64 * 4;
                csf->Samples[i].nGlobalVol = 64;
        }
        for (i = 0; i < MAX_CHANNELS; i++) {
                csf->Channels[i].nPan = 128;
                csf->Channels[i].nVolume = 64;
                csf->Channels[i].dwFlags = 0;
        }
}

//////////////////////////////////////////////////////////
// CSoundFile

CSoundFile *csf_allocate(void)
{
        CSoundFile *csf = calloc(1, sizeof(CSoundFile));
        _csf_reset(csf);
        return csf;
}

void csf_free(CSoundFile *csf)
{
        if (csf) {
                csf_destroy(csf);
                free(csf);
        }
}


static void _init_envelope(INSTRUMENTENVELOPE *env, int n)
{
        env->nNodes = 2;
        env->Ticks[0] = 0;
        env->Ticks[1] = 100;
        env->Values[0] = n;
        env->Values[1] = n;
}

void csf_init_instrument(SONGINSTRUMENT *ins, int samp)
{
        int n;
        _init_envelope(&ins->VolEnv, 64);
        _init_envelope(&ins->PanEnv, 32);
        _init_envelope(&ins->PitchEnv, 32);
        ins->nGlobalVol = 128;
        ins->nPan = 128;
        ins->wMidiBank = -1;
        ins->nMidiProgram = -1;
        ins->nPPC = 60; // why does pitch/pan not use the same note values as everywhere else?!
        for (n = 0; n < 128; n++) {
                ins->Keyboard[n] = samp;
                ins->NoteMap[n] = n + 1;
        }
}

SONGINSTRUMENT *csf_allocate_instrument(void)
{
        SONGINSTRUMENT *ins = calloc(1, sizeof(SONGINSTRUMENT));
        csf_init_instrument(ins, 0);
        return ins;
}

void csf_free_instrument(SONGINSTRUMENT *i)
{
        free(i);
}


void csf_destroy(CSoundFile *csf)
{
        int i;

        for (i = 0; i < MAX_PATTERNS; i++) {
                if (csf->Patterns[i]) {
                        csf_free_pattern(csf->Patterns[i]);
                        csf->Patterns[i] = NULL;
                }
        }
        for (i = 1; i < MAX_SAMPLES; i++) {
                SONGSAMPLE *pins = &csf->Samples[i];
                if (pins->pSample) {
                        csf_free_sample(pins->pSample);
                        pins->pSample = NULL;
                }
        }
        for (i = 0; i < MAX_INSTRUMENTS; i++) {
                if (csf->Instruments[i]) {
                        csf_free_instrument(csf->Instruments[i]);
                        csf->Instruments[i] = NULL;
                }
        }

        csf->m_nType = MOD_TYPE_NONE;
        csf->m_nChannels = csf->m_nSamples = csf->m_nInstruments = 0;

        _csf_reset(csf);
}

MODCOMMAND *csf_allocate_pattern(uint32_t rows, uint32_t channels)
{
        return calloc(rows * channels, sizeof(MODCOMMAND));
}

void csf_free_pattern(void *pat)
{
        free(pat);
}

signed char *csf_allocate_sample(uint32_t nbytes)
{
        signed char *p = calloc(1, (nbytes + 39) & ~7); // magic
        if (p)
                p += 16;
        return p;
}

void csf_free_sample(void *p)
{
        if (p)
                free(p - 16);
}


//////////////////////////////////////////////////////////////////////////
// Misc functions

MODMIDICFG default_midi_cfg;


void csf_reset_midi_cfg(CSoundFile *csf)
{
        memcpy(&csf->m_MidiCfg, &default_midi_cfg, sizeof(default_midi_cfg));
}


int csf_set_wave_config(CSoundFile *csf, uint32_t nRate,uint32_t nBits,uint32_t nChannels)
{
        int bReset = ((gdwMixingFreq != nRate) || (gnBitsPerSample != nBits) || (gnChannels != nChannels));
        gnChannels = nChannels;
        gdwMixingFreq = nRate;
        gnBitsPerSample = nBits;
        csf_init_player(csf, bReset);
//printf("Rate=%u Bits=%u Channels=%u\n",gdwMixingFreq,gnBitsPerSample,gnChannels);
        return 1;
}


int csf_set_resampling_mode(UNUSED CSoundFile *csf, uint32_t nMode)
{
        uint32_t d = gdwSoundSetup & ~(SNDMIX_NORESAMPLING|SNDMIX_HQRESAMPLER|SNDMIX_ULTRAHQSRCMODE);
        switch(nMode) {
                case SRCMODE_NEAREST:   d |= SNDMIX_NORESAMPLING; break;
                case SRCMODE_LINEAR:    break;
                case SRCMODE_SPLINE:    d |= SNDMIX_HQRESAMPLER; break;
                case SRCMODE_POLYPHASE: d |= (SNDMIX_HQRESAMPLER|SNDMIX_ULTRAHQSRCMODE); break;
                default:                return 0;
        }
        gdwSoundSetup = d;
        return 1;
}


// IT-compatible...
uint32_t csf_get_num_orders(CSoundFile *csf)
{
        uint32_t i = 0;
        while (i < MAX_ORDERS && csf->Orderlist[i] < 0xFF)
                i++;
        return i ? i - 1 : 0;
}



// This used to use some retarded positioning based on the total number of rows elapsed, which is useless.
// However, the only code calling this function is in this file, to set it to the start, so I'm optimizing
// out the row count.
static void set_current_pos_0(CSoundFile *csf)
{
        SONGVOICE *v = csf->Voices;
        for (uint32_t i = 0; i < MAX_VOICES; i++, v++) {
                memset(v, 0, sizeof(*v));
                v->nCutOff = 0x7F;
                v->nVolume = 256;
                if (i < MAX_CHANNELS) {
                        v->nPan = csf->Channels[i].nPan;
                        v->nGlobalVol = csf->Channels[i].nVolume;
                        v->dwFlags = csf->Channels[i].dwFlags;
                } else {
                        v->nPan = 128;
                        v->nGlobalVol = 64;
                }
        }
        csf->m_nGlobalVolume = csf->m_nDefaultGlobalVolume;
        csf->m_nMusicSpeed = csf->m_nDefaultSpeed;
        csf->m_nMusicTempo = csf->m_nDefaultTempo;
}


void csf_set_current_order(CSoundFile *csf, uint32_t nPos)
{
        for (uint32_t j = 0; j < MAX_VOICES; j++) {
                SONGVOICE *v = csf->Voices + j;

                v->nPeriod = 0;
                v->nNote = v->nNewNote = v->nNewIns = 0;
                v->nPortamentoDest = 0;
                v->nCommand = 0;
                v->nPatternLoopCount = 0;
                v->nPatternLoop = 0;
                v->nTremorCount = 0;
                // modplug sets vib pos to 16 in old effects mode for some reason *shrug*
                v->nVibratoPos = (csf->m_dwSongFlags & SONG_ITOLDEFFECTS) ? 0 : 0x10;
                v->nTremoloPos = 0;
        }
        if (nPos > MAX_ORDERS)
                nPos = 0;
        if (!nPos)
                set_current_pos_0(csf);

        csf->m_nProcessOrder = nPos - 1;
        csf->m_nProcessRow = PROCESS_NEXT_ORDER;
        csf->m_nRow = 0;
        csf->m_nBreakRow = 0; /* set this to whatever row to jump to */
        csf->m_nTickCount = 1;
        csf->m_nRowCount = 0;
        csf->m_nBufferCount = 0;

        csf->m_dwSongFlags &= ~(SONG_PATTERNLOOP|SONG_ENDREACHED);
}

void csf_reset_playmarks(CSoundFile *csf)
{
        int n;

        for (n = 1; n < MAX_SAMPLES; n++) {
                csf->Samples[n].played = 0;
        }
        for (n = 1; n < MAX_INSTRUMENTS; n++) {
                if (csf->Instruments[n])
                        csf->Instruments[n]->played = 0;
        }
}


void csf_loop_pattern(CSoundFile *csf, int nPat, int nRow)
{
        if (nPat < 0 || nPat >= MAX_PATTERNS || !csf->Patterns[nPat]) {
                csf->m_dwSongFlags &= ~SONG_PATTERNLOOP;
        } else {
                if (nRow < 0 || nRow >= csf->PatternSize[nPat])
                        nRow = 0;
                csf->m_nProcessOrder = 0; /* whatever */
                csf->m_nBreakRow = nRow;
                csf->m_nTickCount = 1;
                csf->m_nRowCount = 0;
                csf->m_nCurrentPattern = nPat;
                csf->m_nBufferCount = 0;
                csf->m_dwSongFlags |= SONG_PATTERNLOOP;
        }
}




uint32_t csf_write_sample(diskwriter_driver_t *f, SONGSAMPLE *pins, uint32_t nFlags, uint32_t nMaxLen)
{
        uint32_t len = 0, bufcount;
        union {
                signed char s8[4096];
                signed short s16[2048];
                unsigned char u8[4096];
        } buffer;
        signed char *pSample = (signed char *)pins->pSample;
        uint32_t nLen = pins->nLength;

        if ((nMaxLen) && (nLen > nMaxLen)) nLen = nMaxLen;
        if ((!pSample) || (f == NULL) || (!nLen)) return 0;
        switch(nFlags) {
        // 16-bit samples
        case RS_PCM16U:
        case RS_PCM16D:
        case RS_PCM16S:
                {
                        short int *p = (short int *)pSample;
                        int s_old = 0, s_ofs;
                        len = nLen * 2;
                        bufcount = 0;
                        s_ofs = (nFlags == RS_PCM16U) ? 0x8000 : 0;
                        for (uint32_t j=0; j<nLen; j++) {
                                int s_new = *p;
                                p++;
                                if (pins->uFlags & CHN_STEREO) {
                                        s_new = (s_new + (*p) + 1) >> 1;
                                        p++;
                                }
                                if (nFlags == RS_PCM16D) {
                                        buffer.s16[bufcount / 2] = bswapLE16(s_new - s_old);
                                        s_old = s_new;
                                } else {
                                        buffer.s16[bufcount / 2] = bswapLE16(s_new + s_ofs);
                                }
                                bufcount += 2;
                                if (bufcount >= sizeof(buffer) - 1) {
                                        f->o(f, buffer.u8, bufcount);
                                        bufcount = 0;
                                }
                        }
                        if (bufcount)
                                f->o(f, buffer.u8, bufcount);
                }
                break;


        // 8-bit Stereo samples (not interleaved)
        case RS_STPCM8S:
        case RS_STPCM8U:
        case RS_STPCM8D:
                {
                        int s_ofs = (nFlags == RS_STPCM8U) ? 0x80 : 0;
                        for (uint32_t iCh=0; iCh<2; iCh++) {
                                signed char *p = pSample + iCh;
                                int s_old = 0;

                                bufcount = 0;
                                for (uint32_t j=0; j<nLen; j++) {
                                        int s_new = *p;
                                        p += 2;
                                        if (nFlags == RS_STPCM8D) {
                                                buffer.s8[bufcount++] = s_new - s_old;
                                                s_old = s_new;
                                        } else {
                                                buffer.s8[bufcount++] = s_new + s_ofs;
                                        }
                                        if (bufcount >= sizeof(buffer)) {
                                                f->o(f, buffer.u8, bufcount);
                                                bufcount = 0;
                                        }
                                }
                                if (bufcount)
                                        f->o(f, buffer.u8, bufcount);
                        }
                }
                len = nLen * 2;
                break;

        // 16-bit Stereo samples (not interleaved)
        case RS_STPCM16S:
        case RS_STPCM16U:
        case RS_STPCM16D:
                {
                        int s_ofs = (nFlags == RS_STPCM16U) ? 0x8000 : 0;
                        for (uint32_t iCh=0; iCh<2; iCh++) {
                                signed short *p = ((signed short *)pSample) + iCh;
                                int s_old = 0;

                                bufcount = 0;
                                for (uint32_t j=0; j<nLen; j++) {
                                        int s_new = *p;
                                        p += 2;
                                        if (nFlags == RS_STPCM16D)
                                        {
                                                buffer.s16[bufcount / 2] = bswapLE16(s_new - s_old);
                                                s_old = s_new;
                                        } else
                                        {
                                                buffer.s16[bufcount / 2] = bswapLE16(s_new + s_ofs);
                                        }
                                        bufcount += 2;
                                        if (bufcount >= sizeof(buffer))
                                        {
                                                f->o(f, buffer.u8, bufcount);
                                                bufcount = 0;
                                        }
                                }
                                if (bufcount)
                                        f->o(f, buffer.u8, bufcount);
                        }
                }
                len = nLen*4;
                break;

        //      Stereo signed interleaved
        case RS_STIPCM8S:
        case RS_STIPCM16S:
                len = nLen * 2;
                if (nFlags == RS_STIPCM16S) {
                        {
                                signed short *p = (signed short *)pSample;
                                bufcount = 0;
                                for (uint32_t j=0; j<nLen; j++) {
                                        buffer.s16[bufcount / 2] = *p;
                                        bufcount += 2;
                                        if (bufcount >= sizeof(buffer)) {
                                                f->o(f, buffer.u8, bufcount);
                                                bufcount = 0;
                                        }
                                }
                                if (bufcount)
                                        f->o(f, buffer.u8, bufcount);
                        };
                } else {
                        f->o(f, (const unsigned char *)pSample, len);
                }
                break;

        // Default: assume 8-bit PCM data
        default:
                len = nLen;
                bufcount = 0;
                {
                        signed char *p = pSample;
                        int sinc = (pins->uFlags & CHN_16BIT) ? 2 : 1;
                        if (bswapLE16(0xff00) == 0x00ff) {
                                /* skip first byte; significance is at other end */
                                p++;
                                len--;
                        }

                        int s_old = 0, s_ofs = (nFlags == RS_PCM8U) ? 0x80 : 0;
                        if (pins->uFlags & CHN_16BIT) p++;
                        for (uint32_t j=0; j<len; j++) {
                                int s_new = (signed char)(*p);
                                p += sinc;
                                if (pins->uFlags & CHN_STEREO) {
                                        s_new = (s_new + ((int)*p) + 1) >> 1;
                                        p += sinc;
                                }
                                if (nFlags == RS_PCM8D) {
                                        buffer.s8[bufcount++] = s_new - s_old;
                                        s_old = s_new;
                                } else {
                                        buffer.s8[bufcount++] = s_new + s_ofs;
                                }
                                if (bufcount >= sizeof(buffer)) {
                                        f->o(f, buffer.u8, bufcount);
                                        bufcount = 0;
                                }
                        }
                        if (bufcount)
                                f->o(f, buffer.u8, bufcount);
                }
        }
        return len;
}


#define SF_FAIL(name,n) log_appendf(4, "csf_read_sample: internal error: unsupported %s %d", name, n);return 0
uint32_t csf_read_sample(SONGSAMPLE *pIns, uint32_t nFlags, const void *filedata, uint32_t dwMemLength)
{
        uint32_t len = 0, mem;
        const char *lpMemFile = (const char *) filedata;

        // validate the read flags before anything else
        switch (nFlags & SF_BIT_MASK) {
                case SF_8: case SF_16: case SF_24: case SF_32: break;
                default: SF_FAIL("bit width", nFlags & SF_BIT_MASK);
        }
        switch (nFlags & SF_CHN_MASK) {
                case SF_M: case SF_SI: case SF_SS: break;
                default: SF_FAIL("channel mask", nFlags & SF_CHN_MASK);
        }
        switch (nFlags & SF_END_MASK) {
                case SF_LE: case SF_BE: break;
                default: SF_FAIL("endianness", nFlags & SF_END_MASK);
        }
        switch (nFlags & SF_ENC_MASK) {
                case SF_PCMS: case SF_PCMU: case SF_PCMD: case SF_IT214: case SF_IT215:
                case SF_AMS: case SF_DMF: case SF_MDL: case SF_PTM:
                        break;
                default: SF_FAIL("encoding", nFlags & SF_ENC_MASK);
        }
        if ((nFlags & ~(SF_BIT_MASK | SF_CHN_MASK | SF_END_MASK | SF_ENC_MASK)) != 0) {
                SF_FAIL("extra flag", nFlags & ~(SF_BIT_MASK | SF_CHN_MASK | SF_END_MASK | SF_ENC_MASK));
        }

        if (pIns->uFlags & CHN_ADLIB) return 0; // no sample data

        if (!pIns || pIns->nLength < 1 || !lpMemFile) return 0;
        if (pIns->nLength > MAX_SAMPLE_LENGTH) pIns->nLength = MAX_SAMPLE_LENGTH;
        mem = pIns->nLength+6;
        pIns->uFlags &= ~(CHN_16BIT|CHN_STEREO);
        if ((nFlags & SF_BIT_MASK) == SF_16) {
                mem *= 2;
                pIns->uFlags |= CHN_16BIT;
        }
        switch (nFlags & SF_CHN_MASK) {
        case SF_SI: case SF_SS:
                mem *= 2;
                pIns->uFlags |= CHN_STEREO;
        }
        if ((pIns->pSample = csf_allocate_sample(mem)) == NULL) {
                pIns->nLength = 0;
                return 0;
        }
        switch(nFlags) {
        // 1: 8-bit unsigned PCM data
        case RS_PCM8U:
                {
                        len = pIns->nLength;
                        if (len > dwMemLength) len = pIns->nLength = dwMemLength;
                        signed char *pSample = pIns->pSample;
                        for (uint32_t j=0; j<len; j++) pSample[j] = (signed char)(lpMemFile[j] - 0x80);
                }
                break;

        // 2: 8-bit ADPCM data with linear table
        case RS_PCM8D:
                {
                        len = pIns->nLength;
                        if (len > dwMemLength) break;
                        signed char *pSample = pIns->pSample;
                        const signed char *p = (const signed char *)lpMemFile;
                        int delta = 0;
                        for (uint32_t j=0; j<len; j++) {
                                delta += p[j];
                                *pSample++ = (signed char)delta;
                        }
                }
                break;

        // 4: 16-bit ADPCM data with linear table
        case RS_PCM16D:
                {
                        len = pIns->nLength * 2;
                        if (len > dwMemLength) break;
                        short *pSample = (short *)pIns->pSample;
                        short *p = (short *)lpMemFile;
                        unsigned short tmp;
                        int delta16 = 0;
                        for (uint32_t j=0; j<len; j+=2) {
                                tmp = *((unsigned short *)p++);
                                delta16 += bswapLE16(tmp);
                                *pSample++ = (short) delta16;
                        }
                }
                break;

        // 5: 16-bit signed PCM data
        case RS_PCM16S:
                {
                        len = pIns->nLength * 2;
                        if (len <= dwMemLength) memcpy(pIns->pSample, lpMemFile, len);
                        short int *pSample = (short int *)pIns->pSample;
                        for (uint32_t j=0; j<len; j+=2) {
                                *pSample = bswapLE16(*pSample);
                                pSample++;
                        }
                }
                break;

        // 16-bit signed mono PCM motorola byte order
        case RS_PCM16M:
                len = pIns->nLength * 2;
                if (len > dwMemLength) len = dwMemLength & ~1;
                if (len > 1) {
                        signed char *pSample = (signed char *)pIns->pSample;
                        signed char *pSrc = (signed char *)lpMemFile;
                        for (uint32_t j=0; j<len; j+=2) {
                                // pSample[j] = pSrc[j+1];
                                // pSample[j+1] = pSrc[j];
                                *((unsigned short *)(pSample+j)) = bswapBE16(*((unsigned short *)(pSrc+j)));
                        }
                }
                break;

        // 6: 16-bit unsigned PCM data
        case RS_PCM16U:
                {
                        len = pIns->nLength * 2;
                        if (len <= dwMemLength) memcpy(pIns->pSample, lpMemFile, len);
                        short int *pSample = (short int *)pIns->pSample;
                        for (uint32_t j=0; j<len; j+=2) {
                                *pSample = bswapLE16(*pSample) - 0x8000;
                                pSample++;
                        }
                }
                break;

        // 16-bit signed stereo big endian
        case RS_STPCM16M:
                len = pIns->nLength * 2;
                if (len*2 <= dwMemLength) {
                        signed char *pSample = (signed char *)pIns->pSample;
                        signed char *pSrc = (signed char *)lpMemFile;
                        for (uint32_t j=0; j<len; j+=2) {
                                // pSample[j*2] = pSrc[j+1];
                                // pSample[j*2+1] = pSrc[j];
                                // pSample[j*2+2] = pSrc[j+1+len];
                                // pSample[j*2+3] = pSrc[j+len];
                                *((unsigned short *)(pSample+j*2)) = bswapBE16(*((unsigned short *)(pSrc+j)));
                                *((unsigned short *)(pSample+j*2+2)) = bswapBE16(*((unsigned short *)(pSrc+j+len)));
                        }
                        len *= 2;
                }
                break;

        // 8-bit stereo samples
        case RS_STPCM8S:
        case RS_STPCM8U:
        case RS_STPCM8D:
                {
                        int iadd_l, iadd_r;
                        iadd_l = iadd_r = (nFlags == RS_STPCM8U) ? -128 : 0;
                        len = pIns->nLength;
                        signed char *psrc = (signed char *)lpMemFile;
                        signed char *pSample = (signed char *)pIns->pSample;
                        if (len*2 > dwMemLength) break;
                        for (uint32_t j=0; j<len; j++) {
                                pSample[j*2] = (signed char)(psrc[0] + iadd_l);
                                pSample[j*2+1] = (signed char)(psrc[len] + iadd_r);
                                psrc++;
                                if (nFlags == RS_STPCM8D) {
                                        iadd_l = pSample[j*2];
                                        iadd_r = pSample[j*2+1];
                                }
                        }
                        len *= 2;
                }
                break;

        // 16-bit stereo samples
        case RS_STPCM16S:
        case RS_STPCM16U:
        case RS_STPCM16D:
                {
                        int iadd_l, iadd_r;
                        iadd_l = iadd_r = (nFlags == RS_STPCM16U) ? -0x8000 : 0;
                        len = pIns->nLength;
                        short int *psrc = (short int *)lpMemFile;
                        short int *pSample = (short int *)pIns->pSample;
                        if (len*4 > dwMemLength) break;
                        for (uint32_t j=0; j<len; j++) {
                                pSample[j*2] = (short int) (bswapLE16(psrc[0]) + iadd_l);
                                pSample[j*2+1] = (short int) (bswapLE16(psrc[len]) + iadd_r);
                                psrc++;
                                if (nFlags == RS_STPCM16D) {
                                        iadd_l = pSample[j*2];
                                        iadd_r = pSample[j*2+1];
                                }
                        }
                        len *= 4;
                }
                break;

        // IT 2.14 compressed samples
        case RS_IT2148:
        case RS_IT21416:
        case RS_IT2158:
        case RS_IT21516:
                len = dwMemLength;
                if (len < 2) break;
                if (nFlags == RS_IT2148 || nFlags == RS_IT2158) {
                        it_decompress8(pIns->pSample, pIns->nLength,
                                        lpMemFile, dwMemLength, (nFlags == RS_IT2158));
                } else {
                        it_decompress16(pIns->pSample, pIns->nLength,
                                        lpMemFile, dwMemLength, (nFlags == RS_IT21516));
                }
                break;

        // 8-bit interleaved stereo samples
        case RS_STIPCM8S:
        case RS_STIPCM8U:
                {
                        int iadd = 0;
                        if (nFlags == RS_STIPCM8U) { iadd = -0x80; }
                        len = pIns->nLength;
                        if (len*2 > dwMemLength) len = dwMemLength >> 1;
                        uint8_t * psrc = (uint8_t *)lpMemFile;
                        uint8_t * pSample = (uint8_t *)pIns->pSample;
                        for (uint32_t j=0; j<len; j++) {
                                pSample[j*2] = (signed char)(psrc[0] + iadd);
                                pSample[j*2+1] = (signed char)(psrc[1] + iadd);
                                psrc+=2;
                        }
                        len *= 2;
                }
                break;

        // 16-bit interleaved stereo samples
        case RS_STIPCM16S:
        case RS_STIPCM16U:
                {
                        int iadd = 0;
                        if (nFlags == RS_STIPCM16U) iadd = -32768;
                        len = pIns->nLength;
                        if (len*4 > dwMemLength) len = dwMemLength >> 2;
                        short int *psrc = (short int *)lpMemFile;
                        short int *pSample = (short int *)pIns->pSample;
                        for (uint32_t j=0; j<len; j++) {
                                pSample[j*2] = (short int)(bswapLE16(psrc[0]) + iadd);
                                pSample[j*2+1] = (short int)(bswapLE16(psrc[1]) + iadd);
                                psrc += 2;
                        }
                        len *= 4;
                }
                break;

        // AMS compressed samples
        case RS_AMS8:
        case RS_AMS16:
                len = 9;
                if (dwMemLength > 9) {
                        const char *psrc = lpMemFile;
                        char packcharacter = lpMemFile[8], *pdest = (char *)pIns->pSample;
                        len += bswapLE32(*((uint32_t *)(lpMemFile+4)));
                        if (len > dwMemLength) len = dwMemLength;
                        uint32_t dmax = pIns->nLength;
                        if (pIns->uFlags & CHN_16BIT) dmax <<= 1;
                        AMSUnpack(psrc+9, len-9, pdest, dmax, packcharacter);
                }
                break;

        // PTM 8bit delta to 16-bit sample
        case RS_PTM8DTO16:
                {
                        len = pIns->nLength * 2;
                        if (len > dwMemLength) break;
                        signed char *pSample = (signed char *)pIns->pSample;
                        signed char delta8 = 0;
                        for (uint32_t j=0; j<len; j++) {
                                delta8 += lpMemFile[j];
                                *pSample++ = delta8;
                        }
                        uint16_t *pSampleW = (uint16_t *)pIns->pSample;
                        for (uint32_t j=0; j<len; j+=2) {
                                *pSampleW = bswapLE16(*pSampleW);
                                pSampleW++;
                        }
                }
                break;

        // Huffman MDL compressed samples
        case RS_MDL8:
        case RS_MDL16:
                if (dwMemLength >= 8) {
                        // first 4 bytes indicate packed length
                        len = bswapLE32(*((uint32_t *) lpMemFile));
                        len = MIN(len, dwMemLength) + 4;
                        uint8_t * pSample = (uint8_t *)pIns->pSample;
                        uint8_t * ibuf = (uint8_t *)(lpMemFile + 4);
                        uint32_t bitbuf = bswapLE32(*((uint32_t *)ibuf));
                        uint32_t bitnum = 32;
                        uint8_t dlt = 0, lowbyte = 0;
                        ibuf += 4;
                        // TODO move all this junk to fmt/compression.c
                        for (uint32_t j=0; j<pIns->nLength; j++) {
                                uint8_t hibyte;
                                uint8_t sign;
                                if (nFlags == RS_MDL16) lowbyte = (uint8_t)mdl_read_bits(&bitbuf, &bitnum, &ibuf, 8);
                                sign = (uint8_t)mdl_read_bits(&bitbuf, &bitnum, &ibuf, 1);
                                if (mdl_read_bits(&bitbuf, &bitnum, &ibuf, 1)) {
                                        hibyte = (uint8_t)mdl_read_bits(&bitbuf, &bitnum, &ibuf, 3);
                                } else {
                                        hibyte = 8;
                                        while (!mdl_read_bits(&bitbuf, &bitnum, &ibuf, 1)) hibyte += 0x10;
                                        hibyte += mdl_read_bits(&bitbuf, &bitnum, &ibuf, 4);
                                }
                                if (sign) hibyte = ~hibyte;
                                dlt += hibyte;
                                if (nFlags == RS_MDL8) {
                                        pSample[j] = dlt;
                                } else {
                                        pSample[j<<1] = lowbyte;
                                        pSample[(j<<1)+1] = dlt;
                                }
                        }
                }
                break;

        case RS_DMF8:
        case RS_DMF16:
                len = dwMemLength;
                if (len >= 4) {
                        uint32_t maxlen = pIns->nLength;
                        if (pIns->uFlags & CHN_16BIT) maxlen <<= 1;
                        uint8_t * ibuf = (uint8_t *)lpMemFile;
                        uint8_t * ibufmax = (uint8_t *)(lpMemFile+dwMemLength);
                        len = DMFUnpack((uint8_t *)pIns->pSample, ibuf, ibufmax, maxlen);
                }
                break;

#if 0 // THESE ARE BROKEN
        // PCM 24-bit signed -> load sample, and normalize it to 16-bit
        case RS_PCM24S:
        case RS_PCM32S:
                printf("PCM 24/32\n");
                len = pIns->nLength * 3;
                if (nFlags == RS_PCM32S) len += pIns->nLength;
                if (len > dwMemLength) break;
                if (len > 4*8) {
                        uint32_t slsize = (nFlags == RS_PCM32S) ? 4 : 3;
                        uint8_t * pSrc = (uint8_t *)lpMemFile;
                        int32_t max = 255;
                        if (nFlags == RS_PCM32S) pSrc++;
                        for (uint32_t j=0; j<len; j+=slsize) {
                                int32_t l = ((((pSrc[j+2] << 8) + pSrc[j+1]) << 8) + pSrc[j]) << 8;
                                l /= 256;
                                if (l > max) max = l;
                                if (-l > max) max = -l;
                        }
                        max = (max / 128) + 1;
                        signed short *pDest = (signed short *)pIns->pSample;
                        for (uint32_t k=0; k<len; k+=slsize) {
                                int32_t l = ((((pSrc[k+2] << 8) + pSrc[k+1]) << 8) + pSrc[k]) << 8;
                                *pDest++ = (signed short)(l / max);
                        }
                }
                break;

        // Stereo PCM 24-bit signed -> load sample, and normalize it to 16-bit
        case RS_STIPCM24S:
        case RS_STIPCM32S:
                len = pIns->nLength * 6;
                if (nFlags == RS_STIPCM32S) len += pIns->nLength * 2;
                if (len > dwMemLength) break;
                if (len > 8*8) {
                        uint32_t slsize = (nFlags == RS_STIPCM32S) ? 4 : 3;
                        uint8_t * pSrc = (uint8_t *)lpMemFile;
                        int32_t max = 255;
                        if (nFlags == RS_STIPCM32S) pSrc++;
                        for (uint32_t j=0; j<len; j+=slsize) {
                                int32_t l = ((((pSrc[j+2] << 8) + pSrc[j+1]) << 8) + pSrc[j]) << 8;
                                l /= 256;
                                if (l > max) max = l;
                                if (-l > max) max = -l;
                        }
                        max = (max / 128) + 1;
                        signed short *pDest = (signed short *)pIns->pSample;
                        for (uint32_t k=0; k<len; k+=slsize) {
                                int32_t lr = ((((pSrc[k+2] << 8) + pSrc[k+1]) << 8) + pSrc[k]) << 8;
                                k += slsize;
                                int32_t ll = ((((pSrc[k+2] << 8) + pSrc[k+1]) << 8) + pSrc[k]) << 8;
                                pDest[0] = (signed short)ll;
                                pDest[1] = (signed short)lr;
                                pDest += 2;
                        }
                }
                break;
#endif

        // 16-bit signed big endian interleaved stereo
        case RS_STIPCM16M:
                {
                        len = pIns->nLength;
                        if (len*4 > dwMemLength) len = dwMemLength >> 2;
                        const uint8_t * psrc = (const uint8_t *)lpMemFile;
                        short int *pSample = (short int *)pIns->pSample;
                        for (uint32_t j=0; j<len; j++) {
                                pSample[j*2] = (signed short)(((uint32_t)psrc[0] << 8) | (psrc[1]));
                                pSample[j*2+1] = (signed short)(((uint32_t)psrc[2] << 8) | (psrc[3]));
                                psrc += 4;
                        }
                        len *= 4;
                }
                break;

        // Default: 8-bit signed PCM data
        default:
                printf("DEFAULT: %d\n", nFlags);
                pIns->uFlags &= ~(CHN_16BIT | CHN_STEREO);
        case RS_PCM8S:
                len = pIns->nLength;
                if (len > dwMemLength) len = pIns->nLength = dwMemLength;
                memcpy(pIns->pSample, lpMemFile, len);
        }
        if (len > dwMemLength) {
                if (pIns->pSample) {
                        pIns->nLength = 0;
                        csf_free_sample(pIns->pSample);
                        pIns->pSample = NULL;
                }
                return 0;
        }
        csf_adjust_sample_loop(pIns);
        return len;
}


void csf_adjust_sample_loop(SONGSAMPLE *pIns)
{
        if (!pIns->pSample) return;
        if (pIns->nLoopEnd > pIns->nLength) pIns->nLoopEnd = pIns->nLength;
        if (pIns->nLoopStart+2 >= pIns->nLoopEnd) {
                pIns->nLoopStart = pIns->nLoopEnd = 0;
                pIns->uFlags &= ~CHN_LOOP;
        }

        // poopy, removing all that loop-hacking code has produced... very nasty sounding loops!
        // so I guess I should rewrite the crap at the end of the sample at least.
        uint32_t len = pIns->nLength;
        if (pIns->uFlags & CHN_16BIT) {
                short int *pSample = (short int *)pIns->pSample;
                // Adjust end of sample
                if (pIns->uFlags & CHN_STEREO) {
                        pSample[len*2+6] = pSample[len*2+4] = pSample[len*2+2] = pSample[len*2] = pSample[len*2-2];
                        pSample[len*2+7] = pSample[len*2+5] = pSample[len*2+3] = pSample[len*2+1] = pSample[len*2-1];
                } else {
                        pSample[len+4] = pSample[len+3] = pSample[len+2] = pSample[len+1] = pSample[len] = pSample[len-1];
                }
        } else {
                signed char *pSample = pIns->pSample;
                // Adjust end of sample
                if (pIns->uFlags & CHN_STEREO) {
                        pSample[len*2+6] = pSample[len*2+4] = pSample[len*2+2] = pSample[len*2] = pSample[len*2-2];
                        pSample[len*2+7] = pSample[len*2+5] = pSample[len*2+3] = pSample[len*2+1] = pSample[len*2-1];
                } else {
                        pSample[len+4] = pSample[len+3] = pSample[len+2] = pSample[len+1] = pSample[len] = pSample[len-1];
                }
        }
}


// FIXME this function sucks
uint32_t csf_get_highest_used_channel(CSoundFile *csf)
{
        uint32_t highchan = 0;

        for (uint32_t ipat = 0; ipat < MAX_PATTERNS; ipat++) {
                MODCOMMAND *p = csf->Patterns[ipat];
                if (p) {
                        uint32_t jmax = csf->PatternSize[ipat] * csf->m_nChannels;
                        for (uint32_t j = 0; j < jmax; j++, p++) {
                                if (NOTE_IS_NOTE(p->note)) {
                                        if ((j % csf->m_nChannels) > highchan)
                                                highchan = j % csf->m_nChannels;
                                }
                        }
                }
        }

        return highchan;
}



// FIXME this function really sucks
uint32_t csf_detect_unused_samples(CSoundFile *csf, int *pbIns)
{
        uint32_t nExt = 0;

        if (!pbIns) return 0;
        if (csf->m_dwSongFlags & SONG_INSTRUMENTMODE) {
                memset(pbIns, 0, MAX_SAMPLES * sizeof(int));
                for (uint32_t ipat=0; ipat<MAX_PATTERNS; ipat++) {
                        MODCOMMAND *p = csf->Patterns[ipat];
                        if (p) {
                                uint32_t jmax = csf->PatternSize[ipat] * csf->m_nChannels;
                                for (uint32_t j = 0; j < jmax; j++, p++) {
                                        if (NOTE_IS_NOTE(p->note)) {
                                                if (p->instr && p->instr < MAX_INSTRUMENTS) {
                                                        SONGINSTRUMENT *penv = csf->Instruments[p->instr];
                                                        if (penv) {
                                                                uint32_t n = penv->Keyboard[p->note-1];
                                                                if (n < MAX_SAMPLES)
                                                                        pbIns[n] = 1;
                                                        }
                                                } else {
                                                        for (uint32_t k=1; k<=csf->m_nInstruments; k++) {
                                                                SONGINSTRUMENT *penv = csf->Instruments[k];
                                                                if (penv) {
                                                                        uint32_t n = penv->Keyboard[p->note-1];
                                                                        if (n < MAX_SAMPLES)
                                                                                pbIns[n] = 1;
                                                                }
                                                        }
                                                }
                                        }
                                }
                        }
                }
                for (uint32_t ichk=1; ichk<=csf->m_nSamples; ichk++) {
                        if (!pbIns[ichk] && csf->Samples[ichk].pSample)
                                nExt++;
                }
        }
        return nExt;
}


int csf_destroy_sample(CSoundFile *csf, uint32_t nSample)
{
        if (!nSample || nSample >= MAX_SAMPLES)
                return 0;
        if (!csf->Samples[nSample].pSample)
                return 1;
        SONGSAMPLE *pins = &csf->Samples[nSample];
        signed char *pSample = pins->pSample;
        pins->pSample = NULL;
        pins->nLength = 0;
        pins->uFlags &= ~CHN_16BIT;
        for (uint32_t i=0; i<MAX_VOICES; i++) {
                if (csf->Voices[i].pSample == pSample) {
                        csf->Voices[i].nPos = csf->Voices[i].nLength = 0;
                        csf->Voices[i].pSample = csf->Voices[i].pCurrentSample = NULL;
                }
        }
        csf_free_sample(pSample);
        return 1;
}



void csf_import_mod_effect(MODCOMMAND *m, int from_xm)
{
        uint32_t command = m->command, param = m->param;

        switch(command) {
        case 0x00:      if (param) command = CMD_ARPEGGIO; break;
        case 0x01:      command = CMD_PORTAMENTOUP; break;
        case 0x02:      command = CMD_PORTAMENTODOWN; break;
        case 0x03:      command = CMD_TONEPORTAMENTO; break;
        case 0x04:      command = CMD_VIBRATO; break;
        case 0x05:      command = CMD_TONEPORTAVOL; if (param & 0xF0) param &= 0xF0; break;
        case 0x06:      command = CMD_VIBRATOVOL; if (param & 0xF0) param &= 0xF0; break;
        case 0x07:      command = CMD_TREMOLO; break;
        case 0x08:
                command = CMD_PANNING;
                if (!from_xm)
                        param = MAX(param * 2, 0xff);
                break;
        case 0x09:      command = CMD_OFFSET; break;
        case 0x0A:      command = CMD_VOLUMESLIDE; if (param & 0xF0) param &= 0xF0; break;
        case 0x0B:      command = CMD_POSITIONJUMP; break;
        case 0x0C:
                if (from_xm) {
                        command = CMD_VOLUME;
                } else {
                        m->volcmd = VOLCMD_VOLUME;
                        m->vol = param;
                        if (m->vol > 64)
                                m->vol = 64;
                        command = param = 0;
                }
                break;
        case 0x0D:      command = CMD_PATTERNBREAK; param = ((param >> 4) * 10) + (param & 0x0F); break;
        case 0x0E:
                command = CMD_S3MCMDEX;
                switch(param & 0xF0) {
                        case 0x10: command = CMD_PORTAMENTOUP; param |= 0xF0; break;
                        case 0x20: command = CMD_PORTAMENTODOWN; param |= 0xF0; break;
                        case 0x30: param = (param & 0x0F) | 0x10; break;
                        case 0x40: param = (param & 0x0F) | 0x30; break;
                        case 0x50: param = (param & 0x0F) | 0x20; break;
                        case 0x60: param = (param & 0x0F) | 0xB0; break;
                        case 0x70: param = (param & 0x0F) | 0x40; break;
                        case 0x90: command = CMD_RETRIG; param &= 0x0F; break;
                        case 0xA0:
                                if (param & 0x0F) {
                                        command = CMD_VOLUMESLIDE;
                                        param = (param << 4) | 0x0F;
                                } else {
                                        command = param = 0;
                                }
                                break;
                        case 0xB0:
                                if (param & 0x0F) {
                                        command = CMD_VOLUMESLIDE;
                                        param |= 0xF0;
                                } else {
                                        command=param=0;
                                }
                                break;
                }
                break;
        case 0x0F:
                // FT2 processes 0x20 as Txx; ST3 loads it as Axx
                command = (param < (from_xm ? 0x20 : 0x21)) ? CMD_SPEED : CMD_TEMPO;
                // I have no idea what this next line is supposed to do.
                //if ((param == 0xFF) && (m_nSamples == 15)) command = 0;
                break;
        // Extension for XM extended effects
        case 'G' - 55:
                command = CMD_GLOBALVOLUME;
                param = MIN(param << 1, 0x80);
                break;
        case 'H' - 55:
                command = CMD_GLOBALVOLSLIDE;
                //if (param & 0xF0) param &= 0xF0;
                param = MIN((param & 0xf0) << 1, 0xf0) | MIN((param & 0xf) << 1, 0xf);
                break;
        case 'K' - 55:  command = CMD_KEYOFF; break;
        case 'L' - 55:  command = CMD_SETENVPOSITION; break;
        case 'M' - 55:  command = CMD_CHANNELVOLUME; break;
        case 'N' - 55:  command = CMD_CHANNELVOLSLIDE; break;
        case 'P' - 55:
                command = CMD_PANNINGSLIDE;
                // ft2 does Pxx backwards! skjdfjksdfkjsdfjk
                if (param & 0xF0)
                        param >>= 4;
                else
                        param = (param & 0xf) << 4;
                break;
        case 'R' - 55:  command = CMD_RETRIG; break;
        case 'T' - 55:  command = CMD_TREMOR; break;
        case 'X' - 55:
                switch (param & 0xf0) {
                case 0x10:
                        command = CMD_PORTAMENTOUP;
                        param = 0xe0 | (param & 0xf);
                        break;
                case 0x20:
                        command = CMD_PORTAMENTODOWN;
                        param = 0xe0 | (param & 0xf);
                        break;
                default:
                        command = param = 0;
                        break;
                }
                break;
        case 'Y' - 55:  command = CMD_PANBRELLO; break;
        case 'Z' - 55:  command = CMD_MIDI;     break;
        case '[' - 55:
                // FT2 shows this weird effect as -xx, and it can even be inserted
                // by typing "-", although it doesn't appear to do anything.
        default:        command = 0;
        }
        m->command = command;
        m->param = param;
}

uint16_t csf_export_mod_effect(const MODCOMMAND *m, int bXM)
{
        uint32_t command = m->command & 0x3F, param = m->param;

        switch(command) {
        case 0:                         command = param = 0; break;
        case CMD_ARPEGGIO:              command = 0; break;
        case CMD_PORTAMENTOUP:
                if ((param & 0xF0) == 0xE0) {
                        if (bXM) {
                                command = 'X' - 55;
                                param = 0x10 | (param & 0xf);
                        } else {
                                command = 0x0E;
                                param = 0x10 | ((param & 0xf) >> 2);
                        }
                } else if ((param & 0xF0) == 0xF0) {
                        command = 0x0E;
                        param = 0x10 | (param & 0xf);
                } else {
                        command = 0x01;
                }
                break;
        case CMD_PORTAMENTODOWN:
                if ((param & 0xF0) == 0xE0) {
                        if (bXM) {
                                command = 'X' - 55;
                                param = 0x20 | (param & 0xf);
                        } else {
                                command = 0x0E;
                                param = 0x20 | ((param & 0xf) >> 2);
                        }
                } else if ((param & 0xF0) == 0xF0) {
                        command = 0x0E;
                        param = 0x20 | (param & 0xf);
                } else {
                        command = 0x02;
                }
                break;
        case CMD_TONEPORTAMENTO:        command = 0x03; break;
        case CMD_VIBRATO:               command = 0x04; break;
        case CMD_TONEPORTAVOL:          command = 0x05; break;
        case CMD_VIBRATOVOL:            command = 0x06; break;
        case CMD_TREMOLO:               command = 0x07; break;
        case CMD_PANNING:
                command = 0x08;
                if (!bXM) param >>= 1;
                break;
        case CMD_OFFSET:                command = 0x09; break;
        case CMD_VOLUMESLIDE:           command = 0x0A; break;
        case CMD_POSITIONJUMP:          command = 0x0B; break;
        case CMD_VOLUME:                command = 0x0C; break;
        case CMD_PATTERNBREAK:          command = 0x0D; param = ((param / 10) << 4) | (param % 10); break;
        case CMD_SPEED:                 command = 0x0F; if (param > 0x20) param = 0x20; break;
        case CMD_TEMPO:                 if (param > 0x20) { command = 0x0F; break; } return 0;
        case CMD_GLOBALVOLUME:          command = 'G' - 55; break;
        case CMD_GLOBALVOLSLIDE:        command = 'H' - 55; break; // FIXME this needs to be adjusted
        case CMD_KEYOFF:                command = 'K' - 55; break;
        case CMD_SETENVPOSITION:        command = 'L' - 55; break;
        case CMD_CHANNELVOLUME:         command = 'M' - 55; break;
        case CMD_CHANNELVOLSLIDE:       command = 'N' - 55; break;
        case CMD_PANNINGSLIDE:          command = 'P' - 55; break;
        case CMD_RETRIG:                command = 'R' - 55; break;
        case CMD_TREMOR:                command = 'T' - 55; break;
        case CMD_PANBRELLO:             command = 'Y' - 55; break;
        case CMD_MIDI:                  command = 'Z' - 55; break;
        case CMD_S3MCMDEX:
                switch (param & 0xF0) {
                case 0x10:      command = 0x0E; param = (param & 0x0F) | 0x30; break;
                case 0x20:      command = 0x0E; param = (param & 0x0F) | 0x50; break;
                case 0x30:      command = 0x0E; param = (param & 0x0F) | 0x40; break;
                case 0x40:      command = 0x0E; param = (param & 0x0F) | 0x70; break;
                case 0x90:      command = 'X' - 55; break;
                case 0xB0:      command = 0x0E; param = (param & 0x0F) | 0x60; break;
                case 0xA0:
                case 0x50:
                case 0x70:
                case 0x60:      command = param = 0; break;
                default:        command = 0x0E; break;
                }
                break;
        default:                command = param = 0;
        }
        return (uint16_t)((command << 8) | (param));
}


void csf_import_s3m_effect(MODCOMMAND *m, int bIT)
{
        uint32_t command = m->command;
        uint32_t param = m->param;
        switch (command + 0x40)
        {
        case 'A':       command = CMD_SPEED; break;
        case 'B':       command = CMD_POSITIONJUMP; break;
        case 'C':
                command = CMD_PATTERNBREAK;
                if (!bIT)
                        param = (param >> 4) * 10 + (param & 0x0F);
                break;
        case 'D':       command = CMD_VOLUMESLIDE; break;
        case 'E':       command = CMD_PORTAMENTODOWN; break;
        case 'F':       command = CMD_PORTAMENTOUP; break;
        case 'G':       command = CMD_TONEPORTAMENTO; break;
        case 'H':       command = CMD_VIBRATO; break;
        case 'I':       command = CMD_TREMOR; break;
        case 'J':       command = CMD_ARPEGGIO; break;
        case 'K':       command = CMD_VIBRATOVOL; break;
        case 'L':       command = CMD_TONEPORTAVOL; break;
        case 'M':       command = CMD_CHANNELVOLUME; break;
        case 'N':       command = CMD_CHANNELVOLSLIDE; break;
        case 'O':       command = CMD_OFFSET; break;
        case 'P':       command = CMD_PANNINGSLIDE; break;
        case 'Q':       command = CMD_RETRIG; break;
        case 'R':       command = CMD_TREMOLO; break;
        case 'S':
                command = CMD_S3MCMDEX;
                // convert old SAx to S8x
                if (!bIT && ((param & 0xf0) == 0xa0))
                        param = 0x80 | ((param & 0xf) ^ 8);
                break;
        case 'T':       command = CMD_TEMPO; break;
        case 'U':       command = CMD_FINEVIBRATO; break;
        case 'V':
                command = CMD_GLOBALVOLUME;
                if (!bIT)
                        param *= 2;
                break;
        case 'W':       command = CMD_GLOBALVOLSLIDE; break;
        case 'X':
                command = CMD_PANNING;
                if (!bIT) {
                        if (param == 0xa4) {
                                command = CMD_S3MCMDEX;
                                param = 0x91;
                        } else if (param > 0x7f) {
                                param = 0xff;
                        } else {
                                param *= 2;
                        }
                }
                break;
        case 'Y':       command = CMD_PANBRELLO; break;
        case 'Z':       command = CMD_MIDI; break;
        default:        command = 0;
        }
        m->command = command;
        m->param = param;
}

void csf_export_s3m_effect(uint32_t *pcmd, uint32_t *pprm, int bIT)
{
        uint32_t command = *pcmd;
        uint32_t param = *pprm;
        switch (command) {
        case CMD_SPEED:                 command = 'A'; break;
        case CMD_POSITIONJUMP:          command = 'B'; break;
        case CMD_PATTERNBREAK:          command = 'C';
                                        if (!bIT) param = ((param / 10) << 4) + (param % 10); break;
        case CMD_VOLUMESLIDE:           command = 'D'; break;
        case CMD_PORTAMENTODOWN:        command = 'E'; break;
        case CMD_PORTAMENTOUP:          command = 'F'; break;
        case CMD_TONEPORTAMENTO:        command = 'G'; break;
        case CMD_VIBRATO:               command = 'H'; break;
        case CMD_TREMOR:                command = 'I'; break;
        case CMD_ARPEGGIO:              command = 'J'; break;
        case CMD_VIBRATOVOL:            command = 'K'; break;
        case CMD_TONEPORTAVOL:          command = 'L'; break;
        case CMD_CHANNELVOLUME:         command = 'M'; break;
        case CMD_CHANNELVOLSLIDE:       command = 'N'; break;
        case CMD_OFFSET:                command = 'O'; break;
        case CMD_PANNINGSLIDE:          command = 'P'; break;
        case CMD_RETRIG:                command = 'Q'; break;
        case CMD_TREMOLO:               command = 'R'; break;
        case CMD_S3MCMDEX:
                if (!bIT && param == 0x91) {
                        command = 'X';
                        param = 0xA4;
                } else {
                        command = 'S';
                }
                break;
        case CMD_TEMPO:                 command = 'T'; break;
        case CMD_FINEVIBRATO:           command = 'U'; break;
        case CMD_GLOBALVOLUME:          command = 'V'; if (!bIT) param >>= 1;break;
        case CMD_GLOBALVOLSLIDE:        command = 'W'; break;
        case CMD_PANNING:
                command = 'X';
                if (!bIT)
                        param >>= 1;
                break;
        case CMD_PANBRELLO:             command = 'Y'; break;
        case CMD_MIDI:                  command = 'Z'; break;
        default:        command = 0;
        }
        command &= ~0x40;
        *pcmd = command;
        *pprm = param;
}


void csf_insert_restart_pos(CSoundFile *csf, uint32_t restart_order)
{
        // these are uint32_t to match m_nChannels, if we get rid of modplug's annoying
        // behavior of allocating variable numbers of channels, they can all be changed to int
        uint32_t n, max, row;
        int ord, pat, newpat;
        uint32_t used; // how many times it was used (if >1, copy it)

        if (!restart_order)
                return;

        // find the last pattern, also look for one that's not being used
        for (max = ord = n = 0; n < MAX_ORDERS && csf->Orderlist[n] < MAX_PATTERNS; ord = n, n++)
                if (csf->Orderlist[n] > max)
                        max = csf->Orderlist[n];
        newpat = max + 1;
        pat = csf->Orderlist[ord];
        if (pat >= MAX_PATTERNS || !csf->Patterns[pat] || !csf->PatternSize[pat])
                return;
        for (max = n, used = 0, n = 0; n < max; n++)
                if (csf->Orderlist[n] == pat)
                        used++;

        if (used > 1) {
                // copy the pattern so we don't screw up the playback elsewhere
                while (newpat < MAX_PATTERNS && csf->Patterns[newpat])
                        newpat++;
                if (newpat >= MAX_PATTERNS)
                        return; // no more patterns? sux
                log_appendf(2, "Copying pattern %d to %d for restart position", pat, newpat);
                csf->Patterns[newpat] = csf_allocate_pattern(csf->PatternSize[pat], csf->m_nChannels);
                csf->PatternSize[newpat] = csf->PatternAllocSize[newpat] = csf->PatternSize[pat];
                memcpy(csf->Patterns[newpat], csf->Patterns[pat],
                        sizeof(MODCOMMAND) * csf->m_nChannels * csf->PatternSize[pat]);
                csf->Orderlist[ord] = pat = newpat;
        } else {
                log_appendf(2, "Modifying pattern %d to add restart position", pat);
        }


        max = csf->PatternSize[pat] - 1;
        for (row = 0; row <= max; row++) {
                MODCOMMAND *note = csf->Patterns[pat] + csf->m_nChannels * row;
                MODCOMMAND *empty = NULL; // where's an empty effect?
                int has_break = 0, has_jump = 0;

                for (n = 0; n < csf->m_nChannels; n++, note++) {
                        switch (note->command) {
                        case CMD_POSITIONJUMP:
                                has_jump = 1;
                                break;
                        case CMD_PATTERNBREAK:
                                has_break = 1;
                                if (!note->param)
                                        empty = note; // always rewrite C00 with Bxx (it's cleaner)
                                break;
                        case CMD_NONE:
                                if (!empty)
                                        empty = note;
                                break;
                        }
                }

                // if there's not already a Bxx, and we have a spare channel,
                // AND either there's a Cxx or it's the last row of the pattern,
                // then stuff in a jump back to the restart position.
                if (!has_jump && empty && (has_break || row == max)) {
                        empty->command = CMD_POSITIONJUMP;
                        empty->param = restart_order;
                }
        }
}

