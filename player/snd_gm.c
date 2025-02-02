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

/* This is a wrapper which converts S3M style thinking
 * into MIDI style thinking. */

#include "headers.h"

#include "log.h"
#include "it.h" // needed for status.flags
#include "player/sndfile.h"
#include "player/snd_gm.h"
#include "song.h" // for 'current_song', which we shouldn't need

#include <math.h> // for log


#define LinearMidivol 1
#define PitchBendCenter 0x2000

// Channel handling modes:
#define AlwaysHonor (0)
#define TryHonor    (1)
#define Ignore      (2)

#define PreferredChannelHandlingMode AlwaysHonor

// The range of bending equivalent to 1 semitone.
// 0x2000 is the value used in TiMiDity++.
// In this module, we prefer a full range of octave, to support a reasonable
// range of pitch-bends used in tracker modules, and we reprogram the MIDI
// synthesizer to support that range. So we specify it as such:
#define SEMITONE_BEND_DEPTH (0x2000 / 12)


/* GENERAL MIDI (GM) COMMANDS:
8x       1000xxxx     nn vv         Note off (key is released)
				    nn=note number
				    vv=velocity

9x       1001xxxx     nn vv         Note on (key is pressed)
				    nn=note number
				    vv=velocity

Ax       1010xxxx     nn vv         Key after-touch
				    nn=note number
				    vv=velocity

Bx       1011xxxx     cc vv         Control Change
				    cc=controller number
				    vv=new value

Cx       1100xxxx     pp            Program (patch) change
				    pp=new program number

Dx       1101xxxx     cc            Channel after-touch
				    cc=channel number

Ex       1110xxxx     bb tt         Pitch wheel change (2000h is normal
							or no change)
				    bb=bottom (least sig) 7 bits of value
				    tt=top (most sig) 7 bits of value

About the controllers... In AWE32 they are:
    0=Bank select               7=Master volume     11=Expression(volume?)
    1=Modulation Wheel(Vibrato)10=Pan Position      64=Sustain Pedal
    6=Data Entry MSB           38=Data Entry LSB    91=Effects Depth(Reverb)
  120=All Sound Off           123=All Notes Off     93=Chorus Depth
  100=RPN # LSB       101=RPN # MSB
   98=NRPN # LSB       99=NRPN # MSB

    1=Vibrato, 121=reset vibrato,bend

    To set RPNs (registered parameters):
      control 101 <- param number MSB
      control 100 <- param number LSB
      control   6 <- value number MSB
     <control  38 <- value number LSB> optional
    For NRPNs, the procedure is the same, but you use 98,99 instead of 100,101.

       param 0 = pitch bend sensitivity
       param 1 = finetuning
       param 2 = coarse tuning
       param 3 = tuning program select
       param 4 = tuning bank select
       param 0x4080 = reset (value omitted)

    References:
       - SoundBlaster AWE32 documentation
       - http://www.philrees.co.uk/nrpnq.htm
*/

static void MPU_SendCommand(song_t *csf, const unsigned char* buf, uint32_t nbytes, int32_t c)
{
	if (!nbytes)
		return;

	csf_midi_send(csf, buf, nbytes, c, 0);
}


static void MPU_Ctrl(song_t *csf, int32_t c, int32_t i, int32_t v)
{
	if (!(status.flags & MIDI_LIKE_TRACKER))
		return;

	unsigned char buf[3] = {0xB0 + c, i, v};
	MPU_SendCommand(csf, buf, 3, c);
}


static void MPU_Patch(song_t *csf, int32_t c, int32_t p)
{
	if (!(status.flags & MIDI_LIKE_TRACKER))
		return;

	unsigned char buf[2] = {0xC0 + c, p};
	MPU_SendCommand(csf, buf, 2, c);
}


static void MPU_Bend(song_t *csf, int32_t c, int32_t w)
{
	if (!(status.flags & MIDI_LIKE_TRACKER))
		return;

	unsigned char buf[3] = {0xE0 + c, w & 127, w >> 7};
	MPU_SendCommand(csf, buf, 3, c);
}


