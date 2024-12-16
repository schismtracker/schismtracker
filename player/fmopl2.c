// license:GPL-2.0+
// copyright-holders:Jarek Burczynski,Tatsuyuki Satoh
/*
**
** File: fmopl.c - software implementation of FM sound generator
**                                            types OPL and OPL2
**
** Copyright Jarek Burczynski (bujar at mame dot net)
** Copyright Tatsuyuki Satoh , MultiArcadeMachineEmulator development
**
** Version 0.72
**

Revision History:

04-08-2003 Jarek Burczynski:
 - removed BFRDY hack. BFRDY is busy flag, and it should be 0 only when the chip
   handles memory read/write or during the adpcm synthesis when the chip
   requests another byte of ADPCM data.

24-07-2003 Jarek Burczynski:
 - added a small hack for Y8950 status BFRDY flag (bit 3 should be set after
   some (unknown) delay). Right now it's always set.

14-06-2003 Jarek Burczynski:
 - implemented all of the status register flags in Y8950 emulation
 - renamed y8950_set_delta_t_memory() parameters from _rom_ to _mem_ since
   they can be either RAM or ROM

08-10-2002 Jarek Burczynski (thanks to Dox for the YM3526 chip)
 - corrected ym3526_read() to always set bit 2 and bit 1
   to HIGH state - identical to ym3812_read (verified on real YM3526)

04-28-2002 Jarek Burczynski:
 - binary exact Envelope Generator (verified on real YM3812);
   compared to YM2151: the EG clock is equal to internal_clock,
   rates are 2 times slower and volume resolution is one bit less
 - modified interface functions (they no longer return pointer -
   that's internal to the emulator now):
    - new wrapper functions for OPLCreate: ym3526_init(), ym3812_init() and y8950_init()
 - corrected 'off by one' error in feedback calculations (when feedback is off)
 - enabled waveform usage (credit goes to Vlad Romascanu and zazzal22)
 - speeded up noise generator calculations (Nicola Salmoria)

03-24-2002 Jarek Burczynski (thanks to Dox for the YM3812 chip)
 Complete rewrite (all verified on real YM3812):
 - corrected sin_tab and tl_tab data
 - corrected operator output calculations
 - corrected waveform_select_enable register;
   simply: ignore all writes to waveform_select register when
   waveform_select_enable == 0 and do not change the waveform previously selected.
 - corrected KSR handling
 - corrected Envelope Generator: attack shape, Sustain mode and
   Percussive/Non-percussive modes handling
 - Envelope Generator rates are two times slower now
 - LFO amplitude (tremolo) and phase modulation (vibrato)
 - rhythm sounds phase generation
 - white noise generator (big thanks to Olivier Galibert for mentioning Berlekamp-Massey algorithm)
 - corrected key on/off handling (the 'key' signal is ORed from three sources: FM, rhythm and CSM)
 - funky details (like ignoring output of operator 1 in BD rhythm sound when connect == 1)

12-28-2001 Acho A. Tang
 - reflected Delta-T EOS status on Y8950 status port.
 - fixed subscription range of attack/decay tables


    To do:
        add delay before key off in CSM mode (see CSMKeyControll)
        verify volume of the FM part on the Y8950
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "player/fmopl.h"


#include "log.h"
/* output final shift */
#define FINAL_SH    (0)
#define MAXOUT      INT16_MAX
#define MINOUT      INT16_MIN

#define FREQ_SH         16  /* 16.16 fixed point (frequency calculations) */
#define EG_SH           16  /* 16.16 fixed point (EG timing)              */
#define LFO_SH          24  /*  8.24 fixed point (LFO calculations)       */
#define TIMER_SH        16  /* 16.16 fixed point (timers calculations)    */

#define FREQ_MASK       ((1<<FREQ_SH)-1)

/* envelope output entries */
#define ENV_BITS        10
#define ENV_LEN         (1<<ENV_BITS)
#define ENV_STEP        (128.0/ENV_LEN)

#define MAX_ATT_INDEX   ((1<<(ENV_BITS-1))-1) /*511*/
#define MIN_ATT_INDEX   (0)

/* sinwave entries */
#define SIN_BITS        10
#define SIN_LEN         (1<<SIN_BITS)
#define SIN_MASK        (SIN_LEN-1)

#define TL_RES_LEN      (256)   /* 8 bits addressing (real chip) */



/* register number to channel number , slot offset */
#define SLOT1 0
#define SLOT2 1

/* Envelope Generator phases */

#define EG_ATT          4
#define EG_DEC          3
#define EG_SUS          2
#define EG_REL          1
#define EG_OFF          0

#define OPL_TYPE_WAVESEL   0x01  /* waveform select     */
#define OPL_TYPE_ADPCM     0x02  /* DELTA-T ADPCM unit  */
#define OPL_TYPE_KEYBOARD  0x04  /* keyboard interface  */
#define OPL_TYPE_IO        0x08  /* I/O port            */

/* ---------- Generic interface section ---------- */
#define OPL_TYPE_YM3526 (0)
#define OPL_TYPE_YM3812 (OPL_TYPE_WAVESEL)
#define OPL_TYPE_Y8950  (OPL_TYPE_ADPCM|OPL_TYPE_KEYBOARD|OPL_TYPE_IO)



typedef struct
{
	uint32_t  ar;         /* attack rate: AR<<2           */
	uint32_t  dr;         /* decay rate:  DR<<2           */
	uint32_t  rr;         /* release rate:RR<<2           */
	uint8_t   KSR;        /* key scale rate               */
	uint8_t   ksl;        /* keyscale level               */
	uint8_t   ksr;        /* key scale rate: kcode>>KSR   */
	uint8_t   mul;        /* multiple: mul_tab[ML]        */

	/* Phase Generator */
	uint32_t  Cnt;        /* frequency counter            */
	uint32_t  Incr;       /* frequency counter step       */
	uint8_t   FB;         /* feedback shift value         */
	int32_t   *connect1;  /* slot1 output pointer         */
	int32_t   op1_out[2]; /* slot1 output for feedback    */
	uint8_t   CON;        /* connection (algorithm) type  */

	/* Envelope Generator */
	uint8_t   eg_type;    /* percussive/non-percussive mode */
	uint8_t   state;      /* phase type                   */
	uint32_t  TL;         /* total level: TL << 2         */
	int32_t   TLL;        /* adjusted now TL              */
	int32_t   volume;     /* envelope counter             */
	uint32_t  sl;         /* sustain level: sl_tab[SL]    */
	uint8_t   eg_sh_ar;   /* (attack state)               */
	uint8_t   eg_sel_ar;  /* (attack state)               */
	uint8_t   eg_sh_dr;   /* (decay state)                */
	uint8_t   eg_sel_dr;  /* (decay state)                */
	uint8_t   eg_sh_rr;   /* (release state)              */
	uint8_t   eg_sel_rr;  /* (release state)              */
	uint32_t  key;        /* 0 = KEY OFF, >0 = KEY ON     */

	/* LFO */
	uint32_t  AMmask;     /* LFO Amplitude Modulation enable mask */
	uint8_t   vib;        /* LFO Phase Modulation enable flag (active high)*/

	/* waveform select */
	uint16_t  wavetable;
} OPL_SLOT;

typedef struct
{
	OPL_SLOT SLOT[2];
	/* phase generator state */
	uint32_t  block_fnum; /* block+fnum                   */
	uint32_t  fc;         /* Freq. Increment base         */
	uint32_t  ksl_base;   /* KeyScaleLevel Base step      */
	uint8_t   kcode;      /* key code (for key scaling)   */
} OPL_CH;

/* OPL state */
typedef struct
{
	/* FM channel slots */
	OPL_CH  P_CH[9];                /* OPL/OPL2 chips have 9 channels*/

	uint32_t  eg_cnt;                 /* global envelope generator counter    */
	uint32_t  eg_timer;               /* global envelope generator counter works at frequency = chipclock/72 */
	uint32_t  eg_timer_add;           /* step of eg_timer                     */
	uint32_t  eg_timer_overflow;      /* envelope generator timer overlfows every 1 sample (on real chip) */

	uint8_t   rhythm;                 /* Rhythm mode                  */

	uint32_t  fn_tab[1024];           /* fnumber->increment counter   */

	/* LFO */
	uint32_t  LFO_AM;
	int32_t   LFO_PM;

	uint8_t   lfo_am_depth;
	uint8_t   lfo_pm_depth_range;
	uint32_t  lfo_am_cnt;
	uint32_t  lfo_am_inc;
	uint32_t  lfo_pm_cnt;
	uint32_t  lfo_pm_inc;

	uint32_t  noise_rng;              /* 23 bit noise shift register  */
	uint32_t  noise_p;                /* current noise 'phase'        */
	uint32_t  noise_f;                /* current noise period         */

	uint8_t   wavesel;                /* waveform select enable flag  */

	uint32_t  T[2];                   /* timer counters               */
	uint8_t   st[2];                  /* timer enable                 */

#if BUILD_Y8950
	/* Delta-T ADPCM unit (Y8950) */

	YM_DELTAT *deltat;

	/* Keyboard and I/O ports interface */
	uint8_t   portDirection;
	uint8_t   portLatch;
	OPL_PORTHANDLER_R porthandler_r;
	OPL_PORTHANDLER_W porthandler_w;
	void *  port_param;
	OPL_PORTHANDLER_R keyboardhandler_r;
	OPL_PORTHANDLER_W keyboardhandler_w;
	void *  keyboard_param;
#endif

	/* external event callback handlers */
	OPL_TIMERHANDLER  timer_handler;    /* TIMER handler                */
	void *TimerParam;                   /* TIMER parameter              */
	OPL_IRQHANDLER    IRQHandler;   /* IRQ handler                  */
	void *IRQParam;                 /* IRQ parameter                */
	OPL_UPDATEHANDLER UpdateHandler;/* stream update handler        */
	void *UpdateParam;              /* stream update parameter      */

	uint8_t type;                     /* chip type                    */
	uint8_t address;                  /* address register             */
	uint8_t status;                   /* status flag                  */
	uint8_t statusmask;               /* status mask                  */
	uint8_t mode;                     /* Reg.08 : CSM,notesel,etc.    */

	uint32_t clock;                   /* master clock  (Hz)           */
	uint32_t rate;                    /* sampling rate (Hz)           */
	double freqbase;                /* frequency base               */
	double TimerBase;               /* Timer base time (==sampling time)*/
	signed int phase_modulation;    /* phase modulation input (SLOT 2) */
	signed int output[1];
#if BUILD_Y8950
	int32_t output_deltat[4];         /* for Y8950 DELTA-T, chip is mono, that 4 here is just for safety */
#endif
} FM_OPL;



