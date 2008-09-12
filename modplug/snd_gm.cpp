/* This is a wrapper which converts S3M style thinking
 * into MIDI style thinking.
*/

#include "it.h"
#include "mplink.h"
#include "snd_gm.h"

#include <math.h> // for log and log2

#ifndef MIDISlideFIX
 #define MIDISlideFIX 1
 /* Changes midi volumes between 1..11 to be 11 */
#endif

static const unsigned MAXCHN = 256;
static const bool LinearMidiVol = true;
static const unsigned PitchBendCenter = 0x2000;

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
    0=Bank select               7=Master Volume     11=Expression(volume?)
    1=Modulation Wheel(Vibrato)10=Pan Position      64=Sustain Pedal
    6=Data Entry MSB           38=Data Entry LSB    91=Effects Depth(Reverb)
  120=All Sound Off           123=All Notes Off     93=Chorus Depth
  100=reg'd param # LSB       101=reg'd param # LSB
   98=!reg'd param # LSB       99=!reg'd param # LSB

    1=Vibrato, 121=reset vibrato,bend
*/

static unsigned RunningStatus = 0;
static void MPU_SendCommand(const unsigned char* buf, unsigned nbytes, int c)
{
    if(!nbytes) return;
    if((buf[0] & 0x80) && buf[0] == RunningStatus)
        { ++buf; --nbytes; }
    else
        RunningStatus = buf[0];
#if 0
    char Buf[2048],*t=Buf;
    t += sprintf(t, "Sending:");
    for(unsigned n=0; n<nbytes; ++n)
        t += sprintf(t, " %02X", buf[n]);
    fprintf(stderr, "%s\n", Buf);
#endif
    mp->MidiSend(buf, nbytes, c, 0);
}
static void MPU_Ctrl(int c, int i, int v)
{
    if(!(status.flags & MIDI_LIKE_TRACKER)) return;
    
    unsigned char buf[3] = {0xB0+c,i,v};
    MPU_SendCommand(buf, 3, c);
}
static void MPU_Patch(int c, int p)
{
    if(!(status.flags & MIDI_LIKE_TRACKER)) return;
    
    unsigned char buf[2] = {0xC0+c, p};
    MPU_SendCommand(buf, 2, c);
}
static void MPU_Bend(int c, int w)
{
    if(!(status.flags & MIDI_LIKE_TRACKER)) return;
    
    unsigned char buf[3] = {0xE0+c, w&127, w>>7};
    MPU_SendCommand(buf, 3, c);
}
static void MPU_NoteOn(int c, int k, int v)
{
    if(!(status.flags & MIDI_LIKE_TRACKER)) return;
    
    unsigned char buf[3] = {0x90+c, k, v};
    MPU_SendCommand(buf, 3, c);
}
static void MPU_NoteOff(int c, int k, int v)
{
    if(!(status.flags & MIDI_LIKE_TRACKER)) return;
    
    if(((unsigned char)RunningStatus) == 0x90+c)
    {
        // send a zero-velocity keyoff instead for optimization
        MPU_NoteOn(c, k, 0);
    }
    else
    {
        unsigned char buf[3] = {0x80+c, k, v};
        MPU_SendCommand(buf, 3, c);
    }
}


static const unsigned char GMVol[64] = /* This converts Adlib volume into MIDI volume */
{
#if MIDISlideFIX
    /*00*/0x00,0x0B,0x0B,0x0B, 0x0B,0x0B,0x0B,0x0B,
    /*08*/0x0B,0x0B,0x0B,0x0B, 0x0B,0x0B,0x0B,0x0B,
    /*16*/0x0B,0x0B,0x0B,0x0C, 0x0D,0x0F,0x10,0x11,
#else
    /*00*/0x00,0x01,0x01,0x01, 0x01,0x01,0x01,0x02,
    /*08*/0x02,0x03,0x04,0x04, 0x05,0x06,0x07,0x08,
    /*16*/0x09,0x0A,0x0B,0x0C, 0x0D,0x0F,0x10,0x11,
#endif
    /*24*/0x13,0x15,0x16,0x18, 0x1A,0x1C,0x1E,0x1F,
    /*32*/0x22,0x24,0x26,0x28, 0x2A,0x2D,0x2F,0x31,
    /*40*/0x34,0x37,0x39,0x3C, 0x3F,0x42,0x45,0x47,
    /*48*/0x4B,0x4E,0x51,0x54, 0x57,0x5B,0x5E,0x61,
    /*56*/0x64,0x69,0x6C,0x70, 0x74,0x78,0x7C,0x7F
};


