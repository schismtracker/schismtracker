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

#include "player/sndfile.h"

#include "player/cmixer.h"
#include "player/snd_fm.h"
#include "player/snd_gm.h"
#include "player/tables.h"

#include "util.h" /* for clamp/min */

#include <math.h>


// see also csf_midi_out_note in sndmix.c
void (*csf_midi_out_raw)(const unsigned char *,uint32_t, uint32_t) = NULL;

/* --------------------------------------------------------------------------------------------------------- */
/* note/freq conversion functions */

int32_t get_note_from_frequency(int32_t frequency, uint32_t c5speed)
{
	int32_t n;
	if (!frequency)
		return 0;
	for (n = 0; n <= 120; n++) {
		/* Essentially, this is just doing a note_to_frequency(n, 8363), but with less
		computation since there's no c5speed to deal with. */
		if (frequency <= get_frequency_from_note(n + 1, c5speed))
			return n + 1;
	}
	return 120;
}

int32_t get_frequency_from_note(int32_t note, uint32_t c5speed)
{
	if (!note || note > 0xF0)
		return 0;
	note--;
	return _muldiv(c5speed, linear_slide_up_table[(note % 12) * 16] << (note / 12), 65536 << 5);
}


uint32_t transpose_to_frequency(int32_t transp, int32_t ftune)
{
	return (uint32_t) (8363.0 * pow(2, (transp * 128.0 + ftune) / 1536.0));
}

int32_t frequency_to_transpose(uint32_t freq)
{
	return (int32_t) (1536.0 * (log(freq / 8363.0) / log(2)));
}

uint32_t calc_halftone(uint32_t hz, int32_t rel)
{
	return pow(2, rel / 12.0) * hz + 0.5;
}

/* --------------------------------------------------------------------------------------------------------- */
/* the full content of snd_fx.cpp follows. */


////////////////////////////////////////////////////////////
// Channels effects

void fx_note_cut(song_t *csf, uint32_t nchan, int clear_note)
{
	song_voice_t *chan = &csf->voices[nchan];
	// stop the current note:
	chan->flags |= CHN_FASTVOLRAMP;
	//if (chan->ptr_instrument) chan->volume = 0;
	chan->length = 0; /* tentative fix: tremor breaks without this, but OpenMPT doesn't do this at all (???) */
	chan->increment = 0;
	if (clear_note) {
		// keep instrument numbers from picking up old notes
		// (SCx doesn't do this)
		chan->frequency = 0;
	}

	if (chan->flags & CHN_ADLIB) {
		//Do this only if really an adlib chan. Important!
		OPL_NoteOff(nchan);
		OPL_Touch(nchan, 0);
	}
	GM_KeyOff(nchan);
	GM_Touch(nchan, 0);
}

void fx_key_off(song_t *csf, uint32_t nchan)
{
	song_voice_t *chan = &csf->voices[nchan];

	/*fprintf(stderr, "KeyOff[%d] [ch%u]: flags=0x%X\n",
		tick_count, (unsigned)nchan, chan->flags);*/
	if (chan->flags & CHN_ADLIB) {
		//Do this only if really an adlib chan. Important!
		OPL_NoteOff(nchan);
	}
	GM_KeyOff(nchan);

	song_instrument_t *penv = (csf->flags & SONG_INSTRUMENTMODE) ? chan->ptr_instrument : NULL;

	/*if ((chan->flags & CHN_ADLIB)
	||  (penv && penv->midi_channel_mask))
	{
		// When in AdLib / MIDI mode, end the sample
		chan->flags |= CHN_FASTVOLRAMP;
		chan->length = 0;
		chan->position    = 0;
		return;
	}*/

	chan->flags |= CHN_KEYOFF;
	//if ((!chan->ptr_instrument) || (!(chan->flags & CHN_VOLENV)))
	if ((csf->flags & SONG_INSTRUMENTMODE) && chan->ptr_instrument && !(chan->flags & CHN_VOLENV)) {
		chan->flags |= CHN_NOTEFADE;
	}
	if (!chan->length)
		return;
	if ((chan->flags & CHN_SUSTAINLOOP) && chan->ptr_sample) {
		song_sample_t *psmp = chan->ptr_sample;
		if (psmp->flags & CHN_LOOP) {
			if (psmp->flags & CHN_PINGPONGLOOP)
				chan->flags |= CHN_PINGPONGLOOP;
			else
				chan->flags &= ~(CHN_PINGPONGLOOP|CHN_PINGPONGFLAG);
			chan->flags |= CHN_LOOP;
			chan->length = psmp->length;
			chan->loop_start = psmp->loop_start;
			chan->loop_end = psmp->loop_end;
			if (chan->length > chan->loop_end) chan->length = chan->loop_end;
			if (chan->position >= chan->length)
				chan->position = chan->position - chan->length + chan->loop_start;
		} else {
			chan->flags &= ~(CHN_LOOP|CHN_PINGPONGLOOP|CHN_PINGPONGFLAG);
			chan->length = psmp->length;
		}
	}
	if (penv && penv->fadeout && (penv->flags & ENV_VOLLOOP))
		chan->flags |= CHN_NOTEFADE;
}


// negative value for slide = down, positive = up
int32_t csf_fx_do_freq_slide(uint32_t flags, int32_t frequency, int32_t slide, int is_tone_portamento)
{
	// IT Linear slides
	if (!frequency) return 0;
	if (flags & SONG_LINEARSLIDES) {
		int32_t old_frequency = frequency;
		uint32_t n = abs(slide);
		if (n > 255 * 4) n = 255 * 4;

		if (slide > 0) {
			if (n < 16)
				frequency = _muldivr(frequency, fine_linear_slide_up_table[n], 65536);
			else
				frequency = _muldivr(frequency, linear_slide_up_table[n / 4], 65536);
			if (old_frequency == frequency)
				frequency++;
		} else if (slide < 0) {
			if (n < 16)
				frequency = _muldivr(frequency, fine_linear_slide_down_table[n], 65536);
			else
				frequency = _muldivr(frequency, linear_slide_down_table[n / 4], 65536);
			if (old_frequency == frequency)
				frequency--;
		}
	} else {
		if (slide < 0) {
			frequency = (int32_t)((1712 * 8363 * (int64_t)frequency) / (((int64_t)(frequency) * -slide) + 1712 * 8363));
		} else if (slide > 0) {
			int32_t frequency_div = 1712 * 8363 - ((int64_t)(frequency) * slide);
			if (frequency_div <= 0) {
				if (is_tone_portamento)
					frequency_div = 1;
				else
					return 0;
			}
			int64_t freq = ((1712 * 8363 * (int64_t)frequency) / frequency_div);
			if (freq > INT32_MAX)
				frequency = INT32_MAX;
			else
				frequency = (int32_t)freq;
		}
	}
	return frequency;
}

static void set_instrument_panning(song_voice_t *chan, int32_t panning)
{
	chan->channel_panning = (int16_t)(chan->panning + 1);
	if (chan->flags & CHN_SURROUND)
		chan->channel_panning |= 0x8000;
	chan->panning = panning;
	chan->flags &= ~CHN_SURROUND;
}

static void fx_fine_portamento_up(uint32_t flags, song_voice_t *chan, uint32_t param)
{
	if ((flags & SONG_FIRSTTICK) && chan->frequency && param) {
		chan->frequency = csf_fx_do_freq_slide(flags, chan->frequency, param * 4, 0);
	}
}

static void fx_fine_portamento_down(uint32_t flags, song_voice_t *chan, uint32_t param)
{
	if ((flags & SONG_FIRSTTICK) && chan->frequency && param) {
		chan->frequency = csf_fx_do_freq_slide(flags, chan->frequency, param * -4, 0);
	}
}

static void fx_extra_fine_portamento_up(uint32_t flags, song_voice_t *chan, uint32_t param)
{
	if ((flags & SONG_FIRSTTICK) && chan->frequency && param) {
		chan->frequency = csf_fx_do_freq_slide(flags, chan->frequency, param, 0);
	}
}

static void fx_extra_fine_portamento_down(uint32_t flags, song_voice_t *chan, uint32_t param)
{
	if ((flags & SONG_FIRSTTICK) && chan->frequency && param) {
		chan->frequency = csf_fx_do_freq_slide(flags, chan->frequency, -(int)param, 0);
	}
}

static void fx_reg_portamento_up(uint32_t flags, song_voice_t *chan, uint32_t param)
{
	if (!(flags & SONG_FIRSTTICK))
		chan->frequency = csf_fx_do_freq_slide(flags, chan->frequency, (int)(param * 4), 0);
}

static void fx_reg_portamento_down(uint32_t flags, song_voice_t *chan, uint32_t param)
{
	if (!(flags & SONG_FIRSTTICK))
		chan->frequency = csf_fx_do_freq_slide(flags, chan->frequency, -(int)(param * 4), 0);
}


static void fx_portamento_up(uint32_t flags, song_voice_t *chan, uint32_t param)
{
	if (!param)
		param = chan->mem_pitchslide;

	switch (param & 0xf0) {
	case 0xe0:
		fx_extra_fine_portamento_up(flags, chan, param & 0x0F);
		break;
	case 0xf0:
		fx_fine_portamento_up(flags, chan, param & 0x0F);
		break;
	default:
		fx_reg_portamento_up(flags, chan, param);
		break;
	}
}

static void fx_portamento_down(uint32_t flags, song_voice_t *chan, uint32_t param)
{
	if (!param)
		param = chan->mem_pitchslide;

	switch (param & 0xf0) {
	case 0xe0:
		fx_extra_fine_portamento_down(flags, chan, param & 0x0F);
		break;
	case 0xf0:
		fx_fine_portamento_down(flags, chan, param & 0x0F);
		break;
	default:
		fx_reg_portamento_down(flags, chan, param);
		break;
	}
}