/* mapping of register number (offset) to slot number used by the emulator */
static const int slot_array[32]=
{
	0, 2, 4, 1, 3, 5,-1,-1,
	6, 8,10, 7, 9,11,-1,-1,
	12,14,16,13,15,17,-1,-1,
	-1,-1,-1,-1,-1,-1,-1,-1
};

/* key scale level */
/* table is 3dB/octave , DV converts this into 6dB/octave */
/* 0.1875 is bit 0 weight of the envelope counter (volume) expressed in the 'decibel' scale */
#define KSC(x) ((uint32_t)((x)/(0.1875/2.0)))
static const uint32_t ksl_tab[8*16]=
{
	/* OCT 0 */
	 KSC(0.000), KSC(0.000), KSC(0.000), KSC(0.000),
	 KSC(0.000), KSC(0.000), KSC(0.000), KSC(0.000),
	 KSC(0.000), KSC(0.000), KSC(0.000), KSC(0.000),
	 KSC(0.000), KSC(0.000), KSC(0.000), KSC(0.000),
	/* OCT 1 */
	 KSC(0.000), KSC(0.000), KSC(0.000), KSC(0.000),
	 KSC(0.000), KSC(0.000), KSC(0.000), KSC(0.000),
	 KSC(0.000), KSC(0.750), KSC(1.125), KSC(1.500),
	 KSC(1.875), KSC(2.250), KSC(2.625), KSC(3.000),
	/* OCT 2 */
	 KSC(0.000), KSC(0.000), KSC(0.000), KSC(0.000),
	 KSC(0.000), KSC(1.125), KSC(1.875), KSC(2.625),
	 KSC(3.000), KSC(3.750), KSC(4.125), KSC(4.500),
	 KSC(4.875), KSC(5.250), KSC(5.625), KSC(6.000),
	/* OCT 3 */
	 KSC(0.000), KSC(0.000), KSC(0.000), KSC(1.875),
	 KSC(3.000), KSC(4.125), KSC(4.875), KSC(5.625),
	 KSC(6.000), KSC(6.750), KSC(7.125), KSC(7.500),
	 KSC(7.875), KSC(8.250), KSC(8.625), KSC(9.000),
	/* OCT 4 */
	 KSC(0.000), KSC(0.000), KSC(3.000), KSC(4.875),
	 KSC(6.000), KSC(7.125), KSC(7.875), KSC(8.625),
	 KSC(9.000), KSC(9.750),KSC(10.125),KSC(10.500),
	KSC(10.875),KSC(11.250),KSC(11.625),KSC(12.000),
	/* OCT 5 */
	 KSC(0.000), KSC(3.000), KSC(6.000), KSC(7.875),
	 KSC(9.000),KSC(10.125),KSC(10.875),KSC(11.625),
	KSC(12.000),KSC(12.750),KSC(13.125),KSC(13.500),
	KSC(13.875),KSC(14.250),KSC(14.625),KSC(15.000),
	/* OCT 6 */
	 KSC(0.000), KSC(6.000), KSC(9.000),KSC(10.875),
	KSC(12.000),KSC(13.125),KSC(13.875),KSC(14.625),
	KSC(15.000),KSC(15.750),KSC(16.125),KSC(16.500),
	KSC(16.875),KSC(17.250),KSC(17.625),KSC(18.000),
	/* OCT 7 */
	 KSC(0.000), KSC(9.000),KSC(12.000),KSC(13.875),
	KSC(15.000),KSC(16.125),KSC(16.875),KSC(17.625),
	KSC(18.000),KSC(18.750),KSC(19.125),KSC(19.500),
	KSC(19.875),KSC(20.250),KSC(20.625),KSC(21.000)
};
#undef KSC

/* 0 / 3.0 / 1.5 / 6.0 dB/OCT */
static const uint32_t ksl_shift[4] = { 31, 1, 2, 0 };


/* sustain level table (3dB per step) */
/* 0 - 15: 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,93 (dB)*/
#define SC(db) (uint32_t) ( db * (2.0/ENV_STEP) )
static const uint32_t sl_tab[16]={
    SC( 0),SC( 1),SC( 2),SC(3 ),SC(4 ),SC(5 ),SC(6 ),SC( 7),
    SC( 8),SC( 9),SC(10),SC(11),SC(12),SC(13),SC(14),SC(31)
};
#undef SC


#define RATE_STEPS (8)
static const unsigned char eg_inc[15*RATE_STEPS]={
/*cycle:0 1  2 3  4 5  6 7*/

/* 0 */ 0,1, 0,1, 0,1, 0,1, /* rates 00..12 0 (increment by 0 or 1) */
/* 1 */ 0,1, 0,1, 1,1, 0,1, /* rates 00..12 1 */
/* 2 */ 0,1, 1,1, 0,1, 1,1, /* rates 00..12 2 */
/* 3 */ 0,1, 1,1, 1,1, 1,1, /* rates 00..12 3 */

/* 4 */ 1,1, 1,1, 1,1, 1,1, /* rate 13 0 (increment by 1) */
/* 5 */ 1,1, 1,2, 1,1, 1,2, /* rate 13 1 */
/* 6 */ 1,2, 1,2, 1,2, 1,2, /* rate 13 2 */
/* 7 */ 1,2, 2,2, 1,2, 2,2, /* rate 13 3 */

/* 8 */ 2,2, 2,2, 2,2, 2,2, /* rate 14 0 (increment by 2) */
/* 9 */ 2,2, 2,4, 2,2, 2,4, /* rate 14 1 */
/*10 */ 2,4, 2,4, 2,4, 2,4, /* rate 14 2 */
/*11 */ 2,4, 4,4, 2,4, 4,4, /* rate 14 3 */

/*12 */ 4,4, 4,4, 4,4, 4,4, /* rates 15 0, 15 1, 15 2, 15 3 (increment by 4) */
/*13 */ 8,8, 8,8, 8,8, 8,8, /* rates 15 2, 15 3 for attack */
/*14 */ 0,0, 0,0, 0,0, 0,0, /* infinity rates for attack and decay(s) */
};


#define O(a) (a*RATE_STEPS)

/*note that there is no O(13) in this table - it's directly in the code */
/* Envelope Generator rates (16 + 64 rates + 16 RKS) */
static const unsigned char eg_rate_select[16+64+16]={
/* 16 infinite time rates */
O(14),O(14),O(14),O(14),O(14),O(14),O(14),O(14),
O(14),O(14),O(14),O(14),O(14),O(14),O(14),O(14),

/* rates 00-12 */
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),
O( 0),O( 1),O( 2),O( 3),

/* rate 13 */
O( 4),O( 5),O( 6),O( 7),

/* rate 14 */
O( 8),O( 9),O(10),O(11),

/* rate 15 */
O(12),O(12),O(12),O(12),

/* 16 dummy rates (same as 15 3) */
O(12),O(12),O(12),O(12),O(12),O(12),O(12),O(12),
O(12),O(12),O(12),O(12),O(12),O(12),O(12),O(12),

};
#undef O

/*rate  0,    1,    2,    3,   4,   5,   6,  7,  8,  9,  10, 11, 12, 13, 14, 15 */
/*shift 12,   11,   10,   9,   8,   7,   6,  5,  4,  3,  2,  1,  0,  0,  0,  0  */
/*mask  4095, 2047, 1023, 511, 255, 127, 63, 31, 15, 7,  3,  1,  0,  0,  0,  0  */

#define O(a) (a*1)
/* Envelope Generator counter shifts (16 + 64 rates + 16 RKS) */
static const unsigned char eg_rate_shift[16+64+16]={
/* 16 infinite time rates */
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),
O(0),O(0),O(0),O(0),O(0),O(0),O(0),O(0),

/* rates 00-12 */
O(12),O(12),O(12),O(12),
O(11),O(11),O(11),O(11),
O(10),O(10),O(10),O(10),
O( 9),O( 9),O( 9),O( 9),
O( 8),O( 8),O( 8),O( 8),
O( 7),O( 7),O( 7),O( 7),
O( 6),O( 6),O( 6),O( 6),
O( 5),O( 5),O( 5),O( 5),
O( 4),O( 4),O( 4),O( 4),
O( 3),O( 3),O( 3),O( 3),
O( 2),O( 2),O( 2),O( 2),
O( 1),O( 1),O( 1),O( 1),
O( 0),O( 0),O( 0),O( 0),

/* rate 13 */
O( 0),O( 0),O( 0),O( 0),

/* rate 14 */
O( 0),O( 0),O( 0),O( 0),

/* rate 15 */
O( 0),O( 0),O( 0),O( 0),

/* 16 dummy rates (same as 15 3) */
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),
O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),O( 0),

};
#undef O


/* multiple table */
#define ML 2
static const uint8_t mul_tab[16]= {
/* 1/2, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,10,12,12,15,15 */
	ML/2, 1*ML, 2*ML, 3*ML, 4*ML, 5*ML, 6*ML, 7*ML,
	8*ML, 9*ML,10*ML,10*ML,12*ML,12*ML,15*ML,15*ML
};
#undef ML

