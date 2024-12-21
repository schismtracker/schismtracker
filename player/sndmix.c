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

#include "song.h"
#include "player/sndfile.h"
#include "player/snd_fm.h"
#include "player/snd_gm.h"
#include "player/cmixer.h"
#include "it.h"

#include "util.h" /* for clamp */

// Volume ramp length, in 1/10 ms
#define VOLUMERAMPLEN   146 // 1.46ms = 64 samples at 44.1kHz

// VU meter
#define VUMETER_DECAY 16

// SNDMIX: These are global flags for playback control
uint32_t max_voices = 32; // ITT it is 1994

// Mixing data initialized in
static uint32_t volume_ramp_samples = 64;
uint32_t global_vu_left = 0;
uint32_t global_vu_right = 0;
int32_t g_dry_rofs_vol = 0;
int32_t g_dry_lofs_vol = 0;

typedef uint32_t (* convert_t)(void *, int32_t *, uint32_t, int32_t *, int32_t *);


// see also csf_midi_out_raw in effects.c
void (*csf_midi_out_note)(int chan, const song_note_t *m) = NULL;


// The volume we have here is in range 0..(63*255) (0..16065)
// We should keep that range, but convert it into a logarithmic
// one such that a change of 256*8 (2048) corresponds to a halving
// of the volume.
//   logvolume = 2^(linvolume / (4096/8)) * (4096/64)
// However, because the resolution of MIDI volumes
// is merely 128 units, we can use a lookup table.
//
// In this table, each value signifies the minimum value
// that volume must be in order for the result to be
// that table index.
static const uint16_t GMvolTransition[128] =
{
    0, 2031, 4039, 5214, 6048, 6694, 7222, 7669,
 8056, 8397, 8702, 8978, 9230, 9462, 9677, 9877,
10064,10239,10405,10562,10710,10852,10986,11115,
11239,11357,11470,11580,11685,11787,11885,11980,
12072,12161,12248,12332,12413,12493,12570,12645,
12718,12790,12860,12928,12995,13060,13123,13186,
13247,13306,13365,13422,13479,13534,13588,13641,
13693,13745,13795,13844,13893,13941,13988,14034,
14080,14125,14169,14213,14256,14298,14340,14381,
14421,14461,14501,14540,14578,14616,14653,14690,
14727,14763,14798,14833,14868,14902,14936,14970,
15003,15035,15068,15100,15131,15163,15194,15224,
15255,15285,15315,15344,15373,15402,15430,15459,
15487,15514,15542,15569,15596,15623,15649,15675,
15701,15727,15753,15778,15803,15828,15853,15877,
15901,15925,15949,15973,15996,16020,16043,16065,
};


// We use binary search to find the right slot
// with at most 7 comparisons.
static uint32_t find_volume(uint16_t vol)
{
	uint32_t l = 0, r = 128;

	while (l < r) {
		uint32_t m = l + ((r - l) / 2);
		uint16_t p = GMvolTransition[m];

		if (p < vol)
			l = m + 1;
		else
			r = m;
	}

	return l;
}


////////////////////////////////////////////////////////////////////////////////////////////
//
// XXX * I prefixed these with `rn_' to avoid any namespace conflicts
// XXX   Needs better naming!
// XXX * Keep inline?
// XXX * Get rid of the pointer passing where it is not needed
//


static inline void rn_tremor(song_voice_t *chan, int32_t *vol)
{
	if ((chan->cd_tremor & 192) == 128)
		*vol = 0;

	chan->flags |= CHN_FASTVOLRAMP;
}


static inline int32_t rn_vibrato(song_t *csf, song_voice_t *chan, int32_t frequency)
{
	uint32_t vibpos = chan->vibrato_position & 0xFF;
	int32_t vdelta;
	uint32_t vdepth;

	switch (chan->vib_type) {
	case VIB_SINE:
	default:
		vdelta = sine_table[vibpos];
		break;
	case VIB_RAMP_DOWN:
		vdelta = ramp_down_table[vibpos];
		break;
	case VIB_SQUARE:
		vdelta = square_table[vibpos];
		break;
	case VIB_RANDOM:
		vdelta = 128 * ((double) rand() / RAND_MAX) - 64;
		break;
	}

	if (csf->flags & SONG_ITOLDEFFECTS) {
		vdepth = 5;
		vdelta = -vdelta; // yes, IT does vibrato backwards in old-effects mode. try it.
	} else {
		vdepth = 6;
	}
	vdelta = (vdelta * (int)chan->vibrato_depth) >> vdepth;

	frequency = csf_fx_do_freq_slide(csf->flags, frequency, vdelta, 0);

	// handle on tick-N, or all ticks if not in old-effects mode
	if (!(csf->flags & SONG_FIRSTTICK) || !(csf->flags & SONG_ITOLDEFFECTS)) {
		chan->vibrato_position = (vibpos + 4 * chan->vibrato_speed) & 0xFF;
	}

	return frequency;
}