static void fx_tone_portamento(uint32_t flags, song_voice_t *chan, uint32_t param)
{
	chan->flags |= CHN_PORTAMENTO;
	if (chan->frequency && chan->portamento_target && !(flags & SONG_FIRSTTICK)) {
		if (!param && chan->row_effect == FX_TONEPORTAVOL)
		{
			if (chan->frequency > 1 && (flags & SONG_LINEARSLIDES))
				chan->frequency--;
			if (chan->frequency < chan->portamento_target) {
				chan->frequency = chan->portamento_target;
				chan->portamento_target = 0;
			}
		} else if (param && chan->frequency < chan->portamento_target) {
			chan->frequency = csf_fx_do_freq_slide(flags, chan->frequency, param * 4, 1);
			if (chan->frequency >= chan->portamento_target) {
				chan->frequency = chan->portamento_target;
				chan->portamento_target = 0;
			}
		} else if (param && chan->frequency >= chan->portamento_target) {
			chan->frequency = csf_fx_do_freq_slide(flags, chan->frequency, param * -4, 1);
			if (chan->frequency < chan->portamento_target) {
				chan->frequency = chan->portamento_target;
				chan->portamento_target = 0;
			}
		}
	}
}

// Implemented for IMF compatibility, can't actually save this in any formats
// sign should be 1 (up) or -1 (down)
static void fx_note_slide(uint32_t flags, song_voice_t *chan, uint32_t param, int32_t sign)
{
	uint8_t x, y;
	if (flags & SONG_FIRSTTICK) {
		x = param & 0xf0;
		if (x)
			chan->note_slide_speed = (x >> 4);
		y = param & 0xf;
		if (y)
			chan->note_slide_step = y;
		chan->note_slide_counter = chan->note_slide_speed;
	} else {
		if (--chan->note_slide_counter == 0) {
			chan->note_slide_counter = chan->note_slide_speed;
			// update it
			chan->frequency = get_frequency_from_note
				(sign * chan->note_slide_step + get_note_from_frequency(chan->frequency, chan->c5speed),
					chan->c5speed);
		}
	}
}



static void fx_vibrato(song_voice_t *p, uint32_t param)
{
	if (param & 0x0F)
		p->vibrato_depth = (param & 0x0F) * 4;
	if (param & 0xF0)
		p->vibrato_speed = (param >> 4) & 0x0F;
	p->flags |= CHN_VIBRATO;
}

static void fx_fine_vibrato(song_voice_t *p, uint32_t param)
{
	if (param & 0x0F)
		p->vibrato_depth = param & 0x0F;
	if (param & 0xF0)
		p->vibrato_speed = (param >> 4) & 0x0F;
	p->flags |= CHN_VIBRATO;
}


static void fx_panbrello(song_voice_t *chan, uint32_t param)
{
	uint32_t panpos = chan->panbrello_position & 0xFF;
	int pdelta = chan->panbrello_delta;

	if (param & 0x0F)
		chan->panbrello_depth = param & 0x0F;
	if (param & 0xF0)
		chan->panbrello_speed = (param >> 4) & 0x0F;

	switch (chan->panbrello_type) {
	case VIB_SINE:
	default:
		pdelta = sine_table[panpos];
		break;
	case VIB_RAMP_DOWN:
		pdelta = ramp_down_table[panpos];
		break;
	case VIB_SQUARE:
		pdelta = square_table[panpos];
		break;
	case VIB_RANDOM:
		pdelta = (rand() & 0x7F) - 0x40;
		break;
	}

	/* OpenMPT test case RandomWaveform.it:
	   Speed for random panbrello says how many ticks the value should be used */
	if (chan->panbrello_type == VIB_RANDOM) {
		if (!chan->panbrello_position || chan->panbrello_position >= chan->panbrello_speed)
			chan->panbrello_position = 0;

		chan->panbrello_position++;
	} else {
		chan->panbrello_position += chan->panbrello_speed;
	}

	chan->panbrello_delta = pdelta;
}


static void fx_volume_up(song_voice_t *chan, uint32_t param)
{
	chan->volume += param * 4;
	if (chan->volume > 256)
		chan->volume = 256;
}

static void fx_volume_down(song_voice_t *chan, uint32_t param)
{
	chan->volume -= param * 4;
	if (chan->volume < 0)
		chan->volume = 0;
}

static void fx_volume_slide(uint32_t flags, song_voice_t *chan, uint32_t param)
{
	// Dxx     Volume slide down
	//
	// if (xx == 0) then xx = last xx for (Dxx/Kxx/Lxx) for this channel.
	if (param)
		chan->mem_volslide = param;
	else
		param = chan->mem_volslide;

	// Order of testing: Dx0, D0x, DxF, DFx
	if (param == (param & 0xf0)) {
		// Dx0     Set effect update for channel enabled if channel is ON.
		//         If x = F, then slide up volume by 15 straight away also (for S3M compat)
		//         Every update, add x to the volume, check and clip values > 64 to 64
		param >>= 4;
		if (param == 0xf || !(flags & SONG_FIRSTTICK))
			fx_volume_up(chan, param);
	} else if (param == (param & 0xf)) {
		// D0x     Set effect update for channel enabled if channel is ON.
		//         If x = F, then slide down volume by 15 straight away also (for S3M)
		//         Every update, subtract x from the volume, check and clip values < 0 to 0
		if (param == 0xf || !(flags & SONG_FIRSTTICK))
			fx_volume_down(chan, param);
	} else if ((param & 0xf) == 0xf) {
		// DxF     Add x to volume straight away. Check and clip values > 64 to 64
		param >>= 4;
		if (flags & SONG_FIRSTTICK)
			fx_volume_up(chan, param);
	} else if ((param & 0xf0) == 0xf0) {
		// DFx     Subtract x from volume straight away. Check and clip values < 0 to 0
		param &= 0xf;
		if (flags & SONG_FIRSTTICK)
			fx_volume_down(chan, param);
	}
}


static void fx_panning_slide(uint32_t flags, song_voice_t *chan, uint32_t param)
{
	int32_t slide = 0;
	if (param)
		chan->mem_panslide = param;
	else
		param = chan->mem_panslide;
	if ((param & 0x0F) == 0x0F && (param & 0xF0)) {
		if (flags & SONG_FIRSTTICK) {
			param = (param & 0xF0) >> 2;
			slide = - (int)param;
		}
	} else if ((param & 0xF0) == 0xF0 && (param & 0x0F)) {
		if (flags & SONG_FIRSTTICK) {
			slide = (param & 0x0F) << 2;
		}
	} else {
		if (!(flags & SONG_FIRSTTICK)) {
			if (param & 0x0F)
				slide = (int)((param & 0x0F) << 2);
			else
				slide = -(int)((param & 0xF0) >> 2);
		}
	}
	if (slide) {
		slide += chan->panning;
		chan->panning = CLAMP(slide, 0, 256);
		chan->channel_panning = 0;
	}
	chan->flags &= ~CHN_SURROUND;
	chan->panbrello_delta = 0;
}


static void fx_tremolo(uint32_t flags, song_voice_t *chan, uint32_t param)
{
	unsigned int trempos = chan->tremolo_position & 0xFF;
	int tdelta;

	if (param & 0x0F)
		chan->tremolo_depth = (param & 0x0F) << 2;
	if (param & 0xF0)
		chan->tremolo_speed = (param >> 4) & 0x0F;

	chan->flags |= CHN_TREMOLO;

	// don't handle on first tick if old-effects mode
	if ((flags & SONG_FIRSTTICK) && (flags & SONG_ITOLDEFFECTS))
		return;

	switch (chan->tremolo_type) {
	case VIB_SINE:
	default:
		tdelta = sine_table[trempos];
		break;
	case VIB_RAMP_DOWN:
		tdelta = ramp_down_table[trempos];
		break;
	case VIB_SQUARE:
		tdelta = square_table[trempos];
		break;
	case VIB_RANDOM:
		tdelta = 128 * ((double) rand() / RAND_MAX) - 64;
		break;
	}

	chan->tremolo_position = (trempos + 4 * chan->tremolo_speed) & 0xFF;
	tdelta = (tdelta * (int)chan->tremolo_depth) >> 5;
	chan->tremolo_delta = tdelta;
}


static void fx_retrig_note(song_t *csf, uint32_t nchan, uint32_t param)
{
	song_voice_t *chan = &csf->voices[nchan];

	//printf("Q%02X note=%02X tick%d  %d\n", param, chan->row_note, tick_count, chan->cd_retrig);
	if ((csf->flags & SONG_FIRSTTICK) && chan->row_note != NOTE_NONE) {
		chan->cd_retrig = param & 0xf;
	} else if (--chan->cd_retrig <= 0) {
		
		// in Impulse Tracker, retrig only works if a sample is currently playing in the channel
		if (chan->position == 0)
			return;
		
		chan->cd_retrig = param & 0xf;
		param >>= 4;
		if (param) {
			int vol = chan->volume;
			if (retrig_table_1[param])
				vol = (vol * retrig_table_1[param]) >> 4;
			else
				vol += (retrig_table_2[param]) << 2;
			chan->volume = CLAMP(vol, 0, 256);
			chan->flags |= CHN_FASTVOLRAMP;
		}

		uint32_t note = chan->new_note;
		int32_t frequency = chan->frequency;
		if (NOTE_IS_NOTE(note) && chan->length)
			csf_check_nna(csf, nchan, 0, note, 1);
		csf_note_change(csf, nchan, note, 1, 1, 0);
		if (frequency && chan->row_note == NOTE_NONE)
			chan->frequency = frequency;
		chan->position = chan->position_frac = 0;
	}
}


static void fx_channel_vol_slide(uint32_t flags, song_voice_t *chan, uint32_t param)
{
	int32_t slide = 0;
	if (param)
		chan->mem_channel_volslide = param;
	else
		param = chan->mem_channel_volslide;
	if ((param & 0x0F) == 0x0F && (param & 0xF0)) {
		if (flags & SONG_FIRSTTICK)
			slide = param >> 4;
	} else if ((param & 0xF0) == 0xF0 && (param & 0x0F)) {
		if (flags & SONG_FIRSTTICK)
			slide = - (int)(param & 0x0F);
	} else {
		if (!(flags & SONG_FIRSTTICK)) {
			if (param & 0x0F)
				slide = -(int)(param & 0x0F);
			else
				slide = (int)((param & 0xF0) >> 4);
		}
	}
	if (slide) {
		slide += chan->global_volume;
		chan->global_volume = CLAMP(slide, 0, 64);
	}
}


