/* This code is based on modplug.
 *
 * Original authors:
 *      Olivier Lapicque <olivierl@jps.net>
 *      Markus Fick <webmaster@mark-f.de> (spline + fir-resampler)
 */

#include <string.h>

#include "stdafx.h"
#include "sndfile.h"

#include "cmixer.h"

#define OFSDECAYSHIFT 8
#define OFSDECAYMASK  0xFF


void init_mix_buffer(int *buffer, unsigned int samples)
{
    memset(buffer, 0, samples * sizeof(int));
}


void stereo_fill(int *buffer, unsigned int samples, int* profs, int *plofs)
{
    int rofs = *profs;
    int lofs = *plofs;

    if (!rofs && !lofs) {
        init_mix_buffer(buffer, samples * 2);
        return;
    }

    for (unsigned int i = 0; i < samples; i++) {
        int x_r = (rofs + (((-rofs) >> 31) & OFSDECAYMASK)) >> OFSDECAYSHIFT;
        int x_l = (lofs + (((-lofs) >> 31) & OFSDECAYMASK)) >> OFSDECAYSHIFT;

        rofs -= x_r;
        lofs -= x_l;
        buffer[i * 2 ]    = x_r;
        buffer[i * 2 + 1] = x_l;
    }

    *profs = rofs;
    *plofs = lofs;
}


void end_channel_ofs(MODCHANNEL *channel, int *buffer, unsigned int samples)
{
    int rofs = channel->nROfs;
    int lofs = channel->nLOfs;

    if (!rofs && !lofs)
        return;

    for (unsigned int i = 0; i < samples; i++) {
        int x_r = (rofs + (((-rofs) >> 31) & OFSDECAYMASK)) >> OFSDECAYSHIFT;
        int x_l = (lofs + (((-lofs) >> 31) & OFSDECAYMASK)) >> OFSDECAYSHIFT;

        rofs -= x_r;
        lofs -= x_l;
        buffer[i * 2]     += x_r;
        buffer[i * 2 + 1] += x_l;
    }

    channel->nROfs = rofs;
    channel->nLOfs = lofs;
}


void interleave_front_rear(int *front, int *rear, unsigned int samples)
{
    for (unsigned int i = 0; i < samples; i++) {
        rear[i]  = front[(i * 2) + 1];
        front[i] = front[i * 2];
    }
}


void mono_from_stereo(int *mix_buf, unsigned int samples)
{
    for (unsigned int j, i = 0; i < samples; i++) {
        j = i << 1;
        mix_buf[i] = (mix_buf[j] + mix_buf[j + 1]) >> 1;
    }
}


static const float f2ic = (float) (1 << 28);
static const float i2fc = (float) (1.0 / (1 << 28));


void stereo_mix_to_float(const int *src, float *out1, float *out2, unsigned int count)
{
    for (unsigned int i = 0; i < count; i++) {
        *out1++ = *src * i2fc;
        src++;

        *out2++ = *src * i2fc;
        src++;
    }
}


void float_to_stereo_mix(const float *in1, const float *in2, int *out, unsigned int count)
{
    for (unsigned int i = 0; i < count; i++) {
        *out++ = (int) (*in1 * f2ic);
        *out++ = (int) (*in2 * f2ic);
        in1++;
        in2++;
    }
}


void mono_mix_to_float(const int *src, float *out, unsigned int count)
{
    for (unsigned int i = 0; i < count; i++) {
        *out++ = *src * i2fc;
        src++;
    }
}


void float_to_mono_mix(const float *in, int *out, unsigned int count)
{
    for (unsigned int i = 0; i < count; i++) {
        *out++ = (int) (*in * f2ic);
        in++;
    }
}


// ----------------------------------------------------------------------------
// Clip and convert functions
// ----------------------------------------------------------------------------
// XXX mins/max were int[2]
// 
// The original C version was written by Rani Assaf <rani@magic.metawire.com>


// Clip and convert to 8 bit
unsigned int clip_32_to_8(void *ptr, int *buffer, unsigned int samples, int *mins, int *maxs)
{
    unsigned char *p = (unsigned char *) ptr;

    for (unsigned int i = 0; i < samples; i++) {
        int n = buffer[i];

        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;

        if (n < mins[i & 1])
            mins[i & 1] = n;
        else if (n > maxs[i & 1])
            maxs[i & 1] = n;

        // 8-bit unsigned
        p[i] = (n >> (24 - MIXING_ATTENUATION)) ^ 0x80;    
    }

    return samples;
}


unsigned int clip_32_to_16(void *ptr, int *buffer, unsigned int samples, int *mins, int *maxs)
{
    signed short *p = (signed short *) ptr;

    for (unsigned int i = 0; i < samples; i++) {
        int n = buffer[i];

        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;
    
        if (n < mins[i & 1])
            mins[i & 1] = n;
        else if (n > maxs[i & 1])
            maxs[i & 1] = n;

        // 16-bit signed
        p[i] = n >> (16 - MIXING_ATTENUATION);    
    }

    return samples * 2;
}


// 24-bit might not work...
unsigned int clip_32_to_24(void *ptr, int *buffer, unsigned int samples, int *mins, int *maxs)
{
    /* the inventor of 24bit anything should be shot */
    unsigned char *p = (unsigned char *) ptr;

    for (unsigned int i = 0; i < samples; i++) {
        int n = buffer[i];

        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;

        if (n < mins[i & 1])
            mins[i & 1] = n;
        else if (n > maxs[i & 1])
            maxs[i & 1] = n;

        // 24-bit signed
        n = n >> (8 - MIXING_ATTENUATION);

        /* err, assume same endian */
        memcpy(p, &n, 3);
        p += 3;
    }

    return samples * 2;
}


// 32-bit might not work...
unsigned int clip_32_to_32(void *ptr, int *buffer, unsigned int samples, int *mins, int *maxs)
{
    signed int *p = (signed int *) ptr;

    for (unsigned int i = 0; i < samples; i++) {
        int n = buffer[i];

        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;

        if (n < mins[i & 1])
            mins[i & 1] = n;
        else if (n > maxs[i & 1])
            maxs[i & 1] = n;

        // 32-bit signed
        p[i] = (n >> MIXING_ATTENUATION);
    }

    return samples * 2;
}

