#ifndef MODPLUG_SND_FX_H
#define MODPLUG_SND_FX_H

#ifdef __cplusplus
extern "C" {
#endif

int get_note_from_period(int period);
int get_period_from_note(int note, unsigned int c5speed, int linear);
unsigned int get_freq_from_period(int period, unsigned int c5speed, int frac, int linear);

unsigned int transpose_to_frequency(int transp, int ftune);
int frequency_to_transpose(unsigned int freq);

#ifdef __cplusplus
}
#endif

#endif