static void MPU_NoteOn(song_t *csf, int32_t c, int32_t k, int32_t v)
{
	if (!(status.flags & MIDI_LIKE_TRACKER))
		return;

	unsigned char buf[3] = {0x90 + c, k, v};
	MPU_SendCommand(csf, buf, 3, c);
}


static void MPU_NoteOff(song_t *csf, int32_t c, int32_t k, int32_t v)
{
	if (!(status.flags & MIDI_LIKE_TRACKER))
		return;

	if (((unsigned char) csf->midi_running_status) == 0x90 + c) {
		// send a zero-velocity keyoff instead for optimization
		MPU_NoteOn(csf, c, k, 0);
	} else {
		unsigned char buf[3] = {0x80+c, k, v};
		MPU_SendCommand(csf, buf, 3, c);
	}
}


static void MPU_SendPN(song_t *csf, int32_t ch,
		       uint32_t portindex,
		       uint32_t param, uint32_t valuehi, uint32_t valuelo)
{
	MPU_Ctrl(csf, ch, portindex+1, param>>7);
	MPU_Ctrl(csf, ch, portindex+0, param & 0x80);

	if (param != 0x4080) {
		MPU_Ctrl(csf, ch, 6, valuehi);

		if (valuelo)
			MPU_Ctrl(csf, ch, 38, valuelo);
	}
}


#define MPU_SendNRPN(csf,ch,param,hi,lo) MPU_SendPN(csf,ch,98,param,hi,lo)
#define MPU_SendRPN(csf,ch,param,hi,lo) MPU_SendPN(csf,ch,100,param,hi,lo)
#define MPU_ResetPN(csf,ch) MPU_SendRPN(csf,ch,0x4080,0,0)


#define s3m_active(ci) \
    ((ci).note && (ci).chan >= 0)

// patch: definitely percussion
// pref_chn_mask: to be played on P channel, so it's percussion
#define s3m_percussion(ci) \
    ((ci).patch & 0x80 || (ci).pref_chn_mask & (1 << 9))


static void s3m_reset(song_s3m_channel_info_t *ci) {
	ci->note          = 0;
	ci->patch         = 0;
	ci->bank          = 0;
	ci->pan           = 0;
	ci->chan          = -1;
	ci->pref_chn_mask = -1;
}

static void msi_reset(song_midi_state_t *msi)
{
	msi->volume = 255;
	msi->patch  = 255;
	msi->bank   = 255;
	msi->bend   = PitchBendCenter;
	msi->pan    = 0;
}

#define msi_know_something(msi) ((msi).patch != 255)


static void msi_set_volume(song_t *csf, song_midi_state_t *msi, int32_t c, uint32_t newvol)
{
	if (msi->volume != newvol) {
		msi->volume = newvol;
		MPU_Ctrl(csf, c, 7, newvol);
	}
}


static void msi_set_patch_and_bank(song_t *csf, song_midi_state_t *msi, int32_t c, int32_t p, int32_t b)
{
	if (msi->bank != b) {
		msi->bank = b;
		MPU_Ctrl(csf, c, 0, b);
	}

	if (msi->patch != p) {
		msi->patch = p;
		MPU_Patch(csf, c, p);
	}
}


static void msi_set_pitch_bend(song_t *csf, song_midi_state_t *msi, int32_t c, int32_t value)
{
	if (msi->bend != value) {
		msi->bend = value;
		MPU_Bend(csf, c, value);
	}
}


static void msi_set_pan(song_t *csf, song_midi_state_t *msi, int32_t c, int32_t value)
{
	if (msi->pan != value) {
	    msi->pan = value;
	    MPU_Ctrl(csf, c, 10, (unsigned char)(value + 128) / 2);
	}
}

static unsigned char GM_volume(unsigned char vol) // Converts the volume
{
	/* Converts volume in range 0..127 to range 0..127 with clamping */
	return CLAMP(vol, 0, 127);
}


