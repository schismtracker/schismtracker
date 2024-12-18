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

#include "headers.h"

#include <math.h>
#include <stdint.h>
#include <assert.h>
#include <time.h>

#include "bswap.h"
#include "bshift.h"
#include "player/sndfile.h"
#include "log.h"
#include "util.h"
#include "ieee-float.h"
#include "fmt.h" // for it_decompress8 / it_decompress16


static void _csf_reset(song_t *csf)
{
	unsigned int i;

	csf->flags = 0;
	csf->pan_separation = 128;
	csf->num_voices = 0;
	csf->freq_factor = csf->tempo_factor = 128;
	csf->initial_global_volume = 128;
	csf->current_global_volume = 128;
	csf->initial_speed = 6;
	csf->initial_tempo = 125;
	csf->process_row = 0;
	csf->row = 0;
	csf->current_pattern = 0;
	csf->current_order = 0;
	csf->process_order = 0;
	csf->mixing_volume = 0x30;
	memset(csf->message, 0, sizeof(csf->message));

	csf->row_highlight_major = 16;
	csf->row_highlight_minor = 4;

	/* This is intentionally crappy quality, so that it's very obvious if it didn't get initialized */
	csf->mix_flags = 0;
	csf->mix_frequency = 4000;
	csf->mix_bits_per_sample = 8;
	csf->mix_channels = 1;

	memset(csf->voices, 0, sizeof(csf->voices));
	memset(csf->voice_mix, 0, sizeof(csf->voice_mix));
	memset(csf->samples, 0, sizeof(csf->samples));
	memset(csf->instruments, 0, sizeof(csf->instruments));
	memset(csf->orderlist, 0xFF, sizeof(csf->orderlist));
	memset(csf->patterns, 0, sizeof(csf->patterns));

	csf_reset_midi_cfg(csf);
	csf_forget_history(csf);

	for (i = 0; i < MAX_PATTERNS; i++) {
		csf->pattern_size[i] = 64;
		csf->pattern_alloc_size[i] = 64;
	}
	for (i = 0; i < MAX_SAMPLES; i++) {
		csf->samples[i].c5speed = 8363;
		csf->samples[i].volume = 64 * 4;
		csf->samples[i].global_volume = 64;
	}
	for (i = 0; i < MAX_CHANNELS; i++) {
		csf->channels[i].panning = 128;
		csf->channels[i].volume = 64;
		csf->channels[i].flags = 0;
	}
}

//////////////////////////////////////////////////////////
// song_t

song_t *csf_allocate(void)
{
	song_t *csf = mem_calloc(1, sizeof(song_t));
	_csf_reset(csf);
	return csf;
}

void csf_free(song_t *csf)
{
	if (csf) {
		csf_destroy(csf);
		free(csf);
	}
}


static void _init_envelope(song_envelope_t *env, int n)
{
	env->nodes = 2;
	env->ticks[0] = 0;
	env->ticks[1] = 100;
	env->values[0] = n;
	env->values[1] = n;
}

void csf_init_instrument(song_instrument_t *ins, int samp)
{
	int n;

	memset(ins, 0, sizeof(*ins));
	_init_envelope(&ins->vol_env, 64);
	_init_envelope(&ins->pan_env, 32);
	_init_envelope(&ins->pitch_env, 32);
	ins->global_volume = 128;
	ins->panning = 128;
	ins->midi_bank = -1;
	ins->midi_program = -1;
	ins->pitch_pan_center = 60; // why does pitch/pan not use the same note values as everywhere else?!
	for (n = 0; n < 128; n++) {
		ins->sample_map[n] = samp;
		ins->note_map[n] = n + 1;
	}
}

song_instrument_t *csf_allocate_instrument(void)
{
	song_instrument_t *ins = mem_alloc(sizeof(song_instrument_t));
	csf_init_instrument(ins, 0);
	return ins;
}

void csf_free_instrument(song_instrument_t *i)
{
	free(i);
}


void csf_destroy(song_t *csf)
{
	int i;

	for (i = 0; i < MAX_PATTERNS; i++) {
		if (csf->patterns[i]) {
			csf_free_pattern(csf->patterns[i]);
			csf->patterns[i] = NULL;
		}
	}
	for (i = 1; i < MAX_SAMPLES; i++) {
		song_sample_t *pins = &csf->samples[i];
		if (pins->data) {
			csf_free_sample(pins->data);
			pins->data = NULL;
		}
	}
	for (i = 0; i < MAX_INSTRUMENTS; i++) {
		if (csf->instruments[i]) {
			csf_free_instrument(csf->instruments[i]);
			csf->instruments[i] = NULL;
		}
	}

	_csf_reset(csf);
}

song_note_t *csf_allocate_pattern(uint32_t rows)
{
	return mem_calloc(rows * MAX_CHANNELS, sizeof(song_note_t));
}

void csf_free_pattern(void *pat)
{
	free(pat);
}

#define CSF_ALLOCATE_PREPEND ((MAX_SAMPLING_POINT_SIZE) * (MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE))
#define CSF_ALLOCATE_APPEND ((1 + 4 + 4) * MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE * 4)

signed char *csf_allocate_sample(uint32_t nbytes)
{
	return (signed char*)mem_calloc(1, nbytes + CSF_ALLOCATE_PREPEND + CSF_ALLOCATE_APPEND) + CSF_ALLOCATE_PREPEND;
}

void csf_free_sample(void *p)
{
	if (p)
		free((signed char*)p - CSF_ALLOCATE_PREPEND);
}

#undef CSF_ALLOCATE_PREPEND
#undef CSF_ALLOCATE_APPEND

void csf_forget_history(song_t *csf)
{
	free(csf->history);
	csf->history = NULL;
	csf->histlen = 0;
	csf->editstart.runtime = timer_ticks();

	time_t thetime = time(NULL);
	localtime_r(&thetime, &csf->editstart.time);
}

/* --------------------------------------------------------------------------------------------------------- */
/* Counting and checking stuff. */

static int name_is_blank(char *name)
{
	int n;
	for (n = 0; n < 25; n++) {
		if (name[n] != '\0' && name[n] != ' ')
			return 0;
	}
	return 1;
}

const song_note_t blank_pattern[64 * 64];
const song_note_t *blank_note = blank_pattern; // Same thing, really.

int csf_note_is_empty(song_note_t *note)
{
	return !memcmp(note, blank_pattern, sizeof(song_note_t));
}

int csf_pattern_is_empty(song_t *csf, int n)
{
	if (!csf->patterns[n])
		return 1;
	if (csf->pattern_size[n] != 64)
		return 0;
	return !memcmp(csf->patterns[n], blank_pattern, sizeof(blank_pattern));
}

int csf_sample_is_empty(song_sample_t *smp)
{
	return (smp->data == NULL
		&& name_is_blank(smp->name)
		&& smp->filename[0] == '\0'
		&& smp->c5speed == 8363
		&& smp->volume == 64*4 //mphack
		&& smp->global_volume == 64
		&& smp->panning == 0
		&& !(smp->flags & (CHN_LOOP | CHN_SUSTAINLOOP | CHN_PANNING))
		&& smp->length == 0
		&& smp->loop_start == 0
		&& smp->loop_end == 0
		&& smp->sustain_start == 0
		&& smp->sustain_end == 0
		&& smp->vib_type == VIB_SINE
		&& smp->vib_rate == 0
		&& smp->vib_depth == 0
		&& smp->vib_speed == 0
	);
}

static int env_is_blank(song_envelope_t *env, int value)
{
	return (env->nodes == 2
		&& env->loop_start == 0
		&& env->loop_end == 0
		&& env->sustain_start == 0
		&& env->sustain_end == 0
		&& env->ticks[0] == 0
		&& env->ticks[1] == 100
		&& env->values[0] == value
		&& env->values[1] == value
	);
}