struct S3MchannelInfo
{
public:
    // Which note is playing in this channel (0 = nothing)
    unsigned char note;
    // Which patch was programmed on this channel (&0x80 = percussion)
    unsigned char patch;
    // Which bank was programmed on this channel
    unsigned char bank;
    // Which pan level was last selected
    signed char pan;
    // Which MIDI channel was allocated for this channel. -1 = none
    signed char chan;
public:
    bool IsActive()     const { return note && chan >= 0; }
    bool IsPercussion() const { return patch & 0x80; }
    
    S3MchannelInfo() : note(0),patch(0),bank(0),pan(0),chan(-1) { }
};

/* This maps S3M concepts into MIDI concepts */
static S3MchannelInfo S3Mchans[MAXCHN];

struct MIDIstateInfo
{
public:
    // Which volume has been configured for this channel
    unsigned char volume;
    // What is the latest patch configured on this channel
    unsigned char patch;
    // What is the latest bank configured on this channel
    unsigned char bank;
    // The latest pitchbend on this channel
    int bend;
    // Latest pan
    signed char pan;
public:
    MIDIstateInfo() : volume(),patch(),bank(), bend(), pan()
    {
        KnowNothing();
    }
    
    void SetVolume(int c, unsigned newvol)
    {
        if(volume == newvol) return;
        MPU_Ctrl(c, 7, volume=newvol);
    }
    
    void SetPatchAndBank(int c, int p, int b)
    {
        if(b != bank)  MPU_Ctrl(c, 0, bank=b);
        if(p != patch) MPU_Patch(c, patch=p);
    }
    
    void SetPitchBend(int c, int value)
    {
        if(value == bend) return;
        MPU_Bend(c, bend = value);
    }
    
    void SetPan(int c, int value)
    {
        if(value == pan) return;
		MPU_Ctrl(c, 10, (unsigned char)((pan=value)+128) / 2);
    }
    
    void KnowNothing()
    {
        volume = 255;
        patch = 255;
        bank = 255;
        bend = PitchBendCenter;
        pan = 0;
    }
    bool KnowSomething() const { return patch != 255; }
};

/* This helps reduce the MIDI traffic, also does some encapsulation */
static MIDIstateInfo MIDIchans[16];

static unsigned char GM_Volume(unsigned char Vol) // Converts the volume
{
	if(LinearMidiVol)return Vol>=63?127:127*Vol/63;
	return GMVol[Vol>63?63:Vol];
	/* Prevent overflows */
}

static int GM_AllocateMelodyChannel(int c, int patch, int bank, int key)
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
    bool bad_channels[16] = {};  // channels having the same key playing
    bool used_channels[16] = {}; // channels having something playing
    
    memset(bad_channels, 0, sizeof(bad_channels));
    memset(used_channels, 0, sizeof(used_channels));
    
    for(unsigned int a=0; a<MAXCHN; ++a)
    {
        if(S3Mchans[a].IsActive() && !S3Mchans[a].IsPercussion())
        {
            //fprintf(stderr, "S3M[%d] active at %d\n", a, S3Mchans[a].chan);
            used_channels[S3Mchans[a].chan] = true; // channel is active
            if(S3Mchans[a].note == key)
                bad_channels[S3Mchans[a].chan] = true; // ...with the same key
        }
    }

    int best_mc = c, best_score = -999;
    for(int mc=0; mc<16; ++mc)
    {
        if(mc == 9) continue; // percussion channel is never chosen for melody.
        int score = 0;
        if(MIDIchans[mc].KnowSomething())
        {
            if(MIDIchans[mc].patch != patch) score -= 4; // different patch
            if(MIDIchans[mc].bank  !=  bank) score -= 6; // different bank
        }
        if(c == mc) score += 1;                      // same channel number
        if(bad_channels[mc]) score -= 9;             // has same key on
        if(!used_channels[mc]) score += 2;           // channel is unused
        //fprintf(stderr, "score %d for channel %d\n", score, mc);
        if(score > best_score) { best_score=score; best_mc=mc; }
    }
    //fprintf(stderr, "BEST SCORE %d FOR CHANNEL %d\n", best_score,best_mc);
    return best_mc;
}