/*  TL_TAB_LEN is calculated as:
*   12 - sinus amplitude bits     (Y axis)
*   2  - sinus sign bit           (Y axis)
*   TL_RES_LEN - sinus resolution (X axis)
*/
#define TL_TAB_LEN (12*2*TL_RES_LEN)
static signed int tl_tab[TL_TAB_LEN];

#define ENV_QUIET       (TL_TAB_LEN>>4)

/* sin waveform table in 'decibel' scale */
/* four waveforms on OPL2 type chips */
static unsigned int sin_tab[SIN_LEN * 4];


/* LFO Amplitude Modulation table (verified on real YM3812)
   27 output levels (triangle waveform); 1 level takes one of: 192, 256 or 448 samples

   Length: 210 elements.

    Each of the elements has to be repeated
    exactly 64 times (on 64 consecutive samples).
    The whole table takes: 64 * 210 = 13440 samples.

    When AM = 1 data is used directly
    When AM = 0 data is divided by 4 before being used (losing precision is important)
*/

#define LFO_AM_TAB_ELEMENTS 210

static const uint8_t lfo_am_table[LFO_AM_TAB_ELEMENTS] = {
0,0,0,0,0,0,0,
1,1,1,1,
2,2,2,2,
3,3,3,3,
4,4,4,4,
5,5,5,5,
6,6,6,6,
7,7,7,7,
8,8,8,8,
9,9,9,9,
10,10,10,10,
11,11,11,11,
12,12,12,12,
13,13,13,13,
14,14,14,14,
15,15,15,15,
16,16,16,16,
17,17,17,17,
18,18,18,18,
19,19,19,19,
20,20,20,20,
21,21,21,21,
22,22,22,22,
23,23,23,23,
24,24,24,24,
25,25,25,25,
26,26,26,
25,25,25,25,
24,24,24,24,
23,23,23,23,
22,22,22,22,
21,21,21,21,
20,20,20,20,
19,19,19,19,
18,18,18,18,
17,17,17,17,
16,16,16,16,
15,15,15,15,
14,14,14,14,
13,13,13,13,
12,12,12,12,
11,11,11,11,
10,10,10,10,
9,9,9,9,
8,8,8,8,
7,7,7,7,
6,6,6,6,
5,5,5,5,
4,4,4,4,
3,3,3,3,
2,2,2,2,
1,1,1,1
};

/* LFO Phase Modulation table (verified on real YM3812) */
static const int8_t lfo_pm_table[8*8*2] = {
/* FNUM2/FNUM = 00 0xxxxxxx (0x0000) */
0, 0, 0, 0, 0, 0, 0, 0, /*LFO PM depth = 0*/
0, 0, 0, 0, 0, 0, 0, 0, /*LFO PM depth = 1*/

/* FNUM2/FNUM = 00 1xxxxxxx (0x0080) */
0, 0, 0, 0, 0, 0, 0, 0, /*LFO PM depth = 0*/
1, 0, 0, 0,-1, 0, 0, 0, /*LFO PM depth = 1*/

/* FNUM2/FNUM = 01 0xxxxxxx (0x0100) */
1, 0, 0, 0,-1, 0, 0, 0, /*LFO PM depth = 0*/
2, 1, 0,-1,-2,-1, 0, 1, /*LFO PM depth = 1*/

/* FNUM2/FNUM = 01 1xxxxxxx (0x0180) */
1, 0, 0, 0,-1, 0, 0, 0, /*LFO PM depth = 0*/
3, 1, 0,-1,-3,-1, 0, 1, /*LFO PM depth = 1*/

/* FNUM2/FNUM = 10 0xxxxxxx (0x0200) */
2, 1, 0,-1,-2,-1, 0, 1, /*LFO PM depth = 0*/
4, 2, 0,-2,-4,-2, 0, 2, /*LFO PM depth = 1*/

/* FNUM2/FNUM = 10 1xxxxxxx (0x0280) */
2, 1, 0,-1,-2,-1, 0, 1, /*LFO PM depth = 0*/
5, 2, 0,-2,-5,-2, 0, 2, /*LFO PM depth = 1*/

/* FNUM2/FNUM = 11 0xxxxxxx (0x0300) */
3, 1, 0,-1,-3,-1, 0, 1, /*LFO PM depth = 0*/
6, 3, 0,-3,-6,-3, 0, 3, /*LFO PM depth = 1*/

/* FNUM2/FNUM = 11 1xxxxxxx (0x0380) */
3, 1, 0,-1,-3,-1, 0, 1, /*LFO PM depth = 0*/
7, 3, 0,-3,-7,-3, 0, 3  /*LFO PM depth = 1*/
};


/* lock level of common table */
static int num_lock = 0;


#define SLOT7_1 (&OPL->P_CH[7].SLOT[SLOT1])
#define SLOT7_2 (&OPL->P_CH[7].SLOT[SLOT2])
#define SLOT8_1 (&OPL->P_CH[8].SLOT[SLOT1])
#define SLOT8_2 (&OPL->P_CH[8].SLOT[SLOT2])




static inline int limit( int val, int max, int min ) {
	if ( val > max )
		val = max;
	else if ( val < min )
		val = min;

	return val;
}


/* status set and IRQ handling */
static inline void OPL_STATUS_SET(FM_OPL *OPL,int flag)
{
	/* set status flag */
	OPL->status |= flag;
	if(!(OPL->status & 0x80))
	{
		if(OPL->status & OPL->statusmask)
		{   /* IRQ on */
			OPL->status |= 0x80;
			/* callback user interrupt handler (IRQ is OFF to ON) */
			if(OPL->IRQHandler) (OPL->IRQHandler)(OPL->IRQParam,1);
		}
	}
}

/* status reset and IRQ handling */
static inline void OPL_STATUS_RESET(FM_OPL *OPL,int flag)
{
	/* reset status flag */
	OPL->status &=~flag;
	if((OPL->status & 0x80))
	{
		if (!(OPL->status & OPL->statusmask) )
		{
			OPL->status &= 0x7f;
			/* callback user interrupt handler (IRQ is ON to OFF) */
			if(OPL->IRQHandler) (OPL->IRQHandler)(OPL->IRQParam,0);
		}
	}
}

/* IRQ mask set */
static inline void OPL_STATUSMASK_SET(FM_OPL *OPL,int flag)
{
	OPL->statusmask = flag;
	/* IRQ handling check */
	OPL_STATUS_SET(OPL,0);
	OPL_STATUS_RESET(OPL,0);
}


/* advance LFO to next sample */
static inline void advance_lfo(FM_OPL *OPL)
{
	uint8_t tmp;

	/* LFO */
	OPL->lfo_am_cnt += OPL->lfo_am_inc;
	if (OPL->lfo_am_cnt >= ((uint32_t)LFO_AM_TAB_ELEMENTS<<LFO_SH) )  /* lfo_am_table is 210 elements long */
		OPL->lfo_am_cnt -= ((uint32_t)LFO_AM_TAB_ELEMENTS<<LFO_SH);

	tmp = lfo_am_table[ OPL->lfo_am_cnt >> LFO_SH ];

	if (OPL->lfo_am_depth)
		OPL->LFO_AM = tmp;
	else
		OPL->LFO_AM = tmp>>2;

	OPL->lfo_pm_cnt += OPL->lfo_pm_inc;
	OPL->LFO_PM = ((OPL->lfo_pm_cnt>>LFO_SH) & 7) | OPL->lfo_pm_depth_range;
}

/* advance to next sample */
static inline void advance(FM_OPL *OPL)
{
	OPL_CH *CH;
	OPL_SLOT *op;
	int i;

	OPL->eg_timer += OPL->eg_timer_add;

	while (OPL->eg_timer >= OPL->eg_timer_overflow)
	{
		OPL->eg_timer -= OPL->eg_timer_overflow;

		OPL->eg_cnt++;

		for (i=0; i<9*2; i++)
		{
			CH  = &OPL->P_CH[i/2];
			op  = &CH->SLOT[i&1];

			/* Envelope Generator */
			switch(op->state)
			{
			case EG_ATT:        /* attack phase */
				if ( !(OPL->eg_cnt & ((1<<op->eg_sh_ar)-1) ) )
				{
					op->volume += (~op->volume *
						(eg_inc[op->eg_sel_ar + ((OPL->eg_cnt>>op->eg_sh_ar)&7)])
					) >>3;

					if (op->volume <= MIN_ATT_INDEX)
					{
						op->volume = MIN_ATT_INDEX;
						op->state = EG_DEC;
					}

				}
			break;

			case EG_DEC:    /* decay phase */
				if ( !(OPL->eg_cnt & ((1<<op->eg_sh_dr)-1) ) )
				{
					op->volume += eg_inc[op->eg_sel_dr + ((OPL->eg_cnt>>op->eg_sh_dr)&7)];

					if ( (uint32_t)op->volume >= op->sl )
						op->state = EG_SUS;

				}
			break;

			case EG_SUS:    /* sustain phase */

				/* this is important behaviour:
				one can change percusive/non-percussive modes on the fly and
				the chip will remain in sustain phase - verified on real YM3812 */

				if(op->eg_type)     /* non-percussive mode */
				{
									/* do nothing */
				}
				else                /* percussive mode */
				{
					/* during sustain phase chip adds Release Rate (in percussive mode) */
					if ( !(OPL->eg_cnt & ((1<<op->eg_sh_rr)-1) ) )
					{
						op->volume += eg_inc[op->eg_sel_rr + ((OPL->eg_cnt>>op->eg_sh_rr)&7)];

						if ( op->volume >= MAX_ATT_INDEX )
							op->volume = MAX_ATT_INDEX;
					}
					/* else do nothing in sustain phase */
				}
			break;

			case EG_REL:    /* release phase */
				if ( !(OPL->eg_cnt & ((1<<op->eg_sh_rr)-1) ) )
				{
					op->volume += eg_inc[op->eg_sel_rr + ((OPL->eg_cnt>>op->eg_sh_rr)&7)];

					if ( op->volume >= MAX_ATT_INDEX )
					{
						op->volume = MAX_ATT_INDEX;
						op->state = EG_OFF;
					}

				}
			break;

			default:
			break;
			}
		}
	}

	for (i=0; i<9*2; i++)
	{
		CH  = &OPL->P_CH[i/2];
		op  = &CH->SLOT[i&1];

		/* Phase Generator */
		if(op->vib)
		{
			uint8_t block;
			uint32_t block_fnum = CH->block_fnum;

			unsigned int fnum_lfo   = (block_fnum&0x0380) >> 7;

			signed int lfo_fn_table_index_offset = lfo_pm_table[OPL->LFO_PM + 16*fnum_lfo ];

			if (lfo_fn_table_index_offset)  /* LFO phase modulation active */
			{
				block_fnum += lfo_fn_table_index_offset;
				block = (block_fnum&0x1c00) >> 10;
				op->Cnt += (OPL->fn_tab[block_fnum&0x03ff] >> (7-block)) * op->mul;
			}
			else    /* LFO phase modulation  = zero */
			{
				op->Cnt += op->Incr;
			}
		}
		else    /* LFO phase modulation disabled for this operator */
		{
			op->Cnt += op->Incr;
		}
	}

	/*  The Noise Generator of the YM3812 is 23-bit shift register.
	*   Period is equal to 2^23-2 samples.
	*   Register works at sampling frequency of the chip, so output
	*   can change on every sample.
	*
	*   Output of the register and input to the bit 22 is:
	*   bit0 XOR bit14 XOR bit15 XOR bit22
	*
	*   Simply use bit 22 as the noise output.
	*/

	OPL->noise_p += OPL->noise_f;
	i = OPL->noise_p >> FREQ_SH;        /* number of events (shifts of the shift register) */
	OPL->noise_p &= FREQ_MASK;
	while (i)
	{
		/*
		uint32_t j;
		j = ( (OPL->noise_rng) ^ (OPL->noise_rng>>14) ^ (OPL->noise_rng>>15) ^ (OPL->noise_rng>>22) ) & 1;
		OPL->noise_rng = (j<<22) | (OPL->noise_rng>>1);
		*/

		/*
		    Instead of doing all the logic operations above, we
		    use a trick here (and use bit 0 as the noise output).
		    The difference is only that the noise bit changes one
		    step ahead. This doesn't matter since we don't know
		    what is real state of the noise_rng after the reset.
		*/

		if (OPL->noise_rng & 1) OPL->noise_rng ^= 0x800302;
		OPL->noise_rng >>= 1;

		i--;
	}
}