int csf_instrument_is_empty(song_instrument_t *ins)
{
	int n;
	if (!ins)
		return 1;

	for (n = 0; n < NOTE_LAST - NOTE_FIRST; n++) {
		if (ins->sample_map[n] != 0 || ins->note_map[n] != (n + NOTE_FIRST))
			return 0;
	}
	return (name_is_blank(ins->name)
		&& ins->filename[0] == '\0'
		&& ins->flags == 0 /* No envelopes, loop points, panning, or carry flags set */
		&& ins->nna == NNA_NOTECUT
		&& ins->dct == DCT_NONE
		&& ins->dca == DCA_NOTECUT
		&& env_is_blank(&ins->vol_env, 64)
		&& ins->global_volume == 128
		&& ins->fadeout == 0
		&& ins->vol_swing == 0
		&& env_is_blank(&ins->pan_env, 32)
		&& ins->panning == 32*4 //mphack
		&& ins->pitch_pan_center == 60 // C-5 (blah)
		&& ins->pitch_pan_separation == 0
		&& ins->pan_swing == 0
		&& env_is_blank(&ins->pitch_env, 32)
		&& ins->ifc == 0
		&& ins->ifr == 0
		&& ins->midi_channel_mask == 0
		&& ins->midi_program == -1
		&& ins->midi_bank == -1
	);
}

// IT-compatible: last order of "main song", or 0
int csf_last_order(song_t *csf)
{
	int n = 0;
	while (n < MAX_ORDERS && csf->orderlist[n] != ORDER_LAST)
		n++;
	return n ? n - 1 : 0;
}

// Total count of orders in orderlist before end of data
int csf_get_num_orders(song_t *csf)
{
	int n = MAX_ORDERS;
	while (n >= 0 && csf->orderlist[--n] == ORDER_LAST) {
	}
	return n + 1;
}

// Total number of non-empty patterns in song, according to csf_pattern_is_empty
int csf_get_num_patterns(song_t *csf)
{
	int n = MAX_PATTERNS - 1;
	while (n && csf_pattern_is_empty(csf, n))
		n--;
	return n+ 1;
}

int csf_get_num_samples(song_t *csf)
{
	int n = MAX_SAMPLES - 1;
	while (n > 0 && csf_sample_is_empty(csf->samples + n))
		n--;
	return n;
}

int csf_get_num_instruments(song_t *csf)
{
	int n = MAX_INSTRUMENTS - 1;
	while (n > 0 && csf_instrument_is_empty(csf->instruments[n]))
		n--;
	return n;
}


int csf_first_blank_sample(song_t *csf, int start)
{
	int n;
	for (n = MAX(start, 1); n < MAX_SAMPLES; n++) {
		if (csf_sample_is_empty(csf->samples + n))
			return n;
	}
	return -1;
}

int csf_first_blank_instrument(song_t *csf, int start)
{
	int n;
	for (n = MAX(start, 1); n < MAX_INSTRUMENTS; n++) {
		if (csf_instrument_is_empty(csf->instruments[n]))
			return n;
	}
	return -1;
}


// FIXME this function sucks
int csf_get_highest_used_channel(song_t *csf)
{
	int highchan = 0, ipat, j, jmax;
	song_note_t *p;

	for (ipat = 0; ipat < MAX_PATTERNS; ipat++) {
		p = csf->patterns[ipat];
		if (!p)
			continue;
		jmax = csf->pattern_size[ipat] * MAX_CHANNELS;
		for (j = 0; j < jmax; j++, p++) {
			if (NOTE_IS_NOTE(p->note)) {
				if ((j % MAX_CHANNELS) > highchan)
					highchan = j % MAX_CHANNELS;
			}
		}
	}

	return highchan;
}

//////////////////////////////////////////////////////////////////////////
// Misc functions

midi_config_t default_midi_config;


void csf_reset_midi_cfg(song_t *csf)
{
	memcpy(&csf->midi_config, &default_midi_config, sizeof(default_midi_config));
}

void csf_copy_midi_cfg(song_t *dest, song_t *src)
{
	memcpy(&dest->midi_config, &src->midi_config, sizeof(midi_config_t));
}


int csf_set_wave_config(song_t *csf, uint32_t rate,uint32_t bits,uint32_t channels)
{
	int reset = ((csf->mix_frequency != rate)
		     || (csf->mix_bits_per_sample != bits)
		     || (csf->mix_channels != channels));
	csf->mix_channels = channels;
	csf->mix_frequency = rate;
	csf->mix_bits_per_sample = bits;
	csf_init_player(csf, reset);
	return 1;
}


int csf_set_resampling_mode(song_t *csf, uint32_t mode)
{
	uint32_t d = csf->mix_flags & ~(SNDMIX_NORESAMPLING|SNDMIX_HQRESAMPLER|SNDMIX_ULTRAHQSRCMODE);
	switch(mode) {
		case SRCMODE_NEAREST:   d |= SNDMIX_NORESAMPLING; break;
		case SRCMODE_LINEAR:    break;
		case SRCMODE_SPLINE:    d |= SNDMIX_HQRESAMPLER; break;
		case SRCMODE_POLYPHASE: d |= (SNDMIX_HQRESAMPLER|SNDMIX_ULTRAHQSRCMODE); break;
		default:                return 0;
	}
	csf->mix_flags = d;
	return 1;
}


// This used to use some retarded positioning based on the total number of rows elapsed, which is useless.
// However, the only code calling this function is in this file, to set it to the start, so I'm optimizing
// out the row count.
static void set_current_pos_0(song_t *csf)
{
	song_voice_t *v = csf->voices;
	for (uint32_t i = 0; i < MAX_VOICES; i++, v++) {
		memset(v, 0, sizeof(*v));
		v->note = v->new_note = 1;
		v->cutoff = 0x7F;
		v->volume = 256;
		if (i < MAX_CHANNELS) {
			v->panning = csf->channels[i].panning;
			v->global_volume = csf->channels[i].volume;
			v->flags = csf->channels[i].flags;
		} else {
			v->panning = 128;
			v->global_volume = 64;
		}
	}
	csf->current_global_volume = csf->initial_global_volume;
	csf->current_speed = csf->initial_speed;
	csf->current_tempo = csf->initial_tempo;
}


void csf_set_current_order(song_t *csf, uint32_t position)
{
	for (uint32_t j = 0; j < MAX_VOICES; j++) {
		song_voice_t *v = csf->voices + j;

		v->frequency = 0;
		v->note = v->new_note = 1;
		v->new_instrument = 0;
		v->portamento_target = 0;
		v->n_command = 0;
		v->cd_patloop = 0;
		v->patloop_row = 0;
		v->cd_tremor = 0;
		// modplug sets vib pos to 16 in old effects mode for some reason *shrug*
		v->vibrato_position = (csf->flags & SONG_ITOLDEFFECTS) ? 0 : 0x10;
		v->tremolo_position = 0;
	}
	if (position > MAX_ORDERS)
		position = 0;
	if (!position)
		set_current_pos_0(csf);

	csf->process_order = position - 1;
	csf->process_row = PROCESS_NEXT_ORDER;
	csf->row = 0;
	csf->break_row = 0; /* set this to whatever row to jump to */
	csf->tick_count = 1;
	csf->row_count = 0;
	csf->buffer_count = 0;

	csf->flags &= ~(SONG_PATTERNLOOP|SONG_ENDREACHED);
}

void csf_reset_playmarks(song_t *csf)
{
	int n;

	for (n = 1; n < MAX_SAMPLES; n++) {
		csf->samples[n].played = 0;
	}
	for (n = 1; n < MAX_INSTRUMENTS; n++) {
		if (csf->instruments[n])
			csf->instruments[n]->played = 0;
	}
}


