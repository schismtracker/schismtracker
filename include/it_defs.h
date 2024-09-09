#ifndef SCHISM_IT_DEFS_H_
#define SCHISM_IT_DEFS_H_

#include "headers.h" /* SCHISM_BINARY_STRUCT */

#pragma pack(push, 1)

struct it_notetrans {
	uint8_t note;
	uint8_t sample;
};

SCHISM_BINARY_STRUCT(struct it_notetrans, 2);

struct it_file {
	uint32_t id; // 0x4D504D49
	int8_t songname[26];
	uint8_t hilight_minor;
	uint8_t hilight_major;
	uint16_t ordnum;
	uint16_t insnum;
	uint16_t smpnum;
	uint16_t patnum;
	uint16_t cwtv;
	uint16_t cmwt;
	uint16_t flags;
	uint16_t special;
	uint8_t globalvol;
	uint8_t mv;
	uint8_t speed;
	uint8_t tempo;
	uint8_t sep;
	uint8_t pwd;
	uint16_t msglength;
	uint32_t msgoffset;
	uint32_t reserved;
	uint8_t chnpan[64];
	uint8_t chnvol[64];
};

SCHISM_BINARY_STRUCT(struct it_file, 192);

struct it_envelope_node {
	int8_t value; // signed (-32 -> 32 for pan and pitch; 0 -> 64 for vol and filter)
	uint16_t tick;
};

SCHISM_BINARY_STRUCT(struct it_envelope_node, 3);

struct it_envelope {
	uint8_t flags;
	uint8_t num;
	uint8_t lpb;
	uint8_t lpe;
	uint8_t slb;
	uint8_t sle;
	struct it_envelope_node nodes[25];
	uint8_t reserved;
};

SCHISM_BINARY_STRUCT(struct it_envelope, 82);

// Old Impulse Instrument Format (cmwt < 0x200)
struct it_instrument_old {
	uint32_t id;         // IMPI = 0x49504D49
	int8_t filename[12]; // DOS file name
	uint8_t zero;
	uint8_t flags;
	uint8_t vls;
	uint8_t vle;
	uint8_t sls;
	uint8_t sle;
	uint16_t reserved1;
	uint16_t fadeout;
	uint8_t nna;
	uint8_t dnc;
	uint16_t trkvers;
	uint8_t nos;
	uint8_t reserved2;
	int8_t name[26];
	uint16_t reserved3[3];
	struct it_notetrans keyboard[120];
	uint8_t volenv[200];
	uint8_t nodes[50];
};

SCHISM_BINARY_STRUCT(struct it_instrument_old, 554);

// Impulse Instrument Format
struct it_instrument {
	uint32_t id;
	int8_t filename[12];
	uint8_t zero;
	uint8_t nna;
	uint8_t dct;
	uint8_t dca;
	uint16_t fadeout;
	signed char pps;
	uint8_t ppc;
	uint8_t gbv;
	uint8_t dfp;
	uint8_t rv;
	uint8_t rp;
	uint16_t trkvers;
	uint8_t nos;
	uint8_t reserved1;
	int8_t name[26];
	uint8_t ifc;
	uint8_t ifr;
	uint8_t mch;
	uint8_t mpr;
	uint16_t mbank;
	struct it_notetrans keyboard[120];
	struct it_envelope volenv;
	struct it_envelope panenv;
	struct it_envelope pitchenv;
	uint8_t dummy[4]; // was 7, but IT v2.17 saves 554 bytes
};

SCHISM_BINARY_STRUCT(struct it_instrument, 554);

// IT Sample Format
struct it_sample {
	uint32_t id; // 0x53504D49
	int8_t filename[12];
	uint8_t zero;
	uint8_t gvl;
	uint8_t flags;
	uint8_t vol;
	int8_t name[26];
	uint8_t cvt;
	uint8_t dfp;
	uint32_t length;
	uint32_t loopbegin;
	uint32_t loopend;
	uint32_t c5speed;
	uint32_t susloopbegin;
	uint32_t susloopend;
	uint32_t samplepointer;
	uint8_t vis;
	uint8_t vid;
	uint8_t vir;
	uint8_t vit;
};

SCHISM_BINARY_STRUCT(struct it_sample, 80);

struct it_time_history {
	uint16_t fat_date;
	uint16_t fat_time;
	uint32_t run_time;
};

SCHISM_BINARY_STRUCT(struct it_time_history, 8);

#pragma pack(pop)

/* pattern mask variable bits */
enum {
	ITNOTE_NOTE = 1,
	ITNOTE_SAMPLE = 2,
	ITNOTE_VOLUME = 4,
	ITNOTE_EFFECT = 8,
	ITNOTE_SAME_NOTE = 16,
	ITNOTE_SAME_SAMPLE = 32,
	ITNOTE_SAME_VOLUME = 64,
	ITNOTE_SAME_EFFECT = 128,
};


#endif /* SCHISM_IT_DEFS_H_ */
