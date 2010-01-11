#include "sndfile.h"
#include "diskwriter.h"
#include "util.h"

#include <stdint.h>


bool CSoundFile::Create(const uint8_t * lpStream, uint32_t dwMemLength)
{
        int i;

        csf_destroy(this);
        m_nType = MOD_TYPE_NONE;
        if (lpStream) {
                if (1
                 && (!ReadMed(lpStream, dwMemLength))
                 && (!ReadMDL(lpStream, dwMemLength))
                 && (!ReadDBM(lpStream, dwMemLength))
                 && (!ReadFAR(lpStream, dwMemLength))
                 && (!ReadAMS(lpStream, dwMemLength))
                 && (!ReadOKT(lpStream, dwMemLength))
                 && (!ReadPTM(lpStream, dwMemLength))
                 && (!ReadUlt(lpStream, dwMemLength))
                 && (!ReadDMF(lpStream, dwMemLength))
                 && (!ReadDSM(lpStream, dwMemLength))
                 && (!ReadUMX(lpStream, dwMemLength))
                 && (!ReadAMF(lpStream, dwMemLength))
                 && (!ReadPSM(lpStream, dwMemLength))
                 && (!ReadMT2(lpStream, dwMemLength))
                 && (!ReadMID(lpStream, dwMemLength))
                 && (!ReadMod(lpStream, dwMemLength))) m_nType = MOD_TYPE_NONE;
        }
        // Adjust channels
        for (i=0; i<MAX_CHANNELS; i++) {
                if (Channels[i].nVolume > 64) Channels[i].nVolume = 64;
                if (Channels[i].nPan > 256) Channels[i].nPan = 128;
                Voices[i].nPan = Channels[i].nPan;
                Voices[i].nGlobalVol = Channels[i].nVolume;
                Voices[i].dwFlags = Channels[i].dwFlags;
                Voices[i].nVolume = 256;
                Voices[i].nCutOff = 0x7F;
        }
        // Checking instruments
        SONGSAMPLE *pins = Samples;

        for (i=0; i<MAX_INSTRUMENTS; i++, pins++)
        {
                if (pins->pSample)
                {
                        if (pins->nLoopEnd > pins->nLength) pins->nLoopEnd = pins->nLength;
                        if (pins->nSustainEnd > pins->nLength) pins->nSustainEnd = pins->nLength;
                } else {
                        pins->nLength = 0;
                        pins->nLoopStart = 0;
                        pins->nLoopEnd = 0;
                        pins->nSustainStart = 0;
                        pins->nSustainEnd = 0;
                }
                if (!pins->nLoopEnd) pins->uFlags &= ~CHN_LOOP;
                if (!pins->nSustainEnd) pins->uFlags &= ~CHN_SUSTAINLOOP;
                if (pins->nGlobalVol > 64) pins->nGlobalVol = 64;
        }
        // Check invalid instruments
        while ((m_nInstruments > 0) && (!Instruments[m_nInstruments])) m_nInstruments--;
        // Set default values
        if (m_nDefaultTempo < 31) m_nDefaultTempo = 31;
        if (!m_nDefaultSpeed) m_nDefaultSpeed = 6;

        m_nMusicSpeed = m_nDefaultSpeed;
        m_nMusicTempo = m_nDefaultTempo;
        m_nGlobalVolume = m_nDefaultGlobalVolume;
        m_nProcessOrder = -1;
        m_nCurrentOrder = 0;
        m_nCurrentPattern = 0;
        m_nBufferCount = 0;
        m_nTickCount = 1;
        m_nRowCount = 1;
        m_nRow = 0;
        m_nProcessRow = 0xfffe;

        for (unsigned int n = 1; n <= this->m_nInstruments; n++) {
                SONGINSTRUMENT *ins = this->Instruments[n];
                if (!ins)
                        continue;
                if (ins->VolEnv.nNodes < 1) {
                        ins->VolEnv.Ticks[0] = 0;
                        ins->VolEnv.Values[0] = 64;
                }
                if (ins->VolEnv.nNodes < 2) {
                        ins->VolEnv.nNodes = 2;
                        ins->VolEnv.Ticks[1] = 100;
                        ins->VolEnv.Values[1] = ins->VolEnv.Values[0];
                }
                if (ins->PanEnv.nNodes < 1) {
                        ins->PanEnv.Ticks[0] = 0;
                        ins->PanEnv.Values[0] = 32;
                }
                if (ins->PanEnv.nNodes < 2) {
                        ins->PanEnv.nNodes = 2;
                        ins->PanEnv.Ticks[1] = 100;
                        ins->PanEnv.Values[1] = ins->PanEnv.Values[0];
                }
                if (ins->PitchEnv.nNodes < 1) {
                        ins->PitchEnv.Ticks[0] = 0;
                        ins->PitchEnv.Values[0] = 32;
                }
                if (ins->PitchEnv.nNodes < 2) {
                        ins->PitchEnv.nNodes = 2;
                        ins->PitchEnv.Ticks[1] = 100;
                        ins->PitchEnv.Values[1] = ins->PitchEnv.Values[0];
                }
                for (int p = 0; p < 128; p++) {
                        if (!NOTE_IS_NOTE(ins->NoteMap[p]))
                                ins->NoteMap[p] = p + 1;
                }
        }

        switch (this->m_nType) {
        default:
                this->m_dwSongFlags |= SONG_COMPATGXX | SONG_ITOLDEFFECTS;
                this->m_nType = MOD_TYPE_IT;
                /* fall through */
        case MOD_TYPE_IT:
                return true;
        case 0:
                return false;
        }
}

int csf_load(CSoundFile *csf, const uint8_t * lpStream, uint32_t dwMemLength)
{
        return csf->Create(lpStream, dwMemLength);
}


/* stupid c++ */

int csf_save_xm(CSoundFile *csf, diskwriter_driver_t *f, UNUSED uint32_t z)
{
        return csf->SaveXM(f, z);
}

int csf_save_s3m(CSoundFile *csf, diskwriter_driver_t *f, UNUSED uint32_t z)
{
        return csf->SaveS3M(f, z);
}

int csf_save_mod(CSoundFile *csf, diskwriter_driver_t *f, UNUSED uint32_t z)
{
        return csf->SaveMod(f, z);
}