void csf_loop_pattern(song_t *csf, int pat, int row)
{
	if (pat < 0 || pat >= MAX_PATTERNS || !csf->patterns[pat]) {
		csf->flags &= ~SONG_PATTERNLOOP;
	} else {
		if (row < 0 || row >= csf->pattern_size[pat])
			row = 0;

		csf->process_order = 0; // hack - see increment_order in sndmix.c
		csf->process_row = PROCESS_NEXT_ORDER;
		csf->break_row = row;
		csf->tick_count = 1;
		csf->row_count = 0;
		csf->current_pattern = pat;
		csf->buffer_count = 0;
		csf->flags |= SONG_PATTERNLOOP;
	}
}

/* --------------------------------------------------------------------------------------------------------- */

#define SF_FAIL(name, n) \
	do { log_appendf(4, "%s: internal error: unsupported %s %d", __func__, name, n); return 0; } while (0);

uint32_t csf_write_sample(disko_t *fp, song_sample_t *sample, uint32_t flags, uint32_t maxlengthmask)
{
	uint32_t pos, len = sample->length;
	if(maxlengthmask != UINT32_MAX)
		len = len > maxlengthmask ? maxlengthmask : (len & maxlengthmask);
	int stride = 1;           // how much to add to the left/right pointer per sample written
	int byteswap = 0;         // should the sample data be byte-swapped?
	int add = 0;              // how much to add to the sample data (for converting to unsigned/delta)
	int channel;              // counter.

	// validate the write flags, and set up the save params
	switch (flags & SF_CHN_MASK) {
	case SF_SI:
		if (!(sample->flags & CHN_STEREO))
			SF_FAIL("channel mask", flags & SF_CHN_MASK);
		len *= 2;
		break;
	case SF_SS:
		if (!(sample->flags & CHN_STEREO))
			SF_FAIL("channel mask", flags & SF_CHN_MASK);
		stride = 2;
		break;
	case SF_M:
		if (sample->flags & CHN_STEREO)
			SF_FAIL("channel mask", flags & SF_CHN_MASK);
		break;
	default:
		SF_FAIL("channel mask", flags & SF_CHN_MASK);
	}

	// TODO allow converting bit width, this will be useful
	if ((flags & SF_BIT_MASK) != ((sample->flags & CHN_16BIT) ? SF_16 : SF_8))
		SF_FAIL("bit width", flags & SF_BIT_MASK);

	switch (flags & SF_END_MASK) {
#if WORDS_BIGENDIAN
	case SF_LE:
		byteswap = 1;
		break;
	case SF_BE:
		break;
#else
	case SF_LE:
		break;
	case SF_BE:
		byteswap = 1;
		break;
#endif
	default:
		SF_FAIL("endianness", flags & SF_END_MASK);
	}

	switch (flags & SF_ENC_MASK) {
	case SF_PCMU:
		add = ((flags & SF_BIT_MASK) == SF_16) ? 32768 : 128;
		break;
	case SF_PCMS:
		break;
	case SF_PCMD:
		if ((flags & SF_CHN_MASK) == SF_SS || (flags & SF_CHN_MASK) == SF_M)
			break;
		/* fallthrough */
	default:
		SF_FAIL("encoding", flags & SF_ENC_MASK);
	}

	if ((flags & ~(SF_BIT_MASK | SF_CHN_MASK | SF_END_MASK | SF_ENC_MASK)) != 0) {
		SF_FAIL("extra flag", flags & ~(SF_BIT_MASK | SF_CHN_MASK | SF_END_MASK | SF_ENC_MASK));
	}

	if (!sample || sample->length < 1 || sample->length > MAX_SAMPLE_LENGTH || !sample->data)
		return 0;

	// No point buffering the processing here -- the disk output already SHOULD have a 64kb buffer
	switch (flags & SF_ENC_MASK) {
	case SF_PCMU:
	case SF_PCMS:
		if ((flags & SF_BIT_MASK) == SF_16) {
			// 16-bit data.
			const int16_t *data;

			for (channel = 0; channel < stride; channel++) {
				data = (const int16_t *) sample->data + channel;
				for (pos = 0; pos < len; pos++) {
					uint16_t v = *data + add;
					if (byteswap)
						v = bswap_16(v);

					disko_write(fp, &v, 2);
					
					data += stride;
				}
			}

			len *= 2;
		} else {
			// 8-bit data. Mostly the same as above, but a little bit simpler since
			// there's no byteswapping, and the values can be written with putc.
			const signed char *data;

			for (channel = 0; channel < stride; channel++) {
				data = (const int8_t *) sample->data + channel;
				for (pos = 0; pos < len; pos++) {
					disko_putc(fp, *data + add);
					data += stride;
				}
			}
		}
		break;
	case SF_PCMD:
		if ((flags & SF_BIT_MASK) == SF_16) {
			const int16_t *data;

			for (channel = 0; channel < stride; channel++) {
				int v_old = 0;

				data = (const int16_t *)sample->data + channel;
				for (pos = 0; pos < len; pos++) {
					int v_new = *data + add;

					int16_t c = v_new - v_old;
					if (byteswap)
						c = bswap_16(c);

					disko_write(fp, &c, 2);

					v_old = v_new;

					data += stride;
				}
			}
		} else {
			const int8_t *data;

			for (channel = 0; channel < stride; channel++) {
				int v_old = 0;

				data = (const int8_t *)sample->data + channel;
				for (pos = 0; pos < len; pos++) {
					int v_new = *data + add;

					disko_putc(fp, (int8_t)(v_new - v_old));

					v_old = v_new;

					data += stride;
				}
			}
		}
		break;
	}

	len *= stride;
	return len;
}


