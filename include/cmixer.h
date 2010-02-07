#ifndef MODPLUG_MIXER_H
#define MODPLUG_MIXER_H

#include "sndfile.h"

// Stuff moved from sndfile.h
#define MIXBUFFERSIZE           512
#define MIXING_ATTENUATION      5
#define MIXING_CLIPMIN          (-0x04000000)
#define MIXING_CLIPMAX          (0x03FFFFFF)
#define VOLUMERAMPPRECISION     12


void init_mix_buffer(int *, unsigned int);
void stereo_fill(int *, unsigned int, int*, int *);
void end_channel_ofs(song_voice_t *, int *, unsigned int);
void interleave_front_rear(int *, int *, unsigned int);
void mono_from_stereo(int *, unsigned int);

void stereo_mix_to_float(const int *, float *, float *, unsigned int);
void float_to_stereo_mix(const float *, const float *, int *, unsigned int);
void mono_mix_to_float(const int *, float *, unsigned int);
void float_to_mono_mix(const float *, int *, unsigned int);

unsigned int csf_create_stereo_mix(song_t *csf, int count);

void setup_channel_filter(song_voice_t *pChn, int reset, int flt_modifier, int freq);


//typedef unsigned int (*convert_clip_t)(void *, int *, unsigned int, int*, int*) __attribute__((cdecl))

unsigned int clip_32_to_8(void *, int *, unsigned int, int *, int *);
unsigned int clip_32_to_16(void *, int *, unsigned int, int *, int *);
unsigned int clip_32_to_24(void *, int *, unsigned int, int *, int *);
unsigned int clip_32_to_32(void *, int *, unsigned int, int *, int *);


void eq_mono(int *, unsigned int);
void eq_stereo(int *, unsigned int);
void initialize_eq(int, float);
void set_eq_gains(const unsigned int *, unsigned int, const unsigned int *, int, int);




// Some things that shouldn't exist...

// fastmix.c
extern int mix_buffer[MIXBUFFERSIZE * 4];
extern int MixRearBuffer[MIXBUFFERSIZE * 2];
extern float mix_buffer_float[MIXBUFFERSIZE * 2];
extern int mix_buffer_multi[64][MIXBUFFERSIZE * 4];

// sndmix.c
extern int g_dry_rofs_vol;
extern int g_dry_lofs_vol;

#endif