static void fx_global_vol_slide(song_t *csf, song_voice_t *chan, uint32_t param)
{
	int32_t slide = 0;
	if (param)
		chan->mem_global_volslide = param;
	else
		param = chan->mem_global_volslide;
	if ((param & 0x0F) == 0x0F && (param & 0xF0)) {
		if (csf->flags & SONG_FIRSTTICK)
			slide = param >> 4;
	} else if ((param & 0xF0) == 0xF0 && (param & 0x0F)) {
		if (csf->flags & SONG_FIRSTTICK)
			slide = -(int)(param & 0x0F);
	} else {
		if (!(csf->flags & SONG_FIRSTTICK)) {
			if (param & 0xF0)
				slide = (int)((param & 0xF0) >> 4);
			else
				slide = -(int)(param & 0x0F);
		}
	}
	if (slide) {
		slide += csf->current_global_volume;
		csf->current_global_volume = CLAMP(slide, 0, 128);
	}
}


static void fx_pattern_loop(song_t *csf, song_voice_t *chan, uint32_t param)
{
	if (param) {
		if (chan->cd_patloop) {
			if (!--chan->cd_patloop) {
				// this should get rid of that nasty infinite loop for cases like
				//     ... .. .. SB0
				//     ... .. .. SB1
				//     ... .. .. SB1
				// it still doesn't work right in a few strange cases, but oh well :P
				chan->patloop_row = csf->row + 1;
				csf->patloop = 0;
				return; // don't loop!
			}
		} else {
			chan->cd_patloop = param;
		}
		csf->process_row = chan->patloop_row - 1;
	} else {
		csf->patloop = 1;
		chan->patloop_row = csf->row;
	}
}


static void fx_special(song_t *csf, uint32_t nchan, uint32_t param)
{
	song_voice_t *chan = &csf->voices[nchan];
	uint32_t command = param & 0xF0;
	param &= 0x0F;
	switch(command) {
	// S0x: Set Filter
	// S1x: Set Glissando Control
	case 0x10:
		chan->flags &= ~CHN_GLISSANDO;
		if (param) chan->flags |= CHN_GLISSANDO;
		break;
	// S2x: Set FineTune (no longer implemented)
	// S3x: Set Vibrato WaveForm
	case 0x30:
		chan->vib_type = param;
		break;
	// S4x: Set Tremolo WaveForm
	case 0x40:
		chan->tremolo_type = param;
		break;
	// S5x: Set Panbrello WaveForm
	case 0x50:
		/* some mpt compat thing */
		chan->panbrello_type = (param < 0x04) ? param : 0;
		chan->panbrello_position = 0;
		break;
	// S6x: Pattern Delay for x ticks
	case 0x60:
		if (csf->flags & SONG_FIRSTTICK) {
			csf->frame_delay += param;
			csf->tick_count += param;
		}
		break;
	// S7x: Envelope Control
	case 0x70:
		if (!(csf->flags & SONG_FIRSTTICK))
			break;
		switch(param) {
		case 0:
		case 1:
		case 2:
			{
				song_voice_t *bkp = &csf->voices[MAX_CHANNELS];
				for (uint32_t i=MAX_CHANNELS; i<MAX_VOICES; i++, bkp++) {
					if (bkp->master_channel == nchan+1) {
						if (param == 1) {
							fx_key_off(csf, i);
						} else if (param == 2) {
							bkp->flags |= CHN_NOTEFADE;
						} else {
							bkp->flags |= CHN_NOTEFADE;
							bkp->fadeout_volume = 0;
						}
					}
				}
			}
			break;
		case  3:        chan->nna = NNA_NOTECUT; break;
		case  4:        chan->nna = NNA_CONTINUE; break;
		case  5:        chan->nna = NNA_NOTEOFF; break;
		case  6:        chan->nna = NNA_NOTEFADE; break;
		case  7:        chan->flags &= ~CHN_VOLENV; break;
		case  8:        chan->flags |= CHN_VOLENV; break;
		case  9:        chan->flags &= ~CHN_PANENV; break;
		case 10:        chan->flags |= CHN_PANENV; break;
		case 11:        chan->flags &= ~CHN_PITCHENV; break;
		case 12:        chan->flags |= CHN_PITCHENV; break;
		}
		break;
	// S8x: Set 4-bit Panning
	case 0x80:
		if (csf->flags & SONG_FIRSTTICK) {
			chan->flags &= ~CHN_SURROUND;
			chan->panbrello_delta = 0;
			chan->panning = (param << 4) + 8;
			chan->channel_panning = 0;
			chan->flags |= CHN_FASTVOLRAMP;
			chan->pan_swing = 0;
		}
		break;
	// S9x: Set Surround
	case 0x90:
		if (param == 1 && (csf->flags & SONG_FIRSTTICK)) {
			chan->flags |= CHN_SURROUND;
			chan->panbrello_delta = 0;
			chan->panning = 128;
			chan->channel_panning = 0;
		}
		break;
	// SAx: Set 64k Offset
	// Note: don't actually APPLY the offset, and don't clear the regular offset value, either.
	case 0xA0:
		if (csf->flags & SONG_FIRSTTICK) {
			chan->mem_offset = (param << 16) | (chan->mem_offset & ~0xf0000);
		}
		break;
	// SBx: Pattern Loop
	case 0xB0:
		if (csf->flags & SONG_FIRSTTICK)
			fx_pattern_loop(csf, chan, param & 0x0F);
		break;
	// SCx: Note Cut
	case 0xC0:
		if (csf->flags & SONG_FIRSTTICK)
			chan->cd_note_cut = param ? param : 1;
		else if (--chan->cd_note_cut == 0)
			fx_note_cut(csf, nchan, 1);
		break;
	// SDx: Note Delay
	// SEx: Pattern Delay for x rows
	case 0xE0:
		if (csf->flags & SONG_FIRSTTICK) {
			if (!csf->row_count) // ugh!
				csf->row_count = param + 1;
		}
		break;
	// SFx: Set Active Midi Macro
	case 0xF0:
		chan->active_macro = param;
		break;
	}
}


// Send exactly one MIDI message
void csf_midi_send(song_t *csf, const unsigned char *data, uint32_t len, uint32_t nchan, int fake)
{
	song_voice_t *chan = &csf->voices[nchan];

	if (len >= 1 && (data[0] == 0xFA || data[0] == 0xFC || data[0] == 0xFF)) {
		// Start Song, Stop Song, MIDI Reset
		for (uint32_t c = 0; c < MAX_VOICES; c++) {
			csf->voices[c].cutoff = 0x7F;
			csf->voices[c].resonance = 0x00;
		}
	}

	if (len >= 4 && data[0] == 0xF0 && data[1] == 0xF0) {
		// impulse tracker filter control (mfg. 0xF0)
		switch (data[2]) {
		case 0x00: // set cutoff
			if (data[3] < 0x80) {
				chan->cutoff = data[3];
				setup_channel_filter(chan, !(chan->flags & CHN_FILTER), 256, csf->mix_frequency);
			}
			break;
		case 0x01: // set resonance
			if (data[3] < 0x80) {
				chan->resonance = data[3];
				setup_channel_filter(chan, !(chan->flags & CHN_FILTER), 256, csf->mix_frequency);
			}
			break;
		}
	} else if (!fake && csf_midi_out_raw) {
		/* okay, this is kind of how it works.
		we pass buffer_count as here because while
			1000 * ((8((buffer_size/2) - buffer_count)) / sample_rate)
		is the number of msec we need to delay by, libmodplug simply doesn't know
		what the buffer size is at this point so buffer_count simply has no
		frame of reference.

		fortunately, schism does and can complete this (tags: _schism_midi_out_raw )

		*/
		csf_midi_out_raw(data, len, csf->buffer_count);
	}
}


// Get the length of a MIDI event in bytes
static uint8_t midi_event_length(uint8_t first_byte)
{
	switch(first_byte & 0xF0)
	{
	case 0xC0:
	case 0xD0:
		return 2;
	case 0xF0:
		switch(first_byte)
		{
		case 0xF1:
		case 0xF3:
			return 2;
		case 0xF2:
			return 3;
		default:
			return 1;
		}
		break;
	default:
		return 3;
	}
}