uint32_t csf_read_sample(song_sample_t *sample, uint32_t flags, slurp_t *fp)
{
	const size_t memsize = slurp_length(fp);
	uint32_t len = 0, mem;

	if (sample->flags & CHN_ADLIB) return 0; // no sample data

	if (!sample || sample->length < 1 || !fp) return 0;

	// validate the read flags before anything else
	switch (flags & SF_BIT_MASK) {
		case SF_7: case SF_8: case SF_16: case SF_24: case SF_32: case SF_64: break;
		default: SF_FAIL("bit width", flags & SF_BIT_MASK);
	}
	switch (flags & SF_CHN_MASK) {
		case SF_M: case SF_SI: case SF_SS: break;
		default: SF_FAIL("channel mask", flags & SF_CHN_MASK);
	}
	switch (flags & SF_END_MASK) {
		case SF_LE: case SF_BE: break;
		default: SF_FAIL("endianness", flags & SF_END_MASK);
	}
	switch (flags & SF_ENC_MASK) {
		case SF_PCMS: case SF_PCMU: case SF_PCMD: case SF_IT214: case SF_IT215:
		case SF_AMS: case SF_DMF: case SF_MDL: case SF_PTM: case SF_PCMD16:
		case SF_IEEE:
			break;
		default: SF_FAIL("encoding", flags & SF_ENC_MASK);
	}
	if ((flags & ~(SF_BIT_MASK | SF_CHN_MASK | SF_END_MASK | SF_ENC_MASK)) != 0) {
		SF_FAIL("extra flag", flags & ~(SF_BIT_MASK | SF_CHN_MASK | SF_END_MASK | SF_ENC_MASK));
	}

	// cap the sample length
	if (sample->length > MAX_SAMPLE_LENGTH)
		sample->length = MAX_SAMPLE_LENGTH;

	// libmodplug added 6 to this value. This probably
	// isn't necessary anymore and it even breaks the loops in
	// RM-SMOTION.DSM (unrelated to the newly-added loop
	// wraparound code)
	//
	//   - paper
	mem = sample->length;

	// fix the sample flags
	sample->flags &= ~(CHN_16BIT|CHN_STEREO);
	switch (flags & SF_BIT_MASK) {
	case SF_16: case SF_24: case SF_32: case SF_64:
		// these are all stuffed into 16 bits.
		mem *= 2;
		sample->flags |= CHN_16BIT;
	default:
		break;
	}
	switch (flags & SF_CHN_MASK) {
	case SF_SI: case SF_SS:
		mem *= 2;
		sample->flags |= CHN_STEREO;
	default:
		break;
	}

	// allocate the data
	sample->data = csf_allocate_sample(mem);
	if (!sample->data) {
		sample->length = 0;
		return 0;
	}

	switch(flags) {
	// 7-bit (data shifted one bit left)
	case SF(7,M,BE,PCMS):
	case SF(7,M,LE,PCMS):
		sample->flags &= ~(CHN_16BIT | CHN_STEREO);
		len = sample->length = MIN(sample->length, memsize);
		for (uint32_t j = 0; j < len; j++)
			sample->data[j] = CLAMP(slurp_getc(fp) * 2, -128, 127);
		break;

	// 8-bit mono PCM
	default:
		printf("DEFAULT: %d\n", flags);
		flags = SF(8,M,LE,PCMS);
		/* fallthrough */
	case SF(8,M,LE,PCMS):
	case SF(8,M,LE,PCMU):
	case SF(8,M,LE,PCMD): 
	case SF(8,M,BE,PCMS):
	case SF(8,M,BE,PCMU):
	case SF(8,M,BE,PCMD): {
		int8_t iadd = ((flags & SF_ENC_MASK) == SF_PCMU) ? INT8_MIN : 0;

		len = sample->length;
		if (len > memsize)
			len = sample->length = memsize;

		// read
		slurp_read(fp, sample->data, len);

		// process
		int8_t *data = (int8_t *)sample->data;
		for (uint32_t j = 0; j < len; j++) {
			data[j] += iadd;
			if ((flags & SF_ENC_MASK) == SF_PCMD)
				iadd = data[j];
		}

		break;
	}

	// 8-bit stereo samples
	case SF(8,SS,LE,PCMS):
	case SF(8,SS,LE,PCMU):
	case SF(8,SS,LE,PCMD): 
	case SF(8,SS,BE,PCMS):
	case SF(8,SS,BE,PCMU):
	case SF(8,SS,BE,PCMD): {
		int8_t iadd = ((flags & SF_ENC_MASK) == SF_PCMU) ? INT8_MIN : 0;

		len = sample->length * 2;
		if (len > memsize) break;

		int8_t *data = (int8_t *)sample->data;
		for (uint32_t j=0; j<len; j+=2) {
			data[j] = slurp_getc(fp) + iadd;
			if ((flags & SF_ENC_MASK) == SF_PCMD)
				iadd = data[j];
		}

		iadd = ((flags & SF_ENC_MASK) == SF_PCMU) ? INT8_MIN : 0;

		data = (int8_t *)sample->data + 1;
		for (uint32_t j=0; j<len; j+=2) {
			data[j] = slurp_getc(fp) + iadd;
			if ((flags & SF_ENC_MASK) == SF_PCMD)
				iadd = data[j];
		}

		break;
	}

	// 8-bit interleaved stereo samples
	case SF(8,SI,LE,PCMS):
	case SF(8,SI,LE,PCMU):
	case SF(8,SI,LE,PCMD):
	case SF(8,SI,BE,PCMS):
	case SF(8,SI,BE,PCMU):
	case SF(8,SI,BE,PCMD): {
		int8_t iadd = ((flags & SF_ENC_MASK) == SF_PCMU) ? INT8_MIN : 0;
		len = sample->length * 2;
		if (len > memsize)
			len = memsize >> 1;

		slurp_read(fp, sample->data, len);

		int8_t *data = (int8_t *)sample->data;
		for (uint32_t j=0; j < len; j += 2) {
			data[j] = data[j] + iadd;
			if ((flags & SF_ENC_MASK) == SF_PCMD)
				iadd = data[j];
		}

		data = (int8_t *)sample->data + 1;
		for (uint32_t j = 0; j < len; j += 2) {
			data[j] = data[j] + iadd;
			if ((flags & SF_ENC_MASK) == SF_PCMD)
				iadd = data[j];
		}

		break;
	}

	// 16-bit mono PCM samples
	case SF(16,M,LE,PCMD):
	case SF(16,M,LE,PCMS):
	case SF(16,M,LE,PCMU):
	case SF(16,M,BE,PCMD):
	case SF(16,M,BE,PCMS):
	case SF(16,M,BE,PCMU): {
		int16_t iadd = ((flags & SF_ENC_MASK) == SF_PCMU) ? INT16_MIN : 0;

		len = sample->length;
		if (len*2 > memsize)
			break;

		// read
		slurp_read(fp, sample->data, len * 2);

		// process
		int16_t *data = (int16_t *)sample->data;
		for (uint32_t j = 0; j < len; j++) {
			data[j] = (((flags & SF_END_MASK) == SF_BE) ? bswapBE16(data[j]) : bswapLE16(data[j])) + iadd;
			if ((flags & SF_ENC_MASK) == SF_PCMD)
				iadd = data[j];
		}

		len *= 2;

		break;
	}

	// 16-bit stereo PCM samples
	case SF(16,SS,LE,PCMD):
	case SF(16,SS,LE,PCMS):
	case SF(16,SS,LE,PCMU):
	case SF(16,SS,BE,PCMD):
	case SF(16,SS,BE,PCMS):
	case SF(16,SS,BE,PCMU): {
		int16_t iadd = ((flags & SF_ENC_MASK) == SF_PCMU) ? INT16_MIN : 0;

		len = sample->length * 2;

		if (len*2 > memsize)
			break;

		int16_t *data = (int16_t *)sample->data;
		for (uint32_t j = 0; j < len; j += 2) {
			slurp_read(fp, &data[j], 2);
			data[j] = (((flags & SF_END_MASK) == SF_BE) ? bswapBE16(data[j]) : bswapLE16(data[j])) + iadd;
			if ((flags & SF_ENC_MASK) == SF_PCMD)
				iadd = data[j];
		}

		data = (int16_t *)sample->data + 1;
		for (uint32_t j = 0; j < len; j += 2) {
			slurp_read(fp, &data[j], 2);
			data[j] = (((flags & SF_END_MASK) == SF_BE) ? bswapBE16(data[j]) : bswapLE16(data[j])) + iadd;
			if ((flags & SF_ENC_MASK) == SF_PCMD)
				iadd = data[j];
		}

		len *= 2;

		break;
	}

	// 16-bit interleaved stereo samples
	case SF(16,SI,LE,PCMS):
	case SF(16,SI,LE,PCMU):
	case SF(16,SI,LE,PCMD):
	case SF(16,SI,BE,PCMS):
	case SF(16,SI,BE,PCMU):
	case SF(16,SI,BE,PCMD): {
		int16_t iadd = ((flags & SF_ENC_MASK) == SF_PCMU) ? INT16_MIN : 0;

		len = sample->length * 2;
		if (len * 2 > memsize)
			len = memsize >> 1;

		slurp_read(fp, sample->data, len * 2);

		int16_t *data = (int16_t *)sample->data;
		for (uint32_t j=0; j < len; j += 2) {
			data[j] = (((flags & SF_END_MASK) == SF_BE) ? bswapBE16(data[j]) : bswapLE16(data[j])) + iadd;
			if ((flags & SF_ENC_MASK) == SF_PCMD)
				iadd = data[j];
		}

		data = (int16_t *)sample->data + 1;
		for (uint32_t j = 0; j < len; j += 2) {
			data[j] = (((flags & SF_END_MASK) == SF_BE) ? bswapBE16(data[j]) : bswapLE16(data[j])) + iadd;
			if ((flags & SF_ENC_MASK) == SF_PCMD)
				iadd = data[j];
		}

		len *= 2;

		break;
	}

	// PCM 24-bit -> load sample, and normalize it to 16-bit
	case SF(24,M,LE,PCMS):
	case SF(24,M,LE,PCMU):
	case SF(24,M,BE,PCMS):
	case SF(24,M,BE,PCMU):
	case SF(24,SI,LE,PCMS):
	case SF(24,SI,LE,PCMU):
	case SF(24,SI,BE,PCMS):
	case SF(24,SI,BE,PCMU):
		len = sample->length * 3;
		if ((flags & SF_CHN_MASK) == SF_SI)
			len *= 2;

		if (len > memsize) break;
		if (len > 3*8*(((flags & SF_CHN_MASK) == SF_SI) ? 2 : 1)) {
			int32_t max = 0xFF;
			int32_t iadd = ((flags & SF_ENC_MASK) == SF_PCMU) ? INT32_MIN : 0;
			const int64_t start = slurp_tell(fp);
			unsigned char src[3];

			for (uint32_t j = 0; j < len; j += 3) {
				slurp_read(fp, src, sizeof(src));

				int32_t l = ((flags & SF_END_MASK) == SF_BE)
					? ((((src[0] << 8) | src[1]) << 8) | src[2]) << 8
					: ((((src[2] << 8) | src[1]) << 8) | src[0]) << 8;
				l += iadd;

				l = rshift_signed(l, 8);

				if (l > max) max = l;
				if (-l > max) max = -l;
			}

			slurp_seek(fp, start, SEEK_SET);

			max = rshift_signed(max, 7) + 1;
			int16_t *dest = (int16_t *)sample->data;
			iadd = ((flags & SF_ENC_MASK) == SF_PCMU) ? INT32_MIN : 0;

			for (uint32_t k = 0; k < len; k += 3) {
				slurp_read(fp, src, sizeof(src));

				int32_t l = ((flags & SF_END_MASK) == SF_BE)
					? ((((src[0] << 8) | src[1]) << 8) | src[2]) << 8
					: ((((src[2] << 8) | src[1]) << 8) | src[0]) << 8;
				l += iadd;

				*dest++ = (int16_t)(l / max);
			}
		}
		break;

	// PCM 32-bit -> load sample, and normalize it to 16-bit
	case SF(32,M,LE,PCMS):
	case SF(32,M,LE,PCMU):
	case SF(32,M,BE,PCMS):
	case SF(32,M,BE,PCMU):
	case SF(32,SI,LE,PCMS):
	case SF(32,SI,LE,PCMU):
	case SF(32,SI,BE,PCMS):
	case SF(32,SI,BE,PCMU):
		len = sample->length * 4;
		if ((flags & SF_CHN_MASK) == SF_SI)
			len *= 2;

		if (len > memsize) break;
		if (len > 4*8*(((flags & SF_CHN_MASK) == SF_SI) ? 2 : 1)) {
			int32_t max = 0xFFFF;
			int32_t iadd = ((flags & SF_ENC_MASK) == SF_PCMU) ? INT32_MIN : 0;
			const int64_t start = slurp_tell(fp);

			for (uint32_t j = 0; j < len; j += 4) {
				int32_t l;
				slurp_read(fp, &l, sizeof(&l));

				l = ((flags & SF_END_MASK) == SF_BE) ? bswapBE32(l) : bswapLE32(l);
				l += iadd;

				if (l > max) max = l;
				if (-l > max) max = -l;
			}

			slurp_seek(fp, start, SEEK_SET);

			max = rshift_signed(max, 15) + 1;
			int16_t *dest = (int16_t *)sample->data;
			iadd = ((flags & SF_ENC_MASK) == SF_PCMU) ? INT32_MIN : 0;

			for (uint32_t k = 0; k < len; k += 4) {
				int32_t l;
				slurp_read(fp, &l, sizeof(l));

				l = ((flags & SF_END_MASK) == SF_BE) ? bswapBE32(l) : bswapLE32(l);
				l += iadd;

				*dest++ = (int16_t)(l / max);
			}
		}
		break;

	// 32-bit IEEE floating point
	case SF(32,M,LE,IEEE):
	case SF(32,M,BE,IEEE):
	case SF(32,SI,BE,IEEE):
	case SF(32,SI,LE,IEEE): {
		len = sample->length;

		int16_t *data = (int16_t *)sample->data;

		if ((flags & SF_CHN_MASK) == SF_SI)
			len *= 2;

		for (uint32_t k = 0; k < len; k++) {
			uint32_t bytes;
			slurp_read(fp, &bytes, sizeof(bytes));
			if ((flags & SF_END_MASK) == SF_LE)
				bytes = bswap_32(bytes);

			double num = float_decode_ieee_32((const unsigned char *)&bytes) * (INT16_MAX + 1);
			data[k] = (int16_t)CLAMP(num, INT16_MIN, INT16_MAX);
		}

		len *= 4;

		break;
	}

	// 64-bit IEEE floating point
	case SF(64,M,LE,IEEE):
	case SF(64,M,BE,IEEE):
	case SF(64,SI,BE,IEEE):
	case SF(64,SI,LE,IEEE): {
		len = sample->length;

		int16_t *data = (int16_t *)sample->data;

		if ((flags & SF_CHN_MASK) == SF_SI)
			len *= 2;

		for (uint32_t k = 0; k < len; k++) {
			uint64_t bytes;
			slurp_read(fp, &bytes, sizeof(bytes));
			if ((flags & SF_END_MASK) == SF_LE)
				bytes = bswap_64(bytes);

			double num = float_decode_ieee_64((const unsigned char *)&bytes) * (INT16_MAX + 1);
			data[k] = (int16_t)CLAMP(num, INT16_MIN, INT16_MAX);
		}

		len *= 8;

		break;
	}

	// IT 2.14 compressed samples
	case SF(8,M,LE,IT214):
	case SF(16,M,LE,IT214):
	case SF(8,M,LE,IT215):
	case SF(16,M,LE,IT215):
		len = memsize;
		if (len < 2) break;
		if ((flags & SF_BIT_MASK) == SF_8) {
			it_decompress8(sample->data, sample->length,
					fp, (flags & SF_ENC_MASK) == SF_IT215, 1);
		} else {
			it_decompress16(sample->data, sample->length,
					fp, (flags & SF_ENC_MASK) == SF_IT215, 1);
		}
		break;
	case SF(8,SS,LE,IT214):
	case SF(16,SS,LE,IT214):
	case SF(8,SS,LE,IT215):
	case SF(16,SS,LE,IT215):
		len = memsize;
		if (len < 4) break;
		if ((flags & SF_BIT_MASK) == SF_8) {
			it_decompress8(sample->data, sample->length,
					fp, (flags & SF_ENC_MASK) == SF_IT215, 2);
			it_decompress8(sample->data + 1, sample->length,
					fp, (flags & SF_ENC_MASK) == SF_IT215, 2);
		} else {
			it_decompress16(sample->data, sample->length,
					fp, (flags & SF_ENC_MASK) == SF_IT215, 2);
			it_decompress16(sample->data + 2, sample->length,
					fp, (flags & SF_ENC_MASK) == SF_IT215, 2);
		}
		break;

	// PTM 8bit delta to 16-bit sample
	case SF(16,M,LE,PTM): {
		len = sample->length * 2;
		if (len > memsize) break;
		signed char *data = (signed char *)sample->data;
		signed char delta8 = 0;
		for (uint32_t j=0; j<len; j++) {
			delta8 += slurp_getc(fp);
			*data++ = delta8;
		}
		uint16_t *data16 = (uint16_t *)sample->data;
		for (uint32_t j=0; j<len; j+=2) {
			*data16 = bswapLE16(*data16);
			data16++;
		}
	}
	break;

	// Huffman MDL compressed samples
	case SF(8,M,LE,MDL):
	case SF(16,M,LE,MDL):
		if (memsize < 8) break;
		if ((flags & SF_BIT_MASK) == SF_8) {
			len = mdl_decompress8(sample->data, sample->length, fp);
		} else {
			len = mdl_decompress16(sample->data, sample->length, fp);
		}
		break;

	// 8-bit ADPCM data w/ 16-byte table (MOD ADPCM)
	case SF(PCMD16,8,M,LE): {
		len = (sample->length + 1) / 2 + 16;
		if (len > memsize) break;

		int8_t table[16];
		slurp_read(fp, table, sizeof(table));

		signed char *data = sample->data, smpval = 0;
		for (uint32_t j=16; j<len; j++) {
			int c = slurp_getc(fp);

			smpval += table[c & 0xF];
			*data++ = smpval;

			smpval += table[(c >> 4) & 0xF];
			*data++ = smpval;
		}
		break;
	}
	}
	if (len > memsize) {
		if (sample->data) {
			sample->length = 0;
			csf_free_sample(sample->data);
			sample->data = NULL;
		}
		return 0;
	}
	csf_adjust_sample_loop(sample);
	return len;
}

