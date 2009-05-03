/*
 * This source code is public domain.
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *          Markus Fick <webmaster@mark-f.de> spline + fir-resampler
*/

#include <stdint.h>
#include <math.h>

#include "stdafx.h"
#include "sndfile.h"
#include "snd_fm.h"
#include "snd_gm.h"

void (*CSoundFile::_multi_out_raw) (int chan, int *buf, int len) = NULL;


// Front Mix Buffer (Also room for interleaved rear mix)
int MixSoundBuffer[MIXBUFFERSIZE * 4];

// Reverb Mix Buffer
#ifndef MODPLUG_NO_REVERB
int MixReverbBuffer[MIXBUFFERSIZE * 2];
extern uint32_t gnReverbSend;
#endif

int MixRearBuffer[MIXBUFFERSIZE * 2];
float MixFloatBuffer[MIXBUFFERSIZE * 2];

int MultiSoundBuffer[64][MIXBUFFERSIZE * 4];


extern long gnDryROfsVol;
extern long gnDryLOfsVol;
extern long gnRvbROfsVol;
extern long gnRvbLOfsVol;

// 4x256 taps polyphase FIR resampling filter
extern short int gFastSinc[];
extern short int gKaiserSinc[];    // 8-taps polyphase



/* The following lut settings are PRECOMPUTED.
 *
 * If you plan on changing these settings, you
 * MUST also regenerate the arrays.
 */

// number of bits used to scale spline coefs
#define SPLINE_QUANTBITS        14
#define SPLINE_QUANTSCALE       (1L<<SPLINE_QUANTBITS)
#define SPLINE_8SHIFT           (SPLINE_QUANTBITS-8)
#define SPLINE_16SHIFT          (SPLINE_QUANTBITS)

// forces coefsset to unity gain
#define SPLINE_CLAMPFORUNITY

// log2(number) of precalculated splines (range is [4..14])
#define SPLINE_FRACBITS 10
#define SPLINE_LUTLEN (1L<<SPLINE_FRACBITS)


// quantizer scale of window coefs
#define WFIR_QUANTBITS          15
#define WFIR_QUANTSCALE         (1L<<WFIR_QUANTBITS)
#define WFIR_8SHIFT                     (WFIR_QUANTBITS-8)
#define WFIR_16BITSHIFT         (WFIR_QUANTBITS)

// log2(number)-1 of precalculated taps range is [4..12]
#define WFIR_FRACBITS           10
#define WFIR_LUTLEN                     ((1L<<(WFIR_FRACBITS+1))+1)

// number of samples in window
#define WFIR_LOG2WIDTH          3
#define WFIR_WIDTH                      (1L<<WFIR_LOG2WIDTH)
#define WFIR_SMPSPERWING        ((WFIR_WIDTH-1)>>1)
// cutoff (1.0 == pi/2)
#define WFIR_CUTOFF             0.90f
// wfir type
#define WFIR_HANN               0
#define WFIR_HAMMING            1
#define WFIR_BLACKMANEXACT      2
#define WFIR_BLACKMAN3T61       3
#define WFIR_BLACKMAN3T67       4
#define WFIR_BLACKMAN4T92       5
#define WFIR_BLACKMAN4T74       6
#define WFIR_KAISER4T           7
#define WFIR_TYPE               WFIR_BLACKMANEXACT
// wfir help
#ifndef M_zPI
#define M_zPI           3.1415926535897932384626433832795
#endif
#define M_zEPS          1e-8
#define M_zBESSELEPS    1e-21

#include "precomp_lut.h"




// ----------------------------------------------------------------------------
// MIXING MACROS
// ----------------------------------------------------------------------------
/////////////////////////////////////////////////////
// Mixing Macros

