/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef SCHISM_SAMPLE_EDIT_H_
#define SCHISM_SAMPLE_EDIT_H_

#include "headers.h"

void sample_sign_convert(song_sample_t * sample);
void sample_reverse(song_sample_t * sample);
void sample_centralise(song_sample_t * sample);
void sample_downmix(song_sample_t * sample);
void sample_amplify(song_sample_t *sample, int32_t percent);
/* Return the maximum amplification that can be done without clipping (as a
 * percentage, suitable to pass to sample_amplify). */
int32_t sample_get_amplify_amount(song_sample_t *sample);

/* if convert_data is nonzero, the sample data is modified (so it sounds
 * the same); otherwise, the sample length is changed and the data is
 * left untouched (so 16 bit samples converted to 8 bit end up sounding
 * like junk, and 8 bit samples converted to 16 bit end up with 2x the
 * pitch) */
void sample_toggle_quality(song_sample_t * sample, int convert_data);

/* resize a sample; if aa is set, attempt to antialias (resample) the
 * output waveform.
 */
void sample_resize(song_sample_t * sample, uint32_t newlen, int aa);

/* AFAIK, this was in some registered versions of IT */
void sample_invert(song_sample_t * sample);

/* Impulse Tracker doesn't do these. */
void sample_delta_decode(song_sample_t * sample);

void sample_mono_left(song_sample_t * sample);
void sample_mono_right(song_sample_t * sample);


#endif /* SCHISM_SAMPLE_EDIT_H_ */
