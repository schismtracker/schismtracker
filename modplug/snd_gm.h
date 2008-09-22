#ifndef _BqtModplugSndGm
#define _BqtModplugSndGm

#ifdef __cplusplus
extern "C" {
#endif

void GM_Patch(int c, unsigned char p);
void GM_DPatch(int ch, unsigned char GM, unsigned char bank);

void GM_Bank(int c, unsigned char b);
void GM_Touch(int c, unsigned char Vol);
void GM_KeyOn(int c, unsigned char key, unsigned char Vol);
void GM_KeyOff(int c);
void GM_Bend(int c, unsigned Count);
void GM_Reset(int quitting); // 0=settings that work for us, 1=normal settings

void GM_Pan(int ch, signed char val); // param: -128..+127

// This function is the core function for MIDI updates.
// It handles keyons, touches and pitch bending.
//   channel   = IT channel on which the event happens
//   Hertz     = The hertz value for this note at the present moment
//   Vol       = The volume for this note at this present moment
//   bend_mode = This parameter can provide a hint for the tone calculator
//               for deciding the note to play. If it is to be expected that
//               a large bend up will follow, it may be a good idea to start
//               from a low bend to utilize the maximum bending scale.
//
// Note that vibrato etc. are emulated by issuing multiple SetFreqAndVol
// commands; they are not translated into MIDI vibrato operator calls.
typedef enum { MIDI_BEND_NORMAL, MIDI_BEND_DOWN, MIDI_BEND_UP } MidiBendMode;
void GM_SetFreqAndVol(int channel, int Hertz, int Vol, MidiBendMode bend_mode);

void GM_SendSongStartCode(void);
void GM_SendSongStopCode(void);
void GM_SendSongContinueCode(void);
void GM_SendSongTickCode(void);
void GM_SendSongPositionCode(unsigned note16pos);
void GM_IncrementSongCounter(int count);

#ifdef __cplusplus
}
#endif

#endif