static int32_t GM_AllocateMelodyChannel(song_t *csf, int32_t c, int32_t patch, int32_t bank, int32_t key, int32_t pref_chn_mask)
{
	/* Returns a MIDI channel number on
	 * which this key can be played safely.
	 *
	 * Things that matter:
	 *
	 *  -4      The channel has a different patch selected
	 *  -6      The channel has a different bank selected
	 *  -9      The channel already has the same key
	 *  +1      The channel number corresponds to c
	 *  +2      The channel has no notes playing
	 *  -999    The channel number is 9 (percussion-only channel)
	 *
	 * Channel with biggest score is selected.
	 *
	 */
	int32_t bad_channels[16] = {0};  // channels having the same key playing
	int32_t used_channels[16] = {0}; // channels having something playing

	for (uint32_t a = 0; a < MAX_VOICES; ++a) {
		if (s3m_active(csf->midi_s3m_chans[a]) &&
		    !s3m_percussion(csf->midi_s3m_chans[a])) {
			//fprintf(stderr, "S3M[%d] active at %d\n", a, csf->midi_s3m_chans[a].chan);
			used_channels[csf->midi_s3m_chans[a].chan] = 1; // channel is active

			if (csf->midi_s3m_chans[a].note == key)
				bad_channels[csf->midi_s3m_chans[a].chan] = 1; // ...with the same key
		}
	}

	int32_t best_mc = c % 16,
	        best_score = -999;

	for (int32_t mc = 0; mc < 16; ++mc) {
		if (mc == 9)
			continue; // percussion channel is never chosen for melody.

		int32_t score = 0;

		if (PreferredChannelHandlingMode != TryHonor &&
		    msi_know_something(csf->midi_chans[mc])) {
			if (csf->midi_chans[mc].patch != patch) score -= 4; // different patch
			if (csf->midi_chans[mc].bank  !=  bank) score -= 6; // different bank
		}

		if (PreferredChannelHandlingMode == TryHonor) {
			if (pref_chn_mask & (1 << mc))
				score += 1; // same channel number
		}
		else if (PreferredChannelHandlingMode == AlwaysHonor) {
			// disallow channels that are not allowed
			if (pref_chn_mask >= 0x10000) {
				if (mc != c % 16)
					continue;
			}
			else if (!(pref_chn_mask & (1 << mc)))
			       continue;
		}
		else {
			if (c == mc)
				score += 1; // same channel number
		}

		if (bad_channels[mc])
			score -= 9; // has same key on

		if (!used_channels[mc])
			score += 2; // channel is unused

		//fprintf(stderr, "score %d for channel %d\n", score, mc);
		if (score > best_score) {
			best_score = score;
			best_mc = mc;
		}
	}

	//fprintf(stderr, "BEST SCORE %d FOR CHANNEL %d\n", best_score,best_mc);
	return best_mc;
}


void GM_Patch(song_t *csf, int32_t c, unsigned char p, int32_t pref_chn_mask)
{
	if (c < 0 || ((uint32_t) c) >= MAX_VOICES)
		return;

	csf->midi_s3m_chans[c].patch         = p; // No actual data is sent.
	csf->midi_s3m_chans[c].pref_chn_mask = pref_chn_mask;
}


void GM_Bank(song_t *csf, int32_t c, unsigned char b)
{
	if (c < 0 || ((uint32_t) c) >= MAX_VOICES)
		return;

	csf->midi_s3m_chans[c].bank = b; // No actual data is sent yet.
}


void GM_Touch(song_t *csf, int32_t c, unsigned char vol)
{
	if (c < 0 || ((uint32_t) c) >= MAX_VOICES)
		return;

	/* This function must only be called when
	 * a key has been played on the channel. */
	if (!s3m_active(csf->midi_s3m_chans[c]))
		return;

	int32_t mc = csf->midi_s3m_chans[c].chan;
	msi_set_volume(csf, &csf->midi_chans[mc], mc, GM_volume(vol));
}


