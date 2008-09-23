extern "C" {
#include "fmopl.h"
}
#include "snd_fm.h"

#define MAX_CHANNELS 256 /* Must not be less than the setting in sndfile.h */

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#define OPLNew(x,r)  YM3812Init(1, (x),(r))
#define OPLResetChip YM3812ResetChip
#define OPLWrite     YM3812Write
#define OPLUpdateOne YM3812UpdateOne
#define OPLClose     YM3812Shutdown

static const int oplbase = 0x388;

// OPL info
static int opl=-1, oplretval=0, oplregno=0;
static int fm_active=0;

static void Fmdrv_Outportb(unsigned port, unsigned value)
{
    if(opl < 0 || ((int)port) < oplbase || ((int)port) >= oplbase+4) return;
    unsigned ind = port-oplbase;
    OPLWrite(opl, ind, value);
    if(ind & 1)
    {
        if(oplregno == 4)
        {
            if(value == 0x80) oplretval = 0x02;
            else if(value == 0x21) oplretval = 0xC0;
    }   }
    else oplregno = value;
}

static unsigned char Fmdrv_Inportb(unsigned port)
{
    return (((int)port) >= oplbase && ((int)port) < oplbase+4) ? oplretval : 0;
}

void Fmdrv_Init(int mixfreq)
{
	if(opl!=-1)
	{
		OPLClose();
		opl=-1;
	}
	opl = OPLNew(1789772*2, mixfreq);
	OPLResetChip(opl);
	OPL_Detect();
}

void Fmdrv_MixTo(int* target, int count)
{
	static short *buf = 0;
	static int buf_size = 0;

    if(!fm_active) return;

	if (buf_size != count*2)
	{
		delete[] buf;
		buf = new short[buf_size = count];
	}
    
    memset(buf, 0, count*2);
    OPLUpdateOne(opl, buf, count);
    
    /*
    static int counter=0;
    for(int a=0; a<count; ++a)
        buf[a] = ((counter++) & 0x100) ? -10000 : 10000;
    */
    
    for(int a=0; a<count; ++a)
    {
        target[a*2+0] += buf[a]*2000;
        target[a*2+1] += buf[a]*2000;
    }
}

/***************************************/
static const char PortBases[9] = {0,1,2, 8,9,10, 16,17,18};
static signed char Pans[MAX_CHANNELS];
static const unsigned char *Dtab[MAX_CHANNELS];

static int SetBase(int c)
{
    return c%9;
}

static void OPL_Byte(unsigned char Index, unsigned char Data)
{
    //register int a;
    Fmdrv_Outportb(oplbase, Index);  //for(a=0; a<6;  a++)Fmdrv_Inportb(oplbase);
    Fmdrv_Outportb(oplbase+1, Data); //for(a=0; a<35; a++)Fmdrv_Inportb(oplbase);
}

void OPL_NoteOff(int c)
{
    c = SetBase(c);
    if(c<9)
    {
        /* KEYON_BLOCK+c seems to not work alone?? */
        OPL_Byte(KEYON_BLOCK+c, 0);
        //OPL_Byte(KSL_LEVEL+  Ope, 0xFF);
        //OPL_Byte(KSL_LEVEL+3+Ope, 0xFF);
    }
}

/* OPL_NoteOn changes the frequency on specified
   channel and guarantees the key is on. (Doesn't
   retrig, just turns the note on and sets freq.) */
/* Could be used for pitch bending also. */
void OPL_NoteOn(int c, int Hertz)
{
//fprintf(stderr, "OPL_NoteOn(%d,%d)\n", c, Hertz);
    int Oct;

    c = SetBase(c);
    if(c >= 9)return;
    
    fm_active=1;

#if 1
    for(Oct=0; Hertz>0x1FF; Oct++)Hertz >>= 1;
#else
    for(Oct=-1; Hertz>0x1FF; Oct++)Hertz >>= 1;
    if(Oct<0)Oct=0;
#endif

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
  
    /* Ok - 1.1.1999/Bisqwit */
    OPL_Byte(0xA0+c, Hertz&255);  //F-Number low 8 bits
    OPL_Byte(0xB0+c, 0x20        //Key on
                      | ((Hertz>>8)&3) //F-number high 2 bits
                      | ((Oct&7)<<2)
          );
}