static inline signed int op_calc(uint32_t phase, unsigned int env, signed int pm, unsigned int wave_tab)
{
	uint32_t p;

	p = (env<<4) + sin_tab[wave_tab + ((((signed int)((phase & ~FREQ_MASK) + (pm << 16)))
					    >> FREQ_SH) & SIN_MASK)];

	if (p >= TL_TAB_LEN)
		return 0;
	return tl_tab[p];
}

static inline signed int op_calc1(uint32_t phase, unsigned int env, signed int pm, unsigned int wave_tab)
{
	uint32_t p;

	p = (env<<4) + sin_tab[wave_tab + ((((signed int)((phase & ~FREQ_MASK) + pm))
					    >> FREQ_SH) & SIN_MASK)];

	if (p >= TL_TAB_LEN)
		return 0;
	return tl_tab[p];
}


#define volume_calc(OP) ((OP)->TLL + ((uint32_t)(OP)->volume) + (OPL->LFO_AM & (OP)->AMmask))

/* calculate output */
static inline void OPL_CALC_CH( FM_OPL *OPL, OPL_CH *CH )
{
	OPL_SLOT *SLOT;
	unsigned int env;
	signed int out;

	OPL->phase_modulation = 0;

	/* SLOT 1 */
	SLOT = &CH->SLOT[SLOT1];
	env  = volume_calc(SLOT);
	out  = SLOT->op1_out[0] + SLOT->op1_out[1];
	SLOT->op1_out[0] = SLOT->op1_out[1];
	*SLOT->connect1 += SLOT->op1_out[0];
	SLOT->op1_out[1] = 0;
	if( env < ENV_QUIET )
	{
		if (!SLOT->FB)
			out = 0;
		SLOT->op1_out[1] = op_calc1(SLOT->Cnt, env, (out<<SLOT->FB), SLOT->wavetable );
	}

	/* SLOT 2 */
	SLOT++;
	env = volume_calc(SLOT);
	if( env < ENV_QUIET )
		OPL->output[0] += op_calc(SLOT->Cnt, env, OPL->phase_modulation, SLOT->wavetable);
}

/*
    operators used in the rhythm sounds generation process:

    Envelope Generator:

channel  operator  register number   Bass  High  Snare Tom  Top
/ slot   number    TL ARDR SLRR Wave Drum  Hat   Drum  Tom  Cymbal
 6 / 0   12        50  70   90   f0  +
 6 / 1   15        53  73   93   f3  +
 7 / 0   13        51  71   91   f1        +
 7 / 1   16        54  74   94   f4              +
 8 / 0   14        52  72   92   f2                    +
 8 / 1   17        55  75   95   f5                          +

    Phase Generator:

channel  operator  register number   Bass  High  Snare Tom  Top
/ slot   number    MULTIPLE          Drum  Hat   Drum  Tom  Cymbal
 6 / 0   12        30                +
 6 / 1   15        33                +
 7 / 0   13        31                      +     +           +
 7 / 1   16        34                -----  n o t  u s e d -----
 8 / 0   14        32                                  +
 8 / 1   17        35                      +                 +

channel  operator  register number   Bass  High  Snare Tom  Top
number   number    BLK/FNUM2 FNUM    Drum  Hat   Drum  Tom  Cymbal
   6     12,15     B6        A6      +

   7     13,16     B7        A7            +     +           +

   8     14,17     B8        A8            +           +     +

*/

/* calculate rhythm */

static inline void OPL_CALC_RH( FM_OPL *OPL, OPL_CH *CH, unsigned int noise )
{
	OPL_SLOT *SLOT;
	signed int out;
	unsigned int env;


	/* Bass Drum (verified on real YM3812):
	  - depends on the channel 6 'connect' register:
	      when connect = 0 it works the same as in normal (non-rhythm) mode (op1->op2->out)
	      when connect = 1 _only_ operator 2 is present on output (op2->out), operator 1 is ignored
	  - output sample always is multiplied by 2
	*/

	OPL->phase_modulation = 0;
	/* SLOT 1 */
	SLOT = &CH[6].SLOT[SLOT1];
	env = volume_calc(SLOT);

	out = SLOT->op1_out[0] + SLOT->op1_out[1];
	SLOT->op1_out[0] = SLOT->op1_out[1];

	if (!SLOT->CON)
		OPL->phase_modulation = SLOT->op1_out[0];
	/* else ignore output of operator 1 */

	SLOT->op1_out[1] = 0;
	if( env < ENV_QUIET )
	{
		if (!SLOT->FB)
			out = 0;
		SLOT->op1_out[1] = op_calc1(SLOT->Cnt, env, (out<<SLOT->FB), SLOT->wavetable );
	}

	/* SLOT 2 */
	SLOT++;
	env = volume_calc(SLOT);
	if( env < ENV_QUIET )
		OPL->output[0] += op_calc(SLOT->Cnt, env, OPL->phase_modulation, SLOT->wavetable) * 2;


	/* Phase generation is based on: */
	/* HH  (13) channel 7->slot 1 combined with channel 8->slot 2
		(same combination as TOP CYMBAL but different output phases) */
	/* SD  (16) channel 7->slot 1 */
	/* TOM (14) channel 8->slot 1 */
	/* TOP (17) channel 7->slot 1 combined with channel 8->slot 2
		(same combination as HIGH HAT but different output phases) */

	/* Envelope generation based on: */
	/* HH  channel 7->slot1 */
	/* SD  channel 7->slot2 */
	/* TOM channel 8->slot1 */
	/* TOP channel 8->slot2 */


	/* The following formulas can be well optimized.
	   I leave them in direct form for now (in case I've missed something).
	*/

	/* High Hat (verified on real YM3812) */
	env = volume_calc(SLOT7_1);
	if( env < ENV_QUIET )
	{
		/* high hat phase generation:
		    phase = d0 or 234 (based on frequency only)
		    phase = 34 or 2d0 (based on noise)
		*/

		/* base frequency derived from operator 1 in channel 7 */
		unsigned char bit7 = ((SLOT7_1->Cnt>>FREQ_SH)>>7)&1;
		unsigned char bit3 = ((SLOT7_1->Cnt>>FREQ_SH)>>3)&1;
		unsigned char bit2 = ((SLOT7_1->Cnt>>FREQ_SH)>>2)&1;

		unsigned char res1 = (bit2 ^ bit7) | bit3;

		/* when res1 = 0 phase = 0x000 | 0xd0; */
		/* when res1 = 1 phase = 0x200 | (0xd0>>2); */
		uint32_t phase = res1 ? (0x200|(0xd0>>2)) : 0xd0;

		/* enable gate based on frequency of operator 2 in channel 8 */
		unsigned char bit5e= ((SLOT8_2->Cnt>>FREQ_SH)>>5)&1;
		unsigned char bit3e= ((SLOT8_2->Cnt>>FREQ_SH)>>3)&1;

		unsigned char res2 = (bit3e ^ bit5e);

		/* when res2 = 0 pass the phase from calculation above (res1); */
		/* when res2 = 1 phase = 0x200 | (0xd0>>2); */
		if (res2)
			phase = (0x200|(0xd0>>2));


		/* when phase & 0x200 is set and noise=1 then phase = 0x200|0xd0 */
		/* when phase & 0x200 is set and noise=0 then phase = 0x200|(0xd0>>2), ie no change */
		if (phase&0x200)
		{
			if (noise)
				phase = 0x200|0xd0;
		}
		else
		/* when phase & 0x200 is clear and noise=1 then phase = 0xd0>>2 */
		/* when phase & 0x200 is clear and noise=0 then phase = 0xd0, ie no change */
		{
			if (noise)
				phase = 0xd0>>2;
		}

		OPL->output[0] += op_calc(phase<<FREQ_SH, env, 0, SLOT7_1->wavetable) * 2;
	}

	/* Snare Drum (verified on real YM3812) */
	env = volume_calc(SLOT7_2);
	if( env < ENV_QUIET )
	{
		/* base frequency derived from operator 1 in channel 7 */
		unsigned char bit8 = ((SLOT7_1->Cnt>>FREQ_SH)>>8)&1;

		/* when bit8 = 0 phase = 0x100; */
		/* when bit8 = 1 phase = 0x200; */
		uint32_t phase = bit8 ? 0x200 : 0x100;

		/* Noise bit XOR'es phase by 0x100 */
		/* when noisebit = 0 pass the phase from calculation above */
		/* when noisebit = 1 phase ^= 0x100; */
		/* in other words: phase ^= (noisebit<<8); */
		if (noise)
			phase ^= 0x100;

		OPL->output[0] += op_calc(phase<<FREQ_SH, env, 0, SLOT7_2->wavetable) * 2;
	}

	/* Tom Tom (verified on real YM3812) */
	env = volume_calc(SLOT8_1);
	if( env < ENV_QUIET )
		OPL->output[0] += op_calc(SLOT8_1->Cnt, env, 0, SLOT8_1->wavetable) * 2;

	/* Top Cymbal (verified on real YM3812) */
	env = volume_calc(SLOT8_2);
	if( env < ENV_QUIET )
	{
		/* base frequency derived from operator 1 in channel 7 */
		unsigned char bit7 = ((SLOT7_1->Cnt>>FREQ_SH)>>7)&1;
		unsigned char bit3 = ((SLOT7_1->Cnt>>FREQ_SH)>>3)&1;
		unsigned char bit2 = ((SLOT7_1->Cnt>>FREQ_SH)>>2)&1;

		unsigned char res1 = (bit2 ^ bit7) | bit3;

		/* when res1 = 0 phase = 0x000 | 0x100; */
		/* when res1 = 1 phase = 0x200 | 0x100; */
		uint32_t phase = res1 ? 0x300 : 0x100;

		/* enable gate based on frequency of operator 2 in channel 8 */
		unsigned char bit5e= ((SLOT8_2->Cnt>>FREQ_SH)>>5)&1;
		unsigned char bit3e= ((SLOT8_2->Cnt>>FREQ_SH)>>3)&1;

		unsigned char res2 = (bit3e ^ bit5e);
		/* when res2 = 0 pass the phase from calculation above (res1); */
		/* when res2 = 1 phase = 0x200 | 0x100; */
		if (res2)
			phase = 0x300;

		OPL->output[0] += op_calc(phase<<FREQ_SH, env, 0, SLOT8_2->wavetable) * 2;
	}
}