static inline int32_t rn_sample_vibrato(song_t *csf, song_voice_t *chan, int32_t frequency)
{
	uint32_t vibpos = chan->autovib_position & 0xFF;
	int32_t vdelta, adepth = 1;
	song_sample_t *pins = chan->ptr_sample;

	/*
	1) Mov AX, [SomeVariableNameRelatingToVibrato]
	2) Add AL, Rate
	3) AdC AH, 0
	4) AH contains the depth of the vibrato as a fine-linear slide.
	5) Mov [SomeVariableNameRelatingToVibrato], AX  ; For the next cycle.
	*/

	/* OpenMPT test case VibratoSweep0.it:
	   don't calculate autovibrato if the speed is 0 */
	if (pins->vib_speed) {
		adepth = chan->autovib_depth; // (1)
		adepth += pins->vib_rate & 0xff; // (2 & 3)
		/* need this cast -- if adepth is unsigned, large autovib will crash the mixer (why? I don't know!)
		but if vib_depth is changed to signed, that screws up other parts of the code. ugh. */
		adepth = MIN(adepth, (int) (pins->vib_depth << 8));
		chan->autovib_depth = adepth; // (5)
		adepth >>= 8; // (4)

		chan->autovib_position += pins->vib_speed;
	}

	switch(pins->vib_type) {
	case VIB_SINE:
	default:
		vdelta = sine_table[vibpos];
		break;
	case VIB_RAMP_DOWN:
		vdelta = ramp_down_table[vibpos];
		break;
	case VIB_SQUARE:
		vdelta = square_table[vibpos];
		break;
	case VIB_RANDOM:
		vdelta = 128 * ((double) rand() / RAND_MAX) - 64;
		break;
	}
	vdelta = (vdelta * adepth) >> 6;

	int32_t l = abs(vdelta);

	const uint32_t *linear_slide_table, *fine_linear_slide_table;
	if (vdelta < 0) {
		linear_slide_table = linear_slide_up_table;
		fine_linear_slide_table = fine_linear_slide_up_table;
	} else {
		linear_slide_table = linear_slide_down_table;
		fine_linear_slide_table = fine_linear_slide_down_table;
	}

	if(l < 16)
		vdelta = _muldiv(frequency, fine_linear_slide_table[l], 0x10000) - frequency;
	else
		vdelta = _muldiv(frequency, linear_slide_table[l >> 2], 0x10000) - frequency;

	return frequency - vdelta;
}


static inline void rn_process_vol_env(song_voice_t* chan, int32_t *nvol) {
	song_instrument_t *penv = chan->ptr_instrument;
	int32_t vol = *nvol;

	if ((chan->flags & CHN_VOLENV || penv->flags & ENV_VOLUME) && penv->vol_env.nodes) {
		int32_t envpos = chan->vol_env_position - 1;
		uint32_t pt = penv->vol_env.nodes - 1;

		if (chan->vol_env_position == 0)
			return;

		for (uint32_t i = 0; i < (uint32_t)(penv->vol_env.nodes - 1); i++) {
			if (envpos <= penv->vol_env.ticks[i]) {
				pt = i;
				break;
			}
		}

		int32_t x2 = penv->vol_env.ticks[pt];
		int32_t x1, envvol;

		if (envpos >= x2) {
			envvol = penv->vol_env.values[pt] << 2;
			x1 = x2;
		} else if (pt) {
			envvol = penv->vol_env.values[pt-1] << 2;
			x1 = penv->vol_env.ticks[pt-1];
		} else {
			envvol = 0;
			x1 = 0;
		}

		if (envpos > x2)
			envpos = x2;

		if (x2 > x1 && envpos > x1) {
			envvol += ((envpos - x1) * (((int32_t)penv->vol_env.values[pt]<<2) - envvol)) / (x2 - x1);
		}

		envvol = CLAMP(envvol, 0, 256);
		vol = (vol * envvol) >> 8;
	}
	
	*nvol = vol;
}


static inline void rn_process_pan_env(song_voice_t* chan) {
	song_instrument_t *penv = chan->ptr_instrument;

	if ((chan->flags & CHN_PANENV || penv->flags & ENV_PANNING) && (penv->pan_env.nodes)) {
		int32_t envpos = chan->pan_env_position - 1;
		uint32_t pt = penv->pan_env.nodes - 1;

		if (chan->pan_env_position == 0)
			return;

		for (uint32_t i=0; i<(uint32_t)(penv->pan_env.nodes-1); i++) {
			if (envpos <= penv->pan_env.ticks[i]) {
				pt = i;
				break;
			}
		}

		int32_t x2 = penv->pan_env.ticks[pt], y2 = penv->pan_env.values[pt];
		int32_t x1, envpan;

		if (envpos >= x2) {
			envpan = y2;
			x1 = x2;
		} else if (pt) {
			envpan = penv->pan_env.values[pt-1];
			x1 = penv->pan_env.ticks[pt-1];
		} else {
			envpan = 128;
			x1 = 0;
		}

		if (x2 > x1 && envpos > x1) {
			envpan += ((envpos - x1) * (y2 - envpan)) / (x2 - x1);
		}

		envpan = CLAMP(envpan, 0, 64);

		int pan = chan->final_panning;

		if (pan >= 128) {
			pan += ((envpan - 32) * (256 - pan)) / 32;
		} else {
			pan += ((envpan - 32) * (pan)) / 32;
		}

		chan->final_panning = pan;
	}
}


static inline void rn_process_ins_fade(song_voice_t *chan, int32_t *nvol) {
	song_instrument_t *penv = chan->ptr_instrument;
	int32_t vol = *nvol;

	if (chan->flags & CHN_NOTEFADE) {
		uint32_t fadeout = penv->fadeout;

		if (fadeout) {
			chan->fadeout_volume -= fadeout << 1;

			if (chan->fadeout_volume <= 0)
				chan->fadeout_volume = 0;

			vol = (vol * chan->fadeout_volume) >> 16;
		} else if (!chan->fadeout_volume) {
			vol = 0;
		}
	}
	*nvol = vol;
}


static inline void rn_process_envelope(song_voice_t *chan, int32_t *nvol)
{
	// Volume Envelope
	rn_process_vol_env(chan, nvol);

	// Panning Envelope
	rn_process_pan_env(chan);

	// FadeOut volume
	rn_process_ins_fade(chan, nvol);
}


