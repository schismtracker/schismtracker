#ifndef SCHISM_PLAYER_SND_GM_H_
#define SCHISM_PLAYER_SND_GM_H_

#include "player/sndfile.h"

void GM_Patch(song_t *csf, int32_t c, unsigned char p, int32_t pref_chn_mask);
void GM_DPatch(song_t *csf, int32_t ch, unsigned char GM, unsigned char bank, int32_t pref_chn_mask);

void GM_Bank(song_t *csf, int32_t c, unsigned char b);
void GM_Touch(song_t *csf, int32_t c, unsigned char Vol); // range 0..127
void GM_KeyOn(song_t *csf, int32_t c, unsigned char key, unsigned char Vol); // vol range 0..127
void GM_KeyOff(song_t *csf, int32_t c);
void GM_Bend(song_t *csf, int32_t c, uint32_t Count);
void GM_Reset(song_t *csf, int quitting); // 0=settings that work for us, 1=normal settings

void GM_Pan(song_t *csf, int32_t ch, signed char val); // param: -128..+127

// This function is the core function for MIDI updates.
// It handles keyons, touches and pitch bending.
//   channel   = IT channel on which the event happens
//   Hertz     = The hertz value for this note at the present moment
//   Vol       = The volume for this note at this present moment (0..127)
//   bend_mode = This parameter can provide a hint for the tone calculator
//               for deciding the note to play. If it is to be expected that
//               a large bend up will follow, it may be a good idea to start
//               from a low bend to utilize the maximum bending scale.
//   keyoff    = if nonzero, don't keyon
//
// Note that vibrato etc. are emulated by issuing multiple SetFreqAndVol
// commands; they are not translated into MIDI vibrato operator calls.
typedef enum { MIDI_BEND_NORMAL, MIDI_BEND_DOWN, MIDI_BEND_UP } MidiBendMode;
void GM_SetFreqAndVol(song_t *csf, int32_t channel, int32_t Hertz, int32_t Vol, MidiBendMode bend_mode, int32_t keyoff);

void GM_SendSongStartCode(song_t *csf);
void GM_SendSongStopCode(song_t *csf);
void GM_SendSongContinueCode(song_t *csf);
void GM_SendSongTickCode(song_t *csf);
void GM_SendSongPositionCode(song_t *csf, uint32_t note16pos);
void GM_IncrementSongCounter(song_t *csf, int32_t count);

#endif /* SCHISM_PLAYER_SND_GM_H_ */