#define SNDMIX_BEGINSAMPLELOOP8 \
        register MODCHANNEL * const pChn = pChannel; \
        nPos = pChn->nPosLo; \
        const signed char *p = (signed char *)(pChn->pCurrentSample + pChn->nPos); \
        if (pChn->dwFlags & CHN_STEREO) p += pChn->nPos; \
        int *pvol = pbuffer;\
        do {

#define SNDMIX_BEGINSAMPLELOOP16\
        register MODCHANNEL * const pChn = pChannel;\
        nPos = pChn->nPosLo;\
        const signed short *p = (signed short *)(pChn->pCurrentSample+(pChn->nPos*2));\
        if (pChn->dwFlags & CHN_STEREO) p += pChn->nPos;\
        int *pvol = pbuffer;\
        do {

#define SNDMIX_ENDSAMPLELOOP \
                nPos += pChn->nInc; \
        } while (pvol < pbufmax); \
        pChn->nPos  += nPos >> 16; \
        pChn->nPosLo = nPos & 0xFFFF;

#define SNDMIX_ENDSAMPLELOOP8   SNDMIX_ENDSAMPLELOOP
#define SNDMIX_ENDSAMPLELOOP16  SNDMIX_ENDSAMPLELOOP


//////////////////////////////////////////////////////////////////////////////
// Mono

// No interpolation
#define SNDMIX_GETMONOVOL8NOIDO \
    int vol = p[nPos >> 16] << 8;

#define SNDMIX_GETMONOVOL16NOIDO \
    int vol = p[nPos >> 16];

// Linear Interpolation
#define SNDMIX_GETMONOVOL8LINEAR \
    int poshi = nPos >> 16; \
    int poslo = (nPos >> 8) & 0xFF; \
    int srcvol = p[poshi]; \
    int destvol = p[poshi+1]; \
    int vol = (srcvol<<8) + ((int)(poslo * (destvol - srcvol)));

#define SNDMIX_GETMONOVOL16LINEAR \
    int poshi = nPos >> 16; \
    int poslo = (nPos >> 8) & 0xFF; \
    int srcvol = p[poshi]; \
    int destvol = p[poshi + 1]; \
    int vol = srcvol + ((int)(poslo * (destvol - srcvol)) >> 8);

// spline interpolation (2 guard bits should be enough???)
#define SPLINE_FRACSHIFT ((16 - SPLINE_FRACBITS) - 2)
#define SPLINE_FRACMASK  (((1L << (16 - SPLINE_FRACSHIFT)) - 1) & ~3)

#define SNDMIX_GETMONOVOL8SPLINE \
        int poshi = nPos >> 16; \
        int poslo = (nPos >> SPLINE_FRACSHIFT) & SPLINE_FRACMASK; \
        int vol   = (cubic_spline_lut[poslo  ]*(int)p[poshi-1] + \
                     cubic_spline_lut[poslo+1]*(int)p[poshi  ] + \
                     cubic_spline_lut[poslo+3]*(int)p[poshi+2] + \
                     cubic_spline_lut[poslo+2]*(int)p[poshi+1]) >> SPLINE_8SHIFT;

#define SNDMIX_GETMONOVOL16SPLINE \
        int poshi       = nPos >> 16; \
        int poslo       = (nPos >> SPLINE_FRACSHIFT) & SPLINE_FRACMASK; \
        int vol         = (cubic_spline_lut[poslo  ]*(int)p[poshi-1] + \
                       cubic_spline_lut[poslo+1]*(int)p[poshi  ] + \
                       cubic_spline_lut[poslo+3]*(int)p[poshi+2] + \
                       cubic_spline_lut[poslo+2]*(int)p[poshi+1]) >> SPLINE_16SHIFT;


// fir interpolation
#define WFIR_FRACSHIFT  (16-(WFIR_FRACBITS+1+WFIR_LOG2WIDTH))
#define WFIR_FRACMASK   ((((1L<<(17-WFIR_FRACSHIFT))-1)&~((1L<<WFIR_LOG2WIDTH)-1)))
#define WFIR_FRACHALVE  (1L<<(16-(WFIR_FRACBITS+2)))

#define SNDMIX_GETMONOVOL8FIRFILTER \
        int poshi  = nPos >> 16;\
        int poslo  = (nPos & 0xFFFF);\
        int firidx = ((poslo+WFIR_FRACHALVE)>>WFIR_FRACSHIFT) & WFIR_FRACMASK; \
        int vol    = (windowed_fir_lut[firidx+0]*(int)p[poshi+1-4]);    \
            vol   += (windowed_fir_lut[firidx+1]*(int)p[poshi+2-4]);    \
            vol   += (windowed_fir_lut[firidx+2]*(int)p[poshi+3-4]);    \
            vol   += (windowed_fir_lut[firidx+3]*(int)p[poshi+4-4]);    \
            vol   += (windowed_fir_lut[firidx+4]*(int)p[poshi+5-4]);    \
            vol   += (windowed_fir_lut[firidx+5]*(int)p[poshi+6-4]);    \
            vol   += (windowed_fir_lut[firidx+6]*(int)p[poshi+7-4]);    \
            vol   += (windowed_fir_lut[firidx+7]*(int)p[poshi+8-4]);    \
            vol  >>= WFIR_8SHIFT;

#define SNDMIX_GETMONOVOL16FIRFILTER \
    int poshi  = nPos >> 16;\
    int poslo  = (nPos & 0xFFFF);\
    int firidx = ((poslo+WFIR_FRACHALVE)>>WFIR_FRACSHIFT) & WFIR_FRACMASK; \
    int vol1   = (windowed_fir_lut[firidx+0]*(int)p[poshi+1-4]);        \
        vol1  += (windowed_fir_lut[firidx+1]*(int)p[poshi+2-4]);        \
        vol1  += (windowed_fir_lut[firidx+2]*(int)p[poshi+3-4]);        \
        vol1  += (windowed_fir_lut[firidx+3]*(int)p[poshi+4-4]);        \
    int vol2   = (windowed_fir_lut[firidx+4]*(int)p[poshi+5-4]);        \
        vol2  += (windowed_fir_lut[firidx+5]*(int)p[poshi+6-4]);        \
        vol2  += (windowed_fir_lut[firidx+6]*(int)p[poshi+7-4]);        \
        vol2  += (windowed_fir_lut[firidx+7]*(int)p[poshi+8-4]);        \
    int vol    = ((vol1>>1)+(vol2>>1)) >> (WFIR_16BITSHIFT-1);

/////////////////////////////////////////////////////////////////////////////
// Stereo

// No interpolation
#define SNDMIX_GETSTEREOVOL8NOIDO\
    int vol_l = p[(nPos>>16)*2] << 8;\
    int vol_r = p[(nPos>>16)*2+1] << 8;

#define SNDMIX_GETSTEREOVOL16NOIDO\
    int vol_l = p[(nPos>>16)*2];\
    int vol_r = p[(nPos>>16)*2+1];

// Linear Interpolation
#define SNDMIX_GETSTEREOVOL8LINEAR\
    int poshi = nPos >> 16;\
    int poslo = (nPos >> 8) & 0xFF;\
    int srcvol_l = p[poshi*2];\
    int vol_l = (srcvol_l<<8) + ((int)(poslo * (p[poshi*2+2] - srcvol_l)));\
    int srcvol_r = p[poshi*2+1];\
    int vol_r = (srcvol_r<<8) + ((int)(poslo * (p[poshi*2+3] - srcvol_r)));

#define SNDMIX_GETSTEREOVOL16LINEAR\
    int poshi = nPos >> 16;\
    int poslo = (nPos >> 8) & 0xFF;\
    int srcvol_l = p[poshi*2];\
    int vol_l = srcvol_l + ((int)(poslo * (p[poshi*2+2] - srcvol_l)) >> 8);\
    int srcvol_r = p[poshi*2+1];\
    int vol_r = srcvol_r + ((int)(poslo * (p[poshi*2+3] - srcvol_r)) >> 8);\

// Spline Interpolation
#define SNDMIX_GETSTEREOVOL8SPLINE \
    int poshi   = nPos >> 16; \
    int poslo   = (nPos >> SPLINE_FRACSHIFT) & SPLINE_FRACMASK; \
    int vol_l   = (cubic_spline_lut[poslo  ]*(int)p[(poshi-1)*2  ] + \
                   cubic_spline_lut[poslo+1]*(int)p[(poshi  )*2  ] + \
                   cubic_spline_lut[poslo+2]*(int)p[(poshi+1)*2  ] + \
                   cubic_spline_lut[poslo+3]*(int)p[(poshi+2)*2  ]) >> SPLINE_8SHIFT; \
    int vol_r   = (cubic_spline_lut[poslo  ]*(int)p[(poshi-1)*2+1] + \
                   cubic_spline_lut[poslo+1]*(int)p[(poshi  )*2+1] + \
                   cubic_spline_lut[poslo+2]*(int)p[(poshi+1)*2+1] + \
                   cubic_spline_lut[poslo+3]*(int)p[(poshi+2)*2+1]) >> SPLINE_8SHIFT;

#define SNDMIX_GETSTEREOVOL16SPLINE \
    int poshi   = nPos >> 16; \
    int poslo   = (nPos >> SPLINE_FRACSHIFT) & SPLINE_FRACMASK; \
    int vol_l   = (cubic_spline_lut[poslo  ]*(int)p[(poshi-1)*2  ] + \
                   cubic_spline_lut[poslo+1]*(int)p[(poshi  )*2  ] + \
                   cubic_spline_lut[poslo+2]*(int)p[(poshi+1)*2  ] + \
                   cubic_spline_lut[poslo+3]*(int)p[(poshi+2)*2  ]) >> SPLINE_16SHIFT; \
    int vol_r   = (cubic_spline_lut[poslo  ]*(int)p[(poshi-1)*2+1] + \
                   cubic_spline_lut[poslo+1]*(int)p[(poshi  )*2+1] + \
                   cubic_spline_lut[poslo+2]*(int)p[(poshi+1)*2+1] + \
                   cubic_spline_lut[poslo+3]*(int)p[(poshi+2)*2+1]) >> SPLINE_16SHIFT;

// fir interpolation
#define SNDMIX_GETSTEREOVOL8FIRFILTER \
    int poshi   = nPos >> 16;\
    int poslo   = (nPos & 0xFFFF);\
    int firidx  = ((poslo+WFIR_FRACHALVE)>>WFIR_FRACSHIFT) & WFIR_FRACMASK; \
    int vol_l   = (windowed_fir_lut[firidx+0]*(int)p[(poshi+1-4)*2  ]);   \
        vol_l  += (windowed_fir_lut[firidx+1]*(int)p[(poshi+2-4)*2  ]);   \
        vol_l  += (windowed_fir_lut[firidx+2]*(int)p[(poshi+3-4)*2  ]);   \
        vol_l  += (windowed_fir_lut[firidx+3]*(int)p[(poshi+4-4)*2  ]);   \
        vol_l  += (windowed_fir_lut[firidx+4]*(int)p[(poshi+5-4)*2  ]);   \
        vol_l  += (windowed_fir_lut[firidx+5]*(int)p[(poshi+6-4)*2  ]);   \
        vol_l  += (windowed_fir_lut[firidx+6]*(int)p[(poshi+7-4)*2  ]);   \
        vol_l  += (windowed_fir_lut[firidx+7]*(int)p[(poshi+8-4)*2  ]);   \
        vol_l >>= WFIR_8SHIFT; \
    int vol_r   = (windowed_fir_lut[firidx+0]*(int)p[(poshi+1-4)*2+1]);   \
        vol_r  += (windowed_fir_lut[firidx+1]*(int)p[(poshi+2-4)*2+1]);   \
        vol_r  += (windowed_fir_lut[firidx+2]*(int)p[(poshi+3-4)*2+1]);   \
        vol_r  += (windowed_fir_lut[firidx+3]*(int)p[(poshi+4-4)*2+1]);   \
        vol_r  += (windowed_fir_lut[firidx+4]*(int)p[(poshi+5-4)*2+1]);   \
        vol_r  += (windowed_fir_lut[firidx+5]*(int)p[(poshi+6-4)*2+1]);   \
        vol_r  += (windowed_fir_lut[firidx+6]*(int)p[(poshi+7-4)*2+1]);   \
        vol_r  += (windowed_fir_lut[firidx+7]*(int)p[(poshi+8-4)*2+1]);   \
        vol_r >>= WFIR_8SHIFT;

#define SNDMIX_GETSTEREOVOL16FIRFILTER \
    int poshi   = nPos >> 16;\
    int poslo   = (nPos & 0xFFFF);\
    int firidx  = ((poslo+WFIR_FRACHALVE)>>WFIR_FRACSHIFT) & WFIR_FRACMASK; \
    int vol1_l  = (windowed_fir_lut[firidx+0]*(int)p[(poshi+1-4)*2  ]);   \
        vol1_l += (windowed_fir_lut[firidx+1]*(int)p[(poshi+2-4)*2  ]);   \
        vol1_l += (windowed_fir_lut[firidx+2]*(int)p[(poshi+3-4)*2  ]);   \
        vol1_l += (windowed_fir_lut[firidx+3]*(int)p[(poshi+4-4)*2  ]);   \
   int vol2_l  = (windowed_fir_lut[firidx+4]*(int)p[(poshi+5-4)*2  ]);    \
       vol2_l += (windowed_fir_lut[firidx+5]*(int)p[(poshi+6-4)*2  ]);    \
       vol2_l += (windowed_fir_lut[firidx+6]*(int)p[(poshi+7-4)*2  ]);    \
       vol2_l += (windowed_fir_lut[firidx+7]*(int)p[(poshi+8-4)*2  ]);    \
   int vol_l   = ((vol1_l>>1)+(vol2_l>>1)) >> (WFIR_16BITSHIFT-1); \
   int vol1_r  = (windowed_fir_lut[firidx+0]*(int)p[(poshi+1-4)*2+1]);    \
       vol1_r += (windowed_fir_lut[firidx+1]*(int)p[(poshi+2-4)*2+1]);    \
       vol1_r += (windowed_fir_lut[firidx+2]*(int)p[(poshi+3-4)*2+1]);    \
       vol1_r += (windowed_fir_lut[firidx+3]*(int)p[(poshi+4-4)*2+1]);    \
   int vol2_r  = (windowed_fir_lut[firidx+4]*(int)p[(poshi+5-4)*2+1]);    \
       vol2_r += (windowed_fir_lut[firidx+5]*(int)p[(poshi+6-4)*2+1]);    \
       vol2_r += (windowed_fir_lut[firidx+6]*(int)p[(poshi+7-4)*2+1]);    \
       vol2_r += (windowed_fir_lut[firidx+7]*(int)p[(poshi+8-4)*2+1]);    \
   int vol_r   = ((vol1_r>>1)+(vol2_r>>1)) >> (WFIR_16BITSHIFT-1);

/////////////////////////////////////////////////////////////////////////////

#define SNDMIX_STOREMONOVOL\
        pvol[0] += vol * pChn->nRightVol;\
        pvol[1] += vol * pChn->nLeftVol;\
        pvol += 2;

#define SNDMIX_STORESTEREOVOL\
        pvol[0] += vol_l * pChn->nRightVol;\
        pvol[1] += vol_r * pChn->nLeftVol;\
        pvol += 2;

#define SNDMIX_STOREFASTMONOVOL\
        int v = vol * pChn->nRightVol;\
        pvol[0] += v;\
        pvol[1] += v;\
        pvol += 2;

#define SNDMIX_RAMPMONOVOL\
        nRampLeftVol += pChn->nLeftRamp;\
        nRampRightVol += pChn->nRightRamp;\
        pvol[0] += vol * (nRampRightVol >> VOLUMERAMPPRECISION);\
        pvol[1] += vol * (nRampLeftVol >> VOLUMERAMPPRECISION);\
        pvol += 2;

#define SNDMIX_RAMPFASTMONOVOL\
    nRampRightVol += pChn->nRightRamp;\
    int fastvol = vol * (nRampRightVol >> VOLUMERAMPPRECISION);\
    pvol[0] += fastvol;\
    pvol[1] += fastvol;\
    pvol += 2;

#define SNDMIX_RAMPSTEREOVOL\
    nRampLeftVol += pChn->nLeftRamp;\
    nRampRightVol += pChn->nRightRamp;\
    pvol[0] += vol_l * (nRampRightVol >> VOLUMERAMPPRECISION);\
    pvol[1] += vol_r * (nRampLeftVol >> VOLUMERAMPPRECISION);\
    pvol += 2;


///////////////////////////////////////////////////
// Resonant Filters

// Mono
#define MIX_BEGIN_FILTER \
    double fy1 = pChannel->nFilter_Y1;\
    double fy2 = pChannel->nFilter_Y2;\
    double ta;

#define MIX_END_FILTER \
    pChannel->nFilter_Y1 = fy1;\
    pChannel->nFilter_Y2 = fy2;

#define SNDMIX_PROCESSFILTER \
    ta = ((double)vol * pChn->nFilter_A0 + fy1 * pChn->nFilter_B0 + fy2 * pChn->nFilter_B1);\
    fy2 = fy1;\
    fy1 = ta;vol=(int)ta;

// Stereo
#define MIX_BEGIN_STEREO_FILTER \
    double fy1 = pChannel->nFilter_Y1;\
    double fy2 = pChannel->nFilter_Y2;\
    double fy3 = pChannel->nFilter_Y3;\
    double fy4 = pChannel->nFilter_Y4;\
    double ta, tb;

#define MIX_END_STEREO_FILTER \
    pChannel->nFilter_Y1 = fy1;\
    pChannel->nFilter_Y2 = fy2;\
    pChannel->nFilter_Y3 = fy3;\
    pChannel->nFilter_Y4 = fy4;\

#define SNDMIX_PROCESSSTEREOFILTER \
    ta = ((double)vol_l * pChn->nFilter_A0 + fy1 * pChn->nFilter_B0 + fy2 * pChn->nFilter_B1);\
    tb = ((double)vol_r * pChn->nFilter_A0 + fy3 * pChn->nFilter_B0 + fy4 * pChn->nFilter_B1);\
    fy2 = fy1; fy1 = ta;vol_l=(int)ta;\
    fy4 = fy3; fy3 = tb;vol_r=(int)tb;


//////////////////////////////////////////////////////////
// Interfaces

typedef VOID(MPPASMCALL * LPMIXINTERFACE) (MODCHANNEL *, int *, int *);


#define BEGIN_MIX_INTERFACE(func) \
    void MPPASMCALL func(MODCHANNEL *pChannel, int *pbuffer, int *pbufmax) \
    { \
        long nPos;


#define END_MIX_INTERFACE() \
        SNDMIX_ENDSAMPLELOOP \
    }


// Volume Ramps
#define BEGIN_RAMPMIX_INTERFACE(func) \
    BEGIN_MIX_INTERFACE(func)\
        long nRampRightVol = pChannel->nRampRightVol; \
        long nRampLeftVol = pChannel->nRampLeftVol;


#define END_RAMPMIX_INTERFACE() \
        SNDMIX_ENDSAMPLELOOP \
        pChannel->nRampRightVol = nRampRightVol; \
        pChannel->nRightVol     = nRampRightVol >> VOLUMERAMPPRECISION; \
        pChannel->nRampLeftVol  = nRampLeftVol; \
        pChannel->nLeftVol      = nRampLeftVol >> VOLUMERAMPPRECISION; \
    }


