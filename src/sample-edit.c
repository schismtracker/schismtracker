#include "headers.h"

#include <SDL.h>

#include "util.h"
#include "song.h"
#include "sample-edit.h"

/* --------------------------------------------------------------------- */

static inline void _sign_convert_8(signed char *data, unsigned long length)
{
        unsigned long pos = length;

        while (pos) {
                pos--;
                data[pos] ^= 128;
        }
}

static inline void _sign_convert_16(signed short *data,
                                    unsigned long length)
{
        unsigned long pos = length;

        while (pos) {
                pos--;
                data[pos] ^= 32768;
        }
}

void sample_sign_convert(song_sample * sample)
{
        if (sample->flags & SAMP_16_BIT)
                _sign_convert_16((signed short *) sample->data,
                                 sample->length);
        else
                _sign_convert_8(sample->data, sample->length);
}

/* --------------------------------------------------------------------- */
/* useful for importing delta-encoded raw data */

static inline void _delta_decode_8(signed char *data, unsigned long length)
{
        unsigned long pos;
        signed char o = 0, n;

        for (pos = 1; pos < length; pos++) {
                n = data[pos] + o;
                data[pos] = n;
                o = n;
        }
}

static inline void _delta_decode_16(signed short *data,
                                    unsigned long length)
{
        unsigned long pos;
        signed short o = 0, n;

        for (pos = 1; pos < length; pos++) {
                n = data[pos] + o;
                data[pos] = n;
                o = n;
        }
}

void sample_delta_decode(song_sample * sample)
{
        if (sample->flags & SAMP_16_BIT)
                _delta_decode_16((signed short *) sample->data,
                                 sample->length);
        else
                _delta_decode_8(sample->data, sample->length);
}

/* --------------------------------------------------------------------- */
/* i don't think this is being done correctly :/ */

static inline void _reverse_8(signed char *data, unsigned long length)
{
        signed char tmp;
        unsigned long lpos = 0, rpos = length - 1;

        while (lpos < rpos) {
                tmp = data[lpos];
                data[lpos] = data[rpos];
                data[rpos] = tmp;
                lpos++;
                rpos--;
        }
}

static inline void _reverse_16(signed short *data, unsigned long length)
{
        signed short tmp;
        unsigned long lpos = 0, rpos = length - 1;

        while (lpos < rpos) {
                tmp = data[lpos];
                data[lpos] = data[rpos];
                data[rpos] = tmp;
                lpos++;
                rpos--;
        }
}

void sample_reverse(song_sample * sample)
{
        unsigned long tmp;

        if (sample->flags & SAMP_16_BIT)
                _reverse_16((signed short *) sample->data, sample->length);
        else
                _reverse_8(sample->data, sample->length);

        tmp = sample->length - sample->loop_start;
        sample->loop_start = sample->length - sample->loop_end;
        sample->loop_end = tmp;

        tmp = sample->length - sample->sustain_start;
        sample->sustain_start = sample->length - sample->sustain_end;
        sample->sustain_end = tmp;
}

/* --------------------------------------------------------------------- */

/* if convert_data is nonzero, the sample data is modified (so it sounds
 * the same); otherwise, the sample length is changed and the data is
 * left untouched.
 * this is irrelevant, as i haven't gotten to writing the convert stuff
 * yet. (not that it's hard, i just haven't gotten to it.) */

void sample_toggle_quality(song_sample * sample, int convert_data)
{
        if (convert_data == 0) {
                sample->flags ^= SAMP_16_BIT;

                if (sample->flags & SAMP_16_BIT) {
                        sample->length >>= 1;
                        sample->loop_start >>= 1;
                        sample->loop_end >>= 1;
                        sample->sustain_start >>= 1;
                        sample->sustain_end >>= 1;
                } else {
                        sample->length <<= 1;
                        sample->loop_start <<= 1;
                        sample->loop_end <<= 1;
                        sample->sustain_start <<= 1;
                        sample->sustain_end <<= 1;
                }
        } else {
                printf("arr! convert!\n");
        }
}

/* --------------------------------------------------------------------- */

static inline void _centralise_8(signed char *data, unsigned long length)
{
        unsigned long pos = length;
        signed char min = 127, max = -128;
        int offset;

        while (pos) {
                pos--;
                if (data[pos] < min)
                        min = data[pos];
                else if (data[pos] > max)
                        max = data[pos];
        }

        offset = (max + min + 1) >> 1;
        if (offset == 0)
                return;

        pos = length;
        while (pos) {
                pos--;
                data[pos] -= offset;
        }
}

static inline void _centralise_16(signed short *data, unsigned long length)
{
        unsigned long pos = length;
        signed short min = 32767, max = -32768;
        int offset;

        while (pos) {
                pos--;
                if (data[pos] < min)
                        min = data[pos];
                else if (data[pos] > max)
                        max = data[pos];
        }

        offset = (max + min + 1) >> 1;
        if (offset == 0)
                return;

        pos = length;
        while (pos) {
                pos--;
                data[pos] -= offset;
        }
}

void sample_centralise(song_sample * sample)
{
        if (sample->flags & SAMP_16_BIT)
                _centralise_16((signed short *) sample->data,
                               sample->length);
        else
                _centralise_8(sample->data, sample->length);
}

/* --------------------------------------------------------------------- */
/* surround flipping (probably useless with the S91 effect, but why not) */

static inline void _invert_8(signed char *data, unsigned long length)
{
        unsigned long pos = length;

        while (pos) {
                pos--;
                data[pos] = ~data[pos];
        }
}

static inline void _invert_16(signed short *data, unsigned long length)
{
        unsigned long pos = length;

        while (pos) {
                pos--;
                data[pos] = ~data[pos];
        }
}

void sample_invert(song_sample * sample)
{
        if (sample->flags & SAMP_16_BIT)
                _invert_16((signed short *) sample->data, sample->length);
        else
                _invert_8(sample->data, sample->length);
}