void GM_Patch(int c, unsigned char p)
{
    if(c < 0 || ((unsigned int)c) >= MAXCHN) return;
	S3Mchans[c].patch = p; // No actual data is sent.
}

void GM_Bank(int c, unsigned char b)
{
    if(c < 0 || ((unsigned int)c) >= MAXCHN) return;
    S3Mchans[c].bank = b; // No actual data is sent yet.
}

void GM_Touch(int c, unsigned char Vol)
{
    if(c < 0 || ((unsigned int)c) >= MAXCHN) return;
	/* This function must only be called when
	 * a key has been played on the channel. */
	if(!S3Mchans[c].IsActive()) return;
	
	int mc = S3Mchans[c].chan;
	MIDIchans[mc].SetVolume(mc, GM_Volume(Vol));
}

void GM_KeyOn(int c, unsigned char key, unsigned char Vol)
{
    if(c < 0 || ((unsigned int)c) >= MAXCHN) return;
    GM_KeyOff(c); // Ensure the previous key on this channel is off.
    
    if(S3Mchans[c].IsActive()) return; // be sure the channel is deactivated.
    
    if(S3Mchans[c].IsPercussion())
    {
        // Percussion always uses channel 9. Key (pitch) is ignored.
        int percu = S3Mchans[c].patch - 128;
        int mc = S3Mchans[c].chan = 9;
		MIDIchans[mc].SetPan(mc, S3Mchans[c].pan);
		MIDIchans[mc].SetVolume(mc, GM_Volume(Vol));
		S3Mchans[c].note = key;
        MPU_NoteOn(mc,
                   S3Mchans[c].note = percu,
                   127);
    }
    else
    {
        // Allocate a MIDI channel for this key.
        // Note: If you need to transpone the key, do it before allocating the channel.
        
        int mc = S3Mchans[c].chan =
            GM_AllocateMelodyChannel(c, S3Mchans[c].patch, S3Mchans[c].bank, key);

        MIDIchans[mc].SetPatchAndBank(mc, S3Mchans[c].patch, S3Mchans[c].bank);
        MIDIchans[mc].SetVolume(mc, GM_Volume(Vol));
        MPU_NoteOn(mc,
                   S3Mchans[c].note = key,
                   127);
		MIDIchans[mc].SetPan(mc, S3Mchans[c].pan);
    }
}

void GM_KeyOff(int c)
{
    if(c < 0 || ((unsigned int)c) >= MAXCHN) return;
    if(!S3Mchans[c].IsActive()) return; // nothing to do

    //fprintf(stderr, "GM_KeyOff(%d)\n", c);
    
    int mc = S3Mchans[c].chan;
    MPU_NoteOff(mc,
                S3Mchans[c].note,
                0);
    S3Mchans[c].chan = -1;
    S3Mchans[c].note = 0;
    S3Mchans[c].pan  = 0;
    // Don't reset the pitch bend, it will make sustains sound bad
}