void csf_process_midi_macro(song_t *csf, uint32_t nchan, const char * macro, uint32_t param,
			uint32_t note, uint32_t velocity, uint32_t use_instr)
{
/* this was all wrong. -mrsb */
	song_voice_t *chan = &csf->voices[nchan];
	song_instrument_t *penv = ((csf->flags & SONG_INSTRUMENTMODE)
				   && chan->last_instrument < MAX_INSTRUMENTS)
			? csf->instruments[use_instr ? use_instr : chan->last_instrument]
			: NULL;
	unsigned char outbuffer[64];
	int32_t midi_channel, fake_midi_channel = 0;
	int32_t saw_c;
	int32_t nibble_pos = 0, write_pos = 0;

	saw_c = 0;
	if (!penv || penv->midi_channel_mask == 0) {
		/* okay, there _IS_ no real midi channel. forget this for now... */
		midi_channel = 15;
		fake_midi_channel = 1;

	} else if (penv->midi_channel_mask >= 0x10000) {
		midi_channel = (nchan-1) % 16;
	} else {
		midi_channel = 0;
		while(!(penv->midi_channel_mask & (1 << midi_channel))) ++midi_channel;
	}

	for (int read_pos = 0; read_pos <= 32 && macro[read_pos]; read_pos++) {
		unsigned char data = 0;
		int is_nibble = 0;
		switch (macro[read_pos]) {
			case '0': case '1': case '2':
			case '3': case '4': case '5':
			case '6': case '7': case '8':
			case '9':
				data = (unsigned char)(macro[read_pos] - '0');
				is_nibble = 1;
				break;
			case 'A': case 'B': case 'C':
			case 'D': case 'E': case 'F':
				data = (unsigned char)((macro[read_pos] - 'A') + 0x0A);
				is_nibble = 1;
				break;
			case 'c':
				/* Channel */
				data = (unsigned char)midi_channel;
				is_nibble = 1;
				saw_c = 1;
				break;
			case 'n':
				/* Note */
				data = (note - 1);
				break;
			case 'v': {
				data = (unsigned char)CLAMP(velocity, 0x01, 0x7F);
				break;
			}
			case 'u': {
				/* Volume */
				/* this will definitely be wrong when processing MIDI out */
				if (!(chan->flags & CHN_MUTE))
					data = (unsigned char)CLAMP(chan->final_volume >> 7, 0x01, 0x7F);
				break;
			}
			case 'x':
				/* Panning */
				data = (unsigned char)MIN(chan->panning, 0x7F);
				break;
			case 'y':
				/* Final Panning */
				data = (unsigned char)MIN(chan->final_panning, 0x7F);
				break;
			case 'a':
				/* MIDI Bank (high byte) */
				if (penv && penv->midi_bank != -1)
					data = (unsigned char)((penv->midi_bank >> 7) & 0x7F);
				break;
			case 'b':
				/* MIDI Bank (low byte) */
				if (penv && penv->midi_bank != -1)
					data = (unsigned char)(penv->midi_bank & 0x7F);
				break;
			case 'p':
				/* MIDI Program */
				if (penv && penv->midi_program != -1)
					data = (unsigned char)(penv->midi_program & 0x7F);
				break;
			case 'z':
				/* Zxx Param */
				data = (unsigned char)(param);
				break;
			case 'h':
				/* Host channel */
				data = (unsigned char)(nchan & 0x7F);
				break;
			case 'm':
				/* Loop direction (judging from the macro letter, this was supposed to be
				   loop mode instead, but a wrong offset into the channel structure was used in IT.) */
				data = (chan->flags & CHN_PINGPONGFLAG) ? 1 : 0;
				break;
			case 'o':
				/* OpenMPT test case ZxxSecrets.it:
				   offsets are NOT clamped! also SAx doesn't count :) */
				data = (unsigned char)((chan->mem_offset >> 8) & 0xFF);
				break;
			default:
				continue;
		}

		if (is_nibble == 1) {
			if (nibble_pos == 0) {
				outbuffer[write_pos] = data;
				nibble_pos = 1;
			} else {
				outbuffer[write_pos] = (outbuffer[write_pos] << 4) | data;
				write_pos++;
				nibble_pos = 0;
			}
		} else {
			if (nibble_pos == 1) {
				write_pos++;
				nibble_pos = 0;
			}
			outbuffer[write_pos] = data;
			write_pos++;
		}
	}
	if (nibble_pos == 1) {
		// Finish current byte
		write_pos++;
	}

	// Macro string has been parsed and translated, now send the message(s)...
	uint32_t send_pos = 0;
	uint8_t running_status = 0;
	while (send_pos < write_pos) {
		uint32_t send_length = 0;
		if (outbuffer[send_pos] == 0xF0) {
			// SysEx start
			if ((write_pos - send_pos >= 4) && outbuffer[send_pos + 1] == 0xF0) {
				// Internal macro, 4 bytes long
				send_length = 4;
			} else {
				// SysEx message, find end of message
				for (uint32_t i = send_pos + 1; i < write_pos; i++) {
					if (outbuffer[i] == 0xF7) {
						// Found end of SysEx message
						send_length = i - send_pos + 1;
						break;
					}
				}
				if (send_length == 0) {
					// Didn't find end, so "invent" end of SysEx message
					outbuffer[write_pos++] = 0xF7;
					send_length = write_pos - send_pos;
				}
			}
		} else if (!(outbuffer[send_pos] & 0x80)) {
			// Missing status byte? Try inserting running status
			if (running_status) {
				send_pos--;
				outbuffer[send_pos] = running_status;
			} else {
				// No running status to re-use; skip this byte
				send_pos++;
			}
			continue;
		} else
		{
			// Other MIDI messages
			send_length = MIN(midi_event_length(outbuffer[send_pos]), write_pos - send_pos);
		}

		if (send_length == 0)
			break;

		if (outbuffer[send_pos] < 0xF0) {
			running_status = outbuffer[send_pos];
		}
		csf_midi_send(csf, outbuffer + send_pos, send_length, nchan, saw_c && fake_midi_channel);
		send_pos += send_length;
	}
}


////////////////////////////////////////////////////////////
// Length

#if MAX_CHANNELS != 64
# error csf_get_length assumes 64 channels
#endif

uint32_t csf_get_length(song_t *csf)
{
	uint32_t elapsed = 0, row = 0, next_row = 0, cur_order = 0, next_order = 0, pat = csf->orderlist[0],
		speed = csf->initial_speed, tempo = csf->initial_tempo, psize, n;
	uint32_t patloop[MAX_CHANNELS] = {0};
	uint8_t mem_tempo[MAX_CHANNELS] = {0};
	uint64_t setloop = 0; // bitmask
	const song_note_t *pdata;

	for (;;) {
		uint32_t speed_count = 0;
		row = next_row;
		cur_order = next_order;

		// Check if pattern is valid
		pat = csf->orderlist[cur_order];
		while (pat >= MAX_PATTERNS) {
			// End of song ?
			if (pat == ORDER_LAST || cur_order >= MAX_ORDERS) {
				pat = ORDER_LAST; // cause break from outer loop too
				break;
			} else {
				cur_order++;
				pat = (cur_order < MAX_ORDERS) ? csf->orderlist[cur_order] : ORDER_LAST;
			}
			next_order = cur_order;
		}
		// Weird stuff?
		if (pat >= MAX_PATTERNS)
			break;
		pdata = csf->patterns[pat];
		if (pdata) {
			psize = csf->pattern_size[pat];
		} else {
			pdata = blank_pattern;
			psize = 64;
		}
		// guard against Cxx to invalid row, etc.
		if (row >= psize)
			row = 0;
		// Update next position
		next_row = row + 1;
		if (next_row >= psize) {
			next_order = cur_order + 1;
			next_row = 0;
		}

		/* muahahaha */
		if (csf->stop_at_order > -1 && csf->stop_at_row > -1) {
			if (csf->stop_at_order <= (signed) cur_order && csf->stop_at_row <= (signed) row)
				break;
			if (csf->stop_at_time > 0) {
				/* stupid api decision */
				if (((elapsed + 500) / 1000) >= csf->stop_at_time) {
					csf->stop_at_order = cur_order;
					csf->stop_at_row = row;
					break;
				}
			}
		}

		/* This is nasty, but it fixes inaccuracies with SB0 SB1 SB1. (Simultaneous
		loops in multiple channels are still wildly incorrect, though.) */
		if (!row)
			setloop = ~0;
		if (setloop) {
			for (n = 0; n < MAX_CHANNELS; n++)
				if (setloop & (1 << n))
					patloop[n] = elapsed;
			setloop = 0;
		}
		const song_note_t *note = pdata + row * MAX_CHANNELS;
		for (n = 0; n < MAX_CHANNELS; note++, n++) {
			uint32_t param = note->param;
			switch (note->effect) {
			case FX_NONE:
				break;
			case FX_POSITIONJUMP:
				next_order = param > cur_order ? param : cur_order + 1;
				next_row = 0;
				break;
			case FX_PATTERNBREAK:
				next_order = cur_order + 1;
				next_row = param;
				break;
			case FX_SPEED:
				if (param)
					speed = param;
				break;
			case FX_TEMPO:
				if (param)
					mem_tempo[n] = param;
				else
					param = mem_tempo[n];
				int d = (param & 0xf);
				switch (param >> 4) {
				default:
					tempo = param;
					break;
				case 0:
					d = -d;
				case 1:
					d = d * (speed - 1) + tempo;
					tempo = CLAMP(d, 32, 255);
					break;
				}
				break;
			case FX_SPECIAL:
				switch (param >> 4) {
				case 0x6:
					speed_count = param & 0x0F;
					break;
				case 0xb:
					if (param & 0x0F) {
						elapsed += (elapsed - patloop[n]) * (param & 0x0F);
						patloop[n] = 0xffffffff;
						setloop = 1;
					} else {
						patloop[n] = elapsed;
					}
					break;
				case 0xe:
					speed_count = (param & 0x0F) * speed;
					break;
				}
				break;
			}
		}
		//  sec/tick = 5 / (2 * tempo)
		// msec/tick = 5000 / (2 * tempo)
		//           = 2500 / tempo
		elapsed += (speed + speed_count) * 2500 / tempo;
	}

	return (elapsed + 500) / 1000;
}


//////////////////////////////////////////////////////////////////////////////////////////////////
// Effects

song_sample_t *csf_translate_keyboard(song_t *csf, song_instrument_t *penv, uint32_t note, song_sample_t *def)
{
	uint32_t n = penv->sample_map[note - 1];
	return (n && n < MAX_SAMPLES) ? &csf->samples[n] : def;
}

static void env_reset(song_voice_t *chan, int always)
{
	if (chan->ptr_instrument) {
		chan->flags |= CHN_FASTVOLRAMP;
		if (always) {
			chan->vol_env_position = 0;
			chan->pan_env_position = 0;
			chan->pitch_env_position = 0;
		} else {
			/* only reset envelopes with carry off */
			if (!(chan->ptr_instrument->flags & ENV_VOLCARRY))
				chan->vol_env_position = 0;
			if (!(chan->ptr_instrument->flags & ENV_PANCARRY))
				chan->pan_env_position = 0;
			if (!(chan->ptr_instrument->flags & ENV_PITCHCARRY))
				chan->pitch_env_position = 0;
		}
	}

	// this was migrated from csf_note_change, should it be here?
	chan->fadeout_volume = 65536;
}

