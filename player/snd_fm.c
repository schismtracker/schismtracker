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
#include "log.h"
#include "util.h" /* for clamp */

#define MAX_VOICES 256 /* Must not be less than the setting in sndfile.h */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define OPLRATEBASE 49716 // It's not a good idea to deviate from this.

#if OPLSOURCE == 2
# define OPLNew(x, r) ym3812_init(x, r)
# define OPLResetChip ym3812_reset_chip
# define OPLWrite     ym3812_write
# define OPLReadChip  ym3812_read
# define OPLUpdateOne ym3812_update_one
# define OPLCloseChip ym3812_shutdown
// OPL2 = 3579552Hz
# define OPLRATEDIVISOR 72
#elif OPLSOURCE == 3
# define OPLNew(x, r)   ymf262_init(x, r)
# define OPLResetChip   ymf262_reset_chip
# define OPLWrite       ymf262_write
# define OPLReadChip    ymf262_read
# define OPLUpdateOne   ymf262_update_one
# define OPLCloseChip   ymf262_shutdown
// OPL3 = 14318208Hz
# define OPLRATEDIVISOR 288
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

static const int oplbase = 0x388;

// OPL info
static struct OPL *opl = NULL;
static uint32_t oplretval = 0, oplregno = 0;
static uint32_t fm_active = 0;

extern int fnumToMilliHertz(unsigned int fnum, unsigned int block, unsigned int conversionFactor);

extern void
milliHertzToFnum(unsigned int milliHertz, unsigned int *fnum, unsigned int *block, unsigned int conversionFactor);


static void Fmdrv_Outportb(unsigned port, unsigned value)
{
	if (opl == NULL || ((int)port) < oplbase || ((int)port) >= oplbase + 4) return;

	unsigned ind = port - oplbase;
	OPLWrite(opl, ind, value);

	if (ind & 1) {
		if (oplregno == 4) {
			if (value == 0x80) oplretval = 0x02;
			else if (value == 0x21) oplretval = 0xC0;
		}
	} else oplregno = value;
}


static unsigned char Fmdrv_Inportb(unsigned port)
{
	return (((int)port) >= oplbase && ((int)port) < oplbase + 4) ? oplretval : 0;
}


void Fmdrv_Init(int mixfreq)
{
	if (opl != NULL) {
		OPLCloseChip(opl);
		opl = NULL;
	}
	// Clock = speed at which the chip works. mixfreq = audio resampler
	opl = OPLNew(OPLRATEBASE * OPLRATEDIVISOR, mixfreq);
	OPL_Reset();
}


void Fmdrv_MixTo(int *target, int count)
{
	static short *buf = NULL;
	static int buf_size = 0;

	if (!fm_active) return;

#if OPLSOURCE == 2
	// mono. Single buffer.
	if (buf_size != count * sizeof(short)) {
		int before = buf_size;
		buf_size = sizeof(short) * count;

		if (before) {
			buf = (short *)mem_realloc(buf, buf_size);
		} else {
			buf = (short *)mem_alloc(buf_size);
		}
	}

	memset(buf, 0, buf_size);
	OPLUpdateOne(opl, buf, count);
	/*
	static int counter = 0;

	for(int a = 0; a < count; ++a)
		buf[a] = ((counter++) & 0x100) ? -10000 : 10000;
	*/

	for (int a = 0; a < count; ++a) {
		target[a * 2 + 0] += buf[a] * OPL_VOLUME;
		target[a * 2 + 1] += buf[a] * OPL_VOLUME;
	}
#else
	//stereo. Four buffers (two unused, so allocating 3 is enough)
	if (buf_size != sizeof(short) * count * 3) {
		int before = buf_size;
		buf_size = sizeof(short) * count * 3;

		if (before) {
			buf = (short *)mem_realloc(buf, buf_size);
		} else {
			buf = (short *)mem_alloc(buf_size);
		}
	}

	memset(buf, 0, buf_size);
	short *bufarray[4] = {buf, buf + count, buf + (count * 2), buf + (count * 2)};
	OPLUpdateOne(opl, bufarray, count);
	/*
	static int counter = 0;

	for(int a = 0; a < count; ++a)
		buf[a] = ((counter++) & 0x100) ? -10000 : 10000;
	*/
	short *bufleft = buf;
	short *bufright = buf + count;
	// IF we wanted to do the stereo mix in software, we could setup the voices always in mono
	// and do the panning here.
	for (int a = 0; a < count; ++a) {
		target[a * 2 + 0] += bufleft[a] * OPL_VOLUME;
		target[a * 2 + 1] += bufright[a] * OPL_VOLUME;
	}
#endif
}