/* generic table initialize */
static int init_tables(void)
{
	signed int i,x;
	signed int n;
	double o,m;


	for (x=0; x<TL_RES_LEN; x++)
	{
		m = (1<<16) / pow(2.0, (x+1) * (ENV_STEP/4.0) / 8.0);
		m = floor(m);

		/* we never reach (1<<16) here due to the (x+1) */
		/* result fits within 16 bits at maximum */

		n = (int)m;     /* 16 bits here */
		n >>= 4;        /* 12 bits here */
		if (n&1)        /* round to nearest */
			n = (n>>1)+1;
		else
			n = n>>1;
						/* 11 bits here (rounded) */
		n <<= 1;        /* 12 bits here (as in real chip) */
		tl_tab[ x*2 + 0 ] = n;
		tl_tab[ x*2 + 1 ] = -tl_tab[ x*2 + 0 ];

		for (i=1; i<12; i++)
		{
			tl_tab[ x*2+0 + i*2*TL_RES_LEN ] =  tl_tab[ x*2+0 ]>>i;
			tl_tab[ x*2+1 + i*2*TL_RES_LEN ] = -tl_tab[ x*2+0 + i*2*TL_RES_LEN ];
		}
	#if 0
			logerror("tl %04i", x*2);
			for (i=0; i<12; i++)
				logerror(", [%02i] %5i", i*2, tl_tab[ x*2 /*+1*/ + i*2*TL_RES_LEN ] );
			logerror("\n");
	#endif
	}
	/*logerror("FMOPL.C: TL_TAB_LEN = %i elements (%i bytes)\n",TL_TAB_LEN, (int)sizeof(tl_tab));*/


	for (i=0; i<SIN_LEN; i++)
	{
		/* non-standard sinus */
		m = sin( ((i*2)+1) * M_PI / SIN_LEN ); /* checked against the real chip */

		/* we never reach zero here due to ((i*2)+1) */

		if (m>0.0)
			o = 8*log(1.0/m)/log(2.0);  /* convert to 'decibels' */
		else
			o = 8*log(-1.0/m)/log(2.0); /* convert to 'decibels' */

		o = o / (ENV_STEP/4);

		n = (int)(2.0*o);
		if (n&1)                        /* round to nearest */
			n = (n>>1)+1;
		else
			n = n>>1;

		sin_tab[ i ] = n*2 + (m>=0.0? 0: 1 );

		/*logerror("FMOPL.C: sin [%4i (hex=%03x)]= %4i (tl_tab value=%5i)\n", i, i,
			sin_tab[i], tl_tab[sin_tab[i]] );*/
	}

	for (i=0; i<SIN_LEN; i++)
	{
		/* waveform 1:  __      __     */
		/*             /  \____/  \____*/
		/* output only first half of the sinus waveform (positive one) */

		if (i & (1<<(SIN_BITS-1)) )
			sin_tab[1*SIN_LEN+i] = TL_TAB_LEN;
		else
			sin_tab[1*SIN_LEN+i] = sin_tab[i];

		/* waveform 2:  __  __  __  __ */
		/*             /  \/  \/  \/  \*/
		/* abs(sin) */

		sin_tab[2*SIN_LEN+i] = sin_tab[i & (SIN_MASK>>1) ];

		/* waveform 3:  _   _   _   _  */
		/*             / |_/ |_/ |_/ |_*/
		/* abs(output only first quarter of the sinus waveform) */

		if (i & (1<<(SIN_BITS-2)) )
			sin_tab[3*SIN_LEN+i] = TL_TAB_LEN;
		else
			sin_tab[3*SIN_LEN+i] = sin_tab[i & (SIN_MASK>>2)];

		/*logerror("FMOPL.C: sin1[%4i]= %4i (tl_tab value=%5i)\n", i,
			sin_tab[1*SIN_LEN+i], tl_tab[sin_tab[1*SIN_LEN+i]] );
		logerror("FMOPL.C: sin2[%4i]= %4i (tl_tab value=%5i)\n", i,
			sin_tab[2*SIN_LEN+i], tl_tab[sin_tab[2*SIN_LEN+i]] );
		logerror("FMOPL.C: sin3[%4i]= %4i (tl_tab value=%5i)\n", i,
			sin_tab[3*SIN_LEN+i], tl_tab[sin_tab[3*SIN_LEN+i]] );*/
	}
	/*logerror("FMOPL.C: ENV_QUIET= %08x (dec*8=%i)\n", ENV_QUIET, ENV_QUIET*8 );*/


	return 1;
}

static void OPLCloseTable( void )
{
}



static void OPL_initalize(FM_OPL *OPL)
{
	int i;

	/* frequency base */
	OPL->freqbase  = (OPL->rate) ? ((double)OPL->clock / 72.0) / OPL->rate  : 0;
#if 0
	OPL->rate = (double)OPL->clock / 72.0;
	OPL->freqbase  = 1.0;
#endif

	/*logerror("freqbase=%f\n", OPL->freqbase);*/

	/* Timer base time */
	OPL->TimerBase = 72.0 / (double)OPL->clock;

	/* make fnumber -> increment counter table */
	for( i=0 ; i < 1024 ; i++ )
	{
		/* opn phase increment counter = 20bit */
		/* -10 because chip works with 10.10 fixed point, while we use 16.16 */
		OPL->fn_tab[i] = (uint32_t)( (double)i * 64 * OPL->freqbase * (1<<(FREQ_SH-10)) );
#if 0
		logerror("FMOPL.C: fn_tab[%4i] = %08x (dec=%8i)\n",
				    i, OPL->fn_tab[i]>>6, OPL->fn_tab[i]>>6 );
#endif
	}

#if 0
	for( i=0 ; i < 16 ; i++ )
	{
		logerror("FMOPL.C: sl_tab[%i] = %08x\n",
			i, sl_tab[i] );
	}
	for( i=0 ; i < 8 ; i++ )
	{
		int j;
		logerror("FMOPL.C: ksl_tab[oct=%2i] =",i);
		for (j=0; j<16; j++)
		{
			logerror("%08x ", ksl_tab[i*16+j] );
		}
		logerror("\n");
	}
#endif


	/* Amplitude modulation: 27 output levels (triangle waveform);
	1 level takes one of: 192, 256 or 448 samples */
	/* One entry from LFO_AM_TABLE lasts for 64 samples */
	OPL->lfo_am_inc = (uint32_t)((1.0 / 64.0 ) * (1<<LFO_SH) * OPL->freqbase);

	/* Vibrato: 8 output levels (triangle waveform); 1 level takes 1024 samples */
	OPL->lfo_pm_inc = (uint32_t)((1.0 / 1024.0) * (1<<LFO_SH) * OPL->freqbase);

	/*logerror ("OPL->lfo_am_inc = %8x ; OPL->lfo_pm_inc = %8x\n", OPL->lfo_am_inc, OPL->lfo_pm_inc);*/

	/* Noise generator: a step takes 1 sample */
	OPL->noise_f = (uint32_t)((1.0 / 1.0) * (1<<FREQ_SH) * OPL->freqbase);

	OPL->eg_timer_add  = (uint32_t)((1<<EG_SH)  * OPL->freqbase);
	OPL->eg_timer_overflow = ( 1 ) * (1<<EG_SH);
	/*logerror("OPLinit eg_timer_add=%8x eg_timer_overflow=%8x\n",
		OPL->eg_timer_add, OPL->eg_timer_overflow);*/

}