static inline int32_t rn_arpeggio(song_t *csf, song_voice_t *chan, int32_t frequency)
{
	int32_t a = 0;

	const uint32_t real_tick_count = (csf->current_speed + csf->frame_delay) - csf->tick_count;
	const uint32_t tick = real_tick_count % (csf->current_speed + csf->frame_delay);
	switch (tick % 3) {
	case 1:
		a = chan->mem_arpeggio >> 4;
		break;
	case 2:
		a = chan->mem_arpeggio & 0xf;
		break;
	}

	if (!a)
		return frequency;

	return _muldiv(frequency, linear_slide_up_table[a * 16], 65536);
}


static inline void rn_pitch_filter_envelope(song_t *csf, song_voice_t *chan,
	int32_t *nenvpitch, int32_t *nfrequency)
{
	song_instrument_t *penv = chan->ptr_instrument;

	if ((chan->flags & CHN_PITCHENV || penv->flags & (ENV_PITCH | ENV_FILTER)) && (penv->pitch_env.nodes)) {
		int32_t envpos = chan->pitch_env_position - 1;
		uint32_t pt = penv->pitch_env.nodes - 1;
		int32_t frequency = *nfrequency;
		int32_t envpitch = *nenvpitch;

		if (chan->pitch_env_position == 0)
			return;

		for (uint32_t i = 0; i < (uint32_t)(penv->pitch_env.nodes - 1); i++) {
			if (envpos <= penv->pitch_env.ticks[i]) {
				pt = i;
				break;
			}
		}

		int32_t x2 = penv->pitch_env.ticks[pt];
		int32_t x1;

		if (envpos >= x2) {
			envpitch = (((int32_t)penv->pitch_env.values[pt]) - 32) * 8;
			x1 = x2;
		} else if (pt) {
			envpitch = (((int32_t)penv->pitch_env.values[pt - 1]) - 32) * 8;
			x1 = penv->pitch_env.ticks[pt - 1];
		} else {
			envpitch = 0;
			x1 = 0;
		}

		if (envpos > x2)
			envpos = x2;

		if (x2 > x1 && envpos > x1) {
			int32_t envpitchdest = (((int32_t)penv->pitch_env.values[pt]) - 32) * 8;
			envpitch += ((envpos - x1) * (envpitchdest - envpitch)) / (x2 - x1);
		}

		// clamp to -255/255?
		envpitch = CLAMP(envpitch, -256, 256);

		// Pitch Envelope
		if (!(penv->flags & ENV_FILTER)) {
			int32_t l = abs(envpitch);

			l = MIN(l, 255);

			int32_t ratio = (envpitch < 0 ? linear_slide_down_table : linear_slide_up_table)[l];
			frequency = _muldiv(frequency, ratio, 0x10000);
		}

		*nfrequency = frequency;
		*nenvpitch = envpitch;
	}
}


static inline void _process_envelope(song_voice_t *chan, song_instrument_t *penv, song_envelope_t *envelope,
				     int32_t *position, uint32_t env_flag, uint32_t loop_flag, uint32_t sus_flag,
				     uint32_t fade_flag)
{
	int32_t start = 0, end = 0x7fffffff;

	if (!(chan->flags & env_flag)) {
		return;
	}

	/* OpenMPT test case EnvOffLength.it */
	if ((penv->flags & sus_flag) && !(chan->old_flags & CHN_KEYOFF)) {
		start = envelope->ticks[envelope->sustain_start];
		end = envelope->ticks[envelope->sustain_end] + 1;
		fade_flag = 0;
	} else if (penv->flags & loop_flag) {
		start = envelope->ticks[envelope->loop_start];
		end = envelope->ticks[envelope->loop_end] + 1;
		fade_flag = 0;
	} else {
		// End of envelope (?)
		start = end = envelope->ticks[envelope->nodes - 1];
	}
	if (*position >= end) {
		if (fade_flag && !envelope->values[envelope->nodes - 1]) {
			chan->fadeout_volume = chan->final_volume = 0;
		}
		*position = start;
		chan->flags |= fade_flag; // only relevant for volume envelope
	}

	(*position)++;
}

static inline void rn_increment_env_pos(song_voice_t *chan)
{
	song_instrument_t *penv = chan->ptr_instrument;

	_process_envelope(chan, penv, &penv->vol_env, &chan->vol_env_position,
			  CHN_VOLENV, ENV_VOLLOOP, ENV_VOLSUSTAIN, CHN_NOTEFADE);
	_process_envelope(chan, penv, &penv->pan_env, &chan->pan_env_position,
			  CHN_PANENV, ENV_PANLOOP, ENV_PANSUSTAIN, 0);
	_process_envelope(chan, penv, &penv->pitch_env, &chan->pitch_env_position,
			  CHN_PITCHENV, ENV_PITCHLOOP, ENV_PITCHSUSTAIN, 0);
}


