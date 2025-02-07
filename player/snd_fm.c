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

#include "player/fmopl.h"
#include "player/snd_fm.h"
#include "player/sndfile.h"
#include "log.h"
#include "util.h" /* for clamp */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define OPLRATEBASE 49716 // It's not a good idea to deviate from this.

#if OPLSOURCE == 2
	#define OPLNew(x,r)  ym3812_init(x, r)
	#define OPLResetChip ym3812_reset_chip
	#define OPLWrite     ym3812_write
	#define OPLReadChip     ym3812_read
	#define OPLUpdateOne ym3812_update_one
	#define OPLCloseChip     ym3812_shutdown
	// OPL2 = 3579552Hz
	#define OPLRATEDIVISOR 72
#elif OPLSOURCE == 3
	#define OPLNew(x,r)  ymf262_init(x, r)
	#define OPLResetChip ymf262_reset_chip
	#define OPLWrite     ymf262_write
	#define OPLReadChip     ymf262_read
	#define OPLUpdateOne ymf262_update_one
	#define OPLCloseChip     ymf262_shutdown
	// OPL3 = 14318208Hz
	#define OPLRATEDIVISOR 288
#else
# error "The current value of OPLSOURCE isn't supported! Check config.h."
#endif

/* Schismtracker output buffer works in 27bits: [MIXING_CLIPMIN..MIXING_CLIPMAX]
fmopl works in 16bits, although tested output used to range +-10000 instead of 
	+-20000 from adlibtracker/screamtracker in dosbox. So we need 11 bits + 1 extra bit.
Also note when comparing volumes, that Screamtracker output on mono with PCM samples is not reduced by half.
*/
#define OPL_VOLUME 2274

/*
The documentation in this file regarding the output ports,
including the comment "Don't ask me why", are attributed
to Jeffrey S. Lee's article:
	Programming the AdLib/Sound Blaster
		 FM Music Chips
	 Version 2.0 (24 Feb 1992)
*/
#define OPL_BASE 0x388

/// Convert the given f-number and block into a note frequency.
/**
* @param fnum
* Input frequency number, between 0 and 1023 inclusive. Values outside this
* range will cause assertion failures.
*
* @param block
* Input block number, between 0 and 7 inclusive. Values outside this range
* will cause assertion failures.
*
* @param conversionFactor
* Conversion factor to use. Normally will be 49716 and occasionally 50000.
*
* @return The converted frequency in milliHertz.
*/
static int32_t fnumToMilliHertz(uint32_t fnum, uint32_t block, uint32_t conversionFactor)
{
	// Original formula
	//return 1000 * conversionFactor * (double)fnum * pow(2, (double)((signed)block - 20));

	// More efficient version
	return (1000ull * conversionFactor * fnum) >> (20 - block);
}