static inline void FM_KEYON(OPL_SLOT *SLOT, uint32_t key_set)
{
	if( !SLOT->key )
	{
		/* restart Phase Generator */
		SLOT->Cnt = 0;
		/* phase -> Attack */
		SLOT->state = EG_ATT;
	}
	SLOT->key |= key_set;
}

static inline void FM_KEYOFF(OPL_SLOT *SLOT, uint32_t key_clr)
{
	if( SLOT->key )
	{
		SLOT->key &= key_clr;

		if( !SLOT->key )
		{
			/* phase -> Release */
			if (SLOT->state>EG_REL)
				SLOT->state = EG_REL;
		}
	}
}

/* update phase increment counter of operator (also update the EG rates if necessary) */
static inline void CALC_FCSLOT(OPL_CH *CH,OPL_SLOT *SLOT)
{
	int ksr;

	/* (frequency) phase increment counter */
	SLOT->Incr = CH->fc * SLOT->mul;
	ksr = CH->kcode >> SLOT->KSR;

	if( SLOT->ksr != ksr )
	{
		SLOT->ksr = ksr;

		/* calculate envelope generator rates */
		if ((SLOT->ar + SLOT->ksr) < 16+62)
		{
			SLOT->eg_sh_ar  = eg_rate_shift [SLOT->ar + SLOT->ksr ];
			SLOT->eg_sel_ar = eg_rate_select[SLOT->ar + SLOT->ksr ];
		}
		else
		{
			SLOT->eg_sh_ar  = 0;
			SLOT->eg_sel_ar = 13*RATE_STEPS;
		}
		SLOT->eg_sh_dr  = eg_rate_shift [SLOT->dr + SLOT->ksr ];
		SLOT->eg_sel_dr = eg_rate_select[SLOT->dr + SLOT->ksr ];
		SLOT->eg_sh_rr  = eg_rate_shift [SLOT->rr + SLOT->ksr ];
		SLOT->eg_sel_rr = eg_rate_select[SLOT->rr + SLOT->ksr ];
	}
}

/* set multi,am,vib,EG-TYP,KSR,mul */
static inline void set_mul(FM_OPL *OPL,int slot,int v)
{
	OPL_CH   *CH   = &OPL->P_CH[slot/2];
	OPL_SLOT *SLOT = &CH->SLOT[slot&1];

	SLOT->mul     = mul_tab[v&0x0f];
	SLOT->KSR     = (v&0x10) ? 0 : 2;
	SLOT->eg_type = (v&0x20);
	SLOT->vib     = (v&0x40);
	SLOT->AMmask  = (v&0x80) ? ~0 : 0;
	CALC_FCSLOT(CH,SLOT);
}

/* set ksl & tl */
static inline void set_ksl_tl(FM_OPL *OPL,int slot,int v)
{
	OPL_CH   *CH   = &OPL->P_CH[slot/2];
	OPL_SLOT *SLOT = &CH->SLOT[slot&1];

	SLOT->ksl = ksl_shift[v >> 6];
	SLOT->TL  = (v&0x3f)<<(ENV_BITS-1-7); /* 7 bits TL (bit 6 = always 0) */

	SLOT->TLL = SLOT->TL + (CH->ksl_base>>SLOT->ksl);
}

/* set attack rate & decay rate  */
static inline void set_ar_dr(FM_OPL *OPL,int slot,int v)
{
	OPL_CH   *CH   = &OPL->P_CH[slot/2];
	OPL_SLOT *SLOT = &CH->SLOT[slot&1];

	SLOT->ar = (v>>4)  ? 16 + ((v>>4)  <<2) : 0;

	if ((SLOT->ar + SLOT->ksr) < 16+62)
	{
		SLOT->eg_sh_ar  = eg_rate_shift [SLOT->ar + SLOT->ksr ];
		SLOT->eg_sel_ar = eg_rate_select[SLOT->ar + SLOT->ksr ];
	}
	else
	{
		SLOT->eg_sh_ar  = 0;
		SLOT->eg_sel_ar = 13*RATE_STEPS;
	}

	SLOT->dr    = (v&0x0f)? 16 + ((v&0x0f)<<2) : 0;
	SLOT->eg_sh_dr  = eg_rate_shift [SLOT->dr + SLOT->ksr ];
	SLOT->eg_sel_dr = eg_rate_select[SLOT->dr + SLOT->ksr ];
}

/* set sustain level & release rate */
static inline void set_sl_rr(FM_OPL *OPL,int slot,int v)
{
	OPL_CH   *CH   = &OPL->P_CH[slot/2];
	OPL_SLOT *SLOT = &CH->SLOT[slot&1];

	SLOT->sl  = sl_tab[ v>>4 ];

	SLOT->rr  = (v&0x0f)? 16 + ((v&0x0f)<<2) : 0;
	SLOT->eg_sh_rr  = eg_rate_shift [SLOT->rr + SLOT->ksr ];
	SLOT->eg_sel_rr = eg_rate_select[SLOT->rr + SLOT->ksr ];
}