/***************************************/


static const char PortBases[9] = {0, 1, 2, 8, 9, 10, 16, 17, 18};

static const unsigned char *Dtab[9];
static unsigned char Keyontab[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
static int Pans[MAX_VOICES];

static int OPLtoChan[9];
static int ChantoOPL[MAX_VOICES];

static int GetVoice(int c)
{
	return ChantoOPL[c];
}
static int SetVoice(int c)
{
	int a, s = -1, t = 0;
	if (ChantoOPL[c] == -1) {
		t = 1;
		// Search for unused chans
		for (a = 0; a < 9; a++) {
			if (OPLtoChan[a] == -1) {
				s = a;
				OPLtoChan[a] = c;
				ChantoOPL[c] = a;
				break;
			}
		}
		if (ChantoOPL[c] == -1) {
			// Search for note-released chans
			for (a = 0; a < 9; a++) {
				if ((Keyontab[a] & KEYON_BIT) == 0) {
					s = a + 10;
					ChantoOPL[OPLtoChan[a]] = -1;
					OPLtoChan[a] = c;
					ChantoOPL[c] = a;
					break;
				}
			}
		}
	}
	//log_appendf(2,"entering with %d. tested? %d. selected %d. Current: %d",c,t,s,ChantoOPL[c]);
	return GetVoice(c);
}


static void FreeVoice(int c)
{
	if (ChantoOPL[c] == -1) return;
	OPLtoChan[ChantoOPL[c]] = -1;
	ChantoOPL[c] = -1;
}

static void OPL_Byte(unsigned int idx, unsigned char data)
{
	//register int a;
	Fmdrv_Outportb(oplbase, idx);      // for(a = 0; a < 6;  a++) Fmdrv_Inportb(oplbase);
	Fmdrv_Outportb(oplbase + 1, data); // for(a = 0; a < 35; a++) Fmdrv_Inportb(oplbase);
}
static void OPL_Byte_RightSide(unsigned int idx, unsigned char data)
{
	//register int a;
	Fmdrv_Outportb(oplbase + 2, idx);  // for(a = 0; a < 6;  a++) Fmdrv_Inportb(oplbase);
	Fmdrv_Outportb(oplbase + 3, data); // for(a = 0; a < 35; a++) Fmdrv_Inportb(oplbase);
}


void OPL_NoteOff(int c)
{
	int oplc = GetVoice(c);
	if (oplc == -1) return;
	Keyontab[oplc] &= ~KEYON_BIT;
	OPL_Byte(KEYON_BLOCK + oplc, Keyontab[oplc]);
}


/* OPL_NoteOn changes the frequency on specified
   channel and guarantees the key is on. (Doesn't
   retrig, just turns the note on and sets freq.)
   If keyoff is nonzero, doesn't even set the note on.
   Could be used for pitch bending also. */
void OPL_HertzTouch(int c, int milliHertz, int keyoff)
{
	int oplc = GetVoice(c);
	if (oplc == -1) return;

	fm_active = 1;

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
	unsigned int outfnum;
	unsigned int outblock;
	const int conversion_factor = OPLRATEBASE; // Frequency of OPL.
	milliHertzToFnum(milliHertz, &outfnum, &outblock, conversion_factor);
	Keyontab[oplc] = (keyoff ? 0 : KEYON_BIT)             // Key on
	                 | (outblock << 2)                    // Octave
	                 | ((outfnum >> 8) & FNUM_HIGH_MASK); // F-number high 2 bits
	OPL_Byte(FNUM_LOW + oplc, outfnum & 0xFF);            // F-Number low 8 bits
	OPL_Byte(KEYON_BLOCK + oplc, Keyontab[oplc]);
}


void OPL_Touch(int c, unsigned vol)
{
	//fprintf(stderr, "OPL_Touch(%d, %p:%02X.%02X.%02X.%02X-%02X.%02X.%02X.%02X-%02X.%02X.%02X, %d)\n",
	//    c, D,D[0],D[1],D[2],D[3],D[4],D[5],D[6],D[7],D[8],D[9],D[10], Vol);

	int oplc = GetVoice(c);
	if (oplc == -1) return;

	const unsigned char *D = Dtab[oplc];
	int Ope = PortBases[oplc];

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
	if (D[10] & CONNECTION_BIT)
		OPL_Byte(KSL_LEVEL + Ope, (D[2] & KSL_MASK) | (63 + ((D[2] & TOTAL_LEVEL_MASK) * vol / 63) - vol));

	OPL_Byte(KSL_LEVEL + 3 + Ope, (D[3] & KSL_MASK) | (63 + ((D[3] & TOTAL_LEVEL_MASK) * vol / 63) - vol));
}


void OPL_Pan(int c, int val)
{
	Pans[c] = CLAMP(val, 0, 256);

	int oplc = GetVoice(c);
	if (oplc == -1) return;

	const unsigned char *D = Dtab[oplc];

	/* feedback, additive synthesis and Panning... */
	OPL_Byte(
		FEEDBACK_CONNECTION + oplc, (D[10] & ~STEREO_BITS)
										| (Pans[c] < 85  ? VOICE_TO_LEFT :
	                                       Pans[c] > 170 ? VOICE_TO_RIGHT :
	                                                       (VOICE_TO_LEFT | VOICE_TO_RIGHT)));
}


void OPL_Patch(int c, const unsigned char *D)
{
	int oplc = SetVoice(c);
	if (oplc == -1) return;

	Dtab[oplc] = D;
	int Ope = PortBases[oplc];

	OPL_Byte(AM_VIB + Ope, D[0]);
	OPL_Byte(KSL_LEVEL + Ope, D[2]);
	OPL_Byte(ATTACK_DECAY + Ope, D[4]);
	OPL_Byte(SUSTAIN_RELEASE + Ope, D[6]);
	OPL_Byte(WAVE_SELECT + Ope, D[8] & 7); // 5 high bits used elsewhere

	OPL_Byte(AM_VIB + 3 + Ope, D[1]);
	OPL_Byte(KSL_LEVEL + 3 + Ope, D[3]);
	OPL_Byte(ATTACK_DECAY + 3 + Ope, D[5]);
	OPL_Byte(SUSTAIN_RELEASE + 3 + Ope, D[7]);
	OPL_Byte(WAVE_SELECT + 3 + Ope, D[9] & 7); // 5 high bits used elsewhere

	/* feedback, additive synthesis and Panning... */
	OPL_Byte(
		FEEDBACK_CONNECTION + oplc, (D[10] & ~STEREO_BITS)
										| (Pans[c] < 85  ? VOICE_TO_LEFT :
	                                       Pans[c] > 170 ? VOICE_TO_RIGHT :
	                                                       (VOICE_TO_LEFT | VOICE_TO_RIGHT)));
}


void OPL_Reset(void)
{
	int a;
	if (opl == NULL) return;

	OPLResetChip(opl);
	OPL_Detect();

	for (a = 0; a < MAX_VOICES; ++a) {
		ChantoOPL[a] = -1;
	}
	for (a = 0; a < 9; ++a) {
		OPLtoChan[a] = -1;
		Dtab[a] = NULL;
	}

	OPL_Byte(TEST_REGISTER, ENABLE_WAVE_SELECT);
#if OPLSOURCE == 3
	//Enable OPL3.
	OPL_Byte_RightSide(OPL3_MODE_REGISTER, OPL3_ENABLE);
#endif

	fm_active = 0;
}


int OPL_Detect(void)
{
	/* Reset timers 1 and 2 */
	OPL_Byte(TIMER_CONTROL_REGISTER, TIMER1_MASK | TIMER2_MASK);

	/* Reset the IRQ of the FM chip */
	OPL_Byte(TIMER_CONTROL_REGISTER, IRQ_RESET);

	unsigned char ST1 = Fmdrv_Inportb(oplbase); /* Status register */

	OPL_Byte(TIMER1_REGISTER, 255);
	OPL_Byte(TIMER_CONTROL_REGISTER, TIMER2_MASK | TIMER1_START);

	/*_asm xor cx,cx;P1:_asm loop P1*/
	unsigned char ST2 = Fmdrv_Inportb(oplbase);

	OPL_Byte(TIMER_CONTROL_REGISTER, TIMER1_MASK | TIMER2_MASK);
	OPL_Byte(TIMER_CONTROL_REGISTER, IRQ_RESET);

	int OPLMode = (ST2 & 0xE0) == 0xC0 && !(ST1 & 0xE0);

	if (!OPLMode) return -1;

	return 0;
}

/* TODO: This should be called from somewhere in schismtracker to free the allocated memory on exit. */
void OPL_Close(void)
{
	if (opl != NULL) {
		OPLCloseChip(opl);
		opl = NULL;
	}
}