void GM_KeyOn(song_t *csf, int32_t c, unsigned char key, unsigned char vol)
{
	if (c < 0 || ((uint32_t) c) >= MAX_VOICES)
		return;

	GM_KeyOff(csf, c); // Ensure the previous key on this channel is off.

	if (s3m_active(csf->midi_s3m_chans[c]))
		return; // be sure the channel is deactivated.

#ifdef GM_DEBUG
	fprintf(stderr, "GM_KeyOn(%d, %d,%d)\n", c, key,vol);
#endif

	if (s3m_percussion(csf->midi_s3m_chans[c])) {
		// Percussion always uses channel 9.
		int32_t percu = key;

		if (csf->midi_s3m_chans[c].patch & 0x80)
			percu = csf->midi_s3m_chans[c].patch - 128;

		int32_t mc = csf->midi_s3m_chans[c].chan = 9;
		// Percussion can have different banks too
		msi_set_patch_and_bank(csf, &csf->midi_chans[mc], mc, csf->midi_s3m_chans[c].patch, csf->midi_s3m_chans[c].bank);
		msi_set_pan(csf, &csf->midi_chans[mc], mc, csf->midi_s3m_chans[c].pan);
		msi_set_volume(csf, &csf->midi_chans[mc], mc, GM_volume(vol));
		csf->midi_s3m_chans[c].note = key;
		MPU_NoteOn(csf, mc, csf->midi_s3m_chans[c].note = percu, 127);
	}
	else {
		// Allocate a MIDI channel for this key.
		// Note: If you need to transpone the key, do it before allocating the channel.

		int32_t mc = csf->midi_s3m_chans[c].chan = GM_AllocateMelodyChannel(
			csf, c, csf->midi_s3m_chans[c].patch, csf->midi_s3m_chans[c].bank,
			key, csf->midi_s3m_chans[c].pref_chn_mask);

		msi_set_patch_and_bank(csf, &csf->midi_chans[mc], mc, csf->midi_s3m_chans[c].patch, csf->midi_s3m_chans[c].bank);
		msi_set_volume(csf, &csf->midi_chans[mc], mc, GM_volume(vol));
		MPU_NoteOn(csf, mc, csf->midi_s3m_chans[c].note = key, 127);
		msi_set_pan(csf, &csf->midi_chans[mc], mc, csf->midi_s3m_chans[c].pan);
	}
}


void GM_KeyOff(song_t *csf, int32_t c)
{
	if (c < 0 || ((uint32_t)c) >= MAX_VOICES)
		return;

	if (!s3m_active(csf->midi_s3m_chans[c]))
		return; // nothing to do

#ifdef GM_DEBUG
	fprintf(stderr, "GM_KeyOff(%d)\n", c);
#endif

	int32_t mc = csf->midi_s3m_chans[c].chan;

	MPU_NoteOff(csf, mc, csf->midi_s3m_chans[c].note, 0);
	csf->midi_s3m_chans[c].chan = -1;
	csf->midi_s3m_chans[c].note = 0;
	csf->midi_s3m_chans[c].pan  = 0;
	// Don't reset the pitch bend, it will make sustains sound bad
}


void GM_Bend(song_t *csf, int32_t c, uint32_t count)
{
       if (c < 0 || ((uint32_t)c) >= MAX_VOICES)
		return;

	/* I hope nobody tries to bend hi-hat or something like that :-) */
	/* 1998-10-03 01:50 Apparently that can happen too...
	   For example in the last pattern of urq.mod there's
	   a hit of a heavy plate, which is followed by a J0A
	   0.5 seconds thereafter for the same channel.
	   Unfortunately MIDI cannot do that. Drum plate
	   sizes can rarely be adjusted while playing. -Bisqwit
	   However, we don't stop anyone from trying...
	*/

	if (s3m_active(csf->midi_s3m_chans[c])) {
		int32_t mc = csf->midi_s3m_chans[c].chan;
		msi_set_pitch_bend(csf, &csf->midi_chans[mc], mc, count);
	}
}