/// Convert a frequency into an OPL f-number
/**
* @param milliHertz
* Input frequency.
*
* @param fnum
* Output frequency number for OPL chip. This is a 10-bit number, so it will
* always be between 0 and 1023 inclusive.
*
* @param block
* Output block number for OPL chip. This is a 3-bit number, so it will
* always be between 0 and 7 inclusive.
*
* @param conversionFactor
* Conversion factor to use. Normally will be 49716 and occasionally 50000.
*
* @post fnum will be set to a value between 0 and 1023 inclusive. block will
* be set to a value between 0 and 7 inclusive. assert() calls inside this
* function ensure this will always be the case.
*
* @note As the block value increases, the frequency difference between two
* adjacent fnum values increases. This means the higher the frequency,
* the less precision is available to represent it. Therefore, converting
* a value to fnum/block and back to milliHertz is not guaranteed to reproduce
* the original value.
*/
static void milliHertzToFnum(uint32_t milliHertz, uint32_t *fnum, uint32_t *block, uint32_t conversionFactor)
{
	// Special case to avoid divide by zero
	if (milliHertz <= 0) {
		*block = 0; // actually any block will work
		*fnum = 0;
		return;
	}

	// Special case for frequencies too high to produce
	if (milliHertz > 6208431) {
		*block = 7;
		*fnum = 1023;
		return;
	}

	/// This formula will provide a pretty good estimate as to the best block to
	/// use for a given frequency.  It tries to use the lowest possible block
	/// number that is capable of representing the given frequency.  This is
	/// because as the block number increases, the precision decreases (i.e. there
	/// are larger steps between adjacent note frequencies.)  The 6M constant is
	/// the largest frequency (in milliHertz) that can be represented by the
	/// block/fnum system.
	//int invertedBlock = log2(6208431 / milliHertz);

	// Very low frequencies will produce very high inverted block numbers, but
	// as they can all be covered by inverted block 7 (block 0) we can just clip
	// the value.
	//if (invertedBlock > 7) invertedBlock = 7;
	//*block = 7 - invertedBlock;

	// This is a bit more efficient and doesn't need log2() from math.h
	if (milliHertz > 3104215) *block = 7;
	else if (milliHertz > 1552107) *block = 6;
	else if (milliHertz > 776053) *block = 5;
	else if (milliHertz > 388026) *block = 4;
	else if (milliHertz > 194013) *block = 3;
	else if (milliHertz > 97006) *block = 2;
	else if (milliHertz > 48503) *block = 1;
	else *block = 0;

	// Original formula
	//*fnum = milliHertz * pow(2, 20 - *block) / 1000 / conversionFactor + 0.5;

	// Slightly more efficient version
	*fnum = ((uint64_t)milliHertz << (20 - *block)) / (conversionFactor * 1000.0) + 0.5;

	if (*fnum > 1023) {
		(*block)++;
		*fnum = ((uint64_t)milliHertz << (20 - *block)) / (conversionFactor * 1000.0) + 0.5;
	}

	return;
}

static void Fmdrv_Outportb(song_t *csf, uint32_t port, uint32_t value)
{
	if (csf->opl == NULL ||
		((int32_t) port) < OPL_BASE ||
		((int32_t) port) >= OPL_BASE + 4)
		return;

	uint32_t ind = port - OPL_BASE;
	OPLWrite(csf->opl, ind, value);

	if (ind & 1) {
		if (csf->oplregno == 4) {
			if (value == 0x80)
				csf->oplretval = 0x02;
			else if (value == 0x21)
				csf->oplretval = 0xC0;
		}
	}
	else
		csf->oplregno = value;
}


static unsigned char Fmdrv_Inportb(song_t *csf, uint32_t port)
{
	return (((int32_t) port) >= OPL_BASE &&
		((int32_t) port) < OPL_BASE + 4) ? csf->oplretval : 0;
}


void Fmdrv_Init(song_t *csf, int32_t mixfreq)
{
	if (csf->opl != NULL) {
		OPLCloseChip(csf->opl);
		csf->opl = NULL;
	}
	// Clock = speed at which the chip works. mixfreq = audio resampler
	csf->opl = OPLNew(OPLRATEBASE * OPLRATEDIVISOR, mixfreq);
	OPL_Reset(csf);
}


void Fmdrv_MixTo(song_t *csf, int32_t *target, uint32_t count)
{
	if (!csf->opl_fm_active)
		return;

#if OPLSOURCE == 2
	SCHISM_VLA_ALLOC(int16_t, buf, count);

	memset(buf, 0, SCHISM_VLA_SIZEOF(buf));

	// mono. Single buffer.

	OPLUpdateOne(csf->opl, buf, ARRAY_SIZE(buf));
	/*
	static int counter = 0;

	for(int a = 0; a < count; ++a)
		buf[a] = ((counter++) & 0x100) ? -10000 : 10000;
	*/

	for (size_t a = 0; a < count; ++a) {
		target[a * 2 + 0] += buf[a] * OPL_VOLUME;
		target[a * 2 + 1] += buf[a] * OPL_VOLUME;
	}

	SCHISM_VLA_FREE(buf);
#else
	SCHISM_VLA_ALLOC(int16_t, buf, count * 3);

	memset(buf, 0, SCHISM_VLA_SIZEOF(buf));

	OPLUpdateOne(csf->opl, (int16_t *[]){ buf, buf + count, buf + (count * 2), buf + (count * 2) }, count);
	/*
	static int counter = 0;

	for(int a = 0; a < count; ++a)
		buf[a] = ((counter++) & 0x100) ? -10000 : 10000;
	*/

	// IF we wanted to do the stereo mix in software, we could setup the voices always in mono
	// and do the panning here.
	for (size_t a = 0; a < count; ++a) {
		target[a * 2 + 0] += buf[a] * OPL_VOLUME;
		target[a * 2 + 1] += buf[count + a] * OPL_VOLUME;
	}

	SCHISM_VLA_FREE(buf);
#endif
}