static inline int32_t rn_update_sample(song_t *csf, song_voice_t *chan, int32_t nchan, int32_t master_vol)
{
	// Adjusting volumes
	if (csf->mix_channels < 2 || (csf->flags & SONG_NOSTEREO)) {
		chan->right_volume_new = (chan->final_volume * master_vol) >> 8;
		chan->left_volume_new = chan->right_volume_new;
	} else if ((chan->flags & CHN_SURROUND) && !(csf->mix_flags & SNDMIX_NOSURROUND)) {
		chan->right_volume_new = (chan->final_volume * master_vol) >> 8;
		chan->left_volume_new = -chan->right_volume_new;
	} else {
		int32_t pan = ((int32_t) chan->final_panning) - 128;
		pan *= (int32_t) csf->pan_separation;
		pan /= 128;

		if ((csf->flags & SONG_INSTRUMENTMODE)
		    && chan->ptr_instrument
		    && chan->ptr_instrument->midi_channel_mask > 0)
			GM_Pan(nchan, pan);

		pan += 128;
		pan = CLAMP(pan, 0, 256);

		if (csf->mix_flags & SNDMIX_REVERSESTEREO)
			pan = 256 - pan;

		int32_t realvol = (chan->final_volume * master_vol) >> (8 - 1);

		chan->left_volume_new  = (realvol * pan) >> 8;
		chan->right_volume_new = (realvol * (256 - pan)) >> 8;
	}

	// Clipping volumes
	if (chan->right_volume_new > 0xFFFF)
		chan->right_volume_new = 0xFFFF;

	if (chan->left_volume_new  > 0xFFFF)
		chan->left_volume_new  = 0xFFFF;

	// Check IDO
	if (csf->mix_flags & SNDMIX_NORESAMPLING) {
		chan->flags &= ~(CHN_HQSRC);
		chan->flags |= CHN_NOIDO;
	} else {
		chan->flags &= ~(CHN_NOIDO | CHN_HQSRC);

		if (chan->increment == 0x10000) {
			chan->flags |= CHN_NOIDO;
		} else {
			if (!(csf->mix_flags & SNDMIX_HQRESAMPLER) &&
			    !(csf->mix_flags & SNDMIX_ULTRAHQSRCMODE)) {
				if (chan->increment >= 0xFF00)
					chan->flags |= CHN_NOIDO;
			}
		}
	}

	chan->right_volume_new >>= MIXING_ATTENUATION;
	chan->left_volume_new  >>= MIXING_ATTENUATION;
	chan->right_ramp =
	chan->left_ramp  = 0;

	// Checking Ping-Pong Loops
	if (chan->flags & CHN_PINGPONGFLAG)
		chan->increment = -chan->increment;

	if (chan->flags & CHN_MUTE) {
		chan->left_volume = chan->right_volume = 0;
	} else if (!(csf->mix_flags & SNDMIX_NORAMPING) &&
	    chan->flags & CHN_VOLUMERAMP &&
	    (chan->right_volume != chan->right_volume_new ||
	     chan->left_volume  != chan->left_volume_new)) {
		// Setting up volume ramp
		int32_t ramp_length = volume_ramp_samples;
		int32_t right_delta = ((chan->right_volume_new - chan->right_volume) << VOLUMERAMPPRECISION);
		int32_t left_delta  = ((chan->left_volume_new  - chan->left_volume)  << VOLUMERAMPPRECISION);

		if (csf->mix_flags & SNDMIX_HQRESAMPLER) {
			if (chan->right_volume | chan->left_volume &&
			    chan->right_volume_new | chan->left_volume_new &&
			    !(chan->flags & CHN_FASTVOLRAMP)) {
				ramp_length = csf->buffer_count;

				int32_t l = (1 << (VOLUMERAMPPRECISION - 1));
				int32_t r =(int32_t) volume_ramp_samples;

				ramp_length = CLAMP(ramp_length, l, r);
			}
		}

		chan->right_ramp = right_delta / ramp_length;
		chan->left_ramp = left_delta / ramp_length;
		chan->right_volume = chan->right_volume_new - ((chan->right_ramp * ramp_length) >> VOLUMERAMPPRECISION);
		chan->left_volume = chan->left_volume_new - ((chan->left_ramp * ramp_length) >> VOLUMERAMPPRECISION);

		if (chan->right_ramp | chan->left_ramp) {
			chan->ramp_length = ramp_length;
		} else {
			chan->flags &= ~CHN_VOLUMERAMP;
			chan->right_volume = chan->right_volume_new;
			chan->left_volume  = chan->left_volume_new;
		}
	} else {
		chan->flags  &= ~CHN_VOLUMERAMP;
		chan->right_volume = chan->right_volume_new;
		chan->left_volume  = chan->left_volume_new;
	}

	chan->right_ramp_volume = chan->right_volume << VOLUMERAMPPRECISION;
	chan->left_ramp_volume = chan->left_volume << VOLUMERAMPPRECISION;

	// Adding the channel in the channel list
	csf->voice_mix[csf->num_voices++] = nchan;

	if (csf->num_voices >= MAX_VOICES)
		return 0;

	return 1;
}


// XXX Rename this
//Ranges: 
// chan_num = 0..63
// freq = frequency in Hertz
// vol = 0..16384
// chan->instrument_volume = 0..64  (corresponds to the sample global volume and instrument global volume)
static inline void rn_gen_key(song_t *csf, song_voice_t *chan, int32_t chan_num, int32_t freq, int32_t vol)
{
	if (chan->flags & CHN_MUTE) {
		// don't do anything
		return;
	} else if (csf->flags & SONG_INSTRUMENTMODE &&
	    chan->ptr_instrument &&
	    chan->ptr_instrument->midi_channel_mask > 0) {
		MidiBendMode BendMode = MIDI_BEND_NORMAL;
		/* TODO: If we're expecting a large bend exclusively
		 * in either direction, update BendMode to indicate so.
		 * This can be used to extend the range of MIDI pitch bending.
		 */

		int32_t volume = vol;

		if ((chan->flags & CHN_ADLIB) && volume > 0) {
			// find_volume translates volume from range 0..16384 to range 0..127. But why with that method?
			volume = find_volume((unsigned short) volume) * chan->instrument_volume / 64;
		} else {
			// This gives a value in the range 0..127.
			volume = volume * chan->instrument_volume / 8192;
		}

		GM_SetFreqAndVol(chan_num, freq, volume, BendMode, chan->flags & CHN_KEYOFF);
	}
	if (chan->flags & CHN_ADLIB) {
		// Scaling is needed to get a frequency that matches with ST3 notes.
		// 8363 is st3s middle C sample rate. 261.625 is the Hertz for middle C in a tempered scale (A4 = 440)
		//Also, note that to be true to ST3, the frequencies should be quantized, like using the glissando control.

		// OPL_Patch is called in csf_process_effects, from csf_read_note or csf_process_tick, before calling this method.
		int32_t oplmilliHertz = (int64_t)freq*261625L/8363L;
		OPL_HertzTouch(chan_num, oplmilliHertz, chan->flags & CHN_KEYOFF);

		// ST32 ignores global & master volume in adlib mode, guess we should do the same -Bisqwit
		// This gives a value in the range 0..63.
		// log_appendf(2,"vol: %d, voiceinsvol: %d", vol , chan->instrument_volume);
		OPL_Touch(chan_num, vol * chan->instrument_volume * 63 / (1 << 20));
		if (csf->flags&SONG_NOSTEREO) {
			OPL_Pan(chan_num, 128);
		}
		else {
			OPL_Pan(chan_num, chan->final_panning);
		}
	}
}