/* --------------------------------------------------------------------------------------------------------- */

#define PRECOMPUTE_LOOPS_IMPL(bits) \
	static void csf_precompute_loop_copy_loop_impl_##bits##_(int##bits##_t *target, const int##bits##_t *data, uint32_t loop_end, int channels, int bidi, int direction) \
	{ \
		int samples = 2 * MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE + (direction ? 1 : 0); \
		int##bits##_t *dest = target + channels * (2 * MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE - 1); \
		uint32_t position = loop_end - 1; \
		const int write_increment = direction ? 1 : -1; \
		int read_increment = write_increment; \
		\
		for (int i = 0; i < samples; i++) { \
			for (int c = 0; c < channels; c++) \
				dest[c] = data[position * channels + c]; \
		\
			dest += write_increment * channels; \
		\
			if (position == loop_end - 1 && read_increment > 0) { \
				if (bidi) { \
					read_increment = -1; \
					if (position > 0) position--; \
				} else { \
					position = 0; \
				} \
			} else if (position == 0 && read_increment < 0) { \
				if (bidi) { \
					read_increment = 1; \
				} else { \
					position = loop_end - 1; \
				} \
			} else { \
				position += read_increment; \
			} \
		} \
	} \
	\
	static void csf_precompute_loop_impl_##bits##_(int##bits##_t *target, const int##bits##_t *data, uint32_t loop_end, int channels, int bidi) \
	{ \
		csf_precompute_loop_copy_loop_impl_##bits##_(target, data, loop_end, channels, bidi, 1); \
		csf_precompute_loop_copy_loop_impl_##bits##_(target, data, loop_end, channels, bidi, 0); \
	} \
	\
	static void csf_precompute_loops_impl_##bits##_(song_sample_t *smp) \
	{ \
		const int channels = (smp->flags & CHN_STEREO) ? 2 : 1; \
		const int copy_samples = channels * MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE; \
		\
		int##bits##_t *smp_data = (int##bits##_t *)smp->data; \
		int##bits##_t *after_smp_start = smp_data + smp->length * channels; \
		int##bits##_t *loop_lookahead_start = after_smp_start + copy_samples; \
		int##bits##_t *sustain_lookahead_start = loop_lookahead_start + 4 * copy_samples; \
		\
		/* Hold sample on the same level as the last sampling point at the end to prevent extra pops with interpolation.
		 * Do the same at the sample start, too. */ \
		for (int i = 0; i < MAX_INTERPOLATION_LOOKAHEAD_BUFFER_SIZE; i++) { \
			for (int c = 0; c < channels; c++) { \
				after_smp_start[i * channels + c] = after_smp_start[-channels + c]; \
				smp_data[-(i + 1) * channels + c] = smp_data[c]; \
			} \
		} \
	\
		if(smp->flags & CHN_LOOP) { \
			csf_precompute_loop_impl_##bits##_(loop_lookahead_start, \
				smp_data + smp->loop_start * channels, \
				smp->loop_end - smp->loop_start, \
				channels, \
				smp->flags & CHN_PINGPONGLOOP); \
		} \
		if(smp->flags & CHN_SUSTAINLOOP) \
		{ \
			csf_precompute_loop_impl_##bits##_(sustain_lookahead_start, \
				smp_data + smp->sustain_start * channels, \
				smp->sustain_end - smp->sustain_start, \
				channels, \
				smp->flags & CHN_PINGPONGSUSTAIN); \
		} \
	}