/***************************************/

static const char PortBases[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};

static int32_t GetVoice(song_t *csf, int32_t c)
{
	return csf->opl_from_chan[c];
}

static int32_t SetVoice(song_t *csf, int32_t c)
{
	int a;

	if (csf->opl_from_chan[c] == -1) {
		// Search for unused chans

		for (a = 0; a < 9; a++) {
			if (csf->opl_to_chan[a] == -1) {
				csf->opl_to_chan[a] = c;
				csf->opl_from_chan[c] = a;
				break;
			}
		}

		if (csf->opl_from_chan[c] == -1) {
			// Search for note-released chans
			for (a = 0; a < 9; a++) {
				if ((csf->opl_keyontab[a]&KEYON_BIT) == 0) {
					csf->opl_from_chan[csf->opl_to_chan[a]] = -1;
					csf->opl_to_chan[a] = c;
					csf->opl_from_chan[c] = a;
					break;
				}
			}
		}
	}
	//log_appendf(2,"entering with %d. tested? %d. selected %d. Current: %d",c,t,s,ChantoOPL[c]);
	return GetVoice(csf, c);
}

#if 0
static void FreeVoice(int c) {
	if (ChantoOPL[c] == -1)
		return;
	OPLtoChan[ChantoOPL[c]]=-1;
	ChantoOPL[c]=-1;
}
#endif

static void OPL_Byte(song_t *csf, uint32_t idx, unsigned char data)
{
	//register int a;
	Fmdrv_Outportb(csf, OPL_BASE, idx);    // for(a = 0; a < 6;  a++) Fmdrv_Inportb(OPL_BASE);
	Fmdrv_Outportb(csf, OPL_BASE + 1, data); // for(a = 0; a < 35; a++) Fmdrv_Inportb(OPL_BASE);
}
static void OPL_Byte_RightSide(song_t *csf, uint32_t idx, unsigned char data)
{
	//register int a;
	Fmdrv_Outportb(csf, OPL_BASE + 2, idx);    // for(a = 0; a < 6;  a++) Fmdrv_Inportb(OPL_BASE);
	Fmdrv_Outportb(csf, OPL_BASE + 3, data); // for(a = 0; a < 35; a++) Fmdrv_Inportb(OPL_BASE);
}


void OPL_NoteOff(song_t *csf, int32_t c)
{
	int32_t oplc = GetVoice(csf, c);
	if (oplc == -1)
		return;

	csf->opl_keyontab[oplc] &= ~(KEYON_BIT);
	OPL_Byte(csf, KEYON_BLOCK + oplc, csf->opl_keyontab[oplc]);
}


/* OPL_NoteOn changes the frequency on specified
	 channel and guarantees the key is on. (Doesn't
	 retrig, just turns the note on and sets freq.)
	 If keyoff is nonzero, doesn't even set the note on.
	 Could be used for pitch bending also. */
void OPL_HertzTouch(song_t *csf, int32_t c, int32_t milliHertz, int32_t keyoff)
{
	int32_t oplc = GetVoice(csf, c);
	if (oplc == -1)
		return;

	csf->opl_fm_active = 1;

/*
	Bytes A0-B8 - Octave / F-Number / Key-On

	7     6     5     4     3     2     1     0
	 +-----+-----+-----+-----+-----+-----+-----+-----+
	 |        F-Number (least significant byte)      |  (A0-A8)
	 +-----+-----+-----+-----+-----+-----+-----+-----+
	 |  Unused   | Key |    Octave       | F-Number  |  (B0-B8)
	 |           | On  |                 | most sig. |
	 +-----+-----+-----+-----+-----+-----+-----+-----+
*/
	uint32_t outfnum;
	uint32_t outblock;
	const int32_t conversion_factor = OPLRATEBASE; // Frequency of OPL.
	milliHertzToFnum(milliHertz, &outfnum, &outblock, conversion_factor);
	csf->opl_keyontab[oplc] = (keyoff ? 0 : KEYON_BIT)      // Key on
				| (outblock << 2)                    // Octave
				| ((outfnum >> 8) & FNUM_HIGH_MASK); // F-number high 2 bits
	OPL_Byte(csf, FNUM_LOW +    oplc, outfnum & 0xFF);  // F-Number low 8 bits
	OPL_Byte(csf, KEYON_BLOCK + oplc, csf->opl_keyontab[oplc]);
}