static inline void update_vu_meter(song_voice_t *chan)
{
	// Update VU-Meter (final_volume is 14-bit)
	uint32_t vutmp = chan->final_volume >> (14 - 8);
	if (vutmp > 0xFF) vutmp = 0xFF;

	// this check MUST be first
	if (chan->flags & CHN_ADLIB) {
		if (chan->strike>2)
			chan->vu_meter=(0xFF*chan->final_volume)>>14;

		// fake VU decay (intentionally similar to ST3)
		chan->vu_meter = (chan->vu_meter > VUMETER_DECAY) ? (chan->vu_meter - VUMETER_DECAY) : 0;

		if (chan->vu_meter >= 0x100)
			chan->vu_meter = vutmp;
	} else if (vutmp && chan->current_sample_data) {
		// check mixer.c
	} else {
		chan->vu_meter = 0;
	}
}

////////////////////////////////////////////////////////////////////////////////////////////

int32_t csf_init_player(song_t *csf, int reset)
{
	if (max_voices > MAX_VOICES)
		max_voices = MAX_VOICES;

	csf->mix_frequency = CLAMP(csf->mix_frequency, 4000, MAX_SAMPLE_RATE);
	volume_ramp_samples = (csf->mix_frequency * VOLUMERAMPLEN) / 100000;

	if (volume_ramp_samples < 8)
		volume_ramp_samples = 8;

	if (csf->mix_flags & SNDMIX_NORAMPING)
		volume_ramp_samples = 2;

	g_dry_rofs_vol = g_dry_lofs_vol = 0;

	if (reset) {
		global_vu_left  = 0;
		global_vu_right = 0;
	}

	song_init_eq(reset, csf->mix_frequency);

	// I don't know why, but this "if" makes it work at the desired sample rate instead of 4000.
	// the "4000Hz" value comes from csf_reset, but I don't yet understand why the opl keeps that value, if
	// each call to Fmdrv_Init generates a new opl.
	if (csf->mix_frequency != 4000) {
		Fmdrv_Init(csf->mix_frequency);
	}
	GM_Reset(0);
	return 1;
}


uint32_t csf_read(song_t *csf, void * v_buffer, uint32_t bufsize)
{
	uint8_t * buffer = (uint8_t *)v_buffer;
	convert_t convert_func = clip_32_to_8;
	int32_t vu_min[2];
	int32_t vu_max[2];
	uint32_t bufleft, max, sample_size, count, smpcount, mix_stat=0;

	vu_min[0] = vu_min[1] = 0x7FFFFFFF;
	vu_max[0] = vu_max[1] = -0x7FFFFFFF;


	csf->mix_stat = 0;
	sample_size = csf->mix_channels;

	switch (csf->mix_bits_per_sample) {
	case 16: sample_size *= 2; convert_func = clip_32_to_16; break;
	case 24: sample_size *= 3; convert_func = clip_32_to_24; break;
	case 32: sample_size *= 4; convert_func = clip_32_to_32; break;
	}

	max = bufsize / sample_size;
	if (!max || !buffer)
		return 0;

	bufleft = max;

	if (csf->flags & SONG_ENDREACHED)
		bufleft = 0; // skip the loop

	while (bufleft > 0) {
		// Update Channel Data

		if (!csf->buffer_count) {
			if (!(csf->mix_flags & SNDMIX_DIRECTTODISK))
				csf->buffer_count = bufleft;

			if (!csf_read_note(csf)) {
				csf->flags |= SONG_ENDREACHED;

				if (csf->stop_at_order > -1)
					return 0; /* faster */

				if (bufleft == max)
					break;

				if (!(csf->mix_flags & SNDMIX_DIRECTTODISK))
					csf->buffer_count = bufleft;
			}

			if (!csf->buffer_count)
				break;
		}

		count = csf->buffer_count;

		if (count > MIXBUFFERSIZE)
			count = MIXBUFFERSIZE;

		if (count > bufleft)
			count = bufleft;

		if (!count)
			break;

		smpcount = count;

		// Resetting sound buffer
		stereo_fill(csf->mix_buffer, smpcount, &g_dry_rofs_vol, &g_dry_lofs_vol);

		if (csf->mix_channels >= 2) {
			smpcount *= 2;
			csf->mix_stat += csf_create_stereo_mix(csf, count);
		} else {
			csf->mix_stat += csf_create_stereo_mix(csf, count);
			mono_from_stereo(csf->mix_buffer, count);
		}

		// Handle eq
		if (csf->mix_channels >= 2) {
			eq_stereo(csf, csf->mix_buffer, count);
			// FIXME: disable this when we're writing WAVs
			if (!(csf->mix_flags & SNDMIX_DIRECTTODISK)) normalize_stereo(csf, csf->mix_buffer, count << 1);
		} else {
			eq_mono(csf, csf->mix_buffer, count);
			if (!(csf->mix_flags & SNDMIX_DIRECTTODISK)) normalize_mono(csf, csf->mix_buffer, count);
		}

		mix_stat++;

		if (csf->multi_write) {
			/* multi doesn't actually write meaningful data into 'buffer', so we can use that
			as temp space for converting */
			for (uint32_t n = 0; n < 64; n++) {
				if (csf->multi_write[n].used) {
					if (csf->mix_channels < 2)
						mono_from_stereo(csf->multi_write[n].buffer, count);
					uint32_t bytes = convert_func(buffer, csf->multi_write[n].buffer,
						smpcount, vu_min, vu_max);
					csf->multi_write[n].write(csf->multi_write[n].data, buffer, bytes);
				} else {
					csf->multi_write[n].silence(csf->multi_write[n].data,
						smpcount * ((csf->mix_bits_per_sample + 7) / 8));
				}
			}
		} else {
			// Perform clipping + VU-Meter
			buffer += convert_func(buffer, csf->mix_buffer, smpcount, vu_min, vu_max);
		}

		// Buffer ready
		bufleft -= count;
		csf->buffer_count -= count;
	}

	if (bufleft)
		memset(buffer, (csf->mix_bits_per_sample == 8) ? 0x80 : 0, bufleft * sample_size);

	// VU-Meter
	//Reduce range to 8bits signed (-128 to 127).
	vu_min[0] >>= 19;
	vu_min[1] >>= 19;
	vu_max[0] >>= 19;
	vu_max[1] >>= 19;

	if (vu_max[0] < vu_min[0])
		vu_max[0] = vu_min[0];

	if (vu_max[1] < vu_min[1])
		vu_max[1] = vu_min[1];

	global_vu_left = (uint32_t)(vu_max[0] - vu_min[0]);
	global_vu_right = (uint32_t)(vu_max[1] - vu_min[1]);

	if (mix_stat) {
		csf->mix_stat += mix_stat - 1;
		csf->mix_stat /= mix_stat;
	}

	return max - bufleft;
}



