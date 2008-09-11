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
void GM_Reset(void);

void GM_Pan(int ch, signed char val); // param: -128..+127

void GM_SetFreqAndVol(int c, int Hertz, int Vol); // for keyons, touches and pitch bending

#ifdef __cplusplus
}
#endif

#endif
