#ifndef _MIXER_H
#define _MIXER_H

#define VOLUME_MAX 100

/* I don't have much use for anything other than the master volume at the
 * moment... maybe in the future, I'll add an option to control the PCM
 * volume instead. */
void mixer_read_master_volume(int *left, int *right);
void mixer_write_master_volume(int left, int right);

#endif /* ! _MIXER_H */
