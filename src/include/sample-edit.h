#ifndef _SAMPLE_EDIT_H
#define _SAMPLE_EDIT_H

void sample_sign_convert(song_sample * sample);
void sample_reverse(song_sample * sample);
void sample_centralise(song_sample * sample);

/* if convert_data is nonzero, the sample data is modified (so it sounds
 * the same); otherwise, the sample length is changed and the data is
 * left untouched (so 16 bit samples converted to 8 bit end up sounding
 * like junk, and 8 bit samples converted to 16 bit end up with 2x the
 * pitch) */
void sample_toggle_quality(song_sample * sample, int convert_data);

/* Impulse Tracker doesn't do these. */
void sample_invert(song_sample * sample);
void sample_delta_decode(song_sample * sample);

#endif /* ! _SAMPLE_EDIT_H */
