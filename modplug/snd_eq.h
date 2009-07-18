#ifndef MODPLUG_SND_EQ_H
#define MODPLUG_SND_EQ_H
#ifdef __cplusplus
extern "C" {
#endif

void eq_mono(int *, unsigned int);
void eq_stereo(int *, unsigned int);
void initialize_eq(int, float);
void set_eq_gains(const unsigned int *, unsigned int, const unsigned int *, int, int);


#ifdef __cplusplus
}
#endif
#endif