void OPL_Touch(int c, unsigned Vol)
{
    if (c < MAX_CHANNELS) OPL_Touch(c, Dtab[c], Vol);
}

void OPL_Touch(int c, const unsigned char *D, unsigned Vol)
{
    if(!D) return;
    
//fprintf(stderr, "OPL_Touch(%d, %p:%02X.%02X.%02X.%02X-%02X.%02X.%02X.%02X-%02X.%02X.%02X, %d)\n",
//    c, D,D[0],D[1],D[2],D[3],D[4],D[5],D[6],D[7],D[8],D[9],D[10], Vol);
    
    Dtab[c] = D;
    //Vol = Vol * (D[8]>>2) / 63;

    c = SetBase(c);
    if(c >= 9)return;

    int Ope = PortBases[c];

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
    #if 1
    
    /* Ok - 1.1.1999/Bisqwit */
    OPL_Byte(KSL_LEVEL+  Ope, (D[2]&KSL_MASK) |
    /*  (63 - ((63-(D[2]&63)) * Vol / 63) )
    */  (63 + (D[2]&63) * Vol / 63 - Vol)
    );
    OPL_Byte(KSL_LEVEL+3+Ope, (D[3]&KSL_MASK) |
    /*  (63 - ((63-(D[3]&63)) * Vol / 63) )
    */  (63 + (D[3]&63) * Vol / 63 - Vol)
    );
    /* Molemmat tekevt saman, tarkistettu assembleria myten.
    
       The later one is clearly shorter, though   
       it has two extra (not needed) instructions.
    */
    
    #else
    
    int level = (D[2]&63) - (Vol*72-8);
    if(level<0)level=0;  
    if(level>63)level=63;

    OPL_Byte(KSL_LEVEL+  Ope, (D[2]&KSL_MASK) | level);

    level = (D[3]&63) - (Vol*72-8);
    if(level<0)level=0;  
    if(level>63)level=63;

    OPL_Byte(KSL_LEVEL+3+Ope, (D[3]&KSL_MASK) | level);
    
    #endif
}

void OPL_Pan(int c, signed char val)
{
    Pans[c] = val;
    /* Doesn't happen immediately! */
}
 
void OPL_Patch(int c, const unsigned char *D)
{
//fprintf(stderr, "OPL_Patch(%d, %p:%02X.%02X.%02X.%02X-%02X.%02X.%02X.%02X-%02X.%02X.%02X)\n",
//    c, D,D[0],D[1],D[2],D[3],D[4],D[5],D[6],D[7],D[8],D[9],D[10]);
    Dtab[c] = D;

    c = SetBase(c);
    if(c >= 9)return;

    int Ope = PortBases[c];
    
    OPL_Byte(AM_VIB+           Ope, D[0]);
    OPL_Byte(ATTACK_DECAY+     Ope, D[4]);
    OPL_Byte(SUSTAIN_RELEASE+  Ope, D[6]);
    OPL_Byte(WAVE_SELECT+      Ope, D[8]&3);// 6 high bits used elsewhere

    OPL_Byte(AM_VIB+         3+Ope, D[1]);
    OPL_Byte(ATTACK_DECAY+   3+Ope, D[5]);
    OPL_Byte(SUSTAIN_RELEASE+3+Ope, D[7]);
    OPL_Byte(WAVE_SELECT+    3+Ope, D[9]&3);// 6 high bits used elsewhere

    /* Panning... */
    OPL_Byte(FEEDBACK_CONNECTION+c, 
        (D[10] & ~STEREO_BITS)
            | (Pans[c]<-32 ? VOICE_TO_LEFT
                : Pans[c]>32 ? VOICE_TO_RIGHT
                : (VOICE_TO_LEFT | VOICE_TO_RIGHT)
            ));
}
 
void OPL_Reset(void)
{
//fprintf(stderr, "OPL_Reset\n");
    int a;

    for(a=0; a<244; a++)
        OPL_Byte(a, 0); 

    OPL_Byte(TEST_REGISTER, ENABLE_WAVE_SELECT);
    
    fm_active=0;
}

int OPL_Detect(void)
{
    SetBase(0);

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
    int OPLMode = (ST2&0xE0)==0xC0 && !(ST1&0xE0);
    if(!OPLMode)return -1;
    
    
    return 0;
}

void OPL_Close(void)
{
    OPL_Reset();
}