void OPL_Touch(song_t *csf, int32_t c, uint32_t vol)
{
//fprintf(stderr, "OPL_Touch(%d, %p:%02X.%02X.%02X.%02X-%02X.%02X.%02X.%02X-%02X.%02X.%02X, %d)\n",
//    c, D,D[0],D[1],D[2],D[3],D[4],D[5],D[6],D[7],D[8],D[9],D[10], Vol);

	int32_t oplc = GetVoice(csf, c);
	if (oplc == -1)
		return;

	const unsigned char *D = csf->opl_dtab[oplc];
	int32_t Ope = PortBases[oplc];

/*
	Bytes 40-55 - Level Key Scaling / Total Level

	7     6     5     4     3     2     1     0
	 +-----+-----+-----+-----+-----+-----+-----+-----+
	 |  Scaling  |             Total Level           |
	 |   Level   | 24    12     6     3    1.5   .75 | <-- dB
	 +-----+-----+-----+-----+-----+-----+-----+-----+
		bits 7-6 - causes output levels to decrease as the frequency
			 rises:
				00   -  no change
				10   -  1.5 dB/8ve
				01   -  3 dB/8ve
				11   -  6 dB/8ve
		bits 5-0 - controls the total output level of the operator.
			 all bits CLEAR is loudest; all bits SET is the
			 softest.  Don't ask me why.
*/
	/* 2008-09-27 Bisqwit:
	 * Did tests in ST3: The value poked
	 * to 0x43, minus from 63, is:
	 *
	 *  OplVol 63  47  31
	 * SmpVol
	 *  64     63  47  31
	 *  32     32  24  15
	 *  16     16  12   8
	 *
	 * This seems to clearly indicate that the value
	 * poked is calculated with 63 - round(oplvol*smpvol/64.0).
	 *
	 * Also, from the documentation we can deduce that
	 * the maximum volume to be set is 47.25 dB and that
	 * each increase by 1 corresponds to 0.75 dB.
	 *
	 * Since we know that 6 dB is equivalent to a doubling
	 * of the volume, we can deduce that an increase or
	 * decrease by 8 will double / halve the volume.
	 *
		D = 63-OPLVol
		NewD = 63-target

		OPLVol = 63 - D
		newvol = clip(vol,63)  -> max value of newvol=63, same as max of OPLVol.
		target = OPLVOL * (newvol/63)


		NewD = 63-(OLPVOL * (newvol/63))
		NewD = 63-((63 - D) * (newvol/63))
		NewD = 63-((63*newvol/63) - (D*newvol/63) )
		NewD = 63-(newvol - (D*newvol/63) )
		NewD = 63-(newvol) + (D*newvol/63)
		NewD = 63 + (D*newvol/63) - newvol
		NewD = 63 + (D*newvol/63) - newvol
	*/
	// On Testing, ST3 does not alter the modulator volume.

	// vol is previously converted to the 0..63 range.

	// Set volume of both operators in additive mode
	if(D[10] & CONNECTION_BIT)
		OPL_Byte(csf, KSL_LEVEL + Ope, (D[2] & KSL_MASK) |
			(63 + ( (D[2]&TOTAL_LEVEL_MASK)*vol / 63) - vol)
		);

	OPL_Byte(csf, KSL_LEVEL+   3+Ope, (D[3] & KSL_MASK) |
		(63 + ( (D[3]&TOTAL_LEVEL_MASK)*vol / 63) - vol)
	);

}