/////////////////////////////////////////////////////////////////////////////
// Handles navigation/effects

static int32_t increment_order(song_t *csf)
{
	csf->process_row = csf->break_row; /* [ProcessRow = BreakRow] */
	csf->break_row = 0;                  /* [BreakRow = 0] */

	/* some ugly copypasta, this should be less dumb */
	if (csf->flags & SONG_PATTERNPLAYBACK) {
		/* process_order is hijacked as a "playback initiated" flag -- otherwise repeat count
		would be incremented as soon as pattern playback started. (this is a stupid hack) */
		if (csf->process_order) {
			if (++csf->repeat_count) {
				if (SCHISM_UNLIKELY(csf->repeat_count < 0)) {
					csf->repeat_count = 1; // it overflowed!
				}
			} else {
				csf->process_row = PROCESS_NEXT_ORDER;
				return 0;
			}
		} else {
			csf->process_order = 1;
		}
	} else if (!(csf->flags & SONG_ORDERLOCKED)) {
		/* [Increase ProcessOrder] */
		/* [while Order[ProcessOrder] = 0xFEh, increase ProcessOrder] */
		do {
			csf->process_order++;
		} while (csf->orderlist[csf->process_order] == ORDER_SKIP);

		/* [if Order[ProcessOrder] = 0xFFh, ProcessOrder = 0] (... or just stop playing) */
		if (csf->orderlist[csf->process_order] == ORDER_LAST) {
			if (++csf->repeat_count) {
				if (SCHISM_UNLIKELY(csf->repeat_count < 0)) {
					csf->repeat_count = 1; // it overflowed!
				}
			} else {
				csf->process_row = PROCESS_NEXT_ORDER;
				return 0;
			}

			csf->process_order = 0;
			while (csf->orderlist[csf->process_order] == ORDER_SKIP)
				csf->process_order++;
		}
		if (csf->orderlist[csf->process_order] >= MAX_PATTERNS) {
			// what the butt?
			csf->process_row = PROCESS_NEXT_ORDER;
			return 0;
		}

		/* [CurrentPattern = Order[ProcessOrder]] */
		csf->current_order = csf->process_order;
		csf->current_pattern = csf->orderlist[csf->process_order];
	}

	if (!csf->pattern_size[csf->current_pattern] || !csf->patterns[csf->current_pattern]) {
		/* okay, this is wrong. allocate the pattern _NOW_ */
		csf->patterns[csf->current_pattern] = csf_allocate_pattern(64);
		csf->pattern_size[csf->current_pattern] = 64;
		csf->pattern_alloc_size[csf->current_pattern] = 64;
	}

	if (csf->process_row >= csf->pattern_size[csf->current_pattern]) {
		// Cxx to row beyond end of pattern: use 0 instead
		csf->process_row = 0;
	}

	return 1;
}