#define BEGIN_FASTRAMPMIX_INTERFACE(func) \
    BEGIN_MIX_INTERFACE(func) \
        long nRampRightVol = pChannel->nRampRightVol;


#define END_FASTRAMPMIX_INTERFACE() \
        SNDMIX_ENDSAMPLELOOP \
        pChannel->nRampRightVol = nRampRightVol; \
        pChannel->nRampLeftVol  = nRampRightVol; \
        pChannel->nRightVol     = nRampRightVol >> VOLUMERAMPPRECISION; \
        pChannel->nLeftVol      = pChannel->nRightVol; \
    }


// Mono Resonant Filters
#define BEGIN_MIX_FLT_INTERFACE(func) \
    BEGIN_MIX_INTERFACE(func) \
    MIX_BEGIN_FILTER


#define END_MIX_FLT_INTERFACE() \
        SNDMIX_ENDSAMPLELOOP \
        MIX_END_FILTER \
    }


#define BEGIN_RAMPMIX_FLT_INTERFACE(func) \
    BEGIN_MIX_INTERFACE(func) \
        long nRampRightVol = pChannel->nRampRightVol; \
        long nRampLeftVol  = pChannel->nRampLeftVol; \
        MIX_BEGIN_FILTER


#define END_RAMPMIX_FLT_INTERFACE() \
        SNDMIX_ENDSAMPLELOOP \
        MIX_END_FILTER \
        pChannel->nRampRightVol = nRampRightVol; \
        pChannel->nRightVol     = nRampRightVol >> VOLUMERAMPPRECISION; \
        pChannel->nRampLeftVol  = nRampLeftVol; \
        pChannel->nLeftVol      = nRampLeftVol >> VOLUMERAMPPRECISION; \
    }


// Stereo Resonant Filters
#define BEGIN_MIX_STFLT_INTERFACE(func) \
    BEGIN_MIX_INTERFACE(func) \
    MIX_BEGIN_STEREO_FILTER


