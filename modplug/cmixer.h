#ifndef MODPLUG_MIXER_H
#define MODPLUG_MIXER_H

#ifdef __cplusplus
extern "C" {
#endif

void init_mix_buffer(int *, unsigned int);
void stereo_fill(int *, unsigned int, int*, int *);
void end_channel_ofs(MODCHANNEL *, int *, unsigned int);
void interleave_front_rear(int *, int *, unsigned int);
void mono_from_stereo(int *, unsigned int);

void stereo_mix_to_float(const int *, float *, float *, unsigned int);
void float_to_stereo_mix(const float *, const float *, int *, unsigned int);
void mono_mix_to_float(const int *, float *, unsigned int);
void float_to_mono_mix(const float *, int *, unsigned int);


//typedef unsigned int (*convert_clip_t)(void *, int *, unsigned int, int*, int*) __attribute__((cdecl))

unsigned int clip_32_to_8(void *, int *, unsigned int, int *, int *);
unsigned int clip_32_to_16(void *, int *, unsigned int, int *, int *);
unsigned int clip_32_to_24(void *, int *, unsigned int, int *, int *);
unsigned int clip_32_to_32(void *, int *, unsigned int, int *, int *);


#ifdef __cplusplus
}
#endif

#endif