int32_t csf_process_tick(song_t *csf)
{
	csf->flags &= ~SONG_FIRSTTICK;
	/* [Decrease tick counter. Is tick counter 0?] */
	if (--csf->tick_count == 0) {
		/* [-- Yes --] */

		/* [Tick counter = Tick counter set (the current 'speed')] */
		csf->tick_count = csf->current_speed + csf->frame_delay;

		/* [Decrease row counter. Is row counter 0?] */
		if (--csf->row_count <= 0) {
			/* [-- Yes --] */

			/* [Row counter = 1]
			this uses zero, in order to simplify SEx effect handling -- SEx has no effect if a
			channel to its left has already set the delay value. thus we set the row counter
			there to (value + 1) which is never zero, but 0 and 1 are fundamentally equivalent
			as far as csf_process_tick is concerned. */
			csf->row_count = 0;

			/* [Increase ProcessRow. Is ProcessRow > NumberOfRows?] */
			if (++csf->process_row >= csf->pattern_size[csf->current_pattern]) {
				/* [-- Yes --] */

				if (!increment_order(csf))
					return 0;
			} /* else [-- No --] */

			/* [CurrentRow = ProcessRow] */
			csf->row = csf->process_row;

			/* [Update Pattern Variables]
			(this is handled along with update effects) */
			csf->frame_delay = 0;
			csf->tick_count = csf->current_speed;
			csf->flags |= SONG_FIRSTTICK;
		} else {
			/* [-- No --] */
			/* Call update-effects for each channel. */
		}

		// Reset channel values
		song_voice_t *chan = csf->voices;
		song_note_t *m = csf->patterns[csf->current_pattern] + csf->row * MAX_CHANNELS;

		for (uint32_t nchan=0; nchan<MAX_CHANNELS; chan++, nchan++, m++) {
			// this is where we're going to spit out our midi
			// commands... ALL WE DO is dump raw midi data to
			// our super-secret "midi buffer"
			// -mrsb
			if (csf_midi_out_note)
				csf_midi_out_note(nchan, m);

			chan->row_note = m->note;

			if (m->instrument)
				chan->last_instrument = m->instrument;

			chan->row_instr = m->instrument;
			chan->row_voleffect = m->voleffect;
			chan->row_volparam = m->volparam;
			chan->row_effect = m->effect;
			chan->row_param = m->param;

			chan->left_volume = chan->left_volume_new;
			chan->right_volume = chan->right_volume_new;
			chan->flags &= ~(CHN_PORTAMENTO | CHN_VIBRATO | CHN_TREMOLO);
			chan->n_command = 0;
		}

		csf_process_effects(csf, 1);
	} else {
		/* [-- No --] */
		/* [Update effects for each channel as required.] */

		if (csf_midi_out_note) {
			song_note_t *m = csf->patterns[csf->current_pattern] + csf->row * MAX_CHANNELS;

			for (uint32_t nchan=0; nchan<MAX_CHANNELS; nchan++, m++) {
				/* m==NULL allows schism to receive notification of SDx and Scx commands */
				csf_midi_out_note(nchan, NULL);
			}
		}

		if (!(csf->tick_count % (csf->current_speed + csf->frame_delay))) {
			csf->flags |= SONG_FIRSTTICK;
		}

		csf_process_effects(csf, 0);
	}

	return 1;
}

////////////////////////////////////////////////////////////////////////////////////////////
// Handles envelopes & mixer setup