#define END_MIX_STFLT_INTERFACE() \
        SNDMIX_ENDSAMPLELOOP \
        MIX_END_STEREO_FILTER \
    }


#define BEGIN_RAMPMIX_STFLT_INTERFACE(func) \
        BEGIN_MIX_INTERFACE(func) \
                long nRampRightVol = pChannel->nRampRightVol; \
                long nRampLeftVol  = pChannel->nRampLeftVol; \
                MIX_BEGIN_STEREO_FILTER


#define END_RAMPMIX_STFLT_INTERFACE() \
        SNDMIX_ENDSAMPLELOOP \
        MIX_END_STEREO_FILTER \
        pChannel->nRampRightVol = nRampRightVol; \
        pChannel->nRightVol     = nRampRightVol >> VOLUMERAMPPRECISION; \
        pChannel->nRampLeftVol  = nRampLeftVol; \
        pChannel->nLeftVol      = nRampLeftVol >> VOLUMERAMPPRECISION; \
    }


/////////////////////////////////////////////////////
//

extern void StereoMixToFloat(const int *pSrc, float *pOut1, float *pOut2, uint32_t nCount, const float _i2fc);
extern void FloatToStereoMix(const float *pIn1, const float *pIn2, int *pOut, uint32_t nCount, const float _f2ic);
extern void MonoMixToFloat(const int *pSrc, float *pOut, uint32_t nCount, const float _i2fc);
extern void FloatToMonoMix(const float *pIn, int *pOut, uint32_t nCount, const float _f2ic);

void InitMixBuffer(int *pBuffer, uint32_t nSamples);
void EndChannelOfs(MODCHANNEL * pChannel, int *pBuffer, uint32_t nSamples);
void StereoFill(int *pBuffer, uint32_t nSamples, long* lpROfs, long* lpLOfs);

void StereoMixToFloat(const int *, float *, float *, uint32_t nCount);
void FloatToStereoMix(const float *pIn1, const float *pIn2, int *pOut, uint32_t nCount);


/////////////////////////////////////////////////////
// Mono samples functions