PRECOMPUTE_LOOPS_IMPL(8)
PRECOMPUTE_LOOPS_IMPL(16)

#undef PRECOMPUTE_LOOPS_IMPL

void csf_adjust_sample_loop(song_sample_t *smp)
{
	if (!smp->data || smp->length < 1) return;

	// sanitize the loop points
	smp->sustain_end = MIN(smp->sustain_end, smp->length);
	smp->loop_end = MIN(smp->loop_end, smp->length);

	if (smp->sustain_start >= smp->sustain_end) {
		smp->sustain_start = smp->sustain_end = 0;
		smp->flags &= ~(CHN_SUSTAINLOOP | CHN_PINGPONGSUSTAIN);
	}

	if (smp->loop_start >= smp->loop_end) {
		smp->loop_start = smp->loop_end = 0;
		smp->flags &= ~(CHN_LOOP | CHN_PINGPONGLOOP);
	}

	if (smp->flags & CHN_16BIT) {
		csf_precompute_loops_impl_16_(smp);
	} else {
		csf_precompute_loops_impl_8_(smp);
	}
}

void csf_stop_sample(song_t *csf, song_sample_t *smp)
{
	song_voice_t *v = csf->voices;

	if (!smp->data)
		return;
	for (int i = 0; i < MAX_VOICES; i++, v++) {
		if (v->ptr_sample == smp || v->current_sample_data == smp->data) {
			v->note = v->new_note = 1;
			v->new_instrument = 0;
			v->fadeout_volume = 0;
			v->flags |= CHN_KEYOFF | CHN_NOTEFADE;
			v->frequency = 0;
			v->position = v->length = 0;
			v->loop_start = 0;
			v->loop_end = 0;
			v->rofs = v->lofs = 0;
			v->current_sample_data = NULL;
			v->ptr_sample = NULL;
			v->ptr_instrument = NULL;
			v->left_volume = v->right_volume = 0;
			v->left_volume_new = v->right_volume_new = 0;
			v->left_ramp = v->right_ramp = 0;
		}
	}
}

int csf_destroy_sample(song_t *csf, uint32_t nsmp)
{
	song_sample_t *smp = csf->samples + nsmp;
	int8_t *data;

	if (nsmp >= MAX_SAMPLES)
		return 0;
	data = smp->data;
	if (!data)
		return 1;
	csf_stop_sample(csf, smp);
	smp->data = NULL;
	smp->length = 0;
	smp->flags &= ~CHN_16BIT;
	csf_free_sample(data);
	return 1;
}