void OPL_Pan(song_t *csf, int32_t c, int32_t val)
{
	csf->opl_pans[c] = CLAMP(val, 0, 256);

	int32_t oplc = GetVoice(csf, c);
	if (oplc == -1)
		return;

	const unsigned char *D = csf->opl_dtab[oplc];

	/* feedback, additive synthesis and Panning... */
	OPL_Byte(csf, FEEDBACK_CONNECTION+oplc, 
		(D[10] & ~STEREO_BITS)
		| (csf->opl_pans[c]<85 ? VOICE_TO_LEFT
			: csf->opl_pans[c]>170 ? VOICE_TO_RIGHT
			: (VOICE_TO_LEFT | VOICE_TO_RIGHT))
	);
}


void OPL_Patch(song_t *csf, int32_t c, const unsigned char *D)
{
	int32_t oplc = SetVoice(csf, c);
	if (oplc == -1)
		return;

	csf->opl_dtab[oplc] = D;
	int32_t Ope = PortBases[oplc];

	OPL_Byte(csf, AM_VIB+           Ope, D[0]);
	OPL_Byte(csf, KSL_LEVEL+        Ope, D[2]);
	OPL_Byte(csf, ATTACK_DECAY+     Ope, D[4]);
	OPL_Byte(csf, SUSTAIN_RELEASE+  Ope, D[6]);
	OPL_Byte(csf, WAVE_SELECT+      Ope, D[8]&7);// 5 high bits used elsewhere

	OPL_Byte(csf, AM_VIB+         3+Ope, D[1]);
	OPL_Byte(csf, KSL_LEVEL+      3+Ope, D[3]);
	OPL_Byte(csf, ATTACK_DECAY+   3+Ope, D[5]);
	OPL_Byte(csf, SUSTAIN_RELEASE+3+Ope, D[7]);
	OPL_Byte(csf, WAVE_SELECT+    3+Ope, D[9]&7);// 5 high bits used elsewhere

	/* feedback, additive synthesis and Panning... */
	OPL_Byte(csf, FEEDBACK_CONNECTION+oplc, 
		(D[10] & ~STEREO_BITS)
		| (csf->opl_pans[c]<85 ? VOICE_TO_LEFT
			: csf->opl_pans[c]>170 ? VOICE_TO_RIGHT
			: (VOICE_TO_LEFT | VOICE_TO_RIGHT))
	);
}


void OPL_Reset(song_t *csf)
{
	int32_t a;
	if (csf->opl == NULL)
		return;

	OPLResetChip(csf->opl);
	OPL_Detect(csf);

	for(a = 0; a < MAX_VOICES; ++a) {
		csf->opl_from_chan[a]=-1;
	}
	for(a = 0; a < 9; ++a) {
		csf->opl_to_chan[a]= -1;
		csf->opl_dtab[a] = NULL;
	}

	OPL_Byte(csf, TEST_REGISTER, ENABLE_WAVE_SELECT);
#if OPLSOURCE == 3
	//Enable OPL3.
	OPL_Byte_RightSide(csf, OPL3_MODE_REGISTER, OPL3_ENABLE);
#endif

	csf->opl_fm_active = 0;
}


int32_t OPL_Detect(song_t *csf)
{
	/* Reset timers 1 and 2 */
	OPL_Byte(csf, TIMER_CONTROL_REGISTER, TIMER1_MASK | TIMER2_MASK);

	/* Reset the IRQ of the FM chip */
	OPL_Byte(csf, TIMER_CONTROL_REGISTER, IRQ_RESET);

	unsigned char ST1 = Fmdrv_Inportb(csf, OPL_BASE); /* Status register */

	OPL_Byte(csf, TIMER1_REGISTER, 255);
	OPL_Byte(csf, TIMER_CONTROL_REGISTER, TIMER2_MASK | TIMER1_START);

	/*_asm xor cx,cx;P1:_asm loop P1*/
	unsigned char ST2 = Fmdrv_Inportb(csf, OPL_BASE);

	OPL_Byte(csf, TIMER_CONTROL_REGISTER, TIMER1_MASK | TIMER2_MASK);
	OPL_Byte(csf, TIMER_CONTROL_REGISTER, IRQ_RESET);

	int32_t OPLMode = (ST2 & 0xE0) == 0xC0 && !(ST1 & 0xE0);

	if (!OPLMode)
		return -1;

	return 0;
}

void OPL_Close(song_t *csf)
{
	if (csf->opl != NULL) {
		OPLCloseChip(csf->opl);
		csf->opl = NULL;
	}
}