void GM_Reset(song_t *csf, int quitting)
{
#ifdef GM_DEBUG
	csf->midi_resetting = 1;
#endif
	uint32_t a;
	//fprintf(stderr, "GM_Reset\n");

	for (a = 0; a < MAX_VOICES; a++) {
		GM_KeyOff(csf, a);
		//csf->midi_s3m_chans[a].patch = csf->midi_s3m_chans[a].bank = csf->midi_s3m_chans[a].pan = 0;
		s3m_reset(&csf->midi_s3m_chans[a]);
	}

	// How many semitones does it take to screw in the full 0x4000 bending range of lightbulbs?
	// We scale the number by 128, because the RPN allows for finetuning.
	int n_semitones_times_128 = 128 * 0x2000 / SEMITONE_BEND_DEPTH;

	if (quitting) {
		// When quitting, we reprogram the pitch bend sensitivity into
		// the range of 1 semitone (TiMiDity++'s default, which is
		// probably a default on other devices as well), instead of
		// what we preferred for IT playback.
		n_semitones_times_128 = 128;
	}

	for (a = 0; a < 16; a++) {
		// XXX
		// XXX Porting note:
		// XXX This might go wrong because the midi struct is already reset
		// XXX  by the constructor in the C++ version.
		// XXX
		MPU_Ctrl(csf, a, 120,  0);   // turn off all sounds
		MPU_Ctrl(csf, a, 123,  0);   // turn off all notes
		MPU_Ctrl(csf, a, 121, 0);    // reset vibrato, bend
		msi_set_pan(csf, &csf->midi_chans[a], a, 0);           // reset pan position
		msi_set_volume(csf, &csf->midi_chans[a], a, 127);      // set channel volume
		msi_set_pitch_bend(csf, &csf->midi_chans[a], a, PitchBendCenter); // reset pitch bends

		msi_reset(&csf->midi_chans[a]);

		// Reprogram the pitch bending sensitivity to our desired depth.
		MPU_SendRPN(csf, a, 0, n_semitones_times_128 / 128,
			  n_semitones_times_128 % 128);

		MPU_ResetPN(csf, a);
	}

#ifdef GM_DEBUG
	csf->midi_resetting = 0;
	fprintf(stderr, "-------------- GM_Reset completed ---------------\n");
#endif
}


void GM_DPatch(song_t *csf, int32_t ch, unsigned char GM, unsigned char bank, int32_t pref_chn_mask)
{
#ifdef GM_DEBUG
	fprintf(stderr, "GM_DPatch(%d, %02X @ %d)\n", ch, GM, bank);
#endif

	if (ch < 0 || ((uint32_t)ch) >= MAX_VOICES)
		return;

	GM_Bank(csf, ch, bank);
	GM_Patch(csf, ch, GM, pref_chn_mask);
}


void GM_Pan(song_t *csf, int32_t c, signed char val)
{
	//fprintf(stderr, "GM_Pan(%d,%d)\n", c,val);
	if (c < 0 || ((uint32_t)c) >= MAX_VOICES)
		return;

	csf->midi_s3m_chans[c].pan = val;

	// If a note is playing, effect immediately.
	if (s3m_active(csf->midi_s3m_chans[c])) {
		int32_t mc = csf->midi_s3m_chans[c].chan;
		msi_set_pan(csf, &csf->midi_chans[mc], mc, val);
	}
}