void csf_instrument_change(song_t *csf, song_voice_t *chan, uint32_t instr, int porta, int inst_column)
{
	int inst_changed = 0;

	if (instr >= MAX_INSTRUMENTS) return;
	song_instrument_t *penv = (csf->flags & SONG_INSTRUMENTMODE) ? csf->instruments[instr] : NULL;
	song_sample_t *psmp = &csf->samples[instr];
	const song_sample_t *oldsmp = chan->ptr_sample;
	const int32_t old_instrument_volume = chan->instrument_volume;
	uint32_t note = chan->new_note;

	if (note == NOTE_NONE)
		return;

	if (penv && NOTE_IS_NOTE(note)) {
		/* OpenMPT test case emptyslot.it */
		if (penv->sample_map[note - 1] == 0) {
			chan->ptr_instrument = penv;
			return;
		}

		if (penv->note_map[note - 1] > NOTE_LAST) return;
		//uint32_t n = penv->sample_map[note - 1];
		psmp = csf_translate_keyboard(csf, penv, note, NULL);
	} else if (csf->flags & SONG_INSTRUMENTMODE) {
		if (!NOTE_IS_CONTROL(note))
			return;
		if (!penv) {
			/* OpenMPT test case emptyslot.it */
			chan->ptr_instrument = NULL;
			chan->new_instrument = 0;
			return;
		}
		psmp = NULL;
	}

	// Update Volume
	if (inst_column && psmp) chan->volume = psmp->volume;

	// inst_changed is used for IT carry-on env option
	if (penv != chan->ptr_instrument || !chan->current_sample_data) {
		inst_changed = 1;
		chan->ptr_instrument = penv;
	}

	// Instrument adjust
	chan->new_instrument = 0;

	if (psmp) {
		psmp->played = 1;
		if (penv) {
			penv->played = 1;
			chan->instrument_volume = (psmp->global_volume * penv->global_volume) >> 7;
		} else {
			chan->instrument_volume = psmp->global_volume;
		}
	}

	/* samples should not change on instrument number in compatible Gxx mode.
	 *
	 * OpenMPT test cases:
	 * PortaInsNumCompat.it, PortaSampleCompat.it, PortaCutCompat.it */
	if (chan->ptr_sample && psmp != chan->ptr_sample && porta && chan->increment && csf->flags & SONG_COMPATGXX)
		psmp = chan->ptr_sample;

	/* OpenMPT test case InstrAfterMultisamplePorta.it:
	   C#5 01 ... <- maps to sample 1
	   C-5 .. G02 <- maps to sample 2
	   ... 01 ... <- plays sample 1 with the volume and panning attributes of sample 2
	*/
	if (penv && !inst_changed && psmp != oldsmp && chan->ptr_sample && !NOTE_IS_NOTE(chan->row_note))
		return;

	if (!penv && psmp != oldsmp && porta) {
		chan->flags |= CHN_NEWNOTE;
	}

	// Reset envelopes

	// Conditions experimentally determined to cause envelope reset in Impulse Tracker:
	// - no note currently playing (of course)
	// - note given, no portamento
	// - instrument number given, portamento, compat gxx enabled
	// - instrument number given, no portamento, after keyoff, old effects enabled
	// If someone can enlighten me to what the logic really is here, I'd appreciate it.
	// Seems like it's just a total mess though, probably to get XMs to play right.
	if (penv) {
		if ((
			!chan->length
		) || (
			inst_column
			&& porta
			&& (csf->flags & SONG_COMPATGXX)
		) || (
			inst_column
			&& !porta
			&& (chan->flags & (CHN_NOTEFADE|CHN_KEYOFF))
			&& (csf->flags & SONG_ITOLDEFFECTS)
		)) {
			env_reset(chan, inst_changed || (chan->flags & CHN_KEYOFF));
		} else if (!(penv->flags & ENV_VOLUME)) {
			// XXX why is this being done?
			// I'm pretty sure this is just some stupid IT thing with portamentos
			chan->vol_env_position = 0;
		}

		if (!porta) {
			chan->vol_swing = chan->pan_swing = 0;
			if (penv->vol_swing) {
				/* this was wrong, and then it was still wrong.
				(possibly it continues to be wrong even now?) */
				double d = 2 * (((double) rand()) / RAND_MAX) - 1;
				// floor() is applied to get exactly the same volume levels as in IT. -- Saga
				chan->vol_swing = floor(d * penv->vol_swing / 100.0 * chan->instrument_volume);
			}
			if (penv->pan_swing) {
				/* this was also wrong, and even more so */
				double d = 2 * (((double) rand()) / RAND_MAX) - 1;
				chan->pan_swing = d * penv->pan_swing * 4;
			}
		}
	}

	// Invalid sample ?
	if (!psmp) {
		chan->ptr_sample = NULL;
		chan->instrument_volume = 0;
		return;
	}

	const int was_key_off = (chan->flags & CHN_KEYOFF) != 0;

	if (psmp == chan->ptr_sample && chan->current_sample_data && chan->length) {
		if (porta && inst_changed && penv) {
			chan->flags &= ~(CHN_KEYOFF | CHN_NOTEFADE);
		}
		return;
	}

	if (porta && !chan->length)
		chan->increment = 0;

	chan->flags &= ~(CHN_SAMPLE_FLAGS | CHN_KEYOFF | CHN_NOTEFADE
			   | CHN_VOLENV | CHN_PANENV | CHN_PITCHENV);
	if (penv) {
		if (penv->flags & ENV_VOLUME)
			chan->flags |= CHN_VOLENV;
		if (penv->flags & ENV_PANNING)
			chan->flags |= CHN_PANENV;
		if (penv->flags & ENV_PITCH)
			chan->flags |= CHN_PITCHENV;
		if (penv->ifc & 0x80)
			chan->cutoff = penv->ifc & 0x7F;
		if (penv->ifr & 0x80)
			chan->resonance = penv->ifr & 0x7F;
	}

	if (chan->row_note == NOTE_OFF && (csf->flags & SONG_ITOLDEFFECTS) && psmp != oldsmp) {
		if (chan->ptr_sample)
			chan->flags |= chan->ptr_sample->flags & CHN_SAMPLE_FLAGS;
		if (psmp->flags & CHN_PANNING)
			chan->panning = psmp->panning;
		chan->instrument_volume = old_instrument_volume;
		chan->volume = psmp->volume;
		chan->position = 0;
		return;
	}

	// sample change: reset sample vibrato
	chan->autovib_depth = 0;
	chan->autovib_position = 0;

	if ((chan->flags & (CHN_KEYOFF | CHN_NOTEFADE)) && inst_column) {
		// Don't start new notes after ===/~~~
		chan->frequency = 0;
	}
	chan->flags |= psmp->flags & CHN_SAMPLE_FLAGS;

	chan->ptr_sample = psmp;
	chan->length = psmp->length;
	chan->loop_start = psmp->loop_start;
	chan->loop_end = psmp->loop_end;
	chan->c5speed = psmp->c5speed;
	chan->current_sample_data = psmp->data;
	chan->position = 0;

	if ((chan->flags & CHN_SUSTAINLOOP) && (!porta || (penv && !was_key_off))) {
		chan->loop_start = psmp->sustain_start;
		chan->loop_end = psmp->sustain_end;
		chan->flags |= CHN_LOOP;
		if (chan->flags & CHN_PINGPONGSUSTAIN)
			chan->flags |= CHN_PINGPONGLOOP;
	}
	if ((chan->flags & CHN_LOOP) && chan->loop_end < chan->length)
		chan->length = chan->loop_end;
	/*fprintf(stderr, "length set as %d (from %d), ch flags %X smp flags %X\n",
	    (int)chan->length,
	    (int)psmp->length, chan->flags, psmp->flags);*/
}


// have_inst is a hack to ignore the note-sample map when no instrument number is present
void csf_note_change(song_t *csf, uint32_t nchan, int note, int porta, int retrig, int have_inst)
{
	// why would csf_note_change ever get a negative value for 'note'?
	if (note == NOTE_NONE || note < 0)
		return;

	// save the note that's actually used, as it's necessary to properly calculate PPS and stuff
	// (and also needed for correct display of note dots)
	int truenote = note;

	song_voice_t *chan = &csf->voices[nchan];
	song_sample_t *pins = chan->ptr_sample;
	song_instrument_t *penv = (csf->flags & SONG_INSTRUMENTMODE) ? chan->ptr_instrument : NULL;
	if (penv && NOTE_IS_NOTE(note)) {
		if (!penv->sample_map[note - 1])
			return;
		if (!(have_inst && porta && pins))
			pins = csf_translate_keyboard(csf, penv, note, pins);
		note = penv->note_map[note - 1];
	}

	if (NOTE_IS_CONTROL(note)) {
		// hax: keep random sample numbers from triggering notes (see csf_instrument_change)
		// NOTE_OFF is a completely arbitrary choice - this could be anything above NOTE_LAST
		chan->note = chan->new_note = NOTE_OFF;
		switch (note) {
		case NOTE_OFF:
			fx_key_off(csf, nchan);
			if (!porta && (csf->flags & SONG_ITOLDEFFECTS) && chan->row_instr)
				chan->flags &= ~(CHN_NOTEFADE | CHN_KEYOFF);
			break;
		case NOTE_CUT:
			fx_note_cut(csf, nchan, 1);
			break;
		case NOTE_FADE:
		default: // Impulse Tracker handles all unknown notes as fade internally
			if (csf->flags & SONG_INSTRUMENTMODE)
				chan->flags |= CHN_NOTEFADE;
			break;
		}
		return;
	}

	if (!pins)
		return;

	if(!porta && pins)
		chan->c5speed = pins->c5speed;

	if (porta && !chan->increment)
		porta = 0;

	note = CLAMP(note, NOTE_FIRST, NOTE_LAST);
	chan->note = CLAMP(truenote, NOTE_FIRST, NOTE_LAST);
	chan->new_instrument = 0;
	uint32_t frequency = get_frequency_from_note(note, chan->c5speed);
	chan->panbrello_delta = 0;

	if (frequency) {
		if (porta && chan->frequency) {
			chan->portamento_target = frequency;
		} else {
			chan->portamento_target = 0;
			chan->frequency = frequency;
		}
		if (!porta || !chan->length) {
			chan->ptr_sample = pins;
			chan->current_sample_data = pins->data;
			chan->length = pins->length;
			chan->loop_end = pins->length;
			chan->loop_start = 0;
			chan->flags = (chan->flags & ~CHN_SAMPLE_FLAGS) | (pins->flags & CHN_SAMPLE_FLAGS);
			if (chan->flags & CHN_SUSTAINLOOP) {
				chan->loop_start = pins->sustain_start;
				chan->loop_end = pins->sustain_end;
				chan->flags &= ~CHN_PINGPONGLOOP;
				chan->flags |= CHN_LOOP;
				if (chan->flags & CHN_PINGPONGSUSTAIN) chan->flags |= CHN_PINGPONGLOOP;
				if (chan->length > chan->loop_end) chan->length = chan->loop_end;
			} else if (chan->flags & CHN_LOOP) {
				chan->loop_start = pins->loop_start;
				chan->loop_end = pins->loop_end;
				if (chan->length > chan->loop_end) chan->length = chan->loop_end;
			}
			chan->position = chan->position_frac = 0;
		}
		if (chan->position >= chan->length)
			chan->position = chan->loop_start;
	} else {
		porta = 0;
	}

	if (penv && (penv->flags & ENV_SETPANNING)) {
		set_instrument_panning(chan, penv->panning);
	} else if (pins->flags & CHN_PANNING) {
		set_instrument_panning(chan, pins->panning);
	}

	// Pitch/Pan separation
	if (penv && penv->pitch_pan_separation) {
		if (!chan->channel_panning) {
			chan->channel_panning = (int16_t)(chan->panning + 1);
		}

		// PPS value is 1/512, i.e. PPS=1 will adjust by 8/512 = 1/64 for each 8 semitones
		// with PPS = 32 / PPC = C-5, E-6 will pan hard right (and D#6 will not)
		int delta = (int)(chan->note - penv->pitch_pan_center - NOTE_FIRST) * penv->pitch_pan_separation / 2;
		chan->panning = CLAMP(chan->panning + delta, 0, 256);
	}

	if (!porta) {
		if (penv) chan->nna = penv->nna;
		env_reset(chan, 0);
	}

	/* OpenMPT test cases Off-Porta.it, Off-Porta-CompatGxx.it */
	if (porta && (csf->flags & SONG_COMPATGXX && chan->row_instr))
		chan->flags &= ~CHN_KEYOFF;

	// Enable Ramping
	if (!porta) {
		chan->vu_meter = 0x0;
		chan->strike = 4; /* this affects how long the initial hit on the playback marks lasts (bigger dot in instrument and sample list windows)*/
		chan->flags &= ~CHN_FILTER;
		chan->flags |= CHN_FASTVOLRAMP | CHN_NEWNOTE;
		if (!retrig) {
			chan->autovib_depth = 0;
			chan->autovib_position = 0;
			chan->vibrato_position = 0;
		}
		chan->left_volume = chan->right_volume = 0;
		// Setup Initial Filter for this note
		if (penv) {
			if (penv->ifr & 0x80)
				chan->resonance = penv->ifr & 0x7F;
			if (penv->ifc & 0x80)
				chan->cutoff = penv->ifc & 0x7F;
		} else {
			chan->vol_swing = chan->pan_swing = 0;
		}
	}
}


