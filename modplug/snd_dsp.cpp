/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
*/

#include "sndfile.h"



void (*csf_midi_out_note)(int chan, const MODCOMMAND *m) = NULL;
void (*csf_midi_out_raw)(const unsigned char *,unsigned int, unsigned int) = NULL;

////////////////////////////////////////////////////////////////////
// DSP Effects internal state

// Noise Reduction: simple low-pass filter
static int32_t nLeftNR = 0;
static int32_t nRightNR = 0;

// Access the main temporary mix buffer directly: avoids an extra pointer
extern int MixSoundBuffer[MIXBUFFERSIZE*2];


void csf_initialize_dsp(CSoundFile *, int reset)
{
        if (reset) {
                // Noise Reduction
                nLeftNR = nRightNR = 0;
        }
}


void csf_process_stereo_dsp(CSoundFile *, int count)
{
        // Noise Reduction
        if (gdwSoundSetup & SNDMIX_NOISEREDUCTION) {
                int n1 = nLeftNR, n2 = nRightNR;
                int *pnr = MixSoundBuffer;

                for (int nr=count; nr; nr--) {
                        int vnr = pnr[0] >> 1;
                        pnr[0] = vnr + n1;
                        n1 = vnr;
                        vnr = pnr[1] >> 1;
                        pnr[1] = vnr + n2;
                        n2 = vnr;
                        pnr += 2;
                }

                nLeftNR = n1;
                nRightNR = n2;
        }
}


void csf_process_mono_dsp(CSoundFile *, int count)
//----------------------------------------
{
        // Noise Reduction
        if (gdwSoundSetup & SNDMIX_NOISEREDUCTION) {
                int n = nLeftNR;
                int *pnr = MixSoundBuffer;

                for (int nr = count; nr; pnr++, nr--) {
                        int vnr = *pnr >> 1;
                        *pnr = vnr + n;
                        n = vnr;
                }

                nLeftNR = n;
        }
}


/////////////////////////////////////////////////////////////////
// Clean DSP Effects interface

int csf_set_wave_config_ex(CSoundFile *csf, int hqido, int bNR, int bEQ)
{
        uint32_t d = gdwSoundSetup & ~(SNDMIX_NORESAMPLING | SNDMIX_HQRESAMPLER | SNDMIX_NOISEREDUCTION | SNDMIX_EQ);

        if (hqido) d |= SNDMIX_HQRESAMPLER;
        if (bNR) d |= SNDMIX_NOISEREDUCTION;
        if (bEQ) d |= SNDMIX_EQ;

        gdwSoundSetup = d;
        csf_init_player(csf, false);
        return true;
}