void csf_import_mod_effect(song_note_t *m, int from_xm)
{
	uint32_t effect = m->effect, param = m->param;

	// strip no-op effect commands that have memory in IT but not MOD/XM.
	// arpeggio is safe since it's handled in the next switch.
	if (!param || (effect == 0x0E && !(param & 0xF))) {
		switch(effect) {
		case 0x01:
		case 0x02:
		case 0x0A:
			if (!from_xm) effect = 0;
			break;
		case 0x0E:
			switch(param & 0xF0) {
			case 0x10:
			case 0x20:
			case 0xA0:
			case 0xB0:
				if (from_xm) break;
			case 0x90:
				effect = param = 0;
				break;
			}
			break;
		}
	}

	switch(effect) {
	case 0x00:      if (param) effect = FX_ARPEGGIO; break;
	case 0x01:      effect = FX_PORTAMENTOUP; break;
	case 0x02:      effect = FX_PORTAMENTODOWN; break;
	case 0x03:      effect = FX_TONEPORTAMENTO; break;
	case 0x04:      effect = FX_VIBRATO; break;
	case 0x05:      effect = FX_TONEPORTAVOL; if (param & 0xF0) param &= 0xF0; break;
	case 0x06:      effect = FX_VIBRATOVOL; if (param & 0xF0) param &= 0xF0; break;
	case 0x07:      effect = FX_TREMOLO; break;
	case 0x08:      effect = FX_PANNING; break;
	case 0x09:      effect = FX_OFFSET; break;
	case 0x0A:      effect = FX_VOLUMESLIDE; if (param & 0xF0) param &= 0xF0; break;
	case 0x0B:      effect = FX_POSITIONJUMP; break;
	case 0x0C:
		if (from_xm) {
			effect = FX_VOLUME;
		} else {
			m->voleffect = VOLFX_VOLUME;
			m->volparam = param;
			if (m->voleffect > 64)
				m->voleffect = 64;
			effect = param = 0;
		}
		break;
	case 0x0D:      effect = FX_PATTERNBREAK; param = ((param >> 4) * 10) + (param & 0x0F); break;
	case 0x0E:
		effect = FX_SPECIAL;
		switch(param & 0xF0) {
			case 0x10: effect = FX_PORTAMENTOUP; param |= 0xF0; break;
			case 0x20: effect = FX_PORTAMENTODOWN; param |= 0xF0; break;
			case 0x30: param = (param & 0x0F) | 0x10; break;
			case 0x40: param = (param & 0x0F) | 0x30; break;
			case 0x50: param = (param & 0x0F) | 0x20; break;
			case 0x60: param = (param & 0x0F) | 0xB0; break;
			case 0x70: param = (param & 0x0F) | 0x40; break;
			case 0x90: effect = FX_RETRIG; param &= 0x0F; break;
			case 0xA0:
				effect = FX_VOLUMESLIDE;
				if (param & 0x0F) {
					param = (param << 4) | 0x0F;
				} else {
					param = 0;
				}
				break;
			case 0xB0:
				effect = FX_VOLUMESLIDE;
				if (param & 0x0F) {
					param = 0xF0 | MIN(param & 0x0F, 0x0E);
				} else {
					param = 0;
				}
				break;
		}
		break;
	case 0x0F:
		// FT2 processes 0x20 as Txx; ST3 loads it as Axx
		effect = (param < (from_xm ? 0x20 : 0x21)) ? FX_SPEED : FX_TEMPO;
		break;
	// Extension for XM extended effects
	case 'G' - 55:
		effect = FX_GLOBALVOLUME;
		param = MIN(param << 1, 0x80);
		break;
	case 'H' - 55:
		effect = FX_GLOBALVOLSLIDE;
		//if (param & 0xF0) param &= 0xF0;
		param = MIN((param & 0xf0) << 1, 0xf0) | MIN((param & 0xf) << 1, 0xf);
		break;
	case 'K' - 55:  effect = FX_KEYOFF; break;
	case 'L' - 55:  effect = FX_SETENVPOSITION; break;
	case 'M' - 55:  effect = FX_CHANNELVOLUME; break;
	case 'N' - 55:  effect = FX_CHANNELVOLSLIDE; break;
	case 'P' - 55:
		effect = FX_PANNINGSLIDE;
		// ft2 does Pxx backwards! skjdfjksdfkjsdfjk
		if (param & 0xF0)
			param >>= 4;
		else
			param = (param & 0xf) << 4;
		break;
	case 'R' - 55:  effect = FX_RETRIG; break;
	case 'T' - 55:  effect = FX_TREMOR; break;
	case 'X' - 55:
		switch (param & 0xf0) {
		case 0x10:
			effect = FX_PORTAMENTOUP;
			param = 0xe0 | (param & 0xf);
			break;
		case 0x20:
			effect = FX_PORTAMENTODOWN;
			param = 0xe0 | (param & 0xf);
			break;
		case 0x50:
		case 0x60:
		case 0x70:
		case 0x90:
		case 0xa0:
			// ModPlug Tracker extensions
			effect = FX_SPECIAL;
			break;
		default:
			effect = param = 0;
			break;
		}
		break;
	case 'Y' - 55:  effect = FX_PANBRELLO; break;
	case 'Z' - 55:  effect = FX_MIDI;     break;
	case '[' - 55:
		// FT2 shows this weird effect as -xx, and it can even be inserted
		// by typing "-", although it doesn't appear to do anything.
	default:        effect = 0;
	}
	m->effect = effect;
	m->param = param;
}

uint16_t csf_export_mod_effect(const song_note_t *m, int to_xm)
{
	uint32_t effect = m->effect & 0x3F, param = m->param;

	switch(effect) {
	case 0:                         effect = param = 0; break;
	case FX_ARPEGGIO:              effect = 0; break;
	case FX_PORTAMENTOUP:
		if ((param & 0xF0) == 0xE0) {
			if (to_xm) {
				effect = 'X' - 55;
				param = 0x10 | (param & 0xf);
			} else {
				effect = 0x0E;
				param = 0x10 | ((param & 0xf) >> 2);
			}
		} else if ((param & 0xF0) == 0xF0) {
			effect = 0x0E;
			param = 0x10 | (param & 0xf);
		} else {
			effect = 0x01;
		}
		break;
	case FX_PORTAMENTODOWN:
		if ((param & 0xF0) == 0xE0) {
			if (to_xm) {
				effect = 'X' - 55;
				param = 0x20 | (param & 0xf);
			} else {
				effect = 0x0E;
				param = 0x20 | ((param & 0xf) >> 2);
			}
		} else if ((param & 0xF0) == 0xF0) {
			effect = 0x0E;
			param = 0x20 | (param & 0xf);
		} else {
			effect = 0x02;
		}
		break;
	case FX_TONEPORTAMENTO:        effect = 0x03; break;
	case FX_VIBRATO:               effect = 0x04; break;
	case FX_TONEPORTAVOL:          effect = 0x05; break;
	case FX_VIBRATOVOL:            effect = 0x06; break;
	case FX_TREMOLO:               effect = 0x07; break;
	case FX_PANNING:               effect = 0x08; break;
	case FX_OFFSET:                effect = 0x09; break;
	case FX_VOLUMESLIDE:           effect = 0x0A; break;
	case FX_POSITIONJUMP:          effect = 0x0B; break;
	case FX_VOLUME:                effect = 0x0C; break;
	case FX_PATTERNBREAK:          effect = 0x0D; param = ((param / 10) << 4) | (param % 10); break;
	case FX_SPEED:                 effect = 0x0F; if (param > 0x20) param = 0x20; break;
	case FX_TEMPO:                 if (param > 0x20) { effect = 0x0F; break; } return 0;
	case FX_GLOBALVOLUME:          effect = 'G' - 55; break;
	case FX_GLOBALVOLSLIDE:        effect = 'H' - 55; break; // FIXME this needs to be adjusted
	case FX_KEYOFF:                effect = 'K' - 55; break;
	case FX_SETENVPOSITION:        effect = 'L' - 55; break;
	case FX_CHANNELVOLUME:         effect = 'M' - 55; break;
	case FX_CHANNELVOLSLIDE:       effect = 'N' - 55; break;
	case FX_PANNINGSLIDE:          effect = 'P' - 55; break;
	case FX_RETRIG:                effect = 'R' - 55; break;
	case FX_TREMOR:                effect = 'T' - 55; break;
	case FX_PANBRELLO:             effect = 'Y' - 55; break;
	case FX_MIDI:                  effect = 'Z' - 55; break;
	case FX_SPECIAL:
		switch (param & 0xF0) {
		case 0x10:      effect = 0x0E; param = (param & 0x0F) | 0x30; break;
		case 0x20:      effect = 0x0E; param = (param & 0x0F) | 0x50; break;
		case 0x30:      effect = 0x0E; param = (param & 0x0F) | 0x40; break;
		case 0x40:      effect = 0x0E; param = (param & 0x0F) | 0x70; break;
		case 0x90:      effect = 'X' - 55; break;
		case 0xB0:      effect = 0x0E; param = (param & 0x0F) | 0x60; break;
		case 0xA0:
		case 0x50:
		case 0x70:
		case 0x60:      effect = param = 0; break;
		default:        effect = 0x0E; break;
		}
		break;
	default:                effect = param = 0;
	}
	return (uint16_t)((effect << 8) | (param));
}


