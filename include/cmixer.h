#ifndef MODPLUG_MIXER_H
#define MODPLUG_MIXER_H

#include "sndfile.h"

// Stuff moved from sndfile.h
#define MIXING_ATTENUATION      5
#define MIXING_CLIPMIN          (-0x04000000)
#define MIXING_CLIPMAX          (0x03FFFFFF)
#define VOLUMERAMPPRECISION     12
#define FILTERPRECISION         13


void init_mix_buffer(int *, unsigned int);
void stereo_fill(int *, unsigned int, int*, int *);
void end_channel_ofs(song_voice_t *, int *, unsigned int);
void interleave_front_rear(int *, int *, unsigned int);
void mono_from_stereo(int *, unsigned int);

unsigned int csf_create_stereo_mix(song_t *csf, int count);

void setup_channel_filter(song_voice_t *pChn, int reset, int flt_modifier, int freq);


//typedef unsigned int (*convert_clip_t)(void *, int *, unsigned int, int*, int*) __attribute__((cdecl))

unsigned int clip_32_to_8(void *, int *, unsigned int, int *, int *);
unsigned int clip_32_to_16(void *, int *, unsigned int, int *, int *);
unsigned int clip_32_to_24(void *, int *, unsigned int, int *, int *);
unsigned int clip_32_to_32(void *, int *, unsigned int, int *, int *);


void normalize_mono(song_t *, int *, unsigned int);
void normalize_stereo(song_t *, int *, unsigned int);
void eq_mono(song_t *, int *, unsigned int);
void eq_stereo(song_t *, int *, unsigned int);
void initialize_eq(int, float);
void set_eq_gains(const unsigned int *, unsigned int, const unsigned int *, int, int);


// sndmix.c
extern int g_dry_rofs_vol;
extern int g_dry_lofs_vol;


// mixer.c
void ResampleMono8BitFirFilter(signed char *oldbuf, signed char *newbuf, unsigned long oldlen, unsigned long newlen);
void ResampleMono16BitFirFilter(signed short *oldbuf, signed short *newbuf, unsigned long oldlen, unsigned long newlen);
void ResampleStereo8BitFirFilter(signed char *oldbuf, signed char *newbuf, unsigned long oldlen, unsigned long newlen);
void ResampleStereo16BitFirFilter(signed short *oldbuf, signed short *newbuf, unsigned long oldlen, unsigned long newlen);

#endif