BEGIN_MIX_INTERFACE(Mono8BitMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8NOIDO
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono16BitMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16NOIDO
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono8BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8LINEAR
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono16BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16LINEAR
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono8BitSplineMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8SPLINE
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono16BitSplineMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16SPLINE
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono8BitFirFilterMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8FIRFILTER SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()

BEGIN_MIX_INTERFACE(Mono16BitFirFilterMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16FIRFILTER
    SNDMIX_STOREMONOVOL
END_MIX_INTERFACE()


// Volume Ramps
BEGIN_RAMPMIX_INTERFACE(Mono8BitRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8NOIDO
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono16BitRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16NOIDO
    SNDMIX_RAMPMONOVOL 
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono8BitLinearRampMix)
   SNDMIX_BEGINSAMPLELOOP8
   SNDMIX_GETMONOVOL8LINEAR
   SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono16BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16LINEAR 
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono8BitSplineRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8SPLINE
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono16BitSplineRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16SPLINE
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono8BitFirFilterRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8FIRFILTER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()

BEGIN_RAMPMIX_INTERFACE(Mono16BitFirFilterRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16FIRFILTER
    SNDMIX_RAMPMONOVOL
END_RAMPMIX_INTERFACE()


//////////////////////////////////////////////////////
// Fast mono mix for leftvol=rightvol (1 less imul)
BEGIN_MIX_INTERFACE(FastMono8BitMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8NOIDO SNDMIX_STOREFASTMONOVOL END_MIX_INTERFACE()
 BEGIN_MIX_INTERFACE(FastMono16BitMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16NOIDO SNDMIX_STOREFASTMONOVOL END_MIX_INTERFACE()
 BEGIN_MIX_INTERFACE(FastMono8BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8LINEAR SNDMIX_STOREFASTMONOVOL END_MIX_INTERFACE()
 BEGIN_MIX_INTERFACE(FastMono16BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16LINEAR SNDMIX_STOREFASTMONOVOL END_MIX_INTERFACE()
 BEGIN_MIX_INTERFACE(FastMono8BitSplineMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8SPLINE SNDMIX_STOREFASTMONOVOL END_MIX_INTERFACE()
 BEGIN_MIX_INTERFACE(FastMono16BitSplineMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16SPLINE SNDMIX_STOREFASTMONOVOL END_MIX_INTERFACE()
 BEGIN_MIX_INTERFACE(FastMono8BitFirFilterMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8FIRFILTER SNDMIX_STOREFASTMONOVOL END_MIX_INTERFACE()
 BEGIN_MIX_INTERFACE(FastMono16BitFirFilterMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16FIRFILTER
    SNDMIX_STOREFASTMONOVOL END_MIX_INTERFACE()

// Fast Ramps
BEGIN_FASTRAMPMIX_INTERFACE(FastMono8BitRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8NOIDO
    SNDMIX_RAMPFASTMONOVOL END_FASTRAMPMIX_INTERFACE()
 BEGIN_FASTRAMPMIX_INTERFACE(FastMono16BitRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16NOIDO
    SNDMIX_RAMPFASTMONOVOL END_FASTRAMPMIX_INTERFACE()
 BEGIN_FASTRAMPMIX_INTERFACE(FastMono8BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8LINEAR
    SNDMIX_RAMPFASTMONOVOL END_FASTRAMPMIX_INTERFACE()
 BEGIN_FASTRAMPMIX_INTERFACE(FastMono16BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16LINEAR
    SNDMIX_RAMPFASTMONOVOL END_FASTRAMPMIX_INTERFACE()
 BEGIN_FASTRAMPMIX_INTERFACE(FastMono8BitSplineRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8SPLINE
    SNDMIX_RAMPFASTMONOVOL END_FASTRAMPMIX_INTERFACE()
 BEGIN_FASTRAMPMIX_INTERFACE(FastMono16BitSplineRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16SPLINE
    SNDMIX_RAMPFASTMONOVOL END_FASTRAMPMIX_INTERFACE()
 BEGIN_FASTRAMPMIX_INTERFACE(FastMono8BitFirFilterRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8FIRFILTER
    SNDMIX_RAMPFASTMONOVOL END_FASTRAMPMIX_INTERFACE()
 BEGIN_FASTRAMPMIX_INTERFACE(FastMono16BitFirFilterRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16FIRFILTER
    SNDMIX_RAMPFASTMONOVOL END_FASTRAMPMIX_INTERFACE()

//////////////////////////////////////////////////////
// Stereo samples
BEGIN_MIX_INTERFACE(Stereo8BitMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8NOIDO SNDMIX_STORESTEREOVOL END_MIX_INTERFACE()
 BEGIN_MIX_INTERFACE(Stereo16BitMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16NOIDO SNDMIX_STORESTEREOVOL END_MIX_INTERFACE()
 BEGIN_MIX_INTERFACE(Stereo8BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8LINEAR SNDMIX_STORESTEREOVOL END_MIX_INTERFACE()
 BEGIN_MIX_INTERFACE(Stereo16BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16LINEAR SNDMIX_STORESTEREOVOL END_MIX_INTERFACE()
 BEGIN_MIX_INTERFACE(Stereo8BitSplineMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8SPLINE SNDMIX_STORESTEREOVOL END_MIX_INTERFACE()
 BEGIN_MIX_INTERFACE(Stereo16BitSplineMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16SPLINE SNDMIX_STORESTEREOVOL END_MIX_INTERFACE()
 BEGIN_MIX_INTERFACE(Stereo8BitFirFilterMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8FIRFILTER SNDMIX_STORESTEREOVOL END_MIX_INTERFACE()
 BEGIN_MIX_INTERFACE(Stereo16BitFirFilterMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16FIRFILTER
    SNDMIX_STORESTEREOVOL END_MIX_INTERFACE()

// Volume Ramps
BEGIN_RAMPMIX_INTERFACE(Stereo8BitRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8NOIDO SNDMIX_RAMPSTEREOVOL END_RAMPMIX_INTERFACE()
 BEGIN_RAMPMIX_INTERFACE(Stereo16BitRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16NOIDO SNDMIX_RAMPSTEREOVOL END_RAMPMIX_INTERFACE()
 BEGIN_RAMPMIX_INTERFACE(Stereo8BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8LINEAR SNDMIX_RAMPSTEREOVOL END_RAMPMIX_INTERFACE()
 BEGIN_RAMPMIX_INTERFACE(Stereo16BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16LINEAR
    SNDMIX_RAMPSTEREOVOL END_RAMPMIX_INTERFACE()
 BEGIN_RAMPMIX_INTERFACE(Stereo8BitSplineRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8SPLINE SNDMIX_RAMPSTEREOVOL END_RAMPMIX_INTERFACE()
 BEGIN_RAMPMIX_INTERFACE(Stereo16BitSplineRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16SPLINE
    SNDMIX_RAMPSTEREOVOL END_RAMPMIX_INTERFACE()
 BEGIN_RAMPMIX_INTERFACE(Stereo8BitFirFilterRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8FIRFILTER
    SNDMIX_RAMPSTEREOVOL END_RAMPMIX_INTERFACE()
 BEGIN_RAMPMIX_INTERFACE(Stereo16BitFirFilterRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16FIRFILTER
    SNDMIX_RAMPSTEREOVOL END_RAMPMIX_INTERFACE()


//////////////////////////////////////////////////////
// Resonant Filter Mix
// Mono Filter Mix
BEGIN_MIX_FLT_INTERFACE(FilterMono8BitMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8NOIDO
    SNDMIX_PROCESSFILTER SNDMIX_STOREMONOVOL END_MIX_FLT_INTERFACE()
 BEGIN_MIX_FLT_INTERFACE(FilterMono16BitMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16NOIDO
    SNDMIX_PROCESSFILTER SNDMIX_STOREMONOVOL END_MIX_FLT_INTERFACE()
 BEGIN_MIX_FLT_INTERFACE(FilterMono8BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8LINEAR
    SNDMIX_PROCESSFILTER SNDMIX_STOREMONOVOL END_MIX_FLT_INTERFACE()
 BEGIN_MIX_FLT_INTERFACE(FilterMono16BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16LINEAR
    SNDMIX_PROCESSFILTER SNDMIX_STOREMONOVOL END_MIX_FLT_INTERFACE()
 BEGIN_MIX_FLT_INTERFACE(FilterMono8BitSplineMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8SPLINE
    SNDMIX_PROCESSFILTER SNDMIX_STOREMONOVOL END_MIX_FLT_INTERFACE()
 BEGIN_MIX_FLT_INTERFACE(FilterMono16BitSplineMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16SPLINE
    SNDMIX_PROCESSFILTER SNDMIX_STOREMONOVOL END_MIX_FLT_INTERFACE()
 BEGIN_MIX_FLT_INTERFACE(FilterMono8BitFirFilterMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8FIRFILTER
    SNDMIX_PROCESSFILTER SNDMIX_STOREMONOVOL END_MIX_FLT_INTERFACE()
 BEGIN_MIX_FLT_INTERFACE(FilterMono16BitFirFilterMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16FIRFILTER
    SNDMIX_PROCESSFILTER SNDMIX_STOREMONOVOL END_MIX_FLT_INTERFACE()
// Filter + Ramp
BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono8BitRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8NOIDO
    SNDMIX_PROCESSFILTER SNDMIX_RAMPMONOVOL END_RAMPMIX_FLT_INTERFACE()
 BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono16BitRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16NOIDO
    SNDMIX_PROCESSFILTER SNDMIX_RAMPMONOVOL END_RAMPMIX_FLT_INTERFACE()
 BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono8BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8LINEAR
    SNDMIX_PROCESSFILTER SNDMIX_RAMPMONOVOL END_RAMPMIX_FLT_INTERFACE()
 BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono16BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16LINEAR
    SNDMIX_PROCESSFILTER SNDMIX_RAMPMONOVOL END_RAMPMIX_FLT_INTERFACE()
 BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono8BitSplineRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8SPLINE
    SNDMIX_PROCESSFILTER SNDMIX_RAMPMONOVOL END_RAMPMIX_FLT_INTERFACE()
 BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono16BitSplineRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16SPLINE
    SNDMIX_PROCESSFILTER SNDMIX_RAMPMONOVOL END_RAMPMIX_FLT_INTERFACE()
 BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono8BitFirFilterRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETMONOVOL8FIRFILTER
    SNDMIX_PROCESSFILTER SNDMIX_RAMPMONOVOL END_RAMPMIX_FLT_INTERFACE()
 BEGIN_RAMPMIX_FLT_INTERFACE(FilterMono16BitFirFilterRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETMONOVOL16FIRFILTER
    SNDMIX_PROCESSFILTER SNDMIX_RAMPMONOVOL END_RAMPMIX_FLT_INTERFACE()

// Stereo Filter Mix
BEGIN_MIX_STFLT_INTERFACE(FilterStereo8BitMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8NOIDO
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL END_MIX_STFLT_INTERFACE()
 BEGIN_MIX_STFLT_INTERFACE(FilterStereo16BitMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16NOIDO
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL END_MIX_STFLT_INTERFACE()
 BEGIN_MIX_STFLT_INTERFACE(FilterStereo8BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8LINEAR
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL END_MIX_STFLT_INTERFACE()
 BEGIN_MIX_STFLT_INTERFACE(FilterStereo16BitLinearMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16LINEAR
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL END_MIX_STFLT_INTERFACE()
 BEGIN_MIX_STFLT_INTERFACE(FilterStereo8BitSplineMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8SPLINE
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL END_MIX_STFLT_INTERFACE()
 BEGIN_MIX_STFLT_INTERFACE(FilterStereo16BitSplineMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16SPLINE
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL END_MIX_STFLT_INTERFACE()
 BEGIN_MIX_STFLT_INTERFACE(FilterStereo8BitFirFilterMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8FIRFILTER
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL END_MIX_STFLT_INTERFACE()
 BEGIN_MIX_STFLT_INTERFACE(FilterStereo16BitFirFilterMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16FIRFILTER
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_STORESTEREOVOL END_MIX_STFLT_INTERFACE()
// Stereo Filter + Ramp
BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo8BitRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8NOIDO
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL END_RAMPMIX_STFLT_INTERFACE()
 BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo16BitRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16NOIDO
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL END_RAMPMIX_STFLT_INTERFACE()
 BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo8BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8LINEAR
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL END_RAMPMIX_STFLT_INTERFACE()
 BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo16BitLinearRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16LINEAR
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL END_RAMPMIX_STFLT_INTERFACE()
 BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo8BitSplineRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8SPLINE
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL END_RAMPMIX_STFLT_INTERFACE()
 BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo16BitSplineRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16SPLINE
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL END_RAMPMIX_STFLT_INTERFACE()
 BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo8BitFirFilterRampMix)
    SNDMIX_BEGINSAMPLELOOP8
    SNDMIX_GETSTEREOVOL8FIRFILTER
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL END_RAMPMIX_STFLT_INTERFACE()
 BEGIN_RAMPMIX_STFLT_INTERFACE(FilterStereo16BitFirFilterRampMix)
    SNDMIX_BEGINSAMPLELOOP16
    SNDMIX_GETSTEREOVOL16FIRFILTER
    SNDMIX_PROCESSSTEREOFILTER
    SNDMIX_RAMPSTEREOVOL END_RAMPMIX_STFLT_INTERFACE()


/////////////////////////////////////////////////////////////////////////////////////
//
// Mix function tables
//
//
// Index is as follow:
//      [b1-b0] format (8-bit-mono, 16-bit-mono, 8-bit-stereo, 16-bit-stereo)
//      [b2]    ramp
//      [b3]    filter
//      [b5-b4] src type
//
#define MIXNDX_16BIT        0x01
#define MIXNDX_STEREO       0x02
#define MIXNDX_RAMP         0x04
#define MIXNDX_FILTER       0x08
#define MIXNDX_LINEARSRC    0x10
#define MIXNDX_SPLINESRC    0x20
#define MIXNDX_FIRSRC       0x30

const LPMIXINTERFACE gpMixFunctionTable[2 * 2 * 16] = {
    // No SRC
    Mono8BitMix, Mono16BitMix, Stereo8BitMix, Stereo16BitMix,
    Mono8BitRampMix, Mono16BitRampMix, Stereo8BitRampMix,
    Stereo16BitRampMix,
    // No SRC, Filter
    FilterMono8BitMix, FilterMono16BitMix, FilterStereo8BitMix,
    FilterStereo16BitMix, FilterMono8BitRampMix,
        FilterMono16BitRampMix,
    FilterStereo8BitRampMix, FilterStereo16BitRampMix,
    // Linear SRC
    Mono8BitLinearMix, Mono16BitLinearMix, Stereo8BitLinearMix,
    Stereo16BitLinearMix, Mono8BitLinearRampMix,
        Mono16BitLinearRampMix,
    Stereo8BitLinearRampMix, Stereo16BitLinearRampMix,
    // Linear SRC, Filter
    FilterMono8BitLinearMix, FilterMono16BitLinearMix,
    FilterStereo8BitLinearMix, FilterStereo16BitLinearMix,
    FilterMono8BitLinearRampMix, FilterMono16BitLinearRampMix,
    FilterStereo8BitLinearRampMix, FilterStereo16BitLinearRampMix,

    // FirFilter SRC
    Mono8BitSplineMix, Mono16BitSplineMix, Stereo8BitSplineMix,
    Stereo16BitSplineMix, Mono8BitSplineRampMix,
        Mono16BitSplineRampMix,
    Stereo8BitSplineRampMix, Stereo16BitSplineRampMix,
    // Spline SRC, Filter
    FilterMono8BitSplineMix, FilterMono16BitSplineMix,
    FilterStereo8BitSplineMix, FilterStereo16BitSplineMix,
    FilterMono8BitSplineRampMix, FilterMono16BitSplineRampMix,
    FilterStereo8BitSplineRampMix, FilterStereo16BitSplineRampMix,

    // FirFilter  SRC
    Mono8BitFirFilterMix, Mono16BitFirFilterMix,
        Stereo8BitFirFilterMix,
    Stereo16BitFirFilterMix, Mono8BitFirFilterRampMix,
    Mono16BitFirFilterRampMix, Stereo8BitFirFilterRampMix,
    Stereo16BitFirFilterRampMix,
    // FirFilter  SRC, Filter
    FilterMono8BitFirFilterMix, FilterMono16BitFirFilterMix,
    FilterStereo8BitFirFilterMix, FilterStereo16BitFirFilterMix,
    FilterMono8BitFirFilterRampMix, FilterMono16BitFirFilterRampMix,
    FilterStereo8BitFirFilterRampMix, FilterStereo16BitFirFilterRampMix
};


const LPMIXINTERFACE gpFastMixFunctionTable[2 * 2 * 16] = {
    // No SRC
    FastMono8BitMix, FastMono16BitMix, Stereo8BitMix, Stereo16BitMix,
    FastMono8BitRampMix, FastMono16BitRampMix, Stereo8BitRampMix,
    Stereo16BitRampMix,

    // No SRC, Filter
    FilterMono8BitMix, FilterMono16BitMix, FilterStereo8BitMix,
    FilterStereo16BitMix, FilterMono8BitRampMix,
    FilterMono16BitRampMix,
    FilterStereo8BitRampMix, FilterStereo16BitRampMix,

    // Linear SRC
    FastMono8BitLinearMix, FastMono16BitLinearMix, Stereo8BitLinearMix,
    Stereo16BitLinearMix, FastMono8BitLinearRampMix,
    FastMono16BitLinearRampMix, Stereo8BitLinearRampMix,
    Stereo16BitLinearRampMix,

    // Linear SRC, Filter
    FilterMono8BitLinearMix, FilterMono16BitLinearMix,
    FilterStereo8BitLinearMix, FilterStereo16BitLinearMix,
    FilterMono8BitLinearRampMix, FilterMono16BitLinearRampMix,
    FilterStereo8BitLinearRampMix, FilterStereo16BitLinearRampMix,

    // Spline SRC
    Mono8BitSplineMix, Mono16BitSplineMix, Stereo8BitSplineMix,
    Stereo16BitSplineMix, Mono8BitSplineRampMix,
    Mono16BitSplineRampMix,
    Stereo8BitSplineRampMix, Stereo16BitSplineRampMix,

    // Spline SRC, Filter
    FilterMono8BitSplineMix, FilterMono16BitSplineMix,
    FilterStereo8BitSplineMix, FilterStereo16BitSplineMix,
    FilterMono8BitSplineRampMix, FilterMono16BitSplineRampMix,
    FilterStereo8BitSplineRampMix, FilterStereo16BitSplineRampMix,

    // FirFilter SRC
    Mono8BitFirFilterMix, Mono16BitFirFilterMix,
    Stereo8BitFirFilterMix,
    Stereo16BitFirFilterMix, Mono8BitFirFilterRampMix,
    Mono16BitFirFilterRampMix, Stereo8BitFirFilterRampMix,
    Stereo16BitFirFilterRampMix,

    // FirFilter SRC, Filter
    FilterMono8BitFirFilterMix, FilterMono16BitFirFilterMix,
    FilterStereo8BitFirFilterMix, FilterStereo16BitFirFilterMix,
    FilterMono8BitFirFilterRampMix, FilterMono16BitFirFilterRampMix,
    FilterStereo8BitFirFilterRampMix,
    FilterStereo16BitFirFilterRampMix,
};


/////////////////////////////////////////////////////////////////////////

static long MPPFASTCALL GetSampleCount(MODCHANNEL * pChn, long nSamples)
//---------------------------------------------------------------------
{
    long nLoopStart =
        (pChn->dwFlags & CHN_LOOP) ? pChn->nLoopStart : 0;
    long nInc = pChn->nInc;

    if ((nSamples <= 0) || (!nInc) || (!pChn->nLength))
        return 0;

    // Under zero ?
    if ((long) pChn->nPos < nLoopStart) {
        if (nInc < 0) {
            // Invert loop for bidi loops
            long nDelta =
                ((nLoopStart - pChn->nPos) << 16) -
                (pChn->nPosLo & 0xFFFF);
            pChn->nPos = nLoopStart | (nDelta >> 16);
            pChn->nPosLo = nDelta & 0xffff;

            if (((long) pChn->nPos < nLoopStart)
                || (pChn->nPos >=
                (nLoopStart + pChn->nLength) / 2)) {
                pChn->nPos = nLoopStart;
                pChn->nPosLo = 0;
            }

            nInc = -nInc;
            pChn->nInc = nInc;
            // go forward
            pChn->dwFlags &= ~(CHN_PINGPONGFLAG);

            if ((!(pChn->dwFlags & CHN_LOOP))
                || (pChn->nPos >= pChn->nLength)) {
                pChn->nPos = pChn->nLength;
                pChn->nPosLo = 0;
                return 0;
            }
        } else {
            // We probably didn't hit the loop end yet (first loop), so we do nothing
            if ((long) pChn->nPos < 0)
                pChn->nPos = 0;
        }
    }
    // Past the end
    else if (pChn->nPos >= pChn->nLength) {
        // not looping -> stop this channel
        if (!(pChn->dwFlags & CHN_LOOP))
            return 0;

        if (pChn->dwFlags & CHN_PINGPONGLOOP) {
            // Invert loop
            if (nInc > 0) {
                nInc = -nInc;
                pChn->nInc = nInc;
            }

            pChn->dwFlags |= CHN_PINGPONGFLAG;
            // adjust loop position
            long nDeltaHi = (pChn->nPos - pChn->nLength);
            long nDeltaLo = 0x10000 - (pChn->nPosLo & 0xffff);
            pChn->nPos =
                pChn->nLength - nDeltaHi - (nDeltaLo >> 16);
            pChn->nPosLo = nDeltaLo & 0xffff;
            if ((pChn->nPos <= pChn->nLoopStart)
                || (pChn->nPos >= pChn->nLength))
                pChn->nPos = pChn->nLength - 1;
        } else {
            if (nInc < 0)    // This is a bug
            {
                nInc = -nInc;
                pChn->nInc = nInc;
            }
            // Restart at loop start
            pChn->nPos += nLoopStart - pChn->nLength;
            if ((long) pChn->nPos < nLoopStart)
                pChn->nPos = pChn->nLoopStart;
        }
    }

    long nPos = pChn->nPos;

    // too big increment, and/or too small loop length
    if (nPos < nLoopStart) {
        if ((nPos < 0) || (nInc < 0))
            return 0;
    }

    if ((nPos < 0) || (nPos >= (long) pChn->nLength))
        return 0;

    long nPosLo = (USHORT) pChn->nPosLo, nSmpCount = nSamples;

    if (nInc < 0) {
        long nInv = -nInc;
        long maxsamples = 16384 / ((nInv >> 16) + 1);
        if (maxsamples < 2)
            maxsamples = 2;
        if (nSamples > maxsamples)
            nSamples = maxsamples;
        long nDeltaHi = (nInv >> 16) * (nSamples - 1);
        long nDeltaLo = (nInv & 0xffff) * (nSamples - 1);
        long nPosDest =
            nPos - nDeltaHi + ((nPosLo - nDeltaLo) >> 16);
        if (nPosDest < nLoopStart) {
            nSmpCount =
                (unsigned long) (((((long long) nPos -
                    nLoopStart) << 16) + nPosLo -
                      1) / nInv) + 1;
        }
    } else {
        long maxsamples = 16384 / ((nInc >> 16) + 1);
        if (maxsamples < 2)
            maxsamples = 2;
        if (nSamples > maxsamples)
            nSamples = maxsamples;
        long nDeltaHi = (nInc >> 16) * (nSamples - 1);
        long nDeltaLo = (nInc & 0xffff) * (nSamples - 1);
        long nPosDest =
            nPos + nDeltaHi + ((nPosLo + nDeltaLo) >> 16);
        if (nPosDest >= (long) pChn->nLength) {
            nSmpCount =
                (unsigned long) (((((long long) pChn->nLength -
                    nPos) << 16) - nPosLo -
                      1) / nInc) + 1;
        }
    }

    if (nSmpCount <= 1)
        return 1;

    if (nSmpCount > nSamples)
        return nSamples;

    return nSmpCount;
}


uint32_t CSoundFile::CreateStereoMix(int count)
//-----------------------------------------
{
    long* pOfsL, *pOfsR;
    uint32_t nchused, nchmixed;

    if (!count)
        return 0;
    if (gnChannels > 2)
        InitMixBuffer(MixRearBuffer, count * 2);
    nchused = nchmixed = 0;
    if (CSoundFile::_multi_out_raw) {
        memset(MultiSoundBuffer, 0, sizeof(MultiSoundBuffer));
    }
    for (uint32_t nChn = 0; nChn < m_nMixChannels; nChn++) {
        const LPMIXINTERFACE *pMixFuncTable;
        MODCHANNEL *const pChannel = &Chn[ChnMix[nChn]];
        uint32_t nFlags, nMasterCh;
        uint32_t nrampsamples;
        long nSmpCount;
        int nsamples;
        int *pbuffer;

        if (!pChannel->pCurrentSample)
            continue;
        nMasterCh =
            (ChnMix[nChn] <
             m_nChannels) ? ChnMix[nChn] +
            1 : pChannel->nMasterChn;
        pOfsR = &gnDryROfsVol;
        pOfsL = &gnDryLOfsVol;
        nFlags = 0;
        if (pChannel->dwFlags & CHN_16BIT)
            nFlags |= MIXNDX_16BIT;
        if (pChannel->dwFlags & CHN_STEREO)
            nFlags |= MIXNDX_STEREO;
        if (pChannel->dwFlags & CHN_FILTER)
            nFlags |= MIXNDX_FILTER;
        if (!(pChannel->dwFlags & CHN_NOIDO)
            && !(gdwSoundSetup & SNDMIX_NORESAMPLING)) {
            // use hq-fir mixer?
            if ((gdwSoundSetup &
                 (SNDMIX_HQRESAMPLER | SNDMIX_ULTRAHQSRCMODE))
                ==
                (SNDMIX_HQRESAMPLER | SNDMIX_ULTRAHQSRCMODE))
                nFlags |= MIXNDX_FIRSRC;
            else if ((gdwSoundSetup & SNDMIX_HQRESAMPLER))
                nFlags |= MIXNDX_SPLINESRC;
            else
                nFlags |= MIXNDX_LINEARSRC;    // use
        }
        if ((nFlags < 0x40)
            && (pChannel->nLeftVol == pChannel->nRightVol)
            && ((!pChannel->nRampLength)
            || (pChannel->nLeftRamp ==
                pChannel->nRightRamp))) {
            pMixFuncTable = gpFastMixFunctionTable;
        } else {
            pMixFuncTable = gpMixFunctionTable;
        }
        nsamples = count;
#ifndef MODPLUG_NO_REVERB
        pbuffer =
            (gdwSoundSetup & SNDMIX_REVERB) ? MixReverbBuffer :
            MixSoundBuffer;
        if (pChannel->dwFlags & CHN_NOREVERB)
            pbuffer = MixSoundBuffer;
        if (pChannel->dwFlags & CHN_REVERB)
            pbuffer = MixReverbBuffer;
        if (pbuffer == MixReverbBuffer) {
            if (!gnReverbSend)
                memset(MixReverbBuffer, 0, count * 8);
            gnReverbSend += count;
        }
#else
        pbuffer = MixSoundBuffer;
#endif
        if (CSoundFile::_multi_out_raw) {
            pbuffer = MultiSoundBuffer[nMasterCh];
        }
        nchused++;
        ////////////////////////////////////////////////////
          SampleLooping:
        nrampsamples = nsamples;
        if (pChannel->nRampLength > 0) {
            if ((long) nrampsamples > pChannel->nRampLength)
                nrampsamples = pChannel->nRampLength;
        }

        nSmpCount = 1;
        /* Figure out the number of remaining samples,
         * unless we're in AdLib or MIDI mode (to prevent
         * artificial KeyOffs)
         */
        if (!(pChannel->dwFlags & CHN_ADLIB)) {
            nSmpCount = GetSampleCount(pChannel, nrampsamples);
        }
        if (nSmpCount <= 0) {
            // Stopping the channel
            pChannel->pCurrentSample = NULL;
            pChannel->nLength = 0;
            pChannel->nPos = 0;
            pChannel->nPosLo = 0;
            pChannel->nRampLength = 0;
            EndChannelOfs(pChannel, pbuffer, nsamples);
            *pOfsR += pChannel->nROfs;
            *pOfsL += pChannel->nLOfs;
            pChannel->nROfs = pChannel->nLOfs = 0;
            pChannel->dwFlags &= ~CHN_PINGPONGFLAG;
            continue;
        }
        // Should we mix this channel ?
        uint32_t naddmix = 0;
        if (((nchmixed >= m_nMaxMixChannels)
             && (!(gdwSoundSetup & SNDMIX_DIRECTTODISK)))
            || ((!pChannel->nRampLength)
            && (!(pChannel->nLeftVol | pChannel->nRightVol))))
        {
            long delta =
                (pChannel->nInc * (long) nSmpCount) +
                (long) pChannel->nPosLo;
            pChannel->nPosLo = delta & 0xFFFF;
            pChannel->nPos += (delta >> 16);
            pChannel->nROfs = pChannel->nLOfs = 0;
            pbuffer += nSmpCount * 2;
        } else
            // Do mixing
        {
            if (pChannel->nLength) {
                pChannel->topnote_offset =
                    ((pChannel->nPos << 16) | pChannel->
                     nPosLo) % pChannel->nLength;
            }

            /* Mix the stream, unless we're in AdLib mode */
            if (!(pChannel->dwFlags & CHN_ADLIB)) {
                // Choose function for mixing
                LPMIXINTERFACE pMixFunc;
                pMixFunc =
                    (pChannel->
                     nRampLength) ? pMixFuncTable[nFlags |
                                  MIXNDX_RAMP]
                    : pMixFuncTable[nFlags];
                int *pbufmax = pbuffer + (nSmpCount * 2);
                pChannel->nROfs = -*(pbufmax - 2);
                pChannel->nLOfs = -*(pbufmax - 1);

                pMixFunc(pChannel, pbuffer, pbufmax);
                pChannel->nROfs += *(pbufmax - 2);
                pChannel->nLOfs += *(pbufmax - 1);
                pbuffer = pbufmax;
                naddmix = 1;
            }

        }
        nsamples -= nSmpCount;
        if (pChannel->nRampLength) {
            pChannel->nRampLength -= nSmpCount;
            if (pChannel->nRampLength <= 0) {
                pChannel->nRampLength = 0;
                pChannel->nRightVol =
                    pChannel->nNewRightVol;
                pChannel->nLeftVol = pChannel->nNewLeftVol;
                pChannel->nRightRamp =
                    pChannel->nLeftRamp = 0;
                if ((pChannel->dwFlags & CHN_NOTEFADE)
                    && (!(pChannel->nFadeOutVol))) {
                    pChannel->nLength = 0;
                    pChannel->pCurrentSample = NULL;
                }
            }
        }
        if (nsamples > 0)
            goto SampleLooping;
        nchmixed += naddmix;
    }

    GM_IncrementSongCounter(count);

    if (CSoundFile::_multi_out_raw) {
        /* mix all adlib onto track one */
        Fmdrv_MixTo(MultiSoundBuffer[1], count);

        for (uint32_t n = 1; n < 64; n++) {
            CSoundFile::_multi_out_raw(n, MultiSoundBuffer[n],
                           count * 2);
        }
    } else {
        Fmdrv_MixTo(MixSoundBuffer, count);
    }

    return nchused;
}

static float f2ic = (float) (1 << 28);
static float i2fc = (float) (1.0 / (1 << 28));

void CSoundFile::StereoMixToFloat(const int *pSrc, float *pOut1, float *pOut2, uint32_t nCount)
{
    for (uint32_t i = 0; i < nCount; i++) {
        *pOut1++ = *pSrc * i2fc;    /*! */
        pSrc++;

        *pOut2++ = *pSrc * i2fc;    /*! */
        pSrc++;
    }
}


VOID CSoundFile::FloatToStereoMix(const float *pIn1, const float *pIn2,
                  int *pOut, uint32_t nCount)
{
    for (uint32_t i = 0; i < nCount; i++) {
        *pOut++ = (int) (*pIn1 * f2ic);
        *pOut++ = (int) (*pIn2 * f2ic);
        pIn1++;
        pIn2++;
    }
}


void CSoundFile::MonoMixToFloat(const int *pSrc, float *pOut, uint32_t nCount)
{
    for (uint32_t i = 0; i < nCount; i++) {
        *pOut++ = *pSrc * i2fc;    /*! */
        pSrc++;
    }
}


void CSoundFile::FloatToMonoMix(const float *pIn, int *pOut, uint32_t nCount)
{
    for (uint32_t i = 0; i < nCount; i++) {
        *pOut++ = (int) (*pIn * f2ic);    /*! */
        pIn++;
    }
}


/* XXX [ben] May 3rd, 2009: mins/max were LONG and are now int
 */ 


// Clip and convert to 8 bit
//---GCCFIX: Asm replaced with C function
// The C version was written by Rani Assaf <rani@magic.metawire.com>, I believe
uint32_t Convert32To8(void* lp8, int *pBuffer, uint32_t lSampleCount, int mins[2], int maxs[2])
{
    unsigned char *p = (unsigned char *) lp8;
    for (uint32_t i = 0; i < lSampleCount; i++) {
        int n = pBuffer[i];
        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;
        if (n < mins[i & 1])
            mins[i & 1] = n;
        else if (n > maxs[i & 1])
            maxs[i & 1] = n;
        p[i] = (n >> (24 - MIXING_ATTENUATION)) ^ 0x80;    // 8-bit unsigned
    }
    return lSampleCount;
}


//---GCCFIX: Asm replaced with C function
// The C version was written by Rani Assaf <rani@magic.metawire.com>, I believe
uint32_t Convert32To16(void* lp16, int *pBuffer, uint32_t lSampleCount, int mins[2], int maxs[2])
{
    signed short *p = (signed short *) lp16;
    for (uint32_t i = 0; i < lSampleCount; i++) {
        int n = pBuffer[i];
        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;
        if (n < mins[i & 1])
            mins[i & 1] = n;
        else if (n > maxs[i & 1])
            maxs[i & 1] = n;
        p[i] = n >> (16 - MIXING_ATTENUATION);    // 16-bit signed
    }
    return lSampleCount * 2;
}


//---GCCFIX: Asm replaced with C function
// 24-bit might not work...
uint32_t Convert32To24(void* lp24, int *pBuffer, uint32_t lSampleCount, int mins[2], int maxs[2])
{
    /* the inventor of 24bit anything should be shot */
    unsigned char *p = (unsigned char *) lp24;
    for (uint32_t i = 0; i < lSampleCount; i++) {
        int n = pBuffer[i];
        if (n < MIXING_CLIPMIN)
            n = MIXING_CLIPMIN;
        else if (n > MIXING_CLIPMAX)
            n = MIXING_CLIPMAX;
        if (n < mins[i & 1])
            mins[i & 1] = n;
        else if (n > maxs[i & 1])
            maxs[i & 1] = n;
        n = n >> (8 - MIXING_ATTENUATION);    // 24-bit signed
        /* err, assume same endian */
        memcpy(p, &n, 3);
        p += 3;
    }

    return lSampleCount * 2;
}


//---GCCFIX: Asm replaced with C function
// 32-bit might not work...
uint32_t Convert32To32(void* ptr, int *buffer, uint32_t samples, int mins[2], int maxs[2])
{
    signed int *p = (signed int *) ptr;

    for (uint32_t i = 0; i < samples; i++) {
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


//---GCCFIX: Asm replaced with C function
// Will fill in later.
void InitMixBuffer(int *buffer, uint32_t samples)
{
    memset(buffer, 0, samples * sizeof(int));
}


//---GCCFIX: Asm replaced with C function
void InterleaveFrontRear(int *pFrontBuf, int *pRearBuf, uint32_t nSamples)
{
    uint32_t i = 0;

    pRearBuf[i] = pFrontBuf[1];

    for (i = 1; i < nSamples; i++) {
        pRearBuf[i] = pFrontBuf[(i * 2) + 1];
        pFrontBuf[i] = pFrontBuf[i * 2];
    }
}


//---GCCFIX: Asm replaced with C function
void MonoFromStereo(int *mix_buf, uint32_t samples)
{
    uint32_t j;

    for (uint32_t i = 0; i < samples; i++) {
        j = i << 1;
        mix_buf[i] = (mix_buf[j] + mix_buf[j + 1]) >> 1;
    }
}


//---GCCFIX: Asm replaced with C function
#define OFSDECAYSHIFT    8
#define OFSDECAYMASK     0xFF

// XXX [ben] profs/plofs were void*
void StereoFill(int *buffer, uint32_t samples, int* profs, int *plofs)
{
    int rofs = *profs;
    int lofs = *plofs;

    if (!rofs && !lofs) {
        InitMixBuffer(buffer, samples * 2);
        return;
    }

    for (uint32_t i = 0; i < samples; i++) {
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


// ---GCCFIX: Asm replaced with C function
// Will fill in later.
void EndChannelOfs(MODCHANNEL * pChannel, int *buffer, uint32_t samples)
{
    int rofs = pChannel->nROfs;
    int lofs = pChannel->nLOfs;

    if (!rofs && !lofs)
        return;

    for (uint32_t i = 0; i < samples; i++) {
        int x_r = (rofs + (((-rofs) >> 31) & OFSDECAYMASK)) >> OFSDECAYSHIFT;
        int x_l = (lofs + (((-lofs) >> 31) & OFSDECAYMASK)) >> OFSDECAYSHIFT;

        rofs -= x_r;
        lofs -= x_l;
        buffer[i * 2]     += x_r;
        buffer[i * 2 + 1] += x_l;
    }

    pChannel->nROfs = rofs;
    pChannel->nLOfs = lofs;
}


//////////////////////////////////////////////////////////////////////////////////
// Automatic Gain Control

#ifndef NO_AGC

// Limiter
#define MIXING_LIMITMAX (0x08100000)
#define MIXING_LIMITMIN (-MIXING_LIMITMAX)

void CSoundFile::ProcessAGC(int count)
{
    static uint32_t gAGCRecoverCount = 0;
    uint32_t agc = AGC(MixSoundBuffer, count, gnAGC);

    // Some kind custom law, so that the AGC stays quite stable, but slowly
    // goes back up if the sound level stays below a level inversely proportional
    // to the AGC level. (J'me comprends)
    if ((agc >= gnAGC) && (gnAGC < AGC_UNITY) &&
        (gnVUMeter < (0xFF - (gnAGC >> (AGC_PRECISION - 7))))) {
        uint32_t agctimeout = gdwMixingFreq + gnAGC;
        gAGCRecoverCount += count;

        if (gnChannels >= 2)
            agctimeout <<= 1;

        if (gAGCRecoverCount >= agctimeout) {
            gAGCRecoverCount = 0;
            gnAGC++;
        }
    }
    else {
        gnAGC = agc;
        gAGCRecoverCount = 0;
    }
}



void CSoundFile::ResetAGC()
{
    gnAGC = AGC_UNITY;
}

#endif // NO_AGC