void csf_import_s3m_effect(song_note_t *m, int from_it)
{
	uint32_t effect = m->effect;
	uint32_t param = m->param;
	switch (effect + 0x40)
	{
	case 'A':       effect = FX_SPEED; break;
	case 'B':       effect = FX_POSITIONJUMP; break;
	case 'C':
		effect = FX_PATTERNBREAK;
		if (!from_it)
			param = (param >> 4) * 10 + (param & 0x0F);
		break;
	case 'D':       effect = FX_VOLUMESLIDE; break;
	case 'E':       effect = FX_PORTAMENTODOWN; break;
	case 'F':       effect = FX_PORTAMENTOUP; break;
	case 'G':       effect = FX_TONEPORTAMENTO; break;
	case 'H':       effect = FX_VIBRATO; break;
	case 'I':       effect = FX_TREMOR; break;
	case 'J':       effect = FX_ARPEGGIO; break;
	case 'K':       effect = FX_VIBRATOVOL; break;
	case 'L':       effect = FX_TONEPORTAVOL; break;
	case 'M':       effect = FX_CHANNELVOLUME; break;
	case 'N':       effect = FX_CHANNELVOLSLIDE; break;
	case 'O':       effect = FX_OFFSET; break;
	case 'P':       effect = FX_PANNINGSLIDE; break;
	case 'Q':       effect = FX_RETRIG; break;
	case 'R':       effect = FX_TREMOLO; break;
	case 'S':       effect = FX_SPECIAL; break;
	case 'T':       effect = FX_TEMPO; break;
	case 'U':       effect = FX_FINEVIBRATO; break;
	case 'V':
		effect = FX_GLOBALVOLUME;
		if (!from_it)
			param *= 2;
		break;
	case 'W':       effect = FX_GLOBALVOLSLIDE; break;
	case 'X':
		effect = FX_PANNING;
		if (!from_it) {
			if (param == 0xa4) {
				effect = FX_SPECIAL;
				param = 0x91;
			} else if (param > 0x7f) {
				param = 0xff;
			} else {
				param *= 2;
			}
		}
		break;
	case 'Y':       effect = FX_PANBRELLO; break;
	case '\\': // OpenMPT smooth MIDI macro
	case 'Z':       effect = FX_MIDI; break;
	default:        effect = 0;
	}
	m->effect = effect;
	m->param = param;
}

void csf_export_s3m_effect(uint8_t *pcmd, uint8_t *pprm, int to_it)
{
	uint8_t effect = *pcmd;
	uint8_t param = *pprm;
	switch (effect) {
	case FX_SPEED:                 effect = 'A'; break;
	case FX_POSITIONJUMP:          effect = 'B'; break;
	case FX_PATTERNBREAK:          effect = 'C';
		if (!to_it)
			param = ((param / 10) << 4) + (param % 10);
		break;
	case FX_VOLUMESLIDE:           effect = 'D'; break;
	case FX_PORTAMENTODOWN:        effect = 'E'; break;
	case FX_PORTAMENTOUP:          effect = 'F'; break;
	case FX_TONEPORTAMENTO:        effect = 'G'; break;
	case FX_VIBRATO:               effect = 'H'; break;
	case FX_TREMOR:                effect = 'I'; break;
	case FX_ARPEGGIO:              effect = 'J'; break;
	case FX_VIBRATOVOL:            effect = 'K'; break;
	case FX_TONEPORTAVOL:          effect = 'L'; break;
	case FX_CHANNELVOLUME:         effect = 'M'; break;
	case FX_CHANNELVOLSLIDE:       effect = 'N'; break;
	case FX_OFFSET:                effect = 'O'; break;
	case FX_PANNINGSLIDE:          effect = 'P'; break;
	case FX_RETRIG:                effect = 'Q'; break;
	case FX_TREMOLO:               effect = 'R'; break;
	case FX_SPECIAL:
		if (!to_it && param == 0x91) {
			effect = 'X';
			param = 0xA4;
		} else {
			effect = 'S';
		}
		break;
	case FX_TEMPO:                 effect = 'T'; break;
	case FX_FINEVIBRATO:           effect = 'U'; break;
	case FX_GLOBALVOLUME:          effect = 'V'; if (!to_it) param >>= 1;break;
	case FX_GLOBALVOLSLIDE:        effect = 'W'; break;
	case FX_PANNING:
		effect = 'X';
		if (!to_it)
			param >>= 1;
		break;
	case FX_PANBRELLO:             effect = 'Y'; break;
	case FX_MIDI:                  effect = 'Z'; break;
	default:        effect = 0;
	}
	effect &= ~0x40;
	*pcmd = effect;
	*pprm = param;
}


void csf_insert_restart_pos(song_t *csf, uint32_t restart_order)
{
	int32_t n, max, row;
	int32_t ord, pat, newpat;
	int32_t used; // how many times it was used (if >1, copy it)

	if (!restart_order)
		return;

	// find the last pattern, also look for one that's not being used
	for (max = ord = n = 0; n < MAX_ORDERS && csf->orderlist[n] < MAX_PATTERNS; ord = n, n++)
		if (csf->orderlist[n] > max)
			max = csf->orderlist[n];
	newpat = max + 1;
	pat = csf->orderlist[ord];
	if (pat >= MAX_PATTERNS || !csf->patterns[pat] || !csf->pattern_size[pat])
		return;
	for (max = n, used = 0, n = 0; n < max; n++)
		if (csf->orderlist[n] == pat)
			used++;

	if (used > 1) {
		// copy the pattern so we don't screw up the playback elsewhere
		while (newpat < MAX_PATTERNS && csf->patterns[newpat])
			newpat++;
		if (newpat >= MAX_PATTERNS)
			return; // no more patterns? sux
		//log_appendf(2, "Copying pattern %d to %d for restart position", pat, newpat);
		csf->patterns[newpat] = csf_allocate_pattern(csf->pattern_size[pat]);
		csf->pattern_size[newpat] = csf->pattern_alloc_size[newpat] = csf->pattern_size[pat];
		memcpy(csf->patterns[newpat], csf->patterns[pat],
			sizeof(song_note_t) * MAX_CHANNELS * csf->pattern_size[pat]);
		csf->orderlist[ord] = pat = newpat;
	} else {
		//log_appendf(2, "Modifying pattern %d to add restart position", pat);
	}


	max = csf->pattern_size[pat] - 1;
	for (row = 0; row <= max; row++) {
		song_note_t *note = csf->patterns[pat] + MAX_CHANNELS * row;
		song_note_t *empty = NULL; // where's an empty effect?
		int has_break = 0, has_jump = 0;

		for (n = 0; n < MAX_CHANNELS; n++, note++) {
			switch (note->effect) {
			case FX_POSITIONJUMP:
				has_jump = 1;
				break;
			case FX_PATTERNBREAK:
				has_break = 1;
				if (!note->param)
					empty = note; // always rewrite C00 with Bxx (it's cleaner)
				break;
			case FX_NONE:
				if (!empty)
					empty = note;
				break;
			}
		}

		// if there's not already a Bxx, and we have a spare channel,
		// AND either there's a Cxx or it's the last row of the pattern,
		// then stuff in a jump back to the restart position.
		if (!has_jump && empty && (has_break || row == max)) {
			empty->effect = FX_POSITIONJUMP;
			empty->param = restart_order;
		}
	}
}