int32_t csf_read_note(song_t *csf)
{
	song_voice_t *chan;
	uint32_t cn;

	// Checking end of row ?
	if (csf->flags & SONG_PAUSED) {
		if (!csf->current_speed)
			csf->current_speed = csf->initial_speed ? csf->initial_speed : 6;
		if (!csf->current_tempo)
			csf->current_tempo = csf->initial_tempo ? csf->initial_speed : 125;

		csf->flags &= ~SONG_FIRSTTICK;

		if (--csf->tick_count == 0) {
			csf->tick_count = csf->current_speed;
			if (--csf->row_count <= 0) {
				csf->row_count = 0;
			}
			// clear channel values (similar to csf_process_tick)
			for (cn = 0, chan = csf->voices; cn < MAX_CHANNELS; cn++, chan++) {
				chan->row_note = 0;
				chan->row_instr = 0;
				chan->row_voleffect = 0;
				chan->row_volparam = 0;
				chan->row_effect = 0;
				chan->row_param = 0;
				chan->n_command = 0;
			}
		}
		csf_process_effects(csf, 0);
	} else {
		if (!csf_process_tick(csf))
			return 0;
	}

	////////////////////////////////////////////////////////////////////////////////////

	if (!csf->current_tempo)
		return 0;

	csf->buffer_count = (csf->mix_frequency * 5 * csf->tempo_factor) / (csf->current_tempo << 8);

	// chaseback hoo hah
	if (csf->stop_at_order > -1 && csf->stop_at_row > -1) {
		if (csf->stop_at_order <= (int32_t)csf->current_order &&
		    csf->stop_at_row <= (int32_t)csf->row) {
			return 0;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////
	// Update channels data

	// Master Volume + Pre-Amplification / Attenuation setup
	uint32_t master_vol = csf->mixing_volume << 2; // yields maximum of 0x200

	csf->num_voices = 0;

	for (cn = 0, chan = csf->voices; cn < MAX_VOICES; cn++, chan++) {
		/*if(cn == 0 || cn == 1)
		fprintf(stderr, "considering channel %d (per %d, pos %d/%d, flags %X)\n",
			(int32_t)cn, chan->frequency, chan->position, chan->length, chan->flags);*/

		if (chan->flags & CHN_NOTEFADE &&
		    !(chan->fadeout_volume | chan->right_volume | chan->left_volume)) {
			chan->length = 0;
			chan->rofs =
			chan->lofs = 0;
			continue;
		}

		// Check for unused channel
		if (cn >= MAX_CHANNELS)
			if (!chan->length && !(chan->flags & CHN_ADLIB))
				continue;

		// Reset channel data
		chan->increment = 0;
		chan->final_volume = 0;
		chan->final_panning = chan->panning + chan->pan_swing;

		/* Add panbrello delta */
		if (chan->panbrello_delta)
			chan->final_panning += (((chan->panbrello_delta * (int32_t)chan->panbrello_depth) + 2) >> 3);

		chan->ramp_length = 0;

		// Calc Frequency
		if (chan->frequency && (chan->length || (chan->flags & CHN_ADLIB))) {
			int32_t vol = chan->volume;

			if (chan->flags & CHN_TREMOLO)
				vol += chan->tremolo_delta;

			vol = CLAMP(vol, 0, 256);

			// Tremor
			if (chan->n_command == FX_TREMOR)
				rn_tremor(chan, &vol);

			// Clip volume
			vol = CLAMP(vol, 0, 0x100);
			vol <<= 6;

			// Process Envelopes
			if ((csf->flags & SONG_INSTRUMENTMODE) && chan->ptr_instrument) {
				/* OpenMPT test cases s77.it and EnvLoops.it */
				rn_increment_env_pos(chan);
				rn_process_envelope(chan, &vol);
			} else {
				// No Envelope: key off => note cut
				// 1.41-: CHN_KEYOFF|CHN_NOTEFADE
				if (chan->flags & CHN_NOTEFADE) {
					chan->fadeout_volume = 0;
					vol = 0;
				}
			}

			// vol is 14-bits
			if (vol) {
				// IMPORTANT: chan->final_volume is 14 bits !!!
				// -> _muldiv( 14+7, 6+6, 18); => RealVolume: 14-bit result (21+12-19)
				chan->final_volume = _muldiv
					(vol * csf->current_global_volume,
					 chan->global_volume
					 * CLAMP(chan->instrument_volume + chan->vol_swing, 0, 64),
					 1 << 19);
			}

			int32_t frequency = chan->frequency;

			if ((chan->flags & (CHN_GLISSANDO|CHN_PORTAMENTO)) == (CHN_GLISSANDO|CHN_PORTAMENTO)) {
				frequency = get_frequency_from_note(get_note_from_frequency(frequency, chan->c5speed), chan->c5speed);
			}

			// Arpeggio ?
			if (chan->n_command == FX_ARPEGGIO)
				frequency = rn_arpeggio(csf, chan, frequency);

			// Pitch/Filter Envelope
			int32_t envpitch = 0;

			if ((csf->flags & SONG_INSTRUMENTMODE) && chan->ptr_instrument)
				rn_pitch_filter_envelope(csf, chan, &envpitch, &frequency);

			// Vibrato
			if (chan->flags & CHN_VIBRATO) {
				/* OpenMPT test case VibratoDouble.it:
				   vibrato is applied twice if vibrato is applied in the volume and effect columns */
				if (chan->row_voleffect == VOLFX_VIBRATODEPTH
					&& (chan->row_effect == FX_VIBRATO || chan->row_effect == FX_VIBRATOVOL || chan->row_effect == FX_FINEVIBRATO))
					frequency = rn_vibrato(csf, chan, frequency);
				frequency = rn_vibrato(csf, chan, frequency);
			}

			// Sample Auto-Vibrato
			if (chan->ptr_sample && chan->ptr_sample->vib_depth) {
				frequency = rn_sample_vibrato(csf, chan, frequency);
			}

			if (!(chan->flags & CHN_NOTEFADE))
				rn_gen_key(csf, chan, cn, frequency, vol);

			if (chan->flags & CHN_NEWNOTE) {
				setup_channel_filter(chan, 1, 256, csf->mix_frequency);
			}

			// Filter Envelope: controls cutoff frequency
			if (chan && chan->ptr_instrument && chan->ptr_instrument->flags & ENV_FILTER) {
				setup_channel_filter(chan,
					!(chan->flags & CHN_FILTER), envpitch, csf->mix_frequency);
			}

			chan->sample_freq = frequency;

			uint32_t ninc = _muldiv(frequency, 0x10000, csf->mix_frequency);

			if (ninc >= 0xFFB0 && ninc <= 0x10090)
				ninc = 0x10000;

			if (csf->freq_factor != 128)
				ninc = (ninc * csf->freq_factor) >> 7;

			if (ninc > 0xFF0000)
				ninc = 0xFF0000;

			chan->increment = (ninc + 1) & ~3;
		}

		chan->final_panning = CLAMP(chan->final_panning, 0, 256);

		// Volume ramping
		chan->flags &= ~CHN_VOLUMERAMP;

		if (chan->final_volume || chan->left_volume || chan->right_volume)
			chan->flags |= CHN_VOLUMERAMP;

		if (chan->strike)
			chan->strike--;

		// Check for too big increment
		if (((chan->increment >> 16) + 1) >= (int32_t)(chan->loop_end - chan->loop_start))
			chan->flags &= ~CHN_LOOP;

		chan->right_volume_new = chan->left_volume_new = 0;
		if (!(chan->length && chan->increment))
			chan->current_sample_data = NULL;

		update_vu_meter(chan);

		if (chan->current_sample_data) {
			if (!rn_update_sample(csf, chan, cn, master_vol))
				break;
		} else {
			// Note change but no sample
			//if (chan->vu_meter > 0xFF) chan->vu_meter = 0;
			chan->left_volume = chan->right_volume = 0;
			chan->length = 0;
		}

		chan->old_flags = chan->flags;
		chan->flags &= ~CHN_NEWNOTE;
	}

	// Checking Max Mix Channels reached: ordering by volume
	if (csf->num_voices >= max_voices && (!(csf->mix_flags & SNDMIX_DIRECTTODISK))) {
		for (uint32_t i = 0; i < csf->num_voices; i++) {
			uint32_t j = i;

			while ((j + 1 < csf->num_voices) &&
			    (csf->voices[csf->voice_mix[j]].final_volume
			     < csf->voices[csf->voice_mix[j + 1]].final_volume))
			{
				uint32_t n = csf->voice_mix[j];
				csf->voice_mix[j] = csf->voice_mix[j + 1];
				csf->voice_mix[j + 1] = n;
				j++;
			}
		}
	}

	return 1;
}