void GM_SetFreqAndVol(song_t *csf, int32_t c, int32_t Hertz, int32_t vol, MidiBendMode bend_mode, int32_t keyoff)
{
#ifdef GM_DEBUG
	fprintf(stderr, "GM_SetFreqAndVol(%d,%d,%d)\n", c,Hertz,vol);
#endif
	if (c < 0 || ((uint32_t)c) >= MAX_VOICES)
		return;

	/*
	Figure out the note and bending corresponding to this Hertz reading.

	TiMiDity++ calculates its frequencies this way (equal temperament):
	  freq(0<=i<128) := 440 * pow(2.0, (i - 69) / 12.0)
	  bend_fine(0<=i<256) := pow(2.0, i/12.0/256)
	  bend_coarse(0<=i<128) := pow(2.0, i/12.0)

	I suppose we can do the mathematical route.  -Bisqwit
	       hertz = 440*pow(2, (midinote-69)/12)
	     Maxima gives us (solve+expand):
	       midinote = 12 * log(hertz/440) / log(2) + 69
	     In other words:
	       midinote = 12 * log2(hertz/440) + 69
	     Or:
	       midinote = 12 * log2(hertz/55) + 33 (but I prefer the above for clarity)

	      (55 and 33 are related to 440 and 69 the following way:
		       log2(440) = ~8.7
		       440/8   = 55
		       log2(8) = 3
		       12 * 3  = 36
		       69-36   = 33.
	       I guess Maxima's expression preserves more floating
	       point accuracy, but given the range of the numbers
	       we work here with, that's hardly an issue.)
	*/
	double midinote = 69 + 12.0 * log(Hertz/440.0) / log(2.0);

	// Reduce by a couple of octaves... Apparently the hertz
	// value that comes from SchismTracker is upscaled by some 2^5.
	midinote -= 12*5;

	int32_t note = csf->midi_s3m_chans[c].note; // what's playing on the channel right now?

	int32_t new_note = !s3m_active(csf->midi_s3m_chans[c]);

	if (new_note && !keyoff) {
		// If the note is not active, activate it first.
		// Choose the nearest note to Hertz.
		note = (int32_t)(midinote + 0.5);

		// If we are expecting a bend exclusively in either direction,
		// prepare to utilize the full extent of available pitch bending.
		if (bend_mode == MIDI_BEND_DOWN) note += (int32_t)(0x2000 / SEMITONE_BEND_DEPTH);
		if (bend_mode == MIDI_BEND_UP)   note -= (int32_t)(0x2000 / SEMITONE_BEND_DEPTH);

		if (note < 1) note = 1;
		if (note > 127) note = 127;
		GM_KeyOn(csf, c, note, vol);
	}

	if (!s3m_percussion(csf->midi_s3m_chans[c])) { // give us a break, don't bend percussive instruments
		double notediff = midinote-note; // The difference is our bend value
		int32_t bend = (int32_t)(notediff * SEMITONE_BEND_DEPTH) + PitchBendCenter;

		// Because the log2 calculation does not always give pure notes,
		// and in fact, gives a lot of variation, we reduce the bending
		// precision to 100 cents. This is accurate enough for almost
		// all purposes, but will significantly reduce the bend event load.
		//const int32_t bend_artificial_inaccuracy = SEMITONE_BEND_DEPTH / 100;
		//bend = (bend / bend_artificial_inaccuracy) * bend_artificial_inaccuracy;

		// Clamp the bending value so that we won't break the protocol
		if(bend < 0) bend = 0;
		if(bend > 0x3FFF) bend = 0x3FFF;

		GM_Bend(csf, c, bend);
	}

	if (vol < 0) vol = 0;
	else if (vol > 127) vol = 127;

	//if (!new_note)
	GM_Touch(csf, c, vol);
}

void GM_SendSongStartCode(song_t *csf)    { unsigned char c = 0xFA; MPU_SendCommand(csf, &c, 1, 0); csf->midi_last_song_counter = 0; }
void GM_SendSongStopCode(song_t *csf)     { unsigned char c = 0xFC; MPU_SendCommand(csf, &c, 1, 0); csf->midi_last_song_counter = 0; }
void GM_SendSongContinueCode(song_t *csf) { unsigned char c = 0xFB; MPU_SendCommand(csf, &c, 1, 0); csf->midi_last_song_counter = 0; }
void GM_SendSongTickCode(song_t *csf)     { unsigned char c = 0xF8; MPU_SendCommand(csf, &c, 1, 0); }

void GM_SendSongPositionCode(song_t *csf, uint32_t note16pos)
{
	unsigned char buf[3] = {0xF2, note16pos & 127, (note16pos >> 7) & 127};
	MPU_SendCommand(csf, buf, 3, 0);
	csf->midi_last_song_counter = 0.0;
}


void GM_IncrementSongCounter(song_t *csf, int32_t count)
{
	/* We assume that one schism tick = one midi tick (24ppq).
	 *
	 * We also know that:
	 *                   5 * mixingrate
	 * Length of tick is -------------- samples
	 *                     2 * cmdT
	 *
	 * where cmdT = last FX_TEMPO = current_tempo
	 */

	int32_t TickLengthInSamplesHi = 5 * current_song->mix_frequency;
	int32_t TickLengthInSamplesLo = 2 * current_song->current_tempo;

	double TickLengthInSamples = TickLengthInSamplesHi / (double) TickLengthInSamplesLo;

	/* TODO: Use fraction arithmetics instead (note: cmdA, cmdT may change any time) */

	csf->midi_last_song_counter += count / TickLengthInSamples;

	int32_t n_Ticks = (int32_t)csf->midi_last_song_counter;

	if (n_Ticks) {
		for (int32_t a = 0; a < n_Ticks; ++a)
			GM_SendSongTickCode(csf);

		csf->midi_last_song_counter -= n_Ticks;
	}
}