/* write a value v to register r on OPL chip */
static void OPLWriteReg(FM_OPL *OPL, int r, int v)
{
	OPL_CH *CH;
	int slot;
	uint32_t block_fnum;


	/* adjust bus to 8 bits */
	r &= 0xff;
	v &= 0xff;

	switch(r&0xe0)
	{
	case 0x00:  /* 00-1f:control */
		switch(r&0x1f)
		{
		case 0x01:  /* waveform select enable */
			if(OPL->type&OPL_TYPE_WAVESEL)
			{
				OPL->wavesel = v&0x20;
				/* do not change the waveform previously selected */
			}
			break;
		case 0x02:  /* Timer 1 */
			OPL->T[0] = (256-v)*4;
			break;
		case 0x03:  /* Timer 2 */
			OPL->T[1] = (256-v)*16;
			break;
		case 0x04:  /* IRQ clear / mask and Timer enable */
			if(v&0x80)
			{   /* IRQ flag clear */
				/* don't reset BFRDY flag or we will have to call deltat module to set the flag */
				OPL_STATUS_RESET(OPL,0x7f-0x08);
			}
			else
			{   /* set IRQ mask ,timer enable*/
				uint8_t st1 = v&1;
				uint8_t st2 = (v>>1)&1;

				/* IRQRST,T1MSK,t2MSK,EOSMSK,BRMSK,x,ST2,ST1 */
				OPL_STATUS_RESET(OPL, v & (0x78-0x08) );
				OPL_STATUSMASK_SET(OPL, (~v) & 0x78 );

				/* timer 2 */
				if(OPL->st[1] != st2)
				{
					double period = st2 ? (OPL->TimerBase * OPL->T[1]) : 0.0;
					OPL->st[1] = st2;
					if (OPL->timer_handler) (OPL->timer_handler)(OPL->TimerParam,1,period);
				}
				/* timer 1 */
				if(OPL->st[0] != st1)
				{
					double period = st1 ? (OPL->TimerBase * OPL->T[0]) : 0.0;
					OPL->st[0] = st1;
					if (OPL->timer_handler) (OPL->timer_handler)(OPL->TimerParam,0,period);
				}
			}
			break;
#if BUILD_Y8950
		case 0x06:      /* Key Board OUT */
			if(OPL->type&OPL_TYPE_KEYBOARD)
			{
				if(OPL->keyboardhandler_w)
					OPL->keyboardhandler_w(OPL->keyboard_param,v);
				else
					logerror("Y8950: write unmapped KEYBOARD port\n");
			}
			break;
		case 0x07:  /* DELTA-T control 1 : START,REC,MEMDATA,REPT,SPOFF,x,x,RST */
			if(OPL->type&OPL_TYPE_ADPCM)
				YM_DELTAT_ADPCM_Write(OPL->deltat,r-0x07,v);
			break;
#endif
		case 0x08:  /* MODE,DELTA-T control 2 : CSM,NOTESEL,x,x,smpl,da/ad,64k,rom */
			OPL->mode = v;
#if BUILD_Y8950
			if(OPL->type&OPL_TYPE_ADPCM) {
				/* mask 4 LSBs in register 08 for DELTA-T unit */
				YM_DELTAT_ADPCM_Write(OPL->deltat,r-0x07,v&0x0f);
			}
#endif
			break;

#if BUILD_Y8950
		case 0x09:      /* START ADD */
		case 0x0a:
		case 0x0b:      /* STOP ADD  */
		case 0x0c:
		case 0x0d:      /* PRESCALE   */
		case 0x0e:
		case 0x0f:      /* ADPCM data write */
		case 0x10:      /* DELTA-N    */
		case 0x11:      /* DELTA-N    */
		case 0x12:      /* ADPCM volume */
			if(OPL->type&OPL_TYPE_ADPCM)
				YM_DELTAT_ADPCM_Write(OPL->deltat,r-0x07,v);
			break;

		case 0x15:      /* DAC data high 8 bits (F7,F6...F2) */
		case 0x16:      /* DAC data low 2 bits (F1, F0 in bits 7,6) */
		case 0x17:      /* DAC data shift (S2,S1,S0 in bits 2,1,0) */
			logerror("FMOPL.C: DAC data register written, but not implemented reg=%02x val=%02x\n",
				r,v);
			break;

		case 0x18:      /* I/O CTRL (Direction) */
			if(OPL->type&OPL_TYPE_IO)
				OPL->portDirection = v&0x0f;
			break;
		case 0x19:      /* I/O DATA */
			if(OPL->type&OPL_TYPE_IO)
			{
				OPL->portLatch = v;
				if(OPL->porthandler_w)
					OPL->porthandler_w(OPL->port_param,v&OPL->portDirection);
			}
			break;
#endif
		default:
			/*logerror("FMOPL.C: write to unknown register: %02x\n",r);*/
			break;
		}
		break;
	case 0x20:  /* am ON, vib ON, ksr, eg_type, mul */
		slot = slot_array[r&0x1f];
		if(slot < 0) return;
		set_mul(OPL,slot,v);
		break;
	case 0x40:
		slot = slot_array[r&0x1f];
		if(slot < 0) return;
		set_ksl_tl(OPL,slot,v);
		break;
	case 0x60:
		slot = slot_array[r&0x1f];
		if(slot < 0) return;
		set_ar_dr(OPL,slot,v);
		break;
	case 0x80:
		slot = slot_array[r&0x1f];
		if(slot < 0) return;
		set_sl_rr(OPL,slot,v);
		break;
	case 0xa0:
		if (r == 0xbd)          /* am depth, vibrato depth, r,bd,sd,tom,tc,hh */
		{
			OPL->lfo_am_depth = v & 0x80;
			OPL->lfo_pm_depth_range = (v&0x40) ? 8 : 0;

			OPL->rhythm  = v&0x3f;

			if(OPL->rhythm&0x20)
			{
				/* BD key on/off */
				if(v&0x10)
				{
					FM_KEYON (&OPL->P_CH[6].SLOT[SLOT1], 2);
					FM_KEYON (&OPL->P_CH[6].SLOT[SLOT2], 2);
				}
				else
				{
					FM_KEYOFF(&OPL->P_CH[6].SLOT[SLOT1],~2);
					FM_KEYOFF(&OPL->P_CH[6].SLOT[SLOT2],~2);
				}
				/* HH key on/off */
				if(v&0x01) FM_KEYON (&OPL->P_CH[7].SLOT[SLOT1], 2);
				else       FM_KEYOFF(&OPL->P_CH[7].SLOT[SLOT1],~2);
				/* SD key on/off */
				if(v&0x08) FM_KEYON (&OPL->P_CH[7].SLOT[SLOT2], 2);
				else       FM_KEYOFF(&OPL->P_CH[7].SLOT[SLOT2],~2);
				/* TOM key on/off */
				if(v&0x04) FM_KEYON (&OPL->P_CH[8].SLOT[SLOT1], 2);
				else       FM_KEYOFF(&OPL->P_CH[8].SLOT[SLOT1],~2);
				/* TOP-CY key on/off */
				if(v&0x02) FM_KEYON (&OPL->P_CH[8].SLOT[SLOT2], 2);
				else       FM_KEYOFF(&OPL->P_CH[8].SLOT[SLOT2],~2);
			}
			else
			{
				/* BD key off */
				FM_KEYOFF(&OPL->P_CH[6].SLOT[SLOT1],~2);
				FM_KEYOFF(&OPL->P_CH[6].SLOT[SLOT2],~2);
				/* HH key off */
				FM_KEYOFF(&OPL->P_CH[7].SLOT[SLOT1],~2);
				/* SD key off */
				FM_KEYOFF(&OPL->P_CH[7].SLOT[SLOT2],~2);
				/* TOM key off */
				FM_KEYOFF(&OPL->P_CH[8].SLOT[SLOT1],~2);
				/* TOP-CY off */
				FM_KEYOFF(&OPL->P_CH[8].SLOT[SLOT2],~2);
			}
			return;
		}
		/* keyon,block,fnum */
		if( (r&0x0f) > 8) return;
		CH = &OPL->P_CH[r&0x0f];
		if(!(r&0x10))
		{   /* a0-a8 */
			block_fnum  = (CH->block_fnum&0x1f00) | v;
		}
		else
		{   /* b0-b8 */
			block_fnum = ((v&0x1f)<<8) | (CH->block_fnum&0xff);

			if(v&0x20)
			{
				FM_KEYON (&CH->SLOT[SLOT1], 1);
				FM_KEYON (&CH->SLOT[SLOT2], 1);
			}
			else
			{
				FM_KEYOFF(&CH->SLOT[SLOT1],~1);
				FM_KEYOFF(&CH->SLOT[SLOT2],~1);
			}
		}
		/* update */
		if(CH->block_fnum != block_fnum)
		{
			uint8_t block  = block_fnum >> 10;

			CH->block_fnum = block_fnum;

			CH->ksl_base = ksl_tab[block_fnum>>6];
			CH->fc       = OPL->fn_tab[block_fnum&0x03ff] >> (7-block);

			/* BLK 2,1,0 bits -> bits 3,2,1 of kcode */
			CH->kcode    = (CH->block_fnum&0x1c00)>>9;

			/* the info below is actually opposite to what is stated in the Manuals
			 (verifed on real YM3812) */
			/* if notesel == 0 -> lsb of kcode is bit 10 (MSB) of fnum  */
			/* if notesel == 1 -> lsb of kcode is bit 9 (MSB-1) of fnum */
			if (OPL->mode&0x40)
				CH->kcode |= (CH->block_fnum&0x100)>>8; /* notesel == 1 */
			else
				CH->kcode |= (CH->block_fnum&0x200)>>9; /* notesel == 0 */

			/* refresh Total Level in both SLOTs of this channel */
			CH->SLOT[SLOT1].TLL = CH->SLOT[SLOT1].TL + (CH->ksl_base>>CH->SLOT[SLOT1].ksl);
			CH->SLOT[SLOT2].TLL = CH->SLOT[SLOT2].TL + (CH->ksl_base>>CH->SLOT[SLOT2].ksl);

			/* refresh frequency counter in both SLOTs of this channel */
			CALC_FCSLOT(CH,&CH->SLOT[SLOT1]);
			CALC_FCSLOT(CH,&CH->SLOT[SLOT2]);
		}
		break;
	case 0xc0:
		/* FB,C */
		if( (r&0x0f) > 8) return;
		CH = &OPL->P_CH[r&0x0f];
		CH->SLOT[SLOT1].FB  = (v>>1)&7 ? ((v>>1)&7) + 7 : 0;
		CH->SLOT[SLOT1].CON = v&1;
		CH->SLOT[SLOT1].connect1 = CH->SLOT[SLOT1].CON ? &OPL->output[0] : &OPL->phase_modulation;
		break;
	case 0xe0: /* waveform select */
		/* simply ignore write to the waveform select register
		if selecting not enabled in test register */
		if(OPL->wavesel)
		{
			slot = slot_array[r&0x1f];
			if(slot < 0) return;
			CH = &OPL->P_CH[slot/2];

			CH->SLOT[slot&1].wavetable = (v&0x03)*SIN_LEN;
		}
		break;
	}
}

/* lock/unlock for common table */
static int OPL_LockTable()
{
	num_lock++;
	if(num_lock>1) return 0;

	/* first time */

	/* allocate total level table (128kb space) */
	if( !init_tables() )
	{
		num_lock--;
		return -1;
	}

	return 0;
}

static void OPL_UnLockTable(void)
{
	if(num_lock) num_lock--;
	if(num_lock) return;

	/* last time */

	OPLCloseTable();

}

static void OPLResetChip(FM_OPL *OPL)
{
	int c,s;
	int i;

	OPL->eg_timer = 0;
	OPL->eg_cnt   = 0;

	OPL->noise_rng = 1; /* noise shift register */
	OPL->mode   = 0;    /* normal mode */
	OPL_STATUS_RESET(OPL,0x7f);

	/* reset with register write */
	OPLWriteReg(OPL,0x01,0); /* wavesel disable */
	OPLWriteReg(OPL,0x02,0); /* Timer1 */
	OPLWriteReg(OPL,0x03,0); /* Timer2 */
	OPLWriteReg(OPL,0x04,0); /* IRQ mask clear */
	for(i = 0xff ; i >= 0x20 ; i-- ) OPLWriteReg(OPL,i,0);

	/* reset operator parameters */
	for( c = 0 ; c < 9 ; c++ )
	{
		OPL_CH *CH = &OPL->P_CH[c];
		for(s = 0 ; s < 2 ; s++ )
		{
			/* wave table */
			CH->SLOT[s].wavetable = 0;
			CH->SLOT[s].state     = EG_OFF;
			CH->SLOT[s].volume    = MAX_ATT_INDEX;
		}
	}
#if BUILD_Y8950
	if(OPL->type&OPL_TYPE_ADPCM)
	{
		YM_DELTAT *DELTAT = OPL->deltat;

		DELTAT->freqbase = OPL->freqbase;
		DELTAT->output_pointer = &OPL->output_deltat[0];
		DELTAT->portshift = 5;
		DELTAT->output_range = 1<<23;
		YM_DELTAT_ADPCM_Reset(DELTAT,0,YM_DELTAT_EMULATION_MODE_NORMAL);
	}
#endif
}


#if 0 // not used anywhere
static void OPL_postload(FM_OPL *OPL)
{
	int slot, ch;

	for( ch=0 ; ch < 9 ; ch++ )
	{
		OPL_CH *CH = &OPL->P_CH[ch];

		/* Look up key scale level */
		uint32_t block_fnum = CH->block_fnum;
		CH->ksl_base = ksl_tab[block_fnum >> 6];
		CH->fc       = OPL->fn_tab[block_fnum & 0x03ff] >> (7 - (block_fnum >> 10));

		for( slot=0 ; slot < 2 ; slot++ )
		{
			OPL_SLOT *SLOT = &CH->SLOT[slot];

			/* Calculate key scale rate */
			SLOT->ksr = CH->kcode >> SLOT->KSR;

			/* Calculate attack, decay and release rates */
			if ((SLOT->ar + SLOT->ksr) < 16+62)
			{
				SLOT->eg_sh_ar  = eg_rate_shift [SLOT->ar + SLOT->ksr ];
				SLOT->eg_sel_ar = eg_rate_select[SLOT->ar + SLOT->ksr ];
			}
			else
			{
				SLOT->eg_sh_ar  = 0;
				SLOT->eg_sel_ar = 13*RATE_STEPS;
			}
			SLOT->eg_sh_dr  = eg_rate_shift [SLOT->dr + SLOT->ksr ];
			SLOT->eg_sel_dr = eg_rate_select[SLOT->dr + SLOT->ksr ];
			SLOT->eg_sh_rr  = eg_rate_shift [SLOT->rr + SLOT->ksr ];
			SLOT->eg_sel_rr = eg_rate_select[SLOT->rr + SLOT->ksr ];

			/* Calculate phase increment */
			SLOT->Incr = CH->fc * SLOT->mul;

			/* Total level */
			SLOT->TLL = SLOT->TL + (CH->ksl_base >> SLOT->ksl);

			/* Connect output */
			SLOT->connect1 = SLOT->CON ? &OPL->output[0] : &OPL->phase_modulation;
		}
	}
#if BUILD_Y8950
	if ( (OPL->type & OPL_TYPE_ADPCM) && (OPL->deltat) )
	{
		// We really should call the postlod function for the YM_DELTAT, but it's hard without registers
		// (see the way the YM2610 does it)
		//YM_DELTAT_postload(OPL->deltat, REGS);
	}
#endif
}
#endif


