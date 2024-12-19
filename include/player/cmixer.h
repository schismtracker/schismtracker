#ifndef SCHISM_PLAYER_CMIXER_H_
#define SCHISM_PLAYER_CMIXER_H_

#include "player/sndfile.h"

// Stuff moved from sndfile.h

#define MIXING_ATTENUATION      5
#define MIXING_CLIPMIN          (-0x04000000)
#define MIXING_CLIPMAX          (0x03FFFFFF)
#define VOLUMERAMPPRECISION     12
#define FILTERPRECISION         ((sizeof(int32_t) * 8) - 8) /* faithfully stolen from openmpt */

void init_mix_buffer(int32_t *, uint32_t);
void stereo_fill(int32_t *, uint32_t, int32_t *, int32_t *);
void end_channel_ofs(song_voice_t *, int32_t *, uint32_t);
void interleave_front_rear(int32_t *, int32_t *, uint32_t);
void mono_from_stereo(int32_t *, uint32_t);

uint32_t csf_create_stereo_mix(song_t *csf, uint32_t count);

void setup_channel_filter(song_voice_t *pChn, int32_t reset, int32_t flt_modifier, int32_t freq);


//typedef unsigned int (*convert_clip_t)(void *, int *, unsigned int, int*, int*) __attribute__((cdecl))

uint32_t clip_32_to_8(void *, int32_t *, uint32_t, int32_t *, int32_t *);
uint32_t clip_32_to_16(void *, int32_t *, uint32_t, int32_t *, int32_t *);
uint32_t clip_32_to_24(void *, int32_t *, uint32_t, int32_t *, int32_t *);
uint32_t clip_32_to_32(void *, int32_t *, uint32_t, int32_t *, int32_t *);


void normalize_mono(song_t *, int32_t *, uint32_t);
void normalize_stereo(song_t *, int32_t *, uint32_t);
void eq_mono(song_t *, int32_t *, uint32_t);
void eq_stereo(song_t *, int32_t *, uint32_t);
void initialize_eq(int32_t, float);
void set_eq_gains(const uint32_t *, uint32_t, const uint32_t *, int32_t, int32_t);

// sndmix.c
extern int32_t g_dry_rofs_vol;
extern int32_t g_dry_lofs_vol;


// mixer.c
void ResampleMono8BitFirFilter(signed char *oldbuf, signed char *newbuf, uint32_t oldlen, uint32_t newlen);
void ResampleMono16BitFirFilter(signed short *oldbuf, signed short *newbuf, uint32_t oldlen, uint32_t newlen);
void ResampleStereo8BitFirFilter(signed char *oldbuf, signed char *newbuf, uint32_t oldlen, uint32_t newlen);
void ResampleStereo16BitFirFilter(signed short *oldbuf, signed short *newbuf, uint32_t oldlen, uint32_t newlen);

#endif /* SCHISM_PLAYER_CMIXER_H_ */

