/*
 * This program is  free software; you can redistribute it  and modify it
 * under the terms of the GNU  General Public License as published by the
 * Free Software Foundation; either version 2  of the license or (at your
 * option) any later version.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *
 * Name                Date             Description
 *
 * Olivier Lapicque    --/--/--         Creation
 * Trevor Nunes        26/01/04         conditional compilation for AMD,MMX calls
 *
 */
#include "sndfile.h"
#include "cmixer.h"
#include <math.h>


#define EQ_BANDWIDTH    2.0
#define EQ_ZERO         0.000001


extern float MixFloatBuffer[];


typedef struct {
    float a0, a1, a2, b1, b2;
    float x1, x2, y1, y2;
    float gain, center_frequency;
    int   enabled;
} eq_band;


static const unsigned int eq_linear_to_db[33] = {
    16, 19, 22, 25, 28, 31, 34, 37,
    40, 43, 46, 49, 52, 55, 58, 61,
    64, 76, 88, 100, 112, 124, 136, 148,
    160, 172, 184, 196, 208, 220, 232, 244, 256
};


//static REAL f2ic = (REAL)(1 << 28);
//static REAL i2fc = (REAL)(1.0 / (1 << 28));

static eq_band eq[MAX_EQ_BANDS * 2] =
{
    // Default: Flat EQ
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1,   120, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1,   600, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  1200, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  3000, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  6000, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 10000, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1,   120, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1,   600, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  1200, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  3000, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1,  6000, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 10000, 0},
};


static void eq_filter(eq_band *pbs, float *pbuffer, unsigned int count)
{
        for (unsigned int i = 0; i < count; i++) {
                float x = pbuffer[i];
                float y = pbs->a1 * pbs->x1 +
                          pbs->a2 * pbs->x2 +
                          pbs->a0 * x +
                          pbs->b1 * pbs->y1 +
                          pbs->b2 * pbs->y2;

                pbs->x2 = pbs->x1;
                pbs->y2 = pbs->y1;
                pbs->x1 = x;
                pbuffer[i] = y;
                pbs->y1 = y;
        }
}


void eq_mono(int *buffer, unsigned int count)
{
        mono_mix_to_float(buffer, MixFloatBuffer, count);

        for (unsigned int b = 0; b < MAX_EQ_BANDS; b++)
        {
                if (eq[b].enabled && eq[b].gain != 1.0f)
                        eq_filter(&eq[b], MixFloatBuffer, count);
        }

        float_to_mono_mix(MixFloatBuffer, buffer, count);
}


// XXX: I rolled the two loops into one. Make sure this works.
void eq_stereo(int *buffer, unsigned int count)
{
        stereo_mix_to_float(buffer, MixFloatBuffer, MixFloatBuffer + MIXBUFFERSIZE, count);

        for (unsigned int b = 0; b < MAX_EQ_BANDS; b++) {
                int br = b + MAX_EQ_BANDS;

                // Left band
                if (eq[b].enabled && eq[b].gain != 1.0f)
                        eq_filter(&eq[b], MixFloatBuffer, count);

                // Right band
                if (eq[br].enabled && eq[br].gain != 1.0f)
                        eq_filter(&eq[br], MixFloatBuffer + MIXBUFFERSIZE, count);
        }

        float_to_stereo_mix(MixFloatBuffer, MixFloatBuffer + MIXBUFFERSIZE, buffer, count);
}


void initialize_eq(int reset, float freq)
{
        //float fMixingFreq = (REAL)gdwMixingFreq;

        // Gain = 0.5 (-6dB) .. 2 (+6dB)
        for (unsigned int band = 0; band < MAX_EQ_BANDS * 2; band++) {
                float k, k2, r, f;
                float v0, v1;
                int b = reset;

                if (!eq[band].enabled) {
                        eq[band].a0 = 0;
                        eq[band].a1 = 0;
                        eq[band].a2 = 0;
                        eq[band].b1 = 0;
                        eq[band].b2 = 0;
                        eq[band].x1 = 0;
                        eq[band].x2 = 0;
                        eq[band].y1 = 0;
                        eq[band].y2 = 0;
                        continue;
                }

                f = eq[band].center_frequency / freq;

                if (f > 0.45f)
                        eq[band].gain = 1;

                //if (f > 0.25)
                //      f = 0.25;

                //k = tan(PI * f);

                k = f * 3.141592654f;
                k = k + k * f;

                //if (k > (float) 0.707)
                //          k = (float) 0.707;

                k2 = k*k;
                v0 = eq[band].gain;
                v1 = 1;

                if (eq[band].gain < 1.0) {
                        v0 *= 0.5f / EQ_BANDWIDTH;
                        v1 *= 0.5f / EQ_BANDWIDTH;
                }
                else {
                        v0 *= 1.0f / EQ_BANDWIDTH;
                        v1 *= 1.0f / EQ_BANDWIDTH;
                }

                r = (1 + v0 * k + k2) / (1 + v1 * k + k2);

                if (r != eq[band].a0) {
                        eq[band].a0 = r;
                        b = 1;
                }

                r = 2 * (k2 - 1) / (1 + v1 * k + k2);

                if (r != eq[band].a1) {
                        eq[band].a1 = r;
                        b = 1;
                }

                r = (1 - v0 * k + k2) / (1 + v1 * k + k2);

                if (r != eq[band].a2) {
                        eq[band].a2 = r;
                        b = 1;
                }

                r = -2 * (k2 - 1) / (1 + v1 * k + k2);

                if (r != eq[band].b1) {
                        eq[band].b1 = r;
                        b = 1;
                }

                r = -(1 - v1 * k + k2) / (1 + v1 * k + k2);

                if (r != eq[band].b2) {
                        eq[band].b2 = r;
                        b = 1;
                }

                if (b) {
                        eq[band].x1 = 0;
                        eq[band].x2 = 0;
                        eq[band].y1 = 0;
                        eq[band].y2 = 0;
                }
        }
}


void set_eq_gains(const unsigned int *gainbuff, unsigned int gains, const unsigned int *freqs,
                  int reset, int mix_freq)
{
        for (unsigned int i = 0; i < MAX_EQ_BANDS; i++) {
                float g, f = 0;

                if (i < gains) {
                        unsigned int n = gainbuff[i];

                        //if (n > 32)
                        //        n = 32;

                        g = 1.0 + (((double) n) / 64.0);

                        if (freqs)
                            f = (float)(int) freqs[i];
                }
                else {
                        g = 1;
                }

                eq[i].gain =
                eq[i + MAX_EQ_BANDS].gain = g;
                eq[i].center_frequency =
                eq[i + MAX_EQ_BANDS].center_frequency = f;

                /* don't enable bands outside... */
                if (f > 20.0f &&
                    i < gains) {
                        eq[i].enabled =
                        eq[i + MAX_EQ_BANDS].enabled = 1;
                }
                else {
                        eq[i].enabled =
                        eq[i + MAX_EQ_BANDS].enabled = 0;
                }
        }

        initialize_eq(reset, mix_freq);
}