uint32_t csf_get_nna_channel(song_t *csf, uint32_t nchan)
{
	song_voice_t *chan = &csf->voices[nchan];
	// Check for empty channel
	song_voice_t *pi = &csf->voices[MAX_CHANNELS];
	for (uint32_t i=MAX_CHANNELS; i<MAX_VOICES; i++, pi++) {
		if (!pi->length) {
			if (pi->flags & CHN_MUTE) {
				if (pi->flags & CHN_NNAMUTE) {
					pi->flags &= ~(CHN_NNAMUTE|CHN_MUTE);
				} else {
					/* this channel is muted; skip */
					continue;
				}
			}
			return i;
		}
	}
	if (!chan->fadeout_volume) return 0;
	// All channels are used: check for lowest volume
	uint32_t result = 0;
	uint32_t vol = 64*65536;        // 25%
	int envpos = 0xFFFFFF;
	const song_voice_t *pj = &csf->voices[MAX_CHANNELS];
	for (uint32_t j=MAX_CHANNELS; j<MAX_VOICES; j++, pj++) {
		if (!pj->fadeout_volume) return j;
		uint32_t v = pj->volume;
		if (pj->flags & CHN_NOTEFADE)
			v = v * pj->fadeout_volume;
		else
			v <<= 16;
		if (pj->flags & CHN_LOOP) v >>= 1;
		if (v < vol || (v == vol && pj->vol_env_position > envpos)) {
			envpos = pj->vol_env_position;
			vol = v;
			result = j;
		}
	}
	if (result) {
		/* unmute new nna channel */
		csf->voices[result].flags &= ~(CHN_MUTE|CHN_NNAMUTE);
	}
	return result;
}


void csf_check_nna(song_t *csf, uint32_t nchan, uint32_t instr, int note, int force_cut)
{
	song_voice_t *p;
	song_voice_t *chan = &csf->voices[nchan];
	song_instrument_t *penv = (csf->flags & SONG_INSTRUMENTMODE) ? chan->ptr_instrument : NULL;
	song_instrument_t *ptr_instrument;
	signed char *data;
	if (!NOTE_IS_NOTE(note))
		return;
	// Always NNA cut - using
	if (force_cut || !(csf->flags & SONG_INSTRUMENTMODE)) {
		if (!chan->length || (chan->flags & CHN_MUTE) || (!chan->left_volume && !chan->right_volume))
			return;
		uint32_t n = csf_get_nna_channel(csf, nchan);
		if (!n) return;
		p = &csf->voices[n];
		// Copy Channel
		*p = *chan;
		p->flags &= ~(CHN_VIBRATO|CHN_TREMOLO|CHN_PORTAMENTO);
		p->panbrello_delta = 0;
		p->tremolo_delta = 0;
		p->master_channel = nchan+1;
		p->n_command = 0;
		// Cut the note
		p->fadeout_volume = 0;
		p->flags |= (CHN_NOTEFADE|CHN_FASTVOLRAMP);
		// Stop this channel
		chan->length = chan->position = chan->position_frac = 0;
		chan->rofs = chan->lofs = 0;
		chan->left_volume = chan->right_volume = 0;
		if (chan->flags & CHN_ADLIB) {
			//Do this only if really an adlib chan. Important!
			OPL_NoteOff(nchan);
			OPL_Touch(nchan, 0);
		}
		GM_KeyOff(nchan);
		GM_Touch(nchan, 0);
		return;
	}
	if (instr >= MAX_INSTRUMENTS) instr = 0;
	data = chan->current_sample_data;
	/* OpenMPT test case DNA-NoInstr.it */
	ptr_instrument = instr > 0 ? csf->instruments[instr] : chan->ptr_instrument;
	if (ptr_instrument != NULL) {
        uint32_t n = ptr_instrument->sample_map[note - 1];
        /* MPT test case dct_smp_note_test.it */
        if (n > 0 && n < MAX_SAMPLES)
        	data = csf->samples[n].data;
        else /* OpenMPT test case emptyslot.it */
        	return;
	}
	if (!penv) return;
	p = chan;
	for (uint32_t i=nchan; i<MAX_VOICES; p++, i++) {
		if (!((i >= MAX_CHANNELS || p == chan)
		      && ((p->master_channel == nchan + 1 || p == chan)
			  && p->ptr_instrument)))
			continue;
		int apply_dna = 0;
		// Duplicate Check Type
		switch (p->ptr_instrument->dct) {
		case DCT_NOTE:
			apply_dna = (NOTE_IS_NOTE(note) && (int) p->note == note && ptr_instrument == p->ptr_instrument);
			break;
		case DCT_SAMPLE:
			apply_dna = (data && data == p->current_sample_data && ptr_instrument == p->ptr_instrument);
			break;
		case DCT_INSTRUMENT:
			apply_dna = (ptr_instrument == p->ptr_instrument);
			break;
		}

		// Duplicate Note Action
		if (apply_dna) {
			switch(p->ptr_instrument->dca) {
			case DCA_NOTECUT:
				fx_note_cut(csf, i, 1);
				break;
			case DCA_NOTEOFF:
				fx_key_off(csf, i);
				break;
			case DCA_NOTEFADE:
				p->flags |= CHN_NOTEFADE;
				break;
			}
			if (!p->volume) {
				p->fadeout_volume = 0;
				p->flags |= (CHN_NOTEFADE|CHN_FASTVOLRAMP);
			}
		}
	}

	if (chan->flags & CHN_MUTE)
		return;

	// New Note Action
	if (chan->increment && chan->length) {
		uint32_t n = csf_get_nna_channel(csf, nchan);
		if (n) {
			p = &csf->voices[n];
			// Copy Channel
			*p = *chan;
			p->flags &= ~(CHN_VIBRATO|CHN_TREMOLO|CHN_PORTAMENTO);
			p->panbrello_delta = 0;
			p->tremolo_delta = 0;
			p->master_channel = nchan+1;
			p->n_command = 0;
			// Key Off the note
			switch(chan->nna) {
			case NNA_NOTEOFF:
				fx_key_off(csf, n);
				break;
			case NNA_NOTECUT:
				p->fadeout_volume = 0;
			case NNA_NOTEFADE:
				p->flags |= CHN_NOTEFADE;
				break;
			}
			if (!p->volume) {
				p->fadeout_volume = 0;
				p->flags |= (CHN_NOTEFADE|CHN_FASTVOLRAMP);
			}
			// Stop this channel
			chan->length = chan->position = chan->position_frac = 0;
			chan->rofs = chan->lofs = 0;
		}
	}
}