/* Create one of virtual YM3812/YM3526/Y8950 */
/* 'clock' is chip clock in Hz  */
/* 'rate'  is sampling rate  */
static FM_OPL *OPLCreate(uint32_t clock, uint32_t rate, int type)
{
	char *ptr;
	FM_OPL *OPL;
	int state_size;

	if (OPL_LockTable() == -1) return NULL;

	/* calculate OPL state size */
	state_size  = sizeof(FM_OPL);

#if BUILD_Y8950
	if (type&OPL_TYPE_ADPCM) state_size+= sizeof(YM_DELTAT);
#endif

	/* allocate memory block */
	ptr = (char *)calloc(1, state_size);
	if (ptr == NULL)
		return NULL;

	OPL  = (FM_OPL *)ptr;

	ptr += sizeof(FM_OPL);

#if BUILD_Y8950
	if (type&OPL_TYPE_ADPCM)
	{
		OPL->deltat = (YM_DELTAT *)ptr;
	}
	ptr += sizeof(YM_DELTAT);
#endif

	OPL->type  = type;
	OPL->clock = clock;
	OPL->rate  = rate;

	/* init global tables */
	OPL_initalize(OPL);

	return OPL;
}

/* Destroy one of virtual YM3812 */
static void OPLDestroy(FM_OPL *OPL)
{
	OPL_UnLockTable();
	free(OPL);
}

/* Optional handlers */

static void OPLSetTimerHandler(FM_OPL *OPL,OPL_TIMERHANDLER timer_handler,void *param)
{
	OPL->timer_handler   = timer_handler;
	OPL->TimerParam = param;
}
static void OPLSetIRQHandler(FM_OPL *OPL,OPL_IRQHANDLER IRQHandler,void *param)
{
	OPL->IRQHandler     = IRQHandler;
	OPL->IRQParam = param;
}
static void OPLSetUpdateHandler(FM_OPL *OPL,OPL_UPDATEHANDLER UpdateHandler,void *param)
{
	OPL->UpdateHandler = UpdateHandler;
	OPL->UpdateParam = param;
}

static int OPLWrite(FM_OPL *OPL,int a,int v)
{
	if( !(a&1) )
	{   /* address port */
		OPL->address = v & 0xff;
	}
	else
	{   /* data port */
		if(OPL->UpdateHandler) OPL->UpdateHandler(OPL->UpdateParam,0);
		OPLWriteReg(OPL,OPL->address,v);
	}
	return OPL->status>>7;
}

static unsigned char OPLRead(FM_OPL *OPL,int a)
{
	if( !(a&1) )
	{
		/* status port */

		#if BUILD_Y8950

		if(OPL->type&OPL_TYPE_ADPCM)    /* Y8950 */
		{
			return (OPL->status & (OPL->statusmask|0x80)) | (OPL->deltat->PCM_BSY&1);
		}

		#endif

		/* OPL and OPL2 */
		return OPL->status & (OPL->statusmask|0x80);
	}

#if BUILD_Y8950
	/* data port */
	switch(OPL->address)
	{
	case 0x05: /* KeyBoard IN */
		if(OPL->type&OPL_TYPE_KEYBOARD)
		{
			if(OPL->keyboardhandler_r)
				return OPL->keyboardhandler_r(OPL->keyboard_param);
			else
				logerror("Y8950: read unmapped KEYBOARD port\n");
		}
		return 0;

	case 0x0f: /* ADPCM-DATA  */
		if(OPL->type&OPL_TYPE_ADPCM)
		{
			uint8_t val;

			val = YM_DELTAT_ADPCM_Read(OPL->deltat);
			/*logerror("Y8950: read ADPCM value read=%02x\n",val);*/
			return val;
		}
		return 0;

	case 0x19: /* I/O DATA    */
		if(OPL->type&OPL_TYPE_IO)
		{
			if(OPL->porthandler_r)
				return OPL->porthandler_r(OPL->port_param);
			else
				logerror("Y8950:read unmapped I/O port\n");
		}
		return 0;
	case 0x1a: /* PCM-DATA    */
		if(OPL->type&OPL_TYPE_ADPCM)
		{
			logerror("Y8950 A/D conversion is accessed but not implemented !\n");
			return 0x80; /* 2's complement PCM data - result from A/D conversion */
		}
		return 0;
	}
#endif

	return 0xff;
}

/* CSM Key Controll */
static inline void CSMKeyControll(OPL_CH *CH)
{
	FM_KEYON (&CH->SLOT[SLOT1], 4);
	FM_KEYON (&CH->SLOT[SLOT2], 4);

	/* The key off should happen exactly one sample later - not implemented correctly yet */

	FM_KEYOFF(&CH->SLOT[SLOT1], ~4);
	FM_KEYOFF(&CH->SLOT[SLOT2], ~4);
}


static int OPLTimerOver(FM_OPL *OPL,int c)
{
	if( c )
	{   /* Timer B */
		OPL_STATUS_SET(OPL,0x20);
	}
	else
	{   /* Timer A */
		OPL_STATUS_SET(OPL,0x40);
		/* CSM mode key,TL controll */
		if( OPL->mode & 0x80 )
		{   /* CSM mode total level latch and auto key on */
			int ch;
			if(OPL->UpdateHandler) OPL->UpdateHandler(OPL->UpdateParam,0);
			for(ch=0; ch<9; ch++)
				CSMKeyControll( &OPL->P_CH[ch] );
		}
	}
	/* reload timer */
	if (OPL->timer_handler) (OPL->timer_handler)(OPL->TimerParam,c,OPL->TimerBase * OPL->T[c]);
	return OPL->status>>7;
}


#define MAX_OPL_CHIPS 2

void * ym3812_init(uint32_t clock, uint32_t rate)
{
    /* emulator create */
	FM_OPL *YM3812 = OPLCreate(clock,rate,OPL_TYPE_YM3812);
	if (YM3812)
	{
		ym3812_reset_chip(YM3812);
	}
	return YM3812;
}

void ym3812_shutdown(void *chip)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;

	/* emulator shutdown */
	OPLDestroy(YM3812);
}
void ym3812_reset_chip(void *chip)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;
	OPLResetChip(YM3812);
}

int ym3812_write(void *chip, int a, int v)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;
	return OPLWrite(YM3812, a, v);
}

unsigned char ym3812_read(void *chip, int a)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;
	/* YM3812 always returns bit2 and bit1 in HIGH state */
	return OPLRead(YM3812, a) | 0x06 ;
}
int ym3812_timer_over(void *chip, int c)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;
	return OPLTimerOver(YM3812, c);
}

void ym3812_set_timer_handler(void *chip, OPL_TIMERHANDLER timer_handler, void *param)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;
	OPLSetTimerHandler(YM3812, timer_handler, param);
}
void ym3812_set_irq_handler(void *chip,OPL_IRQHANDLER IRQHandler,void *param)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;
	OPLSetIRQHandler(YM3812, IRQHandler, param);
}
void ym3812_set_update_handler(void *chip,OPL_UPDATEHANDLER UpdateHandler,void *param)
{
	FM_OPL *YM3812 = (FM_OPL *)chip;
	OPLSetUpdateHandler(YM3812, UpdateHandler, param);
}


/*
** Generate samples for one of the YM3812's
**
** 'which' is the virtual YM3812 number
** '*buffer' is the output buffer pointer
** 'length' is the number of samples that should be generated
*/
void ym3812_update_one(void *chip, OPLSAMPLE *buffer, int length)
{
	FM_OPL      *OPL = (FM_OPL *)chip;
	uint8_t       rhythm = OPL->rhythm&0x20;
	OPLSAMPLE   *buf = buffer;
	int i;

	for( i=0; i < length ; i++ )
	{
		int lt;

		OPL->output[0] = 0;

		advance_lfo(OPL);

		/* FM part */
		OPL_CALC_CH(OPL, &OPL->P_CH[0]);
		OPL_CALC_CH(OPL, &OPL->P_CH[1]);
		OPL_CALC_CH(OPL, &OPL->P_CH[2]);
		OPL_CALC_CH(OPL, &OPL->P_CH[3]);
		OPL_CALC_CH(OPL, &OPL->P_CH[4]);
		OPL_CALC_CH(OPL, &OPL->P_CH[5]);

		if(!rhythm)
		{
			OPL_CALC_CH(OPL, &OPL->P_CH[6]);
			OPL_CALC_CH(OPL, &OPL->P_CH[7]);
			OPL_CALC_CH(OPL, &OPL->P_CH[8]);
		}
		else        /* Rhythm part */
		{
			OPL_CALC_RH(OPL, &OPL->P_CH[0], (OPL->noise_rng>>0)&1 );
		}

		lt = OPL->output[0];

		lt >>= FINAL_SH;

		/* limit check */
		lt = limit( lt , MAXOUT, MINOUT );

		/* store to sound buffer */
		buf[i] = lt;

		advance(OPL);
	}

}