void GM_Bend(int c, unsigned Count)
{
    if(c < 0 || ((unsigned int)c) >= MAXCHN) return;
    /* I hope nobody tries to bend hi-hat or something like that :-) */
    /* 1998-10-03 01:50 Apparently that can happen too...
       For example in the last pattern of urq.mod there's
       a hit of a heavy plate, which is followed by a J0A
       0.5 seconds thereafter for the same channel.
       Unfortunately MIDI cannot do that. Drum plate
       sizes can rarely be adjusted while playing. -Bisqwit
       However, we don't stop anyone from trying...
	*/
    if(S3Mchans[c].IsActive())
    {
        int mc = S3Mchans[c].chan;
        MIDIchans[mc].SetPitchBend(mc, Count);
    }
}

void GM_Reset(void)
{
	unsigned int a;
    //fprintf(stderr, "GM_Reset\n");
	for(a=0; a<MAXCHN; a++)
	{
	    GM_KeyOff(a);
	    S3Mchans[a].patch = S3Mchans[a].bank = S3Mchans[a].pan = 0;
	}
    for(a=0; a<16; a++)
    {
        MPU_Ctrl(a, 0x7B, 0);   // turn off all notes
        MPU_Ctrl(a, 10,   0);   // reset pan position
        MIDIchans[a].SetVolume(a, 127);      // set channel volume
        MIDIchans[a].SetPitchBend(a, PitchBendCenter);// reset pitch bends
        MIDIchans[a].KnowNothing();
	}
}

void GM_DPatch(int ch, unsigned char GM, unsigned char bank)
{
    //fprintf(stderr, "GM_DPatch(%d, %02X @ %d)\n", ch, GM, bank);
    if(ch < 0 || ((unsigned int)ch) >= MAXCHN) return;
	GM_Bank(ch, bank);
	GM_Patch(ch, GM);
}
void GM_Pan(int c, signed char val)
{
    //fprintf(stderr, "GM_Pan(%d,%d)\n", c,val);
    if(c < 0 || ((unsigned int)c) >= MAXCHN) return;
	S3Mchans[c].pan = val;
	
	// If a note is playing, effect immediately.
	if(S3Mchans[c].IsActive())
	{
		int mc = S3Mchans[c].chan;
		MIDIchans[mc].SetPan(mc, val);
	}
}

#if !defined(HAVE_LOG2) && !defined(__USE_ISOC99)
static double log2(double d)
{
    return log(d) / log(2.0);
}
#endif

void GM_SetFreqAndVol(int c, int Hertz, int Vol) // for keyons and pitch bending  alike
{
    //fprintf(stderr, "GM_SetFreqAndVol(%d,%d,%d)\n", c,Hertz,Vol);
    if(c < 0 || ((unsigned int)c) >= MAXCHN) return;
    
    
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
    double midinote = 69 + 12.0 * log2(Hertz/440.0);
    
    midinote -= 12*5;

    int note = S3Mchans[c].note; // what's playing on the channel right now?
    
    bool new_note = !S3Mchans[c].IsActive();
    if(new_note)
    {
        // If the note is not active, activate it first.
        // Choose the nearest note to Hertz.
        note = (int)(midinote + 0.5);
        if(note < 1) note = 1;
        if(note > 127) note = 127;
        GM_KeyOn(c, note, Vol);
    }
    
    if(!S3Mchans[c].IsPercussion()) // give us a break, don't bend percussive instruments
    {
		const int semitone_bend_depth = 0x2000; // The range of bending equivalent to 1 semitone
		
		double notediff = midinote-note; // The difference is our bend value
		int bend = (int)(notediff * semitone_bend_depth) + PitchBendCenter;
		
		bend = (bend / 82) * 82;
		// Because the log2 calculation does not always give pure notes,
		// and in fact, gives a lot of variation, we reduce the bending
		// precision to 100 cents. This is accurate enough for almost
		// all purposes, but will significantly reduce the bend event load.
				
		if(bend < 0) bend = 0;
		if(bend > 0x3FFF) bend = 0x3FFF;
		
		if(Vol < 0) Vol = 0;
		if(Vol > 127) Vol = 127;
		
		GM_Bend(c, bend);
    }
    
    //if(!new_note)
    GM_Touch(c, Vol);
}