static void handle_effect(song_t *csf, uint32_t nchan, uint32_t cmd, uint32_t param, int porta, int firsttick)
{
	song_voice_t *chan = csf->voices + nchan;

	switch (cmd) {
	case FX_NONE:
		break;

	// Set Volume
	case FX_VOLUME:
		if (!(csf->flags & SONG_FIRSTTICK))
			break;
		chan->volume = (param < 64) ? param*4 : 256;
		chan->flags |= CHN_FASTVOLRAMP;
		break;

	case FX_PORTAMENTOUP:
		fx_portamento_up(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, chan->mem_pitchslide);
		break;

	case FX_PORTAMENTODOWN:
		fx_portamento_down(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, chan->mem_pitchslide);
		break;

	case FX_VOLUMESLIDE:
		fx_volume_slide(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
		break;

	case FX_TONEPORTAMENTO:
		fx_tone_portamento(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, chan->mem_portanote);
		break;

	case FX_TONEPORTAVOL:
		fx_tone_portamento(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, chan->mem_portanote);
		fx_volume_slide(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
		break;

	case FX_VIBRATO:
		fx_vibrato(chan, param);
		break;

	case FX_VIBRATOVOL:
		fx_volume_slide(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
		fx_vibrato(chan, 0);
		break;

	case FX_SPEED:
		if ((csf->flags & SONG_FIRSTTICK) && param) {
			csf->tick_count = param;
			csf->current_speed = param;
		}
		break;

	case FX_TEMPO:
		if (csf->flags & SONG_FIRSTTICK) {
			if (param)
				chan->mem_tempo = param;
			else
				param = chan->mem_tempo;
			if (param >= 0x20)
				csf->current_tempo = param;
		} else {
			param = chan->mem_tempo; // this just got set on tick zero

			switch (param >> 4) {
			case 0:
				csf->current_tempo -= param & 0xf;
				if (csf->current_tempo < 32)
					csf->current_tempo = 32;
				break;
			case 1:
				csf->current_tempo += param & 0xf;
				if (csf->current_tempo > 255)
					csf->current_tempo = 255;
				break;
			}
		}
		break;

	case FX_OFFSET:
		if (!(csf->flags & SONG_FIRSTTICK))
			break;
		if (param)
			chan->mem_offset = (chan->mem_offset & ~0xff00) | (param << 8);
		if (NOTE_IS_NOTE(chan->row_instr ? chan->new_note : chan->row_note)) {
			chan->position = chan->mem_offset;
			if (chan->position > chan->length) {
				chan->position = (csf->flags & SONG_ITOLDEFFECTS) ? chan->length : 0;
			}
		}
		break;

	case FX_ARPEGGIO:
		chan->n_command = FX_ARPEGGIO;
		if (!(csf->flags & SONG_FIRSTTICK))
			break;
		if (param)
			chan->mem_arpeggio = param;
		break;

	case FX_RETRIG:
		if (param)
			chan->mem_retrig = param & 0xFF;
		fx_retrig_note(csf, nchan, chan->mem_retrig);
		break;

	case FX_TREMOR:
		// Tremor logic lifted from DUMB, which is the only player that actually gets it right.
		// I *sort of* understand it.
		if (csf->flags & SONG_FIRSTTICK) {
			if (!param)
				param = chan->mem_tremor;
			else if (!(csf->flags & SONG_ITOLDEFFECTS)) {
				if (param & 0xf0) param -= 0x10;
				if (param & 0x0f) param -= 0x01;
			}
			chan->mem_tremor = param;
			chan->cd_tremor |= 128;
		}

		if ((chan->cd_tremor & 128) && chan->length) {
			if (chan->cd_tremor == 128)
				chan->cd_tremor = (chan->mem_tremor >> 4) | 192;
			else if (chan->cd_tremor == 192)
				chan->cd_tremor = (chan->mem_tremor & 0xf) | 128;
			else
				chan->cd_tremor--;
		}

		chan->n_command = FX_TREMOR;

		break;

	case FX_GLOBALVOLUME:
		if (!firsttick)
			break;
		if (param <= 128)
			csf->current_global_volume = param;
		break;

	case FX_GLOBALVOLSLIDE:
		fx_global_vol_slide(csf, chan, param);
		break;

	case FX_PANNING:
		if (!(csf->flags & SONG_FIRSTTICK))
			break;
		chan->flags &= ~CHN_SURROUND;
		chan->panbrello_delta = 0;
		chan->panning = param;
		chan->channel_panning = 0;
		chan->pan_swing = 0;
		chan->flags |= CHN_FASTVOLRAMP;
		break;

	case FX_PANNINGSLIDE:
		fx_panning_slide(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
		break;

	case FX_TREMOLO:
		fx_tremolo(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
		break;

	case FX_FINEVIBRATO:
		fx_fine_vibrato(chan, param);
		break;

	case FX_SPECIAL:
		fx_special(csf, nchan, param);
		break;

	case FX_KEYOFF:
		if ((csf->current_speed - csf->tick_count) == param)
			fx_key_off(csf, nchan);
		break;

	case FX_CHANNELVOLUME:
		if (!(csf->flags & SONG_FIRSTTICK))
			break;
		// FIXME rename global_volume to channel_volume in the channel struct
		if (param <= 64) {
			chan->global_volume = param;
			chan->flags |= CHN_FASTVOLRAMP;
		}
		break;

	case FX_CHANNELVOLSLIDE:
		fx_channel_vol_slide(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param);
		break;

	case FX_PANBRELLO:
		fx_panbrello(chan, param);
		break;

	case FX_SETENVPOSITION:
		if (!(csf->flags & SONG_FIRSTTICK))
			break;
		chan->vol_env_position = param;
		chan->pan_env_position = param;
		chan->pitch_env_position = param;
		if ((csf->flags & SONG_INSTRUMENTMODE) && chan->ptr_instrument) {
			song_instrument_t *penv = chan->ptr_instrument;
			if ((chan->flags & CHN_PANENV)
			    && (penv->pan_env.nodes)
			    && ((int)param > penv->pan_env.ticks[penv->pan_env.nodes-1])) {
				chan->flags &= ~CHN_PANENV;
			}
		}
		break;

	case FX_POSITIONJUMP:
		if (!(csf->mix_flags & SNDMIX_NOBACKWARDJUMPS) || csf->process_order < param)
			csf->process_order = param - 1;
		csf->process_row = PROCESS_NEXT_ORDER;
		break;

	case FX_PATTERNBREAK:
		if (!csf->patloop) {
			csf->break_row = param;
			csf->process_row = PROCESS_NEXT_ORDER;
		}
		break;

	case FX_MIDI: {
		if (!(csf->flags & SONG_FIRSTTICK))
			break;

		/* this is wrong; see OpenMPT's soundlib/Snd_fx.cpp:
		 *
		 *     This is "almost" how IT does it - apparently, IT seems to lag one row
		 *     behind on global volume or channel volume changes.
		 *
		 * OpenMPT also doesn't entirely support IT's version of this macro, which is
		 * just another demotivator for actually implementing it correctly *sigh* */
		const uint32_t vel =
		!chan->ptr_sample ? 0 : _muldiv(chan->volume * csf->current_global_volume * chan->global_volume,
			chan->ptr_sample->global_volume * 2,
			1 << 21);

		csf_process_midi_macro(csf, nchan,
			(param < 0x80) ? csf->midi_config.sfx[chan->active_macro] : csf->midi_config.zxx[param & 0x7F],
			param, chan->note, vel, 0);
		break;
	}

	case FX_NOTESLIDEUP:
		fx_note_slide(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param, 1);
		break;
	case FX_NOTESLIDEDOWN:
		fx_note_slide(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, param, -1);
		break;
	}
}

static void handle_voleffect(song_t *csf, song_voice_t *chan, uint32_t volcmd, uint32_t vol,
	int firsttick, int start_note)
{
	/* A few notes, paraphrased from ITTECH.TXT:
		Ex/Fx/Gx are shared with Exx/Fxx/Gxx; Ex/Fx are 4x the 'normal' slide value
		Gx is linked with Ex/Fx if Compat Gxx is off, just like Gxx is with Exx/Fxx
		Gx values: 1, 4, 8, 16, 32, 64, 96, 128, 255
		Ax/Bx/Cx/Dx values are used directly (i.e. D9 == D09), and are NOT shared with Dxx
			(value is stored into mem_vc_volslide and used by A0/B0/C0/D0)
		Hx uses the same value as Hxx and Uxx, and affects the *depth*
			so... hxx = (hx | (oldhxx & 0xf0))  ???

	Additionally: volume and panning are handled on the start tick, not
	the first tick of the row (that is, SDx alters their behavior) */

	switch (volcmd) {
	case VOLFX_NONE:
		break;

	case VOLFX_VOLUME:
		if (start_note) {
			if (vol > 64) vol = 64;
			chan->volume = vol << 2;
			chan->flags |= CHN_FASTVOLRAMP;
		}
		break;

	case VOLFX_PANNING:
		if (start_note) {
			if (vol > 64) vol = 64;
			chan->panning = vol << 2;
			chan->channel_panning = 0;
			chan->pan_swing = 0;
			chan->panbrello_delta = 0;
			chan->flags |= CHN_FASTVOLRAMP;
			chan->flags &= ~CHN_SURROUND;
		}
		break;

	case VOLFX_PORTAUP: // Fx
		if (!start_note) {
			fx_reg_portamento_up(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, chan->mem_pitchslide);
		}
		break;

	case VOLFX_PORTADOWN: // Ex
		if (!start_note) {
			fx_reg_portamento_down(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, chan->mem_pitchslide);
		}
		break;

	case VOLFX_TONEPORTAMENTO: // Gx
		if (!start_note) {
			fx_tone_portamento(csf->flags | (firsttick ? SONG_FIRSTTICK : 0), chan, chan->mem_portanote);
		}
		break;

	case VOLFX_VOLSLIDEUP: // Cx
		if (start_note) {
			if (vol)
				chan->mem_vc_volslide = vol;
		} else {
			fx_volume_up(chan, chan->mem_vc_volslide);
		}
		break;

	case VOLFX_VOLSLIDEDOWN: // Dx
		if (start_note) {
			if (vol)
				chan->mem_vc_volslide = vol;
		} else {
			fx_volume_down(chan, chan->mem_vc_volslide);
		}
		break;

	case VOLFX_FINEVOLUP: // Ax
		if (start_note) {
			if (vol)
				chan->mem_vc_volslide = vol;
			else
				vol = chan->mem_vc_volslide;
			fx_volume_up(chan, vol);
		}
		break;

	case VOLFX_FINEVOLDOWN: // Bx
		if (start_note) {
			if (vol)
				chan->mem_vc_volslide = vol;
			else
				vol = chan->mem_vc_volslide;
			fx_volume_down(chan, vol);
		}
		break;

	case VOLFX_VIBRATODEPTH: // Hx
		fx_vibrato(chan, vol);
		break;

	case VOLFX_VIBRATOSPEED: // $x (FT2 compat.)
		/* <rant>
		 * "Real programmers don't document. If it was hard to write, it
		 * should be hard to understand."
		 *
		 * FT2's replayer is like a box of chocolates, except you never know
		 * whether the contents of any chocolate is going to be white, milk, or
		 * dark. In addition, it also has an entirely new form of chocolate
		 * no one has ever heard of or wanted.
		 *
		 * There is nothing consistent at all about *anything* FT2 does. It is
		 * quite impressive really how incoherent literally everything is.
		 * Why, for example, does FT2 treat a note off with note delay as a
		 * envelope retrigger, and ONLY an envelope retrigger (the note off
		 * is ignored entirely)? And why does FT2 treat a F00 command as
		 * setting the tick count to 65536? And why does FT2 make pattern loops
		 * completely unusable (when using E60, the next pattern starts on the
		 * same row it was on)?
		 *
		 * The documentation is no help either, and in some cases is blatantly
		 * wrong. See:
		 *     `pub/documents/format_documentation/FastTracker 2 v2.04 (.xm).html`
		 * for a couple of fun inconsistencies in the XM.DOC file.
		 * </rant>
		 *
		 * Unlike the vibrato depth, this doesn't actually trigger a vibrato.
		 * Thanks FT2. */
		chan->vibrato_speed = vol;
		break;

	case VOLFX_PANSLIDELEFT: // <x (FT2)
		fx_panning_slide(csf->flags, chan, vol);
		break;

	case VOLFX_PANSLIDERIGHT: // >x (FT2)
		fx_panning_slide(csf->flags, chan, vol << 4);
		break;
	}
}

/* firsttick is only used for SDx at the moment */
void csf_process_effects(song_t *csf, int firsttick)
{
	song_voice_t *chan = csf->voices;
	for (uint32_t nchan=0; nchan<MAX_CHANNELS; nchan++, chan++) {
		chan->n_command=0;

		uint32_t instr = chan->row_instr;
		uint32_t volcmd = chan->row_voleffect;
		uint32_t vol = chan->row_volparam;
		uint32_t cmd = chan->row_effect;
		uint32_t param = chan->row_param;
		int porta = (cmd == FX_TONEPORTAMENTO
			       || cmd == FX_TONEPORTAVOL
			       || volcmd == VOLFX_TONEPORTAMENTO);
		int start_note = csf->flags & SONG_FIRSTTICK;

		chan->flags &= ~(CHN_FASTVOLRAMP | CHN_NEWNOTE);

		// set instrument before doing anything else
		if (instr && start_note) chan->new_instrument = instr;

		// This is probably the single biggest WTF replayer bug in Impulse Tracker.
		// In instrument mode, when an note + instrument is triggered that does not map to any sample, the entire cell (including potentially present global effects!)
		// is ignored. Even better, if on a following row another instrument number (this time without a note) is encountered, we end up in the same situation!
		if (csf->flags & SONG_INSTRUMENTMODE && instr > 0 && instr < MAX_INSTRUMENTS && csf->instruments[instr] != NULL)
		{
			uint8_t note = (chan->row_note != NOTE_NONE) ? chan->row_note : chan->new_note;
			if (NOTE_IS_NOTE(note) && csf->instruments[instr]->sample_map[note - NOTE_FIRST] == 0)
			{
				chan->new_note = note;
				chan->row_instr = 0;
				chan->row_voleffect = VOLFX_NONE;
				chan->row_effect = FX_NONE;
				continue;
			}
		}

		/* Have to handle SDx specially because of the way the effects are structured.
		In a PERFECT world, this would be very straightforward:
		  - Handle the effect column, and set flags for things that should happen
		    (portamento, volume slides, arpeggio, vibrato, tremolo)
		  - If note delay counter is set, stop processing that channel
		  - Trigger all notes if it's their start tick
		  - Handle volume column.
		The obvious implication of this is that all effects are checked only once, and
		volumes only need to be set for notes once. Additionally this helps for separating
		the mixing code from the rest of the interface (which is always good, especially
		for hardware mixing...)
		Oh well, the world is not perfect. */

		if (cmd == FX_SPECIAL) {
			if (param)
				chan->mem_special = param;
			else
				param = chan->mem_special;
			if (param >> 4 == 0xd) {
				// Ideally this would use SONG_FIRSTTICK, but Impulse Tracker has a bug here :)
				if (firsttick) {
					chan->cd_note_delay = (param & 0xf) ? (param & 0xf) : 1;
					continue; // notes never play on the first tick with SDx, go away
				}
				if (--chan->cd_note_delay > 0)
					continue; // not our turn yet, go away
				start_note = (chan->cd_note_delay == 0);
			}
		}

		// Handles note/instrument/volume changes
		if (start_note) {
			uint32_t note = chan->row_note;
			/* MPT test case InstrumentNumberChange.it */
			if (csf->flags & SONG_INSTRUMENTMODE && (NOTE_IS_NOTE(note) || note == NOTE_NONE)) {
				int instrcheck = instr ? instr : chan->last_instrument;
				if (instrcheck && (instrcheck < 0 || instrcheck > MAX_INSTRUMENTS || csf->instruments[instrcheck] == NULL)) {
					note = NOTE_NONE;
					instr = 0;
				}
			}
			if (csf->flags & SONG_INSTRUMENTMODE && instr && !NOTE_IS_NOTE(note)) {
				if ((porta && csf->flags & SONG_COMPATGXX)
					|| (!porta && csf->flags & SONG_ITOLDEFFECTS)) {
					env_reset(chan, 1);
					chan->fadeout_volume = 65536;
				}
			}

			if (instr && note == NOTE_NONE) {
				if (csf->flags & SONG_INSTRUMENTMODE) {
					if (chan->ptr_sample)
						chan->volume = chan->ptr_sample->volume;
				} else if (instr < MAX_SAMPLES) {
					chan->volume = csf->samples[instr].volume;
				}

				if (csf->flags & SONG_INSTRUMENTMODE) {
					if (instr < MAX_INSTRUMENTS && (chan->ptr_instrument != csf->instruments[instr] || !chan->current_sample_data))
						note = chan->note;
				} else {
					if (instr < MAX_SAMPLES && (chan->ptr_sample != &csf->samples[instr] || !chan->current_sample_data))
						note = chan->note;
				}
			}
			// Invalid Instrument ?
			if (instr >= MAX_INSTRUMENTS)
				instr = 0;

			if (NOTE_IS_CONTROL(note)) {
				if (instr) {
					int smp = instr;
					if (csf->flags & SONG_INSTRUMENTMODE) {
						smp = 0;
						if (csf->instruments[instr])
							smp = csf->instruments[instr]->sample_map[chan->note];
					}
					if (smp > 0 && smp < MAX_SAMPLES)
						chan->volume = csf->samples[smp].volume;
				}

				if (!(csf->flags & SONG_ITOLDEFFECTS))
					instr = 0;
			}

			// Note Cut/Off/Fade => ignore instrument
			if (NOTE_IS_CONTROL(note) || (note != NOTE_NONE && !porta)) {
				/* This is required when the instrument changes (KeyOff is not called) */
				/* Possibly a better bugfix could be devised. --Bisqwit */
				if (chan->flags & CHN_ADLIB) {
					//Do this only if really an adlib chan. Important!
					OPL_NoteOff(nchan);
					OPL_Touch(nchan, 0);
				}
				GM_KeyOff(nchan);
				GM_Touch(nchan, 0);
			}

			const int previous_new_note = chan->new_note; 
			if (NOTE_IS_NOTE(note)) {
				chan->new_note = note;

				if (!porta)
					csf_check_nna(csf, nchan, instr, note, 0);

				if (chan->channel_panning > 0) {
					chan->panning = (chan->channel_panning & 0x7FFF) - 1;
					if (chan->channel_panning & 0x8000)
						chan->flags |= CHN_SURROUND;
					chan->channel_panning = 0;
				}
			}
			// Instrument Change ?
			if (instr) {
				const song_sample_t *psmp = chan->ptr_sample;
				//const song_instrument_t *penv = chan->ptr_instrument;

				csf_instrument_change(csf, chan, instr, porta, 1);
				if (csf->samples[instr].flags & CHN_ADLIB) {
					OPL_Patch(nchan, csf->samples[instr].adlib_bytes);
				}

				if((csf->flags & SONG_INSTRUMENTMODE) && csf->instruments[instr])
					GM_DPatch(nchan, csf->instruments[instr]->midi_program,
						csf->instruments[instr]->midi_bank,
						csf->instruments[instr]->midi_channel_mask);

				if (NOTE_IS_NOTE(note)) {
					chan->new_instrument = 0;
					if (psmp != chan->ptr_sample) {
						chan->position = chan->position_frac = 0;
					}
				}
			}
			// New Note ?
			if (note != NOTE_NONE) {
				if (!instr && chan->new_instrument && NOTE_IS_NOTE(note)) {
					if (NOTE_IS_NOTE(previous_new_note)) {
						chan->new_note = previous_new_note;
					}
					csf_instrument_change(csf, chan, chan->new_instrument, porta, 0);
					chan->new_note = note;
					if ((csf->flags & SONG_INSTRUMENTMODE)
					    && chan->new_instrument < MAX_INSTRUMENTS
					    && csf->instruments[chan->new_instrument]) {
						if (csf->samples[chan->new_instrument].flags & CHN_ADLIB) {
							OPL_Patch(nchan, csf->samples[chan->new_instrument].adlib_bytes);
						}
						GM_DPatch(nchan, csf->instruments[chan->new_instrument]->midi_program,
							csf->instruments[chan->new_instrument]->midi_bank,
							csf->instruments[chan->new_instrument]->midi_channel_mask);
					}
					chan->new_instrument = 0;
				}
				csf_note_change(csf, nchan, note, porta, 0, !instr);
			}
		}

		// Initialize portamento command memory (needs to be done in exactly this order)
		if (firsttick) {
			const int effect_column_tone_porta = (cmd == FX_TONEPORTAMENTO || cmd == FX_TONEPORTAVOL);
			if (effect_column_tone_porta) {
				uint32_t toneporta_param = (cmd != FX_TONEPORTAVOL ? param : 0);
				if (toneporta_param)
					chan->mem_portanote = toneporta_param;
				else if(!toneporta_param && !(csf->flags & SONG_COMPATGXX))
					chan->mem_portanote = chan->mem_pitchslide;
				if (!(csf->flags & SONG_COMPATGXX))
					chan->mem_pitchslide = chan->mem_portanote;
			}
			if (volcmd == VOLFX_TONEPORTAMENTO) {
				if (vol)
					chan->mem_portanote = vc_portamento_table[vol & 0x0F];
				if (!(csf->flags & SONG_COMPATGXX))
					chan->mem_pitchslide = chan->mem_portanote;
			}

			if (vol && (volcmd == VOLFX_PORTAUP || volcmd == VOLFX_PORTADOWN)) {
				chan->mem_pitchslide = 4 * vol;
				if (!effect_column_tone_porta && !(csf->flags & SONG_COMPATGXX))
					chan->mem_portanote = chan->mem_pitchslide;
			}
			if (param && (cmd == FX_PORTAMENTOUP || cmd == FX_PORTAMENTODOWN)) {
				chan->mem_pitchslide = param;
				if (!(csf->flags & SONG_COMPATGXX))
					chan->mem_portanote = chan->mem_pitchslide;
			}
		}

		handle_voleffect(csf, chan, volcmd, vol, firsttick, start_note);
		handle_effect(csf, nchan, cmd, param, porta, firsttick);
	}
}
